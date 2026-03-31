# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4143/5409 lines (76.59%)

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
|   767110 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   767112 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   767082 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   767074 |    94 | `	return FALSE;` |
|   383579 |    95 |  |
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
|   456362 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   456364 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   456364 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   456360 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   456360 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   456360 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   456360 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   456360 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   456360 |   142 | `	pCons->xExpand = xExpand;` |
|   456360 |   143 | `	pCons->pUserData = pUserData;` |
|   456360 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   456360 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   456360 |   151 | `	return SXRET_OK;` |
|   228183 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   977880 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   977882 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   977882 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   977882 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   977882 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   977882 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   977882 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   977882 |   185 | `	pFunc->pVm   = pVm;` |
|   977882 |   186 | `	pFunc->xFunc = xFunc;` |
|   977882 |   187 | `	pFunc->pUserData = pUserData;` |
|   977882 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   977882 |   190 | `	*ppOut = pFunc;` |
|   977882 |   191 | `	return SXRET_OK;` |
|   488942 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   980128 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   980130 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   980130 |   213 | `	if( pEntry ){` |
|     2250 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2250 |   215 | `		pFunc->pUserData = pUserData;` |
|     2250 |   216 | `		pFunc->xFunc = xFunc;` |
|     2250 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2250 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   977882 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   977882 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   977882 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   977882 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   977882 |   233 | `	return SXRET_OK;` |
|   490066 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   106166 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   106168 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   106168 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   106168 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   106168 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   106168 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   106168 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   106168 |   260 | `	pFunc->iFlags = iFlags;` |
|   106168 |   261 | `	pFunc->pUserData = pUserData;` |
|   106168 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   106168 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   385282 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   385284 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    33074 |   283 | `		pName = &pFunc->sName;` |
|    16536 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   385284 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   385284 |   287 | `	if( pEntry ){` |
|   299418 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   299418 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   299418 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|    85868 |   297 | `	pFunc->pNextName = 0;` |
|    85868 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    85868 |   299 | `	return rc;` |
|   192643 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    30510 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    30512 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    30512 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    30512 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    30482 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    30482 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    30482 |   324 | `	return rc;` |
|    15257 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  2818412 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2818414 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  2818414 |   342 | `	sInstr.iP1 = iP1;` |
|  2818414 |   343 | `	sInstr.iP2 = iP2;` |
|  2818414 |   344 | `	sInstr.p3  = p3;` |
|  2818414 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   179026 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    89512 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  2818414 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2818414 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  2818414 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   258000 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   258002 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   258002 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   258002 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   129000 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   129002 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   176442 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   176444 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   176444 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|   787512 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|   787514 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   167532 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   167534 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   552470 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   552472 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    25684 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    25686 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    25686 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    25686 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    25686 |   417 | `	return &aInstr[n - 2];` |
|    12844 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    14966 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    14968 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    14968 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    14968 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    14968 |   437 | `	pFrame->pUserData = pUserData;` |
|    14968 |   438 | `	pFrame->pThis = pThis;` |
|    14968 |   439 | `	pFrame->pVm = pVm;` |
|    14968 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    14968 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    14968 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    14968 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    14968 |   444 | `	return pFrame;` |
|     7485 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Enter a VM frame.` |
|        - |   448 | ` */` |
|    14966 |   449 | `static sxi32 VmEnterFrame(` |
|        - |   450 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   451 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   452 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   453 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   454 | `	)` |
|        2 |   455 |  |
|        - |   456 | `	VmFrame *pFrame;` |
|        - |   457 | `	/* Allocate a new frame */` |
|    14968 |   458 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    14968 |   459 | `	if( pFrame == 0 ){` |
|      ! 0 |   460 | `		return SXERR_MEM;` |
|        - |   461 | `	}` |
|        - |   462 | `	/* Link to the list of active VM frame */` |
|    14968 |   463 | `	pFrame->pParent = pVm->pFrame;` |
|    14968 |   464 | `	pVm->pFrame = pFrame;` |
|    14968 |   465 | `	if( ppFrame ){` |
|        - |   466 | `		/* Write a pointer to the new VM frame */` |
|    12462 |   467 | `		*ppFrame = pFrame;` |
|     6230 |   468 | `	}` |
|    14968 |   469 | `	return SXRET_OK;` |
|     7485 |   470 |  |
|        - |   471 | `/*` |
|        - |   472 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   473 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   474 | ` * information.` |
|        - |   475 | ` */` |
|       52 |   476 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   477 |  |
|        - |   478 | `	VmFrame *pTarget,*pFrame;` |
|       54 |   479 | `	SyHashEntry *pEntry = 0;` |
|        - |   480 | `	sxi32 rc;` |
|        - |   481 | `	/* Point to the upper frame */` |
|       54 |   482 | `	pFrame = pVm->pFrame;` |
|       54 |   483 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   484 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   485 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   486 | `	}` |
|       54 |   487 | `	pTarget = pFrame;` |
|       54 |   488 | `	pFrame = pTarget->pParent;` |
|       54 |   489 | `	while( pFrame ){` |
|       54 |   490 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   491 | `			/* Query the current frame */` |
|       54 |   492 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       54 |   493 | `			if( pEntry ){` |
|        - |   494 | `				/* Variable found */` |
|       54 |   495 | `				break;` |
|        - |   496 | `			}` |
|      ! 0 |   497 | `		}` |
|        - |   498 | `		/* Point to the upper frame */` |
|      ! 0 |   499 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   500 | `	}` |
|       54 |   501 | `	if( pEntry == 0 ){` |
|        - |   502 | `		/* Inexistant variable */` |
|      ! 0 |   503 | `		return SXERR_NOTFOUND;` |
|        - |   504 | `	}` |
|        - |   505 | `	/* Link to the current frame */` |
|       54 |   506 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       54 |   507 | `	if( rc == SXRET_OK ){` |
|        - |   508 | `		sxu32 nIdx;` |
|       54 |   509 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       54 |   510 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       26 |   511 | `	}` |
|       54 |   512 | `	return rc;` |
|       28 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Leave the top-most active frame.` |
|        - |   516 | ` */` |
|    12460 |   517 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   518 |  |
|    12462 |   519 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12462 |   520 | `	if( pCurFrame ){` |
|        - |   521 | `		/* Unlink from the list of active VM frame */` |
|    12462 |   522 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12462 |   523 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   524 | `			VmSlot  *aSlot;` |
|        - |   525 | `			sxu32 n;` |
|        - |   526 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12414 |   527 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    88072 |   528 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   529 | `				/* Unset the local variable */` |
|    75660 |   530 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    37831 |   531 | `			}` |
|        - |   532 | `			/* Remove local reference */` |
|    12414 |   533 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    88128 |   534 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    75716 |   535 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    37859 |   536 | `			}` |
|     6206 |   537 | `		}` |
|        - |   538 | `		/* Release internal containers */` |
|    12462 |   539 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12462 |   540 | `		SySetRelease(&pCurFrame->sArg);` |
|    12462 |   541 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12462 |   542 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   543 | `		/* Release the whole structure */` |
|    12462 |   544 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6230 |   545 | `	}` |
|    12462 |   546 |  |
|        - |   547 | `/*` |
|        - |   548 | ` * Compare two functions signature and return the comparison result.` |
|        - |   549 | ` */` |
|      818 |   550 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   551 |  |
|      819 |   552 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   553 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   554 | `	const char *zSin = pSecond->zString;` |
|      819 |   555 | `	const char *zFin = pFirst->zString;` |
|      819 |   556 | `	const char *zPtr = zFin;` |
|      409 |   557 | `	for(;;){` |
|      819 |   558 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   559 | `			break;` |
|        - |   560 | `		}` |
|      ! 0 |   561 | `		if( zFin[0] != zSin[0] ){` |
|        - |   562 | `			/* mismatch */` |
|      ! 0 |   563 | `			break;` |
|        - |   564 | `		}` |
|      ! 0 |   565 | `		zFin++;` |
|      ! 0 |   566 | `		zSin++;` |
|      ! 0 |   567 | `	}` |
|      819 |   568 | `	return (int)(zFin-zPtr);` |
|        1 |   569 |  |
|        - |   570 | `/*` |
|        - |   571 | ` * Select the appropriate VM function for the current call context.` |
|        - |   572 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   573 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   574 | ` * Refer to the official documentation for more information.` |
|        - |   575 | ` */` |
|      132 |   576 | `static ph7_vm_func * VmOverload(` |
|        - |   577 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   578 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   579 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   580 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   581 | `	)` |
|        2 |   582 |  |
|        - |   583 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   584 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   585 | `	ph7_vm_func *pLink;` |
|        - |   586 | `	SyString sArgSig;` |
|        - |   587 | `	SyBlob sSig;` |
|        - |   588 |  |
|      134 |   589 | `	pLink = pList;` |
|      134 |   590 | `	i = 0;` |
|        - |   591 | `	/* Put functions expecting the same number of passed arguments */` |
|     1062 |   592 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1000 |   593 | `		if( pLink == 0 ){` |
|       72 |   594 | `			break;` |
|        - |   595 | `		}` |
|      930 |   596 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   597 | `			/* Candidate for overloading */` |
|      884 |   598 | `			apSet[i++] = pLink;` |
|      441 |   599 | `		}` |
|        - |   600 | `		/* Point to the next entry */` |
|      930 |   601 | `		pLink = pLink->pNextName;` |
|        2 |   602 | `	}` |
|      134 |   603 | `	if( i < 1 ){` |
|        - |   604 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   605 | `		return pList;` |
|        - |   606 | `	}` |
|      134 |   607 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   608 | `		/* Return the only candidate */` |
|       32 |   609 | `		return apSet[0];` |
|        - |   610 | `	}` |
|        - |   611 | `	/* Calculate function signature */` |
|      103 |   612 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   613 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   614 | `		int c = 'n'; /* null */` |
|      253 |   615 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   616 | `			/* Hashmap */` |
|       45 |   617 | `			c = 'h';` |
|      231 |   618 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   619 | `			/* bool */` |
|      ! 0 |   620 | `			c = 'b';` |
|      209 |   621 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   622 | `			/* int */` |
|        5 |   623 | `			c = 'i';` |
|      207 |   624 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   625 | `			/* String */` |
|      105 |   626 | `			c = 's';` |
|      153 |   627 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   628 | `			/* Float */` |
|      ! 0 |   629 | `			c = 'f';` |
|      101 |   630 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   631 | `			/* Class instance */` |
|      ! 0 |   632 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   633 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   634 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   635 | `			c = -1;` |
|      ! 0 |   636 | `		}` |
|      253 |   637 | `		if( c > 0 ){` |
|      253 |   638 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   639 | `		}` |
|      127 |   640 | `	}` |
|      103 |   641 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   642 | `	iTarget = 0;` |
|      103 |   643 | `	iMax = -1;` |
|        - |   644 | `	/* Select the appropriate function */` |
|      921 |   645 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   646 | `		/* Compare the two signatures */` |
|      819 |   647 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   648 | `		if( iCur > iMax ){` |
|      103 |   649 | `			iMax = iCur;` |
|      103 |   650 | `			iTarget = j;` |
|       51 |   651 | `		}` |
|      410 |   652 | `	}` |
|      103 |   653 | `	SyBlobRelease(&sSig);` |
|        - |   654 | `	/* Appropriate function for the current call context */` |
|      103 |   655 | `	return apSet[iTarget];` |
|       68 |   656 |  |
|        - |   657 | `/* Forward declaration */` |
|        - |   658 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   659 | `/*` |
|        - |   660 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   661 | ` * it can be instanciated from the executed PHP script.` |
|        - |   662 | ` */` |
|    89288 |   663 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   664 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   665 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   666 | `	)` |
|        2 |   667 |  |
|        - |   668 | `	ph7_class_method *pMeth;` |
|        - |   669 | `	ph7_class_attr *pAttr;` |
|        - |   670 | `	SyHashEntry *pEntry;` |
|        - |   671 | `	sxi32 rc;` |
|        - |   672 | `	/* Reset the loop cursor */` |
|    89290 |   673 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   674 | `	/* Process only static and constant attribute */` |
|   354132 |   675 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   676 | `		/* Extract the current attribute */` |
|   220200 |   677 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   220200 |   678 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   679 | `			ph7_value *pMemObj;` |
|        - |   680 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1294 |   681 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1294 |   682 | `			if( pMemObj == 0 ){` |
|      ! 0 |   683 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   684 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   685 | `					&pClass->sName,&pAttr->sName` |
|        - |   686 | `					);` |
|      ! 0 |   687 | `				return SXERR_MEM;` |
|        - |   688 | `			}` |
|     1294 |   689 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   690 | `				/* Initialize attribute default value (any complex expression) */` |
|     1294 |   691 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      646 |   692 | `			}` |
|        - |   693 | `			/* Record attribute index */` |
|     1294 |   694 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   695 | `			/* Install static attribute in the reference table */` |
|     1294 |   696 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      646 |   697 | `		}` |
|        2 |   698 | `	}` |
|        - |   699 | `	/* Install class methods */` |
|    89290 |   700 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   701 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   702 | `		 */` |
|    46188 |   703 | `		return SXRET_OK;` |
|        - |   704 | `	}` |
|        - |   705 | `	/* Create constructor alias if not yet done */` |
|    43104 |   706 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   707 | `		/* User constructor with the same base class name */` |
|      278 |   708 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      278 |   709 | `		if( pEntry ){` |
|      ! 0 |   710 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   711 | `			/* Create the alias */` |
|      ! 0 |   712 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   713 | `		}` |
|      138 |   714 | `	}` |
|        - |   715 | `	/* Install the methods now */` |
|    43104 |   716 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   416871 |   717 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   352218 |   718 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   352218 |   719 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   352212 |   720 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   352212 |   721 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   722 | `				return rc;` |
|        - |   723 | `			}` |
|   176105 |   724 | `		}` |
|        2 |   725 | `	}` |
|        - |   726 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    43104 |   727 | `	pClass->bMounted = TRUE;` |
|    43104 |   728 | `	return SXRET_OK;` |
|    44646 |   729 |  |
|        - |   730 | `/*` |
|        - |   731 | ` * Allocate a private frame for attributes of the given` |
|        - |   732 | ` * class instance (Object in the PHP jargon).` |
|        - |   733 | ` */` |
|     1124 |   734 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   735 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   736 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   737 | `	)` |
|        2 |   738 |  |
|     1126 |   739 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   740 | `	ph7_class_attr *pAttr;` |
|        - |   741 | `	SyHashEntry *pEntry;` |
|        - |   742 | `	sxi32 rc;` |
|        - |   743 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1126 |   744 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4698 |   745 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   746 | `		VmClassAttr *pVmAttr;` |
|        - |   747 | `		/* Extract the current attribute */` |
|     3574 |   748 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3574 |   749 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3574 |   750 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   751 | `			return SXERR_MEM;` |
|        - |   752 | `		}` |
|     3574 |   753 | `		pVmAttr->pAttr = pAttr;` |
|     3574 |   754 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   755 | `			ph7_value *pMemObj;` |
|        - |   756 | `			/* Reserve a memory object for this attribute */` |
|     3568 |   757 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3568 |   758 | `			if( pMemObj == 0 ){` |
|      ! 0 |   759 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   760 | `				return SXERR_MEM;` |
|        - |   761 | `			}` |
|     3568 |   762 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3568 |   763 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   764 | `				/* Initialize attribute default value (any complex expression) */` |
|     1170 |   765 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      584 |   766 | `			}` |
|     3568 |   767 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3568 |   768 | `			if( rc != SXRET_OK ){` |
|        - |   769 | `				VmSlot sSlot;` |
|        - |   770 | `				/* Restore memory object */` |
|      ! 0 |   771 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   772 | `				sSlot.pUserData = 0;` |
|      ! 0 |   773 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   774 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   775 | `				return SXERR_MEM;` |
|        - |   776 | `			}` |
|        - |   777 | `			/* Install attribute in the reference table */` |
|     3568 |   778 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1785 |   779 | `		}else{` |
|        - |   780 | `			/* Install static/constant attribute */` |
|        8 |   781 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   782 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   783 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   784 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   785 | `				return SXERR_MEM;` |
|        - |   786 | `			}` |
|        - |   787 | `		}` |
|        2 |   788 | `	}` |
|     1126 |   789 | `	return SXRET_OK;` |
|      564 |   790 |  |
|        - |   791 | `/* Forward declaration */` |
|        - |   792 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   793 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   794 | `/*` |
|        - |   795 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   796 | ` */` |
|        - |   797 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   798 | `/*` |
|        - |   799 | ` * Reserve a constant memory object.` |
|        - |   800 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   801 | ` */` |
|   305946 |   802 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   803 |  |
|        - |   804 | `	ph7_value *pObj;` |
|        - |   805 | `	sxi32 rc;` |
|   305948 |   806 | `	if( pIndex ){` |
|        - |   807 | `		/* Object index in the object table */` |
|   298430 |   808 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   149214 |   809 | `	}` |
|        - |   810 | `	/* Reserve a slot for the new object */` |
|   305948 |   811 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   305948 |   812 | `	if( rc != SXRET_OK ){` |
|        - |   813 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   814 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   815 | `		 */` |
|      ! 0 |   816 | `		return 0;` |
|        - |   817 | `	}` |
|   305948 |   818 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   305948 |   819 | `	return pObj;` |
|   152975 |   820 |  |
|        - |   821 | `/*` |
|        - |   822 | ` * Reserve a memory object.` |
|        - |   823 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   824 | ` */` |
|  2140134 |   825 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   826 |  |
|        - |   827 | `	ph7_value *pObj;` |
|        - |   828 | `	sxi32 rc;` |
|  2140136 |   829 | `	if( pIndex ){` |
|        - |   830 | `		/* Object index in the object table */` |
|  2140136 |   831 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1070067 |   832 | `	}` |
|        - |   833 | `	/* Reserve a slot for the new object */` |
|  2140136 |   834 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2140136 |   835 | `	if( rc != SXRET_OK ){` |
|        - |   836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   838 | `		 */` |
|      ! 0 |   839 | `		return 0;` |
|        - |   840 | `	}` |
|  2140136 |   841 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2140136 |   842 | `	return pObj;` |
|  1070069 |   843 |  |
|        - |   844 | `/* Forward declaration */` |
|        - |   845 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   846 | `/*` |
|        - |   847 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   848 | ` * directly as foreign functions.` |
|        - |   849 | ` */` |
|        - |   850 | `#define PH7_BUILTIN_LIB \` |
|        - |   851 | `	"class Exception { "\` |
|        - |   852 | `    "protected $message = 'Unknown exception';"\` |
|        - |   853 | `    "protected $code = 0;"\` |
|        - |   854 | `    "protected $file;"\` |
|        - |   855 | `    "protected $line;"\` |
|        - |   856 | `    "protected $trace;"\` |
|        - |   857 | `    "protected $previous;"\` |
|        - |   858 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   859 | `	"   if( isset($message) ){"\` |
|        - |   860 | `	"	  $this->message = $message;"\` |
|        - |   861 | `	"   }"\` |
|        - |   862 | `	"   $this->code = $code;"\` |
|        - |   863 | `	"   $this->file = __FILE__;"\` |
|        - |   864 | `	"   $this->line = __LINE__;"\` |
|        - |   865 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   866 | `	"   if( isset($previous) ){"\` |
|        - |   867 | `	"     $this->previous = $previous;"\` |
|        - |   868 | `	"   }"\` |
|        - |   869 | `	"}"\` |
|        - |   870 | `	"public function getMessage(){"\` |
|        - |   871 | `	"   return $this->message;"\` |
|        - |   872 | `	"}"\` |
|        - |   873 | `	" public function getCode(){"\` |
|        - |   874 | `	"  return $this->code;"\` |
|        - |   875 | `	"}"\` |
|        - |   876 | `	"public function getFile(){"\` |
|        - |   877 | `	"  return $this->file;"\` |
|        - |   878 | `	"}"\` |
|        - |   879 | `	"public function getLine(){"\` |
|        - |   880 | `	"  return $this->line;"\` |
|        - |   881 | `	"}"\` |
|        - |   882 | `	"public function getTrace(){"\` |
|        - |   883 | `	"   return $this->trace;"\` |
|        - |   884 | `	"}"\` |
|        - |   885 | `	"public function getTraceAsString(){"\` |
|        - |   886 | `	"  return debug_string_backtrace();"\` |
|        - |   887 | `	"}"\` |
|        - |   888 | `	"public function getPrevious(){"\` |
|        - |   889 | `	"    return $this->previous;"\` |
|        - |   890 | `	"}"\` |
|        - |   891 | `	"public function __toString(){"\` |
|        - |   892 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   893 | `    "}"\` |
|        - |   894 | `	"}"\` |
|        - |   895 | `	"class Error extends Exception { }"\` |
|        - |   896 | `	"class TypeError extends Error { }"\` |
|        - |   897 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   898 | `	"class ValueError extends Error { }"\` |
|        - |   899 | `	"class AssertionError extends Error { }"\` |
|        - |   900 | `	"class ErrorException extends Exception { "\` |
|        - |   901 | `	"protected $severity;"\` |
|        - |   902 | `	"public function __construct(string $message = null,"\` |
|        - |   903 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   904 | `	"   if( isset($message) ){"\` |
|        - |   905 | `	"	  $this->message = $message;"\` |
|        - |   906 | `	"   }"\` |
|        - |   907 | `	"   $this->severity = $severity;"\` |
|        - |   908 | `	"   $this->code = $code;"\` |
|        - |   909 | `	"   $this->file = $filename;"\` |
|        - |   910 | `	"   $this->line = $lineno;"\` |
|        - |   911 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   912 | `	"   if( isset($previous) ){"\` |
|        - |   913 | `	"     $this->previous = $previous;"\` |
|        - |   914 | `	"   }"\` |
|        - |   915 | `	"}"\` |
|        - |   916 | `	"public function getSeverity(){"\` |
|        - |   917 | `	"   return $this->severity;"\` |
|        - |   918 | `    "}"\` |
|        - |   919 | `	"}"\` |
|        - |   920 | `	"interface Iterator {"\` |
|        - |   921 | `	"public function current();"\` |
|        - |   922 | `	"public function key();"\` |
|        - |   923 | `	"public function next();"\` |
|        - |   924 | `	"public function rewind();"\` |
|        - |   925 | `	"public function valid();"\` |
|        - |   926 | `	"}"\` |
|        - |   927 | `	"interface IteratorAggregate {"\` |
|        - |   928 | `	"public function getIterator();"\` |
|        - |   929 | `	"}"\` |
|        - |   930 | `	"interface Serializable {"\` |
|        - |   931 | `	"public function serialize();"\` |
|        - |   932 | `	"public function unserialize(string $serialized);"\` |
|        - |   933 | `	"}"\` |
|        - |   934 | `	"/* Directory releated IO */"\` |
|        - |   935 | `	"class Directory {"\` |
|        - |   936 | `	"public $handle = null;"\` |
|        - |   937 | `	"public $path  = null;"\` |
|        - |   938 | `	"public function __construct(string $path)"\` |
|        - |   939 | `	"{"\` |
|        - |   940 | `	"   $this->handle = opendir($path);"\` |
|        - |   941 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   942 | `	"      $this->path = $path;"\` |
|        - |   943 | `	"   }"\` |
|        - |   944 | `	"}"\` |
|        - |   945 | `	"public function __destruct()"\` |
|        - |   946 | `	"{"\` |
|        - |   947 | `	"  if( $this->handle != null ){"\` |
|        - |   948 | `	"       closedir($this->handle);"\` |
|        - |   949 | `	"  }"\` |
|        - |   950 | `	"}"\` |
|        - |   951 | `	"public function read()"\` |
|        - |   952 | `	"{"\` |
|        - |   953 | `	"    return readdir($this->handle);"\` |
|        - |   954 | `	"}"\` |
|        - |   955 | `	"public function rewind()"\` |
|        - |   956 | `	"{"\` |
|        - |   957 | `	"    rewinddir($this->handle);"\` |
|        - |   958 | `	"}"\` |
|        - |   959 | `	"public function close()"\` |
|        - |   960 | `	"{"\` |
|        - |   961 | `	"    closedir($this->handle);"\` |
|        - |   962 | `	"    $this->handle = null;"\` |
|        - |   963 | `	"}"\` |
|        - |   964 | `	"}"\` |
|        - |   965 | `	"class stdClass{"\` |
|        - |   966 | `	"  public $value;"\` |
|        - |   967 | `	" /* Magic methods */"\` |
|        - |   968 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |   969 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |   970 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |   971 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |   972 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |   973 | `	"}"\` |
|        - |   974 | `	"function dir(string $path){"\` |
|        - |   975 | `	"   return new Directory($path);"\` |
|        - |   976 | `	"}"\` |
|        - |   977 | `	"function Dir(string $path){"\` |
|        - |   978 | `	"   return new Directory($path);"\` |
|        - |   979 | `	"}"\` |
|        - |   980 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |   981 | `    "{"\` |
|        - |   982 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |   983 | `	"  $aDir = array();"\` |
|        - |   984 | `	"  $pHandle = opendir($directory);"\` |
|        - |   985 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |   986 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |   987 | `	"      $aDir[] = $pEntry;"\` |
|        - |   988 | `	"   }"\` |
|        - |   989 | `	"  closedir($pHandle);"\` |
|        - |   990 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |   991 | `	"      rsort($aDir);"\` |
|        - |   992 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |   993 | `	"      sort($aDir);"\` |
|        - |   994 | `	"  }"\` |
|        - |   995 | `	"  return $aDir;"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |   998 | `	"/* Open the target directory */"\` |
|        - |   999 | `	"$zDir = dirname($pattern);"\` |
|        - |  1000 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1001 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1002 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1003 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1004 | `	"	return FALSE;"\` |
|        - |  1005 | `	"}"\` |
|        - |  1006 | `	"$pattern = basename($pattern);"\` |
|        - |  1007 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1008 | `	"/* Loop throw available entries */"\` |
|        - |  1009 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1010 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1011 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1012 | `	"	if( $rc ){"\` |
|        - |  1013 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1014 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1015 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1016 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1017 | `	"		  }"\` |
|        - |  1018 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1019 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1020 | `	"		 continue;"\` |
|        - |  1021 | `	"	   }"\` |
|        - |  1022 | `	"	   /* Add the entry */"\` |
|        - |  1023 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1024 | `	"	}"\` |
|        - |  1025 | `	" }"\` |
|        - |  1026 | `	"/* Close the handle */"\` |
|        - |  1027 | `	"closedir($pHandle);"\` |
|        - |  1028 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1029 | `	"  /* Sort the array */"\` |
|        - |  1030 | `	"  sort($pArray);"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1033 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1034 | `	"  $pArray[] = $pattern;"\` |
|        - |  1035 | `	"}"\` |
|        - |  1036 | `	"/* Return the created array */"\` |
|        - |  1037 | `	"return $pArray;"\` |
|        - |  1038 | `   "}"\` |
|        - |  1039 | `   "/* Creates a temporary file */"\` |
|        - |  1040 | `   "function tmpfile(){"\` |
|        - |  1041 | `   "  /* Extract the temp directory */"\` |
|        - |  1042 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1043 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1044 | `   "    /* Use the current dir */"\` |
|        - |  1045 | `   "    $zTempDir = '.';"\` |
|        - |  1046 | `   "  }"\` |
|        - |  1047 | `   "  /* Create the file */"\` |
|        - |  1048 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1049 | `   "  return $pHandle;"\` |
|        - |  1050 | `   "}"\` |
|        - |  1051 | `   "/* Creates a temporary filename */"\` |
|        - |  1052 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1053 | `   "{"\` |
|        - |  1054 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1055 | `   "}"\` |
|        - |  1056 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1057 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1058 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1059 | `   "/* Copy arguments */"\` |
|        - |  1060 | `   "$nArgs = func_num_args();"\` |
|        - |  1061 | `   "$pNew = array();"\` |
|        - |  1062 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1063 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1064 | `    "}"\` |
|        - |  1065 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1066 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1067 | `	"/* Erase */"\` |
|        - |  1068 | `	"array_erase($pArray);"\` |
|        - |  1069 | `	"/* Unshift */"\` |
|        - |  1070 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1071 | `	"return sizeof($pArray);"\` |
|        - |  1072 | `    "}"\` |
|        - |  1073 | `	"function array_merge_recursive(){"\` |
|        - |  1074 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1075 | `    "$arrays = func_get_args();"\` |
|        - |  1076 | `    "$narrays = count($arrays);"\` |
|        - |  1077 | `    "$ret = array();"\` |
|        - |  1078 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1079 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1080 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1081 | `	 " }"\` |
|        - |  1082 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1083 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1084 | `     "  if( $keyIsInt ) {"\` |
|        - |  1085 | `     "   $ret[] = $value;"\` |
|        - |  1086 | `     "  } else {"\` |
|        - |  1087 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1088 | `     "    $cur = $ret[$key];"\` |
|        - |  1089 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1090 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1091 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1092 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1093 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1094 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1095 | `     "    } else {"\` |
|        - |  1096 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1097 | `     "    }"\` |
|        - |  1098 | `     "   } else {"\` |
|        - |  1099 | `     "    $ret[$key] = $value;"\` |
|        - |  1100 | `     "   }"\` |
|        - |  1101 | `     "  }"\` |
|        - |  1102 | `     " }"\` |
|        - |  1103 | `	 " }"\` |
|        - |  1104 | `	 " return $ret;"\` |
|        - |  1105 | `    "}"\` |
|        - |  1106 | `	"function max(){"\` |
|        - |  1107 | `    "  $pArgs = func_get_args();"\` |
|        - |  1108 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1109 | `	"  return null;"\` |
|        - |  1110 | `    " }"\` |
|        - |  1111 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1112 | `    " $pArg = $pArgs[0];"\` |
|        - |  1113 | `	" if( !is_array($pArg) ){"\` |
|        - |  1114 | `	"   return $pArg; "\` |
|        - |  1115 | `	" }"\` |
|        - |  1116 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1117 | `	"   return null;"\` |
|        - |  1118 | `	" }"\` |
|        - |  1119 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1120 | `	" reset($pArg);"\` |
|        - |  1121 | `	" $max = current($pArg);"\` |
|        - |  1122 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1123 | `	"   if( $val > $max ){"\` |
|        - |  1124 | `	"     $max = $val;"\` |
|        - |  1125 | `    " }"\` |
|        - |  1126 | `	" }"\` |
|        - |  1127 | `	" return $max;"\` |
|        - |  1128 | `    " }"\` |
|        - |  1129 | `    " $max = $pArgs[0];"\` |
|        - |  1130 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1131 | `    " $val = $pArgs[$i];"\` |
|        - |  1132 | `	"if( $val > $max ){"\` |
|        - |  1133 | `	" $max = $val;"\` |
|        - |  1134 | `	"}"\` |
|        - |  1135 | `    " }"\` |
|        - |  1136 | `	" return $max;"\` |
|        - |  1137 | `    "}"\` |
|        - |  1138 | `	"function min(){"\` |
|        - |  1139 | `    "  $pArgs = func_get_args();"\` |
|        - |  1140 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1141 | `	"  return null;"\` |
|        - |  1142 | `    " }"\` |
|        - |  1143 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1144 | `    " $pArg = $pArgs[0];"\` |
|        - |  1145 | `	" if( !is_array($pArg) ){"\` |
|        - |  1146 | `	"   return $pArg; "\` |
|        - |  1147 | `	" }"\` |
|        - |  1148 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1149 | `	"   return null;"\` |
|        - |  1150 | `	" }"\` |
|        - |  1151 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1152 | `	" reset($pArg);"\` |
|        - |  1153 | `	" $min = current($pArg);"\` |
|        - |  1154 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1155 | `	"   if( $val < $min ){"\` |
|        - |  1156 | `	"     $min = $val;"\` |
|        - |  1157 | `    " }"\` |
|        - |  1158 | `	" }"\` |
|        - |  1159 | `	" return $min;"\` |
|        - |  1160 | `    " }"\` |
|        - |  1161 | `    " $min = $pArgs[0];"\` |
|        - |  1162 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1163 | `    " $val = $pArgs[$i];"\` |
|        - |  1164 | `	"if( $val < $min ){"\` |
|        - |  1165 | `	" $min = $val;"\` |
|        - |  1166 | `	" }"\` |
|        - |  1167 | `    " }"\` |
|        - |  1168 | `	" return $min;"\` |
|        - |  1169 | `	"}"\` |
|        - |  1170 | `	"function fileowner(string $file){"\` |
|        - |  1171 | `    " $a = stat($file);"\` |
|        - |  1172 | `	" if( !is_array($a) ){"\` |
|        - |  1173 | `	"	return false;"\` |
|        - |  1174 | `	" }"\` |
|        - |  1175 | `	" return $a['uid'];"\` |
|        - |  1176 | `    "}"\` |
|        - |  1177 | `    "function filegroup(string $file){"\` |
|        - |  1178 | `	" $a = stat($file);"\` |
|        - |  1179 | `	" if( !is_array($a) ){"\` |
|        - |  1180 | `	"	return false;"\` |
|        - |  1181 | `	" }"\` |
|        - |  1182 | `	" return $a['gid'];"\` |
|        - |  1183 | `    "}"\` |
|        - |  1184 | `	 "function fileinode(string $file){"\` |
|        - |  1185 | `	" $a = stat($file);"\` |
|        - |  1186 | `	" if( !is_array($a) ){"\` |
|        - |  1187 | `	"	return false;"\` |
|        - |  1188 | `	" }"\` |
|        - |  1189 | `	" return $a['ino'];"\` |
|        - |  1190 | `    "}"` |
|        - |  1191 |  |
|        - |  1192 | `/*` |
|        - |  1193 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1194 | ` * start compiling the target PHP program.` |
|        - |  1195 | ` */` |
|     2506 |  1196 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1197 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1198 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1199 | `	 )` |
|        2 |  1200 |  |
|        - |  1201 | `	SyString sBuiltin;` |
|        - |  1202 | `	ph7_value *pObj;` |
|        - |  1203 | `	sxi32 rc;` |
|        - |  1204 | `	/* Zero the structure */` |
|     2508 |  1205 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1206 | `	/* Initialize VM fields */` |
|     2508 |  1207 | `	pVm->pEngine = &(*pEngine);` |
|     2508 |  1208 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1209 | `	/* Instructions containers */` |
|     2508 |  1210 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2508 |  1211 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2508 |  1212 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1213 | `	/* Object containers */` |
|     2508 |  1214 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2508 |  1215 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1216 | `	/* Virtual machine internal containers */` |
|     2508 |  1217 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2508 |  1218 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2508 |  1219 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2508 |  1220 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2508 |  1221 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2508 |  1222 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2508 |  1223 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2508 |  1224 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2508 |  1225 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2508 |  1226 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2508 |  1227 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2508 |  1228 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2508 |  1229 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2508 |  1230 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2508 |  1231 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2508 |  1232 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2508 |  1233 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2508 |  1234 | `	pVm->pPendingException = 0;` |
|        - |  1235 | `	/* Configuration containers */` |
|     2508 |  1236 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2508 |  1237 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2508 |  1238 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2508 |  1239 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2508 |  1240 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1241 | `	/* Error callbacks containers */` |
|     2508 |  1242 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2508 |  1243 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2508 |  1244 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2508 |  1245 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2508 |  1246 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1247 | `	/* Set a default recursion limit */` |
|        - |  1248 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2508 |  1249 | `	pVm->nMaxDepth = 32;` |
|        - |  1250 | `#else` |
|        - |  1251 | `	pVm->nMaxDepth = 16;` |
|        - |  1252 | `#endif` |
|        - |  1253 | `	/* Default assertion flags */` |
|     2508 |  1254 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1255 | `	/* JSON return status */` |
|     2508 |  1256 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1257 | `	/* PRNG context */` |
|     2508 |  1258 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1259 | `	/* Install the null constant */` |
|     2508 |  1260 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2508 |  1261 | `	if( pObj == 0 ){` |
|      ! 0 |  1262 | `		rc = SXERR_MEM;` |
|      ! 0 |  1263 | `		goto Err;` |
|        - |  1264 | `	}` |
|     2508 |  1265 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1266 | `	/* Install the boolean TRUE constant */` |
|     2508 |  1267 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2508 |  1268 | `	if( pObj == 0 ){` |
|      ! 0 |  1269 | `		rc = SXERR_MEM;` |
|      ! 0 |  1270 | `		goto Err;` |
|        - |  1271 | `	}` |
|     2508 |  1272 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1273 | `	/* Install the boolean FALSE constant */` |
|     2508 |  1274 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2508 |  1275 | `	if( pObj == 0 ){` |
|      ! 0 |  1276 | `		rc = SXERR_MEM;` |
|      ! 0 |  1277 | `		goto Err;` |
|        - |  1278 | `	}` |
|     2508 |  1279 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1280 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1281 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1282 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2508 |  1283 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2508 |  1284 | `	if( pObj == 0 ){` |
|      ! 0 |  1285 | `		rc = SXERR_MEM;` |
|      ! 0 |  1286 | `		goto Err;` |
|        - |  1287 | `	}` |
|     2508 |  1288 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1289 | `	/* Create the global frame */` |
|     2508 |  1290 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2508 |  1291 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1292 | `		goto Err;` |
|        - |  1293 | `	}` |
|        - |  1294 | `	/* Initialize the code generator */` |
|     2508 |  1295 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2508 |  1296 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1297 | `		goto Err;` |
|        - |  1298 | `	}` |
|        - |  1299 | `	/* VM correctly initialized,set the magic number */` |
|     2508 |  1300 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2508 |  1301 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1302 | `	/* Compile the built-in library */` |
|     2508 |  1303 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1304 | `	/* Reset the code generator */` |
|     2508 |  1305 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2508 |  1306 | `	return SXRET_OK;` |
|      ! 0 |  1307 | `Err:` |
|      ! 0 |  1308 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1309 | `	return rc;` |
|     1255 |  1310 |  |
|        - |  1311 | `/*` |
|        - |  1312 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1313 | ` * routine which store the output in an internal blob.` |
|        - |  1314 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1315 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1316 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1317 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1318 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1319 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1320 | ` * to finish executing and extracting the output.` |
|        - |  1321 | ` */` |
|      ! 0 |  1322 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1323 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1324 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1325 | `	void *pUserData     /* User private data */` |
|        - |  1326 | `	)` |
|      ! 0 |  1327 |  |
|        - |  1328 | `	 sxi32 rc;` |
|        - |  1329 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1330 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1331 | `	 return rc;` |
|      ! 0 |  1332 |  |
|        - |  1333 | `#define VM_STACK_GUARD 16` |
|        - |  1334 | `/*` |
|        - |  1335 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1336 | ` * our compiled PHP program.` |
|        - |  1337 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1338 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1339 | ` */` |
|    30884 |  1340 | `static ph7_value * VmNewOperandStack(` |
|        - |  1341 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1342 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1343 | `	)` |
|        2 |  1344 |  |
|        - |  1345 | `	ph7_value *pStack;` |
|        - |  1346 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1347 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1348 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1349 | `  ** on the maximum stack depth required.` |
|        - |  1350 | `  **` |
|        - |  1351 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1352 | `  */` |
|    30886 |  1353 | `	nInstr += VM_STACK_GUARD;` |
|    30886 |  1354 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    30886 |  1355 | `	if( pStack == 0 ){` |
|      ! 0 |  1356 | `		return 0;` |
|        - |  1357 | `	}` |
|        - |  1358 | `	/* Initialize the operand stack */` |
|  1953612 |  1359 | `	while( nInstr > 0 ){` |
|  1922728 |  1360 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1922728 |  1361 | `		--nInstr;` |
|        2 |  1362 | `	}` |
|        - |  1363 | `	/* Ready for bytecode execution */` |
|    30886 |  1364 | `	return pStack;` |
|    15444 |  1365 |  |
|        - |  1366 | `/* Forward declaration */` |
|        - |  1367 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1368 | `/*` |
|        - |  1369 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1370 | ` * This routine gets called by the PH7 engine after` |
|        - |  1371 | ` * successful compilation of the target PHP program.` |
|        - |  1372 | ` */` |
|     2248 |  1373 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1374 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1375 | `	)` |
|        2 |  1376 |  |
|        - |  1377 | `	SyHashEntry *pEntry;` |
|        - |  1378 | `	sxi32 rc;` |
|     2250 |  1379 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1380 | `		/* Initialize your VM first */` |
|      ! 0 |  1381 | `		return SXERR_CORRUPT;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* Mark the VM ready for byte-code execution */` |
|     2250 |  1384 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1385 | `	/* Release the code generator now we have compiled our program */` |
|     2250 |  1386 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1387 | `	/* Emit the DONE instruction */` |
|     2250 |  1388 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2250 |  1389 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1390 | `		return SXERR_MEM;` |
|        - |  1391 | `	}` |
|        - |  1392 | `	/* Script return value */` |
|     2250 |  1393 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1394 | `	/* Allocate a new operand stack */` |
|     2250 |  1395 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2250 |  1396 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1397 | `		return SXERR_MEM;` |
|        - |  1398 | `	}` |
|        - |  1399 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1400 | `	 * private data. */` |
|     2250 |  1401 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2250 |  1402 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1403 | `	/* Allocate the reference table */` |
|     2250 |  1404 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2250 |  1405 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2250 |  1406 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1407 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1408 | `		return SXERR_MEM;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Zero the reference table */` |
|     2250 |  1411 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1412 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2250 |  1413 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2250 |  1414 | `	if( rc != SXRET_OK ){` |
|        - |  1415 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1416 | `		return rc;` |
|        - |  1417 | `	}` |
|        - |  1418 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2250 |  1419 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2250 |  1420 | `	if( rc != SXRET_OK ){` |
|        - |  1421 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1422 | `		return rc;` |
|        - |  1423 | `	}` |
|        - |  1424 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2250 |  1425 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1426 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2250 |  1427 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1428 | `	/* Initialize and install static and constants class attributes */` |
|     2250 |  1429 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    29376 |  1430 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    27128 |  1431 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    27128 |  1432 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1433 | `			return rc;` |
|        - |  1434 | `		}` |
|        2 |  1435 | `	}` |
|        - |  1436 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2250 |  1437 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1438 | `	/* VM is ready for bytecode execution */` |
|     2250 |  1439 | `	return SXRET_OK;` |
|     1126 |  1440 |  |
|        - |  1441 | `/*` |
|        - |  1442 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1443 | ` */` |
|      ! 0 |  1444 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1445 |  |
|      ! 0 |  1446 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1447 | `		return SXERR_CORRUPT;` |
|        - |  1448 | `	}` |
|        - |  1449 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1450 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1451 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1452 | `	/* Set the ready flag */` |
|      ! 0 |  1453 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1454 | `	return SXRET_OK;` |
|      ! 0 |  1455 |  |
|        - |  1456 | `/*` |
|        - |  1457 | ` * Release a Virtual Machine.` |
|        - |  1458 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1459 | ` */` |
|     2240 |  1460 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1461 |  |
|        - |  1462 | `	/* Set the stale magic number */` |
|     2242 |  1463 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1464 | `	/* Release the private memory subsystem */` |
|     2242 |  1465 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2242 |  1466 | `	return SXRET_OK;` |
|        2 |  1467 |  |
|        - |  1468 | `/*` |
|        - |  1469 | ` * Initialize a foreign function call context.` |
|        - |  1470 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1471 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1472 | ` * functions.` |
|        - |  1473 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1474 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1475 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1476 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1477 | ` */` |
|   546618 |  1478 | `static sxi32 VmInitCallContext(` |
|        - |  1479 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1480 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1481 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1482 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1483 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1484 | `	)` |
|        2 |  1485 |  |
|   546620 |  1486 | `	pOut->pFunc = pFunc;` |
|   546620 |  1487 | `	pOut->pVm   = pVm;` |
|   546620 |  1488 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   546620 |  1489 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1490 | `	/* Assume a null return value */` |
|   546620 |  1491 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   546620 |  1492 | `	pOut->pRet = pRet;` |
|   546620 |  1493 | `	pOut->iFlags = iFlags;` |
|   546620 |  1494 | `	return SXRET_OK;` |
|        2 |  1495 |  |
|        - |  1496 | `/*` |
|        - |  1497 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1498 | ` * left behind.` |
|        - |  1499 | ` */` |
|   546618 |  1500 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1501 |  |
|        - |  1502 | `	sxu32 n;` |
|   546620 |  1503 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6674 |  1504 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19036 |  1505 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12364 |  1506 | `			if( apObj[n] == 0 ){` |
|        - |  1507 | `				/* Already released */` |
|      250 |  1508 | `				continue;` |
|        - |  1509 | `			}` |
|    12116 |  1510 | `			PH7_MemObjRelease(apObj[n]);` |
|    12116 |  1511 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6059 |  1512 | `		}` |
|     6674 |  1513 | `		SySetRelease(&pCtx->sVar);` |
|     3336 |  1514 | `	}` |
|   546620 |  1515 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1516 | `		ph7_aux_data *aAux;` |
|        - |  1517 | `		void *pChunk;` |
|        - |  1518 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1519 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1520 | `		 */` |
|        9 |  1521 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1522 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1523 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1524 | `			/* Release the chunk */` |
|       25 |  1525 | `			if( pChunk ){` |
|       25 |  1526 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1527 | `			}` |
|       13 |  1528 | `		}` |
|        9 |  1529 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1530 | `	}` |
|   546620 |  1531 |  |
|        - |  1532 | `/*` |
|        - |  1533 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1534 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1535 | ` */` |
|      248 |  1536 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1537 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1538 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1539 | `	)` |
|        2 |  1540 |  |
|      250 |  1541 | `	if( pValue == 0 ){` |
|        - |  1542 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1543 | `		return;` |
|        - |  1544 | `	}` |
|      250 |  1545 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1546 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1547 | `		sxu32 n;` |
|      936 |  1548 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1549 | `			if( apObj[n] == pValue ){` |
|      250 |  1550 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1551 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1552 | `				/* Mark as released */` |
|      250 |  1553 | `				apObj[n] = 0;` |
|      250 |  1554 | `				break;` |
|        - |  1555 | `			}` |
|      345 |  1556 | `		}` |
|      124 |  1557 | `	}` |
|      126 |  1558 |  |
|        - |  1559 | `/*` |
|        - |  1560 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1561 | ` */` |
|  3230374 |  1562 | `static void VmPopOperand(` |
|        - |  1563 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1564 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1565 | `	)` |
|        2 |  1566 |  |
|  3230376 |  1567 | `	ph7_value *pTos = *ppTos;` |
|  6860618 |  1568 | `	while( nPop > 0 ){` |
|  3630244 |  1569 | `		PH7_MemObjRelease(pTos);` |
|  3630244 |  1570 | `		pTos--;` |
|  3630244 |  1571 | `		nPop--;` |
|        2 |  1572 | `	}` |
|        - |  1573 | `	/* Top of the stack */` |
|  3230376 |  1574 | `	*ppTos = pTos;` |
|  3230376 |  1575 |  |
|        - |  1576 | `/*` |
|        - |  1577 | ` * Reserve a memory object.` |
|        - |  1578 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1579 | ` */` |
|  2996604 |  1580 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1581 |  |
|  2996606 |  1582 | `	ph7_value *pObj = 0;` |
|        - |  1583 | `	VmSlot *pSlot;` |
|        - |  1584 | `	sxu32 nIdx;` |
|        - |  1585 | `	/* Check for a free slot */` |
|  2996606 |  1586 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2996606 |  1587 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2996606 |  1588 | `	if( pSlot ){` |
|   856472 |  1589 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   856472 |  1590 | `		nIdx = pSlot->nIdx;` |
|   428235 |  1591 | `	}` |
|  2996606 |  1592 | `	if( pObj == 0 ){` |
|        - |  1593 | `		/* Reserve a new memory object */` |
|  2140136 |  1594 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2140136 |  1595 | `		if( pObj == 0 ){` |
|      ! 0 |  1596 | `			return 0;` |
|        - |  1597 | `		}` |
|  1070067 |  1598 | `	}` |
|        - |  1599 | `	/* Set a null default value */` |
|  2996606 |  1600 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2996606 |  1601 | `	pObj->nIdx = nIdx;` |
|  2996606 |  1602 | `	return pObj;` |
|  1498304 |  1603 |  |
|        - |  1604 | `/*` |
|        - |  1605 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1606 | ` */` |
|    28188 |  1607 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1608 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1609 | `	const char *zKey,  /* Entry key */` |
|        - |  1610 | `	sxu32 nByte,       /* Key length */` |
|        - |  1611 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1612 | `	)` |
|        2 |  1613 |  |
|        - |  1614 | `	ph7_value sKey;` |
|        - |  1615 | `	sxi32 rc;` |
|    28190 |  1616 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    28190 |  1617 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1618 | `	/* Perform the insertion */` |
|    28190 |  1619 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    28190 |  1620 | `	PH7_MemObjRelease(&sKey);` |
|    28190 |  1621 | `	return rc;` |
|        2 |  1622 |  |
|        - |  1623 | `/*` |
|        - |  1624 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1625 | ` * Return a pointer to the variable value on success.` |
|        - |  1626 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1627 | ` */` |
|  3029274 |  1628 | `static ph7_value * VmExtractMemObj(` |
|        - |  1629 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1630 | `	const SyString *pName, /* Variable name */` |
|        - |  1631 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1632 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1633 | `	)` |
|        2 |  1634 |  |
|  3029276 |  1635 | `	int bNullify = FALSE;` |
|        - |  1636 | `	SyHashEntry *pEntry;` |
|        - |  1637 | `	VmFrame *pFrame;` |
|        - |  1638 | `	ph7_value *pObj;` |
|        - |  1639 | `	sxu32 nIdx;` |
|        - |  1640 | `	sxi32 rc;` |
|        - |  1641 | `	/* Point to the top active frame */` |
|  3029276 |  1642 | `	pFrame = pVm->pFrame;` |
|  3029294 |  1643 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1644 | `		/* Safely ignore the exception frame */` |
|       20 |  1645 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        2 |  1646 | `	}` |
|        - |  1647 | `	/* Perform the lookup */` |
|  3029276 |  1648 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1649 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1650 | `		pName = &sAnnon;` |
|        - |  1651 | `		/* Always nullify the object */` |
|      ! 0 |  1652 | `		bNullify = TRUE;` |
|      ! 0 |  1653 | `		bDup = FALSE;` |
|      ! 0 |  1654 | `	}` |
|        - |  1655 | `	/* Check the superglobals table first */` |
|  3029276 |  1656 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3029276 |  1657 | `	if( pEntry == 0 ){` |
|        - |  1658 | `		/* Query the top active frame */` |
|  3029240 |  1659 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3029240 |  1660 | `		if( pEntry == 0 ){` |
|    81974 |  1661 | `			char *zName = (char *)pName->zString;` |
|        - |  1662 | `			VmSlot sLocal;` |
|    81974 |  1663 | `			if( !bCreate ){` |
|        - |  1664 | `				/* Do not create the variable,return NULL instead */` |
|      632 |  1665 | `				return 0;` |
|        - |  1666 | `			}` |
|        - |  1667 | `			/* No such variable,automatically create a new one and install` |
|        - |  1668 | `			 * it in the current frame.` |
|        - |  1669 | `			 */` |
|    81344 |  1670 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    81344 |  1671 | `			if( pObj == 0 ){` |
|      ! 0 |  1672 | `				return 0;` |
|        - |  1673 | `			}` |
|    81344 |  1674 | `			nIdx = pObj->nIdx;` |
|    81344 |  1675 | `			if( bDup ){` |
|        - |  1676 | `				/* Duplicate name */` |
|      164 |  1677 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1678 | `				if( zName == 0 ){` |
|      ! 0 |  1679 | `					return 0;` |
|        - |  1680 | `				}` |
|       81 |  1681 | `			}` |
|        - |  1682 | `			/* Link to the top active VM frame */` |
|    81344 |  1683 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    81344 |  1684 | `			if( rc != SXRET_OK ){` |
|        - |  1685 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1686 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1687 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1688 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1689 | `				return 0;` |
|        - |  1690 | `			}` |
|    81344 |  1691 | `			if( pFrame->pParent != 0 ){` |
|        - |  1692 | `				/* Local variable */` |
|    75660 |  1693 | `				sLocal.nIdx = nIdx;` |
|    75660 |  1694 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    37831 |  1695 | `			}else{` |
|        - |  1696 | `				/* Register in the $GLOBALS array */` |
|     5686 |  1697 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1698 | `			}` |
|        - |  1699 | `			/* Install in the reference table */` |
|    81344 |  1700 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1701 | `			/* Save object index */` |
|    81344 |  1702 | `			pObj->nIdx = nIdx;` |
|    40673 |  1703 | `		}else{` |
|        - |  1704 | `			/* Extract variable contents */` |
|  2947268 |  1705 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2947268 |  1706 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2947268 |  1707 | `			if( bNullify && pObj ){` |
|      ! 0 |  1708 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1709 | `			}` |
|        - |  1710 | `		}` |
|  1514416 |  1711 | `	}else{` |
|        - |  1712 | `		/* Superglobal */` |
|       38 |  1713 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1714 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1715 | `	}` |
|  3028646 |  1716 | `	return pObj;` |
|  1514749 |  1717 |  |
|        - |  1718 | `/*` |
|        - |  1719 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1720 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1721 | ` */` |
|     2274 |  1722 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1723 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1724 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1725 | `	sxu32 nByte        /* zName length */` |
|        - |  1726 | `	)` |
|        2 |  1727 |  |
|        - |  1728 | `	SyHashEntry *pEntry;` |
|        - |  1729 | `	ph7_value *pValue;` |
|        - |  1730 | `	sxu32 nIdx;` |
|        - |  1731 | `	/* Query the superglobal table */` |
|     2276 |  1732 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2276 |  1733 | `	if( pEntry == 0 ){` |
|        - |  1734 | `		/* No such entry */` |
|      ! 0 |  1735 | `		return 0;` |
|        - |  1736 | `	}` |
|        - |  1737 | `	/* Extract the superglobal index in the global object pool */` |
|     2276 |  1738 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1739 | `	/* Extract the variable value  */` |
|     2276 |  1740 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2276 |  1741 | `	return pValue;` |
|     1139 |  1742 |  |
|        - |  1743 | `/*` |
|        - |  1744 | ` * Perform a raw hashmap insertion.` |
|        - |  1745 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1746 | ` */` |
|     2272 |  1747 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1748 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1749 | `	const char *zKey,   /* Entry key */` |
|        - |  1750 | `	int nKeylen,        /* zKey length*/` |
|        - |  1751 | `	const char *zData,  /* Entry data */` |
|        - |  1752 | `	int nLen            /* zData length */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|        - |  1755 | `	ph7_value sKey,sValue;` |
|        - |  1756 | `	sxi32 rc;` |
|     2274 |  1757 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2274 |  1758 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2274 |  1759 | `	if( zKey ){` |
|     2252 |  1760 | `		if( nKeylen < 0 ){` |
|     2252 |  1761 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1125 |  1762 | `		}` |
|     2252 |  1763 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1125 |  1764 | `	}` |
|     2274 |  1765 | `	if( zData ){` |
|     2274 |  1766 | `		if( nLen < 0 ){` |
|        - |  1767 | `			/* Compute length automatically */` |
|      ! 0 |  1768 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1769 | `		}` |
|     2274 |  1770 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1136 |  1771 | `	}` |
|        - |  1772 | `	/* Perform the insertion */` |
|     2274 |  1773 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2274 |  1774 | `	PH7_MemObjRelease(&sKey);` |
|     2274 |  1775 | `	PH7_MemObjRelease(&sValue);` |
|     2274 |  1776 | `	return rc;` |
|        2 |  1777 |  |
|        - |  1778 | `/*` |
|        - |  1779 | ` * Configure a working virtual machine instance.` |
|        - |  1780 | ` *` |
|        - |  1781 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1782 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1783 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1784 | ` * The second argument to this function is an integer configuration option` |
|        - |  1785 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1786 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1787 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1788 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1789 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1790 | ` */` |
|    35992 |  1791 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1792 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1793 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1794 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1795 | `	)` |
|        2 |  1796 |  |
|    35994 |  1797 | `	sxi32 rc = SXRET_OK;` |
|    35994 |  1798 | `	switch(nOp){` |
|     1124 |  1799 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2250 |  1800 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2250 |  1801 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1802 | `		/* VM output consumer callback */` |
|        - |  1803 | `#ifdef UNTRUST` |
|        - |  1804 | `		if( xConsumer == 0 ){` |
|        - |  1805 | `			rc = SXERR_CORRUPT;` |
|        - |  1806 | `			break;` |
|        - |  1807 | `		}` |
|        - |  1808 | `#endif` |
|        - |  1809 | `		/* Install the output consumer */` |
|     2250 |  1810 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2250 |  1811 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2250 |  1812 | `		break;` |
|        - |  1813 | `							   }` |
|     1124 |  1814 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1815 | `		/* Import path */` |
|        - |  1816 | `		  const char *zPath;` |
|        - |  1817 | `		  SyString sPath;` |
|     2250 |  1818 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1819 | `#if defined(UNTRUST)` |
|        - |  1820 | `		  if( zPath == 0 ){` |
|        - |  1821 | `			  rc = SXERR_EMPTY;` |
|        - |  1822 | `			  break;` |
|        - |  1823 | `		  }` |
|        - |  1824 | `#endif` |
|     2250 |  1825 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1826 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1827 | `#ifdef __WINNT__` |
|        2 |  1828 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1829 | `#endif` |
|     4498 |  1830 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1831 | `		  /* Remove leading and trailing white spaces */` |
|     2250 |  1832 | `		  SyStringFullTrim(&sPath);` |
|     2250 |  1833 | `		  if( sPath.nByte > 0 ){` |
|        - |  1834 | `			  /* Store the path in the corresponding conatiner */` |
|     2250 |  1835 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1124 |  1836 | `		  }` |
|     2250 |  1837 | `		  break;` |
|        - |  1838 | `									 }` |
|     1124 |  1839 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1840 | `		/* Run-Time Error report */` |
|     2250 |  1841 | `		pVm->bErrReport = 1;` |
|     2250 |  1842 | `		break;` |
|      ! 0 |  1843 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1844 | `		/* Recursion depth */` |
|      ! 0 |  1845 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1846 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1847 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1848 | `		}` |
|      ! 0 |  1849 | `		break;` |
|        - |  1850 | `									   }` |
|      ! 0 |  1851 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1852 | `		/* VM output length in bytes */` |
|      ! 0 |  1853 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1854 | `#ifdef UNTRUST` |
|        - |  1855 | `		if( pOut == 0 ){` |
|        - |  1856 | `			rc = SXERR_CORRUPT;` |
|        - |  1857 | `			break;` |
|        - |  1858 | `		}` |
|        - |  1859 | `#endif` |
|      ! 0 |  1860 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1861 | `		break;` |
|        - |  1862 | `							   }` |
|        - |  1863 |  |
|    11240 |  1864 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1865 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1866 | `		/* Create a new superglobal/global variable */` |
|    22482 |  1867 | `		const char *zName = va_arg(ap,const char *);` |
|    22482 |  1868 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1869 | `		SyHashEntry *pEntry;` |
|        - |  1870 | `		ph7_value *pObj;` |
|        - |  1871 | `		sxu32 nByte;` |
|        - |  1872 | `		sxu32 nIdx;` |
|        - |  1873 | `#ifdef UNTRUST` |
|        - |  1874 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1875 | `			rc = SXERR_CORRUPT;` |
|        - |  1876 | `			break;` |
|        - |  1877 | `		}` |
|        - |  1878 | `#endif` |
|    22482 |  1879 | `		nByte = SyStrlen(zName);` |
|    22482 |  1880 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1881 | `			/* Check if the superglobal is already installed */` |
|    22482 |  1882 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11242 |  1883 | `		}else{` |
|        - |  1884 | `			/* Query the top active VM frame */` |
|      ! 0 |  1885 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1886 | `		}` |
|    22482 |  1887 | `		if( pEntry ){` |
|        - |  1888 | `			/* Variable already installed */` |
|      ! 0 |  1889 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1890 | `			/* Extract contents */` |
|      ! 0 |  1891 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1892 | `			if( pObj ){` |
|        - |  1893 | `				/* Overwrite old contents */` |
|      ! 0 |  1894 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1895 | `			}` |
|      ! 0 |  1896 | `		}else{` |
|        - |  1897 | `			/* Install a new variable */` |
|    22482 |  1898 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    22482 |  1899 | `			if( pObj == 0 ){` |
|      ! 0 |  1900 | `				rc = SXERR_MEM;` |
|      ! 0 |  1901 | `				break;` |
|        - |  1902 | `			}` |
|    22482 |  1903 | `			nIdx = pObj->nIdx;` |
|        - |  1904 | `			/* Copy value */` |
|    22482 |  1905 | `			PH7_MemObjStore(pValue,pObj);` |
|    22482 |  1906 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1907 | `				/* Install the superglobal */` |
|    22482 |  1908 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11242 |  1909 | `			}else{` |
|        - |  1910 | `				/* Install in the current frame */` |
|      ! 0 |  1911 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1912 | `			}` |
|    22482 |  1913 | `			if( rc == SXRET_OK ){` |
|        - |  1914 | `				SyHashEntry *pRef;` |
|    22482 |  1915 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    22482 |  1916 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11242 |  1917 | `				}else{` |
|      ! 0 |  1918 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1919 | `				}` |
|        - |  1920 | `				/* Install in the reference table */` |
|    22482 |  1921 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    22482 |  1922 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1923 | `					/* Register in the $GLOBALS array */` |
|    22482 |  1924 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11240 |  1925 | `				}` |
|    11240 |  1926 | `			}` |
|        - |  1927 | `		}` |
|    22482 |  1928 | `		break;` |
|        - |  1929 | `									}` |
|     1125 |  1930 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1931 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1932 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1933 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1934 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1935 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1936 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2252 |  1937 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2252 |  1938 | `		const char *zValue = va_arg(ap,const char *);` |
|     2252 |  1939 | `		int nLen = va_arg(ap,int);` |
|        - |  1940 | `		ph7_hashmap *pMap;` |
|        - |  1941 | `		ph7_value *pValue;` |
|     2252 |  1942 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1943 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1944 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2251 |  1945 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1946 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1947 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2250 |  1948 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1949 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1950 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2250 |  1951 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1952 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1953 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2250 |  1954 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1955 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1956 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2250 |  1957 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1958 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1959 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1960 | `		}else{` |
|        - |  1961 | `			/* Extract the $_SERVER superglobal */` |
|     2250 |  1962 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1963 | `		}` |
|     2252 |  1964 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1965 | `			/* No such entry */` |
|      ! 0 |  1966 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1967 | `			break;` |
|        - |  1968 | `		}` |
|        - |  1969 | `		/* Point to the hashmap */` |
|     2252 |  1970 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1971 | `		/* Perform the insertion */` |
|     2252 |  1972 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2252 |  1973 | `		break;` |
|        - |  1974 | `								   }` |
|       11 |  1975 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  1976 | `		/* Script arguments */` |
|       24 |  1977 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  1978 | `		ph7_hashmap *pMap;` |
|        - |  1979 | `		ph7_value *pValue;` |
|        - |  1980 | `		sxu32 n;` |
|       24 |  1981 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  1982 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  1983 | `			break;` |
|        - |  1984 | `		}` |
|        - |  1985 | `		/* Extract the $argv array */` |
|       24 |  1986 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  1987 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1988 | `			/* No such entry */` |
|      ! 0 |  1989 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1990 | `			break;` |
|        - |  1991 | `		}` |
|        - |  1992 | `		/* Point to the hashmap */` |
|       24 |  1993 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1994 | `		/* Perform the insertion */` |
|       24 |  1995 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  1996 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  1997 | `		if( rc == SXRET_OK ){` |
|       24 |  1998 | `			if( pMap->nEntry > 1 ){` |
|        - |  1999 | `				/* Append space separator first */` |
|       18 |  2000 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2001 | `			}` |
|       24 |  2002 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2003 | `		}` |
|       24 |  2004 | `		break;` |
|        - |  2005 | `								  }` |
|      ! 0 |  2006 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2007 | `		/* error_log() consumer */` |
|      ! 0 |  2008 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2009 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2010 | `		break;` |
|        - |  2011 | `										}` |
|      ! 0 |  2012 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2013 | `		/* Script return value */` |
|      ! 0 |  2014 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2015 | `#ifdef UNTRUST` |
|        - |  2016 | `		if( ppValue == 0 ){` |
|        - |  2017 | `			rc = SXERR_CORRUPT;` |
|        - |  2018 | `			break;` |
|        - |  2019 | `		}` |
|        - |  2020 | `#endif` |
|      ! 0 |  2021 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2022 | `		break;` |
|        - |  2023 | `								   }` |
|     2248 |  2024 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2025 | `		/* Register an IO stream device */` |
|     4498 |  2026 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2027 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6744 |  2028 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4498 |  2029 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2030 | `				/* Invalid stream */` |
|      ! 0 |  2031 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2032 | `				break;` |
|        - |  2033 | `		}` |
|     4498 |  2034 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2035 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2250 |  2036 | `			pVm->pDefStream = pStream;` |
|     1124 |  2037 | `		}` |
|        - |  2038 | `		/* Insert in the appropriate container */` |
|     4498 |  2039 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4498 |  2040 | `		break;` |
|        - |  2041 | `								  }` |
|      ! 0 |  2042 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2043 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2044 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2045 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2046 | `#ifdef UNTRUST` |
|        - |  2047 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2048 | `			rc = SXERR_CORRUPT;` |
|        - |  2049 | `			break;` |
|        - |  2050 | `		}` |
|        - |  2051 | `#endif` |
|      ! 0 |  2052 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2053 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2054 | `		break;` |
|        - |  2055 | `									   }` |
|      ! 0 |  2056 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2057 | `		/* Raw HTTP request*/` |
|      ! 0 |  2058 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2059 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2060 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2061 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2062 | `			break;` |
|        - |  2063 | `		}` |
|      ! 0 |  2064 | `		if( nByte < 0 ){` |
|        - |  2065 | `			/* Compute length automatically */` |
|      ! 0 |  2066 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2067 | `		}` |
|        - |  2068 | `		/* Process the request */` |
|      ! 0 |  2069 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2070 | `		break;` |
|        - |  2071 | `									}` |
|      ! 0 |  2072 | `	default:` |
|        - |  2073 | `		/* Unknown configuration option */` |
|      ! 0 |  2074 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2075 | `		break;` |
|        - |  2076 | `	}` |
|    35994 |  2077 | `	return rc;` |
|        2 |  2078 |  |
|        - |  2079 | `/* Forward declaration */` |
|        - |  2080 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2081 | `/*` |
|        - |  2082 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2083 | ` * format.` |
|        - |  2084 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2085 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2086 | ` * (STDOUT).` |
|        - |  2087 | ` */` |
|        2 |  2088 | `static sxi32 VmByteCodeDump(` |
|        - |  2089 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2090 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2091 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2092 | `	)` |
|        1 |  2093 |  |
|        - |  2094 | `	static const char zDump[] = {` |
|        - |  2095 | `		"====================================================\n"` |
|        - |  2096 | `		"PH7 VM Dump\n"` |
|        - |  2097 | `		"====================================================\n"` |
|        - |  2098 | `	};` |
|        - |  2099 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2100 | `	sxi32 rc = SXRET_OK;` |
|        - |  2101 | `	sxu32 n;` |
|        - |  2102 | `	/* Point to the PH7 instructions */` |
|        3 |  2103 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2104 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2105 | `	n = 0;` |
|        3 |  2106 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2107 | `	/* Dump instructions */` |
|        7 |  2108 | `	for(;;){` |
|       15 |  2109 | `		if( pInstr >= pEnd ){` |
|        - |  2110 | `			/* No more instructions */` |
|        3 |  2111 | `			break;` |
|        - |  2112 | `		}` |
|        - |  2113 | `		/* Format and call the consumer callback */` |
|       19 |  2114 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2115 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2116 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2117 | `		if( rc != SXRET_OK ){` |
|        - |  2118 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2119 | `			return rc;` |
|        - |  2120 | `		}` |
|       13 |  2121 | `		++n;` |
|       13 |  2122 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2123 | `	}` |
|        3 |  2124 | `	return rc;` |
|        2 |  2125 |  |
|        - |  2126 | `/* Forward declaration */` |
|        - |  2127 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2128 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2129 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2130 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2131 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2132 | `/*` |
|        - |  2133 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2134 | ` * consumer callback.` |
|        - |  2135 | ` */` |
|      542 |  2136 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2137 |  |
|      543 |  2138 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      543 |  2139 | `	sxi32 rc = SXRET_OK;` |
|        - |  2140 | `	/* Append a new line */` |
|        - |  2141 | `#ifdef __WINNT__` |
|        1 |  2142 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2143 | `#else` |
|      542 |  2144 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2145 | `#endif` |
|        - |  2146 | `	/* Invoke the output consumer callback */` |
|      543 |  2147 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      543 |  2148 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2149 | `		/* Increment output length */` |
|      543 |  2150 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      271 |  2151 | `	}` |
|      543 |  2152 | `	return rc;` |
|        1 |  2153 |  |
|        - |  2154 | `/*` |
|        - |  2155 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2156 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2157 | ` * information.` |
|        - |  2158 | ` */` |
|      130 |  2159 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2160 |  |
|      132 |  2161 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2162 | `		ph7_value apArg[4];` |
|        - |  2163 | `		ph7_value *apArgPtr[4];` |
|        - |  2164 | `		ph7_value sResult;` |
|        - |  2165 | `		SyString sErr;` |
|        - |  2166 | `		/* Prepare arguments */` |
|       61 |  2167 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2168 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2169 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2170 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2171 | `		if( pFile ){` |
|       61 |  2172 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2173 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2174 | `		}else{` |
|      ! 0 |  2175 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2176 | `		}` |
|       61 |  2177 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2178 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2179 | `		/* Set up pointer array */` |
|       61 |  2180 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2181 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2182 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2183 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2184 | `		/* Call the handler */` |
|       61 |  2185 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2186 | `		/* Check return value */` |
|       61 |  2187 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2188 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2189 | `		}` |
|        - |  2190 | `		/* Release */` |
|       61 |  2191 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2192 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2193 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2194 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2195 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2196 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2197 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2198 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2199 | `	}` |
|        - |  2200 | `	/* No handler, always call error handler */` |
|       71 |  2201 | `	return TRUE;` |
|       67 |  2202 |  |
|       94 |  2203 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2204 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2205 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2206 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2207 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2208 | `	)` |
|        2 |  2209 |  |
|       96 |  2210 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2211 | `	SyString *pFile;` |
|        - |  2212 | `	char *zErr;` |
|       96 |  2213 | `	sxi32 rc = SXRET_OK;` |
|       96 |  2214 | `	if( !pVm->bErrReport ){` |
|        - |  2215 | `		/* Don't bother reporting errors */` |
|        3 |  2216 | `		return SXRET_OK;` |
|        - |  2217 | `	}` |
|        - |  2218 | `	/* Reset the working buffer */` |
|       94 |  2219 | `	SyBlobReset(pWorker);` |
|        - |  2220 | `	/* Peek the processed file if available */` |
|       94 |  2221 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       94 |  2222 | `	if( pFile ){` |
|        - |  2223 | `		/* Append file name */` |
|       94 |  2224 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       94 |  2225 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       46 |  2226 | `	}` |
|        - |  2227 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2228 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2229 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2230 | `	 * E_DEPRECATED). */` |
|       94 |  2231 | `	zErr = "Error:  ";` |
|       94 |  2232 | `	switch(iErr){` |
|       17 |  2233 | `	case PH7_CTX_WARNING:` |
|       36 |  2234 | `		zErr = "Warning:  ";` |
|       36 |  2235 | `		break;` |
|        6 |  2236 | `	case PH7_CTX_NOTICE:` |
|       14 |  2237 | `		zErr = "Notice:  ";` |
|       12 |  2238 | `		break;` |
|       23 |  2239 | `	default:` |
|        - |  2240 | `		/* keep iErr unchanged */` |
|       46 |  2241 | `		break;` |
|        - |  2242 | `	}` |
|       94 |  2243 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       94 |  2244 | `	if( pFuncName ){` |
|        - |  2245 | `		/* Append function name first */` |
|       21 |  2246 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       21 |  2247 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       10 |  2248 | `	}` |
|       94 |  2249 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2250 | `	/* Check for user error handler.  compute length of C string */` |
|       94 |  2251 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       45 |  2252 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       22 |  2253 | `	}` |
|       94 |  2254 | `	return rc;` |
|       49 |  2255 |  |
|        - |  2256 | `/*` |
|        - |  2257 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2258 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2259 | ` * information.` |
|        - |  2260 | ` */` |
|       38 |  2261 | `static sxi32 VmThrowErrorAp(` |
|        - |  2262 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2263 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2264 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2265 | `	const char *zFormat, /* Format message */` |
|        - |  2266 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2267 | `	)` |
|        2 |  2268 |  |
|       40 |  2269 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2270 | `	SyBlob sMsg;` |
|        - |  2271 | `	SyString *pFile;` |
|        - |  2272 | `	char *zErr;` |
|       40 |  2273 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2274 | `	if( !pVm->bErrReport ){` |
|        - |  2275 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2276 | `		return SXRET_OK;` |
|        - |  2277 | `	}` |
|        - |  2278 | `	/* Reset the working buffer */` |
|       40 |  2279 | `	SyBlobReset(pWorker);` |
|        - |  2280 | `	/* Peek the processed file if available */` |
|       40 |  2281 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2282 | `	if( pFile ){` |
|        - |  2283 | `		/* Append file name */` |
|       40 |  2284 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2285 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2286 | `	}` |
|        - |  2287 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2288 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2289 | `	 * the correct errno value. */` |
|       40 |  2290 | `	zErr = "Error:  ";` |
|       40 |  2291 | `	switch(iErr){` |
|        4 |  2292 | `	case PH7_CTX_WARNING:` |
|        9 |  2293 | `		zErr = "Warning:  ";` |
|        9 |  2294 | `		break;` |
|        3 |  2295 | `	case PH7_CTX_NOTICE:` |
|        7 |  2296 | `		zErr = "Notice:  ";` |
|        6 |  2297 | `		break;` |
|       12 |  2298 | `	default:` |
|        - |  2299 | `		/* do not change iErr */` |
|       24 |  2300 | `		break;` |
|        - |  2301 | `	}` |
|       40 |  2302 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2303 | `	if( pFuncName ){` |
|        - |  2304 | `		/* Append function name first */` |
|       26 |  2305 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2306 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2307 | `	}` |
|        - |  2308 | `	/* Format the raw message */` |
|       40 |  2309 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2310 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2311 | `	/* Check if a user error handler is installed */` |
|       40 |  2312 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2313 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2314 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2315 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2316 | `	}` |
|       40 |  2317 | `	SyBlobRelease(&sMsg);` |
|       40 |  2318 | `	return rc;` |
|       21 |  2319 |  |
|        - |  2320 | `/*` |
|        - |  2321 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2322 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2323 | ` * information.` |
|        - |  2324 | ` * ------------------------------------` |
|        - |  2325 | ` * Simple boring wrapper function.` |
|        - |  2326 | ` * ------------------------------------` |
|        - |  2327 | ` */` |
|       14 |  2328 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2329 |  |
|        - |  2330 | `	va_list ap;` |
|        - |  2331 | `	sxi32 rc;` |
|       15 |  2332 | `	va_start(ap,zFormat);` |
|       15 |  2333 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2334 | `	va_end(ap);` |
|       15 |  2335 | `	return rc;` |
|        1 |  2336 |  |
|        - |  2337 | `/*` |
|        - |  2338 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2339 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2340 | ` * information.` |
|        - |  2341 | ` * ------------------------------------` |
|        - |  2342 | ` * Simple boring wrapper function.` |
|        - |  2343 | ` * ------------------------------------` |
|        - |  2344 | ` */` |
|       24 |  2345 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2346 |  |
|        - |  2347 | `	sxi32 rc;` |
|       26 |  2348 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2349 | `	return rc;` |
|        2 |  2350 |  |
|        - |  2351 | `/*` |
|        - |  2352 | ` * Resolve function context from the current frame.` |
|        - |  2353 | ` */` |
|      934 |  2354 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2355 |  |
|        - |  2356 | `	VmFrame *pFrame;` |
|        - |  2357 | `	ph7_vm_func *pFunc;` |
|      935 |  2358 | `	*pzFuncName = 0;` |
|      935 |  2359 | `	*pnFuncLen = 0;` |
|      935 |  2360 | `	pFrame = pVm->pFrame;` |
|      935 |  2361 | `	if( pFrame == 0 ){` |
|      ! 0 |  2362 | `		return;` |
|        - |  2363 | `	}` |
|      935 |  2364 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2365 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2366 | `	}` |
|      935 |  2367 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2368 | `		return;` |
|        - |  2369 | `	}` |
|        7 |  2370 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2371 | `	if( pFunc == 0 ){` |
|      ! 0 |  2372 | `		return;` |
|        - |  2373 | `	}` |
|        7 |  2374 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2375 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2376 |  |
|        - |  2377 | `/*` |
|        - |  2378 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2379 | ` */` |
|      470 |  2380 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2381 |  |
|        - |  2382 | `	SyBlob sOut;` |
|        - |  2383 | `	SyString *pFile;` |
|      471 |  2384 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2385 | `		return PH7_OK;` |
|        - |  2386 | `	}` |
|      471 |  2387 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2388 | `		zClass = "Exception";` |
|      ! 0 |  2389 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2390 | `	}` |
|      471 |  2391 | `	if( zMsg == 0 ){` |
|      ! 0 |  2392 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2393 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2394 | `	}` |
|      471 |  2395 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2396 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2397 | `	}` |
|      471 |  2398 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2399 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2400 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2401 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2402 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2403 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2404 | `	if( pFile ){` |
|      471 |  2405 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2406 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2407 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2408 | `	}` |
|      471 |  2409 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2410 | `	if( pFile ){` |
|      471 |  2411 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2412 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2413 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2414 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2415 | `		}else{` |
|      465 |  2416 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2417 | `		}` |
|      235 |  2418 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2419 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2420 | `	}else{` |
|      ! 0 |  2421 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2422 | `	}` |
|      471 |  2423 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2424 | `	if( pFile ){` |
|      471 |  2425 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2426 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2427 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2428 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2429 | `	}` |
|      471 |  2430 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2431 | `	SyBlobRelease(&sOut);` |
|      471 |  2432 | `	return PH7_ABORT;` |
|      236 |  2433 |  |
|        - |  2434 | `/*` |
|        - |  2435 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2436 | ` */` |
|      468 |  2437 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2438 |  |
|        - |  2439 | `	ph7_vm *pVm;` |
|        - |  2440 | `	ph7_class *pClass;` |
|        - |  2441 | `	ph7_class_instance *pThis;` |
|        - |  2442 | `	ph7_class_method *pCons;` |
|        - |  2443 | `	ph7_value sArg;` |
|        - |  2444 | `	ph7_value *apArg[1];` |
|        - |  2445 | `	SyBlob sMsg;` |
|        - |  2446 | `	SyString sMsgStr;` |
|        - |  2447 | `	VmFrame *pFrame;` |
|        - |  2448 | `	va_list ap;` |
|        - |  2449 | `	sxi32 rc;` |
|        - |  2450 |  |
|      470 |  2451 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2452 | `		return PH7_ABORT;` |
|        - |  2453 | `	}` |
|      470 |  2454 | `	pVm = pCtx->pVm;` |
|      470 |  2455 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2456 | `		zClass = "Error";` |
|      ! 0 |  2457 | `	}` |
|      470 |  2458 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      470 |  2459 | `	if( pClass == 0 ){` |
|      ! 0 |  2460 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2461 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2462 | `			zClass` |
|        - |  2463 | `			);` |
|        - |  2464 | `	}` |
|      470 |  2465 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      470 |  2466 | `	if( pThis == 0 ){` |
|      ! 0 |  2467 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2468 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2469 | `			);` |
|        - |  2470 | `	}` |
|        - |  2471 |  |
|      470 |  2472 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      470 |  2473 | `	va_start(ap,zFormat);` |
|      470 |  2474 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      470 |  2475 | `	va_end(ap);` |
|        - |  2476 |  |
|      470 |  2477 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      470 |  2478 | `	if( pCons ){` |
|      470 |  2479 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      470 |  2480 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      470 |  2481 | `		apArg[0] = &sArg;` |
|      470 |  2482 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      470 |  2483 | `		PH7_MemObjRelease(&sArg);` |
|      234 |  2484 | `	}` |
|      470 |  2485 | `	SyBlobRelease(&sMsg);` |
|        - |  2486 |  |
|      470 |  2487 | `	pFrame = pVm->pFrame;` |
|      470 |  2488 | `	if( pFrame ){` |
|      476 |  2489 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  2490 | `			pFrame = pFrame->pParent;` |
|        1 |  2491 | `		}` |
|      470 |  2492 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      234 |  2493 | `	}` |
|      470 |  2494 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      470 |  2495 | `	PH7_ClassInstanceUnref(pThis);` |
|      470 |  2496 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2497 | `		return PH7_ABORT;` |
|        - |  2498 | `	}` |
|        7 |  2499 | `	return PH7_EXCEPTION;` |
|      236 |  2500 |  |
|        - |  2501 | `/*` |
|        - |  2502 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2503 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2504 | ` */` |
|      ! 0 |  2505 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2506 |  |
|        - |  2507 | `	ph7_vm *pVm;` |
|        - |  2508 | `	SyBlob sMsg;` |
|      ! 0 |  2509 | `	const char *zFuncName = 0;` |
|      ! 0 |  2510 | `	int nFuncLen = 0;` |
|        - |  2511 | `	va_list ap;` |
|        - |  2512 | `	sxi32 rc;` |
|        - |  2513 |  |
|      ! 0 |  2514 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2515 | `		return PH7_OK;` |
|        - |  2516 | `	}` |
|      ! 0 |  2517 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2518 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2519 | `		zClass = "Error";` |
|      ! 0 |  2520 | `	}` |
|        - |  2521 |  |
|      ! 0 |  2522 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2523 |  |
|      ! 0 |  2524 | `	va_start(ap,zFormat);` |
|      ! 0 |  2525 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2526 | `	va_end(ap);` |
|        - |  2527 |  |
|      ! 0 |  2528 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2529 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2530 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2531 | `	}` |
|      ! 0 |  2532 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2533 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2534 | `	}` |
|      ! 0 |  2535 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2536 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2537 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2538 | `	return rc;` |
|      ! 0 |  2539 |  |
|        - |  2540 | `/*` |
|        - |  2541 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2542 | ` *` |
|        - |  2543 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2544 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2545 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2546 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2547 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2548 | ` * then the program execution is halted.` |
|        - |  2549 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2550 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2551 | ` * or to reset the VM to it's initial state.` |
|        - |  2552 | ` */` |
|    30884 |  2553 | `static sxi32 VmByteCodeExec(` |
|        - |  2554 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2555 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2556 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2557 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2558 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2559 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2560 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2561 | `	)` |
|        2 |  2562 |  |
|        - |  2563 | `	VmInstr *pInstr;` |
|        - |  2564 | `	ph7_value *pTos;` |
|        - |  2565 | `	SySet aArg;` |
|        - |  2566 | `	sxi32 pc;` |
|        - |  2567 | `	sxi32 rc;` |
|        - |  2568 | `	/* Argument container */` |
|    30886 |  2569 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    30886 |  2570 | `	if( nTos < 0 ){` |
|    29146 |  2571 | `		pTos = &pStack[-1];` |
|    14574 |  2572 | `	}else{` |
|     1742 |  2573 | `		pTos = &pStack[nTos];` |
|        - |  2574 | `	}` |
|    30886 |  2575 | `	pc = 0;` |
|        - |  2576 | `	/* Execute as much as we can */` |
|  4839246 |  2577 | `	for(;;){` |
|        - |  2578 | `		/* Fetch the instruction to execute */` |
|  9677790 |  2579 | `		pInstr = &aInstr[pc];` |
|  9677790 |  2580 | `		rc = SXRET_OK;` |
|        - |  2581 | `/*` |
|        - |  2582 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2583 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2584 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2585 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2586 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2587 | ` */` |
|  9677790 |  2588 | `		switch(pInstr->iOp){` |
|        - |  2589 | `/*` |
|        - |  2590 | ` * DONE: P1 * *` |
|        - |  2591 | ` *` |
|        - |  2592 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2593 | ` * and return immediately.` |
|        - |  2594 | ` */` |
|    15199 |  2595 | `case PH7_OP_DONE:` |
|    30400 |  2596 | `	if( pInstr->iP1 ){` |
|        - |  2597 | `#ifdef UNTRUST` |
|        - |  2598 | `		if( pTos < pStack ){` |
|        - |  2599 | `			goto Abort;` |
|        - |  2600 | `		}` |
|        - |  2601 | `#endif` |
|    17588 |  2602 | `		if( pLastRef ){` |
|    11430 |  2603 | `			*pLastRef = pTos->nIdx;` |
|     5714 |  2604 | `		}` |
|    17588 |  2605 | `		if( pResult ){` |
|        - |  2606 | `			/* Execution result */` |
|    16738 |  2607 | `			PH7_MemObjStore(pTos,pResult);` |
|     8368 |  2608 | `		}` |
|    17588 |  2609 | `		VmPopOperand(&pTos,1);` |
|    21607 |  2610 | `	}else if( pLastRef ){` |
|        - |  2611 | `		/* Nothing referenced */` |
|      952 |  2612 | `		*pLastRef = SXU32_HIGH;` |
|      475 |  2613 | `	}` |
|    30400 |  2614 | `	goto Done;` |
|        - |  2615 | `/*` |
|        - |  2616 | ` * HALT: P1 * *` |
|        - |  2617 | ` *` |
|        - |  2618 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2619 | ` * and abort immediately.` |
|        - |  2620 | ` */` |
|        4 |  2621 | `case PH7_OP_HALT:` |
|        9 |  2622 | `	if( pInstr->iP1 ){` |
|        - |  2623 | `#ifdef UNTRUST` |
|        - |  2624 | `		if( pTos < pStack ){` |
|        - |  2625 | `			goto Abort;` |
|        - |  2626 | `		}` |
|        - |  2627 | `#endif` |
|        9 |  2628 | `		if( pLastRef ){` |
|      ! 0 |  2629 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2630 | `		}` |
|        9 |  2631 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2632 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2633 | `				/* Output the exit message */` |
|        7 |  2634 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2635 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2636 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2637 | `					/* Increment output length */` |
|        5 |  2638 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2639 | `				}` |
|        3 |  2640 | `			}` |
|        7 |  2641 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2642 | `			/* Record exit status */` |
|        5 |  2643 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2644 | `		}` |
|        9 |  2645 | `		VmPopOperand(&pTos,1);` |
|        4 |  2646 | `	}else if( pLastRef ){` |
|        - |  2647 | `		/* Nothing referenced */` |
|      ! 0 |  2648 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2649 | `	}` |
|        - |  2650 | `	/* Check if we're in an included file context */` |
|        9 |  2651 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2652 | `		/* Terminate the entire process */` |
|        9 |  2653 | `		exit(pVm->iExitStatus);` |
|        - |  2654 | `	}` |
|      ! 0 |  2655 | `	goto Abort;` |
|        - |  2656 | `/*` |
|        - |  2657 | ` * JMP: * P2 *` |
|        - |  2658 | ` *` |
|        - |  2659 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2660 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2661 | ` */` |
|   208966 |  2662 | `case PH7_OP_JMP:` |
|   417978 |  2663 | `	pc = pInstr->iP2 - 1;` |
|   417978 |  2664 | `	break;` |
|        - |  2665 | `/*` |
|        - |  2666 | ` * JZ: P1 P2 *` |
|        - |  2667 | ` *` |
|        - |  2668 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2669 | ` * entry in the stack if P1 is zero.` |
|        - |  2670 | ` */` |
|   487422 |  2671 | `case PH7_OP_JZ:` |
|        - |  2672 | `#ifdef UNTRUST` |
|        - |  2673 | `	if( pTos < pStack ){` |
|        - |  2674 | `		goto Abort;` |
|        - |  2675 | `	}` |
|        - |  2676 | `#endif` |
|        - |  2677 | `	/* Get a boolean value */` |
|   974934 |  2678 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2679 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2680 | `	}` |
|   974934 |  2681 | `	if( !pTos->x.iVal ){` |
|        - |  2682 | `		/* Take the jump */` |
|   490358 |  2683 | `		pc = pInstr->iP2 - 1;` |
|   245178 |  2684 | `	}` |
|   974934 |  2685 | `	if( !pInstr->iP1 ){` |
|   777982 |  2686 | `		VmPopOperand(&pTos,1);` |
|   389012 |  2687 | `	}` |
|   974934 |  2688 | `	break;` |
|        - |  2689 | `/*` |
|        - |  2690 | ` * JNZ: P1 P2 *` |
|        - |  2691 | ` *` |
|        - |  2692 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2693 | ` * entry in the stack if P1 is zero.` |
|        - |  2694 | ` */` |
|    53121 |  2695 | `case PH7_OP_JNZ:` |
|        - |  2696 | `#ifdef UNTRUST` |
|        - |  2697 | `	if( pTos < pStack ){` |
|        - |  2698 | `		goto Abort;` |
|        - |  2699 | `	}` |
|        - |  2700 | `#endif` |
|        - |  2701 | `	/* Get a boolean value */` |
|   106244 |  2702 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2703 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2704 | `	}` |
|   106244 |  2705 | `	if( pTos->x.iVal ){` |
|        - |  2706 | `		/* Take the jump */` |
|     4294 |  2707 | `		pc = pInstr->iP2 - 1;` |
|     2146 |  2708 | `	}` |
|   106244 |  2709 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2710 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2711 | `	}` |
|   106244 |  2712 | `	break;` |
|        - |  2713 | `/*` |
|        - |  2714 | ` * NOOP: * * *` |
|        - |  2715 | ` *` |
|        - |  2716 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2717 | ` * destination.` |
|        - |  2718 | ` */` |
|      ! 0 |  2719 | `case PH7_OP_NOOP:` |
|      ! 0 |  2720 | `	break;` |
|        - |  2721 | `/*` |
|        - |  2722 | ` * POP: P1 * *` |
|        - |  2723 | ` *` |
|        - |  2724 | ` * Pop P1 elements from the operand stack.` |
|        - |  2725 | ` */` |
|   379672 |  2726 | `case PH7_OP_POP: {` |
|   759390 |  2727 | `	sxi32 n = pInstr->iP1;` |
|   759390 |  2728 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2729 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2730 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2731 | `	}` |
|   759390 |  2732 | `	VmPopOperand(&pTos,n);` |
|   759390 |  2733 | `	break;` |
|        - |  2734 | `				 }` |
|        - |  2735 | `/*` |
|        - |  2736 | ` * DUP: * * *` |
|        - |  2737 | ` *` |
|        - |  2738 | ` * Duplicate the top of the stack.` |
|        - |  2739 | ` */` |
|       33 |  2740 | `case PH7_OP_DUP:` |
|        - |  2741 | `#ifdef UNTRUST` |
|        - |  2742 | `	if( pTos < pStack ){` |
|        - |  2743 | `		goto Abort;` |
|        - |  2744 | `	}` |
|        - |  2745 | `#endif` |
|       68 |  2746 | `	pTos++;` |
|       68 |  2747 | `	PH7_MemObjInit(pVm,pTos);` |
|       68 |  2748 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       68 |  2749 | `	break;` |
|        - |  2750 | `/*` |
|        - |  2751 | ` * NSSWITCH: * * P3` |
|        - |  2752 | ` *` |
|        - |  2753 | ` * Switch the active namespace at runtime.` |
|        - |  2754 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2755 | ` */` |
|     6175 |  2756 | `case PH7_OP_NSSWITCH:` |
|    12352 |  2757 | `	SyBlobReset(&pVm->sNamespace);` |
|    12352 |  2758 | `	if( pInstr->p3 ){` |
|       49 |  2759 | `		const char *zNs = (const char *)pInstr->p3;` |
|       49 |  2760 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       24 |  2761 | `	}` |
|    12352 |  2762 | `	break;` |
|        - |  2763 | `/*` |
|        - |  2764 | ` * CVT_INT: * * *` |
|        - |  2765 | ` *` |
|        - |  2766 | ` * Force the top of the stack to be an integer.` |
|        - |  2767 | ` */` |
|       35 |  2768 | `case PH7_OP_CVT_INT:` |
|        - |  2769 | `#ifdef UNTRUST` |
|        - |  2770 | `	if( pTos < pStack ){` |
|        - |  2771 | `		goto Abort;` |
|        - |  2772 | `	}` |
|        - |  2773 | `#endif` |
|       72 |  2774 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2775 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2776 | `	}` |
|        - |  2777 | `	/* Invalidate any prior representation */` |
|       72 |  2778 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2779 | `	break;` |
|        - |  2780 | `/*` |
|        - |  2781 | ` * CVT_REAL: * * *` |
|        - |  2782 | ` *` |
|        - |  2783 | ` * Force the top of the stack to be a real.` |
|        - |  2784 | ` */` |
|        4 |  2785 | `case PH7_OP_CVT_REAL:` |
|        - |  2786 | `#ifdef UNTRUST` |
|        - |  2787 | `	if( pTos < pStack ){` |
|        - |  2788 | `		goto Abort;` |
|        - |  2789 | `	}` |
|        - |  2790 | `#endif` |
|        9 |  2791 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2792 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2793 | `	}` |
|        - |  2794 | `	/* Invalidate any prior representation */` |
|        9 |  2795 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2796 | `	break;` |
|        - |  2797 | `/*` |
|        - |  2798 | ` * CVT_STR: * * *` |
|        - |  2799 | ` *` |
|        - |  2800 | ` * Force the top of the stack to be a string.` |
|        - |  2801 | ` */` |
|      146 |  2802 | `case PH7_OP_CVT_STR:` |
|        - |  2803 | `#ifdef UNTRUST` |
|        - |  2804 | `	if( pTos < pStack ){` |
|        - |  2805 | `		goto Abort;` |
|        - |  2806 | `	}` |
|        - |  2807 | `#endif` |
|      294 |  2808 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2809 | `		PH7_MemObjToString(pTos);` |
|      146 |  2810 | `	}` |
|      294 |  2811 | `	break;` |
|        - |  2812 | `/*` |
|        - |  2813 | ` * CVT_BOOL: * * *` |
|        - |  2814 | ` *` |
|        - |  2815 | ` * Force the top of the stack to be a boolean.` |
|        - |  2816 | ` */` |
|        5 |  2817 | `case PH7_OP_CVT_BOOL:` |
|        - |  2818 | `#ifdef UNTRUST` |
|        - |  2819 | `	if( pTos < pStack ){` |
|        - |  2820 | `		goto Abort;` |
|        - |  2821 | `	}` |
|        - |  2822 | `#endif` |
|       11 |  2823 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2824 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2825 | `	}` |
|       11 |  2826 | `	break;` |
|        - |  2827 | `/*` |
|        - |  2828 | ` * CVT_NULL: * * *` |
|        - |  2829 | ` *` |
|        - |  2830 | ` * Nullify the top of the stack.` |
|        - |  2831 | ` */` |
|        3 |  2832 | `case PH7_OP_CVT_NULL:` |
|        - |  2833 | `#ifdef UNTRUST` |
|        - |  2834 | `	if( pTos < pStack ){` |
|        - |  2835 | `		goto Abort;` |
|        - |  2836 | `	}` |
|        - |  2837 | `#endif` |
|        7 |  2838 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2839 | `	break;` |
|        - |  2840 | `/*` |
|        - |  2841 | ` * CVT_NUMC: * * *` |
|        - |  2842 | ` *` |
|        - |  2843 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2844 | ` */` |
|      ! 0 |  2845 | `case PH7_OP_CVT_NUMC:` |
|        - |  2846 | `#ifdef UNTRUST` |
|        - |  2847 | `	if( pTos < pStack ){` |
|        - |  2848 | `		goto Abort;` |
|        - |  2849 | `	}` |
|        - |  2850 | `#endif` |
|        - |  2851 | `	/* Force a numeric cast */` |
|      ! 0 |  2852 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2853 | `	break;` |
|        - |  2854 | `/*` |
|        - |  2855 | ` * CVT_ARRAY: * * *` |
|        - |  2856 | ` *` |
|        - |  2857 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2858 | ` */` |
|       10 |  2859 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2860 | `#ifdef UNTRUST` |
|        - |  2861 | `	if( pTos < pStack ){` |
|        - |  2862 | `		goto Abort;` |
|        - |  2863 | `	}` |
|        - |  2864 | `#endif` |
|        - |  2865 | `	/* Force a hashmap cast */` |
|       21 |  2866 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2867 | `	if( rc != SXRET_OK ){` |
|        - |  2868 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2869 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2870 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2871 | `	}` |
|       21 |  2872 | `	break;` |
|        - |  2873 | `/*` |
|        - |  2874 | ` * CVT_OBJ: * * *` |
|        - |  2875 | ` *` |
|        - |  2876 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2877 | ` */` |
|        8 |  2878 | `case PH7_OP_CVT_OBJ:` |
|        - |  2879 | `#ifdef UNTRUST` |
|        - |  2880 | `	if( pTos < pStack ){` |
|        - |  2881 | `		goto Abort;` |
|        - |  2882 | `	}` |
|        - |  2883 | `#endif` |
|       17 |  2884 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2885 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2886 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2887 | `	}` |
|       17 |  2888 | `	break;` |
|        - |  2889 | `/*` |
|        - |  2890 | ` * ERR_CTRL * * *` |
|        - |  2891 | ` *` |
|        - |  2892 | ` * Error control operator.` |
|        - |  2893 | ` */` |
|    12308 |  2894 | `case PH7_OP_ERR_CTRL:` |
|        - |  2895 | `	/*` |
|        - |  2896 | `	 * TICKET 1433-038:` |
|        - |  2897 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2898 | `	 * use the public API,to control error output.` |
|        - |  2899 | `	 */` |
|    24616 |  2900 | `	break;` |
|        - |  2901 | `/*` |
|        - |  2902 | ` * IS_A * * *` |
|        - |  2903 | ` *` |
|        - |  2904 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2905 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2906 | ` * holding a class name or an object).` |
|        - |  2907 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2908 | ` */` |
|       23 |  2909 | `case PH7_OP_IS_A:{` |
|       48 |  2910 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  2911 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2912 | `#ifdef UNTRUST` |
|        - |  2913 | `	if( pNos < pStack ){` |
|        - |  2914 | `		goto Abort;` |
|        - |  2915 | `	}` |
|        - |  2916 | `#endif` |
|       48 |  2917 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  2918 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  2919 | `		ph7_class *pClass = 0;` |
|        - |  2920 | `		/* Extract the target class */` |
|       46 |  2921 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2922 | `			/* Instance already loaded */` |
|      ! 0 |  2923 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  2924 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  2925 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  2926 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  2927 | `			/* Handle self/static/parent keywords */` |
|       46 |  2928 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  2929 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  2930 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  2931 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  2932 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  2933 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  2934 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  2935 | `					pClass = pSelf->pBase;` |
|        2 |  2936 | `				}` |
|        3 |  2937 | `			}else{` |
|       36 |  2938 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  2939 | `			}` |
|       22 |  2940 | `		}` |
|       46 |  2941 | `		if( pClass ){` |
|        - |  2942 | `			/* Perform the query */` |
|       46 |  2943 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  2944 | `		}` |
|       22 |  2945 | `	}` |
|        - |  2946 | `	/* Push result */` |
|       48 |  2947 | `	VmPopOperand(&pTos,1);` |
|       48 |  2948 | `	PH7_MemObjRelease(pTos);` |
|       48 |  2949 | `	pTos->x.iVal = iRes;` |
|       48 |  2950 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  2951 | `	break;` |
|        - |  2952 | `				 }` |
|        - |  2953 |  |
|        - |  2954 | `/*` |
|        - |  2955 | ` * LOADC P1 P2 *` |
|        - |  2956 | ` *` |
|        - |  2957 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2958 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2959 | ` */` |
|   800316 |  2960 | `case PH7_OP_LOADC: {` |
|        - |  2961 | `	ph7_value *pObj;` |
|        - |  2962 | `	/* Reserve a room */` |
|  1600678 |  2963 | `	pTos++;` |
|  2393143 |  2964 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1600678 |  2965 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2966 | `			SyHashEntry *pEntry;` |
|        - |  2967 | `			/* Candidate for expansion via user defined callbacks */` |
|    15794 |  2968 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15794 |  2969 | `			if( pEntry ){` |
|    15790 |  2970 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2971 | `				/* Set a NULL default value */` |
|    15790 |  2972 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15790 |  2973 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2974 | `				/* Invoke the callback and deal with the expanded value */` |
|    15790 |  2975 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2976 | `				/* Mark as constant */` |
|    15790 |  2977 | `				pTos->nIdx = SXU32_HIGH;` |
|    15790 |  2978 | `				break;` |
|        - |  2979 | `			}` |
|        - |  2980 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  2981 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  2982 | `			 * through to string value for backward compatibility. */` |
|        - |  2983 | `			{` |
|        6 |  2984 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  2985 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  2986 | `				sxu32 j;` |
|       32 |  2987 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  2988 | `					if( zLit[j] == '\\' ){` |
|        - |  2989 | `						/* Qualified name: must be a real constant.` |
|        - |  2990 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  2991 | `						{` |
|        3 |  2992 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  2993 | `							SyBlob sErr;` |
|        3 |  2994 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  2995 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  2996 | `							if( pErrFile ){` |
|        3 |  2997 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  2998 | `							}` |
|        3 |  2999 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3000 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3001 | `							SyBlobRelease(&sErr);` |
|        - |  3002 | `						}` |
|        3 |  3003 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3004 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3005 | `						goto LoadC_Done;` |
|        - |  3006 | `					}` |
|       15 |  3007 | `				}` |
|        - |  3008 | `			}` |
|        1 |  3009 | `		}` |
|  1584888 |  3010 | `		PH7_MemObjLoad(pObj,pTos);` |
|   792467 |  3011 | `	}else{` |
|        - |  3012 | `		/* Set a NULL value */` |
|      ! 0 |  3013 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3014 | `	}` |
|   792422 |  3015 | `LoadC_Done:` |
|        - |  3016 | `	/* Mark as constant */` |
|  1584890 |  3017 | `	pTos->nIdx = SXU32_HIGH;` |
|  1584890 |  3018 | `	break;` |
|        - |  3019 | `				  }` |
|        - |  3020 | `/*` |
|        - |  3021 | ` * LOAD: P1 * P3` |
|        - |  3022 | ` *` |
|        - |  3023 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3024 | ` * from the P3 operand.` |
|        - |  3025 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3026 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3027 | ` */` |
|  1321480 |  3028 | `case PH7_OP_LOAD:{` |
|        - |  3029 | `	ph7_value *pObj;` |
|        - |  3030 | `	SyString sName;` |
|  2643182 |  3031 | `	if( pInstr->p3 == 0 ){` |
|        - |  3032 | `		/* Take the variable name from the top of the stack */` |
|        - |  3033 | `#ifdef UNTRUST` |
|        - |  3034 | `		if( pTos < pStack ){` |
|        - |  3035 | `			goto Abort;` |
|        - |  3036 | `		}` |
|        - |  3037 | `#endif` |
|        - |  3038 | `		/* Force a string cast */` |
|       19 |  3039 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3040 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3041 | `		}` |
|       19 |  3042 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3043 | `	}else{` |
|  2643164 |  3044 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3045 | `		/* Reserve a room for the target object */` |
|  2643164 |  3046 | `		pTos++;` |
|        - |  3047 | `	}` |
|        - |  3048 | `	/* Extract the requested memory object */` |
|  2643182 |  3049 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2643182 |  3050 | `	if( pObj == 0 ){` |
|      624 |  3051 | `		if( pInstr->iP1 ){` |
|        - |  3052 | `			/* Variable not found,load NULL */` |
|      624 |  3053 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3054 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3055 | `			}else{` |
|      624 |  3056 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3057 | `			}` |
|      624 |  3058 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1321793 |  3059 | `			break;` |
|      ! 0 |  3060 | `		}else{` |
|        - |  3061 | `			/* Fatal error */` |
|      ! 0 |  3062 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3063 | `			goto Abort;` |
|        - |  3064 | `		}` |
|        - |  3065 | `	}` |
|        - |  3066 | `	/* Load variable contents */` |
|  2642560 |  3067 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2642560 |  3068 | `	pTos->nIdx = pObj->nIdx;` |
|  2642560 |  3069 | `	break;` |
|        - |  3070 | `				   }` |
|        - |  3071 | `/*` |
|        - |  3072 | ` * LOAD_MAP P1 * *` |
|        - |  3073 | ` *` |
|        - |  3074 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3075 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3076 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3077 | ` */` |
|    17791 |  3078 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3079 | `	ph7_hashmap *pMap;` |
|        - |  3080 | `	/* Allocate a new hashmap instance */` |
|    35584 |  3081 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    35584 |  3082 | `	if( pMap == 0 ){` |
|      ! 0 |  3083 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3084 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3085 | `		goto Abort;` |
|        - |  3086 | `	}` |
|    35584 |  3087 | `	if( pInstr->iP1 > 0 ){` |
|     2168 |  3088 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3089 | `		/* Perform the insertion */` |
|     6600 |  3090 | `		while( pEntry < pTos ){` |
|     4434 |  3091 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3092 | `				/* Insertion by reference */` |
|      142 |  3093 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3094 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3095 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3096 | `					);` |
|       48 |  3097 | `			}else{` |
|        - |  3098 | `				/* Standard insertion */` |
|     6509 |  3099 | `				PH7_HashmapInsert(pMap,` |
|     4338 |  3100 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2169 |  3101 | `					&pEntry[1]` |
|        - |  3102 | `				);` |
|        - |  3103 | `			}` |
|        - |  3104 | `			/* Next pair on the stack */` |
|     4434 |  3105 | `			pEntry += 2;` |
|        2 |  3106 | `		}` |
|        - |  3107 | `		/* Pop P1 elements */` |
|     2168 |  3108 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1083 |  3109 | `	}` |
|        - |  3110 | `	/* Push the hashmap */` |
|    35584 |  3111 | `	pTos++;` |
|    35584 |  3112 | `	pTos->nIdx = SXU32_HIGH;` |
|    35584 |  3113 | `	pTos->x.pOther = pMap;` |
|    35584 |  3114 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    35584 |  3115 | `	break;` |
|        - |  3116 | `					  }` |
|        - |  3117 | `/*` |
|        - |  3118 | ` * LOAD_LIST: P1 * *` |
|        - |  3119 | ` *` |
|        - |  3120 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3121 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3122 | ` * Caveats:` |
|        - |  3123 | ` *  This implementation support only a single nesting level.` |
|        - |  3124 | ` */` |
|       22 |  3125 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3126 | `	ph7_value *pEntry;` |
|       46 |  3127 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3128 | `		/* Empty list,break immediately */` |
|      ! 0 |  3129 | `		break;` |
|        - |  3130 | `	}` |
|       46 |  3131 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3132 | `#ifdef UNTRUST` |
|        - |  3133 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3134 | `		goto Abort;` |
|        - |  3135 | `	}` |
|        - |  3136 | `#endif` |
|       46 |  3137 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       42 |  3138 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3139 | `		ph7_hashmap_node *pNode;` |
|        - |  3140 | `		ph7_value sKey,*pObj;` |
|        - |  3141 | `		/* Start Copying */` |
|       42 |  3142 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      130 |  3143 | `		while( pEntry <= pTos ){` |
|       90 |  3144 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       86 |  3145 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       86 |  3146 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       86 |  3147 | `					if( rc == SXRET_OK ){` |
|        - |  3148 | `						/* Store node value */` |
|       86 |  3149 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       44 |  3150 | `					}else{` |
|        - |  3151 | `						/* Nullify the variable */` |
|      ! 0 |  3152 | `						PH7_MemObjRelease(pObj);` |
|        - |  3153 | `					}` |
|       42 |  3154 | `				}` |
|       42 |  3155 | `			}` |
|       90 |  3156 | `			sKey.x.iVal++; /* Next numeric index */` |
|       90 |  3157 | `			pEntry++;` |
|        2 |  3158 | `		}` |
|       20 |  3159 | `	}` |
|       46 |  3160 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       46 |  3161 | `	break;` |
|        - |  3162 | `					   }` |
|        - |  3163 | `/*` |
|        - |  3164 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3165 | ` *` |
|        - |  3166 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3167 | ` * from the stack.` |
|        - |  3168 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3169 | ` * instead.` |
|        - |  3170 | ` */` |
|   213993 |  3171 | `case PH7_OP_LOAD_IDX: {` |
|   428032 |  3172 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   428032 |  3173 | `	ph7_hashmap *pMap = 0;` |
|        - |  3174 | `	ph7_value *pIdx;` |
|   428032 |  3175 | `	pIdx = 0;` |
|   428032 |  3176 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3177 | `		if( !pInstr->iP2){` |
|        - |  3178 | `			/* No available index,load NULL */` |
|      ! 0 |  3179 | `			if( pTos >= pStack ){` |
|      ! 0 |  3180 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3181 | `			}else{` |
|        - |  3182 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3183 | `				pTos++;` |
|      ! 0 |  3184 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3185 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3186 | `			}` |
|        - |  3187 | `			/* Emit a notice */` |
|      ! 0 |  3188 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3189 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3190 | `			break;` |
|        - |  3191 | `		}` |
|      ! 0 |  3192 | `	}else{` |
|   428032 |  3193 | `		pIdx = pTos;` |
|   428032 |  3194 | `		pTos--;` |
|        - |  3195 | `	}` |
|   428032 |  3196 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3197 | `		/* String access */` |
|   339788 |  3198 | `		if( pIdx ){` |
|        - |  3199 | `			sxu32 nOfft;` |
|   339788 |  3200 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3201 | `				/* Force an int cast */` |
|      ! 0 |  3202 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3203 | `			}` |
|   339788 |  3204 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   339788 |  3205 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3206 | `				/* Invalid offset,load null */` |
|      ! 0 |  3207 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3208 | `			}else{` |
|   339788 |  3209 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   339788 |  3210 | `				int c = zData[nOfft];` |
|   339788 |  3211 | `				PH7_MemObjRelease(pTos);` |
|   339788 |  3212 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   339788 |  3213 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3214 | `			}` |
|   169917 |  3215 | `		}else{` |
|        - |  3216 | `			/* No available index,load NULL */` |
|      ! 0 |  3217 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3218 | `		}` |
|   339788 |  3219 | `		break;` |
|        - |  3220 | `	}` |
|    88246 |  3221 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3222 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3223 | `			ph7_value *pObj;` |
|      ! 0 |  3224 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3225 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3226 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3227 | `			}` |
|      ! 0 |  3228 | `		}` |
|      ! 0 |  3229 | `	}` |
|    88246 |  3230 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    88246 |  3231 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3232 | `		/* Point to the hashmap */` |
|    88246 |  3233 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    88246 |  3234 | `		if( pIdx ){` |
|        - |  3235 | `			/* Load the desired entry */` |
|    88246 |  3236 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    44122 |  3237 | `		}` |
|    88246 |  3238 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3239 | `			/* Create a new empty entry */` |
|      ! 0 |  3240 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3241 | `			if( rc == SXRET_OK ){` |
|        - |  3242 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3243 | `				pNode = pMap->pLast;` |
|      ! 0 |  3244 | `			}` |
|      ! 0 |  3245 | `		}` |
|    44122 |  3246 | `	}` |
|    88246 |  3247 | `	if( pIdx ){` |
|    88246 |  3248 | `		PH7_MemObjRelease(pIdx);` |
|    44122 |  3249 | `	}` |
|    88246 |  3250 | `	if( rc == SXRET_OK ){` |
|        - |  3251 | `		/* Load entry contents */` |
|    40218 |  3252 | `		if( pMap->iRef < 2 ){` |
|        - |  3253 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3254 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3255 | `			 */` |
|       24 |  3256 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3257 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3258 | `		}else{` |
|    40196 |  3259 | `			pTos->nIdx = pNode->nValIdx;` |
|    40196 |  3260 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    40196 |  3261 | `			PH7_HashmapUnref(pMap);` |
|        - |  3262 | `		}` |
|    20110 |  3263 | `	}else{` |
|        - |  3264 | `		/* No such entry,load NULL */` |
|    48030 |  3265 | `		PH7_MemObjRelease(pTos);` |
|    48030 |  3266 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3267 | `	}` |
|    88246 |  3268 | `	break;` |
|        - |  3269 | `					  }` |
|        - |  3270 | `/*` |
|        - |  3271 | ` * LOAD_CLOSURE * * P3` |
|        - |  3272 | ` *` |
|        - |  3273 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3274 | ` * name in the stack.` |
|        - |  3275 | ` */` |
|        2 |  3276 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3277 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3278 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3279 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3280 | `		ph7_vm_func *pClosure;` |
|        - |  3281 | `		char *zName;` |
|        - |  3282 | `		sxu32 mLen;` |
|        - |  3283 | `		sxu32 n;` |
|        - |  3284 | `		/* Create a new VM function */` |
|        5 |  3285 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3286 | `		/* Generate an unique closure name */` |
|        5 |  3287 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3288 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3289 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3290 | `			goto Abort;` |
|        - |  3291 | `		}` |
|        5 |  3292 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3293 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3294 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3295 | `		}` |
|        - |  3296 | `		/* Zero the stucture */` |
|        5 |  3297 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3298 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3299 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3300 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3301 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3302 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3303 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3304 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3305 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3306 | `		/* Register the closure */` |
|        5 |  3307 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3308 | `		/* Set up closure environment */` |
|        5 |  3309 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3310 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3311 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3312 | `			ph7_value *pValue;` |
|        9 |  3313 | `			pEnv = &aEnv[n];` |
|        9 |  3314 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3315 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3316 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3317 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3318 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3319 | `				/* Pass by reference */` |
|      ! 0 |  3320 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3321 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3322 | `					);` |
|      ! 0 |  3323 | `			}` |
|        - |  3324 | `			/* Standard pass by value */` |
|        9 |  3325 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3326 | `			if( pValue ){` |
|        - |  3327 | `				/* Copy imported value */` |
|        5 |  3328 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3329 | `			}` |
|        - |  3330 | `			/* Insert the imported variable */` |
|        9 |  3331 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3332 | `		}` |
|        - |  3333 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3334 | `		pTos++;` |
|        5 |  3335 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3336 | `	}` |
|        5 |  3337 | `	break;` |
|        - |  3338 | `						 }` |
|        - |  3339 | `/*` |
|        - |  3340 | ` * STORE * P2 P3` |
|        - |  3341 | ` *` |
|        - |  3342 | ` * Perform a store (Assignment) operation.` |
|        - |  3343 | ` */` |
|   109701 |  3344 | `case PH7_OP_STORE: {` |
|        - |  3345 | `	ph7_value *pObj;` |
|        - |  3346 | `	SyString sName;` |
|        - |  3347 | `#ifdef UNTRUST` |
|        - |  3348 | `	if( pTos < pStack ){` |
|        - |  3349 | `		goto Abort;` |
|        - |  3350 | `	}` |
|        - |  3351 | `#endif` |
|   219404 |  3352 | `	if( pInstr->iP2 ){` |
|        - |  3353 | `		sxu32 nIdx;` |
|        - |  3354 | `		/* Member store operation */` |
|     2912 |  3355 | `		nIdx = pTos->nIdx;` |
|     2912 |  3356 | `		VmPopOperand(&pTos,1);` |
|     2912 |  3357 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3358 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3359 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3360 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3361 | `		}else{` |
|        - |  3362 | `			/* Point to the desired memory object */` |
|     2908 |  3363 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2908 |  3364 | `			if( pObj ){` |
|        - |  3365 | `				/* Perform the store operation */` |
|     2908 |  3366 | `				PH7_MemObjStore(pTos,pObj);` |
|     1453 |  3367 | `			}` |
|        - |  3368 | `		}` |
|   111158 |  3369 | `		break;` |
|   216494 |  3370 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3371 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3372 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3373 | `			/* Force a string cast */` |
|      ! 0 |  3374 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3375 | `		}` |
|        7 |  3376 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3377 | `		pTos--;` |
|        - |  3378 | `#ifdef UNTRUST` |
|        - |  3379 | `		if( pTos < pStack  ){` |
|        - |  3380 | `			goto Abort;` |
|        - |  3381 | `		}` |
|        - |  3382 | `#endif` |
|        4 |  3383 | `	}else{` |
|   216488 |  3384 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3385 | `	}` |
|        - |  3386 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   216494 |  3387 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   216494 |  3388 | `	if( pObj == 0 ){` |
|      ! 0 |  3389 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3390 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3391 | `		goto Abort;` |
|        - |  3392 | `	}` |
|   216494 |  3393 | `	if( !pInstr->p3 ){` |
|        7 |  3394 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3395 | `	}` |
|        - |  3396 | `	/* Perform the store operation */` |
|   216494 |  3397 | `	PH7_MemObjStore(pTos,pObj);` |
|   216494 |  3398 | `	break;` |
|        - |  3399 | `				   }` |
|        - |  3400 | `/*` |
|        - |  3401 | ` * STORE_IDX:   P1 * P3` |
|        - |  3402 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3403 | ` *` |
|        - |  3404 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3405 | ` */` |
|    79685 |  3406 | `case PH7_OP_STORE_IDX:` |
|        - |  3407 | `case PH7_OP_STORE_IDX_REF: {` |
|   159372 |  3408 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3409 | `	ph7_value *pKey;` |
|        - |  3410 | `	sxu32 nIdx;` |
|   159372 |  3411 | `	if( pInstr->iP1 ){` |
|        - |  3412 | `		/* Key is next on stack */` |
|    56662 |  3413 | `		pKey = pTos;` |
|    56662 |  3414 | `		pTos--;` |
|    28332 |  3415 | `	}else{` |
|   102712 |  3416 | `		pKey = 0;` |
|        - |  3417 | `	}` |
|   159372 |  3418 | `	nIdx = pTos->nIdx;` |
|   159372 |  3419 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3420 | `		/* Hashmap already loaded */` |
|   159320 |  3421 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   159320 |  3422 | `		if( pMap->iRef < 2 ){` |
|        - |  3423 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3424 | `			pMap->iRef = 2;` |
|      ! 0 |  3425 | `		}` |
|    79661 |  3426 | `	}else{` |
|        - |  3427 | `		ph7_value *pObj;` |
|       53 |  3428 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3429 | `		if( pObj == 0 ){` |
|      ! 0 |  3430 | `			if( pKey ){` |
|      ! 0 |  3431 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3432 | `			}` |
|      ! 0 |  3433 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3434 | `			break;` |
|        - |  3435 | `		}` |
|        - |  3436 | `		/* Phase#1: Load the array */` |
|       53 |  3437 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3438 | `			VmPopOperand(&pTos,1);` |
|       53 |  3439 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3440 | `				/* Force a string cast */` |
|      ! 0 |  3441 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3442 | `			}` |
|       53 |  3443 | `			if( pKey == 0 ){` |
|        - |  3444 | `				/* Append string */` |
|        3 |  3445 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3446 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3447 | `				}` |
|        2 |  3448 | `			}else{` |
|        - |  3449 | `				sxu32 nOfft;` |
|       51 |  3450 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3451 | `					/* Force an int cast */` |
|       51 |  3452 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3453 | `				}` |
|       51 |  3454 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3455 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3456 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3457 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3458 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3459 | `				}else{` |
|      ! 0 |  3460 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3461 | `						/* Perform an append operation */` |
|      ! 0 |  3462 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3463 | `					}` |
|        - |  3464 | `				}` |
|        - |  3465 | `			}` |
|       53 |  3466 | `			if( pKey ){` |
|       51 |  3467 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3468 | `			}` |
|       53 |  3469 | `			break;` |
|      ! 0 |  3470 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3471 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3472 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3473 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3474 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3475 | `				goto Abort;` |
|        - |  3476 | `			}` |
|      ! 0 |  3477 | `		}` |
|      ! 0 |  3478 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3479 | `	}` |
|   159320 |  3480 | `	VmPopOperand(&pTos,1);` |
|        - |  3481 | `	/* Phase#2: Perform the insertion */` |
|   159320 |  3482 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3483 | `		/* Insertion by reference */` |
|       15 |  3484 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3485 | `	}else{` |
|   159306 |  3486 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3487 | `	}` |
|   159320 |  3488 | `	if( pKey ){` |
|    56612 |  3489 | `		PH7_MemObjRelease(pKey);` |
|    28305 |  3490 | `	}` |
|   159320 |  3491 | `	break;` |
|        - |  3492 | `					   }` |
|        - |  3493 | `/*` |
|        - |  3494 | ` * INCR: P1 * *` |
|        - |  3495 | ` *` |
|        - |  3496 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3497 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3498 | ` * the stack and increment after that.` |
|        - |  3499 | ` */` |
|   150848 |  3500 | `case PH7_OP_INCR:` |
|        - |  3501 | `#ifdef UNTRUST` |
|        - |  3502 | `	if( pTos < pStack ){` |
|        - |  3503 | `		goto Abort;` |
|        - |  3504 | `	}` |
|        - |  3505 | `#endif` |
|   301742 |  3506 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   301742 |  3507 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3508 | `			ph7_value *pObj;` |
|   301742 |  3509 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3510 | `				/* Force a numeric cast */` |
|   301742 |  3511 | `				PH7_MemObjToNumeric(pObj);` |
|   301742 |  3512 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3513 | `					pObj->rVal++;` |
|        - |  3514 | `					/* Try to get an integer representation */` |
|      ! 0 |  3515 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3516 | `				}else{` |
|   301742 |  3517 | `					pObj->x.iVal++;` |
|   301742 |  3518 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3519 | `				}` |
|   301742 |  3520 | `				if( pInstr->iP1 ){` |
|        - |  3521 | `					/* Pre-icrement */` |
|       71 |  3522 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3523 | `				}` |
|   150892 |  3524 | `			}` |
|   150894 |  3525 | `		}else{` |
|      ! 0 |  3526 | `			if( pInstr->iP1 ){` |
|        - |  3527 | `				/* Force a numeric cast */` |
|      ! 0 |  3528 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3529 | `				/* Pre-increment */` |
|      ! 0 |  3530 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3531 | `					pTos->rVal++;` |
|        - |  3532 | `					/* Try to get an integer representation */` |
|      ! 0 |  3533 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3534 | `				}else{` |
|      ! 0 |  3535 | `					pTos->x.iVal++;` |
|      ! 0 |  3536 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3537 | `				}` |
|      ! 0 |  3538 | `			}` |
|        - |  3539 | `		}` |
|   150892 |  3540 | `	}` |
|   301742 |  3541 | `	break;` |
|        - |  3542 | `/*` |
|        - |  3543 | ` * DECR: P1 * *` |
|        - |  3544 | ` *` |
|        - |  3545 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3546 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3547 | ` * and decrement after that.` |
|        - |  3548 | ` */` |
|        2 |  3549 | `case PH7_OP_DECR:` |
|        - |  3550 | `#ifdef UNTRUST` |
|        - |  3551 | `	if( pTos < pStack ){` |
|        - |  3552 | `		goto Abort;` |
|        - |  3553 | `	}` |
|        - |  3554 | `#endif` |
|        5 |  3555 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3556 | `		/* Force a numeric cast */` |
|        5 |  3557 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3558 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3559 | `			ph7_value *pObj;` |
|        5 |  3560 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3561 | `				/* Force a numeric cast */` |
|        5 |  3562 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3563 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3564 | `					pObj->rVal--;` |
|        - |  3565 | `					/* Try to get an integer representation */` |
|      ! 0 |  3566 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3567 | `				}else{` |
|        5 |  3568 | `					pObj->x.iVal--;` |
|        5 |  3569 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3570 | `				}` |
|        5 |  3571 | `				if( pInstr->iP1 ){` |
|        - |  3572 | `					/* Pre-icrement */` |
|      ! 0 |  3573 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3574 | `				}` |
|        2 |  3575 | `			}` |
|        3 |  3576 | `		}else{` |
|      ! 0 |  3577 | `			if( pInstr->iP1 ){` |
|        - |  3578 | `				/* Pre-increment */` |
|      ! 0 |  3579 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3580 | `					pTos->rVal--;` |
|        - |  3581 | `					/* Try to get an integer representation */` |
|      ! 0 |  3582 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3583 | `				}else{` |
|      ! 0 |  3584 | `					pTos->x.iVal--;` |
|      ! 0 |  3585 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3586 | `				}` |
|      ! 0 |  3587 | `			}` |
|        - |  3588 | `		}` |
|        2 |  3589 | `	}` |
|        5 |  3590 | `	break;` |
|        - |  3591 | `/*` |
|        - |  3592 | ` * UMINUS: * * *` |
|        - |  3593 | ` *` |
|        - |  3594 | ` * Perform a unary minus operation.` |
|        - |  3595 | ` */` |
|    23001 |  3596 | `case PH7_OP_UMINUS:` |
|        - |  3597 | `#ifdef UNTRUST` |
|        - |  3598 | `	if( pTos < pStack ){` |
|        - |  3599 | `		goto Abort;` |
|        - |  3600 | `	}` |
|        - |  3601 | `#endif` |
|        - |  3602 | `	/* Force a numeric (integer,real or both) cast */` |
|    46004 |  3603 | `	PH7_MemObjToNumeric(pTos);` |
|    46004 |  3604 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3605 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3606 | `	}` |
|    46004 |  3607 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    45974 |  3608 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22986 |  3609 | `	}` |
|    46004 |  3610 | `	break;` |
|        - |  3611 | `/*` |
|        - |  3612 | ` * UPLUS: * * *` |
|        - |  3613 | ` *` |
|        - |  3614 | ` * Perform a unary plus operation.` |
|        - |  3615 | ` */` |
|       16 |  3616 | `case PH7_OP_UPLUS:` |
|        - |  3617 | `#ifdef UNTRUST` |
|        - |  3618 | `	if( pTos < pStack ){` |
|        - |  3619 | `		goto Abort;` |
|        - |  3620 | `	}` |
|        - |  3621 | `#endif` |
|        - |  3622 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3623 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3624 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3625 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3626 | `	}` |
|       33 |  3627 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3628 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3629 | `	}` |
|       33 |  3630 | `	break;` |
|        - |  3631 | `/*` |
|        - |  3632 | ` * OP_LNOT: * * *` |
|        - |  3633 | ` *` |
|        - |  3634 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3635 | ` * with its complement.` |
|        - |  3636 | ` */` |
|    39439 |  3637 | `case PH7_OP_LNOT:` |
|        - |  3638 | `#ifdef UNTRUST` |
|        - |  3639 | `	if( pTos < pStack ){` |
|        - |  3640 | `		goto Abort;` |
|        - |  3641 | `	}` |
|        - |  3642 | `#endif` |
|        - |  3643 | `	/* Force a boolean cast */` |
|    78924 |  3644 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3645 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3646 | `	}` |
|    78924 |  3647 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    78924 |  3648 | `	break;` |
|        - |  3649 | `/*` |
|        - |  3650 | ` * OP_BITNOT: * * *` |
|        - |  3651 | ` *` |
|        - |  3652 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3653 | ` * with its ones-complement.` |
|        - |  3654 | ` */` |
|       14 |  3655 | `case PH7_OP_BITNOT:` |
|        - |  3656 | `#ifdef UNTRUST` |
|        - |  3657 | `	if( pTos < pStack ){` |
|        - |  3658 | `		goto Abort;` |
|        - |  3659 | `	}` |
|        - |  3660 | `#endif` |
|        - |  3661 | `	/* Force an integer cast */` |
|       30 |  3662 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3663 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3664 | `	}` |
|       30 |  3665 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3666 | `	break;` |
|        - |  3667 | `/* OP_MUL * * *` |
|        - |  3668 | ` * OP_MUL_STORE * * *` |
|        - |  3669 | ` *` |
|        - |  3670 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3671 | ` * and push the result back onto the stack.` |
|        - |  3672 | ` */` |
|     1240 |  3673 | `case PH7_OP_MUL:` |
|        - |  3674 | `case PH7_OP_MUL_STORE: {` |
|     2482 |  3675 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3676 | `	/* Force the operand to be numeric */` |
|        - |  3677 | `#ifdef UNTRUST` |
|        - |  3678 | `	if( pNos < pStack ){` |
|        - |  3679 | `		goto Abort;` |
|        - |  3680 | `	}` |
|        - |  3681 | `#endif` |
|     2482 |  3682 | `	PH7_MemObjToNumeric(pTos);` |
|     2482 |  3683 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3684 | `	/* Perform the requested operation */` |
|     2482 |  3685 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3686 | `		/* Floating point arithemic */` |
|        - |  3687 | `		ph7_real a,b,r;` |
|       17 |  3688 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3689 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3690 | `		}` |
|       17 |  3691 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3692 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3693 | `		}` |
|       17 |  3694 | `		a = pNos->rVal;` |
|       17 |  3695 | `		b = pTos->rVal;` |
|       17 |  3696 | `		r = a * b;` |
|        - |  3697 | `		/* Push the result */` |
|       17 |  3698 | `		pNos->rVal = r;` |
|       17 |  3699 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3700 | `		/* Try to get an integer representation */` |
|       17 |  3701 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3702 | `	}else{` |
|        - |  3703 | `		/* Integer arithmetic */` |
|        - |  3704 | `		sxi64 a,b,r;` |
|     2466 |  3705 | `		a = pNos->x.iVal;` |
|     2466 |  3706 | `		b = pTos->x.iVal;` |
|     2466 |  3707 | `		r = a * b;` |
|        - |  3708 | `		/* Push the result */` |
|     2466 |  3709 | `		pNos->x.iVal = r;` |
|     2466 |  3710 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3711 | `	}` |
|     2482 |  3712 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3713 | `		ph7_value *pObj;` |
|       19 |  3714 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3715 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3716 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3717 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3718 | `		}` |
|        9 |  3719 | `	}` |
|     2482 |  3720 | `	VmPopOperand(&pTos,1);` |
|     2482 |  3721 | `	break;` |
|        - |  3722 | `				 }` |
|        - |  3723 | `/* OP_ADD * * *` |
|        - |  3724 | ` *` |
|        - |  3725 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3726 | ` * and push the result back onto the stack.` |
|        - |  3727 | ` */` |
|      429 |  3728 | `case PH7_OP_ADD:{` |
|      860 |  3729 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3730 | `#ifdef UNTRUST` |
|        - |  3731 | `	if( pNos < pStack ){` |
|        - |  3732 | `		goto Abort;` |
|        - |  3733 | `	}` |
|        - |  3734 | `#endif` |
|        - |  3735 | `	/* Perform the addition */` |
|      860 |  3736 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      860 |  3737 | `	VmPopOperand(&pTos,1);` |
|      860 |  3738 | `	break;` |
|        - |  3739 | `				}` |
|        - |  3740 | `/*` |
|        - |  3741 | ` * OP_ADD_STORE * * *` |
|        - |  3742 | ` *` |
|        - |  3743 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3744 | ` * and push the result back onto the stack.` |
|        - |  3745 | ` */` |
|      482 |  3746 | `case PH7_OP_ADD_STORE:{` |
|      966 |  3747 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3748 | `	ph7_value *pObj;` |
|        - |  3749 | `	sxu32 nIdx;` |
|        - |  3750 | `#ifdef UNTRUST` |
|        - |  3751 | `	if( pNos < pStack ){` |
|        - |  3752 | `		goto Abort;` |
|        - |  3753 | `	}` |
|        - |  3754 | `#endif` |
|        - |  3755 | `	/* Perform the addition */` |
|      966 |  3756 | `	nIdx = pTos->nIdx;` |
|      966 |  3757 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3758 | `	/* Peform the store operation */` |
|      966 |  3759 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3760 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      966 |  3761 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      966 |  3762 | `		PH7_MemObjStore(pTos,pObj);` |
|      482 |  3763 | `	}` |
|        - |  3764 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      966 |  3765 | `	PH7_MemObjStore(pTos,pNos);` |
|      966 |  3766 | `	VmPopOperand(&pTos,1);` |
|      966 |  3767 | `	break;` |
|        - |  3768 | `				}` |
|        - |  3769 | `/* OP_SUB * * *` |
|        - |  3770 | ` *` |
|        - |  3771 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3772 | ` * first (what was next on the stack) from the second (the` |
|        - |  3773 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3774 | ` */` |
|      299 |  3775 | `case PH7_OP_SUB: {` |
|      600 |  3776 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3777 | `#ifdef UNTRUST` |
|        - |  3778 | `	if( pNos < pStack ){` |
|        - |  3779 | `		goto Abort;` |
|        - |  3780 | `	}` |
|        - |  3781 | `#endif` |
|      600 |  3782 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3783 | `		/* Floating point arithemic */` |
|        - |  3784 | `		ph7_real a,b,r;` |
|       95 |  3785 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3786 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3787 | `		}` |
|       95 |  3788 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3789 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3790 | `		}` |
|       95 |  3791 | `		a = pNos->rVal;` |
|       95 |  3792 | `		b = pTos->rVal;` |
|       95 |  3793 | `		r = a - b;` |
|        - |  3794 | `		/* Push the result */` |
|       95 |  3795 | `		pNos->rVal = r;` |
|       95 |  3796 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3797 | `		/* Try to get an integer representation */` |
|       95 |  3798 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3799 | `	}else{` |
|        - |  3800 | `		/* Integer arithmetic */` |
|        - |  3801 | `		sxi64 a,b,r;` |
|      506 |  3802 | `		a = pNos->x.iVal;` |
|      506 |  3803 | `		b = pTos->x.iVal;` |
|      506 |  3804 | `		r = a - b;` |
|        - |  3805 | `		/* Push the result */` |
|      506 |  3806 | `		pNos->x.iVal = r;` |
|      506 |  3807 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3808 | `	}` |
|      600 |  3809 | `	VmPopOperand(&pTos,1);` |
|      600 |  3810 | `	break;` |
|        - |  3811 | `				 }` |
|        - |  3812 | `/* OP_SUB_STORE * * *` |
|        - |  3813 | ` *` |
|        - |  3814 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3815 | ` * first (what was next on the stack) from the second (the` |
|        - |  3816 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3817 | ` */` |
|        1 |  3818 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3819 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3820 | `	ph7_value *pObj;` |
|        - |  3821 | `#ifdef UNTRUST` |
|        - |  3822 | `	if( pNos < pStack ){` |
|        - |  3823 | `		goto Abort;` |
|        - |  3824 | `	}` |
|        - |  3825 | `#endif` |
|        3 |  3826 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3827 | `		/* Floating point arithemic */` |
|        - |  3828 | `		ph7_real a,b,r;` |
|      ! 0 |  3829 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3830 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3831 | `		}` |
|      ! 0 |  3832 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3833 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3834 | `		}` |
|      ! 0 |  3835 | `		a = pTos->rVal;` |
|      ! 0 |  3836 | `		b = pNos->rVal;` |
|      ! 0 |  3837 | `		r = a - b;` |
|        - |  3838 | `		/* Push the result */` |
|      ! 0 |  3839 | `		pNos->rVal = r;` |
|      ! 0 |  3840 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3841 | `		/* Try to get an integer representation */` |
|      ! 0 |  3842 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3843 | `	}else{` |
|        - |  3844 | `		/* Integer arithmetic */` |
|        - |  3845 | `		sxi64 a,b,r;` |
|        3 |  3846 | `		a = pTos->x.iVal;` |
|        3 |  3847 | `		b = pNos->x.iVal;` |
|        3 |  3848 | `		r = a - b;` |
|        - |  3849 | `		/* Push the result */` |
|        3 |  3850 | `		pNos->x.iVal = r;` |
|        3 |  3851 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3852 | `	}` |
|        3 |  3853 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3854 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3855 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3856 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3857 | `	}` |
|        3 |  3858 | `	VmPopOperand(&pTos,1);` |
|        3 |  3859 | `	break;` |
|        - |  3860 | `				 }` |
|        - |  3861 |  |
|        - |  3862 | `/*` |
|        - |  3863 | ` * OP_MOD * * *` |
|        - |  3864 | ` *` |
|        - |  3865 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3866 | ` * first (what was next on the stack) from the second (the` |
|        - |  3867 | ` * top of the stack) and push the remainder after division` |
|        - |  3868 | ` * onto the stack.` |
|        - |  3869 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3870 | ` */` |
|      296 |  3871 | `case PH7_OP_MOD:{` |
|      594 |  3872 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3873 | `	sxi64 a,b,r;` |
|        - |  3874 | `#ifdef UNTRUST` |
|        - |  3875 | `	if( pNos < pStack ){` |
|        - |  3876 | `		goto Abort;` |
|        - |  3877 | `	}` |
|        - |  3878 | `#endif` |
|        - |  3879 | `	/* Force the operands to be integer */` |
|      594 |  3880 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3881 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3882 | `	}` |
|      594 |  3883 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3884 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3885 | `	}` |
|        - |  3886 | `	/* Perform the requested operation */` |
|      594 |  3887 | `	a = pNos->x.iVal;` |
|      594 |  3888 | `	b = pTos->x.iVal;` |
|      594 |  3889 | `	if( b == 0 ){` |
|        3 |  3890 | `		r = 0;` |
|        3 |  3891 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3892 | `		/* goto Abort; */` |
|        2 |  3893 | `	}else{` |
|      591 |  3894 | `		r = a%b;` |
|        - |  3895 | `	}` |
|        - |  3896 | `	/* Push the result */` |
|      594 |  3897 | `	pNos->x.iVal = r;` |
|      594 |  3898 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3899 | `	VmPopOperand(&pTos,1);` |
|      594 |  3900 | `	break;` |
|        - |  3901 | `				}` |
|        - |  3902 | `/*` |
|        - |  3903 | ` * OP_MOD_STORE * * *` |
|        - |  3904 | ` *` |
|        - |  3905 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3906 | ` * first (what was next on the stack) from the second (the` |
|        - |  3907 | ` * top of the stack) and push the remainder after division` |
|        - |  3908 | ` * onto the stack.` |
|        - |  3909 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3910 | ` */` |
|        1 |  3911 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3912 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3913 | `	ph7_value *pObj;` |
|        - |  3914 | `	sxi64 a,b,r;` |
|        - |  3915 | `#ifdef UNTRUST` |
|        - |  3916 | `	if( pNos < pStack ){` |
|        - |  3917 | `		goto Abort;` |
|        - |  3918 | `	}` |
|        - |  3919 | `#endif` |
|        - |  3920 | `	/* Force the operands to be integer */` |
|        3 |  3921 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3922 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3923 | `	}` |
|        3 |  3924 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3925 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3926 | `	}` |
|        - |  3927 | `	/* Perform the requested operation */` |
|        3 |  3928 | `	a = pTos->x.iVal;` |
|        3 |  3929 | `	b = pNos->x.iVal;` |
|        3 |  3930 | `	if( b == 0 ){` |
|      ! 0 |  3931 | `		r = 0;` |
|      ! 0 |  3932 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3933 | `		/* goto Abort; */` |
|      ! 0 |  3934 | `	}else{` |
|        3 |  3935 | `		r = a%b;` |
|        - |  3936 | `	}` |
|        - |  3937 | `	/* Push the result */` |
|        3 |  3938 | `	pNos->x.iVal = r;` |
|        3 |  3939 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3940 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3941 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3942 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3943 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3944 | `	}` |
|        3 |  3945 | `	VmPopOperand(&pTos,1);` |
|        3 |  3946 | `	break;` |
|        - |  3947 | `				}` |
|        - |  3948 | `/*` |
|        - |  3949 | ` * OP_DIV * * *` |
|        - |  3950 | ` *` |
|        - |  3951 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3952 | ` * first (what was next on the stack) from the second (the` |
|        - |  3953 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3954 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3955 | ` */` |
|       28 |  3956 | `case PH7_OP_DIV:{` |
|       58 |  3957 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3958 | `	ph7_real a,b,r;` |
|        - |  3959 | `#ifdef UNTRUST` |
|        - |  3960 | `	if( pNos < pStack ){` |
|        - |  3961 | `		goto Abort;` |
|        - |  3962 | `	}` |
|        - |  3963 | `#endif` |
|        - |  3964 | `	/* Force the operands to be real */` |
|       58 |  3965 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3966 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3967 | `	}` |
|       58 |  3968 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3969 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3970 | `	}` |
|        - |  3971 | `	/* Perform the requested operation */` |
|       58 |  3972 | `	a = pNos->rVal;` |
|       58 |  3973 | `	b = pTos->rVal;` |
|       58 |  3974 | `	if( b == 0 ){` |
|        - |  3975 | `		/* Division by zero */` |
|        3 |  3976 | `		pNos->rVal = 0;` |
|        3 |  3977 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3978 | `		/* goto Abort; */` |
|        2 |  3979 | `	}else{` |
|       55 |  3980 | `		r = a/b;` |
|        - |  3981 | `		/* Push the result */` |
|       55 |  3982 | `		pNos->rVal = r;` |
|       55 |  3983 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3984 | `		/* Try to get an integer representation */` |
|       55 |  3985 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3986 | `	}` |
|       58 |  3987 | `	VmPopOperand(&pTos,1);` |
|       58 |  3988 | `	break;` |
|        - |  3989 | `				}` |
|        - |  3990 | `/*` |
|        - |  3991 | ` * OP_DIV_STORE * * *` |
|        - |  3992 | ` *` |
|        - |  3993 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3994 | ` * first (what was next on the stack) from the second (the` |
|        - |  3995 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3996 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3997 | ` */` |
|        1 |  3998 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3999 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4000 | `	ph7_value *pObj;` |
|        - |  4001 | `	ph7_real a,b,r;` |
|        - |  4002 | `#ifdef UNTRUST` |
|        - |  4003 | `	if( pNos < pStack ){` |
|        - |  4004 | `		goto Abort;` |
|        - |  4005 | `	}` |
|        - |  4006 | `#endif` |
|        - |  4007 | `	/* Force the operands to be real */` |
|        3 |  4008 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4009 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4010 | `	}` |
|        3 |  4011 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4012 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4013 | `	}` |
|        - |  4014 | `	/* Perform the requested operation */` |
|        3 |  4015 | `	a = pTos->rVal;` |
|        3 |  4016 | `	b = pNos->rVal;` |
|        3 |  4017 | `	if( b == 0 ){` |
|        - |  4018 | `		/* Division by zero */` |
|      ! 0 |  4019 | `		r = 0;` |
|      ! 0 |  4020 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4021 | `		/* goto Abort; */` |
|      ! 0 |  4022 | `	}else{` |
|        3 |  4023 | `		r = a/b;` |
|        - |  4024 | `		/* Push the result */` |
|        3 |  4025 | `		pNos->rVal = r;` |
|        3 |  4026 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4027 | `		/* Try to get an integer representation */` |
|        3 |  4028 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4029 | `	}` |
|        3 |  4030 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4031 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4032 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4033 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4034 | `	}` |
|        3 |  4035 | `	VmPopOperand(&pTos,1);` |
|        3 |  4036 | `	break;` |
|        - |  4037 | `				}` |
|        - |  4038 | `/* OP_BAND * * *` |
|        - |  4039 | ` *` |
|        - |  4040 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4041 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4042 | ` * two elements.` |
|        - |  4043 | `*/` |
|        - |  4044 | `/* OP_BOR * * *` |
|        - |  4045 | ` *` |
|        - |  4046 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4047 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4048 | ` * two elements.` |
|        - |  4049 | ` */` |
|        - |  4050 | `/* OP_BXOR * * *` |
|        - |  4051 | ` *` |
|        - |  4052 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4053 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4054 | ` * two elements.` |
|        - |  4055 | ` */` |
|       30 |  4056 | `case PH7_OP_BAND:` |
|        - |  4057 | `case PH7_OP_BOR:` |
|        - |  4058 | `case PH7_OP_BXOR:{` |
|       62 |  4059 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4060 | `	sxi64 a,b,r;` |
|        - |  4061 | `#ifdef UNTRUST` |
|        - |  4062 | `	if( pNos < pStack ){` |
|        - |  4063 | `		goto Abort;` |
|        - |  4064 | `	}` |
|        - |  4065 | `#endif` |
|        - |  4066 | `	/* Force the operands to be integer */` |
|       62 |  4067 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4068 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4069 | `	}` |
|       62 |  4070 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4071 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4072 | `	}` |
|        - |  4073 | `	/* Perform the requested operation */` |
|       62 |  4074 | `	a = pNos->x.iVal;` |
|       62 |  4075 | `	b = pTos->x.iVal;` |
|       62 |  4076 | `	switch(pInstr->iOp){` |
|        6 |  4077 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4078 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4079 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4080 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4081 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4082 | `	case PH7_OP_BAND:` |
|       38 |  4083 | `	default:          r = a&b; break;` |
|        - |  4084 | `	}` |
|        - |  4085 | `	/* Push the result */` |
|       62 |  4086 | `	pNos->x.iVal = r;` |
|       62 |  4087 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4088 | `	VmPopOperand(&pTos,1);` |
|       62 |  4089 | `	break;` |
|        - |  4090 | `				 }` |
|        - |  4091 | `/* OP_BAND_STORE * * *` |
|        - |  4092 | ` *` |
|        - |  4093 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4094 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4095 | ` * two elements.` |
|        - |  4096 | `*/` |
|        - |  4097 | `/* OP_BOR_STORE * * *` |
|        - |  4098 | ` *` |
|        - |  4099 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4100 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4101 | ` * two elements.` |
|        - |  4102 | ` */` |
|        - |  4103 | `/* OP_BXOR_STORE * * *` |
|        - |  4104 | ` *` |
|        - |  4105 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4106 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4107 | ` * two elements.` |
|        - |  4108 | ` */` |
|        7 |  4109 | `case PH7_OP_BAND_STORE:` |
|        - |  4110 | `case PH7_OP_BOR_STORE:` |
|        - |  4111 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4112 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4113 | `	ph7_value *pObj;` |
|        - |  4114 | `	sxi64 a,b,r;` |
|        - |  4115 | `#ifdef UNTRUST` |
|        - |  4116 | `	if( pNos < pStack ){` |
|        - |  4117 | `		goto Abort;` |
|        - |  4118 | `	}` |
|        - |  4119 | `#endif` |
|        - |  4120 | `	/* Force the operands to be integer */` |
|       15 |  4121 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4122 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4123 | `	}` |
|       15 |  4124 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4125 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4126 | `	}` |
|        - |  4127 | `	/* Perform the requested operation */` |
|       15 |  4128 | `	a = pTos->x.iVal;` |
|       15 |  4129 | `	b = pNos->x.iVal;` |
|       15 |  4130 | `	switch(pInstr->iOp){` |
|        2 |  4131 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4132 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4133 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4134 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4135 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4136 | `	case PH7_OP_BAND:` |
|        5 |  4137 | `	default:          r = a&b; break;` |
|        - |  4138 | `	}` |
|        - |  4139 | `	/* Push the result */` |
|       15 |  4140 | `	pNos->x.iVal = r;` |
|       15 |  4141 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4142 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4143 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4144 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4145 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4146 | `	}` |
|       15 |  4147 | `	VmPopOperand(&pTos,1);` |
|       15 |  4148 | `	break;` |
|        - |  4149 | `				 }` |
|        - |  4150 | `/* OP_SHL * * *` |
|        - |  4151 | ` *` |
|        - |  4152 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4153 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4154 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4155 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4156 | ` */` |
|        - |  4157 | `/* OP_SHR * * *` |
|        - |  4158 | ` *` |
|        - |  4159 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4160 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4161 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4162 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4163 | ` */` |
|        9 |  4164 | `case PH7_OP_SHL:` |
|        - |  4165 | `case PH7_OP_SHR: {` |
|       19 |  4166 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4167 | `	sxi64 a,r;` |
|        - |  4168 | `	sxi32 b;` |
|        - |  4169 | `#ifdef UNTRUST` |
|        - |  4170 | `	if( pNos < pStack ){` |
|        - |  4171 | `		goto Abort;` |
|        - |  4172 | `	}` |
|        - |  4173 | `#endif` |
|        - |  4174 | `	/* Force the operands to be integer */` |
|       19 |  4175 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4176 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4177 | `	}` |
|       19 |  4178 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4179 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4180 | `	}` |
|        - |  4181 | `	/* Perform the requested operation */` |
|       19 |  4182 | `	a = pNos->x.iVal;` |
|       19 |  4183 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4184 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4185 | `		r = a << b;` |
|        6 |  4186 | `	}else{` |
|        9 |  4187 | `		r = a >> b;` |
|        - |  4188 | `	}` |
|        - |  4189 | `	/* Push the result */` |
|       19 |  4190 | `	pNos->x.iVal = r;` |
|       19 |  4191 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4192 | `	VmPopOperand(&pTos,1);` |
|       19 |  4193 | `	break;` |
|        - |  4194 | `				 }` |
|        - |  4195 | `/*  OP_SHL_STORE * * *` |
|        - |  4196 | ` *` |
|        - |  4197 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4198 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4199 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4200 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4201 | ` */` |
|        - |  4202 | `/* OP_SHR_STORE * * *` |
|        - |  4203 | ` *` |
|        - |  4204 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4205 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4206 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4207 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4208 | ` */` |
|        7 |  4209 | `case PH7_OP_SHL_STORE:` |
|        - |  4210 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4211 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4212 | `	ph7_value *pObj;` |
|        - |  4213 | `	sxi64 a,r;` |
|        - |  4214 | `	sxi32 b;` |
|        - |  4215 | `#ifdef UNTRUST` |
|        - |  4216 | `	if( pNos < pStack ){` |
|        - |  4217 | `		goto Abort;` |
|        - |  4218 | `	}` |
|        - |  4219 | `#endif` |
|        - |  4220 | `	/* Force the operands to be integer */` |
|       15 |  4221 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4222 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4223 | `	}` |
|       15 |  4224 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4225 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4226 | `	}` |
|        - |  4227 | `	/* Perform the requested operation */` |
|       15 |  4228 | `	a = pTos->x.iVal;` |
|       15 |  4229 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4230 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4231 | `		r = a << b;` |
|        4 |  4232 | `	}else{` |
|        9 |  4233 | `		r = a >> b;` |
|        - |  4234 | `	}` |
|        - |  4235 | `	/* Push the result */` |
|       15 |  4236 | `	pNos->x.iVal = r;` |
|       15 |  4237 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4238 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4239 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4240 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4241 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4242 | `	}` |
|       15 |  4243 | `	VmPopOperand(&pTos,1);` |
|       15 |  4244 | `	break;` |
|        - |  4245 | `				 }` |
|        - |  4246 | `/* CAT:  P1 * *` |
|        - |  4247 | ` *` |
|        - |  4248 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4249 | ` * back.` |
|        - |  4250 | ` */` |
|    61282 |  4251 | `case PH7_OP_CAT:{` |
|        - |  4252 | `	ph7_value *pNos,*pCur;` |
|   122566 |  4253 | `	if( pInstr->iP1 < 1 ){` |
|    95628 |  4254 | `		pNos = &pTos[-1];` |
|    47815 |  4255 | `	}else{` |
|    26940 |  4256 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4257 | `	}` |
|        - |  4258 | `#ifdef UNTRUST` |
|        - |  4259 | `	if( pNos < pStack ){` |
|        - |  4260 | `		goto Abort;` |
|        - |  4261 | `	}` |
|        - |  4262 | `#endif` |
|        - |  4263 | `	/* Force a string cast */` |
|   122566 |  4264 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1014 |  4265 | `		PH7_MemObjToString(pNos);` |
|      506 |  4266 | `	}` |
|   122566 |  4267 | `	pCur = &pNos[1];` |
|   247062 |  4268 | `	while( pCur <= pTos ){` |
|   124498 |  4269 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50500 |  4270 | `			PH7_MemObjToString(pCur);` |
|    25249 |  4271 | `		}` |
|        - |  4272 | `		/* Perform the concatenation */` |
|   124498 |  4273 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   124460 |  4274 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    62229 |  4275 | `		}` |
|   124498 |  4276 | `		SyBlobRelease(&pCur->sBlob);` |
|   124498 |  4277 | `		pCur++;` |
|        2 |  4278 | `	}` |
|   122566 |  4279 | `	pTos = pNos;` |
|   122566 |  4280 | `	break;` |
|        - |  4281 | `				}` |
|        - |  4282 | `/*  CAT_STORE: * * *` |
|        - |  4283 | ` *` |
|        - |  4284 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4285 | ` * back.` |
|        - |  4286 | ` */` |
|     3312 |  4287 | `case PH7_OP_CAT_STORE:{` |
|     6626 |  4288 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4289 | `	ph7_value *pObj;` |
|        - |  4290 | `#ifdef UNTRUST` |
|        - |  4291 | `	if( pNos < pStack ){` |
|        - |  4292 | `		goto Abort;` |
|        - |  4293 | `	}` |
|        - |  4294 | `#endif` |
|     6626 |  4295 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4296 | `		/* Force a string cast */` |
|      ! 0 |  4297 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4298 | `	}` |
|     6626 |  4299 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4300 | `		/* Force a string cast */` |
|      ! 0 |  4301 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4302 | `	}` |
|        - |  4303 | `	/* Perform the concatenation (Reverse order) */` |
|     6626 |  4304 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6626 |  4305 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3312 |  4306 | `	}` |
|        - |  4307 | `	/* Perform the store operation */` |
|     6626 |  4308 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4309 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6626 |  4310 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6626 |  4311 | `		PH7_MemObjStore(pTos,pObj);` |
|     3312 |  4312 | `	}` |
|     6626 |  4313 | `	PH7_MemObjStore(pTos,pNos);` |
|     6626 |  4314 | `	VmPopOperand(&pTos,1);` |
|     6626 |  4315 | `	break;` |
|        - |  4316 | `				}` |
|        - |  4317 | `/* OP_AND: * * *` |
|        - |  4318 | ` *` |
|        - |  4319 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4320 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4321 | ` * stack.` |
|        - |  4322 | ` */` |
|        - |  4323 | `/* OP_OR: * * *` |
|        - |  4324 | ` *` |
|        - |  4325 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4326 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4327 | ` * stack.` |
|        - |  4328 | ` */` |
|    93688 |  4329 | `case PH7_OP_LAND:` |
|        - |  4330 | `case PH7_OP_LOR: {` |
|   187422 |  4331 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4332 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4333 | `#ifdef UNTRUST` |
|        - |  4334 | `	if( pNos < pStack ){` |
|        - |  4335 | `		goto Abort;` |
|        - |  4336 | `	}` |
|        - |  4337 | `#endif` |
|        - |  4338 | `	/* Force a boolean cast */` |
|   187422 |  4339 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4340 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4341 | `	}` |
|   187422 |  4342 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4343 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4344 | `	}` |
|   187422 |  4345 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   187422 |  4346 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   187422 |  4347 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4348 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    85472 |  4349 | `		v1 = and_logic[v1*3+v2];` |
|    42759 |  4350 | `	}else{` |
|        - |  4351 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   101952 |  4352 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4353 | `	}` |
|   187422 |  4354 | `	if( v1 == 2 ){` |
|      ! 0 |  4355 | `		v1 = 1;` |
|      ! 0 |  4356 | `	}` |
|   187422 |  4357 | `	VmPopOperand(&pTos,1);` |
|   187422 |  4358 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   187422 |  4359 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   187422 |  4360 | `	break;` |
|        - |  4361 | `				 }` |
|        - |  4362 | `/* OP_LXOR: * * *` |
|        - |  4363 | ` *` |
|        - |  4364 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4365 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4366 | ` * stack.` |
|        - |  4367 | ` * According to the PHP language reference manual:` |
|        - |  4368 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4369 | ` *  TRUE,but not both.` |
|        - |  4370 | ` */` |
|        5 |  4371 | `case PH7_OP_LXOR:{` |
|       11 |  4372 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4373 | `	sxi32 v = 0;` |
|        - |  4374 | `#ifdef UNTRUST` |
|        - |  4375 | `	if( pNos < pStack ){` |
|        - |  4376 | `		goto Abort;` |
|        - |  4377 | `	}` |
|        - |  4378 | `#endif` |
|        - |  4379 | `	/* Force a boolean cast */` |
|       11 |  4380 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4381 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4382 | `	}` |
|       11 |  4383 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4384 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4385 | `	}` |
|       11 |  4386 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4387 | `		v = 1;` |
|        3 |  4388 | `	}` |
|       11 |  4389 | `	VmPopOperand(&pTos,1);` |
|       11 |  4390 | `	pTos->x.iVal = v;` |
|       11 |  4391 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4392 | `	break;` |
|        - |  4393 | `				 }` |
|        - |  4394 | `/* OP_EQ P1 P2 P3` |
|        - |  4395 | ` *` |
|        - |  4396 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4397 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4398 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4399 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4400 | ` */` |
|        - |  4401 | `/* OP_NEQ P1 P2 P3` |
|        - |  4402 | ` *` |
|        - |  4403 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4404 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4405 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4406 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4407 | ` */` |
|     3803 |  4408 | `case PH7_OP_EQ:` |
|        - |  4409 | `case PH7_OP_NEQ: {` |
|     7608 |  4410 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4411 | `	/* Perform the comparison and act accordingly */` |
|        - |  4412 | `#ifdef UNTRUST` |
|        - |  4413 | `	if( pNos < pStack ){` |
|        - |  4414 | `		goto Abort;` |
|        - |  4415 | `	}` |
|        - |  4416 | `#endif` |
|     7608 |  4417 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7608 |  4418 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4419 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7599 |  4420 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7564 |  4421 | `		rc = rc == 0;` |
|     3783 |  4422 | `	}else{` |
|       28 |  4423 | `		rc = rc != 0;` |
|        - |  4424 | `	}` |
|     7608 |  4425 | `	VmPopOperand(&pTos,1);` |
|     7608 |  4426 | `	if( !pInstr->iP2 ){` |
|        - |  4427 | `		/* Push comparison result without taking the jump */` |
|     7608 |  4428 | `		PH7_MemObjRelease(pTos);` |
|     7608 |  4429 | `		pTos->x.iVal = rc;` |
|        - |  4430 | `		/* Invalidate any prior representation */` |
|     7608 |  4431 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3805 |  4432 | `	}else{` |
|      ! 0 |  4433 | `		if( rc ){` |
|        - |  4434 | `			/* Jump to the desired location */` |
|      ! 0 |  4435 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4436 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4437 | `		}` |
|        - |  4438 | `	}` |
|     7608 |  4439 | `	break;` |
|        - |  4440 | `				 }` |
|        - |  4441 | `/* OP_TEQ P1 P2 *` |
|        - |  4442 | ` *` |
|        - |  4443 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4444 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4445 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4446 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4447 | ` */` |
|   128482 |  4448 | `case PH7_OP_TEQ: {` |
|   256966 |  4449 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4450 | `	/* Perform the comparison and act accordingly */` |
|        - |  4451 | `#ifdef UNTRUST` |
|        - |  4452 | `	if( pNos < pStack ){` |
|        - |  4453 | `		goto Abort;` |
|        - |  4454 | `	}` |
|        - |  4455 | `#endif` |
|   256966 |  4456 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   256966 |  4457 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4458 | `		rc = 0;` |
|        2 |  4459 | `	}else{` |
|   256964 |  4460 | `		rc = rc == 0;` |
|        - |  4461 | `	}` |
|   256966 |  4462 | `	VmPopOperand(&pTos,1);` |
|   256966 |  4463 | `	if( !pInstr->iP2 ){` |
|        - |  4464 | `		/* Push comparison result without taking the jump */` |
|   256966 |  4465 | `		PH7_MemObjRelease(pTos);` |
|   256966 |  4466 | `		pTos->x.iVal = rc;` |
|        - |  4467 | `		/* Invalidate any prior representation */` |
|   256966 |  4468 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   128484 |  4469 | `	}else{` |
|      ! 0 |  4470 | `		if( rc ){` |
|        - |  4471 | `			/* Jump to the desired location */` |
|      ! 0 |  4472 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4473 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4474 | `		}` |
|        - |  4475 | `	}` |
|   256966 |  4476 | `	break;` |
|        - |  4477 | `				 }` |
|        - |  4478 | `/* OP_TNE P1 P2 *` |
|        - |  4479 | ` *` |
|        - |  4480 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4481 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4482 | ` * instruction.` |
|        - |  4483 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4484 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4485 | ` *` |
|        - |  4486 | ` */` |
|   100368 |  4487 | `case PH7_OP_TNE: {` |
|   200738 |  4488 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4489 | `	/* Perform the comparison and act accordingly */` |
|        - |  4490 | `#ifdef UNTRUST` |
|        - |  4491 | `	if( pNos < pStack ){` |
|        - |  4492 | `		goto Abort;` |
|        - |  4493 | `	}` |
|        - |  4494 | `#endif` |
|   200738 |  4495 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   200738 |  4496 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4497 | `		rc = 1;` |
|        2 |  4498 | `	}else{` |
|   200736 |  4499 | `		rc = rc != 0;` |
|        - |  4500 | `	}` |
|   200738 |  4501 | `	VmPopOperand(&pTos,1);` |
|   200738 |  4502 | `	if( !pInstr->iP2 ){` |
|        - |  4503 | `		/* Push comparison result without taking the jump */` |
|   200738 |  4504 | `		PH7_MemObjRelease(pTos);` |
|   200738 |  4505 | `		pTos->x.iVal = rc;` |
|        - |  4506 | `		/* Invalidate any prior representation */` |
|   200738 |  4507 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100370 |  4508 | `	}else{` |
|      ! 0 |  4509 | `		if( rc ){` |
|        - |  4510 | `			/* Jump to the desired location */` |
|      ! 0 |  4511 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4512 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4513 | `		}` |
|        - |  4514 | `	}` |
|   200738 |  4515 | `	break;` |
|        - |  4516 | `				 }` |
|        - |  4517 | `/* OP_LT P1 P2 P3` |
|        - |  4518 | ` *` |
|        - |  4519 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4520 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4521 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4522 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4523 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4524 | ` *` |
|        - |  4525 | ` */` |
|        - |  4526 | `/* OP_LE P1 P2 P3` |
|        - |  4527 | ` *` |
|        - |  4528 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4529 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4530 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4531 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4532 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4533 | ` *` |
|        - |  4534 | ` */` |
|   102250 |  4535 | `case PH7_OP_LT:` |
|        - |  4536 | `case PH7_OP_LE: {` |
|   204546 |  4537 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4538 | `	/* Perform the comparison and act accordingly */` |
|        - |  4539 | `#ifdef UNTRUST` |
|        - |  4540 | `	if( pNos < pStack ){` |
|        - |  4541 | `		goto Abort;` |
|        - |  4542 | `	}` |
|        - |  4543 | `#endif` |
|   204546 |  4544 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204546 |  4545 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4546 | `		rc = 0;` |
|   204542 |  4547 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      408 |  4548 | `		rc = rc < 1;` |
|      205 |  4549 | `	}else{` |
|   204132 |  4550 | `		rc = rc < 0;` |
|        - |  4551 | `	}` |
|   204546 |  4552 | `	VmPopOperand(&pTos,1);` |
|   204546 |  4553 | `	if( !pInstr->iP2 ){` |
|        - |  4554 | `		/* Push comparison result without taking the jump */` |
|   204546 |  4555 | `		PH7_MemObjRelease(pTos);` |
|   204546 |  4556 | `		pTos->x.iVal = rc;` |
|        - |  4557 | `		/* Invalidate any prior representation */` |
|   204546 |  4558 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102296 |  4559 | `	}else{` |
|      ! 0 |  4560 | `		if( rc ){` |
|        - |  4561 | `			/* Jump to the desired location */` |
|      ! 0 |  4562 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4563 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4564 | `		}` |
|        - |  4565 | `	}` |
|   204546 |  4566 | `	break;` |
|        - |  4567 | `				}` |
|        - |  4568 | `/* OP_GT P1 P2 P3` |
|        - |  4569 | ` *` |
|        - |  4570 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4571 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4572 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4573 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4574 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4575 | ` *` |
|        - |  4576 | ` */` |
|        - |  4577 | `/* OP_GE P1 P2 P3` |
|        - |  4578 | ` *` |
|        - |  4579 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4580 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4581 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4582 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4583 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4584 | ` *` |
|        - |  4585 | ` */` |
|    48630 |  4586 | `case PH7_OP_GT:` |
|        - |  4587 | `case PH7_OP_GE: {` |
|    97262 |  4588 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4589 | `	/* Perform the comparison and act accordingly */` |
|        - |  4590 | `#ifdef UNTRUST` |
|        - |  4591 | `	if( pNos < pStack ){` |
|        - |  4592 | `		goto Abort;` |
|        - |  4593 | `	}` |
|        - |  4594 | `#endif` |
|    97262 |  4595 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97262 |  4596 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4597 | `		rc = 0;` |
|    97258 |  4598 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97106 |  4599 | `		rc = rc >= 0;` |
|    48554 |  4600 | `	}else{` |
|      150 |  4601 | `		rc = rc > 0;` |
|        - |  4602 | `	}` |
|    97262 |  4603 | `	VmPopOperand(&pTos,1);` |
|    97262 |  4604 | `	if( !pInstr->iP2 ){` |
|        - |  4605 | `		/* Push comparison result without taking the jump */` |
|    97262 |  4606 | `		PH7_MemObjRelease(pTos);` |
|    97262 |  4607 | `		pTos->x.iVal = rc;` |
|        - |  4608 | `		/* Invalidate any prior representation */` |
|    97262 |  4609 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48632 |  4610 | `	}else{` |
|      ! 0 |  4611 | `		if( rc ){` |
|        - |  4612 | `			/* Jump to the desired location */` |
|      ! 0 |  4613 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4614 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4615 | `		}` |
|        - |  4616 | `	}` |
|    97262 |  4617 | `	break;` |
|        - |  4618 | `				}` |
|        - |  4619 | `/* OP_SEQ P1 P2 *` |
|        - |  4620 | ` * Strict string comparison.` |
|        - |  4621 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4622 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4623 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4624 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4625 | ` * use PH7_OP_EQ.` |
|        - |  4626 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4627 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4628 | ` */` |
|        - |  4629 | `/* OP_SNE P1 P2 *` |
|        - |  4630 | ` * Strict string comparison.` |
|        - |  4631 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4632 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4633 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4634 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4635 | ` * use PH7_OP_EQ.` |
|        - |  4636 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4637 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4638 | ` */` |
|       18 |  4639 | `case PH7_OP_SEQ:` |
|        - |  4640 | `case PH7_OP_SNE: {` |
|       38 |  4641 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4642 | `	SyString s1,s2;` |
|        - |  4643 | `	/* Perform the comparison and act accordingly */` |
|        - |  4644 | `#ifdef UNTRUST` |
|        - |  4645 | `	if( pNos < pStack ){` |
|        - |  4646 | `		goto Abort;` |
|        - |  4647 | `	}` |
|        - |  4648 | `#endif` |
|        - |  4649 | `	/* Force a string cast */` |
|       38 |  4650 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4651 | `		PH7_MemObjToString(pTos);` |
|        2 |  4652 | `	}` |
|       38 |  4653 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4654 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4655 | `	}` |
|       38 |  4656 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4657 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4658 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4659 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4660 | `		rc = rc != 0;` |
|      ! 0 |  4661 | `	}else{` |
|       38 |  4662 | `		rc = rc == 0;` |
|        - |  4663 | `	}` |
|       38 |  4664 | `	VmPopOperand(&pTos,1);` |
|       38 |  4665 | `	if( !pInstr->iP2 ){` |
|        - |  4666 | `		/* Push comparison result without taking the jump */` |
|       38 |  4667 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4668 | `		pTos->x.iVal = rc;` |
|        - |  4669 | `		/* Invalidate any prior representation */` |
|       38 |  4670 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4671 | `	}else{` |
|      ! 0 |  4672 | `		if( rc ){` |
|        - |  4673 | `			/* Jump to the desired location */` |
|      ! 0 |  4674 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4675 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4676 | `		}` |
|        - |  4677 | `	}` |
|       38 |  4678 | `	break;` |
|        - |  4679 | `				 }` |
|        - |  4680 | `/*` |
|        - |  4681 | ` * OP_LOAD_REF * * *` |
|        - |  4682 | ` * Push the index of a referenced object on the stack.` |
|        - |  4683 | ` */` |
|       57 |  4684 | `case PH7_OP_LOAD_REF: {` |
|        - |  4685 | `	sxu32 nIdx;` |
|        - |  4686 | `#ifdef UNTRUST` |
|        - |  4687 | `	if( pTos < pStack ){` |
|        - |  4688 | `		goto Abort;` |
|        - |  4689 | `	}` |
|        - |  4690 | `#endif` |
|        - |  4691 | `	/* Extract memory object index */` |
|      115 |  4692 | `	nIdx = pTos->nIdx;` |
|      115 |  4693 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4694 | `		/* Nullify the object */` |
|       95 |  4695 | `		PH7_MemObjRelease(pTos);` |
|        - |  4696 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4697 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4698 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4699 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4700 | `	}` |
|      115 |  4701 | `	break;` |
|        - |  4702 | `					  }` |
|        - |  4703 | `/*` |
|        - |  4704 | ` * OP_STORE_REF * * P3` |
|        - |  4705 | ` * Perform an assignment operation by reference.` |
|        - |  4706 | ` */` |
|       14 |  4707 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4708 | `	 SyString sName = { 0 , 0 };` |
|        - |  4709 | `	 VmFrame *pFrameLocal;` |
|        - |  4710 | `	SyHashEntry *pEntry;` |
|        - |  4711 | `	sxu32 nIdx;` |
|        - |  4712 | `#ifdef UNTRUST` |
|        - |  4713 | `	if( pTos < pStack ){` |
|        - |  4714 | `		goto Abort;` |
|        - |  4715 | `	}` |
|        - |  4716 | `#endif` |
|       30 |  4717 | `	if( pInstr->p3 == 0 ){` |
|        - |  4718 | `		char *zName;` |
|        - |  4719 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4720 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4721 | `			/* Force a string cast */` |
|      ! 0 |  4722 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4723 | `		}` |
|      ! 0 |  4724 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4725 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4726 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4727 | `			if( zName ){` |
|      ! 0 |  4728 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4729 | `			}` |
|      ! 0 |  4730 | `		}` |
|      ! 0 |  4731 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4732 | `		pTos--;` |
|      ! 0 |  4733 | `	}else{` |
|       30 |  4734 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4735 | `	}` |
|       30 |  4736 | `	nIdx = pTos->nIdx;` |
|       30 |  4737 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4738 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4739 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4740 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4741 | `		}else{` |
|        - |  4742 | `			ph7_value *pObj;` |
|        - |  4743 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4744 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4745 | `			if( pObj == 0 ){` |
|      ! 0 |  4746 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4747 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4748 | `				goto Abort;` |
|        - |  4749 | `			}` |
|        - |  4750 | `			/* Perform the store operation */` |
|      ! 0 |  4751 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4752 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4753 | `		}` |
|       30 |  4754 | `	}else if( sName.nByte > 0){` |
|       30 |  4755 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4756 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4757 | `		}else{` |
|       30 |  4758 | `			pFrameLocal = pVm->pFrame;` |
|       30 |  4759 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4760 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  4761 | `				pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4762 | `			}` |
|        - |  4763 | `			/* Query the local frame */` |
|       30 |  4764 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4765 | `			if( pEntry ){` |
|      ! 0 |  4766 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4767 | `			}else{` |
|       30 |  4768 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4769 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4770 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4771 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4772 | `				}` |
|       30 |  4773 | `				if( rc == SXRET_OK ){` |
|       30 |  4774 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4775 | `				}` |
|        - |  4776 | `			}` |
|        - |  4777 | `		}` |
|       14 |  4778 | `	}` |
|       30 |  4779 | `	break;` |
|        - |  4780 | `				 }` |
|        - |  4781 | `/*` |
|        - |  4782 | ` * OP_UPLINK P1 * *` |
|        - |  4783 | ` * Link a variable to the top active VM frame.` |
|        - |  4784 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4785 | ` */` |
|       25 |  4786 | `case PH7_OP_UPLINK: {` |
|       52 |  4787 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4788 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4789 | `		SyString sName;` |
|        - |  4790 | `		/* Perform the link */` |
|      104 |  4791 | `		while( pLink <= pTos ){` |
|       54 |  4792 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4793 | `				/* Force a string cast */` |
|      ! 0 |  4794 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4795 | `			}` |
|       54 |  4796 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4797 | `			if( sName.nByte > 0 ){` |
|       54 |  4798 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4799 | `			}` |
|       54 |  4800 | `			pLink++;` |
|        2 |  4801 | `		}` |
|       25 |  4802 | `	}` |
|       52 |  4803 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4804 | `	break;` |
|        - |  4805 | `					}` |
|        - |  4806 | `/*` |
|        - |  4807 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4808 | ` * Push an exception in the corresponding container so that` |
|        - |  4809 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4810 | ` */` |
|       24 |  4811 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       50 |  4812 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4813 | `	VmFrame *pFrameLocal;` |
|        - |  4814 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       50 |  4815 | `	pException->iFinallyDone = 0;` |
|       50 |  4816 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4817 | `	/* Create the exception frame */` |
|       50 |  4818 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       50 |  4819 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4820 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4821 | `		goto Abort;` |
|        - |  4822 | `	}` |
|        - |  4823 | `	/* Mark the special frame */` |
|       50 |  4824 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       50 |  4825 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4826 | `	/* Point to the frame that trigger the exception */` |
|       50 |  4827 | `	pFrameLocal = pFrameLocal->pParent;` |
|       58 |  4828 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|       10 |  4829 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4830 | `	}` |
|       50 |  4831 | `	pException->pFrame = pFrameLocal;` |
|       50 |  4832 | `	break;` |
|        - |  4833 | `							}` |
|        - |  4834 | `/*` |
|        - |  4835 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4836 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4837 | ` */` |
|       24 |  4838 | `case PH7_OP_POP_EXCEPTION: {` |
|       50 |  4839 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       50 |  4840 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4841 | `		ph7_exception **apException;` |
|        - |  4842 | `		/* Pop the loaded exception */` |
|       22 |  4843 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       22 |  4844 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       20 |  4845 | `			(void)SySetPop(&pVm->aException);` |
|        9 |  4846 | `		}` |
|       10 |  4847 | `	}` |
|       50 |  4848 | `	pException->pFrame = 0;` |
|        - |  4849 | `	/* Leave the exception frame */` |
|       50 |  4850 | `	VmLeaveFrame(&(*pVm));` |
|        - |  4851 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       50 |  4852 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  4853 | `		sxi32 rcFinally;` |
|       13 |  4854 | `		pException->iFinallyDone = 1;` |
|       13 |  4855 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       13 |  4856 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  4857 | `			goto Abort;` |
|        - |  4858 | `		}` |
|        6 |  4859 | `	}` |
|       50 |  4860 | `	break;` |
|        - |  4861 | `							}` |
|        - |  4862 |  |
|        - |  4863 | `/*` |
|        - |  4864 | ` * OP_THROW * P2 *` |
|        - |  4865 | ` * Throw an user exception.` |
|        - |  4866 | ` */` |
|       16 |  4867 | `case PH7_OP_THROW: {` |
|       34 |  4868 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       34 |  4869 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4870 | `#ifdef UNTRUST` |
|        - |  4871 | `	if( pTos < pStack ){` |
|        - |  4872 | `		goto Abort;` |
|        - |  4873 | `	}` |
|        - |  4874 | `#endif` |
|       52 |  4875 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4876 | `		/* Safely ignore the exception frame */` |
|       20 |  4877 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4878 | `	}` |
|        - |  4879 | `	/* Tell the upper layer that an exception was thrown */` |
|       34 |  4880 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       34 |  4881 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  4882 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4883 | `		ph7_class *pException;` |
|        - |  4884 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4885 | `		 */` |
|       34 |  4886 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       34 |  4887 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4888 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4889 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4890 | `			if( rc == SXERR_ABORT ){` |
|        - |  4891 | `				/* Abort processing immediately */` |
|      ! 0 |  4892 | `				goto Abort;` |
|        - |  4893 | `			}` |
|      ! 0 |  4894 | `		}else{` |
|        - |  4895 | `			/* Throw the exception */` |
|       34 |  4896 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       34 |  4897 | `			if( rc == SXERR_ABORT ){` |
|        - |  4898 | `				/* Abort processing immediately */` |
|        9 |  4899 | `				goto Abort;` |
|        - |  4900 | `			}` |
|        - |  4901 | `		}` |
|       14 |  4902 | `	}else{` |
|        - |  4903 | `		/* Expecting a class instance */` |
|      ! 0 |  4904 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4905 | `		if( rc == SXERR_ABORT ){` |
|        - |  4906 | `			/* Abort processing immediately */` |
|      ! 0 |  4907 | `			goto Abort;` |
|        - |  4908 | `		}` |
|        - |  4909 | `	}` |
|        - |  4910 | `	/* Pop the top entry */` |
|       26 |  4911 | `	VmPopOperand(&pTos,1);` |
|        - |  4912 | `	/* Perform an unconditional jump */` |
|       26 |  4913 | `	pc = nJump - 1;` |
|       26 |  4914 | `	break;` |
|        - |  4915 | `				   }` |
|        - |  4916 | `/*` |
|        - |  4917 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4918 | ` * Prepare a foreach step.` |
|        - |  4919 | ` */` |
|     4781 |  4920 | `case PH7_OP_FOREACH_INIT: {` |
|     9564 |  4921 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4922 | `	void *pName;` |
|        - |  4923 | `#ifdef UNTRUST` |
|        - |  4924 | `	if( pTos < pStack ){` |
|        - |  4925 | `		goto Abort;` |
|        - |  4926 | `	}` |
|        - |  4927 | `#endif` |
|     9564 |  4928 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4929 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4930 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4931 | `			/* Force a string cast */` |
|      ! 0 |  4932 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4933 | `		}` |
|        - |  4934 | `		/* Duplicate name */` |
|      ! 0 |  4935 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4936 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4937 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4938 | `		}` |
|      ! 0 |  4939 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4940 | `	}` |
|     9564 |  4941 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4942 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4943 | `			/* Force a string cast */` |
|      ! 0 |  4944 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4945 | `		}` |
|        - |  4946 | `		/* Duplicate name */` |
|      ! 0 |  4947 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4948 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4949 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4950 | `		}` |
|      ! 0 |  4951 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4952 | `	}` |
|        - |  4953 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9564 |  4954 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4955 | `		/* Jump out of the loop */` |
|      ! 0 |  4956 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4957 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4958 | `		}` |
|      ! 0 |  4959 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4960 | `	}else{` |
|        - |  4961 | `		ph7_foreach_step *pStep;` |
|     9564 |  4962 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9564 |  4963 | `		if( pStep == 0 ){` |
|      ! 0 |  4964 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4965 | `			/* Jump out of the loop */` |
|      ! 0 |  4966 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4967 | `		}else{` |
|        - |  4968 | `			/* Zero the structure */` |
|     9564 |  4969 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4970 | `			/* Prepare the step */` |
|     9564 |  4971 | `			pStep->iFlags = pInfo->iFlags;` |
|     9564 |  4972 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9548 |  4973 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4974 | `				/* Reset the internal loop cursor */` |
|     9548 |  4975 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4976 | `				/* Mark the step */` |
|     9548 |  4977 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9548 |  4978 | `				pStep->xIter.pMap = pMap;` |
|     9548 |  4979 | `				pMap->iRef++;` |
|     4775 |  4980 | `			}else{` |
|       18 |  4981 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4982 | `				ph7_class *pIteratorClass;` |
|        - |  4983 | `				/* Check if the object implements Iterator */` |
|       18 |  4984 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       21 |  4985 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  4986 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  4987 | `					ph7_class_method *pRewind;` |
|        7 |  4988 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        7 |  4989 | `					pStep->xIter.pThis = pThis;` |
|        7 |  4990 | `					pThis->iRef++;` |
|        7 |  4991 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|        7 |  4992 | `					if( pRewind ){` |
|        7 |  4993 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        3 |  4994 | `					}` |
|        4 |  4995 | `				}else{` |
|        - |  4996 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  4997 | `					ph7_class *pIterAggClass;` |
|       12 |  4998 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  4999 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5000 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5001 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5002 | `						ph7_class_method *pGetIter;` |
|        3 |  5003 | `						int iterAggOk = 0;` |
|        3 |  5004 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5005 | `						if( pGetIter ){` |
|        - |  5006 | `							ph7_value sResult;` |
|        3 |  5007 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5008 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5009 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5010 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5011 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5012 | `									ph7_class_method *pRewind;` |
|        3 |  5013 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5014 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5015 | `									pIterObj->iRef++;` |
|        - |  5016 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5017 | `									pStep->pOwner = pThis;` |
|        3 |  5018 | `									pThis->iRef++;` |
|        3 |  5019 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5020 | `									if( pRewind ){` |
|        3 |  5021 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5022 | `									}` |
|        3 |  5023 | `									iterAggOk = 1;` |
|        1 |  5024 | `								}` |
|        1 |  5025 | `							}` |
|        3 |  5026 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5027 | `						}` |
|        3 |  5028 | `						if( !iterAggOk ){` |
|        - |  5029 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5030 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5031 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5032 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5033 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5034 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5035 | `						}` |
|        2 |  5036 | `					}else{` |
|        - |  5037 | `						/* Plain object iteration via hAttr */` |
|        9 |  5038 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5039 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5040 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5041 | `						pThis->iRef++;` |
|        - |  5042 | `					}` |
|        - |  5043 | `				}` |
|        - |  5044 | `			}` |
|        - |  5045 | `		}` |
|     9564 |  5046 | `		if( pStep ){` |
|     9564 |  5047 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5048 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5049 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5050 | `				/* Jump out of the loop */` |
|      ! 0 |  5051 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5052 | `			}` |
|     4781 |  5053 | `		}` |
|        - |  5054 | `	}` |
|     9564 |  5055 | `	VmPopOperand(&pTos,1);` |
|     9564 |  5056 | `	break;` |
|        - |  5057 | `						  }` |
|        - |  5058 | `/*` |
|        - |  5059 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5060 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5061 | ` */` |
|    76749 |  5062 | `case PH7_OP_FOREACH_STEP: {` |
|   153500 |  5063 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5064 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5065 | `	ph7_value *pValue;` |
|        - |  5066 | `	VmFrame *pFrameLocal;` |
|        - |  5067 | `	/* Peek the last step */` |
|   153500 |  5068 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   153500 |  5069 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   153500 |  5070 | `	pFrameLocal = pVm->pFrame;` |
|   153500 |  5071 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5072 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  5073 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5074 | `	}` |
|   153500 |  5075 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   153440 |  5076 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5077 | `		ph7_hashmap_node *pNode;` |
|        - |  5078 | `		/* Extract the current node value */` |
|   153440 |  5079 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   153440 |  5080 | `		if( pNode == 0 ){` |
|        - |  5081 | `			/* No more entry to process */` |
|     9548 |  5082 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9548 |  5083 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5084 | `				/* Break the reference with the last element */` |
|        5 |  5085 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5086 | `			}` |
|        - |  5087 | `			/* Automatically reset the loop cursor */` |
|     9548 |  5088 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5089 | `			/* Cleanup the mess left behind */` |
|     9548 |  5090 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9548 |  5091 | `			SySetPop(&pInfo->aStep);` |
|     9548 |  5092 | `			PH7_HashmapUnref(pMap);` |
|     4775 |  5093 | `		}else{` |
|   143894 |  5094 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5095 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5096 | `				if( pKey ){` |
|      416 |  5097 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5098 | `				}` |
|      207 |  5099 | `			}` |
|   143894 |  5100 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5101 | `				SyHashEntry *pEntry;` |
|        - |  5102 | `				/* Pass by reference */` |
|       13 |  5103 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5104 | `				if( pEntry ){` |
|       13 |  5105 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5106 | `				}else{` |
|      ! 0 |  5107 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5108 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5109 | `				}` |
|        7 |  5110 | `			}else{` |
|        - |  5111 | `				/* Make a copy of the entry value */` |
|   143882 |  5112 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   143882 |  5113 | `				if( pValue ){` |
|   143882 |  5114 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    71940 |  5115 | `				}` |
|        - |  5116 | `			}` |
|        2 |  5117 | `		}` |
|    76781 |  5118 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5119 | `		/* Iterator-based iteration.` |
|        - |  5120 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5121 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5122 | `		 */` |
|       37 |  5123 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5124 | `		ph7_class_method *pMethod;` |
|        - |  5125 | `		ph7_value sResult;` |
|       37 |  5126 | `		int isValid = 0;` |
|        - |  5127 | `		/* Call next() to advance — but skip on the first iteration */` |
|       37 |  5128 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|        9 |  5129 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|        5 |  5130 | `		}else{` |
|       29 |  5131 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       29 |  5132 | `			if( pMethod ){` |
|       29 |  5133 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       14 |  5134 | `			}` |
|        - |  5135 | `		}` |
|        - |  5136 | `		/* Call valid() */` |
|       37 |  5137 | `		PH7_MemObjInit(pVm,&sResult);` |
|       37 |  5138 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       37 |  5139 | `		if( pMethod ){` |
|       37 |  5140 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       37 |  5141 | `			PH7_MemObjToBool(&sResult);` |
|       37 |  5142 | `			isValid = (sResult.x.iVal != 0);` |
|       18 |  5143 | `		}` |
|       37 |  5144 | `		PH7_MemObjRelease(&sResult);` |
|       37 |  5145 | `		if( !isValid ){` |
|        - |  5146 | `			/* Iterator exhausted */` |
|        7 |  5147 | `			pc = pInstr->iP2 - 1;` |
|        - |  5148 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|        7 |  5149 | `			if( pStep->pOwner ){` |
|        3 |  5150 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5151 | `			}` |
|        7 |  5152 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        7 |  5153 | `			SySetPop(&pInfo->aStep);` |
|        7 |  5154 | `			PH7_ClassInstanceUnref(pThis);` |
|        4 |  5155 | `		}else{` |
|        - |  5156 | `			/* Call current() to get value */` |
|       31 |  5157 | `			PH7_MemObjInit(pVm,&sResult);` |
|       31 |  5158 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       31 |  5159 | `			if( pMethod ){` |
|       31 |  5160 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       15 |  5161 | `			}` |
|       31 |  5162 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       31 |  5163 | `			if( pValue ){` |
|       31 |  5164 | `				PH7_MemObjStore(&sResult,pValue);` |
|       15 |  5165 | `			}` |
|       31 |  5166 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5167 | `			/* Call key() if needed */` |
|       31 |  5168 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5169 | `				ph7_value sKey;` |
|       23 |  5170 | `				PH7_MemObjInit(pVm,&sKey);` |
|       23 |  5171 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       23 |  5172 | `				if( pMethod ){` |
|       23 |  5173 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       11 |  5174 | `				}` |
|       23 |  5175 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       23 |  5176 | `				if( pValue ){` |
|       23 |  5177 | `					PH7_MemObjStore(&sKey,pValue);` |
|       11 |  5178 | `				}` |
|       23 |  5179 | `				PH7_MemObjRelease(&sKey);` |
|       11 |  5180 | `			}` |
|        - |  5181 | `		}` |
|       19 |  5182 | `	}else{` |
|       25 |  5183 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5184 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5185 | `		SyHashEntry *pEntry;` |
|        - |  5186 | `		/* Point to the next attribute */` |
|       29 |  5187 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5188 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5189 | `			/* Check access permission */` |
|       31 |  5190 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5191 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5192 | `					break; /* Access is granted */` |
|        - |  5193 | `			}` |
|        1 |  5194 | `		}` |
|       25 |  5195 | `		if( pEntry == 0 ){` |
|        - |  5196 | `			/* Clean up the mess left behind */` |
|        9 |  5197 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5198 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5199 | `				/* Break the reference with the last element */` |
|        3 |  5200 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5201 | `			}` |
|        9 |  5202 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5203 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5204 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5205 | `		}else{` |
|       17 |  5206 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5207 | `			ph7_value *pAttrValue;` |
|       17 |  5208 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5209 | `				/* Fill with the current attribute name */` |
|       17 |  5210 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5211 | `				if( pKey ){` |
|       17 |  5212 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5213 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5214 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5215 | `				}` |
|        8 |  5216 | `			}` |
|        - |  5217 | `			/* Extract attribute value */` |
|       17 |  5218 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5219 | `			if( pAttrValue ){` |
|       17 |  5220 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5221 | `					/* Pass by reference */` |
|        3 |  5222 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5223 | `					if( pEntry ){` |
|        3 |  5224 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5225 | `					}else{` |
|      ! 0 |  5226 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5227 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5228 | `					}` |
|        2 |  5229 | `				}else{` |
|        - |  5230 | `					/* Make a copy of the attribute value */` |
|       15 |  5231 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5232 | `					if( pValue ){` |
|       15 |  5233 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5234 | `					}` |
|        - |  5235 | `				}` |
|        8 |  5236 | `			}` |
|        - |  5237 | `		}` |
|        - |  5238 | `	}` |
|   153500 |  5239 | `	break;` |
|        - |  5240 | `						  }` |
|        - |  5241 | `/*` |
|        - |  5242 | ` * OP_MEMBER P1 P2` |
|        - |  5243 | ` * Load class attribute/method on the stack.` |
|        - |  5244 | ` */` |
|     2065 |  5245 | `case PH7_OP_MEMBER: {` |
|        - |  5246 | `	ph7_class_instance *pThis;` |
|        - |  5247 | `	ph7_value *pNos;` |
|        - |  5248 | `	SyString sName;` |
|     4132 |  5249 | `	if( !pInstr->iP1 ){` |
|     4038 |  5250 | `		pNos = &pTos[-1];` |
|        - |  5251 | `#ifdef UNTRUST` |
|        - |  5252 | `		if( pNos < pStack ){` |
|        - |  5253 | `			goto Abort;` |
|        - |  5254 | `		}` |
|        - |  5255 | `#endif` |
|     4038 |  5256 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5257 | `			ph7_class *pClass;` |
|        - |  5258 | `			/* Class already instantiated */` |
|     4038 |  5259 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5260 | `			/* Point to the instantiated class */` |
|     4038 |  5261 | `			pClass = pThis->pClass;` |
|        - |  5262 | `			/* Extract attribute name first */` |
|     4038 |  5263 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4038 |  5264 | `			if( pInstr->iP2 ){` |
|        - |  5265 | `				/* Method call */` |
|      270 |  5266 | `				ph7_class_method *pMeth = 0;` |
|      270 |  5267 | `				if( sName.nByte > 0 ){` |
|        - |  5268 | `					/* Extract the target method */` |
|      270 |  5269 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      134 |  5270 | `				}` |
|      270 |  5271 | `				if( pMeth == 0 ){` |
|      ! 0 |  5272 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5273 | `						&pClass->sName,&sName` |
|        - |  5274 | `						);` |
|        - |  5275 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5276 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5277 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5278 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5279 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5280 | `				}else{` |
|        - |  5281 | `					/* Push method name on the stack */` |
|      270 |  5282 | `					PH7_MemObjRelease(pTos);` |
|      270 |  5283 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      270 |  5284 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5285 | `				}` |
|      270 |  5286 | `				pTos->nIdx = SXU32_HIGH;` |
|      136 |  5287 | `			}else{` |
|        - |  5288 | `				/* Attribute access */` |
|     3770 |  5289 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5290 | `				SyHashEntry *pEntry;` |
|        - |  5291 | `				/* Extract the target attribute */` |
|     3770 |  5292 | `				if( sName.nByte > 0 ){` |
|     3770 |  5293 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3770 |  5294 | `					if( pEntry ){` |
|        - |  5295 | `						/* Point to the attribute value */` |
|     3768 |  5296 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1883 |  5297 | `					}` |
|     1884 |  5298 | `				}` |
|     3770 |  5299 | `				if( pObjAttr == 0 ){` |
|        - |  5300 | `					/* No such attribute,load null */` |
|        4 |  5301 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5302 | `						&pClass->sName,&sName);` |
|        - |  5303 | `					/* Call the __get magic method if available */` |
|        3 |  5304 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5305 | `				}` |
|     3770 |  5306 | `				VmPopOperand(&pTos,1);` |
|        - |  5307 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5308 | `				 * This is due to the following case:` |
|        - |  5309 | `				 *     (new TestClass())->foo;` |
|        - |  5310 | `				 */` |
|     3770 |  5311 | `				pThis->iRef++;` |
|     3770 |  5312 | `				PH7_MemObjRelease(pTos);` |
|     3770 |  5313 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3770 |  5314 | `				if( pObjAttr ){` |
|     3768 |  5315 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5316 | `					/* Check attribute access */` |
|     3768 |  5317 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5318 | `						/* Load attribute */` |
|     3768 |  5319 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3768 |  5320 | `						if( pValue ){` |
|     3768 |  5321 | `							if( pThis->iRef < 2 ){` |
|        - |  5322 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5323 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5324 | `								 */` |
|        3 |  5325 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5326 | `							}else{` |
|        - |  5327 | `								/* Simple load */` |
|     3766 |  5328 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5329 | `							}` |
|     3768 |  5330 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3766 |  5331 | `								if( pThis->iRef > 1 ){` |
|        - |  5332 | `									/* Load attribute index */` |
|     3764 |  5333 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1881 |  5334 | `								}` |
|     1882 |  5335 | `							}` |
|     1883 |  5336 | `						}` |
|     1883 |  5337 | `					}` |
|     1883 |  5338 | `				}` |
|        - |  5339 | `				/* Safely unreference the object */` |
|     3770 |  5340 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5341 | `			}` |
|     2020 |  5342 | `		}else{` |
|      ! 0 |  5343 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5344 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5345 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5346 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5347 | `		}` |
|     2020 |  5348 | `	}else{` |
|        - |  5349 | `		/* Static member access using class name */` |
|       96 |  5350 | `		pNos = pTos;` |
|       96 |  5351 | `		pThis = 0;` |
|       96 |  5352 | `		if( !pInstr->p3 ){` |
|       84 |  5353 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       84 |  5354 | `			pNos--;` |
|        - |  5355 | `#ifdef UNTRUST` |
|        - |  5356 | `			if( pNos < pStack ){` |
|        - |  5357 | `				goto Abort;` |
|        - |  5358 | `			}` |
|        - |  5359 | `#endif` |
|       43 |  5360 | `		}else{` |
|        - |  5361 | `			/* Attribute name already computed */` |
|       14 |  5362 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5363 | `		}` |
|       96 |  5364 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       96 |  5365 | `			ph7_class *pClass = 0;` |
|       96 |  5366 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5367 | `				/* Class already instantiated */` |
|      ! 0 |  5368 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5369 | `				pClass = pThis->pClass;` |
|      ! 0 |  5370 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5371 | `			}else{` |
|        - |  5372 | `				/* Try to extract the target class */` |
|       96 |  5373 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       96 |  5374 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|       96 |  5375 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5376 | `					/* Handle self/static/parent keywords */` |
|       96 |  5377 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       26 |  5378 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       84 |  5379 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5380 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       71 |  5381 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5382 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5383 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5384 | `							pClass = pSelf->pBase;` |
|        6 |  5385 | `						}` |
|        8 |  5386 | `					}else{` |
|       46 |  5387 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5388 | `					}` |
|       47 |  5389 | `				}` |
|        - |  5390 | `			}` |
|       96 |  5391 | `			if( pClass == 0 ){` |
|        - |  5392 | `				/* Undefined class */` |
|      ! 0 |  5393 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5394 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5395 | `					);` |
|      ! 0 |  5396 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5397 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5398 | `				}` |
|      ! 0 |  5399 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5400 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5401 | `			}else{` |
|       96 |  5402 | `				if( pInstr->iP2 ){` |
|        - |  5403 | `					/* Method call */` |
|       30 |  5404 | `					ph7_class_method *pMeth = 0;` |
|       30 |  5405 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5406 | `						/* Extract the target method */` |
|       30 |  5407 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       14 |  5408 | `					}` |
|       30 |  5409 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5410 | `						if( pMeth ){` |
|      ! 0 |  5411 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5412 | `								&pClass->sName,&sName` |
|        - |  5413 | `								);` |
|      ! 0 |  5414 | `						}else{` |
|      ! 0 |  5415 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5416 | `								&pClass->sName,&sName` |
|        - |  5417 | `								);` |
|        - |  5418 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5419 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5420 | `						}` |
|        - |  5421 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5422 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5423 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5424 | `						}` |
|      ! 0 |  5425 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5426 | `					}else{` |
|        - |  5427 | `						/* Push method name on the stack */` |
|       30 |  5428 | `						PH7_MemObjRelease(pTos);` |
|       30 |  5429 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       30 |  5430 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5431 | `					}` |
|       30 |  5432 | `					pTos->nIdx = SXU32_HIGH;` |
|       16 |  5433 | `				}else{` |
|        - |  5434 | `					/* Attribute access */` |
|       68 |  5435 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5436 | `					/* Check for special ::class pseudo-constant */` |
|       98 |  5437 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       60 |  5438 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5439 | `						/* ::class returns the fully qualified class name */` |
|        - |  5440 | `						/* Pop the attribute name from the stack */` |
|       50 |  5441 | `						if( !pInstr->p3 ){` |
|       50 |  5442 | `							VmPopOperand(&pTos,1);` |
|       24 |  5443 | `						}` |
|       50 |  5444 | `						PH7_MemObjRelease(pTos);` |
|        - |  5445 | `						/* Load the class name */` |
|       50 |  5446 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       50 |  5447 | `						pTos->nIdx = SXU32_HIGH;` |
|       26 |  5448 | `					}else{` |
|        - |  5449 | `						/* Extract the target attribute */` |
|       20 |  5450 | `						if( sName.nByte > 0 ){` |
|       20 |  5451 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5452 | `						}` |
|       20 |  5453 | `						if( pAttr == 0 ){` |
|        - |  5454 | `							/* No such attribute,load null */` |
|      ! 0 |  5455 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5456 | `								&pClass->sName,&sName);` |
|        - |  5457 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5458 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5459 | `						}` |
|        - |  5460 | `						/* Pop the attribute name from the stack */` |
|       20 |  5461 | `						if( !pInstr->p3 ){` |
|        7 |  5462 | `							VmPopOperand(&pTos,1);` |
|        3 |  5463 | `						}` |
|       20 |  5464 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5465 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5466 | `						if( pAttr ){` |
|       20 |  5467 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5468 | `								/* Access to a non static attribute */` |
|      ! 0 |  5469 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5470 | `									&pClass->sName,&pAttr->sName` |
|        - |  5471 | `									);` |
|      ! 0 |  5472 | `							}else{` |
|        - |  5473 | `								ph7_value *pValue;` |
|        - |  5474 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5475 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5476 | `									/* Load the desired attribute */` |
|       20 |  5477 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5478 | `									if( pValue ){` |
|       20 |  5479 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5480 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5481 | `											/* Load index number */` |
|       14 |  5482 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5483 | `										}` |
|        9 |  5484 | `									}` |
|        9 |  5485 | `								}` |
|        - |  5486 | `							}` |
|        9 |  5487 | `						}` |
|        - |  5488 | `					}` |
|        - |  5489 | `				}` |
|       96 |  5490 | `				if( pThis ){` |
|        - |  5491 | `					/* Safely unreference the object */` |
|      ! 0 |  5492 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5493 | `				}` |
|        - |  5494 | `			}` |
|       49 |  5495 | `		}else{` |
|        - |  5496 | `			/* Pop operands */` |
|      ! 0 |  5497 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5498 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5499 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5500 | `			}` |
|      ! 0 |  5501 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5502 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5503 | `		}` |
|        - |  5504 | `	}` |
|     4132 |  5505 | `	break;` |
|        - |  5506 | `					}` |
|        - |  5507 | `/*` |
|        - |  5508 | ` * OP_NEW P1 * * *` |
|        - |  5509 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5510 | ` */` |
|      299 |  5511 | `case PH7_OP_NEW: {` |
|      600 |  5512 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      600 |  5513 | `	ph7_class *pClass = 0;` |
|        - |  5514 | `	ph7_class_instance *pNew;` |
|      600 |  5515 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5516 | `		/* Try to extract the desired class */` |
|      899 |  5517 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      598 |  5518 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      299 |  5519 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5520 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5521 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5522 | `	}` |
|      600 |  5523 | `	if( pClass == 0 ){` |
|        - |  5524 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5525 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5526 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5527 | `			);` |
|        - |  5528 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5529 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5530 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5531 | `			/* Pop given arguments */` |
|      ! 0 |  5532 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5533 | `		}` |
|      ! 0 |  5534 | `		goto Abort;` |
|      ! 0 |  5535 | `	}else{` |
|        - |  5536 | `		ph7_class_method *pCons;` |
|        - |  5537 | `		/* Create a new class instance */` |
|      600 |  5538 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      600 |  5539 | `		if( pNew == 0 ){` |
|      ! 0 |  5540 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5541 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5542 | `				&pClass->sName` |
|        - |  5543 | `			);` |
|      ! 0 |  5544 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5545 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5546 | `				/* Pop given arguments */` |
|      ! 0 |  5547 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5548 | `			}` |
|      ! 0 |  5549 | `			break;` |
|        - |  5550 | `		}` |
|        - |  5551 | `		/* Check if a constructor is available */` |
|      600 |  5552 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      600 |  5553 | `		if( pCons == 0 ){` |
|      520 |  5554 | `			SyString *pName = &pClass->sName;` |
|        - |  5555 | `			/* Check for a constructor with the same base class name */` |
|      520 |  5556 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      259 |  5557 | `		}` |
|      600 |  5558 | `		if( pCons ){` |
|        - |  5559 | `			/* Call the class constructor */` |
|       82 |  5560 | `			SySetReset(&aArg);` |
|      154 |  5561 | `			while( pArg < pTos ){` |
|       74 |  5562 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       74 |  5563 | `				pArg++;` |
|        2 |  5564 | `			}` |
|       82 |  5565 | `			if( pVm->bErrReport ){` |
|        - |  5566 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5567 | `				sxu32 n;` |
|       39 |  5568 | `				n = SySetUsed(&aArg);` |
|        - |  5569 | `				/* Emit a notice for missing arguments */` |
|       87 |  5570 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  5571 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  5572 | `					if( pFuncArg ){` |
|       49 |  5573 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5574 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5575 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5576 | `						}` |
|       24 |  5577 | `					}` |
|       49 |  5578 | `					n++;` |
|        1 |  5579 | `				}` |
|       19 |  5580 | `			}` |
|       82 |  5581 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5582 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       82 |  5583 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5584 | `				pNew->iRef = 1;` |
|      ! 0 |  5585 | `			}` |
|       40 |  5586 | `		}` |
|      600 |  5587 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5588 | `			/* Pop given arguments */` |
|       66 |  5589 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       32 |  5590 | `		}` |
|      600 |  5591 | `		PH7_MemObjRelease(pTos);` |
|      600 |  5592 | `		pTos->x.pOther = pNew;` |
|      600 |  5593 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5594 | `	}` |
|      600 |  5595 | `	break;` |
|        - |  5596 | `				 }` |
|        - |  5597 | `/*` |
|        - |  5598 | ` * OP_CLONE * * *` |
|        - |  5599 | ` * Perfome a clone operation.` |
|        - |  5600 | ` */` |
|       23 |  5601 | `case PH7_OP_CLONE: {` |
|        - |  5602 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5603 | `#ifdef UNTRUST` |
|        - |  5604 | `	if( pTos < pStack ){` |
|        - |  5605 | `		goto Abort;` |
|        - |  5606 | `	}` |
|        - |  5607 | `#endif` |
|        - |  5608 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5609 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5610 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5611 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5612 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5613 | `		break;` |
|        - |  5614 | `	}` |
|        - |  5615 | `	/* Point to the source */` |
|       44 |  5616 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5617 | `	/* Perform the clone operation */` |
|       44 |  5618 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5619 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5620 | `	if( pClone == 0 ){` |
|      ! 0 |  5621 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5622 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5623 | `	}else{` |
|        - |  5624 | `		/* Load the cloned object */` |
|       44 |  5625 | `		pTos->x.pOther = pClone;` |
|       44 |  5626 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5627 | `	}` |
|       44 |  5628 | `	break;` |
|        - |  5629 | `				   }` |
|        - |  5630 | `/*` |
|        - |  5631 | ` * OP_SWITCH * * P3` |
|        - |  5632 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5633 | ` */` |
|       18 |  5634 | `case PH7_OP_SWITCH: {` |
|       38 |  5635 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5636 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5637 | `	ph7_value sValue,sCaseValue;` |
|        - |  5638 | `	sxu32 n,nEntry;` |
|        - |  5639 | `#ifdef UNTRUST` |
|        - |  5640 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5641 | `		goto Abort;` |
|        - |  5642 | `	}` |
|        - |  5643 | `#endif` |
|        - |  5644 | `	/* Point to the case table  */` |
|       38 |  5645 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5646 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5647 | `	/* Select the appropriate case block to execute */` |
|       38 |  5648 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5649 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5650 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5651 | `		pCase = &aCase[n];` |
|       92 |  5652 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5653 | `		/* Execute the case expression first */` |
|       92 |  5654 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5655 | `		/* Compare the two expression */` |
|       92 |  5656 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5657 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5658 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5659 | `		if( rc == 0 ){` |
|        - |  5660 | `			/* Value match,jump to this block */` |
|       38 |  5661 | `			pc = pCase->nStart - 1;` |
|       38 |  5662 | `			break;` |
|        - |  5663 | `		}` |
|       29 |  5664 | `	}` |
|       38 |  5665 | `	VmPopOperand(&pTos,1);` |
|       38 |  5666 | `	if( n >= nEntry ){` |
|        - |  5667 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5668 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5669 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5670 | `		}else{` |
|        - |  5671 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5672 | `			pc = pSwitch->nOut - 1;` |
|        - |  5673 | `		}` |
|      ! 0 |  5674 | `	}` |
|       38 |  5675 | `	break;` |
|        - |  5676 | `					}` |
|        - |  5677 | `/*` |
|        - |  5678 | ` * OP_CALL P1 * *` |
|        - |  5679 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5680 | ` *  function on the stack.` |
|        - |  5681 | ` */` |
|   279483 |  5682 | `case PH7_OP_CALL: {` |
|   559012 |  5683 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5684 | `	SyHashEntry *pEntry;` |
|        - |  5685 | `	SyString sName;` |
|        - |  5686 | `	/* Extract function name */` |
|   559012 |  5687 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5688 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5689 | `			ph7_value sResult;` |
|      ! 0 |  5690 | `			SySetReset(&aArg);` |
|      ! 0 |  5691 | `			while( pArg < pTos ){` |
|      ! 0 |  5692 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5693 | `				pArg++;` |
|      ! 0 |  5694 | `			}` |
|      ! 0 |  5695 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5696 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5697 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5698 | `			SySetReset(&aArg);` |
|        - |  5699 | `			/* Pop given arguments */` |
|      ! 0 |  5700 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5701 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5702 | `			}` |
|        - |  5703 | `			/* Copy result */` |
|      ! 0 |  5704 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5705 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5706 | `		}else{` |
|        3 |  5707 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5708 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5709 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5710 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5711 | `			}else{` |
|        - |  5712 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5713 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5714 | `			}` |
|        - |  5715 | `			/* Pop given arguments */` |
|        3 |  5716 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5717 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5718 | `			}` |
|        - |  5719 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5720 | `			PH7_MemObjRelease(pTos);` |
|        - |  5721 | `		}` |
|   279250 |  5722 | `		break;` |
|        - |  5723 | `	}` |
|   559010 |  5724 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5725 | `	/* Check for a compiled function first.` |
|        - |  5726 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5727 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   559010 |  5728 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5729 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5730 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5731 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5732 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5733 | `	 * function calls inside namespaces. */` |
|   559010 |  5734 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5735 | `		const char *zFunc;` |
|        - |  5736 | `		const char *zEnd;` |
|        - |  5737 | `		const char *z;` |
|        - |  5738 | `		SyString sGlobal;` |
|       15 |  5739 | `		zFunc = sName.zString;` |
|       15 |  5740 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5741 | `		z = zEnd;` |
|        - |  5742 | `		/* Find last namespace separator */` |
|      133 |  5743 | `		while( z > zFunc ){` |
|      133 |  5744 | `			if( z[-1] == '\\' ){` |
|       15 |  5745 | `				break;` |
|        - |  5746 | `			}` |
|      119 |  5747 | `			z--;` |
|        1 |  5748 | `		}` |
|       15 |  5749 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5750 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5751 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5752 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5753 | `		}` |
|        7 |  5754 | `	}` |
|   559010 |  5755 | `	if( pEntry ){` |
|        - |  5756 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5757 | `		ph7_class_instance *pThis;` |
|        - |  5758 | `		ph7_value *pFrameStack;` |
|        - |  5759 | `		ph7_vm_func *pVmFunc;` |
|        - |  5760 | `		ph7_class *pSelf;` |
|        - |  5761 | `		VmFrame *pFrame;` |
|        - |  5762 | `		ph7_value *pObj;` |
|        - |  5763 | `		VmSlot sArg;` |
|        - |  5764 | `		sxu32 n;` |
|        - |  5765 | `		/* initialize fields */` |
|    12388 |  5766 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12388 |  5767 | `		pThis = 0;` |
|    12388 |  5768 | `		pSelf = 0;` |
|    12388 |  5769 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5770 | `			ph7_class_method *pMeth;` |
|        - |  5771 | `			/* Class method call */` |
|     1592 |  5772 | `			ph7_value *pTarget = &pTos[-1];` |
|     1592 |  5773 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5774 | `				/* Extract the 'this' pointer */` |
|     1592 |  5775 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5776 | `					/* Instance already loaded */` |
|     1558 |  5777 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1558 |  5778 | `					pThis->iRef++;` |
|     1558 |  5779 | `					pSelf = pThis->pClass;` |
|      778 |  5780 | `				}` |
|     1592 |  5781 | `				if( pSelf == 0 ){` |
|       36 |  5782 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5783 | `						/* "Late Static Binding" class name */` |
|       44 |  5784 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       14 |  5785 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       14 |  5786 | `					}` |
|       36 |  5787 | `					if( pSelf == 0 ){` |
|       13 |  5788 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5789 | `					}` |
|       17 |  5790 | `				}` |
|     1592 |  5791 | `				if( pThis == 0  ){` |
|       36 |  5792 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  5793 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5794 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5795 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5796 | `					}` |
|       36 |  5797 | `					if( pFrameLocal->pParent ){` |
|        - |  5798 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5799 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5800 | `						if( pThis ){` |
|       13 |  5801 | `							pThis->iRef++;` |
|        6 |  5802 | `						}` |
|        9 |  5803 | `					}` |
|       17 |  5804 | `				}` |
|     1592 |  5805 | `				VmPopOperand(&pTos,1);` |
|     1592 |  5806 | `				PH7_MemObjRelease(pTos);` |
|        - |  5807 | `				/* Synchronize pointers */` |
|     1592 |  5808 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5809 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5810 | `				 * user have already computed the random generated unique class method name` |
|        - |  5811 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5812 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5813 | `				 */` |
|     1592 |  5814 | `				while( pArg < pStack ){` |
|      ! 0 |  5815 | `					pArg++;` |
|      ! 0 |  5816 | `				}` |
|     1592 |  5817 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5818 | `					/* Check if the call is allowed */` |
|     1592 |  5819 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1592 |  5820 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  5821 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5822 | `							/* Pop given arguments */` |
|      ! 0 |  5823 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5824 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5825 | `							}` |
|        - |  5826 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5827 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5828 | `							break;` |
|        - |  5829 | `						}` |
|        3 |  5830 | `					}` |
|      795 |  5831 | `				}` |
|      795 |  5832 | `			}` |
|      795 |  5833 | `		}` |
|        - |  5834 | `		/* Check The recursion limit */` |
|    12388 |  5835 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5836 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5837 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5838 | `				&pVmFunc->sName);` |
|        - |  5839 | `			/* Pop given arguments */` |
|        3 |  5840 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5841 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5842 | `			}` |
|        - |  5843 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5844 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5845 | `			break;` |
|        - |  5846 | `		}` |
|    12386 |  5847 | `		if( pVmFunc->pNextName ){` |
|        - |  5848 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  5849 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  5850 | `		}` |
|        - |  5851 | `		/* Extract the formal argument set */` |
|    12386 |  5852 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5853 | `		/* Create a new VM frame  */` |
|    12386 |  5854 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12386 |  5855 | `		if( rc != SXRET_OK ){` |
|        - |  5856 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5857 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5858 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5859 | `				&pVmFunc->sName);` |
|        - |  5860 | `			/* Pop given arguments */` |
|      ! 0 |  5861 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5862 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5863 | `			}` |
|        - |  5864 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5865 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5866 | `			break;` |
|        - |  5867 | `		}` |
|    12386 |  5868 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5869 | `			/* Install the '$this' variable */` |
|        - |  5870 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1568 |  5871 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1568 |  5872 | `			if( pObj ){` |
|        - |  5873 | `				/* Reflect the change */` |
|     1568 |  5874 | `				pObj->x.pOther = pThis;` |
|     1568 |  5875 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      783 |  5876 | `			}` |
|      783 |  5877 | `		}` |
|    12386 |  5878 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5879 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5880 | `			/* Install static variables */` |
|      ! 0 |  5881 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5882 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5883 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5884 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5885 | `					/* Initialize the static variables */` |
|      ! 0 |  5886 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5887 | `					if( pObj ){` |
|        - |  5888 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5889 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5890 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5891 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5892 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5893 | `						}` |
|      ! 0 |  5894 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5895 | `					}else{` |
|      ! 0 |  5896 | `						continue;` |
|        - |  5897 | `					}` |
|      ! 0 |  5898 | `				}` |
|        - |  5899 | `				/* Install in the current frame */` |
|      ! 0 |  5900 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5901 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5902 | `			}` |
|      ! 0 |  5903 | `		}` |
|        - |  5904 | `		/* Push arguments in the local frame */` |
|    12386 |  5905 | `		n = 0;` |
|    34178 |  5906 | `		while( pArg < pTos ){` |
|    21794 |  5907 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    21644 |  5908 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5909 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5910 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5911 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5912 | `						goto Abort;` |
|        - |  5913 | `					}` |
|      ! 0 |  5914 | `				}` |
|        - |  5915 | `				/* Make sure the given arguments are of the correct type */` |
|    21644 |  5916 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5917 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5918 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5919 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5920 | `						ph7_class *pClass;` |
|        - |  5921 | `						/* Try to extract the desired class */` |
|      ! 0 |  5922 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5923 | `						if( pClass ){` |
|      ! 0 |  5924 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5925 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5926 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5927 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5928 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5929 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5930 | `								}` |
|      ! 0 |  5931 | `							}else{` |
|        - |  5932 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5933 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5934 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5935 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5936 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5937 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5938 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5939 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5940 | `								}` |
|        - |  5941 | `							}` |
|      ! 0 |  5942 | `						}` |
|     1088 |  5943 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5944 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5945 | `						/* Cast to the desired type */` |
|      ! 0 |  5946 | `						xCast(pArg);` |
|      ! 0 |  5947 | `					}` |
|      543 |  5948 | `				}` |
|    21644 |  5949 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5950 | `					/* Pass by reference */` |
|       48 |  5951 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5952 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5953 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5954 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5955 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5956 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5957 | `						}` |
|        - |  5958 | `						/* Switch to pass by value */` |
|      ! 0 |  5959 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5960 | `					}else{` |
|        - |  5961 | `						SyHashEntry *pRefEntry;` |
|        - |  5962 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5963 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5964 | `						if( pRefEntry == 0 ){` |
|       71 |  5965 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5966 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5967 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5968 | `							sArg.pUserData = 0;` |
|       48 |  5969 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5970 | `						}` |
|       48 |  5971 | `						pObj = 0;` |
|        - |  5972 | `					}` |
|       25 |  5973 | `				}else{` |
|        - |  5974 | `					/* Pass by value,make a copy of the given argument */` |
|    21598 |  5975 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5976 | `				}` |
|    10823 |  5977 | `			}else{` |
|        - |  5978 | `				char zName[32];` |
|        - |  5979 | `				SyString sArgName;` |
|        - |  5980 | `				/* Set a dummy name */` |
|      152 |  5981 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5982 | `				sArgName.zString = zName;` |
|        - |  5983 | `				/* Annonymous argument */` |
|      152 |  5984 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5985 | `			}` |
|    21794 |  5986 | `			if( pObj ){` |
|    21748 |  5987 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5988 | `				/* Insert argument index  */` |
|    21748 |  5989 | `				sArg.nIdx = pObj->nIdx;` |
|    21748 |  5990 | `				sArg.pUserData = 0;` |
|    21748 |  5991 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10873 |  5992 | `			}` |
|    21794 |  5993 | `			PH7_MemObjRelease(pArg);` |
|    21794 |  5994 | `			pArg++;` |
|    21794 |  5995 | `			++n;` |
|        2 |  5996 | `		}` |
|        - |  5997 | `		/* Set up closure environment */` |
|    12386 |  5998 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5999 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6000 | `			ph7_value *pValue;` |
|        - |  6001 | `			sxu32 iEnv;` |
|        9 |  6002 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  6003 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  6004 | `				pEnv = &aEnv[iEnv];` |
|       17 |  6005 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6006 | `					/* Do not install null value */` |
|        9 |  6007 | `					continue;` |
|        - |  6008 | `				}` |
|        9 |  6009 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  6010 | `				if( pValue == 0 ){` |
|      ! 0 |  6011 | `					continue;` |
|        - |  6012 | `				}` |
|        - |  6013 | `				/* Invalidate any prior representation */` |
|        9 |  6014 | `				PH7_MemObjRelease(pValue);` |
|        - |  6015 | `				/* Duplicate bound variable value */` |
|        9 |  6016 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  6017 | `			}` |
|        4 |  6018 | `		}` |
|        - |  6019 | `		/* Process default values */` |
|    14244 |  6020 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1860 |  6021 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1854 |  6022 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1854 |  6023 | `				if( pObj ){` |
|        - |  6024 | `					/* Evaluate the default value and extract it's result */` |
|     1854 |  6025 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1854 |  6026 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6027 | `						goto Abort;` |
|        - |  6028 | `					}` |
|        - |  6029 | `					/* Insert argument index */` |
|     1854 |  6030 | `					sArg.nIdx = pObj->nIdx;` |
|     1854 |  6031 | `					sArg.pUserData = 0;` |
|     1854 |  6032 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6033 | `					/* Make sure the default argument is of the correct type */` |
|     1854 |  6034 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6035 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6036 | `						/* Cast to the desired type */` |
|      ! 0 |  6037 | `						xCast(pObj);` |
|      ! 0 |  6038 | `					}` |
|      926 |  6039 | `				}` |
|      926 |  6040 | `			}` |
|     1860 |  6041 | `			++n;` |
|        2 |  6042 | `		}` |
|        - |  6043 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6044 | `		 * does not return anything.` |
|        - |  6045 | `		 */` |
|    12386 |  6046 | `		PH7_MemObjRelease(pTos);` |
|    12386 |  6047 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6048 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12386 |  6049 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12386 |  6050 | `		if( pFrameStack == 0 ){` |
|        - |  6051 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6052 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6053 | `				&pVmFunc->sName);` |
|      ! 0 |  6054 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6055 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6056 | `			}` |
|      ! 0 |  6057 | `			break;` |
|        - |  6058 | `		}` |
|    12386 |  6059 | `		if( pSelf ){` |
|        - |  6060 | `			/* Push class name */` |
|     1590 |  6061 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      794 |  6062 | `		}` |
|        - |  6063 | `		/* Increment nesting level */` |
|    12386 |  6064 | `		pVm->nRecursionDepth++;` |
|        - |  6065 | `		/* Execute function body */` |
|    12386 |  6066 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  6067 | `		/* Decrement nesting level */` |
|    12386 |  6068 | `		pVm->nRecursionDepth--;` |
|    12386 |  6069 | `		if( pSelf ){` |
|        - |  6070 | `			/* Pop class name */` |
|     1590 |  6071 | `			(void)SySetPop(&pVm->aSelf);` |
|      794 |  6072 | `		}` |
|        - |  6073 | `		/* Cleanup the mess left behind */` |
|    12386 |  6074 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6075 | `			/* Return by reference,reflect that */` |
|        9 |  6076 | `			if( n != SXU32_HIGH ){` |
|        9 |  6077 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6078 | `				sxu32 i;` |
|        - |  6079 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6080 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6081 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6082 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6083 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6084 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6085 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6086 | `								&pVmFunc->sName);` |
|      ! 0 |  6087 | `						}` |
|      ! 0 |  6088 | `						n = SXU32_HIGH;` |
|      ! 0 |  6089 | `						break;` |
|        - |  6090 | `					}` |
|        3 |  6091 | `				}` |
|        5 |  6092 | `			}else{` |
|      ! 0 |  6093 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6094 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6095 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6096 | `						&pVmFunc->sName);` |
|      ! 0 |  6097 | `				}` |
|        - |  6098 | `			}` |
|        9 |  6099 | `			pTos->nIdx = n;` |
|        4 |  6100 | `		}` |
|        - |  6101 | `		/* Cleanup the mess left behind */` |
|    12386 |  6102 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6103 | `			/* An exception was throw in this frame */` |
|        7 |  6104 | `			pFrame = pFrame->pParent;` |
|        7 |  6105 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6106 | `				/* Pop the resutlt */` |
|        5 |  6107 | `				VmPopOperand(&pTos,1);` |
|        - |  6108 | `				/* Jump to this destination */` |
|        5 |  6109 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  6110 | `				rc = PH7_OK;` |
|        3 |  6111 | `			}else{` |
|        3 |  6112 | `				if( pFrame->pParent ){` |
|        3 |  6113 | `					rc = PH7_EXCEPTION;` |
|        2 |  6114 | `				}else{` |
|        - |  6115 | `					/* Continue normal execution */` |
|      ! 0 |  6116 | `					rc = PH7_OK;` |
|        - |  6117 | `				}` |
|        - |  6118 | `			}` |
|        3 |  6119 | `		}` |
|        - |  6120 | `		/* Free the operand stack */` |
|    12386 |  6121 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6122 | `		/* Leave the frame */` |
|    12386 |  6123 | `		VmLeaveFrame(&(*pVm));` |
|    12386 |  6124 | `		if( rc == PH7_ABORT ){` |
|        - |  6125 | `			/* Abort processing immeditaley */` |
|        7 |  6126 | `			goto Abort;` |
|    12380 |  6127 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6128 | `			goto Exception;` |
|        - |  6129 | `		}` |
|     6190 |  6130 | `	}else{` |
|        - |  6131 | `		ph7_user_func *pFunc;` |
|        - |  6132 | `		ph7_context sCtx;` |
|        - |  6133 | `		ph7_value sRet;` |
|        - |  6134 | `		/* Look for an installed foreign function.` |
|        - |  6135 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6136 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6137 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6138 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6139 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6140 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   546624 |  6141 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   546624 |  6142 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6143 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6144 | `			const char *zShort = sName.zString;` |
|        - |  6145 | `			sxu32 i;` |
|      217 |  6146 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6147 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6148 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6149 | `				}` |
|      102 |  6150 | `			}` |
|       15 |  6151 | `			if( zShort != sName.zString ){` |
|       15 |  6152 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6153 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6154 | `			}` |
|        7 |  6155 | `		}` |
|   546624 |  6156 | `		if( pEntry == 0 ){` |
|        - |  6157 | `			/* Call to undefined function */` |
|        5 |  6158 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6159 | `			/* Pop given arguments */` |
|        5 |  6160 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6161 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6162 | `			}` |
|        - |  6163 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6164 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6165 | `			break;` |
|        - |  6166 | `		}` |
|   546620 |  6167 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6168 | `		/* Start collecting function arguments */` |
|   546620 |  6169 | `		SySetReset(&aArg);` |
|  1469294 |  6170 | `		while( pArg < pTos ){` |
|   922676 |  6171 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   922676 |  6172 | `			pArg++;` |
|        2 |  6173 | `		}` |
|        - |  6174 | `		/* Assume a null return value */` |
|   546620 |  6175 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6176 | `		/* Init the call context */` |
|   546620 |  6177 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6178 | `		/* Call the foreign function */` |
|   546620 |  6179 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6180 | `		/* Release the call context */` |
|   546620 |  6181 | `		VmReleaseCallContext(&sCtx);` |
|   546620 |  6182 | `		if( rc == PH7_ABORT ){` |
|      463 |  6183 | `			goto Abort;` |
|   546158 |  6184 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6185 | `			VmFrame *pFrm = pVm->pFrame;` |
|       13 |  6186 | `			while( pFrm->pParent && (pFrm->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  6187 | `				pFrm = pFrm->pParent;` |
|        1 |  6188 | `			}` |
|        7 |  6189 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6190 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6191 | `				goto Exception;` |
|        - |  6192 | `			}` |
|        - |  6193 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6194 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6195 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6196 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6197 | `			}` |
|        - |  6198 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6199 | `			VmPopOperand(&pTos,1);` |
|        - |  6200 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6201 | `			pFrm = pVm->pFrame;` |
|        7 |  6202 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6203 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6204 | `			}` |
|        7 |  6205 | `			break;` |
|        - |  6206 | `		}` |
|   546152 |  6207 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6208 | `			/* Pop function name and arguments */` |
|   528858 |  6209 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   264450 |  6210 | `		}` |
|        - |  6211 | `		/* Save foreign function return value */` |
|   546152 |  6212 | `		PH7_MemObjStore(&sRet,pTos);` |
|   546152 |  6213 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6214 | `	}` |
|   558528 |  6215 | `	break;` |
|        - |  6216 | `				  }` |
|        - |  6217 | `/*` |
|        - |  6218 | ` * OP_CONSUME: P1 * *` |
|        - |  6219 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6220 | ` */` |
|    10852 |  6221 | `case PH7_OP_CONSUME: {` |
|    21706 |  6222 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    21706 |  6223 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6224 |  |
|    21706 |  6225 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    21706 |  6226 | `	pCur = pOut;` |
|        - |  6227 | `	/* Start the consume process  */` |
|    43410 |  6228 | `	while( pOut <= pTos ){` |
|        - |  6229 | `		/* Force a string cast */` |
|    21706 |  6230 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6231 | `			PH7_MemObjToString(pOut);` |
|      149 |  6232 | `		}` |
|    21706 |  6233 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6234 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6235 | `			/* Invoke the output consumer callback */` |
|    11918 |  6236 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    11918 |  6237 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6238 | `				/* Increment output length */` |
|     5472 |  6239 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2735 |  6240 | `			}` |
|    11918 |  6241 | `			SyBlobRelease(&pOut->sBlob);` |
|    11918 |  6242 | `			if( rc == SXERR_ABORT ){` |
|        - |  6243 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6244 | `				goto Abort;` |
|        - |  6245 | `			}` |
|     5958 |  6246 | `		}` |
|    21706 |  6247 | `		pOut++;` |
|        2 |  6248 | `	}` |
|    21706 |  6249 | `	pTos = &pCur[-1];` |
|    21704 |  6250 | `	break;` |
|        - |  6251 | `					 }` |
|        - |  6252 |  |
|        - |  6253 | `		} /* Switch() */` |
|  9646906 |  6254 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6255 | `	} /* For(;;) */` |
|    15199 |  6256 | `Done:` |
|    30400 |  6257 | `	SySetRelease(&aArg);` |
|    30400 |  6258 | `	return SXRET_OK;` |
|      238 |  6259 | `Abort:` |
|      477 |  6260 | `	SySetRelease(&aArg);` |
|     1661 |  6261 | `	while( pTos >= pStack ){` |
|     1185 |  6262 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6263 | `		pTos--;` |
|        1 |  6264 | `	}` |
|      477 |  6265 | `	return PH7_ABORT;` |
|        1 |  6266 | `Exception:` |
|        3 |  6267 | `	SySetRelease(&aArg);` |
|        5 |  6268 | `	while( pTos >= pStack ){` |
|        3 |  6269 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6270 | `		pTos--;` |
|        1 |  6271 | `	}` |
|        3 |  6272 | `	return PH7_EXCEPTION;` |
|    15440 |  6273 |  |
|        - |  6274 | `/*` |
|        - |  6275 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6276 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6277 | ` * See block-comment on that function for additional information.` |
|        - |  6278 | ` */` |
|    14512 |  6279 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6280 |  |
|        - |  6281 | `	ph7_value *pStack;` |
|        - |  6282 | `	sxi32 rc;` |
|        - |  6283 | `	/* Allocate a new operand stack */` |
|    14514 |  6284 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14514 |  6285 | `	if( pStack == 0 ){` |
|      ! 0 |  6286 | `		return SXERR_MEM;` |
|        - |  6287 | `	}` |
|        - |  6288 | `	/* Execute the program */` |
|    14514 |  6289 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6290 | `	/* Free the operand stack */` |
|    14514 |  6291 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6292 | `	/* Execution result */` |
|    14514 |  6293 | `	return rc;` |
|     7258 |  6294 |  |
|        - |  6295 | `/*` |
|        - |  6296 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6297 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6298 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6299 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6300 | ` * execution ends.` |
|        - |  6301 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6302 | ` * additional information.` |
|        - |  6303 | ` */` |
|     2240 |  6304 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6305 |  |
|        - |  6306 | `	VmShutdownCB *pEntry;` |
|        - |  6307 | `	ph7_value *apArg[10];` |
|        - |  6308 | `	sxu32 n,nEntry;` |
|        - |  6309 | `	int i;` |
|        - |  6310 | `	/* Point to the stack of registered callbacks */` |
|     2242 |  6311 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    24642 |  6312 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    22402 |  6313 | `		apArg[i] = 0;` |
|    11202 |  6314 | `	}` |
|     2244 |  6315 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6316 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6317 | `		if( pEntry ){` |
|        - |  6318 | `			/* Prepare callback arguments if any */` |
|        3 |  6319 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6320 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6321 | `					break;` |
|        - |  6322 | `				}` |
|      ! 0 |  6323 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6324 | `			}` |
|        - |  6325 | `			/* Invoke the callback */` |
|        3 |  6326 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6327 | `			/*` |
|        - |  6328 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6329 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6330 | `			 */` |
|        3 |  6331 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6332 | `			if( pEntry ){` |
|        3 |  6333 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6334 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6335 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6336 | `				}` |
|        1 |  6337 | `			}` |
|        1 |  6338 | `		}` |
|        2 |  6339 | `	}` |
|     2242 |  6340 | `	SySetReset(&pVm->aShutdown);` |
|     2242 |  6341 |  |
|        - |  6342 | `/*` |
|        - |  6343 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6344 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6345 | ` * See block-comment on that function for additional information.` |
|        - |  6346 | ` */` |
|     2248 |  6347 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6348 |  |
|        - |  6349 | `	/* Make sure we are ready to execute this program */` |
|     2250 |  6350 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6351 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6352 | `	}` |
|        - |  6353 | `	/* Set the execution magic number  */` |
|     2250 |  6354 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6355 | `	/* Execute the program */` |
|     2250 |  6356 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6357 | `	/* Invoke any shutdown callbacks */` |
|     2246 |  6358 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6359 | `	/*` |
|        - |  6360 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6361 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6362 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6363 | `	 */` |
|     2246 |  6364 | `	return SXRET_OK;` |
|     1126 |  6365 |  |
|        - |  6366 | `/*` |
|        - |  6367 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6368 | ` * the desired message.` |
|        - |  6369 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6370 | ` * in 'api.c' for additional information.` |
|        - |  6371 | ` */` |
|      350 |  6372 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6373 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6374 | `	SyString *pString /* Message to output */` |
|        - |  6375 | `	)` |
|        2 |  6376 |  |
|      352 |  6377 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6378 | `	sxi32 rc = SXRET_OK;` |
|        - |  6379 | `	/* Call the output consumer */` |
|      352 |  6380 | `	if( pString->nByte > 0 ){` |
|      352 |  6381 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6382 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6383 | `			/* Increment output length */` |
|       17 |  6384 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6385 | `		}` |
|      175 |  6386 | `	}` |
|      352 |  6387 | `	return rc;` |
|        2 |  6388 |  |
|        - |  6389 | `/*` |
|        - |  6390 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6391 | ` * callback to consume the formatted message.` |
|        - |  6392 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6393 | ` * in 'api.c' for additional information.` |
|        - |  6394 | ` */` |
|        2 |  6395 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6396 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6397 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6398 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6399 | `	)` |
|        1 |  6400 |  |
|        3 |  6401 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6402 | `	sxi32 rc = SXRET_OK;` |
|        - |  6403 | `	SyBlob sWorker;` |
|        - |  6404 | `	/* Format the message and call the output consumer */` |
|        3 |  6405 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6406 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6407 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6408 | `		/* Consume the formatted message */` |
|        3 |  6409 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6410 | `	}` |
|        3 |  6411 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6412 | `		/* Increment output length */` |
|      ! 0 |  6413 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6414 | `	}` |
|        - |  6415 | `	/* Release the working buffer */` |
|        3 |  6416 | `	SyBlobRelease(&sWorker);` |
|        3 |  6417 | `	return rc;` |
|        1 |  6418 |  |
|        - |  6419 | `/*` |
|        - |  6420 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6421 | ` * This function never fail and always return a pointer` |
|        - |  6422 | ` * to a null terminated string.` |
|        - |  6423 | ` */` |
|       12 |  6424 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6425 |  |
|       13 |  6426 | `	const char *zOp = "Unknown     ";` |
|       13 |  6427 | `	switch(nOp){` |
|        3 |  6428 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6429 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6430 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6431 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6432 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6433 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6434 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6435 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6436 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6437 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6438 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6439 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6440 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6441 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6442 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6443 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6444 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6445 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6446 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6447 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6448 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6449 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6450 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6451 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6452 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6453 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6454 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6455 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6456 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6457 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6458 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6459 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6460 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6461 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6462 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6463 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6464 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6465 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6466 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6467 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6468 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6469 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6470 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6471 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6472 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6473 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6474 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6475 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6476 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6477 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  6478 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6479 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6480 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6481 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6482 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6483 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6484 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6485 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6486 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6487 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6488 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6489 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6490 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6491 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6492 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6493 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6494 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6495 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6496 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6497 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6498 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6499 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6500 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6501 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6502 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6503 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6504 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6505 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6506 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6507 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6508 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6509 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6510 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6511 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6512 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6513 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6514 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6515 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6516 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6517 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6518 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6519 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6520 | `	default:` |
|      ! 0 |  6521 | `		break;` |
|        - |  6522 | `	}` |
|       13 |  6523 | `	return zOp;` |
|        1 |  6524 |  |
|        - |  6525 | `/*` |
|        - |  6526 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6527 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6528 | ` * is responsible of consuming the generated dump.` |
|        - |  6529 | ` */` |
|        2 |  6530 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6531 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6532 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6533 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6534 | `	)` |
|        1 |  6535 |  |
|        - |  6536 | `	sxi32 rc;` |
|        3 |  6537 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6538 | `	return rc;` |
|        1 |  6539 |  |
|        - |  6540 | `/*` |
|        - |  6541 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6542 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6543 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6544 | ` * in 'compile.c' for additional information.` |
|        - |  6545 | ` */` |
|        8 |  6546 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6547 |  |
|        9 |  6548 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6549 | `	/* Evaluate and expand constant value */` |
|        9 |  6550 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6551 |  |
|        - |  6552 | `/*` |
|        - |  6553 | ` * Section:` |
|        - |  6554 | ` *  Function handling functions.` |
|        - |  6555 | ` * Status:` |
|        - |  6556 | ` *    Stable.` |
|        - |  6557 | ` */` |
|        - |  6558 | `/*` |
|        - |  6559 | ` * int func_num_args(void)` |
|        - |  6560 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6561 | ` * Parameters` |
|        - |  6562 | ` *   None.` |
|        - |  6563 | ` * Return` |
|        - |  6564 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6565 | ` *  or -1 if called from the globe scope.` |
|        - |  6566 | ` */` |
|      906 |  6567 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6568 |  |
|        - |  6569 | `	VmFrame *pFrame;` |
|        - |  6570 | `	ph7_vm *pVm;` |
|        - |  6571 | `	/* Point to the target VM */` |
|      908 |  6572 | `	pVm = pCtx->pVm;` |
|        - |  6573 | `	/* Current frame */` |
|      908 |  6574 | `	pFrame = pVm->pFrame;` |
|      908 |  6575 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6576 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6577 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6578 | `	}` |
|      908 |  6579 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6580 | `		SXUNUSED(nArg);` |
|      ! 0 |  6581 | `		SXUNUSED(apArg);` |
|        - |  6582 | `		/* Global frame,return -1 */` |
|      ! 0 |  6583 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6584 | `		return SXRET_OK;` |
|        - |  6585 | `	}` |
|        - |  6586 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6587 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6588 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6589 | `	return SXRET_OK;` |
|      455 |  6590 |  |
|        - |  6591 | `/*` |
|        - |  6592 | ` * value func_get_arg(int $arg_num)` |
|        - |  6593 | ` *   Return an item from the argument list.` |
|        - |  6594 | ` * Parameters` |
|        - |  6595 | ` *  Argument number(index start from zero).` |
|        - |  6596 | ` * Return` |
|        - |  6597 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6598 | ` */` |
|       22 |  6599 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6600 |  |
|       24 |  6601 | `	ph7_value *pObj = 0;` |
|       24 |  6602 | `	VmSlot *pSlot = 0;` |
|        - |  6603 | `	VmFrame *pFrame;` |
|        - |  6604 | `	ph7_vm *pVm;` |
|        - |  6605 | `	/* Point to the target VM */` |
|       24 |  6606 | `	pVm = pCtx->pVm;` |
|        - |  6607 | `	/* Current frame */` |
|       24 |  6608 | `	pFrame = pVm->pFrame;` |
|       24 |  6609 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6610 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6611 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6612 | `	}` |
|       24 |  6613 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6614 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6615 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6616 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6617 | `		return SXRET_OK;` |
|        - |  6618 | `	}` |
|        - |  6619 | `	/* Extract the desired index */` |
|       21 |  6620 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6621 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6622 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6623 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6624 | `		return SXRET_OK;` |
|        - |  6625 | `	}` |
|        - |  6626 | `	/* Extract the desired argument */` |
|       21 |  6627 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6628 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6629 | `			/* Return the desired argument */` |
|       21 |  6630 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6631 | `		}else{` |
|        - |  6632 | `			/* No such argument,return false */` |
|      ! 0 |  6633 | `			ph7_result_bool(pCtx,0);` |
|        - |  6634 | `		}` |
|       11 |  6635 | `	}else{` |
|        - |  6636 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6637 | `		ph7_result_bool(pCtx,0);` |
|        - |  6638 | `	}` |
|       21 |  6639 | `	return SXRET_OK;` |
|       13 |  6640 |  |
|        - |  6641 | `/*` |
|        - |  6642 | ` * array func_get_args_byref(void)` |
|        - |  6643 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6644 | ` * Parameters` |
|        - |  6645 | ` *  None.` |
|        - |  6646 | ` * Return` |
|        - |  6647 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6648 | ` *  member of the current user-defined function's argument list.` |
|        - |  6649 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6650 | ` * NOTE:` |
|        - |  6651 | ` *  Arguments are returned to the array by reference.` |
|        - |  6652 | ` */` |
|        2 |  6653 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6654 |  |
|        - |  6655 | `	ph7_value *pArray;` |
|        - |  6656 | `	VmFrame *pFrame;` |
|        - |  6657 | `	VmSlot *aSlot;` |
|        - |  6658 | `	sxu32 n;` |
|        - |  6659 | `	/* Point to the current frame */` |
|        3 |  6660 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6661 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6662 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6663 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6664 | `	}` |
|        3 |  6665 | `	if( pFrame->pParent == 0 ){` |
|        - |  6666 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6667 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6668 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6669 | `		return SXRET_OK;` |
|        - |  6670 | `	}` |
|        - |  6671 | `	/* Create a new array */` |
|        3 |  6672 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6673 | `	if( pArray == 0 ){` |
|      ! 0 |  6674 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6675 | `		SXUNUSED(apArg);` |
|      ! 0 |  6676 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6677 | `		return SXRET_OK;` |
|        - |  6678 | `	}` |
|        - |  6679 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6680 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6681 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6682 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6683 | `	}` |
|        - |  6684 | `	/* Return the freshly created array */` |
|        3 |  6685 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6686 | `	return SXRET_OK;` |
|        2 |  6687 |  |
|        - |  6688 | `/*` |
|        - |  6689 | ` * array func_get_args(void)` |
|        - |  6690 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6691 | ` * Parameters` |
|        - |  6692 | ` *  None.` |
|        - |  6693 | ` * Return` |
|        - |  6694 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6695 | ` *  member of the current user-defined function's argument list.` |
|        - |  6696 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6697 | ` */` |
|       62 |  6698 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6699 |  |
|       64 |  6700 | `	ph7_value *pObj = 0;` |
|        - |  6701 | `	ph7_value *pArray;` |
|        - |  6702 | `	VmFrame *pFrame;` |
|        - |  6703 | `	VmSlot *aSlot;` |
|        - |  6704 | `	sxu32 n;` |
|        - |  6705 | `	/* Point to the current frame */` |
|       64 |  6706 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6707 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6708 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6709 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6710 | `	}` |
|       64 |  6711 | `	if( pFrame->pParent == 0 ){` |
|        - |  6712 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6713 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6714 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6715 | `		return SXRET_OK;` |
|        - |  6716 | `	}` |
|        - |  6717 | `	/* Create a new array */` |
|       64 |  6718 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6719 | `	if( pArray == 0 ){` |
|      ! 0 |  6720 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6721 | `		SXUNUSED(apArg);` |
|      ! 0 |  6722 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6723 | `		return SXRET_OK;` |
|        - |  6724 | `	}` |
|        - |  6725 | `	/* Start filling the array with the given arguments */` |
|       64 |  6726 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6727 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6728 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6729 | `		if( pObj ){` |
|      130 |  6730 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6731 | `		}` |
|       66 |  6732 | `	}` |
|        - |  6733 | `	/* Return the freshly created array */` |
|       64 |  6734 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6735 | `	return SXRET_OK;` |
|       33 |  6736 |  |
|        - |  6737 | `/*` |
|        - |  6738 | ` * bool function_exists(string $name)` |
|        - |  6739 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6740 | ` * Parameters` |
|        - |  6741 | ` *  The name of the desired function.` |
|        - |  6742 | ` * Return` |
|        - |  6743 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6744 | ` */` |
|     1646 |  6745 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6746 |  |
|        - |  6747 | `	const char *zName;` |
|        - |  6748 | `	ph7_vm *pVm;` |
|        - |  6749 | `	int nLen;` |
|        - |  6750 | `	int res;` |
|     1648 |  6751 | `	if( nArg < 1 ){` |
|        - |  6752 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6753 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6754 | `		return SXRET_OK;` |
|        - |  6755 | `	}` |
|        - |  6756 | `	/* Point to the target VM */` |
|     1648 |  6757 | `	pVm = pCtx->pVm;` |
|        - |  6758 | `	/* Extract the function name */` |
|     1648 |  6759 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6760 | `	/* Assume the function is not defined */` |
|     1648 |  6761 | `	res = 0;` |
|        - |  6762 | `	/* Perform the lookup */` |
|     2469 |  6763 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1642 |  6764 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6765 | `			/* Function is defined */` |
|      206 |  6766 | `			res = 1;` |
|      102 |  6767 | `	}` |
|     1648 |  6768 | `	ph7_result_bool(pCtx,res);` |
|     1648 |  6769 | `	return SXRET_OK;` |
|      825 |  6770 |  |
|        - |  6771 | `/*` |
|        - |  6772 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6773 | ` * [i.e: Whether it is callable or not].` |
|        - |  6774 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6775 | ` */` |
|    16002 |  6776 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6777 |  |
|    16004 |  6778 | `	int res = 0;` |
|    16004 |  6779 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6780 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6781 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6782 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6783 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6784 | `		if( pMethod && CallInvoke ){` |
|        - |  6785 | `			ph7_value sResult;` |
|        - |  6786 | `			sxi32 rc;` |
|        - |  6787 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6788 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6789 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6790 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6791 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6792 | `			}` |
|      ! 0 |  6793 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6794 | `		}` |
|    16004 |  6795 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6796 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6797 | `		if( pMap->nEntry == 2 ){` |
|        - |  6798 | `			ph7_class *pClass;` |
|        - |  6799 | `			ph7_value *pV;` |
|        - |  6800 | `			/* Extract the target class */` |
|       12 |  6801 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6802 | `			if( pV ){` |
|       12 |  6803 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6804 | `				if( pClass ){` |
|        - |  6805 | `					ph7_class_method *pMethod;` |
|        - |  6806 | `					/* Extract the target method */` |
|       10 |  6807 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6808 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6809 | `						/* Perform the lookup */` |
|       10 |  6810 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6811 | `						if( pMethod ){` |
|        - |  6812 | `							/* Method is callable */` |
|        5 |  6813 | `							res = 1;` |
|        2 |  6814 | `						}` |
|        4 |  6815 | `					}` |
|        4 |  6816 | `				}` |
|        5 |  6817 | `			}` |
|        7 |  6818 | `		}` |
|    15991 |  6819 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6820 | `		const char *zName;` |
|        - |  6821 | `		int nLen;` |
|        - |  6822 | `		/* Extract the name */` |
|     4700 |  6823 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6824 | `		/* Perform the lookup */` |
|     4715 |  6825 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6826 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6827 | `				/* Function is callable */` |
|     4682 |  6828 | `				res = 1;` |
|     2340 |  6829 | `		}` |
|     2349 |  6830 | `	}` |
|    16004 |  6831 | `	return res;` |
|        2 |  6832 |  |
|        - |  6833 | `/*` |
|        - |  6834 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6835 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6836 | ` * Parameters` |
|        - |  6837 | ` * $name` |
|        - |  6838 | ` *    The callback function to check` |
|        - |  6839 | ` * $syntax_only` |
|        - |  6840 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6841 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6842 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6843 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6844 | ` *    a string.` |
|        - |  6845 | ` * Return` |
|        - |  6846 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6847 | ` */` |
|       14 |  6848 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6849 |  |
|        - |  6850 | `	ph7_vm *pVm;` |
|        - |  6851 | `	int res;` |
|       15 |  6852 | `	if( nArg < 1 ){` |
|        - |  6853 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6854 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6855 | `		return SXRET_OK;` |
|        - |  6856 | `	}` |
|        - |  6857 | `	/* Point to the target VM */` |
|       15 |  6858 | `	pVm = pCtx->pVm;` |
|        - |  6859 | `	/* Perform the requested operation */` |
|       15 |  6860 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6861 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6862 | `	return SXRET_OK;` |
|        8 |  6863 |  |
|        - |  6864 | `/*` |
|        - |  6865 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6866 | ` * defined below.` |
|        - |  6867 | ` */` |
|     1082 |  6868 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6869 |  |
|     1083 |  6870 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6871 | `	ph7_value sName;` |
|        - |  6872 | `	sxi32 rc;` |
|        - |  6873 | `	/* Prepare the function name for insertion */` |
|     1083 |  6874 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6875 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6876 | `	/* Perform the insertion */` |
|     1083 |  6877 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6878 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6879 | `	return rc;` |
|        1 |  6880 |  |
|        - |  6881 | `/*` |
|        - |  6882 | ` * array get_defined_functions(void)` |
|        - |  6883 | ` *  Returns an array of all defined functions.` |
|        - |  6884 | ` * Parameter` |
|        - |  6885 | ` *  None.` |
|        - |  6886 | ` * Return` |
|        - |  6887 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6888 | ` *  both built-in (internal) and user-defined.` |
|        - |  6889 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6890 | ` *  defined ones using $arr["user"].` |
|        - |  6891 | ` * Note:` |
|        - |  6892 | ` *  NULL is returned on failure.` |
|        - |  6893 | ` */` |
|        2 |  6894 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6895 |  |
|        - |  6896 | `	ph7_value *pArray,*pEntry;` |
|        - |  6897 | `	/* NOTE:` |
|        - |  6898 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6899 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6900 | `	 */` |
|        3 |  6901 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6902 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6903 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6904 | `		SXUNUSED(apArg);` |
|        - |  6905 | `		/* Return NULL */` |
|      ! 0 |  6906 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6907 | `		return SXRET_OK;` |
|        - |  6908 | `	}` |
|        3 |  6909 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6910 | `	if( pEntry == 0 ){` |
|        - |  6911 | `		/* Return NULL */` |
|      ! 0 |  6912 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6913 | `		return SXRET_OK;` |
|        - |  6914 | `	}` |
|        - |  6915 | `	/* Fill with the appropriate information */` |
|        3 |  6916 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6917 | `	/* Create the 'internal' index */` |
|        3 |  6918 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6919 | `	/* Create the user-func array */` |
|        3 |  6920 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6921 | `	if( pEntry == 0 ){` |
|        - |  6922 | `		/* Return NULL */` |
|      ! 0 |  6923 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6924 | `		return SXRET_OK;` |
|        - |  6925 | `	}` |
|        - |  6926 | `	/* Fill with the appropriate information */` |
|        3 |  6927 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6928 | `	/* Create the 'user' index */` |
|        3 |  6929 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6930 | `	/* Return the multi-dimensional array */` |
|        3 |  6931 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6932 | `	return SXRET_OK;` |
|        2 |  6933 |  |
|        - |  6934 | `/*` |
|        - |  6935 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6936 | ` *  Register a function for execution on shutdown.` |
|        - |  6937 | ` * Note` |
|        - |  6938 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6939 | ` *  be called in the same order as they were registered.` |
|        - |  6940 | ` * Parameters` |
|        - |  6941 | ` *  $callback` |
|        - |  6942 | ` *   The shutdown callback to register.` |
|        - |  6943 | ` * $param` |
|        - |  6944 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6945 | ` * Return` |
|        - |  6946 | ` *  Nothing.` |
|        - |  6947 | ` */` |
|        2 |  6948 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6949 |  |
|        - |  6950 | `	VmShutdownCB sEntry;` |
|        - |  6951 | `	int i,j;` |
|        3 |  6952 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6953 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6954 | `		return PH7_OK;` |
|        - |  6955 | `	}` |
|        - |  6956 | `	/* Zero the Entry */` |
|        3 |  6957 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6958 | `	/* Initialize fields */` |
|        3 |  6959 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6960 | `	/* Save the callback name for later invocation name */` |
|        3 |  6961 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6962 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6963 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6964 | `	}` |
|        - |  6965 | `	/* Copy arguments */` |
|        3 |  6966 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6967 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6968 | `			/* Limit reached */` |
|      ! 0 |  6969 | `			break;` |
|        - |  6970 | `		}` |
|      ! 0 |  6971 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6972 | `	}` |
|        3 |  6973 | `	sEntry.nArg = j;` |
|        - |  6974 | `	/* Install the callback */` |
|        3 |  6975 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6976 | `	return PH7_OK;` |
|        2 |  6977 |  |
|        - |  6978 | `/*` |
|        - |  6979 | ` * Section:` |
|        - |  6980 | ` *  Class handling functions.` |
|        - |  6981 | ` * Status:` |
|        - |  6982 | ` *    Stable.` |
|        - |  6983 | ` */` |
|        - |  6984 | `/*` |
|        - |  6985 | ` * Extract the top active class. NULL is returned` |
|        - |  6986 | ` * if the class stack is empty.` |
|        - |  6987 | ` */` |
|      536 |  6988 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6989 |  |
|      538 |  6990 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6991 | `	ph7_class **apClass;` |
|      538 |  6992 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6993 | `		/* Empty stack,return NULL */` |
|       15 |  6994 | `		return 0;` |
|        - |  6995 | `	}` |
|        - |  6996 | `	/* Peek the last entry */` |
|      524 |  6997 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      524 |  6998 | `	return apClass[pSet->nUsed - 1];` |
|      270 |  6999 |  |
|        - |  7000 | `/*` |
|        - |  7001 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  7002 | ` *   Get the class that declared the currently executing method.` |
|        - |  7003 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  7004 | ` *` |
|        - |  7005 | ` * Parameters` |
|        - |  7006 | ` *   pVm: Target VM` |
|        - |  7007 | ` *` |
|        - |  7008 | ` * Return` |
|        - |  7009 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  7010 | ` *   - Not executing within a class method` |
|        - |  7011 | ` *` |
|        - |  7012 | ` * Note` |
|        - |  7013 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  7014 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  7015 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  7016 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  7017 | ` *   declaring class.` |
|        - |  7018 | ` */` |
|       48 |  7019 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  7020 |  |
|       50 |  7021 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  7022 | `	ph7_vm_func *pVmFunc;` |
|        - |  7023 |  |
|        - |  7024 | `	/* Skip exception frames to find the actual method frame */` |
|       50 |  7025 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  7026 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  7027 | `	}` |
|        - |  7028 |  |
|        - |  7029 | `	/* Check if we're in a method context */` |
|       50 |  7030 | `	if( pFrame->pParent ){` |
|       46 |  7031 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       46 |  7032 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  7033 | `			/* Return the declaring class */` |
|       46 |  7034 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  7035 | `		}` |
|      ! 0 |  7036 | `	}` |
|        - |  7037 |  |
|        5 |  7038 | `	return 0;` |
|       26 |  7039 |  |
|        - |  7040 |  |
|        - |  7041 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  7042 | `/*` |
|        - |  7043 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7044 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7045 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7046 | ` * return value indicates failure.` |
|        - |  7047 | ` */` |
|     1294 |  7048 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7049 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7050 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7051 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7052 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7053 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7054 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7055 | `	)` |
|        2 |  7056 |  |
|        - |  7057 | `	ph7_value *aStack;` |
|        - |  7058 | `	VmInstr aInstr[2];` |
|        - |  7059 | `	int iCursor;` |
|        - |  7060 | `	int i;` |
|        - |  7061 | `	/* Create a new operand stack */` |
|     1296 |  7062 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1296 |  7063 | `	if( aStack == 0 ){` |
|      ! 0 |  7064 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7065 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7066 | `		return SXERR_MEM;` |
|        - |  7067 | `	}` |
|        - |  7068 | `	/* Fill the operand stack with the given arguments */` |
|     1866 |  7069 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      572 |  7070 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7071 | `		/*` |
|        - |  7072 | `		 * Symisc eXtension:` |
|        - |  7073 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7074 | `		 */` |
|      572 |  7075 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      287 |  7076 | `	}` |
|     1296 |  7077 | `	iCursor = nArg + 1;` |
|     1296 |  7078 | `	if( pThis ){` |
|        - |  7079 | `		/*` |
|        - |  7080 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7081 | `		 */` |
|     1290 |  7082 | `		pThis->iRef++; /* Increment reference count */` |
|     1290 |  7083 | `		aStack[i].x.pOther = pThis;` |
|     1290 |  7084 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      644 |  7085 | `	}` |
|     1296 |  7086 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1296 |  7087 | `	i++;` |
|        - |  7088 | `	/* Push method name */` |
|     1296 |  7089 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1296 |  7090 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1296 |  7091 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1296 |  7092 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7093 | `	/* Emit the CALL istruction */` |
|     1296 |  7094 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1296 |  7095 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1296 |  7096 | `	aInstr[0].iP2 = 0;` |
|     1296 |  7097 | `	aInstr[0].p3  = 0;` |
|        - |  7098 | `	/* Emit the DONE instruction */` |
|     1296 |  7099 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1296 |  7100 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1296 |  7101 | `	aInstr[1].iP2 = 0;` |
|     1296 |  7102 | `	aInstr[1].p3  = 0;` |
|        - |  7103 | `	/* Execute the method body (if available) */` |
|     1296 |  7104 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7105 | `	/* Clean up the mess left behind */` |
|     1296 |  7106 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1296 |  7107 | `	return PH7_OK;` |
|      649 |  7108 |  |
|        - |  7109 | `/*` |
|        - |  7110 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7111 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7112 | ` * in the apArg[] array.` |
|        - |  7113 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7114 | ` * return value indicates failure.` |
|        - |  7115 | ` */` |
|      926 |  7116 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7117 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7118 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7119 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7120 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7121 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7122 | `	)` |
|        2 |  7123 |  |
|        - |  7124 | `	ph7_value *aStack;` |
|        - |  7125 | `	VmInstr aInstr[2];` |
|        - |  7126 | `	int i;` |
|      928 |  7127 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7128 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  7129 | `		if( pResult ){` |
|        - |  7130 | `			/* Assume a null return value */` |
|      ! 0 |  7131 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7132 | `		}` |
|      471 |  7133 | `		return SXERR_INVALID;` |
|        - |  7134 | `	}` |
|      458 |  7135 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7136 | `		/* Class method */` |
|       11 |  7137 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7138 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7139 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7140 | `		ph7_class *pClass = 0;` |
|        - |  7141 | `		ph7_value *pValue;` |
|        - |  7142 | `		sxi32 rc;` |
|       11 |  7143 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7144 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7145 | `			if( pResult ){` |
|        - |  7146 | `				/* Assume a null return value */` |
|      ! 0 |  7147 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7148 | `			}` |
|      ! 0 |  7149 | `			return SXRET_OK;` |
|        - |  7150 | `		}` |
|        - |  7151 | `		/* Extract the class name or an instance of it */` |
|       11 |  7152 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7153 | `		if( pValue ){` |
|       11 |  7154 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7155 | `		}` |
|       11 |  7156 | `		if( pClass == 0 ){` |
|        - |  7157 | `			/* No such class,return NULL */` |
|      ! 0 |  7158 | `			if( pResult ){` |
|      ! 0 |  7159 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7160 | `			}` |
|      ! 0 |  7161 | `			return SXRET_OK;` |
|        - |  7162 | `		}` |
|       11 |  7163 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7164 | `			/* Point to the class instance */` |
|        5 |  7165 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7166 | `		}` |
|        - |  7167 | `		/* Try to extract the method */` |
|       11 |  7168 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7169 | `		if( pValue ){` |
|       11 |  7170 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7171 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7172 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7173 | `			}` |
|        5 |  7174 | `		}` |
|       11 |  7175 | `		if( pMethod == 0 ){` |
|        - |  7176 | `			/* No such method,return NULL */` |
|      ! 0 |  7177 | `			if( pResult ){` |
|      ! 0 |  7178 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7179 | `			}` |
|      ! 0 |  7180 | `			return SXRET_OK;` |
|        - |  7181 | `		}` |
|        - |  7182 | `		/* Call the class method */` |
|       11 |  7183 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7184 | `		return rc;` |
|        - |  7185 | `	}` |
|        - |  7186 | `	/* Create a new operand stack */` |
|      448 |  7187 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7188 | `	if( aStack == 0 ){` |
|      ! 0 |  7189 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7190 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7191 | `		if( pResult ){` |
|        - |  7192 | `			/* Assume a null return value */` |
|      ! 0 |  7193 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7194 | `		}` |
|      ! 0 |  7195 | `		return SXERR_MEM;` |
|        - |  7196 | `	}` |
|        - |  7197 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7198 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7199 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7200 | `		/*` |
|        - |  7201 | `		 * Symisc eXtension:` |
|        - |  7202 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7203 | `		 */` |
|     1024 |  7204 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7205 | `	}` |
|        - |  7206 | `	/* Push the function name */` |
|      448 |  7207 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7208 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7209 | `	/* Emit the CALL istruction */` |
|      448 |  7210 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7211 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7212 | `	aInstr[0].iP2 = 0;` |
|      448 |  7213 | `	aInstr[0].p3  = 0;` |
|        - |  7214 | `	/* Emit the DONE instruction */` |
|      448 |  7215 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7216 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7217 | `	aInstr[1].iP2 = 0;` |
|      448 |  7218 | `	aInstr[1].p3  = 0;` |
|        - |  7219 | `	/* Execute the function body (if available) */` |
|      448 |  7220 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7221 | `	/* Clean up the mess left behind */` |
|      448 |  7222 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7223 | `	return PH7_OK;` |
|      465 |  7224 |  |
|        - |  7225 | `/*` |
|        - |  7226 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7227 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7228 | ` * parameter.` |
|        - |  7229 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7230 | ` * return value indicates failure.` |
|        - |  7231 | ` */` |
|      236 |  7232 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7233 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7234 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7235 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7236 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7237 | `	)` |
|        1 |  7238 |  |
|        - |  7239 | `	ph7_value *pArg;` |
|        - |  7240 | `	SySet aArg;` |
|        - |  7241 | `	va_list ap;` |
|        - |  7242 | `	sxi32 rc;` |
|      237 |  7243 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7244 | `	/* Copy arguments one after one */` |
|      237 |  7245 | `	va_start(ap,pResult);` |
|      393 |  7246 | `	for(;;){` |
|      787 |  7247 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7248 | `		if( pArg == 0 ){` |
|      237 |  7249 | `			break;` |
|        - |  7250 | `		}` |
|      551 |  7251 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7252 | `	}` |
|        - |  7253 | `	/* Call the core routine */` |
|      237 |  7254 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7255 | `	/* Cleanup */` |
|      237 |  7256 | `	SySetRelease(&aArg);` |
|      237 |  7257 | `	return rc;` |
|        1 |  7258 |  |
|        - |  7259 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7260 | `/*` |
|        - |  7261 | ` * bool defined(string $name)` |
|        - |  7262 | ` *  Checks whether a given named constant exists.` |
|        - |  7263 | ` * Parameter:` |
|        - |  7264 | ` *  Name of the desired constant.` |
|        - |  7265 | ` * Return` |
|        - |  7266 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7267 | ` */` |
|       14 |  7268 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7269 |  |
|        - |  7270 | `	const char *zName;` |
|       16 |  7271 | `	int nLen = 0;` |
|       16 |  7272 | `	int res = 0;` |
|       16 |  7273 | `	if( nArg < 1 ){` |
|        - |  7274 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7275 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7276 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7277 | `		return SXRET_OK;` |
|        - |  7278 | `	}` |
|        - |  7279 | `	/* Extract constant name */` |
|       16 |  7280 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7281 | `	/* Perform the lookup */` |
|       16 |  7282 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7283 | `		/* Already defined */` |
|       10 |  7284 | `		res = 1;` |
|        4 |  7285 | `	}` |
|       16 |  7286 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7287 | `	return SXRET_OK;` |
|        9 |  7288 |  |
|        - |  7289 | `/*` |
|        - |  7290 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7291 | ` * below.` |
|        - |  7292 | ` */` |
|        8 |  7293 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7294 |  |
|       10 |  7295 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7296 | `	/* Expand constant value */` |
|       10 |  7297 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7298 |  |
|        - |  7299 | `/*` |
|        - |  7300 | ` * bool define(string $constant_name,expression value)` |
|        - |  7301 | ` *  Defines a named constant at runtime.` |
|        - |  7302 | ` * Parameter:` |
|        - |  7303 | ` *  $constant_name` |
|        - |  7304 | ` *   The name of the constant` |
|        - |  7305 | ` *  $value` |
|        - |  7306 | ` *   Constant value` |
|        - |  7307 | ` * Return:` |
|        - |  7308 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7309 | ` */` |
|       10 |  7310 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7311 |  |
|        - |  7312 | `	const char *zName;  /* Constant name */` |
|        - |  7313 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7314 | `	int nLen = 0;       /* Name length */` |
|        - |  7315 | `	sxi32 rc;` |
|       12 |  7316 | `	if( nArg < 2 ){` |
|        - |  7317 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7318 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7319 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7320 | `		return SXRET_OK;` |
|        - |  7321 | `	}` |
|       12 |  7322 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7323 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7324 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7325 | `		return SXRET_OK;` |
|        - |  7326 | `	}` |
|        - |  7327 | `	/* Extract constant name */` |
|       12 |  7328 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7329 | `	if( nLen < 1 ){` |
|      ! 0 |  7330 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7331 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7332 | `		return SXRET_OK;` |
|        - |  7333 | `	}` |
|        - |  7334 | `	/* Duplicate constant value */` |
|       12 |  7335 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7336 | `	if( pValue == 0 ){` |
|      ! 0 |  7337 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7338 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7339 | `		return SXRET_OK;` |
|        - |  7340 | `	}` |
|        - |  7341 | `	/* Initialize the memory object */` |
|       12 |  7342 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7343 | `	/* Register the constant */` |
|       12 |  7344 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7345 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7346 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7347 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7348 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7349 | `		return SXRET_OK;` |
|        - |  7350 | `	}` |
|        - |  7351 | `	/* Duplicate constant value */` |
|       12 |  7352 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7353 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7354 | `		/* Lower case the constant name */` |
|      ! 0 |  7355 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7356 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7357 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7358 | `				/* UTF-8 stream */` |
|      ! 0 |  7359 | `				zCur++;` |
|      ! 0 |  7360 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7361 | `					zCur++;` |
|      ! 0 |  7362 | `				}` |
|      ! 0 |  7363 | `				continue;` |
|        - |  7364 | `			}` |
|      ! 0 |  7365 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7366 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7367 | `				zCur[0] = (char)c;` |
|      ! 0 |  7368 | `			}` |
|      ! 0 |  7369 | `			zCur++;` |
|      ! 0 |  7370 | `		}` |
|        - |  7371 | `		/* Finally,register the constant */` |
|      ! 0 |  7372 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7373 | `	}` |
|        - |  7374 | `	/* All done,return TRUE */` |
|       12 |  7375 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7376 | `	return SXRET_OK;` |
|        7 |  7377 |  |
|        - |  7378 | `/*` |
|        - |  7379 | ` * value constant(string $name)` |
|        - |  7380 | ` *  Returns the value of a constant` |
|        - |  7381 | ` * Parameter` |
|        - |  7382 | ` *  $name` |
|        - |  7383 | ` *    Name of the constant.` |
|        - |  7384 | ` * Return` |
|        - |  7385 | ` *  Constant value or NULL if not defined.` |
|        - |  7386 | ` */` |
|        8 |  7387 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7388 |  |
|        - |  7389 | `	SyHashEntry *pEntry;` |
|        - |  7390 | `	ph7_constant *pCons;` |
|        - |  7391 | `	const char *zName; /* Constant name */` |
|        - |  7392 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7393 | `	int nLen;` |
|       10 |  7394 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7395 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7396 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7397 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7398 | `		return SXRET_OK;` |
|        - |  7399 | `	}` |
|        - |  7400 | `	/* Extract the constant name */` |
|       10 |  7401 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7402 | `	/* Perform the query */` |
|       10 |  7403 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7404 | `	if( pEntry == 0 ){` |
|        3 |  7405 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7406 | `		ph7_result_null(pCtx);` |
|        3 |  7407 | `		return SXRET_OK;` |
|        - |  7408 | `	}` |
|        8 |  7409 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7410 | `	/* Point to the structure that describe the constant */` |
|        8 |  7411 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7412 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7413 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7414 | `	/* Return that value */` |
|        8 |  7415 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7416 | `	/* Cleanup */` |
|        8 |  7417 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7418 | `	return SXRET_OK;` |
|        6 |  7419 |  |
|        - |  7420 | `/*` |
|        - |  7421 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7422 | ` * defined below.` |
|        - |  7423 | ` */` |
|      416 |  7424 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7425 |  |
|      417 |  7426 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7427 | `	ph7_value sName;` |
|        - |  7428 | `	sxi32 rc;` |
|        - |  7429 | `	/* Prepare the constant name for insertion */` |
|      417 |  7430 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7431 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7432 | `	/* Perform the insertion */` |
|      417 |  7433 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7434 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7435 | `	return rc;` |
|        1 |  7436 |  |
|        - |  7437 | `/*` |
|        - |  7438 | ` * array get_defined_constants(void)` |
|        - |  7439 | ` *  Returns an associative array with the names of all defined` |
|        - |  7440 | ` *  constants.` |
|        - |  7441 | ` * Parameters` |
|        - |  7442 | ` *  NONE.` |
|        - |  7443 | ` * Returns` |
|        - |  7444 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7445 | ` */` |
|        2 |  7446 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7447 |  |
|        - |  7448 | `	ph7_value *pArray;` |
|        - |  7449 | `	/* Create the array first*/` |
|        3 |  7450 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7451 | `	if( pArray == 0 ){` |
|      ! 0 |  7452 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7453 | `		SXUNUSED(apArg);` |
|        - |  7454 | `		/* Return NULL */` |
|      ! 0 |  7455 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7456 | `		return SXRET_OK;` |
|        - |  7457 | `	}` |
|        - |  7458 | `	/* Fill the array with the defined constants */` |
|        3 |  7459 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7460 | `	/* Return the created array */` |
|        3 |  7461 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7462 | `	return SXRET_OK;` |
|        2 |  7463 |  |
|        - |  7464 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7465 | `/*` |
|        - |  7466 | ` * Section:` |
|        - |  7467 | ` *  Random numbers/string generators.` |
|        - |  7468 | ` * Status:` |
|        - |  7469 | ` *    Stable.` |
|        - |  7470 | ` */` |
|        - |  7471 | `/*` |
|        - |  7472 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7473 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7474 | ` * used by te SQLite3 library.` |
|        - |  7475 | ` */` |
|     2320 |  7476 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7477 |  |
|        - |  7478 | `	sxu32 iNum;` |
|     2322 |  7479 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2322 |  7480 | `	return iNum;` |
|        2 |  7481 |  |
|        - |  7482 | `/*` |
|        - |  7483 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7484 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7485 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7486 | ` * by te SQLite3 library.` |
|        - |  7487 | ` */` |
|    73232 |  7488 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7489 |  |
|        - |  7490 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7491 | `	int i;` |
|        - |  7492 | `	/* Generate a binary string first */` |
|    73234 |  7493 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7494 | `	/* Turn the binary string into english based alphabet */` |
|   805722 |  7495 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   732490 |  7496 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   366246 |  7497 | `	 }` |
|    73234 |  7498 |  |
|        - |  7499 | `/*` |
|        - |  7500 | ` * int rand()` |
|        - |  7501 | ` * int mt_rand()` |
|        - |  7502 | ` * int rand(int $min,int $max)` |
|        - |  7503 | ` * int mt_rand(int $min,int $max)` |
|        - |  7504 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7505 | ` * Parameter` |
|        - |  7506 | ` *  $min` |
|        - |  7507 | ` *    The lowest value to return (default: 0)` |
|        - |  7508 | ` *  $max` |
|        - |  7509 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7510 | ` * Return` |
|        - |  7511 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7512 | ` * Note:` |
|        - |  7513 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7514 | ` *  by te SQLite3 library.` |
|        - |  7515 | ` */` |
|       20 |  7516 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7517 |  |
|        - |  7518 | `	sxu32 iNum;` |
|        - |  7519 | `	/* Generate the random number */` |
|       21 |  7520 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7521 | `	if( nArg > 1 ){` |
|        - |  7522 | `		sxu32 iMin,iMax;` |
|        3 |  7523 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7524 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7525 | `		if( iMin < iMax ){` |
|        3 |  7526 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7527 | `			if( iDiv > 0 ){` |
|        3 |  7528 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7529 | `			}` |
|        1 |  7530 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7531 | `			iNum %= iMax;` |
|      ! 0 |  7532 | `		}` |
|        1 |  7533 | `	}` |
|        - |  7534 | `	/* Return the number */` |
|       21 |  7535 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7536 | `	return SXRET_OK;` |
|        1 |  7537 |  |
|        - |  7538 | `/*` |
|        - |  7539 | ` * int getrandmax(void)` |
|        - |  7540 | ` * int mt_getrandmax(void)` |
|        - |  7541 | ` * int rc4_getrandmax(void)` |
|        - |  7542 | ` *   Show largest possible random value` |
|        - |  7543 | ` * Return` |
|        - |  7544 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7545 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7546 | ` * Note:` |
|        - |  7547 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7548 | ` *  by te SQLite3 library.` |
|        - |  7549 | ` */` |
|        4 |  7550 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7551 |  |
|        2 |  7552 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7553 | `	SXUNUSED(apArg);` |
|        5 |  7554 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7555 | `	return SXRET_OK;` |
|        1 |  7556 |  |
|        - |  7557 | `/*` |
|        - |  7558 | ` * string rand_str()` |
|        - |  7559 | ` * string rand_str(int $len)` |
|        - |  7560 | ` *  Generate a random string (English alphabet).` |
|        - |  7561 | ` * Parameter` |
|        - |  7562 | ` *  $len` |
|        - |  7563 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7564 | ` * Return` |
|        - |  7565 | ` *   A pseudo random string.` |
|        - |  7566 | ` * Note:` |
|        - |  7567 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7568 | ` *  by te SQLite3 library.` |
|        - |  7569 | ` *  This function is a symisc extension.` |
|        - |  7570 | ` */` |
|      120 |  7571 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7572 |  |
|        - |  7573 | `	char zString[1024];` |
|      122 |  7574 | `	int iLen = 0x10;` |
|      122 |  7575 | `	if( nArg > 0 ){` |
|        - |  7576 | `		/* Get the desired length */` |
|      122 |  7577 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7578 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7579 | `			/* Default length */` |
|        3 |  7580 | `			iLen = 0x10;` |
|        1 |  7581 | `		}` |
|       60 |  7582 | `	}` |
|        - |  7583 | `	/* Generate the random string */` |
|      122 |  7584 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7585 | `	/* Return the generated string */` |
|      122 |  7586 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7587 | `	return SXRET_OK;` |
|        2 |  7588 |  |
|        - |  7589 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7590 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7591 | `/* Unique ID private data */` |
|        - |  7592 | `struct unique_id_data` |
|        - |  7593 |  |
|        - |  7594 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7595 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7596 | `};` |
|        - |  7597 | `/*` |
|        - |  7598 | ` * Binary to hex consumer callback.` |
|        - |  7599 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7600 | ` * defined below.` |
|        - |  7601 | ` */` |
|      192 |  7602 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7603 |  |
|      193 |  7604 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7605 | `	sxu32 nBuflen;` |
|        - |  7606 | `	/* Extract result buffer length */` |
|      193 |  7607 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7608 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7609 | `			/*` |
|        - |  7610 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7611 | `			 * string will be 13 characters long` |
|        - |  7612 | `			 */` |
|       25 |  7613 | `		return SXERR_ABORT;` |
|        - |  7614 | `	}` |
|      169 |  7615 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7616 | `		return SXERR_ABORT;` |
|        - |  7617 | `	}` |
|        - |  7618 | `	/* Safely Consume the hex stream */` |
|      169 |  7619 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7620 | `	return SXRET_OK;` |
|       97 |  7621 |  |
|        - |  7622 | `/*` |
|        - |  7623 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7624 | ` *  Generate a unique ID` |
|        - |  7625 | ` * Parameter` |
|        - |  7626 | ` * $prefix` |
|        - |  7627 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7628 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7629 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7630 | ` * $more_entropy` |
|        - |  7631 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7632 | ` *  that the result will be unique.` |
|        - |  7633 | ` * Return` |
|        - |  7634 | ` *  Returns the unique identifier, as a string.` |
|        - |  7635 | ` */` |
|       24 |  7636 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7637 |  |
|        - |  7638 | `	struct unique_id_data sUniq;` |
|        - |  7639 | `	unsigned char zDigest[20];` |
|       25 |  7640 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7641 | `	const char *zPrefix;` |
|        - |  7642 | `	SHA1Context sCtx;` |
|        - |  7643 | `	char zRandom[7];` |
|        - |  7644 | `	int nPrefix;` |
|        - |  7645 | `	int entropy;` |
|        - |  7646 | `	/* Generate a random string first */` |
|       25 |  7647 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7648 | `	/* Initialize fields */` |
|       25 |  7649 | `	zPrefix = 0;` |
|       25 |  7650 | `	nPrefix = 0;` |
|       25 |  7651 | `	entropy = 0;` |
|       25 |  7652 | `	if( nArg > 0 ){` |
|        - |  7653 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7654 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7655 | `		if( nArg > 1 ){` |
|      ! 0 |  7656 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7657 | `		}` |
|      ! 0 |  7658 | `	}` |
|       25 |  7659 | `	SHA1Init(&sCtx);` |
|        - |  7660 | `	/* Generate the random ID */` |
|       25 |  7661 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7662 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7663 | `	}` |
|        - |  7664 | `	/* Append the random ID */` |
|       25 |  7665 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7666 | `	/* Append the random string */` |
|       25 |  7667 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7668 | `	/* Increment the number */` |
|       25 |  7669 | `	pVm->unique_id++;` |
|       25 |  7670 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7671 | `	/* Hexify the digest */` |
|       25 |  7672 | `	sUniq.pCtx = pCtx;` |
|       25 |  7673 | `	sUniq.entropy = entropy;` |
|       25 |  7674 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7675 | `	/* All done */` |
|       25 |  7676 | `	return PH7_OK;` |
|        1 |  7677 |  |
|        - |  7678 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7679 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7680 | `/*` |
|        - |  7681 | ` * Section:` |
|        - |  7682 | ` *  Language construct implementation as foreign functions.` |
|        - |  7683 | ` * Status:` |
|        - |  7684 | ` *    Stable.` |
|        - |  7685 | ` */` |
|        - |  7686 | `/*` |
|        - |  7687 | ` * void echo($string...)` |
|        - |  7688 | ` *  Output one or more messages.` |
|        - |  7689 | ` * Parameters` |
|        - |  7690 | ` *  $string` |
|        - |  7691 | ` *   Message to output.` |
|        - |  7692 | ` * Return` |
|        - |  7693 | ` *  NULL.` |
|        - |  7694 | ` */` |
|      ! 0 |  7695 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7696 |  |
|        - |  7697 | `	const char *zData;` |
|      ! 0 |  7698 | `	int nDataLen = 0;` |
|        - |  7699 | `	ph7_vm *pVm;` |
|        - |  7700 | `	int i,rc;` |
|        - |  7701 | `	/* Point to the target VM */` |
|      ! 0 |  7702 | `	pVm = pCtx->pVm;` |
|        - |  7703 | `	/* Output */` |
|      ! 0 |  7704 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7705 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7706 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7707 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7708 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7709 | `				/* Increment output length */` |
|      ! 0 |  7710 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7711 | `			}` |
|      ! 0 |  7712 | `			if( rc == SXERR_ABORT ){` |
|        - |  7713 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7714 | `				return PH7_ABORT;` |
|        - |  7715 | `			}` |
|      ! 0 |  7716 | `		}` |
|      ! 0 |  7717 | `	}` |
|      ! 0 |  7718 | `	return SXRET_OK;` |
|      ! 0 |  7719 |  |
|        - |  7720 | `/*` |
|        - |  7721 | ` * int print($string...)` |
|        - |  7722 | ` *  Output one or more messages.` |
|        - |  7723 | ` * Parameters` |
|        - |  7724 | ` *  $string` |
|        - |  7725 | ` *   Message to output.` |
|        - |  7726 | ` * Return` |
|        - |  7727 | ` *  1 always.` |
|        - |  7728 | ` */` |
|        2 |  7729 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7730 |  |
|        - |  7731 | `	const char *zData;` |
|        3 |  7732 | `	int nDataLen = 0;` |
|        - |  7733 | `	ph7_vm *pVm;` |
|        - |  7734 | `	int i,rc;` |
|        - |  7735 | `	/* Point to the target VM */` |
|        3 |  7736 | `	pVm = pCtx->pVm;` |
|        - |  7737 | `	/* Output */` |
|        5 |  7738 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7739 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7740 | `		if( nDataLen > 0 ){` |
|        3 |  7741 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7742 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7743 | `				/* Increment output length */` |
|        3 |  7744 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7745 | `			}` |
|        3 |  7746 | `			if( rc == SXERR_ABORT ){` |
|        - |  7747 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7748 | `				return PH7_ABORT;` |
|        - |  7749 | `			}` |
|        1 |  7750 | `		}` |
|        2 |  7751 | `	}` |
|        - |  7752 | `	/* Return 1 */` |
|        3 |  7753 | `	ph7_result_int(pCtx,1);` |
|        3 |  7754 | `	return SXRET_OK;` |
|        2 |  7755 |  |
|        - |  7756 | `/*` |
|        - |  7757 | ` * void exit(string $msg)` |
|        - |  7758 | ` * void exit(int $status)` |
|        - |  7759 | ` * void die(string $ms)` |
|        - |  7760 | ` * void die(int $status)` |
|        - |  7761 | ` *   Output a message and terminate program execution.` |
|        - |  7762 | ` * Parameter` |
|        - |  7763 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7764 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7765 | ` *  and not printed` |
|        - |  7766 | ` * Return` |
|        - |  7767 | ` *  NULL` |
|        - |  7768 | ` */` |
|      ! 0 |  7769 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7770 |  |
|      ! 0 |  7771 | `	if( nArg > 0 ){` |
|      ! 0 |  7772 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7773 | `			const char *zData;` |
|      ! 0 |  7774 | `			int iLen = 0;` |
|        - |  7775 | `			/* Print exit message */` |
|      ! 0 |  7776 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7777 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7778 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7779 | `			sxi32 iExitStatus;` |
|        - |  7780 | `			/* Record exit status code */` |
|      ! 0 |  7781 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7782 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7783 | `		}` |
|      ! 0 |  7784 | `	}` |
|        - |  7785 | `	/* Check if we are in an included file */` |
|      ! 0 |  7786 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7787 | `		/* Exit the entire process */` |
|      ! 0 |  7788 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7789 | `	}` |
|        - |  7790 | `	/* Abort processing immediately */` |
|      ! 0 |  7791 | `	return PH7_ABORT;` |
|      ! 0 |  7792 |  |
|        - |  7793 | `/*` |
|        - |  7794 | ` * bool isset($var,...)` |
|        - |  7795 | ` *  Finds out whether a variable is set.` |
|        - |  7796 | ` * Parameters` |
|        - |  7797 | ` *  One or more variable to check.` |
|        - |  7798 | ` * Return` |
|        - |  7799 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7800 | ` */` |
|    71022 |  7801 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7802 |  |
|        - |  7803 | `	ph7_value *pObj;` |
|    71024 |  7804 | `	int res = 0;` |
|        - |  7805 | `	int i;` |
|    71024 |  7806 | `	if( nArg < 1 ){` |
|        - |  7807 | `		/* Missing arguments,return false */` |
|      ! 0 |  7808 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7809 | `		return SXRET_OK;` |
|        - |  7810 | `	}` |
|        - |  7811 | `	/* Iterate over available arguments */` |
|    93786 |  7812 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    71024 |  7813 | `		pObj = apArg[i];` |
|    71024 |  7814 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    47756 |  7815 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7816 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7817 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7818 | `			}` |
|    23877 |  7819 | `		}` |
|    71024 |  7820 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    71024 |  7821 | `		if( !res ){` |
|        - |  7822 | `			/* Variable not set,return FALSE */` |
|    48262 |  7823 | `			ph7_result_bool(pCtx,0);` |
|    48262 |  7824 | `			return SXRET_OK;` |
|        - |  7825 | `		}` |
|    11383 |  7826 | `	}` |
|        - |  7827 | `	/* All given variable are set,return TRUE */` |
|    22764 |  7828 | `	ph7_result_bool(pCtx,1);` |
|    22764 |  7829 | `	return SXRET_OK;` |
|    35513 |  7830 |  |
|        - |  7831 | `/*` |
|        - |  7832 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7833 | ` * frame,the reference table and discard it's contents.` |
|        - |  7834 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7835 | ` */` |
|  2962448 |  7836 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7837 |  |
|        - |  7838 | `	ph7_value *pObj;` |
|        - |  7839 | `	VmRefObj *pRef;` |
|  2962450 |  7840 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2962450 |  7841 | `	if( pObj ){` |
|        - |  7842 | `		/* Release the object */` |
|  2962450 |  7843 | `		PH7_MemObjRelease(pObj);` |
|  1481224 |  7844 | `	}` |
|        - |  7845 | `	/* Remove old reference links */` |
|  2962450 |  7846 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2962450 |  7847 | `	if( pRef ){` |
|  2962430 |  7848 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7849 | `		/* Unlink from the reference table */` |
|  2962430 |  7850 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2962430 |  7851 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7852 | `			VmSlot sFree;` |
|        - |  7853 | `			/* Restore to the free list */` |
|  2962424 |  7854 | `			sFree.nIdx = nObjIdx;` |
|  2962424 |  7855 | `			sFree.pUserData = 0;` |
|  2962424 |  7856 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1481211 |  7857 | `		}` |
|  1481214 |  7858 | `	}` |
|  2962450 |  7859 | `	return SXRET_OK;` |
|        2 |  7860 |  |
|        - |  7861 | `/*` |
|        - |  7862 | ` * void unset($var,...)` |
|        - |  7863 | ` *   Unset one or more given variable.` |
|        - |  7864 | ` * Parameters` |
|        - |  7865 | ` *  One or more variable to unset.` |
|        - |  7866 | ` * Return` |
|        - |  7867 | ` *  Nothing.` |
|        - |  7868 | ` */` |
|     3258 |  7869 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7870 |  |
|        - |  7871 | `	ph7_value *pObj;` |
|        - |  7872 | `	ph7_vm *pVm;` |
|        - |  7873 | `	int i;` |
|        - |  7874 | `	/* Point to the target VM */` |
|     3260 |  7875 | `	pVm = pCtx->pVm;` |
|        - |  7876 | `	/* Iterate and unset */` |
|     9662 |  7877 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6404 |  7878 | `		pObj = apArg[i];` |
|     6404 |  7879 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      868 |  7880 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7881 | `				/* Throw an error */` |
|      ! 0 |  7882 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7883 | `			}` |
|      435 |  7884 | `		}else{` |
|     5537 |  7885 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7886 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5537 |  7887 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5531 |  7888 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2765 |  7889 | `			}` |
|        - |  7890 | `		}` |
|     3203 |  7891 | `	}` |
|     3260 |  7892 | `	return SXRET_OK;` |
|        2 |  7893 |  |
|        - |  7894 | `/*` |
|        - |  7895 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7896 | ` */` |
|      110 |  7897 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7898 |  |
|      111 |  7899 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7900 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7901 | `	ph7_value *pObj;` |
|        - |  7902 | `	sxu32 nIdx;` |
|        - |  7903 | `	/* Extract the memory object */` |
|      111 |  7904 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7905 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7906 | `	if( pObj ){` |
|      111 |  7907 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7908 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7909 | `				SyString sName;` |
|        - |  7910 | `				ph7_value sKey;` |
|        - |  7911 | `				/* Perform the insertion */` |
|      109 |  7912 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7913 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7914 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7915 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7916 | `			}` |
|       54 |  7917 | `		}` |
|       55 |  7918 | `	}` |
|      111 |  7919 | `	return SXRET_OK;` |
|        1 |  7920 |  |
|        - |  7921 | `/*` |
|        - |  7922 | ` * array get_defined_vars(void)` |
|        - |  7923 | ` *  Returns an array of all defined variables.` |
|        - |  7924 | ` * Parameter` |
|        - |  7925 | ` *  None` |
|        - |  7926 | ` * Return` |
|        - |  7927 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7928 | ` */` |
|        2 |  7929 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7930 |  |
|        3 |  7931 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7932 | `	ph7_value *pArray;` |
|        - |  7933 | `	/* Create a new array */` |
|        3 |  7934 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7935 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7936 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7937 | `		SXUNUSED(apArg);` |
|        - |  7938 | `		/* Return NULL */` |
|      ! 0 |  7939 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7940 | `		return SXRET_OK;` |
|        - |  7941 | `	}` |
|        - |  7942 | `	/* Superglobals first */` |
|        3 |  7943 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7944 | `	/* Then variable defined in the current frame */` |
|        3 |  7945 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7946 | `	/* Finally,return the created array */` |
|        3 |  7947 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7948 | `	return SXRET_OK;` |
|        2 |  7949 |  |
|        - |  7950 | `/*` |
|        - |  7951 | ` * bool gettype($var)` |
|        - |  7952 | ` *  Get the type of a variable` |
|        - |  7953 | ` * Parameters` |
|        - |  7954 | ` *   $var` |
|        - |  7955 | ` *    The variable being type checked.` |
|        - |  7956 | ` * Return` |
|        - |  7957 | ` *   String representation of the given variable type.` |
|        - |  7958 | ` */` |
|       32 |  7959 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7960 |  |
|       34 |  7961 | `	const char *zType = "Empty";` |
|       34 |  7962 | `	if( nArg > 0 ){` |
|       34 |  7963 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7964 | `	}` |
|        - |  7965 | `	/* Return the variable type */` |
|       34 |  7966 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7967 | `	return SXRET_OK;` |
|        2 |  7968 |  |
|        - |  7969 | `/*` |
|        - |  7970 | ` * string get_resource_type(resource $handle)` |
|        - |  7971 | ` *  This function gets the type of the given resource.` |
|        - |  7972 | ` * Parameters` |
|        - |  7973 | ` *  $handle` |
|        - |  7974 | ` *  The evaluated resource handle.` |
|        - |  7975 | ` * Return` |
|        - |  7976 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7977 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7978 | ` *  the return value will be the string Unknown.` |
|        - |  7979 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7980 | ` *  is not a resource.` |
|        - |  7981 | ` */` |
|        2 |  7982 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7983 |  |
|        3 |  7984 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7985 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7986 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7987 | `		return PH7_OK;` |
|        - |  7988 | `	}` |
|        3 |  7989 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7990 | `	return SXRET_OK;` |
|        2 |  7991 |  |
|        - |  7992 | `/*` |
|        - |  7993 | ` * void var_dump(expression,....)` |
|        - |  7994 | ` *   var_dump � Dumps information about a variable` |
|        - |  7995 | ` * Parameters` |
|        - |  7996 | ` *   One or more expression to dump.` |
|        - |  7997 | ` * Returns` |
|        - |  7998 | ` *  Nothing.` |
|        - |  7999 | ` */` |
|      218 |  8000 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8001 |  |
|        - |  8002 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  8003 | `	int i;` |
|      220 |  8004 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  8005 | `	/* Dump one or more expressions */` |
|      444 |  8006 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  8007 | `		ph7_value *pObj = apArg[i];` |
|        - |  8008 | `		/* Reset the working buffer */` |
|      226 |  8009 | `		SyBlobReset(&sDump);` |
|        - |  8010 | `		/* Dump the given expression */` |
|      226 |  8011 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  8012 | `		/* Output */` |
|      226 |  8013 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  8014 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  8015 | `		}` |
|      114 |  8016 | `	}` |
|        - |  8017 | `	/* Release the working buffer */` |
|      220 |  8018 | `	SyBlobRelease(&sDump);` |
|      220 |  8019 | `	return SXRET_OK;` |
|        2 |  8020 |  |
|        - |  8021 | `/*` |
|        - |  8022 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  8023 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  8024 | ` * Parameters` |
|        - |  8025 | ` *   expression: Expression to dump` |
|        - |  8026 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  8027 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  8028 | ` *            print_r() will return the information rather than print it.` |
|        - |  8029 | ` * Return` |
|        - |  8030 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  8031 | ` *  Otherwise, the return value is TRUE.` |
|        - |  8032 | ` */` |
|       16 |  8033 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8034 |  |
|       17 |  8035 | `	int ret_string = 0;` |
|        - |  8036 | `	SyBlob sDump;` |
|       17 |  8037 | `	if( nArg < 1 ){` |
|        - |  8038 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8039 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8040 | `		return SXRET_OK;` |
|        - |  8041 | `	}` |
|       17 |  8042 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  8043 | `	if ( nArg > 1 ){` |
|        - |  8044 | `		/* Where to redirect output */` |
|       11 |  8045 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  8046 | `	}` |
|        - |  8047 | `	/* Generate dump */` |
|       17 |  8048 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8049 | `	if( !ret_string ){` |
|        - |  8050 | `		/* Output dump */` |
|        7 |  8051 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8052 | `		/* Return true */` |
|        7 |  8053 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8054 | `	}else{` |
|        - |  8055 | `		/* Generated dump as return value */` |
|       11 |  8056 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8057 | `	}` |
|        - |  8058 | `	/* Release the working buffer */` |
|       17 |  8059 | `	SyBlobRelease(&sDump);` |
|       17 |  8060 | `	return SXRET_OK;` |
|        9 |  8061 |  |
|        - |  8062 | `/*` |
|        - |  8063 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  8064 | ` * Same job as print_r. (see coment above)` |
|        - |  8065 | ` */` |
|        2 |  8066 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8067 |  |
|        3 |  8068 | `	int ret_string = 0;` |
|        - |  8069 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  8070 | `	if( nArg < 1 ){` |
|        - |  8071 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8072 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8073 | `		return SXRET_OK;` |
|        - |  8074 | `	}` |
|        3 |  8075 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  8076 | `	if ( nArg > 1 ){` |
|        - |  8077 | `		/* Where to redirect output */` |
|        3 |  8078 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  8079 | `	}` |
|        - |  8080 | `	/* Generate dump */` |
|        3 |  8081 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  8082 | `	if( !ret_string ){` |
|        - |  8083 | `		/* Output dump */` |
|      ! 0 |  8084 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8085 | `		/* Return NULL */` |
|      ! 0 |  8086 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8087 | `	}else{` |
|        - |  8088 | `		/* Generated dump as return value */` |
|        3 |  8089 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8090 | `	}` |
|        - |  8091 | `	/* Release the working buffer */` |
|        3 |  8092 | `	SyBlobRelease(&sDump);` |
|        3 |  8093 | `	return SXRET_OK;` |
|        2 |  8094 |  |
|        - |  8095 | `/*` |
|        - |  8096 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  8097 | ` *  Set/get the various assert flags.` |
|        - |  8098 | ` * Parameter` |
|        - |  8099 | ` * $what` |
|        - |  8100 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  8101 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  8102 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  8103 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  8104 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  8105 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  8106 | ` * $value` |
|        - |  8107 | ` *   An optional new value for the option.` |
|        - |  8108 | ` * Return` |
|        - |  8109 | ` *  Old setting on success or FALSE on failure.` |
|        - |  8110 | ` */` |
|       30 |  8111 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8112 |  |
|       32 |  8113 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8114 | `	int iOption;` |
|        - |  8115 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  8116 | `	if( nArg < 1 ){` |
|        3 |  8117 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8118 | `			"ArgumentCountError",` |
|        - |  8119 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  8120 | `			);` |
|        - |  8121 | `	}` |
|        - |  8122 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  8123 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  8124 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  8125 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8126 | `			"TypeError",` |
|        - |  8127 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  8128 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  8129 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  8130 | `			);` |
|        - |  8131 | `	}` |
|       30 |  8132 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  8133 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  8134 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  8135 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  8136 | `	switch( iOption ){` |
|        6 |  8137 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  8138 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  8139 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  8140 | `		if( nArg > 1 ){` |
|        5 |  8141 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8142 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  8143 | `			}else{` |
|        3 |  8144 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  8145 | `			}` |
|        2 |  8146 | `		}` |
|       14 |  8147 | `		break;` |
|        1 |  8148 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8149 | `		/* Return old callback or null */` |
|        3 |  8150 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8151 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8152 | `		}else{` |
|        3 |  8153 | `			ph7_result_null(pCtx);` |
|        - |  8154 | `		}` |
|        3 |  8155 | `		if( nArg > 1 ){` |
|      ! 0 |  8156 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8157 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8158 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8159 | `			}else{` |
|      ! 0 |  8160 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8161 | `			}` |
|      ! 0 |  8162 | `		}` |
|        3 |  8163 | `		break;` |
|        5 |  8164 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8165 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8166 | `		if( nArg > 1 ){` |
|        5 |  8167 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8168 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8169 | `			}else{` |
|        3 |  8170 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8171 | `			}` |
|        2 |  8172 | `		}` |
|       11 |  8173 | `		break;` |
|      ! 0 |  8174 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8175 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8176 | `		break;` |
|        1 |  8177 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8178 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8179 | `		break;` |
|      ! 0 |  8180 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8181 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8182 | `		break;` |
|        1 |  8183 | `	default:` |
|        - |  8184 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8185 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8186 | `			"ValueError",` |
|        - |  8187 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8188 | `			);` |
|        - |  8189 | `	}` |
|       28 |  8190 | `	return PH7_OK;` |
|       17 |  8191 |  |
|        - |  8192 | `/*` |
|        - |  8193 | ` * bool assert(mixed $assertion)` |
|        - |  8194 | ` *  Checks if assertion is FALSE.` |
|        - |  8195 | ` * Parameter` |
|        - |  8196 | ` *  $assertion` |
|        - |  8197 | ` *    The assertion to test.` |
|        - |  8198 | ` * Return` |
|        - |  8199 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8200 | ` */` |
|       26 |  8201 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8202 |  |
|       28 |  8203 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8204 | `	int iFlags,iResult;` |
|        - |  8205 | `	const char *zDesc;` |
|        - |  8206 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8207 | `	if( nArg < 1 ){` |
|        3 |  8208 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8209 | `			"ArgumentCountError",` |
|        - |  8210 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8211 | `			);` |
|        - |  8212 | `	}` |
|       26 |  8213 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8214 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8215 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8216 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8217 | `		return PH7_OK;` |
|        - |  8218 | `	}` |
|        - |  8219 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8220 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8221 | `	if( !iResult ){` |
|        - |  8222 | `		/* Assertion failed */` |
|        - |  8223 | `		/* Extract optional description */` |
|       13 |  8224 | `		zDesc = 0;` |
|       13 |  8225 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8226 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8227 | `		}` |
|       13 |  8228 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8229 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8230 | `			ph7_value sFile,sLine;` |
|        - |  8231 | `			ph7_value *apCbArg[3];` |
|        - |  8232 | `			SyString *pFile;` |
|        - |  8233 | `			/* Extract the processed script */` |
|      ! 0 |  8234 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8235 | `			if( pFile == 0 ){` |
|      ! 0 |  8236 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8237 | `			}` |
|        - |  8238 | `			/* Invoke the callback */` |
|      ! 0 |  8239 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8240 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8241 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8242 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8243 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8244 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8245 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8246 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8247 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8248 | `		}` |
|       13 |  8249 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8250 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8251 | `			return PH7_ABORT;` |
|        - |  8252 | `		}` |
|        - |  8253 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8254 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8255 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8256 | `				"AssertionError",` |
|        - |  8257 | `				"%s",` |
|        1 |  8258 | `				zDesc` |
|        - |  8259 | `				);` |
|      ! 0 |  8260 | `		}else{` |
|       11 |  8261 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8262 | `				"AssertionError",` |
|        - |  8263 | `				"assert(false)"` |
|        - |  8264 | `				);` |
|        - |  8265 | `		}` |
|        - |  8266 | `	}` |
|        - |  8267 | `	/* Assertion passed */` |
|       14 |  8268 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8269 | `	return PH7_OK;` |
|       15 |  8270 |  |
|        - |  8271 | `/*` |
|        - |  8272 | ` * Section:` |
|        - |  8273 | ` *  Error reporting functions.` |
|        - |  8274 | ` * Status:` |
|        - |  8275 | ` *    Stable.` |
|        - |  8276 | ` */` |
|        - |  8277 | `/*` |
|        - |  8278 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8279 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8280 | ` * Parameters` |
|        - |  8281 | ` *  $error_msg` |
|        - |  8282 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8283 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8284 | ` * $error_type` |
|        - |  8285 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8286 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8287 | ` * Return` |
|        - |  8288 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8289 | ` */` |
|       12 |  8290 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8291 |  |
|       14 |  8292 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8293 | `	int rc = PH7_OK;` |
|       14 |  8294 | `	if( nArg > 0 ){` |
|        - |  8295 | `		const char *zErr;` |
|        - |  8296 | `		int nLen;` |
|        - |  8297 | `		/* Extract the error message */` |
|       12 |  8298 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8299 | `		if( nArg > 1 ){` |
|        - |  8300 | `			/* Extract the error type */` |
|       12 |  8301 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8302 | `			switch( nErr ){` |
|        1 |  8303 | `			case 1:   /* E_ERROR */` |
|        - |  8304 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8305 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8306 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8307 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8308 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8309 | `				break;` |
|        1 |  8310 | `			case 2:   /* E_WARNING */` |
|        - |  8311 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8312 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8313 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8314 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8315 | `				break;` |
|        3 |  8316 | `			default:` |
|        8 |  8317 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8318 | `				break;` |
|        - |  8319 | `			}` |
|        5 |  8320 | `		}` |
|        - |  8321 | `		/* Report error */` |
|       12 |  8322 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8323 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8324 | `			return rc;` |
|        - |  8325 | `		}` |
|        - |  8326 | `		/* Return true */` |
|       12 |  8327 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8328 | `	}else{` |
|        - |  8329 | `		/* Missing arguments,return FALSE */` |
|        3 |  8330 | `		ph7_result_bool(pCtx,0);` |
|        - |  8331 | `	}` |
|       14 |  8332 | `	return rc;` |
|        8 |  8333 |  |
|        - |  8334 | `/*` |
|        - |  8335 | ` * int error_reporting([int $level])` |
|        - |  8336 | ` *  Sets which PHP errors are reported.` |
|        - |  8337 | ` * Parameters` |
|        - |  8338 | ` *  $level` |
|        - |  8339 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8340 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8341 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8342 | ` *   levels will not always behave as expected.` |
|        - |  8343 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8344 | ` *   in the predefined constants.` |
|        - |  8345 | ` * Return` |
|        - |  8346 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8347 | ` *   parameter is given.` |
|        - |  8348 | ` */` |
|       40 |  8349 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8350 |  |
|       42 |  8351 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8352 | `	int nOld;` |
|        - |  8353 | `	/* Extract the old reporting level */` |
|       42 |  8354 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8355 | `	if( nArg > 0 ){` |
|        - |  8356 | `		int nNew;` |
|        - |  8357 | `		/* Extract the desired error reporting level */` |
|       34 |  8358 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8359 | `		if( !nNew ){` |
|        - |  8360 | `			/* Do not report errors at all */` |
|        5 |  8361 | `			pVm->bErrReport = 0;` |
|        3 |  8362 | `		}else{` |
|        - |  8363 | `			/* Report all errors */` |
|       30 |  8364 | `			pVm->bErrReport = 1;` |
|        - |  8365 | `		}` |
|       16 |  8366 | `	}` |
|        - |  8367 | `	/* Return the old level */` |
|       42 |  8368 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8369 | `	return PH7_OK;` |
|        2 |  8370 |  |
|        - |  8371 | `/*` |
|        - |  8372 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8373 | ` *  Send an error message somewhere.` |
|        - |  8374 | ` * Parameter` |
|        - |  8375 | ` *  $message` |
|        - |  8376 | ` *   The error message that should be logged.` |
|        - |  8377 | ` *  $message_type` |
|        - |  8378 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8379 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8380 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8381 | ` *       This is the default option.` |
|        - |  8382 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8383 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8384 | ` *    2  No longer an option.` |
|        - |  8385 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8386 | ` *       to the end of the message string.` |
|        - |  8387 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8388 | ` *  $destination` |
|        - |  8389 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8390 | ` *  $extra_headers` |
|        - |  8391 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8392 | ` * Return` |
|        - |  8393 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8394 | ` * NOTE:` |
|        - |  8395 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8396 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8397 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8398 | ` *  Otherwise this function is no-op.` |
|        - |  8399 | ` */` |
|        4 |  8400 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8401 |  |
|        - |  8402 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8403 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8404 | `	int iType = 0;` |
|        5 |  8405 | `	if( nArg < 1 ){` |
|        - |  8406 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8407 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8408 | `		return PH7_OK;` |
|        - |  8409 | `	}` |
|        5 |  8410 | `	if( pVm->xErrLog  ){` |
|        - |  8411 | `		/* Invoke the user callback */` |
|      ! 0 |  8412 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8413 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8414 | `		if( nArg > 1 ){` |
|      ! 0 |  8415 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8416 | `			if( nArg > 2 ){` |
|      ! 0 |  8417 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8418 | `				if( nArg > 3 ){` |
|      ! 0 |  8419 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8420 | `				}` |
|      ! 0 |  8421 | `			}` |
|      ! 0 |  8422 | `		}` |
|      ! 0 |  8423 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8424 | `	}` |
|        - |  8425 | `	/* Retun TRUE */` |
|        5 |  8426 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8427 | `	return PH7_OK;` |
|        3 |  8428 |  |
|        - |  8429 | `/*` |
|        - |  8430 | ` * bool restore_exception_handler(void)` |
|        - |  8431 | ` *  Restores the previously defined exception handler function.` |
|        - |  8432 | ` * Parameter` |
|        - |  8433 | ` *  None` |
|        - |  8434 | ` * Return` |
|        - |  8435 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8436 | ` */` |
|        4 |  8437 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8438 |  |
|        5 |  8439 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8440 | `	ph7_value *pOld,*pNew;` |
|        - |  8441 | `	/* Point to the old and the new handler */` |
|        5 |  8442 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8443 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8444 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8445 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8446 | `		SXUNUSED(apArg);` |
|        - |  8447 | `		/* No installed handler,return FALSE */` |
|        5 |  8448 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8449 | `		return PH7_OK;` |
|        - |  8450 | `	}` |
|        - |  8451 | `	/* Copy the old handler */` |
|      ! 0 |  8452 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8453 | `	PH7_MemObjRelease(pOld);` |
|        - |  8454 | `	/* Return TRUE */` |
|      ! 0 |  8455 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8456 | `	return PH7_OK;` |
|        3 |  8457 |  |
|        - |  8458 | `/*` |
|        - |  8459 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8460 | ` *  Sets a user-defined exception handler function.` |
|        - |  8461 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8462 | ` * NOTE` |
|        - |  8463 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8464 | ` *  the satndard PHP engine.` |
|        - |  8465 | ` * Parameters` |
|        - |  8466 | ` *  $exception_handler` |
|        - |  8467 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8468 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8469 | ` *   that was thrown.` |
|        - |  8470 | ` *  Note:` |
|        - |  8471 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8472 | ` * Return` |
|        - |  8473 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8474 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8475 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8476 | ` */` |
|        4 |  8477 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8478 |  |
|        6 |  8479 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8480 | `	ph7_value *pOld,*pNew;` |
|        - |  8481 | `	/* Point to the old and the new handler */` |
|        6 |  8482 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8483 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8484 | `	/* Return the old handler */` |
|        6 |  8485 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8486 | `	if( nArg > 0 ){` |
|        6 |  8487 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8488 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8489 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8490 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8491 | `		}else{` |
|        6 |  8492 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8493 | `			/* Install the new handler */` |
|        6 |  8494 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8495 | `		}` |
|        2 |  8496 | `	}` |
|        6 |  8497 | `	return PH7_OK;` |
|        2 |  8498 |  |
|        - |  8499 | `/*` |
|        - |  8500 | ` * bool restore_error_handler(void)` |
|        - |  8501 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8502 | ` * Parameters:` |
|        - |  8503 | ` *  None.` |
|        - |  8504 | ` * Return` |
|        - |  8505 | ` *  Always TRUE.` |
|        - |  8506 | ` */` |
|        4 |  8507 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8508 |  |
|        5 |  8509 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8510 | `	ph7_value *pOld,*pNew;` |
|        - |  8511 | `	/* Point to the old and the new handler */` |
|        5 |  8512 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8513 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8514 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8515 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8516 | `		SXUNUSED(apArg);` |
|        - |  8517 | `		/* No installed callback,return FALSE */` |
|        5 |  8518 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8519 | `		return PH7_OK;` |
|        - |  8520 | `	}` |
|        - |  8521 | `	/* Copy the old callback */` |
|      ! 0 |  8522 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8523 | `	PH7_MemObjRelease(pOld);` |
|        - |  8524 | `	/* Return TRUE */` |
|      ! 0 |  8525 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8526 | `	return PH7_OK;` |
|        3 |  8527 |  |
|        - |  8528 | `/*` |
|        - |  8529 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8530 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8531 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8532 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8533 | ` *  Sets a user-defined error handler function.` |
|        - |  8534 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8535 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8536 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8537 | ` *  conditions (using trigger_error()).` |
|        - |  8538 | ` * Parameters` |
|        - |  8539 | ` *  $error_handler` |
|        - |  8540 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8541 | ` *   describing the error.` |
|        - |  8542 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8543 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8544 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8545 | ` *   The function can be shown as:` |
|        - |  8546 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8547 | ` *     errno` |
|        - |  8548 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8549 | ` *   errstr` |
|        - |  8550 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8551 | ` *   errfile` |
|        - |  8552 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8553 | ` *     was raised in, as a string.` |
|        - |  8554 | ` *  Note:` |
|        - |  8555 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8556 | ` * Return` |
|        - |  8557 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8558 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8559 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8560 | ` */` |
|     8722 |  8561 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8562 |  |
|     8724 |  8563 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8564 | `	ph7_value *pOld,*pNew;` |
|        - |  8565 | `	/* Point to the old and the new handler */` |
|     8724 |  8566 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8567 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8568 | `	/* Return the old handler */` |
|     8724 |  8569 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8570 | `	if( nArg > 0 ){` |
|     8724 |  8571 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8572 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8573 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8574 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8575 | `		}else{` |
|     4364 |  8576 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8577 | `			/* Install the new handler */` |
|     4364 |  8578 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8579 | `		}` |
|     4361 |  8580 | `	}` |
|     8724 |  8581 | `	return PH7_OK;` |
|        2 |  8582 |  |
|        - |  8583 | `/*` |
|        - |  8584 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8585 | ` *  Generates a backtrace.` |
|        - |  8586 | ` * Paramaeter` |
|        - |  8587 | ` *  $options` |
|        - |  8588 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8589 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8590 | ` *   all the function/method arguments, to save memory.` |
|        - |  8591 | ` * $limit` |
|        - |  8592 | ` *   (Not Used)` |
|        - |  8593 | ` * Return` |
|        - |  8594 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8595 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8596 | ` *          Name        Type      Description` |
|        - |  8597 | ` *          ------      ------     -----------` |
|        - |  8598 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8599 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8600 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8601 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8602 | ` *          object      object    The current object.` |
|        - |  8603 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8604 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8605 | ` */` |
|      502 |  8606 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8607 |  |
|      504 |  8608 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8609 | `	ph7_value *pArray;` |
|        - |  8610 | `	ph7_class *pClass;` |
|        - |  8611 | `	ph7_value *pValue;` |
|        - |  8612 | `	SyString *pFile;` |
|        - |  8613 | `	/* Create a new array */` |
|      504 |  8614 | `	pArray = ph7_context_new_array(pCtx);` |
|      504 |  8615 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      504 |  8616 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8617 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8618 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8619 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8620 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8621 | `		SXUNUSED(apArg);` |
|      ! 0 |  8622 | `		return PH7_OK;` |
|        - |  8623 | `	}` |
|        - |  8624 | `	/* Dump running function name and it's arguments  */` |
|      504 |  8625 | `	if( pVm->pFrame->pParent ){` |
|      504 |  8626 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8627 | `		ph7_vm_func *pFunc;` |
|        - |  8628 | `		ph7_value *pArg;` |
|      504 |  8629 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8630 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8631 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8632 | `		}` |
|      504 |  8633 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      504 |  8634 | `		if( pFrame->pParent && pFunc ){` |
|      504 |  8635 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      504 |  8636 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      504 |  8637 | `			ph7_value_reset_string_cursor(pValue);` |
|      251 |  8638 | `		}` |
|        - |  8639 | `		/* Function arguments */` |
|      504 |  8640 | `		pArg = ph7_context_new_array(pCtx);` |
|      504 |  8641 | `		if( pArg  ){` |
|        - |  8642 | `			ph7_value *pObj;` |
|        - |  8643 | `			VmSlot *aSlot;` |
|        - |  8644 | `			sxu32 n;` |
|        - |  8645 | `			/* Start filling the array with the given arguments */` |
|      504 |  8646 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2002 |  8647 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1500 |  8648 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1500 |  8649 | `				if( pObj ){` |
|     1500 |  8650 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      749 |  8651 | `				}` |
|      751 |  8652 | `			}` |
|        - |  8653 | `			/* Save the array */` |
|      504 |  8654 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      251 |  8655 | `		}` |
|      251 |  8656 | `	}` |
|      504 |  8657 | `	ph7_value_int(pValue,1);` |
|        - |  8658 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8659 | `	 * line numbers at run-time. )` |
|        - |  8660 | `	 */` |
|      504 |  8661 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8662 | `	/* Current processed script */` |
|      504 |  8663 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      504 |  8664 | `	if( pFile ){` |
|      504 |  8665 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      504 |  8666 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      504 |  8667 | `		ph7_value_reset_string_cursor(pValue);` |
|      251 |  8668 | `	}` |
|        - |  8669 | `	/* Top class */` |
|      504 |  8670 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      504 |  8671 | `	if( pClass ){` |
|      500 |  8672 | `		ph7_value_reset_string_cursor(pValue);` |
|      500 |  8673 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      500 |  8674 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      249 |  8675 | `	}` |
|        - |  8676 | `	/* Return the freshly created array */` |
|      504 |  8677 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8678 | `	/*` |
|        - |  8679 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8680 | `	 * as soon we return from this function.` |
|        - |  8681 | `	 */` |
|      504 |  8682 | `	return PH7_OK;` |
|      253 |  8683 |  |
|        - |  8684 | `/*` |
|        - |  8685 | ` * Generate a small backtrace.` |
|        - |  8686 | ` * Store the generated dump in the given BLOB` |
|        - |  8687 | ` */` |
|        4 |  8688 | `static int VmMiniBacktrace(` |
|        - |  8689 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8690 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8691 | `	)` |
|        1 |  8692 |  |
|        5 |  8693 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8694 | `	ph7_vm_func *pFunc;` |
|        - |  8695 | `	ph7_class *pClass;` |
|        - |  8696 | `	SyString *pFile;` |
|        - |  8697 | `	/* Called function */` |
|        5 |  8698 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8699 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8700 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8701 | `	}` |
|        5 |  8702 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8703 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8704 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8705 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8706 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8707 | `	}else{` |
|      ! 0 |  8708 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8709 | `	}` |
|        5 |  8710 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8711 | `	/* Current processed script */` |
|        5 |  8712 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8713 | `	if( pFile ){` |
|        5 |  8714 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8715 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8716 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8717 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8718 | `	}` |
|        - |  8719 | `	/* Top class */` |
|        5 |  8720 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8721 | `	if( pClass ){` |
|      ! 0 |  8722 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8723 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8724 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8725 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8726 | `	}` |
|        5 |  8727 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8728 | `	/* All done */` |
|        5 |  8729 | `	return SXRET_OK;` |
|        1 |  8730 |  |
|        - |  8731 | `/*` |
|        - |  8732 | ` * void debug_print_backtrace()` |
|        - |  8733 | ` *  Prints a backtrace` |
|        - |  8734 | ` * Parameters` |
|        - |  8735 | ` * None` |
|        - |  8736 | ` * Return` |
|        - |  8737 | ` * NULL` |
|        - |  8738 | ` */` |
|        2 |  8739 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8740 |  |
|        3 |  8741 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8742 | `	SyBlob sDump;` |
|        3 |  8743 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8744 | `	/* Generate the backtrace */` |
|        3 |  8745 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8746 | `	/* Output backtrace */` |
|        3 |  8747 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8748 | `	/* All done,cleanup */` |
|        3 |  8749 | `	SyBlobRelease(&sDump);` |
|        1 |  8750 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8751 | `	SXUNUSED(apArg);` |
|        3 |  8752 | `	return PH7_OK;` |
|        1 |  8753 |  |
|        - |  8754 | `/*` |
|        - |  8755 | ` * string debug_string_backtrace()` |
|        - |  8756 | ` *  Generate a backtrace` |
|        - |  8757 | ` * Parameters` |
|        - |  8758 | ` * None` |
|        - |  8759 | ` * Return` |
|        - |  8760 | ` *  A mini backtrace().` |
|        - |  8761 | ` * Note that this is a symisc extension.` |
|        - |  8762 | ` */` |
|        2 |  8763 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8764 |  |
|        3 |  8765 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8766 | `	SyBlob sDump;` |
|        3 |  8767 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8768 | `	/* Generate the backtrace */` |
|        3 |  8769 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8770 | `	/* Return the backtrace */` |
|        3 |  8771 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8772 | `	/* All done,cleanup */` |
|        3 |  8773 | `	SyBlobRelease(&sDump);` |
|        1 |  8774 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8775 | `	SXUNUSED(apArg);` |
|        3 |  8776 | `	return PH7_OK;` |
|        1 |  8777 |  |
|        - |  8778 | `/*` |
|        - |  8779 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8780 | ` * exception is triggered.` |
|        - |  8781 | ` */` |
|      472 |  8782 | `static sxi32 VmUncaughtException(` |
|        - |  8783 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8784 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8785 | `	)` |
|        1 |  8786 |  |
|        - |  8787 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8788 | `	int nArg = 1;` |
|        - |  8789 | `	sxi32 rc;` |
|      473 |  8790 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8791 | `		/* Nesting limit reached */` |
|      ! 0 |  8792 | `		return SXRET_OK;` |
|        - |  8793 | `	}` |
|        - |  8794 | `	/* Call any exception handler if available */` |
|      473 |  8795 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8796 | `	if( pThis ){` |
|        - |  8797 | `		/* Load the exception instance */` |
|      473 |  8798 | `		sArg.x.pOther = pThis;` |
|      473 |  8799 | `		pThis->iRef++;` |
|      473 |  8800 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8801 | `	}else{` |
|      ! 0 |  8802 | `		nArg = 0;` |
|        - |  8803 | `	}` |
|      473 |  8804 | `	apArg[0] = &sArg;` |
|        - |  8805 | `	/* Call the exception handler if available */` |
|      473 |  8806 | `	pVm->nExceptDepth++;` |
|      473 |  8807 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8808 | `	pVm->nExceptDepth--;` |
|      473 |  8809 | `	if( rc != SXRET_OK ){` |
|        - |  8810 | `		SyBlob sMsgBuf;` |
|      471 |  8811 | `		const char *zClass = "Exception";` |
|      471 |  8812 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8813 | `		const char *zMsg;` |
|        - |  8814 | `		sxu32 nMsg;` |
|        - |  8815 | `		const char *zFuncName;` |
|        - |  8816 | `		int nFuncLen;` |
|      471 |  8817 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8818 | `		if( pThis ){` |
|        - |  8819 | `			ph7_class_method *pGetMessage;` |
|        - |  8820 | `			ph7_value sMsg;` |
|        - |  8821 | `			const char *zTmp;` |
|        - |  8822 | `			int nTmp;` |
|      471 |  8823 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8824 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8825 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8826 | `			if( pGetMessage ){` |
|      471 |  8827 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8828 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8829 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8830 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8831 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8832 | `					}` |
|      235 |  8833 | `				}` |
|      471 |  8834 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8835 | `			}` |
|      235 |  8836 | `		}` |
|      471 |  8837 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8838 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8839 | `		}` |
|      471 |  8840 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8841 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8842 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8843 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8844 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8845 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8846 | `		rc = SXERR_ABORT;` |
|      235 |  8847 | `	}` |
|      473 |  8848 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8849 | `	return rc;` |
|      237 |  8850 |  |
|        - |  8851 | `/*` |
|        - |  8852 | ` * Throw an user exception.` |
|        - |  8853 | ` */` |
|      506 |  8854 | `static sxi32 VmThrowException(` |
|        - |  8855 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8856 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8857 | `	)` |
|        2 |  8858 |  |
|        - |  8859 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8860 | `	ph7_exception **apException;` |
|        - |  8861 | `	ph7_exception *pException;` |
|        - |  8862 | `	/* Point to the stack of loaded exceptions */` |
|      508 |  8863 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      508 |  8864 | `	pException = 0;` |
|      508 |  8865 | `	pCatch = 0;` |
|      508 |  8866 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8867 | `		ph7_exception_block *aCatch;` |
|        - |  8868 | `		ph7_class *pClass;` |
|        - |  8869 | `		sxu32 j;` |
|        - |  8870 | `		/* Locate the appropriate block to execute */` |
|       32 |  8871 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       32 |  8872 | `		(void)SySetPop(&pVm->aException);` |
|       32 |  8873 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       32 |  8874 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       30 |  8875 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8876 | `			/* Extract the target class */` |
|       30 |  8877 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       30 |  8878 | `			if( pClass == 0 ){` |
|        - |  8879 | `				/* No such class */` |
|      ! 0 |  8880 | `				continue;` |
|        - |  8881 | `			}` |
|       30 |  8882 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8883 | `				/* Catch block found,break immeditaley */` |
|       30 |  8884 | `				pCatch = &aCatch[j];` |
|       30 |  8885 | `				break;` |
|        - |  8886 | `			}` |
|      ! 0 |  8887 | `		}` |
|       15 |  8888 | `	}` |
|        - |  8889 | `	/* Execute the cached block if available */` |
|      508 |  8890 | `	if( pCatch == 0 ){` |
|        - |  8891 | `		sxi32 rc;` |
|        - |  8892 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 |  8893 | `		if( pException && pException->iHasFinally ){` |
|        3 |  8894 | `			pException->iFinallyDone = 1;` |
|        3 |  8895 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 |  8896 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8897 | `				return SXERR_ABORT;` |
|        - |  8898 | `			}` |
|        1 |  8899 | `		}` |
|        - |  8900 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 |  8901 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8902 | `			/* Re-throw to the outer handler */` |
|        3 |  8903 | `			return VmThrowException(&(*pVm),pThis);` |
|        - |  8904 | `		}` |
|        - |  8905 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - |  8906 | `		 * (catch body re-throw with finally pending), defer the` |
|        - |  8907 | `		 * exception instead of reporting it uncaught.` |
|        - |  8908 | `		 */` |
|      478 |  8909 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - |  8910 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - |  8911 | `			 * by looking for a catch frame on the stack.` |
|        - |  8912 | `			 */` |
|      478 |  8913 | `			VmFrame *pF = pVm->pFrame;` |
|      478 |  8914 | `			int inCatch = 0;` |
|      956 |  8915 | `			while( pF ){` |
|      484 |  8916 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 |  8917 | `					inCatch = 1;` |
|        6 |  8918 | `					break;` |
|        - |  8919 | `				}` |
|      479 |  8920 | `				pF = pF->pParent;` |
|        1 |  8921 | `			}` |
|      478 |  8922 | `			if( inCatch ){` |
|        - |  8923 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 |  8924 | `				pThis->iRef++;` |
|        6 |  8925 | `				pVm->pPendingException = pThis;` |
|        6 |  8926 | `				return SXRET_OK;` |
|        - |  8927 | `			}` |
|      236 |  8928 | `		}` |
|        - |  8929 | `		/* Truly uncaught */` |
|      473 |  8930 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8931 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8932 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8933 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  8934 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8935 | `			}` |
|      ! 0 |  8936 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 |  8937 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8938 | `			}` |
|      ! 0 |  8939 | `		}` |
|      473 |  8940 | `		return rc;` |
|      ! 0 |  8941 | `	}else{` |
|       30 |  8942 | `		VmFrame *pFrame = pVm->pFrame;` |
|       30 |  8943 | `		ph7_exception **apSaved = 0;` |
|        - |  8944 | `		sxu32 nSavedCount;` |
|        - |  8945 | `		sxi32 rc;` |
|       58 |  8946 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|       30 |  8947 | `			pFrame = pFrame->pParent;` |
|        2 |  8948 | `		}` |
|       30 |  8949 | `		if( pException->pFrame == pFrame ){` |
|       22 |  8950 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       10 |  8951 | `		}` |
|        - |  8952 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - |  8953 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - |  8954 | `		 * our finally block. We save the stack contents and restore after.` |
|        - |  8955 | `		 */` |
|       30 |  8956 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       30 |  8957 | `		if( nSavedCount > 0 ){` |
|       11 |  8958 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 |  8959 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8960 | `			if( apSaved ){` |
|       11 |  8961 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 |  8962 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8963 | `				SySetReset(&pVm->aException);` |
|        3 |  8964 | `			}` |
|        3 |  8965 | `		}` |
|        - |  8966 | `		/* Create a private frame first */` |
|       30 |  8967 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       30 |  8968 | `		if( rc == SXRET_OK ){` |
|       30 |  8969 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       30 |  8970 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       30 |  8971 | `			if( pObj ){` |
|       30 |  8972 | `				pThis->iRef++;` |
|       30 |  8973 | `				pObj->x.pOther = pThis;` |
|       30 |  8974 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       14 |  8975 | `			}` |
|        - |  8976 | `			/* Execute the catch block */` |
|       30 |  8977 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8978 | `			/* Leave the frame */` |
|       30 |  8979 | `			VmLeaveFrame(&(*pVm));` |
|       14 |  8980 | `		}` |
|        - |  8981 | `		/* Restore the outer exception handlers */` |
|       30 |  8982 | `		if( apSaved ){` |
|        - |  8983 | `			sxu32 k;` |
|        - |  8984 | `			/* Any new entries pushed during catch execution (from nested` |
|        - |  8985 | `			 * try blocks inside the catch body) are already consumed.` |
|        - |  8986 | `			 * Restore the original outer entries.` |
|        - |  8987 | `			 */` |
|        8 |  8988 | `			SySetReset(&pVm->aException);` |
|       14 |  8989 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 |  8990 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 |  8991 | `			}` |
|        8 |  8992 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 |  8993 | `		}` |
|        - |  8994 | `		/* Execute the finally block after catch */` |
|       30 |  8995 | `		if( pException->iHasFinally ){` |
|        9 |  8996 | `			pException->iFinallyDone = 1;` |
|        - |  8997 | `			{` |
|        9 |  8998 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        9 |  8999 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 |  9000 | `					return SXERR_ABORT;` |
|        - |  9001 | `				}` |
|        - |  9002 | `			}` |
|        4 |  9003 | `		}` |
|       30 |  9004 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9005 | `			return SXERR_ABORT;` |
|        - |  9006 | `		}` |
|        - |  9007 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - |  9008 | `		 * pPendingException (because outer handlers were hidden).` |
|        - |  9009 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - |  9010 | `		 */` |
|       30 |  9011 | `		if( pVm->pPendingException ){` |
|        6 |  9012 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 |  9013 | `			pVm->pPendingException = 0;` |
|        6 |  9014 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - |  9015 | `		}` |
|        - |  9016 | `	}` |
|        - |  9017 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9018 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9019 | `	 */` |
|       26 |  9020 | `	return SXRET_OK;` |
|      255 |  9021 |  |
|        - |  9022 | `/*` |
|        - |  9023 | ` * Section:` |
|        - |  9024 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9025 | ` * Status:` |
|        - |  9026 | ` *    Stable.` |
|        - |  9027 | ` */` |
|        - |  9028 | `/*` |
|        - |  9029 | ` * string ph7version(void)` |
|        - |  9030 | ` *  Returns the running version of the PH7 version.` |
|        - |  9031 | ` * Parameters` |
|        - |  9032 | ` *  None` |
|        - |  9033 | ` * Return` |
|        - |  9034 | ` * Current PH7 version.` |
|        - |  9035 | ` */` |
|        2 |  9036 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9037 |  |
|        1 |  9038 | `	SXUNUSED(nArg);` |
|        1 |  9039 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9040 | `	/* Current engine version */` |
|        3 |  9041 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9042 | `	return PH7_OK;` |
|        1 |  9043 |  |
|        - |  9044 | `/*` |
|        - |  9045 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9046 | ` */` |
|        - |  9047 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9048 | ` "<html><head>"\` |
|        - |  9049 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9050 | ` "<style type=\"text/css\">"\` |
|        - |  9051 | ` "div {"\` |
|        - |  9052 | `     "border: 1px solid #cccccc;"\` |
|        - |  9053 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9054 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9055 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9056 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9057 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9058 | `     "-o-border-radius: 10px;"\` |
|        - |  9059 | `     "border-radius: 10px;"\` |
|        - |  9060 | `     "padding-left: 2em;"\` |
|        - |  9061 | `     "background-color: white;"\` |
|        - |  9062 | `     "margin-left: auto;"\` |
|        - |  9063 | `     "font-family: verdana;"\` |
|        - |  9064 | `     "padding-right: 2em;"\` |
|        - |  9065 | `     "margin-right: auto;"\` |
|        - |  9066 | `     "}"\` |
|        - |  9067 | `     "body {"\` |
|        - |  9068 | `     "padding: 0.2em;"\` |
|        - |  9069 | `     "font-style: normal;"\` |
|        - |  9070 | `     "font-size: medium;"\` |
|        - |  9071 | `     "background-color: #f2f2f2;"\` |
|        - |  9072 | `     "}"\` |
|        - |  9073 | `     "hr {"\` |
|        - |  9074 | `     "border-style: solid none none;"\` |
|        - |  9075 | `     "border-width: 1px medium medium;"\` |
|        - |  9076 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9077 | `     "height: 1px;"\` |
|        - |  9078 | `     "}"\` |
|        - |  9079 | `     "a {"\` |
|        - |  9080 | `     "color: #3366cc;"\` |
|        - |  9081 | `     "text-decoration: none;"\` |
|        - |  9082 | `     "}"\` |
|        - |  9083 | `     "a:hover {"\` |
|        - |  9084 | `     "color: #999999;"\` |
|        - |  9085 | `     "}"\` |
|        - |  9086 | `     "a:active {"\` |
|        - |  9087 | `     "color: #663399;"\` |
|        - |  9088 | `     "}"\` |
|        - |  9089 | `     "h1 {"\` |
|        - |  9090 | `     "margin: 0;"\` |
|        - |  9091 | `     "padding: 0;"\` |
|        - |  9092 | `     "font-family: Verdana;"\` |
|        - |  9093 | `     "font-weight: bold;"\` |
|        - |  9094 | `     "font-style: normal;"\` |
|        - |  9095 | `     "font-size: medium;"\` |
|        - |  9096 | `     "text-transform: capitalize;"\` |
|        - |  9097 | `     "color: #0a328c;"\` |
|        - |  9098 | `     "}"\` |
|        - |  9099 | `     "p {"\` |
|        - |  9100 | `     "margin: 0 auto;"\` |
|        - |  9101 | `     "font-size: medium;"\` |
|        - |  9102 | `     "font-style: normal;"\` |
|        - |  9103 | `     "font-family: verdana;"\` |
|        - |  9104 | `     "}"\` |
|        - |  9105 | `"</style></head><body>"\` |
|        - |  9106 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  9107 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  9108 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  9109 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  9110 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  9111 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  9112 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  9113 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  9114 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  9115 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  9116 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  9117 |  |
|        - |  9118 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9119 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  9120 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  9121 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9122 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9123 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9124 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9125 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9126 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9127 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9128 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9129 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9130 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9131 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9132 |  |
|        - |  9133 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9134 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9135 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9136 | `"&nbsp;*<br>"\` |
|        - |  9137 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9138 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9139 | `"&nbsp;* are met:<br>"\` |
|        - |  9140 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9141 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9142 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9143 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9144 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9145 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9146 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9147 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9148 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9149 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9150 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9151 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9152 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9153 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9154 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9155 | `"&nbsp;*<br>"\` |
|        - |  9156 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9157 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9158 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9159 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9160 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9161 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9162 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9163 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9164 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9165 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9166 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9167 | `"&nbsp;*/<br>"\` |
|        - |  9168 | `"</span></small></small></p>"\` |
|        - |  9169 | `"</div></body></html>"` |
|        - |  9170 | `/*` |
|        - |  9171 | ` * bool ph7credits(void)` |
|        - |  9172 | ` * bool ph7info(void)` |
|        - |  9173 | ` * bool ph7copyright(void)` |
|        - |  9174 | ` *  Prints out the credits for PH7 engine` |
|        - |  9175 | ` * Parameters` |
|        - |  9176 | ` *  None` |
|        - |  9177 | ` * Return` |
|        - |  9178 | ` *  Always TRUE` |
|        - |  9179 | ` */` |
|        2 |  9180 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9181 |  |
|        3 |  9182 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9183 | `	/* Expand the HTML page above*/` |
|        3 |  9184 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9185 | `	ph7_context_output_format(` |
|        1 |  9186 | `		pCtx,` |
|        - |  9187 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9188 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9189 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9190 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9191 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9192 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9193 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9194 | `#ifdef __WINNT__` |
|        - |  9195 | `		"Windows NT"` |
|        - |  9196 | `#elif defined(__UNIXES__)` |
|        - |  9197 | `		"UNIX-Like"` |
|        - |  9198 | `#else` |
|        - |  9199 | `		"Other OS"` |
|        - |  9200 | `#endif` |
|        - |  9201 | `		);` |
|        3 |  9202 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9203 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9204 | `	SXUNUSED(apArg);` |
|        - |  9205 | `	/* Return TRUE */` |
|        - |  9206 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9207 | `	return PH7_OK;` |
|        1 |  9208 |  |
|        - |  9209 | `/*` |
|        - |  9210 | ` * Section:` |
|        - |  9211 | ` *    URL related routines.` |
|        - |  9212 | ` * Status:` |
|        - |  9213 | ` *    Stable.` |
|        - |  9214 | ` */` |
|        - |  9215 | `/*` |
|        - |  9216 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9217 | ` *  Parse a URL and return its fields.` |
|        - |  9218 | ` * Parameters` |
|        - |  9219 | ` *  $url` |
|        - |  9220 | ` *   The URL to parse.` |
|        - |  9221 | ` * $component` |
|        - |  9222 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9223 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9224 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9225 | ` *  in which case the return value will be an integer).` |
|        - |  9226 | ` * Return` |
|        - |  9227 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9228 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9229 | ` *  this array are:` |
|        - |  9230 | ` *   scheme - e.g. http` |
|        - |  9231 | ` *   host` |
|        - |  9232 | ` *   port` |
|        - |  9233 | ` *   user` |
|        - |  9234 | ` *   pass` |
|        - |  9235 | ` *   path` |
|        - |  9236 | ` *   query - after the question mark ?` |
|        - |  9237 | ` *   fragment - after the hashmark #` |
|        - |  9238 | ` * Note:` |
|        - |  9239 | ` *  FALSE is returned on failure.` |
|        - |  9240 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9241 | ` *  with the standard PHP engine.` |
|        - |  9242 | ` */` |
|       28 |  9243 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9244 |  |
|        - |  9245 | `	const char *zStr; /* Input string */` |
|        - |  9246 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9247 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9248 | `	int nLen;` |
|        - |  9249 | `	sxi32 rc;` |
|       29 |  9250 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9251 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9252 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9253 | `		return PH7_OK;` |
|        - |  9254 | `	}` |
|        - |  9255 | `	/* Extract the given URI */` |
|       29 |  9256 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9257 | `	if( nLen < 1 ){` |
|        - |  9258 | `		/* Nothing to process,return FALSE */` |
|        3 |  9259 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9260 | `		return PH7_OK;` |
|        - |  9261 | `	}` |
|        - |  9262 | `	/* Get a parse */` |
|       27 |  9263 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9264 | `	if( rc != SXRET_OK ){` |
|        - |  9265 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9266 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9267 | `		return PH7_OK;` |
|        - |  9268 | `	}` |
|       27 |  9269 | `	if( nArg > 1 ){` |
|      ! 0 |  9270 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9271 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9272 | `		switch(nComponent){` |
|      ! 0 |  9273 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9274 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9275 | `			if( pComp->nByte < 1 ){` |
|        - |  9276 | `				/* No available value,return NULL */` |
|      ! 0 |  9277 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9278 | `			}else{` |
|      ! 0 |  9279 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9280 | `			}` |
|      ! 0 |  9281 | `			break;` |
|      ! 0 |  9282 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9283 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9284 | `			if( pComp->nByte < 1 ){` |
|        - |  9285 | `				/* No available value,return NULL */` |
|      ! 0 |  9286 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9287 | `			}else{` |
|      ! 0 |  9288 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9289 | `			}` |
|      ! 0 |  9290 | `			break;` |
|      ! 0 |  9291 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9292 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9293 | `			if( pComp->nByte < 1 ){` |
|        - |  9294 | `				/* No available value,return NULL */` |
|      ! 0 |  9295 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9296 | `			}else{` |
|      ! 0 |  9297 | `				int iPort = 0;` |
|        - |  9298 | `				/* Cast the value to integer */` |
|      ! 0 |  9299 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9300 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9301 | `			}` |
|      ! 0 |  9302 | `			break;` |
|      ! 0 |  9303 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9304 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9305 | `			if( pComp->nByte < 1 ){` |
|        - |  9306 | `				/* No available value,return NULL */` |
|      ! 0 |  9307 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9308 | `			}else{` |
|      ! 0 |  9309 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9310 | `			}` |
|      ! 0 |  9311 | `			break;` |
|      ! 0 |  9312 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9313 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9314 | `			if( pComp->nByte < 1 ){` |
|        - |  9315 | `				/* No available value,return NULL */` |
|      ! 0 |  9316 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9317 | `			}else{` |
|      ! 0 |  9318 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9319 | `			}` |
|      ! 0 |  9320 | `			break;` |
|      ! 0 |  9321 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9322 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9323 | `			if( pComp->nByte < 1 ){` |
|        - |  9324 | `				/* No available value,return NULL */` |
|      ! 0 |  9325 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9326 | `			}else{` |
|      ! 0 |  9327 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9328 | `			}` |
|      ! 0 |  9329 | `			break;` |
|      ! 0 |  9330 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9331 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9332 | `			if( pComp->nByte < 1 ){` |
|        - |  9333 | `				/* No available value,return NULL */` |
|      ! 0 |  9334 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9335 | `			}else{` |
|      ! 0 |  9336 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9337 | `			}` |
|      ! 0 |  9338 | `			break;` |
|      ! 0 |  9339 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9340 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9341 | `			if( pComp->nByte < 1 ){` |
|        - |  9342 | `				/* No available value,return NULL */` |
|      ! 0 |  9343 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9344 | `			}else{` |
|      ! 0 |  9345 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9346 | `			}` |
|      ! 0 |  9347 | `			break;` |
|      ! 0 |  9348 | `		default:` |
|        - |  9349 | `			/* No such entry,return NULL */` |
|      ! 0 |  9350 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9351 | `			break;` |
|        - |  9352 | `		}` |
|      ! 0 |  9353 | `	}else{` |
|        - |  9354 | `		ph7_value *pArray,*pValue;` |
|        - |  9355 | `		/* Return an associative array */` |
|       27 |  9356 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9357 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9358 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9359 | `			/* Out of memory */` |
|      ! 0 |  9360 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9361 | `			/* Return false */` |
|      ! 0 |  9362 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9363 | `			return PH7_OK;` |
|        - |  9364 | `		}` |
|        - |  9365 | `		/* Fill the array */` |
|       27 |  9366 | `		pComp = &sURI.sScheme;` |
|       27 |  9367 | `		if( pComp->nByte > 0 ){` |
|       19 |  9368 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9369 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9370 | `		}` |
|        - |  9371 | `		/* Reset the string cursor */` |
|       27 |  9372 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9373 | `		pComp = &sURI.sHost;` |
|       27 |  9374 | `		if( pComp->nByte > 0 ){` |
|       25 |  9375 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9376 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9377 | `		}` |
|        - |  9378 | `		/* Reset the string cursor */` |
|       27 |  9379 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9380 | `		pComp = &sURI.sPort;` |
|       27 |  9381 | `		if( pComp->nByte > 0 ){` |
|       11 |  9382 | `			int iPort = 0;/* cc warning */` |
|        - |  9383 | `			/* Convert to integer */` |
|       11 |  9384 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9385 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9386 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9387 | `		}` |
|        - |  9388 | `		/* Reset the string cursor */` |
|       27 |  9389 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9390 | `		pComp = &sURI.sUser;` |
|       27 |  9391 | `		if( pComp->nByte > 0 ){` |
|        7 |  9392 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9393 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9394 | `		}` |
|        - |  9395 | `		/* Reset the string cursor */` |
|       27 |  9396 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9397 | `		pComp = &sURI.sPass;` |
|       27 |  9398 | `		if( pComp->nByte > 0 ){` |
|        7 |  9399 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9400 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9401 | `		}` |
|        - |  9402 | `		/* Reset the string cursor */` |
|       27 |  9403 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9404 | `		pComp = &sURI.sPath;` |
|       27 |  9405 | `		if( pComp->nByte > 0 ){` |
|       17 |  9406 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9407 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9408 | `		}` |
|        - |  9409 | `		/* Reset the string cursor */` |
|       27 |  9410 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9411 | `		pComp = &sURI.sQuery;` |
|       27 |  9412 | `		if( pComp->nByte > 0 ){` |
|        5 |  9413 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9414 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9415 | `		}` |
|        - |  9416 | `		/* Reset the string cursor */` |
|       27 |  9417 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9418 | `		pComp = &sURI.sFragment;` |
|       27 |  9419 | `		if( pComp->nByte > 0 ){` |
|        5 |  9420 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9421 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9422 | `		}` |
|        - |  9423 | `		/* Return the created array */` |
|       27 |  9424 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9425 | `		/* NOTE:` |
|        - |  9426 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9427 | `		 * automatically as soon we return from this function.` |
|        - |  9428 | `		 */` |
|        - |  9429 | `	}` |
|        - |  9430 | `	/* All done */` |
|       27 |  9431 | `	return PH7_OK;` |
|       15 |  9432 |  |
|        - |  9433 | `/*` |
|        - |  9434 | ` * Section:` |
|        - |  9435 | ` *   Array related routines.` |
|        - |  9436 | ` * Status:` |
|        - |  9437 | ` *    Stable.` |
|        - |  9438 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9439 | ` *  Array related functions that need access to the underlying` |
|        - |  9440 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9441 | ` */` |
|        - |  9442 | `/*` |
|        - |  9443 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9444 | ` * of the following structure.` |
|        - |  9445 | ` */` |
|        - |  9446 | `struct compact_data` |
|        - |  9447 |  |
|        - |  9448 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9449 | `	int nRecCount;      /* Recursion count */` |
|        - |  9450 | `};` |
|        - |  9451 | `/*` |
|        - |  9452 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9453 | ` */` |
|      ! 0 |  9454 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9455 |  |
|      ! 0 |  9456 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9457 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9458 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9459 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9460 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9461 | `		SyString sVar;` |
|      ! 0 |  9462 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9463 | `		if( sVar.nByte > 0 ){` |
|        - |  9464 | `			/* Query the current frame */` |
|      ! 0 |  9465 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9466 | `			/* ^` |
|        - |  9467 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9468 | `			 */` |
|      ! 0 |  9469 | `			if( pKey ){` |
|        - |  9470 | `				/* Perform the insertion */` |
|      ! 0 |  9471 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9472 | `			}` |
|      ! 0 |  9473 | `		}` |
|      ! 0 |  9474 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9475 | `		int rc;` |
|        - |  9476 | `		/* Recursively traverse this array */` |
|      ! 0 |  9477 | `		pData->nRecCount++;` |
|      ! 0 |  9478 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9479 | `		pData->nRecCount--;` |
|      ! 0 |  9480 | `		return rc;` |
|        - |  9481 | `	}` |
|      ! 0 |  9482 | `	return SXRET_OK;` |
|      ! 0 |  9483 |  |
|        - |  9484 | `/*` |
|        - |  9485 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9486 | ` *  Create array containing variables and their values.` |
|        - |  9487 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9488 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9489 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9490 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9491 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9492 | ` * Parameters` |
|        - |  9493 | ` *  $varname` |
|        - |  9494 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9495 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9496 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9497 | ` *   it recursively.` |
|        - |  9498 | ` * Return` |
|        - |  9499 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9500 | ` */` |
|        2 |  9501 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9502 |  |
|        - |  9503 | `	ph7_value *pArray,*pObj;` |
|        3 |  9504 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9505 | `	const char *zName;` |
|        - |  9506 | `	SyString sVar;` |
|        - |  9507 | `	int i,nLen;` |
|        3 |  9508 | `	if( nArg < 1 ){` |
|        - |  9509 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9510 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9511 | `		return PH7_OK;` |
|        - |  9512 | `	}` |
|        - |  9513 | `	/* Create the array */` |
|        3 |  9514 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9515 | `	if( pArray == 0 ){` |
|        - |  9516 | `		/* Out of memory */` |
|      ! 0 |  9517 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9518 | `		/* Return NULL */` |
|      ! 0 |  9519 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9520 | `		return PH7_OK;` |
|        - |  9521 | `	}` |
|        - |  9522 | `	/* Perform the requested operation */` |
|        7 |  9523 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9524 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9525 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9526 | `				struct compact_data sData;` |
|      ! 0 |  9527 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9528 | `				/* Recursively walk the array */` |
|      ! 0 |  9529 | `				sData.nRecCount = 0;` |
|      ! 0 |  9530 | `				sData.pArray = pArray;` |
|      ! 0 |  9531 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9532 | `			}` |
|      ! 0 |  9533 | `		}else{` |
|        - |  9534 | `			/* Extract variable name */` |
|        5 |  9535 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9536 | `			if( nLen > 0 ){` |
|        5 |  9537 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9538 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9539 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9540 | `				if( pObj ){` |
|        5 |  9541 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9542 | `				}` |
|        2 |  9543 | `			}` |
|        - |  9544 | `		}` |
|        3 |  9545 | `	}` |
|        - |  9546 | `	/* Return the array */` |
|        3 |  9547 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9548 | `	return PH7_OK;` |
|        2 |  9549 |  |
|        - |  9550 | `/*` |
|        - |  9551 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9552 | ` * of the following structure.` |
|        - |  9553 | ` */` |
|        - |  9554 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9555 | `struct extract_aux_data` |
|        - |  9556 |  |
|        - |  9557 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9558 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9559 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9560 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9561 | `	int iFlags;           /* Control flags */` |
|        - |  9562 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9563 | `};` |
|        - |  9564 | `/* Forward declaration */` |
|        - |  9565 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9566 | `/*` |
|        - |  9567 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9568 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9569 | ` * Parameters` |
|        - |  9570 | ` * $var_array` |
|        - |  9571 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9572 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9573 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9574 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9575 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9576 | ` * $extract_type` |
|        - |  9577 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9578 | ` *  It can be one of the following values:` |
|        - |  9579 | ` *   EXTR_OVERWRITE` |
|        - |  9580 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9581 | ` *   EXTR_SKIP` |
|        - |  9582 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9583 | ` *   EXTR_PREFIX_SAME` |
|        - |  9584 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9585 | ` *   EXTR_PREFIX_ALL` |
|        - |  9586 | ` *       Prefix all variable names with prefix.` |
|        - |  9587 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9588 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9589 | ` *   EXTR_IF_EXISTS` |
|        - |  9590 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9591 | ` *       otherwise do nothing.` |
|        - |  9592 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9593 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9594 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9595 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9596 | ` *      the current symbol table.` |
|        - |  9597 | ` * $prefix` |
|        - |  9598 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9599 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9600 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9601 | ` *  underscore character.` |
|        - |  9602 | ` * Return` |
|        - |  9603 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9604 | ` */` |
|        4 |  9605 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9606 |  |
|        - |  9607 | `	extract_aux_data sAux;` |
|        - |  9608 | `	ph7_hashmap *pMap;` |
|        5 |  9609 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9610 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9611 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9612 | `		return PH7_OK;` |
|        - |  9613 | `	}` |
|        - |  9614 | `	/* Point to the target hashmap */` |
|        5 |  9615 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9616 | `	if( pMap->nEntry < 1 ){` |
|        - |  9617 | `		/* Empty map,return  0 */` |
|      ! 0 |  9618 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9619 | `		return PH7_OK;` |
|        - |  9620 | `	}` |
|        - |  9621 | `	/* Prepare the aux data */` |
|        5 |  9622 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9623 | `	if( nArg > 1 ){` |
|        3 |  9624 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9625 | `		if( nArg > 2 ){` |
|      ! 0 |  9626 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9627 | `		}` |
|        1 |  9628 | `	}` |
|        5 |  9629 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9630 | `	/* Invoke the worker callback */` |
|        5 |  9631 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9632 | `	/* Number of variables successfully imported */` |
|        5 |  9633 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9634 | `	return PH7_OK;` |
|        3 |  9635 |  |
|        - |  9636 | `/*` |
|        - |  9637 | ` * Worker callback for the [extract()] function defined` |
|        - |  9638 | ` * below.` |
|        - |  9639 | ` */` |
|        8 |  9640 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9641 |  |
|        9 |  9642 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9643 | `	int iFlags = pAux->iFlags;` |
|        9 |  9644 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9645 | `	ph7_value *pObj;` |
|        - |  9646 | `	SyString sVar;` |
|        9 |  9647 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9648 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9649 | `	}` |
|        - |  9650 | `	/* Perform a string cast */` |
|        9 |  9651 | `	PH7_MemObjToString(pKey);` |
|        9 |  9652 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9653 | `		/* Unavailable variable name */` |
|      ! 0 |  9654 | `		return SXRET_OK;` |
|        - |  9655 | `	}` |
|        9 |  9656 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9657 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9658 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9659 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9660 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9661 | `			);` |
|      ! 0 |  9662 | `	}else{` |
|       13 |  9663 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9664 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9665 | `	}` |
|        9 |  9666 | `	sVar.zString = pAux->zWorker;` |
|        - |  9667 | `	/* Try to extract the variable */` |
|        9 |  9668 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9669 | `	if( pObj ){` |
|        - |  9670 | `		/* Collision */` |
|        5 |  9671 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9672 | `			return SXRET_OK;` |
|        - |  9673 | `		}` |
|        5 |  9674 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9675 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9676 | `				/* Already prefixed */` |
|      ! 0 |  9677 | `				return SXRET_OK;` |
|        - |  9678 | `			}` |
|      ! 0 |  9679 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9680 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9681 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9682 | `				);` |
|      ! 0 |  9683 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9684 | `		}` |
|        3 |  9685 | `	}else{` |
|        - |  9686 | `		/* Create the variable */` |
|        5 |  9687 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9688 | `	}` |
|        9 |  9689 | `	if( pObj ){` |
|        - |  9690 | `		/* Overwrite the old value */` |
|        9 |  9691 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9692 | `		/* Increment counter */` |
|        9 |  9693 | `		pAux->iCount++;` |
|        4 |  9694 | `	}` |
|        9 |  9695 | `	return SXRET_OK;` |
|        5 |  9696 |  |
|        - |  9697 | `/*` |
|        - |  9698 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9699 | ` * defined below.` |
|        - |  9700 | ` */` |
|        2 |  9701 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9702 |  |
|        3 |  9703 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9704 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9705 | `	ph7_value *pObj;` |
|        - |  9706 | `	SyString sVar;` |
|        - |  9707 | `	/* Perform a string cast */` |
|        3 |  9708 | `	PH7_MemObjToString(pKey);` |
|        3 |  9709 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9710 | `		/* Unavailable variable name */` |
|      ! 0 |  9711 | `		return SXRET_OK;` |
|        - |  9712 | `	}` |
|        3 |  9713 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9714 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9715 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9716 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9717 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9718 | `			);` |
|        2 |  9719 | `	}else{` |
|      ! 0 |  9720 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9721 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9722 | `	}` |
|        3 |  9723 | `	sVar.zString = pAux->zWorker;` |
|        - |  9724 | `	/* Extract the variable */` |
|        3 |  9725 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9726 | `	if( pObj ){` |
|        3 |  9727 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9728 | `	}` |
|        3 |  9729 | `	return SXRET_OK;` |
|        2 |  9730 |  |
|        - |  9731 | `/*` |
|        - |  9732 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9733 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9734 | ` * Parameters` |
|        - |  9735 | ` * $types` |
|        - |  9736 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9737 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9738 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9739 | ` *  POST includes the POST uploaded file information.` |
|        - |  9740 | ` *  Note:` |
|        - |  9741 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9742 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9743 | ` * $prefix` |
|        - |  9744 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9745 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9746 | ` *  variable named $pref_userid.` |
|        - |  9747 | ` * Return` |
|        - |  9748 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9749 | ` */` |
|        2 |  9750 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9751 |  |
|        - |  9752 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9753 | `	extract_aux_data sAux;` |
|        - |  9754 | `	int nLen,nPrefixLen;` |
|        - |  9755 | `	ph7_value *pSuper;` |
|        - |  9756 | `	ph7_vm *pVm;` |
|        - |  9757 | `	/* By default import only $_GET variables  */` |
|        3 |  9758 | `	zImport = "G";` |
|        3 |  9759 | `	nLen = (int)sizeof(char);` |
|        3 |  9760 | `	zPrefix = 0;` |
|        3 |  9761 | `	nPrefixLen = 0;` |
|        3 |  9762 | `	if( nArg > 0 ){` |
|        3 |  9763 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9764 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9765 | `		}` |
|        3 |  9766 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9767 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9768 | `		}` |
|        1 |  9769 | `	}` |
|        - |  9770 | `	/* Point to the underlying VM */` |
|        3 |  9771 | `	pVm = pCtx->pVm;` |
|        - |  9772 | `	/* Initialize the aux data */` |
|        3 |  9773 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9774 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9775 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9776 | `	sAux.pVm = pVm;` |
|        - |  9777 | `	/* Extract */` |
|        3 |  9778 | `	zEnd = &zImport[nLen];` |
|        5 |  9779 | `	while( zImport < zEnd ){` |
|        3 |  9780 | `		int c = zImport[0];` |
|        3 |  9781 | `		pSuper = 0;` |
|        3 |  9782 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9783 | `			/* Import $_GET variables */` |
|        3 |  9784 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9785 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9786 | `			/* Import $_POST variables */` |
|      ! 0 |  9787 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9788 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9789 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9790 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9791 | `		}` |
|        3 |  9792 | `		if( pSuper ){` |
|        - |  9793 | `			/* Iterate throw array entries */` |
|        3 |  9794 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9795 | `		}` |
|        - |  9796 | `		/* Advance the cursor */` |
|        3 |  9797 | `		zImport++;` |
|        1 |  9798 | `	}` |
|        - |  9799 | `	/* All done,return TRUE*/` |
|        3 |  9800 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9801 | `	return PH7_OK;` |
|        1 |  9802 |  |
|        - |  9803 | `/*` |
|        - |  9804 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9805 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9806 | ` * information.` |
|        - |  9807 | ` */` |
|    10054 |  9808 | `static sxi32 VmEvalChunk(` |
|        - |  9809 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9810 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9811 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9812 | `	int iFlags,         /* Compile flag */` |
|        - |  9813 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9814 | `	)` |
|        2 |  9815 |  |
|        - |  9816 | `	SySet *pByteCode,aByteCode;` |
|        - |  9817 | `	SyBlob sSavedNs;` |
|    10056 |  9818 | `	ProcConsumer xErr = 0;` |
|    10056 |  9819 | `	void *pErrData = 0;` |
|        - |  9820 | `	/* Initialize bytecode container */` |
|    10056 |  9821 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10056 |  9822 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9823 | `	/* Reset the code generator */` |
|    10056 |  9824 | `	if( bTrueReturn ){` |
|        - |  9825 | `		/* Included file,log compile-time errors */` |
|     7535 |  9826 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7535 |  9827 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3767 |  9828 | `	}` |
|    10056 |  9829 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9830 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |  9831 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |  9832 | `	 * the caller's namespace is restored. */` |
|    10056 |  9833 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10056 |  9834 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10056 |  9835 | `	if( bTrueReturn ){` |
|        - |  9836 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7535 |  9837 | `		SyBlobReset(&pVm->sNamespace);` |
|     3767 |  9838 | `	}` |
|        - |  9839 | `	/* Swap bytecode container */` |
|    10056 |  9840 | `	pByteCode = pVm->pByteContainer;` |
|    10056 |  9841 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9842 | `	/* Compile the chunk */` |
|    10056 |  9843 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15083 |  9844 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9845 | `		/* Compilation error,return false */` |
|        3 |  9846 | `		if( pCtx ){` |
|        3 |  9847 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9848 | `		}` |
|        2 |  9849 | `	}else{` |
|        - |  9850 | `		/* Mount any newly defined classes */` |
|        - |  9851 | `		SyHashEntry *pEntry;` |
|        - |  9852 | `		ph7_class *pClass;` |
|        - |  9853 | `		ph7_value sResult; /* Return value */` |
|        - |  9854 | `		sxi32 rc;` |
|    10054 |  9855 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   275724 |  9856 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   260646 |  9857 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9858 | `			/* Only mount classes that haven't been mounted yet */` |
|   260646 |  9859 | `			if( !pClass->bMounted ){` |
|    62164 |  9860 | `				rc = VmMountUserClass(pVm,pClass);` |
|    62164 |  9861 | `				if( rc != SXRET_OK ){` |
|        - |  9862 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9863 | `					if( pCtx ){` |
|      ! 0 |  9864 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9865 | `					}` |
|      ! 0 |  9866 | `					goto Cleanup;` |
|        - |  9867 | `				}` |
|    31081 |  9868 | `			}` |
|        2 |  9869 | `		}` |
|    10054 |  9870 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9871 | `			/* Out of memory */` |
|      ! 0 |  9872 | `			if( pCtx ){` |
|      ! 0 |  9873 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9874 | `			}` |
|      ! 0 |  9875 | `			goto Cleanup;` |
|        - |  9876 | `		}` |
|    10054 |  9877 | `		if( bTrueReturn ){` |
|        - |  9878 | `			/* Assume a boolean true return value */` |
|     7535 |  9879 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3768 |  9880 | `		}else{` |
|        - |  9881 | `			/* Assume a null return value */` |
|     2520 |  9882 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9883 | `		}` |
|        - |  9884 | `		/* Execute the compiled chunk */` |
|    10054 |  9885 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10054 |  9886 | `		if( pCtx ){` |
|        - |  9887 | `			/* Set the execution result */` |
|     7548 |  9888 | `			ph7_result_value(pCtx,&sResult);` |
|     3773 |  9889 | `		}` |
|    10054 |  9890 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9891 | `	}` |
|     5027 |  9892 | `Cleanup:` |
|        - |  9893 | `	/* Cleanup the mess left behind */` |
|    10056 |  9894 | `	pVm->pByteContainer = pByteCode;` |
|    10056 |  9895 | `	SySetRelease(&aByteCode);` |
|        - |  9896 | `	/* Restore caller's namespace state */` |
|    10056 |  9897 | `	SyBlobReset(&pVm->sNamespace);` |
|    10056 |  9898 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10056 |  9899 | `	SyBlobRelease(&sSavedNs);` |
|    10056 |  9900 | `	return SXRET_OK;` |
|        2 |  9901 |  |
|        - |  9902 | `/*` |
|        - |  9903 | ` * value eval(string $code)` |
|        - |  9904 | ` *   Evaluate a string as PHP code.` |
|        - |  9905 | ` * Parameter` |
|        - |  9906 | ` *  code: PHP code to evaluate.` |
|        - |  9907 | ` * Return` |
|        - |  9908 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9909 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9910 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9911 | ` */` |
|       16 |  9912 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9913 |  |
|        - |  9914 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9915 | `	if( nArg < 1 ){` |
|        - |  9916 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9917 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9918 | `		return SXRET_OK;` |
|        - |  9919 | `	}` |
|        - |  9920 | `	/* Chunk to evaluate */` |
|       18 |  9921 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9922 | `	if( sChunk.nByte < 1 ){` |
|        - |  9923 | `		/* Empty string,return NULL */` |
|        3 |  9924 | `		ph7_result_null(pCtx);` |
|        3 |  9925 | `		return SXRET_OK;` |
|        - |  9926 | `	}` |
|        - |  9927 | `	/* Eval the chunk */` |
|       16 |  9928 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9929 | `	return SXRET_OK;` |
|       10 |  9930 |  |
|        - |  9931 | `/*` |
|        - |  9932 | ` * Check if a file path is already included.` |
|        - |  9933 | ` */` |
|    15064 |  9934 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9935 |  |
|        - |  9936 | `	SyString *aEntries;` |
|        - |  9937 | `	sxu32 n;` |
|    15065 |  9938 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9939 | `	/* Perform a linear search */` |
| 56720651 |  9940 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56705593 |  9941 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9942 | `			/* Already included */` |
|        7 |  9943 | `			return TRUE;` |
|        - |  9944 | `		}` |
| 28352794 |  9945 | `	}` |
|    15059 |  9946 | `	return FALSE;` |
|     7533 |  9947 |  |
|        - |  9948 | `/*` |
|        - |  9949 | ` * Push a file path in the appropriate VM container.` |
|        - |  9950 | ` */` |
|    17562 |  9951 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9952 |  |
|        - |  9953 | `	SyString sPath;` |
|        - |  9954 | `	char *zDup;` |
|        - |  9955 | `#ifdef __WINNT__` |
|        - |  9956 | `	char *zCur;` |
|        - |  9957 | `#endif` |
|        - |  9958 | `	sxi32 rc;` |
|    17564 |  9959 | `	if( nLen < 0 ){` |
|     2500 |  9960 | `		nLen = SyStrlen(zPath);` |
|     1249 |  9961 | `	}` |
|        - |  9962 | `	/* Duplicate the file path first */` |
|    17564 |  9963 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17564 |  9964 | `	if( zDup == 0 ){` |
|      ! 0 |  9965 | `		return SXERR_MEM;` |
|        - |  9966 | `	}` |
|        - |  9967 | `#ifdef __WINNT__` |
|        - |  9968 | `	/* Normalize path on windows` |
|        - |  9969 | `	 * Example:` |
|        - |  9970 | `	 *    Path/To/File.php` |
|        - |  9971 | `	 * becomes` |
|        - |  9972 | `	 *   path\to\file.php` |
|        - |  9973 | `	 */` |
|        2 |  9974 | `	zCur = zDup;` |
|        2 |  9975 | `	while( zCur[0] != 0 ){` |
|        2 |  9976 | `		if( zCur[0] == '/' ){` |
|        2 |  9977 | `			zCur[0] = '\\';` |
|        2 |  9978 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9979 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9980 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9981 | `		}` |
|        2 |  9982 | `		zCur++;` |
|        2 |  9983 | `	}` |
|        - |  9984 | `#endif` |
|        - |  9985 | `	/* Install the file path */` |
|    17564 |  9986 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17564 |  9987 | `	if( !bMain ){` |
|    15065 |  9988 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9989 | `			/* Already included */` |
|        7 |  9990 | `			*pNew = 0;` |
|        4 |  9991 | `		}else{` |
|        - |  9992 | `			/* Insert in the corresponding container */` |
|    15059 |  9993 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15059 |  9994 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9995 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9996 | `				return rc;` |
|        - |  9997 | `			}` |
|    15059 |  9998 | `			*pNew = 1;` |
|        - |  9999 | `		}` |
|     7532 | 10000 | `	}` |
|    17564 | 10001 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17564 | 10002 | `	return SXRET_OK;` |
|     8783 | 10003 |  |
|        - | 10004 | `/*` |
|        - | 10005 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10006 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10007 | ` * indicates failure.` |
|        - | 10008 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10009 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10010 | ` * operations.` |
|        - | 10011 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10012 | ` * this function is a no-op.` |
|        - | 10013 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10014 | ` * constructs for more information.` |
|        - | 10015 | ` */` |
|     7540 | 10016 | `static sxi32 VmExecIncludedFile(` |
|        - | 10017 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10018 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10019 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10020 | `	 )` |
|        2 | 10021 |  |
|        - | 10022 | `	sxi32 rc;` |
|        - | 10023 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10024 | `	const ph7_io_stream *pStream;` |
|        - | 10025 | `	SyBlob sContents;` |
|        - | 10026 | `	void *pHandle;` |
|        - | 10027 | `	ph7_vm *pVm;` |
|        - | 10028 | `	int isNew;` |
|        - | 10029 | `	/* Initialize fields */` |
|     7542 | 10030 | `	pVm = pCtx->pVm;` |
|     7542 | 10031 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7542 | 10032 | `	isNew = 0;` |
|        - | 10033 | `	/* Extract the associated stream */` |
|     7542 | 10034 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10035 | `	/*` |
|        - | 10036 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10037 | `	 * in a read-only mode.` |
|        - | 10038 | `	 */` |
|     7542 | 10039 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7542 | 10040 | `	if( pHandle == 0 ){` |
|        3 | 10041 | `		return SXERR_IO;` |
|        - | 10042 | `	}` |
|     7539 | 10043 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7539 | 10044 | `	if( IncludeOnce && !isNew ){` |
|        - | 10045 | `		/* Already included */` |
|        5 | 10046 | `		rc = SXERR_EXISTS;` |
|        3 | 10047 | `	}else{` |
|        - | 10048 | `		/* Read the whole file contents */` |
|     7535 | 10049 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7535 | 10050 | `		if( rc == SXRET_OK ){` |
|        - | 10051 | `			SyString sScript;` |
|        - | 10052 | `			/* Compile and execute the script */` |
|     7535 | 10053 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7535 | 10054 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3767 | 10055 | `		}` |
|        - | 10056 | `	}` |
|        - | 10057 | `	/* Pop from the set of included file */` |
|     7539 | 10058 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10059 | `	/* Close the handle */` |
|     7539 | 10060 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10061 | `	/* Release the working buffer */` |
|     7539 | 10062 | `	SyBlobRelease(&sContents);` |
|        - | 10063 | `#else` |
|        - | 10064 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10065 | `	SXUNUSED(pPath);` |
|        - | 10066 | `	SXUNUSED(IncludeOnce);` |
|        - | 10067 | `	rc = SXERR_IO;` |
|        - | 10068 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7539 | 10069 | `	return rc;` |
|     3772 | 10070 |  |
|        - | 10071 | `/*` |
|        - | 10072 | ` * string get_include_path(void)` |
|        - | 10073 | ` *  Gets the current include_path configuration option.` |
|        - | 10074 | ` * Parameter` |
|        - | 10075 | ` *  None` |
|        - | 10076 | ` * Return` |
|        - | 10077 | ` *  Included paths as a string` |
|        - | 10078 | ` */` |
|        2 | 10079 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10080 |  |
|        3 | 10081 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10082 | `	SyString *aEntry;` |
|        - | 10083 | `	int dir_sep;` |
|        - | 10084 | `	sxu32 n;` |
|        - | 10085 | `#ifdef __WINNT__` |
|        1 | 10086 | `	dir_sep = ';';` |
|        - | 10087 | `#else` |
|        - | 10088 | `	/* Assume UNIX path separator */` |
|        2 | 10089 | `	dir_sep = ':';` |
|        - | 10090 | `#endif` |
|        1 | 10091 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10092 | `	SXUNUSED(apArg);` |
|        - | 10093 | `	/* Point to the list of import paths */` |
|        3 | 10094 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 10095 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 10096 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 10097 | `		if( n > 0 ){` |
|        - | 10098 | `			/* Append dir seprator */` |
|      ! 0 | 10099 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 10100 | `		}` |
|        - | 10101 | `		/* Append path */` |
|        3 | 10102 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 10103 | `	}` |
|        3 | 10104 | `	return PH7_OK;` |
|        1 | 10105 |  |
|        - | 10106 | `/*` |
|        - | 10107 | ` * string get_get_included_files(void)` |
|        - | 10108 | ` *  Gets the current include_path configuration option.` |
|        - | 10109 | ` * Parameter` |
|        - | 10110 | ` *  None` |
|        - | 10111 | ` * Return` |
|        - | 10112 | ` *  Included paths as a string` |
|        - | 10113 | ` */` |
|        2 | 10114 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10115 |  |
|        3 | 10116 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 10117 | `	ph7_value *pArray,*pWorker;` |
|        - | 10118 | `	SyString *pEntry;` |
|        - | 10119 | `	int c,d;` |
|        - | 10120 | `	/* Create an array and a working value */` |
|        3 | 10121 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10122 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10123 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10124 | `		/* Out of memory,return null */` |
|      ! 0 | 10125 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10126 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10127 | `		SXUNUSED(apArg);` |
|      ! 0 | 10128 | `		return PH7_OK;` |
|        - | 10129 | `	}` |
|        3 | 10130 | `	c = d = '/';` |
|        - | 10131 | `#ifdef __WINNT__` |
|        1 | 10132 | `	d = '\\';` |
|        - | 10133 | `#endif` |
|        - | 10134 | `	/* Iterate throw entries */` |
|        3 | 10135 | `	SySetResetCursor(pFiles);` |
|     3691 | 10136 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10137 | `		const char *zBase,*zEnd;` |
|        - | 10138 | `		int iLen;` |
|        - | 10139 | `		/* reset the string cursor */` |
|     3689 | 10140 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10141 | `		/* Extract base name */` |
|     3689 | 10142 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10143 | `		/* Ignore trailing '/' */` |
|     5533 | 10144 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10145 | `			zEnd--;` |
|      ! 0 | 10146 | `		}` |
|     3689 | 10147 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 | 10148 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 | 10149 | `			zEnd--;` |
|        1 | 10150 | `		}` |
|     3689 | 10151 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 | 10152 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10153 | `		/* Copy entry name */` |
|     3689 | 10154 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10155 | `		/* Perform the insertion */` |
|     3689 | 10156 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10157 | `	}` |
|        - | 10158 | `	/* All done,return the created array */` |
|        3 | 10159 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10160 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10161 | `	 * by the engine as soon we return from this foreign` |
|        - | 10162 | `	 * function.` |
|        - | 10163 | `	 */` |
|        3 | 10164 | `	return PH7_OK;` |
|        2 | 10165 |  |
|        - | 10166 | `/*` |
|        - | 10167 | ` * include:` |
|        - | 10168 | ` * According to the PHP reference manual.` |
|        - | 10169 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10170 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10171 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10172 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10173 | ` *  and the current working directory before failing. The include()` |
|        - | 10174 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10175 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10176 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10177 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10178 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10179 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10180 | ` *  directory to find the requested file.` |
|        - | 10181 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10182 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10183 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10184 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10185 | ` */` |
|     7528 | 10186 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10187 |  |
|        - | 10188 | `	SyString sFile;` |
|        - | 10189 | `	sxi32 rc;` |
|     7530 | 10190 | `	if( nArg < 1 ){` |
|        - | 10191 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10192 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10193 | `		return SXRET_OK;` |
|        - | 10194 | `	}` |
|        - | 10195 | `	/* File to include */` |
|     7530 | 10196 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7530 | 10197 | `	if( sFile.nByte < 1 ){` |
|        - | 10198 | `		/* Empty string,return NULL */` |
|      ! 0 | 10199 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10200 | `		return SXRET_OK;` |
|        - | 10201 | `	}` |
|        - | 10202 | `	/* Open,compile and execute the desired script */` |
|     7530 | 10203 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7530 | 10204 | `	if( rc != SXRET_OK ){` |
|        - | 10205 | `		/* Emit a warning and return false */` |
|        3 | 10206 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10207 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10208 | `	}` |
|     7530 | 10209 | `	return SXRET_OK;` |
|     3766 | 10210 |  |
|        - | 10211 | `/*` |
|        - | 10212 | ` * include_once:` |
|        - | 10213 | ` *  According to the PHP reference manual.` |
|        - | 10214 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10215 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10216 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10217 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10218 | ` *   just once.` |
|        - | 10219 | ` */` |
|        4 | 10220 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10221 |  |
|        - | 10222 | `	SyString sFile;` |
|        - | 10223 | `	sxi32 rc;` |
|        5 | 10224 | `	if( nArg < 1 ){` |
|        - | 10225 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10226 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10227 | `		return SXRET_OK;` |
|        - | 10228 | `	}` |
|        - | 10229 | `	/* File to include */` |
|        5 | 10230 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10231 | `	if( sFile.nByte < 1 ){` |
|        - | 10232 | `		/* Empty string,return NULL */` |
|      ! 0 | 10233 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10234 | `		return SXRET_OK;` |
|        - | 10235 | `	}` |
|        - | 10236 | `	/* Open,compile and execute the desired script */` |
|        5 | 10237 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10238 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10239 | `		/* File already included,return TRUE */` |
|        3 | 10240 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10241 | `		return SXRET_OK;` |
|        - | 10242 | `	}` |
|        3 | 10243 | `	if( rc != SXRET_OK ){` |
|        - | 10244 | `		/* Emit a warning and return false */` |
|      ! 0 | 10245 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10246 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10247 | ` 	}` |
|        3 | 10248 | `	return SXRET_OK;` |
|        3 | 10249 |  |
|        - | 10250 | `/*` |
|        - | 10251 | ` * require.` |
|        - | 10252 | ` *  According to the PHP reference manual.` |
|        - | 10253 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10254 | ` *   also produce a fatal level error.` |
|        - | 10255 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10256 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10257 | ` */` |
|        4 | 10258 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10259 |  |
|        - | 10260 | `	SyString sFile;` |
|        - | 10261 | `	sxi32 rc;` |
|        5 | 10262 | `	if( nArg < 1 ){` |
|        - | 10263 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10264 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10265 | `		return SXRET_OK;` |
|        - | 10266 | `	}` |
|        - | 10267 | `	/* File to include */` |
|        5 | 10268 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10269 | `	if( sFile.nByte < 1 ){` |
|        - | 10270 | `		/* Empty string,return NULL */` |
|      ! 0 | 10271 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10272 | `		return SXRET_OK;` |
|        - | 10273 | `	}` |
|        - | 10274 | `	/* Open,compile and execute the desired script */` |
|        5 | 10275 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10276 | `	if( rc != SXRET_OK ){` |
|        - | 10277 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10278 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10279 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10280 | `		return PH7_ABORT;` |
|        - | 10281 | `	}` |
|        5 | 10282 | `	return SXRET_OK;` |
|        3 | 10283 |  |
|        - | 10284 | `/*` |
|        - | 10285 | ` * require_once:` |
|        - | 10286 | ` *  According to the PHP reference manual.` |
|        - | 10287 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10288 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10289 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10290 | ` *   and how it differs from its non _once siblings.` |
|        - | 10291 | ` */` |
|        4 | 10292 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10293 |  |
|        - | 10294 | `	SyString sFile;` |
|        - | 10295 | `	sxi32 rc;` |
|        5 | 10296 | `	if( nArg < 1 ){` |
|        - | 10297 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10298 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10299 | `		return SXRET_OK;` |
|        - | 10300 | `	}` |
|        - | 10301 | `	/* File to include */` |
|        5 | 10302 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10303 | `	if( sFile.nByte < 1 ){` |
|        - | 10304 | `		/* Empty string,return NULL */` |
|      ! 0 | 10305 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10306 | `		return SXRET_OK;` |
|        - | 10307 | `	}` |
|        - | 10308 | `	/* Open,compile and execute the desired script */` |
|        5 | 10309 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10310 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10311 | `		/* File already included,return TRUE */` |
|        3 | 10312 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10313 | `		return SXRET_OK;` |
|        - | 10314 | `	}` |
|        3 | 10315 | `	if( rc != SXRET_OK ){` |
|        - | 10316 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10317 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10318 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10319 | `		return PH7_ABORT;` |
|        - | 10320 | `	}` |
|        3 | 10321 | `	return SXRET_OK;` |
|        3 | 10322 |  |
|        - | 10323 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10324 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10325 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10326 | `/* Table of built-in VM functions. */` |
|        - | 10327 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10328 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10329 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10330 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10331 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10332 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10333 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10334 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10335 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10336 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10337 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10338 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10339 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10340 | `	    /* Constants management */` |
|        - | 10341 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10342 | `	{ "define",   vm_builtin_define               },` |
|        - | 10343 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10344 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10345 | `	   /* Class/Object functions */` |
|        - | 10346 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10347 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10348 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10349 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10350 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10351 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10352 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10353 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10354 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10355 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10356 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10357 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10358 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10359 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10360 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10361 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10362 | `	   /* Random numbers/strings generators */` |
|        - | 10363 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10364 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10365 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10366 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10367 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10368 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10369 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10370 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10371 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10372 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10373 | `	   /* Language constructs functions */` |
|        - | 10374 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10375 | `	{ "print", vm_builtin_print                   },` |
|        - | 10376 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10377 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10378 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10379 | `	  /* Variable handling functions */` |
|        - | 10380 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10381 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10382 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10383 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10384 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10385 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10386 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10387 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10388 | `	  /* Ouput control functions */` |
|        - | 10389 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10390 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10391 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10392 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10393 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10394 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10395 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10396 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10397 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10398 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10399 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10400 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10401 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10402 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10403 | `	  /* Assertion functions */` |
|        - | 10404 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10405 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10406 | `	  /* Error reporting functions */` |
|        - | 10407 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10408 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10409 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10410 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10411 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10412 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10413 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10414 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10415 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10416 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10417 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10418 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10419 | `	  /* Release info */` |
|        - | 10420 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10421 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10422 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10423 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10424 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10425 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10426 | `	  /* hashmap */` |
|        - | 10427 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10428 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10429 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10430 | `	  /* URL related function */` |
|        - | 10431 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10432 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10433 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10434 | `	   /* XML processing functions */` |
|        - | 10435 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10436 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10437 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10438 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10439 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10440 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10441 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10442 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10443 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10444 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10445 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10446 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10447 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10448 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10449 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10450 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10451 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10452 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10453 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10454 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10455 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10456 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10457 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10458 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10459 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10460 | `	   /* Command line processing */` |
|        - | 10461 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10462 | `	   /* JSON encoding/decoding */` |
|        - | 10463 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10464 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10465 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10466 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10467 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10468 | `	   /* Files/URI inclusion facility */` |
|        - | 10469 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10470 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10471 | `	{ "include",      vm_builtin_include          },` |
|        - | 10472 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10473 | `	{ "require",      vm_builtin_require          },` |
|        - | 10474 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10475 | `};` |
|        - | 10476 | `/*` |
|        - | 10477 | ` * Register the built-in VM functions defined above.` |
|        - | 10478 | ` */` |
|     2248 | 10479 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10480 |  |
|        - | 10481 | `	sxi32 rc;` |
|        - | 10482 | `	sxu32 n;` |
|   281002 | 10483 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10484 | `		/* Note that these special functions have access` |
|        - | 10485 | `		 * to the underlying virtual machine as their` |
|        - | 10486 | `		 * private data.` |
|        - | 10487 | `		 */` |
|   278754 | 10488 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   278754 | 10489 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10490 | `			return rc;` |
|        - | 10491 | `		}` |
|   139378 | 10492 | `	}` |
|     2250 | 10493 | `	return SXRET_OK;` |
|     1126 | 10494 |  |
|        - | 10495 | `/*` |
|        - | 10496 | ` * Check if the given name refer to an installed class.` |
|        - | 10497 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10498 | ` */` |
|    16450 | 10499 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10500 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10501 | `	const char *zName,  /* Name of the target class */` |
|        - | 10502 | `	sxu32 nByte,        /* zName length */` |
|        - | 10503 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10504 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10505 | `						 */` |
|        - | 10506 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10507 | `	)` |
|        2 | 10508 |  |
|        - | 10509 | `	SyHashEntry *pEntry;` |
|        - | 10510 | `	ph7_class *pClass;` |
|     8225 | 10511 | `	SXUNUSED(iNest);` |
|        - | 10512 | `	/* Exact class lookup.` |
|        - | 10513 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 10514 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    16452 | 10515 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    16452 | 10516 | `	if( pEntry == 0 ){` |
|       10 | 10517 | `		return 0;` |
|        - | 10518 | `	}` |
|    16444 | 10519 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    16444 | 10520 | `	if( !iLoadable ){` |
|    15318 | 10521 | `		return pClass;` |
|        - | 10522 | `	}` |
|        - | 10523 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1128 | 10524 | `	while(pClass){` |
|     1128 | 10525 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1128 | 10526 | `			return pClass;` |
|        - | 10527 | `		}` |
|      ! 0 | 10528 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10529 | `	}` |
|      ! 0 | 10530 | `	return 0;` |
|     8227 | 10531 |  |
|        - | 10532 | `/*` |
|        - | 10533 | ` * Reference Table Implementation` |
|        - | 10534 | ` * Status: stable <chm@symisc.net>` |
|        - | 10535 | ` * Intro` |
|        - | 10536 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10537 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10538 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10539 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10540 | ` *  Refer to the official for more information on this powerful` |
|        - | 10541 | ` *  extension.` |
|        - | 10542 | ` */` |
|        - | 10543 | `/*` |
|        - | 10544 | ` * Allocate a new reference entry.` |
|        - | 10545 | ` */` |
|  2994356 | 10546 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10547 |  |
|        - | 10548 | `	VmRefObj *pRef;` |
|        - | 10549 | `	/* Allocate a new instance */` |
|  2994358 | 10550 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2994358 | 10551 | `	if( pRef == 0 ){` |
|      ! 0 | 10552 | `		return 0;` |
|        - | 10553 | `	}` |
|        - | 10554 | `	/* Zero the structure */` |
|  2994358 | 10555 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10556 | `	/* Initialize fields */` |
|  2994358 | 10557 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2994358 | 10558 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2994358 | 10559 | `	pRef->nIdx = nIdx;` |
|  2994358 | 10560 | `	return pRef;` |
|  1497180 | 10561 |  |
|        - | 10562 | `/*` |
|        - | 10563 | ` * Default hash function used by the reference table` |
|        - | 10564 | ` * for lookup/insertion operations.` |
|        - | 10565 | ` */` |
| 16612372 | 10566 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10567 |  |
|        - | 10568 | `	/* Calculate the hash based on the memory object index */` |
| 16612374 | 10569 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10570 |  |
|        - | 10571 | `/*` |
|        - | 10572 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10573 | ` * in the reference table.` |
|        - | 10574 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10575 | ` * otherwise.` |
|        - | 10576 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10577 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10578 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10579 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10580 | ` * Refer to the official for more information on this powerful` |
|        - | 10581 | ` * extension.` |
|        - | 10582 | ` */` |
|  8938904 | 10583 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10584 |  |
|        - | 10585 | `	VmRefObj *pRef;` |
|        - | 10586 | `	sxu32 nBucket;` |
|        - | 10587 | `	/* Point to the appropriate bucket */` |
|  8938906 | 10588 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10589 | `	/* Perform the lookup */` |
|  8938906 | 10590 | `	pRef = pVm->apRefObj[nBucket];` |
| 18828365 | 10591 | `	for(;;){` |
| 37653256 | 10592 | `		if( pRef == 0 ){` |
|  3070038 | 10593 | `			break;` |
|        - | 10594 | `		}` |
| 34583220 | 10595 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10596 | `			/* Entry found */` |
|  5868870 | 10597 | `			return pRef;` |
|        - | 10598 | `		}` |
|        - | 10599 | `		/* Point to the next entry */` |
| 28714352 | 10600 | `		pRef = pRef->pNextCollide;` |
|        2 | 10601 | `	}` |
|        - | 10602 | `	/* No such entry,return NULL */` |
|  3070038 | 10603 | `	return 0;` |
|  4469454 | 10604 |  |
|        - | 10605 | `/*` |
|        - | 10606 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10607 | ` *` |
|        - | 10608 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10609 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10610 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10611 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10612 | ` * Refer to the official for more information on this powerful` |
|        - | 10613 | ` * extension.` |
|        - | 10614 | ` */` |
|  2994356 | 10615 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10616 |  |
|        - | 10617 | `	sxu32 nBucket;` |
|  2994358 | 10618 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10619 | `		VmRefObj **apNew;` |
|        - | 10620 | `		sxu32 nNew;` |
|        - | 10621 | `		/* Allocate a larger table */` |
|     3528 | 10622 | `		nNew = pVm->nRefSize << 1;` |
|     3528 | 10623 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3528 | 10624 | `		if( apNew ){` |
|     3528 | 10625 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10626 | `			sxu32 n;` |
|        - | 10627 | `			/* Zero the structure */` |
|     3528 | 10628 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10629 | `			/* Rehash all referenced entries */` |
|  2835408 | 10630 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10631 | `				/* Remove old collision links */` |
|  2831882 | 10632 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10633 | `				/* Point to the appropriate bucket */` |
|  2831882 | 10634 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10635 | `				/* Insert the entry  */` |
|  2831882 | 10636 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2831882 | 10637 | `				if( apNew[nBucket] ){` |
|  2298896 | 10638 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10639 | `				}` |
|  2831882 | 10640 | `				apNew[nBucket] = pEntry;` |
|        - | 10641 | `				/* Point to the next entry */` |
|  2831882 | 10642 | `				pEntry = pEntry->pNext;` |
|  1415942 | 10643 | `			}` |
|        - | 10644 | `			/* Release the old table */` |
|     3528 | 10645 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10646 | `			/* Install the new one */` |
|     3528 | 10647 | `			pVm->apRefObj = apNew;` |
|     3528 | 10648 | `			pVm->nRefSize = nNew;` |
|     1763 | 10649 | `		}` |
|     1763 | 10650 | `	}` |
|        - | 10651 | `	/* Point to the appropriate bucket */` |
|  2994358 | 10652 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10653 | `	/* Insert the entry */` |
|  2994358 | 10654 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2994358 | 10655 | `	if( pVm->apRefObj[nBucket] ){` |
|  2481957 | 10656 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1240751 | 10657 | `	}` |
|  2994358 | 10658 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2994358 | 10659 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2994358 | 10660 | `	pVm->nRefUsed++;` |
|  2994358 | 10661 | `	return SXRET_OK;` |
|        2 | 10662 |  |
|        - | 10663 | `/*` |
|        - | 10664 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10665 | ` * the reference table.` |
|        - | 10666 | ` * This function is invoked when the user perform an unset` |
|        - | 10667 | ` * call [i.e: unset($var); ].` |
|        - | 10668 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10669 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10670 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10671 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10672 | ` * Refer to the official for more information on this powerful` |
|        - | 10673 | ` * extension.` |
|        - | 10674 | ` */` |
|  2962428 | 10675 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10676 |  |
|        - | 10677 | `	ph7_hashmap_node **apNode;` |
|        - | 10678 | `	SyHashEntry **apEntry;` |
|        - | 10679 | `	sxu32 n;` |
|        - | 10680 | `	/* Point to the reference table */` |
|  2962430 | 10681 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2962430 | 10682 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10683 | `	/* Unlink the entry from the reference table */` |
|  3043072 | 10684 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    80644 | 10685 | `		if( apEntry[n] ){` |
|    80594 | 10686 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    40296 | 10687 | `		}` |
|    40323 | 10688 | `	}` |
|  5846066 | 10689 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2883638 | 10690 | `		if( apNode[n] ){` |
|     5635 | 10691 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2817 | 10692 | `		}` |
|  1441820 | 10693 | `	}` |
|  2962430 | 10694 | `	if( pRef->pPrevCollide ){` |
|  1115198 | 10695 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   557473 | 10696 | `	}else{` |
|  1847234 | 10697 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10698 | `	}` |
|  2962430 | 10699 | `	if( pRef->pNextCollide ){` |
|  1669606 | 10700 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   834713 | 10701 | `	}` |
|  2962430 | 10702 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10703 | `	/* Release the node */` |
|  2962430 | 10704 | `	SySetRelease(&pRef->aReference);` |
|  2962430 | 10705 | `	SySetRelease(&pRef->aArrEntries);` |
|  2962430 | 10706 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2962430 | 10707 | `	pVm->nRefUsed--;` |
|  2962430 | 10708 | `	return SXRET_OK;` |
|        2 | 10709 |  |
|        - | 10710 | `/*` |
|        - | 10711 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10712 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10713 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10714 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10715 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10716 | ` * Refer to the official for more information on this powerful` |
|        - | 10717 | ` * extension.` |
|        - | 10718 | ` */` |
|  3022734 | 10719 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10720 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10721 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10722 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10723 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10724 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10725 | `	)` |
|        2 | 10726 |  |
|  3022736 | 10727 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10728 | `	VmRefObj *pRef;` |
|        - | 10729 | `	/* Check if the referenced object already exists */` |
|  3022736 | 10730 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3022736 | 10731 | `	if( pRef == 0 ){` |
|        - | 10732 | `		/* Create a new entry */` |
|  2994358 | 10733 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2994358 | 10734 | `		if( pRef == 0 ){` |
|      ! 0 | 10735 | `			return SXERR_MEM;` |
|        - | 10736 | `		}` |
|  2994358 | 10737 | `		pRef->iFlags = iFlags;` |
|        - | 10738 | `		/* Install the entry */` |
|  2994358 | 10739 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1497178 | 10740 | `	}` |
|  3022896 | 10741 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10742 | `		/* Safely ignore the exception frame */` |
|      162 | 10743 | `		pFrame = pFrame->pParent;` |
|        2 | 10744 | `	}` |
|  3022736 | 10745 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10746 | `		VmSlot sRef;` |
|        - | 10747 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10748 | `		 * be deleted when we leave this frame.` |
|        - | 10749 | `		 */` |
|    75716 | 10750 | `		sRef.nIdx = nIdx;` |
|    75716 | 10751 | `		sRef.pUserData = pEntry;` |
|    75716 | 10752 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10753 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10754 | `		}` |
|    37857 | 10755 | `	}` |
|  3022736 | 10756 | `	if( pEntry ){` |
|        - | 10757 | `		/* Address of the hash-entry */` |
|   103904 | 10758 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    51951 | 10759 | `	}` |
|  3022736 | 10760 | `	if( pMapEntry ){` |
|        - | 10761 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2913976 | 10762 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1456987 | 10763 | `	}` |
|  3022736 | 10764 | `	return SXRET_OK;` |
|  1511369 | 10765 |  |
|        - | 10766 | `/*` |
|        - | 10767 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10768 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10769 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10770 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10771 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10772 | ` * Refer to the official for more information on this powerful` |
|        - | 10773 | ` * extension.` |
|        - | 10774 | ` */` |
|  2953722 | 10775 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10776 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10777 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10778 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10779 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10780 | `	)` |
|        2 | 10781 |  |
|        - | 10782 | `	VmRefObj *pRef;` |
|        - | 10783 | `	sxu32 n;` |
|        - | 10784 | `	/* Check if the referenced object already exists */` |
|  2953724 | 10785 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2953724 | 10786 | `	if( pRef == 0 ){` |
|        - | 10787 | `		/* Not such entry */` |
|    75662 | 10788 | `		return SXERR_NOTFOUND;` |
|        - | 10789 | `	}` |
|        - | 10790 | `	/* Remove the desired entry */` |
|  2878064 | 10791 | `	if( pEntry ){` |
|        - | 10792 | `		SyHashEntry **apEntry;` |
|       56 | 10793 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10794 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10795 | `			if( apEntry[n] == pEntry ){` |
|        - | 10796 | `				/* Nullify the entry */` |
|       56 | 10797 | `				apEntry[n] = 0;` |
|        - | 10798 | `				/*` |
|        - | 10799 | `				 * NOTE:` |
|        - | 10800 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10801 | `				 * we avoid wasting spaces.` |
|        - | 10802 | `				 */` |
|       27 | 10803 | `			}` |
|       79 | 10804 | `		}` |
|       27 | 10805 | `	}` |
|  2878064 | 10806 | `	if( pMapEntry ){` |
|        - | 10807 | `		ph7_hashmap_node **apNode;` |
|  2878010 | 10808 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5756106 | 10809 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2878098 | 10810 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10811 | `				/* nullify the entry */` |
|  2878010 | 10812 | `				apNode[n] = 0;` |
|  1439004 | 10813 | `			}` |
|  1439050 | 10814 | `		}` |
|  1439004 | 10815 | `	}` |
|  2878064 | 10816 | `	return SXRET_OK;` |
|  1476863 | 10817 |  |
|        - | 10818 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10819 | `/*` |
|        - | 10820 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10821 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10822 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10823 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10824 | ` * For more information on how to register IO stream devices,please` |
|        - | 10825 | ` * refer to the official documentation.` |
|        - | 10826 | ` */` |
|    23024 | 10827 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10828 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10829 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10830 | `	int nByte              /* *pzDevice length*/` |
|        - | 10831 | `	)` |
|        2 | 10832 |  |
|        - | 10833 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10834 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10835 | `	SyString sDev,sCur;` |
|        - | 10836 | `	sxu32 n,nEntry;` |
|        - | 10837 | `	int rc;` |
|        - | 10838 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23026 | 10839 | `	zNext = zCur = zIn = *pzDevice;` |
|    23026 | 10840 | `	zEnd = &zIn[nByte];` |
|  1473393 | 10841 | `	while( zIn < zEnd ){` |
|  1450371 | 10842 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10843 | `			/* Got one */` |
|        3 | 10844 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10845 | `			break;` |
|        - | 10846 | `		}` |
|        - | 10847 | `		/* Advance the cursor */` |
|  1450369 | 10848 | `		zIn++;` |
|        2 | 10849 | `	}` |
|    23026 | 10850 | `	if( zIn >= zEnd ){` |
|        - | 10851 | `		/* No such scheme,return the default stream */` |
|    23024 | 10852 | `		return pVm->pDefStream;` |
|        - | 10853 | `	}` |
|        3 | 10854 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10855 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10856 | `	SyStringFullTrim(&sDev);` |
|        - | 10857 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10858 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10859 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10860 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10861 | `		pStream = apStream[n];` |
|        3 | 10862 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10863 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10864 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10865 | `		if( rc == 0 ){` |
|        - | 10866 | `			/* Stream device found */` |
|        3 | 10867 | `			*pzDevice = zNext;` |
|        3 | 10868 | `			return pStream;` |
|        - | 10869 | `		}` |
|      ! 0 | 10870 | `	}` |
|        - | 10871 | `	/* No such stream,return NULL */` |
|      ! 0 | 10872 | `	return 0;` |
|    11514 | 10873 |  |
|        - | 10874 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10875 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10876 |  |
