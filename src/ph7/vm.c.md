# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4055/5316 lines (76.28%)

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
|   755218 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   755220 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   755190 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   755182 |    94 | `	return FALSE;` |
|   377633 |    95 |  |
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
|   446618 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   446620 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   446620 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   446616 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   446616 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   446616 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   446616 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   446616 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   446616 |   142 | `	pCons->xExpand = xExpand;` |
|   446616 |   143 | `	pCons->pUserData = pUserData;` |
|   446616 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   446616 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   446616 |   151 | `	return SXRET_OK;` |
|   223311 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   957000 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   957002 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   957002 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   957002 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   957002 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   957002 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   957002 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   957002 |   185 | `	pFunc->pVm   = pVm;` |
|   957002 |   186 | `	pFunc->xFunc = xFunc;` |
|   957002 |   187 | `	pFunc->pUserData = pUserData;` |
|   957002 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   957002 |   190 | `	*ppOut = pFunc;` |
|   957002 |   191 | `	return SXRET_OK;` |
|   478502 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   959200 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   959202 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   959202 |   213 | `	if( pEntry ){` |
|     2202 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2202 |   215 | `		pFunc->pUserData = pUserData;` |
|     2202 |   216 | `		pFunc->xFunc = xFunc;` |
|     2202 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2202 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   957002 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   957002 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   957002 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   957002 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   957002 |   233 | `	return SXRET_OK;` |
|   479602 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   103056 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   103058 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   103058 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   103058 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   103058 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   103058 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   103058 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   103058 |   260 | `	pFunc->iFlags = iFlags;` |
|   103058 |   261 | `	pFunc->pUserData = pUserData;` |
|   103058 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   103058 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   375410 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   375412 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    32138 |   283 | `		pName = &pFunc->sName;` |
|    16068 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   375412 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   375412 |   287 | `	if( pEntry ){` |
|   292048 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   292048 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   292048 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|    83366 |   297 | `	pFunc->pNextName = 0;` |
|    83366 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    83366 |   299 | `	return rc;` |
|   187707 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    29586 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    29588 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    29588 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    29588 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    29558 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    29558 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    29558 |   324 | `	return rc;` |
|    14795 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  2741536 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2741538 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  2741538 |   342 | `	sInstr.iP1 = iP1;` |
|  2741538 |   343 | `	sInstr.iP2 = iP2;` |
|  2741538 |   344 | `	sInstr.p3  = p3;` |
|  2741538 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   174094 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    87046 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  2741538 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2741538 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  2741538 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   250520 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   250522 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   250522 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   250522 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   125260 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   125262 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   171582 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   171584 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   171584 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|   765396 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|   765398 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   162886 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   162888 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   537052 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   537054 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    24944 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    24946 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    24946 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    24946 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    24946 |   417 | `	return &aInstr[n - 2];` |
|    12474 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    14620 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    14622 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    14622 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    14622 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    14622 |   437 | `	pFrame->pUserData = pUserData;` |
|    14622 |   438 | `	pFrame->pThis = pThis;` |
|    14622 |   439 | `	pFrame->pVm = pVm;` |
|    14622 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    14622 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    14622 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    14622 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    14622 |   444 | `	return pFrame;` |
|     7312 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Enter a VM frame.` |
|        - |   448 | ` */` |
|    14620 |   449 | `static sxi32 VmEnterFrame(` |
|        - |   450 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   451 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   452 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   453 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   454 | `	)` |
|        2 |   455 |  |
|        - |   456 | `	VmFrame *pFrame;` |
|        - |   457 | `	/* Allocate a new frame */` |
|    14622 |   458 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    14622 |   459 | `	if( pFrame == 0 ){` |
|      ! 0 |   460 | `		return SXERR_MEM;` |
|        - |   461 | `	}` |
|        - |   462 | `	/* Link to the list of active VM frame */` |
|    14622 |   463 | `	pFrame->pParent = pVm->pFrame;` |
|    14622 |   464 | `	pVm->pFrame = pFrame;` |
|    14622 |   465 | `	if( ppFrame ){` |
|        - |   466 | `		/* Write a pointer to the new VM frame */` |
|    12188 |   467 | `		*ppFrame = pFrame;` |
|     6093 |   468 | `	}` |
|    14622 |   469 | `	return SXRET_OK;` |
|     7312 |   470 |  |
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
|    12186 |   517 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   518 |  |
|    12188 |   519 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12188 |   520 | `	if( pCurFrame ){` |
|        - |   521 | `		/* Unlink from the list of active VM frame */` |
|    12188 |   522 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12188 |   523 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   524 | `			VmSlot  *aSlot;` |
|        - |   525 | `			sxu32 n;` |
|        - |   526 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12140 |   527 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    86792 |   528 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   529 | `				/* Unset the local variable */` |
|    74654 |   530 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    37328 |   531 | `			}` |
|        - |   532 | `			/* Remove local reference */` |
|    12140 |   533 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    86848 |   534 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    74710 |   535 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    37356 |   536 | `			}` |
|     6069 |   537 | `		}` |
|        - |   538 | `		/* Release internal containers */` |
|    12188 |   539 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12188 |   540 | `		SySetRelease(&pCurFrame->sArg);` |
|    12188 |   541 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12188 |   542 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   543 | `		/* Release the whole structure */` |
|    12188 |   544 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6093 |   545 | `	}` |
|    12188 |   546 |  |
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
|    87836 |   663 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   664 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   665 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   666 | `	)` |
|        2 |   667 |  |
|        - |   668 | `	ph7_class_method *pMeth;` |
|        - |   669 | `	ph7_class_attr *pAttr;` |
|        - |   670 | `	SyHashEntry *pEntry;` |
|        - |   671 | `	sxi32 rc;` |
|        - |   672 | `	/* Reset the loop cursor */` |
|    87838 |   673 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   674 | `	/* Process only static and constant attribute */` |
|   346416 |   675 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   676 | `		/* Extract the current attribute */` |
|   214662 |   677 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   214662 |   678 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    87838 |   700 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   701 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   702 | `		 */` |
|    45826 |   703 | `		return SXRET_OK;` |
|        - |   704 | `	}` |
|        - |   705 | `	/* Create constructor alias if not yet done */` |
|    42014 |   706 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   707 | `		/* User constructor with the same base class name */` |
|      276 |   708 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      276 |   709 | `		if( pEntry ){` |
|      ! 0 |   710 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   711 | `			/* Create the alias */` |
|      ! 0 |   712 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   713 | `		}` |
|      137 |   714 | `	}` |
|        - |   715 | `	/* Install the methods now */` |
|    42014 |   716 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   406300 |   717 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   343282 |   718 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   343282 |   719 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   343276 |   720 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   343276 |   721 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   722 | `				return rc;` |
|        - |   723 | `			}` |
|   171637 |   724 | `		}` |
|        2 |   725 | `	}` |
|        - |   726 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    42014 |   727 | `	pClass->bMounted = TRUE;` |
|    42014 |   728 | `	return SXRET_OK;` |
|    43920 |   729 |  |
|        - |   730 | `/*` |
|        - |   731 | ` * Allocate a private frame for attributes of the given` |
|        - |   732 | ` * class instance (Object in the PHP jargon).` |
|        - |   733 | ` */` |
|     1114 |   734 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   735 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   736 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   737 | `	)` |
|        2 |   738 |  |
|     1116 |   739 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   740 | `	ph7_class_attr *pAttr;` |
|        - |   741 | `	SyHashEntry *pEntry;` |
|        - |   742 | `	sxi32 rc;` |
|        - |   743 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1116 |   744 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4670 |   745 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   746 | `		VmClassAttr *pVmAttr;` |
|        - |   747 | `		/* Extract the current attribute */` |
|     3556 |   748 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3556 |   749 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3556 |   750 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   751 | `			return SXERR_MEM;` |
|        - |   752 | `		}` |
|     3556 |   753 | `		pVmAttr->pAttr = pAttr;` |
|     3556 |   754 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   755 | `			ph7_value *pMemObj;` |
|        - |   756 | `			/* Reserve a memory object for this attribute */` |
|     3550 |   757 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3550 |   758 | `			if( pMemObj == 0 ){` |
|      ! 0 |   759 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   760 | `				return SXERR_MEM;` |
|        - |   761 | `			}` |
|     3550 |   762 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3550 |   763 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   764 | `				/* Initialize attribute default value (any complex expression) */` |
|     1164 |   765 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      581 |   766 | `			}` |
|     3550 |   767 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3550 |   768 | `			if( rc != SXRET_OK ){` |
|        - |   769 | `				VmSlot sSlot;` |
|        - |   770 | `				/* Restore memory object */` |
|      ! 0 |   771 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   772 | `				sSlot.pUserData = 0;` |
|      ! 0 |   773 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   774 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   775 | `				return SXERR_MEM;` |
|        - |   776 | `			}` |
|        - |   777 | `			/* Install attribute in the reference table */` |
|     3550 |   778 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1776 |   779 | `		}else{` |
|        - |   780 | `			/* Install static/constant attribute */` |
|        8 |   781 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   782 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   783 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   784 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   785 | `				return SXERR_MEM;` |
|        - |   786 | `			}` |
|        - |   787 | `		}` |
|        2 |   788 | `	}` |
|     1116 |   789 | `	return SXRET_OK;` |
|      559 |   790 |  |
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
|   298000 |   802 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   803 |  |
|        - |   804 | `	ph7_value *pObj;` |
|        - |   805 | `	sxi32 rc;` |
|   298002 |   806 | `	if( pIndex ){` |
|        - |   807 | `		/* Object index in the object table */` |
|   290700 |   808 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   145349 |   809 | `	}` |
|        - |   810 | `	/* Reserve a slot for the new object */` |
|   298002 |   811 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   298002 |   812 | `	if( rc != SXRET_OK ){` |
|        - |   813 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   814 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   815 | `		 */` |
|      ! 0 |   816 | `		return 0;` |
|        - |   817 | `	}` |
|   298002 |   818 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   298002 |   819 | `	return pObj;` |
|   149002 |   820 |  |
|        - |   821 | `/*` |
|        - |   822 | ` * Reserve a memory object.` |
|        - |   823 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   824 | ` */` |
|  2139384 |   825 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   826 |  |
|        - |   827 | `	ph7_value *pObj;` |
|        - |   828 | `	sxi32 rc;` |
|  2139386 |   829 | `	if( pIndex ){` |
|        - |   830 | `		/* Object index in the object table */` |
|  2139386 |   831 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1069692 |   832 | `	}` |
|        - |   833 | `	/* Reserve a slot for the new object */` |
|  2139386 |   834 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2139386 |   835 | `	if( rc != SXRET_OK ){` |
|        - |   836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   838 | `		 */` |
|      ! 0 |   839 | `		return 0;` |
|        - |   840 | `	}` |
|  2139386 |   841 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2139386 |   842 | `	return pObj;` |
|  1069694 |   843 |  |
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
|     2434 |  1196 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1197 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1198 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1199 | `	 )` |
|        2 |  1200 |  |
|        - |  1201 | `	SyString sBuiltin;` |
|        - |  1202 | `	ph7_value *pObj;` |
|        - |  1203 | `	sxi32 rc;` |
|        - |  1204 | `	/* Zero the structure */` |
|     2436 |  1205 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1206 | `	/* Initialize VM fields */` |
|     2436 |  1207 | `	pVm->pEngine = &(*pEngine);` |
|     2436 |  1208 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1209 | `	/* Instructions containers */` |
|     2436 |  1210 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2436 |  1211 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2436 |  1212 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1213 | `	/* Object containers */` |
|     2436 |  1214 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2436 |  1215 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1216 | `	/* Virtual machine internal containers */` |
|     2436 |  1217 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2436 |  1218 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2436 |  1219 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2436 |  1220 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2436 |  1221 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2436 |  1222 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2436 |  1223 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2436 |  1224 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2436 |  1225 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2436 |  1226 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2436 |  1227 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2436 |  1228 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2436 |  1229 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2436 |  1230 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2436 |  1231 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2436 |  1232 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2436 |  1233 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2436 |  1234 | `	pVm->pPendingException = 0;` |
|        - |  1235 | `	/* Configuration containers */` |
|     2436 |  1236 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2436 |  1237 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2436 |  1238 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2436 |  1239 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2436 |  1240 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1241 | `	/* Error callbacks containers */` |
|     2436 |  1242 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2436 |  1243 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2436 |  1244 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2436 |  1245 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2436 |  1246 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1247 | `	/* Set a default recursion limit */` |
|        - |  1248 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2436 |  1249 | `	pVm->nMaxDepth = 32;` |
|        - |  1250 | `#else` |
|        - |  1251 | `	pVm->nMaxDepth = 16;` |
|        - |  1252 | `#endif` |
|        - |  1253 | `	/* Default assertion flags */` |
|     2436 |  1254 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1255 | `	/* JSON return status */` |
|     2436 |  1256 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1257 | `	/* PRNG context */` |
|     2436 |  1258 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1259 | `	/* Install the null constant */` |
|     2436 |  1260 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2436 |  1261 | `	if( pObj == 0 ){` |
|      ! 0 |  1262 | `		rc = SXERR_MEM;` |
|      ! 0 |  1263 | `		goto Err;` |
|        - |  1264 | `	}` |
|     2436 |  1265 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1266 | `	/* Install the boolean TRUE constant */` |
|     2436 |  1267 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2436 |  1268 | `	if( pObj == 0 ){` |
|      ! 0 |  1269 | `		rc = SXERR_MEM;` |
|      ! 0 |  1270 | `		goto Err;` |
|        - |  1271 | `	}` |
|     2436 |  1272 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1273 | `	/* Install the boolean FALSE constant */` |
|     2436 |  1274 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2436 |  1275 | `	if( pObj == 0 ){` |
|      ! 0 |  1276 | `		rc = SXERR_MEM;` |
|      ! 0 |  1277 | `		goto Err;` |
|        - |  1278 | `	}` |
|     2436 |  1279 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1280 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1281 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1282 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2436 |  1283 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2436 |  1284 | `	if( pObj == 0 ){` |
|      ! 0 |  1285 | `		rc = SXERR_MEM;` |
|      ! 0 |  1286 | `		goto Err;` |
|        - |  1287 | `	}` |
|     2436 |  1288 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1289 | `	/* Create the global frame */` |
|     2436 |  1290 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2436 |  1291 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1292 | `		goto Err;` |
|        - |  1293 | `	}` |
|        - |  1294 | `	/* Initialize the code generator */` |
|     2436 |  1295 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2436 |  1296 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1297 | `		goto Err;` |
|        - |  1298 | `	}` |
|        - |  1299 | `	/* VM correctly initialized,set the magic number */` |
|     2436 |  1300 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2436 |  1301 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1302 | `	/* Compile the built-in library */` |
|     2436 |  1303 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1304 | `	/* Reset the code generator */` |
|     2436 |  1305 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2436 |  1306 | `	return SXRET_OK;` |
|      ! 0 |  1307 | `Err:` |
|      ! 0 |  1308 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1309 | `	return rc;` |
|     1219 |  1310 |  |
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
|    30350 |  1340 | `static ph7_value * VmNewOperandStack(` |
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
|    30352 |  1353 | `	nInstr += VM_STACK_GUARD;` |
|    30352 |  1354 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    30352 |  1355 | `	if( pStack == 0 ){` |
|      ! 0 |  1356 | `		return 0;` |
|        - |  1357 | `	}` |
|        - |  1358 | `	/* Initialize the operand stack */` |
|  1926036 |  1359 | `	while( nInstr > 0 ){` |
|  1895686 |  1360 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1895686 |  1361 | `		--nInstr;` |
|        2 |  1362 | `	}` |
|        - |  1363 | `	/* Ready for bytecode execution */` |
|    30352 |  1364 | `	return pStack;` |
|    15177 |  1365 |  |
|        - |  1366 | `/* Forward declaration */` |
|        - |  1367 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1368 | `/*` |
|        - |  1369 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1370 | ` * This routine gets called by the PH7 engine after` |
|        - |  1371 | ` * successful compilation of the target PHP program.` |
|        - |  1372 | ` */` |
|     2200 |  1373 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1374 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1375 | `	)` |
|        2 |  1376 |  |
|        - |  1377 | `	SyHashEntry *pEntry;` |
|        - |  1378 | `	sxi32 rc;` |
|     2202 |  1379 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1380 | `		/* Initialize your VM first */` |
|      ! 0 |  1381 | `		return SXERR_CORRUPT;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* Mark the VM ready for byte-code execution */` |
|     2202 |  1384 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1385 | `	/* Release the code generator now we have compiled our program */` |
|     2202 |  1386 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1387 | `	/* Emit the DONE instruction */` |
|     2202 |  1388 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2202 |  1389 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1390 | `		return SXERR_MEM;` |
|        - |  1391 | `	}` |
|        - |  1392 | `	/* Script return value */` |
|     2202 |  1393 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1394 | `	/* Allocate a new operand stack */` |
|     2202 |  1395 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2202 |  1396 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1397 | `		return SXERR_MEM;` |
|        - |  1398 | `	}` |
|        - |  1399 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1400 | `	 * private data. */` |
|     2202 |  1401 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2202 |  1402 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1403 | `	/* Allocate the reference table */` |
|     2202 |  1404 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2202 |  1405 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2202 |  1406 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1407 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1408 | `		return SXERR_MEM;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Zero the reference table */` |
|     2202 |  1411 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1412 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2202 |  1413 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2202 |  1414 | `	if( rc != SXRET_OK ){` |
|        - |  1415 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1416 | `		return rc;` |
|        - |  1417 | `	}` |
|        - |  1418 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2202 |  1419 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2202 |  1420 | `	if( rc != SXRET_OK ){` |
|        - |  1421 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1422 | `		return rc;` |
|        - |  1423 | `	}` |
|        - |  1424 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2202 |  1425 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1426 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2202 |  1427 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1428 | `	/* Initialize and install static and constants class attributes */` |
|     2202 |  1429 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    28740 |  1430 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    26540 |  1431 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    26540 |  1432 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1433 | `			return rc;` |
|        - |  1434 | `		}` |
|        2 |  1435 | `	}` |
|        - |  1436 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2202 |  1437 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1438 | `	/* VM is ready for bytecode execution */` |
|     2202 |  1439 | `	return SXRET_OK;` |
|     1102 |  1440 |  |
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
|     2192 |  1460 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1461 |  |
|        - |  1462 | `	/* Set the stale magic number */` |
|     2194 |  1463 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1464 | `	/* Release the private memory subsystem */` |
|     2194 |  1465 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2194 |  1466 | `	return SXRET_OK;` |
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
|   542376 |  1478 | `static sxi32 VmInitCallContext(` |
|        - |  1479 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1480 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1481 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1482 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1483 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1484 | `	)` |
|        2 |  1485 |  |
|   542378 |  1486 | `	pOut->pFunc = pFunc;` |
|   542378 |  1487 | `	pOut->pVm   = pVm;` |
|   542378 |  1488 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   542378 |  1489 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1490 | `	/* Assume a null return value */` |
|   542378 |  1491 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   542378 |  1492 | `	pOut->pRet = pRet;` |
|   542378 |  1493 | `	pOut->iFlags = iFlags;` |
|   542378 |  1494 | `	return SXRET_OK;` |
|        2 |  1495 |  |
|        - |  1496 | `/*` |
|        - |  1497 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1498 | ` * left behind.` |
|        - |  1499 | ` */` |
|   542376 |  1500 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1501 |  |
|        - |  1502 | `	sxu32 n;` |
|   542378 |  1503 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6638 |  1504 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    18928 |  1505 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12292 |  1506 | `			if( apObj[n] == 0 ){` |
|        - |  1507 | `				/* Already released */` |
|      250 |  1508 | `				continue;` |
|        - |  1509 | `			}` |
|    12044 |  1510 | `			PH7_MemObjRelease(apObj[n]);` |
|    12044 |  1511 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6023 |  1512 | `		}` |
|     6638 |  1513 | `		SySetRelease(&pCtx->sVar);` |
|     3318 |  1514 | `	}` |
|   542378 |  1515 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   542378 |  1531 |  |
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
|  3190488 |  1562 | `static void VmPopOperand(` |
|        - |  1563 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1564 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1565 | `	)` |
|        2 |  1566 |  |
|  3190490 |  1567 | `	ph7_value *pTos = *ppTos;` |
|  6777746 |  1568 | `	while( nPop > 0 ){` |
|  3587258 |  1569 | `		PH7_MemObjRelease(pTos);` |
|  3587258 |  1570 | `		pTos--;` |
|  3587258 |  1571 | `		nPop--;` |
|        2 |  1572 | `	}` |
|        - |  1573 | `	/* Top of the stack */` |
|  3190490 |  1574 | `	*ppTos = pTos;` |
|  3190490 |  1575 |  |
|        - |  1576 | `/*` |
|        - |  1577 | ` * Reserve a memory object.` |
|        - |  1578 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1579 | ` */` |
|  2993126 |  1580 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1581 |  |
|  2993128 |  1582 | `	ph7_value *pObj = 0;` |
|        - |  1583 | `	VmSlot *pSlot;` |
|        - |  1584 | `	sxu32 nIdx;` |
|        - |  1585 | `	/* Check for a free slot */` |
|  2993128 |  1586 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2993128 |  1587 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2993128 |  1588 | `	if( pSlot ){` |
|   853744 |  1589 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   853744 |  1590 | `		nIdx = pSlot->nIdx;` |
|   426871 |  1591 | `	}` |
|  2993128 |  1592 | `	if( pObj == 0 ){` |
|        - |  1593 | `		/* Reserve a new memory object */` |
|  2139386 |  1594 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2139386 |  1595 | `		if( pObj == 0 ){` |
|      ! 0 |  1596 | `			return 0;` |
|        - |  1597 | `		}` |
|  1069692 |  1598 | `	}` |
|        - |  1599 | `	/* Set a null default value */` |
|  2993128 |  1600 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2993128 |  1601 | `	pObj->nIdx = nIdx;` |
|  2993128 |  1602 | `	return pObj;` |
|  1496565 |  1603 |  |
|        - |  1604 | `/*` |
|        - |  1605 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1606 | ` */` |
|    27670 |  1607 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1608 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1609 | `	const char *zKey,  /* Entry key */` |
|        - |  1610 | `	sxu32 nByte,       /* Key length */` |
|        - |  1611 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1612 | `	)` |
|        2 |  1613 |  |
|        - |  1614 | `	ph7_value sKey;` |
|        - |  1615 | `	sxi32 rc;` |
|    27672 |  1616 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    27672 |  1617 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1618 | `	/* Perform the insertion */` |
|    27672 |  1619 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    27672 |  1620 | `	PH7_MemObjRelease(&sKey);` |
|    27672 |  1621 | `	return rc;` |
|        2 |  1622 |  |
|        - |  1623 | `/*` |
|        - |  1624 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1625 | ` * Return a pointer to the variable value on success.` |
|        - |  1626 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1627 | ` */` |
|  2987706 |  1628 | `static ph7_value * VmExtractMemObj(` |
|        - |  1629 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1630 | `	const SyString *pName, /* Variable name */` |
|        - |  1631 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1632 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1633 | `	)` |
|        2 |  1634 |  |
|  2987708 |  1635 | `	int bNullify = FALSE;` |
|        - |  1636 | `	SyHashEntry *pEntry;` |
|        - |  1637 | `	VmFrame *pFrame;` |
|        - |  1638 | `	ph7_value *pObj;` |
|        - |  1639 | `	sxu32 nIdx;` |
|        - |  1640 | `	sxi32 rc;` |
|        - |  1641 | `	/* Point to the top active frame */` |
|  2987708 |  1642 | `	pFrame = pVm->pFrame;` |
|  2987726 |  1643 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1644 | `		/* Safely ignore the exception frame */` |
|       20 |  1645 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        2 |  1646 | `	}` |
|        - |  1647 | `	/* Perform the lookup */` |
|  2987708 |  1648 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1649 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1650 | `		pName = &sAnnon;` |
|        - |  1651 | `		/* Always nullify the object */` |
|      ! 0 |  1652 | `		bNullify = TRUE;` |
|      ! 0 |  1653 | `		bDup = FALSE;` |
|      ! 0 |  1654 | `	}` |
|        - |  1655 | `	/* Check the superglobals table first */` |
|  2987708 |  1656 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  2987708 |  1657 | `	if( pEntry == 0 ){` |
|        - |  1658 | `		/* Query the top active frame */` |
|  2987672 |  1659 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  2987672 |  1660 | `		if( pEntry == 0 ){` |
|    80930 |  1661 | `			char *zName = (char *)pName->zString;` |
|        - |  1662 | `			VmSlot sLocal;` |
|    80930 |  1663 | `			if( !bCreate ){` |
|        - |  1664 | `				/* Do not create the variable,return NULL instead */` |
|      632 |  1665 | `				return 0;` |
|        - |  1666 | `			}` |
|        - |  1667 | `			/* No such variable,automatically create a new one and install` |
|        - |  1668 | `			 * it in the current frame.` |
|        - |  1669 | `			 */` |
|    80300 |  1670 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    80300 |  1671 | `			if( pObj == 0 ){` |
|      ! 0 |  1672 | `				return 0;` |
|        - |  1673 | `			}` |
|    80300 |  1674 | `			nIdx = pObj->nIdx;` |
|    80300 |  1675 | `			if( bDup ){` |
|        - |  1676 | `				/* Duplicate name */` |
|      164 |  1677 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1678 | `				if( zName == 0 ){` |
|      ! 0 |  1679 | `					return 0;` |
|        - |  1680 | `				}` |
|       81 |  1681 | `			}` |
|        - |  1682 | `			/* Link to the top active VM frame */` |
|    80300 |  1683 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    80300 |  1684 | `			if( rc != SXRET_OK ){` |
|        - |  1685 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1686 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1687 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1688 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1689 | `				return 0;` |
|        - |  1690 | `			}` |
|    80300 |  1691 | `			if( pFrame->pParent != 0 ){` |
|        - |  1692 | `				/* Local variable */` |
|    74654 |  1693 | `				sLocal.nIdx = nIdx;` |
|    74654 |  1694 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    37328 |  1695 | `			}else{` |
|        - |  1696 | `				/* Register in the $GLOBALS array */` |
|     5648 |  1697 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1698 | `			}` |
|        - |  1699 | `			/* Install in the reference table */` |
|    80300 |  1700 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1701 | `			/* Save object index */` |
|    80300 |  1702 | `			pObj->nIdx = nIdx;` |
|    40151 |  1703 | `		}else{` |
|        - |  1704 | `			/* Extract variable contents */` |
|  2906744 |  1705 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2906744 |  1706 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2906744 |  1707 | `			if( bNullify && pObj ){` |
|      ! 0 |  1708 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1709 | `			}` |
|        - |  1710 | `		}` |
|  1493632 |  1711 | `	}else{` |
|        - |  1712 | `		/* Superglobal */` |
|       38 |  1713 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1714 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1715 | `	}` |
|  2987078 |  1716 | `	return pObj;` |
|  1493965 |  1717 |  |
|        - |  1718 | `/*` |
|        - |  1719 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1720 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1721 | ` */` |
|     2226 |  1722 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1723 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1724 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1725 | `	sxu32 nByte        /* zName length */` |
|        - |  1726 | `	)` |
|        2 |  1727 |  |
|        - |  1728 | `	SyHashEntry *pEntry;` |
|        - |  1729 | `	ph7_value *pValue;` |
|        - |  1730 | `	sxu32 nIdx;` |
|        - |  1731 | `	/* Query the superglobal table */` |
|     2228 |  1732 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2228 |  1733 | `	if( pEntry == 0 ){` |
|        - |  1734 | `		/* No such entry */` |
|      ! 0 |  1735 | `		return 0;` |
|        - |  1736 | `	}` |
|        - |  1737 | `	/* Extract the superglobal index in the global object pool */` |
|     2228 |  1738 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1739 | `	/* Extract the variable value  */` |
|     2228 |  1740 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2228 |  1741 | `	return pValue;` |
|     1115 |  1742 |  |
|        - |  1743 | `/*` |
|        - |  1744 | ` * Perform a raw hashmap insertion.` |
|        - |  1745 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1746 | ` */` |
|     2224 |  1747 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1748 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1749 | `	const char *zKey,   /* Entry key */` |
|        - |  1750 | `	int nKeylen,        /* zKey length*/` |
|        - |  1751 | `	const char *zData,  /* Entry data */` |
|        - |  1752 | `	int nLen            /* zData length */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|        - |  1755 | `	ph7_value sKey,sValue;` |
|        - |  1756 | `	sxi32 rc;` |
|     2226 |  1757 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2226 |  1758 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2226 |  1759 | `	if( zKey ){` |
|     2204 |  1760 | `		if( nKeylen < 0 ){` |
|     2204 |  1761 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1101 |  1762 | `		}` |
|     2204 |  1763 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1101 |  1764 | `	}` |
|     2226 |  1765 | `	if( zData ){` |
|     2226 |  1766 | `		if( nLen < 0 ){` |
|        - |  1767 | `			/* Compute length automatically */` |
|      ! 0 |  1768 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1769 | `		}` |
|     2226 |  1770 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1112 |  1771 | `	}` |
|        - |  1772 | `	/* Perform the insertion */` |
|     2226 |  1773 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2226 |  1774 | `	PH7_MemObjRelease(&sKey);` |
|     2226 |  1775 | `	PH7_MemObjRelease(&sValue);` |
|     2226 |  1776 | `	return rc;` |
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
|    35224 |  1791 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1792 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1793 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1794 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1795 | `	)` |
|        2 |  1796 |  |
|    35226 |  1797 | `	sxi32 rc = SXRET_OK;` |
|    35226 |  1798 | `	switch(nOp){` |
|     1100 |  1799 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2202 |  1800 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2202 |  1801 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1802 | `		/* VM output consumer callback */` |
|        - |  1803 | `#ifdef UNTRUST` |
|        - |  1804 | `		if( xConsumer == 0 ){` |
|        - |  1805 | `			rc = SXERR_CORRUPT;` |
|        - |  1806 | `			break;` |
|        - |  1807 | `		}` |
|        - |  1808 | `#endif` |
|        - |  1809 | `		/* Install the output consumer */` |
|     2202 |  1810 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2202 |  1811 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2202 |  1812 | `		break;` |
|        - |  1813 | `							   }` |
|     1100 |  1814 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1815 | `		/* Import path */` |
|        - |  1816 | `		  const char *zPath;` |
|        - |  1817 | `		  SyString sPath;` |
|     2202 |  1818 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1819 | `#if defined(UNTRUST)` |
|        - |  1820 | `		  if( zPath == 0 ){` |
|        - |  1821 | `			  rc = SXERR_EMPTY;` |
|        - |  1822 | `			  break;` |
|        - |  1823 | `		  }` |
|        - |  1824 | `#endif` |
|     2202 |  1825 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1826 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1827 | `#ifdef __WINNT__` |
|        2 |  1828 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1829 | `#endif` |
|     4402 |  1830 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1831 | `		  /* Remove leading and trailing white spaces */` |
|     2202 |  1832 | `		  SyStringFullTrim(&sPath);` |
|     2202 |  1833 | `		  if( sPath.nByte > 0 ){` |
|        - |  1834 | `			  /* Store the path in the corresponding conatiner */` |
|     2202 |  1835 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1100 |  1836 | `		  }` |
|     2202 |  1837 | `		  break;` |
|        - |  1838 | `									 }` |
|     1100 |  1839 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1840 | `		/* Run-Time Error report */` |
|     2202 |  1841 | `		pVm->bErrReport = 1;` |
|     2202 |  1842 | `		break;` |
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
|    11000 |  1864 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1865 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1866 | `		/* Create a new superglobal/global variable */` |
|    22002 |  1867 | `		const char *zName = va_arg(ap,const char *);` |
|    22002 |  1868 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    22002 |  1879 | `		nByte = SyStrlen(zName);` |
|    22002 |  1880 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1881 | `			/* Check if the superglobal is already installed */` |
|    22002 |  1882 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11002 |  1883 | `		}else{` |
|        - |  1884 | `			/* Query the top active VM frame */` |
|      ! 0 |  1885 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1886 | `		}` |
|    22002 |  1887 | `		if( pEntry ){` |
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
|    22002 |  1898 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    22002 |  1899 | `			if( pObj == 0 ){` |
|      ! 0 |  1900 | `				rc = SXERR_MEM;` |
|      ! 0 |  1901 | `				break;` |
|        - |  1902 | `			}` |
|    22002 |  1903 | `			nIdx = pObj->nIdx;` |
|        - |  1904 | `			/* Copy value */` |
|    22002 |  1905 | `			PH7_MemObjStore(pValue,pObj);` |
|    22002 |  1906 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1907 | `				/* Install the superglobal */` |
|    22002 |  1908 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11002 |  1909 | `			}else{` |
|        - |  1910 | `				/* Install in the current frame */` |
|      ! 0 |  1911 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1912 | `			}` |
|    22002 |  1913 | `			if( rc == SXRET_OK ){` |
|        - |  1914 | `				SyHashEntry *pRef;` |
|    22002 |  1915 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    22002 |  1916 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11002 |  1917 | `				}else{` |
|      ! 0 |  1918 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1919 | `				}` |
|        - |  1920 | `				/* Install in the reference table */` |
|    22002 |  1921 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    22002 |  1922 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1923 | `					/* Register in the $GLOBALS array */` |
|    22002 |  1924 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11000 |  1925 | `				}` |
|    11000 |  1926 | `			}` |
|        - |  1927 | `		}` |
|    22002 |  1928 | `		break;` |
|        - |  1929 | `									}` |
|     1101 |  1930 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1931 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1932 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1933 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1934 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1935 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1936 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2204 |  1937 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2204 |  1938 | `		const char *zValue = va_arg(ap,const char *);` |
|     2204 |  1939 | `		int nLen = va_arg(ap,int);` |
|        - |  1940 | `		ph7_hashmap *pMap;` |
|        - |  1941 | `		ph7_value *pValue;` |
|     2204 |  1942 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1943 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1944 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2203 |  1945 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1946 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1947 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2202 |  1948 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1949 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1950 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2202 |  1951 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1952 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1953 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2202 |  1954 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1955 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1956 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2202 |  1957 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1958 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1959 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1960 | `		}else{` |
|        - |  1961 | `			/* Extract the $_SERVER superglobal */` |
|     2202 |  1962 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1963 | `		}` |
|     2204 |  1964 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1965 | `			/* No such entry */` |
|      ! 0 |  1966 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1967 | `			break;` |
|        - |  1968 | `		}` |
|        - |  1969 | `		/* Point to the hashmap */` |
|     2204 |  1970 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1971 | `		/* Perform the insertion */` |
|     2204 |  1972 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2204 |  1973 | `		break;` |
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
|     2200 |  2024 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2025 | `		/* Register an IO stream device */` |
|     4402 |  2026 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2027 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6600 |  2028 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4402 |  2029 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2030 | `				/* Invalid stream */` |
|      ! 0 |  2031 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2032 | `				break;` |
|        - |  2033 | `		}` |
|     4402 |  2034 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2035 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2202 |  2036 | `			pVm->pDefStream = pStream;` |
|     1100 |  2037 | `		}` |
|        - |  2038 | `		/* Insert in the appropriate container */` |
|     4402 |  2039 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4402 |  2040 | `		break;` |
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
|    35226 |  2077 | `	return rc;` |
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
|    30350 |  2553 | `static sxi32 VmByteCodeExec(` |
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
|    30352 |  2569 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    30352 |  2570 | `	if( nTos < 0 ){` |
|    28746 |  2571 | `		pTos = &pStack[-1];` |
|    14374 |  2572 | `	}else{` |
|     1608 |  2573 | `		pTos = &pStack[nTos];` |
|        - |  2574 | `	}` |
|    30352 |  2575 | `	pc = 0;` |
|        - |  2576 | `	/* Execute as much as we can */` |
|  4779619 |  2577 | `	for(;;){` |
|        - |  2578 | `		/* Fetch the instruction to execute */` |
|  9558536 |  2579 | `		pInstr = &aInstr[pc];` |
|  9558536 |  2580 | `		rc = SXRET_OK;` |
|        - |  2581 | `/*` |
|        - |  2582 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2583 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2584 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2585 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2586 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2587 | ` */` |
|  9558536 |  2588 | `		switch(pInstr->iOp){` |
|        - |  2589 | `/*` |
|        - |  2590 | ` * DONE: P1 * *` |
|        - |  2591 | ` *` |
|        - |  2592 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2593 | ` * and return immediately.` |
|        - |  2594 | ` */` |
|    14932 |  2595 | `case PH7_OP_DONE:` |
|    29866 |  2596 | `	if( pInstr->iP1 ){` |
|        - |  2597 | `#ifdef UNTRUST` |
|        - |  2598 | `		if( pTos < pStack ){` |
|        - |  2599 | `			goto Abort;` |
|        - |  2600 | `		}` |
|        - |  2601 | `#endif` |
|    17218 |  2602 | `		if( pLastRef ){` |
|    11200 |  2603 | `			*pLastRef = pTos->nIdx;` |
|     5599 |  2604 | `		}` |
|    17218 |  2605 | `		if( pResult ){` |
|        - |  2606 | `			/* Execution result */` |
|    16412 |  2607 | `			PH7_MemObjStore(pTos,pResult);` |
|     8205 |  2608 | `		}` |
|    17218 |  2609 | `		VmPopOperand(&pTos,1);` |
|    21258 |  2610 | `	}else if( pLastRef ){` |
|        - |  2611 | `		/* Nothing referenced */` |
|      908 |  2612 | `		*pLastRef = SXU32_HIGH;` |
|      453 |  2613 | `	}` |
|    29866 |  2614 | `	goto Done;` |
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
|   206638 |  2662 | `case PH7_OP_JMP:` |
|   413322 |  2663 | `	pc = pInstr->iP2 - 1;` |
|   413322 |  2664 | `	break;` |
|        - |  2665 | `/*` |
|        - |  2666 | ` * JZ: P1 P2 *` |
|        - |  2667 | ` *` |
|        - |  2668 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2669 | ` * entry in the stack if P1 is zero.` |
|        - |  2670 | ` */` |
|   481243 |  2671 | `case PH7_OP_JZ:` |
|        - |  2672 | `#ifdef UNTRUST` |
|        - |  2673 | `	if( pTos < pStack ){` |
|        - |  2674 | `		goto Abort;` |
|        - |  2675 | `	}` |
|        - |  2676 | `#endif` |
|        - |  2677 | `	/* Get a boolean value */` |
|   962576 |  2678 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2679 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2680 | `	}` |
|   962576 |  2681 | `	if( !pTos->x.iVal ){` |
|        - |  2682 | `		/* Take the jump */` |
|   483154 |  2683 | `		pc = pInstr->iP2 - 1;` |
|   241576 |  2684 | `	}` |
|   962576 |  2685 | `	if( !pInstr->iP1 ){` |
|   767302 |  2686 | `		VmPopOperand(&pTos,1);` |
|   383672 |  2687 | `	}` |
|   962576 |  2688 | `	break;` |
|        - |  2689 | `/*` |
|        - |  2690 | ` * JNZ: P1 P2 *` |
|        - |  2691 | ` *` |
|        - |  2692 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2693 | ` * entry in the stack if P1 is zero.` |
|        - |  2694 | ` */` |
|    51988 |  2695 | `case PH7_OP_JNZ:` |
|        - |  2696 | `#ifdef UNTRUST` |
|        - |  2697 | `	if( pTos < pStack ){` |
|        - |  2698 | `		goto Abort;` |
|        - |  2699 | `	}` |
|        - |  2700 | `#endif` |
|        - |  2701 | `	/* Get a boolean value */` |
|   103978 |  2702 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2703 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2704 | `	}` |
|   103978 |  2705 | `	if( pTos->x.iVal ){` |
|        - |  2706 | `		/* Take the jump */` |
|     4258 |  2707 | `		pc = pInstr->iP2 - 1;` |
|     2128 |  2708 | `	}` |
|   103978 |  2709 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2710 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2711 | `	}` |
|   103978 |  2712 | `	break;` |
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
|   375565 |  2726 | `case PH7_OP_POP: {` |
|   751176 |  2727 | `	sxi32 n = pInstr->iP1;` |
|   751176 |  2728 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2729 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2730 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2731 | `	}` |
|   751176 |  2732 | `	VmPopOperand(&pTos,n);` |
|   751176 |  2733 | `	break;` |
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
|     6115 |  2756 | `case PH7_OP_NSSWITCH:` |
|    12232 |  2757 | `	SyBlobReset(&pVm->sNamespace);` |
|    12232 |  2758 | `	if( pInstr->p3 ){` |
|       49 |  2759 | `		const char *zNs = (const char *)pInstr->p3;` |
|       49 |  2760 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       24 |  2761 | `	}` |
|    12232 |  2762 | `	break;` |
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
|    12218 |  2894 | `case PH7_OP_ERR_CTRL:` |
|        - |  2895 | `	/*` |
|        - |  2896 | `	 * TICKET 1433-038:` |
|        - |  2897 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2898 | `	 * use the public API,to control error output.` |
|        - |  2899 | `	 */` |
|    24436 |  2900 | `	break;` |
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
|   793395 |  2960 | `case PH7_OP_LOADC: {` |
|        - |  2961 | `	ph7_value *pObj;` |
|        - |  2962 | `	/* Reserve a room */` |
|  1586836 |  2963 | `	pTos++;` |
|  2372452 |  2964 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1586836 |  2965 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2966 | `			SyHashEntry *pEntry;` |
|        - |  2967 | `			/* Candidate for expansion via user defined callbacks */` |
|    15650 |  2968 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15650 |  2969 | `			if( pEntry ){` |
|    15646 |  2970 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2971 | `				/* Set a NULL default value */` |
|    15646 |  2972 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15646 |  2973 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2974 | `				/* Invoke the callback and deal with the expanded value */` |
|    15646 |  2975 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2976 | `				/* Mark as constant */` |
|    15646 |  2977 | `				pTos->nIdx = SXU32_HIGH;` |
|    15646 |  2978 | `				break;` |
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
|  1571190 |  3010 | `		PH7_MemObjLoad(pObj,pTos);` |
|   785618 |  3011 | `	}else{` |
|        - |  3012 | `		/* Set a NULL value */` |
|      ! 0 |  3013 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3014 | `	}` |
|   785573 |  3015 | `LoadC_Done:` |
|        - |  3016 | `	/* Mark as constant */` |
|  1571192 |  3017 | `	pTos->nIdx = SXU32_HIGH;` |
|  1571192 |  3018 | `	break;` |
|        - |  3019 | `				  }` |
|        - |  3020 | `/*` |
|        - |  3021 | ` * LOAD: P1 * P3` |
|        - |  3022 | ` *` |
|        - |  3023 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3024 | ` * from the P3 operand.` |
|        - |  3025 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3026 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3027 | ` */` |
|  1302490 |  3028 | `case PH7_OP_LOAD:{` |
|        - |  3029 | `	ph7_value *pObj;` |
|        - |  3030 | `	SyString sName;` |
|  2605202 |  3031 | `	if( pInstr->p3 == 0 ){` |
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
|  2605184 |  3044 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3045 | `		/* Reserve a room for the target object */` |
|  2605184 |  3046 | `		pTos++;` |
|        - |  3047 | `	}` |
|        - |  3048 | `	/* Extract the requested memory object */` |
|  2605202 |  3049 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2605202 |  3050 | `	if( pObj == 0 ){` |
|      624 |  3051 | `		if( pInstr->iP1 ){` |
|        - |  3052 | `			/* Variable not found,load NULL */` |
|      624 |  3053 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3054 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3055 | `			}else{` |
|      624 |  3056 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3057 | `			}` |
|      624 |  3058 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1302803 |  3059 | `			break;` |
|      ! 0 |  3060 | `		}else{` |
|        - |  3061 | `			/* Fatal error */` |
|      ! 0 |  3062 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3063 | `			goto Abort;` |
|        - |  3064 | `		}` |
|        - |  3065 | `	}` |
|        - |  3066 | `	/* Load variable contents */` |
|  2604580 |  3067 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2604580 |  3068 | `	pTos->nIdx = pObj->nIdx;` |
|  2604580 |  3069 | `	break;` |
|        - |  3070 | `				   }` |
|        - |  3071 | `/*` |
|        - |  3072 | ` * LOAD_MAP P1 * *` |
|        - |  3073 | ` *` |
|        - |  3074 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3075 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3076 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3077 | ` */` |
|    17657 |  3078 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3079 | `	ph7_hashmap *pMap;` |
|        - |  3080 | `	/* Allocate a new hashmap instance */` |
|    35316 |  3081 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    35316 |  3082 | `	if( pMap == 0 ){` |
|      ! 0 |  3083 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3084 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3085 | `		goto Abort;` |
|        - |  3086 | `	}` |
|    35316 |  3087 | `	if( pInstr->iP1 > 0 ){` |
|     2152 |  3088 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3089 | `		/* Perform the insertion */` |
|     6548 |  3090 | `		while( pEntry < pTos ){` |
|     4398 |  3091 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3092 | `				/* Insertion by reference */` |
|      142 |  3093 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3094 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3095 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3096 | `					);` |
|       48 |  3097 | `			}else{` |
|        - |  3098 | `				/* Standard insertion */` |
|     6455 |  3099 | `				PH7_HashmapInsert(pMap,` |
|     4302 |  3100 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2151 |  3101 | `					&pEntry[1]` |
|        - |  3102 | `				);` |
|        - |  3103 | `			}` |
|        - |  3104 | `			/* Next pair on the stack */` |
|     4398 |  3105 | `			pEntry += 2;` |
|        2 |  3106 | `		}` |
|        - |  3107 | `		/* Pop P1 elements */` |
|     2152 |  3108 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1075 |  3109 | `	}` |
|        - |  3110 | `	/* Push the hashmap */` |
|    35316 |  3111 | `	pTos++;` |
|    35316 |  3112 | `	pTos->nIdx = SXU32_HIGH;` |
|    35316 |  3113 | `	pTos->x.pOther = pMap;` |
|    35316 |  3114 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    35316 |  3115 | `	break;` |
|        - |  3116 | `					  }` |
|        - |  3117 | `/*` |
|        - |  3118 | ` * LOAD_LIST: P1 * *` |
|        - |  3119 | ` *` |
|        - |  3120 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3121 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3122 | ` * Caveats:` |
|        - |  3123 | ` *  This implementation support only a single nesting level.` |
|        - |  3124 | ` */` |
|       17 |  3125 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3126 | `	ph7_value *pEntry;` |
|       35 |  3127 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3128 | `		/* Empty list,break immediately */` |
|      ! 0 |  3129 | `		break;` |
|        - |  3130 | `	}` |
|       35 |  3131 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3132 | `#ifdef UNTRUST` |
|        - |  3133 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3134 | `		goto Abort;` |
|        - |  3135 | `	}` |
|        - |  3136 | `#endif` |
|       35 |  3137 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3138 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3139 | `		ph7_hashmap_node *pNode;` |
|        - |  3140 | `		ph7_value sKey,*pObj;` |
|        - |  3141 | `		/* Start Copying */` |
|       31 |  3142 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3143 | `		while( pEntry <= pTos ){` |
|       69 |  3144 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3145 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3146 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3147 | `					if( rc == SXRET_OK ){` |
|        - |  3148 | `						/* Store node value */` |
|       65 |  3149 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3150 | `					}else{` |
|        - |  3151 | `						/* Nullify the variable */` |
|      ! 0 |  3152 | `						PH7_MemObjRelease(pObj);` |
|        - |  3153 | `					}` |
|       32 |  3154 | `				}` |
|       32 |  3155 | `			}` |
|       69 |  3156 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3157 | `			pEntry++;` |
|        1 |  3158 | `		}` |
|       15 |  3159 | `	}` |
|       35 |  3160 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3161 | `	break;` |
|        - |  3162 | `					   }` |
|        - |  3163 | `/*` |
|        - |  3164 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3165 | ` *` |
|        - |  3166 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3167 | ` * from the stack.` |
|        - |  3168 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3169 | ` * instead.` |
|        - |  3170 | ` */` |
|   210068 |  3171 | `case PH7_OP_LOAD_IDX: {` |
|   420182 |  3172 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   420182 |  3173 | `	ph7_hashmap *pMap = 0;` |
|        - |  3174 | `	ph7_value *pIdx;` |
|   420182 |  3175 | `	pIdx = 0;` |
|   420182 |  3176 | `	if( pInstr->iP1 == 0 ){` |
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
|   420182 |  3193 | `		pIdx = pTos;` |
|   420182 |  3194 | `		pTos--;` |
|        - |  3195 | `	}` |
|   420182 |  3196 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3197 | `		/* String access */` |
|   332628 |  3198 | `		if( pIdx ){` |
|        - |  3199 | `			sxu32 nOfft;` |
|   332628 |  3200 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3201 | `				/* Force an int cast */` |
|      ! 0 |  3202 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3203 | `			}` |
|   332628 |  3204 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   332628 |  3205 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3206 | `				/* Invalid offset,load null */` |
|      ! 0 |  3207 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3208 | `			}else{` |
|   332628 |  3209 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   332628 |  3210 | `				int c = zData[nOfft];` |
|   332628 |  3211 | `				PH7_MemObjRelease(pTos);` |
|   332628 |  3212 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   332628 |  3213 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3214 | `			}` |
|   166337 |  3215 | `		}else{` |
|        - |  3216 | `			/* No available index,load NULL */` |
|      ! 0 |  3217 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3218 | `		}` |
|   332628 |  3219 | `		break;` |
|        - |  3220 | `	}` |
|    87556 |  3221 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3222 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3223 | `			ph7_value *pObj;` |
|      ! 0 |  3224 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3225 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3226 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3227 | `			}` |
|      ! 0 |  3228 | `		}` |
|      ! 0 |  3229 | `	}` |
|    87556 |  3230 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    87556 |  3231 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3232 | `		/* Point to the hashmap */` |
|    87556 |  3233 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    87556 |  3234 | `		if( pIdx ){` |
|        - |  3235 | `			/* Load the desired entry */` |
|    87556 |  3236 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    43777 |  3237 | `		}` |
|    87556 |  3238 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3239 | `			/* Create a new empty entry */` |
|      ! 0 |  3240 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3241 | `			if( rc == SXRET_OK ){` |
|        - |  3242 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3243 | `				pNode = pMap->pLast;` |
|      ! 0 |  3244 | `			}` |
|      ! 0 |  3245 | `		}` |
|    43777 |  3246 | `	}` |
|    87556 |  3247 | `	if( pIdx ){` |
|    87556 |  3248 | `		PH7_MemObjRelease(pIdx);` |
|    43777 |  3249 | `	}` |
|    87556 |  3250 | `	if( rc == SXRET_OK ){` |
|        - |  3251 | `		/* Load entry contents */` |
|    39960 |  3252 | `		if( pMap->iRef < 2 ){` |
|        - |  3253 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3254 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3255 | `			 */` |
|       24 |  3256 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3257 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3258 | `		}else{` |
|    39938 |  3259 | `			pTos->nIdx = pNode->nValIdx;` |
|    39938 |  3260 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    39938 |  3261 | `			PH7_HashmapUnref(pMap);` |
|        - |  3262 | `		}` |
|    19981 |  3263 | `	}else{` |
|        - |  3264 | `		/* No such entry,load NULL */` |
|    47598 |  3265 | `		PH7_MemObjRelease(pTos);` |
|    47598 |  3266 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3267 | `	}` |
|    87556 |  3268 | `	break;` |
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
|   108734 |  3344 | `case PH7_OP_STORE: {` |
|        - |  3345 | `	ph7_value *pObj;` |
|        - |  3346 | `	SyString sName;` |
|        - |  3347 | `#ifdef UNTRUST` |
|        - |  3348 | `	if( pTos < pStack ){` |
|        - |  3349 | `		goto Abort;` |
|        - |  3350 | `	}` |
|        - |  3351 | `#endif` |
|   217470 |  3352 | `	if( pInstr->iP2 ){` |
|        - |  3353 | `		sxu32 nIdx;` |
|        - |  3354 | `		/* Member store operation */` |
|     2894 |  3355 | `		nIdx = pTos->nIdx;` |
|     2894 |  3356 | `		VmPopOperand(&pTos,1);` |
|     2894 |  3357 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3358 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3359 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3360 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3361 | `		}else{` |
|        - |  3362 | `			/* Point to the desired memory object */` |
|     2890 |  3363 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2890 |  3364 | `			if( pObj ){` |
|        - |  3365 | `				/* Perform the store operation */` |
|     2890 |  3366 | `				PH7_MemObjStore(pTos,pObj);` |
|     1444 |  3367 | `			}` |
|        - |  3368 | `		}` |
|   110182 |  3369 | `		break;` |
|   214578 |  3370 | `	}else if( pInstr->p3 == 0 ){` |
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
|   214572 |  3384 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3385 | `	}` |
|        - |  3386 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   214578 |  3387 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   214578 |  3388 | `	if( pObj == 0 ){` |
|      ! 0 |  3389 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3390 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3391 | `		goto Abort;` |
|        - |  3392 | `	}` |
|   214578 |  3393 | `	if( !pInstr->p3 ){` |
|        7 |  3394 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3395 | `	}` |
|        - |  3396 | `	/* Perform the store operation */` |
|   214578 |  3397 | `	PH7_MemObjStore(pTos,pObj);` |
|   214578 |  3398 | `	break;` |
|        - |  3399 | `				   }` |
|        - |  3400 | `/*` |
|        - |  3401 | ` * STORE_IDX:   P1 * P3` |
|        - |  3402 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3403 | ` *` |
|        - |  3404 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3405 | ` */` |
|    79251 |  3406 | `case PH7_OP_STORE_IDX:` |
|        - |  3407 | `case PH7_OP_STORE_IDX_REF: {` |
|   158504 |  3408 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3409 | `	ph7_value *pKey;` |
|        - |  3410 | `	sxu32 nIdx;` |
|   158504 |  3411 | `	if( pInstr->iP1 ){` |
|        - |  3412 | `		/* Key is next on stack */` |
|    56482 |  3413 | `		pKey = pTos;` |
|    56482 |  3414 | `		pTos--;` |
|    28242 |  3415 | `	}else{` |
|   102024 |  3416 | `		pKey = 0;` |
|        - |  3417 | `	}` |
|   158504 |  3418 | `	nIdx = pTos->nIdx;` |
|   158504 |  3419 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3420 | `		/* Hashmap already loaded */` |
|   158452 |  3421 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   158452 |  3422 | `		if( pMap->iRef < 2 ){` |
|        - |  3423 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3424 | `			pMap->iRef = 2;` |
|      ! 0 |  3425 | `		}` |
|    79227 |  3426 | `	}else{` |
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
|   158452 |  3480 | `	VmPopOperand(&pTos,1);` |
|        - |  3481 | `	/* Phase#2: Perform the insertion */` |
|   158452 |  3482 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3483 | `		/* Insertion by reference */` |
|       15 |  3484 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3485 | `	}else{` |
|   158438 |  3486 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3487 | `	}` |
|   158452 |  3488 | `	if( pKey ){` |
|    56432 |  3489 | `		PH7_MemObjRelease(pKey);` |
|    28215 |  3490 | `	}` |
|   158452 |  3491 | `	break;` |
|        - |  3492 | `					   }` |
|        - |  3493 | `/*` |
|        - |  3494 | ` * INCR: P1 * *` |
|        - |  3495 | ` *` |
|        - |  3496 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3497 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3498 | ` * the stack and increment after that.` |
|        - |  3499 | ` */` |
|   148407 |  3500 | `case PH7_OP_INCR:` |
|        - |  3501 | `#ifdef UNTRUST` |
|        - |  3502 | `	if( pTos < pStack ){` |
|        - |  3503 | `		goto Abort;` |
|        - |  3504 | `	}` |
|        - |  3505 | `#endif` |
|   296860 |  3506 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   296860 |  3507 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3508 | `			ph7_value *pObj;` |
|   296860 |  3509 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3510 | `				/* Force a numeric cast */` |
|   296860 |  3511 | `				PH7_MemObjToNumeric(pObj);` |
|   296860 |  3512 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3513 | `					pObj->rVal++;` |
|        - |  3514 | `					/* Try to get an integer representation */` |
|      ! 0 |  3515 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3516 | `				}else{` |
|   296860 |  3517 | `					pObj->x.iVal++;` |
|   296860 |  3518 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3519 | `				}` |
|   296860 |  3520 | `				if( pInstr->iP1 ){` |
|        - |  3521 | `					/* Pre-icrement */` |
|       71 |  3522 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3523 | `				}` |
|   148451 |  3524 | `			}` |
|   148453 |  3525 | `		}else{` |
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
|   148451 |  3540 | `	}` |
|   296860 |  3541 | `	break;` |
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
|    22821 |  3596 | `case PH7_OP_UMINUS:` |
|        - |  3597 | `#ifdef UNTRUST` |
|        - |  3598 | `	if( pTos < pStack ){` |
|        - |  3599 | `		goto Abort;` |
|        - |  3600 | `	}` |
|        - |  3601 | `#endif` |
|        - |  3602 | `	/* Force a numeric (integer,real or both) cast */` |
|    45644 |  3603 | `	PH7_MemObjToNumeric(pTos);` |
|    45644 |  3604 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3605 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3606 | `	}` |
|    45644 |  3607 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    45614 |  3608 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22806 |  3609 | `	}` |
|    45644 |  3610 | `	break;` |
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
|    39061 |  3637 | `case PH7_OP_LNOT:` |
|        - |  3638 | `#ifdef UNTRUST` |
|        - |  3639 | `	if( pTos < pStack ){` |
|        - |  3640 | `		goto Abort;` |
|        - |  3641 | `	}` |
|        - |  3642 | `#endif` |
|        - |  3643 | `	/* Force a boolean cast */` |
|    78168 |  3644 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3645 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3646 | `	}` |
|    78168 |  3647 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    78168 |  3648 | `	break;` |
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
|     1234 |  3673 | `case PH7_OP_MUL:` |
|        - |  3674 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3675 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3676 | `	/* Force the operand to be numeric */` |
|        - |  3677 | `#ifdef UNTRUST` |
|        - |  3678 | `	if( pNos < pStack ){` |
|        - |  3679 | `		goto Abort;` |
|        - |  3680 | `	}` |
|        - |  3681 | `#endif` |
|     2470 |  3682 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3683 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3684 | `	/* Perform the requested operation */` |
|     2470 |  3685 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
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
|     2454 |  3705 | `		a = pNos->x.iVal;` |
|     2454 |  3706 | `		b = pTos->x.iVal;` |
|     2454 |  3707 | `		r = a * b;` |
|        - |  3708 | `		/* Push the result */` |
|     2454 |  3709 | `		pNos->x.iVal = r;` |
|     2454 |  3710 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3711 | `	}` |
|     2470 |  3712 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3713 | `		ph7_value *pObj;` |
|       19 |  3714 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3715 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3716 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3717 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3718 | `		}` |
|        9 |  3719 | `	}` |
|     2470 |  3720 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3721 | `	break;` |
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
|      294 |  3775 | `case PH7_OP_SUB: {` |
|      589 |  3776 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3777 | `#ifdef UNTRUST` |
|        - |  3778 | `	if( pNos < pStack ){` |
|        - |  3779 | `		goto Abort;` |
|        - |  3780 | `	}` |
|        - |  3781 | `#endif` |
|      589 |  3782 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
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
|      495 |  3802 | `		a = pNos->x.iVal;` |
|      495 |  3803 | `		b = pTos->x.iVal;` |
|      495 |  3804 | `		r = a - b;` |
|        - |  3805 | `		/* Push the result */` |
|      495 |  3806 | `		pNos->x.iVal = r;` |
|      495 |  3807 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3808 | `	}` |
|      589 |  3809 | `	VmPopOperand(&pTos,1);` |
|      589 |  3810 | `	break;` |
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
|    60843 |  4251 | `case PH7_OP_CAT:{` |
|        - |  4252 | `	ph7_value *pNos,*pCur;` |
|   121688 |  4253 | `	if( pInstr->iP1 < 1 ){` |
|    94788 |  4254 | `		pNos = &pTos[-1];` |
|    47395 |  4255 | `	}else{` |
|    26902 |  4256 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4257 | `	}` |
|        - |  4258 | `#ifdef UNTRUST` |
|        - |  4259 | `	if( pNos < pStack ){` |
|        - |  4260 | `		goto Abort;` |
|        - |  4261 | `	}` |
|        - |  4262 | `#endif` |
|        - |  4263 | `	/* Force a string cast */` |
|   121688 |  4264 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      980 |  4265 | `		PH7_MemObjToString(pNos);` |
|      489 |  4266 | `	}` |
|   121688 |  4267 | `	pCur = &pNos[1];` |
|   245234 |  4268 | `	while( pCur <= pTos ){` |
|   123548 |  4269 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50468 |  4270 | `			PH7_MemObjToString(pCur);` |
|    25233 |  4271 | `		}` |
|        - |  4272 | `		/* Perform the concatenation */` |
|   123548 |  4273 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   123510 |  4274 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    61754 |  4275 | `		}` |
|   123548 |  4276 | `		SyBlobRelease(&pCur->sBlob);` |
|   123548 |  4277 | `		pCur++;` |
|        2 |  4278 | `	}` |
|   121688 |  4279 | `	pTos = pNos;` |
|   121688 |  4280 | `	break;` |
|        - |  4281 | `				}` |
|        - |  4282 | `/*  CAT_STORE: * * *` |
|        - |  4283 | ` *` |
|        - |  4284 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4285 | ` * back.` |
|        - |  4286 | ` */` |
|     3231 |  4287 | `case PH7_OP_CAT_STORE:{` |
|     6464 |  4288 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4289 | `	ph7_value *pObj;` |
|        - |  4290 | `#ifdef UNTRUST` |
|        - |  4291 | `	if( pNos < pStack ){` |
|        - |  4292 | `		goto Abort;` |
|        - |  4293 | `	}` |
|        - |  4294 | `#endif` |
|     6464 |  4295 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4296 | `		/* Force a string cast */` |
|      ! 0 |  4297 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4298 | `	}` |
|     6464 |  4299 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4300 | `		/* Force a string cast */` |
|      ! 0 |  4301 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4302 | `	}` |
|        - |  4303 | `	/* Perform the concatenation (Reverse order) */` |
|     6464 |  4304 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6464 |  4305 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3231 |  4306 | `	}` |
|        - |  4307 | `	/* Perform the store operation */` |
|     6464 |  4308 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4309 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6464 |  4310 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6464 |  4311 | `		PH7_MemObjStore(pTos,pObj);` |
|     3231 |  4312 | `	}` |
|     6464 |  4313 | `	PH7_MemObjStore(pTos,pNos);` |
|     6464 |  4314 | `	VmPopOperand(&pTos,1);` |
|     6464 |  4315 | `	break;` |
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
|    92204 |  4329 | `case PH7_OP_LAND:` |
|        - |  4330 | `case PH7_OP_LOR: {` |
|   184454 |  4331 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4332 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4333 | `#ifdef UNTRUST` |
|        - |  4334 | `	if( pNos < pStack ){` |
|        - |  4335 | `		goto Abort;` |
|        - |  4336 | `	}` |
|        - |  4337 | `#endif` |
|        - |  4338 | `	/* Force a boolean cast */` |
|   184454 |  4339 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4340 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4341 | `	}` |
|   184454 |  4342 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4343 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4344 | `	}` |
|   184454 |  4345 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   184454 |  4346 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   184454 |  4347 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4348 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    84734 |  4349 | `		v1 = and_logic[v1*3+v2];` |
|    42390 |  4350 | `	}else{` |
|        - |  4351 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|    99722 |  4352 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4353 | `	}` |
|   184454 |  4354 | `	if( v1 == 2 ){` |
|      ! 0 |  4355 | `		v1 = 1;` |
|      ! 0 |  4356 | `	}` |
|   184454 |  4357 | `	VmPopOperand(&pTos,1);` |
|   184454 |  4358 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   184454 |  4359 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   184454 |  4360 | `	break;` |
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
|     3785 |  4408 | `case PH7_OP_EQ:` |
|        - |  4409 | `case PH7_OP_NEQ: {` |
|     7572 |  4410 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4411 | `	/* Perform the comparison and act accordingly */` |
|        - |  4412 | `#ifdef UNTRUST` |
|        - |  4413 | `	if( pNos < pStack ){` |
|        - |  4414 | `		goto Abort;` |
|        - |  4415 | `	}` |
|        - |  4416 | `#endif` |
|     7572 |  4417 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7572 |  4418 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4419 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7563 |  4420 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7528 |  4421 | `		rc = rc == 0;` |
|     3765 |  4422 | `	}else{` |
|       28 |  4423 | `		rc = rc != 0;` |
|        - |  4424 | `	}` |
|     7572 |  4425 | `	VmPopOperand(&pTos,1);` |
|     7572 |  4426 | `	if( !pInstr->iP2 ){` |
|        - |  4427 | `		/* Push comparison result without taking the jump */` |
|     7572 |  4428 | `		PH7_MemObjRelease(pTos);` |
|     7572 |  4429 | `		pTos->x.iVal = rc;` |
|        - |  4430 | `		/* Invalidate any prior representation */` |
|     7572 |  4431 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3787 |  4432 | `	}else{` |
|      ! 0 |  4433 | `		if( rc ){` |
|        - |  4434 | `			/* Jump to the desired location */` |
|      ! 0 |  4435 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4436 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4437 | `		}` |
|        - |  4438 | `	}` |
|     7572 |  4439 | `	break;` |
|        - |  4440 | `				 }` |
|        - |  4441 | `/* OP_TEQ P1 P2 *` |
|        - |  4442 | ` *` |
|        - |  4443 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4444 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4445 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4446 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4447 | ` */` |
|   126555 |  4448 | `case PH7_OP_TEQ: {` |
|   253112 |  4449 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4450 | `	/* Perform the comparison and act accordingly */` |
|        - |  4451 | `#ifdef UNTRUST` |
|        - |  4452 | `	if( pNos < pStack ){` |
|        - |  4453 | `		goto Abort;` |
|        - |  4454 | `	}` |
|        - |  4455 | `#endif` |
|   253112 |  4456 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   253112 |  4457 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4458 | `		rc = 0;` |
|        2 |  4459 | `	}else{` |
|   253110 |  4460 | `		rc = rc == 0;` |
|        - |  4461 | `	}` |
|   253112 |  4462 | `	VmPopOperand(&pTos,1);` |
|   253112 |  4463 | `	if( !pInstr->iP2 ){` |
|        - |  4464 | `		/* Push comparison result without taking the jump */` |
|   253112 |  4465 | `		PH7_MemObjRelease(pTos);` |
|   253112 |  4466 | `		pTos->x.iVal = rc;` |
|        - |  4467 | `		/* Invalidate any prior representation */` |
|   253112 |  4468 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   126557 |  4469 | `	}else{` |
|      ! 0 |  4470 | `		if( rc ){` |
|        - |  4471 | `			/* Jump to the desired location */` |
|      ! 0 |  4472 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4473 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4474 | `		}` |
|        - |  4475 | `	}` |
|   253112 |  4476 | `	break;` |
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
|    98883 |  4487 | `case PH7_OP_TNE: {` |
|   197768 |  4488 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4489 | `	/* Perform the comparison and act accordingly */` |
|        - |  4490 | `#ifdef UNTRUST` |
|        - |  4491 | `	if( pNos < pStack ){` |
|        - |  4492 | `		goto Abort;` |
|        - |  4493 | `	}` |
|        - |  4494 | `#endif` |
|   197768 |  4495 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   197768 |  4496 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4497 | `		rc = 1;` |
|        2 |  4498 | `	}else{` |
|   197766 |  4499 | `		rc = rc != 0;` |
|        - |  4500 | `	}` |
|   197768 |  4501 | `	VmPopOperand(&pTos,1);` |
|   197768 |  4502 | `	if( !pInstr->iP2 ){` |
|        - |  4503 | `		/* Push comparison result without taking the jump */` |
|   197768 |  4504 | `		PH7_MemObjRelease(pTos);` |
|   197768 |  4505 | `		pTos->x.iVal = rc;` |
|        - |  4506 | `		/* Invalidate any prior representation */` |
|   197768 |  4507 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    98885 |  4508 | `	}else{` |
|      ! 0 |  4509 | `		if( rc ){` |
|        - |  4510 | `			/* Jump to the desired location */` |
|      ! 0 |  4511 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4512 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4513 | `		}` |
|        - |  4514 | `	}` |
|   197768 |  4515 | `	break;` |
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
|   100881 |  4535 | `case PH7_OP_LT:` |
|        - |  4536 | `case PH7_OP_LE: {` |
|   201808 |  4537 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4538 | `	/* Perform the comparison and act accordingly */` |
|        - |  4539 | `#ifdef UNTRUST` |
|        - |  4540 | `	if( pNos < pStack ){` |
|        - |  4541 | `		goto Abort;` |
|        - |  4542 | `	}` |
|        - |  4543 | `#endif` |
|   201808 |  4544 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   201808 |  4545 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4546 | `		rc = 0;` |
|   201804 |  4547 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4548 | `		rc = rc < 1;` |
|      198 |  4549 | `	}else{` |
|   201406 |  4550 | `		rc = rc < 0;` |
|        - |  4551 | `	}` |
|   201808 |  4552 | `	VmPopOperand(&pTos,1);` |
|   201808 |  4553 | `	if( !pInstr->iP2 ){` |
|        - |  4554 | `		/* Push comparison result without taking the jump */` |
|   201808 |  4555 | `		PH7_MemObjRelease(pTos);` |
|   201808 |  4556 | `		pTos->x.iVal = rc;` |
|        - |  4557 | `		/* Invalidate any prior representation */` |
|   201808 |  4558 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100927 |  4559 | `	}else{` |
|      ! 0 |  4560 | `		if( rc ){` |
|        - |  4561 | `			/* Jump to the desired location */` |
|      ! 0 |  4562 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4563 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4564 | `		}` |
|        - |  4565 | `	}` |
|   201808 |  4566 | `	break;` |
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
|    47483 |  4586 | `case PH7_OP_GT:` |
|        - |  4587 | `case PH7_OP_GE: {` |
|    94968 |  4588 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4589 | `	/* Perform the comparison and act accordingly */` |
|        - |  4590 | `#ifdef UNTRUST` |
|        - |  4591 | `	if( pNos < pStack ){` |
|        - |  4592 | `		goto Abort;` |
|        - |  4593 | `	}` |
|        - |  4594 | `#endif` |
|    94968 |  4595 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    94968 |  4596 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4597 | `		rc = 0;` |
|    94964 |  4598 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    94812 |  4599 | `		rc = rc >= 0;` |
|    47407 |  4600 | `	}else{` |
|      150 |  4601 | `		rc = rc > 0;` |
|        - |  4602 | `	}` |
|    94968 |  4603 | `	VmPopOperand(&pTos,1);` |
|    94968 |  4604 | `	if( !pInstr->iP2 ){` |
|        - |  4605 | `		/* Push comparison result without taking the jump */` |
|    94968 |  4606 | `		PH7_MemObjRelease(pTos);` |
|    94968 |  4607 | `		pTos->x.iVal = rc;` |
|        - |  4608 | `		/* Invalidate any prior representation */` |
|    94968 |  4609 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    47485 |  4610 | `	}else{` |
|      ! 0 |  4611 | `		if( rc ){` |
|        - |  4612 | `			/* Jump to the desired location */` |
|      ! 0 |  4613 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4614 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4615 | `		}` |
|        - |  4616 | `	}` |
|    94968 |  4617 | `	break;` |
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
|     4739 |  4920 | `case PH7_OP_FOREACH_INIT: {` |
|     9480 |  4921 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4922 | `	void *pName;` |
|        - |  4923 | `#ifdef UNTRUST` |
|        - |  4924 | `	if( pTos < pStack ){` |
|        - |  4925 | `		goto Abort;` |
|        - |  4926 | `	}` |
|        - |  4927 | `#endif` |
|     9480 |  4928 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
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
|     9480 |  4941 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
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
|     9480 |  4954 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4955 | `		/* Jump out of the loop */` |
|      ! 0 |  4956 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4957 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4958 | `		}` |
|      ! 0 |  4959 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4960 | `	}else{` |
|        - |  4961 | `		ph7_foreach_step *pStep;` |
|     9480 |  4962 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9480 |  4963 | `		if( pStep == 0 ){` |
|      ! 0 |  4964 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4965 | `			/* Jump out of the loop */` |
|      ! 0 |  4966 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4967 | `		}else{` |
|        - |  4968 | `			/* Zero the structure */` |
|     9480 |  4969 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4970 | `			/* Prepare the step */` |
|     9480 |  4971 | `			pStep->iFlags = pInfo->iFlags;` |
|     9480 |  4972 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9472 |  4973 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4974 | `				/* Reset the internal loop cursor */` |
|     9472 |  4975 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4976 | `				/* Mark the step */` |
|     9472 |  4977 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9472 |  4978 | `				pStep->xIter.pMap = pMap;` |
|     9472 |  4979 | `				pMap->iRef++;` |
|     4737 |  4980 | `			}else{` |
|        9 |  4981 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4982 | `				/* Reset the loop cursor */` |
|        9 |  4983 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4984 | `				/* Mark the step */` |
|        9 |  4985 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4986 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4987 | `				pThis->iRef++;` |
|        - |  4988 | `			}` |
|        - |  4989 | `		}` |
|     9480 |  4990 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4991 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4992 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4993 | `			/* Jump out of the loop */` |
|      ! 0 |  4994 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4995 | `		}` |
|        - |  4996 | `	}` |
|     9480 |  4997 | `	VmPopOperand(&pTos,1);` |
|     9480 |  4998 | `	break;` |
|        - |  4999 | `						  }` |
|        - |  5000 | `/*` |
|        - |  5001 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5002 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5003 | ` */` |
|    76092 |  5004 | `case PH7_OP_FOREACH_STEP: {` |
|   152186 |  5005 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5006 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5007 | `	ph7_value *pValue;` |
|        - |  5008 | `	VmFrame *pFrameLocal;` |
|        - |  5009 | `	/* Peek the last step */` |
|   152186 |  5010 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   152186 |  5011 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   152186 |  5012 | `	pFrameLocal = pVm->pFrame;` |
|   152186 |  5013 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5014 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  5015 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5016 | `	}` |
|   152186 |  5017 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   152162 |  5018 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5019 | `		ph7_hashmap_node *pNode;` |
|        - |  5020 | `		/* Extract the current node value */` |
|   152162 |  5021 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   152162 |  5022 | `		if( pNode == 0 ){` |
|        - |  5023 | `			/* No more entry to process */` |
|     9472 |  5024 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9472 |  5025 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5026 | `				/* Break the reference with the last element */` |
|        5 |  5027 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5028 | `			}` |
|        - |  5029 | `			/* Automatically reset the loop cursor */` |
|     9472 |  5030 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5031 | `			/* Cleanup the mess left behind */` |
|     9472 |  5032 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9472 |  5033 | `			SySetPop(&pInfo->aStep);` |
|     9472 |  5034 | `			PH7_HashmapUnref(pMap);` |
|     4737 |  5035 | `		}else{` |
|   142692 |  5036 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      412 |  5037 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      412 |  5038 | `				if( pKey ){` |
|      412 |  5039 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      205 |  5040 | `				}` |
|      205 |  5041 | `			}` |
|   142692 |  5042 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5043 | `				SyHashEntry *pEntry;` |
|        - |  5044 | `				/* Pass by reference */` |
|       13 |  5045 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5046 | `				if( pEntry ){` |
|       13 |  5047 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5048 | `				}else{` |
|      ! 0 |  5049 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5050 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5051 | `				}` |
|        7 |  5052 | `			}else{` |
|        - |  5053 | `				/* Make a copy of the entry value */` |
|   142680 |  5054 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   142680 |  5055 | `				if( pValue ){` |
|   142680 |  5056 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    71339 |  5057 | `				}` |
|        - |  5058 | `			}` |
|        - |  5059 | `		}` |
|    76082 |  5060 | `	}else{` |
|       25 |  5061 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5062 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5063 | `		SyHashEntry *pEntry;` |
|        - |  5064 | `		/* Point to the next attribute */` |
|       29 |  5065 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5066 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5067 | `			/* Check access permission */` |
|       31 |  5068 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5069 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5070 | `					break; /* Access is granted */` |
|        - |  5071 | `			}` |
|        1 |  5072 | `		}` |
|       25 |  5073 | `		if( pEntry == 0 ){` |
|        - |  5074 | `			/* Clean up the mess left behind */` |
|        9 |  5075 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5076 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5077 | `				/* Break the reference with the last element */` |
|        3 |  5078 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5079 | `			}` |
|        9 |  5080 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5081 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5082 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5083 | `		}else{` |
|       17 |  5084 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5085 | `			ph7_value *pAttrValue;` |
|       17 |  5086 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5087 | `				/* Fill with the current attribute name */` |
|       17 |  5088 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5089 | `				if( pKey ){` |
|       17 |  5090 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5091 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5092 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5093 | `				}` |
|        8 |  5094 | `			}` |
|        - |  5095 | `			/* Extract attribute value */` |
|       17 |  5096 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5097 | `			if( pAttrValue ){` |
|       17 |  5098 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5099 | `					/* Pass by reference */` |
|        3 |  5100 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5101 | `					if( pEntry ){` |
|        3 |  5102 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5103 | `					}else{` |
|      ! 0 |  5104 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5105 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5106 | `					}` |
|        2 |  5107 | `				}else{` |
|        - |  5108 | `					/* Make a copy of the attribute value */` |
|       15 |  5109 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5110 | `					if( pValue ){` |
|       15 |  5111 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5112 | `					}` |
|        - |  5113 | `				}` |
|        8 |  5114 | `			}` |
|        - |  5115 | `		}` |
|        - |  5116 | `	}` |
|   152186 |  5117 | `	break;` |
|        - |  5118 | `						  }` |
|        - |  5119 | `/*` |
|        - |  5120 | ` * OP_MEMBER P1 P2` |
|        - |  5121 | ` * Load class attribute/method on the stack.` |
|        - |  5122 | ` */` |
|     1966 |  5123 | `case PH7_OP_MEMBER: {` |
|        - |  5124 | `	ph7_class_instance *pThis;` |
|        - |  5125 | `	ph7_value *pNos;` |
|        - |  5126 | `	SyString sName;` |
|     3934 |  5127 | `	if( !pInstr->iP1 ){` |
|     3840 |  5128 | `		pNos = &pTos[-1];` |
|        - |  5129 | `#ifdef UNTRUST` |
|        - |  5130 | `		if( pNos < pStack ){` |
|        - |  5131 | `			goto Abort;` |
|        - |  5132 | `		}` |
|        - |  5133 | `#endif` |
|     3840 |  5134 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5135 | `			ph7_class *pClass;` |
|        - |  5136 | `			/* Class already instantiated */` |
|     3840 |  5137 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5138 | `			/* Point to the instantiated class */` |
|     3840 |  5139 | `			pClass = pThis->pClass;` |
|        - |  5140 | `			/* Extract attribute name first */` |
|     3840 |  5141 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     3840 |  5142 | `			if( pInstr->iP2 ){` |
|        - |  5143 | `				/* Method call */` |
|      260 |  5144 | `				ph7_class_method *pMeth = 0;` |
|      260 |  5145 | `				if( sName.nByte > 0 ){` |
|        - |  5146 | `					/* Extract the target method */` |
|      260 |  5147 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      129 |  5148 | `				}` |
|      260 |  5149 | `				if( pMeth == 0 ){` |
|      ! 0 |  5150 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5151 | `						&pClass->sName,&sName` |
|        - |  5152 | `						);` |
|        - |  5153 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5154 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5155 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5156 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5157 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5158 | `				}else{` |
|        - |  5159 | `					/* Push method name on the stack */` |
|      260 |  5160 | `					PH7_MemObjRelease(pTos);` |
|      260 |  5161 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      260 |  5162 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5163 | `				}` |
|      260 |  5164 | `				pTos->nIdx = SXU32_HIGH;` |
|      131 |  5165 | `			}else{` |
|        - |  5166 | `				/* Attribute access */` |
|     3582 |  5167 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5168 | `				SyHashEntry *pEntry;` |
|        - |  5169 | `				/* Extract the target attribute */` |
|     3582 |  5170 | `				if( sName.nByte > 0 ){` |
|     3582 |  5171 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3582 |  5172 | `					if( pEntry ){` |
|        - |  5173 | `						/* Point to the attribute value */` |
|     3580 |  5174 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1789 |  5175 | `					}` |
|     1790 |  5176 | `				}` |
|     3582 |  5177 | `				if( pObjAttr == 0 ){` |
|        - |  5178 | `					/* No such attribute,load null */` |
|        4 |  5179 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5180 | `						&pClass->sName,&sName);` |
|        - |  5181 | `					/* Call the __get magic method if available */` |
|        3 |  5182 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5183 | `				}` |
|     3582 |  5184 | `				VmPopOperand(&pTos,1);` |
|        - |  5185 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5186 | `				 * This is due to the following case:` |
|        - |  5187 | `				 *     (new TestClass())->foo;` |
|        - |  5188 | `				 */` |
|     3582 |  5189 | `				pThis->iRef++;` |
|     3582 |  5190 | `				PH7_MemObjRelease(pTos);` |
|     3582 |  5191 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3582 |  5192 | `				if( pObjAttr ){` |
|     3580 |  5193 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5194 | `					/* Check attribute access */` |
|     3580 |  5195 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5196 | `						/* Load attribute */` |
|     3580 |  5197 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3580 |  5198 | `						if( pValue ){` |
|     3580 |  5199 | `							if( pThis->iRef < 2 ){` |
|        - |  5200 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5201 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5202 | `								 */` |
|        3 |  5203 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5204 | `							}else{` |
|        - |  5205 | `								/* Simple load */` |
|     3578 |  5206 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5207 | `							}` |
|     3580 |  5208 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3578 |  5209 | `								if( pThis->iRef > 1 ){` |
|        - |  5210 | `									/* Load attribute index */` |
|     3576 |  5211 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1787 |  5212 | `								}` |
|     1788 |  5213 | `							}` |
|     1789 |  5214 | `						}` |
|     1789 |  5215 | `					}` |
|     1789 |  5216 | `				}` |
|        - |  5217 | `				/* Safely unreference the object */` |
|     3582 |  5218 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5219 | `			}` |
|     1921 |  5220 | `		}else{` |
|      ! 0 |  5221 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5222 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5223 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5224 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5225 | `		}` |
|     1921 |  5226 | `	}else{` |
|        - |  5227 | `		/* Static member access using class name */` |
|       96 |  5228 | `		pNos = pTos;` |
|       96 |  5229 | `		pThis = 0;` |
|       96 |  5230 | `		if( !pInstr->p3 ){` |
|       84 |  5231 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       84 |  5232 | `			pNos--;` |
|        - |  5233 | `#ifdef UNTRUST` |
|        - |  5234 | `			if( pNos < pStack ){` |
|        - |  5235 | `				goto Abort;` |
|        - |  5236 | `			}` |
|        - |  5237 | `#endif` |
|       43 |  5238 | `		}else{` |
|        - |  5239 | `			/* Attribute name already computed */` |
|       14 |  5240 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5241 | `		}` |
|       96 |  5242 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       96 |  5243 | `			ph7_class *pClass = 0;` |
|       96 |  5244 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5245 | `				/* Class already instantiated */` |
|      ! 0 |  5246 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5247 | `				pClass = pThis->pClass;` |
|      ! 0 |  5248 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5249 | `			}else{` |
|        - |  5250 | `				/* Try to extract the target class */` |
|       96 |  5251 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       96 |  5252 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|       96 |  5253 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5254 | `					/* Handle self/static/parent keywords */` |
|       96 |  5255 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       26 |  5256 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       84 |  5257 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5258 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       71 |  5259 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5260 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5261 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5262 | `							pClass = pSelf->pBase;` |
|        6 |  5263 | `						}` |
|        8 |  5264 | `					}else{` |
|       46 |  5265 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5266 | `					}` |
|       47 |  5267 | `				}` |
|        - |  5268 | `			}` |
|       96 |  5269 | `			if( pClass == 0 ){` |
|        - |  5270 | `				/* Undefined class */` |
|      ! 0 |  5271 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5272 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5273 | `					);` |
|      ! 0 |  5274 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5275 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5276 | `				}` |
|      ! 0 |  5277 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5278 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5279 | `			}else{` |
|       96 |  5280 | `				if( pInstr->iP2 ){` |
|        - |  5281 | `					/* Method call */` |
|       30 |  5282 | `					ph7_class_method *pMeth = 0;` |
|       30 |  5283 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5284 | `						/* Extract the target method */` |
|       30 |  5285 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       14 |  5286 | `					}` |
|       30 |  5287 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5288 | `						if( pMeth ){` |
|      ! 0 |  5289 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5290 | `								&pClass->sName,&sName` |
|        - |  5291 | `								);` |
|      ! 0 |  5292 | `						}else{` |
|      ! 0 |  5293 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5294 | `								&pClass->sName,&sName` |
|        - |  5295 | `								);` |
|        - |  5296 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5297 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5298 | `						}` |
|        - |  5299 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5300 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5301 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5302 | `						}` |
|      ! 0 |  5303 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5304 | `					}else{` |
|        - |  5305 | `						/* Push method name on the stack */` |
|       30 |  5306 | `						PH7_MemObjRelease(pTos);` |
|       30 |  5307 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       30 |  5308 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5309 | `					}` |
|       30 |  5310 | `					pTos->nIdx = SXU32_HIGH;` |
|       16 |  5311 | `				}else{` |
|        - |  5312 | `					/* Attribute access */` |
|       68 |  5313 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5314 | `					/* Check for special ::class pseudo-constant */` |
|       98 |  5315 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       60 |  5316 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5317 | `						/* ::class returns the fully qualified class name */` |
|        - |  5318 | `						/* Pop the attribute name from the stack */` |
|       50 |  5319 | `						if( !pInstr->p3 ){` |
|       50 |  5320 | `							VmPopOperand(&pTos,1);` |
|       24 |  5321 | `						}` |
|       50 |  5322 | `						PH7_MemObjRelease(pTos);` |
|        - |  5323 | `						/* Load the class name */` |
|       50 |  5324 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       50 |  5325 | `						pTos->nIdx = SXU32_HIGH;` |
|       26 |  5326 | `					}else{` |
|        - |  5327 | `						/* Extract the target attribute */` |
|       20 |  5328 | `						if( sName.nByte > 0 ){` |
|       20 |  5329 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5330 | `						}` |
|       20 |  5331 | `						if( pAttr == 0 ){` |
|        - |  5332 | `							/* No such attribute,load null */` |
|      ! 0 |  5333 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5334 | `								&pClass->sName,&sName);` |
|        - |  5335 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5336 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5337 | `						}` |
|        - |  5338 | `						/* Pop the attribute name from the stack */` |
|       20 |  5339 | `						if( !pInstr->p3 ){` |
|        7 |  5340 | `							VmPopOperand(&pTos,1);` |
|        3 |  5341 | `						}` |
|       20 |  5342 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5343 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5344 | `						if( pAttr ){` |
|       20 |  5345 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5346 | `								/* Access to a non static attribute */` |
|      ! 0 |  5347 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5348 | `									&pClass->sName,&pAttr->sName` |
|        - |  5349 | `									);` |
|      ! 0 |  5350 | `							}else{` |
|        - |  5351 | `								ph7_value *pValue;` |
|        - |  5352 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5353 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5354 | `									/* Load the desired attribute */` |
|       20 |  5355 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5356 | `									if( pValue ){` |
|       20 |  5357 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5358 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5359 | `											/* Load index number */` |
|       14 |  5360 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5361 | `										}` |
|        9 |  5362 | `									}` |
|        9 |  5363 | `								}` |
|        - |  5364 | `							}` |
|        9 |  5365 | `						}` |
|        - |  5366 | `					}` |
|        - |  5367 | `				}` |
|       96 |  5368 | `				if( pThis ){` |
|        - |  5369 | `					/* Safely unreference the object */` |
|      ! 0 |  5370 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5371 | `				}` |
|        - |  5372 | `			}` |
|       49 |  5373 | `		}else{` |
|        - |  5374 | `			/* Pop operands */` |
|      ! 0 |  5375 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5376 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5377 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5378 | `			}` |
|      ! 0 |  5379 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5380 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5381 | `		}` |
|        - |  5382 | `	}` |
|     3934 |  5383 | `	break;` |
|        - |  5384 | `					}` |
|        - |  5385 | `/*` |
|        - |  5386 | ` * OP_NEW P1 * * *` |
|        - |  5387 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5388 | ` */` |
|      294 |  5389 | `case PH7_OP_NEW: {` |
|      590 |  5390 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      590 |  5391 | `	ph7_class *pClass = 0;` |
|        - |  5392 | `	ph7_class_instance *pNew;` |
|      590 |  5393 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5394 | `		/* Try to extract the desired class */` |
|      884 |  5395 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      588 |  5396 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      294 |  5397 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5398 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5399 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5400 | `	}` |
|      590 |  5401 | `	if( pClass == 0 ){` |
|        - |  5402 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5403 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5404 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5405 | `			);` |
|        - |  5406 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5407 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5408 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5409 | `			/* Pop given arguments */` |
|      ! 0 |  5410 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5411 | `		}` |
|      ! 0 |  5412 | `		goto Abort;` |
|      ! 0 |  5413 | `	}else{` |
|        - |  5414 | `		ph7_class_method *pCons;` |
|        - |  5415 | `		/* Create a new class instance */` |
|      590 |  5416 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      590 |  5417 | `		if( pNew == 0 ){` |
|      ! 0 |  5418 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5419 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5420 | `				&pClass->sName` |
|        - |  5421 | `			);` |
|      ! 0 |  5422 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5423 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5424 | `				/* Pop given arguments */` |
|      ! 0 |  5425 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5426 | `			}` |
|      ! 0 |  5427 | `			break;` |
|        - |  5428 | `		}` |
|        - |  5429 | `		/* Check if a constructor is available */` |
|      590 |  5430 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      590 |  5431 | `		if( pCons == 0 ){` |
|      518 |  5432 | `			SyString *pName = &pClass->sName;` |
|        - |  5433 | `			/* Check for a constructor with the same base class name */` |
|      518 |  5434 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      258 |  5435 | `		}` |
|      590 |  5436 | `		if( pCons ){` |
|        - |  5437 | `			/* Call the class constructor */` |
|       74 |  5438 | `			SySetReset(&aArg);` |
|      136 |  5439 | `			while( pArg < pTos ){` |
|       64 |  5440 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       64 |  5441 | `				pArg++;` |
|        2 |  5442 | `			}` |
|       74 |  5443 | `			if( pVm->bErrReport ){` |
|        - |  5444 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5445 | `				sxu32 n;` |
|       31 |  5446 | `				n = SySetUsed(&aArg);` |
|        - |  5447 | `				/* Emit a notice for missing arguments */` |
|       79 |  5448 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  5449 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  5450 | `					if( pFuncArg ){` |
|       49 |  5451 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5452 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5453 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5454 | `						}` |
|       24 |  5455 | `					}` |
|       49 |  5456 | `					n++;` |
|        1 |  5457 | `				}` |
|       15 |  5458 | `			}` |
|       74 |  5459 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5460 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       74 |  5461 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5462 | `				pNew->iRef = 1;` |
|      ! 0 |  5463 | `			}` |
|       36 |  5464 | `		}` |
|      590 |  5465 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5466 | `			/* Pop given arguments */` |
|       58 |  5467 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       28 |  5468 | `		}` |
|      590 |  5469 | `		PH7_MemObjRelease(pTos);` |
|      590 |  5470 | `		pTos->x.pOther = pNew;` |
|      590 |  5471 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5472 | `	}` |
|      590 |  5473 | `	break;` |
|        - |  5474 | `				 }` |
|        - |  5475 | `/*` |
|        - |  5476 | ` * OP_CLONE * * *` |
|        - |  5477 | ` * Perfome a clone operation.` |
|        - |  5478 | ` */` |
|       23 |  5479 | `case PH7_OP_CLONE: {` |
|        - |  5480 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5481 | `#ifdef UNTRUST` |
|        - |  5482 | `	if( pTos < pStack ){` |
|        - |  5483 | `		goto Abort;` |
|        - |  5484 | `	}` |
|        - |  5485 | `#endif` |
|        - |  5486 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5487 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5488 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5489 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5490 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5491 | `		break;` |
|        - |  5492 | `	}` |
|        - |  5493 | `	/* Point to the source */` |
|       44 |  5494 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5495 | `	/* Perform the clone operation */` |
|       44 |  5496 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5497 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5498 | `	if( pClone == 0 ){` |
|      ! 0 |  5499 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5500 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5501 | `	}else{` |
|        - |  5502 | `		/* Load the cloned object */` |
|       44 |  5503 | `		pTos->x.pOther = pClone;` |
|       44 |  5504 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5505 | `	}` |
|       44 |  5506 | `	break;` |
|        - |  5507 | `				   }` |
|        - |  5508 | `/*` |
|        - |  5509 | ` * OP_SWITCH * * P3` |
|        - |  5510 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5511 | ` */` |
|       18 |  5512 | `case PH7_OP_SWITCH: {` |
|       38 |  5513 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5514 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5515 | `	ph7_value sValue,sCaseValue;` |
|        - |  5516 | `	sxu32 n,nEntry;` |
|        - |  5517 | `#ifdef UNTRUST` |
|        - |  5518 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5519 | `		goto Abort;` |
|        - |  5520 | `	}` |
|        - |  5521 | `#endif` |
|        - |  5522 | `	/* Point to the case table  */` |
|       38 |  5523 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5524 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5525 | `	/* Select the appropriate case block to execute */` |
|       38 |  5526 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5527 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5528 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5529 | `		pCase = &aCase[n];` |
|       92 |  5530 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5531 | `		/* Execute the case expression first */` |
|       92 |  5532 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5533 | `		/* Compare the two expression */` |
|       92 |  5534 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5535 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5536 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5537 | `		if( rc == 0 ){` |
|        - |  5538 | `			/* Value match,jump to this block */` |
|       38 |  5539 | `			pc = pCase->nStart - 1;` |
|       38 |  5540 | `			break;` |
|        - |  5541 | `		}` |
|       29 |  5542 | `	}` |
|       38 |  5543 | `	VmPopOperand(&pTos,1);` |
|       38 |  5544 | `	if( n >= nEntry ){` |
|        - |  5545 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5546 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5547 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5548 | `		}else{` |
|        - |  5549 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5550 | `			pc = pSwitch->nOut - 1;` |
|        - |  5551 | `		}` |
|      ! 0 |  5552 | `	}` |
|       38 |  5553 | `	break;` |
|        - |  5554 | `					}` |
|        - |  5555 | `/*` |
|        - |  5556 | ` * OP_CALL P1 * *` |
|        - |  5557 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5558 | ` *  function on the stack.` |
|        - |  5559 | ` */` |
|   277225 |  5560 | `case PH7_OP_CALL: {` |
|   554496 |  5561 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5562 | `	SyHashEntry *pEntry;` |
|        - |  5563 | `	SyString sName;` |
|        - |  5564 | `	/* Extract function name */` |
|   554496 |  5565 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5566 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5567 | `			ph7_value sResult;` |
|      ! 0 |  5568 | `			SySetReset(&aArg);` |
|      ! 0 |  5569 | `			while( pArg < pTos ){` |
|      ! 0 |  5570 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5571 | `				pArg++;` |
|      ! 0 |  5572 | `			}` |
|      ! 0 |  5573 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5574 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5575 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5576 | `			SySetReset(&aArg);` |
|        - |  5577 | `			/* Pop given arguments */` |
|      ! 0 |  5578 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5579 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5580 | `			}` |
|        - |  5581 | `			/* Copy result */` |
|      ! 0 |  5582 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5583 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5584 | `		}else{` |
|        3 |  5585 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5586 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5587 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5588 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5589 | `			}else{` |
|        - |  5590 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5591 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5592 | `			}` |
|        - |  5593 | `			/* Pop given arguments */` |
|        3 |  5594 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5595 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5596 | `			}` |
|        - |  5597 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5598 | `			PH7_MemObjRelease(pTos);` |
|        - |  5599 | `		}` |
|   276992 |  5600 | `		break;` |
|        - |  5601 | `	}` |
|   554494 |  5602 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5603 | `	/* Check for a compiled function first.` |
|        - |  5604 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5605 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   554494 |  5606 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5607 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5608 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5609 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5610 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5611 | `	 * function calls inside namespaces. */` |
|   554494 |  5612 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5613 | `		const char *zFunc;` |
|        - |  5614 | `		const char *zEnd;` |
|        - |  5615 | `		const char *z;` |
|        - |  5616 | `		SyString sGlobal;` |
|       15 |  5617 | `		zFunc = sName.zString;` |
|       15 |  5618 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5619 | `		z = zEnd;` |
|        - |  5620 | `		/* Find last namespace separator */` |
|      133 |  5621 | `		while( z > zFunc ){` |
|      133 |  5622 | `			if( z[-1] == '\\' ){` |
|       15 |  5623 | `				break;` |
|        - |  5624 | `			}` |
|      119 |  5625 | `			z--;` |
|        1 |  5626 | `		}` |
|       15 |  5627 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5628 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5629 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5630 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5631 | `		}` |
|        7 |  5632 | `	}` |
|   554494 |  5633 | `	if( pEntry ){` |
|        - |  5634 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5635 | `		ph7_class_instance *pThis;` |
|        - |  5636 | `		ph7_value *pFrameStack;` |
|        - |  5637 | `		ph7_vm_func *pVmFunc;` |
|        - |  5638 | `		ph7_class *pSelf;` |
|        - |  5639 | `		VmFrame *pFrame;` |
|        - |  5640 | `		ph7_value *pObj;` |
|        - |  5641 | `		VmSlot sArg;` |
|        - |  5642 | `		sxu32 n;` |
|        - |  5643 | `		/* initialize fields */` |
|    12114 |  5644 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12114 |  5645 | `		pThis = 0;` |
|    12114 |  5646 | `		pSelf = 0;` |
|    12114 |  5647 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5648 | `			ph7_class_method *pMeth;` |
|        - |  5649 | `			/* Class method call */` |
|     1448 |  5650 | `			ph7_value *pTarget = &pTos[-1];` |
|     1448 |  5651 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5652 | `				/* Extract the 'this' pointer */` |
|     1448 |  5653 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5654 | `					/* Instance already loaded */` |
|     1414 |  5655 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1414 |  5656 | `					pThis->iRef++;` |
|     1414 |  5657 | `					pSelf = pThis->pClass;` |
|      706 |  5658 | `				}` |
|     1448 |  5659 | `				if( pSelf == 0 ){` |
|       36 |  5660 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5661 | `						/* "Late Static Binding" class name */` |
|       44 |  5662 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       14 |  5663 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       14 |  5664 | `					}` |
|       36 |  5665 | `					if( pSelf == 0 ){` |
|       13 |  5666 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5667 | `					}` |
|       17 |  5668 | `				}` |
|     1448 |  5669 | `				if( pThis == 0  ){` |
|       36 |  5670 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  5671 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5672 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5673 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5674 | `					}` |
|       36 |  5675 | `					if( pFrameLocal->pParent ){` |
|        - |  5676 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5677 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5678 | `						if( pThis ){` |
|       13 |  5679 | `							pThis->iRef++;` |
|        6 |  5680 | `						}` |
|        9 |  5681 | `					}` |
|       17 |  5682 | `				}` |
|     1448 |  5683 | `				VmPopOperand(&pTos,1);` |
|     1448 |  5684 | `				PH7_MemObjRelease(pTos);` |
|        - |  5685 | `				/* Synchronize pointers */` |
|     1448 |  5686 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5687 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5688 | `				 * user have already computed the random generated unique class method name` |
|        - |  5689 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5690 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5691 | `				 */` |
|     1448 |  5692 | `				while( pArg < pStack ){` |
|      ! 0 |  5693 | `					pArg++;` |
|      ! 0 |  5694 | `				}` |
|     1448 |  5695 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5696 | `					/* Check if the call is allowed */` |
|     1448 |  5697 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1448 |  5698 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  5699 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5700 | `							/* Pop given arguments */` |
|      ! 0 |  5701 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5702 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5703 | `							}` |
|        - |  5704 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5705 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5706 | `							break;` |
|        - |  5707 | `						}` |
|        3 |  5708 | `					}` |
|      723 |  5709 | `				}` |
|      723 |  5710 | `			}` |
|      723 |  5711 | `		}` |
|        - |  5712 | `		/* Check The recursion limit */` |
|    12114 |  5713 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5714 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5715 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5716 | `				&pVmFunc->sName);` |
|        - |  5717 | `			/* Pop given arguments */` |
|        3 |  5718 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5719 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5720 | `			}` |
|        - |  5721 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5722 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5723 | `			break;` |
|        - |  5724 | `		}` |
|    12112 |  5725 | `		if( pVmFunc->pNextName ){` |
|        - |  5726 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  5727 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  5728 | `		}` |
|        - |  5729 | `		/* Extract the formal argument set */` |
|    12112 |  5730 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5731 | `		/* Create a new VM frame  */` |
|    12112 |  5732 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12112 |  5733 | `		if( rc != SXRET_OK ){` |
|        - |  5734 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5735 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5736 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5737 | `				&pVmFunc->sName);` |
|        - |  5738 | `			/* Pop given arguments */` |
|      ! 0 |  5739 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5740 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5741 | `			}` |
|        - |  5742 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5743 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5744 | `			break;` |
|        - |  5745 | `		}` |
|    12112 |  5746 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5747 | `			/* Install the '$this' variable */` |
|        - |  5748 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1424 |  5749 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1424 |  5750 | `			if( pObj ){` |
|        - |  5751 | `				/* Reflect the change */` |
|     1424 |  5752 | `				pObj->x.pOther = pThis;` |
|     1424 |  5753 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      711 |  5754 | `			}` |
|      711 |  5755 | `		}` |
|    12112 |  5756 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5757 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5758 | `			/* Install static variables */` |
|      ! 0 |  5759 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5760 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5761 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5762 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5763 | `					/* Initialize the static variables */` |
|      ! 0 |  5764 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5765 | `					if( pObj ){` |
|        - |  5766 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5767 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5768 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5769 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5770 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5771 | `						}` |
|      ! 0 |  5772 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5773 | `					}else{` |
|      ! 0 |  5774 | `						continue;` |
|        - |  5775 | `					}` |
|      ! 0 |  5776 | `				}` |
|        - |  5777 | `				/* Install in the current frame */` |
|      ! 0 |  5778 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5779 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5780 | `			}` |
|      ! 0 |  5781 | `		}` |
|        - |  5782 | `		/* Push arguments in the local frame */` |
|    12112 |  5783 | `		n = 0;` |
|    33634 |  5784 | `		while( pArg < pTos ){` |
|    21524 |  5785 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    21374 |  5786 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5787 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5788 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5789 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5790 | `						goto Abort;` |
|        - |  5791 | `					}` |
|      ! 0 |  5792 | `				}` |
|        - |  5793 | `				/* Make sure the given arguments are of the correct type */` |
|    21374 |  5794 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5795 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5796 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5797 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5798 | `						ph7_class *pClass;` |
|        - |  5799 | `						/* Try to extract the desired class */` |
|      ! 0 |  5800 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5801 | `						if( pClass ){` |
|      ! 0 |  5802 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5803 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5804 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5805 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5806 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5807 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5808 | `								}` |
|      ! 0 |  5809 | `							}else{` |
|        - |  5810 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5811 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5812 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5813 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5814 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5815 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5816 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5817 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5818 | `								}` |
|        - |  5819 | `							}` |
|      ! 0 |  5820 | `						}` |
|     1088 |  5821 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5822 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5823 | `						/* Cast to the desired type */` |
|      ! 0 |  5824 | `						xCast(pArg);` |
|      ! 0 |  5825 | `					}` |
|      543 |  5826 | `				}` |
|    21374 |  5827 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5828 | `					/* Pass by reference */` |
|       48 |  5829 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5830 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5831 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5832 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5833 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5834 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5835 | `						}` |
|        - |  5836 | `						/* Switch to pass by value */` |
|      ! 0 |  5837 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5838 | `					}else{` |
|        - |  5839 | `						SyHashEntry *pRefEntry;` |
|        - |  5840 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5841 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5842 | `						if( pRefEntry == 0 ){` |
|       71 |  5843 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5844 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5845 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5846 | `							sArg.pUserData = 0;` |
|       48 |  5847 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5848 | `						}` |
|       48 |  5849 | `						pObj = 0;` |
|        - |  5850 | `					}` |
|       25 |  5851 | `				}else{` |
|        - |  5852 | `					/* Pass by value,make a copy of the given argument */` |
|    21328 |  5853 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5854 | `				}` |
|    10688 |  5855 | `			}else{` |
|        - |  5856 | `				char zName[32];` |
|        - |  5857 | `				SyString sArgName;` |
|        - |  5858 | `				/* Set a dummy name */` |
|      152 |  5859 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5860 | `				sArgName.zString = zName;` |
|        - |  5861 | `				/* Annonymous argument */` |
|      152 |  5862 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5863 | `			}` |
|    21524 |  5864 | `			if( pObj ){` |
|    21478 |  5865 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5866 | `				/* Insert argument index  */` |
|    21478 |  5867 | `				sArg.nIdx = pObj->nIdx;` |
|    21478 |  5868 | `				sArg.pUserData = 0;` |
|    21478 |  5869 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10738 |  5870 | `			}` |
|    21524 |  5871 | `			PH7_MemObjRelease(pArg);` |
|    21524 |  5872 | `			pArg++;` |
|    21524 |  5873 | `			++n;` |
|        2 |  5874 | `		}` |
|        - |  5875 | `		/* Set up closure environment */` |
|    12112 |  5876 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5877 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5878 | `			ph7_value *pValue;` |
|        - |  5879 | `			sxu32 iEnv;` |
|        9 |  5880 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5881 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5882 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5883 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5884 | `					/* Do not install null value */` |
|        9 |  5885 | `					continue;` |
|        - |  5886 | `				}` |
|        9 |  5887 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5888 | `				if( pValue == 0 ){` |
|      ! 0 |  5889 | `					continue;` |
|        - |  5890 | `				}` |
|        - |  5891 | `				/* Invalidate any prior representation */` |
|        9 |  5892 | `				PH7_MemObjRelease(pValue);` |
|        - |  5893 | `				/* Duplicate bound variable value */` |
|        9 |  5894 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5895 | `			}` |
|        4 |  5896 | `		}` |
|        - |  5897 | `		/* Process default values */` |
|    13970 |  5898 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1860 |  5899 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1854 |  5900 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1854 |  5901 | `				if( pObj ){` |
|        - |  5902 | `					/* Evaluate the default value and extract it's result */` |
|     1854 |  5903 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1854 |  5904 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5905 | `						goto Abort;` |
|        - |  5906 | `					}` |
|        - |  5907 | `					/* Insert argument index */` |
|     1854 |  5908 | `					sArg.nIdx = pObj->nIdx;` |
|     1854 |  5909 | `					sArg.pUserData = 0;` |
|     1854 |  5910 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5911 | `					/* Make sure the default argument is of the correct type */` |
|     1854 |  5912 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5913 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5914 | `						/* Cast to the desired type */` |
|      ! 0 |  5915 | `						xCast(pObj);` |
|      ! 0 |  5916 | `					}` |
|      926 |  5917 | `				}` |
|      926 |  5918 | `			}` |
|     1860 |  5919 | `			++n;` |
|        2 |  5920 | `		}` |
|        - |  5921 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5922 | `		 * does not return anything.` |
|        - |  5923 | `		 */` |
|    12112 |  5924 | `		PH7_MemObjRelease(pTos);` |
|    12112 |  5925 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5926 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12112 |  5927 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12112 |  5928 | `		if( pFrameStack == 0 ){` |
|        - |  5929 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5930 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5931 | `				&pVmFunc->sName);` |
|      ! 0 |  5932 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5933 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5934 | `			}` |
|      ! 0 |  5935 | `			break;` |
|        - |  5936 | `		}` |
|    12112 |  5937 | `		if( pSelf ){` |
|        - |  5938 | `			/* Push class name */` |
|     1446 |  5939 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      722 |  5940 | `		}` |
|        - |  5941 | `		/* Increment nesting level */` |
|    12112 |  5942 | `		pVm->nRecursionDepth++;` |
|        - |  5943 | `		/* Execute function body */` |
|    12112 |  5944 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5945 | `		/* Decrement nesting level */` |
|    12112 |  5946 | `		pVm->nRecursionDepth--;` |
|    12112 |  5947 | `		if( pSelf ){` |
|        - |  5948 | `			/* Pop class name */` |
|     1446 |  5949 | `			(void)SySetPop(&pVm->aSelf);` |
|      722 |  5950 | `		}` |
|        - |  5951 | `		/* Cleanup the mess left behind */` |
|    12112 |  5952 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5953 | `			/* Return by reference,reflect that */` |
|        9 |  5954 | `			if( n != SXU32_HIGH ){` |
|        9 |  5955 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5956 | `				sxu32 i;` |
|        - |  5957 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5958 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5959 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5960 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5961 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5962 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5963 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5964 | `								&pVmFunc->sName);` |
|      ! 0 |  5965 | `						}` |
|      ! 0 |  5966 | `						n = SXU32_HIGH;` |
|      ! 0 |  5967 | `						break;` |
|        - |  5968 | `					}` |
|        3 |  5969 | `				}` |
|        5 |  5970 | `			}else{` |
|      ! 0 |  5971 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5972 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5973 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5974 | `						&pVmFunc->sName);` |
|      ! 0 |  5975 | `				}` |
|        - |  5976 | `			}` |
|        9 |  5977 | `			pTos->nIdx = n;` |
|        4 |  5978 | `		}` |
|        - |  5979 | `		/* Cleanup the mess left behind */` |
|    12112 |  5980 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5981 | `			/* An exception was throw in this frame */` |
|        7 |  5982 | `			pFrame = pFrame->pParent;` |
|        7 |  5983 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5984 | `				/* Pop the resutlt */` |
|        5 |  5985 | `				VmPopOperand(&pTos,1);` |
|        - |  5986 | `				/* Jump to this destination */` |
|        5 |  5987 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5988 | `				rc = PH7_OK;` |
|        3 |  5989 | `			}else{` |
|        3 |  5990 | `				if( pFrame->pParent ){` |
|        3 |  5991 | `					rc = PH7_EXCEPTION;` |
|        2 |  5992 | `				}else{` |
|        - |  5993 | `					/* Continue normal execution */` |
|      ! 0 |  5994 | `					rc = PH7_OK;` |
|        - |  5995 | `				}` |
|        - |  5996 | `			}` |
|        3 |  5997 | `		}` |
|        - |  5998 | `		/* Free the operand stack */` |
|    12112 |  5999 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6000 | `		/* Leave the frame */` |
|    12112 |  6001 | `		VmLeaveFrame(&(*pVm));` |
|    12112 |  6002 | `		if( rc == PH7_ABORT ){` |
|        - |  6003 | `			/* Abort processing immeditaley */` |
|        7 |  6004 | `			goto Abort;` |
|    12106 |  6005 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6006 | `			goto Exception;` |
|        - |  6007 | `		}` |
|     6053 |  6008 | `	}else{` |
|        - |  6009 | `		ph7_user_func *pFunc;` |
|        - |  6010 | `		ph7_context sCtx;` |
|        - |  6011 | `		ph7_value sRet;` |
|        - |  6012 | `		/* Look for an installed foreign function.` |
|        - |  6013 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6014 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6015 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6016 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6017 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6018 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   542382 |  6019 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   542382 |  6020 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6021 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6022 | `			const char *zShort = sName.zString;` |
|        - |  6023 | `			sxu32 i;` |
|      217 |  6024 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6025 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6026 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6027 | `				}` |
|      102 |  6028 | `			}` |
|       15 |  6029 | `			if( zShort != sName.zString ){` |
|       15 |  6030 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6031 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6032 | `			}` |
|        7 |  6033 | `		}` |
|   542382 |  6034 | `		if( pEntry == 0 ){` |
|        - |  6035 | `			/* Call to undefined function */` |
|        5 |  6036 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6037 | `			/* Pop given arguments */` |
|        5 |  6038 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6039 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6040 | `			}` |
|        - |  6041 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6042 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6043 | `			break;` |
|        - |  6044 | `		}` |
|   542378 |  6045 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6046 | `		/* Start collecting function arguments */` |
|   542378 |  6047 | `		SySetReset(&aArg);` |
|  1457778 |  6048 | `		while( pArg < pTos ){` |
|   915402 |  6049 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   915402 |  6050 | `			pArg++;` |
|        2 |  6051 | `		}` |
|        - |  6052 | `		/* Assume a null return value */` |
|   542378 |  6053 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6054 | `		/* Init the call context */` |
|   542378 |  6055 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6056 | `		/* Call the foreign function */` |
|   542378 |  6057 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6058 | `		/* Release the call context */` |
|   542378 |  6059 | `		VmReleaseCallContext(&sCtx);` |
|   542378 |  6060 | `		if( rc == PH7_ABORT ){` |
|      463 |  6061 | `			goto Abort;` |
|   541916 |  6062 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6063 | `			VmFrame *pFrm = pVm->pFrame;` |
|       13 |  6064 | `			while( pFrm->pParent && (pFrm->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  6065 | `				pFrm = pFrm->pParent;` |
|        1 |  6066 | `			}` |
|        7 |  6067 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6068 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6069 | `				goto Exception;` |
|        - |  6070 | `			}` |
|        - |  6071 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6072 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6073 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6074 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6075 | `			}` |
|        - |  6076 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6077 | `			VmPopOperand(&pTos,1);` |
|        - |  6078 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6079 | `			pFrm = pVm->pFrame;` |
|        7 |  6080 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6081 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6082 | `			}` |
|        7 |  6083 | `			break;` |
|        - |  6084 | `		}` |
|   541910 |  6085 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6086 | `			/* Pop function name and arguments */` |
|   524616 |  6087 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   262329 |  6088 | `		}` |
|        - |  6089 | `		/* Save foreign function return value */` |
|   541910 |  6090 | `		PH7_MemObjStore(&sRet,pTos);` |
|   541910 |  6091 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6092 | `	}` |
|   554012 |  6093 | `	break;` |
|        - |  6094 | `				  }` |
|        - |  6095 | `/*` |
|        - |  6096 | ` * OP_CONSUME: P1 * *` |
|        - |  6097 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6098 | ` */` |
|    10786 |  6099 | `case PH7_OP_CONSUME: {` |
|    21574 |  6100 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    21574 |  6101 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6102 |  |
|    21574 |  6103 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    21574 |  6104 | `	pCur = pOut;` |
|        - |  6105 | `	/* Start the consume process  */` |
|    43146 |  6106 | `	while( pOut <= pTos ){` |
|        - |  6107 | `		/* Force a string cast */` |
|    21574 |  6108 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6109 | `			PH7_MemObjToString(pOut);` |
|      149 |  6110 | `		}` |
|    21574 |  6111 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6112 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6113 | `			/* Invoke the output consumer callback */` |
|    11834 |  6114 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    11834 |  6115 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6116 | `				/* Increment output length */` |
|     5388 |  6117 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2693 |  6118 | `			}` |
|    11834 |  6119 | `			SyBlobRelease(&pOut->sBlob);` |
|    11834 |  6120 | `			if( rc == SXERR_ABORT ){` |
|        - |  6121 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6122 | `				goto Abort;` |
|        - |  6123 | `			}` |
|     5916 |  6124 | `		}` |
|    21574 |  6125 | `		pOut++;` |
|        2 |  6126 | `	}` |
|    21574 |  6127 | `	pTos = &pCur[-1];` |
|    21572 |  6128 | `	break;` |
|        - |  6129 | `					 }` |
|        - |  6130 |  |
|        - |  6131 | `		} /* Switch() */` |
|  9528186 |  6132 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6133 | `	} /* For(;;) */` |
|    14932 |  6134 | `Done:` |
|    29866 |  6135 | `	SySetRelease(&aArg);` |
|    29866 |  6136 | `	return SXRET_OK;` |
|      238 |  6137 | `Abort:` |
|      477 |  6138 | `	SySetRelease(&aArg);` |
|     1661 |  6139 | `	while( pTos >= pStack ){` |
|     1185 |  6140 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6141 | `		pTos--;` |
|        1 |  6142 | `	}` |
|      477 |  6143 | `	return PH7_ABORT;` |
|        1 |  6144 | `Exception:` |
|        3 |  6145 | `	SySetRelease(&aArg);` |
|        5 |  6146 | `	while( pTos >= pStack ){` |
|        3 |  6147 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6148 | `		pTos--;` |
|        1 |  6149 | `	}` |
|        3 |  6150 | `	return PH7_EXCEPTION;` |
|    15173 |  6151 |  |
|        - |  6152 | `/*` |
|        - |  6153 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6154 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6155 | ` * See block-comment on that function for additional information.` |
|        - |  6156 | ` */` |
|    14434 |  6157 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6158 |  |
|        - |  6159 | `	ph7_value *pStack;` |
|        - |  6160 | `	sxi32 rc;` |
|        - |  6161 | `	/* Allocate a new operand stack */` |
|    14436 |  6162 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14436 |  6163 | `	if( pStack == 0 ){` |
|      ! 0 |  6164 | `		return SXERR_MEM;` |
|        - |  6165 | `	}` |
|        - |  6166 | `	/* Execute the program */` |
|    14436 |  6167 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6168 | `	/* Free the operand stack */` |
|    14436 |  6169 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6170 | `	/* Execution result */` |
|    14436 |  6171 | `	return rc;` |
|     7219 |  6172 |  |
|        - |  6173 | `/*` |
|        - |  6174 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6175 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6176 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6177 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6178 | ` * execution ends.` |
|        - |  6179 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6180 | ` * additional information.` |
|        - |  6181 | ` */` |
|     2192 |  6182 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6183 |  |
|        - |  6184 | `	VmShutdownCB *pEntry;` |
|        - |  6185 | `	ph7_value *apArg[10];` |
|        - |  6186 | `	sxu32 n,nEntry;` |
|        - |  6187 | `	int i;` |
|        - |  6188 | `	/* Point to the stack of registered callbacks */` |
|     2194 |  6189 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    24114 |  6190 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    21922 |  6191 | `		apArg[i] = 0;` |
|    10962 |  6192 | `	}` |
|     2196 |  6193 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6194 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6195 | `		if( pEntry ){` |
|        - |  6196 | `			/* Prepare callback arguments if any */` |
|        3 |  6197 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6198 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6199 | `					break;` |
|        - |  6200 | `				}` |
|      ! 0 |  6201 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6202 | `			}` |
|        - |  6203 | `			/* Invoke the callback */` |
|        3 |  6204 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6205 | `			/*` |
|        - |  6206 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6207 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6208 | `			 */` |
|        3 |  6209 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6210 | `			if( pEntry ){` |
|        3 |  6211 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6212 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6213 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6214 | `				}` |
|        1 |  6215 | `			}` |
|        1 |  6216 | `		}` |
|        2 |  6217 | `	}` |
|     2194 |  6218 | `	SySetReset(&pVm->aShutdown);` |
|     2194 |  6219 |  |
|        - |  6220 | `/*` |
|        - |  6221 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6222 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6223 | ` * See block-comment on that function for additional information.` |
|        - |  6224 | ` */` |
|     2200 |  6225 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6226 |  |
|        - |  6227 | `	/* Make sure we are ready to execute this program */` |
|     2202 |  6228 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6229 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6230 | `	}` |
|        - |  6231 | `	/* Set the execution magic number  */` |
|     2202 |  6232 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6233 | `	/* Execute the program */` |
|     2202 |  6234 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6235 | `	/* Invoke any shutdown callbacks */` |
|     2198 |  6236 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6237 | `	/*` |
|        - |  6238 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6239 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6240 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6241 | `	 */` |
|     2198 |  6242 | `	return SXRET_OK;` |
|     1102 |  6243 |  |
|        - |  6244 | `/*` |
|        - |  6245 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6246 | ` * the desired message.` |
|        - |  6247 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6248 | ` * in 'api.c' for additional information.` |
|        - |  6249 | ` */` |
|      350 |  6250 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6251 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6252 | `	SyString *pString /* Message to output */` |
|        - |  6253 | `	)` |
|        2 |  6254 |  |
|      352 |  6255 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6256 | `	sxi32 rc = SXRET_OK;` |
|        - |  6257 | `	/* Call the output consumer */` |
|      352 |  6258 | `	if( pString->nByte > 0 ){` |
|      352 |  6259 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6260 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6261 | `			/* Increment output length */` |
|       17 |  6262 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6263 | `		}` |
|      175 |  6264 | `	}` |
|      352 |  6265 | `	return rc;` |
|        2 |  6266 |  |
|        - |  6267 | `/*` |
|        - |  6268 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6269 | ` * callback to consume the formatted message.` |
|        - |  6270 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6271 | ` * in 'api.c' for additional information.` |
|        - |  6272 | ` */` |
|        2 |  6273 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6274 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6275 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6276 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6277 | `	)` |
|        1 |  6278 |  |
|        3 |  6279 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6280 | `	sxi32 rc = SXRET_OK;` |
|        - |  6281 | `	SyBlob sWorker;` |
|        - |  6282 | `	/* Format the message and call the output consumer */` |
|        3 |  6283 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6284 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6285 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6286 | `		/* Consume the formatted message */` |
|        3 |  6287 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6288 | `	}` |
|        3 |  6289 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6290 | `		/* Increment output length */` |
|      ! 0 |  6291 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6292 | `	}` |
|        - |  6293 | `	/* Release the working buffer */` |
|        3 |  6294 | `	SyBlobRelease(&sWorker);` |
|        3 |  6295 | `	return rc;` |
|        1 |  6296 |  |
|        - |  6297 | `/*` |
|        - |  6298 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6299 | ` * This function never fail and always return a pointer` |
|        - |  6300 | ` * to a null terminated string.` |
|        - |  6301 | ` */` |
|       12 |  6302 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6303 |  |
|       13 |  6304 | `	const char *zOp = "Unknown     ";` |
|       13 |  6305 | `	switch(nOp){` |
|        3 |  6306 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6307 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6308 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6309 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6310 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6311 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6312 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6313 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6314 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6315 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6316 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6317 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6318 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6319 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6320 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6321 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6322 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6323 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6324 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6325 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6326 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6327 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6328 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6329 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6330 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6331 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6332 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6333 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6334 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6335 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6336 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6337 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6338 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6339 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6340 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6341 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6342 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6343 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6344 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6345 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6346 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6347 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6348 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6349 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6350 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6351 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6352 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6353 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6354 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6355 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  6356 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6357 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6358 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6359 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6360 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6361 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6362 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6363 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6364 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6365 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6366 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6367 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6368 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6369 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6370 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6371 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6372 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6373 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6374 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6375 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6376 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6377 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6378 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6379 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6380 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6381 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6382 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6383 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6384 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6385 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6386 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6387 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6388 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6389 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6390 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6391 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6392 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6393 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6394 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6395 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6396 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6397 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6398 | `	default:` |
|      ! 0 |  6399 | `		break;` |
|        - |  6400 | `	}` |
|       13 |  6401 | `	return zOp;` |
|        1 |  6402 |  |
|        - |  6403 | `/*` |
|        - |  6404 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6405 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6406 | ` * is responsible of consuming the generated dump.` |
|        - |  6407 | ` */` |
|        2 |  6408 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6409 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6410 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6411 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6412 | `	)` |
|        1 |  6413 |  |
|        - |  6414 | `	sxi32 rc;` |
|        3 |  6415 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6416 | `	return rc;` |
|        1 |  6417 |  |
|        - |  6418 | `/*` |
|        - |  6419 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6420 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6421 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6422 | ` * in 'compile.c' for additional information.` |
|        - |  6423 | ` */` |
|        8 |  6424 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6425 |  |
|        9 |  6426 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6427 | `	/* Evaluate and expand constant value */` |
|        9 |  6428 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6429 |  |
|        - |  6430 | `/*` |
|        - |  6431 | ` * Section:` |
|        - |  6432 | ` *  Function handling functions.` |
|        - |  6433 | ` * Status:` |
|        - |  6434 | ` *    Stable.` |
|        - |  6435 | ` */` |
|        - |  6436 | `/*` |
|        - |  6437 | ` * int func_num_args(void)` |
|        - |  6438 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6439 | ` * Parameters` |
|        - |  6440 | ` *   None.` |
|        - |  6441 | ` * Return` |
|        - |  6442 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6443 | ` *  or -1 if called from the globe scope.` |
|        - |  6444 | ` */` |
|      906 |  6445 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6446 |  |
|        - |  6447 | `	VmFrame *pFrame;` |
|        - |  6448 | `	ph7_vm *pVm;` |
|        - |  6449 | `	/* Point to the target VM */` |
|      908 |  6450 | `	pVm = pCtx->pVm;` |
|        - |  6451 | `	/* Current frame */` |
|      908 |  6452 | `	pFrame = pVm->pFrame;` |
|      908 |  6453 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6454 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6455 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6456 | `	}` |
|      908 |  6457 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6458 | `		SXUNUSED(nArg);` |
|      ! 0 |  6459 | `		SXUNUSED(apArg);` |
|        - |  6460 | `		/* Global frame,return -1 */` |
|      ! 0 |  6461 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6462 | `		return SXRET_OK;` |
|        - |  6463 | `	}` |
|        - |  6464 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6465 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6466 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6467 | `	return SXRET_OK;` |
|      455 |  6468 |  |
|        - |  6469 | `/*` |
|        - |  6470 | ` * value func_get_arg(int $arg_num)` |
|        - |  6471 | ` *   Return an item from the argument list.` |
|        - |  6472 | ` * Parameters` |
|        - |  6473 | ` *  Argument number(index start from zero).` |
|        - |  6474 | ` * Return` |
|        - |  6475 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6476 | ` */` |
|       22 |  6477 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6478 |  |
|       24 |  6479 | `	ph7_value *pObj = 0;` |
|       24 |  6480 | `	VmSlot *pSlot = 0;` |
|        - |  6481 | `	VmFrame *pFrame;` |
|        - |  6482 | `	ph7_vm *pVm;` |
|        - |  6483 | `	/* Point to the target VM */` |
|       24 |  6484 | `	pVm = pCtx->pVm;` |
|        - |  6485 | `	/* Current frame */` |
|       24 |  6486 | `	pFrame = pVm->pFrame;` |
|       24 |  6487 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6488 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6489 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6490 | `	}` |
|       24 |  6491 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6492 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6493 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6494 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6495 | `		return SXRET_OK;` |
|        - |  6496 | `	}` |
|        - |  6497 | `	/* Extract the desired index */` |
|       21 |  6498 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6499 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6500 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6501 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6502 | `		return SXRET_OK;` |
|        - |  6503 | `	}` |
|        - |  6504 | `	/* Extract the desired argument */` |
|       21 |  6505 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6506 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6507 | `			/* Return the desired argument */` |
|       21 |  6508 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6509 | `		}else{` |
|        - |  6510 | `			/* No such argument,return false */` |
|      ! 0 |  6511 | `			ph7_result_bool(pCtx,0);` |
|        - |  6512 | `		}` |
|       11 |  6513 | `	}else{` |
|        - |  6514 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6515 | `		ph7_result_bool(pCtx,0);` |
|        - |  6516 | `	}` |
|       21 |  6517 | `	return SXRET_OK;` |
|       13 |  6518 |  |
|        - |  6519 | `/*` |
|        - |  6520 | ` * array func_get_args_byref(void)` |
|        - |  6521 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6522 | ` * Parameters` |
|        - |  6523 | ` *  None.` |
|        - |  6524 | ` * Return` |
|        - |  6525 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6526 | ` *  member of the current user-defined function's argument list.` |
|        - |  6527 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6528 | ` * NOTE:` |
|        - |  6529 | ` *  Arguments are returned to the array by reference.` |
|        - |  6530 | ` */` |
|        2 |  6531 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6532 |  |
|        - |  6533 | `	ph7_value *pArray;` |
|        - |  6534 | `	VmFrame *pFrame;` |
|        - |  6535 | `	VmSlot *aSlot;` |
|        - |  6536 | `	sxu32 n;` |
|        - |  6537 | `	/* Point to the current frame */` |
|        3 |  6538 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6539 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6540 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6541 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6542 | `	}` |
|        3 |  6543 | `	if( pFrame->pParent == 0 ){` |
|        - |  6544 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6545 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6546 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6547 | `		return SXRET_OK;` |
|        - |  6548 | `	}` |
|        - |  6549 | `	/* Create a new array */` |
|        3 |  6550 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6551 | `	if( pArray == 0 ){` |
|      ! 0 |  6552 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6553 | `		SXUNUSED(apArg);` |
|      ! 0 |  6554 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6555 | `		return SXRET_OK;` |
|        - |  6556 | `	}` |
|        - |  6557 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6558 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6559 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6560 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6561 | `	}` |
|        - |  6562 | `	/* Return the freshly created array */` |
|        3 |  6563 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6564 | `	return SXRET_OK;` |
|        2 |  6565 |  |
|        - |  6566 | `/*` |
|        - |  6567 | ` * array func_get_args(void)` |
|        - |  6568 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6569 | ` * Parameters` |
|        - |  6570 | ` *  None.` |
|        - |  6571 | ` * Return` |
|        - |  6572 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6573 | ` *  member of the current user-defined function's argument list.` |
|        - |  6574 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6575 | ` */` |
|       62 |  6576 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6577 |  |
|       64 |  6578 | `	ph7_value *pObj = 0;` |
|        - |  6579 | `	ph7_value *pArray;` |
|        - |  6580 | `	VmFrame *pFrame;` |
|        - |  6581 | `	VmSlot *aSlot;` |
|        - |  6582 | `	sxu32 n;` |
|        - |  6583 | `	/* Point to the current frame */` |
|       64 |  6584 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6585 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6586 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6587 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6588 | `	}` |
|       64 |  6589 | `	if( pFrame->pParent == 0 ){` |
|        - |  6590 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6591 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6592 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6593 | `		return SXRET_OK;` |
|        - |  6594 | `	}` |
|        - |  6595 | `	/* Create a new array */` |
|       64 |  6596 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6597 | `	if( pArray == 0 ){` |
|      ! 0 |  6598 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6599 | `		SXUNUSED(apArg);` |
|      ! 0 |  6600 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6601 | `		return SXRET_OK;` |
|        - |  6602 | `	}` |
|        - |  6603 | `	/* Start filling the array with the given arguments */` |
|       64 |  6604 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6605 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6606 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6607 | `		if( pObj ){` |
|      130 |  6608 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6609 | `		}` |
|       66 |  6610 | `	}` |
|        - |  6611 | `	/* Return the freshly created array */` |
|       64 |  6612 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6613 | `	return SXRET_OK;` |
|       33 |  6614 |  |
|        - |  6615 | `/*` |
|        - |  6616 | ` * bool function_exists(string $name)` |
|        - |  6617 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6618 | ` * Parameters` |
|        - |  6619 | ` *  The name of the desired function.` |
|        - |  6620 | ` * Return` |
|        - |  6621 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6622 | ` */` |
|     1638 |  6623 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6624 |  |
|        - |  6625 | `	const char *zName;` |
|        - |  6626 | `	ph7_vm *pVm;` |
|        - |  6627 | `	int nLen;` |
|        - |  6628 | `	int res;` |
|     1640 |  6629 | `	if( nArg < 1 ){` |
|        - |  6630 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6631 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6632 | `		return SXRET_OK;` |
|        - |  6633 | `	}` |
|        - |  6634 | `	/* Point to the target VM */` |
|     1640 |  6635 | `	pVm = pCtx->pVm;` |
|        - |  6636 | `	/* Extract the function name */` |
|     1640 |  6637 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6638 | `	/* Assume the function is not defined */` |
|     1640 |  6639 | `	res = 0;` |
|        - |  6640 | `	/* Perform the lookup */` |
|     2457 |  6641 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1634 |  6642 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6643 | `			/* Function is defined */` |
|      206 |  6644 | `			res = 1;` |
|      102 |  6645 | `	}` |
|     1640 |  6646 | `	ph7_result_bool(pCtx,res);` |
|     1640 |  6647 | `	return SXRET_OK;` |
|      821 |  6648 |  |
|        - |  6649 | `/*` |
|        - |  6650 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6651 | ` * [i.e: Whether it is callable or not].` |
|        - |  6652 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6653 | ` */` |
|    16002 |  6654 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6655 |  |
|    16004 |  6656 | `	int res = 0;` |
|    16004 |  6657 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6658 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6659 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6660 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6661 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6662 | `		if( pMethod && CallInvoke ){` |
|        - |  6663 | `			ph7_value sResult;` |
|        - |  6664 | `			sxi32 rc;` |
|        - |  6665 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6666 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6667 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6668 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6669 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6670 | `			}` |
|      ! 0 |  6671 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6672 | `		}` |
|    16004 |  6673 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6674 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6675 | `		if( pMap->nEntry == 2 ){` |
|        - |  6676 | `			ph7_class *pClass;` |
|        - |  6677 | `			ph7_value *pV;` |
|        - |  6678 | `			/* Extract the target class */` |
|       12 |  6679 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6680 | `			if( pV ){` |
|       12 |  6681 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6682 | `				if( pClass ){` |
|        - |  6683 | `					ph7_class_method *pMethod;` |
|        - |  6684 | `					/* Extract the target method */` |
|       10 |  6685 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6686 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6687 | `						/* Perform the lookup */` |
|       10 |  6688 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6689 | `						if( pMethod ){` |
|        - |  6690 | `							/* Method is callable */` |
|        5 |  6691 | `							res = 1;` |
|        2 |  6692 | `						}` |
|        4 |  6693 | `					}` |
|        4 |  6694 | `				}` |
|        5 |  6695 | `			}` |
|        7 |  6696 | `		}` |
|    15991 |  6697 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6698 | `		const char *zName;` |
|        - |  6699 | `		int nLen;` |
|        - |  6700 | `		/* Extract the name */` |
|     4700 |  6701 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6702 | `		/* Perform the lookup */` |
|     4715 |  6703 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6704 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6705 | `				/* Function is callable */` |
|     4682 |  6706 | `				res = 1;` |
|     2340 |  6707 | `		}` |
|     2349 |  6708 | `	}` |
|    16004 |  6709 | `	return res;` |
|        2 |  6710 |  |
|        - |  6711 | `/*` |
|        - |  6712 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6713 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6714 | ` * Parameters` |
|        - |  6715 | ` * $name` |
|        - |  6716 | ` *    The callback function to check` |
|        - |  6717 | ` * $syntax_only` |
|        - |  6718 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6719 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6720 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6721 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6722 | ` *    a string.` |
|        - |  6723 | ` * Return` |
|        - |  6724 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6725 | ` */` |
|       14 |  6726 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6727 |  |
|        - |  6728 | `	ph7_vm *pVm;` |
|        - |  6729 | `	int res;` |
|       15 |  6730 | `	if( nArg < 1 ){` |
|        - |  6731 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6732 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6733 | `		return SXRET_OK;` |
|        - |  6734 | `	}` |
|        - |  6735 | `	/* Point to the target VM */` |
|       15 |  6736 | `	pVm = pCtx->pVm;` |
|        - |  6737 | `	/* Perform the requested operation */` |
|       15 |  6738 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6739 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6740 | `	return SXRET_OK;` |
|        8 |  6741 |  |
|        - |  6742 | `/*` |
|        - |  6743 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6744 | ` * defined below.` |
|        - |  6745 | ` */` |
|     1082 |  6746 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6747 |  |
|     1083 |  6748 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6749 | `	ph7_value sName;` |
|        - |  6750 | `	sxi32 rc;` |
|        - |  6751 | `	/* Prepare the function name for insertion */` |
|     1083 |  6752 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6753 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6754 | `	/* Perform the insertion */` |
|     1083 |  6755 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6756 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6757 | `	return rc;` |
|        1 |  6758 |  |
|        - |  6759 | `/*` |
|        - |  6760 | ` * array get_defined_functions(void)` |
|        - |  6761 | ` *  Returns an array of all defined functions.` |
|        - |  6762 | ` * Parameter` |
|        - |  6763 | ` *  None.` |
|        - |  6764 | ` * Return` |
|        - |  6765 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6766 | ` *  both built-in (internal) and user-defined.` |
|        - |  6767 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6768 | ` *  defined ones using $arr["user"].` |
|        - |  6769 | ` * Note:` |
|        - |  6770 | ` *  NULL is returned on failure.` |
|        - |  6771 | ` */` |
|        2 |  6772 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6773 |  |
|        - |  6774 | `	ph7_value *pArray,*pEntry;` |
|        - |  6775 | `	/* NOTE:` |
|        - |  6776 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6777 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6778 | `	 */` |
|        3 |  6779 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6780 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6781 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6782 | `		SXUNUSED(apArg);` |
|        - |  6783 | `		/* Return NULL */` |
|      ! 0 |  6784 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6785 | `		return SXRET_OK;` |
|        - |  6786 | `	}` |
|        3 |  6787 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6788 | `	if( pEntry == 0 ){` |
|        - |  6789 | `		/* Return NULL */` |
|      ! 0 |  6790 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6791 | `		return SXRET_OK;` |
|        - |  6792 | `	}` |
|        - |  6793 | `	/* Fill with the appropriate information */` |
|        3 |  6794 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6795 | `	/* Create the 'internal' index */` |
|        3 |  6796 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6797 | `	/* Create the user-func array */` |
|        3 |  6798 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6799 | `	if( pEntry == 0 ){` |
|        - |  6800 | `		/* Return NULL */` |
|      ! 0 |  6801 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6802 | `		return SXRET_OK;` |
|        - |  6803 | `	}` |
|        - |  6804 | `	/* Fill with the appropriate information */` |
|        3 |  6805 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6806 | `	/* Create the 'user' index */` |
|        3 |  6807 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6808 | `	/* Return the multi-dimensional array */` |
|        3 |  6809 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6810 | `	return SXRET_OK;` |
|        2 |  6811 |  |
|        - |  6812 | `/*` |
|        - |  6813 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6814 | ` *  Register a function for execution on shutdown.` |
|        - |  6815 | ` * Note` |
|        - |  6816 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6817 | ` *  be called in the same order as they were registered.` |
|        - |  6818 | ` * Parameters` |
|        - |  6819 | ` *  $callback` |
|        - |  6820 | ` *   The shutdown callback to register.` |
|        - |  6821 | ` * $param` |
|        - |  6822 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6823 | ` * Return` |
|        - |  6824 | ` *  Nothing.` |
|        - |  6825 | ` */` |
|        2 |  6826 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6827 |  |
|        - |  6828 | `	VmShutdownCB sEntry;` |
|        - |  6829 | `	int i,j;` |
|        3 |  6830 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6831 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6832 | `		return PH7_OK;` |
|        - |  6833 | `	}` |
|        - |  6834 | `	/* Zero the Entry */` |
|        3 |  6835 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6836 | `	/* Initialize fields */` |
|        3 |  6837 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6838 | `	/* Save the callback name for later invocation name */` |
|        3 |  6839 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6840 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6841 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6842 | `	}` |
|        - |  6843 | `	/* Copy arguments */` |
|        3 |  6844 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6845 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6846 | `			/* Limit reached */` |
|      ! 0 |  6847 | `			break;` |
|        - |  6848 | `		}` |
|      ! 0 |  6849 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6850 | `	}` |
|        3 |  6851 | `	sEntry.nArg = j;` |
|        - |  6852 | `	/* Install the callback */` |
|        3 |  6853 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6854 | `	return PH7_OK;` |
|        2 |  6855 |  |
|        - |  6856 | `/*` |
|        - |  6857 | ` * Section:` |
|        - |  6858 | ` *  Class handling functions.` |
|        - |  6859 | ` * Status:` |
|        - |  6860 | ` *    Stable.` |
|        - |  6861 | ` */` |
|        - |  6862 | `/*` |
|        - |  6863 | ` * Extract the top active class. NULL is returned` |
|        - |  6864 | ` * if the class stack is empty.` |
|        - |  6865 | ` */` |
|      536 |  6866 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6867 |  |
|      538 |  6868 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6869 | `	ph7_class **apClass;` |
|      538 |  6870 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6871 | `		/* Empty stack,return NULL */` |
|       15 |  6872 | `		return 0;` |
|        - |  6873 | `	}` |
|        - |  6874 | `	/* Peek the last entry */` |
|      524 |  6875 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      524 |  6876 | `	return apClass[pSet->nUsed - 1];` |
|      270 |  6877 |  |
|        - |  6878 | `/*` |
|        - |  6879 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6880 | ` *   Get the class that declared the currently executing method.` |
|        - |  6881 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6882 | ` *` |
|        - |  6883 | ` * Parameters` |
|        - |  6884 | ` *   pVm: Target VM` |
|        - |  6885 | ` *` |
|        - |  6886 | ` * Return` |
|        - |  6887 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6888 | ` *   - Not executing within a class method` |
|        - |  6889 | ` *` |
|        - |  6890 | ` * Note` |
|        - |  6891 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6892 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6893 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6894 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6895 | ` *   declaring class.` |
|        - |  6896 | ` */` |
|       48 |  6897 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  6898 |  |
|       50 |  6899 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6900 | `	ph7_vm_func *pVmFunc;` |
|        - |  6901 |  |
|        - |  6902 | `	/* Skip exception frames to find the actual method frame */` |
|       50 |  6903 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6904 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6905 | `	}` |
|        - |  6906 |  |
|        - |  6907 | `	/* Check if we're in a method context */` |
|       50 |  6908 | `	if( pFrame->pParent ){` |
|       46 |  6909 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       46 |  6910 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6911 | `			/* Return the declaring class */` |
|       46 |  6912 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6913 | `		}` |
|      ! 0 |  6914 | `	}` |
|        - |  6915 |  |
|        5 |  6916 | `	return 0;` |
|       26 |  6917 |  |
|        - |  6918 |  |
|        - |  6919 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6920 | `/*` |
|        - |  6921 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6922 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6923 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6924 | ` * return value indicates failure.` |
|        - |  6925 | ` */` |
|     1160 |  6926 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6927 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6928 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6929 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6930 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6931 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6932 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6933 | `	)` |
|        2 |  6934 |  |
|        - |  6935 | `	ph7_value *aStack;` |
|        - |  6936 | `	VmInstr aInstr[2];` |
|        - |  6937 | `	int iCursor;` |
|        - |  6938 | `	int i;` |
|        - |  6939 | `	/* Create a new operand stack */` |
|     1162 |  6940 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1162 |  6941 | `	if( aStack == 0 ){` |
|      ! 0 |  6942 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6943 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6944 | `		return SXERR_MEM;` |
|        - |  6945 | `	}` |
|        - |  6946 | `	/* Fill the operand stack with the given arguments */` |
|     1722 |  6947 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      562 |  6948 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6949 | `		/*` |
|        - |  6950 | `		 * Symisc eXtension:` |
|        - |  6951 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6952 | `		 */` |
|      562 |  6953 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      282 |  6954 | `	}` |
|     1162 |  6955 | `	iCursor = nArg + 1;` |
|     1162 |  6956 | `	if( pThis ){` |
|        - |  6957 | `		/*` |
|        - |  6958 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6959 | `		 */` |
|     1156 |  6960 | `		pThis->iRef++; /* Increment reference count */` |
|     1156 |  6961 | `		aStack[i].x.pOther = pThis;` |
|     1156 |  6962 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      577 |  6963 | `	}` |
|     1162 |  6964 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1162 |  6965 | `	i++;` |
|        - |  6966 | `	/* Push method name */` |
|     1162 |  6967 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1162 |  6968 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1162 |  6969 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1162 |  6970 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6971 | `	/* Emit the CALL istruction */` |
|     1162 |  6972 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1162 |  6973 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1162 |  6974 | `	aInstr[0].iP2 = 0;` |
|     1162 |  6975 | `	aInstr[0].p3  = 0;` |
|        - |  6976 | `	/* Emit the DONE instruction */` |
|     1162 |  6977 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1162 |  6978 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1162 |  6979 | `	aInstr[1].iP2 = 0;` |
|     1162 |  6980 | `	aInstr[1].p3  = 0;` |
|        - |  6981 | `	/* Execute the method body (if available) */` |
|     1162 |  6982 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6983 | `	/* Clean up the mess left behind */` |
|     1162 |  6984 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1162 |  6985 | `	return PH7_OK;` |
|      582 |  6986 |  |
|        - |  6987 | `/*` |
|        - |  6988 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6989 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6990 | ` * in the apArg[] array.` |
|        - |  6991 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6992 | ` * return value indicates failure.` |
|        - |  6993 | ` */` |
|      926 |  6994 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6995 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6996 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6997 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6998 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6999 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7000 | `	)` |
|        2 |  7001 |  |
|        - |  7002 | `	ph7_value *aStack;` |
|        - |  7003 | `	VmInstr aInstr[2];` |
|        - |  7004 | `	int i;` |
|      928 |  7005 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7006 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  7007 | `		if( pResult ){` |
|        - |  7008 | `			/* Assume a null return value */` |
|      ! 0 |  7009 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7010 | `		}` |
|      471 |  7011 | `		return SXERR_INVALID;` |
|        - |  7012 | `	}` |
|      458 |  7013 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7014 | `		/* Class method */` |
|       11 |  7015 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7016 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7017 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7018 | `		ph7_class *pClass = 0;` |
|        - |  7019 | `		ph7_value *pValue;` |
|        - |  7020 | `		sxi32 rc;` |
|       11 |  7021 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7022 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7023 | `			if( pResult ){` |
|        - |  7024 | `				/* Assume a null return value */` |
|      ! 0 |  7025 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7026 | `			}` |
|      ! 0 |  7027 | `			return SXRET_OK;` |
|        - |  7028 | `		}` |
|        - |  7029 | `		/* Extract the class name or an instance of it */` |
|       11 |  7030 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7031 | `		if( pValue ){` |
|       11 |  7032 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7033 | `		}` |
|       11 |  7034 | `		if( pClass == 0 ){` |
|        - |  7035 | `			/* No such class,return NULL */` |
|      ! 0 |  7036 | `			if( pResult ){` |
|      ! 0 |  7037 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7038 | `			}` |
|      ! 0 |  7039 | `			return SXRET_OK;` |
|        - |  7040 | `		}` |
|       11 |  7041 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7042 | `			/* Point to the class instance */` |
|        5 |  7043 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7044 | `		}` |
|        - |  7045 | `		/* Try to extract the method */` |
|       11 |  7046 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7047 | `		if( pValue ){` |
|       11 |  7048 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7049 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7050 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7051 | `			}` |
|        5 |  7052 | `		}` |
|       11 |  7053 | `		if( pMethod == 0 ){` |
|        - |  7054 | `			/* No such method,return NULL */` |
|      ! 0 |  7055 | `			if( pResult ){` |
|      ! 0 |  7056 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7057 | `			}` |
|      ! 0 |  7058 | `			return SXRET_OK;` |
|        - |  7059 | `		}` |
|        - |  7060 | `		/* Call the class method */` |
|       11 |  7061 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7062 | `		return rc;` |
|        - |  7063 | `	}` |
|        - |  7064 | `	/* Create a new operand stack */` |
|      448 |  7065 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7066 | `	if( aStack == 0 ){` |
|      ! 0 |  7067 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7068 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7069 | `		if( pResult ){` |
|        - |  7070 | `			/* Assume a null return value */` |
|      ! 0 |  7071 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7072 | `		}` |
|      ! 0 |  7073 | `		return SXERR_MEM;` |
|        - |  7074 | `	}` |
|        - |  7075 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7076 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7077 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7078 | `		/*` |
|        - |  7079 | `		 * Symisc eXtension:` |
|        - |  7080 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7081 | `		 */` |
|     1024 |  7082 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7083 | `	}` |
|        - |  7084 | `	/* Push the function name */` |
|      448 |  7085 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7086 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7087 | `	/* Emit the CALL istruction */` |
|      448 |  7088 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7089 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7090 | `	aInstr[0].iP2 = 0;` |
|      448 |  7091 | `	aInstr[0].p3  = 0;` |
|        - |  7092 | `	/* Emit the DONE instruction */` |
|      448 |  7093 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7094 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7095 | `	aInstr[1].iP2 = 0;` |
|      448 |  7096 | `	aInstr[1].p3  = 0;` |
|        - |  7097 | `	/* Execute the function body (if available) */` |
|      448 |  7098 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7099 | `	/* Clean up the mess left behind */` |
|      448 |  7100 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7101 | `	return PH7_OK;` |
|      465 |  7102 |  |
|        - |  7103 | `/*` |
|        - |  7104 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7105 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7106 | ` * parameter.` |
|        - |  7107 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7108 | ` * return value indicates failure.` |
|        - |  7109 | ` */` |
|      236 |  7110 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7111 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7112 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7113 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7114 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7115 | `	)` |
|        1 |  7116 |  |
|        - |  7117 | `	ph7_value *pArg;` |
|        - |  7118 | `	SySet aArg;` |
|        - |  7119 | `	va_list ap;` |
|        - |  7120 | `	sxi32 rc;` |
|      237 |  7121 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7122 | `	/* Copy arguments one after one */` |
|      237 |  7123 | `	va_start(ap,pResult);` |
|      393 |  7124 | `	for(;;){` |
|      787 |  7125 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7126 | `		if( pArg == 0 ){` |
|      237 |  7127 | `			break;` |
|        - |  7128 | `		}` |
|      551 |  7129 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7130 | `	}` |
|        - |  7131 | `	/* Call the core routine */` |
|      237 |  7132 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7133 | `	/* Cleanup */` |
|      237 |  7134 | `	SySetRelease(&aArg);` |
|      237 |  7135 | `	return rc;` |
|        1 |  7136 |  |
|        - |  7137 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7138 | `/*` |
|        - |  7139 | ` * bool defined(string $name)` |
|        - |  7140 | ` *  Checks whether a given named constant exists.` |
|        - |  7141 | ` * Parameter:` |
|        - |  7142 | ` *  Name of the desired constant.` |
|        - |  7143 | ` * Return` |
|        - |  7144 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7145 | ` */` |
|       14 |  7146 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7147 |  |
|        - |  7148 | `	const char *zName;` |
|       16 |  7149 | `	int nLen = 0;` |
|       16 |  7150 | `	int res = 0;` |
|       16 |  7151 | `	if( nArg < 1 ){` |
|        - |  7152 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7153 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7154 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7155 | `		return SXRET_OK;` |
|        - |  7156 | `	}` |
|        - |  7157 | `	/* Extract constant name */` |
|       16 |  7158 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7159 | `	/* Perform the lookup */` |
|       16 |  7160 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7161 | `		/* Already defined */` |
|       10 |  7162 | `		res = 1;` |
|        4 |  7163 | `	}` |
|       16 |  7164 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7165 | `	return SXRET_OK;` |
|        9 |  7166 |  |
|        - |  7167 | `/*` |
|        - |  7168 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7169 | ` * below.` |
|        - |  7170 | ` */` |
|        8 |  7171 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7172 |  |
|       10 |  7173 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7174 | `	/* Expand constant value */` |
|       10 |  7175 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7176 |  |
|        - |  7177 | `/*` |
|        - |  7178 | ` * bool define(string $constant_name,expression value)` |
|        - |  7179 | ` *  Defines a named constant at runtime.` |
|        - |  7180 | ` * Parameter:` |
|        - |  7181 | ` *  $constant_name` |
|        - |  7182 | ` *   The name of the constant` |
|        - |  7183 | ` *  $value` |
|        - |  7184 | ` *   Constant value` |
|        - |  7185 | ` * Return:` |
|        - |  7186 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7187 | ` */` |
|       10 |  7188 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7189 |  |
|        - |  7190 | `	const char *zName;  /* Constant name */` |
|        - |  7191 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7192 | `	int nLen = 0;       /* Name length */` |
|        - |  7193 | `	sxi32 rc;` |
|       12 |  7194 | `	if( nArg < 2 ){` |
|        - |  7195 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7196 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7197 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7198 | `		return SXRET_OK;` |
|        - |  7199 | `	}` |
|       12 |  7200 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7201 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7202 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7203 | `		return SXRET_OK;` |
|        - |  7204 | `	}` |
|        - |  7205 | `	/* Extract constant name */` |
|       12 |  7206 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7207 | `	if( nLen < 1 ){` |
|      ! 0 |  7208 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7209 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7210 | `		return SXRET_OK;` |
|        - |  7211 | `	}` |
|        - |  7212 | `	/* Duplicate constant value */` |
|       12 |  7213 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7214 | `	if( pValue == 0 ){` |
|      ! 0 |  7215 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7216 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7217 | `		return SXRET_OK;` |
|        - |  7218 | `	}` |
|        - |  7219 | `	/* Initialize the memory object */` |
|       12 |  7220 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7221 | `	/* Register the constant */` |
|       12 |  7222 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7223 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7224 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7225 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7226 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7227 | `		return SXRET_OK;` |
|        - |  7228 | `	}` |
|        - |  7229 | `	/* Duplicate constant value */` |
|       12 |  7230 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7231 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7232 | `		/* Lower case the constant name */` |
|      ! 0 |  7233 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7234 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7235 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7236 | `				/* UTF-8 stream */` |
|      ! 0 |  7237 | `				zCur++;` |
|      ! 0 |  7238 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7239 | `					zCur++;` |
|      ! 0 |  7240 | `				}` |
|      ! 0 |  7241 | `				continue;` |
|        - |  7242 | `			}` |
|      ! 0 |  7243 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7244 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7245 | `				zCur[0] = (char)c;` |
|      ! 0 |  7246 | `			}` |
|      ! 0 |  7247 | `			zCur++;` |
|      ! 0 |  7248 | `		}` |
|        - |  7249 | `		/* Finally,register the constant */` |
|      ! 0 |  7250 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7251 | `	}` |
|        - |  7252 | `	/* All done,return TRUE */` |
|       12 |  7253 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7254 | `	return SXRET_OK;` |
|        7 |  7255 |  |
|        - |  7256 | `/*` |
|        - |  7257 | ` * value constant(string $name)` |
|        - |  7258 | ` *  Returns the value of a constant` |
|        - |  7259 | ` * Parameter` |
|        - |  7260 | ` *  $name` |
|        - |  7261 | ` *    Name of the constant.` |
|        - |  7262 | ` * Return` |
|        - |  7263 | ` *  Constant value or NULL if not defined.` |
|        - |  7264 | ` */` |
|        8 |  7265 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7266 |  |
|        - |  7267 | `	SyHashEntry *pEntry;` |
|        - |  7268 | `	ph7_constant *pCons;` |
|        - |  7269 | `	const char *zName; /* Constant name */` |
|        - |  7270 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7271 | `	int nLen;` |
|       10 |  7272 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7273 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7274 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7275 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7276 | `		return SXRET_OK;` |
|        - |  7277 | `	}` |
|        - |  7278 | `	/* Extract the constant name */` |
|       10 |  7279 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7280 | `	/* Perform the query */` |
|       10 |  7281 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7282 | `	if( pEntry == 0 ){` |
|        3 |  7283 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7284 | `		ph7_result_null(pCtx);` |
|        3 |  7285 | `		return SXRET_OK;` |
|        - |  7286 | `	}` |
|        8 |  7287 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7288 | `	/* Point to the structure that describe the constant */` |
|        8 |  7289 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7290 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7291 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7292 | `	/* Return that value */` |
|        8 |  7293 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7294 | `	/* Cleanup */` |
|        8 |  7295 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7296 | `	return SXRET_OK;` |
|        6 |  7297 |  |
|        - |  7298 | `/*` |
|        - |  7299 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7300 | ` * defined below.` |
|        - |  7301 | ` */` |
|      416 |  7302 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7303 |  |
|      417 |  7304 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7305 | `	ph7_value sName;` |
|        - |  7306 | `	sxi32 rc;` |
|        - |  7307 | `	/* Prepare the constant name for insertion */` |
|      417 |  7308 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7309 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7310 | `	/* Perform the insertion */` |
|      417 |  7311 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7312 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7313 | `	return rc;` |
|        1 |  7314 |  |
|        - |  7315 | `/*` |
|        - |  7316 | ` * array get_defined_constants(void)` |
|        - |  7317 | ` *  Returns an associative array with the names of all defined` |
|        - |  7318 | ` *  constants.` |
|        - |  7319 | ` * Parameters` |
|        - |  7320 | ` *  NONE.` |
|        - |  7321 | ` * Returns` |
|        - |  7322 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7323 | ` */` |
|        2 |  7324 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7325 |  |
|        - |  7326 | `	ph7_value *pArray;` |
|        - |  7327 | `	/* Create the array first*/` |
|        3 |  7328 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7329 | `	if( pArray == 0 ){` |
|      ! 0 |  7330 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7331 | `		SXUNUSED(apArg);` |
|        - |  7332 | `		/* Return NULL */` |
|      ! 0 |  7333 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7334 | `		return SXRET_OK;` |
|        - |  7335 | `	}` |
|        - |  7336 | `	/* Fill the array with the defined constants */` |
|        3 |  7337 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7338 | `	/* Return the created array */` |
|        3 |  7339 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7340 | `	return SXRET_OK;` |
|        2 |  7341 |  |
|        - |  7342 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7343 | `/*` |
|        - |  7344 | ` * Section:` |
|        - |  7345 | ` *  Random numbers/string generators.` |
|        - |  7346 | ` * Status:` |
|        - |  7347 | ` *    Stable.` |
|        - |  7348 | ` */` |
|        - |  7349 | `/*` |
|        - |  7350 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7351 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7352 | ` * used by te SQLite3 library.` |
|        - |  7353 | ` */` |
|     2272 |  7354 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7355 |  |
|        - |  7356 | `	sxu32 iNum;` |
|     2274 |  7357 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2274 |  7358 | `	return iNum;` |
|        2 |  7359 |  |
|        - |  7360 | `/*` |
|        - |  7361 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7362 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7363 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7364 | ` * by te SQLite3 library.` |
|        - |  7365 | ` */` |
|    71058 |  7366 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7367 |  |
|        - |  7368 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7369 | `	int i;` |
|        - |  7370 | `	/* Generate a binary string first */` |
|    71060 |  7371 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7372 | `	/* Turn the binary string into english based alphabet */` |
|   781808 |  7373 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   710750 |  7374 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   355376 |  7375 | `	 }` |
|    71060 |  7376 |  |
|        - |  7377 | `/*` |
|        - |  7378 | ` * int rand()` |
|        - |  7379 | ` * int mt_rand()` |
|        - |  7380 | ` * int rand(int $min,int $max)` |
|        - |  7381 | ` * int mt_rand(int $min,int $max)` |
|        - |  7382 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7383 | ` * Parameter` |
|        - |  7384 | ` *  $min` |
|        - |  7385 | ` *    The lowest value to return (default: 0)` |
|        - |  7386 | ` *  $max` |
|        - |  7387 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7388 | ` * Return` |
|        - |  7389 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7390 | ` * Note:` |
|        - |  7391 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7392 | ` *  by te SQLite3 library.` |
|        - |  7393 | ` */` |
|       20 |  7394 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7395 |  |
|        - |  7396 | `	sxu32 iNum;` |
|        - |  7397 | `	/* Generate the random number */` |
|       21 |  7398 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7399 | `	if( nArg > 1 ){` |
|        - |  7400 | `		sxu32 iMin,iMax;` |
|        3 |  7401 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7402 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7403 | `		if( iMin < iMax ){` |
|        3 |  7404 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7405 | `			if( iDiv > 0 ){` |
|        3 |  7406 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7407 | `			}` |
|        1 |  7408 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7409 | `			iNum %= iMax;` |
|      ! 0 |  7410 | `		}` |
|        1 |  7411 | `	}` |
|        - |  7412 | `	/* Return the number */` |
|       21 |  7413 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7414 | `	return SXRET_OK;` |
|        1 |  7415 |  |
|        - |  7416 | `/*` |
|        - |  7417 | ` * int getrandmax(void)` |
|        - |  7418 | ` * int mt_getrandmax(void)` |
|        - |  7419 | ` * int rc4_getrandmax(void)` |
|        - |  7420 | ` *   Show largest possible random value` |
|        - |  7421 | ` * Return` |
|        - |  7422 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7423 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7424 | ` * Note:` |
|        - |  7425 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7426 | ` *  by te SQLite3 library.` |
|        - |  7427 | ` */` |
|        4 |  7428 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7429 |  |
|        2 |  7430 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7431 | `	SXUNUSED(apArg);` |
|        5 |  7432 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7433 | `	return SXRET_OK;` |
|        1 |  7434 |  |
|        - |  7435 | `/*` |
|        - |  7436 | ` * string rand_str()` |
|        - |  7437 | ` * string rand_str(int $len)` |
|        - |  7438 | ` *  Generate a random string (English alphabet).` |
|        - |  7439 | ` * Parameter` |
|        - |  7440 | ` *  $len` |
|        - |  7441 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7442 | ` * Return` |
|        - |  7443 | ` *   A pseudo random string.` |
|        - |  7444 | ` * Note:` |
|        - |  7445 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7446 | ` *  by te SQLite3 library.` |
|        - |  7447 | ` *  This function is a symisc extension.` |
|        - |  7448 | ` */` |
|      120 |  7449 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7450 |  |
|        - |  7451 | `	char zString[1024];` |
|      122 |  7452 | `	int iLen = 0x10;` |
|      122 |  7453 | `	if( nArg > 0 ){` |
|        - |  7454 | `		/* Get the desired length */` |
|      122 |  7455 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7456 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7457 | `			/* Default length */` |
|        3 |  7458 | `			iLen = 0x10;` |
|        1 |  7459 | `		}` |
|       60 |  7460 | `	}` |
|        - |  7461 | `	/* Generate the random string */` |
|      122 |  7462 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7463 | `	/* Return the generated string */` |
|      122 |  7464 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7465 | `	return SXRET_OK;` |
|        2 |  7466 |  |
|        - |  7467 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7468 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7469 | `/* Unique ID private data */` |
|        - |  7470 | `struct unique_id_data` |
|        - |  7471 |  |
|        - |  7472 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7473 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7474 | `};` |
|        - |  7475 | `/*` |
|        - |  7476 | ` * Binary to hex consumer callback.` |
|        - |  7477 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7478 | ` * defined below.` |
|        - |  7479 | ` */` |
|      192 |  7480 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7481 |  |
|      193 |  7482 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7483 | `	sxu32 nBuflen;` |
|        - |  7484 | `	/* Extract result buffer length */` |
|      193 |  7485 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7486 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7487 | `			/*` |
|        - |  7488 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7489 | `			 * string will be 13 characters long` |
|        - |  7490 | `			 */` |
|       25 |  7491 | `		return SXERR_ABORT;` |
|        - |  7492 | `	}` |
|      169 |  7493 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7494 | `		return SXERR_ABORT;` |
|        - |  7495 | `	}` |
|        - |  7496 | `	/* Safely Consume the hex stream */` |
|      169 |  7497 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7498 | `	return SXRET_OK;` |
|       97 |  7499 |  |
|        - |  7500 | `/*` |
|        - |  7501 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7502 | ` *  Generate a unique ID` |
|        - |  7503 | ` * Parameter` |
|        - |  7504 | ` * $prefix` |
|        - |  7505 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7506 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7507 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7508 | ` * $more_entropy` |
|        - |  7509 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7510 | ` *  that the result will be unique.` |
|        - |  7511 | ` * Return` |
|        - |  7512 | ` *  Returns the unique identifier, as a string.` |
|        - |  7513 | ` */` |
|       24 |  7514 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7515 |  |
|        - |  7516 | `	struct unique_id_data sUniq;` |
|        - |  7517 | `	unsigned char zDigest[20];` |
|       25 |  7518 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7519 | `	const char *zPrefix;` |
|        - |  7520 | `	SHA1Context sCtx;` |
|        - |  7521 | `	char zRandom[7];` |
|        - |  7522 | `	int nPrefix;` |
|        - |  7523 | `	int entropy;` |
|        - |  7524 | `	/* Generate a random string first */` |
|       25 |  7525 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7526 | `	/* Initialize fields */` |
|       25 |  7527 | `	zPrefix = 0;` |
|       25 |  7528 | `	nPrefix = 0;` |
|       25 |  7529 | `	entropy = 0;` |
|       25 |  7530 | `	if( nArg > 0 ){` |
|        - |  7531 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7532 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7533 | `		if( nArg > 1 ){` |
|      ! 0 |  7534 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7535 | `		}` |
|      ! 0 |  7536 | `	}` |
|       25 |  7537 | `	SHA1Init(&sCtx);` |
|        - |  7538 | `	/* Generate the random ID */` |
|       25 |  7539 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7540 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7541 | `	}` |
|        - |  7542 | `	/* Append the random ID */` |
|       25 |  7543 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7544 | `	/* Append the random string */` |
|       25 |  7545 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7546 | `	/* Increment the number */` |
|       25 |  7547 | `	pVm->unique_id++;` |
|       25 |  7548 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7549 | `	/* Hexify the digest */` |
|       25 |  7550 | `	sUniq.pCtx = pCtx;` |
|       25 |  7551 | `	sUniq.entropy = entropy;` |
|       25 |  7552 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7553 | `	/* All done */` |
|       25 |  7554 | `	return PH7_OK;` |
|        1 |  7555 |  |
|        - |  7556 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7557 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7558 | `/*` |
|        - |  7559 | ` * Section:` |
|        - |  7560 | ` *  Language construct implementation as foreign functions.` |
|        - |  7561 | ` * Status:` |
|        - |  7562 | ` *    Stable.` |
|        - |  7563 | ` */` |
|        - |  7564 | `/*` |
|        - |  7565 | ` * void echo($string...)` |
|        - |  7566 | ` *  Output one or more messages.` |
|        - |  7567 | ` * Parameters` |
|        - |  7568 | ` *  $string` |
|        - |  7569 | ` *   Message to output.` |
|        - |  7570 | ` * Return` |
|        - |  7571 | ` *  NULL.` |
|        - |  7572 | ` */` |
|      ! 0 |  7573 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7574 |  |
|        - |  7575 | `	const char *zData;` |
|      ! 0 |  7576 | `	int nDataLen = 0;` |
|        - |  7577 | `	ph7_vm *pVm;` |
|        - |  7578 | `	int i,rc;` |
|        - |  7579 | `	/* Point to the target VM */` |
|      ! 0 |  7580 | `	pVm = pCtx->pVm;` |
|        - |  7581 | `	/* Output */` |
|      ! 0 |  7582 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7583 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7584 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7585 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7586 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7587 | `				/* Increment output length */` |
|      ! 0 |  7588 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7589 | `			}` |
|      ! 0 |  7590 | `			if( rc == SXERR_ABORT ){` |
|        - |  7591 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7592 | `				return PH7_ABORT;` |
|        - |  7593 | `			}` |
|      ! 0 |  7594 | `		}` |
|      ! 0 |  7595 | `	}` |
|      ! 0 |  7596 | `	return SXRET_OK;` |
|      ! 0 |  7597 |  |
|        - |  7598 | `/*` |
|        - |  7599 | ` * int print($string...)` |
|        - |  7600 | ` *  Output one or more messages.` |
|        - |  7601 | ` * Parameters` |
|        - |  7602 | ` *  $string` |
|        - |  7603 | ` *   Message to output.` |
|        - |  7604 | ` * Return` |
|        - |  7605 | ` *  1 always.` |
|        - |  7606 | ` */` |
|        2 |  7607 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7608 |  |
|        - |  7609 | `	const char *zData;` |
|        3 |  7610 | `	int nDataLen = 0;` |
|        - |  7611 | `	ph7_vm *pVm;` |
|        - |  7612 | `	int i,rc;` |
|        - |  7613 | `	/* Point to the target VM */` |
|        3 |  7614 | `	pVm = pCtx->pVm;` |
|        - |  7615 | `	/* Output */` |
|        5 |  7616 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7617 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7618 | `		if( nDataLen > 0 ){` |
|        3 |  7619 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7620 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7621 | `				/* Increment output length */` |
|        3 |  7622 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7623 | `			}` |
|        3 |  7624 | `			if( rc == SXERR_ABORT ){` |
|        - |  7625 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7626 | `				return PH7_ABORT;` |
|        - |  7627 | `			}` |
|        1 |  7628 | `		}` |
|        2 |  7629 | `	}` |
|        - |  7630 | `	/* Return 1 */` |
|        3 |  7631 | `	ph7_result_int(pCtx,1);` |
|        3 |  7632 | `	return SXRET_OK;` |
|        2 |  7633 |  |
|        - |  7634 | `/*` |
|        - |  7635 | ` * void exit(string $msg)` |
|        - |  7636 | ` * void exit(int $status)` |
|        - |  7637 | ` * void die(string $ms)` |
|        - |  7638 | ` * void die(int $status)` |
|        - |  7639 | ` *   Output a message and terminate program execution.` |
|        - |  7640 | ` * Parameter` |
|        - |  7641 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7642 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7643 | ` *  and not printed` |
|        - |  7644 | ` * Return` |
|        - |  7645 | ` *  NULL` |
|        - |  7646 | ` */` |
|      ! 0 |  7647 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7648 |  |
|      ! 0 |  7649 | `	if( nArg > 0 ){` |
|      ! 0 |  7650 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7651 | `			const char *zData;` |
|      ! 0 |  7652 | `			int iLen = 0;` |
|        - |  7653 | `			/* Print exit message */` |
|      ! 0 |  7654 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7655 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7656 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7657 | `			sxi32 iExitStatus;` |
|        - |  7658 | `			/* Record exit status code */` |
|      ! 0 |  7659 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7660 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7661 | `		}` |
|      ! 0 |  7662 | `	}` |
|        - |  7663 | `	/* Check if we are in an included file */` |
|      ! 0 |  7664 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7665 | `		/* Exit the entire process */` |
|      ! 0 |  7666 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7667 | `	}` |
|        - |  7668 | `	/* Abort processing immediately */` |
|      ! 0 |  7669 | `	return PH7_ABORT;` |
|      ! 0 |  7670 |  |
|        - |  7671 | `/*` |
|        - |  7672 | ` * bool isset($var,...)` |
|        - |  7673 | ` *  Finds out whether a variable is set.` |
|        - |  7674 | ` * Parameters` |
|        - |  7675 | ` *  One or more variable to check.` |
|        - |  7676 | ` * Return` |
|        - |  7677 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7678 | ` */` |
|    70446 |  7679 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7680 |  |
|        - |  7681 | `	ph7_value *pObj;` |
|    70448 |  7682 | `	int res = 0;` |
|        - |  7683 | `	int i;` |
|    70448 |  7684 | `	if( nArg < 1 ){` |
|        - |  7685 | `		/* Missing arguments,return false */` |
|      ! 0 |  7686 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7687 | `		return SXRET_OK;` |
|        - |  7688 | `	}` |
|        - |  7689 | `	/* Iterate over available arguments */` |
|    93066 |  7690 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    70448 |  7691 | `		pObj = apArg[i];` |
|    70448 |  7692 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    47324 |  7693 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7694 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7695 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7696 | `			}` |
|    23661 |  7697 | `		}` |
|    70448 |  7698 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    70448 |  7699 | `		if( !res ){` |
|        - |  7700 | `			/* Variable not set,return FALSE */` |
|    47830 |  7701 | `			ph7_result_bool(pCtx,0);` |
|    47830 |  7702 | `			return SXRET_OK;` |
|        - |  7703 | `		}` |
|    11311 |  7704 | `	}` |
|        - |  7705 | `	/* All given variable are set,return TRUE */` |
|    22620 |  7706 | `	ph7_result_bool(pCtx,1);` |
|    22620 |  7707 | `	return SXRET_OK;` |
|    35225 |  7708 |  |
|        - |  7709 | `/*` |
|        - |  7710 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7711 | ` * frame,the reference table and discard it's contents.` |
|        - |  7712 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7713 | ` */` |
|  2959664 |  7714 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7715 |  |
|        - |  7716 | `	ph7_value *pObj;` |
|        - |  7717 | `	VmRefObj *pRef;` |
|  2959666 |  7718 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2959666 |  7719 | `	if( pObj ){` |
|        - |  7720 | `		/* Release the object */` |
|  2959666 |  7721 | `		PH7_MemObjRelease(pObj);` |
|  1479832 |  7722 | `	}` |
|        - |  7723 | `	/* Remove old reference links */` |
|  2959666 |  7724 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2959666 |  7725 | `	if( pRef ){` |
|  2959646 |  7726 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7727 | `		/* Unlink from the reference table */` |
|  2959646 |  7728 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2959646 |  7729 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7730 | `			VmSlot sFree;` |
|        - |  7731 | `			/* Restore to the free list */` |
|  2959640 |  7732 | `			sFree.nIdx = nObjIdx;` |
|  2959640 |  7733 | `			sFree.pUserData = 0;` |
|  2959640 |  7734 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1479819 |  7735 | `		}` |
|  1479822 |  7736 | `	}` |
|  2959666 |  7737 | `	return SXRET_OK;` |
|        2 |  7738 |  |
|        - |  7739 | `/*` |
|        - |  7740 | ` * void unset($var,...)` |
|        - |  7741 | ` *   Unset one or more given variable.` |
|        - |  7742 | ` * Parameters` |
|        - |  7743 | ` *  One or more variable to unset.` |
|        - |  7744 | ` * Return` |
|        - |  7745 | ` *  Nothing.` |
|        - |  7746 | ` */` |
|     3258 |  7747 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7748 |  |
|        - |  7749 | `	ph7_value *pObj;` |
|        - |  7750 | `	ph7_vm *pVm;` |
|        - |  7751 | `	int i;` |
|        - |  7752 | `	/* Point to the target VM */` |
|     3260 |  7753 | `	pVm = pCtx->pVm;` |
|        - |  7754 | `	/* Iterate and unset */` |
|     9662 |  7755 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6404 |  7756 | `		pObj = apArg[i];` |
|     6404 |  7757 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      868 |  7758 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7759 | `				/* Throw an error */` |
|      ! 0 |  7760 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7761 | `			}` |
|      435 |  7762 | `		}else{` |
|     5537 |  7763 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7764 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5537 |  7765 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5531 |  7766 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2765 |  7767 | `			}` |
|        - |  7768 | `		}` |
|     3203 |  7769 | `	}` |
|     3260 |  7770 | `	return SXRET_OK;` |
|        2 |  7771 |  |
|        - |  7772 | `/*` |
|        - |  7773 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7774 | ` */` |
|      110 |  7775 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7776 |  |
|      111 |  7777 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7778 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7779 | `	ph7_value *pObj;` |
|        - |  7780 | `	sxu32 nIdx;` |
|        - |  7781 | `	/* Extract the memory object */` |
|      111 |  7782 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7783 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7784 | `	if( pObj ){` |
|      111 |  7785 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7786 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7787 | `				SyString sName;` |
|        - |  7788 | `				ph7_value sKey;` |
|        - |  7789 | `				/* Perform the insertion */` |
|      109 |  7790 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7791 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7792 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7793 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7794 | `			}` |
|       54 |  7795 | `		}` |
|       55 |  7796 | `	}` |
|      111 |  7797 | `	return SXRET_OK;` |
|        1 |  7798 |  |
|        - |  7799 | `/*` |
|        - |  7800 | ` * array get_defined_vars(void)` |
|        - |  7801 | ` *  Returns an array of all defined variables.` |
|        - |  7802 | ` * Parameter` |
|        - |  7803 | ` *  None` |
|        - |  7804 | ` * Return` |
|        - |  7805 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7806 | ` */` |
|        2 |  7807 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7808 |  |
|        3 |  7809 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7810 | `	ph7_value *pArray;` |
|        - |  7811 | `	/* Create a new array */` |
|        3 |  7812 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7813 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7814 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7815 | `		SXUNUSED(apArg);` |
|        - |  7816 | `		/* Return NULL */` |
|      ! 0 |  7817 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7818 | `		return SXRET_OK;` |
|        - |  7819 | `	}` |
|        - |  7820 | `	/* Superglobals first */` |
|        3 |  7821 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7822 | `	/* Then variable defined in the current frame */` |
|        3 |  7823 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7824 | `	/* Finally,return the created array */` |
|        3 |  7825 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7826 | `	return SXRET_OK;` |
|        2 |  7827 |  |
|        - |  7828 | `/*` |
|        - |  7829 | ` * bool gettype($var)` |
|        - |  7830 | ` *  Get the type of a variable` |
|        - |  7831 | ` * Parameters` |
|        - |  7832 | ` *   $var` |
|        - |  7833 | ` *    The variable being type checked.` |
|        - |  7834 | ` * Return` |
|        - |  7835 | ` *   String representation of the given variable type.` |
|        - |  7836 | ` */` |
|       32 |  7837 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7838 |  |
|       34 |  7839 | `	const char *zType = "Empty";` |
|       34 |  7840 | `	if( nArg > 0 ){` |
|       34 |  7841 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7842 | `	}` |
|        - |  7843 | `	/* Return the variable type */` |
|       34 |  7844 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7845 | `	return SXRET_OK;` |
|        2 |  7846 |  |
|        - |  7847 | `/*` |
|        - |  7848 | ` * string get_resource_type(resource $handle)` |
|        - |  7849 | ` *  This function gets the type of the given resource.` |
|        - |  7850 | ` * Parameters` |
|        - |  7851 | ` *  $handle` |
|        - |  7852 | ` *  The evaluated resource handle.` |
|        - |  7853 | ` * Return` |
|        - |  7854 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7855 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7856 | ` *  the return value will be the string Unknown.` |
|        - |  7857 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7858 | ` *  is not a resource.` |
|        - |  7859 | ` */` |
|        2 |  7860 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7861 |  |
|        3 |  7862 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7863 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7864 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7865 | `		return PH7_OK;` |
|        - |  7866 | `	}` |
|        3 |  7867 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7868 | `	return SXRET_OK;` |
|        2 |  7869 |  |
|        - |  7870 | `/*` |
|        - |  7871 | ` * void var_dump(expression,....)` |
|        - |  7872 | ` *   var_dump � Dumps information about a variable` |
|        - |  7873 | ` * Parameters` |
|        - |  7874 | ` *   One or more expression to dump.` |
|        - |  7875 | ` * Returns` |
|        - |  7876 | ` *  Nothing.` |
|        - |  7877 | ` */` |
|      218 |  7878 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7879 |  |
|        - |  7880 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7881 | `	int i;` |
|      220 |  7882 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7883 | `	/* Dump one or more expressions */` |
|      444 |  7884 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  7885 | `		ph7_value *pObj = apArg[i];` |
|        - |  7886 | `		/* Reset the working buffer */` |
|      226 |  7887 | `		SyBlobReset(&sDump);` |
|        - |  7888 | `		/* Dump the given expression */` |
|      226 |  7889 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7890 | `		/* Output */` |
|      226 |  7891 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  7892 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  7893 | `		}` |
|      114 |  7894 | `	}` |
|        - |  7895 | `	/* Release the working buffer */` |
|      220 |  7896 | `	SyBlobRelease(&sDump);` |
|      220 |  7897 | `	return SXRET_OK;` |
|        2 |  7898 |  |
|        - |  7899 | `/*` |
|        - |  7900 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7901 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7902 | ` * Parameters` |
|        - |  7903 | ` *   expression: Expression to dump` |
|        - |  7904 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7905 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7906 | ` *            print_r() will return the information rather than print it.` |
|        - |  7907 | ` * Return` |
|        - |  7908 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7909 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7910 | ` */` |
|       16 |  7911 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7912 |  |
|       17 |  7913 | `	int ret_string = 0;` |
|        - |  7914 | `	SyBlob sDump;` |
|       17 |  7915 | `	if( nArg < 1 ){` |
|        - |  7916 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7917 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7918 | `		return SXRET_OK;` |
|        - |  7919 | `	}` |
|       17 |  7920 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7921 | `	if ( nArg > 1 ){` |
|        - |  7922 | `		/* Where to redirect output */` |
|       11 |  7923 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7924 | `	}` |
|        - |  7925 | `	/* Generate dump */` |
|       17 |  7926 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  7927 | `	if( !ret_string ){` |
|        - |  7928 | `		/* Output dump */` |
|        7 |  7929 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7930 | `		/* Return true */` |
|        7 |  7931 | `		ph7_result_bool(pCtx,1);` |
|        4 |  7932 | `	}else{` |
|        - |  7933 | `		/* Generated dump as return value */` |
|       11 |  7934 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7935 | `	}` |
|        - |  7936 | `	/* Release the working buffer */` |
|       17 |  7937 | `	SyBlobRelease(&sDump);` |
|       17 |  7938 | `	return SXRET_OK;` |
|        9 |  7939 |  |
|        - |  7940 | `/*` |
|        - |  7941 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  7942 | ` * Same job as print_r. (see coment above)` |
|        - |  7943 | ` */` |
|        2 |  7944 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7945 |  |
|        3 |  7946 | `	int ret_string = 0;` |
|        - |  7947 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  7948 | `	if( nArg < 1 ){` |
|        - |  7949 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7950 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7951 | `		return SXRET_OK;` |
|        - |  7952 | `	}` |
|        3 |  7953 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  7954 | `	if ( nArg > 1 ){` |
|        - |  7955 | `		/* Where to redirect output */` |
|        3 |  7956 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  7957 | `	}` |
|        - |  7958 | `	/* Generate dump */` |
|        3 |  7959 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  7960 | `	if( !ret_string ){` |
|        - |  7961 | `		/* Output dump */` |
|      ! 0 |  7962 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7963 | `		/* Return NULL */` |
|      ! 0 |  7964 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7965 | `	}else{` |
|        - |  7966 | `		/* Generated dump as return value */` |
|        3 |  7967 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7968 | `	}` |
|        - |  7969 | `	/* Release the working buffer */` |
|        3 |  7970 | `	SyBlobRelease(&sDump);` |
|        3 |  7971 | `	return SXRET_OK;` |
|        2 |  7972 |  |
|        - |  7973 | `/*` |
|        - |  7974 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  7975 | ` *  Set/get the various assert flags.` |
|        - |  7976 | ` * Parameter` |
|        - |  7977 | ` * $what` |
|        - |  7978 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  7979 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  7980 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7981 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  7982 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7983 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  7984 | ` * $value` |
|        - |  7985 | ` *   An optional new value for the option.` |
|        - |  7986 | ` * Return` |
|        - |  7987 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7988 | ` */` |
|       30 |  7989 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7990 |  |
|       32 |  7991 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7992 | `	int iOption;` |
|        - |  7993 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  7994 | `	if( nArg < 1 ){` |
|        3 |  7995 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7996 | `			"ArgumentCountError",` |
|        - |  7997 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  7998 | `			);` |
|        - |  7999 | `	}` |
|        - |  8000 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  8001 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  8002 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  8003 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8004 | `			"TypeError",` |
|        - |  8005 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  8006 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  8007 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  8008 | `			);` |
|        - |  8009 | `	}` |
|       30 |  8010 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  8011 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  8012 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  8013 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  8014 | `	switch( iOption ){` |
|        6 |  8015 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  8016 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  8017 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  8018 | `		if( nArg > 1 ){` |
|        5 |  8019 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8020 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  8021 | `			}else{` |
|        3 |  8022 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  8023 | `			}` |
|        2 |  8024 | `		}` |
|       14 |  8025 | `		break;` |
|        1 |  8026 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8027 | `		/* Return old callback or null */` |
|        3 |  8028 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8029 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8030 | `		}else{` |
|        3 |  8031 | `			ph7_result_null(pCtx);` |
|        - |  8032 | `		}` |
|        3 |  8033 | `		if( nArg > 1 ){` |
|      ! 0 |  8034 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8035 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8036 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8037 | `			}else{` |
|      ! 0 |  8038 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8039 | `			}` |
|      ! 0 |  8040 | `		}` |
|        3 |  8041 | `		break;` |
|        5 |  8042 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8043 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8044 | `		if( nArg > 1 ){` |
|        5 |  8045 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8046 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8047 | `			}else{` |
|        3 |  8048 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8049 | `			}` |
|        2 |  8050 | `		}` |
|       11 |  8051 | `		break;` |
|      ! 0 |  8052 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8053 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8054 | `		break;` |
|        1 |  8055 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8056 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8057 | `		break;` |
|      ! 0 |  8058 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8059 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8060 | `		break;` |
|        1 |  8061 | `	default:` |
|        - |  8062 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8063 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8064 | `			"ValueError",` |
|        - |  8065 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8066 | `			);` |
|        - |  8067 | `	}` |
|       28 |  8068 | `	return PH7_OK;` |
|       17 |  8069 |  |
|        - |  8070 | `/*` |
|        - |  8071 | ` * bool assert(mixed $assertion)` |
|        - |  8072 | ` *  Checks if assertion is FALSE.` |
|        - |  8073 | ` * Parameter` |
|        - |  8074 | ` *  $assertion` |
|        - |  8075 | ` *    The assertion to test.` |
|        - |  8076 | ` * Return` |
|        - |  8077 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8078 | ` */` |
|       26 |  8079 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8080 |  |
|       28 |  8081 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8082 | `	int iFlags,iResult;` |
|        - |  8083 | `	const char *zDesc;` |
|        - |  8084 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8085 | `	if( nArg < 1 ){` |
|        3 |  8086 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8087 | `			"ArgumentCountError",` |
|        - |  8088 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8089 | `			);` |
|        - |  8090 | `	}` |
|       26 |  8091 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8092 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8093 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8094 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8095 | `		return PH7_OK;` |
|        - |  8096 | `	}` |
|        - |  8097 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8098 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8099 | `	if( !iResult ){` |
|        - |  8100 | `		/* Assertion failed */` |
|        - |  8101 | `		/* Extract optional description */` |
|       13 |  8102 | `		zDesc = 0;` |
|       13 |  8103 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8104 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8105 | `		}` |
|       13 |  8106 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8107 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8108 | `			ph7_value sFile,sLine;` |
|        - |  8109 | `			ph7_value *apCbArg[3];` |
|        - |  8110 | `			SyString *pFile;` |
|        - |  8111 | `			/* Extract the processed script */` |
|      ! 0 |  8112 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8113 | `			if( pFile == 0 ){` |
|      ! 0 |  8114 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8115 | `			}` |
|        - |  8116 | `			/* Invoke the callback */` |
|      ! 0 |  8117 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8118 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8119 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8120 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8121 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8122 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8123 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8124 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8125 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8126 | `		}` |
|       13 |  8127 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8128 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8129 | `			return PH7_ABORT;` |
|        - |  8130 | `		}` |
|        - |  8131 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8132 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8133 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8134 | `				"AssertionError",` |
|        - |  8135 | `				"%s",` |
|        1 |  8136 | `				zDesc` |
|        - |  8137 | `				);` |
|      ! 0 |  8138 | `		}else{` |
|       11 |  8139 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8140 | `				"AssertionError",` |
|        - |  8141 | `				"assert(false)"` |
|        - |  8142 | `				);` |
|        - |  8143 | `		}` |
|        - |  8144 | `	}` |
|        - |  8145 | `	/* Assertion passed */` |
|       14 |  8146 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8147 | `	return PH7_OK;` |
|       15 |  8148 |  |
|        - |  8149 | `/*` |
|        - |  8150 | ` * Section:` |
|        - |  8151 | ` *  Error reporting functions.` |
|        - |  8152 | ` * Status:` |
|        - |  8153 | ` *    Stable.` |
|        - |  8154 | ` */` |
|        - |  8155 | `/*` |
|        - |  8156 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8157 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8158 | ` * Parameters` |
|        - |  8159 | ` *  $error_msg` |
|        - |  8160 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8161 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8162 | ` * $error_type` |
|        - |  8163 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8164 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8165 | ` * Return` |
|        - |  8166 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8167 | ` */` |
|       12 |  8168 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8169 |  |
|       14 |  8170 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8171 | `	int rc = PH7_OK;` |
|       14 |  8172 | `	if( nArg > 0 ){` |
|        - |  8173 | `		const char *zErr;` |
|        - |  8174 | `		int nLen;` |
|        - |  8175 | `		/* Extract the error message */` |
|       12 |  8176 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8177 | `		if( nArg > 1 ){` |
|        - |  8178 | `			/* Extract the error type */` |
|       12 |  8179 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8180 | `			switch( nErr ){` |
|        1 |  8181 | `			case 1:   /* E_ERROR */` |
|        - |  8182 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8183 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8184 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8185 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8186 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8187 | `				break;` |
|        1 |  8188 | `			case 2:   /* E_WARNING */` |
|        - |  8189 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8190 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8191 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8192 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8193 | `				break;` |
|        3 |  8194 | `			default:` |
|        8 |  8195 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8196 | `				break;` |
|        - |  8197 | `			}` |
|        5 |  8198 | `		}` |
|        - |  8199 | `		/* Report error */` |
|       12 |  8200 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8201 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8202 | `			return rc;` |
|        - |  8203 | `		}` |
|        - |  8204 | `		/* Return true */` |
|       12 |  8205 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8206 | `	}else{` |
|        - |  8207 | `		/* Missing arguments,return FALSE */` |
|        3 |  8208 | `		ph7_result_bool(pCtx,0);` |
|        - |  8209 | `	}` |
|       14 |  8210 | `	return rc;` |
|        8 |  8211 |  |
|        - |  8212 | `/*` |
|        - |  8213 | ` * int error_reporting([int $level])` |
|        - |  8214 | ` *  Sets which PHP errors are reported.` |
|        - |  8215 | ` * Parameters` |
|        - |  8216 | ` *  $level` |
|        - |  8217 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8218 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8219 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8220 | ` *   levels will not always behave as expected.` |
|        - |  8221 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8222 | ` *   in the predefined constants.` |
|        - |  8223 | ` * Return` |
|        - |  8224 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8225 | ` *   parameter is given.` |
|        - |  8226 | ` */` |
|       40 |  8227 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8228 |  |
|       42 |  8229 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8230 | `	int nOld;` |
|        - |  8231 | `	/* Extract the old reporting level */` |
|       42 |  8232 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8233 | `	if( nArg > 0 ){` |
|        - |  8234 | `		int nNew;` |
|        - |  8235 | `		/* Extract the desired error reporting level */` |
|       34 |  8236 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8237 | `		if( !nNew ){` |
|        - |  8238 | `			/* Do not report errors at all */` |
|        5 |  8239 | `			pVm->bErrReport = 0;` |
|        3 |  8240 | `		}else{` |
|        - |  8241 | `			/* Report all errors */` |
|       30 |  8242 | `			pVm->bErrReport = 1;` |
|        - |  8243 | `		}` |
|       16 |  8244 | `	}` |
|        - |  8245 | `	/* Return the old level */` |
|       42 |  8246 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8247 | `	return PH7_OK;` |
|        2 |  8248 |  |
|        - |  8249 | `/*` |
|        - |  8250 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8251 | ` *  Send an error message somewhere.` |
|        - |  8252 | ` * Parameter` |
|        - |  8253 | ` *  $message` |
|        - |  8254 | ` *   The error message that should be logged.` |
|        - |  8255 | ` *  $message_type` |
|        - |  8256 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8257 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8258 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8259 | ` *       This is the default option.` |
|        - |  8260 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8261 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8262 | ` *    2  No longer an option.` |
|        - |  8263 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8264 | ` *       to the end of the message string.` |
|        - |  8265 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8266 | ` *  $destination` |
|        - |  8267 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8268 | ` *  $extra_headers` |
|        - |  8269 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8270 | ` * Return` |
|        - |  8271 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8272 | ` * NOTE:` |
|        - |  8273 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8274 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8275 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8276 | ` *  Otherwise this function is no-op.` |
|        - |  8277 | ` */` |
|        4 |  8278 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8279 |  |
|        - |  8280 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8281 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8282 | `	int iType = 0;` |
|        5 |  8283 | `	if( nArg < 1 ){` |
|        - |  8284 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8285 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8286 | `		return PH7_OK;` |
|        - |  8287 | `	}` |
|        5 |  8288 | `	if( pVm->xErrLog  ){` |
|        - |  8289 | `		/* Invoke the user callback */` |
|      ! 0 |  8290 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8291 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8292 | `		if( nArg > 1 ){` |
|      ! 0 |  8293 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8294 | `			if( nArg > 2 ){` |
|      ! 0 |  8295 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8296 | `				if( nArg > 3 ){` |
|      ! 0 |  8297 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8298 | `				}` |
|      ! 0 |  8299 | `			}` |
|      ! 0 |  8300 | `		}` |
|      ! 0 |  8301 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8302 | `	}` |
|        - |  8303 | `	/* Retun TRUE */` |
|        5 |  8304 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8305 | `	return PH7_OK;` |
|        3 |  8306 |  |
|        - |  8307 | `/*` |
|        - |  8308 | ` * bool restore_exception_handler(void)` |
|        - |  8309 | ` *  Restores the previously defined exception handler function.` |
|        - |  8310 | ` * Parameter` |
|        - |  8311 | ` *  None` |
|        - |  8312 | ` * Return` |
|        - |  8313 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8314 | ` */` |
|        4 |  8315 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8316 |  |
|        5 |  8317 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8318 | `	ph7_value *pOld,*pNew;` |
|        - |  8319 | `	/* Point to the old and the new handler */` |
|        5 |  8320 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8321 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8322 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8323 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8324 | `		SXUNUSED(apArg);` |
|        - |  8325 | `		/* No installed handler,return FALSE */` |
|        5 |  8326 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8327 | `		return PH7_OK;` |
|        - |  8328 | `	}` |
|        - |  8329 | `	/* Copy the old handler */` |
|      ! 0 |  8330 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8331 | `	PH7_MemObjRelease(pOld);` |
|        - |  8332 | `	/* Return TRUE */` |
|      ! 0 |  8333 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8334 | `	return PH7_OK;` |
|        3 |  8335 |  |
|        - |  8336 | `/*` |
|        - |  8337 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8338 | ` *  Sets a user-defined exception handler function.` |
|        - |  8339 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8340 | ` * NOTE` |
|        - |  8341 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8342 | ` *  the satndard PHP engine.` |
|        - |  8343 | ` * Parameters` |
|        - |  8344 | ` *  $exception_handler` |
|        - |  8345 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8346 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8347 | ` *   that was thrown.` |
|        - |  8348 | ` *  Note:` |
|        - |  8349 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8350 | ` * Return` |
|        - |  8351 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8352 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8353 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8354 | ` */` |
|        4 |  8355 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8356 |  |
|        6 |  8357 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8358 | `	ph7_value *pOld,*pNew;` |
|        - |  8359 | `	/* Point to the old and the new handler */` |
|        6 |  8360 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8361 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8362 | `	/* Return the old handler */` |
|        6 |  8363 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8364 | `	if( nArg > 0 ){` |
|        6 |  8365 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8366 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8367 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8368 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8369 | `		}else{` |
|        6 |  8370 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8371 | `			/* Install the new handler */` |
|        6 |  8372 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8373 | `		}` |
|        2 |  8374 | `	}` |
|        6 |  8375 | `	return PH7_OK;` |
|        2 |  8376 |  |
|        - |  8377 | `/*` |
|        - |  8378 | ` * bool restore_error_handler(void)` |
|        - |  8379 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8380 | ` * Parameters:` |
|        - |  8381 | ` *  None.` |
|        - |  8382 | ` * Return` |
|        - |  8383 | ` *  Always TRUE.` |
|        - |  8384 | ` */` |
|        4 |  8385 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8386 |  |
|        5 |  8387 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8388 | `	ph7_value *pOld,*pNew;` |
|        - |  8389 | `	/* Point to the old and the new handler */` |
|        5 |  8390 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8391 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8392 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8393 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8394 | `		SXUNUSED(apArg);` |
|        - |  8395 | `		/* No installed callback,return FALSE */` |
|        5 |  8396 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8397 | `		return PH7_OK;` |
|        - |  8398 | `	}` |
|        - |  8399 | `	/* Copy the old callback */` |
|      ! 0 |  8400 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8401 | `	PH7_MemObjRelease(pOld);` |
|        - |  8402 | `	/* Return TRUE */` |
|      ! 0 |  8403 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8404 | `	return PH7_OK;` |
|        3 |  8405 |  |
|        - |  8406 | `/*` |
|        - |  8407 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8408 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8409 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8410 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8411 | ` *  Sets a user-defined error handler function.` |
|        - |  8412 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8413 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8414 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8415 | ` *  conditions (using trigger_error()).` |
|        - |  8416 | ` * Parameters` |
|        - |  8417 | ` *  $error_handler` |
|        - |  8418 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8419 | ` *   describing the error.` |
|        - |  8420 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8421 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8422 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8423 | ` *   The function can be shown as:` |
|        - |  8424 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8425 | ` *     errno` |
|        - |  8426 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8427 | ` *   errstr` |
|        - |  8428 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8429 | ` *   errfile` |
|        - |  8430 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8431 | ` *     was raised in, as a string.` |
|        - |  8432 | ` *  Note:` |
|        - |  8433 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8434 | ` * Return` |
|        - |  8435 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8436 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8437 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8438 | ` */` |
|     8722 |  8439 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8440 |  |
|     8724 |  8441 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8442 | `	ph7_value *pOld,*pNew;` |
|        - |  8443 | `	/* Point to the old and the new handler */` |
|     8724 |  8444 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8445 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8446 | `	/* Return the old handler */` |
|     8724 |  8447 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8448 | `	if( nArg > 0 ){` |
|     8724 |  8449 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8450 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8451 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8452 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8453 | `		}else{` |
|     4364 |  8454 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8455 | `			/* Install the new handler */` |
|     4364 |  8456 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8457 | `		}` |
|     4361 |  8458 | `	}` |
|     8724 |  8459 | `	return PH7_OK;` |
|        2 |  8460 |  |
|        - |  8461 | `/*` |
|        - |  8462 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8463 | ` *  Generates a backtrace.` |
|        - |  8464 | ` * Paramaeter` |
|        - |  8465 | ` *  $options` |
|        - |  8466 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8467 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8468 | ` *   all the function/method arguments, to save memory.` |
|        - |  8469 | ` * $limit` |
|        - |  8470 | ` *   (Not Used)` |
|        - |  8471 | ` * Return` |
|        - |  8472 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8473 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8474 | ` *          Name        Type      Description` |
|        - |  8475 | ` *          ------      ------     -----------` |
|        - |  8476 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8477 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8478 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8479 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8480 | ` *          object      object    The current object.` |
|        - |  8481 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8482 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8483 | ` */` |
|      502 |  8484 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8485 |  |
|      504 |  8486 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8487 | `	ph7_value *pArray;` |
|        - |  8488 | `	ph7_class *pClass;` |
|        - |  8489 | `	ph7_value *pValue;` |
|        - |  8490 | `	SyString *pFile;` |
|        - |  8491 | `	/* Create a new array */` |
|      504 |  8492 | `	pArray = ph7_context_new_array(pCtx);` |
|      504 |  8493 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      504 |  8494 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8495 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8496 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8497 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8498 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8499 | `		SXUNUSED(apArg);` |
|      ! 0 |  8500 | `		return PH7_OK;` |
|        - |  8501 | `	}` |
|        - |  8502 | `	/* Dump running function name and it's arguments  */` |
|      504 |  8503 | `	if( pVm->pFrame->pParent ){` |
|      504 |  8504 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8505 | `		ph7_vm_func *pFunc;` |
|        - |  8506 | `		ph7_value *pArg;` |
|      504 |  8507 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8508 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8509 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8510 | `		}` |
|      504 |  8511 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      504 |  8512 | `		if( pFrame->pParent && pFunc ){` |
|      504 |  8513 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      504 |  8514 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      504 |  8515 | `			ph7_value_reset_string_cursor(pValue);` |
|      251 |  8516 | `		}` |
|        - |  8517 | `		/* Function arguments */` |
|      504 |  8518 | `		pArg = ph7_context_new_array(pCtx);` |
|      504 |  8519 | `		if( pArg  ){` |
|        - |  8520 | `			ph7_value *pObj;` |
|        - |  8521 | `			VmSlot *aSlot;` |
|        - |  8522 | `			sxu32 n;` |
|        - |  8523 | `			/* Start filling the array with the given arguments */` |
|      504 |  8524 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2002 |  8525 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1500 |  8526 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1500 |  8527 | `				if( pObj ){` |
|     1500 |  8528 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      749 |  8529 | `				}` |
|      751 |  8530 | `			}` |
|        - |  8531 | `			/* Save the array */` |
|      504 |  8532 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      251 |  8533 | `		}` |
|      251 |  8534 | `	}` |
|      504 |  8535 | `	ph7_value_int(pValue,1);` |
|        - |  8536 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8537 | `	 * line numbers at run-time. )` |
|        - |  8538 | `	 */` |
|      504 |  8539 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8540 | `	/* Current processed script */` |
|      504 |  8541 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      504 |  8542 | `	if( pFile ){` |
|      504 |  8543 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      504 |  8544 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      504 |  8545 | `		ph7_value_reset_string_cursor(pValue);` |
|      251 |  8546 | `	}` |
|        - |  8547 | `	/* Top class */` |
|      504 |  8548 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      504 |  8549 | `	if( pClass ){` |
|      500 |  8550 | `		ph7_value_reset_string_cursor(pValue);` |
|      500 |  8551 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      500 |  8552 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      249 |  8553 | `	}` |
|        - |  8554 | `	/* Return the freshly created array */` |
|      504 |  8555 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8556 | `	/*` |
|        - |  8557 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8558 | `	 * as soon we return from this function.` |
|        - |  8559 | `	 */` |
|      504 |  8560 | `	return PH7_OK;` |
|      253 |  8561 |  |
|        - |  8562 | `/*` |
|        - |  8563 | ` * Generate a small backtrace.` |
|        - |  8564 | ` * Store the generated dump in the given BLOB` |
|        - |  8565 | ` */` |
|        4 |  8566 | `static int VmMiniBacktrace(` |
|        - |  8567 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8568 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8569 | `	)` |
|        1 |  8570 |  |
|        5 |  8571 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8572 | `	ph7_vm_func *pFunc;` |
|        - |  8573 | `	ph7_class *pClass;` |
|        - |  8574 | `	SyString *pFile;` |
|        - |  8575 | `	/* Called function */` |
|        5 |  8576 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8577 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8578 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8579 | `	}` |
|        5 |  8580 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8581 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8582 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8583 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8584 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8585 | `	}else{` |
|      ! 0 |  8586 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8587 | `	}` |
|        5 |  8588 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8589 | `	/* Current processed script */` |
|        5 |  8590 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8591 | `	if( pFile ){` |
|        5 |  8592 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8593 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8594 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8595 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8596 | `	}` |
|        - |  8597 | `	/* Top class */` |
|        5 |  8598 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8599 | `	if( pClass ){` |
|      ! 0 |  8600 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8601 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8602 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8603 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8604 | `	}` |
|        5 |  8605 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8606 | `	/* All done */` |
|        5 |  8607 | `	return SXRET_OK;` |
|        1 |  8608 |  |
|        - |  8609 | `/*` |
|        - |  8610 | ` * void debug_print_backtrace()` |
|        - |  8611 | ` *  Prints a backtrace` |
|        - |  8612 | ` * Parameters` |
|        - |  8613 | ` * None` |
|        - |  8614 | ` * Return` |
|        - |  8615 | ` * NULL` |
|        - |  8616 | ` */` |
|        2 |  8617 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8618 |  |
|        3 |  8619 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8620 | `	SyBlob sDump;` |
|        3 |  8621 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8622 | `	/* Generate the backtrace */` |
|        3 |  8623 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8624 | `	/* Output backtrace */` |
|        3 |  8625 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8626 | `	/* All done,cleanup */` |
|        3 |  8627 | `	SyBlobRelease(&sDump);` |
|        1 |  8628 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8629 | `	SXUNUSED(apArg);` |
|        3 |  8630 | `	return PH7_OK;` |
|        1 |  8631 |  |
|        - |  8632 | `/*` |
|        - |  8633 | ` * string debug_string_backtrace()` |
|        - |  8634 | ` *  Generate a backtrace` |
|        - |  8635 | ` * Parameters` |
|        - |  8636 | ` * None` |
|        - |  8637 | ` * Return` |
|        - |  8638 | ` *  A mini backtrace().` |
|        - |  8639 | ` * Note that this is a symisc extension.` |
|        - |  8640 | ` */` |
|        2 |  8641 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8642 |  |
|        3 |  8643 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8644 | `	SyBlob sDump;` |
|        3 |  8645 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8646 | `	/* Generate the backtrace */` |
|        3 |  8647 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8648 | `	/* Return the backtrace */` |
|        3 |  8649 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8650 | `	/* All done,cleanup */` |
|        3 |  8651 | `	SyBlobRelease(&sDump);` |
|        1 |  8652 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8653 | `	SXUNUSED(apArg);` |
|        3 |  8654 | `	return PH7_OK;` |
|        1 |  8655 |  |
|        - |  8656 | `/*` |
|        - |  8657 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8658 | ` * exception is triggered.` |
|        - |  8659 | ` */` |
|      472 |  8660 | `static sxi32 VmUncaughtException(` |
|        - |  8661 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8662 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8663 | `	)` |
|        1 |  8664 |  |
|        - |  8665 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8666 | `	int nArg = 1;` |
|        - |  8667 | `	sxi32 rc;` |
|      473 |  8668 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8669 | `		/* Nesting limit reached */` |
|      ! 0 |  8670 | `		return SXRET_OK;` |
|        - |  8671 | `	}` |
|        - |  8672 | `	/* Call any exception handler if available */` |
|      473 |  8673 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8674 | `	if( pThis ){` |
|        - |  8675 | `		/* Load the exception instance */` |
|      473 |  8676 | `		sArg.x.pOther = pThis;` |
|      473 |  8677 | `		pThis->iRef++;` |
|      473 |  8678 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8679 | `	}else{` |
|      ! 0 |  8680 | `		nArg = 0;` |
|        - |  8681 | `	}` |
|      473 |  8682 | `	apArg[0] = &sArg;` |
|        - |  8683 | `	/* Call the exception handler if available */` |
|      473 |  8684 | `	pVm->nExceptDepth++;` |
|      473 |  8685 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8686 | `	pVm->nExceptDepth--;` |
|      473 |  8687 | `	if( rc != SXRET_OK ){` |
|        - |  8688 | `		SyBlob sMsgBuf;` |
|      471 |  8689 | `		const char *zClass = "Exception";` |
|      471 |  8690 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8691 | `		const char *zMsg;` |
|        - |  8692 | `		sxu32 nMsg;` |
|        - |  8693 | `		const char *zFuncName;` |
|        - |  8694 | `		int nFuncLen;` |
|      471 |  8695 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8696 | `		if( pThis ){` |
|        - |  8697 | `			ph7_class_method *pGetMessage;` |
|        - |  8698 | `			ph7_value sMsg;` |
|        - |  8699 | `			const char *zTmp;` |
|        - |  8700 | `			int nTmp;` |
|      471 |  8701 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8702 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8703 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8704 | `			if( pGetMessage ){` |
|      471 |  8705 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8706 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8707 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8708 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8709 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8710 | `					}` |
|      235 |  8711 | `				}` |
|      471 |  8712 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8713 | `			}` |
|      235 |  8714 | `		}` |
|      471 |  8715 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8716 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8717 | `		}` |
|      471 |  8718 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8719 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8720 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8721 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8722 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8723 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8724 | `		rc = SXERR_ABORT;` |
|      235 |  8725 | `	}` |
|      473 |  8726 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8727 | `	return rc;` |
|      237 |  8728 |  |
|        - |  8729 | `/*` |
|        - |  8730 | ` * Throw an user exception.` |
|        - |  8731 | ` */` |
|      506 |  8732 | `static sxi32 VmThrowException(` |
|        - |  8733 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8734 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8735 | `	)` |
|        2 |  8736 |  |
|        - |  8737 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8738 | `	ph7_exception **apException;` |
|        - |  8739 | `	ph7_exception *pException;` |
|        - |  8740 | `	/* Point to the stack of loaded exceptions */` |
|      508 |  8741 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      508 |  8742 | `	pException = 0;` |
|      508 |  8743 | `	pCatch = 0;` |
|      508 |  8744 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8745 | `		ph7_exception_block *aCatch;` |
|        - |  8746 | `		ph7_class *pClass;` |
|        - |  8747 | `		sxu32 j;` |
|        - |  8748 | `		/* Locate the appropriate block to execute */` |
|       32 |  8749 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       32 |  8750 | `		(void)SySetPop(&pVm->aException);` |
|       32 |  8751 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       32 |  8752 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       30 |  8753 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8754 | `			/* Extract the target class */` |
|       30 |  8755 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       30 |  8756 | `			if( pClass == 0 ){` |
|        - |  8757 | `				/* No such class */` |
|      ! 0 |  8758 | `				continue;` |
|        - |  8759 | `			}` |
|       30 |  8760 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8761 | `				/* Catch block found,break immeditaley */` |
|       30 |  8762 | `				pCatch = &aCatch[j];` |
|       30 |  8763 | `				break;` |
|        - |  8764 | `			}` |
|      ! 0 |  8765 | `		}` |
|       15 |  8766 | `	}` |
|        - |  8767 | `	/* Execute the cached block if available */` |
|      508 |  8768 | `	if( pCatch == 0 ){` |
|        - |  8769 | `		sxi32 rc;` |
|        - |  8770 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 |  8771 | `		if( pException && pException->iHasFinally ){` |
|        3 |  8772 | `			pException->iFinallyDone = 1;` |
|        3 |  8773 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 |  8774 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8775 | `				return SXERR_ABORT;` |
|        - |  8776 | `			}` |
|        1 |  8777 | `		}` |
|        - |  8778 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 |  8779 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8780 | `			/* Re-throw to the outer handler */` |
|        3 |  8781 | `			return VmThrowException(&(*pVm),pThis);` |
|        - |  8782 | `		}` |
|        - |  8783 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - |  8784 | `		 * (catch body re-throw with finally pending), defer the` |
|        - |  8785 | `		 * exception instead of reporting it uncaught.` |
|        - |  8786 | `		 */` |
|      478 |  8787 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - |  8788 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - |  8789 | `			 * by looking for a catch frame on the stack.` |
|        - |  8790 | `			 */` |
|      478 |  8791 | `			VmFrame *pF = pVm->pFrame;` |
|      478 |  8792 | `			int inCatch = 0;` |
|      956 |  8793 | `			while( pF ){` |
|      484 |  8794 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 |  8795 | `					inCatch = 1;` |
|        6 |  8796 | `					break;` |
|        - |  8797 | `				}` |
|      479 |  8798 | `				pF = pF->pParent;` |
|        1 |  8799 | `			}` |
|      478 |  8800 | `			if( inCatch ){` |
|        - |  8801 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 |  8802 | `				pThis->iRef++;` |
|        6 |  8803 | `				pVm->pPendingException = pThis;` |
|        6 |  8804 | `				return SXRET_OK;` |
|        - |  8805 | `			}` |
|      236 |  8806 | `		}` |
|        - |  8807 | `		/* Truly uncaught */` |
|      473 |  8808 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8809 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8810 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8811 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  8812 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8813 | `			}` |
|      ! 0 |  8814 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 |  8815 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8816 | `			}` |
|      ! 0 |  8817 | `		}` |
|      473 |  8818 | `		return rc;` |
|      ! 0 |  8819 | `	}else{` |
|       30 |  8820 | `		VmFrame *pFrame = pVm->pFrame;` |
|       30 |  8821 | `		ph7_exception **apSaved = 0;` |
|        - |  8822 | `		sxu32 nSavedCount;` |
|        - |  8823 | `		sxi32 rc;` |
|       58 |  8824 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|       30 |  8825 | `			pFrame = pFrame->pParent;` |
|        2 |  8826 | `		}` |
|       30 |  8827 | `		if( pException->pFrame == pFrame ){` |
|       22 |  8828 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       10 |  8829 | `		}` |
|        - |  8830 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - |  8831 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - |  8832 | `		 * our finally block. We save the stack contents and restore after.` |
|        - |  8833 | `		 */` |
|       30 |  8834 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       30 |  8835 | `		if( nSavedCount > 0 ){` |
|       11 |  8836 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 |  8837 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8838 | `			if( apSaved ){` |
|       11 |  8839 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 |  8840 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8841 | `				SySetReset(&pVm->aException);` |
|        3 |  8842 | `			}` |
|        3 |  8843 | `		}` |
|        - |  8844 | `		/* Create a private frame first */` |
|       30 |  8845 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       30 |  8846 | `		if( rc == SXRET_OK ){` |
|       30 |  8847 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       30 |  8848 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       30 |  8849 | `			if( pObj ){` |
|       30 |  8850 | `				pThis->iRef++;` |
|       30 |  8851 | `				pObj->x.pOther = pThis;` |
|       30 |  8852 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       14 |  8853 | `			}` |
|        - |  8854 | `			/* Execute the catch block */` |
|       30 |  8855 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8856 | `			/* Leave the frame */` |
|       30 |  8857 | `			VmLeaveFrame(&(*pVm));` |
|       14 |  8858 | `		}` |
|        - |  8859 | `		/* Restore the outer exception handlers */` |
|       30 |  8860 | `		if( apSaved ){` |
|        - |  8861 | `			sxu32 k;` |
|        - |  8862 | `			/* Any new entries pushed during catch execution (from nested` |
|        - |  8863 | `			 * try blocks inside the catch body) are already consumed.` |
|        - |  8864 | `			 * Restore the original outer entries.` |
|        - |  8865 | `			 */` |
|        8 |  8866 | `			SySetReset(&pVm->aException);` |
|       14 |  8867 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 |  8868 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 |  8869 | `			}` |
|        8 |  8870 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 |  8871 | `		}` |
|        - |  8872 | `		/* Execute the finally block after catch */` |
|       30 |  8873 | `		if( pException->iHasFinally ){` |
|        9 |  8874 | `			pException->iFinallyDone = 1;` |
|        - |  8875 | `			{` |
|        9 |  8876 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        9 |  8877 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 |  8878 | `					return SXERR_ABORT;` |
|        - |  8879 | `				}` |
|        - |  8880 | `			}` |
|        4 |  8881 | `		}` |
|       30 |  8882 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8883 | `			return SXERR_ABORT;` |
|        - |  8884 | `		}` |
|        - |  8885 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - |  8886 | `		 * pPendingException (because outer handlers were hidden).` |
|        - |  8887 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - |  8888 | `		 */` |
|       30 |  8889 | `		if( pVm->pPendingException ){` |
|        6 |  8890 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 |  8891 | `			pVm->pPendingException = 0;` |
|        6 |  8892 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - |  8893 | `		}` |
|        - |  8894 | `	}` |
|        - |  8895 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8896 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8897 | `	 */` |
|       26 |  8898 | `	return SXRET_OK;` |
|      255 |  8899 |  |
|        - |  8900 | `/*` |
|        - |  8901 | ` * Section:` |
|        - |  8902 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8903 | ` * Status:` |
|        - |  8904 | ` *    Stable.` |
|        - |  8905 | ` */` |
|        - |  8906 | `/*` |
|        - |  8907 | ` * string ph7version(void)` |
|        - |  8908 | ` *  Returns the running version of the PH7 version.` |
|        - |  8909 | ` * Parameters` |
|        - |  8910 | ` *  None` |
|        - |  8911 | ` * Return` |
|        - |  8912 | ` * Current PH7 version.` |
|        - |  8913 | ` */` |
|        2 |  8914 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8915 |  |
|        1 |  8916 | `	SXUNUSED(nArg);` |
|        1 |  8917 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8918 | `	/* Current engine version */` |
|        3 |  8919 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8920 | `	return PH7_OK;` |
|        1 |  8921 |  |
|        - |  8922 | `/*` |
|        - |  8923 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8924 | ` */` |
|        - |  8925 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8926 | ` "<html><head>"\` |
|        - |  8927 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8928 | ` "<style type=\"text/css\">"\` |
|        - |  8929 | ` "div {"\` |
|        - |  8930 | `     "border: 1px solid #cccccc;"\` |
|        - |  8931 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8932 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8933 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8934 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8935 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8936 | `     "-o-border-radius: 10px;"\` |
|        - |  8937 | `     "border-radius: 10px;"\` |
|        - |  8938 | `     "padding-left: 2em;"\` |
|        - |  8939 | `     "background-color: white;"\` |
|        - |  8940 | `     "margin-left: auto;"\` |
|        - |  8941 | `     "font-family: verdana;"\` |
|        - |  8942 | `     "padding-right: 2em;"\` |
|        - |  8943 | `     "margin-right: auto;"\` |
|        - |  8944 | `     "}"\` |
|        - |  8945 | `     "body {"\` |
|        - |  8946 | `     "padding: 0.2em;"\` |
|        - |  8947 | `     "font-style: normal;"\` |
|        - |  8948 | `     "font-size: medium;"\` |
|        - |  8949 | `     "background-color: #f2f2f2;"\` |
|        - |  8950 | `     "}"\` |
|        - |  8951 | `     "hr {"\` |
|        - |  8952 | `     "border-style: solid none none;"\` |
|        - |  8953 | `     "border-width: 1px medium medium;"\` |
|        - |  8954 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8955 | `     "height: 1px;"\` |
|        - |  8956 | `     "}"\` |
|        - |  8957 | `     "a {"\` |
|        - |  8958 | `     "color: #3366cc;"\` |
|        - |  8959 | `     "text-decoration: none;"\` |
|        - |  8960 | `     "}"\` |
|        - |  8961 | `     "a:hover {"\` |
|        - |  8962 | `     "color: #999999;"\` |
|        - |  8963 | `     "}"\` |
|        - |  8964 | `     "a:active {"\` |
|        - |  8965 | `     "color: #663399;"\` |
|        - |  8966 | `     "}"\` |
|        - |  8967 | `     "h1 {"\` |
|        - |  8968 | `     "margin: 0;"\` |
|        - |  8969 | `     "padding: 0;"\` |
|        - |  8970 | `     "font-family: Verdana;"\` |
|        - |  8971 | `     "font-weight: bold;"\` |
|        - |  8972 | `     "font-style: normal;"\` |
|        - |  8973 | `     "font-size: medium;"\` |
|        - |  8974 | `     "text-transform: capitalize;"\` |
|        - |  8975 | `     "color: #0a328c;"\` |
|        - |  8976 | `     "}"\` |
|        - |  8977 | `     "p {"\` |
|        - |  8978 | `     "margin: 0 auto;"\` |
|        - |  8979 | `     "font-size: medium;"\` |
|        - |  8980 | `     "font-style: normal;"\` |
|        - |  8981 | `     "font-family: verdana;"\` |
|        - |  8982 | `     "}"\` |
|        - |  8983 | `"</style></head><body>"\` |
|        - |  8984 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8985 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8986 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8987 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8988 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8989 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8990 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8991 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8992 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8993 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8994 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8995 |  |
|        - |  8996 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8997 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8998 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8999 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9000 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9001 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9002 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9003 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9004 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9005 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9006 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9007 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9008 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9009 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9010 |  |
|        - |  9011 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9012 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9013 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9014 | `"&nbsp;*<br>"\` |
|        - |  9015 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9016 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9017 | `"&nbsp;* are met:<br>"\` |
|        - |  9018 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9019 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9020 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9021 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9022 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9023 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9024 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9025 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9026 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9027 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9028 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9029 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9030 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9031 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9032 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9033 | `"&nbsp;*<br>"\` |
|        - |  9034 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9035 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9036 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9037 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9038 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9039 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9040 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9041 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9042 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9043 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9044 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9045 | `"&nbsp;*/<br>"\` |
|        - |  9046 | `"</span></small></small></p>"\` |
|        - |  9047 | `"</div></body></html>"` |
|        - |  9048 | `/*` |
|        - |  9049 | ` * bool ph7credits(void)` |
|        - |  9050 | ` * bool ph7info(void)` |
|        - |  9051 | ` * bool ph7copyright(void)` |
|        - |  9052 | ` *  Prints out the credits for PH7 engine` |
|        - |  9053 | ` * Parameters` |
|        - |  9054 | ` *  None` |
|        - |  9055 | ` * Return` |
|        - |  9056 | ` *  Always TRUE` |
|        - |  9057 | ` */` |
|        2 |  9058 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9059 |  |
|        3 |  9060 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9061 | `	/* Expand the HTML page above*/` |
|        3 |  9062 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9063 | `	ph7_context_output_format(` |
|        1 |  9064 | `		pCtx,` |
|        - |  9065 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9066 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9067 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9068 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9069 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9070 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9071 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9072 | `#ifdef __WINNT__` |
|        - |  9073 | `		"Windows NT"` |
|        - |  9074 | `#elif defined(__UNIXES__)` |
|        - |  9075 | `		"UNIX-Like"` |
|        - |  9076 | `#else` |
|        - |  9077 | `		"Other OS"` |
|        - |  9078 | `#endif` |
|        - |  9079 | `		);` |
|        3 |  9080 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9081 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9082 | `	SXUNUSED(apArg);` |
|        - |  9083 | `	/* Return TRUE */` |
|        - |  9084 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9085 | `	return PH7_OK;` |
|        1 |  9086 |  |
|        - |  9087 | `/*` |
|        - |  9088 | ` * Section:` |
|        - |  9089 | ` *    URL related routines.` |
|        - |  9090 | ` * Status:` |
|        - |  9091 | ` *    Stable.` |
|        - |  9092 | ` */` |
|        - |  9093 | `/*` |
|        - |  9094 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9095 | ` *  Parse a URL and return its fields.` |
|        - |  9096 | ` * Parameters` |
|        - |  9097 | ` *  $url` |
|        - |  9098 | ` *   The URL to parse.` |
|        - |  9099 | ` * $component` |
|        - |  9100 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9101 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9102 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9103 | ` *  in which case the return value will be an integer).` |
|        - |  9104 | ` * Return` |
|        - |  9105 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9106 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9107 | ` *  this array are:` |
|        - |  9108 | ` *   scheme - e.g. http` |
|        - |  9109 | ` *   host` |
|        - |  9110 | ` *   port` |
|        - |  9111 | ` *   user` |
|        - |  9112 | ` *   pass` |
|        - |  9113 | ` *   path` |
|        - |  9114 | ` *   query - after the question mark ?` |
|        - |  9115 | ` *   fragment - after the hashmark #` |
|        - |  9116 | ` * Note:` |
|        - |  9117 | ` *  FALSE is returned on failure.` |
|        - |  9118 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9119 | ` *  with the standard PHP engine.` |
|        - |  9120 | ` */` |
|       28 |  9121 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9122 |  |
|        - |  9123 | `	const char *zStr; /* Input string */` |
|        - |  9124 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9125 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9126 | `	int nLen;` |
|        - |  9127 | `	sxi32 rc;` |
|       29 |  9128 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9129 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9130 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9131 | `		return PH7_OK;` |
|        - |  9132 | `	}` |
|        - |  9133 | `	/* Extract the given URI */` |
|       29 |  9134 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9135 | `	if( nLen < 1 ){` |
|        - |  9136 | `		/* Nothing to process,return FALSE */` |
|        3 |  9137 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9138 | `		return PH7_OK;` |
|        - |  9139 | `	}` |
|        - |  9140 | `	/* Get a parse */` |
|       27 |  9141 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9142 | `	if( rc != SXRET_OK ){` |
|        - |  9143 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9144 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9145 | `		return PH7_OK;` |
|        - |  9146 | `	}` |
|       27 |  9147 | `	if( nArg > 1 ){` |
|      ! 0 |  9148 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9149 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9150 | `		switch(nComponent){` |
|      ! 0 |  9151 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9152 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9153 | `			if( pComp->nByte < 1 ){` |
|        - |  9154 | `				/* No available value,return NULL */` |
|      ! 0 |  9155 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9156 | `			}else{` |
|      ! 0 |  9157 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9158 | `			}` |
|      ! 0 |  9159 | `			break;` |
|      ! 0 |  9160 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9161 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9162 | `			if( pComp->nByte < 1 ){` |
|        - |  9163 | `				/* No available value,return NULL */` |
|      ! 0 |  9164 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9165 | `			}else{` |
|      ! 0 |  9166 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9167 | `			}` |
|      ! 0 |  9168 | `			break;` |
|      ! 0 |  9169 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9170 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9171 | `			if( pComp->nByte < 1 ){` |
|        - |  9172 | `				/* No available value,return NULL */` |
|      ! 0 |  9173 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9174 | `			}else{` |
|      ! 0 |  9175 | `				int iPort = 0;` |
|        - |  9176 | `				/* Cast the value to integer */` |
|      ! 0 |  9177 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9178 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9179 | `			}` |
|      ! 0 |  9180 | `			break;` |
|      ! 0 |  9181 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9182 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9183 | `			if( pComp->nByte < 1 ){` |
|        - |  9184 | `				/* No available value,return NULL */` |
|      ! 0 |  9185 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9186 | `			}else{` |
|      ! 0 |  9187 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9188 | `			}` |
|      ! 0 |  9189 | `			break;` |
|      ! 0 |  9190 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9191 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9192 | `			if( pComp->nByte < 1 ){` |
|        - |  9193 | `				/* No available value,return NULL */` |
|      ! 0 |  9194 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9195 | `			}else{` |
|      ! 0 |  9196 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9197 | `			}` |
|      ! 0 |  9198 | `			break;` |
|      ! 0 |  9199 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9200 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9201 | `			if( pComp->nByte < 1 ){` |
|        - |  9202 | `				/* No available value,return NULL */` |
|      ! 0 |  9203 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9204 | `			}else{` |
|      ! 0 |  9205 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9206 | `			}` |
|      ! 0 |  9207 | `			break;` |
|      ! 0 |  9208 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9209 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9210 | `			if( pComp->nByte < 1 ){` |
|        - |  9211 | `				/* No available value,return NULL */` |
|      ! 0 |  9212 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9213 | `			}else{` |
|      ! 0 |  9214 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9215 | `			}` |
|      ! 0 |  9216 | `			break;` |
|      ! 0 |  9217 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9218 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9219 | `			if( pComp->nByte < 1 ){` |
|        - |  9220 | `				/* No available value,return NULL */` |
|      ! 0 |  9221 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9222 | `			}else{` |
|      ! 0 |  9223 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9224 | `			}` |
|      ! 0 |  9225 | `			break;` |
|      ! 0 |  9226 | `		default:` |
|        - |  9227 | `			/* No such entry,return NULL */` |
|      ! 0 |  9228 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9229 | `			break;` |
|        - |  9230 | `		}` |
|      ! 0 |  9231 | `	}else{` |
|        - |  9232 | `		ph7_value *pArray,*pValue;` |
|        - |  9233 | `		/* Return an associative array */` |
|       27 |  9234 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9235 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9236 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9237 | `			/* Out of memory */` |
|      ! 0 |  9238 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9239 | `			/* Return false */` |
|      ! 0 |  9240 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9241 | `			return PH7_OK;` |
|        - |  9242 | `		}` |
|        - |  9243 | `		/* Fill the array */` |
|       27 |  9244 | `		pComp = &sURI.sScheme;` |
|       27 |  9245 | `		if( pComp->nByte > 0 ){` |
|       19 |  9246 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9247 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9248 | `		}` |
|        - |  9249 | `		/* Reset the string cursor */` |
|       27 |  9250 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9251 | `		pComp = &sURI.sHost;` |
|       27 |  9252 | `		if( pComp->nByte > 0 ){` |
|       25 |  9253 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9254 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9255 | `		}` |
|        - |  9256 | `		/* Reset the string cursor */` |
|       27 |  9257 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9258 | `		pComp = &sURI.sPort;` |
|       27 |  9259 | `		if( pComp->nByte > 0 ){` |
|       11 |  9260 | `			int iPort = 0;/* cc warning */` |
|        - |  9261 | `			/* Convert to integer */` |
|       11 |  9262 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9263 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9264 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9265 | `		}` |
|        - |  9266 | `		/* Reset the string cursor */` |
|       27 |  9267 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9268 | `		pComp = &sURI.sUser;` |
|       27 |  9269 | `		if( pComp->nByte > 0 ){` |
|        7 |  9270 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9271 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9272 | `		}` |
|        - |  9273 | `		/* Reset the string cursor */` |
|       27 |  9274 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9275 | `		pComp = &sURI.sPass;` |
|       27 |  9276 | `		if( pComp->nByte > 0 ){` |
|        7 |  9277 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9278 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9279 | `		}` |
|        - |  9280 | `		/* Reset the string cursor */` |
|       27 |  9281 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9282 | `		pComp = &sURI.sPath;` |
|       27 |  9283 | `		if( pComp->nByte > 0 ){` |
|       17 |  9284 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9285 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9286 | `		}` |
|        - |  9287 | `		/* Reset the string cursor */` |
|       27 |  9288 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9289 | `		pComp = &sURI.sQuery;` |
|       27 |  9290 | `		if( pComp->nByte > 0 ){` |
|        5 |  9291 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9292 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9293 | `		}` |
|        - |  9294 | `		/* Reset the string cursor */` |
|       27 |  9295 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9296 | `		pComp = &sURI.sFragment;` |
|       27 |  9297 | `		if( pComp->nByte > 0 ){` |
|        5 |  9298 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9299 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9300 | `		}` |
|        - |  9301 | `		/* Return the created array */` |
|       27 |  9302 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9303 | `		/* NOTE:` |
|        - |  9304 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9305 | `		 * automatically as soon we return from this function.` |
|        - |  9306 | `		 */` |
|        - |  9307 | `	}` |
|        - |  9308 | `	/* All done */` |
|       27 |  9309 | `	return PH7_OK;` |
|       15 |  9310 |  |
|        - |  9311 | `/*` |
|        - |  9312 | ` * Section:` |
|        - |  9313 | ` *   Array related routines.` |
|        - |  9314 | ` * Status:` |
|        - |  9315 | ` *    Stable.` |
|        - |  9316 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9317 | ` *  Array related functions that need access to the underlying` |
|        - |  9318 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9319 | ` */` |
|        - |  9320 | `/*` |
|        - |  9321 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9322 | ` * of the following structure.` |
|        - |  9323 | ` */` |
|        - |  9324 | `struct compact_data` |
|        - |  9325 |  |
|        - |  9326 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9327 | `	int nRecCount;      /* Recursion count */` |
|        - |  9328 | `};` |
|        - |  9329 | `/*` |
|        - |  9330 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9331 | ` */` |
|      ! 0 |  9332 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9333 |  |
|      ! 0 |  9334 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9335 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9336 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9337 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9338 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9339 | `		SyString sVar;` |
|      ! 0 |  9340 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9341 | `		if( sVar.nByte > 0 ){` |
|        - |  9342 | `			/* Query the current frame */` |
|      ! 0 |  9343 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9344 | `			/* ^` |
|        - |  9345 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9346 | `			 */` |
|      ! 0 |  9347 | `			if( pKey ){` |
|        - |  9348 | `				/* Perform the insertion */` |
|      ! 0 |  9349 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9350 | `			}` |
|      ! 0 |  9351 | `		}` |
|      ! 0 |  9352 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9353 | `		int rc;` |
|        - |  9354 | `		/* Recursively traverse this array */` |
|      ! 0 |  9355 | `		pData->nRecCount++;` |
|      ! 0 |  9356 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9357 | `		pData->nRecCount--;` |
|      ! 0 |  9358 | `		return rc;` |
|        - |  9359 | `	}` |
|      ! 0 |  9360 | `	return SXRET_OK;` |
|      ! 0 |  9361 |  |
|        - |  9362 | `/*` |
|        - |  9363 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9364 | ` *  Create array containing variables and their values.` |
|        - |  9365 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9366 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9367 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9368 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9369 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9370 | ` * Parameters` |
|        - |  9371 | ` *  $varname` |
|        - |  9372 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9373 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9374 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9375 | ` *   it recursively.` |
|        - |  9376 | ` * Return` |
|        - |  9377 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9378 | ` */` |
|        2 |  9379 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9380 |  |
|        - |  9381 | `	ph7_value *pArray,*pObj;` |
|        3 |  9382 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9383 | `	const char *zName;` |
|        - |  9384 | `	SyString sVar;` |
|        - |  9385 | `	int i,nLen;` |
|        3 |  9386 | `	if( nArg < 1 ){` |
|        - |  9387 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9388 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9389 | `		return PH7_OK;` |
|        - |  9390 | `	}` |
|        - |  9391 | `	/* Create the array */` |
|        3 |  9392 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9393 | `	if( pArray == 0 ){` |
|        - |  9394 | `		/* Out of memory */` |
|      ! 0 |  9395 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9396 | `		/* Return NULL */` |
|      ! 0 |  9397 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9398 | `		return PH7_OK;` |
|        - |  9399 | `	}` |
|        - |  9400 | `	/* Perform the requested operation */` |
|        7 |  9401 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9402 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9403 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9404 | `				struct compact_data sData;` |
|      ! 0 |  9405 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9406 | `				/* Recursively walk the array */` |
|      ! 0 |  9407 | `				sData.nRecCount = 0;` |
|      ! 0 |  9408 | `				sData.pArray = pArray;` |
|      ! 0 |  9409 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9410 | `			}` |
|      ! 0 |  9411 | `		}else{` |
|        - |  9412 | `			/* Extract variable name */` |
|        5 |  9413 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9414 | `			if( nLen > 0 ){` |
|        5 |  9415 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9416 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9417 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9418 | `				if( pObj ){` |
|        5 |  9419 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9420 | `				}` |
|        2 |  9421 | `			}` |
|        - |  9422 | `		}` |
|        3 |  9423 | `	}` |
|        - |  9424 | `	/* Return the array */` |
|        3 |  9425 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9426 | `	return PH7_OK;` |
|        2 |  9427 |  |
|        - |  9428 | `/*` |
|        - |  9429 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9430 | ` * of the following structure.` |
|        - |  9431 | ` */` |
|        - |  9432 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9433 | `struct extract_aux_data` |
|        - |  9434 |  |
|        - |  9435 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9436 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9437 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9438 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9439 | `	int iFlags;           /* Control flags */` |
|        - |  9440 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9441 | `};` |
|        - |  9442 | `/* Forward declaration */` |
|        - |  9443 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9444 | `/*` |
|        - |  9445 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9446 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9447 | ` * Parameters` |
|        - |  9448 | ` * $var_array` |
|        - |  9449 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9450 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9451 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9452 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9453 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9454 | ` * $extract_type` |
|        - |  9455 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9456 | ` *  It can be one of the following values:` |
|        - |  9457 | ` *   EXTR_OVERWRITE` |
|        - |  9458 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9459 | ` *   EXTR_SKIP` |
|        - |  9460 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9461 | ` *   EXTR_PREFIX_SAME` |
|        - |  9462 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9463 | ` *   EXTR_PREFIX_ALL` |
|        - |  9464 | ` *       Prefix all variable names with prefix.` |
|        - |  9465 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9466 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9467 | ` *   EXTR_IF_EXISTS` |
|        - |  9468 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9469 | ` *       otherwise do nothing.` |
|        - |  9470 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9471 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9472 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9473 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9474 | ` *      the current symbol table.` |
|        - |  9475 | ` * $prefix` |
|        - |  9476 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9477 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9478 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9479 | ` *  underscore character.` |
|        - |  9480 | ` * Return` |
|        - |  9481 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9482 | ` */` |
|        4 |  9483 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9484 |  |
|        - |  9485 | `	extract_aux_data sAux;` |
|        - |  9486 | `	ph7_hashmap *pMap;` |
|        5 |  9487 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9488 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9489 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9490 | `		return PH7_OK;` |
|        - |  9491 | `	}` |
|        - |  9492 | `	/* Point to the target hashmap */` |
|        5 |  9493 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9494 | `	if( pMap->nEntry < 1 ){` |
|        - |  9495 | `		/* Empty map,return  0 */` |
|      ! 0 |  9496 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9497 | `		return PH7_OK;` |
|        - |  9498 | `	}` |
|        - |  9499 | `	/* Prepare the aux data */` |
|        5 |  9500 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9501 | `	if( nArg > 1 ){` |
|        3 |  9502 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9503 | `		if( nArg > 2 ){` |
|      ! 0 |  9504 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9505 | `		}` |
|        1 |  9506 | `	}` |
|        5 |  9507 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9508 | `	/* Invoke the worker callback */` |
|        5 |  9509 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9510 | `	/* Number of variables successfully imported */` |
|        5 |  9511 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9512 | `	return PH7_OK;` |
|        3 |  9513 |  |
|        - |  9514 | `/*` |
|        - |  9515 | ` * Worker callback for the [extract()] function defined` |
|        - |  9516 | ` * below.` |
|        - |  9517 | ` */` |
|        8 |  9518 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9519 |  |
|        9 |  9520 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9521 | `	int iFlags = pAux->iFlags;` |
|        9 |  9522 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9523 | `	ph7_value *pObj;` |
|        - |  9524 | `	SyString sVar;` |
|        9 |  9525 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9526 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9527 | `	}` |
|        - |  9528 | `	/* Perform a string cast */` |
|        9 |  9529 | `	PH7_MemObjToString(pKey);` |
|        9 |  9530 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9531 | `		/* Unavailable variable name */` |
|      ! 0 |  9532 | `		return SXRET_OK;` |
|        - |  9533 | `	}` |
|        9 |  9534 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9535 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9536 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9537 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9538 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9539 | `			);` |
|      ! 0 |  9540 | `	}else{` |
|       13 |  9541 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9542 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9543 | `	}` |
|        9 |  9544 | `	sVar.zString = pAux->zWorker;` |
|        - |  9545 | `	/* Try to extract the variable */` |
|        9 |  9546 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9547 | `	if( pObj ){` |
|        - |  9548 | `		/* Collision */` |
|        5 |  9549 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9550 | `			return SXRET_OK;` |
|        - |  9551 | `		}` |
|        5 |  9552 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9553 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9554 | `				/* Already prefixed */` |
|      ! 0 |  9555 | `				return SXRET_OK;` |
|        - |  9556 | `			}` |
|      ! 0 |  9557 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9558 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9559 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9560 | `				);` |
|      ! 0 |  9561 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9562 | `		}` |
|        3 |  9563 | `	}else{` |
|        - |  9564 | `		/* Create the variable */` |
|        5 |  9565 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9566 | `	}` |
|        9 |  9567 | `	if( pObj ){` |
|        - |  9568 | `		/* Overwrite the old value */` |
|        9 |  9569 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9570 | `		/* Increment counter */` |
|        9 |  9571 | `		pAux->iCount++;` |
|        4 |  9572 | `	}` |
|        9 |  9573 | `	return SXRET_OK;` |
|        5 |  9574 |  |
|        - |  9575 | `/*` |
|        - |  9576 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9577 | ` * defined below.` |
|        - |  9578 | ` */` |
|        2 |  9579 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9580 |  |
|        3 |  9581 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9582 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9583 | `	ph7_value *pObj;` |
|        - |  9584 | `	SyString sVar;` |
|        - |  9585 | `	/* Perform a string cast */` |
|        3 |  9586 | `	PH7_MemObjToString(pKey);` |
|        3 |  9587 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9588 | `		/* Unavailable variable name */` |
|      ! 0 |  9589 | `		return SXRET_OK;` |
|        - |  9590 | `	}` |
|        3 |  9591 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9592 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9593 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9594 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9595 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9596 | `			);` |
|        2 |  9597 | `	}else{` |
|      ! 0 |  9598 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9599 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9600 | `	}` |
|        3 |  9601 | `	sVar.zString = pAux->zWorker;` |
|        - |  9602 | `	/* Extract the variable */` |
|        3 |  9603 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9604 | `	if( pObj ){` |
|        3 |  9605 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9606 | `	}` |
|        3 |  9607 | `	return SXRET_OK;` |
|        2 |  9608 |  |
|        - |  9609 | `/*` |
|        - |  9610 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9611 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9612 | ` * Parameters` |
|        - |  9613 | ` * $types` |
|        - |  9614 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9615 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9616 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9617 | ` *  POST includes the POST uploaded file information.` |
|        - |  9618 | ` *  Note:` |
|        - |  9619 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9620 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9621 | ` * $prefix` |
|        - |  9622 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9623 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9624 | ` *  variable named $pref_userid.` |
|        - |  9625 | ` * Return` |
|        - |  9626 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9627 | ` */` |
|        2 |  9628 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9629 |  |
|        - |  9630 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9631 | `	extract_aux_data sAux;` |
|        - |  9632 | `	int nLen,nPrefixLen;` |
|        - |  9633 | `	ph7_value *pSuper;` |
|        - |  9634 | `	ph7_vm *pVm;` |
|        - |  9635 | `	/* By default import only $_GET variables  */` |
|        3 |  9636 | `	zImport = "G";` |
|        3 |  9637 | `	nLen = (int)sizeof(char);` |
|        3 |  9638 | `	zPrefix = 0;` |
|        3 |  9639 | `	nPrefixLen = 0;` |
|        3 |  9640 | `	if( nArg > 0 ){` |
|        3 |  9641 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9642 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9643 | `		}` |
|        3 |  9644 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9645 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9646 | `		}` |
|        1 |  9647 | `	}` |
|        - |  9648 | `	/* Point to the underlying VM */` |
|        3 |  9649 | `	pVm = pCtx->pVm;` |
|        - |  9650 | `	/* Initialize the aux data */` |
|        3 |  9651 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9652 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9653 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9654 | `	sAux.pVm = pVm;` |
|        - |  9655 | `	/* Extract */` |
|        3 |  9656 | `	zEnd = &zImport[nLen];` |
|        5 |  9657 | `	while( zImport < zEnd ){` |
|        3 |  9658 | `		int c = zImport[0];` |
|        3 |  9659 | `		pSuper = 0;` |
|        3 |  9660 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9661 | `			/* Import $_GET variables */` |
|        3 |  9662 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9663 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9664 | `			/* Import $_POST variables */` |
|      ! 0 |  9665 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9666 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9667 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9668 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9669 | `		}` |
|        3 |  9670 | `		if( pSuper ){` |
|        - |  9671 | `			/* Iterate throw array entries */` |
|        3 |  9672 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9673 | `		}` |
|        - |  9674 | `		/* Advance the cursor */` |
|        3 |  9675 | `		zImport++;` |
|        1 |  9676 | `	}` |
|        - |  9677 | `	/* All done,return TRUE*/` |
|        3 |  9678 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9679 | `	return PH7_OK;` |
|        1 |  9680 |  |
|        - |  9681 | `/*` |
|        - |  9682 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9683 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9684 | ` * information.` |
|        - |  9685 | ` */` |
|     9982 |  9686 | `static sxi32 VmEvalChunk(` |
|        - |  9687 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9688 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9689 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9690 | `	int iFlags,         /* Compile flag */` |
|        - |  9691 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9692 | `	)` |
|        2 |  9693 |  |
|        - |  9694 | `	SySet *pByteCode,aByteCode;` |
|        - |  9695 | `	SyBlob sSavedNs;` |
|     9984 |  9696 | `	ProcConsumer xErr = 0;` |
|     9984 |  9697 | `	void *pErrData = 0;` |
|        - |  9698 | `	/* Initialize bytecode container */` |
|     9984 |  9699 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9984 |  9700 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9701 | `	/* Reset the code generator */` |
|     9984 |  9702 | `	if( bTrueReturn ){` |
|        - |  9703 | `		/* Included file,log compile-time errors */` |
|     7535 |  9704 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7535 |  9705 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3767 |  9706 | `	}` |
|     9984 |  9707 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9708 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |  9709 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |  9710 | `	 * the caller's namespace is restored. */` |
|     9984 |  9711 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|     9984 |  9712 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|     9984 |  9713 | `	if( bTrueReturn ){` |
|        - |  9714 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7535 |  9715 | `		SyBlobReset(&pVm->sNamespace);` |
|     3767 |  9716 | `	}` |
|        - |  9717 | `	/* Swap bytecode container */` |
|     9984 |  9718 | `	pByteCode = pVm->pByteContainer;` |
|     9984 |  9719 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9720 | `	/* Compile the chunk */` |
|     9984 |  9721 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14975 |  9722 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9723 | `		/* Compilation error,return false */` |
|        3 |  9724 | `		if( pCtx ){` |
|        3 |  9725 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9726 | `		}` |
|        2 |  9727 | `	}else{` |
|        - |  9728 | `		/* Mount any newly defined classes */` |
|        - |  9729 | `		SyHashEntry *pEntry;` |
|        - |  9730 | `		ph7_class *pClass;` |
|        - |  9731 | `		ph7_value sResult; /* Return value */` |
|        - |  9732 | `		sxi32 rc;` |
|     9982 |  9733 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   274752 |  9734 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   259782 |  9735 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9736 | `			/* Only mount classes that haven't been mounted yet */` |
|   259782 |  9737 | `			if( !pClass->bMounted ){` |
|    61300 |  9738 | `				rc = VmMountUserClass(pVm,pClass);` |
|    61300 |  9739 | `				if( rc != SXRET_OK ){` |
|        - |  9740 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9741 | `					if( pCtx ){` |
|      ! 0 |  9742 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9743 | `					}` |
|      ! 0 |  9744 | `					goto Cleanup;` |
|        - |  9745 | `				}` |
|    30649 |  9746 | `			}` |
|        2 |  9747 | `		}` |
|     9982 |  9748 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9749 | `			/* Out of memory */` |
|      ! 0 |  9750 | `			if( pCtx ){` |
|      ! 0 |  9751 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9752 | `			}` |
|      ! 0 |  9753 | `			goto Cleanup;` |
|        - |  9754 | `		}` |
|     9982 |  9755 | `		if( bTrueReturn ){` |
|        - |  9756 | `			/* Assume a boolean true return value */` |
|     7535 |  9757 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3768 |  9758 | `		}else{` |
|        - |  9759 | `			/* Assume a null return value */` |
|     2448 |  9760 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9761 | `		}` |
|        - |  9762 | `		/* Execute the compiled chunk */` |
|     9982 |  9763 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9982 |  9764 | `		if( pCtx ){` |
|        - |  9765 | `			/* Set the execution result */` |
|     7548 |  9766 | `			ph7_result_value(pCtx,&sResult);` |
|     3773 |  9767 | `		}` |
|     9982 |  9768 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9769 | `	}` |
|     4991 |  9770 | `Cleanup:` |
|        - |  9771 | `	/* Cleanup the mess left behind */` |
|     9984 |  9772 | `	pVm->pByteContainer = pByteCode;` |
|     9984 |  9773 | `	SySetRelease(&aByteCode);` |
|        - |  9774 | `	/* Restore caller's namespace state */` |
|     9984 |  9775 | `	SyBlobReset(&pVm->sNamespace);` |
|     9984 |  9776 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|     9984 |  9777 | `	SyBlobRelease(&sSavedNs);` |
|     9984 |  9778 | `	return SXRET_OK;` |
|        2 |  9779 |  |
|        - |  9780 | `/*` |
|        - |  9781 | ` * value eval(string $code)` |
|        - |  9782 | ` *   Evaluate a string as PHP code.` |
|        - |  9783 | ` * Parameter` |
|        - |  9784 | ` *  code: PHP code to evaluate.` |
|        - |  9785 | ` * Return` |
|        - |  9786 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9787 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9788 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9789 | ` */` |
|       16 |  9790 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9791 |  |
|        - |  9792 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9793 | `	if( nArg < 1 ){` |
|        - |  9794 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9795 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9796 | `		return SXRET_OK;` |
|        - |  9797 | `	}` |
|        - |  9798 | `	/* Chunk to evaluate */` |
|       18 |  9799 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9800 | `	if( sChunk.nByte < 1 ){` |
|        - |  9801 | `		/* Empty string,return NULL */` |
|        3 |  9802 | `		ph7_result_null(pCtx);` |
|        3 |  9803 | `		return SXRET_OK;` |
|        - |  9804 | `	}` |
|        - |  9805 | `	/* Eval the chunk */` |
|       16 |  9806 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9807 | `	return SXRET_OK;` |
|       10 |  9808 |  |
|        - |  9809 | `/*` |
|        - |  9810 | ` * Check if a file path is already included.` |
|        - |  9811 | ` */` |
|    15064 |  9812 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9813 |  |
|        - |  9814 | `	SyString *aEntries;` |
|        - |  9815 | `	sxu32 n;` |
|    15065 |  9816 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9817 | `	/* Perform a linear search */` |
| 56720651 |  9818 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56705593 |  9819 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9820 | `			/* Already included */` |
|        7 |  9821 | `			return TRUE;` |
|        - |  9822 | `		}` |
| 28352794 |  9823 | `	}` |
|    15059 |  9824 | `	return FALSE;` |
|     7533 |  9825 |  |
|        - |  9826 | `/*` |
|        - |  9827 | ` * Push a file path in the appropriate VM container.` |
|        - |  9828 | ` */` |
|    17490 |  9829 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9830 |  |
|        - |  9831 | `	SyString sPath;` |
|        - |  9832 | `	char *zDup;` |
|        - |  9833 | `#ifdef __WINNT__` |
|        - |  9834 | `	char *zCur;` |
|        - |  9835 | `#endif` |
|        - |  9836 | `	sxi32 rc;` |
|    17492 |  9837 | `	if( nLen < 0 ){` |
|     2428 |  9838 | `		nLen = SyStrlen(zPath);` |
|     1213 |  9839 | `	}` |
|        - |  9840 | `	/* Duplicate the file path first */` |
|    17492 |  9841 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17492 |  9842 | `	if( zDup == 0 ){` |
|      ! 0 |  9843 | `		return SXERR_MEM;` |
|        - |  9844 | `	}` |
|        - |  9845 | `#ifdef __WINNT__` |
|        - |  9846 | `	/* Normalize path on windows` |
|        - |  9847 | `	 * Example:` |
|        - |  9848 | `	 *    Path/To/File.php` |
|        - |  9849 | `	 * becomes` |
|        - |  9850 | `	 *   path\to\file.php` |
|        - |  9851 | `	 */` |
|        2 |  9852 | `	zCur = zDup;` |
|        2 |  9853 | `	while( zCur[0] != 0 ){` |
|        2 |  9854 | `		if( zCur[0] == '/' ){` |
|        2 |  9855 | `			zCur[0] = '\\';` |
|        2 |  9856 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9857 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9858 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9859 | `		}` |
|        2 |  9860 | `		zCur++;` |
|        2 |  9861 | `	}` |
|        - |  9862 | `#endif` |
|        - |  9863 | `	/* Install the file path */` |
|    17492 |  9864 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17492 |  9865 | `	if( !bMain ){` |
|    15065 |  9866 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9867 | `			/* Already included */` |
|        7 |  9868 | `			*pNew = 0;` |
|        4 |  9869 | `		}else{` |
|        - |  9870 | `			/* Insert in the corresponding container */` |
|    15059 |  9871 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15059 |  9872 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9873 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9874 | `				return rc;` |
|        - |  9875 | `			}` |
|    15059 |  9876 | `			*pNew = 1;` |
|        - |  9877 | `		}` |
|     7532 |  9878 | `	}` |
|    17492 |  9879 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17492 |  9880 | `	return SXRET_OK;` |
|     8747 |  9881 |  |
|        - |  9882 | `/*` |
|        - |  9883 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9884 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9885 | ` * indicates failure.` |
|        - |  9886 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9887 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9888 | ` * operations.` |
|        - |  9889 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9890 | ` * this function is a no-op.` |
|        - |  9891 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9892 | ` * constructs for more information.` |
|        - |  9893 | ` */` |
|     7540 |  9894 | `static sxi32 VmExecIncludedFile(` |
|        - |  9895 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9896 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9897 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9898 | `	 )` |
|        2 |  9899 |  |
|        - |  9900 | `	sxi32 rc;` |
|        - |  9901 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9902 | `	const ph7_io_stream *pStream;` |
|        - |  9903 | `	SyBlob sContents;` |
|        - |  9904 | `	void *pHandle;` |
|        - |  9905 | `	ph7_vm *pVm;` |
|        - |  9906 | `	int isNew;` |
|        - |  9907 | `	/* Initialize fields */` |
|     7542 |  9908 | `	pVm = pCtx->pVm;` |
|     7542 |  9909 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7542 |  9910 | `	isNew = 0;` |
|        - |  9911 | `	/* Extract the associated stream */` |
|     7542 |  9912 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9913 | `	/*` |
|        - |  9914 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9915 | `	 * in a read-only mode.` |
|        - |  9916 | `	 */` |
|     7542 |  9917 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7542 |  9918 | `	if( pHandle == 0 ){` |
|        3 |  9919 | `		return SXERR_IO;` |
|        - |  9920 | `	}` |
|     7539 |  9921 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7539 |  9922 | `	if( IncludeOnce && !isNew ){` |
|        - |  9923 | `		/* Already included */` |
|        5 |  9924 | `		rc = SXERR_EXISTS;` |
|        3 |  9925 | `	}else{` |
|        - |  9926 | `		/* Read the whole file contents */` |
|     7535 |  9927 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7535 |  9928 | `		if( rc == SXRET_OK ){` |
|        - |  9929 | `			SyString sScript;` |
|        - |  9930 | `			/* Compile and execute the script */` |
|     7535 |  9931 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7535 |  9932 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3767 |  9933 | `		}` |
|        - |  9934 | `	}` |
|        - |  9935 | `	/* Pop from the set of included file */` |
|     7539 |  9936 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9937 | `	/* Close the handle */` |
|     7539 |  9938 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9939 | `	/* Release the working buffer */` |
|     7539 |  9940 | `	SyBlobRelease(&sContents);` |
|        - |  9941 | `#else` |
|        - |  9942 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9943 | `	SXUNUSED(pPath);` |
|        - |  9944 | `	SXUNUSED(IncludeOnce);` |
|        - |  9945 | `	rc = SXERR_IO;` |
|        - |  9946 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7539 |  9947 | `	return rc;` |
|     3772 |  9948 |  |
|        - |  9949 | `/*` |
|        - |  9950 | ` * string get_include_path(void)` |
|        - |  9951 | ` *  Gets the current include_path configuration option.` |
|        - |  9952 | ` * Parameter` |
|        - |  9953 | ` *  None` |
|        - |  9954 | ` * Return` |
|        - |  9955 | ` *  Included paths as a string` |
|        - |  9956 | ` */` |
|        2 |  9957 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9958 |  |
|        3 |  9959 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9960 | `	SyString *aEntry;` |
|        - |  9961 | `	int dir_sep;` |
|        - |  9962 | `	sxu32 n;` |
|        - |  9963 | `#ifdef __WINNT__` |
|        1 |  9964 | `	dir_sep = ';';` |
|        - |  9965 | `#else` |
|        - |  9966 | `	/* Assume UNIX path separator */` |
|        2 |  9967 | `	dir_sep = ':';` |
|        - |  9968 | `#endif` |
|        1 |  9969 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9970 | `	SXUNUSED(apArg);` |
|        - |  9971 | `	/* Point to the list of import paths */` |
|        3 |  9972 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9973 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9974 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9975 | `		if( n > 0 ){` |
|        - |  9976 | `			/* Append dir seprator */` |
|      ! 0 |  9977 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9978 | `		}` |
|        - |  9979 | `		/* Append path */` |
|        3 |  9980 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9981 | `	}` |
|        3 |  9982 | `	return PH7_OK;` |
|        1 |  9983 |  |
|        - |  9984 | `/*` |
|        - |  9985 | ` * string get_get_included_files(void)` |
|        - |  9986 | ` *  Gets the current include_path configuration option.` |
|        - |  9987 | ` * Parameter` |
|        - |  9988 | ` *  None` |
|        - |  9989 | ` * Return` |
|        - |  9990 | ` *  Included paths as a string` |
|        - |  9991 | ` */` |
|        2 |  9992 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9993 |  |
|        3 |  9994 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9995 | `	ph7_value *pArray,*pWorker;` |
|        - |  9996 | `	SyString *pEntry;` |
|        - |  9997 | `	int c,d;` |
|        - |  9998 | `	/* Create an array and a working value */` |
|        3 |  9999 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10000 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10001 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10002 | `		/* Out of memory,return null */` |
|      ! 0 | 10003 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10004 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10005 | `		SXUNUSED(apArg);` |
|      ! 0 | 10006 | `		return PH7_OK;` |
|        - | 10007 | `	}` |
|        3 | 10008 | `	c = d = '/';` |
|        - | 10009 | `#ifdef __WINNT__` |
|        1 | 10010 | `	d = '\\';` |
|        - | 10011 | `#endif` |
|        - | 10012 | `	/* Iterate throw entries */` |
|        3 | 10013 | `	SySetResetCursor(pFiles);` |
|     3691 | 10014 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10015 | `		const char *zBase,*zEnd;` |
|        - | 10016 | `		int iLen;` |
|        - | 10017 | `		/* reset the string cursor */` |
|     3689 | 10018 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10019 | `		/* Extract base name */` |
|     3689 | 10020 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10021 | `		/* Ignore trailing '/' */` |
|     5533 | 10022 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10023 | `			zEnd--;` |
|      ! 0 | 10024 | `		}` |
|     3689 | 10025 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 | 10026 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 | 10027 | `			zEnd--;` |
|        1 | 10028 | `		}` |
|     3689 | 10029 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 | 10030 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10031 | `		/* Copy entry name */` |
|     3689 | 10032 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10033 | `		/* Perform the insertion */` |
|     3689 | 10034 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10035 | `	}` |
|        - | 10036 | `	/* All done,return the created array */` |
|        3 | 10037 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10038 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10039 | `	 * by the engine as soon we return from this foreign` |
|        - | 10040 | `	 * function.` |
|        - | 10041 | `	 */` |
|        3 | 10042 | `	return PH7_OK;` |
|        2 | 10043 |  |
|        - | 10044 | `/*` |
|        - | 10045 | ` * include:` |
|        - | 10046 | ` * According to the PHP reference manual.` |
|        - | 10047 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10048 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10049 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10050 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10051 | ` *  and the current working directory before failing. The include()` |
|        - | 10052 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10053 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10054 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10055 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10056 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10057 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10058 | ` *  directory to find the requested file.` |
|        - | 10059 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10060 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10061 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10062 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10063 | ` */` |
|     7528 | 10064 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10065 |  |
|        - | 10066 | `	SyString sFile;` |
|        - | 10067 | `	sxi32 rc;` |
|     7530 | 10068 | `	if( nArg < 1 ){` |
|        - | 10069 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10070 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10071 | `		return SXRET_OK;` |
|        - | 10072 | `	}` |
|        - | 10073 | `	/* File to include */` |
|     7530 | 10074 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7530 | 10075 | `	if( sFile.nByte < 1 ){` |
|        - | 10076 | `		/* Empty string,return NULL */` |
|      ! 0 | 10077 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10078 | `		return SXRET_OK;` |
|        - | 10079 | `	}` |
|        - | 10080 | `	/* Open,compile and execute the desired script */` |
|     7530 | 10081 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7530 | 10082 | `	if( rc != SXRET_OK ){` |
|        - | 10083 | `		/* Emit a warning and return false */` |
|        3 | 10084 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10085 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10086 | `	}` |
|     7530 | 10087 | `	return SXRET_OK;` |
|     3766 | 10088 |  |
|        - | 10089 | `/*` |
|        - | 10090 | ` * include_once:` |
|        - | 10091 | ` *  According to the PHP reference manual.` |
|        - | 10092 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10093 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10094 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10095 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10096 | ` *   just once.` |
|        - | 10097 | ` */` |
|        4 | 10098 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10099 |  |
|        - | 10100 | `	SyString sFile;` |
|        - | 10101 | `	sxi32 rc;` |
|        5 | 10102 | `	if( nArg < 1 ){` |
|        - | 10103 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10104 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10105 | `		return SXRET_OK;` |
|        - | 10106 | `	}` |
|        - | 10107 | `	/* File to include */` |
|        5 | 10108 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10109 | `	if( sFile.nByte < 1 ){` |
|        - | 10110 | `		/* Empty string,return NULL */` |
|      ! 0 | 10111 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10112 | `		return SXRET_OK;` |
|        - | 10113 | `	}` |
|        - | 10114 | `	/* Open,compile and execute the desired script */` |
|        5 | 10115 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10116 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10117 | `		/* File already included,return TRUE */` |
|        3 | 10118 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10119 | `		return SXRET_OK;` |
|        - | 10120 | `	}` |
|        3 | 10121 | `	if( rc != SXRET_OK ){` |
|        - | 10122 | `		/* Emit a warning and return false */` |
|      ! 0 | 10123 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10124 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10125 | ` 	}` |
|        3 | 10126 | `	return SXRET_OK;` |
|        3 | 10127 |  |
|        - | 10128 | `/*` |
|        - | 10129 | ` * require.` |
|        - | 10130 | ` *  According to the PHP reference manual.` |
|        - | 10131 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10132 | ` *   also produce a fatal level error.` |
|        - | 10133 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10134 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10135 | ` */` |
|        4 | 10136 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10137 |  |
|        - | 10138 | `	SyString sFile;` |
|        - | 10139 | `	sxi32 rc;` |
|        5 | 10140 | `	if( nArg < 1 ){` |
|        - | 10141 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10142 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10143 | `		return SXRET_OK;` |
|        - | 10144 | `	}` |
|        - | 10145 | `	/* File to include */` |
|        5 | 10146 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10147 | `	if( sFile.nByte < 1 ){` |
|        - | 10148 | `		/* Empty string,return NULL */` |
|      ! 0 | 10149 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10150 | `		return SXRET_OK;` |
|        - | 10151 | `	}` |
|        - | 10152 | `	/* Open,compile and execute the desired script */` |
|        5 | 10153 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10154 | `	if( rc != SXRET_OK ){` |
|        - | 10155 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10156 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10157 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10158 | `		return PH7_ABORT;` |
|        - | 10159 | `	}` |
|        5 | 10160 | `	return SXRET_OK;` |
|        3 | 10161 |  |
|        - | 10162 | `/*` |
|        - | 10163 | ` * require_once:` |
|        - | 10164 | ` *  According to the PHP reference manual.` |
|        - | 10165 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10166 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10167 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10168 | ` *   and how it differs from its non _once siblings.` |
|        - | 10169 | ` */` |
|        4 | 10170 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10171 |  |
|        - | 10172 | `	SyString sFile;` |
|        - | 10173 | `	sxi32 rc;` |
|        5 | 10174 | `	if( nArg < 1 ){` |
|        - | 10175 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10176 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10177 | `		return SXRET_OK;` |
|        - | 10178 | `	}` |
|        - | 10179 | `	/* File to include */` |
|        5 | 10180 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10181 | `	if( sFile.nByte < 1 ){` |
|        - | 10182 | `		/* Empty string,return NULL */` |
|      ! 0 | 10183 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10184 | `		return SXRET_OK;` |
|        - | 10185 | `	}` |
|        - | 10186 | `	/* Open,compile and execute the desired script */` |
|        5 | 10187 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10188 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10189 | `		/* File already included,return TRUE */` |
|        3 | 10190 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10191 | `		return SXRET_OK;` |
|        - | 10192 | `	}` |
|        3 | 10193 | `	if( rc != SXRET_OK ){` |
|        - | 10194 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10195 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10196 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10197 | `		return PH7_ABORT;` |
|        - | 10198 | `	}` |
|        3 | 10199 | `	return SXRET_OK;` |
|        3 | 10200 |  |
|        - | 10201 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10202 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10203 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10204 | `/* Table of built-in VM functions. */` |
|        - | 10205 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10206 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10207 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10208 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10209 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10210 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10211 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10212 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10213 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10214 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10215 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10216 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10217 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10218 | `	    /* Constants management */` |
|        - | 10219 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10220 | `	{ "define",   vm_builtin_define               },` |
|        - | 10221 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10222 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10223 | `	   /* Class/Object functions */` |
|        - | 10224 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10225 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10226 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10227 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10228 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10229 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10230 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10231 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10232 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10233 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10234 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10235 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10236 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10237 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10238 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10239 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10240 | `	   /* Random numbers/strings generators */` |
|        - | 10241 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10242 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10243 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10244 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10245 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10246 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10247 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10248 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10249 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10250 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10251 | `	   /* Language constructs functions */` |
|        - | 10252 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10253 | `	{ "print", vm_builtin_print                   },` |
|        - | 10254 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10255 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10256 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10257 | `	  /* Variable handling functions */` |
|        - | 10258 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10259 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10260 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10261 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10262 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10263 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10264 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10265 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10266 | `	  /* Ouput control functions */` |
|        - | 10267 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10268 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10269 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10270 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10271 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10272 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10273 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10274 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10275 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10276 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10277 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10278 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10279 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10280 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10281 | `	  /* Assertion functions */` |
|        - | 10282 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10283 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10284 | `	  /* Error reporting functions */` |
|        - | 10285 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10286 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10287 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10288 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10289 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10290 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10291 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10292 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10293 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10294 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10295 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10296 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10297 | `	  /* Release info */` |
|        - | 10298 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10299 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10300 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10301 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10302 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10303 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10304 | `	  /* hashmap */` |
|        - | 10305 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10306 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10307 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10308 | `	  /* URL related function */` |
|        - | 10309 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10310 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10311 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10312 | `	   /* XML processing functions */` |
|        - | 10313 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10314 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10315 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10316 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10317 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10318 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10319 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10320 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10321 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10322 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10323 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10324 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10325 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10326 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10327 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10328 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10329 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10330 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10331 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10332 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10333 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10334 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10335 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10336 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10337 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10338 | `	   /* Command line processing */` |
|        - | 10339 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10340 | `	   /* JSON encoding/decoding */` |
|        - | 10341 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10342 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10343 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10344 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10345 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10346 | `	   /* Files/URI inclusion facility */` |
|        - | 10347 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10348 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10349 | `	{ "include",      vm_builtin_include          },` |
|        - | 10350 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10351 | `	{ "require",      vm_builtin_require          },` |
|        - | 10352 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10353 | `};` |
|        - | 10354 | `/*` |
|        - | 10355 | ` * Register the built-in VM functions defined above.` |
|        - | 10356 | ` */` |
|     2200 | 10357 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10358 |  |
|        - | 10359 | `	sxi32 rc;` |
|        - | 10360 | `	sxu32 n;` |
|   275002 | 10361 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10362 | `		/* Note that these special functions have access` |
|        - | 10363 | `		 * to the underlying virtual machine as their` |
|        - | 10364 | `		 * private data.` |
|        - | 10365 | `		 */` |
|   272802 | 10366 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   272802 | 10367 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10368 | `			return rc;` |
|        - | 10369 | `		}` |
|   136402 | 10370 | `	}` |
|     2202 | 10371 | `	return SXRET_OK;` |
|     1102 | 10372 |  |
|        - | 10373 | `/*` |
|        - | 10374 | ` * Check if the given name refer to an installed class.` |
|        - | 10375 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10376 | ` */` |
|    15940 | 10377 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10378 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10379 | `	const char *zName,  /* Name of the target class */` |
|        - | 10380 | `	sxu32 nByte,        /* zName length */` |
|        - | 10381 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10382 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10383 | `						 */` |
|        - | 10384 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10385 | `	)` |
|        2 | 10386 |  |
|        - | 10387 | `	SyHashEntry *pEntry;` |
|        - | 10388 | `	ph7_class *pClass;` |
|     7970 | 10389 | `	SXUNUSED(iNest);` |
|        - | 10390 | `	/* Exact class lookup.` |
|        - | 10391 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 10392 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    15942 | 10393 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    15942 | 10394 | `	if( pEntry == 0 ){` |
|        7 | 10395 | `		return 0;` |
|        - | 10396 | `	}` |
|    15936 | 10397 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    15936 | 10398 | `	if( !iLoadable ){` |
|    14820 | 10399 | `		return pClass;` |
|        - | 10400 | `	}` |
|        - | 10401 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1118 | 10402 | `	while(pClass){` |
|     1118 | 10403 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1118 | 10404 | `			return pClass;` |
|        - | 10405 | `		}` |
|      ! 0 | 10406 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10407 | `	}` |
|      ! 0 | 10408 | `	return 0;` |
|     7972 | 10409 |  |
|        - | 10410 | `/*` |
|        - | 10411 | ` * Reference Table Implementation` |
|        - | 10412 | ` * Status: stable <chm@symisc.net>` |
|        - | 10413 | ` * Intro` |
|        - | 10414 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10415 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10416 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10417 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10418 | ` *  Refer to the official for more information on this powerful` |
|        - | 10419 | ` *  extension.` |
|        - | 10420 | ` */` |
|        - | 10421 | `/*` |
|        - | 10422 | ` * Allocate a new reference entry.` |
|        - | 10423 | ` */` |
|  2990926 | 10424 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10425 |  |
|        - | 10426 | `	VmRefObj *pRef;` |
|        - | 10427 | `	/* Allocate a new instance */` |
|  2990928 | 10428 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2990928 | 10429 | `	if( pRef == 0 ){` |
|      ! 0 | 10430 | `		return 0;` |
|        - | 10431 | `	}` |
|        - | 10432 | `	/* Zero the structure */` |
|  2990928 | 10433 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10434 | `	/* Initialize fields */` |
|  2990928 | 10435 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2990928 | 10436 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2990928 | 10437 | `	pRef->nIdx = nIdx;` |
|  2990928 | 10438 | `	return pRef;` |
|  1495465 | 10439 |  |
|        - | 10440 | `/*` |
|        - | 10441 | ` * Default hash function used by the reference table` |
|        - | 10442 | ` * for lookup/insertion operations.` |
|        - | 10443 | ` */` |
| 16595737 | 10444 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10445 |  |
|        - | 10446 | `	/* Calculate the hash based on the memory object index */` |
| 16595739 | 10447 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10448 |  |
|        - | 10449 | `/*` |
|        - | 10450 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10451 | ` * in the reference table.` |
|        - | 10452 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10453 | ` * otherwise.` |
|        - | 10454 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10455 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10456 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10457 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10458 | ` * Refer to the official for more information on this powerful` |
|        - | 10459 | ` * extension.` |
|        - | 10460 | ` */` |
|  8929398 | 10461 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10462 |  |
|        - | 10463 | `	VmRefObj *pRef;` |
|        - | 10464 | `	sxu32 nBucket;` |
|        - | 10465 | `	/* Point to the appropriate bucket */` |
|  8929400 | 10466 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10467 | `	/* Perform the lookup */` |
|  8929400 | 10468 | `	pRef = pVm->apRefObj[nBucket];` |
| 18830286 | 10469 | `	for(;;){` |
| 37656096 | 10470 | `		if( pRef == 0 ){` |
|  3065602 | 10471 | `			break;` |
|        - | 10472 | `		}` |
| 34590496 | 10473 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10474 | `			/* Entry found */` |
|  5863800 | 10475 | `			return pRef;` |
|        - | 10476 | `		}` |
|        - | 10477 | `		/* Point to the next entry */` |
| 28726698 | 10478 | `		pRef = pRef->pNextCollide;` |
|        2 | 10479 | `	}` |
|        - | 10480 | `	/* No such entry,return NULL */` |
|  3065602 | 10481 | `	return 0;` |
|  4464701 | 10482 |  |
|        - | 10483 | `/*` |
|        - | 10484 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10485 | ` *` |
|        - | 10486 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10487 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10488 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10489 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10490 | ` * Refer to the official for more information on this powerful` |
|        - | 10491 | ` * extension.` |
|        - | 10492 | ` */` |
|  2990926 | 10493 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10494 |  |
|        - | 10495 | `	sxu32 nBucket;` |
|  2990928 | 10496 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10497 | `		VmRefObj **apNew;` |
|        - | 10498 | `		sxu32 nNew;` |
|        - | 10499 | `		/* Allocate a larger table */` |
|     3464 | 10500 | `		nNew = pVm->nRefSize << 1;` |
|     3464 | 10501 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3464 | 10502 | `		if( apNew ){` |
|     3464 | 10503 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10504 | `			sxu32 n;` |
|        - | 10505 | `			/* Zero the structure */` |
|     3464 | 10506 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10507 | `			/* Rehash all referenced entries */` |
|  2834858 | 10508 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10509 | `				/* Remove old collision links */` |
|  2831396 | 10510 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10511 | `				/* Point to the appropriate bucket */` |
|  2831396 | 10512 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10513 | `				/* Insert the entry  */` |
|  2831396 | 10514 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2831396 | 10515 | `				if( apNew[nBucket] ){` |
|  2298896 | 10516 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10517 | `				}` |
|  2831396 | 10518 | `				apNew[nBucket] = pEntry;` |
|        - | 10519 | `				/* Point to the next entry */` |
|  2831396 | 10520 | `				pEntry = pEntry->pNext;` |
|  1415699 | 10521 | `			}` |
|        - | 10522 | `			/* Release the old table */` |
|     3464 | 10523 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10524 | `			/* Install the new one */` |
|     3464 | 10525 | `			pVm->apRefObj = apNew;` |
|     3464 | 10526 | `			pVm->nRefSize = nNew;` |
|     1731 | 10527 | `		}` |
|     1731 | 10528 | `	}` |
|        - | 10529 | `	/* Point to the appropriate bucket */` |
|  2990928 | 10530 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10531 | `	/* Insert the entry */` |
|  2990928 | 10532 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2990928 | 10533 | `	if( pVm->apRefObj[nBucket] ){` |
|  2482861 | 10534 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1241354 | 10535 | `	}` |
|  2990928 | 10536 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2990928 | 10537 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2990928 | 10538 | `	pVm->nRefUsed++;` |
|  2990928 | 10539 | `	return SXRET_OK;` |
|        2 | 10540 |  |
|        - | 10541 | `/*` |
|        - | 10542 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10543 | ` * the reference table.` |
|        - | 10544 | ` * This function is invoked when the user perform an unset` |
|        - | 10545 | ` * call [i.e: unset($var); ].` |
|        - | 10546 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10547 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10548 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10549 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10550 | ` * Refer to the official for more information on this powerful` |
|        - | 10551 | ` * extension.` |
|        - | 10552 | ` */` |
|  2959644 | 10553 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10554 |  |
|        - | 10555 | `	ph7_hashmap_node **apNode;` |
|        - | 10556 | `	SyHashEntry **apEntry;` |
|        - | 10557 | `	sxu32 n;` |
|        - | 10558 | `	/* Point to the reference table */` |
|  2959646 | 10559 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2959646 | 10560 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10561 | `	/* Unlink the entry from the reference table */` |
|  3039282 | 10562 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    79638 | 10563 | `		if( apEntry[n] ){` |
|    79588 | 10564 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    39793 | 10565 | `		}` |
|    39820 | 10566 | `	}` |
|  5841514 | 10567 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2881870 | 10568 | `		if( apNode[n] ){` |
|     5635 | 10569 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2817 | 10570 | `		}` |
|  1440936 | 10571 | `	}` |
|  2959646 | 10572 | `	if( pRef->pPrevCollide ){` |
|  1115627 | 10573 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   557965 | 10574 | `	}else{` |
|  1844021 | 10575 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10576 | `	}` |
|  2959646 | 10577 | `	if( pRef->pNextCollide ){` |
|  1671176 | 10578 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   835499 | 10579 | `	}` |
|  2959646 | 10580 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10581 | `	/* Release the node */` |
|  2959646 | 10582 | `	SySetRelease(&pRef->aReference);` |
|  2959646 | 10583 | `	SySetRelease(&pRef->aArrEntries);` |
|  2959646 | 10584 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2959646 | 10585 | `	pVm->nRefUsed--;` |
|  2959646 | 10586 | `	return SXRET_OK;` |
|        2 | 10587 |  |
|        - | 10588 | `/*` |
|        - | 10589 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10590 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10591 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10592 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10593 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10594 | ` * Refer to the official for more information on this powerful` |
|        - | 10595 | ` * extension.` |
|        - | 10596 | ` */` |
|  3018786 | 10597 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10598 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10599 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10600 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10601 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10602 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10603 | `	)` |
|        2 | 10604 |  |
|  3018788 | 10605 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10606 | `	VmRefObj *pRef;` |
|        - | 10607 | `	/* Check if the referenced object already exists */` |
|  3018788 | 10608 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3018788 | 10609 | `	if( pRef == 0 ){` |
|        - | 10610 | `		/* Create a new entry */` |
|  2990928 | 10611 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2990928 | 10612 | `		if( pRef == 0 ){` |
|      ! 0 | 10613 | `			return SXERR_MEM;` |
|        - | 10614 | `		}` |
|  2990928 | 10615 | `		pRef->iFlags = iFlags;` |
|        - | 10616 | `		/* Install the entry */` |
|  2990928 | 10617 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1495463 | 10618 | `	}` |
|  3018948 | 10619 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10620 | `		/* Safely ignore the exception frame */` |
|      162 | 10621 | `		pFrame = pFrame->pParent;` |
|        2 | 10622 | `	}` |
|  3018788 | 10623 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10624 | `		VmSlot sRef;` |
|        - | 10625 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10626 | `		 * be deleted when we leave this frame.` |
|        - | 10627 | `		 */` |
|    74710 | 10628 | `		sRef.nIdx = nIdx;` |
|    74710 | 10629 | `		sRef.pUserData = pEntry;` |
|    74710 | 10630 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10631 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10632 | `		}` |
|    37354 | 10633 | `	}` |
|  3018788 | 10634 | `	if( pEntry ){` |
|        - | 10635 | `		/* Address of the hash-entry */` |
|   102380 | 10636 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    51189 | 10637 | `	}` |
|  3018788 | 10638 | `	if( pMapEntry ){` |
|        - | 10639 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2911570 | 10640 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1455784 | 10641 | `	}` |
|  3018788 | 10642 | `	return SXRET_OK;` |
|  1509395 | 10643 |  |
|        - | 10644 | `/*` |
|        - | 10645 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10646 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10647 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10648 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10649 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10650 | ` * Refer to the official for more information on this powerful` |
|        - | 10651 | ` * extension.` |
|        - | 10652 | ` */` |
|  2950948 | 10653 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10654 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10655 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10656 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10657 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10658 | `	)` |
|        2 | 10659 |  |
|        - | 10660 | `	VmRefObj *pRef;` |
|        - | 10661 | `	sxu32 n;` |
|        - | 10662 | `	/* Check if the referenced object already exists */` |
|  2950950 | 10663 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2950950 | 10664 | `	if( pRef == 0 ){` |
|        - | 10665 | `		/* Not such entry */` |
|    74656 | 10666 | `		return SXERR_NOTFOUND;` |
|        - | 10667 | `	}` |
|        - | 10668 | `	/* Remove the desired entry */` |
|  2876296 | 10669 | `	if( pEntry ){` |
|        - | 10670 | `		SyHashEntry **apEntry;` |
|       56 | 10671 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10672 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10673 | `			if( apEntry[n] == pEntry ){` |
|        - | 10674 | `				/* Nullify the entry */` |
|       56 | 10675 | `				apEntry[n] = 0;` |
|        - | 10676 | `				/*` |
|        - | 10677 | `				 * NOTE:` |
|        - | 10678 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10679 | `				 * we avoid wasting spaces.` |
|        - | 10680 | `				 */` |
|       27 | 10681 | `			}` |
|       79 | 10682 | `		}` |
|       27 | 10683 | `	}` |
|  2876296 | 10684 | `	if( pMapEntry ){` |
|        - | 10685 | `		ph7_hashmap_node **apNode;` |
|  2876242 | 10686 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5752570 | 10687 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2876330 | 10688 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10689 | `				/* nullify the entry */` |
|  2876242 | 10690 | `				apNode[n] = 0;` |
|  1438120 | 10691 | `			}` |
|  1438166 | 10692 | `		}` |
|  1438120 | 10693 | `	}` |
|  2876296 | 10694 | `	return SXRET_OK;` |
|  1475476 | 10695 |  |
|        - | 10696 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10697 | `/*` |
|        - | 10698 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10699 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10700 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10701 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10702 | ` * For more information on how to register IO stream devices,please` |
|        - | 10703 | ` * refer to the official documentation.` |
|        - | 10704 | ` */` |
|    22916 | 10705 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10706 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10707 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10708 | `	int nByte              /* *pzDevice length*/` |
|        - | 10709 | `	)` |
|        2 | 10710 |  |
|        - | 10711 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10712 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10713 | `	SyString sDev,sCur;` |
|        - | 10714 | `	sxu32 n,nEntry;` |
|        - | 10715 | `	int rc;` |
|        - | 10716 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    22918 | 10717 | `	zNext = zCur = zIn = *pzDevice;` |
|    22918 | 10718 | `	zEnd = &zIn[nByte];` |
|  1466596 | 10719 | `	while( zIn < zEnd ){` |
|  1443682 | 10720 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10721 | `			/* Got one */` |
|        3 | 10722 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10723 | `			break;` |
|        - | 10724 | `		}` |
|        - | 10725 | `		/* Advance the cursor */` |
|  1443680 | 10726 | `		zIn++;` |
|        2 | 10727 | `	}` |
|    22918 | 10728 | `	if( zIn >= zEnd ){` |
|        - | 10729 | `		/* No such scheme,return the default stream */` |
|    22916 | 10730 | `		return pVm->pDefStream;` |
|        - | 10731 | `	}` |
|        3 | 10732 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10733 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10734 | `	SyStringFullTrim(&sDev);` |
|        - | 10735 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10736 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10737 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10738 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10739 | `		pStream = apStream[n];` |
|        3 | 10740 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10741 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10742 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10743 | `		if( rc == 0 ){` |
|        - | 10744 | `			/* Stream device found */` |
|        3 | 10745 | `			*pzDevice = zNext;` |
|        3 | 10746 | `			return pStream;` |
|        - | 10747 | `		}` |
|      ! 0 | 10748 | `	}` |
|        - | 10749 | `	/* No such stream,return NULL */` |
|      ! 0 | 10750 | `	return 0;` |
|    11460 | 10751 |  |
|        - | 10752 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10753 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10754 |  |
