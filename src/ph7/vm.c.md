# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4709/6285 lines (74.92%)

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
|   780498 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   780500 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   780470 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   780462 |    94 | `	return FALSE;` |
|   390273 |    95 |  |
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
|   545990 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   545992 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   545992 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   545988 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   545988 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   545988 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   545988 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   545988 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   545988 |   142 | `	pCons->xExpand = xExpand;` |
|   545988 |   143 | `	pCons->pUserData = pUserData;` |
|   545988 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   545988 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   545988 |   151 | `	return SXRET_OK;` |
|   272997 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|  1184944 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1184946 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1184946 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|  1184946 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1184946 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|  1184946 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|  1184946 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1184946 |   185 | `	pFunc->pVm   = pVm;` |
|  1184946 |   186 | `	pFunc->xFunc = xFunc;` |
|  1184946 |   187 | `	pFunc->pUserData = pUserData;` |
|  1184946 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|  1184946 |   190 | `	*ppOut = pFunc;` |
|  1184946 |   191 | `	return SXRET_OK;` |
|   592474 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|  1187460 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1187462 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1187462 |   213 | `	if( pEntry ){` |
|     2518 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2518 |   215 | `		pFunc->pUserData = pUserData;` |
|     2518 |   216 | `		pFunc->xFunc = xFunc;` |
|     2518 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2518 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|  1184946 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1184946 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|  1184946 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1184946 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|  1184946 |   233 | `	return SXRET_OK;` |
|   593732 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   170318 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   170320 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   170320 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   170320 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   170320 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   170320 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   170320 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   170320 |   260 | `	pFunc->iFlags = iFlags;` |
|   170320 |   261 | `	pFunc->pUserData = pUserData;` |
|   170320 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   170320 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   576842 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   576844 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    36638 |   283 | `		pName = &pFunc->sName;` |
|    18318 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   576844 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   576844 |   287 | `	if( pEntry ){` |
|   428994 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   428994 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   428994 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|   147852 |   297 | `	pFunc->pNextName = 0;` |
|   147852 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   147852 |   299 | `	return rc;` |
|   288423 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    42098 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    42100 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    42100 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    42100 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    42070 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    42070 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    42070 |   324 | `	return rc;` |
|    21051 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  3421032 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3421034 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  3421034 |   342 | `	sInstr.iP1 = iP1;` |
|  3421034 |   343 | `	sInstr.iP2 = iP2;` |
|  3421034 |   344 | `	sInstr.p3  = p3;` |
|  3421034 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   197702 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    98850 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  3421034 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3421034 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  3421034 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   407848 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   407850 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   407850 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   407850 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   203924 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   203926 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   194848 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   194850 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   194850 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|  1028694 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|  1028696 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   185346 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   185348 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   664512 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   664514 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    28460 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    28462 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    28462 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    28462 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    28462 |   417 | `	return &aInstr[n - 2];` |
|    14232 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    16140 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    16142 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16142 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    16142 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    16142 |   437 | `	pFrame->pUserData = pUserData;` |
|    16142 |   438 | `	pFrame->pThis = pThis;` |
|    16142 |   439 | `	pFrame->pVm = pVm;` |
|    16142 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16142 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16142 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16142 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16142 |   444 | `	return pFrame;` |
|     8072 |   445 |  |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   448 | `/*` |
|        - |   449 | ` * Enter a VM frame.` |
|        - |   450 | ` */` |
|    16098 |   451 | `static sxi32 VmEnterFrame(` |
|        - |   452 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   453 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   454 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   455 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   456 | `	)` |
|        2 |   457 |  |
|        - |   458 | `	VmFrame *pFrame;` |
|        - |   459 | `	/* Allocate a new frame */` |
|    16100 |   460 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    16100 |   461 | `	if( pFrame == 0 ){` |
|      ! 0 |   462 | `		return SXERR_MEM;` |
|        - |   463 | `	}` |
|        - |   464 | `	/* Link to the list of active VM frame */` |
|    16100 |   465 | `	pFrame->pParent = pVm->pFrame;` |
|    16100 |   466 | `	pVm->pFrame = pFrame;` |
|    16100 |   467 | `	if( ppFrame ){` |
|        - |   468 | `		/* Write a pointer to the new VM frame */` |
|    13324 |   469 | `		*ppFrame = pFrame;` |
|     6661 |   470 | `	}` |
|    16100 |   471 | `	return SXRET_OK;` |
|     8051 |   472 |  |
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
|    13322 |   516 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   517 |  |
|    13324 |   518 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13324 |   519 | `	if( pCurFrame ){` |
|        - |   520 | `		/* Unlink from the list of active VM frame */` |
|    13324 |   521 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13324 |   522 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   523 | `			VmSlot  *aSlot;` |
|        - |   524 | `			sxu32 n;` |
|        - |   525 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13260 |   526 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    92474 |   527 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   528 | `				/* Unset the local variable */` |
|    79216 |   529 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    39609 |   530 | `			}` |
|        - |   531 | `			/* Remove local reference */` |
|    13260 |   532 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    92530 |   533 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    79272 |   534 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    39637 |   535 | `			}` |
|     6629 |   536 | `		}` |
|        - |   537 | `		/* Release internal containers */` |
|    13324 |   538 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13324 |   539 | `		SySetRelease(&pCurFrame->sArg);` |
|    13324 |   540 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13324 |   541 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   542 | `		/* Release the whole structure */` |
|    13324 |   543 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6661 |   544 | `	}` |
|    13324 |   545 |  |
|        - |   546 | `/*` |
|        - |   547 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   548 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   549 | ` * should be skipped when looking for the real execution context.` |
|        - |   550 | ` */` |
|  6307826 |   551 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   552 |  |
|  6308104 |   553 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   554 | `		pFrame = pFrame->pParent;` |
|        2 |   555 | `	}` |
|  6307828 |   556 | `	return pFrame;` |
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
|   112150 |   674 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   675 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   676 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   677 | `	)` |
|        2 |   678 |  |
|        - |   679 | `	ph7_class_method *pMeth;` |
|        - |   680 | `	ph7_class_attr *pAttr;` |
|        - |   681 | `	SyHashEntry *pEntry;` |
|        - |   682 | `	sxi32 rc;` |
|        - |   683 | `	/* Reset the loop cursor */` |
|   112152 |   684 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   685 | `	/* Process only static and constant attribute */` |
|   460803 |   686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   687 | `		/* Extract the current attribute */` |
|   292578 |   688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   292578 |   689 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   112152 |   711 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   712 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   713 | `		 */` |
|    48322 |   714 | `		return SXRET_OK;` |
|        - |   715 | `	}` |
|        - |   716 | `	/* Create constructor alias if not yet done */` |
|    63832 |   717 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   718 | `		/* User constructor with the same base class name */` |
|     5578 |   719 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5578 |   720 | `		if( pEntry ){` |
|      ! 0 |   721 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   722 | `			/* Create the alias */` |
|      ! 0 |   723 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   724 | `		}` |
|     2788 |   725 | `	}` |
|        - |   726 | `	/* Install the methods now */` |
|    63832 |   727 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   635959 |   728 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   540214 |   729 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   540214 |   730 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   540208 |   731 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   540208 |   732 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   733 | `				return rc;` |
|        - |   734 | `			}` |
|   270103 |   735 | `		}` |
|        2 |   736 | `	}` |
|        - |   737 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    63832 |   738 | `	pClass->bMounted = TRUE;` |
|    63832 |   739 | `	return SXRET_OK;` |
|    56077 |   740 |  |
|        - |   741 | `/*` |
|        - |   742 | ` * Allocate a private frame for attributes of the given` |
|        - |   743 | ` * class instance (Object in the PHP jargon).` |
|        - |   744 | ` */` |
|     1184 |   745 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   747 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   748 | `	)` |
|        2 |   749 |  |
|     1186 |   750 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   751 | `	ph7_class_attr *pAttr;` |
|        - |   752 | `	SyHashEntry *pEntry;` |
|        - |   753 | `	sxi32 rc;` |
|        - |   754 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1186 |   755 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4874 |   756 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
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
|     1186 |   800 | `	return SXRET_OK;` |
|      594 |   801 |  |
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
|   390500 |   813 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   814 |  |
|        - |   815 | `	ph7_value *pObj;` |
|        - |   816 | `	sxi32 rc;` |
|   390502 |   817 | `	if( pIndex ){` |
|        - |   818 | `		/* Object index in the object table */` |
|   382174 |   819 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   191086 |   820 | `	}` |
|        - |   821 | `	/* Reserve a slot for the new object */` |
|   390502 |   822 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   390502 |   823 | `	if( rc != SXRET_OK ){` |
|        - |   824 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   825 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   826 | `		 */` |
|      ! 0 |   827 | `		return 0;` |
|        - |   828 | `	}` |
|   390502 |   829 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   390502 |   830 | `	return pObj;` |
|   195252 |   831 |  |
|        - |   832 | `/*` |
|        - |   833 | ` * Reserve a memory object.` |
|        - |   834 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   835 | ` */` |
|  2145008 |   836 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   837 |  |
|        - |   838 | `	ph7_value *pObj;` |
|        - |   839 | `	sxi32 rc;` |
|  2145010 |   840 | `	if( pIndex ){` |
|        - |   841 | `		/* Object index in the object table */` |
|  2145010 |   842 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1072504 |   843 | `	}` |
|        - |   844 | `	/* Reserve a slot for the new object */` |
|  2145010 |   845 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2145010 |   846 | `	if( rc != SXRET_OK ){` |
|        - |   847 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   848 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   849 | `		 */` |
|      ! 0 |   850 | `		return 0;` |
|        - |   851 | `	}` |
|  2145010 |   852 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2145010 |   853 | `	return pObj;` |
|  1072506 |   854 |  |
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
|     2776 |  1262 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1263 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1264 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1265 | `	 )` |
|        2 |  1266 |  |
|        - |  1267 | `	SyString sBuiltin;` |
|        - |  1268 | `	ph7_value *pObj;` |
|        - |  1269 | `	sxi32 rc;` |
|        - |  1270 | `	/* Zero the structure */` |
|     2778 |  1271 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1272 | `	/* Initialize VM fields */` |
|     2778 |  1273 | `	pVm->pEngine = &(*pEngine);` |
|     2778 |  1274 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1275 | `	/* Instructions containers */` |
|     2778 |  1276 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2778 |  1277 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2778 |  1278 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1279 | `	/* Object containers */` |
|     2778 |  1280 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2778 |  1281 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1282 | `	/* Virtual machine internal containers */` |
|     2778 |  1283 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2778 |  1284 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2778 |  1285 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2778 |  1286 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2778 |  1287 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2778 |  1288 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2778 |  1289 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2778 |  1290 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2778 |  1291 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2778 |  1292 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2778 |  1293 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2778 |  1294 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2778 |  1295 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2778 |  1296 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2778 |  1297 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2778 |  1298 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2778 |  1299 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2778 |  1300 | `	pVm->pPendingException = 0;` |
|        - |  1301 | `	/* Configuration containers */` |
|     2778 |  1302 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2778 |  1303 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2778 |  1304 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2778 |  1305 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2778 |  1306 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2778 |  1307 | `	pVm->iResponseStatus = 200;` |
|     2778 |  1308 | `	pVm->bHeadersSent = 0;` |
|     2778 |  1309 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1310 | `	/* Error callbacks containers */` |
|     2778 |  1311 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2778 |  1312 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2778 |  1313 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2778 |  1314 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2778 |  1315 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1316 | `	/* Set a default recursion limit */` |
|        - |  1317 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2778 |  1318 | `	pVm->nMaxDepth = 32;` |
|        - |  1319 | `#else` |
|        - |  1320 | `	pVm->nMaxDepth = 16;` |
|        - |  1321 | `#endif` |
|        - |  1322 | `	/* Default assertion flags */` |
|     2778 |  1323 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1324 | `	/* JSON return status */` |
|     2778 |  1325 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1326 | `	/* PRNG context */` |
|     2778 |  1327 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1328 | `	/* Install the null constant */` |
|     2778 |  1329 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2778 |  1330 | `	if( pObj == 0 ){` |
|      ! 0 |  1331 | `		rc = SXERR_MEM;` |
|      ! 0 |  1332 | `		goto Err;` |
|        - |  1333 | `	}` |
|     2778 |  1334 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1335 | `	/* Install the boolean TRUE constant */` |
|     2778 |  1336 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2778 |  1337 | `	if( pObj == 0 ){` |
|      ! 0 |  1338 | `		rc = SXERR_MEM;` |
|      ! 0 |  1339 | `		goto Err;` |
|        - |  1340 | `	}` |
|     2778 |  1341 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1342 | `	/* Install the boolean FALSE constant */` |
|     2778 |  1343 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2778 |  1344 | `	if( pObj == 0 ){` |
|      ! 0 |  1345 | `		rc = SXERR_MEM;` |
|      ! 0 |  1346 | `		goto Err;` |
|        - |  1347 | `	}` |
|     2778 |  1348 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1349 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1350 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1351 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2778 |  1352 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2778 |  1353 | `	if( pObj == 0 ){` |
|      ! 0 |  1354 | `		rc = SXERR_MEM;` |
|      ! 0 |  1355 | `		goto Err;` |
|        - |  1356 | `	}` |
|     2778 |  1357 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1358 | `	/* Create the global frame */` |
|     2778 |  1359 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2778 |  1360 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1361 | `		goto Err;` |
|        - |  1362 | `	}` |
|        - |  1363 | `	/* Initialize the code generator */` |
|     2778 |  1364 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2778 |  1365 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1366 | `		goto Err;` |
|        - |  1367 | `	}` |
|        - |  1368 | `	/* VM correctly initialized,set the magic number */` |
|     2778 |  1369 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2778 |  1370 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1371 | `	/* Compile the built-in library */` |
|     2778 |  1372 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1373 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2778 |  1374 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1375 | `	/* Register Fiber internal C functions */` |
|     2778 |  1376 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2778 |  1377 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2778 |  1378 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2778 |  1379 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2778 |  1380 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2778 |  1381 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2778 |  1382 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2778 |  1383 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2778 |  1384 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2778 |  1385 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1386 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2778 |  1387 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2778 |  1388 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2778 |  1389 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2778 |  1390 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2778 |  1391 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2778 |  1392 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2778 |  1393 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2778 |  1394 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2778 |  1395 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2778 |  1396 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1397 | `	/* Reset the code generator */` |
|     2778 |  1398 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2778 |  1399 | `	return SXRET_OK;` |
|      ! 0 |  1400 | `Err:` |
|      ! 0 |  1401 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1402 | `	return rc;` |
|     1390 |  1403 |  |
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
|    13482 |  1430 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1431 |  |
|    13484 |  1432 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    13484 |  1433 | `	if( xCons != VmObConsumer ){` |
|     6572 |  1434 | `		pVm->nOutputLen += nLen;` |
|     6572 |  1435 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      916 |  1436 | `			pVm->bHeadersSent = 1;` |
|      457 |  1437 | `		}` |
|     3285 |  1438 | `	}` |
|    13484 |  1439 |  |
|        - |  1440 | `#define VM_STACK_GUARD 16` |
|        - |  1441 | `/*` |
|        - |  1442 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1443 | ` * our compiled PHP program.` |
|        - |  1444 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1445 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1446 | ` */` |
|    32710 |  1447 | `static ph7_value * VmNewOperandStack(` |
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
|    32712 |  1460 | `	nInstr += VM_STACK_GUARD;` |
|    32712 |  1461 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    32712 |  1462 | `	if( pStack == 0 ){` |
|      ! 0 |  1463 | `		return 0;` |
|        - |  1464 | `	}` |
|        - |  1465 | `	/* Initialize the operand stack */` |
|  2049688 |  1466 | `	while( nInstr > 0 ){` |
|  2016978 |  1467 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2016978 |  1468 | `		--nInstr;` |
|        2 |  1469 | `	}` |
|        - |  1470 | `	/* Ready for bytecode execution */` |
|    32712 |  1471 | `	return pStack;` |
|    16357 |  1472 |  |
|        - |  1473 | `/* Forward declaration */` |
|        - |  1474 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1475 | `/*` |
|        - |  1476 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1477 | ` * This routine gets called by the PH7 engine after` |
|        - |  1478 | ` * successful compilation of the target PHP program.` |
|        - |  1479 | ` */` |
|     2516 |  1480 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1481 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1482 | `	)` |
|        2 |  1483 |  |
|        - |  1484 | `	SyHashEntry *pEntry;` |
|        - |  1485 | `	sxi32 rc;` |
|     2518 |  1486 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1487 | `		/* Initialize your VM first */` |
|      ! 0 |  1488 | `		return SXERR_CORRUPT;` |
|        - |  1489 | `	}` |
|        - |  1490 | `	/* Mark the VM ready for byte-code execution */` |
|     2518 |  1491 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1492 | `	/* Release the code generator now we have compiled our program */` |
|     2518 |  1493 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1494 | `	/* Emit the DONE instruction */` |
|     2518 |  1495 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2518 |  1496 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1497 | `		return SXERR_MEM;` |
|        - |  1498 | `	}` |
|        - |  1499 | `	/* Script return value */` |
|     2518 |  1500 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1501 | `	/* Allocate a new operand stack */` |
|     2518 |  1502 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2518 |  1503 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1504 | `		return SXERR_MEM;` |
|        - |  1505 | `	}` |
|        - |  1506 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1507 | `	 * private data. */` |
|     2518 |  1508 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2518 |  1509 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1510 | `	/* Allocate the reference table */` |
|     2518 |  1511 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2518 |  1512 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2518 |  1513 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1514 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1515 | `		return SXERR_MEM;` |
|        - |  1516 | `	}` |
|        - |  1517 | `	/* Zero the reference table */` |
|     2518 |  1518 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1519 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2518 |  1520 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2518 |  1521 | `	if( rc != SXRET_OK ){` |
|        - |  1522 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1523 | `		return rc;` |
|        - |  1524 | `	}` |
|        - |  1525 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2518 |  1526 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2518 |  1527 | `	if( rc != SXRET_OK ){` |
|        - |  1528 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1529 | `		return rc;` |
|        - |  1530 | `	}` |
|        - |  1531 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2518 |  1532 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1533 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2518 |  1534 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1535 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2518 |  1536 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1537 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1538 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2518 |  1539 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2518 |  1540 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1541 | `#endif` |
|        - |  1542 | `	/* Initialize and install static and constants class attributes */` |
|     2518 |  1543 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    40422 |  1544 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    37906 |  1545 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    37906 |  1546 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1547 | `			return rc;` |
|        - |  1548 | `		}` |
|        2 |  1549 | `	}` |
|        - |  1550 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2518 |  1551 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1552 | `	/* VM is ready for bytecode execution */` |
|     2518 |  1553 | `	return SXRET_OK;` |
|     1260 |  1554 |  |
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
|     2508 |  1579 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1580 |  |
|        - |  1581 | `	/* Set the stale magic number */` |
|     2510 |  1582 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1583 | `	/* Release the private memory subsystem */` |
|     2510 |  1584 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2510 |  1585 | `	return SXRET_OK;` |
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
|   568578 |  1597 | `static sxi32 VmInitCallContext(` |
|        - |  1598 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1599 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1600 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1601 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1602 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1603 | `	)` |
|        2 |  1604 |  |
|   568580 |  1605 | `	pOut->pFunc = pFunc;` |
|   568580 |  1606 | `	pOut->pVm   = pVm;` |
|   568580 |  1607 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   568580 |  1608 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1609 | `	/* Assume a null return value */` |
|   568580 |  1610 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   568580 |  1611 | `	pOut->pRet = pRet;` |
|   568580 |  1612 | `	pOut->iFlags = iFlags;` |
|   568580 |  1613 | `	return SXRET_OK;` |
|        2 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1617 | ` * left behind.` |
|        - |  1618 | ` */` |
|   568578 |  1619 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1620 |  |
|        - |  1621 | `	sxu32 n;` |
|   568580 |  1622 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6914 |  1623 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19732 |  1624 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12820 |  1625 | `			if( apObj[n] == 0 ){` |
|        - |  1626 | `				/* Already released */` |
|      298 |  1627 | `				continue;` |
|        - |  1628 | `			}` |
|    12524 |  1629 | `			PH7_MemObjRelease(apObj[n]);` |
|    12524 |  1630 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6263 |  1631 | `		}` |
|     6914 |  1632 | `		SySetRelease(&pCtx->sVar);` |
|     3456 |  1633 | `	}` |
|   568580 |  1634 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   568580 |  1650 |  |
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
|  3312482 |  1681 | `static void VmPopOperand(` |
|        - |  1682 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1683 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1684 | `	)` |
|        2 |  1685 |  |
|  3312484 |  1686 | `	ph7_value *pTos = *ppTos;` |
|  7037284 |  1687 | `	while( nPop > 0 ){` |
|  3724802 |  1688 | `		PH7_MemObjRelease(pTos);` |
|  3724802 |  1689 | `		pTos--;` |
|  3724802 |  1690 | `		nPop--;` |
|        2 |  1691 | `	}` |
|        - |  1692 | `	/* Top of the stack */` |
|  3312484 |  1693 | `	*ppTos = pTos;` |
|  3312484 |  1694 |  |
|        - |  1695 | `/*` |
|        - |  1696 | ` * Reserve a memory object.` |
|        - |  1697 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1698 | ` */` |
|  3022880 |  1699 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1700 |  |
|  3022882 |  1701 | `	ph7_value *pObj = 0;` |
|        - |  1702 | `	VmSlot *pSlot;` |
|        - |  1703 | `	sxu32 nIdx;` |
|        - |  1704 | `	/* Check for a free slot */` |
|  3022882 |  1705 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3022882 |  1706 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3022882 |  1707 | `	if( pSlot ){` |
|   877874 |  1708 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   877874 |  1709 | `		nIdx = pSlot->nIdx;` |
|   438936 |  1710 | `	}` |
|  3022882 |  1711 | `	if( pObj == 0 ){` |
|        - |  1712 | `		/* Reserve a new memory object */` |
|  2145010 |  1713 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2145010 |  1714 | `		if( pObj == 0 ){` |
|      ! 0 |  1715 | `			return 0;` |
|        - |  1716 | `		}` |
|  1072504 |  1717 | `	}` |
|        - |  1718 | `	/* Set a null default value */` |
|  3022882 |  1719 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3022882 |  1720 | `	pObj->nIdx = nIdx;` |
|  3022882 |  1721 | `	return pObj;` |
|  1511442 |  1722 |  |
|        - |  1723 | `/*` |
|        - |  1724 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1725 | ` */` |
|    32080 |  1726 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1727 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1728 | `	const char *zKey,  /* Entry key */` |
|        - |  1729 | `	sxu32 nByte,       /* Key length */` |
|        - |  1730 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1731 | `	)` |
|        2 |  1732 |  |
|        - |  1733 | `	ph7_value sKey;` |
|        - |  1734 | `	sxi32 rc;` |
|    32082 |  1735 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    32082 |  1736 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1737 | `	/* Perform the insertion */` |
|    32082 |  1738 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    32082 |  1739 | `	PH7_MemObjRelease(&sKey);` |
|    32082 |  1740 | `	return rc;` |
|        2 |  1741 |  |
|        - |  1742 | `/*` |
|        - |  1743 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1744 | ` * Return a pointer to the variable value on success.` |
|        - |  1745 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1746 | ` */` |
|  3091920 |  1747 | `static ph7_value * VmExtractMemObj(` |
|        - |  1748 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1749 | `	const SyString *pName, /* Variable name */` |
|        - |  1750 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1751 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1752 | `	)` |
|        2 |  1753 |  |
|  3091922 |  1754 | `	int bNullify = FALSE;` |
|        - |  1755 | `	SyHashEntry *pEntry;` |
|        - |  1756 | `	VmFrame *pFrame;` |
|        - |  1757 | `	ph7_value *pObj;` |
|        - |  1758 | `	sxu32 nIdx;` |
|        - |  1759 | `	sxi32 rc;` |
|        - |  1760 | `	/* Point to the top active frame */` |
|  3091922 |  1761 | `	pFrame = pVm->pFrame;` |
|  3091922 |  1762 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1763 | `	/* Perform the lookup */` |
|  3091922 |  1764 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1765 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1766 | `		pName = &sAnnon;` |
|        - |  1767 | `		/* Always nullify the object */` |
|      ! 0 |  1768 | `		bNullify = TRUE;` |
|      ! 0 |  1769 | `		bDup = FALSE;` |
|      ! 0 |  1770 | `	}` |
|        - |  1771 | `	/* Check the superglobals table first */` |
|  3091922 |  1772 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3091922 |  1773 | `	if( pEntry == 0 ){` |
|        - |  1774 | `		/* Query the top active frame */` |
|  3091882 |  1775 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3091882 |  1776 | `		if( pEntry == 0 ){` |
|    86182 |  1777 | `			char *zName = (char *)pName->zString;` |
|        - |  1778 | `			VmSlot sLocal;` |
|    86182 |  1779 | `			if( !bCreate ){` |
|        - |  1780 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1781 | `				return 0;` |
|        - |  1782 | `			}` |
|        - |  1783 | `			/* No such variable,automatically create a new one and install` |
|        - |  1784 | `			 * it in the current frame.` |
|        - |  1785 | `			 */` |
|    86146 |  1786 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    86146 |  1787 | `			if( pObj == 0 ){` |
|      ! 0 |  1788 | `				return 0;` |
|        - |  1789 | `			}` |
|    86146 |  1790 | `			nIdx = pObj->nIdx;` |
|    86146 |  1791 | `			if( bDup ){` |
|        - |  1792 | `				/* Duplicate name */` |
|      168 |  1793 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1794 | `				if( zName == 0 ){` |
|      ! 0 |  1795 | `					return 0;` |
|        - |  1796 | `				}` |
|       83 |  1797 | `			}` |
|        - |  1798 | `			/* Link to the top active VM frame */` |
|    86146 |  1799 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    86146 |  1800 | `			if( rc != SXRET_OK ){` |
|        - |  1801 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1802 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1803 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1804 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1805 | `				return 0;` |
|        - |  1806 | `			}` |
|    86146 |  1807 | `			if( pFrame->pParent != 0 ){` |
|        - |  1808 | `				/* Local variable */` |
|    79252 |  1809 | `				sLocal.nIdx = nIdx;` |
|    79252 |  1810 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    39627 |  1811 | `			}else{` |
|        - |  1812 | `				/* Register in the $GLOBALS array */` |
|     6896 |  1813 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1814 | `			}` |
|        - |  1815 | `			/* Install in the reference table */` |
|    86146 |  1816 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1817 | `			/* Save object index */` |
|    86146 |  1818 | `			pObj->nIdx = nIdx;` |
|    43074 |  1819 | `		}else{` |
|        - |  1820 | `			/* Extract variable contents */` |
|  3005702 |  1821 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3005702 |  1822 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3005702 |  1823 | `			if( bNullify && pObj ){` |
|      ! 0 |  1824 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1825 | `			}` |
|        - |  1826 | `		}` |
|  1546034 |  1827 | `	}else{` |
|        - |  1828 | `		/* Superglobal */` |
|       42 |  1829 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1830 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1831 | `	}` |
|  3091886 |  1832 | `	return pObj;` |
|  1546072 |  1833 |  |
|        - |  1834 | `/*` |
|        - |  1835 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1836 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1837 | ` */` |
|     2820 |  1838 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1839 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1840 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1841 | `	sxu32 nByte        /* zName length */` |
|        - |  1842 | `	)` |
|        2 |  1843 |  |
|        - |  1844 | `	SyHashEntry *pEntry;` |
|        - |  1845 | `	ph7_value *pValue;` |
|        - |  1846 | `	sxu32 nIdx;` |
|        - |  1847 | `	/* Query the superglobal table */` |
|     2822 |  1848 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2822 |  1849 | `	if( pEntry == 0 ){` |
|        - |  1850 | `		/* No such entry */` |
|      ! 0 |  1851 | `		return 0;` |
|        - |  1852 | `	}` |
|        - |  1853 | `	/* Extract the superglobal index in the global object pool */` |
|     2822 |  1854 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1855 | `	/* Extract the variable value  */` |
|     2822 |  1856 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2822 |  1857 | `	return pValue;` |
|     1412 |  1858 |  |
|        - |  1859 | `/*` |
|        - |  1860 | ` * Perform a raw hashmap insertion.` |
|        - |  1861 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1862 | ` */` |
|     2850 |  1863 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1864 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1865 | `	const char *zKey,   /* Entry key */` |
|        - |  1866 | `	int nKeylen,        /* zKey length*/` |
|        - |  1867 | `	const char *zData,  /* Entry data */` |
|        - |  1868 | `	int nLen            /* zData length */` |
|        - |  1869 | `	)` |
|        2 |  1870 |  |
|        - |  1871 | `	ph7_value sKey,sValue;` |
|        - |  1872 | `	sxi32 rc;` |
|     2852 |  1873 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2852 |  1874 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2852 |  1875 | `	if( zKey ){` |
|     2830 |  1876 | `		if( nKeylen < 0 ){` |
|     2778 |  1877 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1388 |  1878 | `		}` |
|     2830 |  1879 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1414 |  1880 | `	}` |
|     2852 |  1881 | `	if( zData ){` |
|     2852 |  1882 | `		if( nLen < 0 ){` |
|        - |  1883 | `			/* Compute length automatically */` |
|      144 |  1884 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1885 | `		}` |
|     2852 |  1886 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1425 |  1887 | `	}` |
|        - |  1888 | `	/* Perform the insertion */` |
|     2852 |  1889 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2852 |  1890 | `	PH7_MemObjRelease(&sKey);` |
|     2852 |  1891 | `	PH7_MemObjRelease(&sValue);` |
|     2852 |  1892 | `	return rc;` |
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
|    40586 |  1907 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1908 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1909 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1910 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1911 | `	)` |
|        2 |  1912 |  |
|    40588 |  1913 | `	sxi32 rc = SXRET_OK;` |
|    40588 |  1914 | `	switch(nOp){` |
|     1250 |  1915 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2502 |  1916 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2502 |  1917 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1918 | `		/* VM output consumer callback */` |
|        - |  1919 | `#ifdef UNTRUST` |
|        - |  1920 | `		if( xConsumer == 0 ){` |
|        - |  1921 | `			rc = SXERR_CORRUPT;` |
|        - |  1922 | `			break;` |
|        - |  1923 | `		}` |
|        - |  1924 | `#endif` |
|        - |  1925 | `		/* Install the output consumer */` |
|     2502 |  1926 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2502 |  1927 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2502 |  1928 | `		break;` |
|        - |  1929 | `							   }` |
|     1258 |  1930 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1931 | `		/* Import path */` |
|        - |  1932 | `		  const char *zPath;` |
|        - |  1933 | `		  SyString sPath;` |
|     2518 |  1934 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1935 | `#if defined(UNTRUST)` |
|        - |  1936 | `		  if( zPath == 0 ){` |
|        - |  1937 | `			  rc = SXERR_EMPTY;` |
|        - |  1938 | `			  break;` |
|        - |  1939 | `		  }` |
|        - |  1940 | `#endif` |
|     2518 |  1941 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1942 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1943 | `#ifdef __WINNT__` |
|        2 |  1944 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1945 | `#endif` |
|     5034 |  1946 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1947 | `		  /* Remove leading and trailing white spaces */` |
|     2518 |  1948 | `		  SyStringFullTrim(&sPath);` |
|     2518 |  1949 | `		  if( sPath.nByte > 0 ){` |
|        - |  1950 | `			  /* Store the path in the corresponding conatiner */` |
|     2518 |  1951 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1258 |  1952 | `		  }` |
|     2518 |  1953 | `		  break;` |
|        - |  1954 | `									 }` |
|     1258 |  1955 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1956 | `		/* Run-Time Error report */` |
|     2518 |  1957 | `		pVm->bErrReport = 1;` |
|     2518 |  1958 | `		break;` |
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
|    12580 |  1980 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1981 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1982 | `		/* Create a new superglobal/global variable */` |
|    25162 |  1983 | `		const char *zName = va_arg(ap,const char *);` |
|    25162 |  1984 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    25162 |  1995 | `		nByte = SyStrlen(zName);` |
|    25162 |  1996 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1997 | `			/* Check if the superglobal is already installed */` |
|    25162 |  1998 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12582 |  1999 | `		}else{` |
|        - |  2000 | `			/* Query the top active VM frame */` |
|      ! 0 |  2001 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2002 | `		}` |
|    25162 |  2003 | `		if( pEntry ){` |
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
|    25162 |  2014 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    25162 |  2015 | `			if( pObj == 0 ){` |
|      ! 0 |  2016 | `				rc = SXERR_MEM;` |
|      ! 0 |  2017 | `				break;` |
|        - |  2018 | `			}` |
|    25162 |  2019 | `			nIdx = pObj->nIdx;` |
|        - |  2020 | `			/* Copy value */` |
|    25162 |  2021 | `			PH7_MemObjStore(pValue,pObj);` |
|    25162 |  2022 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2023 | `				/* Install the superglobal */` |
|    25162 |  2024 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12582 |  2025 | `			}else{` |
|        - |  2026 | `				/* Install in the current frame */` |
|      ! 0 |  2027 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2028 | `			}` |
|    25162 |  2029 | `			if( rc == SXRET_OK ){` |
|        - |  2030 | `				SyHashEntry *pRef;` |
|    25162 |  2031 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    25162 |  2032 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12582 |  2033 | `				}else{` |
|      ! 0 |  2034 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2035 | `				}` |
|        - |  2036 | `				/* Install in the reference table */` |
|    25162 |  2037 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    25162 |  2038 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2039 | `					/* Register in the $GLOBALS array */` |
|    25162 |  2040 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12580 |  2041 | `				}` |
|    12580 |  2042 | `			}` |
|        - |  2043 | `		}` |
|    25162 |  2044 | `		break;` |
|        - |  2045 | `									}` |
|     1388 |  2046 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2047 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2048 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2049 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2050 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2051 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2052 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2778 |  2053 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2778 |  2054 | `		const char *zValue = va_arg(ap,const char *);` |
|     2778 |  2055 | `		int nLen = va_arg(ap,int);` |
|        - |  2056 | `		ph7_hashmap *pMap;` |
|        - |  2057 | `		ph7_value *pValue;` |
|     2778 |  2058 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2059 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2060 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2777 |  2061 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2062 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2063 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2776 |  2064 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2065 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2066 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2776 |  2067 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2068 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2069 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2776 |  2070 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2071 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2072 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2776 |  2073 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2074 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2075 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2076 | `		}else{` |
|        - |  2077 | `			/* Extract the $_SERVER superglobal */` |
|     2776 |  2078 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2079 | `		}` |
|     2778 |  2080 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2081 | `			/* No such entry */` |
|      ! 0 |  2082 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2083 | `			break;` |
|        - |  2084 | `		}` |
|        - |  2085 | `		/* Point to the hashmap */` |
|     2778 |  2086 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2087 | `		/* Perform the insertion */` |
|     2778 |  2088 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2778 |  2089 | `		break;` |
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
|     2516 |  2140 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2141 | `		/* Register an IO stream device */` |
|     5034 |  2142 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2143 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7548 |  2144 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5034 |  2145 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2146 | `				/* Invalid stream */` |
|      ! 0 |  2147 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2148 | `				break;` |
|        - |  2149 | `		}` |
|     5034 |  2150 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2151 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2518 |  2152 | `			pVm->pDefStream = pStream;` |
|     1258 |  2153 | `		}` |
|        - |  2154 | `		/* Insert in the appropriate container */` |
|     5034 |  2155 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5034 |  2156 | `		break;` |
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
|    40588 |  2224 | `	return rc;` |
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
|    32796 |  2714 | `static sxi32 VmByteCodeExec(` |
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
|    32798 |  2732 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    32798 |  2733 | `	if( nTos < 0 ){` |
|    30778 |  2734 | `		pTos = &pStack[-1];` |
|    15390 |  2735 | `	}else{` |
|     2022 |  2736 | `		pTos = &pStack[nTos];` |
|        - |  2737 | `	}` |
|    32798 |  2738 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    32798 |  2739 | `	pc = nPc;` |
|        - |  2740 | `	/* Execute as much as we can */` |
|  4956778 |  2741 | `	for(;;){` |
|        - |  2742 | `		/* Fetch the instruction to execute */` |
|  9912854 |  2743 | `		pInstr = &aInstr[pc];` |
|  9912854 |  2744 | `		rc = SXRET_OK;` |
|        - |  2745 | `/*` |
|        - |  2746 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2747 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2748 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2749 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2750 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2751 | ` */` |
|  9912854 |  2752 | `		switch(pInstr->iOp){` |
|        - |  2753 | `/*` |
|        - |  2754 | ` * DONE: P1 * *` |
|        - |  2755 | ` *` |
|        - |  2756 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2757 | ` * and return immediately.` |
|        - |  2758 | ` */` |
|    16087 |  2759 | `case PH7_OP_DONE:` |
|    32176 |  2760 | `	if( pInstr->iP1 ){` |
|        - |  2761 | `#ifdef UNTRUST` |
|        - |  2762 | `		if( pTos < pStack ){` |
|        - |  2763 | `			goto Abort;` |
|        - |  2764 | `		}` |
|        - |  2765 | `#endif` |
|    18648 |  2766 | `		if( pLastRef ){` |
|    12190 |  2767 | `			*pLastRef = pTos->nIdx;` |
|     6094 |  2768 | `		}` |
|    18648 |  2769 | `		if( pResult ){` |
|        - |  2770 | `			/* Execution result */` |
|    17712 |  2771 | `			PH7_MemObjStore(pTos,pResult);` |
|     8855 |  2772 | `		}` |
|    18648 |  2773 | `		VmPopOperand(&pTos,1);` |
|    22853 |  2774 | `	}else if( pLastRef ){` |
|        - |  2775 | `		/* Nothing referenced */` |
|      988 |  2776 | `		*pLastRef = SXU32_HIGH;` |
|      493 |  2777 | `	}` |
|        - |  2778 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2779 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2780 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2781 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2782 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2783 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2784 | `	 * block can override it.` |
|        - |  2785 | `	 */` |
|    32178 |  2786 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
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
|    32176 |  2801 | `	goto Done;` |
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
|   213652 |  2846 | `case PH7_OP_JMP:` |
|   427350 |  2847 | `	pc = pInstr->iP2 - 1;` |
|   427350 |  2848 | `	break;` |
|        - |  2849 | `/*` |
|        - |  2850 | ` * JZ: P1 P2 *` |
|        - |  2851 | ` *` |
|        - |  2852 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2853 | ` * entry in the stack if P1 is zero.` |
|        - |  2854 | ` */` |
|   499154 |  2855 | `case PH7_OP_JZ:` |
|        - |  2856 | `#ifdef UNTRUST` |
|        - |  2857 | `	if( pTos < pStack ){` |
|        - |  2858 | `		goto Abort;` |
|        - |  2859 | `	}` |
|        - |  2860 | `#endif` |
|        - |  2861 | `	/* Get a boolean value */` |
|   998398 |  2862 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2863 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2864 | `	}` |
|   998398 |  2865 | `	if( !pTos->x.iVal ){` |
|        - |  2866 | `		/* Take the jump */` |
|   503996 |  2867 | `		pc = pInstr->iP2 - 1;` |
|   251997 |  2868 | `	}` |
|   998398 |  2869 | `	if( !pInstr->iP1 ){` |
|   795332 |  2870 | `		VmPopOperand(&pTos,1);` |
|   397687 |  2871 | `	}` |
|   998398 |  2872 | `	break;` |
|        - |  2873 | `/*` |
|        - |  2874 | ` * JNZ: P1 P2 *` |
|        - |  2875 | ` *` |
|        - |  2876 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2877 | ` * entry in the stack if P1 is zero.` |
|        - |  2878 | ` */` |
|    53423 |  2879 | `case PH7_OP_JNZ:` |
|        - |  2880 | `#ifdef UNTRUST` |
|        - |  2881 | `	if( pTos < pStack ){` |
|        - |  2882 | `		goto Abort;` |
|        - |  2883 | `	}` |
|        - |  2884 | `#endif` |
|        - |  2885 | `	/* Get a boolean value */` |
|   106848 |  2886 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2887 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2888 | `	}` |
|   106848 |  2889 | `	if( pTos->x.iVal ){` |
|        - |  2890 | `		/* Take the jump */` |
|     4450 |  2891 | `		pc = pInstr->iP2 - 1;` |
|     2224 |  2892 | `	}` |
|   106848 |  2893 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2894 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2895 | `	}` |
|   106848 |  2896 | `	break;` |
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
|   389996 |  2910 | `case PH7_OP_POP: {` |
|   780038 |  2911 | `	sxi32 n = pInstr->iP1;` |
|   780038 |  2912 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2913 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2914 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2915 | `	}` |
|   780038 |  2916 | `	VmPopOperand(&pTos,n);` |
|   780038 |  2917 | `	break;` |
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
|     6496 |  2940 | `case PH7_OP_NSSWITCH:` |
|    12994 |  2941 | `	SyBlobReset(&pVm->sNamespace);` |
|    12994 |  2942 | `	if( pInstr->p3 ){` |
|       51 |  2943 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2944 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2945 | `	}` |
|    12994 |  2946 | `	break;` |
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
|    12724 |  3078 | `case PH7_OP_ERR_CTRL:` |
|        - |  3079 | `	/*` |
|        - |  3080 | `	 * TICKET 1433-038:` |
|        - |  3081 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3082 | `	 * use the public API,to control error output.` |
|        - |  3083 | `	 */` |
|    25448 |  3084 | `	break;` |
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
|   829117 |  3144 | `case PH7_OP_LOADC: {` |
|        - |  3145 | `	ph7_value *pObj;` |
|        - |  3146 | `	/* Reserve a room */` |
|  1658280 |  3147 | `	pTos++;` |
|  2479224 |  3148 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1658280 |  3149 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3150 | `			SyHashEntry *pEntry;` |
|        - |  3151 | `			/* Candidate for expansion via user defined callbacks */` |
|    16438 |  3152 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16438 |  3153 | `			if( pEntry ){` |
|    16434 |  3154 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3155 | `				/* Set a NULL default value */` |
|    16434 |  3156 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16434 |  3157 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3158 | `				/* Invoke the callback and deal with the expanded value */` |
|    16434 |  3159 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3160 | `				/* Mark as constant */` |
|    16434 |  3161 | `				pTos->nIdx = SXU32_HIGH;` |
|    16434 |  3162 | `				break;` |
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
|  1641846 |  3194 | `		PH7_MemObjLoad(pObj,pTos);` |
|   820946 |  3195 | `	}else{` |
|        - |  3196 | `		/* Set a NULL value */` |
|      ! 0 |  3197 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3198 | `	}` |
|   820901 |  3199 | `LoadC_Done:` |
|        - |  3200 | `	/* Mark as constant */` |
|  1641848 |  3201 | `	pTos->nIdx = SXU32_HIGH;` |
|  1641848 |  3202 | `	break;` |
|        - |  3203 | `				  }` |
|        - |  3204 | `/*` |
|        - |  3205 | ` * LOAD: P1 * P3` |
|        - |  3206 | ` *` |
|        - |  3207 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3208 | ` * from the P3 operand.` |
|        - |  3209 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3210 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3211 | ` */` |
|  1344756 |  3212 | `case PH7_OP_LOAD:{` |
|        - |  3213 | `	ph7_value *pObj;` |
|        - |  3214 | `	SyString sName;` |
|  2689734 |  3215 | `	if( pInstr->p3 == 0 ){` |
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
|  2689716 |  3228 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3229 | `		/* Reserve a room for the target object */` |
|  2689716 |  3230 | `		pTos++;` |
|        - |  3231 | `	}` |
|        - |  3232 | `	/* Extract the requested memory object */` |
|  2689734 |  3233 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2689734 |  3234 | `	if( pObj == 0 ){` |
|       26 |  3235 | `		if( pInstr->iP1 ){` |
|        - |  3236 | `			/* Variable not found,load NULL */` |
|       26 |  3237 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3238 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3239 | `			}else{` |
|       26 |  3240 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3241 | `			}` |
|       26 |  3242 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1344770 |  3243 | `			break;` |
|      ! 0 |  3244 | `		}else{` |
|        - |  3245 | `			/* Fatal error */` |
|      ! 0 |  3246 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3247 | `			goto Abort;` |
|        - |  3248 | `		}` |
|        - |  3249 | `	}` |
|        - |  3250 | `	/* Load variable contents */` |
|  2689710 |  3251 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2689710 |  3252 | `	pTos->nIdx = pObj->nIdx;` |
|  2689710 |  3253 | `	break;` |
|        - |  3254 | `				   }` |
|        - |  3255 | `/*` |
|        - |  3256 | ` * LOAD_MAP P1 * *` |
|        - |  3257 | ` *` |
|        - |  3258 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3259 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3260 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3261 | ` */` |
|    18417 |  3262 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3263 | `	ph7_hashmap *pMap;` |
|        - |  3264 | `	/* Allocate a new hashmap instance */` |
|    36836 |  3265 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    36836 |  3266 | `	if( pMap == 0 ){` |
|      ! 0 |  3267 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3268 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3269 | `		goto Abort;` |
|        - |  3270 | `	}` |
|    36836 |  3271 | `	if( pInstr->iP1 > 0 ){` |
|     2238 |  3272 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3273 | `		/* Perform the insertion */` |
|     6838 |  3274 | `		while( pEntry < pTos ){` |
|     4602 |  3275 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3276 | `				/* Insertion by reference */` |
|      142 |  3277 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3278 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3279 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3280 | `					);` |
|       48 |  3281 | `			}else{` |
|        - |  3282 | `				/* Standard insertion */` |
|     6761 |  3283 | `				PH7_HashmapInsert(pMap,` |
|     4506 |  3284 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2253 |  3285 | `					&pEntry[1]` |
|        - |  3286 | `				);` |
|        - |  3287 | `			}` |
|        - |  3288 | `			/* Next pair on the stack */` |
|     4602 |  3289 | `			pEntry += 2;` |
|        2 |  3290 | `		}` |
|        - |  3291 | `		/* Pop P1 elements */` |
|     2238 |  3292 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1118 |  3293 | `	}` |
|        - |  3294 | `	/* Push the hashmap */` |
|    36836 |  3295 | `	pTos++;` |
|    36836 |  3296 | `	pTos->nIdx = SXU32_HIGH;` |
|    36836 |  3297 | `	pTos->x.pOther = pMap;` |
|    36836 |  3298 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    36836 |  3299 | `	break;` |
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
|   216050 |  3355 | `case PH7_OP_LOAD_IDX: {` |
|   432146 |  3356 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   432146 |  3357 | `	ph7_hashmap *pMap = 0;` |
|        - |  3358 | `	ph7_value *pIdx;` |
|   432146 |  3359 | `	pIdx = 0;` |
|   432146 |  3360 | `	if( pInstr->iP1 == 0 ){` |
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
|   432146 |  3377 | `		pIdx = pTos;` |
|   432146 |  3378 | `		pTos--;` |
|        - |  3379 | `	}` |
|   432146 |  3380 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
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
|    91472 |  3405 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3406 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3407 | `			ph7_value *pObj;` |
|      ! 0 |  3408 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3409 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3410 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3411 | `			}` |
|      ! 0 |  3412 | `		}` |
|      ! 0 |  3413 | `	}` |
|    91472 |  3414 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    91472 |  3415 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    91472 |  3416 | `		if( pInstr->iP2 ){` |
|        - |  3417 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3418 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3419 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3420 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3421 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3422 | `		}` |
|        - |  3423 | `		/* Point to the hashmap */` |
|    91472 |  3424 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    91472 |  3425 | `		if( pIdx ){` |
|        - |  3426 | `			/* Load the desired entry */` |
|    91472 |  3427 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    45735 |  3428 | `		}` |
|    91472 |  3429 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3430 | `			/* Create a new empty entry */` |
|      265 |  3431 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3432 | `			if( rc == SXRET_OK ){` |
|        - |  3433 | `				/* Point to the last inserted entry */` |
|      265 |  3434 | `				pNode = pMap->pLast;` |
|      132 |  3435 | `			}` |
|      132 |  3436 | `		}` |
|    45735 |  3437 | `	}` |
|    91472 |  3438 | `	if( pIdx ){` |
|    91472 |  3439 | `		PH7_MemObjRelease(pIdx);` |
|    45735 |  3440 | `	}` |
|    91472 |  3441 | `	if( rc == SXRET_OK ){` |
|        - |  3442 | `		/* Load entry contents */` |
|    41924 |  3443 | `		if( pMap->iRef < 2 ){` |
|        - |  3444 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3445 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3446 | `			 */` |
|       24 |  3447 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3448 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3449 | `		}else{` |
|    41902 |  3450 | `			pTos->nIdx = pNode->nValIdx;` |
|    41902 |  3451 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    41902 |  3452 | `			PH7_HashmapUnref(pMap);` |
|        - |  3453 | `		}` |
|    20963 |  3454 | `	}else{` |
|        - |  3455 | `		/* No such entry,load NULL */` |
|    49550 |  3456 | `		PH7_MemObjRelease(pTos);` |
|    49550 |  3457 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3458 | `	}` |
|    91472 |  3459 | `	break;` |
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
|       10 |  3496 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3497 | `		/* Register the closure */` |
|       10 |  3498 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3499 | `		/* Set up closure environment */` |
|       10 |  3500 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       10 |  3501 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       28 |  3502 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3503 | `			ph7_value *pValue;` |
|       20 |  3504 | `			pEnv = &aEnv[n];` |
|       20 |  3505 | `			sEnv.sName  = pEnv->sName;` |
|       20 |  3506 | `			sEnv.iFlags = pEnv->iFlags;` |
|       20 |  3507 | `			sEnv.nIdx = SXU32_HIGH;` |
|       20 |  3508 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       20 |  3509 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3510 | `				/* Pass by reference */` |
|      ! 0 |  3511 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3512 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3513 | `					);` |
|      ! 0 |  3514 | `			}` |
|        - |  3515 | `			/* Standard pass by value */` |
|       20 |  3516 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       20 |  3517 | `			if( pValue ){` |
|        - |  3518 | `				/* Copy imported value */` |
|       12 |  3519 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        5 |  3520 | `			}` |
|        - |  3521 | `			/* Insert the imported variable */` |
|       20 |  3522 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       11 |  3523 | `		}` |
|        - |  3524 | `		/* Finally,load the closure name on the stack */` |
|       10 |  3525 | `		pTos++;` |
|       10 |  3526 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        4 |  3527 | `	}` |
|       10 |  3528 | `	break;` |
|        - |  3529 | `						 }` |
|        - |  3530 | `/*` |
|        - |  3531 | ` * STORE * P2 P3` |
|        - |  3532 | ` *` |
|        - |  3533 | ` * Perform a store (Assignment) operation.` |
|        - |  3534 | ` */` |
|   113998 |  3535 | `case PH7_OP_STORE: {` |
|        - |  3536 | `	ph7_value *pObj;` |
|        - |  3537 | `	SyString sName;` |
|        - |  3538 | `#ifdef UNTRUST` |
|        - |  3539 | `	if( pTos < pStack ){` |
|        - |  3540 | `		goto Abort;` |
|        - |  3541 | `	}` |
|        - |  3542 | `#endif` |
|   227998 |  3543 | `	if( pInstr->iP2 ){` |
|        - |  3544 | `		sxu32 nIdx;` |
|        - |  3545 | `		/* Member store operation */` |
|     2954 |  3546 | `		nIdx = pTos->nIdx;` |
|     2954 |  3547 | `		VmPopOperand(&pTos,1);` |
|     2954 |  3548 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3549 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3550 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3551 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3552 | `		}else{` |
|        - |  3553 | `			/* Point to the desired memory object */` |
|     2950 |  3554 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2950 |  3555 | `			if( pObj ){` |
|        - |  3556 | `				/* Perform the store operation */` |
|     2950 |  3557 | `				PH7_MemObjStore(pTos,pObj);` |
|     1474 |  3558 | `			}` |
|        - |  3559 | `		}` |
|   115476 |  3560 | `		break;` |
|   225046 |  3561 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3562 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3563 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3564 | `			/* Force a string cast */` |
|      ! 0 |  3565 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3566 | `		}` |
|        7 |  3567 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3568 | `		pTos--;` |
|        - |  3569 | `#ifdef UNTRUST` |
|        - |  3570 | `		if( pTos < pStack  ){` |
|        - |  3571 | `			goto Abort;` |
|        - |  3572 | `		}` |
|        - |  3573 | `#endif` |
|        4 |  3574 | `	}else{` |
|   225040 |  3575 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3576 | `	}` |
|        - |  3577 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   225046 |  3578 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   225046 |  3579 | `	if( pObj == 0 ){` |
|      ! 0 |  3580 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3581 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3582 | `		goto Abort;` |
|        - |  3583 | `	}` |
|   225046 |  3584 | `	if( !pInstr->p3 ){` |
|        7 |  3585 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3586 | `	}` |
|        - |  3587 | `	/* Perform the store operation */` |
|   225046 |  3588 | `	PH7_MemObjStore(pTos,pObj);` |
|   225046 |  3589 | `	break;` |
|        - |  3590 | `				   }` |
|        - |  3591 | `/*` |
|        - |  3592 | ` * STORE_IDX:   P1 * P3` |
|        - |  3593 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3594 | ` *` |
|        - |  3595 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3596 | ` */` |
|    82021 |  3597 | `case PH7_OP_STORE_IDX:` |
|        - |  3598 | `case PH7_OP_STORE_IDX_REF: {` |
|   164044 |  3599 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3600 | `	ph7_value *pKey;` |
|        - |  3601 | `	sxu32 nIdx;` |
|   164044 |  3602 | `	if( pInstr->iP1 ){` |
|        - |  3603 | `		/* Key is next on stack */` |
|    57522 |  3604 | `		pKey = pTos;` |
|    57522 |  3605 | `		pTos--;` |
|    28762 |  3606 | `	}else{` |
|   106524 |  3607 | `		pKey = 0;` |
|        - |  3608 | `	}` |
|   164044 |  3609 | `	nIdx = pTos->nIdx;` |
|   164044 |  3610 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3611 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3612 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3613 | `		 * checking true sharing count, then re-add after separation. */` |
|   163992 |  3614 | `		if( nIdx != SXU32_HIGH ){` |
|   163992 |  3615 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   245987 |  3616 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   163992 |  3617 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3618 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3619 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3620 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3621 | `				 * refcounts if the backing array was already separated. */` |
|   163992 |  3622 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   163992 |  3623 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   163992 |  3624 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   163992 |  3625 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   163992 |  3626 | `					pTos->x.pOther = pMap;` |
|    81997 |  3627 | `				}else{` |
|        - |  3628 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3629 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3630 | `					pMap = pCur;` |
|        - |  3631 | `				}` |
|    81997 |  3632 | `			}else{` |
|      ! 0 |  3633 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3634 | `			}` |
|    81997 |  3635 | `		}else{` |
|      ! 0 |  3636 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3637 | `		}` |
|   163992 |  3638 | `		if( pMap->iRef < 2 ){` |
|        - |  3639 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3640 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3641 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3642 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3643 | `			pMap->iRef = 2;` |
|      ! 0 |  3644 | `		}` |
|    81997 |  3645 | `	}else{` |
|        - |  3646 | `		ph7_value *pObj;` |
|       53 |  3647 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3648 | `		if( pObj == 0 ){` |
|      ! 0 |  3649 | `			if( pKey ){` |
|      ! 0 |  3650 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3651 | `			}` |
|      ! 0 |  3652 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3653 | `			break;` |
|        - |  3654 | `		}` |
|        - |  3655 | `		/* Phase#1: Load the array */` |
|       53 |  3656 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3657 | `			VmPopOperand(&pTos,1);` |
|       53 |  3658 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3659 | `				/* Force a string cast */` |
|      ! 0 |  3660 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3661 | `			}` |
|       53 |  3662 | `			if( pKey == 0 ){` |
|        - |  3663 | `				/* Append string */` |
|        3 |  3664 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3665 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3666 | `				}` |
|        2 |  3667 | `			}else{` |
|        - |  3668 | `				sxu32 nOfft;` |
|       51 |  3669 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3670 | `					/* Force an int cast */` |
|       51 |  3671 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3672 | `				}` |
|       51 |  3673 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3674 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3675 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3676 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3677 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3678 | `				}else{` |
|      ! 0 |  3679 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3680 | `						/* Perform an append operation */` |
|      ! 0 |  3681 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3682 | `					}` |
|        - |  3683 | `				}` |
|        - |  3684 | `			}` |
|       53 |  3685 | `			if( pKey ){` |
|       51 |  3686 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3687 | `			}` |
|       53 |  3688 | `			break;` |
|      ! 0 |  3689 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3690 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3691 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3692 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3693 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3694 | `				goto Abort;` |
|        - |  3695 | `			}` |
|      ! 0 |  3696 | `		}` |
|        - |  3697 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3698 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3699 | `	}` |
|   163992 |  3700 | `	VmPopOperand(&pTos,1);` |
|        - |  3701 | `	/* Phase#2: Perform the insertion */` |
|   163992 |  3702 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3703 | `		/* Insertion by reference */` |
|       15 |  3704 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3705 | `	}else{` |
|   163978 |  3706 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3707 | `	}` |
|   163992 |  3708 | `	if( pKey ){` |
|    57472 |  3709 | `		PH7_MemObjRelease(pKey);` |
|    28735 |  3710 | `	}` |
|   163992 |  3711 | `	break;` |
|        - |  3712 | `					   }` |
|        - |  3713 | `/*` |
|        - |  3714 | ` * INCR: P1 * *` |
|        - |  3715 | ` *` |
|        - |  3716 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3717 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3718 | ` * the stack and increment after that.` |
|        - |  3719 | ` */` |
|   151326 |  3720 | `case PH7_OP_INCR:` |
|        - |  3721 | `#ifdef UNTRUST` |
|        - |  3722 | `	if( pTos < pStack ){` |
|        - |  3723 | `		goto Abort;` |
|        - |  3724 | `	}` |
|        - |  3725 | `#endif` |
|   302698 |  3726 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302698 |  3727 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3728 | `			ph7_value *pObj;` |
|   302698 |  3729 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3730 | `				/* Force a numeric cast */` |
|   302698 |  3731 | `				PH7_MemObjToNumeric(pObj);` |
|   302698 |  3732 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3733 | `					pObj->rVal++;` |
|        - |  3734 | `					/* Try to get an integer representation */` |
|      ! 0 |  3735 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3736 | `				}else{` |
|   302698 |  3737 | `					pObj->x.iVal++;` |
|   302698 |  3738 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3739 | `				}` |
|   302698 |  3740 | `				if( pInstr->iP1 ){` |
|        - |  3741 | `					/* Pre-icrement */` |
|       71 |  3742 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3743 | `				}` |
|   151370 |  3744 | `			}` |
|   151372 |  3745 | `		}else{` |
|      ! 0 |  3746 | `			if( pInstr->iP1 ){` |
|        - |  3747 | `				/* Force a numeric cast */` |
|      ! 0 |  3748 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3749 | `				/* Pre-increment */` |
|      ! 0 |  3750 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3751 | `					pTos->rVal++;` |
|        - |  3752 | `					/* Try to get an integer representation */` |
|      ! 0 |  3753 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3754 | `				}else{` |
|      ! 0 |  3755 | `					pTos->x.iVal++;` |
|      ! 0 |  3756 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3757 | `				}` |
|      ! 0 |  3758 | `			}` |
|        - |  3759 | `		}` |
|   151370 |  3760 | `	}` |
|   302698 |  3761 | `	break;` |
|        - |  3762 | `/*` |
|        - |  3763 | ` * DECR: P1 * *` |
|        - |  3764 | ` *` |
|        - |  3765 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3766 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3767 | ` * and decrement after that.` |
|        - |  3768 | ` */` |
|        2 |  3769 | `case PH7_OP_DECR:` |
|        - |  3770 | `#ifdef UNTRUST` |
|        - |  3771 | `	if( pTos < pStack ){` |
|        - |  3772 | `		goto Abort;` |
|        - |  3773 | `	}` |
|        - |  3774 | `#endif` |
|        5 |  3775 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3776 | `		/* Force a numeric cast */` |
|        5 |  3777 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3778 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3779 | `			ph7_value *pObj;` |
|        5 |  3780 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3781 | `				/* Force a numeric cast */` |
|        5 |  3782 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3783 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3784 | `					pObj->rVal--;` |
|        - |  3785 | `					/* Try to get an integer representation */` |
|      ! 0 |  3786 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3787 | `				}else{` |
|        5 |  3788 | `					pObj->x.iVal--;` |
|        5 |  3789 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3790 | `				}` |
|        5 |  3791 | `				if( pInstr->iP1 ){` |
|        - |  3792 | `					/* Pre-icrement */` |
|      ! 0 |  3793 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3794 | `				}` |
|        2 |  3795 | `			}` |
|        3 |  3796 | `		}else{` |
|      ! 0 |  3797 | `			if( pInstr->iP1 ){` |
|        - |  3798 | `				/* Pre-increment */` |
|      ! 0 |  3799 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3800 | `					pTos->rVal--;` |
|        - |  3801 | `					/* Try to get an integer representation */` |
|      ! 0 |  3802 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3803 | `				}else{` |
|      ! 0 |  3804 | `					pTos->x.iVal--;` |
|      ! 0 |  3805 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3806 | `				}` |
|      ! 0 |  3807 | `			}` |
|        - |  3808 | `		}` |
|        2 |  3809 | `	}` |
|        5 |  3810 | `	break;` |
|        - |  3811 | `/*` |
|        - |  3812 | ` * UMINUS: * * *` |
|        - |  3813 | ` *` |
|        - |  3814 | ` * Perform a unary minus operation.` |
|        - |  3815 | ` */` |
|    23825 |  3816 | `case PH7_OP_UMINUS:` |
|        - |  3817 | `#ifdef UNTRUST` |
|        - |  3818 | `	if( pTos < pStack ){` |
|        - |  3819 | `		goto Abort;` |
|        - |  3820 | `	}` |
|        - |  3821 | `#endif` |
|        - |  3822 | `	/* Force a numeric (integer,real or both) cast */` |
|    47652 |  3823 | `	PH7_MemObjToNumeric(pTos);` |
|    47652 |  3824 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3825 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3826 | `	}` |
|    47652 |  3827 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    47622 |  3828 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23810 |  3829 | `	}` |
|    47652 |  3830 | `	break;` |
|        - |  3831 | `/*` |
|        - |  3832 | ` * UPLUS: * * *` |
|        - |  3833 | ` *` |
|        - |  3834 | ` * Perform a unary plus operation.` |
|        - |  3835 | ` */` |
|       16 |  3836 | `case PH7_OP_UPLUS:` |
|        - |  3837 | `#ifdef UNTRUST` |
|        - |  3838 | `	if( pTos < pStack ){` |
|        - |  3839 | `		goto Abort;` |
|        - |  3840 | `	}` |
|        - |  3841 | `#endif` |
|        - |  3842 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3843 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3844 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3845 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3846 | `	}` |
|       33 |  3847 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3848 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3849 | `	}` |
|       33 |  3850 | `	break;` |
|        - |  3851 | `/*` |
|        - |  3852 | ` * OP_LNOT: * * *` |
|        - |  3853 | ` *` |
|        - |  3854 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3855 | ` * with its complement.` |
|        - |  3856 | ` */` |
|    40119 |  3857 | `case PH7_OP_LNOT:` |
|        - |  3858 | `#ifdef UNTRUST` |
|        - |  3859 | `	if( pTos < pStack ){` |
|        - |  3860 | `		goto Abort;` |
|        - |  3861 | `	}` |
|        - |  3862 | `#endif` |
|        - |  3863 | `	/* Force a boolean cast */` |
|    80284 |  3864 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3865 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3866 | `	}` |
|    80284 |  3867 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    80284 |  3868 | `	break;` |
|        - |  3869 | `/*` |
|        - |  3870 | ` * OP_BITNOT: * * *` |
|        - |  3871 | ` *` |
|        - |  3872 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3873 | ` * with its ones-complement.` |
|        - |  3874 | ` */` |
|       14 |  3875 | `case PH7_OP_BITNOT:` |
|        - |  3876 | `#ifdef UNTRUST` |
|        - |  3877 | `	if( pTos < pStack ){` |
|        - |  3878 | `		goto Abort;` |
|        - |  3879 | `	}` |
|        - |  3880 | `#endif` |
|        - |  3881 | `	/* Force an integer cast */` |
|       30 |  3882 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3883 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3884 | `	}` |
|       30 |  3885 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3886 | `	break;` |
|        - |  3887 | `/* OP_MUL * * *` |
|        - |  3888 | ` * OP_MUL_STORE * * *` |
|        - |  3889 | ` *` |
|        - |  3890 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3891 | ` * and push the result back onto the stack.` |
|        - |  3892 | ` */` |
|     1247 |  3893 | `case PH7_OP_MUL:` |
|        - |  3894 | `case PH7_OP_MUL_STORE: {` |
|     2496 |  3895 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3896 | `	/* Force the operand to be numeric */` |
|        - |  3897 | `#ifdef UNTRUST` |
|        - |  3898 | `	if( pNos < pStack ){` |
|        - |  3899 | `		goto Abort;` |
|        - |  3900 | `	}` |
|        - |  3901 | `#endif` |
|     2496 |  3902 | `	PH7_MemObjToNumeric(pTos);` |
|     2496 |  3903 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3904 | `	/* Perform the requested operation */` |
|     2496 |  3905 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3906 | `		/* Floating point arithemic */` |
|        - |  3907 | `		ph7_real a,b,r;` |
|       17 |  3908 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3909 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3910 | `		}` |
|       17 |  3911 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3912 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3913 | `		}` |
|       17 |  3914 | `		a = pNos->rVal;` |
|       17 |  3915 | `		b = pTos->rVal;` |
|       17 |  3916 | `		r = a * b;` |
|        - |  3917 | `		/* Push the result */` |
|       17 |  3918 | `		pNos->rVal = r;` |
|       17 |  3919 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3920 | `		/* Try to get an integer representation */` |
|       17 |  3921 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3922 | `	}else{` |
|        - |  3923 | `		/* Integer arithmetic */` |
|        - |  3924 | `		sxi64 a,b,r;` |
|     2480 |  3925 | `		a = pNos->x.iVal;` |
|     2480 |  3926 | `		b = pTos->x.iVal;` |
|     2480 |  3927 | `		r = a * b;` |
|        - |  3928 | `		/* Push the result */` |
|     2480 |  3929 | `		pNos->x.iVal = r;` |
|     2480 |  3930 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3931 | `	}` |
|     2496 |  3932 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3933 | `		ph7_value *pObj;` |
|       25 |  3934 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3935 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       25 |  3936 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       25 |  3937 | `			PH7_MemObjStore(pNos,pObj);` |
|       12 |  3938 | `		}` |
|       12 |  3939 | `	}` |
|     2496 |  3940 | `	VmPopOperand(&pTos,1);` |
|     2496 |  3941 | `	break;` |
|        - |  3942 | `				 }` |
|        - |  3943 | `/* OP_ADD * * *` |
|        - |  3944 | ` *` |
|        - |  3945 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3946 | ` * and push the result back onto the stack.` |
|        - |  3947 | ` */` |
|      439 |  3948 | `case PH7_OP_ADD:{` |
|      880 |  3949 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3950 | `#ifdef UNTRUST` |
|        - |  3951 | `	if( pNos < pStack ){` |
|        - |  3952 | `		goto Abort;` |
|        - |  3953 | `	}` |
|        - |  3954 | `#endif` |
|        - |  3955 | `	/* Perform the addition */` |
|      880 |  3956 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      880 |  3957 | `	VmPopOperand(&pTos,1);` |
|      880 |  3958 | `	break;` |
|        - |  3959 | `				}` |
|        - |  3960 | `/*` |
|        - |  3961 | ` * OP_ADD_STORE * * *` |
|        - |  3962 | ` *` |
|        - |  3963 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3964 | ` * and push the result back onto the stack.` |
|        - |  3965 | ` */` |
|      483 |  3966 | `case PH7_OP_ADD_STORE:{` |
|      968 |  3967 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3968 | `	ph7_value *pObj;` |
|        - |  3969 | `	sxu32 nIdx;` |
|        - |  3970 | `#ifdef UNTRUST` |
|        - |  3971 | `	if( pNos < pStack ){` |
|        - |  3972 | `		goto Abort;` |
|        - |  3973 | `	}` |
|        - |  3974 | `#endif` |
|        - |  3975 | `	/* Perform the addition */` |
|      968 |  3976 | `	nIdx = pTos->nIdx;` |
|      968 |  3977 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3978 | `	/* Peform the store operation */` |
|      968 |  3979 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3980 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      968 |  3981 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      968 |  3982 | `		PH7_MemObjStore(pTos,pObj);` |
|      483 |  3983 | `	}` |
|        - |  3984 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      968 |  3985 | `	PH7_MemObjStore(pTos,pNos);` |
|      968 |  3986 | `	VmPopOperand(&pTos,1);` |
|      968 |  3987 | `	break;` |
|        - |  3988 | `				}` |
|        - |  3989 | `/* OP_SUB * * *` |
|        - |  3990 | ` *` |
|        - |  3991 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3992 | ` * first (what was next on the stack) from the second (the` |
|        - |  3993 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3994 | ` */` |
|      299 |  3995 | `case PH7_OP_SUB: {` |
|      600 |  3996 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3997 | `#ifdef UNTRUST` |
|        - |  3998 | `	if( pNos < pStack ){` |
|        - |  3999 | `		goto Abort;` |
|        - |  4000 | `	}` |
|        - |  4001 | `#endif` |
|      600 |  4002 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4003 | `		/* Floating point arithemic */` |
|        - |  4004 | `		ph7_real a,b,r;` |
|       95 |  4005 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4006 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4007 | `		}` |
|       95 |  4008 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4009 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4010 | `		}` |
|       95 |  4011 | `		a = pNos->rVal;` |
|       95 |  4012 | `		b = pTos->rVal;` |
|       95 |  4013 | `		r = a - b;` |
|        - |  4014 | `		/* Push the result */` |
|       95 |  4015 | `		pNos->rVal = r;` |
|       95 |  4016 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4017 | `		/* Try to get an integer representation */` |
|       95 |  4018 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4019 | `	}else{` |
|        - |  4020 | `		/* Integer arithmetic */` |
|        - |  4021 | `		sxi64 a,b,r;` |
|      506 |  4022 | `		a = pNos->x.iVal;` |
|      506 |  4023 | `		b = pTos->x.iVal;` |
|      506 |  4024 | `		r = a - b;` |
|        - |  4025 | `		/* Push the result */` |
|      506 |  4026 | `		pNos->x.iVal = r;` |
|      506 |  4027 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4028 | `	}` |
|      600 |  4029 | `	VmPopOperand(&pTos,1);` |
|      600 |  4030 | `	break;` |
|        - |  4031 | `				 }` |
|        - |  4032 | `/* OP_SUB_STORE * * *` |
|        - |  4033 | ` *` |
|        - |  4034 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4035 | ` * first (what was next on the stack) from the second (the` |
|        - |  4036 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4037 | ` */` |
|        1 |  4038 | `case PH7_OP_SUB_STORE: {` |
|        3 |  4039 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4040 | `	ph7_value *pObj;` |
|        - |  4041 | `#ifdef UNTRUST` |
|        - |  4042 | `	if( pNos < pStack ){` |
|        - |  4043 | `		goto Abort;` |
|        - |  4044 | `	}` |
|        - |  4045 | `#endif` |
|        3 |  4046 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4047 | `		/* Floating point arithemic */` |
|        - |  4048 | `		ph7_real a,b,r;` |
|      ! 0 |  4049 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4050 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4051 | `		}` |
|      ! 0 |  4052 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4053 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4054 | `		}` |
|      ! 0 |  4055 | `		a = pTos->rVal;` |
|      ! 0 |  4056 | `		b = pNos->rVal;` |
|      ! 0 |  4057 | `		r = a - b;` |
|        - |  4058 | `		/* Push the result */` |
|      ! 0 |  4059 | `		pNos->rVal = r;` |
|      ! 0 |  4060 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4061 | `		/* Try to get an integer representation */` |
|      ! 0 |  4062 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4063 | `	}else{` |
|        - |  4064 | `		/* Integer arithmetic */` |
|        - |  4065 | `		sxi64 a,b,r;` |
|        3 |  4066 | `		a = pTos->x.iVal;` |
|        3 |  4067 | `		b = pNos->x.iVal;` |
|        3 |  4068 | `		r = a - b;` |
|        - |  4069 | `		/* Push the result */` |
|        3 |  4070 | `		pNos->x.iVal = r;` |
|        3 |  4071 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4072 | `	}` |
|        3 |  4073 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4074 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4075 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4076 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4077 | `	}` |
|        3 |  4078 | `	VmPopOperand(&pTos,1);` |
|        3 |  4079 | `	break;` |
|        - |  4080 | `				 }` |
|        - |  4081 |  |
|        - |  4082 | `/*` |
|        - |  4083 | ` * OP_MOD * * *` |
|        - |  4084 | ` *` |
|        - |  4085 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4086 | ` * first (what was next on the stack) from the second (the` |
|        - |  4087 | ` * top of the stack) and push the remainder after division` |
|        - |  4088 | ` * onto the stack.` |
|        - |  4089 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4090 | ` */` |
|      305 |  4091 | `case PH7_OP_MOD:{` |
|      612 |  4092 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4093 | `	sxi64 a,b,r;` |
|        - |  4094 | `#ifdef UNTRUST` |
|        - |  4095 | `	if( pNos < pStack ){` |
|        - |  4096 | `		goto Abort;` |
|        - |  4097 | `	}` |
|        - |  4098 | `#endif` |
|        - |  4099 | `	/* Force the operands to be integer */` |
|      612 |  4100 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4101 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4102 | `	}` |
|      612 |  4103 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4104 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4105 | `	}` |
|        - |  4106 | `	/* Perform the requested operation */` |
|      612 |  4107 | `	a = pNos->x.iVal;` |
|      612 |  4108 | `	b = pTos->x.iVal;` |
|      612 |  4109 | `	if( b == 0 ){` |
|        3 |  4110 | `		r = 0;` |
|        3 |  4111 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4112 | `		/* goto Abort; */` |
|        2 |  4113 | `	}else{` |
|      609 |  4114 | `		r = a%b;` |
|        - |  4115 | `	}` |
|        - |  4116 | `	/* Push the result */` |
|      612 |  4117 | `	pNos->x.iVal = r;` |
|      612 |  4118 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      612 |  4119 | `	VmPopOperand(&pTos,1);` |
|      612 |  4120 | `	break;` |
|        - |  4121 | `				}` |
|        - |  4122 | `/*` |
|        - |  4123 | ` * OP_MOD_STORE * * *` |
|        - |  4124 | ` *` |
|        - |  4125 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4126 | ` * first (what was next on the stack) from the second (the` |
|        - |  4127 | ` * top of the stack) and push the remainder after division` |
|        - |  4128 | ` * onto the stack.` |
|        - |  4129 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4130 | ` */` |
|        1 |  4131 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4132 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4133 | `	ph7_value *pObj;` |
|        - |  4134 | `	sxi64 a,b,r;` |
|        - |  4135 | `#ifdef UNTRUST` |
|        - |  4136 | `	if( pNos < pStack ){` |
|        - |  4137 | `		goto Abort;` |
|        - |  4138 | `	}` |
|        - |  4139 | `#endif` |
|        - |  4140 | `	/* Force the operands to be integer */` |
|        3 |  4141 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4142 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4143 | `	}` |
|        3 |  4144 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4145 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4146 | `	}` |
|        - |  4147 | `	/* Perform the requested operation */` |
|        3 |  4148 | `	a = pTos->x.iVal;` |
|        3 |  4149 | `	b = pNos->x.iVal;` |
|        3 |  4150 | `	if( b == 0 ){` |
|      ! 0 |  4151 | `		r = 0;` |
|      ! 0 |  4152 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4153 | `		/* goto Abort; */` |
|      ! 0 |  4154 | `	}else{` |
|        3 |  4155 | `		r = a%b;` |
|        - |  4156 | `	}` |
|        - |  4157 | `	/* Push the result */` |
|        3 |  4158 | `	pNos->x.iVal = r;` |
|        3 |  4159 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4160 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4161 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4162 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4163 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4164 | `	}` |
|        3 |  4165 | `	VmPopOperand(&pTos,1);` |
|        3 |  4166 | `	break;` |
|        - |  4167 | `				}` |
|        - |  4168 | `/*` |
|        - |  4169 | ` * OP_DIV * * *` |
|        - |  4170 | ` *` |
|        - |  4171 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4172 | ` * first (what was next on the stack) from the second (the` |
|        - |  4173 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4174 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4175 | ` */` |
|       28 |  4176 | `case PH7_OP_DIV:{` |
|       58 |  4177 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4178 | `	ph7_real a,b,r;` |
|        - |  4179 | `#ifdef UNTRUST` |
|        - |  4180 | `	if( pNos < pStack ){` |
|        - |  4181 | `		goto Abort;` |
|        - |  4182 | `	}` |
|        - |  4183 | `#endif` |
|        - |  4184 | `	/* Force the operands to be real */` |
|       58 |  4185 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  4186 | `		PH7_MemObjToReal(pTos);` |
|       26 |  4187 | `	}` |
|       58 |  4188 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  4189 | `		PH7_MemObjToReal(pNos);` |
|        9 |  4190 | `	}` |
|        - |  4191 | `	/* Perform the requested operation */` |
|       58 |  4192 | `	a = pNos->rVal;` |
|       58 |  4193 | `	b = pTos->rVal;` |
|       58 |  4194 | `	if( b == 0 ){` |
|        - |  4195 | `		/* Division by zero */` |
|        3 |  4196 | `		pNos->rVal = 0;` |
|        3 |  4197 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4198 | `		/* goto Abort; */` |
|        2 |  4199 | `	}else{` |
|       55 |  4200 | `		r = a/b;` |
|        - |  4201 | `		/* Push the result */` |
|       55 |  4202 | `		pNos->rVal = r;` |
|       55 |  4203 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4204 | `		/* Try to get an integer representation */` |
|       55 |  4205 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4206 | `	}` |
|       58 |  4207 | `	VmPopOperand(&pTos,1);` |
|       58 |  4208 | `	break;` |
|        - |  4209 | `				}` |
|        - |  4210 | `/*` |
|        - |  4211 | ` * OP_DIV_STORE * * *` |
|        - |  4212 | ` *` |
|        - |  4213 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4214 | ` * first (what was next on the stack) from the second (the` |
|        - |  4215 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4216 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4217 | ` */` |
|        1 |  4218 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4219 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4220 | `	ph7_value *pObj;` |
|        - |  4221 | `	ph7_real a,b,r;` |
|        - |  4222 | `#ifdef UNTRUST` |
|        - |  4223 | `	if( pNos < pStack ){` |
|        - |  4224 | `		goto Abort;` |
|        - |  4225 | `	}` |
|        - |  4226 | `#endif` |
|        - |  4227 | `	/* Force the operands to be real */` |
|        3 |  4228 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4229 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4230 | `	}` |
|        3 |  4231 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4232 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4233 | `	}` |
|        - |  4234 | `	/* Perform the requested operation */` |
|        3 |  4235 | `	a = pTos->rVal;` |
|        3 |  4236 | `	b = pNos->rVal;` |
|        3 |  4237 | `	if( b == 0 ){` |
|        - |  4238 | `		/* Division by zero */` |
|      ! 0 |  4239 | `		r = 0;` |
|      ! 0 |  4240 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4241 | `		/* goto Abort; */` |
|      ! 0 |  4242 | `	}else{` |
|        3 |  4243 | `		r = a/b;` |
|        - |  4244 | `		/* Push the result */` |
|        3 |  4245 | `		pNos->rVal = r;` |
|        3 |  4246 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4247 | `		/* Try to get an integer representation */` |
|        3 |  4248 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4249 | `	}` |
|        3 |  4250 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4251 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4252 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4253 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4254 | `	}` |
|        3 |  4255 | `	VmPopOperand(&pTos,1);` |
|        3 |  4256 | `	break;` |
|        - |  4257 | `				}` |
|        - |  4258 | `/* OP_BAND * * *` |
|        - |  4259 | ` *` |
|        - |  4260 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4261 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4262 | ` * two elements.` |
|        - |  4263 | `*/` |
|        - |  4264 | `/* OP_BOR * * *` |
|        - |  4265 | ` *` |
|        - |  4266 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4267 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4268 | ` * two elements.` |
|        - |  4269 | ` */` |
|        - |  4270 | `/* OP_BXOR * * *` |
|        - |  4271 | ` *` |
|        - |  4272 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4273 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4274 | ` * two elements.` |
|        - |  4275 | ` */` |
|       30 |  4276 | `case PH7_OP_BAND:` |
|        - |  4277 | `case PH7_OP_BOR:` |
|        - |  4278 | `case PH7_OP_BXOR:{` |
|       62 |  4279 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4280 | `	sxi64 a,b,r;` |
|        - |  4281 | `#ifdef UNTRUST` |
|        - |  4282 | `	if( pNos < pStack ){` |
|        - |  4283 | `		goto Abort;` |
|        - |  4284 | `	}` |
|        - |  4285 | `#endif` |
|        - |  4286 | `	/* Force the operands to be integer */` |
|       62 |  4287 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4288 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4289 | `	}` |
|       62 |  4290 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4291 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4292 | `	}` |
|        - |  4293 | `	/* Perform the requested operation */` |
|       62 |  4294 | `	a = pNos->x.iVal;` |
|       62 |  4295 | `	b = pTos->x.iVal;` |
|       62 |  4296 | `	switch(pInstr->iOp){` |
|        6 |  4297 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4298 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4299 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4300 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4301 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4302 | `	case PH7_OP_BAND:` |
|       38 |  4303 | `	default:          r = a&b; break;` |
|        - |  4304 | `	}` |
|        - |  4305 | `	/* Push the result */` |
|       62 |  4306 | `	pNos->x.iVal = r;` |
|       62 |  4307 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4308 | `	VmPopOperand(&pTos,1);` |
|       62 |  4309 | `	break;` |
|        - |  4310 | `				 }` |
|        - |  4311 | `/* OP_BAND_STORE * * *` |
|        - |  4312 | ` *` |
|        - |  4313 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4314 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4315 | ` * two elements.` |
|        - |  4316 | `*/` |
|        - |  4317 | `/* OP_BOR_STORE * * *` |
|        - |  4318 | ` *` |
|        - |  4319 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4320 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4321 | ` * two elements.` |
|        - |  4322 | ` */` |
|        - |  4323 | `/* OP_BXOR_STORE * * *` |
|        - |  4324 | ` *` |
|        - |  4325 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4326 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4327 | ` * two elements.` |
|        - |  4328 | ` */` |
|        7 |  4329 | `case PH7_OP_BAND_STORE:` |
|        - |  4330 | `case PH7_OP_BOR_STORE:` |
|        - |  4331 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4332 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4333 | `	ph7_value *pObj;` |
|        - |  4334 | `	sxi64 a,b,r;` |
|        - |  4335 | `#ifdef UNTRUST` |
|        - |  4336 | `	if( pNos < pStack ){` |
|        - |  4337 | `		goto Abort;` |
|        - |  4338 | `	}` |
|        - |  4339 | `#endif` |
|        - |  4340 | `	/* Force the operands to be integer */` |
|       15 |  4341 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4342 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4343 | `	}` |
|       15 |  4344 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4345 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4346 | `	}` |
|        - |  4347 | `	/* Perform the requested operation */` |
|       15 |  4348 | `	a = pTos->x.iVal;` |
|       15 |  4349 | `	b = pNos->x.iVal;` |
|       15 |  4350 | `	switch(pInstr->iOp){` |
|        2 |  4351 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4352 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4353 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4354 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4355 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4356 | `	case PH7_OP_BAND:` |
|        5 |  4357 | `	default:          r = a&b; break;` |
|        - |  4358 | `	}` |
|        - |  4359 | `	/* Push the result */` |
|       15 |  4360 | `	pNos->x.iVal = r;` |
|       15 |  4361 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4362 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4363 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4364 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4365 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4366 | `	}` |
|       15 |  4367 | `	VmPopOperand(&pTos,1);` |
|       15 |  4368 | `	break;` |
|        - |  4369 | `				 }` |
|        - |  4370 | `/* OP_SHL * * *` |
|        - |  4371 | ` *` |
|        - |  4372 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4373 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4374 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4375 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4376 | ` */` |
|        - |  4377 | `/* OP_SHR * * *` |
|        - |  4378 | ` *` |
|        - |  4379 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4380 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4381 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4382 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4383 | ` */` |
|        9 |  4384 | `case PH7_OP_SHL:` |
|        - |  4385 | `case PH7_OP_SHR: {` |
|       19 |  4386 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4387 | `	sxi64 a,r;` |
|        - |  4388 | `	sxi32 b;` |
|        - |  4389 | `#ifdef UNTRUST` |
|        - |  4390 | `	if( pNos < pStack ){` |
|        - |  4391 | `		goto Abort;` |
|        - |  4392 | `	}` |
|        - |  4393 | `#endif` |
|        - |  4394 | `	/* Force the operands to be integer */` |
|       19 |  4395 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4396 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4397 | `	}` |
|       19 |  4398 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4399 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4400 | `	}` |
|        - |  4401 | `	/* Perform the requested operation */` |
|       19 |  4402 | `	a = pNos->x.iVal;` |
|       19 |  4403 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4404 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4405 | `		r = a << b;` |
|        6 |  4406 | `	}else{` |
|        9 |  4407 | `		r = a >> b;` |
|        - |  4408 | `	}` |
|        - |  4409 | `	/* Push the result */` |
|       19 |  4410 | `	pNos->x.iVal = r;` |
|       19 |  4411 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4412 | `	VmPopOperand(&pTos,1);` |
|       19 |  4413 | `	break;` |
|        - |  4414 | `				 }` |
|        - |  4415 | `/*  OP_SHL_STORE * * *` |
|        - |  4416 | ` *` |
|        - |  4417 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4418 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4419 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4420 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4421 | ` */` |
|        - |  4422 | `/* OP_SHR_STORE * * *` |
|        - |  4423 | ` *` |
|        - |  4424 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4425 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4426 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4427 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4428 | ` */` |
|        7 |  4429 | `case PH7_OP_SHL_STORE:` |
|        - |  4430 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4431 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4432 | `	ph7_value *pObj;` |
|        - |  4433 | `	sxi64 a,r;` |
|        - |  4434 | `	sxi32 b;` |
|        - |  4435 | `#ifdef UNTRUST` |
|        - |  4436 | `	if( pNos < pStack ){` |
|        - |  4437 | `		goto Abort;` |
|        - |  4438 | `	}` |
|        - |  4439 | `#endif` |
|        - |  4440 | `	/* Force the operands to be integer */` |
|       15 |  4441 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4442 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4443 | `	}` |
|       15 |  4444 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4445 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4446 | `	}` |
|        - |  4447 | `	/* Perform the requested operation */` |
|       15 |  4448 | `	a = pTos->x.iVal;` |
|       15 |  4449 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4450 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4451 | `		r = a << b;` |
|        4 |  4452 | `	}else{` |
|        9 |  4453 | `		r = a >> b;` |
|        - |  4454 | `	}` |
|        - |  4455 | `	/* Push the result */` |
|       15 |  4456 | `	pNos->x.iVal = r;` |
|       15 |  4457 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4458 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4459 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4460 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4461 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4462 | `	}` |
|       15 |  4463 | `	VmPopOperand(&pTos,1);` |
|       15 |  4464 | `	break;` |
|        - |  4465 | `				 }` |
|        - |  4466 | `/* CAT:  P1 * *` |
|        - |  4467 | ` *` |
|        - |  4468 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4469 | ` * back.` |
|        - |  4470 | ` */` |
|    63364 |  4471 | `case PH7_OP_CAT:{` |
|        - |  4472 | `	ph7_value *pNos,*pCur;` |
|   126730 |  4473 | `	if( pInstr->iP1 < 1 ){` |
|    99698 |  4474 | `		pNos = &pTos[-1];` |
|    49850 |  4475 | `	}else{` |
|    27034 |  4476 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4477 | `	}` |
|        - |  4478 | `#ifdef UNTRUST` |
|        - |  4479 | `	if( pNos < pStack ){` |
|        - |  4480 | `		goto Abort;` |
|        - |  4481 | `	}` |
|        - |  4482 | `#endif` |
|        - |  4483 | `	/* Force a string cast */` |
|   126730 |  4484 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1210 |  4485 | `		PH7_MemObjToString(pNos);` |
|      604 |  4486 | `	}` |
|   126730 |  4487 | `	pCur = &pNos[1];` |
|   255482 |  4488 | `	while( pCur <= pTos ){` |
|   128754 |  4489 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50638 |  4490 | `			PH7_MemObjToString(pCur);` |
|    25318 |  4491 | `		}` |
|        - |  4492 | `		/* Perform the concatenation */` |
|   128754 |  4493 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   128716 |  4494 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    64357 |  4495 | `		}` |
|   128754 |  4496 | `		SyBlobRelease(&pCur->sBlob);` |
|   128754 |  4497 | `		pCur++;` |
|        2 |  4498 | `	}` |
|   126730 |  4499 | `	pTos = pNos;` |
|   126730 |  4500 | `	break;` |
|        - |  4501 | `				}` |
|        - |  4502 | `/*  CAT_STORE: * * *` |
|        - |  4503 | ` *` |
|        - |  4504 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4505 | ` * back.` |
|        - |  4506 | ` */` |
|     3653 |  4507 | `case PH7_OP_CAT_STORE:{` |
|     7308 |  4508 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4509 | `	ph7_value *pObj;` |
|        - |  4510 | `#ifdef UNTRUST` |
|        - |  4511 | `	if( pNos < pStack ){` |
|        - |  4512 | `		goto Abort;` |
|        - |  4513 | `	}` |
|        - |  4514 | `#endif` |
|     7308 |  4515 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4516 | `		/* Force a string cast */` |
|      ! 0 |  4517 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4518 | `	}` |
|     7308 |  4519 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4520 | `		/* Force a string cast */` |
|      ! 0 |  4521 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4522 | `	}` |
|        - |  4523 | `	/* Perform the concatenation (Reverse order) */` |
|     7308 |  4524 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7308 |  4525 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3653 |  4526 | `	}` |
|        - |  4527 | `	/* Perform the store operation */` |
|     7308 |  4528 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4529 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7308 |  4530 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7308 |  4531 | `		PH7_MemObjStore(pTos,pObj);` |
|     3653 |  4532 | `	}` |
|     7308 |  4533 | `	PH7_MemObjStore(pTos,pNos);` |
|     7308 |  4534 | `	VmPopOperand(&pTos,1);` |
|     7308 |  4535 | `	break;` |
|        - |  4536 | `				}` |
|        - |  4537 | `/* OP_AND: * * *` |
|        - |  4538 | ` *` |
|        - |  4539 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4540 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4541 | ` * stack.` |
|        - |  4542 | ` */` |
|        - |  4543 | `/* OP_OR: * * *` |
|        - |  4544 | ` *` |
|        - |  4545 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4546 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4547 | ` * stack.` |
|        - |  4548 | ` */` |
|    94509 |  4549 | `case PH7_OP_LAND:` |
|        - |  4550 | `case PH7_OP_LOR: {` |
|   189064 |  4551 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4552 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4553 | `#ifdef UNTRUST` |
|        - |  4554 | `	if( pNos < pStack ){` |
|        - |  4555 | `		goto Abort;` |
|        - |  4556 | `	}` |
|        - |  4557 | `#endif` |
|        - |  4558 | `	/* Force a boolean cast */` |
|   189064 |  4559 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4560 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4561 | `	}` |
|   189064 |  4562 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4563 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4564 | `	}` |
|   189064 |  4565 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   189064 |  4566 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   189064 |  4567 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4568 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    86666 |  4569 | `		v1 = and_logic[v1*3+v2];` |
|    43356 |  4570 | `	}else{` |
|        - |  4571 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102400 |  4572 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4573 | `	}` |
|   189064 |  4574 | `	if( v1 == 2 ){` |
|      ! 0 |  4575 | `		v1 = 1;` |
|      ! 0 |  4576 | `	}` |
|   189064 |  4577 | `	VmPopOperand(&pTos,1);` |
|   189064 |  4578 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   189064 |  4579 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   189064 |  4580 | `	break;` |
|        - |  4581 | `				 }` |
|        - |  4582 | `/* OP_LXOR: * * *` |
|        - |  4583 | ` *` |
|        - |  4584 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4585 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4586 | ` * stack.` |
|        - |  4587 | ` * According to the PHP language reference manual:` |
|        - |  4588 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4589 | ` *  TRUE,but not both.` |
|        - |  4590 | ` */` |
|        5 |  4591 | `case PH7_OP_LXOR:{` |
|       11 |  4592 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4593 | `	sxi32 v = 0;` |
|        - |  4594 | `#ifdef UNTRUST` |
|        - |  4595 | `	if( pNos < pStack ){` |
|        - |  4596 | `		goto Abort;` |
|        - |  4597 | `	}` |
|        - |  4598 | `#endif` |
|        - |  4599 | `	/* Force a boolean cast */` |
|       11 |  4600 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4601 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4602 | `	}` |
|       11 |  4603 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4604 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4605 | `	}` |
|       11 |  4606 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4607 | `		v = 1;` |
|        3 |  4608 | `	}` |
|       11 |  4609 | `	VmPopOperand(&pTos,1);` |
|       11 |  4610 | `	pTos->x.iVal = v;` |
|       11 |  4611 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4612 | `	break;` |
|        - |  4613 | `				 }` |
|        - |  4614 | `/* OP_EQ P1 P2 P3` |
|        - |  4615 | ` *` |
|        - |  4616 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4617 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4618 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4619 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4620 | ` */` |
|        - |  4621 | `/* OP_NEQ P1 P2 P3` |
|        - |  4622 | ` *` |
|        - |  4623 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4624 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4625 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4626 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4627 | ` */` |
|     3928 |  4628 | `case PH7_OP_EQ:` |
|        - |  4629 | `case PH7_OP_NEQ: {` |
|     7858 |  4630 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4631 | `	/* Perform the comparison and act accordingly */` |
|        - |  4632 | `#ifdef UNTRUST` |
|        - |  4633 | `	if( pNos < pStack ){` |
|        - |  4634 | `		goto Abort;` |
|        - |  4635 | `	}` |
|        - |  4636 | `#endif` |
|     7858 |  4637 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7858 |  4638 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4639 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7849 |  4640 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7814 |  4641 | `		rc = rc == 0;` |
|     3908 |  4642 | `	}else{` |
|       28 |  4643 | `		rc = rc != 0;` |
|        - |  4644 | `	}` |
|     7858 |  4645 | `	VmPopOperand(&pTos,1);` |
|     7858 |  4646 | `	if( !pInstr->iP2 ){` |
|        - |  4647 | `		/* Push comparison result without taking the jump */` |
|     7858 |  4648 | `		PH7_MemObjRelease(pTos);` |
|     7858 |  4649 | `		pTos->x.iVal = rc;` |
|        - |  4650 | `		/* Invalidate any prior representation */` |
|     7858 |  4651 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3930 |  4652 | `	}else{` |
|      ! 0 |  4653 | `		if( rc ){` |
|        - |  4654 | `			/* Jump to the desired location */` |
|      ! 0 |  4655 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4656 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4657 | `		}` |
|        - |  4658 | `	}` |
|     7858 |  4659 | `	break;` |
|        - |  4660 | `				 }` |
|        - |  4661 | `/* OP_TEQ P1 P2 *` |
|        - |  4662 | ` *` |
|        - |  4663 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4664 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4665 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4666 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4667 | ` */` |
|   132251 |  4668 | `case PH7_OP_TEQ: {` |
|   264504 |  4669 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4670 | `	/* Perform the comparison and act accordingly */` |
|        - |  4671 | `#ifdef UNTRUST` |
|        - |  4672 | `	if( pNos < pStack ){` |
|        - |  4673 | `		goto Abort;` |
|        - |  4674 | `	}` |
|        - |  4675 | `#endif` |
|   264504 |  4676 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   264504 |  4677 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4678 | `		rc = 0;` |
|        2 |  4679 | `	}else{` |
|   264502 |  4680 | `		rc = rc == 0;` |
|        - |  4681 | `	}` |
|   264504 |  4682 | `	VmPopOperand(&pTos,1);` |
|   264504 |  4683 | `	if( !pInstr->iP2 ){` |
|        - |  4684 | `		/* Push comparison result without taking the jump */` |
|   264504 |  4685 | `		PH7_MemObjRelease(pTos);` |
|   264504 |  4686 | `		pTos->x.iVal = rc;` |
|        - |  4687 | `		/* Invalidate any prior representation */` |
|   264504 |  4688 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   132253 |  4689 | `	}else{` |
|      ! 0 |  4690 | `		if( rc ){` |
|        - |  4691 | `			/* Jump to the desired location */` |
|      ! 0 |  4692 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4693 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4694 | `		}` |
|        - |  4695 | `	}` |
|   264504 |  4696 | `	break;` |
|        - |  4697 | `				 }` |
|        - |  4698 | `/* OP_TNE P1 P2 *` |
|        - |  4699 | ` *` |
|        - |  4700 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4701 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4702 | ` * instruction.` |
|        - |  4703 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4704 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4705 | ` *` |
|        - |  4706 | ` */` |
|   102828 |  4707 | `case PH7_OP_TNE: {` |
|   205658 |  4708 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4709 | `	/* Perform the comparison and act accordingly */` |
|        - |  4710 | `#ifdef UNTRUST` |
|        - |  4711 | `	if( pNos < pStack ){` |
|        - |  4712 | `		goto Abort;` |
|        - |  4713 | `	}` |
|        - |  4714 | `#endif` |
|   205658 |  4715 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   205658 |  4716 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4717 | `		rc = 1;` |
|        2 |  4718 | `	}else{` |
|   205656 |  4719 | `		rc = rc != 0;` |
|        - |  4720 | `	}` |
|   205658 |  4721 | `	VmPopOperand(&pTos,1);` |
|   205658 |  4722 | `	if( !pInstr->iP2 ){` |
|        - |  4723 | `		/* Push comparison result without taking the jump */` |
|   205658 |  4724 | `		PH7_MemObjRelease(pTos);` |
|   205658 |  4725 | `		pTos->x.iVal = rc;` |
|        - |  4726 | `		/* Invalidate any prior representation */` |
|   205658 |  4727 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102830 |  4728 | `	}else{` |
|      ! 0 |  4729 | `		if( rc ){` |
|        - |  4730 | `			/* Jump to the desired location */` |
|      ! 0 |  4731 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4732 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4733 | `		}` |
|        - |  4734 | `	}` |
|   205658 |  4735 | `	break;` |
|        - |  4736 | `				 }` |
|        - |  4737 | `/* OP_LT P1 P2 P3` |
|        - |  4738 | ` *` |
|        - |  4739 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4740 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4741 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4742 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4743 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4744 | ` *` |
|        - |  4745 | ` */` |
|        - |  4746 | `/* OP_LE P1 P2 P3` |
|        - |  4747 | ` *` |
|        - |  4748 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4749 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4750 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4751 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4752 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4753 | ` *` |
|        - |  4754 | ` */` |
|   102449 |  4755 | `case PH7_OP_LT:` |
|        - |  4756 | `case PH7_OP_LE: {` |
|   204944 |  4757 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4758 | `	/* Perform the comparison and act accordingly */` |
|        - |  4759 | `#ifdef UNTRUST` |
|        - |  4760 | `	if( pNos < pStack ){` |
|        - |  4761 | `		goto Abort;` |
|        - |  4762 | `	}` |
|        - |  4763 | `#endif` |
|   204944 |  4764 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204944 |  4765 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4766 | `		rc = 0;` |
|   204940 |  4767 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      430 |  4768 | `		rc = rc < 1;` |
|      216 |  4769 | `	}else{` |
|   204508 |  4770 | `		rc = rc < 0;` |
|        - |  4771 | `	}` |
|   204944 |  4772 | `	VmPopOperand(&pTos,1);` |
|   204944 |  4773 | `	if( !pInstr->iP2 ){` |
|        - |  4774 | `		/* Push comparison result without taking the jump */` |
|   204944 |  4775 | `		PH7_MemObjRelease(pTos);` |
|   204944 |  4776 | `		pTos->x.iVal = rc;` |
|        - |  4777 | `		/* Invalidate any prior representation */` |
|   204944 |  4778 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102495 |  4779 | `	}else{` |
|      ! 0 |  4780 | `		if( rc ){` |
|        - |  4781 | `			/* Jump to the desired location */` |
|      ! 0 |  4782 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4783 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4784 | `		}` |
|        - |  4785 | `	}` |
|   204944 |  4786 | `	break;` |
|        - |  4787 | `				}` |
|        - |  4788 | `/* OP_GT P1 P2 P3` |
|        - |  4789 | ` *` |
|        - |  4790 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4791 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4792 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4793 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4794 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4795 | ` *` |
|        - |  4796 | ` */` |
|        - |  4797 | `/* OP_GE P1 P2 P3` |
|        - |  4798 | ` *` |
|        - |  4799 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4800 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4801 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4802 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4803 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4804 | ` *` |
|        - |  4805 | ` */` |
|    48771 |  4806 | `case PH7_OP_GT:` |
|        - |  4807 | `case PH7_OP_GE: {` |
|    97544 |  4808 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4809 | `	/* Perform the comparison and act accordingly */` |
|        - |  4810 | `#ifdef UNTRUST` |
|        - |  4811 | `	if( pNos < pStack ){` |
|        - |  4812 | `		goto Abort;` |
|        - |  4813 | `	}` |
|        - |  4814 | `#endif` |
|    97544 |  4815 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97544 |  4816 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4817 | `		rc = 0;` |
|    97540 |  4818 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97388 |  4819 | `		rc = rc >= 0;` |
|    48695 |  4820 | `	}else{` |
|      150 |  4821 | `		rc = rc > 0;` |
|        - |  4822 | `	}` |
|    97544 |  4823 | `	VmPopOperand(&pTos,1);` |
|    97544 |  4824 | `	if( !pInstr->iP2 ){` |
|        - |  4825 | `		/* Push comparison result without taking the jump */` |
|    97544 |  4826 | `		PH7_MemObjRelease(pTos);` |
|    97544 |  4827 | `		pTos->x.iVal = rc;` |
|        - |  4828 | `		/* Invalidate any prior representation */` |
|    97544 |  4829 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48773 |  4830 | `	}else{` |
|      ! 0 |  4831 | `		if( rc ){` |
|        - |  4832 | `			/* Jump to the desired location */` |
|      ! 0 |  4833 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4834 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4835 | `		}` |
|        - |  4836 | `	}` |
|    97544 |  4837 | `	break;` |
|        - |  4838 | `				}` |
|        - |  4839 | `/* OP_SEQ P1 P2 *` |
|        - |  4840 | ` * Strict string comparison.` |
|        - |  4841 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4842 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4843 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4844 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4845 | ` * use PH7_OP_EQ.` |
|        - |  4846 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4847 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4848 | ` */` |
|        - |  4849 | `/* OP_SNE P1 P2 *` |
|        - |  4850 | ` * Strict string comparison.` |
|        - |  4851 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4852 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4853 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4854 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4855 | ` * use PH7_OP_EQ.` |
|        - |  4856 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4857 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4858 | ` */` |
|       18 |  4859 | `case PH7_OP_SEQ:` |
|        - |  4860 | `case PH7_OP_SNE: {` |
|       38 |  4861 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4862 | `	SyString s1,s2;` |
|        - |  4863 | `	/* Perform the comparison and act accordingly */` |
|        - |  4864 | `#ifdef UNTRUST` |
|        - |  4865 | `	if( pNos < pStack ){` |
|        - |  4866 | `		goto Abort;` |
|        - |  4867 | `	}` |
|        - |  4868 | `#endif` |
|        - |  4869 | `	/* Force a string cast */` |
|       38 |  4870 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4871 | `		PH7_MemObjToString(pTos);` |
|        2 |  4872 | `	}` |
|       38 |  4873 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4874 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4875 | `	}` |
|       38 |  4876 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4877 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4878 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4879 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4880 | `		rc = rc != 0;` |
|      ! 0 |  4881 | `	}else{` |
|       38 |  4882 | `		rc = rc == 0;` |
|        - |  4883 | `	}` |
|       38 |  4884 | `	VmPopOperand(&pTos,1);` |
|       38 |  4885 | `	if( !pInstr->iP2 ){` |
|        - |  4886 | `		/* Push comparison result without taking the jump */` |
|       38 |  4887 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4888 | `		pTos->x.iVal = rc;` |
|        - |  4889 | `		/* Invalidate any prior representation */` |
|       38 |  4890 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4891 | `	}else{` |
|      ! 0 |  4892 | `		if( rc ){` |
|        - |  4893 | `			/* Jump to the desired location */` |
|      ! 0 |  4894 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4895 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4896 | `		}` |
|        - |  4897 | `	}` |
|       38 |  4898 | `	break;` |
|        - |  4899 | `				 }` |
|        - |  4900 | `/*` |
|        - |  4901 | ` * OP_LOAD_REF * * *` |
|        - |  4902 | ` * Push the index of a referenced object on the stack.` |
|        - |  4903 | ` */` |
|       57 |  4904 | `case PH7_OP_LOAD_REF: {` |
|        - |  4905 | `	sxu32 nIdx;` |
|        - |  4906 | `#ifdef UNTRUST` |
|        - |  4907 | `	if( pTos < pStack ){` |
|        - |  4908 | `		goto Abort;` |
|        - |  4909 | `	}` |
|        - |  4910 | `#endif` |
|        - |  4911 | `	/* Extract memory object index */` |
|      115 |  4912 | `	nIdx = pTos->nIdx;` |
|      115 |  4913 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4914 | `		/* Nullify the object */` |
|       95 |  4915 | `		PH7_MemObjRelease(pTos);` |
|        - |  4916 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4917 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4918 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4919 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4920 | `	}` |
|      115 |  4921 | `	break;` |
|        - |  4922 | `					  }` |
|        - |  4923 | `/*` |
|        - |  4924 | ` * OP_STORE_REF * * P3` |
|        - |  4925 | ` * Perform an assignment operation by reference.` |
|        - |  4926 | ` */` |
|       15 |  4927 | ` case PH7_OP_STORE_REF: {` |
|       32 |  4928 | `	 SyString sName = { 0 , 0 };` |
|        - |  4929 | `	 VmFrame *pFrameLocal;` |
|        - |  4930 | `	SyHashEntry *pEntry;` |
|        - |  4931 | `	sxu32 nIdx;` |
|        - |  4932 | `#ifdef UNTRUST` |
|        - |  4933 | `	if( pTos < pStack ){` |
|        - |  4934 | `		goto Abort;` |
|        - |  4935 | `	}` |
|        - |  4936 | `#endif` |
|       32 |  4937 | `	if( pInstr->p3 == 0 ){` |
|        - |  4938 | `		char *zName;` |
|        - |  4939 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4940 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4941 | `			/* Force a string cast */` |
|      ! 0 |  4942 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4943 | `		}` |
|      ! 0 |  4944 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4945 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4946 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4947 | `			if( zName ){` |
|      ! 0 |  4948 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4949 | `			}` |
|      ! 0 |  4950 | `		}` |
|      ! 0 |  4951 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4952 | `		pTos--;` |
|      ! 0 |  4953 | `	}else{` |
|       32 |  4954 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4955 | `	}` |
|       32 |  4956 | `	nIdx = pTos->nIdx;` |
|       32 |  4957 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4958 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4959 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4960 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4961 | `		}else{` |
|        - |  4962 | `			ph7_value *pObj;` |
|        - |  4963 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4964 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4965 | `			if( pObj == 0 ){` |
|      ! 0 |  4966 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4967 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4968 | `				goto Abort;` |
|        - |  4969 | `			}` |
|        - |  4970 | `			/* Perform the store operation */` |
|      ! 0 |  4971 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4972 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4973 | `		}` |
|       32 |  4974 | `	}else if( sName.nByte > 0){` |
|       32 |  4975 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4976 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4977 | `		}else{` |
|       32 |  4978 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  4979 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4980 | `			/* Query the local frame */` |
|       32 |  4981 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  4982 | `			if( pEntry ){` |
|      ! 0 |  4983 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4984 | `			}else{` |
|       32 |  4985 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  4986 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4987 | `					/* Insert in the $GLOBALS array */` |
|       28 |  4988 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  4989 | `				}` |
|       32 |  4990 | `				if( rc == SXRET_OK ){` |
|       32 |  4991 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  4992 | `				}` |
|        - |  4993 | `			}` |
|        - |  4994 | `		}` |
|       15 |  4995 | `	}` |
|       32 |  4996 | `	break;` |
|        - |  4997 | `				 }` |
|        - |  4998 | `/*` |
|        - |  4999 | ` * OP_UPLINK P1 * *` |
|        - |  5000 | ` * Link a variable to the top active VM frame.` |
|        - |  5001 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5002 | ` */` |
|       25 |  5003 | `case PH7_OP_UPLINK: {` |
|       52 |  5004 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5005 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5006 | `		SyString sName;` |
|        - |  5007 | `		/* Perform the link */` |
|      104 |  5008 | `		while( pLink <= pTos ){` |
|       54 |  5009 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5010 | `				/* Force a string cast */` |
|      ! 0 |  5011 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5012 | `			}` |
|       54 |  5013 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5014 | `			if( sName.nByte > 0 ){` |
|       54 |  5015 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5016 | `			}` |
|       54 |  5017 | `			pLink++;` |
|        2 |  5018 | `		}` |
|       25 |  5019 | `	}` |
|       52 |  5020 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5021 | `	break;` |
|        - |  5022 | `					}` |
|        - |  5023 | `/*` |
|        - |  5024 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5025 | ` * Push an exception in the corresponding container so that` |
|        - |  5026 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5027 | ` */` |
|       32 |  5028 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5029 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5030 | `	VmFrame *pFrameLocal;` |
|        - |  5031 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5032 | `	pException->iFinallyDone = 0;` |
|       66 |  5033 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5034 | `	/* Create the exception frame */` |
|       66 |  5035 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5036 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5037 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5038 | `		goto Abort;` |
|        - |  5039 | `	}` |
|        - |  5040 | `	/* Mark the special frame */` |
|       66 |  5041 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5042 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5043 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5044 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5045 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5046 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5047 | `	break;` |
|        - |  5048 | `							}` |
|        - |  5049 | `/*` |
|        - |  5050 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5051 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5052 | ` */` |
|       31 |  5053 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5054 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5055 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5056 | `		ph7_exception **apException;` |
|        - |  5057 | `		/* Pop the loaded exception */` |
|       28 |  5058 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5059 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5060 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5061 | `		}` |
|       13 |  5062 | `	}` |
|       64 |  5063 | `	pException->pFrame = 0;` |
|        - |  5064 | `	/* Leave the exception frame */` |
|       64 |  5065 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5066 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5067 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5068 | `		sxi32 rcFinally;` |
|       19 |  5069 | `		pException->iFinallyDone = 1;` |
|       19 |  5070 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  5071 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5072 | `			goto Abort;` |
|        - |  5073 | `		}` |
|        9 |  5074 | `	}` |
|       64 |  5075 | `	break;` |
|        - |  5076 | `							}` |
|        - |  5077 |  |
|        - |  5078 | `/*` |
|        - |  5079 | ` * OP_THROW * P2 *` |
|        - |  5080 | ` * Throw an user exception.` |
|        - |  5081 | ` */` |
|       18 |  5082 | `case PH7_OP_THROW: {` |
|       38 |  5083 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5084 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5085 | `#ifdef UNTRUST` |
|        - |  5086 | `	if( pTos < pStack ){` |
|        - |  5087 | `		goto Abort;` |
|        - |  5088 | `	}` |
|        - |  5089 | `#endif` |
|       38 |  5090 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5091 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5092 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5093 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5094 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5095 | `		ph7_class *pException;` |
|        - |  5096 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5097 | `		 */` |
|       38 |  5098 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5099 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5100 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5101 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5102 | `			if( rc == SXERR_ABORT ){` |
|        - |  5103 | `				/* Abort processing immediately */` |
|      ! 0 |  5104 | `				goto Abort;` |
|        - |  5105 | `			}` |
|      ! 0 |  5106 | `		}else{` |
|        - |  5107 | `			/* Throw the exception */` |
|       38 |  5108 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5109 | `			if( rc == SXERR_ABORT ){` |
|        - |  5110 | `				/* Abort processing immediately */` |
|        9 |  5111 | `				goto Abort;` |
|        - |  5112 | `			}` |
|        - |  5113 | `		}` |
|       16 |  5114 | `	}else{` |
|        - |  5115 | `		/* Expecting a class instance */` |
|      ! 0 |  5116 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5117 | `		if( rc == SXERR_ABORT ){` |
|        - |  5118 | `			/* Abort processing immediately */` |
|      ! 0 |  5119 | `			goto Abort;` |
|        - |  5120 | `		}` |
|        - |  5121 | `	}` |
|        - |  5122 | `	/* Pop the top entry */` |
|       30 |  5123 | `	VmPopOperand(&pTos,1);` |
|        - |  5124 | `	/* Perform an unconditional jump */` |
|       30 |  5125 | `	pc = nJump - 1;` |
|       30 |  5126 | `	break;` |
|        - |  5127 | `				   }` |
|        - |  5128 | `/*` |
|        - |  5129 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5130 | ` * Prepare a foreach step.` |
|        - |  5131 | ` */` |
|     4961 |  5132 | `case PH7_OP_FOREACH_INIT: {` |
|     9924 |  5133 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5134 | `	void *pName;` |
|        - |  5135 | `#ifdef UNTRUST` |
|        - |  5136 | `	if( pTos < pStack ){` |
|        - |  5137 | `		goto Abort;` |
|        - |  5138 | `	}` |
|        - |  5139 | `#endif` |
|     9924 |  5140 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5141 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5142 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5143 | `			/* Force a string cast */` |
|      ! 0 |  5144 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5145 | `		}` |
|        - |  5146 | `		/* Duplicate name */` |
|      ! 0 |  5147 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5148 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5149 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5150 | `		}` |
|      ! 0 |  5151 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5152 | `	}` |
|     9924 |  5153 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5154 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5155 | `			/* Force a string cast */` |
|      ! 0 |  5156 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5157 | `		}` |
|        - |  5158 | `		/* Duplicate name */` |
|      ! 0 |  5159 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5160 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5161 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5162 | `		}` |
|      ! 0 |  5163 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5164 | `	}` |
|        - |  5165 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9924 |  5166 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5167 | `		/* Jump out of the loop */` |
|      ! 0 |  5168 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5169 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5170 | `		}` |
|      ! 0 |  5171 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5172 | `	}else{` |
|        - |  5173 | `		ph7_foreach_step *pStep;` |
|     9924 |  5174 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9924 |  5175 | `		if( pStep == 0 ){` |
|      ! 0 |  5176 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5177 | `			/* Jump out of the loop */` |
|      ! 0 |  5178 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5179 | `		}else{` |
|        - |  5180 | `			/* Zero the structure */` |
|     9924 |  5181 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5182 | `			/* Prepare the step */` |
|     9924 |  5183 | `			pStep->iFlags = pInfo->iFlags;` |
|     9924 |  5184 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5185 | `				ph7_hashmap *pMap;` |
|        - |  5186 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5187 | `				 * source array so mutations don't affect other sharers. */` |
|     9896 |  5188 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|       10 |  5189 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|       10 |  5190 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|       10 |  5191 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5192 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5193 | `						 * variable still points at the same hashmap as` |
|        - |  5194 | `						 * the stack value. */` |
|       10 |  5195 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|       10 |  5196 | `							pCur->iRef--;` |
|       10 |  5197 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|       10 |  5198 | `							pTos->x.pOther = pBacking->x.pOther;` |
|       10 |  5199 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5200 | `						}` |
|        4 |  5201 | `					}` |
|        4 |  5202 | `				}` |
|     9896 |  5203 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5204 | `				/* Reset the internal loop cursor */` |
|     9896 |  5205 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5206 | `				/* Mark the step */` |
|     9896 |  5207 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9896 |  5208 | `				pStep->xIter.pMap = pMap;` |
|     9896 |  5209 | `				pMap->iRef++;` |
|     4949 |  5210 | `			}else{` |
|       30 |  5211 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5212 | `				ph7_class *pIteratorClass;` |
|        - |  5213 | `				/* Check if the object implements Iterator */` |
|       30 |  5214 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5215 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5216 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5217 | `					ph7_class_method *pRewind;` |
|       19 |  5218 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       19 |  5219 | `					pStep->xIter.pThis = pThis;` |
|       19 |  5220 | `					pThis->iRef++;` |
|       19 |  5221 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       19 |  5222 | `					if( pRewind ){` |
|       19 |  5223 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5224 | `					}` |
|       10 |  5225 | `				}else{` |
|        - |  5226 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5227 | `					ph7_class *pIterAggClass;` |
|       12 |  5228 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5229 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5230 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5231 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5232 | `						ph7_class_method *pGetIter;` |
|        3 |  5233 | `						int iterAggOk = 0;` |
|        3 |  5234 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5235 | `						if( pGetIter ){` |
|        - |  5236 | `							ph7_value sResult;` |
|        3 |  5237 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5238 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5239 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5240 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5241 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5242 | `									ph7_class_method *pRewind;` |
|        3 |  5243 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5244 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5245 | `									pIterObj->iRef++;` |
|        - |  5246 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5247 | `									pStep->pOwner = pThis;` |
|        3 |  5248 | `									pThis->iRef++;` |
|        3 |  5249 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5250 | `									if( pRewind ){` |
|        3 |  5251 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5252 | `									}` |
|        3 |  5253 | `									iterAggOk = 1;` |
|        1 |  5254 | `								}` |
|        1 |  5255 | `							}` |
|        3 |  5256 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5257 | `						}` |
|        3 |  5258 | `						if( !iterAggOk ){` |
|        - |  5259 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5260 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5261 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5262 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5263 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5264 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5265 | `						}` |
|        2 |  5266 | `					}else{` |
|        - |  5267 | `						/* Plain object iteration via hAttr */` |
|        9 |  5268 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5269 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5270 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5271 | `						pThis->iRef++;` |
|        - |  5272 | `					}` |
|        - |  5273 | `				}` |
|        - |  5274 | `			}` |
|        - |  5275 | `		}` |
|     9924 |  5276 | `		if( pStep ){` |
|     9924 |  5277 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5278 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5279 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5280 | `				/* Jump out of the loop */` |
|      ! 0 |  5281 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5282 | `			}` |
|     4961 |  5283 | `		}` |
|        - |  5284 | `	}` |
|     9924 |  5285 | `	VmPopOperand(&pTos,1);` |
|     9924 |  5286 | `	break;` |
|        - |  5287 | `						  }` |
|        - |  5288 | `/*` |
|        - |  5289 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5290 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5291 | ` */` |
|    79979 |  5292 | `case PH7_OP_FOREACH_STEP: {` |
|   159960 |  5293 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5294 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5295 | `	ph7_value *pValue;` |
|        - |  5296 | `	VmFrame *pFrameLocal;` |
|        - |  5297 | `	/* Peek the last step */` |
|   159960 |  5298 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   159960 |  5299 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   159960 |  5300 | `	pFrameLocal = pVm->pFrame;` |
|   159960 |  5301 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   159960 |  5302 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   159848 |  5303 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5304 | `		ph7_hashmap_node *pNode;` |
|        - |  5305 | `		/* Extract the current node value */` |
|   159848 |  5306 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   159848 |  5307 | `		if( pNode == 0 ){` |
|        - |  5308 | `			/* No more entry to process */` |
|     9894 |  5309 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9894 |  5310 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5311 | `				/* Break the reference with the last element */` |
|        7 |  5312 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5313 | `			}` |
|        - |  5314 | `			/* Automatically reset the loop cursor */` |
|     9894 |  5315 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5316 | `			/* Cleanup the mess left behind */` |
|     9894 |  5317 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9894 |  5318 | `			SySetPop(&pInfo->aStep);` |
|     9894 |  5319 | `			PH7_HashmapUnref(pMap);` |
|     4948 |  5320 | `		}else{` |
|   149956 |  5321 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5322 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5323 | `				if( pKey ){` |
|      416 |  5324 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5325 | `				}` |
|      207 |  5326 | `			}` |
|   149956 |  5327 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5328 | `				SyHashEntry *pEntry;` |
|        - |  5329 | `				/* Pass by reference */` |
|       24 |  5330 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       24 |  5331 | `				if( pEntry ){` |
|       22 |  5332 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5333 | `				}else{` |
|        4 |  5334 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5335 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5336 | `				}` |
|       13 |  5337 | `			}else{` |
|        - |  5338 | `				/* Make a copy of the entry value */` |
|   149934 |  5339 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   149934 |  5340 | `				if( pValue ){` |
|   149934 |  5341 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    74966 |  5342 | `				}` |
|        - |  5343 | `			}` |
|        2 |  5344 | `		}` |
|    80037 |  5345 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5346 | `		/* Iterator-based iteration.` |
|        - |  5347 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5348 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5349 | `		 */` |
|       89 |  5350 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5351 | `		ph7_class_method *pMethod;` |
|        - |  5352 | `		ph7_value sResult;` |
|       89 |  5353 | `		int isValid = 0;` |
|        - |  5354 | `		/* Call next() to advance — but skip on the first iteration */` |
|       89 |  5355 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       21 |  5356 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       11 |  5357 | `		}else{` |
|       69 |  5358 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       69 |  5359 | `			if( pMethod ){` |
|       69 |  5360 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5361 | `			}` |
|        - |  5362 | `		}` |
|        - |  5363 | `		/* Call valid() */` |
|       89 |  5364 | `		PH7_MemObjInit(pVm,&sResult);` |
|       89 |  5365 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       89 |  5366 | `		if( pMethod ){` |
|       89 |  5367 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       89 |  5368 | `			PH7_MemObjToBool(&sResult);` |
|       89 |  5369 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5370 | `		}` |
|       89 |  5371 | `		PH7_MemObjRelease(&sResult);` |
|       89 |  5372 | `		if( !isValid ){` |
|        - |  5373 | `			/* Iterator exhausted */` |
|       19 |  5374 | `			pc = pInstr->iP2 - 1;` |
|        - |  5375 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       19 |  5376 | `			if( pStep->pOwner ){` |
|        3 |  5377 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5378 | `			}` |
|       19 |  5379 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       19 |  5380 | `			SySetPop(&pInfo->aStep);` |
|       19 |  5381 | `			PH7_ClassInstanceUnref(pThis);` |
|       10 |  5382 | `		}else{` |
|        - |  5383 | `			/* Call current() to get value */` |
|       71 |  5384 | `			PH7_MemObjInit(pVm,&sResult);` |
|       71 |  5385 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       71 |  5386 | `			if( pMethod ){` |
|       71 |  5387 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5388 | `			}` |
|       71 |  5389 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       71 |  5390 | `			if( pValue ){` |
|       71 |  5391 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5392 | `			}` |
|       71 |  5393 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5394 | `			/* Call key() if needed */` |
|       71 |  5395 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5396 | `				ph7_value sKey;` |
|       35 |  5397 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5398 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5399 | `				if( pMethod ){` |
|       35 |  5400 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5401 | `				}` |
|       35 |  5402 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5403 | `				if( pValue ){` |
|       35 |  5404 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5405 | `				}` |
|       35 |  5406 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5407 | `			}` |
|        - |  5408 | `		}` |
|       45 |  5409 | `	}else{` |
|       25 |  5410 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5411 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5412 | `		SyHashEntry *pEntry;` |
|        - |  5413 | `		/* Point to the next attribute */` |
|       29 |  5414 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5415 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5416 | `			/* Check access permission */` |
|       31 |  5417 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5418 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5419 | `					break; /* Access is granted */` |
|        - |  5420 | `			}` |
|        1 |  5421 | `		}` |
|       25 |  5422 | `		if( pEntry == 0 ){` |
|        - |  5423 | `			/* Clean up the mess left behind */` |
|        9 |  5424 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5425 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5426 | `				/* Break the reference with the last element */` |
|        3 |  5427 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5428 | `			}` |
|        9 |  5429 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5430 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5431 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5432 | `		}else{` |
|       17 |  5433 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5434 | `			ph7_value *pAttrValue;` |
|       17 |  5435 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5436 | `				/* Fill with the current attribute name */` |
|       17 |  5437 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5438 | `				if( pKey ){` |
|       17 |  5439 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5440 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5441 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5442 | `				}` |
|        8 |  5443 | `			}` |
|        - |  5444 | `			/* Extract attribute value */` |
|       17 |  5445 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5446 | `			if( pAttrValue ){` |
|       17 |  5447 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5448 | `					/* Pass by reference */` |
|        3 |  5449 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5450 | `					if( pEntry ){` |
|        3 |  5451 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5452 | `					}else{` |
|      ! 0 |  5453 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5454 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5455 | `					}` |
|        2 |  5456 | `				}else{` |
|        - |  5457 | `					/* Make a copy of the attribute value */` |
|       15 |  5458 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5459 | `					if( pValue ){` |
|       15 |  5460 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5461 | `					}` |
|        - |  5462 | `				}` |
|        8 |  5463 | `			}` |
|        - |  5464 | `		}` |
|        - |  5465 | `	}` |
|   159960 |  5466 | `	break;` |
|        - |  5467 | `						  }` |
|        - |  5468 | `/*` |
|        - |  5469 | ` * OP_MEMBER P1 P2` |
|        - |  5470 | ` * Load class attribute/method on the stack.` |
|        - |  5471 | ` */` |
|     2180 |  5472 | `case PH7_OP_MEMBER: {` |
|        - |  5473 | `	ph7_class_instance *pThis;` |
|        - |  5474 | `	ph7_value *pNos;` |
|        - |  5475 | `	SyString sName;` |
|     4362 |  5476 | `	if( !pInstr->iP1 ){` |
|     4226 |  5477 | `		pNos = &pTos[-1];` |
|        - |  5478 | `#ifdef UNTRUST` |
|        - |  5479 | `		if( pNos < pStack ){` |
|        - |  5480 | `			goto Abort;` |
|        - |  5481 | `		}` |
|        - |  5482 | `#endif` |
|     4226 |  5483 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5484 | `			ph7_class *pClass;` |
|        - |  5485 | `			/* Class already instantiated */` |
|     4226 |  5486 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5487 | `			/* Point to the instantiated class */` |
|     4226 |  5488 | `			pClass = pThis->pClass;` |
|        - |  5489 | `			/* Extract attribute name first */` |
|     4226 |  5490 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4226 |  5491 | `			if( pInstr->iP2 ){` |
|        - |  5492 | `				/* Method call */` |
|      406 |  5493 | `				ph7_class_method *pMeth = 0;` |
|      406 |  5494 | `				if( sName.nByte > 0 ){` |
|        - |  5495 | `					/* Extract the target method */` |
|      406 |  5496 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      202 |  5497 | `				}` |
|      406 |  5498 | `				if( pMeth == 0 ){` |
|      ! 0 |  5499 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5500 | `						&pClass->sName,&sName` |
|        - |  5501 | `						);` |
|        - |  5502 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5503 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5504 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5505 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5506 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5507 | `				}else{` |
|        - |  5508 | `					/* Push method name on the stack */` |
|      406 |  5509 | `					PH7_MemObjRelease(pTos);` |
|      406 |  5510 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      406 |  5511 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5512 | `				}` |
|      406 |  5513 | `				pTos->nIdx = SXU32_HIGH;` |
|      204 |  5514 | `			}else{` |
|        - |  5515 | `				/* Attribute access */` |
|     3822 |  5516 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5517 | `				SyHashEntry *pEntry;` |
|        - |  5518 | `				/* Extract the target attribute */` |
|     3822 |  5519 | `				if( sName.nByte > 0 ){` |
|     3822 |  5520 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3822 |  5521 | `					if( pEntry ){` |
|        - |  5522 | `						/* Point to the attribute value */` |
|     3820 |  5523 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1909 |  5524 | `					}` |
|     1910 |  5525 | `				}` |
|     3822 |  5526 | `				if( pObjAttr == 0 ){` |
|        - |  5527 | `					/* No such attribute,load null */` |
|        4 |  5528 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5529 | `						&pClass->sName,&sName);` |
|        - |  5530 | `					/* Call the __get magic method if available */` |
|        3 |  5531 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5532 | `				}` |
|     3822 |  5533 | `				VmPopOperand(&pTos,1);` |
|        - |  5534 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5535 | `				 * This is due to the following case:` |
|        - |  5536 | `				 *     (new TestClass())->foo;` |
|        - |  5537 | `				 */` |
|     3822 |  5538 | `				pThis->iRef++;` |
|     3822 |  5539 | `				PH7_MemObjRelease(pTos);` |
|     3822 |  5540 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3822 |  5541 | `				if( pObjAttr ){` |
|     3820 |  5542 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5543 | `					/* Check attribute access */` |
|     3820 |  5544 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5545 | `						/* Load attribute */` |
|     3820 |  5546 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3820 |  5547 | `						if( pValue ){` |
|     3820 |  5548 | `							if( pThis->iRef < 2 ){` |
|        - |  5549 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5550 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5551 | `								 */` |
|        3 |  5552 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5553 | `							}else{` |
|        - |  5554 | `								/* Simple load */` |
|     3818 |  5555 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5556 | `							}` |
|     3820 |  5557 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3818 |  5558 | `								if( pThis->iRef > 1 ){` |
|        - |  5559 | `									/* Load attribute index */` |
|     3816 |  5560 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1907 |  5561 | `								}` |
|     1908 |  5562 | `							}` |
|     1909 |  5563 | `						}` |
|     1909 |  5564 | `					}` |
|     1909 |  5565 | `				}` |
|        - |  5566 | `				/* Safely unreference the object */` |
|     3822 |  5567 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5568 | `			}` |
|     2114 |  5569 | `		}else{` |
|      ! 0 |  5570 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5571 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5572 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5573 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5574 | `		}` |
|     2114 |  5575 | `	}else{` |
|        - |  5576 | `		/* Static member access using class name */` |
|      138 |  5577 | `		pNos = pTos;` |
|      138 |  5578 | `		pThis = 0;` |
|      138 |  5579 | `		if( !pInstr->p3 ){` |
|      126 |  5580 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      126 |  5581 | `			pNos--;` |
|        - |  5582 | `#ifdef UNTRUST` |
|        - |  5583 | `			if( pNos < pStack ){` |
|        - |  5584 | `				goto Abort;` |
|        - |  5585 | `			}` |
|        - |  5586 | `#endif` |
|       64 |  5587 | `		}else{` |
|        - |  5588 | `			/* Attribute name already computed */` |
|       14 |  5589 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5590 | `		}` |
|      138 |  5591 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      138 |  5592 | `			ph7_class *pClass = 0;` |
|      138 |  5593 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5594 | `				/* Class already instantiated */` |
|      ! 0 |  5595 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5596 | `				pClass = pThis->pClass;` |
|      ! 0 |  5597 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5598 | `			}else{` |
|        - |  5599 | `				/* Try to extract the target class */` |
|      138 |  5600 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      138 |  5601 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      138 |  5602 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5603 | `					/* Handle self/static/parent keywords */` |
|      138 |  5604 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5605 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5606 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5607 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5608 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5609 | `						}` |
|      124 |  5610 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5611 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      109 |  5612 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5613 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5614 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5615 | `							pClass = pSelf->pBase;` |
|        6 |  5616 | `						}` |
|        8 |  5617 | `					}else{` |
|       84 |  5618 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5619 | `					}` |
|       68 |  5620 | `				}` |
|        - |  5621 | `			}` |
|      138 |  5622 | `			if( pClass == 0 ){` |
|        - |  5623 | `				/* Undefined class */` |
|      ! 0 |  5624 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5625 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5626 | `					);` |
|      ! 0 |  5627 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5628 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5629 | `				}` |
|      ! 0 |  5630 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5631 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5632 | `			}else{` |
|      138 |  5633 | `				if( pInstr->iP2 ){` |
|        - |  5634 | `					/* Method call */` |
|       68 |  5635 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5636 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5637 | `						/* Extract the target method */` |
|       68 |  5638 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5639 | `					}` |
|       68 |  5640 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5641 | `						if( pMeth ){` |
|      ! 0 |  5642 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5643 | `								&pClass->sName,&sName` |
|        - |  5644 | `								);` |
|      ! 0 |  5645 | `						}else{` |
|      ! 0 |  5646 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5647 | `								&pClass->sName,&sName` |
|        - |  5648 | `								);` |
|        - |  5649 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5650 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5651 | `						}` |
|        - |  5652 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5653 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5654 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5655 | `						}` |
|      ! 0 |  5656 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5657 | `					}else{` |
|        - |  5658 | `						/* Push method name on the stack */` |
|       68 |  5659 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5660 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5661 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5662 | `					}` |
|       68 |  5663 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5664 | `				}else{` |
|        - |  5665 | `					/* Attribute access */` |
|       72 |  5666 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5667 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5668 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5669 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5670 | `						/* ::class returns the fully qualified class name */` |
|        - |  5671 | `						/* Pop the attribute name from the stack */` |
|       54 |  5672 | `						if( !pInstr->p3 ){` |
|       54 |  5673 | `							VmPopOperand(&pTos,1);` |
|       26 |  5674 | `						}` |
|       54 |  5675 | `						PH7_MemObjRelease(pTos);` |
|        - |  5676 | `						/* Load the class name */` |
|       54 |  5677 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5678 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5679 | `					}else{` |
|        - |  5680 | `						/* Extract the target attribute */` |
|       20 |  5681 | `						if( sName.nByte > 0 ){` |
|       20 |  5682 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5683 | `						}` |
|       20 |  5684 | `						if( pAttr == 0 ){` |
|        - |  5685 | `							/* No such attribute,load null */` |
|      ! 0 |  5686 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5687 | `								&pClass->sName,&sName);` |
|        - |  5688 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5689 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5690 | `						}` |
|        - |  5691 | `						/* Pop the attribute name from the stack */` |
|       20 |  5692 | `						if( !pInstr->p3 ){` |
|        7 |  5693 | `							VmPopOperand(&pTos,1);` |
|        3 |  5694 | `						}` |
|       20 |  5695 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5696 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5697 | `						if( pAttr ){` |
|       20 |  5698 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5699 | `								/* Access to a non static attribute */` |
|      ! 0 |  5700 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5701 | `									&pClass->sName,&pAttr->sName` |
|        - |  5702 | `									);` |
|      ! 0 |  5703 | `							}else{` |
|        - |  5704 | `								ph7_value *pValue;` |
|        - |  5705 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5706 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5707 | `									/* Load the desired attribute */` |
|       20 |  5708 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5709 | `									if( pValue ){` |
|       20 |  5710 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5711 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5712 | `											/* Load index number */` |
|       14 |  5713 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5714 | `										}` |
|        9 |  5715 | `									}` |
|        9 |  5716 | `								}` |
|        - |  5717 | `							}` |
|        9 |  5718 | `						}` |
|        - |  5719 | `					}` |
|        - |  5720 | `				}` |
|      138 |  5721 | `				if( pThis ){` |
|        - |  5722 | `					/* Safely unreference the object */` |
|      ! 0 |  5723 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5724 | `				}` |
|        - |  5725 | `			}` |
|       70 |  5726 | `		}else{` |
|        - |  5727 | `			/* Pop operands */` |
|      ! 0 |  5728 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5729 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5730 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5731 | `			}` |
|      ! 0 |  5732 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5733 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5734 | `		}` |
|        - |  5735 | `	}` |
|     4362 |  5736 | `	break;` |
|        - |  5737 | `					}` |
|        - |  5738 | `/*` |
|        - |  5739 | ` * OP_NEW P1 * * *` |
|        - |  5740 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5741 | ` */` |
|      318 |  5742 | `case PH7_OP_NEW: {` |
|      638 |  5743 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      638 |  5744 | `	ph7_class *pClass = 0;` |
|        - |  5745 | `	ph7_class_instance *pNew;` |
|      638 |  5746 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5747 | `		/* Try to extract the desired class */` |
|      956 |  5748 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      636 |  5749 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      318 |  5750 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5751 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5752 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5753 | `	}` |
|      638 |  5754 | `	if( pClass == 0 ){` |
|        - |  5755 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5756 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5757 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5758 | `			);` |
|        - |  5759 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5760 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5761 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5762 | `			/* Pop given arguments */` |
|      ! 0 |  5763 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5764 | `		}` |
|      ! 0 |  5765 | `		goto Abort;` |
|      ! 0 |  5766 | `	}else{` |
|        - |  5767 | `		ph7_class_method *pCons;` |
|        - |  5768 | `		/* Create a new class instance */` |
|      638 |  5769 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      638 |  5770 | `		if( pNew == 0 ){` |
|      ! 0 |  5771 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5772 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5773 | `				&pClass->sName` |
|        - |  5774 | `			);` |
|      ! 0 |  5775 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5776 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5777 | `				/* Pop given arguments */` |
|      ! 0 |  5778 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5779 | `			}` |
|      ! 0 |  5780 | `			break;` |
|        - |  5781 | `		}` |
|        - |  5782 | `		/* Check if a constructor is available */` |
|      638 |  5783 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      638 |  5784 | `		if( pCons == 0 ){` |
|      528 |  5785 | `			SyString *pName = &pClass->sName;` |
|        - |  5786 | `			/* Check for a constructor with the same base class name */` |
|      528 |  5787 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      263 |  5788 | `		}` |
|      638 |  5789 | `		if( pCons ){` |
|        - |  5790 | `			/* Call the class constructor */` |
|      112 |  5791 | `			SySetReset(&aArg);` |
|      212 |  5792 | `			while( pArg < pTos ){` |
|      102 |  5793 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      102 |  5794 | `				pArg++;` |
|        2 |  5795 | `			}` |
|      112 |  5796 | `			if( pVm->bErrReport ){` |
|        - |  5797 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5798 | `				sxu32 n;` |
|       69 |  5799 | `				n = SySetUsed(&aArg);` |
|        - |  5800 | `				/* Emit a notice for missing arguments */` |
|      125 |  5801 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       57 |  5802 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       57 |  5803 | `					if( pFuncArg ){` |
|       57 |  5804 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5805 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5806 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5807 | `						}` |
|       28 |  5808 | `					}` |
|       57 |  5809 | `					n++;` |
|        1 |  5810 | `				}` |
|       34 |  5811 | `			}` |
|      112 |  5812 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5813 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      112 |  5814 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5815 | `				pNew->iRef = 1;` |
|      ! 0 |  5816 | `			}` |
|       55 |  5817 | `		}` |
|      638 |  5818 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5819 | `			/* Pop given arguments */` |
|       94 |  5820 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       46 |  5821 | `		}` |
|      638 |  5822 | `		PH7_MemObjRelease(pTos);` |
|      638 |  5823 | `		pTos->x.pOther = pNew;` |
|      638 |  5824 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5825 | `	}` |
|      638 |  5826 | `	break;` |
|        - |  5827 | `				 }` |
|        - |  5828 | `/*` |
|        - |  5829 | ` * OP_CLONE * * *` |
|        - |  5830 | ` * Perfome a clone operation.` |
|        - |  5831 | ` */` |
|       23 |  5832 | `case PH7_OP_CLONE: {` |
|        - |  5833 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5834 | `#ifdef UNTRUST` |
|        - |  5835 | `	if( pTos < pStack ){` |
|        - |  5836 | `		goto Abort;` |
|        - |  5837 | `	}` |
|        - |  5838 | `#endif` |
|        - |  5839 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5840 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5841 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5842 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5843 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5844 | `		break;` |
|        - |  5845 | `	}` |
|        - |  5846 | `	/* Point to the source */` |
|       44 |  5847 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5848 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  5849 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  5850 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5851 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  5852 | `			&pSrc->pClass->sName);` |
|      ! 0 |  5853 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5854 | `		break;` |
|        - |  5855 | `	}` |
|        - |  5856 | `	/* Perform the clone operation */` |
|       44 |  5857 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5858 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5859 | `	if( pClone == 0 ){` |
|      ! 0 |  5860 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5861 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5862 | `	}else{` |
|        - |  5863 | `		/* Load the cloned object */` |
|       44 |  5864 | `		pTos->x.pOther = pClone;` |
|       44 |  5865 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5866 | `	}` |
|       44 |  5867 | `	break;` |
|        - |  5868 | `				   }` |
|        - |  5869 | `/*` |
|        - |  5870 | ` * OP_SWITCH * * P3` |
|        - |  5871 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5872 | ` */` |
|       18 |  5873 | `case PH7_OP_SWITCH: {` |
|       38 |  5874 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5875 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5876 | `	ph7_value sValue,sCaseValue;` |
|        - |  5877 | `	sxu32 n,nEntry;` |
|        - |  5878 | `#ifdef UNTRUST` |
|        - |  5879 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5880 | `		goto Abort;` |
|        - |  5881 | `	}` |
|        - |  5882 | `#endif` |
|        - |  5883 | `	/* Point to the case table  */` |
|       38 |  5884 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5885 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5886 | `	/* Select the appropriate case block to execute */` |
|       38 |  5887 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5888 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5889 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5890 | `		pCase = &aCase[n];` |
|       92 |  5891 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5892 | `		/* Execute the case expression first */` |
|       92 |  5893 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5894 | `		/* Compare the two expression */` |
|       92 |  5895 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5896 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5897 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5898 | `		if( rc == 0 ){` |
|        - |  5899 | `			/* Value match,jump to this block */` |
|       38 |  5900 | `			pc = pCase->nStart - 1;` |
|       38 |  5901 | `			break;` |
|        - |  5902 | `		}` |
|       29 |  5903 | `	}` |
|       38 |  5904 | `	VmPopOperand(&pTos,1);` |
|       38 |  5905 | `	if( n >= nEntry ){` |
|        - |  5906 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5907 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5908 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5909 | `		}else{` |
|        - |  5910 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5911 | `			pc = pSwitch->nOut - 1;` |
|        - |  5912 | `		}` |
|      ! 0 |  5913 | `	}` |
|       38 |  5914 | `	break;` |
|        - |  5915 | `					}` |
|        - |  5916 | `/*` |
|        - |  5917 | ` * OP_YIELD P1 P2 *` |
|        - |  5918 | ` *  Yield a value from a generator function.` |
|        - |  5919 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  5920 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  5921 | ` */` |
|       28 |  5922 | `case PH7_OP_YIELD: {` |
|        - |  5923 | `	ph7_generator *pGen;` |
|       57 |  5924 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  5925 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  5926 | `		goto Abort;` |
|        - |  5927 | `	}` |
|       57 |  5928 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       57 |  5929 | `	if( pInstr->iP2 ){` |
|        - |  5930 | `		/* yield $key => $value: value on top, key below */` |
|        - |  5931 | `#ifdef UNTRUST` |
|        - |  5932 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  5933 | `#endif` |
|        7 |  5934 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  5935 | `		VmPopOperand(&pTos, 1);` |
|        7 |  5936 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  5937 | `		VmPopOperand(&pTos, 1);` |
|        - |  5938 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  5939 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  5940 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  5941 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  5942 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  5943 | `			}` |
|        1 |  5944 | `		}` |
|       54 |  5945 | `	}else if( pInstr->iP1 ){` |
|        - |  5946 | `		/* yield $value */` |
|        - |  5947 | `#ifdef UNTRUST` |
|        - |  5948 | `		if( pTos < pStack ) goto Abort;` |
|        - |  5949 | `#endif` |
|       51 |  5950 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       51 |  5951 | `		VmPopOperand(&pTos, 1);` |
|        - |  5952 | `		/* Auto-increment key */` |
|       51 |  5953 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       51 |  5954 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       51 |  5955 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       26 |  5956 | `	}else{` |
|        - |  5957 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  5958 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  5959 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  5960 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  5961 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  5962 | `	}` |
|        - |  5963 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       57 |  5964 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       57 |  5965 | `	goto Suspend;` |
|        - |  5966 |  |
|        - |  5967 | `/*` |
|        - |  5968 | ` * OP_CALL P1 * *` |
|        - |  5969 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5970 | ` *  function on the stack.` |
|        - |  5971 | ` */` |
|   290891 |  5972 | `case PH7_OP_CALL: {` |
|   581828 |  5973 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5974 | `	SyHashEntry *pEntry;` |
|        - |  5975 | `	SyString sName;` |
|        - |  5976 | `	/* Extract function name */` |
|   581828 |  5977 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5978 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5979 | `			ph7_value sResult;` |
|      ! 0 |  5980 | `			SySetReset(&aArg);` |
|      ! 0 |  5981 | `			while( pArg < pTos ){` |
|      ! 0 |  5982 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5983 | `				pArg++;` |
|      ! 0 |  5984 | `			}` |
|      ! 0 |  5985 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5986 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5987 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5988 | `			SySetReset(&aArg);` |
|        - |  5989 | `			/* Pop given arguments */` |
|      ! 0 |  5990 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5991 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5992 | `			}` |
|        - |  5993 | `			/* Copy result */` |
|      ! 0 |  5994 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5995 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5996 | `		}else{` |
|        3 |  5997 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5998 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5999 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6000 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6001 | `			}else{` |
|        - |  6002 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6003 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6004 | `			}` |
|        - |  6005 | `			/* Pop given arguments */` |
|        3 |  6006 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6007 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6008 | `			}` |
|        - |  6009 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6010 | `			PH7_MemObjRelease(pTos);` |
|        - |  6011 | `		}` |
|   290618 |  6012 | `		break;` |
|        - |  6013 | `	}` |
|   581826 |  6014 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6015 | `	/* Check for a compiled function first.` |
|        - |  6016 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6017 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   581826 |  6018 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6019 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6020 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6021 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6022 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6023 | `	 * function calls inside namespaces. */` |
|   581826 |  6024 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6025 | `		const char *zFunc;` |
|        - |  6026 | `		const char *zEnd;` |
|        - |  6027 | `		const char *z;` |
|        - |  6028 | `		SyString sGlobal;` |
|       15 |  6029 | `		zFunc = sName.zString;` |
|       15 |  6030 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  6031 | `		z = zEnd;` |
|        - |  6032 | `		/* Find last namespace separator */` |
|      133 |  6033 | `		while( z > zFunc ){` |
|      133 |  6034 | `			if( z[-1] == '\\' ){` |
|       15 |  6035 | `				break;` |
|        - |  6036 | `			}` |
|      119 |  6037 | `			z--;` |
|        1 |  6038 | `		}` |
|       15 |  6039 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6040 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  6041 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  6042 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  6043 | `		}` |
|        7 |  6044 | `	}` |
|   581826 |  6045 | `	if( pEntry ){` |
|        - |  6046 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6047 | `		ph7_class_instance *pThis;` |
|        - |  6048 | `		ph7_value *pFrameStack;` |
|        - |  6049 | `		ph7_vm_func *pVmFunc;` |
|        - |  6050 | `		ph7_class *pSelf;` |
|        - |  6051 | `		VmFrame *pFrame;` |
|        - |  6052 | `		ph7_value *pObj;` |
|        - |  6053 | `		VmSlot sArg;` |
|        - |  6054 | `		sxu32 n;` |
|        - |  6055 | `		/* initialize fields */` |
|    13244 |  6056 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13244 |  6057 | `		pThis = 0;` |
|    13244 |  6058 | `		pSelf = 0;` |
|    13244 |  6059 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6060 | `			ph7_class_method *pMeth;` |
|        - |  6061 | `			/* Class method call */` |
|     1956 |  6062 | `			ph7_value *pTarget = &pTos[-1];` |
|     1956 |  6063 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6064 | `				/* Extract the 'this' pointer */` |
|     1956 |  6065 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6066 | `					/* Instance already loaded */` |
|     1884 |  6067 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1884 |  6068 | `					pThis->iRef++;` |
|     1884 |  6069 | `					pSelf = pThis->pClass;` |
|      941 |  6070 | `				}` |
|     1956 |  6071 | `				if( pSelf == 0 ){` |
|       74 |  6072 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6073 | `						/* "Late Static Binding" class name */` |
|      101 |  6074 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6075 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6076 | `					}` |
|       74 |  6077 | `					if( pSelf == 0 ){` |
|       13 |  6078 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6079 | `					}` |
|       36 |  6080 | `				}` |
|     1956 |  6081 | `				if( pThis == 0  ){` |
|       74 |  6082 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6083 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6084 | `					if( pFrameLocal->pParent ){` |
|        - |  6085 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6086 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6087 | `						if( pThis ){` |
|       13 |  6088 | `							pThis->iRef++;` |
|        6 |  6089 | `						}` |
|       28 |  6090 | `					}` |
|       36 |  6091 | `				}` |
|     1956 |  6092 | `				VmPopOperand(&pTos,1);` |
|     1956 |  6093 | `				PH7_MemObjRelease(pTos);` |
|        - |  6094 | `				/* Synchronize pointers */` |
|     1956 |  6095 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  6096 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6097 | `				 * user have already computed the random generated unique class method name` |
|        - |  6098 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6099 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6100 | `				 */` |
|     1956 |  6101 | `				while( pArg < pStack ){` |
|      ! 0 |  6102 | `					pArg++;` |
|      ! 0 |  6103 | `				}` |
|     1956 |  6104 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6105 | `					/* Check if the call is allowed */` |
|     1956 |  6106 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1956 |  6107 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6108 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6109 | `							/* Pop given arguments */` |
|      ! 0 |  6110 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6111 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6112 | `							}` |
|        - |  6113 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6114 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6115 | `							break;` |
|        - |  6116 | `						}` |
|        3 |  6117 | `					}` |
|      977 |  6118 | `				}` |
|      977 |  6119 | `			}` |
|      977 |  6120 | `		}` |
|        - |  6121 | `		/* Check The recursion limit */` |
|    13244 |  6122 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6123 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6124 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6125 | `				&pVmFunc->sName);` |
|        - |  6126 | `			/* Pop given arguments */` |
|        3 |  6127 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6128 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6129 | `			}` |
|        - |  6130 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6131 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6132 | `			break;` |
|        - |  6133 | `		}` |
|    13242 |  6134 | `		if( pVmFunc->pNextName ){` |
|        - |  6135 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6136 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6137 | `		}` |
|    13242 |  6138 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6139 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6140 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6141 | `			ph7_generator *pGenerator;` |
|        - |  6142 | `			ph7_class_instance *pGenObj;` |
|        - |  6143 | `			ph7_value *pCtxAttr;` |
|        - |  6144 | `			SyString sAttrName;` |
|        - |  6145 | `			ph7_value **apCallArgs;` |
|        - |  6146 | `			int nCallArgs, iArg;` |
|        - |  6147 | `			/* Collect arguments from the operand stack */` |
|       19 |  6148 | `			nCallArgs = (int)(pTos - pArg);` |
|       19 |  6149 | `			apCallArgs = 0;` |
|       19 |  6150 | `			if( nCallArgs > 0 ){` |
|        7 |  6151 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6152 | `					nCallArgs * sizeof(ph7_value *));` |
|        5 |  6153 | `				if( apCallArgs == 0 ){` |
|        - |  6154 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6155 | `					nCallArgs = 0;` |
|      ! 0 |  6156 | `				}else{` |
|       11 |  6157 | `					for( iArg = 0; iArg < nCallArgs; iArg++ ){` |
|        7 |  6158 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        4 |  6159 | `					}` |
|        - |  6160 | `				}` |
|        2 |  6161 | `			}` |
|        - |  6162 | `			/* Create execution context and generator wrapper */` |
|       19 |  6163 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       19 |  6164 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6165 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6166 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6167 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6168 | `				break;` |
|        - |  6169 | `			}` |
|       19 |  6170 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       19 |  6171 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6172 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6173 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6174 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6175 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6176 | `				break;` |
|        - |  6177 | `			}` |
|        - |  6178 | `			/* Set up the frame with arguments, closure env, $this */` |
|       19 |  6179 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       19 |  6180 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       19 |  6181 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nCallArgs, apCallArgs);` |
|       19 |  6182 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       19 |  6183 | `			pExecCtx->pFrame->pParent = 0;` |
|       19 |  6184 | `			if( apCallArgs ){` |
|        5 |  6185 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6186 | `			}` |
|       19 |  6187 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6188 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6189 | `				if( pThis ){` |
|      ! 0 |  6190 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6191 | `				}` |
|      ! 0 |  6192 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6193 | `					goto Abort;` |
|        - |  6194 | `				}` |
|      ! 0 |  6195 | `				break;` |
|        - |  6196 | `			}` |
|        - |  6197 | `			/* Create Generator class instance */` |
|       19 |  6198 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       19 |  6199 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6200 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6201 | `				break;` |
|        - |  6202 | `			}` |
|        - |  6203 | `			/* Store generator in __ctx attribute */` |
|       19 |  6204 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       19 |  6205 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       19 |  6206 | `			if( pCtxAttr ){` |
|       19 |  6207 | `				pCtxAttr->x.pOther = pGenerator;` |
|       19 |  6208 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6209 | `			}` |
|        - |  6210 | `			/* Pop args and function name, push Generator object */` |
|       19 |  6211 | `			PH7_MemObjRelease(pTos);` |
|       19 |  6212 | `			pTos = &pTos[-pInstr->iP1];` |
|       19 |  6213 | `			pTos->x.pOther = pGenObj;` |
|       19 |  6214 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       19 |  6215 | `			pGenObj->iRef++;` |
|       19 |  6216 | `			if( pThis ){` |
|      ! 0 |  6217 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6218 | `			}` |
|       19 |  6219 | `			break;` |
|        - |  6220 | `		}` |
|        - |  6221 | `		/* Extract the formal argument set */` |
|    13224 |  6222 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6223 | `		/* Create a new VM frame  */` |
|    13224 |  6224 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13224 |  6225 | `		if( rc != SXRET_OK ){` |
|        - |  6226 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6227 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6228 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6229 | `				&pVmFunc->sName);` |
|        - |  6230 | `			/* Pop given arguments */` |
|      ! 0 |  6231 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6232 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6233 | `			}` |
|        - |  6234 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6235 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6236 | `			break;` |
|        - |  6237 | `		}` |
|    13224 |  6238 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6239 | `			/* Install the '$this' variable */` |
|        - |  6240 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1894 |  6241 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1894 |  6242 | `			if( pObj ){` |
|        - |  6243 | `				/* Reflect the change */` |
|     1894 |  6244 | `				pObj->x.pOther = pThis;` |
|     1894 |  6245 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      946 |  6246 | `			}` |
|      946 |  6247 | `		}` |
|    13224 |  6248 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6249 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6250 | `			/* Install static variables */` |
|      ! 0 |  6251 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6252 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6253 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6254 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6255 | `					/* Initialize the static variables */` |
|      ! 0 |  6256 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6257 | `					if( pObj ){` |
|        - |  6258 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6259 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6260 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6261 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6262 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6263 | `						}` |
|      ! 0 |  6264 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6265 | `					}else{` |
|      ! 0 |  6266 | `						continue;` |
|        - |  6267 | `					}` |
|      ! 0 |  6268 | `				}` |
|        - |  6269 | `				/* Install in the current frame */` |
|      ! 0 |  6270 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6271 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6272 | `			}` |
|      ! 0 |  6273 | `		}` |
|        - |  6274 | `		/* Push arguments in the local frame */` |
|    13224 |  6275 | `		n = 0;` |
|    36020 |  6276 | `		while( pArg < pTos ){` |
|    22798 |  6277 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22644 |  6278 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  6279 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  6280 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6281 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6282 | `						goto Abort;` |
|        - |  6283 | `					}` |
|      ! 0 |  6284 | `				}` |
|        - |  6285 | `				/* Make sure the given arguments are of the correct type */` |
|    22644 |  6286 | `				if( aFormalArg[n].nType > 0 ){` |
|     1110 |  6287 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6288 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  6289 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6290 | `						ph7_class *pClass;` |
|        - |  6291 | `						/* Try to extract the desired class */` |
|      ! 0 |  6292 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  6293 | `						if( pClass ){` |
|      ! 0 |  6294 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6295 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6296 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6297 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6298 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6299 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6300 | `								}` |
|      ! 0 |  6301 | `							}else{` |
|        - |  6302 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  6303 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6304 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  6305 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6306 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6307 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6308 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6309 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6310 | `								}` |
|        - |  6311 | `							}` |
|      ! 0 |  6312 | `						}` |
|     1110 |  6313 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6314 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6315 | `						/* Cast to the desired type */` |
|      ! 0 |  6316 | `						xCast(pArg);` |
|      ! 0 |  6317 | `					}` |
|      554 |  6318 | `				}` |
|    22644 |  6319 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6320 | `					/* Pass by reference */` |
|       50 |  6321 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6322 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6323 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6324 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6325 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6326 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6327 | `						}` |
|        - |  6328 | `						/* Switch to pass by value */` |
|      ! 0 |  6329 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6330 | `					}else{` |
|        - |  6331 | `						SyHashEntry *pRefEntry;` |
|        - |  6332 | `						/* Install the referenced variable in the private function frame */` |
|       50 |  6333 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       50 |  6334 | `						if( pRefEntry == 0 ){` |
|       74 |  6335 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       48 |  6336 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       50 |  6337 | `							sArg.nIdx = pArg->nIdx;` |
|       50 |  6338 | `							sArg.pUserData = 0;` |
|       50 |  6339 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       24 |  6340 | `						}` |
|       50 |  6341 | `						pObj = 0;` |
|        - |  6342 | `					}` |
|       26 |  6343 | `				}else{` |
|        - |  6344 | `					/* Pass by value,make a copy of the given argument */` |
|    22596 |  6345 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6346 | `				}` |
|    11323 |  6347 | `			}else{` |
|        - |  6348 | `				char zName[32];` |
|        - |  6349 | `				SyString sArgName;` |
|        - |  6350 | `				/* Set a dummy name */` |
|      156 |  6351 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6352 | `				sArgName.zString = zName;` |
|        - |  6353 | `				/* Annonymous argument */` |
|      156 |  6354 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6355 | `			}` |
|    22798 |  6356 | `			if( pObj ){` |
|    22750 |  6357 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6358 | `				/* Insert argument index  */` |
|    22750 |  6359 | `				sArg.nIdx = pObj->nIdx;` |
|    22750 |  6360 | `				sArg.pUserData = 0;` |
|    22750 |  6361 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11374 |  6362 | `			}` |
|    22798 |  6363 | `			PH7_MemObjRelease(pArg);` |
|    22798 |  6364 | `			pArg++;` |
|    22798 |  6365 | `			++n;` |
|        2 |  6366 | `		}` |
|        - |  6367 | `		/* Set up closure environment */` |
|    13224 |  6368 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6369 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6370 | `			ph7_value *pValue;` |
|        - |  6371 | `			sxu32 iEnv;` |
|       11 |  6372 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6373 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6374 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6375 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6376 | `					/* Do not install null value */` |
|       11 |  6377 | `					continue;` |
|        - |  6378 | `				}` |
|       11 |  6379 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6380 | `				if( pValue == 0 ){` |
|      ! 0 |  6381 | `					continue;` |
|        - |  6382 | `				}` |
|        - |  6383 | `				/* Invalidate any prior representation */` |
|       11 |  6384 | `				PH7_MemObjRelease(pValue);` |
|        - |  6385 | `				/* Duplicate bound variable value */` |
|       11 |  6386 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6387 | `			}` |
|        5 |  6388 | `		}` |
|        - |  6389 | `		/* Process default values */` |
|    15158 |  6390 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1936 |  6391 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1930 |  6392 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1930 |  6393 | `				if( pObj ){` |
|        - |  6394 | `					/* Evaluate the default value and extract it's result */` |
|     1930 |  6395 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1930 |  6396 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6397 | `						goto Abort;` |
|        - |  6398 | `					}` |
|        - |  6399 | `					/* Insert argument index */` |
|     1930 |  6400 | `					sArg.nIdx = pObj->nIdx;` |
|     1930 |  6401 | `					sArg.pUserData = 0;` |
|     1930 |  6402 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6403 | `					/* Make sure the default argument is of the correct type */` |
|     1930 |  6404 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6405 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6406 | `						/* Cast to the desired type */` |
|      ! 0 |  6407 | `						xCast(pObj);` |
|      ! 0 |  6408 | `					}` |
|      964 |  6409 | `				}` |
|      964 |  6410 | `			}` |
|     1936 |  6411 | `			++n;` |
|        2 |  6412 | `		}` |
|        - |  6413 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6414 | `		 * does not return anything.` |
|        - |  6415 | `		 */` |
|    13224 |  6416 | `		PH7_MemObjRelease(pTos);` |
|    13224 |  6417 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6418 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13224 |  6419 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13224 |  6420 | `		if( pFrameStack == 0 ){` |
|        - |  6421 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6422 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6423 | `				&pVmFunc->sName);` |
|      ! 0 |  6424 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6425 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6426 | `			}` |
|      ! 0 |  6427 | `			break;` |
|        - |  6428 | `		}` |
|    13224 |  6429 | `		if( pSelf ){` |
|        - |  6430 | `			/* Push class name */` |
|     1954 |  6431 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      976 |  6432 | `		}` |
|        - |  6433 | `		/* Increment nesting level */` |
|    13224 |  6434 | `		pVm->nRecursionDepth++;` |
|        - |  6435 | `		/* Execute function body */` |
|    13224 |  6436 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6437 | `		/* Decrement nesting level */` |
|    13224 |  6438 | `		pVm->nRecursionDepth--;` |
|    13224 |  6439 | `		if( pSelf ){` |
|        - |  6440 | `			/* Pop class name */` |
|     1954 |  6441 | `			(void)SySetPop(&pVm->aSelf);` |
|      976 |  6442 | `		}` |
|        - |  6443 | `		/* Cleanup the mess left behind */` |
|    13224 |  6444 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6445 | `			/* Return by reference,reflect that */` |
|        9 |  6446 | `			if( n != SXU32_HIGH ){` |
|        9 |  6447 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6448 | `				sxu32 i;` |
|        - |  6449 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6450 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6451 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6452 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6453 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6454 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6455 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6456 | `								&pVmFunc->sName);` |
|      ! 0 |  6457 | `						}` |
|      ! 0 |  6458 | `						n = SXU32_HIGH;` |
|      ! 0 |  6459 | `						break;` |
|        - |  6460 | `					}` |
|        3 |  6461 | `				}` |
|        5 |  6462 | `			}else{` |
|      ! 0 |  6463 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6464 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6465 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6466 | `						&pVmFunc->sName);` |
|      ! 0 |  6467 | `				}` |
|        - |  6468 | `			}` |
|        9 |  6469 | `			pTos->nIdx = n;` |
|        4 |  6470 | `		}` |
|        - |  6471 | `		/* Cleanup the mess left behind */` |
|    13224 |  6472 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6473 | `			/* An exception was throw in this frame */` |
|       12 |  6474 | `			pFrame = pFrame->pParent;` |
|       12 |  6475 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6476 | `				/* Pop the resutlt */` |
|       10 |  6477 | `				VmPopOperand(&pTos,1);` |
|        - |  6478 | `				/* Jump to this destination */` |
|       10 |  6479 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6480 | `				rc = PH7_OK;` |
|        6 |  6481 | `			}else{` |
|        3 |  6482 | `				if( pFrame->pParent ){` |
|        3 |  6483 | `					rc = PH7_EXCEPTION;` |
|        2 |  6484 | `				}else{` |
|        - |  6485 | `					/* Continue normal execution */` |
|      ! 0 |  6486 | `					rc = PH7_OK;` |
|        - |  6487 | `				}` |
|        - |  6488 | `			}` |
|        5 |  6489 | `		}` |
|        - |  6490 | `		/* Free the operand stack */` |
|    13224 |  6491 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6492 | `		/* Leave the frame */` |
|    13224 |  6493 | `		VmLeaveFrame(&(*pVm));` |
|    13224 |  6494 | `		if( rc == PH7_ABORT ){` |
|        - |  6495 | `			/* Abort processing immeditaley */` |
|        7 |  6496 | `			goto Abort;` |
|    13218 |  6497 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6498 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6499 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6500 | `			 * overwriting the state saved by the inner level.` |
|        - |  6501 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6502 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       39 |  6503 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       39 |  6504 | `			goto Suspend;` |
|    13180 |  6505 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6506 | `			goto Exception;` |
|        - |  6507 | `		}` |
|     6590 |  6508 | `	}else{` |
|        - |  6509 | `		ph7_user_func *pFunc;` |
|        - |  6510 | `		ph7_context sCtx;` |
|        - |  6511 | `		ph7_value sRet;` |
|        - |  6512 | `		/* Look for an installed foreign function.` |
|        - |  6513 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6514 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6515 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6516 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6517 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6518 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   568584 |  6519 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   568584 |  6520 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6521 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6522 | `			const char *zShort = sName.zString;` |
|        - |  6523 | `			sxu32 i;` |
|      217 |  6524 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6525 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6526 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6527 | `				}` |
|      102 |  6528 | `			}` |
|       15 |  6529 | `			if( zShort != sName.zString ){` |
|       15 |  6530 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6531 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6532 | `			}` |
|        7 |  6533 | `		}` |
|   568584 |  6534 | `		if( pEntry == 0 ){` |
|        - |  6535 | `			/* Call to undefined function */` |
|        5 |  6536 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6537 | `			/* Pop given arguments */` |
|        5 |  6538 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6539 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6540 | `			}` |
|        - |  6541 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6542 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6543 | `			break;` |
|        - |  6544 | `		}` |
|   568580 |  6545 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6546 | `		/* Start collecting function arguments */` |
|   568580 |  6547 | `		SySetReset(&aArg);` |
|  1525052 |  6548 | `		while( pArg < pTos ){` |
|   956474 |  6549 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   956474 |  6550 | `			pArg++;` |
|        2 |  6551 | `		}` |
|        - |  6552 | `		/* Assume a null return value */` |
|   568580 |  6553 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6554 | `		/* Init the call context */` |
|   568580 |  6555 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6556 | `		/* Call the foreign function */` |
|   568580 |  6557 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6558 | `		/* Release the call context */` |
|   568580 |  6559 | `		VmReleaseCallContext(&sCtx);` |
|   568580 |  6560 | `		if( rc == PH7_ABORT ){` |
|      463 |  6561 | `			goto Abort;` |
|   568118 |  6562 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6563 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6564 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6565 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6566 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6567 | `				goto Exception;` |
|        - |  6568 | `			}` |
|        - |  6569 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6570 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6571 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6572 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6573 | `			}` |
|        - |  6574 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6575 | `			VmPopOperand(&pTos,1);` |
|        - |  6576 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6577 | `			pFrm = pVm->pFrame;` |
|        7 |  6578 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6579 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6580 | `			}` |
|        7 |  6581 | `			break;` |
|        - |  6582 | `		}` |
|   568108 |  6583 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6584 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6585 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6586 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6587 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6588 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6589 | `			 * body), the user-function path above will handle re-saving. */` |
|       39 |  6590 | `			PH7_MemObjRelease(&sRet);` |
|       39 |  6591 | `			if( pInstr->iP1 > 0 ){` |
|       39 |  6592 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6593 | `			}` |
|        - |  6594 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6595 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       39 |  6596 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       39 |  6597 | `			goto Suspend;` |
|        - |  6598 | `		}` |
|   568070 |  6599 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6600 | `			/* Pop function name and arguments */` |
|   550434 |  6601 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   275238 |  6602 | `		}` |
|        - |  6603 | `		/* Save foreign function return value */` |
|   568070 |  6604 | `		PH7_MemObjStore(&sRet,pTos);` |
|   568070 |  6605 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6606 | `	}` |
|   581246 |  6607 | `	break;` |
|        - |  6608 | `				  }` |
|        - |  6609 | `/*` |
|        - |  6610 | ` * OP_CONSUME: P1 * *` |
|        - |  6611 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6612 | ` */` |
|    11369 |  6613 | `case PH7_OP_CONSUME: {` |
|    22740 |  6614 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    22740 |  6615 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6616 |  |
|    22740 |  6617 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    22740 |  6618 | `	pCur = pOut;` |
|        - |  6619 | `	/* Start the consume process  */` |
|    45478 |  6620 | `	while( pOut <= pTos ){` |
|        - |  6621 | `		/* Force a string cast */` |
|    22740 |  6622 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      304 |  6623 | `			PH7_MemObjToString(pOut);` |
|      151 |  6624 | `		}` |
|    22740 |  6625 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6626 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6627 | `			/* Invoke the output consumer callback */` |
|    12582 |  6628 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    12582 |  6629 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    12582 |  6630 | `			SyBlobRelease(&pOut->sBlob);` |
|    12582 |  6631 | `			if( rc == SXERR_ABORT ){` |
|        - |  6632 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6633 | `				goto Abort;` |
|        - |  6634 | `			}` |
|     6290 |  6635 | `		}` |
|    22740 |  6636 | `		pOut++;` |
|        2 |  6637 | `	}` |
|    22740 |  6638 | `	pTos = &pCur[-1];` |
|    22738 |  6639 | `	break;` |
|        - |  6640 | `					 }` |
|        - |  6641 |  |
|        - |  6642 | `		} /* Switch() */` |
|  9880058 |  6643 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6644 | `	} /* For(;;) */` |
|    16087 |  6645 | `Done:` |
|    32176 |  6646 | `	SySetRelease(&aArg);` |
|    32176 |  6647 | `	return SXRET_OK;` |
|       66 |  6648 | `Suspend:` |
|      133 |  6649 | `	SySetRelease(&aArg);` |
|      133 |  6650 | `	return PH7_SUSPEND;` |
|      238 |  6651 | `Abort:` |
|      477 |  6652 | `	SySetRelease(&aArg);` |
|     1661 |  6653 | `	while( pTos >= pStack ){` |
|     1185 |  6654 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6655 | `		pTos--;` |
|        1 |  6656 | `	}` |
|      477 |  6657 | `	return PH7_ABORT;` |
|        3 |  6658 | `Exception:` |
|        8 |  6659 | `	SySetRelease(&aArg);` |
|       22 |  6660 | `	while( pTos >= pStack ){` |
|       16 |  6661 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6662 | `		pTos--;` |
|        2 |  6663 | `	}` |
|        8 |  6664 | `	return PH7_EXCEPTION;` |
|    16396 |  6665 |  |
|        - |  6666 | `/*` |
|        - |  6667 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6668 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6669 | ` * See block-comment on that function for additional information.` |
|        - |  6670 | ` */` |
|    14996 |  6671 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6672 |  |
|        - |  6673 | `	ph7_value *pStack;` |
|        - |  6674 | `	sxi32 rc;` |
|        - |  6675 | `	/* Allocate a new operand stack */` |
|    14998 |  6676 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14998 |  6677 | `	if( pStack == 0 ){` |
|      ! 0 |  6678 | `		return SXERR_MEM;` |
|        - |  6679 | `	}` |
|        - |  6680 | `	/* Execute the program */` |
|    14998 |  6681 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6682 | `	/* Free the operand stack */` |
|    14998 |  6683 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6684 | `	/* Execution result */` |
|    14998 |  6685 | `	return rc;` |
|     7500 |  6686 |  |
|        - |  6687 | `/*` |
|        - |  6688 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6689 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6690 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6691 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6692 | ` * execution ends.` |
|        - |  6693 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6694 | ` * additional information.` |
|        - |  6695 | ` */` |
|     2508 |  6696 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6697 |  |
|        - |  6698 | `	VmShutdownCB *pEntry;` |
|        - |  6699 | `	ph7_value *apArg[10];` |
|        - |  6700 | `	sxu32 n,nEntry;` |
|        - |  6701 | `	int i;` |
|        - |  6702 | `	/* Point to the stack of registered callbacks */` |
|     2510 |  6703 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    27590 |  6704 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    25082 |  6705 | `		apArg[i] = 0;` |
|    12542 |  6706 | `	}` |
|     2512 |  6707 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6708 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6709 | `		if( pEntry ){` |
|        - |  6710 | `			/* Prepare callback arguments if any */` |
|        3 |  6711 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6712 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6713 | `					break;` |
|        - |  6714 | `				}` |
|      ! 0 |  6715 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6716 | `			}` |
|        - |  6717 | `			/* Invoke the callback */` |
|        3 |  6718 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6719 | `			/*` |
|        - |  6720 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6721 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6722 | `			 */` |
|        3 |  6723 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6724 | `			if( pEntry ){` |
|        3 |  6725 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6726 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6727 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6728 | `				}` |
|        1 |  6729 | `			}` |
|        1 |  6730 | `		}` |
|        2 |  6731 | `	}` |
|     2510 |  6732 | `	SySetReset(&pVm->aShutdown);` |
|     2510 |  6733 |  |
|        - |  6734 | `/*` |
|        - |  6735 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6736 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6737 | ` * See block-comment on that function for additional information.` |
|        - |  6738 | ` */` |
|     2516 |  6739 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6740 |  |
|        - |  6741 | `	/* Make sure we are ready to execute this program */` |
|     2518 |  6742 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6743 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6744 | `	}` |
|        - |  6745 | `	/* Set the execution magic number  */` |
|     2518 |  6746 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6747 | `	/* Execute the program */` |
|     2518 |  6748 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6749 | `	/* Invoke any shutdown callbacks */` |
|     2514 |  6750 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6751 | `	/*` |
|        - |  6752 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6753 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6754 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6755 | `	 */` |
|     2514 |  6756 | `	return SXRET_OK;` |
|     1260 |  6757 |  |
|        - |  6758 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6759 | `/*` |
|        - |  6760 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6761 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6762 | ` */` |
|       42 |  6763 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        1 |  6764 |  |
|        - |  6765 | `	ph7_exec_ctx *pCtx;` |
|        - |  6766 | `	ph7_value *pStack;` |
|        - |  6767 | `	VmFrame *pFrame;` |
|       43 |  6768 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       43 |  6769 | `	if( pCtx == 0 ){` |
|      ! 0 |  6770 | `		return 0;` |
|        - |  6771 | `	}` |
|       43 |  6772 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       43 |  6773 | `	pCtx->pVm = pVm;` |
|       43 |  6774 | `	pCtx->pFunc = pFunc;` |
|       43 |  6775 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       43 |  6776 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       43 |  6777 | `	pCtx->pc = 0;` |
|       43 |  6778 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       43 |  6779 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  6780 | `	/* Allocate a private operand stack */` |
|       43 |  6781 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       43 |  6782 | `	if( pStack == 0 ){` |
|      ! 0 |  6783 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6784 | `		return 0;` |
|        - |  6785 | `	}` |
|       43 |  6786 | `	pCtx->pStack = pStack;` |
|        - |  6787 | `	/* Create a detached frame for the fiber */` |
|       43 |  6788 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       43 |  6789 | `	if( pFrame == 0 ){` |
|      ! 0 |  6790 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  6791 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6792 | `		return 0;` |
|        - |  6793 | `	}` |
|       43 |  6794 | `	pCtx->pFrame = pFrame;` |
|       43 |  6795 | `	return pCtx;` |
|       22 |  6796 |  |
|        - |  6797 | `/*` |
|        - |  6798 | ` * Start executing a fiber context for the first time.` |
|        - |  6799 | ` */` |
|       42 |  6800 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        1 |  6801 |  |
|        - |  6802 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6803 | `	sxi32 rc;` |
|       43 |  6804 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  6805 | `		return SXERR_INVALID;` |
|        - |  6806 | `	}` |
|        - |  6807 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       43 |  6808 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       43 |  6809 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6810 | `	/* Save and set the active context */` |
|       43 |  6811 | `	pOldCtx = pVm->pActiveCtx;` |
|       43 |  6812 | `	pVm->pActiveCtx = pCtx;` |
|       43 |  6813 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       43 |  6814 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       43 |  6815 | `	pVm->nRecursionDepth++;` |
|        - |  6816 | `	/* Execute from the beginning */` |
|       64 |  6817 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  6818 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       43 |  6819 | `	pVm->nRecursionDepth--;` |
|        - |  6820 | `	/* Restore the previous context */` |
|       43 |  6821 | `	pVm->pActiveCtx = pOldCtx;` |
|       43 |  6822 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6823 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       41 |  6824 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       41 |  6825 | `		pCtx->pFrame->pParent = 0;` |
|       41 |  6826 | `		if( pResult ){` |
|       23 |  6827 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  6828 | `		}` |
|       41 |  6829 | `		return SXRET_OK;` |
|        - |  6830 | `	}` |
|        - |  6831 | `	/* Detach frame */` |
|        3 |  6832 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  6833 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  6834 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  6835 | `	}` |
|        3 |  6836 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6837 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6838 | `		return PH7_ABORT;` |
|        - |  6839 | `	}` |
|        3 |  6840 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6841 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6842 | `		return PH7_EXCEPTION;` |
|        - |  6843 | `	}` |
|        - |  6844 | `	/* Normal completion */` |
|        3 |  6845 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  6846 | `	if( pResult ){` |
|        3 |  6847 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  6848 | `	}` |
|        3 |  6849 | `	return SXRET_OK;` |
|       22 |  6850 |  |
|        - |  6851 | `/*` |
|        - |  6852 | ` * Resume a suspended fiber context.` |
|        - |  6853 | ` */` |
|       86 |  6854 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        1 |  6855 |  |
|        - |  6856 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6857 | `	sxi32 rc;` |
|       87 |  6858 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  6859 | `		return SXERR_INVALID;` |
|        - |  6860 | `	}` |
|        - |  6861 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  6862 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  6863 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       87 |  6864 | `	if( pResumeValue ){` |
|       39 |  6865 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       20 |  6866 | `	}else{` |
|       49 |  6867 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  6868 | `	}` |
|       87 |  6869 | `	pCtx->nTos++;` |
|        - |  6870 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       87 |  6871 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       87 |  6872 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6873 | `	/* Save and set the active context */` |
|       87 |  6874 | `	pOldCtx = pVm->pActiveCtx;` |
|       87 |  6875 | `	pVm->pActiveCtx = pCtx;` |
|       87 |  6876 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       87 |  6877 | `	pVm->nRecursionDepth++;` |
|        - |  6878 | `	/* Resume execution from saved PC */` |
|      130 |  6879 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  6880 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       87 |  6881 | `	pVm->nRecursionDepth--;` |
|        - |  6882 | `	/* Restore the previous context */` |
|       87 |  6883 | `	pVm->pActiveCtx = pOldCtx;` |
|       87 |  6884 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6885 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       55 |  6886 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       55 |  6887 | `		pCtx->pFrame->pParent = 0;` |
|       55 |  6888 | `		if( pResult ){` |
|       17 |  6889 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  6890 | `		}` |
|       55 |  6891 | `		return SXRET_OK;` |
|        - |  6892 | `	}` |
|        - |  6893 | `	/* Detach frame */` |
|       33 |  6894 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       33 |  6895 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       33 |  6896 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  6897 | `	}` |
|       33 |  6898 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6899 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6900 | `		return PH7_ABORT;` |
|        - |  6901 | `	}` |
|       33 |  6902 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6903 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6904 | `		return PH7_EXCEPTION;` |
|        - |  6905 | `	}` |
|        - |  6906 | `	/* Normal completion */` |
|       33 |  6907 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       33 |  6908 | `	if( pResult ){` |
|       19 |  6909 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  6910 | `	}` |
|       33 |  6911 | `	return SXRET_OK;` |
|       44 |  6912 |  |
|        - |  6913 | `/*` |
|        - |  6914 | ` * Release an execution context and all its resources.` |
|        - |  6915 | ` */` |
|      ! 0 |  6916 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|      ! 0 |  6917 |  |
|      ! 0 |  6918 | `	if( pCtx == 0 ){` |
|      ! 0 |  6919 | `		return;` |
|        - |  6920 | `	}` |
|      ! 0 |  6921 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  6922 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  6923 | `		return;` |
|        - |  6924 | `	}` |
|      ! 0 |  6925 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  6926 | `	/* Release values */` |
|      ! 0 |  6927 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|      ! 0 |  6928 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  6929 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|      ! 0 |  6930 | `	if( pCtx->pFrame ){` |
|        - |  6931 | `		VmSlot *aSlot;` |
|        - |  6932 | `		sxu32 n;` |
|        - |  6933 | `		/* Free local variables */` |
|      ! 0 |  6934 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|      ! 0 |  6935 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|      ! 0 |  6936 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|      ! 0 |  6937 | `		}` |
|        - |  6938 | `		/* Remove local references */` |
|      ! 0 |  6939 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|      ! 0 |  6940 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|      ! 0 |  6941 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|      ! 0 |  6942 | `		}` |
|      ! 0 |  6943 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|      ! 0 |  6944 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|      ! 0 |  6945 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|      ! 0 |  6946 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|      ! 0 |  6947 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|      ! 0 |  6948 | `		pCtx->pFrame = 0;` |
|      ! 0 |  6949 | `	}` |
|        - |  6950 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  6951 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  6952 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|      ! 0 |  6953 | `	if( pCtx->pStack ){` |
|      ! 0 |  6954 | `		if( pCtx->nTos >= 0 ){` |
|      ! 0 |  6955 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|      ! 0 |  6956 | `			while( pTos >= pCtx->pStack ){` |
|      ! 0 |  6957 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6958 | `				pTos--;` |
|      ! 0 |  6959 | `			}` |
|      ! 0 |  6960 | `		}` |
|      ! 0 |  6961 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|      ! 0 |  6962 | `		pCtx->pStack = 0;` |
|      ! 0 |  6963 | `	}` |
|        - |  6964 | `	/* Free the context itself */` |
|      ! 0 |  6965 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6966 |  |
|        - |  6967 | `/*` |
|        - |  6968 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  6969 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  6970 | ` */` |
|       86 |  6971 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        1 |  6972 |  |
|        - |  6973 | `	ph7_class_instance *pThis;` |
|        - |  6974 | `	SyString sAttr;` |
|        - |  6975 | `	ph7_value *pAttr;` |
|       87 |  6976 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6977 | `		return 0;` |
|        - |  6978 | `	}` |
|       87 |  6979 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       87 |  6980 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  6981 | `		return 0;` |
|        - |  6982 | `	}` |
|       87 |  6983 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       87 |  6984 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       87 |  6985 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       31 |  6986 | `		return 0;` |
|        - |  6987 | `	}` |
|       57 |  6988 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       44 |  6989 |  |
|        - |  6990 | `/*` |
|        - |  6991 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  6992 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  6993 | ` */` |
|       38 |  6994 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  6995 |  |
|       39 |  6996 | `	ph7_vm *pVm = pCtx->pVm;` |
|       39 |  6997 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  6998 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  6999 | `			"Cannot suspend outside of a fiber");` |
|        - |  7000 | `	}` |
|       39 |  7001 | `	if( nArg > 0 ){` |
|       39 |  7002 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       20 |  7003 | `	}else{` |
|      ! 0 |  7004 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7005 | `	}` |
|       39 |  7006 | `	return PH7_SUSPEND;` |
|       20 |  7007 |  |
|        - |  7008 | `/*` |
|        - |  7009 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7010 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7011 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7012 | ` */` |
|       24 |  7013 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7014 |  |
|        - |  7015 | `	ph7_class_instance *pThis;` |
|        - |  7016 | `	ph7_value *pAttr;` |
|        - |  7017 | `	SyString sAttrName;` |
|       25 |  7018 | `	if( nArg < 2 ){` |
|      ! 0 |  7019 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7020 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7021 | `	}` |
|       25 |  7022 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7023 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7024 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7025 | `	}` |
|       25 |  7026 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       25 |  7027 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7028 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7029 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7030 | `	}` |
|        - |  7031 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       25 |  7032 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7033 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7034 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7035 | `	}` |
|        - |  7036 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       25 |  7037 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  7038 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7039 | `	if( pAttr ){` |
|       25 |  7040 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7041 | `	}` |
|       25 |  7042 | `	return PH7_OK;` |
|       13 |  7043 |  |
|        - |  7044 | `/*` |
|        - |  7045 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7046 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7047 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7048 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7049 | ` */` |
|       24 |  7050 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7051 | `	ph7_class_instance **ppThis)` |
|        1 |  7052 |  |
|       25 |  7053 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7054 | `	ph7_value *pCallable;` |
|        - |  7055 | `	SyString sAttrName;` |
|       25 |  7056 | `	*ppThis = 0;` |
|       25 |  7057 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  7058 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       25 |  7059 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7060 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7061 | `		return 0;` |
|        - |  7062 | `	}` |
|       25 |  7063 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7064 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7065 | `		SyString sName;` |
|        - |  7066 | `		SyHashEntry *pEntry;` |
|        - |  7067 | `		ph7_vm_func *pFunc;` |
|       25 |  7068 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       25 |  7069 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       25 |  7070 | `		if( pEntry == 0 ){` |
|      ! 0 |  7071 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7072 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7073 | `			return 0;` |
|        - |  7074 | `		}` |
|       25 |  7075 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       25 |  7076 | `		return pFunc;` |
|      ! 0 |  7077 | `	}else{` |
|        - |  7078 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7079 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7080 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7081 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7082 | `		if( pMethod == 0 ){` |
|      ! 0 |  7083 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7084 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7085 | `			return 0;` |
|        - |  7086 | `		}` |
|      ! 0 |  7087 | `		*ppThis = pClosure;` |
|      ! 0 |  7088 | `		return &pMethod->sFunc;` |
|        - |  7089 | `	}` |
|       13 |  7090 |  |
|        - |  7091 | `/*` |
|        - |  7092 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7093 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7094 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7095 | ` */` |
|       42 |  7096 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7097 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        1 |  7098 |  |
|       43 |  7099 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7100 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7101 | `	sxu32 nFormal, n;` |
|        - |  7102 | `	VmSlot sSlot;` |
|        - |  7103 | `	sxi32 rc;` |
|        - |  7104 | `	/* Install $this for closure/method callables */` |
|       43 |  7105 | `	if( pClosureThis ){` |
|        - |  7106 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7107 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7108 | `		if( pObj ){` |
|      ! 0 |  7109 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7110 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7111 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7112 | `		}` |
|      ! 0 |  7113 | `	}` |
|        - |  7114 | `	/* Install static variables */` |
|       43 |  7115 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7116 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7117 | `		ph7_value *pVal;` |
|      ! 0 |  7118 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7119 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7120 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7121 | `			if( pVal ){` |
|      ! 0 |  7122 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7123 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7124 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7125 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7126 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7127 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7128 | `				}` |
|      ! 0 |  7129 | `			}` |
|      ! 0 |  7130 | `		}` |
|      ! 0 |  7131 | `	}` |
|        - |  7132 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       43 |  7133 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       43 |  7134 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       53 |  7135 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7136 | `		ph7_value *pObj;` |
|       11 |  7137 | `		if( n < (sxu32)nArg ){` |
|        - |  7138 | `			/* Argument provided — install with type casting */` |
|       11 |  7139 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       11 |  7140 | `			if( pObj ){` |
|       11 |  7141 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7142 | `				/* Type casting */` |
|       11 |  7143 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7144 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7145 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7146 | `						if( xCast ){` |
|      ! 0 |  7147 | `							xCast(pObj);` |
|      ! 0 |  7148 | `						}` |
|      ! 0 |  7149 | `					}` |
|      ! 0 |  7150 | `				}` |
|       11 |  7151 | `				sSlot.nIdx = pObj->nIdx;` |
|       11 |  7152 | `				sSlot.pUserData = 0;` |
|       11 |  7153 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        6 |  7154 | `			}` |
|        5 |  7155 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7156 | `			/* Default value */` |
|      ! 0 |  7157 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7158 | `			if( pObj ){` |
|      ! 0 |  7159 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7160 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7161 | `					return rc;` |
|        - |  7162 | `				}` |
|      ! 0 |  7163 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7164 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7165 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7166 | `						if( xCast ){` |
|      ! 0 |  7167 | `							xCast(pObj);` |
|      ! 0 |  7168 | `						}` |
|      ! 0 |  7169 | `					}` |
|      ! 0 |  7170 | `				}` |
|      ! 0 |  7171 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7172 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7173 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7174 | `			}` |
|      ! 0 |  7175 | `		}` |
|        6 |  7176 | `	}` |
|        - |  7177 | `	/* Install closure environment (captured variables) */` |
|       43 |  7178 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7179 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7180 | `		ph7_value *pValue;` |
|        - |  7181 | `		sxu32 iEnv;` |
|        3 |  7182 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7183 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7184 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7185 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7186 | `				continue;` |
|        - |  7187 | `			}` |
|        5 |  7188 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7189 | `			if( pValue == 0 ){` |
|      ! 0 |  7190 | `				continue;` |
|        - |  7191 | `			}` |
|        5 |  7192 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7193 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7194 | `		}` |
|        1 |  7195 | `	}` |
|       43 |  7196 | `	return SXRET_OK;` |
|       22 |  7197 |  |
|        - |  7198 | `/*` |
|        - |  7199 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7200 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7201 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7202 | ` */` |
|       26 |  7203 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7204 |  |
|       27 |  7205 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7206 | `	ph7_class_instance *pThis;` |
|        - |  7207 | `	ph7_class_instance *pClosureThis;` |
|        - |  7208 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7209 | `	ph7_vm_func *pFunc;` |
|        - |  7210 | `	ph7_value sResult;` |
|        - |  7211 | `	ph7_value *pCtxAttr;` |
|        - |  7212 | `	SyString sAttrName;` |
|        - |  7213 | `	sxi32 rc;` |
|       27 |  7214 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7215 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7216 | `	}` |
|       27 |  7217 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7218 | `	/* Check if already started (has a __ctx) */` |
|       27 |  7219 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       27 |  7220 | `	if( pExecCtx != 0 ){` |
|        3 |  7221 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7222 | `			"Cannot start a fiber that has already been started");` |
|        - |  7223 | `	}` |
|        - |  7224 | `	/* Resolve callable */` |
|       25 |  7225 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       25 |  7226 | `	if( pFunc == 0 ){` |
|      ! 0 |  7227 | `		return PH7_EXCEPTION;` |
|        - |  7228 | `	}` |
|        - |  7229 | `	/* Create execution context now that we know the function */` |
|       25 |  7230 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       25 |  7231 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7232 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7233 | `			"Fiber::start(): out of memory");` |
|        - |  7234 | `	}` |
|        - |  7235 | `	/* Store context in $this->__ctx */` |
|       25 |  7236 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       25 |  7237 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7238 | `	if( pCtxAttr ){` |
|       25 |  7239 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       25 |  7240 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7241 | `	}` |
|        - |  7242 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7243 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7244 | `	 * into the fiber's frame, not the caller's. */` |
|       25 |  7245 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       25 |  7246 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7247 | `	/* Unpack the args array and install into the frame */` |
|        - |  7248 | `	{` |
|       25 |  7249 | `		ph7_value **apValues = 0;` |
|       25 |  7250 | `		int nActual = 0;` |
|       25 |  7251 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       25 |  7252 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7253 | `			ph7_hashmap_node *pNode;` |
|       25 |  7254 | `			sxu32 nCount = pMap->nEntry;` |
|       25 |  7255 | `			if( nCount > 0 ){` |
|        3 |  7256 | `				sxu32 idx = 0;` |
|        4 |  7257 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7258 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7259 | `				if( apValues ){` |
|        3 |  7260 | `					pNode = pMap->pFirst;` |
|        7 |  7261 | `					while( pNode && idx < nCount ){` |
|        5 |  7262 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7263 | `						idx++;` |
|        5 |  7264 | `						pNode = pNode->pPrev;` |
|        1 |  7265 | `					}` |
|        3 |  7266 | `					nActual = (int)idx;` |
|        1 |  7267 | `				}` |
|        1 |  7268 | `			}` |
|       12 |  7269 | `		}` |
|       25 |  7270 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       25 |  7271 | `		if( apValues ){` |
|        3 |  7272 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7273 | `		}` |
|        - |  7274 | `	}` |
|        - |  7275 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       25 |  7276 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       25 |  7277 | `	pExecCtx->pFrame->pParent = 0;` |
|       25 |  7278 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7279 | `		return PH7_ABORT;` |
|        - |  7280 | `	}` |
|       25 |  7281 | `	PH7_MemObjInit(pVm, &sResult);` |
|       25 |  7282 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       25 |  7283 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7284 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7285 | `		return PH7_ABORT;` |
|        - |  7286 | `	}` |
|       25 |  7287 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7288 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7289 | `		return PH7_EXCEPTION;` |
|        - |  7290 | `	}` |
|       25 |  7291 | `	ph7_result_value(pCtx, &sResult);` |
|       25 |  7292 | `	PH7_MemObjRelease(&sResult);` |
|       25 |  7293 | `	return PH7_OK;` |
|       14 |  7294 |  |
|        - |  7295 | `/*` |
|        - |  7296 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7297 | ` */` |
|       36 |  7298 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7299 |  |
|       37 |  7300 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7301 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7302 | `	ph7_value sResult;` |
|        - |  7303 | `	ph7_value *pResumeVal;` |
|        - |  7304 | `	sxi32 rc;` |
|       37 |  7305 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7306 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7307 | `		return PH7_OK;` |
|        - |  7308 | `	}` |
|       37 |  7309 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       37 |  7310 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7311 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7312 | `		return PH7_OK;` |
|        - |  7313 | `	}` |
|       37 |  7314 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7315 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7316 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7317 | `	}` |
|       35 |  7318 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       35 |  7319 | `	PH7_MemObjInit(pVm, &sResult);` |
|       35 |  7320 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       35 |  7321 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7322 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7323 | `		return PH7_ABORT;` |
|        - |  7324 | `	}` |
|       35 |  7325 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7326 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7327 | `		return PH7_EXCEPTION;` |
|        - |  7328 | `	}` |
|       35 |  7329 | `	ph7_result_value(pCtx, &sResult);` |
|       35 |  7330 | `	PH7_MemObjRelease(&sResult);` |
|       35 |  7331 | `	return PH7_OK;` |
|       19 |  7332 |  |
|        - |  7333 | `/*` |
|        - |  7334 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7335 | ` */` |
|        6 |  7336 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7337 |  |
|        7 |  7338 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7339 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7340 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7341 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7342 | `		return PH7_OK;` |
|        - |  7343 | `	}` |
|        7 |  7344 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        7 |  7345 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7346 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7347 | `		return PH7_OK;` |
|        - |  7348 | `	}` |
|        7 |  7349 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7350 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7351 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7352 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7353 | `		}` |
|      ! 0 |  7354 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7355 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7356 | `	}` |
|        7 |  7357 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        7 |  7358 | `	return PH7_OK;` |
|        4 |  7359 |  |
|        - |  7360 | `/*` |
|        - |  7361 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7362 | ` */` |
|        6 |  7363 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7364 |  |
|        - |  7365 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7366 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7367 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7368 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7369 | `	return PH7_OK;` |
|        4 |  7370 |  |
|      ! 0 |  7371 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7372 |  |
|        - |  7373 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7374 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7375 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7376 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7377 | `	return PH7_OK;` |
|      ! 0 |  7378 |  |
|        6 |  7379 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7380 |  |
|        - |  7381 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7382 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7383 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7384 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7385 | `	return PH7_OK;` |
|        4 |  7386 |  |
|        6 |  7387 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7388 |  |
|        - |  7389 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7390 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7391 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7392 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7393 | `	return PH7_OK;` |
|        4 |  7394 |  |
|        - |  7395 | `/*` |
|        - |  7396 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7397 | ` */` |
|      ! 0 |  7398 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7399 |  |
|      ! 0 |  7400 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7401 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7402 | `	if( nArg < 1 ){` |
|      ! 0 |  7403 | `		return PH7_OK;` |
|        - |  7404 | `	}` |
|      ! 0 |  7405 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|      ! 0 |  7406 | `	if( pExecCtx ){` |
|      ! 0 |  7407 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7408 | `		/* Clear the attribute so double-free is prevented */` |
|      ! 0 |  7409 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7410 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7411 | `			SyString sAttrName;` |
|        - |  7412 | `			ph7_value *pAttr;` |
|      ! 0 |  7413 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7414 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7415 | `			if( pAttr ){` |
|      ! 0 |  7416 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7417 | `			}` |
|      ! 0 |  7418 | `		}` |
|      ! 0 |  7419 | `	}` |
|      ! 0 |  7420 | `	return PH7_OK;` |
|      ! 0 |  7421 |  |
|        - |  7422 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7423 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7424 |  |
|        - |  7425 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7426 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7427 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7428 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7429 |  |
|      ! 0 |  7430 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7431 |  |
|        - |  7432 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7433 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7434 | `	ph7_exec_ctx *pCtx;` |
|        - |  7435 | `	ph7_vm_func *pFunc;` |
|        - |  7436 | `	ph7_value *pCallable;` |
|        - |  7437 | `	ph7_value *pCtxAttr;` |
|        - |  7438 | `	SyString sAttrName;` |
|        - |  7439 | `	/* Must not already be started */` |
|      ! 0 |  7440 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7441 | `	if( pCtx != 0 ){` |
|      ! 0 |  7442 | `		return SXERR_INVALID;` |
|        - |  7443 | `	}` |
|      ! 0 |  7444 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7445 | `		return SXERR_INVALID;` |
|        - |  7446 | `	}` |
|      ! 0 |  7447 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7448 | `	/* Get the callable */` |
|      ! 0 |  7449 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7450 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7451 | `	if( pCallable == 0 ){` |
|      ! 0 |  7452 | `		return SXERR_INVALID;` |
|        - |  7453 | `	}` |
|        - |  7454 | `	/* Resolve callable */` |
|      ! 0 |  7455 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7456 | `		SyString sName;` |
|        - |  7457 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7458 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7459 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7460 | `		if( pEntry == 0 ){` |
|      ! 0 |  7461 | `			return SXERR_NOTFOUND;` |
|        - |  7462 | `		}` |
|      ! 0 |  7463 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7464 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7465 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7466 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7467 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7468 | `		if( pMethod == 0 ){` |
|      ! 0 |  7469 | `			return SXERR_INVALID;` |
|        - |  7470 | `		}` |
|      ! 0 |  7471 | `		pClosureThis = pClosure;` |
|      ! 0 |  7472 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7473 | `	}else{` |
|      ! 0 |  7474 | `		return SXERR_INVALID;` |
|        - |  7475 | `	}` |
|        - |  7476 | `	/* Create context */` |
|      ! 0 |  7477 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7478 | `	if( pCtx == 0 ){` |
|      ! 0 |  7479 | `		return SXERR_MEM;` |
|        - |  7480 | `	}` |
|        - |  7481 | `	/* Store in __ctx */` |
|      ! 0 |  7482 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7483 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7484 | `	if( pCtxAttr ){` |
|      ! 0 |  7485 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7486 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7487 | `	}` |
|        - |  7488 | `	/* Set up frame with args */` |
|      ! 0 |  7489 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7490 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7491 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7492 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7493 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7494 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7495 |  |
|      ! 0 |  7496 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7497 |  |
|      ! 0 |  7498 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7499 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7500 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7501 |  |
|      ! 0 |  7502 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7503 |  |
|      ! 0 |  7504 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7505 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7506 |  |
|      ! 0 |  7507 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7508 |  |
|      ! 0 |  7509 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7510 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7511 |  |
|      ! 0 |  7512 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7513 |  |
|      ! 0 |  7514 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7515 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7516 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7517 |  |
|        - |  7518 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7519 | `/*` |
|        - |  7520 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7521 | ` */` |
|       18 |  7522 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7523 |  |
|        - |  7524 | `	ph7_generator *pGen;` |
|       19 |  7525 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       19 |  7526 | `	if( pGen == 0 ){` |
|      ! 0 |  7527 | `		return 0;` |
|        - |  7528 | `	}` |
|       19 |  7529 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       19 |  7530 | `	pGen->pCtx = pCtx;` |
|       19 |  7531 | `	pGen->iImplicitKey = 0;` |
|       19 |  7532 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       19 |  7533 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7534 | `	/* Link the generator back to the exec context */` |
|       19 |  7535 | `	pCtx->pPrivate = pGen;` |
|       19 |  7536 | `	return pGen;` |
|       10 |  7537 |  |
|        - |  7538 | `/*` |
|        - |  7539 | ` * Release a generator and its execution context.` |
|        - |  7540 | ` */` |
|      ! 0 |  7541 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7542 |  |
|      ! 0 |  7543 | `	if( pGen == 0 ){` |
|      ! 0 |  7544 | `		return;` |
|        - |  7545 | `	}` |
|      ! 0 |  7546 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7547 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7548 | `	if( pGen->pCtx ){` |
|      ! 0 |  7549 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7550 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7551 | `		pGen->pCtx = 0;` |
|      ! 0 |  7552 | `	}` |
|      ! 0 |  7553 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7554 |  |
|        - |  7555 | `/*` |
|        - |  7556 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7557 | ` */` |
|      192 |  7558 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        1 |  7559 |  |
|        - |  7560 | `	ph7_class_instance *pThis;` |
|        - |  7561 | `	SyString sAttr;` |
|        - |  7562 | `	ph7_value *pAttr;` |
|      193 |  7563 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7564 | `		return 0;` |
|        - |  7565 | `	}` |
|      193 |  7566 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      193 |  7567 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7568 | `		return 0;` |
|        - |  7569 | `	}` |
|      193 |  7570 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      193 |  7571 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      193 |  7572 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7573 | `		return 0;` |
|        - |  7574 | `	}` |
|      193 |  7575 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       97 |  7576 |  |
|        - |  7577 | `/*` |
|        - |  7578 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7579 | ` */` |
|       18 |  7580 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7581 |  |
|        - |  7582 | `	ph7_generator *pGen;` |
|        - |  7583 | `	sxi32 rc;` |
|       19 |  7584 | `	if( nArg < 1 ) return PH7_OK;` |
|       19 |  7585 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       19 |  7586 | `	if( pGen == 0 ) return PH7_OK;` |
|       19 |  7587 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       19 |  7588 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       19 |  7589 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       19 |  7590 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7591 | `	}` |
|       19 |  7592 | `	return PH7_OK;` |
|       10 |  7593 |  |
|        - |  7594 | `/*` |
|        - |  7595 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7596 | ` */` |
|       52 |  7597 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7598 |  |
|        - |  7599 | `	ph7_generator *pGen;` |
|       53 |  7600 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       53 |  7601 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       53 |  7602 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       53 |  7603 | `	return PH7_OK;` |
|       27 |  7604 |  |
|        - |  7605 | `/*` |
|        - |  7606 | ` * Generator::current() — return the last yielded value.` |
|        - |  7607 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7608 | ` */` |
|       56 |  7609 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7610 |  |
|        - |  7611 | `	ph7_generator *pGen;` |
|        - |  7612 | `	sxi32 rc;` |
|       57 |  7613 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       57 |  7614 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       57 |  7615 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       57 |  7616 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7617 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7618 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7619 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7620 | `	}` |
|       57 |  7621 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       57 |  7622 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       29 |  7623 | `	}else{` |
|      ! 0 |  7624 | `		ph7_result_null(pCtx);` |
|        - |  7625 | `	}` |
|       57 |  7626 | `	return PH7_OK;` |
|       29 |  7627 |  |
|        - |  7628 | `/*` |
|        - |  7629 | ` * Generator::key() — return the last yielded key.` |
|        - |  7630 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7631 | ` */` |
|       12 |  7632 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7633 |  |
|        - |  7634 | `	ph7_generator *pGen;` |
|        - |  7635 | `	sxi32 rc;` |
|       13 |  7636 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7637 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7638 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7639 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7640 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7641 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7642 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7643 | `	}` |
|       13 |  7644 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7645 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7646 | `	}else{` |
|      ! 0 |  7647 | `		ph7_result_null(pCtx);` |
|        - |  7648 | `	}` |
|       13 |  7649 | `	return PH7_OK;` |
|        7 |  7650 |  |
|        - |  7651 | `/*` |
|        - |  7652 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7653 | ` */` |
|       48 |  7654 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7655 |  |
|        - |  7656 | `	ph7_generator *pGen;` |
|        - |  7657 | `	sxi32 rc;` |
|       49 |  7658 | `	if( nArg < 1 ) return PH7_OK;` |
|       49 |  7659 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       49 |  7660 | `	if( pGen == 0 ) return PH7_OK;` |
|       49 |  7661 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7662 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       49 |  7663 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       49 |  7664 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       25 |  7665 | `	}else{` |
|      ! 0 |  7666 | `		return PH7_OK;` |
|        - |  7667 | `	}` |
|       49 |  7668 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       49 |  7669 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       49 |  7670 | `	return PH7_OK;` |
|       25 |  7671 |  |
|        - |  7672 | `/*` |
|        - |  7673 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7674 | ` */` |
|        4 |  7675 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7676 |  |
|        - |  7677 | `	ph7_generator *pGen;` |
|        - |  7678 | `	ph7_value *pSendVal;` |
|        - |  7679 | `	sxi32 rc;` |
|        5 |  7680 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7681 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7682 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7683 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7684 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7685 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7686 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7687 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7688 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7689 | `	}else{` |
|      ! 0 |  7690 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7691 | `		return PH7_OK;` |
|        - |  7692 | `	}` |
|        5 |  7693 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7694 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7695 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7696 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7697 | `	}else{` |
|        3 |  7698 | `		ph7_result_null(pCtx);` |
|        - |  7699 | `	}` |
|        5 |  7700 | `	return PH7_OK;` |
|        3 |  7701 |  |
|        - |  7702 | `/*` |
|        - |  7703 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7704 | ` *` |
|        - |  7705 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7706 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7707 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7708 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7709 | ` * the exception to the caller.` |
|        - |  7710 | ` */` |
|      ! 0 |  7711 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7712 |  |
|        - |  7713 | `	ph7_generator *pGen;` |
|        - |  7714 | `	const char *zMsg;` |
|        - |  7715 | `	int nLen;` |
|      ! 0 |  7716 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7717 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7718 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7719 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7720 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7721 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7722 | `			"Cannot throw into a closed generator");` |
|        - |  7723 | `	}` |
|        - |  7724 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7725 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7726 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7727 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7728 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7729 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7730 | `	nLen = 0;` |
|      ! 0 |  7731 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7732 | `		/* Try to get the exception's message */` |
|        - |  7733 | `		SyString sAttr;` |
|        - |  7734 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  7735 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  7736 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  7737 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  7738 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  7739 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  7740 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  7741 | `		}` |
|      ! 0 |  7742 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  7743 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  7744 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  7745 | `	}` |
|      ! 0 |  7746 | `	(void)nLen;` |
|      ! 0 |  7747 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  7748 |  |
|        - |  7749 | `/*` |
|        - |  7750 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  7751 | ` */` |
|        2 |  7752 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7753 |  |
|        - |  7754 | `	ph7_generator *pGen;` |
|        3 |  7755 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7756 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  7757 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7758 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7759 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7760 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  7761 | `	}` |
|        3 |  7762 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  7763 | `	return PH7_OK;` |
|        2 |  7764 |  |
|        - |  7765 | `/*` |
|        - |  7766 | ` * Generator::__destruct() — clean up.` |
|        - |  7767 | ` */` |
|      ! 0 |  7768 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7769 |  |
|        - |  7770 | `	ph7_generator *pGen;` |
|      ! 0 |  7771 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  7772 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7773 | `	if( pGen ){` |
|      ! 0 |  7774 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  7775 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7776 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7777 | `			SyString sAttrName;` |
|        - |  7778 | `			ph7_value *pAttr;` |
|      ! 0 |  7779 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7780 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7781 | `			if( pAttr ){` |
|      ! 0 |  7782 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7783 | `			}` |
|      ! 0 |  7784 | `		}` |
|      ! 0 |  7785 | `	}` |
|      ! 0 |  7786 | `	return PH7_OK;` |
|      ! 0 |  7787 |  |
|        - |  7788 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  7789 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  7790 | `/*` |
|        - |  7791 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  7792 | ` * the desired message.` |
|        - |  7793 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  7794 | ` * in 'api.c' for additional information.` |
|        - |  7795 | ` */` |
|      350 |  7796 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  7797 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  7798 | `	SyString *pString /* Message to output */` |
|        - |  7799 | `	)` |
|        2 |  7800 |  |
|      352 |  7801 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  7802 | `	sxi32 rc = SXRET_OK;` |
|        - |  7803 | `	/* Call the output consumer */` |
|      352 |  7804 | `	if( pString->nByte > 0 ){` |
|      352 |  7805 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  7806 | `		VmTrackOutput(pVm, pString->nByte);` |
|      175 |  7807 | `	}` |
|      352 |  7808 | `	return rc;` |
|        2 |  7809 |  |
|        - |  7810 | `/*` |
|        - |  7811 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  7812 | ` * callback to consume the formatted message.` |
|        - |  7813 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  7814 | ` * in 'api.c' for additional information.` |
|        - |  7815 | ` */` |
|        2 |  7816 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  7817 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  7818 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  7819 | `	va_list ap           /* Variable list of arguments */` |
|        - |  7820 | `	)` |
|        1 |  7821 |  |
|        3 |  7822 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  7823 | `	sxi32 rc = SXRET_OK;` |
|        - |  7824 | `	SyBlob sWorker;` |
|        - |  7825 | `	/* Format the message and call the output consumer */` |
|        3 |  7826 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  7827 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  7828 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  7829 | `		/* Consume the formatted message */` |
|        3 |  7830 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  7831 | `	}` |
|        3 |  7832 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  7833 | `	/* Release the working buffer */` |
|        3 |  7834 | `	SyBlobRelease(&sWorker);` |
|        3 |  7835 | `	return rc;` |
|        1 |  7836 |  |
|        - |  7837 | `/*` |
|        - |  7838 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  7839 | ` * This function never fail and always return a pointer` |
|        - |  7840 | ` * to a null terminated string.` |
|        - |  7841 | ` */` |
|       12 |  7842 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  7843 |  |
|       13 |  7844 | `	const char *zOp = "Unknown     ";` |
|       13 |  7845 | `	switch(nOp){` |
|        3 |  7846 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  7847 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  7848 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  7849 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  7850 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  7851 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  7852 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  7853 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  7854 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  7855 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  7856 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  7857 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  7858 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  7859 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  7860 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  7861 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  7862 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  7863 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  7864 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  7865 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  7866 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  7867 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  7868 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  7869 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  7870 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  7871 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  7872 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  7873 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  7874 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  7875 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  7876 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  7877 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  7878 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  7879 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  7880 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  7881 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  7882 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  7883 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  7884 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  7885 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  7886 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  7887 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  7888 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  7889 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  7890 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  7891 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  7892 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  7893 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  7894 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  7895 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  7896 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  7897 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  7898 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  7899 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  7900 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  7901 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  7902 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  7903 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  7904 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  7905 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  7906 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  7907 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  7908 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  7909 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  7910 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  7911 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  7912 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  7913 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  7914 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  7915 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  7916 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  7917 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  7918 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  7919 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  7920 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  7921 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  7922 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  7923 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  7924 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  7925 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  7926 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  7927 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  7928 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  7929 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  7930 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  7931 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  7932 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  7933 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  7934 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  7935 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  7936 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  7937 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  7938 | `	default:` |
|      ! 0 |  7939 | `		break;` |
|        - |  7940 | `	}` |
|       13 |  7941 | `	return zOp;` |
|        1 |  7942 |  |
|        - |  7943 | `/*` |
|        - |  7944 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  7945 | ` * The xConsumer() callback which is an used defined function` |
|        - |  7946 | ` * is responsible of consuming the generated dump.` |
|        - |  7947 | ` */` |
|        2 |  7948 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  7949 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  7950 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  7951 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  7952 | `	)` |
|        1 |  7953 |  |
|        - |  7954 | `	sxi32 rc;` |
|        3 |  7955 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  7956 | `	return rc;` |
|        1 |  7957 |  |
|        - |  7958 | `/*` |
|        - |  7959 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  7960 | ` * outside a class body [i.e: global or function scope].` |
|        - |  7961 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  7962 | ` * in 'compile.c' for additional information.` |
|        - |  7963 | ` */` |
|        8 |  7964 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  7965 |  |
|        9 |  7966 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  7967 | `	/* Evaluate and expand constant value */` |
|        9 |  7968 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  7969 |  |
|        - |  7970 | `/*` |
|        - |  7971 | ` * Section:` |
|        - |  7972 | ` *  Function handling functions.` |
|        - |  7973 | ` * Status:` |
|        - |  7974 | ` *    Stable.` |
|        - |  7975 | ` */` |
|        - |  7976 | `/*` |
|        - |  7977 | ` * int func_num_args(void)` |
|        - |  7978 | ` *   Returns the number of arguments passed to the function.` |
|        - |  7979 | ` * Parameters` |
|        - |  7980 | ` *   None.` |
|        - |  7981 | ` * Return` |
|        - |  7982 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  7983 | ` *  or -1 if called from the globe scope.` |
|        - |  7984 | ` */` |
|      928 |  7985 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7986 |  |
|        - |  7987 | `	VmFrame *pFrame;` |
|        - |  7988 | `	ph7_vm *pVm;` |
|        - |  7989 | `	/* Point to the target VM */` |
|      930 |  7990 | `	pVm = pCtx->pVm;` |
|        - |  7991 | `	/* Current frame */` |
|      930 |  7992 | `	pFrame = pVm->pFrame;` |
|      930 |  7993 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      930 |  7994 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  7995 | `		SXUNUSED(nArg);` |
|      ! 0 |  7996 | `		SXUNUSED(apArg);` |
|        - |  7997 | `		/* Global frame,return -1 */` |
|      ! 0 |  7998 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  7999 | `		return SXRET_OK;` |
|        - |  8000 | `	}` |
|        - |  8001 | `	/* Total number of arguments passed to the enclosing function */` |
|      930 |  8002 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      930 |  8003 | `	ph7_result_int(pCtx,nArg);` |
|      930 |  8004 | `	return SXRET_OK;` |
|      466 |  8005 |  |
|        - |  8006 | `/*` |
|        - |  8007 | ` * value func_get_arg(int $arg_num)` |
|        - |  8008 | ` *   Return an item from the argument list.` |
|        - |  8009 | ` * Parameters` |
|        - |  8010 | ` *  Argument number(index start from zero).` |
|        - |  8011 | ` * Return` |
|        - |  8012 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8013 | ` */` |
|       22 |  8014 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8015 |  |
|       24 |  8016 | `	ph7_value *pObj = 0;` |
|       24 |  8017 | `	VmSlot *pSlot = 0;` |
|        - |  8018 | `	VmFrame *pFrame;` |
|        - |  8019 | `	ph7_vm *pVm;` |
|        - |  8020 | `	/* Point to the target VM */` |
|       24 |  8021 | `	pVm = pCtx->pVm;` |
|        - |  8022 | `	/* Current frame */` |
|       24 |  8023 | `	pFrame = pVm->pFrame;` |
|       24 |  8024 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8025 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8026 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8027 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8028 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8029 | `		return SXRET_OK;` |
|        - |  8030 | `	}` |
|        - |  8031 | `	/* Extract the desired index */` |
|       21 |  8032 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8033 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8034 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8035 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8036 | `		return SXRET_OK;` |
|        - |  8037 | `	}` |
|        - |  8038 | `	/* Extract the desired argument */` |
|       21 |  8039 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8040 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8041 | `			/* Return the desired argument */` |
|       21 |  8042 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8043 | `		}else{` |
|        - |  8044 | `			/* No such argument,return false */` |
|      ! 0 |  8045 | `			ph7_result_bool(pCtx,0);` |
|        - |  8046 | `		}` |
|       11 |  8047 | `	}else{` |
|        - |  8048 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8049 | `		ph7_result_bool(pCtx,0);` |
|        - |  8050 | `	}` |
|       21 |  8051 | `	return SXRET_OK;` |
|       13 |  8052 |  |
|        - |  8053 | `/*` |
|        - |  8054 | ` * array func_get_args_byref(void)` |
|        - |  8055 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8056 | ` * Parameters` |
|        - |  8057 | ` *  None.` |
|        - |  8058 | ` * Return` |
|        - |  8059 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8060 | ` *  member of the current user-defined function's argument list.` |
|        - |  8061 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8062 | ` * NOTE:` |
|        - |  8063 | ` *  Arguments are returned to the array by reference.` |
|        - |  8064 | ` */` |
|        2 |  8065 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8066 |  |
|        - |  8067 | `	ph7_value *pArray;` |
|        - |  8068 | `	VmFrame *pFrame;` |
|        - |  8069 | `	VmSlot *aSlot;` |
|        - |  8070 | `	sxu32 n;` |
|        - |  8071 | `	/* Point to the current frame */` |
|        3 |  8072 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8073 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8074 | `	if( pFrame->pParent == 0 ){` |
|        - |  8075 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8076 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8077 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8078 | `		return SXRET_OK;` |
|        - |  8079 | `	}` |
|        - |  8080 | `	/* Create a new array */` |
|        3 |  8081 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8082 | `	if( pArray == 0 ){` |
|      ! 0 |  8083 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8084 | `		SXUNUSED(apArg);` |
|      ! 0 |  8085 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8086 | `		return SXRET_OK;` |
|        - |  8087 | `	}` |
|        - |  8088 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8089 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8090 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8091 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8092 | `	}` |
|        - |  8093 | `	/* Return the freshly created array */` |
|        3 |  8094 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8095 | `	return SXRET_OK;` |
|        2 |  8096 |  |
|        - |  8097 | `/*` |
|        - |  8098 | ` * array func_get_args(void)` |
|        - |  8099 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8100 | ` * Parameters` |
|        - |  8101 | ` *  None.` |
|        - |  8102 | ` * Return` |
|        - |  8103 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8104 | ` *  member of the current user-defined function's argument list.` |
|        - |  8105 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8106 | ` */` |
|       88 |  8107 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8108 |  |
|       90 |  8109 | `	ph7_value *pObj = 0;` |
|        - |  8110 | `	ph7_value *pArray;` |
|        - |  8111 | `	VmFrame *pFrame;` |
|        - |  8112 | `	VmSlot *aSlot;` |
|        - |  8113 | `	sxu32 n;` |
|        - |  8114 | `	/* Point to the current frame */` |
|       90 |  8115 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8116 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8117 | `	if( pFrame->pParent == 0 ){` |
|        - |  8118 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8119 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8120 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8121 | `		return SXRET_OK;` |
|        - |  8122 | `	}` |
|        - |  8123 | `	/* Create a new array */` |
|       90 |  8124 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8125 | `	if( pArray == 0 ){` |
|      ! 0 |  8126 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8127 | `		SXUNUSED(apArg);` |
|      ! 0 |  8128 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8129 | `		return SXRET_OK;` |
|        - |  8130 | `	}` |
|        - |  8131 | `	/* Start filling the array with the given arguments */` |
|       90 |  8132 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8133 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8134 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8135 | `		if( pObj ){` |
|      134 |  8136 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8137 | `		}` |
|       68 |  8138 | `	}` |
|        - |  8139 | `	/* Return the freshly created array */` |
|       90 |  8140 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8141 | `	return SXRET_OK;` |
|       46 |  8142 |  |
|        - |  8143 | `/*` |
|        - |  8144 | ` * bool function_exists(string $name)` |
|        - |  8145 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8146 | ` * Parameters` |
|        - |  8147 | ` *  The name of the desired function.` |
|        - |  8148 | ` * Return` |
|        - |  8149 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8150 | ` */` |
|     1682 |  8151 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8152 |  |
|        - |  8153 | `	const char *zName;` |
|        - |  8154 | `	ph7_vm *pVm;` |
|        - |  8155 | `	int nLen;` |
|        - |  8156 | `	int res;` |
|     1684 |  8157 | `	if( nArg < 1 ){` |
|        - |  8158 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8159 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8160 | `		return SXRET_OK;` |
|        - |  8161 | `	}` |
|        - |  8162 | `	/* Point to the target VM */` |
|     1684 |  8163 | `	pVm = pCtx->pVm;` |
|        - |  8164 | `	/* Extract the function name */` |
|     1684 |  8165 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8166 | `	/* Assume the function is not defined */` |
|     1684 |  8167 | `	res = 0;` |
|        - |  8168 | `	/* Perform the lookup */` |
|     2523 |  8169 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1678 |  8170 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8171 | `			/* Function is defined */` |
|      206 |  8172 | `			res = 1;` |
|      102 |  8173 | `	}` |
|     1684 |  8174 | `	ph7_result_bool(pCtx,res);` |
|     1684 |  8175 | `	return SXRET_OK;` |
|      843 |  8176 |  |
|        - |  8177 | `/*` |
|        - |  8178 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8179 | ` * [i.e: Whether it is callable or not].` |
|        - |  8180 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8181 | ` */` |
|    16236 |  8182 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8183 |  |
|    16238 |  8184 | `	int res = 0;` |
|    16238 |  8185 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8186 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8187 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8188 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8189 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8190 | `		if( pMethod && CallInvoke ){` |
|        - |  8191 | `			ph7_value sResult;` |
|        - |  8192 | `			sxi32 rc;` |
|        - |  8193 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8194 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8195 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8196 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8197 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8198 | `			}` |
|      ! 0 |  8199 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8200 | `		}` |
|    16238 |  8201 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8202 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8203 | `		if( pMap->nEntry == 2 ){` |
|        - |  8204 | `			ph7_class *pClass;` |
|        - |  8205 | `			ph7_value *pV;` |
|        - |  8206 | `			/* Extract the target class */` |
|       12 |  8207 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8208 | `			if( pV ){` |
|       12 |  8209 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8210 | `				if( pClass ){` |
|        - |  8211 | `					ph7_class_method *pMethod;` |
|        - |  8212 | `					/* Extract the target method */` |
|       10 |  8213 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8214 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8215 | `						/* Perform the lookup */` |
|       10 |  8216 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8217 | `						if( pMethod ){` |
|        - |  8218 | `							/* Method is callable */` |
|        5 |  8219 | `							res = 1;` |
|        2 |  8220 | `						}` |
|        4 |  8221 | `					}` |
|        4 |  8222 | `				}` |
|        5 |  8223 | `			}` |
|        7 |  8224 | `		}` |
|    16225 |  8225 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8226 | `		const char *zName;` |
|        - |  8227 | `		int nLen;` |
|        - |  8228 | `		/* Extract the name */` |
|     4752 |  8229 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8230 | `		/* Perform the lookup */` |
|     4767 |  8231 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8232 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8233 | `				/* Function is callable */` |
|     4734 |  8234 | `				res = 1;` |
|     2366 |  8235 | `		}` |
|     2375 |  8236 | `	}` |
|    16238 |  8237 | `	return res;` |
|        2 |  8238 |  |
|        - |  8239 | `/*` |
|        - |  8240 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8241 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8242 | ` * Parameters` |
|        - |  8243 | ` * $name` |
|        - |  8244 | ` *    The callback function to check` |
|        - |  8245 | ` * $syntax_only` |
|        - |  8246 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8247 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8248 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8249 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8250 | ` *    a string.` |
|        - |  8251 | ` * Return` |
|        - |  8252 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8253 | ` */` |
|       14 |  8254 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8255 |  |
|        - |  8256 | `	ph7_vm *pVm;` |
|        - |  8257 | `	int res;` |
|       15 |  8258 | `	if( nArg < 1 ){` |
|        - |  8259 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8260 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8261 | `		return SXRET_OK;` |
|        - |  8262 | `	}` |
|        - |  8263 | `	/* Point to the target VM */` |
|       15 |  8264 | `	pVm = pCtx->pVm;` |
|        - |  8265 | `	/* Perform the requested operation */` |
|       15 |  8266 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8267 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8268 | `	return SXRET_OK;` |
|        8 |  8269 |  |
|        - |  8270 | `/*` |
|        - |  8271 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8272 | ` * defined below.` |
|        - |  8273 | ` */` |
|     1188 |  8274 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8275 |  |
|     1189 |  8276 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8277 | `	ph7_value sName;` |
|        - |  8278 | `	sxi32 rc;` |
|        - |  8279 | `	/* Prepare the function name for insertion */` |
|     1189 |  8280 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1189 |  8281 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8282 | `	/* Perform the insertion */` |
|     1189 |  8283 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1189 |  8284 | `	PH7_MemObjRelease(&sName);` |
|     1189 |  8285 | `	return rc;` |
|        1 |  8286 |  |
|        - |  8287 | `/*` |
|        - |  8288 | ` * array get_defined_functions(void)` |
|        - |  8289 | ` *  Returns an array of all defined functions.` |
|        - |  8290 | ` * Parameter` |
|        - |  8291 | ` *  None.` |
|        - |  8292 | ` * Return` |
|        - |  8293 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8294 | ` *  both built-in (internal) and user-defined.` |
|        - |  8295 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8296 | ` *  defined ones using $arr["user"].` |
|        - |  8297 | ` * Note:` |
|        - |  8298 | ` *  NULL is returned on failure.` |
|        - |  8299 | ` */` |
|        2 |  8300 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8301 |  |
|        - |  8302 | `	ph7_value *pArray,*pEntry;` |
|        - |  8303 | `	/* NOTE:` |
|        - |  8304 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8305 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8306 | `	 */` |
|        3 |  8307 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8308 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8309 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8310 | `		SXUNUSED(apArg);` |
|        - |  8311 | `		/* Return NULL */` |
|      ! 0 |  8312 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8313 | `		return SXRET_OK;` |
|        - |  8314 | `	}` |
|        3 |  8315 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8316 | `	if( pEntry == 0 ){` |
|        - |  8317 | `		/* Return NULL */` |
|      ! 0 |  8318 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8319 | `		return SXRET_OK;` |
|        - |  8320 | `	}` |
|        - |  8321 | `	/* Fill with the appropriate information */` |
|        3 |  8322 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8323 | `	/* Create the 'internal' index */` |
|        3 |  8324 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8325 | `	/* Create the user-func array */` |
|        3 |  8326 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8327 | `	if( pEntry == 0 ){` |
|        - |  8328 | `		/* Return NULL */` |
|      ! 0 |  8329 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8330 | `		return SXRET_OK;` |
|        - |  8331 | `	}` |
|        - |  8332 | `	/* Fill with the appropriate information */` |
|        3 |  8333 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8334 | `	/* Create the 'user' index */` |
|        3 |  8335 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8336 | `	/* Return the multi-dimensional array */` |
|        3 |  8337 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8338 | `	return SXRET_OK;` |
|        2 |  8339 |  |
|        - |  8340 | `/*` |
|        - |  8341 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8342 | ` *  Register a function for execution on shutdown.` |
|        - |  8343 | ` * Note` |
|        - |  8344 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8345 | ` *  be called in the same order as they were registered.` |
|        - |  8346 | ` * Parameters` |
|        - |  8347 | ` *  $callback` |
|        - |  8348 | ` *   The shutdown callback to register.` |
|        - |  8349 | ` * $param` |
|        - |  8350 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8351 | ` * Return` |
|        - |  8352 | ` *  Nothing.` |
|        - |  8353 | ` */` |
|        2 |  8354 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8355 |  |
|        - |  8356 | `	VmShutdownCB sEntry;` |
|        - |  8357 | `	int i,j;` |
|        3 |  8358 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8359 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8360 | `		return PH7_OK;` |
|        - |  8361 | `	}` |
|        - |  8362 | `	/* Zero the Entry */` |
|        3 |  8363 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8364 | `	/* Initialize fields */` |
|        3 |  8365 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8366 | `	/* Save the callback name for later invocation name */` |
|        3 |  8367 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8368 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8369 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8370 | `	}` |
|        - |  8371 | `	/* Copy arguments */` |
|        3 |  8372 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8373 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8374 | `			/* Limit reached */` |
|      ! 0 |  8375 | `			break;` |
|        - |  8376 | `		}` |
|      ! 0 |  8377 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8378 | `	}` |
|        3 |  8379 | `	sEntry.nArg = j;` |
|        - |  8380 | `	/* Install the callback */` |
|        3 |  8381 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8382 | `	return PH7_OK;` |
|        2 |  8383 |  |
|        - |  8384 | `/*` |
|        - |  8385 | ` * Section:` |
|        - |  8386 | ` *  Class handling functions.` |
|        - |  8387 | ` * Status:` |
|        - |  8388 | ` *    Stable.` |
|        - |  8389 | ` */` |
|        - |  8390 | `/*` |
|        - |  8391 | ` * Extract the top active class. NULL is returned` |
|        - |  8392 | ` * if the class stack is empty.` |
|        - |  8393 | ` */` |
|      556 |  8394 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8395 |  |
|      558 |  8396 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8397 | `	ph7_class **apClass;` |
|      558 |  8398 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8399 | `		/* Empty stack,return NULL */` |
|       15 |  8400 | `		return 0;` |
|        - |  8401 | `	}` |
|        - |  8402 | `	/* Peek the last entry */` |
|      544 |  8403 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      544 |  8404 | `	return apClass[pSet->nUsed - 1];` |
|      280 |  8405 |  |
|        - |  8406 | `/*` |
|        - |  8407 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8408 | ` *   Get the class that declared the currently executing method.` |
|        - |  8409 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8410 | ` *` |
|        - |  8411 | ` * Parameters` |
|        - |  8412 | ` *   pVm: Target VM` |
|        - |  8413 | ` *` |
|        - |  8414 | ` * Return` |
|        - |  8415 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8416 | ` *   - Not executing within a class method` |
|        - |  8417 | ` *` |
|        - |  8418 | ` * Note` |
|        - |  8419 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8420 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8421 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8422 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8423 | ` *   declaring class.` |
|        - |  8424 | ` */` |
|       52 |  8425 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8426 |  |
|       54 |  8427 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8428 | `	ph7_vm_func *pVmFunc;` |
|        - |  8429 |  |
|        - |  8430 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  8431 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8432 |  |
|        - |  8433 | `	/* Check if we're in a method context */` |
|       54 |  8434 | `	if( pFrame->pParent ){` |
|       50 |  8435 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  8436 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8437 | `			/* Return the declaring class */` |
|       50 |  8438 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8439 | `		}` |
|      ! 0 |  8440 | `	}` |
|        - |  8441 |  |
|        5 |  8442 | `	return 0;` |
|       28 |  8443 |  |
|        - |  8444 |  |
|        - |  8445 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8446 | `/*` |
|        - |  8447 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8448 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8449 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8450 | ` * return value indicates failure.` |
|        - |  8451 | ` */` |
|     1484 |  8452 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8453 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8454 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8455 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8456 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8457 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8458 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8459 | `	)` |
|        2 |  8460 |  |
|        - |  8461 | `	ph7_value *aStack;` |
|        - |  8462 | `	VmInstr aInstr[2];` |
|        - |  8463 | `	int iCursor;` |
|        - |  8464 | `	int i;` |
|        - |  8465 | `	/* Create a new operand stack */` |
|     1486 |  8466 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1486 |  8467 | `	if( aStack == 0 ){` |
|      ! 0 |  8468 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8469 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8470 | `		return SXERR_MEM;` |
|        - |  8471 | `	}` |
|        - |  8472 | `	/* Fill the operand stack with the given arguments */` |
|     2088 |  8473 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      604 |  8474 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8475 | `		/*` |
|        - |  8476 | `		 * Symisc eXtension:` |
|        - |  8477 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8478 | `		 */` |
|      604 |  8479 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      303 |  8480 | `	}` |
|     1486 |  8481 | `	iCursor = nArg + 1;` |
|     1486 |  8482 | `	if( pThis ){` |
|        - |  8483 | `		/*` |
|        - |  8484 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8485 | `		 */` |
|     1480 |  8486 | `		pThis->iRef++; /* Increment reference count */` |
|     1480 |  8487 | `		aStack[i].x.pOther = pThis;` |
|     1480 |  8488 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      739 |  8489 | `	}` |
|     1486 |  8490 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1486 |  8491 | `	i++;` |
|        - |  8492 | `	/* Push method name */` |
|     1486 |  8493 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1486 |  8494 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1486 |  8495 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1486 |  8496 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8497 | `	/* Emit the CALL istruction */` |
|     1486 |  8498 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1486 |  8499 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1486 |  8500 | `	aInstr[0].iP2 = 0;` |
|     1486 |  8501 | `	aInstr[0].p3  = 0;` |
|        - |  8502 | `	/* Emit the DONE instruction */` |
|     1486 |  8503 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1486 |  8504 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1486 |  8505 | `	aInstr[1].iP2 = 0;` |
|     1486 |  8506 | `	aInstr[1].p3  = 0;` |
|        - |  8507 | `	/* Execute the method body (if available) */` |
|     1486 |  8508 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8509 | `	/* Clean up the mess left behind */` |
|     1486 |  8510 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1486 |  8511 | `	return PH7_OK;` |
|      744 |  8512 |  |
|        - |  8513 | `/*` |
|        - |  8514 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8515 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8516 | ` * in the apArg[] array.` |
|        - |  8517 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8518 | ` * return value indicates failure.` |
|        - |  8519 | ` */` |
|      930 |  8520 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8521 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8522 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8523 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8524 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8525 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8526 | `	)` |
|        2 |  8527 |  |
|        - |  8528 | `	ph7_value *aStack;` |
|        - |  8529 | `	VmInstr aInstr[2];` |
|        - |  8530 | `	int i;` |
|      932 |  8531 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8532 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  8533 | `		if( pResult ){` |
|        - |  8534 | `			/* Assume a null return value */` |
|      ! 0 |  8535 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8536 | `		}` |
|      471 |  8537 | `		return SXERR_INVALID;` |
|        - |  8538 | `	}` |
|      462 |  8539 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8540 | `		/* Class method */` |
|       11 |  8541 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8542 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8543 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8544 | `		ph7_class *pClass = 0;` |
|        - |  8545 | `		ph7_value *pValue;` |
|        - |  8546 | `		sxi32 rc;` |
|       11 |  8547 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8548 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8549 | `			if( pResult ){` |
|        - |  8550 | `				/* Assume a null return value */` |
|      ! 0 |  8551 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8552 | `			}` |
|      ! 0 |  8553 | `			return SXRET_OK;` |
|        - |  8554 | `		}` |
|        - |  8555 | `		/* Extract the class name or an instance of it */` |
|       11 |  8556 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8557 | `		if( pValue ){` |
|       11 |  8558 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8559 | `		}` |
|       11 |  8560 | `		if( pClass == 0 ){` |
|        - |  8561 | `			/* No such class,return NULL */` |
|      ! 0 |  8562 | `			if( pResult ){` |
|      ! 0 |  8563 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8564 | `			}` |
|      ! 0 |  8565 | `			return SXRET_OK;` |
|        - |  8566 | `		}` |
|       11 |  8567 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8568 | `			/* Point to the class instance */` |
|        5 |  8569 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8570 | `		}` |
|        - |  8571 | `		/* Try to extract the method */` |
|       11 |  8572 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8573 | `		if( pValue ){` |
|       11 |  8574 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8575 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8576 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8577 | `			}` |
|        5 |  8578 | `		}` |
|       11 |  8579 | `		if( pMethod == 0 ){` |
|        - |  8580 | `			/* No such method,return NULL */` |
|      ! 0 |  8581 | `			if( pResult ){` |
|      ! 0 |  8582 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8583 | `			}` |
|      ! 0 |  8584 | `			return SXRET_OK;` |
|        - |  8585 | `		}` |
|        - |  8586 | `		/* Call the class method */` |
|       11 |  8587 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8588 | `		return rc;` |
|        - |  8589 | `	}` |
|        - |  8590 | `	/* Create a new operand stack */` |
|      452 |  8591 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      452 |  8592 | `	if( aStack == 0 ){` |
|      ! 0 |  8593 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8594 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8595 | `		if( pResult ){` |
|        - |  8596 | `			/* Assume a null return value */` |
|      ! 0 |  8597 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8598 | `		}` |
|      ! 0 |  8599 | `		return SXERR_MEM;` |
|        - |  8600 | `	}` |
|        - |  8601 | `	/* Fill the operand stack with the given arguments */` |
|     1478 |  8602 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1028 |  8603 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8604 | `		/*` |
|        - |  8605 | `		 * Symisc eXtension:` |
|        - |  8606 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8607 | `		 */` |
|     1028 |  8608 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      515 |  8609 | `	}` |
|        - |  8610 | `	/* Push the function name */` |
|      452 |  8611 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      452 |  8612 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8613 | `	/* Emit the CALL istruction */` |
|      452 |  8614 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      452 |  8615 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      452 |  8616 | `	aInstr[0].iP2 = 0;` |
|      452 |  8617 | `	aInstr[0].p3  = 0;` |
|        - |  8618 | `	/* Emit the DONE instruction */` |
|      452 |  8619 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      452 |  8620 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      452 |  8621 | `	aInstr[1].iP2 = 0;` |
|      452 |  8622 | `	aInstr[1].p3  = 0;` |
|        - |  8623 | `	/* Execute the function body (if available) */` |
|      452 |  8624 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8625 | `	/* Clean up the mess left behind */` |
|      452 |  8626 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      452 |  8627 | `	return PH7_OK;` |
|      467 |  8628 |  |
|        - |  8629 | `/*` |
|        - |  8630 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8631 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8632 | ` * parameter.` |
|        - |  8633 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8634 | ` * return value indicates failure.` |
|        - |  8635 | ` */` |
|      236 |  8636 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8637 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8638 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8639 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8640 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8641 | `	)` |
|        1 |  8642 |  |
|        - |  8643 | `	ph7_value *pArg;` |
|        - |  8644 | `	SySet aArg;` |
|        - |  8645 | `	va_list ap;` |
|        - |  8646 | `	sxi32 rc;` |
|      237 |  8647 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8648 | `	/* Copy arguments one after one */` |
|      237 |  8649 | `	va_start(ap,pResult);` |
|      393 |  8650 | `	for(;;){` |
|      787 |  8651 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8652 | `		if( pArg == 0 ){` |
|      237 |  8653 | `			break;` |
|        - |  8654 | `		}` |
|      551 |  8655 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8656 | `	}` |
|        - |  8657 | `	/* Call the core routine */` |
|      237 |  8658 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8659 | `	/* Cleanup */` |
|      237 |  8660 | `	SySetRelease(&aArg);` |
|      237 |  8661 | `	return rc;` |
|        1 |  8662 |  |
|        - |  8663 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8664 | `/*` |
|        - |  8665 | ` * bool defined(string $name)` |
|        - |  8666 | ` *  Checks whether a given named constant exists.` |
|        - |  8667 | ` * Parameter:` |
|        - |  8668 | ` *  Name of the desired constant.` |
|        - |  8669 | ` * Return` |
|        - |  8670 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8671 | ` */` |
|       14 |  8672 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8673 |  |
|        - |  8674 | `	const char *zName;` |
|       16 |  8675 | `	int nLen = 0;` |
|       16 |  8676 | `	int res = 0;` |
|       16 |  8677 | `	if( nArg < 1 ){` |
|        - |  8678 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8679 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8680 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8681 | `		return SXRET_OK;` |
|        - |  8682 | `	}` |
|        - |  8683 | `	/* Extract constant name */` |
|       16 |  8684 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8685 | `	/* Perform the lookup */` |
|       16 |  8686 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8687 | `		/* Already defined */` |
|       10 |  8688 | `		res = 1;` |
|        4 |  8689 | `	}` |
|       16 |  8690 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8691 | `	return SXRET_OK;` |
|        9 |  8692 |  |
|        - |  8693 | `/*` |
|        - |  8694 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8695 | ` * below.` |
|        - |  8696 | ` */` |
|        8 |  8697 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8698 |  |
|       10 |  8699 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8700 | `	/* Expand constant value */` |
|       10 |  8701 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  8702 |  |
|        - |  8703 | `/*` |
|        - |  8704 | ` * bool define(string $constant_name,expression value)` |
|        - |  8705 | ` *  Defines a named constant at runtime.` |
|        - |  8706 | ` * Parameter:` |
|        - |  8707 | ` *  $constant_name` |
|        - |  8708 | ` *   The name of the constant` |
|        - |  8709 | ` *  $value` |
|        - |  8710 | ` *   Constant value` |
|        - |  8711 | ` * Return:` |
|        - |  8712 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8713 | ` */` |
|       10 |  8714 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8715 |  |
|        - |  8716 | `	const char *zName;  /* Constant name */` |
|        - |  8717 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  8718 | `	int nLen = 0;       /* Name length */` |
|        - |  8719 | `	sxi32 rc;` |
|       12 |  8720 | `	if( nArg < 2 ){` |
|        - |  8721 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8722 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8723 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8724 | `		return SXRET_OK;` |
|        - |  8725 | `	}` |
|       12 |  8726 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8727 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8728 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8729 | `		return SXRET_OK;` |
|        - |  8730 | `	}` |
|        - |  8731 | `	/* Extract constant name */` |
|       12 |  8732 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8733 | `	if( nLen < 1 ){` |
|      ! 0 |  8734 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8735 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8736 | `		return SXRET_OK;` |
|        - |  8737 | `	}` |
|        - |  8738 | `	/* Duplicate constant value */` |
|       12 |  8739 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  8740 | `	if( pValue == 0 ){` |
|      ! 0 |  8741 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8742 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8743 | `		return SXRET_OK;` |
|        - |  8744 | `	}` |
|        - |  8745 | `	/* Initialize the memory object */` |
|       12 |  8746 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8747 | `	/* Register the constant */` |
|       12 |  8748 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8749 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8750 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8751 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8752 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8753 | `		return SXRET_OK;` |
|        - |  8754 | `	}` |
|        - |  8755 | `	/* Duplicate constant value */` |
|       12 |  8756 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8757 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8758 | `		/* Lower case the constant name */` |
|      ! 0 |  8759 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8760 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8761 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8762 | `				/* UTF-8 stream */` |
|      ! 0 |  8763 | `				zCur++;` |
|      ! 0 |  8764 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8765 | `					zCur++;` |
|      ! 0 |  8766 | `				}` |
|      ! 0 |  8767 | `				continue;` |
|        - |  8768 | `			}` |
|      ! 0 |  8769 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8770 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8771 | `				zCur[0] = (char)c;` |
|      ! 0 |  8772 | `			}` |
|      ! 0 |  8773 | `			zCur++;` |
|      ! 0 |  8774 | `		}` |
|        - |  8775 | `		/* Finally,register the constant */` |
|      ! 0 |  8776 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8777 | `	}` |
|        - |  8778 | `	/* All done,return TRUE */` |
|       12 |  8779 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8780 | `	return SXRET_OK;` |
|        7 |  8781 |  |
|        - |  8782 | `/*` |
|        - |  8783 | ` * value constant(string $name)` |
|        - |  8784 | ` *  Returns the value of a constant` |
|        - |  8785 | ` * Parameter` |
|        - |  8786 | ` *  $name` |
|        - |  8787 | ` *    Name of the constant.` |
|        - |  8788 | ` * Return` |
|        - |  8789 | ` *  Constant value or NULL if not defined.` |
|        - |  8790 | ` */` |
|        8 |  8791 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8792 |  |
|        - |  8793 | `	SyHashEntry *pEntry;` |
|        - |  8794 | `	ph7_constant *pCons;` |
|        - |  8795 | `	const char *zName; /* Constant name */` |
|        - |  8796 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8797 | `	int nLen;` |
|       10 |  8798 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8799 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8800 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8801 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8802 | `		return SXRET_OK;` |
|        - |  8803 | `	}` |
|        - |  8804 | `	/* Extract the constant name */` |
|       10 |  8805 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8806 | `	/* Perform the query */` |
|       10 |  8807 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8808 | `	if( pEntry == 0 ){` |
|        3 |  8809 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8810 | `		ph7_result_null(pCtx);` |
|        3 |  8811 | `		return SXRET_OK;` |
|        - |  8812 | `	}` |
|        8 |  8813 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8814 | `	/* Point to the structure that describe the constant */` |
|        8 |  8815 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8816 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8817 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8818 | `	/* Return that value */` |
|        8 |  8819 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8820 | `	/* Cleanup */` |
|        8 |  8821 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8822 | `	return SXRET_OK;` |
|        6 |  8823 |  |
|        - |  8824 | `/*` |
|        - |  8825 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8826 | ` * defined below.` |
|        - |  8827 | ` */` |
|      444 |  8828 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8829 |  |
|      445 |  8830 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8831 | `	ph7_value sName;` |
|        - |  8832 | `	sxi32 rc;` |
|        - |  8833 | `	/* Prepare the constant name for insertion */` |
|      445 |  8834 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      445 |  8835 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8836 | `	/* Perform the insertion */` |
|      445 |  8837 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      445 |  8838 | `	PH7_MemObjRelease(&sName);` |
|      445 |  8839 | `	return rc;` |
|        1 |  8840 |  |
|        - |  8841 | `/*` |
|        - |  8842 | ` * array get_defined_constants(void)` |
|        - |  8843 | ` *  Returns an associative array with the names of all defined` |
|        - |  8844 | ` *  constants.` |
|        - |  8845 | ` * Parameters` |
|        - |  8846 | ` *  NONE.` |
|        - |  8847 | ` * Returns` |
|        - |  8848 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8849 | ` */` |
|        2 |  8850 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8851 |  |
|        - |  8852 | `	ph7_value *pArray;` |
|        - |  8853 | `	/* Create the array first*/` |
|        3 |  8854 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8855 | `	if( pArray == 0 ){` |
|      ! 0 |  8856 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8857 | `		SXUNUSED(apArg);` |
|        - |  8858 | `		/* Return NULL */` |
|      ! 0 |  8859 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8860 | `		return SXRET_OK;` |
|        - |  8861 | `	}` |
|        - |  8862 | `	/* Fill the array with the defined constants */` |
|        3 |  8863 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8864 | `	/* Return the created array */` |
|        3 |  8865 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8866 | `	return SXRET_OK;` |
|        2 |  8867 |  |
|        - |  8868 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  8869 | `/*` |
|        - |  8870 | ` * Section:` |
|        - |  8871 | ` *  Random numbers/string generators.` |
|        - |  8872 | ` * Status:` |
|        - |  8873 | ` *    Stable.` |
|        - |  8874 | ` */` |
|        - |  8875 | `/*` |
|        - |  8876 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  8877 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  8878 | ` * used by te SQLite3 library.` |
|        - |  8879 | ` */` |
|     2587 |  8880 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  8881 |  |
|        - |  8882 | `	sxu32 iNum;` |
|     2589 |  8883 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2589 |  8884 | `	return iNum;` |
|        2 |  8885 |  |
|        - |  8886 | `/*` |
|        - |  8887 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  8888 | ` * Note that the generated string is NOT null terminated.` |
|        - |  8889 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  8890 | ` * by te SQLite3 library.` |
|        - |  8891 | ` */` |
|   133820 |  8892 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  8893 |  |
|        - |  8894 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  8895 | `	int i;` |
|        - |  8896 | `	/* Generate a binary string first */` |
|   133822 |  8897 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  8898 | `	/* Turn the binary string into english based alphabet */` |
|  1472190 |  8899 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1338370 |  8900 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   669186 |  8901 | `	 }` |
|   133822 |  8902 |  |
|        - |  8903 | `/*` |
|        - |  8904 | ` * int rand()` |
|        - |  8905 | ` * int mt_rand()` |
|        - |  8906 | ` * int rand(int $min,int $max)` |
|        - |  8907 | ` * int mt_rand(int $min,int $max)` |
|        - |  8908 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  8909 | ` * Parameter` |
|        - |  8910 | ` *  $min` |
|        - |  8911 | ` *    The lowest value to return (default: 0)` |
|        - |  8912 | ` *  $max` |
|        - |  8913 | ` *   The highest value to return (default: getrandmax())` |
|        - |  8914 | ` * Return` |
|        - |  8915 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  8916 | ` * Note:` |
|        - |  8917 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8918 | ` *  by te SQLite3 library.` |
|        - |  8919 | ` */` |
|       20 |  8920 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8921 |  |
|        - |  8922 | `	sxu32 iNum;` |
|        - |  8923 | `	/* Generate the random number */` |
|       21 |  8924 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  8925 | `	if( nArg > 1 ){` |
|        - |  8926 | `		sxu32 iMin,iMax;` |
|        3 |  8927 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  8928 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  8929 | `		if( iMin < iMax ){` |
|        3 |  8930 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  8931 | `			if( iDiv > 0 ){` |
|        3 |  8932 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  8933 | `			}` |
|        1 |  8934 | `		}else if(iMax > 0 ){` |
|      ! 0 |  8935 | `			iNum %= iMax;` |
|      ! 0 |  8936 | `		}` |
|        1 |  8937 | `	}` |
|        - |  8938 | `	/* Return the number */` |
|       21 |  8939 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  8940 | `	return SXRET_OK;` |
|        1 |  8941 |  |
|        - |  8942 | `/*` |
|        - |  8943 | ` * int getrandmax(void)` |
|        - |  8944 | ` * int mt_getrandmax(void)` |
|        - |  8945 | ` * int rc4_getrandmax(void)` |
|        - |  8946 | ` *   Show largest possible random value` |
|        - |  8947 | ` * Return` |
|        - |  8948 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  8949 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  8950 | ` * Note:` |
|        - |  8951 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8952 | ` *  by te SQLite3 library.` |
|        - |  8953 | ` */` |
|        4 |  8954 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8955 |  |
|        2 |  8956 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  8957 | `	SXUNUSED(apArg);` |
|        5 |  8958 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  8959 | `	return SXRET_OK;` |
|        1 |  8960 |  |
|        - |  8961 | `/*` |
|        - |  8962 | ` * string rand_str()` |
|        - |  8963 | ` * string rand_str(int $len)` |
|        - |  8964 | ` *  Generate a random string (English alphabet).` |
|        - |  8965 | ` * Parameter` |
|        - |  8966 | ` *  $len` |
|        - |  8967 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  8968 | ` * Return` |
|        - |  8969 | ` *   A pseudo random string.` |
|        - |  8970 | ` * Note:` |
|        - |  8971 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  8972 | ` *  by te SQLite3 library.` |
|        - |  8973 | ` *  This function is a symisc extension.` |
|        - |  8974 | ` */` |
|      120 |  8975 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8976 |  |
|        - |  8977 | `	char zString[1024];` |
|      122 |  8978 | `	int iLen = 0x10;` |
|      122 |  8979 | `	if( nArg > 0 ){` |
|        - |  8980 | `		/* Get the desired length */` |
|      122 |  8981 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  8982 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  8983 | `			/* Default length */` |
|        3 |  8984 | `			iLen = 0x10;` |
|        1 |  8985 | `		}` |
|       60 |  8986 | `	}` |
|        - |  8987 | `	/* Generate the random string */` |
|      122 |  8988 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  8989 | `	/* Return the generated string */` |
|      122 |  8990 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  8991 | `	return SXRET_OK;` |
|        2 |  8992 |  |
|        - |  8993 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  8994 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  8995 | `/* Unique ID private data */` |
|        - |  8996 | `struct unique_id_data` |
|        - |  8997 |  |
|        - |  8998 | `	ph7_context *pCtx; /* Call context */` |
|        - |  8999 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9000 | `};` |
|        - |  9001 | `/*` |
|        - |  9002 | ` * Binary to hex consumer callback.` |
|        - |  9003 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9004 | ` * defined below.` |
|        - |  9005 | ` */` |
|      192 |  9006 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9007 |  |
|      193 |  9008 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9009 | `	sxu32 nBuflen;` |
|        - |  9010 | `	/* Extract result buffer length */` |
|      193 |  9011 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9012 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9013 | `			/*` |
|        - |  9014 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9015 | `			 * string will be 13 characters long` |
|        - |  9016 | `			 */` |
|       25 |  9017 | `		return SXERR_ABORT;` |
|        - |  9018 | `	}` |
|      169 |  9019 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9020 | `		return SXERR_ABORT;` |
|        - |  9021 | `	}` |
|        - |  9022 | `	/* Safely Consume the hex stream */` |
|      169 |  9023 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9024 | `	return SXRET_OK;` |
|       97 |  9025 |  |
|        - |  9026 | `/*` |
|        - |  9027 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9028 | ` *  Generate a unique ID` |
|        - |  9029 | ` * Parameter` |
|        - |  9030 | ` * $prefix` |
|        - |  9031 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9032 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9033 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9034 | ` * $more_entropy` |
|        - |  9035 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9036 | ` *  that the result will be unique.` |
|        - |  9037 | ` * Return` |
|        - |  9038 | ` *  Returns the unique identifier, as a string.` |
|        - |  9039 | ` */` |
|       24 |  9040 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9041 |  |
|        - |  9042 | `	struct unique_id_data sUniq;` |
|        - |  9043 | `	unsigned char zDigest[20];` |
|       25 |  9044 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9045 | `	const char *zPrefix;` |
|        - |  9046 | `	SHA1Context sCtx;` |
|        - |  9047 | `	char zRandom[7];` |
|        - |  9048 | `	int nPrefix;` |
|        - |  9049 | `	int entropy;` |
|        - |  9050 | `	/* Generate a random string first */` |
|       25 |  9051 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9052 | `	/* Initialize fields */` |
|       25 |  9053 | `	zPrefix = 0;` |
|       25 |  9054 | `	nPrefix = 0;` |
|       25 |  9055 | `	entropy = 0;` |
|       25 |  9056 | `	if( nArg > 0 ){` |
|        - |  9057 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9058 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9059 | `		if( nArg > 1 ){` |
|      ! 0 |  9060 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9061 | `		}` |
|      ! 0 |  9062 | `	}` |
|       25 |  9063 | `	SHA1Init(&sCtx);` |
|        - |  9064 | `	/* Generate the random ID */` |
|       25 |  9065 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9066 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9067 | `	}` |
|        - |  9068 | `	/* Append the random ID */` |
|       25 |  9069 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9070 | `	/* Append the random string */` |
|       25 |  9071 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9072 | `	/* Increment the number */` |
|       25 |  9073 | `	pVm->unique_id++;` |
|       25 |  9074 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9075 | `	/* Hexify the digest */` |
|       25 |  9076 | `	sUniq.pCtx = pCtx;` |
|       25 |  9077 | `	sUniq.entropy = entropy;` |
|       25 |  9078 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9079 | `	/* All done */` |
|       25 |  9080 | `	return PH7_OK;` |
|        1 |  9081 |  |
|        - |  9082 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9083 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9084 | `/*` |
|        - |  9085 | ` * Section:` |
|        - |  9086 | ` *  Language construct implementation as foreign functions.` |
|        - |  9087 | ` * Status:` |
|        - |  9088 | ` *    Stable.` |
|        - |  9089 | ` */` |
|        - |  9090 | `/*` |
|        - |  9091 | ` * void echo($string...)` |
|        - |  9092 | ` *  Output one or more messages.` |
|        - |  9093 | ` * Parameters` |
|        - |  9094 | ` *  $string` |
|        - |  9095 | ` *   Message to output.` |
|        - |  9096 | ` * Return` |
|        - |  9097 | ` *  NULL.` |
|        - |  9098 | ` */` |
|      ! 0 |  9099 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9100 |  |
|        - |  9101 | `	const char *zData;` |
|      ! 0 |  9102 | `	int nDataLen = 0;` |
|        - |  9103 | `	ph7_vm *pVm;` |
|        - |  9104 | `	int i,rc;` |
|        - |  9105 | `	/* Point to the target VM */` |
|      ! 0 |  9106 | `	pVm = pCtx->pVm;` |
|        - |  9107 | `	/* Output */` |
|      ! 0 |  9108 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9109 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9110 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9111 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9112 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9113 | `			if( rc == SXERR_ABORT ){` |
|        - |  9114 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9115 | `				return PH7_ABORT;` |
|        - |  9116 | `			}` |
|      ! 0 |  9117 | `		}` |
|      ! 0 |  9118 | `	}` |
|      ! 0 |  9119 | `	return SXRET_OK;` |
|      ! 0 |  9120 |  |
|        - |  9121 | `/*` |
|        - |  9122 | ` * int print($string...)` |
|        - |  9123 | ` *  Output one or more messages.` |
|        - |  9124 | ` * Parameters` |
|        - |  9125 | ` *  $string` |
|        - |  9126 | ` *   Message to output.` |
|        - |  9127 | ` * Return` |
|        - |  9128 | ` *  1 always.` |
|        - |  9129 | ` */` |
|        2 |  9130 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9131 |  |
|        - |  9132 | `	const char *zData;` |
|        3 |  9133 | `	int nDataLen = 0;` |
|        - |  9134 | `	ph7_vm *pVm;` |
|        - |  9135 | `	int i,rc;` |
|        - |  9136 | `	/* Point to the target VM */` |
|        3 |  9137 | `	pVm = pCtx->pVm;` |
|        - |  9138 | `	/* Output */` |
|        5 |  9139 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9140 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9141 | `		if( nDataLen > 0 ){` |
|        3 |  9142 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9143 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9144 | `			if( rc == SXERR_ABORT ){` |
|        - |  9145 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9146 | `				return PH7_ABORT;` |
|        - |  9147 | `			}` |
|        1 |  9148 | `		}` |
|        2 |  9149 | `	}` |
|        - |  9150 | `	/* Return 1 */` |
|        3 |  9151 | `	ph7_result_int(pCtx,1);` |
|        3 |  9152 | `	return SXRET_OK;` |
|        2 |  9153 |  |
|        - |  9154 | `/*` |
|        - |  9155 | ` * void exit(string $msg)` |
|        - |  9156 | ` * void exit(int $status)` |
|        - |  9157 | ` * void die(string $ms)` |
|        - |  9158 | ` * void die(int $status)` |
|        - |  9159 | ` *   Output a message and terminate program execution.` |
|        - |  9160 | ` * Parameter` |
|        - |  9161 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9162 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9163 | ` *  and not printed` |
|        - |  9164 | ` * Return` |
|        - |  9165 | ` *  NULL` |
|        - |  9166 | ` */` |
|      ! 0 |  9167 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9168 |  |
|      ! 0 |  9169 | `	if( nArg > 0 ){` |
|      ! 0 |  9170 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9171 | `			const char *zData;` |
|      ! 0 |  9172 | `			int iLen = 0;` |
|        - |  9173 | `			/* Print exit message */` |
|      ! 0 |  9174 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9175 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9176 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9177 | `			sxi32 iExitStatus;` |
|        - |  9178 | `			/* Record exit status code */` |
|      ! 0 |  9179 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9180 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9181 | `		}` |
|      ! 0 |  9182 | `	}` |
|        - |  9183 | `	/* Check if we are in an included file */` |
|      ! 0 |  9184 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9185 | `		/* Exit the entire process */` |
|      ! 0 |  9186 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9187 | `	}` |
|        - |  9188 | `	/* Abort processing immediately */` |
|      ! 0 |  9189 | `	return PH7_ABORT;` |
|      ! 0 |  9190 |  |
|        - |  9191 | `/*` |
|        - |  9192 | ` * bool isset($var,...)` |
|        - |  9193 | ` *  Finds out whether a variable is set.` |
|        - |  9194 | ` * Parameters` |
|        - |  9195 | ` *  One or more variable to check.` |
|        - |  9196 | ` * Return` |
|        - |  9197 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9198 | ` */` |
|    73542 |  9199 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9200 |  |
|        - |  9201 | `	ph7_value *pObj;` |
|    73544 |  9202 | `	int res = 0;` |
|        - |  9203 | `	int i;` |
|    73544 |  9204 | `	if( nArg < 1 ){` |
|        - |  9205 | `		/* Missing arguments,return false */` |
|      ! 0 |  9206 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9207 | `		return SXRET_OK;` |
|        - |  9208 | `	}` |
|        - |  9209 | `	/* Iterate over available arguments */` |
|    97030 |  9210 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    73544 |  9211 | `		pObj = apArg[i];` |
|    73544 |  9212 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    49544 |  9213 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9214 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9215 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9216 | `			}` |
|    24771 |  9217 | `		}` |
|    73544 |  9218 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    73544 |  9219 | `		if( !res ){` |
|        - |  9220 | `			/* Variable not set,return FALSE */` |
|    50058 |  9221 | `			ph7_result_bool(pCtx,0);` |
|    50058 |  9222 | `			return SXRET_OK;` |
|        - |  9223 | `		}` |
|    11745 |  9224 | `	}` |
|        - |  9225 | `	/* All given variable are set,return TRUE */` |
|    23488 |  9226 | `	ph7_result_bool(pCtx,1);` |
|    23488 |  9227 | `	return SXRET_OK;` |
|    36773 |  9228 |  |
|        - |  9229 | `/*` |
|        - |  9230 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9231 | ` * frame,the reference table and discard it's contents.` |
|        - |  9232 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9233 | ` */` |
|  2984514 |  9234 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9235 |  |
|        - |  9236 | `	ph7_value *pObj;` |
|        - |  9237 | `	VmRefObj *pRef;` |
|  2984516 |  9238 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2984516 |  9239 | `	if( pObj ){` |
|        - |  9240 | `		/* Release the object */` |
|  2984516 |  9241 | `		PH7_MemObjRelease(pObj);` |
|  1492257 |  9242 | `	}` |
|        - |  9243 | `	/* Remove old reference links */` |
|  2984516 |  9244 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2984516 |  9245 | `	if( pRef ){` |
|  2984510 |  9246 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9247 | `		/* Unlink from the reference table */` |
|  2984510 |  9248 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2984510 |  9249 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9250 | `			VmSlot sFree;` |
|        - |  9251 | `			/* Restore to the free list */` |
|  2984504 |  9252 | `			sFree.nIdx = nObjIdx;` |
|  2984504 |  9253 | `			sFree.pUserData = 0;` |
|  2984504 |  9254 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1492251 |  9255 | `		}` |
|  1492254 |  9256 | `	}` |
|  2984516 |  9257 | `	return SXRET_OK;` |
|        2 |  9258 |  |
|        - |  9259 | `/*` |
|        - |  9260 | ` * void unset($var,...)` |
|        - |  9261 | ` *   Unset one or more given variable.` |
|        - |  9262 | ` * Parameters` |
|        - |  9263 | ` *  One or more variable to unset.` |
|        - |  9264 | ` * Return` |
|        - |  9265 | ` *  Nothing.` |
|        - |  9266 | ` */` |
|     6678 |  9267 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9268 |  |
|        - |  9269 | `	ph7_value *pObj;` |
|        - |  9270 | `	ph7_vm *pVm;` |
|        - |  9271 | `	int i;` |
|        - |  9272 | `	/* Point to the target VM */` |
|     6680 |  9273 | `	pVm = pCtx->pVm;` |
|        - |  9274 | `	/* Iterate and unset */` |
|    13358 |  9275 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6680 |  9276 | `		pObj = apArg[i];` |
|     6680 |  9277 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9278 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9279 | `				/* Throw an error */` |
|      ! 0 |  9280 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9281 | `			}` |
|      ! 0 |  9282 | `		}else{` |
|     6680 |  9283 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9284 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6680 |  9285 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6674 |  9286 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3336 |  9287 | `			}` |
|        - |  9288 | `		}` |
|     3341 |  9289 | `	}` |
|     6680 |  9290 | `	return SXRET_OK;` |
|        2 |  9291 |  |
|        - |  9292 | `/*` |
|        - |  9293 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9294 | ` */` |
|      110 |  9295 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9296 |  |
|      111 |  9297 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9298 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9299 | `	ph7_value *pObj;` |
|        - |  9300 | `	sxu32 nIdx;` |
|        - |  9301 | `	/* Extract the memory object */` |
|      111 |  9302 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9303 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9304 | `	if( pObj ){` |
|      111 |  9305 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9306 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9307 | `				SyString sName;` |
|        - |  9308 | `				ph7_value sKey;` |
|        - |  9309 | `				/* Perform the insertion */` |
|      109 |  9310 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9311 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9312 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9313 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9314 | `			}` |
|       54 |  9315 | `		}` |
|       55 |  9316 | `	}` |
|      111 |  9317 | `	return SXRET_OK;` |
|        1 |  9318 |  |
|        - |  9319 | `/*` |
|        - |  9320 | ` * array get_defined_vars(void)` |
|        - |  9321 | ` *  Returns an array of all defined variables.` |
|        - |  9322 | ` * Parameter` |
|        - |  9323 | ` *  None` |
|        - |  9324 | ` * Return` |
|        - |  9325 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9326 | ` */` |
|        2 |  9327 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9328 |  |
|        3 |  9329 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9330 | `	ph7_value *pArray;` |
|        - |  9331 | `	/* Create a new array */` |
|        3 |  9332 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9333 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9334 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9335 | `		SXUNUSED(apArg);` |
|        - |  9336 | `		/* Return NULL */` |
|      ! 0 |  9337 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9338 | `		return SXRET_OK;` |
|        - |  9339 | `	}` |
|        - |  9340 | `	/* Superglobals first */` |
|        3 |  9341 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9342 | `	/* Then variable defined in the current frame */` |
|        3 |  9343 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9344 | `	/* Finally,return the created array */` |
|        3 |  9345 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9346 | `	return SXRET_OK;` |
|        2 |  9347 |  |
|        - |  9348 | `/*` |
|        - |  9349 | ` * bool gettype($var)` |
|        - |  9350 | ` *  Get the type of a variable` |
|        - |  9351 | ` * Parameters` |
|        - |  9352 | ` *   $var` |
|        - |  9353 | ` *    The variable being type checked.` |
|        - |  9354 | ` * Return` |
|        - |  9355 | ` *   String representation of the given variable type.` |
|        - |  9356 | ` */` |
|       32 |  9357 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9358 |  |
|       34 |  9359 | `	const char *zType = "Empty";` |
|       34 |  9360 | `	if( nArg > 0 ){` |
|       34 |  9361 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9362 | `	}` |
|        - |  9363 | `	/* Return the variable type */` |
|       34 |  9364 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9365 | `	return SXRET_OK;` |
|        2 |  9366 |  |
|        - |  9367 | `/*` |
|        - |  9368 | ` * string get_resource_type(resource $handle)` |
|        - |  9369 | ` *  This function gets the type of the given resource.` |
|        - |  9370 | ` * Parameters` |
|        - |  9371 | ` *  $handle` |
|        - |  9372 | ` *  The evaluated resource handle.` |
|        - |  9373 | ` * Return` |
|        - |  9374 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9375 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9376 | ` *  the return value will be the string Unknown.` |
|        - |  9377 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9378 | ` *  is not a resource.` |
|        - |  9379 | ` */` |
|        2 |  9380 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9381 |  |
|        3 |  9382 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9383 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9384 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9385 | `		return PH7_OK;` |
|        - |  9386 | `	}` |
|        3 |  9387 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9388 | `	return SXRET_OK;` |
|        2 |  9389 |  |
|        - |  9390 | `/*` |
|        - |  9391 | ` * void var_dump(expression,....)` |
|        - |  9392 | ` *   var_dump � Dumps information about a variable` |
|        - |  9393 | ` * Parameters` |
|        - |  9394 | ` *   One or more expression to dump.` |
|        - |  9395 | ` * Returns` |
|        - |  9396 | ` *  Nothing.` |
|        - |  9397 | ` */` |
|      218 |  9398 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9399 |  |
|        - |  9400 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9401 | `	int i;` |
|      220 |  9402 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9403 | `	/* Dump one or more expressions */` |
|      444 |  9404 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9405 | `		ph7_value *pObj = apArg[i];` |
|        - |  9406 | `		/* Reset the working buffer */` |
|      226 |  9407 | `		SyBlobReset(&sDump);` |
|        - |  9408 | `		/* Dump the given expression */` |
|      226 |  9409 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9410 | `		/* Output */` |
|      226 |  9411 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9412 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9413 | `		}` |
|      114 |  9414 | `	}` |
|        - |  9415 | `	/* Release the working buffer */` |
|      220 |  9416 | `	SyBlobRelease(&sDump);` |
|      220 |  9417 | `	return SXRET_OK;` |
|        2 |  9418 |  |
|        - |  9419 | `/*` |
|        - |  9420 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9421 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9422 | ` * Parameters` |
|        - |  9423 | ` *   expression: Expression to dump` |
|        - |  9424 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9425 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9426 | ` *            print_r() will return the information rather than print it.` |
|        - |  9427 | ` * Return` |
|        - |  9428 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9429 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9430 | ` */` |
|       16 |  9431 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9432 |  |
|       17 |  9433 | `	int ret_string = 0;` |
|        - |  9434 | `	SyBlob sDump;` |
|       17 |  9435 | `	if( nArg < 1 ){` |
|        - |  9436 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9437 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9438 | `		return SXRET_OK;` |
|        - |  9439 | `	}` |
|       17 |  9440 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9441 | `	if ( nArg > 1 ){` |
|        - |  9442 | `		/* Where to redirect output */` |
|       11 |  9443 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9444 | `	}` |
|        - |  9445 | `	/* Generate dump */` |
|       17 |  9446 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9447 | `	if( !ret_string ){` |
|        - |  9448 | `		/* Output dump */` |
|        7 |  9449 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9450 | `		/* Return true */` |
|        7 |  9451 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9452 | `	}else{` |
|        - |  9453 | `		/* Generated dump as return value */` |
|       11 |  9454 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9455 | `	}` |
|        - |  9456 | `	/* Release the working buffer */` |
|       17 |  9457 | `	SyBlobRelease(&sDump);` |
|       17 |  9458 | `	return SXRET_OK;` |
|        9 |  9459 |  |
|        - |  9460 | `/*` |
|        - |  9461 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9462 | ` * Same job as print_r. (see coment above)` |
|        - |  9463 | ` */` |
|        2 |  9464 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9465 |  |
|        3 |  9466 | `	int ret_string = 0;` |
|        - |  9467 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9468 | `	if( nArg < 1 ){` |
|        - |  9469 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9470 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9471 | `		return SXRET_OK;` |
|        - |  9472 | `	}` |
|        3 |  9473 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9474 | `	if ( nArg > 1 ){` |
|        - |  9475 | `		/* Where to redirect output */` |
|        3 |  9476 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9477 | `	}` |
|        - |  9478 | `	/* Generate dump */` |
|        3 |  9479 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9480 | `	if( !ret_string ){` |
|        - |  9481 | `		/* Output dump */` |
|      ! 0 |  9482 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9483 | `		/* Return NULL */` |
|      ! 0 |  9484 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9485 | `	}else{` |
|        - |  9486 | `		/* Generated dump as return value */` |
|        3 |  9487 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9488 | `	}` |
|        - |  9489 | `	/* Release the working buffer */` |
|        3 |  9490 | `	SyBlobRelease(&sDump);` |
|        3 |  9491 | `	return SXRET_OK;` |
|        2 |  9492 |  |
|        - |  9493 | `/*` |
|        - |  9494 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9495 | ` *  Set/get the various assert flags.` |
|        - |  9496 | ` * Parameter` |
|        - |  9497 | ` * $what` |
|        - |  9498 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9499 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9500 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9501 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9502 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9503 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9504 | ` * $value` |
|        - |  9505 | ` *   An optional new value for the option.` |
|        - |  9506 | ` * Return` |
|        - |  9507 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9508 | ` */` |
|       30 |  9509 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9510 |  |
|       32 |  9511 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9512 | `	int iOption;` |
|        - |  9513 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  9514 | `	if( nArg < 1 ){` |
|        3 |  9515 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9516 | `			"ArgumentCountError",` |
|        - |  9517 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9518 | `			);` |
|        - |  9519 | `	}` |
|        - |  9520 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  9521 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  9522 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9523 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9524 | `			"TypeError",` |
|        - |  9525 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9526 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9527 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9528 | `			);` |
|        - |  9529 | `	}` |
|       30 |  9530 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9531 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9532 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9533 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  9534 | `	switch( iOption ){` |
|        6 |  9535 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9536 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  9537 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  9538 | `		if( nArg > 1 ){` |
|        5 |  9539 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9540 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9541 | `			}else{` |
|        3 |  9542 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9543 | `			}` |
|        2 |  9544 | `		}` |
|       14 |  9545 | `		break;` |
|        1 |  9546 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9547 | `		/* Return old callback or null */` |
|        3 |  9548 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9549 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9550 | `		}else{` |
|        3 |  9551 | `			ph7_result_null(pCtx);` |
|        - |  9552 | `		}` |
|        3 |  9553 | `		if( nArg > 1 ){` |
|      ! 0 |  9554 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9555 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9556 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9557 | `			}else{` |
|      ! 0 |  9558 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9559 | `			}` |
|      ! 0 |  9560 | `		}` |
|        3 |  9561 | `		break;` |
|        5 |  9562 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9563 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9564 | `		if( nArg > 1 ){` |
|        5 |  9565 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9566 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9567 | `			}else{` |
|        3 |  9568 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9569 | `			}` |
|        2 |  9570 | `		}` |
|       11 |  9571 | `		break;` |
|      ! 0 |  9572 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9573 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9574 | `		break;` |
|        1 |  9575 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9576 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9577 | `		break;` |
|      ! 0 |  9578 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9579 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9580 | `		break;` |
|        1 |  9581 | `	default:` |
|        - |  9582 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9583 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9584 | `			"ValueError",` |
|        - |  9585 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9586 | `			);` |
|        - |  9587 | `	}` |
|       28 |  9588 | `	return PH7_OK;` |
|       17 |  9589 |  |
|        - |  9590 | `/*` |
|        - |  9591 | ` * bool assert(mixed $assertion)` |
|        - |  9592 | ` *  Checks if assertion is FALSE.` |
|        - |  9593 | ` * Parameter` |
|        - |  9594 | ` *  $assertion` |
|        - |  9595 | ` *    The assertion to test.` |
|        - |  9596 | ` * Return` |
|        - |  9597 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9598 | ` */` |
|       26 |  9599 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9600 |  |
|       28 |  9601 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9602 | `	int iFlags,iResult;` |
|        - |  9603 | `	const char *zDesc;` |
|        - |  9604 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  9605 | `	if( nArg < 1 ){` |
|        3 |  9606 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9607 | `			"ArgumentCountError",` |
|        - |  9608 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9609 | `			);` |
|        - |  9610 | `	}` |
|       26 |  9611 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  9612 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9613 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9614 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9615 | `		return PH7_OK;` |
|        - |  9616 | `	}` |
|        - |  9617 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  9618 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  9619 | `	if( !iResult ){` |
|        - |  9620 | `		/* Assertion failed */` |
|        - |  9621 | `		/* Extract optional description */` |
|       13 |  9622 | `		zDesc = 0;` |
|       13 |  9623 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9624 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9625 | `		}` |
|       13 |  9626 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9627 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9628 | `			ph7_value sFile,sLine;` |
|        - |  9629 | `			ph7_value *apCbArg[3];` |
|        - |  9630 | `			SyString *pFile;` |
|        - |  9631 | `			/* Extract the processed script */` |
|      ! 0 |  9632 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9633 | `			if( pFile == 0 ){` |
|      ! 0 |  9634 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9635 | `			}` |
|        - |  9636 | `			/* Invoke the callback */` |
|      ! 0 |  9637 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9638 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9639 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9640 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9641 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9642 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9643 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9644 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9645 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9646 | `		}` |
|       13 |  9647 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9648 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9649 | `			return PH7_ABORT;` |
|        - |  9650 | `		}` |
|        - |  9651 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9652 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9653 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9654 | `				"AssertionError",` |
|        - |  9655 | `				"%s",` |
|        1 |  9656 | `				zDesc` |
|        - |  9657 | `				);` |
|      ! 0 |  9658 | `		}else{` |
|       11 |  9659 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9660 | `				"AssertionError",` |
|        - |  9661 | `				"assert(false)"` |
|        - |  9662 | `				);` |
|        - |  9663 | `		}` |
|        - |  9664 | `	}` |
|        - |  9665 | `	/* Assertion passed */` |
|       14 |  9666 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9667 | `	return PH7_OK;` |
|       15 |  9668 |  |
|        - |  9669 | `/*` |
|        - |  9670 | ` * Section:` |
|        - |  9671 | ` *  Error reporting functions.` |
|        - |  9672 | ` * Status:` |
|        - |  9673 | ` *    Stable.` |
|        - |  9674 | ` */` |
|        - |  9675 | `/*` |
|        - |  9676 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9677 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9678 | ` * Parameters` |
|        - |  9679 | ` *  $error_msg` |
|        - |  9680 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9681 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9682 | ` * $error_type` |
|        - |  9683 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9684 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9685 | ` * Return` |
|        - |  9686 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9687 | ` */` |
|       12 |  9688 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9689 |  |
|       14 |  9690 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9691 | `	int rc = PH7_OK;` |
|       14 |  9692 | `	if( nArg > 0 ){` |
|        - |  9693 | `		const char *zErr;` |
|        - |  9694 | `		int nLen;` |
|        - |  9695 | `		/* Extract the error message */` |
|       12 |  9696 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9697 | `		if( nArg > 1 ){` |
|        - |  9698 | `			/* Extract the error type */` |
|       12 |  9699 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9700 | `			switch( nErr ){` |
|        1 |  9701 | `			case 1:   /* E_ERROR */` |
|        - |  9702 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9703 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9704 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9705 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9706 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9707 | `				break;` |
|        1 |  9708 | `			case 2:   /* E_WARNING */` |
|        - |  9709 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9710 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9711 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9712 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9713 | `				break;` |
|        3 |  9714 | `			default:` |
|        8 |  9715 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9716 | `				break;` |
|        - |  9717 | `			}` |
|        5 |  9718 | `		}` |
|        - |  9719 | `		/* Report error */` |
|       12 |  9720 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9721 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9722 | `			return rc;` |
|        - |  9723 | `		}` |
|        - |  9724 | `		/* Return true */` |
|       12 |  9725 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9726 | `	}else{` |
|        - |  9727 | `		/* Missing arguments,return FALSE */` |
|        3 |  9728 | `		ph7_result_bool(pCtx,0);` |
|        - |  9729 | `	}` |
|       14 |  9730 | `	return rc;` |
|        8 |  9731 |  |
|        - |  9732 | `/*` |
|        - |  9733 | ` * int error_reporting([int $level])` |
|        - |  9734 | ` *  Sets which PHP errors are reported.` |
|        - |  9735 | ` * Parameters` |
|        - |  9736 | ` *  $level` |
|        - |  9737 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9738 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9739 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9740 | ` *   levels will not always behave as expected.` |
|        - |  9741 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9742 | ` *   in the predefined constants.` |
|        - |  9743 | ` * Return` |
|        - |  9744 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9745 | ` *   parameter is given.` |
|        - |  9746 | ` */` |
|       42 |  9747 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9748 |  |
|       44 |  9749 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9750 | `	int nOld;` |
|        - |  9751 | `	/* Extract the old reporting level */` |
|       44 |  9752 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       44 |  9753 | `	if( nArg > 0 ){` |
|        - |  9754 | `		int nNew;` |
|        - |  9755 | `		/* Extract the desired error reporting level */` |
|       36 |  9756 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       36 |  9757 | `		if( !nNew ){` |
|        - |  9758 | `			/* Do not report errors at all */` |
|        5 |  9759 | `			pVm->bErrReport = 0;` |
|        3 |  9760 | `		}else{` |
|        - |  9761 | `			/* Report all errors */` |
|       32 |  9762 | `			pVm->bErrReport = 1;` |
|        - |  9763 | `		}` |
|       17 |  9764 | `	}` |
|        - |  9765 | `	/* Return the old level */` |
|       44 |  9766 | `	ph7_result_int(pCtx,nOld);` |
|       44 |  9767 | `	return PH7_OK;` |
|        2 |  9768 |  |
|        - |  9769 | `/*` |
|        - |  9770 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9771 | ` *  Send an error message somewhere.` |
|        - |  9772 | ` * Parameter` |
|        - |  9773 | ` *  $message` |
|        - |  9774 | ` *   The error message that should be logged.` |
|        - |  9775 | ` *  $message_type` |
|        - |  9776 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9777 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9778 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9779 | ` *       This is the default option.` |
|        - |  9780 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9781 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9782 | ` *    2  No longer an option.` |
|        - |  9783 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9784 | ` *       to the end of the message string.` |
|        - |  9785 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9786 | ` *  $destination` |
|        - |  9787 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9788 | ` *  $extra_headers` |
|        - |  9789 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9790 | ` * Return` |
|        - |  9791 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9792 | ` * NOTE:` |
|        - |  9793 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9794 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9795 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9796 | ` *  Otherwise this function is no-op.` |
|        - |  9797 | ` */` |
|        4 |  9798 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9799 |  |
|        - |  9800 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9801 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9802 | `	int iType = 0;` |
|        5 |  9803 | `	if( nArg < 1 ){` |
|        - |  9804 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9805 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9806 | `		return PH7_OK;` |
|        - |  9807 | `	}` |
|        5 |  9808 | `	if( pVm->xErrLog  ){` |
|        - |  9809 | `		/* Invoke the user callback */` |
|      ! 0 |  9810 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9811 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9812 | `		if( nArg > 1 ){` |
|      ! 0 |  9813 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9814 | `			if( nArg > 2 ){` |
|      ! 0 |  9815 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9816 | `				if( nArg > 3 ){` |
|      ! 0 |  9817 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9818 | `				}` |
|      ! 0 |  9819 | `			}` |
|      ! 0 |  9820 | `		}` |
|      ! 0 |  9821 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9822 | `	}` |
|        - |  9823 | `	/* Retun TRUE */` |
|        5 |  9824 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9825 | `	return PH7_OK;` |
|        3 |  9826 |  |
|        - |  9827 | `/*` |
|        - |  9828 | ` * bool restore_exception_handler(void)` |
|        - |  9829 | ` *  Restores the previously defined exception handler function.` |
|        - |  9830 | ` * Parameter` |
|        - |  9831 | ` *  None` |
|        - |  9832 | ` * Return` |
|        - |  9833 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9834 | ` */` |
|        4 |  9835 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9836 |  |
|        5 |  9837 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9838 | `	ph7_value *pOld,*pNew;` |
|        - |  9839 | `	/* Point to the old and the new handler */` |
|        5 |  9840 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9841 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9842 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9843 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9844 | `		SXUNUSED(apArg);` |
|        - |  9845 | `		/* No installed handler,return FALSE */` |
|        5 |  9846 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9847 | `		return PH7_OK;` |
|        - |  9848 | `	}` |
|        - |  9849 | `	/* Copy the old handler */` |
|      ! 0 |  9850 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9851 | `	PH7_MemObjRelease(pOld);` |
|        - |  9852 | `	/* Return TRUE */` |
|      ! 0 |  9853 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9854 | `	return PH7_OK;` |
|        3 |  9855 |  |
|        - |  9856 | `/*` |
|        - |  9857 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9858 | ` *  Sets a user-defined exception handler function.` |
|        - |  9859 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9860 | ` * NOTE` |
|        - |  9861 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9862 | ` *  the satndard PHP engine.` |
|        - |  9863 | ` * Parameters` |
|        - |  9864 | ` *  $exception_handler` |
|        - |  9865 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9866 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  9867 | ` *   that was thrown.` |
|        - |  9868 | ` *  Note:` |
|        - |  9869 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9870 | ` * Return` |
|        - |  9871 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  9872 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9873 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9874 | ` */` |
|        4 |  9875 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9876 |  |
|        6 |  9877 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9878 | `	ph7_value *pOld,*pNew;` |
|        - |  9879 | `	/* Point to the old and the new handler */` |
|        6 |  9880 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  9881 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  9882 | `	/* Return the old handler */` |
|        6 |  9883 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  9884 | `	if( nArg > 0 ){` |
|        6 |  9885 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9886 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  9887 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  9888 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  9889 | `		}else{` |
|        6 |  9890 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9891 | `			/* Install the new handler */` |
|        6 |  9892 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9893 | `		}` |
|        2 |  9894 | `	}` |
|        6 |  9895 | `	return PH7_OK;` |
|        2 |  9896 |  |
|        - |  9897 | `/*` |
|        - |  9898 | ` * bool restore_error_handler(void)` |
|        - |  9899 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9900 | ` * Parameters:` |
|        - |  9901 | ` *  None.` |
|        - |  9902 | ` * Return` |
|        - |  9903 | ` *  Always TRUE.` |
|        - |  9904 | ` */` |
|        4 |  9905 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9906 |  |
|        5 |  9907 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9908 | `	ph7_value *pOld,*pNew;` |
|        - |  9909 | `	/* Point to the old and the new handler */` |
|        5 |  9910 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  9911 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  9912 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9913 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9914 | `		SXUNUSED(apArg);` |
|        - |  9915 | `		/* No installed callback,return FALSE */` |
|        5 |  9916 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9917 | `		return PH7_OK;` |
|        - |  9918 | `	}` |
|        - |  9919 | `	/* Copy the old callback */` |
|      ! 0 |  9920 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9921 | `	PH7_MemObjRelease(pOld);` |
|        - |  9922 | `	/* Return TRUE */` |
|      ! 0 |  9923 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9924 | `	return PH7_OK;` |
|        3 |  9925 |  |
|        - |  9926 | `/*` |
|        - |  9927 | ` * value set_error_handler(callable $error_handler)` |
|        - |  9928 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9929 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  9930 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  9931 | ` *  Sets a user-defined error handler function.` |
|        - |  9932 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  9933 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  9934 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  9935 | ` *  conditions (using trigger_error()).` |
|        - |  9936 | ` * Parameters` |
|        - |  9937 | ` *  $error_handler` |
|        - |  9938 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  9939 | ` *   describing the error.` |
|        - |  9940 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  9941 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  9942 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  9943 | ` *   The function can be shown as:` |
|        - |  9944 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  9945 | ` *     errno` |
|        - |  9946 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  9947 | ` *   errstr` |
|        - |  9948 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  9949 | ` *   errfile` |
|        - |  9950 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  9951 | ` *     was raised in, as a string.` |
|        - |  9952 | ` *  Note:` |
|        - |  9953 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  9954 | ` * Return` |
|        - |  9955 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  9956 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  9957 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  9958 | ` */` |
|     8822 |  9959 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9960 |  |
|     8824 |  9961 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9962 | `	ph7_value *pOld,*pNew;` |
|        - |  9963 | `	/* Point to the old and the new handler */` |
|     8824 |  9964 | `	pOld = &pVm->aErrCB[0];` |
|     8824 |  9965 | `	pNew = &pVm->aErrCB[1];` |
|        - |  9966 | `	/* Return the old handler */` |
|     8824 |  9967 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8824 |  9968 | `	if( nArg > 0 ){` |
|     8824 |  9969 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  9970 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4411 |  9971 | `			PH7_MemObjRelease(pNew);` |
|     4411 |  9972 | `			ph7_result_bool(pCtx,1);` |
|     2206 |  9973 | `		}else{` |
|     4414 |  9974 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  9975 | `			/* Install the new handler */` |
|     4414 |  9976 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  9977 | `		}` |
|     4411 |  9978 | `	}` |
|     8824 |  9979 | `	return PH7_OK;` |
|        2 |  9980 |  |
|        - |  9981 | `/*` |
|        - |  9982 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  9983 | ` *  Generates a backtrace.` |
|        - |  9984 | ` * Paramaeter` |
|        - |  9985 | ` *  $options` |
|        - |  9986 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  9987 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  9988 | ` *   all the function/method arguments, to save memory.` |
|        - |  9989 | ` * $limit` |
|        - |  9990 | ` *   (Not Used)` |
|        - |  9991 | ` * Return` |
|        - |  9992 | ` *  An array.The possible returned elements are as follows:` |
|        - |  9993 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  9994 | ` *          Name        Type      Description` |
|        - |  9995 | ` *          ------      ------     -----------` |
|        - |  9996 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  9997 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  9998 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  9999 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10000 | ` *          object      object    The current object.` |
|        - | 10001 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10002 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10003 | ` */` |
|      510 | 10004 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10005 |  |
|      512 | 10006 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10007 | `	ph7_value *pArray;` |
|        - | 10008 | `	ph7_class *pClass;` |
|        - | 10009 | `	ph7_value *pValue;` |
|        - | 10010 | `	SyString *pFile;` |
|        - | 10011 | `	/* Create a new array */` |
|      512 | 10012 | `	pArray = ph7_context_new_array(pCtx);` |
|      512 | 10013 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      512 | 10014 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10015 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10016 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10017 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10018 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10019 | `		SXUNUSED(apArg);` |
|      ! 0 | 10020 | `		return PH7_OK;` |
|        - | 10021 | `	}` |
|        - | 10022 | `	/* Dump running function name and it's arguments  */` |
|      512 | 10023 | `	if( pVm->pFrame->pParent ){` |
|      512 | 10024 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10025 | `		ph7_vm_func *pFunc;` |
|        - | 10026 | `		ph7_value *pArg;` |
|      512 | 10027 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      512 | 10028 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      512 | 10029 | `		if( pFrame->pParent && pFunc ){` |
|      512 | 10030 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      512 | 10031 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      512 | 10032 | `			ph7_value_reset_string_cursor(pValue);` |
|      255 | 10033 | `		}` |
|        - | 10034 | `		/* Function arguments */` |
|      512 | 10035 | `		pArg = ph7_context_new_array(pCtx);` |
|      512 | 10036 | `		if( pArg  ){` |
|        - | 10037 | `			ph7_value *pObj;` |
|        - | 10038 | `			VmSlot *aSlot;` |
|        - | 10039 | `			sxu32 n;` |
|        - | 10040 | `			/* Start filling the array with the given arguments */` |
|      512 | 10041 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2034 | 10042 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1524 | 10043 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1524 | 10044 | `				if( pObj ){` |
|     1524 | 10045 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      761 | 10046 | `				}` |
|      763 | 10047 | `			}` |
|        - | 10048 | `			/* Save the array */` |
|      512 | 10049 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      255 | 10050 | `		}` |
|      255 | 10051 | `	}` |
|      512 | 10052 | `	ph7_value_int(pValue,1);` |
|        - | 10053 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10054 | `	 * line numbers at run-time. )` |
|        - | 10055 | `	 */` |
|      512 | 10056 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10057 | `	/* Current processed script */` |
|      512 | 10058 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      512 | 10059 | `	if( pFile ){` |
|      512 | 10060 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      512 | 10061 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      512 | 10062 | `		ph7_value_reset_string_cursor(pValue);` |
|      255 | 10063 | `	}` |
|        - | 10064 | `	/* Top class */` |
|      512 | 10065 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      512 | 10066 | `	if( pClass ){` |
|      508 | 10067 | `		ph7_value_reset_string_cursor(pValue);` |
|      508 | 10068 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      508 | 10069 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      253 | 10070 | `	}` |
|        - | 10071 | `	/* Return the freshly created array */` |
|      512 | 10072 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10073 | `	/*` |
|        - | 10074 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10075 | `	 * as soon we return from this function.` |
|        - | 10076 | `	 */` |
|      512 | 10077 | `	return PH7_OK;` |
|      257 | 10078 |  |
|        - | 10079 | `/*` |
|        - | 10080 | ` * Generate a small backtrace.` |
|        - | 10081 | ` * Store the generated dump in the given BLOB` |
|        - | 10082 | ` */` |
|        4 | 10083 | `static int VmMiniBacktrace(` |
|        - | 10084 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10085 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10086 | `	)` |
|        1 | 10087 |  |
|        5 | 10088 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10089 | `	ph7_vm_func *pFunc;` |
|        - | 10090 | `	ph7_class *pClass;` |
|        - | 10091 | `	SyString *pFile;` |
|        - | 10092 | `	/* Called function */` |
|        5 | 10093 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10094 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10095 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10096 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10097 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10098 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10099 | `	}else{` |
|      ! 0 | 10100 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10101 | `	}` |
|        5 | 10102 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10103 | `	/* Current processed script */` |
|        5 | 10104 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10105 | `	if( pFile ){` |
|        5 | 10106 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10107 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10108 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10109 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10110 | `	}` |
|        - | 10111 | `	/* Top class */` |
|        5 | 10112 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10113 | `	if( pClass ){` |
|      ! 0 | 10114 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10115 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10116 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10117 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10118 | `	}` |
|        5 | 10119 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10120 | `	/* All done */` |
|        5 | 10121 | `	return SXRET_OK;` |
|        1 | 10122 |  |
|        - | 10123 | `/*` |
|        - | 10124 | ` * void debug_print_backtrace()` |
|        - | 10125 | ` *  Prints a backtrace` |
|        - | 10126 | ` * Parameters` |
|        - | 10127 | ` * None` |
|        - | 10128 | ` * Return` |
|        - | 10129 | ` * NULL` |
|        - | 10130 | ` */` |
|        2 | 10131 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10132 |  |
|        3 | 10133 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10134 | `	SyBlob sDump;` |
|        3 | 10135 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10136 | `	/* Generate the backtrace */` |
|        3 | 10137 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10138 | `	/* Output backtrace */` |
|        3 | 10139 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10140 | `	/* All done,cleanup */` |
|        3 | 10141 | `	SyBlobRelease(&sDump);` |
|        1 | 10142 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10143 | `	SXUNUSED(apArg);` |
|        3 | 10144 | `	return PH7_OK;` |
|        1 | 10145 |  |
|        - | 10146 | `/*` |
|        - | 10147 | ` * string debug_string_backtrace()` |
|        - | 10148 | ` *  Generate a backtrace` |
|        - | 10149 | ` * Parameters` |
|        - | 10150 | ` * None` |
|        - | 10151 | ` * Return` |
|        - | 10152 | ` *  A mini backtrace().` |
|        - | 10153 | ` * Note that this is a symisc extension.` |
|        - | 10154 | ` */` |
|        2 | 10155 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10156 |  |
|        3 | 10157 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10158 | `	SyBlob sDump;` |
|        3 | 10159 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10160 | `	/* Generate the backtrace */` |
|        3 | 10161 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10162 | `	/* Return the backtrace */` |
|        3 | 10163 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10164 | `	/* All done,cleanup */` |
|        3 | 10165 | `	SyBlobRelease(&sDump);` |
|        1 | 10166 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10167 | `	SXUNUSED(apArg);` |
|        3 | 10168 | `	return PH7_OK;` |
|        1 | 10169 |  |
|        - | 10170 | `/*` |
|        - | 10171 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10172 | ` * exception is triggered.` |
|        - | 10173 | ` */` |
|      472 | 10174 | `static sxi32 VmUncaughtException(` |
|        - | 10175 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10176 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10177 | `	)` |
|        1 | 10178 |  |
|        - | 10179 | `	ph7_value *apArg[2],sArg;` |
|      473 | 10180 | `	int nArg = 1;` |
|        - | 10181 | `	sxi32 rc;` |
|      473 | 10182 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10183 | `		/* Nesting limit reached */` |
|      ! 0 | 10184 | `		return SXRET_OK;` |
|        - | 10185 | `	}` |
|        - | 10186 | `	/* Call any exception handler if available */` |
|      473 | 10187 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 | 10188 | `	if( pThis ){` |
|        - | 10189 | `		/* Load the exception instance */` |
|      473 | 10190 | `		sArg.x.pOther = pThis;` |
|      473 | 10191 | `		pThis->iRef++;` |
|      473 | 10192 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 | 10193 | `	}else{` |
|      ! 0 | 10194 | `		nArg = 0;` |
|        - | 10195 | `	}` |
|      473 | 10196 | `	apArg[0] = &sArg;` |
|        - | 10197 | `	/* Call the exception handler if available */` |
|      473 | 10198 | `	pVm->nExceptDepth++;` |
|      473 | 10199 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 | 10200 | `	pVm->nExceptDepth--;` |
|      473 | 10201 | `	if( rc != SXRET_OK ){` |
|        - | 10202 | `		SyBlob sMsgBuf;` |
|      471 | 10203 | `		const char *zClass = "Exception";` |
|      471 | 10204 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10205 | `		const char *zMsg;` |
|        - | 10206 | `		sxu32 nMsg;` |
|        - | 10207 | `		const char *zFuncName;` |
|        - | 10208 | `		int nFuncLen;` |
|      471 | 10209 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 | 10210 | `		if( pThis ){` |
|        - | 10211 | `			ph7_class_method *pGetMessage;` |
|        - | 10212 | `			ph7_value sMsg;` |
|        - | 10213 | `			const char *zTmp;` |
|        - | 10214 | `			int nTmp;` |
|      471 | 10215 | `			zClass = pThis->pClass->sName.zString;` |
|      471 | 10216 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 | 10217 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 | 10218 | `			if( pGetMessage ){` |
|      471 | 10219 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 | 10220 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 | 10221 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 | 10222 | `					if( zTmp && nTmp > 0 ){` |
|      471 | 10223 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 | 10224 | `					}` |
|      235 | 10225 | `				}` |
|      471 | 10226 | `				PH7_MemObjRelease(&sMsg);` |
|      235 | 10227 | `			}` |
|      235 | 10228 | `		}` |
|      471 | 10229 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10230 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10231 | `		}` |
|      471 | 10232 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 | 10233 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 | 10234 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 | 10235 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 | 10236 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10237 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 | 10238 | `		rc = SXERR_ABORT;` |
|      235 | 10239 | `	}` |
|      473 | 10240 | `	PH7_MemObjRelease(&sArg);` |
|      473 | 10241 | `	return rc;` |
|      237 | 10242 |  |
|        - | 10243 | `/*` |
|        - | 10244 | ` * Throw a user exception.` |
|        - | 10245 | ` *` |
|        - | 10246 | ` * Exception dispatch follows this sequence:` |
|        - | 10247 | ` *` |
|        - | 10248 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10249 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10250 | ` *` |
|        - | 10251 | ` * 2. If NO catch matches:` |
|        - | 10252 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10253 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10254 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10255 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10256 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10257 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10258 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10259 | ` *` |
|        - | 10260 | ` * 3. If a catch DOES match:` |
|        - | 10261 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10262 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10263 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10264 | ` *       finally block.` |
|        - | 10265 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10266 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10267 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10268 | ` *       in pPendingException (step 2c).` |
|        - | 10269 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10270 | ` *    d. Run finally (if present).` |
|        - | 10271 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10272 | ` *       that handlers are restored and finally has run.` |
|        - | 10273 | ` */` |
|      514 | 10274 | `static sxi32 VmThrowException(` |
|        - | 10275 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10276 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10277 | `	)` |
|        2 | 10278 |  |
|        - | 10279 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10280 | `	ph7_exception **apException;` |
|        - | 10281 | `	ph7_exception *pException;` |
|        - | 10282 | `	/* Point to the stack of loaded exceptions */` |
|      516 | 10283 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      516 | 10284 | `	pException = 0;` |
|      516 | 10285 | `	pCatch = 0;` |
|      516 | 10286 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10287 | `		ph7_exception_block *aCatch;` |
|        - | 10288 | `		ph7_class *pClass;` |
|        - | 10289 | `		sxu32 j;` |
|        - | 10290 | `		/* Locate the appropriate block to execute */` |
|       40 | 10291 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10292 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10293 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10294 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10295 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10296 | `			/* Extract the target class */` |
|       38 | 10297 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10298 | `			if( pClass == 0 ){` |
|        - | 10299 | `				/* No such class */` |
|      ! 0 | 10300 | `				continue;` |
|        - | 10301 | `			}` |
|       38 | 10302 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10303 | `				/* Catch block found,break immeditaley */` |
|       38 | 10304 | `				pCatch = &aCatch[j];` |
|       38 | 10305 | `				break;` |
|        - | 10306 | `			}` |
|      ! 0 | 10307 | `		}` |
|       19 | 10308 | `	}` |
|        - | 10309 | `	/* Execute the cached block if available */` |
|      516 | 10310 | `	if( pCatch == 0 ){` |
|        - | 10311 | `		sxi32 rc;` |
|        - | 10312 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 | 10313 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10314 | `			pException->iFinallyDone = 1;` |
|        3 | 10315 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10316 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10317 | `				return SXERR_ABORT;` |
|        - | 10318 | `			}` |
|        1 | 10319 | `		}` |
|        - | 10320 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 | 10321 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10322 | `			/* Re-throw to the outer handler */` |
|        3 | 10323 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10324 | `		}` |
|        - | 10325 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10326 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10327 | `		 * exception instead of reporting it uncaught.` |
|        - | 10328 | `		 */` |
|      478 | 10329 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10330 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10331 | `			 * by looking for a catch frame on the stack.` |
|        - | 10332 | `			 */` |
|      478 | 10333 | `			VmFrame *pF = pVm->pFrame;` |
|      478 | 10334 | `			int inCatch = 0;` |
|      956 | 10335 | `			while( pF ){` |
|      484 | 10336 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 | 10337 | `					inCatch = 1;` |
|        6 | 10338 | `					break;` |
|        - | 10339 | `				}` |
|      479 | 10340 | `				pF = pF->pParent;` |
|        1 | 10341 | `			}` |
|      478 | 10342 | `			if( inCatch ){` |
|        - | 10343 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 | 10344 | `				pThis->iRef++;` |
|        6 | 10345 | `				pVm->pPendingException = pThis;` |
|        6 | 10346 | `				return SXRET_OK;` |
|        - | 10347 | `			}` |
|      236 | 10348 | `		}` |
|        - | 10349 | `		/* Truly uncaught */` |
|      473 | 10350 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 | 10351 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10352 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10353 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10354 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10355 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10356 | `			}` |
|      ! 0 | 10357 | `		}` |
|      473 | 10358 | `		return rc;` |
|      ! 0 | 10359 | `	}else{` |
|       38 | 10360 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10361 | `		ph7_exception **apSaved = 0;` |
|        - | 10362 | `		sxu32 nSavedCount;` |
|        - | 10363 | `		sxi32 rc;` |
|       38 | 10364 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10365 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10366 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10367 | `		}` |
|        - | 10368 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10369 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10370 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10371 | `		 */` |
|       38 | 10372 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10373 | `		if( nSavedCount > 0 ){` |
|       11 | 10374 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10375 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 | 10376 | `			if( apSaved ){` |
|       11 | 10377 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10378 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 | 10379 | `				SySetReset(&pVm->aException);` |
|        3 | 10380 | `			}` |
|        3 | 10381 | `		}` |
|        - | 10382 | `		/* Create a private frame first */` |
|       38 | 10383 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10384 | `		if( rc == SXRET_OK ){` |
|       38 | 10385 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10386 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10387 | `			if( pObj ){` |
|       38 | 10388 | `				pThis->iRef++;` |
|       38 | 10389 | `				pObj->x.pOther = pThis;` |
|       38 | 10390 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10391 | `			}` |
|        - | 10392 | `			/* Execute the catch block */` |
|       38 | 10393 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10394 | `			/* Leave the frame */` |
|       38 | 10395 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10396 | `		}` |
|        - | 10397 | `		/* Restore the outer exception handlers */` |
|       38 | 10398 | `		if( apSaved ){` |
|        - | 10399 | `			sxu32 k;` |
|        - | 10400 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10401 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10402 | `			 * Restore the original outer entries.` |
|        - | 10403 | `			 */` |
|        8 | 10404 | `			SySetReset(&pVm->aException);` |
|       14 | 10405 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 | 10406 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 10407 | `			}` |
|        8 | 10408 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10409 | `		}` |
|        - | 10410 | `		/* Execute the finally block after catch */` |
|       38 | 10411 | `		if( pException->iHasFinally ){` |
|       11 | 10412 | `			pException->iFinallyDone = 1;` |
|        - | 10413 | `			{` |
|       11 | 10414 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 | 10415 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10416 | `					return SXERR_ABORT;` |
|        - | 10417 | `				}` |
|        - | 10418 | `			}` |
|        5 | 10419 | `		}` |
|       38 | 10420 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10421 | `			return SXERR_ABORT;` |
|        - | 10422 | `		}` |
|        - | 10423 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10424 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10425 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10426 | `		 */` |
|       38 | 10427 | `		if( pVm->pPendingException ){` |
|        6 | 10428 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 | 10429 | `			pVm->pPendingException = 0;` |
|        6 | 10430 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10431 | `		}` |
|        - | 10432 | `	}` |
|        - | 10433 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10434 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10435 | `	 */` |
|       34 | 10436 | `	return SXRET_OK;` |
|      259 | 10437 |  |
|        - | 10438 | `/*` |
|        - | 10439 | ` * Section:` |
|        - | 10440 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10441 | ` * Status:` |
|        - | 10442 | ` *    Stable.` |
|        - | 10443 | ` */` |
|        - | 10444 | `/*` |
|        - | 10445 | ` * string ph7version(void)` |
|        - | 10446 | ` *  Returns the running version of the PH7 version.` |
|        - | 10447 | ` * Parameters` |
|        - | 10448 | ` *  None` |
|        - | 10449 | ` * Return` |
|        - | 10450 | ` * Current PH7 version.` |
|        - | 10451 | ` */` |
|        2 | 10452 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10453 |  |
|        1 | 10454 | `	SXUNUSED(nArg);` |
|        1 | 10455 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10456 | `	/* Current engine version */` |
|        3 | 10457 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10458 | `	return PH7_OK;` |
|        1 | 10459 |  |
|        - | 10460 | `/*` |
|        - | 10461 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10462 | ` */` |
|        - | 10463 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10464 | ` "<html><head>"\` |
|        - | 10465 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10466 | ` "<style type=\"text/css\">"\` |
|        - | 10467 | ` "div {"\` |
|        - | 10468 | `     "border: 1px solid #cccccc;"\` |
|        - | 10469 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10470 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10471 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10472 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10473 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10474 | `     "-o-border-radius: 10px;"\` |
|        - | 10475 | `     "border-radius: 10px;"\` |
|        - | 10476 | `     "padding-left: 2em;"\` |
|        - | 10477 | `     "background-color: white;"\` |
|        - | 10478 | `     "margin-left: auto;"\` |
|        - | 10479 | `     "font-family: verdana;"\` |
|        - | 10480 | `     "padding-right: 2em;"\` |
|        - | 10481 | `     "margin-right: auto;"\` |
|        - | 10482 | `     "}"\` |
|        - | 10483 | `     "body {"\` |
|        - | 10484 | `     "padding: 0.2em;"\` |
|        - | 10485 | `     "font-style: normal;"\` |
|        - | 10486 | `     "font-size: medium;"\` |
|        - | 10487 | `     "background-color: #f2f2f2;"\` |
|        - | 10488 | `     "}"\` |
|        - | 10489 | `     "hr {"\` |
|        - | 10490 | `     "border-style: solid none none;"\` |
|        - | 10491 | `     "border-width: 1px medium medium;"\` |
|        - | 10492 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10493 | `     "height: 1px;"\` |
|        - | 10494 | `     "}"\` |
|        - | 10495 | `     "a {"\` |
|        - | 10496 | `     "color: #3366cc;"\` |
|        - | 10497 | `     "text-decoration: none;"\` |
|        - | 10498 | `     "}"\` |
|        - | 10499 | `     "a:hover {"\` |
|        - | 10500 | `     "color: #999999;"\` |
|        - | 10501 | `     "}"\` |
|        - | 10502 | `     "a:active {"\` |
|        - | 10503 | `     "color: #663399;"\` |
|        - | 10504 | `     "}"\` |
|        - | 10505 | `     "h1 {"\` |
|        - | 10506 | `     "margin: 0;"\` |
|        - | 10507 | `     "padding: 0;"\` |
|        - | 10508 | `     "font-family: Verdana;"\` |
|        - | 10509 | `     "font-weight: bold;"\` |
|        - | 10510 | `     "font-style: normal;"\` |
|        - | 10511 | `     "font-size: medium;"\` |
|        - | 10512 | `     "text-transform: capitalize;"\` |
|        - | 10513 | `     "color: #0a328c;"\` |
|        - | 10514 | `     "}"\` |
|        - | 10515 | `     "p {"\` |
|        - | 10516 | `     "margin: 0 auto;"\` |
|        - | 10517 | `     "font-size: medium;"\` |
|        - | 10518 | `     "font-style: normal;"\` |
|        - | 10519 | `     "font-family: verdana;"\` |
|        - | 10520 | `     "}"\` |
|        - | 10521 | `"</style></head><body>"\` |
|        - | 10522 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10523 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10524 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10525 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10526 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10527 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10528 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10529 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10530 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10531 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10532 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10533 |  |
|        - | 10534 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10535 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10536 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10537 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10538 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10539 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10540 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10541 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10542 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10543 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10544 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10545 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10546 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10547 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10548 |  |
|        - | 10549 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10550 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10551 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10552 | `"&nbsp;*<br>"\` |
|        - | 10553 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10554 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10555 | `"&nbsp;* are met:<br>"\` |
|        - | 10556 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10557 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10558 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10559 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10560 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10561 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10562 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10563 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10564 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10565 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10566 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10567 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10568 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10569 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10570 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10571 | `"&nbsp;*<br>"\` |
|        - | 10572 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10573 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10574 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10575 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10576 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10577 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10578 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10579 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10580 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10581 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10582 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10583 | `"&nbsp;*/<br>"\` |
|        - | 10584 | `"</span></small></small></p>"\` |
|        - | 10585 | `"</div></body></html>"` |
|        - | 10586 | `/*` |
|        - | 10587 | ` * bool ph7credits(void)` |
|        - | 10588 | ` * bool ph7info(void)` |
|        - | 10589 | ` * bool ph7copyright(void)` |
|        - | 10590 | ` *  Prints out the credits for PH7 engine` |
|        - | 10591 | ` * Parameters` |
|        - | 10592 | ` *  None` |
|        - | 10593 | ` * Return` |
|        - | 10594 | ` *  Always TRUE` |
|        - | 10595 | ` */` |
|        2 | 10596 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10597 |  |
|        3 | 10598 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10599 | `	/* Expand the HTML page above*/` |
|        3 | 10600 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10601 | `	ph7_context_output_format(` |
|        1 | 10602 | `		pCtx,` |
|        - | 10603 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10604 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10605 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10606 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10607 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10608 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10609 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10610 | `#ifdef __WINNT__` |
|        - | 10611 | `		"Windows NT"` |
|        - | 10612 | `#elif defined(__UNIXES__)` |
|        - | 10613 | `		"UNIX-Like"` |
|        - | 10614 | `#else` |
|        - | 10615 | `		"Other OS"` |
|        - | 10616 | `#endif` |
|        - | 10617 | `		);` |
|        3 | 10618 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10619 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10620 | `	SXUNUSED(apArg);` |
|        - | 10621 | `	/* Return TRUE */` |
|        - | 10622 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10623 | `	return PH7_OK;` |
|        1 | 10624 |  |
|        - | 10625 | `/*` |
|        - | 10626 | ` * Section:` |
|        - | 10627 | ` *    URL related routines.` |
|        - | 10628 | ` * Status:` |
|        - | 10629 | ` *    Stable.` |
|        - | 10630 | ` */` |
|        - | 10631 | `/*` |
|        - | 10632 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10633 | ` *  Parse a URL and return its fields.` |
|        - | 10634 | ` * Parameters` |
|        - | 10635 | ` *  $url` |
|        - | 10636 | ` *   The URL to parse.` |
|        - | 10637 | ` * $component` |
|        - | 10638 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10639 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10640 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10641 | ` *  in which case the return value will be an integer).` |
|        - | 10642 | ` * Return` |
|        - | 10643 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10644 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10645 | ` *  this array are:` |
|        - | 10646 | ` *   scheme - e.g. http` |
|        - | 10647 | ` *   host` |
|        - | 10648 | ` *   port` |
|        - | 10649 | ` *   user` |
|        - | 10650 | ` *   pass` |
|        - | 10651 | ` *   path` |
|        - | 10652 | ` *   query - after the question mark ?` |
|        - | 10653 | ` *   fragment - after the hashmark #` |
|        - | 10654 | ` * Note:` |
|        - | 10655 | ` *  FALSE is returned on failure.` |
|        - | 10656 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10657 | ` *  with the standard PHP engine.` |
|        - | 10658 | ` */` |
|       28 | 10659 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10660 |  |
|        - | 10661 | `	const char *zStr; /* Input string */` |
|        - | 10662 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10663 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10664 | `	int nLen;` |
|        - | 10665 | `	sxi32 rc;` |
|       29 | 10666 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10667 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10668 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10669 | `		return PH7_OK;` |
|        - | 10670 | `	}` |
|        - | 10671 | `	/* Extract the given URI */` |
|       29 | 10672 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10673 | `	if( nLen < 1 ){` |
|        - | 10674 | `		/* Nothing to process,return FALSE */` |
|        3 | 10675 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10676 | `		return PH7_OK;` |
|        - | 10677 | `	}` |
|        - | 10678 | `	/* Get a parse */` |
|       27 | 10679 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10680 | `	if( rc != SXRET_OK ){` |
|        - | 10681 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10682 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10683 | `		return PH7_OK;` |
|        - | 10684 | `	}` |
|       27 | 10685 | `	if( nArg > 1 ){` |
|      ! 0 | 10686 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10687 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10688 | `		switch(nComponent){` |
|      ! 0 | 10689 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10690 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10691 | `			if( pComp->nByte < 1 ){` |
|        - | 10692 | `				/* No available value,return NULL */` |
|      ! 0 | 10693 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10694 | `			}else{` |
|      ! 0 | 10695 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10696 | `			}` |
|      ! 0 | 10697 | `			break;` |
|      ! 0 | 10698 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10699 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10700 | `			if( pComp->nByte < 1 ){` |
|        - | 10701 | `				/* No available value,return NULL */` |
|      ! 0 | 10702 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10703 | `			}else{` |
|      ! 0 | 10704 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10705 | `			}` |
|      ! 0 | 10706 | `			break;` |
|      ! 0 | 10707 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10708 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10709 | `			if( pComp->nByte < 1 ){` |
|        - | 10710 | `				/* No available value,return NULL */` |
|      ! 0 | 10711 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10712 | `			}else{` |
|      ! 0 | 10713 | `				int iPort = 0;` |
|        - | 10714 | `				/* Cast the value to integer */` |
|      ! 0 | 10715 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10716 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10717 | `			}` |
|      ! 0 | 10718 | `			break;` |
|      ! 0 | 10719 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10720 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10721 | `			if( pComp->nByte < 1 ){` |
|        - | 10722 | `				/* No available value,return NULL */` |
|      ! 0 | 10723 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10724 | `			}else{` |
|      ! 0 | 10725 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10726 | `			}` |
|      ! 0 | 10727 | `			break;` |
|      ! 0 | 10728 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10729 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10730 | `			if( pComp->nByte < 1 ){` |
|        - | 10731 | `				/* No available value,return NULL */` |
|      ! 0 | 10732 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10733 | `			}else{` |
|      ! 0 | 10734 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10735 | `			}` |
|      ! 0 | 10736 | `			break;` |
|      ! 0 | 10737 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10738 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10739 | `			if( pComp->nByte < 1 ){` |
|        - | 10740 | `				/* No available value,return NULL */` |
|      ! 0 | 10741 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10742 | `			}else{` |
|      ! 0 | 10743 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10744 | `			}` |
|      ! 0 | 10745 | `			break;` |
|      ! 0 | 10746 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10747 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10748 | `			if( pComp->nByte < 1 ){` |
|        - | 10749 | `				/* No available value,return NULL */` |
|      ! 0 | 10750 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10751 | `			}else{` |
|      ! 0 | 10752 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10753 | `			}` |
|      ! 0 | 10754 | `			break;` |
|      ! 0 | 10755 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10756 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10757 | `			if( pComp->nByte < 1 ){` |
|        - | 10758 | `				/* No available value,return NULL */` |
|      ! 0 | 10759 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10760 | `			}else{` |
|      ! 0 | 10761 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10762 | `			}` |
|      ! 0 | 10763 | `			break;` |
|      ! 0 | 10764 | `		default:` |
|        - | 10765 | `			/* No such entry,return NULL */` |
|      ! 0 | 10766 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10767 | `			break;` |
|        - | 10768 | `		}` |
|      ! 0 | 10769 | `	}else{` |
|        - | 10770 | `		ph7_value *pArray,*pValue;` |
|        - | 10771 | `		/* Return an associative array */` |
|       27 | 10772 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10773 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10774 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10775 | `			/* Out of memory */` |
|      ! 0 | 10776 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10777 | `			/* Return false */` |
|      ! 0 | 10778 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10779 | `			return PH7_OK;` |
|        - | 10780 | `		}` |
|        - | 10781 | `		/* Fill the array */` |
|       27 | 10782 | `		pComp = &sURI.sScheme;` |
|       27 | 10783 | `		if( pComp->nByte > 0 ){` |
|       19 | 10784 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10785 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10786 | `		}` |
|        - | 10787 | `		/* Reset the string cursor */` |
|       27 | 10788 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10789 | `		pComp = &sURI.sHost;` |
|       27 | 10790 | `		if( pComp->nByte > 0 ){` |
|       25 | 10791 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10792 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10793 | `		}` |
|        - | 10794 | `		/* Reset the string cursor */` |
|       27 | 10795 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10796 | `		pComp = &sURI.sPort;` |
|       27 | 10797 | `		if( pComp->nByte > 0 ){` |
|       11 | 10798 | `			int iPort = 0;/* cc warning */` |
|        - | 10799 | `			/* Convert to integer */` |
|       11 | 10800 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10801 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10802 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10803 | `		}` |
|        - | 10804 | `		/* Reset the string cursor */` |
|       27 | 10805 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10806 | `		pComp = &sURI.sUser;` |
|       27 | 10807 | `		if( pComp->nByte > 0 ){` |
|        7 | 10808 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10809 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10810 | `		}` |
|        - | 10811 | `		/* Reset the string cursor */` |
|       27 | 10812 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10813 | `		pComp = &sURI.sPass;` |
|       27 | 10814 | `		if( pComp->nByte > 0 ){` |
|        7 | 10815 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10816 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10817 | `		}` |
|        - | 10818 | `		/* Reset the string cursor */` |
|       27 | 10819 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10820 | `		pComp = &sURI.sPath;` |
|       27 | 10821 | `		if( pComp->nByte > 0 ){` |
|       17 | 10822 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10823 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10824 | `		}` |
|        - | 10825 | `		/* Reset the string cursor */` |
|       27 | 10826 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10827 | `		pComp = &sURI.sQuery;` |
|       27 | 10828 | `		if( pComp->nByte > 0 ){` |
|        5 | 10829 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10830 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10831 | `		}` |
|        - | 10832 | `		/* Reset the string cursor */` |
|       27 | 10833 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10834 | `		pComp = &sURI.sFragment;` |
|       27 | 10835 | `		if( pComp->nByte > 0 ){` |
|        5 | 10836 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10837 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10838 | `		}` |
|        - | 10839 | `		/* Return the created array */` |
|       27 | 10840 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10841 | `		/* NOTE:` |
|        - | 10842 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10843 | `		 * automatically as soon we return from this function.` |
|        - | 10844 | `		 */` |
|        - | 10845 | `	}` |
|        - | 10846 | `	/* All done */` |
|       27 | 10847 | `	return PH7_OK;` |
|       15 | 10848 |  |
|        - | 10849 | `/*` |
|        - | 10850 | ` * Section:` |
|        - | 10851 | ` *   Array related routines.` |
|        - | 10852 | ` * Status:` |
|        - | 10853 | ` *    Stable.` |
|        - | 10854 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10855 | ` *  Array related functions that need access to the underlying` |
|        - | 10856 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10857 | ` */` |
|        - | 10858 | `/*` |
|        - | 10859 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10860 | ` * of the following structure.` |
|        - | 10861 | ` */` |
|        - | 10862 | `struct compact_data` |
|        - | 10863 |  |
|        - | 10864 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10865 | `	int nRecCount;      /* Recursion count */` |
|        - | 10866 | `};` |
|        - | 10867 | `/*` |
|        - | 10868 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 10869 | ` */` |
|      ! 0 | 10870 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 10871 |  |
|      ! 0 | 10872 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 10873 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 10874 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10875 | `	/* Act according to the hashmap value */` |
|      ! 0 | 10876 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 10877 | `		SyString sVar;` |
|      ! 0 | 10878 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 10879 | `		if( sVar.nByte > 0 ){` |
|        - | 10880 | `			/* Query the current frame */` |
|      ! 0 | 10881 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 10882 | `			/* ^` |
|        - | 10883 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 10884 | `			 */` |
|      ! 0 | 10885 | `			if( pKey ){` |
|        - | 10886 | `				/* Perform the insertion */` |
|      ! 0 | 10887 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 10888 | `			}` |
|      ! 0 | 10889 | `		}` |
|      ! 0 | 10890 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 10891 | `		int rc;` |
|        - | 10892 | `		/* Recursively traverse this array */` |
|      ! 0 | 10893 | `		pData->nRecCount++;` |
|      ! 0 | 10894 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 10895 | `		pData->nRecCount--;` |
|      ! 0 | 10896 | `		return rc;` |
|        - | 10897 | `	}` |
|      ! 0 | 10898 | `	return SXRET_OK;` |
|      ! 0 | 10899 |  |
|        - | 10900 | `/*` |
|        - | 10901 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 10902 | ` *  Create array containing variables and their values.` |
|        - | 10903 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 10904 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 10905 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 10906 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 10907 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 10908 | ` * Parameters` |
|        - | 10909 | ` *  $varname` |
|        - | 10910 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 10911 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 10912 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 10913 | ` *   it recursively.` |
|        - | 10914 | ` * Return` |
|        - | 10915 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 10916 | ` */` |
|        2 | 10917 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10918 |  |
|        - | 10919 | `	ph7_value *pArray,*pObj;` |
|        3 | 10920 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10921 | `	const char *zName;` |
|        - | 10922 | `	SyString sVar;` |
|        - | 10923 | `	int i,nLen;` |
|        3 | 10924 | `	if( nArg < 1 ){` |
|        - | 10925 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 10926 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10927 | `		return PH7_OK;` |
|        - | 10928 | `	}` |
|        - | 10929 | `	/* Create the array */` |
|        3 | 10930 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10931 | `	if( pArray == 0 ){` |
|        - | 10932 | `		/* Out of memory */` |
|      ! 0 | 10933 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10934 | `		/* Return NULL */` |
|      ! 0 | 10935 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10936 | `		return PH7_OK;` |
|        - | 10937 | `	}` |
|        - | 10938 | `	/* Perform the requested operation */` |
|        7 | 10939 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 10940 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 10941 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 10942 | `				struct compact_data sData;` |
|      ! 0 | 10943 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 10944 | `				/* Recursively walk the array */` |
|      ! 0 | 10945 | `				sData.nRecCount = 0;` |
|      ! 0 | 10946 | `				sData.pArray = pArray;` |
|      ! 0 | 10947 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 10948 | `			}` |
|      ! 0 | 10949 | `		}else{` |
|        - | 10950 | `			/* Extract variable name */` |
|        5 | 10951 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 10952 | `			if( nLen > 0 ){` |
|        5 | 10953 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 10954 | `				/* Check if the variable is available in the current frame */` |
|        5 | 10955 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 10956 | `				if( pObj ){` |
|        5 | 10957 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 10958 | `				}` |
|        2 | 10959 | `			}` |
|        - | 10960 | `		}` |
|        3 | 10961 | `	}` |
|        - | 10962 | `	/* Return the array */` |
|        3 | 10963 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10964 | `	return PH7_OK;` |
|        2 | 10965 |  |
|        - | 10966 | `/*` |
|        - | 10967 | ` * The [extract()] function store it's state information in an instance` |
|        - | 10968 | ` * of the following structure.` |
|        - | 10969 | ` */` |
|        - | 10970 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 10971 | `struct extract_aux_data` |
|        - | 10972 |  |
|        - | 10973 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 10974 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 10975 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 10976 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 10977 | `	int iFlags;           /* Control flags */` |
|        - | 10978 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 10979 | `};` |
|        - | 10980 | `/* Forward declaration */` |
|        - | 10981 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 10982 | `/*` |
|        - | 10983 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 10984 | ` *   Import variables into the current symbol table from an array.` |
|        - | 10985 | ` * Parameters` |
|        - | 10986 | ` * $var_array` |
|        - | 10987 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 10988 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 10989 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 10990 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 10991 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 10992 | ` * $extract_type` |
|        - | 10993 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 10994 | ` *  It can be one of the following values:` |
|        - | 10995 | ` *   EXTR_OVERWRITE` |
|        - | 10996 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 10997 | ` *   EXTR_SKIP` |
|        - | 10998 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 10999 | ` *   EXTR_PREFIX_SAME` |
|        - | 11000 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11001 | ` *   EXTR_PREFIX_ALL` |
|        - | 11002 | ` *       Prefix all variable names with prefix.` |
|        - | 11003 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11004 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11005 | ` *   EXTR_IF_EXISTS` |
|        - | 11006 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11007 | ` *       otherwise do nothing.` |
|        - | 11008 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11009 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11010 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11011 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11012 | ` *      the current symbol table.` |
|        - | 11013 | ` * $prefix` |
|        - | 11014 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11015 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11016 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11017 | ` *  underscore character.` |
|        - | 11018 | ` * Return` |
|        - | 11019 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11020 | ` */` |
|        4 | 11021 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11022 |  |
|        - | 11023 | `	extract_aux_data sAux;` |
|        - | 11024 | `	ph7_hashmap *pMap;` |
|        5 | 11025 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11026 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11027 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11028 | `		return PH7_OK;` |
|        - | 11029 | `	}` |
|        - | 11030 | `	/* Point to the target hashmap */` |
|        5 | 11031 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11032 | `	if( pMap->nEntry < 1 ){` |
|        - | 11033 | `		/* Empty map,return  0 */` |
|      ! 0 | 11034 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11035 | `		return PH7_OK;` |
|        - | 11036 | `	}` |
|        - | 11037 | `	/* Prepare the aux data */` |
|        5 | 11038 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11039 | `	if( nArg > 1 ){` |
|        3 | 11040 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11041 | `		if( nArg > 2 ){` |
|      ! 0 | 11042 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11043 | `		}` |
|        1 | 11044 | `	}` |
|        5 | 11045 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11046 | `	/* Invoke the worker callback */` |
|        5 | 11047 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11048 | `	/* Number of variables successfully imported */` |
|        5 | 11049 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11050 | `	return PH7_OK;` |
|        3 | 11051 |  |
|        - | 11052 | `/*` |
|        - | 11053 | ` * Worker callback for the [extract()] function defined` |
|        - | 11054 | ` * below.` |
|        - | 11055 | ` */` |
|        8 | 11056 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11057 |  |
|        9 | 11058 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11059 | `	int iFlags = pAux->iFlags;` |
|        9 | 11060 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11061 | `	ph7_value *pObj;` |
|        - | 11062 | `	SyString sVar;` |
|        9 | 11063 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11064 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11065 | `	}` |
|        - | 11066 | `	/* Perform a string cast */` |
|        9 | 11067 | `	PH7_MemObjToString(pKey);` |
|        9 | 11068 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11069 | `		/* Unavailable variable name */` |
|      ! 0 | 11070 | `		return SXRET_OK;` |
|        - | 11071 | `	}` |
|        9 | 11072 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11073 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11074 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11075 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11076 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11077 | `			);` |
|      ! 0 | 11078 | `	}else{` |
|       13 | 11079 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11080 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11081 | `	}` |
|        9 | 11082 | `	sVar.zString = pAux->zWorker;` |
|        - | 11083 | `	/* Try to extract the variable */` |
|        9 | 11084 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11085 | `	if( pObj ){` |
|        - | 11086 | `		/* Collision */` |
|        5 | 11087 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11088 | `			return SXRET_OK;` |
|        - | 11089 | `		}` |
|        5 | 11090 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11091 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11092 | `				/* Already prefixed */` |
|      ! 0 | 11093 | `				return SXRET_OK;` |
|        - | 11094 | `			}` |
|      ! 0 | 11095 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11096 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11097 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11098 | `				);` |
|      ! 0 | 11099 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11100 | `		}` |
|        3 | 11101 | `	}else{` |
|        - | 11102 | `		/* Create the variable */` |
|        5 | 11103 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11104 | `	}` |
|        9 | 11105 | `	if( pObj ){` |
|        - | 11106 | `		/* Overwrite the old value */` |
|        9 | 11107 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11108 | `		/* Increment counter */` |
|        9 | 11109 | `		pAux->iCount++;` |
|        4 | 11110 | `	}` |
|        9 | 11111 | `	return SXRET_OK;` |
|        5 | 11112 |  |
|        - | 11113 | `/*` |
|        - | 11114 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11115 | ` * defined below.` |
|        - | 11116 | ` */` |
|        2 | 11117 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11118 |  |
|        3 | 11119 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11120 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11121 | `	ph7_value *pObj;` |
|        - | 11122 | `	SyString sVar;` |
|        - | 11123 | `	/* Perform a string cast */` |
|        3 | 11124 | `	PH7_MemObjToString(pKey);` |
|        3 | 11125 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11126 | `		/* Unavailable variable name */` |
|      ! 0 | 11127 | `		return SXRET_OK;` |
|        - | 11128 | `	}` |
|        3 | 11129 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11130 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11131 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11132 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11133 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11134 | `			);` |
|        2 | 11135 | `	}else{` |
|      ! 0 | 11136 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11137 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11138 | `	}` |
|        3 | 11139 | `	sVar.zString = pAux->zWorker;` |
|        - | 11140 | `	/* Extract the variable */` |
|        3 | 11141 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11142 | `	if( pObj ){` |
|        3 | 11143 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11144 | `	}` |
|        3 | 11145 | `	return SXRET_OK;` |
|        2 | 11146 |  |
|        - | 11147 | `/*` |
|        - | 11148 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11149 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11150 | ` * Parameters` |
|        - | 11151 | ` * $types` |
|        - | 11152 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11153 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11154 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11155 | ` *  POST includes the POST uploaded file information.` |
|        - | 11156 | ` *  Note:` |
|        - | 11157 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11158 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11159 | ` * $prefix` |
|        - | 11160 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11161 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11162 | ` *  variable named $pref_userid.` |
|        - | 11163 | ` * Return` |
|        - | 11164 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11165 | ` */` |
|        2 | 11166 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11167 |  |
|        - | 11168 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11169 | `	extract_aux_data sAux;` |
|        - | 11170 | `	int nLen,nPrefixLen;` |
|        - | 11171 | `	ph7_value *pSuper;` |
|        - | 11172 | `	ph7_vm *pVm;` |
|        - | 11173 | `	/* By default import only $_GET variables  */` |
|        3 | 11174 | `	zImport = "G";` |
|        3 | 11175 | `	nLen = (int)sizeof(char);` |
|        3 | 11176 | `	zPrefix = 0;` |
|        3 | 11177 | `	nPrefixLen = 0;` |
|        3 | 11178 | `	if( nArg > 0 ){` |
|        3 | 11179 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11180 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11181 | `		}` |
|        3 | 11182 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11183 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11184 | `		}` |
|        1 | 11185 | `	}` |
|        - | 11186 | `	/* Point to the underlying VM */` |
|        3 | 11187 | `	pVm = pCtx->pVm;` |
|        - | 11188 | `	/* Initialize the aux data */` |
|        3 | 11189 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11190 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11191 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11192 | `	sAux.pVm = pVm;` |
|        - | 11193 | `	/* Extract */` |
|        3 | 11194 | `	zEnd = &zImport[nLen];` |
|        5 | 11195 | `	while( zImport < zEnd ){` |
|        3 | 11196 | `		int c = zImport[0];` |
|        3 | 11197 | `		pSuper = 0;` |
|        3 | 11198 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11199 | `			/* Import $_GET variables */` |
|        3 | 11200 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11201 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11202 | `			/* Import $_POST variables */` |
|      ! 0 | 11203 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11204 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11205 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11206 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11207 | `		}` |
|        3 | 11208 | `		if( pSuper ){` |
|        - | 11209 | `			/* Iterate throw array entries */` |
|        3 | 11210 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11211 | `		}` |
|        - | 11212 | `		/* Advance the cursor */` |
|        3 | 11213 | `		zImport++;` |
|        1 | 11214 | `	}` |
|        - | 11215 | `	/* All done,return TRUE*/` |
|        3 | 11216 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11217 | `	return PH7_OK;` |
|        1 | 11218 |  |
|        - | 11219 | `/*` |
|        - | 11220 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11221 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11222 | ` * information.` |
|        - | 11223 | ` */` |
|    10426 | 11224 | `static sxi32 VmEvalChunk(` |
|        - | 11225 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11226 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11227 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11228 | `	int iFlags,         /* Compile flag */` |
|        - | 11229 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11230 | `	)` |
|        2 | 11231 |  |
|        - | 11232 | `	SySet *pByteCode,aByteCode;` |
|        - | 11233 | `	SyBlob sSavedNs;` |
|    10428 | 11234 | `	ProcConsumer xErr = 0;` |
|    10428 | 11235 | `	void *pErrData = 0;` |
|        - | 11236 | `	/* Initialize bytecode container */` |
|    10428 | 11237 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10428 | 11238 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11239 | `	/* Reset the code generator */` |
|    10428 | 11240 | `	if( bTrueReturn ){` |
|        - | 11241 | `		/* Included file,log compile-time errors */` |
|     7637 | 11242 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7637 | 11243 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3818 | 11244 | `	}` |
|    10428 | 11245 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11246 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11247 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11248 | `	 * the caller's namespace is restored. */` |
|    10428 | 11249 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10428 | 11250 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10428 | 11251 | `	if( bTrueReturn ){` |
|        - | 11252 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7637 | 11253 | `		SyBlobReset(&pVm->sNamespace);` |
|     3818 | 11254 | `	}` |
|        - | 11255 | `	/* Swap bytecode container */` |
|    10428 | 11256 | `	pByteCode = pVm->pByteContainer;` |
|    10428 | 11257 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11258 | `	/* Compile the chunk */` |
|    10428 | 11259 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15641 | 11260 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11261 | `		/* Compilation error,return false */` |
|        3 | 11262 | `		if( pCtx ){` |
|        3 | 11263 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11264 | `		}` |
|        2 | 11265 | `	}else{` |
|        - | 11266 | `		/* Mount any newly defined classes */` |
|        - | 11267 | `		SyHashEntry *pEntry;` |
|        - | 11268 | `		ph7_class *pClass;` |
|        - | 11269 | `		ph7_value sResult; /* Return value */` |
|        - | 11270 | `		sxi32 rc;` |
|    10426 | 11271 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   315820 | 11272 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   300184 | 11273 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11274 | `			/* Only mount classes that haven't been mounted yet */` |
|   300184 | 11275 | `			if( !pClass->bMounted ){` |
|    74248 | 11276 | `				rc = VmMountUserClass(pVm,pClass);` |
|    74248 | 11277 | `				if( rc != SXRET_OK ){` |
|        - | 11278 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11279 | `					if( pCtx ){` |
|      ! 0 | 11280 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11281 | `					}` |
|      ! 0 | 11282 | `					goto Cleanup;` |
|        - | 11283 | `				}` |
|    37123 | 11284 | `			}` |
|        2 | 11285 | `		}` |
|    10426 | 11286 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11287 | `			/* Out of memory */` |
|      ! 0 | 11288 | `			if( pCtx ){` |
|      ! 0 | 11289 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11290 | `			}` |
|      ! 0 | 11291 | `			goto Cleanup;` |
|        - | 11292 | `		}` |
|    10426 | 11293 | `		if( bTrueReturn ){` |
|        - | 11294 | `			/* Assume a boolean true return value */` |
|     7637 | 11295 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3819 | 11296 | `		}else{` |
|        - | 11297 | `			/* Assume a null return value */` |
|     2790 | 11298 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11299 | `		}` |
|        - | 11300 | `		/* Execute the compiled chunk */` |
|    10426 | 11301 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10426 | 11302 | `		if( pCtx ){` |
|        - | 11303 | `			/* Set the execution result */` |
|     7650 | 11304 | `			ph7_result_value(pCtx,&sResult);` |
|     3824 | 11305 | `		}` |
|    10426 | 11306 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11307 | `	}` |
|     5213 | 11308 | `Cleanup:` |
|        - | 11309 | `	/* Cleanup the mess left behind */` |
|    10428 | 11310 | `	pVm->pByteContainer = pByteCode;` |
|    10428 | 11311 | `	SySetRelease(&aByteCode);` |
|        - | 11312 | `	/* Restore caller's namespace state */` |
|    10428 | 11313 | `	SyBlobReset(&pVm->sNamespace);` |
|    10428 | 11314 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10428 | 11315 | `	SyBlobRelease(&sSavedNs);` |
|    10428 | 11316 | `	return SXRET_OK;` |
|        2 | 11317 |  |
|        - | 11318 | `/*` |
|        - | 11319 | ` * value eval(string $code)` |
|        - | 11320 | ` *   Evaluate a string as PHP code.` |
|        - | 11321 | ` * Parameter` |
|        - | 11322 | ` *  code: PHP code to evaluate.` |
|        - | 11323 | ` * Return` |
|        - | 11324 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11325 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11326 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11327 | ` */` |
|       16 | 11328 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11329 |  |
|        - | 11330 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 11331 | `	if( nArg < 1 ){` |
|        - | 11332 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11333 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11334 | `		return SXRET_OK;` |
|        - | 11335 | `	}` |
|        - | 11336 | `	/* Chunk to evaluate */` |
|       18 | 11337 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 11338 | `	if( sChunk.nByte < 1 ){` |
|        - | 11339 | `		/* Empty string,return NULL */` |
|        3 | 11340 | `		ph7_result_null(pCtx);` |
|        3 | 11341 | `		return SXRET_OK;` |
|        - | 11342 | `	}` |
|        - | 11343 | `	/* Eval the chunk */` |
|       16 | 11344 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 11345 | `	return SXRET_OK;` |
|       10 | 11346 |  |
|        - | 11347 | `/*` |
|        - | 11348 | ` * Check if a file path is already included.` |
|        - | 11349 | ` */` |
|    15268 | 11350 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 11351 |  |
|        - | 11352 | `	SyString *aEntries;` |
|        - | 11353 | `	sxu32 n;` |
|    15269 | 11354 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11355 | `	/* Perform a linear search */` |
| 58267061 | 11356 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 58251799 | 11357 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11358 | `			/* Already included */` |
|        7 | 11359 | `			return TRUE;` |
|        - | 11360 | `		}` |
| 29125897 | 11361 | `	}` |
|    15263 | 11362 | `	return FALSE;` |
|     7635 | 11363 |  |
|        - | 11364 | `/*` |
|        - | 11365 | ` * Push a file path in the appropriate VM container.` |
|        - | 11366 | ` */` |
|    18036 | 11367 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11368 |  |
|        - | 11369 | `	SyString sPath;` |
|        - | 11370 | `	char *zDup;` |
|        - | 11371 | `#ifdef __WINNT__` |
|        - | 11372 | `	char *zCur;` |
|        - | 11373 | `#endif` |
|        - | 11374 | `	sxi32 rc;` |
|    18038 | 11375 | `	if( nLen < 0 ){` |
|     2770 | 11376 | `		nLen = SyStrlen(zPath);` |
|     1384 | 11377 | `	}` |
|        - | 11378 | `	/* Duplicate the file path first */` |
|    18038 | 11379 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18038 | 11380 | `	if( zDup == 0 ){` |
|      ! 0 | 11381 | `		return SXERR_MEM;` |
|        - | 11382 | `	}` |
|        - | 11383 | `#ifdef __WINNT__` |
|        - | 11384 | `	/* Normalize path on windows` |
|        - | 11385 | `	 * Example:` |
|        - | 11386 | `	 *    Path/To/File.php` |
|        - | 11387 | `	 * becomes` |
|        - | 11388 | `	 *   path\to\file.php` |
|        - | 11389 | `	 */` |
|        2 | 11390 | `	zCur = zDup;` |
|        2 | 11391 | `	while( zCur[0] != 0 ){` |
|        2 | 11392 | `		if( zCur[0] == '/' ){` |
|        2 | 11393 | `			zCur[0] = '\\';` |
|        2 | 11394 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11395 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11396 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11397 | `		}` |
|        2 | 11398 | `		zCur++;` |
|        2 | 11399 | `	}` |
|        - | 11400 | `#endif` |
|        - | 11401 | `	/* Install the file path */` |
|    18038 | 11402 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18038 | 11403 | `	if( !bMain ){` |
|    15269 | 11404 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11405 | `			/* Already included */` |
|        7 | 11406 | `			*pNew = 0;` |
|        4 | 11407 | `		}else{` |
|        - | 11408 | `			/* Insert in the corresponding container */` |
|    15263 | 11409 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15263 | 11410 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11411 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11412 | `				return rc;` |
|        - | 11413 | `			}` |
|    15263 | 11414 | `			*pNew = 1;` |
|        - | 11415 | `		}` |
|     7634 | 11416 | `	}` |
|    18038 | 11417 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18038 | 11418 | `	return SXRET_OK;` |
|     9020 | 11419 |  |
|        - | 11420 | `/*` |
|        - | 11421 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11422 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11423 | ` * indicates failure.` |
|        - | 11424 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11425 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11426 | ` * operations.` |
|        - | 11427 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11428 | ` * this function is a no-op.` |
|        - | 11429 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11430 | ` * constructs for more information.` |
|        - | 11431 | ` */` |
|     7642 | 11432 | `static sxi32 VmExecIncludedFile(` |
|        - | 11433 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11434 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11435 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11436 | `	 )` |
|        2 | 11437 |  |
|        - | 11438 | `	sxi32 rc;` |
|        - | 11439 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11440 | `	const ph7_io_stream *pStream;` |
|        - | 11441 | `	SyBlob sContents;` |
|        - | 11442 | `	void *pHandle;` |
|        - | 11443 | `	ph7_vm *pVm;` |
|        - | 11444 | `	int isNew;` |
|        - | 11445 | `	/* Initialize fields */` |
|     7644 | 11446 | `	pVm = pCtx->pVm;` |
|     7644 | 11447 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7644 | 11448 | `	isNew = 0;` |
|        - | 11449 | `	/* Extract the associated stream */` |
|     7644 | 11450 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11451 | `	/*` |
|        - | 11452 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11453 | `	 * in a read-only mode.` |
|        - | 11454 | `	 */` |
|     7644 | 11455 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7644 | 11456 | `	if( pHandle == 0 ){` |
|        3 | 11457 | `		return SXERR_IO;` |
|        - | 11458 | `	}` |
|     7641 | 11459 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7641 | 11460 | `	if( IncludeOnce && !isNew ){` |
|        - | 11461 | `		/* Already included */` |
|        5 | 11462 | `		rc = SXERR_EXISTS;` |
|        3 | 11463 | `	}else{` |
|        - | 11464 | `		/* Read the whole file contents */` |
|     7637 | 11465 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7637 | 11466 | `		if( rc == SXRET_OK ){` |
|        - | 11467 | `			SyString sScript;` |
|        - | 11468 | `			/* Compile and execute the script */` |
|     7637 | 11469 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7637 | 11470 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3818 | 11471 | `		}` |
|        - | 11472 | `	}` |
|        - | 11473 | `	/* Pop from the set of included file */` |
|     7641 | 11474 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11475 | `	/* Close the handle */` |
|     7641 | 11476 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11477 | `	/* Release the working buffer */` |
|     7641 | 11478 | `	SyBlobRelease(&sContents);` |
|        - | 11479 | `#else` |
|        - | 11480 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11481 | `	SXUNUSED(pPath);` |
|        - | 11482 | `	SXUNUSED(IncludeOnce);` |
|        - | 11483 | `	rc = SXERR_IO;` |
|        - | 11484 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7641 | 11485 | `	return rc;` |
|     3823 | 11486 |  |
|        - | 11487 | `/*` |
|        - | 11488 | ` * string get_include_path(void)` |
|        - | 11489 | ` *  Gets the current include_path configuration option.` |
|        - | 11490 | ` * Parameter` |
|        - | 11491 | ` *  None` |
|        - | 11492 | ` * Return` |
|        - | 11493 | ` *  Included paths as a string` |
|        - | 11494 | ` */` |
|        2 | 11495 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11496 |  |
|        3 | 11497 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11498 | `	SyString *aEntry;` |
|        - | 11499 | `	int dir_sep;` |
|        - | 11500 | `	sxu32 n;` |
|        - | 11501 | `#ifdef __WINNT__` |
|        1 | 11502 | `	dir_sep = ';';` |
|        - | 11503 | `#else` |
|        - | 11504 | `	/* Assume UNIX path separator */` |
|        2 | 11505 | `	dir_sep = ':';` |
|        - | 11506 | `#endif` |
|        1 | 11507 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11508 | `	SXUNUSED(apArg);` |
|        - | 11509 | `	/* Point to the list of import paths */` |
|        3 | 11510 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11511 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11512 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11513 | `		if( n > 0 ){` |
|        - | 11514 | `			/* Append dir seprator */` |
|      ! 0 | 11515 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11516 | `		}` |
|        - | 11517 | `		/* Append path */` |
|        3 | 11518 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11519 | `	}` |
|        3 | 11520 | `	return PH7_OK;` |
|        1 | 11521 |  |
|        - | 11522 | `/*` |
|        - | 11523 | ` * string get_get_included_files(void)` |
|        - | 11524 | ` *  Gets the current include_path configuration option.` |
|        - | 11525 | ` * Parameter` |
|        - | 11526 | ` *  None` |
|        - | 11527 | ` * Return` |
|        - | 11528 | ` *  Included paths as a string` |
|        - | 11529 | ` */` |
|        2 | 11530 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11531 |  |
|        3 | 11532 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11533 | `	ph7_value *pArray,*pWorker;` |
|        - | 11534 | `	SyString *pEntry;` |
|        - | 11535 | `	int c,d;` |
|        - | 11536 | `	/* Create an array and a working value */` |
|        3 | 11537 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11538 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11539 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11540 | `		/* Out of memory,return null */` |
|      ! 0 | 11541 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11542 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11543 | `		SXUNUSED(apArg);` |
|      ! 0 | 11544 | `		return PH7_OK;` |
|        - | 11545 | `	}` |
|        3 | 11546 | `	c = d = '/';` |
|        - | 11547 | `#ifdef __WINNT__` |
|        1 | 11548 | `	d = '\\';` |
|        - | 11549 | `#endif` |
|        - | 11550 | `	/* Iterate throw entries */` |
|        3 | 11551 | `	SySetResetCursor(pFiles);` |
|     3689 | 11552 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11553 | `		const char *zBase,*zEnd;` |
|        - | 11554 | `		int iLen;` |
|        - | 11555 | `		/* reset the string cursor */` |
|     3687 | 11556 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11557 | `		/* Extract base name */` |
|     3687 | 11558 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11559 | `		/* Ignore trailing '/' */` |
|     5530 | 11560 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11561 | `			zEnd--;` |
|      ! 0 | 11562 | `		}` |
|     3687 | 11563 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113770 | 11564 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108241 | 11565 | `			zEnd--;` |
|        1 | 11566 | `		}` |
|     3687 | 11567 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3687 | 11568 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11569 | `		/* Copy entry name */` |
|     3687 | 11570 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11571 | `		/* Perform the insertion */` |
|     3687 | 11572 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11573 | `	}` |
|        - | 11574 | `	/* All done,return the created array */` |
|        3 | 11575 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11576 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11577 | `	 * by the engine as soon we return from this foreign` |
|        - | 11578 | `	 * function.` |
|        - | 11579 | `	 */` |
|        3 | 11580 | `	return PH7_OK;` |
|        2 | 11581 |  |
|        - | 11582 | `/*` |
|        - | 11583 | ` * include:` |
|        - | 11584 | ` * According to the PHP reference manual.` |
|        - | 11585 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11586 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11587 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11588 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11589 | ` *  and the current working directory before failing. The include()` |
|        - | 11590 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11591 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11592 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11593 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11594 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11595 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11596 | ` *  directory to find the requested file.` |
|        - | 11597 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11598 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11599 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11600 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11601 | ` */` |
|     7630 | 11602 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11603 |  |
|        - | 11604 | `	SyString sFile;` |
|        - | 11605 | `	sxi32 rc;` |
|     7632 | 11606 | `	if( nArg < 1 ){` |
|        - | 11607 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11608 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11609 | `		return SXRET_OK;` |
|        - | 11610 | `	}` |
|        - | 11611 | `	/* File to include */` |
|     7632 | 11612 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7632 | 11613 | `	if( sFile.nByte < 1 ){` |
|        - | 11614 | `		/* Empty string,return NULL */` |
|      ! 0 | 11615 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11616 | `		return SXRET_OK;` |
|        - | 11617 | `	}` |
|        - | 11618 | `	/* Open,compile and execute the desired script */` |
|     7632 | 11619 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7632 | 11620 | `	if( rc != SXRET_OK ){` |
|        - | 11621 | `		/* Emit a warning and return false */` |
|        3 | 11622 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11623 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11624 | `	}` |
|     7632 | 11625 | `	return SXRET_OK;` |
|     3817 | 11626 |  |
|        - | 11627 | `/*` |
|        - | 11628 | ` * include_once:` |
|        - | 11629 | ` *  According to the PHP reference manual.` |
|        - | 11630 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11631 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11632 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11633 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11634 | ` *   just once.` |
|        - | 11635 | ` */` |
|        4 | 11636 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11637 |  |
|        - | 11638 | `	SyString sFile;` |
|        - | 11639 | `	sxi32 rc;` |
|        5 | 11640 | `	if( nArg < 1 ){` |
|        - | 11641 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11642 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11643 | `		return SXRET_OK;` |
|        - | 11644 | `	}` |
|        - | 11645 | `	/* File to include */` |
|        5 | 11646 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11647 | `	if( sFile.nByte < 1 ){` |
|        - | 11648 | `		/* Empty string,return NULL */` |
|      ! 0 | 11649 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11650 | `		return SXRET_OK;` |
|        - | 11651 | `	}` |
|        - | 11652 | `	/* Open,compile and execute the desired script */` |
|        5 | 11653 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11654 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11655 | `		/* File already included,return TRUE */` |
|        3 | 11656 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11657 | `		return SXRET_OK;` |
|        - | 11658 | `	}` |
|        3 | 11659 | `	if( rc != SXRET_OK ){` |
|        - | 11660 | `		/* Emit a warning and return false */` |
|      ! 0 | 11661 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11662 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11663 | ` 	}` |
|        3 | 11664 | `	return SXRET_OK;` |
|        3 | 11665 |  |
|        - | 11666 | `/*` |
|        - | 11667 | ` * require.` |
|        - | 11668 | ` *  According to the PHP reference manual.` |
|        - | 11669 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11670 | ` *   also produce a fatal level error.` |
|        - | 11671 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11672 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11673 | ` */` |
|        4 | 11674 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11675 |  |
|        - | 11676 | `	SyString sFile;` |
|        - | 11677 | `	sxi32 rc;` |
|        5 | 11678 | `	if( nArg < 1 ){` |
|        - | 11679 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11680 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11681 | `		return SXRET_OK;` |
|        - | 11682 | `	}` |
|        - | 11683 | `	/* File to include */` |
|        5 | 11684 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11685 | `	if( sFile.nByte < 1 ){` |
|        - | 11686 | `		/* Empty string,return NULL */` |
|      ! 0 | 11687 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11688 | `		return SXRET_OK;` |
|        - | 11689 | `	}` |
|        - | 11690 | `	/* Open,compile and execute the desired script */` |
|        5 | 11691 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11692 | `	if( rc != SXRET_OK ){` |
|        - | 11693 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11694 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11695 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11696 | `		return PH7_ABORT;` |
|        - | 11697 | `	}` |
|        5 | 11698 | `	return SXRET_OK;` |
|        3 | 11699 |  |
|        - | 11700 | `/*` |
|        - | 11701 | ` * require_once:` |
|        - | 11702 | ` *  According to the PHP reference manual.` |
|        - | 11703 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11704 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11705 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11706 | ` *   and how it differs from its non _once siblings.` |
|        - | 11707 | ` */` |
|        4 | 11708 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11709 |  |
|        - | 11710 | `	SyString sFile;` |
|        - | 11711 | `	sxi32 rc;` |
|        5 | 11712 | `	if( nArg < 1 ){` |
|        - | 11713 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11714 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11715 | `		return SXRET_OK;` |
|        - | 11716 | `	}` |
|        - | 11717 | `	/* File to include */` |
|        5 | 11718 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11719 | `	if( sFile.nByte < 1 ){` |
|        - | 11720 | `		/* Empty string,return NULL */` |
|      ! 0 | 11721 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11722 | `		return SXRET_OK;` |
|        - | 11723 | `	}` |
|        - | 11724 | `	/* Open,compile and execute the desired script */` |
|        5 | 11725 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11726 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11727 | `		/* File already included,return TRUE */` |
|        3 | 11728 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11729 | `		return SXRET_OK;` |
|        - | 11730 | `	}` |
|        3 | 11731 | `	if( rc != SXRET_OK ){` |
|        - | 11732 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11733 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11734 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11735 | `		return PH7_ABORT;` |
|        - | 11736 | `	}` |
|        3 | 11737 | `	return SXRET_OK;` |
|        3 | 11738 |  |
|        - | 11739 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11740 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11741 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11742 | `/* Table of built-in VM functions. */` |
|        - | 11743 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 11744 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 11745 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 11746 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 11747 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 11748 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 11749 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 11750 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 11751 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 11752 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 11753 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 11754 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 11755 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 11756 | `	    /* Constants management */` |
|        - | 11757 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 11758 | `	{ "define",   vm_builtin_define               },` |
|        - | 11759 | `	{ "constant", vm_builtin_constant             },` |
|        - | 11760 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 11761 | `	   /* Class/Object functions */` |
|        - | 11762 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 11763 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 11764 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 11765 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 11766 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 11767 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 11768 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 11769 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 11770 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 11771 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 11772 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 11773 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 11774 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 11775 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 11776 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 11777 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 11778 | `	   /* Random numbers/strings generators */` |
|        - | 11779 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 11780 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 11781 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 11782 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 11783 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 11784 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11785 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11786 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 11787 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11788 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11789 | `	   /* Language constructs functions */` |
|        - | 11790 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 11791 | `	{ "print", vm_builtin_print                   },` |
|        - | 11792 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 11793 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 11794 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 11795 | `	  /* Variable handling functions */` |
|        - | 11796 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 11797 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 11798 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 11799 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 11800 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 11801 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 11802 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 11803 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 11804 | `	  /* Ouput control functions */` |
|        - | 11805 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 11806 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 11807 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 11808 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 11809 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 11810 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 11811 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 11812 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 11813 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 11814 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 11815 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 11816 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 11817 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 11818 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 11819 | `	  /* Assertion functions */` |
|        - | 11820 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 11821 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 11822 | `	  /* Error reporting functions */` |
|        - | 11823 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 11824 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 11825 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 11826 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 11827 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 11828 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 11829 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 11830 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 11831 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 11832 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 11833 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 11834 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 11835 | `	  /* Release info */` |
|        - | 11836 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 11837 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 11838 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 11839 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 11840 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 11841 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 11842 | `	  /* hashmap */` |
|        - | 11843 | `	{"compact",          vm_builtin_compact       },` |
|        - | 11844 | `	{"extract",          vm_builtin_extract       },` |
|        - | 11845 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 11846 | `	  /* URL related function */` |
|        - | 11847 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 11848 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 11849 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11850 | `	   /* XML processing functions */` |
|        - | 11851 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 11852 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 11853 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 11854 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 11855 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 11856 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 11857 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 11858 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 11859 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 11860 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 11861 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 11862 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 11863 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 11864 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 11865 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 11866 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 11867 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 11868 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 11869 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 11870 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 11871 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 11872 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11873 | `	   /* UTF-8 encoding/decoding */` |
|        - | 11874 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 11875 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 11876 | `	   /* Command line processing */` |
|        - | 11877 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 11878 | `	   /* JSON encoding/decoding */` |
|        - | 11879 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 11880 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 11881 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 11882 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 11883 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 11884 | `	   /* Files/URI inclusion facility */` |
|        - | 11885 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 11886 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 11887 | `	{ "include",      vm_builtin_include          },` |
|        - | 11888 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 11889 | `	{ "require",      vm_builtin_require          },` |
|        - | 11890 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 11891 | `};` |
|        - | 11892 | `/*` |
|        - | 11893 | ` * Register the built-in VM functions defined above.` |
|        - | 11894 | ` */` |
|     2516 | 11895 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 11896 |  |
|        - | 11897 | `	sxi32 rc;` |
|        - | 11898 | `	sxu32 n;` |
|   314502 | 11899 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 11900 | `		/* Note that these special functions have access` |
|        - | 11901 | `		 * to the underlying virtual machine as their` |
|        - | 11902 | `		 * private data.` |
|        - | 11903 | `		 */` |
|   311986 | 11904 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   311986 | 11905 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 11906 | `			return rc;` |
|        - | 11907 | `		}` |
|   155994 | 11908 | `	}` |
|     2518 | 11909 | `	return SXRET_OK;` |
|     1260 | 11910 |  |
|        - | 11911 | `/*` |
|        - | 11912 | ` * Check if the given name refer to an installed class.` |
|        - | 11913 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 11914 | ` */` |
|    29326 | 11915 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 11916 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 11917 | `	const char *zName,  /* Name of the target class */` |
|        - | 11918 | `	sxu32 nByte,        /* zName length */` |
|        - | 11919 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 11920 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 11921 | `						 */` |
|        - | 11922 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 11923 | `	)` |
|        2 | 11924 |  |
|        - | 11925 | `	SyHashEntry *pEntry;` |
|        - | 11926 | `	ph7_class *pClass;` |
|    14663 | 11927 | `	SXUNUSED(iNest);` |
|        - | 11928 | `	/* Exact class lookup.` |
|        - | 11929 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 11930 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    29328 | 11931 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    29328 | 11932 | `	if( pEntry == 0 ){` |
|       10 | 11933 | `		return 0;` |
|        - | 11934 | `	}` |
|    29320 | 11935 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    29320 | 11936 | `	if( !iLoadable ){` |
|    28140 | 11937 | `		return pClass;` |
|        - | 11938 | `	}` |
|        - | 11939 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1182 | 11940 | `	while(pClass){` |
|     1182 | 11941 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1182 | 11942 | `			return pClass;` |
|        - | 11943 | `		}` |
|      ! 0 | 11944 | `		pClass = pClass->pNextName;` |
|      ! 0 | 11945 | `	}` |
|      ! 0 | 11946 | `	return 0;` |
|    14665 | 11947 |  |
|        - | 11948 | `/*` |
|        - | 11949 | ` * Reference Table Implementation` |
|        - | 11950 | ` * Status: stable <chm@symisc.net>` |
|        - | 11951 | ` * Intro` |
|        - | 11952 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 11953 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 11954 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 11955 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 11956 | ` *  Refer to the official for more information on this powerful` |
|        - | 11957 | ` *  extension.` |
|        - | 11958 | ` */` |
|        - | 11959 | `/*` |
|        - | 11960 | ` * Allocate a new reference entry.` |
|        - | 11961 | ` */` |
|  3020364 | 11962 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 11963 |  |
|        - | 11964 | `	VmRefObj *pRef;` |
|        - | 11965 | `	/* Allocate a new instance */` |
|  3020366 | 11966 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3020366 | 11967 | `	if( pRef == 0 ){` |
|      ! 0 | 11968 | `		return 0;` |
|        - | 11969 | `	}` |
|        - | 11970 | `	/* Zero the structure */` |
|  3020366 | 11971 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 11972 | `	/* Initialize fields */` |
|  3020366 | 11973 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3020366 | 11974 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3020366 | 11975 | `	pRef->nIdx = nIdx;` |
|  3020366 | 11976 | `	return pRef;` |
|  1510184 | 11977 |  |
|        - | 11978 | `/*` |
|        - | 11979 | ` * Default hash function used by the reference table` |
|        - | 11980 | ` * for lookup/insertion operations.` |
|        - | 11981 | ` */` |
| 16731688 | 11982 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 11983 |  |
|        - | 11984 | `	/* Calculate the hash based on the memory object index */` |
| 16731690 | 11985 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 11986 |  |
|        - | 11987 | `/*` |
|        - | 11988 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 11989 | ` * in the reference table.` |
|        - | 11990 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 11991 | ` * otherwise.` |
|        - | 11992 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 11993 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 11994 | ` * the reference implementation is consistent,solid and it's` |
|        - | 11995 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 11996 | ` * Refer to the official for more information on this powerful` |
|        - | 11997 | ` * extension.` |
|        - | 11998 | ` */` |
|  9011748 | 11999 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12000 |  |
|        - | 12001 | `	VmRefObj *pRef;` |
|        - | 12002 | `	sxu32 nBucket;` |
|        - | 12003 | `	/* Point to the appropriate bucket */` |
|  9011750 | 12004 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12005 | `	/* Perform the lookup */` |
|  9011750 | 12006 | `	pRef = pVm->apRefObj[nBucket];` |
| 19331285 | 12007 | `	for(;;){` |
| 38654558 | 12008 | `		if( pRef == 0 ){` |
|  3099588 | 12009 | `			break;` |
|        - | 12010 | `		}` |
| 35554972 | 12011 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12012 | `			/* Entry found */` |
|  5912164 | 12013 | `			return pRef;` |
|        - | 12014 | `		}` |
|        - | 12015 | `		/* Point to the next entry */` |
| 29642810 | 12016 | `		pRef = pRef->pNextCollide;` |
|        2 | 12017 | `	}` |
|        - | 12018 | `	/* No such entry,return NULL */` |
|  3099588 | 12019 | `	return 0;` |
|  4505876 | 12020 |  |
|        - | 12021 | `/*` |
|        - | 12022 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12023 | ` *` |
|        - | 12024 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12025 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12026 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12027 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12028 | ` * Refer to the official for more information on this powerful` |
|        - | 12029 | ` * extension.` |
|        - | 12030 | ` */` |
|  3020364 | 12031 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12032 |  |
|        - | 12033 | `	sxu32 nBucket;` |
|  3020366 | 12034 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12035 | `		VmRefObj **apNew;` |
|        - | 12036 | `		sxu32 nNew;` |
|        - | 12037 | `		/* Allocate a larger table */` |
|     4286 | 12038 | `		nNew = pVm->nRefSize << 1;` |
|     4286 | 12039 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4286 | 12040 | `		if( apNew ){` |
|     4286 | 12041 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12042 | `			sxu32 n;` |
|        - | 12043 | `			/* Zero the structure */` |
|     4286 | 12044 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12045 | `			/* Rehash all referenced entries */` |
|  2843494 | 12046 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12047 | `				/* Remove old collision links */` |
|  2839210 | 12048 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12049 | `				/* Point to the appropriate bucket */` |
|  2839210 | 12050 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12051 | `				/* Insert the entry  */` |
|  2839210 | 12052 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2839210 | 12053 | `				if( apNew[nBucket] ){` |
|  2298896 | 12054 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12055 | `				}` |
|  2839210 | 12056 | `				apNew[nBucket] = pEntry;` |
|        - | 12057 | `				/* Point to the next entry */` |
|  2839210 | 12058 | `				pEntry = pEntry->pNext;` |
|  1419606 | 12059 | `			}` |
|        - | 12060 | `			/* Release the old table */` |
|     4286 | 12061 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12062 | `			/* Install the new one */` |
|     4286 | 12063 | `			pVm->apRefObj = apNew;` |
|     4286 | 12064 | `			pVm->nRefSize = nNew;` |
|     2142 | 12065 | `		}` |
|     2142 | 12066 | `	}` |
|        - | 12067 | `	/* Point to the appropriate bucket */` |
|  3020366 | 12068 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12069 | `	/* Insert the entry */` |
|  3020366 | 12070 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3020366 | 12071 | `	if( pVm->apRefObj[nBucket] ){` |
|  2503621 | 12072 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1251831 | 12073 | `	}` |
|  3020366 | 12074 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3020366 | 12075 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3020366 | 12076 | `	pVm->nRefUsed++;` |
|  3020366 | 12077 | `	return SXRET_OK;` |
|        2 | 12078 |  |
|        - | 12079 | `/*` |
|        - | 12080 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12081 | ` * the reference table.` |
|        - | 12082 | ` * This function is invoked when the user perform an unset` |
|        - | 12083 | ` * call [i.e: unset($var); ].` |
|        - | 12084 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12085 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12086 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12087 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12088 | ` * Refer to the official for more information on this powerful` |
|        - | 12089 | ` * extension.` |
|        - | 12090 | ` */` |
|  2984508 | 12091 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12092 |  |
|        - | 12093 | `	ph7_hashmap_node **apNode;` |
|        - | 12094 | `	SyHashEntry **apEntry;` |
|        - | 12095 | `	sxu32 n;` |
|        - | 12096 | `	/* Point to the reference table */` |
|  2984510 | 12097 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2984510 | 12098 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12099 | `	/* Unlink the entry from the reference table */` |
|  3069598 | 12100 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    85090 | 12101 | `		if( apEntry[n] ){` |
|    85040 | 12102 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    42519 | 12103 | `		}` |
|    42546 | 12104 | `	}` |
|  5886622 | 12105 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2902114 | 12106 | `		if( apNode[n] ){` |
|     6794 | 12107 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3396 | 12108 | `		}` |
|  1451058 | 12109 | `	}` |
|  2984510 | 12110 | `	if( pRef->pPrevCollide ){` |
|  1124142 | 12111 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   562451 | 12112 | `	}else{` |
|  1860370 | 12113 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12114 | `	}` |
|  2984510 | 12115 | `	if( pRef->pNextCollide ){` |
|  1692989 | 12116 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   846485 | 12117 | `	}` |
|  2984510 | 12118 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12119 | `	/* Release the node */` |
|  2984510 | 12120 | `	SySetRelease(&pRef->aReference);` |
|  2984510 | 12121 | `	SySetRelease(&pRef->aArrEntries);` |
|  2984510 | 12122 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2984510 | 12123 | `	pVm->nRefUsed--;` |
|  2984510 | 12124 | `	return SXRET_OK;` |
|        2 | 12125 |  |
|        - | 12126 | `/*` |
|        - | 12127 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12128 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12129 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12130 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12131 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12132 | ` * Refer to the official for more information on this powerful` |
|        - | 12133 | ` * extension.` |
|        - | 12134 | ` */` |
|  3052636 | 12135 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12136 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12137 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12138 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12139 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12140 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12141 | `	)` |
|        2 | 12142 |  |
|  3052638 | 12143 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12144 | `	VmRefObj *pRef;` |
|        - | 12145 | `	/* Check if the referenced object already exists */` |
|  3052638 | 12146 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3052638 | 12147 | `	if( pRef == 0 ){` |
|        - | 12148 | `		/* Create a new entry */` |
|  3020366 | 12149 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3020366 | 12150 | `		if( pRef == 0 ){` |
|      ! 0 | 12151 | `			return SXERR_MEM;` |
|        - | 12152 | `		}` |
|  3020366 | 12153 | `		pRef->iFlags = iFlags;` |
|        - | 12154 | `		/* Install the entry */` |
|  3020366 | 12155 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1510182 | 12156 | `	}` |
|  3052638 | 12157 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3052638 | 12158 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12159 | `		VmSlot sRef;` |
|        - | 12160 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12161 | `		 * be deleted when we leave this frame.` |
|        - | 12162 | `		 */` |
|    79308 | 12163 | `		sRef.nIdx = nIdx;` |
|    79308 | 12164 | `		sRef.pUserData = pEntry;` |
|    79308 | 12165 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12166 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12167 | `		}` |
|    39653 | 12168 | `	}` |
|  3052638 | 12169 | `	if( pEntry ){` |
|        - | 12170 | `		/* Address of the hash-entry */` |
|   111388 | 12171 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    55693 | 12172 | `	}` |
|  3052638 | 12173 | `	if( pMapEntry ){` |
|        - | 12174 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2936278 | 12175 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1468138 | 12176 | `	}` |
|  3052638 | 12177 | `	return SXRET_OK;` |
|  1526320 | 12178 |  |
|        - | 12179 | `/*` |
|        - | 12180 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12181 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12182 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12183 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12184 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12185 | ` * Refer to the official for more information on this powerful` |
|        - | 12186 | ` * extension.` |
|        - | 12187 | ` */` |
|  2974598 | 12188 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12189 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12190 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12191 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12192 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12193 | `	)` |
|        2 | 12194 |  |
|        - | 12195 | `	VmRefObj *pRef;` |
|        - | 12196 | `	sxu32 n;` |
|        - | 12197 | `	/* Check if the referenced object already exists */` |
|  2974600 | 12198 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2974600 | 12199 | `	if( pRef == 0 ){` |
|        - | 12200 | `		/* Not such entry */` |
|    79218 | 12201 | `		return SXERR_NOTFOUND;` |
|        - | 12202 | `	}` |
|        - | 12203 | `	/* Remove the desired entry */` |
|  2895384 | 12204 | `	if( pEntry ){` |
|        - | 12205 | `		SyHashEntry **apEntry;` |
|       56 | 12206 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12207 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12208 | `			if( apEntry[n] == pEntry ){` |
|        - | 12209 | `				/* Nullify the entry */` |
|       56 | 12210 | `				apEntry[n] = 0;` |
|        - | 12211 | `				/*` |
|        - | 12212 | `				 * NOTE:` |
|        - | 12213 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12214 | `				 * we avoid wasting spaces.` |
|        - | 12215 | `				 */` |
|       27 | 12216 | `			}` |
|       79 | 12217 | `		}` |
|       27 | 12218 | `	}` |
|  2895384 | 12219 | `	if( pMapEntry ){` |
|        - | 12220 | `		ph7_hashmap_node **apNode;` |
|  2895330 | 12221 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5790752 | 12222 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2895424 | 12223 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12224 | `				/* nullify the entry */` |
|  2895330 | 12225 | `				apNode[n] = 0;` |
|  1447664 | 12226 | `			}` |
|  1447713 | 12227 | `		}` |
|  1447664 | 12228 | `	}` |
|  2895384 | 12229 | `	return SXRET_OK;` |
|  1487301 | 12230 |  |
|        - | 12231 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12232 | `/*` |
|        - | 12233 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12234 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12235 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12236 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12237 | ` * For more information on how to register IO stream devices,please` |
|        - | 12238 | ` * refer to the official documentation.` |
|        - | 12239 | ` */` |
|    23676 | 12240 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12241 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12242 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12243 | `	int nByte              /* *pzDevice length*/` |
|        - | 12244 | `	)` |
|        2 | 12245 |  |
|        - | 12246 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12247 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12248 | `	SyString sDev,sCur;` |
|        - | 12249 | `	sxu32 n,nEntry;` |
|        - | 12250 | `	int rc;` |
|        - | 12251 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23678 | 12252 | `	zNext = zCur = zIn = *pzDevice;` |
|    23678 | 12253 | `	zEnd = &zIn[nByte];` |
|  1512077 | 12254 | `	while( zIn < zEnd ){` |
|  1488403 | 12255 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12256 | `			/* Got one */` |
|        3 | 12257 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12258 | `			break;` |
|        - | 12259 | `		}` |
|        - | 12260 | `		/* Advance the cursor */` |
|  1488401 | 12261 | `		zIn++;` |
|        2 | 12262 | `	}` |
|    23678 | 12263 | `	if( zIn >= zEnd ){` |
|        - | 12264 | `		/* No such scheme,return the default stream */` |
|    23676 | 12265 | `		return pVm->pDefStream;` |
|        - | 12266 | `	}` |
|        3 | 12267 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12268 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12269 | `	SyStringFullTrim(&sDev);` |
|        - | 12270 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12271 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12272 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12273 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12274 | `		pStream = apStream[n];` |
|        3 | 12275 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12276 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12277 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12278 | `		if( rc == 0 ){` |
|        - | 12279 | `			/* Stream device found */` |
|        3 | 12280 | `			*pzDevice = zNext;` |
|        3 | 12281 | `			return pStream;` |
|        - | 12282 | `		}` |
|      ! 0 | 12283 | `	}` |
|        - | 12284 | `	/* No such stream,return NULL */` |
|      ! 0 | 12285 | `	return 0;` |
|    11840 | 12286 |  |
|        - | 12287 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12288 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12289 |  |
