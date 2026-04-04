# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4788/6361 lines (75.27%)

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
|   783494 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   783496 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   783466 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   783458 |    94 | `	return FALSE;` |
|   391771 |    95 |  |
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
|   561180 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   561182 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   561182 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   561178 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   561178 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   561178 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   561178 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   561178 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   561178 |   142 | `	pCons->xExpand = xExpand;` |
|   561178 |   143 | `	pCons->pUserData = pUserData;` |
|   561178 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   561178 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   561178 |   151 | `	return SXRET_OK;` |
|   280592 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|  1217774 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1217776 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1217776 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|  1217776 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1217776 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|  1217776 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|  1217776 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1217776 |   185 | `	pFunc->pVm   = pVm;` |
|  1217776 |   186 | `	pFunc->xFunc = xFunc;` |
|  1217776 |   187 | `	pFunc->pUserData = pUserData;` |
|  1217776 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|  1217776 |   190 | `	*ppOut = pFunc;` |
|  1217776 |   191 | `	return SXRET_OK;` |
|   608889 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|  1220360 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1220362 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1220362 |   213 | `	if( pEntry ){` |
|     2588 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2588 |   215 | `		pFunc->pUserData = pUserData;` |
|     2588 |   216 | `		pFunc->xFunc = xFunc;` |
|     2588 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2588 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|  1217776 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1217776 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|  1217776 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1217776 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|  1217776 |   233 | `	return SXRET_OK;` |
|   610182 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   174666 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   174668 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   174668 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   174668 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   174668 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   174668 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   174668 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   174668 |   260 | `	pFunc->iFlags = iFlags;` |
|   174668 |   261 | `	pFunc->pUserData = pUserData;` |
|   174668 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   174668 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   592124 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   592126 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    37614 |   283 | `		pName = &pFunc->sName;` |
|    18806 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   592126 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   592126 |   287 | `	if( pEntry ){` |
|   440492 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   440492 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   440492 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|   151636 |   297 | `	pFunc->pNextName = 0;` |
|   151636 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   151636 |   299 | `	return rc;` |
|   296064 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    43160 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    43162 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    43162 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    43162 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    43132 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    43132 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    43132 |   324 | `	return rc;` |
|    21582 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  3504036 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3504038 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  3504038 |   342 | `	sInstr.iP1 = iP1;` |
|  3504038 |   343 | `	sInstr.iP2 = iP2;` |
|  3504038 |   344 | `	sInstr.p3  = p3;` |
|  3504038 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   202520 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   101259 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  3504038 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3504038 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  3504038 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   418216 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   418218 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   418218 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   418218 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   209108 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   209110 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   199596 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   199598 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   199598 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|  1054244 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|  1054246 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   189906 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   189908 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   680982 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   680984 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    29180 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    29182 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    29182 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    29182 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    29182 |   417 | `	return &aInstr[n - 2];` |
|    14592 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    16410 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    16412 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16412 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    16412 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    16412 |   437 | `	pFrame->pUserData = pUserData;` |
|    16412 |   438 | `	pFrame->pThis = pThis;` |
|    16412 |   439 | `	pFrame->pVm = pVm;` |
|    16412 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16412 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16412 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16412 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16412 |   444 | `	return pFrame;` |
|     8207 |   445 |  |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   448 | `/*` |
|        - |   449 | ` * Enter a VM frame.` |
|        - |   450 | ` */` |
|    16368 |   451 | `static sxi32 VmEnterFrame(` |
|        - |   452 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   453 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   454 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   455 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   456 | `	)` |
|        2 |   457 |  |
|        - |   458 | `	VmFrame *pFrame;` |
|        - |   459 | `	/* Allocate a new frame */` |
|    16370 |   460 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    16370 |   461 | `	if( pFrame == 0 ){` |
|      ! 0 |   462 | `		return SXERR_MEM;` |
|        - |   463 | `	}` |
|        - |   464 | `	/* Link to the list of active VM frame */` |
|    16370 |   465 | `	pFrame->pParent = pVm->pFrame;` |
|    16370 |   466 | `	pVm->pFrame = pFrame;` |
|    16370 |   467 | `	if( ppFrame ){` |
|        - |   468 | `		/* Write a pointer to the new VM frame */` |
|    13524 |   469 | `		*ppFrame = pFrame;` |
|     6761 |   470 | `	}` |
|    16370 |   471 | `	return SXRET_OK;` |
|     8186 |   472 |  |
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
|    13522 |   516 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   517 |  |
|    13524 |   518 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13524 |   519 | `	if( pCurFrame ){` |
|        - |   520 | `		/* Unlink from the list of active VM frame */` |
|    13524 |   521 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13524 |   522 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   523 | `			VmSlot  *aSlot;` |
|        - |   524 | `			sxu32 n;` |
|        - |   525 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13460 |   526 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    93488 |   527 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   528 | `				/* Unset the local variable */` |
|    80030 |   529 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    40016 |   530 | `			}` |
|        - |   531 | `			/* Remove local reference */` |
|    13460 |   532 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    93544 |   533 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    80086 |   534 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    40044 |   535 | `			}` |
|     6729 |   536 | `		}` |
|        - |   537 | `		/* Release internal containers */` |
|    13524 |   538 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13524 |   539 | `		SySetRelease(&pCurFrame->sArg);` |
|    13524 |   540 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13524 |   541 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   542 | `		/* Release the whole structure */` |
|    13524 |   543 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6761 |   544 | `	}` |
|    13524 |   545 |  |
|        - |   546 | `/*` |
|        - |   547 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   548 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   549 | ` * should be skipped when looking for the real execution context.` |
|        - |   550 | ` */` |
|  6327886 |   551 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   552 |  |
|  6328164 |   553 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   554 | `		pFrame = pFrame->pParent;` |
|        2 |   555 | `	}` |
|  6327888 |   556 | `	return pFrame;` |
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
|   114262 |   674 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   675 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   676 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   677 | `	)` |
|        2 |   678 |  |
|        - |   679 | `	ph7_class_method *pMeth;` |
|        - |   680 | `	ph7_class_attr *pAttr;` |
|        - |   681 | `	SyHashEntry *pEntry;` |
|        - |   682 | `	sxi32 rc;` |
|        - |   683 | `	/* Reset the loop cursor */` |
|   114264 |   684 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   685 | `	/* Process only static and constant attribute */` |
|   471683 |   686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   687 | `		/* Extract the current attribute */` |
|   300290 |   688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   300290 |   689 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   114264 |   711 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   712 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   713 | `		 */` |
|    48744 |   714 | `		return SXRET_OK;` |
|        - |   715 | `	}` |
|        - |   716 | `	/* Create constructor alias if not yet done */` |
|    65522 |   717 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   718 | `		/* User constructor with the same base class name */` |
|     5726 |   719 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5726 |   720 | `		if( pEntry ){` |
|      ! 0 |   721 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   722 | `			/* Create the alias */` |
|      ! 0 |   723 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   724 | `		}` |
|     2862 |   725 | `	}` |
|        - |   726 | `	/* Install the methods now */` |
|    65522 |   727 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   652802 |   728 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   554522 |   729 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   554522 |   730 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   554514 |   731 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   554514 |   732 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   733 | `				return rc;` |
|        - |   734 | `			}` |
|   277256 |   735 | `		}` |
|        2 |   736 | `	}` |
|        - |   737 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    65522 |   738 | `	pClass->bMounted = TRUE;` |
|    65522 |   739 | `	return SXRET_OK;` |
|    57133 |   740 |  |
|        - |   741 | `/*` |
|        - |   742 | ` * Allocate a private frame for attributes of the given` |
|        - |   743 | ` * class instance (Object in the PHP jargon).` |
|        - |   744 | ` */` |
|     1194 |   745 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   747 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   748 | `	)` |
|        2 |   749 |  |
|     1196 |   750 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   751 | `	ph7_class_attr *pAttr;` |
|        - |   752 | `	SyHashEntry *pEntry;` |
|        - |   753 | `	sxi32 rc;` |
|        - |   754 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1196 |   755 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4908 |   756 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   757 | `		VmClassAttr *pVmAttr;` |
|        - |   758 | `		/* Extract the current attribute */` |
|     3714 |   759 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3714 |   760 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3714 |   761 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   762 | `			return SXERR_MEM;` |
|        - |   763 | `		}` |
|     3714 |   764 | `		pVmAttr->pAttr = pAttr;` |
|     3714 |   765 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   766 | `			ph7_value *pMemObj;` |
|        - |   767 | `			/* Reserve a memory object for this attribute */` |
|     3708 |   768 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3708 |   769 | `			if( pMemObj == 0 ){` |
|      ! 0 |   770 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   771 | `				return SXERR_MEM;` |
|        - |   772 | `			}` |
|     3708 |   773 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3708 |   774 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   775 | `				/* Initialize attribute default value (any complex expression) */` |
|     1196 |   776 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      597 |   777 | `			}` |
|     3708 |   778 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3708 |   779 | `			if( rc != SXRET_OK ){` |
|        - |   780 | `				VmSlot sSlot;` |
|        - |   781 | `				/* Restore memory object */` |
|      ! 0 |   782 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   783 | `				sSlot.pUserData = 0;` |
|      ! 0 |   784 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   785 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   786 | `				return SXERR_MEM;` |
|        - |   787 | `			}` |
|        - |   788 | `			/* Install attribute in the reference table */` |
|     3708 |   789 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1855 |   790 | `		}else{` |
|        - |   791 | `			/* Install static/constant attribute */` |
|        8 |   792 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   793 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   794 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `		}` |
|        2 |   799 | `	}` |
|     1196 |   800 | `	return SXRET_OK;` |
|      599 |   801 |  |
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
|   399832 |   813 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   814 |  |
|        - |   815 | `	ph7_value *pObj;` |
|        - |   816 | `	sxi32 rc;` |
|   399834 |   817 | `	if( pIndex ){` |
|        - |   818 | `		/* Object index in the object table */` |
|   391296 |   819 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   195647 |   820 | `	}` |
|        - |   821 | `	/* Reserve a slot for the new object */` |
|   399834 |   822 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   399834 |   823 | `	if( rc != SXRET_OK ){` |
|        - |   824 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   825 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   826 | `		 */` |
|      ! 0 |   827 | `		return 0;` |
|        - |   828 | `	}` |
|   399834 |   829 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   399834 |   830 | `	return pObj;` |
|   199918 |   831 |  |
|        - |   832 | `/*` |
|        - |   833 | ` * Reserve a memory object.` |
|        - |   834 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   835 | ` */` |
|  2146124 |   836 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   837 |  |
|        - |   838 | `	ph7_value *pObj;` |
|        - |   839 | `	sxi32 rc;` |
|  2146126 |   840 | `	if( pIndex ){` |
|        - |   841 | `		/* Object index in the object table */` |
|  2146126 |   842 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1073062 |   843 | `	}` |
|        - |   844 | `	/* Reserve a slot for the new object */` |
|  2146126 |   845 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2146126 |   846 | `	if( rc != SXRET_OK ){` |
|        - |   847 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   848 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   849 | `		 */` |
|      ! 0 |   850 | `		return 0;` |
|        - |   851 | `	}` |
|  2146126 |   852 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2146126 |   853 | `	return pObj;` |
|  1073064 |   854 |  |
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
|     2846 |  1262 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1263 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1264 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1265 | `	 )` |
|        2 |  1266 |  |
|        - |  1267 | `	SyString sBuiltin;` |
|        - |  1268 | `	ph7_value *pObj;` |
|        - |  1269 | `	sxi32 rc;` |
|        - |  1270 | `	/* Zero the structure */` |
|     2848 |  1271 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1272 | `	/* Initialize VM fields */` |
|     2848 |  1273 | `	pVm->pEngine = &(*pEngine);` |
|     2848 |  1274 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1275 | `	/* Instructions containers */` |
|     2848 |  1276 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2848 |  1277 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2848 |  1278 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1279 | `	/* Object containers */` |
|     2848 |  1280 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2848 |  1281 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1282 | `	/* Virtual machine internal containers */` |
|     2848 |  1283 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2848 |  1284 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2848 |  1285 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2848 |  1286 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2848 |  1287 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2848 |  1288 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2848 |  1289 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2848 |  1290 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2848 |  1291 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2848 |  1292 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2848 |  1293 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2848 |  1294 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2848 |  1295 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2848 |  1296 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2848 |  1297 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2848 |  1298 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2848 |  1299 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2848 |  1300 | `	pVm->pPendingException = 0;` |
|        - |  1301 | `	/* Configuration containers */` |
|     2848 |  1302 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2848 |  1303 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2848 |  1304 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2848 |  1305 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2848 |  1306 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2848 |  1307 | `	pVm->iResponseStatus = 200;` |
|     2848 |  1308 | `	pVm->bHeadersSent = 0;` |
|     2848 |  1309 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1310 | `	/* Error callbacks containers */` |
|     2848 |  1311 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2848 |  1312 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2848 |  1313 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2848 |  1314 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2848 |  1315 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1316 | `	/* Set a default recursion limit */` |
|        - |  1317 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2848 |  1318 | `	pVm->nMaxDepth = 32;` |
|        - |  1319 | `#else` |
|        - |  1320 | `	pVm->nMaxDepth = 16;` |
|        - |  1321 | `#endif` |
|        - |  1322 | `	/* Default assertion flags */` |
|     2848 |  1323 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1324 | `	/* JSON return status */` |
|     2848 |  1325 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1326 | `	/* PRNG context */` |
|     2848 |  1327 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1328 | `	/* Install the null constant */` |
|     2848 |  1329 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2848 |  1330 | `	if( pObj == 0 ){` |
|      ! 0 |  1331 | `		rc = SXERR_MEM;` |
|      ! 0 |  1332 | `		goto Err;` |
|        - |  1333 | `	}` |
|     2848 |  1334 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1335 | `	/* Install the boolean TRUE constant */` |
|     2848 |  1336 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2848 |  1337 | `	if( pObj == 0 ){` |
|      ! 0 |  1338 | `		rc = SXERR_MEM;` |
|      ! 0 |  1339 | `		goto Err;` |
|        - |  1340 | `	}` |
|     2848 |  1341 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1342 | `	/* Install the boolean FALSE constant */` |
|     2848 |  1343 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2848 |  1344 | `	if( pObj == 0 ){` |
|      ! 0 |  1345 | `		rc = SXERR_MEM;` |
|      ! 0 |  1346 | `		goto Err;` |
|        - |  1347 | `	}` |
|     2848 |  1348 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1349 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1350 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1351 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2848 |  1352 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2848 |  1353 | `	if( pObj == 0 ){` |
|      ! 0 |  1354 | `		rc = SXERR_MEM;` |
|      ! 0 |  1355 | `		goto Err;` |
|        - |  1356 | `	}` |
|     2848 |  1357 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1358 | `	/* Create the global frame */` |
|     2848 |  1359 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2848 |  1360 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1361 | `		goto Err;` |
|        - |  1362 | `	}` |
|        - |  1363 | `	/* Initialize the code generator */` |
|     2848 |  1364 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2848 |  1365 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1366 | `		goto Err;` |
|        - |  1367 | `	}` |
|        - |  1368 | `	/* VM correctly initialized,set the magic number */` |
|     2848 |  1369 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2848 |  1370 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1371 | `	/* Compile the built-in library */` |
|     2848 |  1372 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1373 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2848 |  1374 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1375 | `	/* Register Fiber internal C functions */` |
|     2848 |  1376 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2848 |  1377 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2848 |  1378 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2848 |  1379 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2848 |  1380 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2848 |  1381 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2848 |  1382 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2848 |  1383 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2848 |  1384 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2848 |  1385 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1386 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2848 |  1387 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2848 |  1388 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2848 |  1389 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2848 |  1390 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2848 |  1391 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2848 |  1392 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2848 |  1393 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2848 |  1394 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2848 |  1395 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2848 |  1396 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1397 | `	/* Reset the code generator */` |
|     2848 |  1398 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2848 |  1399 | `	return SXRET_OK;` |
|      ! 0 |  1400 | `Err:` |
|      ! 0 |  1401 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1402 | `	return rc;` |
|     1425 |  1403 |  |
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
|    13636 |  1430 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1431 |  |
|    13638 |  1432 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    13638 |  1433 | `	if( xCons != VmObConsumer ){` |
|     6726 |  1434 | `		pVm->nOutputLen += nLen;` |
|     6726 |  1435 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      950 |  1436 | `			pVm->bHeadersSent = 1;` |
|      474 |  1437 | `		}` |
|     3362 |  1438 | `	}` |
|    13638 |  1439 |  |
|        - |  1440 | `#define VM_STACK_GUARD 16` |
|        - |  1441 | `/*` |
|        - |  1442 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1443 | ` * our compiled PHP program.` |
|        - |  1444 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1445 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1446 | ` */` |
|    33070 |  1447 | `static ph7_value * VmNewOperandStack(` |
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
|    33072 |  1460 | `	nInstr += VM_STACK_GUARD;` |
|    33072 |  1461 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    33072 |  1462 | `	if( pStack == 0 ){` |
|      ! 0 |  1463 | `		return 0;` |
|        - |  1464 | `	}` |
|        - |  1465 | `	/* Initialize the operand stack */` |
|  2067180 |  1466 | `	while( nInstr > 0 ){` |
|  2034110 |  1467 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2034110 |  1468 | `		--nInstr;` |
|        2 |  1469 | `	}` |
|        - |  1470 | `	/* Ready for bytecode execution */` |
|    33072 |  1471 | `	return pStack;` |
|    16537 |  1472 |  |
|        - |  1473 | `/* Forward declaration */` |
|        - |  1474 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1475 | `/*` |
|        - |  1476 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1477 | ` * This routine gets called by the PH7 engine after` |
|        - |  1478 | ` * successful compilation of the target PHP program.` |
|        - |  1479 | ` */` |
|     2586 |  1480 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1481 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1482 | `	)` |
|        2 |  1483 |  |
|        - |  1484 | `	SyHashEntry *pEntry;` |
|        - |  1485 | `	sxi32 rc;` |
|     2588 |  1486 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1487 | `		/* Initialize your VM first */` |
|      ! 0 |  1488 | `		return SXERR_CORRUPT;` |
|        - |  1489 | `	}` |
|        - |  1490 | `	/* Mark the VM ready for byte-code execution */` |
|     2588 |  1491 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1492 | `	/* Release the code generator now we have compiled our program */` |
|     2588 |  1493 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1494 | `	/* Emit the DONE instruction */` |
|     2588 |  1495 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2588 |  1496 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1497 | `		return SXERR_MEM;` |
|        - |  1498 | `	}` |
|        - |  1499 | `	/* Script return value */` |
|     2588 |  1500 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1501 | `	/* Allocate a new operand stack */` |
|     2588 |  1502 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2588 |  1503 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1504 | `		return SXERR_MEM;` |
|        - |  1505 | `	}` |
|        - |  1506 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1507 | `	 * private data. */` |
|     2588 |  1508 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2588 |  1509 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1510 | `	/* Allocate the reference table */` |
|     2588 |  1511 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2588 |  1512 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2588 |  1513 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1514 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1515 | `		return SXERR_MEM;` |
|        - |  1516 | `	}` |
|        - |  1517 | `	/* Zero the reference table */` |
|     2588 |  1518 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1519 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2588 |  1520 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2588 |  1521 | `	if( rc != SXRET_OK ){` |
|        - |  1522 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1523 | `		return rc;` |
|        - |  1524 | `	}` |
|        - |  1525 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2588 |  1526 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2588 |  1527 | `	if( rc != SXRET_OK ){` |
|        - |  1528 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1529 | `		return rc;` |
|        - |  1530 | `	}` |
|        - |  1531 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2588 |  1532 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1533 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2588 |  1534 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1535 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2588 |  1536 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1537 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1538 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2588 |  1539 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2588 |  1540 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1541 | `#endif` |
|        - |  1542 | `	/* Initialize and install static and constants class attributes */` |
|     2588 |  1543 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    41554 |  1544 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    38968 |  1545 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    38968 |  1546 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1547 | `			return rc;` |
|        - |  1548 | `		}` |
|        2 |  1549 | `	}` |
|        - |  1550 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2588 |  1551 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1552 | `	/* VM is ready for bytecode execution */` |
|     2588 |  1553 | `	return SXRET_OK;` |
|     1295 |  1554 |  |
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
|     2578 |  1579 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1580 |  |
|        - |  1581 | `	/* Set the stale magic number */` |
|     2580 |  1582 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1583 | `	/* Release the private memory subsystem */` |
|     2580 |  1584 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2580 |  1585 | `	return SXRET_OK;` |
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
|   572450 |  1597 | `static sxi32 VmInitCallContext(` |
|        - |  1598 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1599 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1600 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1601 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1602 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1603 | `	)` |
|        2 |  1604 |  |
|   572452 |  1605 | `	pOut->pFunc = pFunc;` |
|   572452 |  1606 | `	pOut->pVm   = pVm;` |
|   572452 |  1607 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   572452 |  1608 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1609 | `	/* Assume a null return value */` |
|   572452 |  1610 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   572452 |  1611 | `	pOut->pRet = pRet;` |
|   572452 |  1612 | `	pOut->iFlags = iFlags;` |
|   572452 |  1613 | `	return SXRET_OK;` |
|        2 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1617 | ` * left behind.` |
|        - |  1618 | ` */` |
|   572450 |  1619 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1620 |  |
|        - |  1621 | `	sxu32 n;` |
|   572452 |  1622 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6952 |  1623 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19850 |  1624 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12900 |  1625 | `			if( apObj[n] == 0 ){` |
|        - |  1626 | `				/* Already released */` |
|      298 |  1627 | `				continue;` |
|        - |  1628 | `			}` |
|    12604 |  1629 | `			PH7_MemObjRelease(apObj[n]);` |
|    12604 |  1630 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6303 |  1631 | `		}` |
|     6952 |  1632 | `		SySetRelease(&pCtx->sVar);` |
|     3475 |  1633 | `	}` |
|   572452 |  1634 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   572452 |  1650 |  |
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
|  3328948 |  1681 | `static void VmPopOperand(` |
|        - |  1682 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1683 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1684 | `	)` |
|        2 |  1685 |  |
|  3328950 |  1686 | `	ph7_value *pTos = *ppTos;` |
|  7073844 |  1687 | `	while( nPop > 0 ){` |
|  3744896 |  1688 | `		PH7_MemObjRelease(pTos);` |
|  3744896 |  1689 | `		pTos--;` |
|  3744896 |  1690 | `		nPop--;` |
|        2 |  1691 | `	}` |
|        - |  1692 | `	/* Top of the stack */` |
|  3328950 |  1693 | `	*ppTos = pTos;` |
|  3328950 |  1694 |  |
|        - |  1695 | `/*` |
|        - |  1696 | ` * Reserve a memory object.` |
|        - |  1697 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1698 | ` */` |
|  3027166 |  1699 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1700 |  |
|  3027168 |  1701 | `	ph7_value *pObj = 0;` |
|        - |  1702 | `	VmSlot *pSlot;` |
|        - |  1703 | `	sxu32 nIdx;` |
|        - |  1704 | `	/* Check for a free slot */` |
|  3027168 |  1705 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3027168 |  1706 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3027168 |  1707 | `	if( pSlot ){` |
|   881044 |  1708 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   881044 |  1709 | `		nIdx = pSlot->nIdx;` |
|   440521 |  1710 | `	}` |
|  3027168 |  1711 | `	if( pObj == 0 ){` |
|        - |  1712 | `		/* Reserve a new memory object */` |
|  2146126 |  1713 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2146126 |  1714 | `		if( pObj == 0 ){` |
|      ! 0 |  1715 | `			return 0;` |
|        - |  1716 | `		}` |
|  1073062 |  1717 | `	}` |
|        - |  1718 | `	/* Set a null default value */` |
|  3027168 |  1719 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3027168 |  1720 | `	pObj->nIdx = nIdx;` |
|  3027168 |  1721 | `	return pObj;` |
|  1513585 |  1722 |  |
|        - |  1723 | `/*` |
|        - |  1724 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1725 | ` */` |
|    32836 |  1726 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1727 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1728 | `	const char *zKey,  /* Entry key */` |
|        - |  1729 | `	sxu32 nByte,       /* Key length */` |
|        - |  1730 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1731 | `	)` |
|        2 |  1732 |  |
|        - |  1733 | `	ph7_value sKey;` |
|        - |  1734 | `	sxi32 rc;` |
|    32838 |  1735 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    32838 |  1736 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1737 | `	/* Perform the insertion */` |
|    32838 |  1738 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    32838 |  1739 | `	PH7_MemObjRelease(&sKey);` |
|    32838 |  1740 | `	return rc;` |
|        2 |  1741 |  |
|        - |  1742 | `/*` |
|        - |  1743 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1744 | ` * Return a pointer to the variable value on success.` |
|        - |  1745 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1746 | ` */` |
|  3105442 |  1747 | `static ph7_value * VmExtractMemObj(` |
|        - |  1748 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1749 | `	const SyString *pName, /* Variable name */` |
|        - |  1750 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1751 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1752 | `	)` |
|        2 |  1753 |  |
|  3105444 |  1754 | `	int bNullify = FALSE;` |
|        - |  1755 | `	SyHashEntry *pEntry;` |
|        - |  1756 | `	VmFrame *pFrame;` |
|        - |  1757 | `	ph7_value *pObj;` |
|        - |  1758 | `	sxu32 nIdx;` |
|        - |  1759 | `	sxi32 rc;` |
|        - |  1760 | `	/* Point to the top active frame */` |
|  3105444 |  1761 | `	pFrame = pVm->pFrame;` |
|  3105444 |  1762 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1763 | `	/* Perform the lookup */` |
|  3105444 |  1764 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1765 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1766 | `		pName = &sAnnon;` |
|        - |  1767 | `		/* Always nullify the object */` |
|      ! 0 |  1768 | `		bNullify = TRUE;` |
|      ! 0 |  1769 | `		bDup = FALSE;` |
|      ! 0 |  1770 | `	}` |
|        - |  1771 | `	/* Check the superglobals table first */` |
|  3105444 |  1772 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3105444 |  1773 | `	if( pEntry == 0 ){` |
|        - |  1774 | `		/* Query the top active frame */` |
|  3105404 |  1775 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3105404 |  1776 | `		if( pEntry == 0 ){` |
|    87052 |  1777 | `			char *zName = (char *)pName->zString;` |
|        - |  1778 | `			VmSlot sLocal;` |
|    87052 |  1779 | `			if( !bCreate ){` |
|        - |  1780 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1781 | `				return 0;` |
|        - |  1782 | `			}` |
|        - |  1783 | `			/* No such variable,automatically create a new one and install` |
|        - |  1784 | `			 * it in the current frame.` |
|        - |  1785 | `			 */` |
|    87016 |  1786 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    87016 |  1787 | `			if( pObj == 0 ){` |
|      ! 0 |  1788 | `				return 0;` |
|        - |  1789 | `			}` |
|    87016 |  1790 | `			nIdx = pObj->nIdx;` |
|    87016 |  1791 | `			if( bDup ){` |
|        - |  1792 | `				/* Duplicate name */` |
|      168 |  1793 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1794 | `				if( zName == 0 ){` |
|      ! 0 |  1795 | `					return 0;` |
|        - |  1796 | `				}` |
|       83 |  1797 | `			}` |
|        - |  1798 | `			/* Link to the top active VM frame */` |
|    87016 |  1799 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    87016 |  1800 | `			if( rc != SXRET_OK ){` |
|        - |  1801 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1802 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1803 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1804 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1805 | `				return 0;` |
|        - |  1806 | `			}` |
|    87016 |  1807 | `			if( pFrame->pParent != 0 ){` |
|        - |  1808 | `				/* Local variable */` |
|    80066 |  1809 | `				sLocal.nIdx = nIdx;` |
|    80066 |  1810 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    40034 |  1811 | `			}else{` |
|        - |  1812 | `				/* Register in the $GLOBALS array */` |
|     6952 |  1813 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1814 | `			}` |
|        - |  1815 | `			/* Install in the reference table */` |
|    87016 |  1816 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1817 | `			/* Save object index */` |
|    87016 |  1818 | `			pObj->nIdx = nIdx;` |
|    43509 |  1819 | `		}else{` |
|        - |  1820 | `			/* Extract variable contents */` |
|  3018354 |  1821 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3018354 |  1822 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3018354 |  1823 | `			if( bNullify && pObj ){` |
|      ! 0 |  1824 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1825 | `			}` |
|        - |  1826 | `		}` |
|  1552795 |  1827 | `	}else{` |
|        - |  1828 | `		/* Superglobal */` |
|       42 |  1829 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1830 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1831 | `	}` |
|  3105408 |  1832 | `	return pObj;` |
|  1552833 |  1833 |  |
|        - |  1834 | `/*` |
|        - |  1835 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1836 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1837 | ` */` |
|     2890 |  1838 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1839 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1840 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1841 | `	sxu32 nByte        /* zName length */` |
|        - |  1842 | `	)` |
|        2 |  1843 |  |
|        - |  1844 | `	SyHashEntry *pEntry;` |
|        - |  1845 | `	ph7_value *pValue;` |
|        - |  1846 | `	sxu32 nIdx;` |
|        - |  1847 | `	/* Query the superglobal table */` |
|     2892 |  1848 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2892 |  1849 | `	if( pEntry == 0 ){` |
|        - |  1850 | `		/* No such entry */` |
|      ! 0 |  1851 | `		return 0;` |
|        - |  1852 | `	}` |
|        - |  1853 | `	/* Extract the superglobal index in the global object pool */` |
|     2892 |  1854 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1855 | `	/* Extract the variable value  */` |
|     2892 |  1856 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2892 |  1857 | `	return pValue;` |
|     1447 |  1858 |  |
|        - |  1859 | `/*` |
|        - |  1860 | ` * Perform a raw hashmap insertion.` |
|        - |  1861 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1862 | ` */` |
|     2920 |  1863 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1864 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1865 | `	const char *zKey,   /* Entry key */` |
|        - |  1866 | `	int nKeylen,        /* zKey length*/` |
|        - |  1867 | `	const char *zData,  /* Entry data */` |
|        - |  1868 | `	int nLen            /* zData length */` |
|        - |  1869 | `	)` |
|        2 |  1870 |  |
|        - |  1871 | `	ph7_value sKey,sValue;` |
|        - |  1872 | `	sxi32 rc;` |
|     2922 |  1873 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2922 |  1874 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2922 |  1875 | `	if( zKey ){` |
|     2900 |  1876 | `		if( nKeylen < 0 ){` |
|     2848 |  1877 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1423 |  1878 | `		}` |
|     2900 |  1879 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1449 |  1880 | `	}` |
|     2922 |  1881 | `	if( zData ){` |
|     2922 |  1882 | `		if( nLen < 0 ){` |
|        - |  1883 | `			/* Compute length automatically */` |
|      144 |  1884 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1885 | `		}` |
|     2922 |  1886 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1460 |  1887 | `	}` |
|        - |  1888 | `	/* Perform the insertion */` |
|     2922 |  1889 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2922 |  1890 | `	PH7_MemObjRelease(&sKey);` |
|     2922 |  1891 | `	PH7_MemObjRelease(&sValue);` |
|     2922 |  1892 | `	return rc;` |
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
|    41706 |  1907 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1908 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1909 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1910 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1911 | `	)` |
|        2 |  1912 |  |
|    41708 |  1913 | `	sxi32 rc = SXRET_OK;` |
|    41708 |  1914 | `	switch(nOp){` |
|     1285 |  1915 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2572 |  1916 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2572 |  1917 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1918 | `		/* VM output consumer callback */` |
|        - |  1919 | `#ifdef UNTRUST` |
|        - |  1920 | `		if( xConsumer == 0 ){` |
|        - |  1921 | `			rc = SXERR_CORRUPT;` |
|        - |  1922 | `			break;` |
|        - |  1923 | `		}` |
|        - |  1924 | `#endif` |
|        - |  1925 | `		/* Install the output consumer */` |
|     2572 |  1926 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2572 |  1927 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2572 |  1928 | `		break;` |
|        - |  1929 | `							   }` |
|     1293 |  1930 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1931 | `		/* Import path */` |
|        - |  1932 | `		  const char *zPath;` |
|        - |  1933 | `		  SyString sPath;` |
|     2588 |  1934 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1935 | `#if defined(UNTRUST)` |
|        - |  1936 | `		  if( zPath == 0 ){` |
|        - |  1937 | `			  rc = SXERR_EMPTY;` |
|        - |  1938 | `			  break;` |
|        - |  1939 | `		  }` |
|        - |  1940 | `#endif` |
|     2588 |  1941 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1942 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1943 | `#ifdef __WINNT__` |
|        2 |  1944 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1945 | `#endif` |
|     5174 |  1946 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1947 | `		  /* Remove leading and trailing white spaces */` |
|     2588 |  1948 | `		  SyStringFullTrim(&sPath);` |
|     2588 |  1949 | `		  if( sPath.nByte > 0 ){` |
|        - |  1950 | `			  /* Store the path in the corresponding conatiner */` |
|     2588 |  1951 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1293 |  1952 | `		  }` |
|     2588 |  1953 | `		  break;` |
|        - |  1954 | `									 }` |
|     1293 |  1955 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1956 | `		/* Run-Time Error report */` |
|     2588 |  1957 | `		pVm->bErrReport = 1;` |
|     2588 |  1958 | `		break;` |
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
|    12930 |  1980 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1981 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1982 | `		/* Create a new superglobal/global variable */` |
|    25862 |  1983 | `		const char *zName = va_arg(ap,const char *);` |
|    25862 |  1984 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    25862 |  1995 | `		nByte = SyStrlen(zName);` |
|    25862 |  1996 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1997 | `			/* Check if the superglobal is already installed */` |
|    25862 |  1998 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12932 |  1999 | `		}else{` |
|        - |  2000 | `			/* Query the top active VM frame */` |
|      ! 0 |  2001 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2002 | `		}` |
|    25862 |  2003 | `		if( pEntry ){` |
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
|    25862 |  2014 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    25862 |  2015 | `			if( pObj == 0 ){` |
|      ! 0 |  2016 | `				rc = SXERR_MEM;` |
|      ! 0 |  2017 | `				break;` |
|        - |  2018 | `			}` |
|    25862 |  2019 | `			nIdx = pObj->nIdx;` |
|        - |  2020 | `			/* Copy value */` |
|    25862 |  2021 | `			PH7_MemObjStore(pValue,pObj);` |
|    25862 |  2022 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2023 | `				/* Install the superglobal */` |
|    25862 |  2024 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12932 |  2025 | `			}else{` |
|        - |  2026 | `				/* Install in the current frame */` |
|      ! 0 |  2027 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2028 | `			}` |
|    25862 |  2029 | `			if( rc == SXRET_OK ){` |
|        - |  2030 | `				SyHashEntry *pRef;` |
|    25862 |  2031 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    25862 |  2032 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12932 |  2033 | `				}else{` |
|      ! 0 |  2034 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2035 | `				}` |
|        - |  2036 | `				/* Install in the reference table */` |
|    25862 |  2037 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    25862 |  2038 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2039 | `					/* Register in the $GLOBALS array */` |
|    25862 |  2040 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12930 |  2041 | `				}` |
|    12930 |  2042 | `			}` |
|        - |  2043 | `		}` |
|    25862 |  2044 | `		break;` |
|        - |  2045 | `									}` |
|     1423 |  2046 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2047 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2048 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2049 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2050 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2051 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2052 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2848 |  2053 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2848 |  2054 | `		const char *zValue = va_arg(ap,const char *);` |
|     2848 |  2055 | `		int nLen = va_arg(ap,int);` |
|        - |  2056 | `		ph7_hashmap *pMap;` |
|        - |  2057 | `		ph7_value *pValue;` |
|     2848 |  2058 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2059 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2060 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2847 |  2061 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2062 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2063 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2846 |  2064 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2065 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2066 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2846 |  2067 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2068 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2069 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2846 |  2070 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2071 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2072 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2846 |  2073 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2074 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2075 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2076 | `		}else{` |
|        - |  2077 | `			/* Extract the $_SERVER superglobal */` |
|     2846 |  2078 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2079 | `		}` |
|     2848 |  2080 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2081 | `			/* No such entry */` |
|      ! 0 |  2082 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2083 | `			break;` |
|        - |  2084 | `		}` |
|        - |  2085 | `		/* Point to the hashmap */` |
|     2848 |  2086 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2087 | `		/* Perform the insertion */` |
|     2848 |  2088 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2848 |  2089 | `		break;` |
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
|     2586 |  2140 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2141 | `		/* Register an IO stream device */` |
|     5174 |  2142 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2143 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7758 |  2144 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5174 |  2145 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2146 | `				/* Invalid stream */` |
|      ! 0 |  2147 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2148 | `				break;` |
|        - |  2149 | `		}` |
|     5174 |  2150 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2151 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2588 |  2152 | `			pVm->pDefStream = pStream;` |
|     1293 |  2153 | `		}` |
|        - |  2154 | `		/* Insert in the appropriate container */` |
|     5174 |  2155 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5174 |  2156 | `		break;` |
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
|    41708 |  2224 | `	return rc;` |
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
|    33156 |  2714 | `static sxi32 VmByteCodeExec(` |
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
|    33158 |  2732 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    33158 |  2733 | `	if( nTos < 0 ){` |
|    31134 |  2734 | `		pTos = &pStack[-1];` |
|    15568 |  2735 | `	}else{` |
|     2026 |  2736 | `		pTos = &pStack[nTos];` |
|        - |  2737 | `	}` |
|    33158 |  2738 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    33158 |  2739 | `	pc = nPc;` |
|        - |  2740 | `	/* Execute as much as we can */` |
|  4981316 |  2741 | `	for(;;){` |
|        - |  2742 | `		/* Fetch the instruction to execute */` |
|  9961930 |  2743 | `		pInstr = &aInstr[pc];` |
|  9961930 |  2744 | `		rc = SXRET_OK;` |
|        - |  2745 | `/*` |
|        - |  2746 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2747 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2748 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2749 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2750 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2751 | ` */` |
|  9961930 |  2752 | `		switch(pInstr->iOp){` |
|        - |  2753 | `/*` |
|        - |  2754 | ` * DONE: P1 * *` |
|        - |  2755 | ` *` |
|        - |  2756 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2757 | ` * and return immediately.` |
|        - |  2758 | ` */` |
|    16267 |  2759 | `case PH7_OP_DONE:` |
|    32536 |  2760 | `	if( pInstr->iP1 ){` |
|        - |  2761 | `#ifdef UNTRUST` |
|        - |  2762 | `		if( pTos < pStack ){` |
|        - |  2763 | `			goto Abort;` |
|        - |  2764 | `		}` |
|        - |  2765 | `#endif` |
|    18854 |  2766 | `		if( pLastRef ){` |
|    12376 |  2767 | `			*pLastRef = pTos->nIdx;` |
|     6187 |  2768 | `		}` |
|    18854 |  2769 | `		if( pResult ){` |
|        - |  2770 | `			/* Execution result */` |
|    17914 |  2771 | `			PH7_MemObjStore(pTos,pResult);` |
|     8956 |  2772 | `		}` |
|    18854 |  2773 | `		VmPopOperand(&pTos,1);` |
|    23110 |  2774 | `	}else if( pLastRef ){` |
|        - |  2775 | `		/* Nothing referenced */` |
|     1002 |  2776 | `		*pLastRef = SXU32_HIGH;` |
|      500 |  2777 | `	}` |
|        - |  2778 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2779 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2780 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2781 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2782 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2783 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2784 | `	 * block can override it.` |
|        - |  2785 | `	 */` |
|    32538 |  2786 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
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
|    32536 |  2801 | `	goto Done;` |
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
|   214713 |  2846 | `case PH7_OP_JMP:` |
|   429472 |  2847 | `	pc = pInstr->iP2 - 1;` |
|   429472 |  2848 | `	break;` |
|        - |  2849 | `/*` |
|        - |  2850 | ` * JZ: P1 P2 *` |
|        - |  2851 | ` *` |
|        - |  2852 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2853 | ` * entry in the stack if P1 is zero.` |
|        - |  2854 | ` */` |
|   501843 |  2855 | `case PH7_OP_JZ:` |
|        - |  2856 | `#ifdef UNTRUST` |
|        - |  2857 | `	if( pTos < pStack ){` |
|        - |  2858 | `		goto Abort;` |
|        - |  2859 | `	}` |
|        - |  2860 | `#endif` |
|        - |  2861 | `	/* Get a boolean value */` |
|  1003776 |  2862 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2863 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2864 | `	}` |
|  1003776 |  2865 | `	if( !pTos->x.iVal ){` |
|        - |  2866 | `		/* Take the jump */` |
|   507190 |  2867 | `		pc = pInstr->iP2 - 1;` |
|   253594 |  2868 | `	}` |
|  1003776 |  2869 | `	if( !pInstr->iP1 ){` |
|   799246 |  2870 | `		VmPopOperand(&pTos,1);` |
|   399644 |  2871 | `	}` |
|  1003776 |  2872 | `	break;` |
|        - |  2873 | `/*` |
|        - |  2874 | ` * JNZ: P1 P2 *` |
|        - |  2875 | ` *` |
|        - |  2876 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2877 | ` * entry in the stack if P1 is zero.` |
|        - |  2878 | ` */` |
|    53457 |  2879 | `case PH7_OP_JNZ:` |
|        - |  2880 | `#ifdef UNTRUST` |
|        - |  2881 | `	if( pTos < pStack ){` |
|        - |  2882 | `		goto Abort;` |
|        - |  2883 | `	}` |
|        - |  2884 | `#endif` |
|        - |  2885 | `	/* Get a boolean value */` |
|   106916 |  2886 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2887 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2888 | `	}` |
|   106916 |  2889 | `	if( pTos->x.iVal ){` |
|        - |  2890 | `		/* Take the jump */` |
|     4484 |  2891 | `		pc = pInstr->iP2 - 1;` |
|     2241 |  2892 | `	}` |
|   106916 |  2893 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2894 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2895 | `	}` |
|   106916 |  2896 | `	break;` |
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
|   391819 |  2910 | `case PH7_OP_POP: {` |
|   783684 |  2911 | `	sxi32 n = pInstr->iP1;` |
|   783684 |  2912 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2913 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2914 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2915 | `	}` |
|   783684 |  2916 | `	VmPopOperand(&pTos,n);` |
|   783684 |  2917 | `	break;` |
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
|     6567 |  2940 | `case PH7_OP_NSSWITCH:` |
|    13136 |  2941 | `	SyBlobReset(&pVm->sNamespace);` |
|    13136 |  2942 | `	if( pInstr->p3 ){` |
|       53 |  2943 | `		const char *zNs = (const char *)pInstr->p3;` |
|       53 |  2944 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       26 |  2945 | `	}` |
|    13136 |  2946 | `	break;` |
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
|    12809 |  3078 | `case PH7_OP_ERR_CTRL:` |
|        - |  3079 | `	/*` |
|        - |  3080 | `	 * TICKET 1433-038:` |
|        - |  3081 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3082 | `	 * use the public API,to control error output.` |
|        - |  3083 | `	 */` |
|    25618 |  3084 | `	break;` |
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
|   835245 |  3144 | `case PH7_OP_LOADC: {` |
|        - |  3145 | `	ph7_value *pObj;` |
|        - |  3146 | `	/* Reserve a room */` |
|  1670536 |  3147 | `	pTos++;` |
|  2497537 |  3148 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1670536 |  3149 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3150 | `			SyHashEntry *pEntry;` |
|        - |  3151 | `			/* Candidate for expansion via user defined callbacks */` |
|    16580 |  3152 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16580 |  3153 | `			if( pEntry ){` |
|    16576 |  3154 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3155 | `				/* Set a NULL default value */` |
|    16576 |  3156 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16576 |  3157 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3158 | `				/* Invoke the callback and deal with the expanded value */` |
|    16576 |  3159 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3160 | `				/* Mark as constant */` |
|    16576 |  3161 | `				pTos->nIdx = SXU32_HIGH;` |
|    16576 |  3162 | `				break;` |
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
|  1653960 |  3194 | `		PH7_MemObjLoad(pObj,pTos);` |
|   827003 |  3195 | `	}else{` |
|        - |  3196 | `		/* Set a NULL value */` |
|      ! 0 |  3197 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3198 | `	}` |
|   826958 |  3199 | `LoadC_Done:` |
|        - |  3200 | `	/* Mark as constant */` |
|  1653962 |  3201 | `	pTos->nIdx = SXU32_HIGH;` |
|  1653962 |  3202 | `	break;` |
|        - |  3203 | `				  }` |
|        - |  3204 | `/*` |
|        - |  3205 | ` * LOAD: P1 * P3` |
|        - |  3206 | ` *` |
|        - |  3207 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3208 | ` * from the P3 operand.` |
|        - |  3209 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3210 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3211 | ` */` |
|  1349688 |  3212 | `case PH7_OP_LOAD:{` |
|        - |  3213 | `	ph7_value *pObj;` |
|        - |  3214 | `	SyString sName;` |
|  2699598 |  3215 | `	if( pInstr->p3 == 0 ){` |
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
|  2699580 |  3228 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3229 | `		/* Reserve a room for the target object */` |
|  2699580 |  3230 | `		pTos++;` |
|        - |  3231 | `	}` |
|        - |  3232 | `	/* Extract the requested memory object */` |
|  2699598 |  3233 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2699598 |  3234 | `	if( pObj == 0 ){` |
|       26 |  3235 | `		if( pInstr->iP1 ){` |
|        - |  3236 | `			/* Variable not found,load NULL */` |
|       26 |  3237 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3238 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3239 | `			}else{` |
|       26 |  3240 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3241 | `			}` |
|       26 |  3242 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1349702 |  3243 | `			break;` |
|      ! 0 |  3244 | `		}else{` |
|        - |  3245 | `			/* Fatal error */` |
|      ! 0 |  3246 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3247 | `			goto Abort;` |
|        - |  3248 | `		}` |
|        - |  3249 | `	}` |
|        - |  3250 | `	/* Load variable contents */` |
|  2699574 |  3251 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2699574 |  3252 | `	pTos->nIdx = pObj->nIdx;` |
|  2699574 |  3253 | `	break;` |
|        - |  3254 | `				   }` |
|        - |  3255 | `/*` |
|        - |  3256 | ` * LOAD_MAP P1 * *` |
|        - |  3257 | ` *` |
|        - |  3258 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3259 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3260 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3261 | ` */` |
|    18546 |  3262 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3263 | `	ph7_hashmap *pMap;` |
|        - |  3264 | `	/* Allocate a new hashmap instance */` |
|    37094 |  3265 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    37094 |  3266 | `	if( pMap == 0 ){` |
|      ! 0 |  3267 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3268 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3269 | `		goto Abort;` |
|        - |  3270 | `	}` |
|    37094 |  3271 | `	if( pInstr->iP1 > 0 ){` |
|     2254 |  3272 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3273 | `		/* Perform the insertion */` |
|     6894 |  3274 | `		while( pEntry < pTos ){` |
|     4642 |  3275 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3276 | `				/* Insertion by reference */` |
|      142 |  3277 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3278 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3279 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3280 | `					);` |
|       48 |  3281 | `			}else{` |
|        - |  3282 | `				/* Standard insertion */` |
|     6821 |  3283 | `				PH7_HashmapInsert(pMap,` |
|     4546 |  3284 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2273 |  3285 | `					&pEntry[1]` |
|        - |  3286 | `				);` |
|        - |  3287 | `			}` |
|        - |  3288 | `			/* Next pair on the stack */` |
|     4642 |  3289 | `			pEntry += 2;` |
|        2 |  3290 | `		}` |
|        - |  3291 | `		/* Pop P1 elements */` |
|     2254 |  3292 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1126 |  3293 | `	}` |
|        - |  3294 | `	/* Push the hashmap */` |
|    37094 |  3295 | `	pTos++;` |
|    37094 |  3296 | `	pTos->nIdx = SXU32_HIGH;` |
|    37094 |  3297 | `	pTos->x.pOther = pMap;` |
|    37094 |  3298 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    37094 |  3299 | `	break;` |
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
|   216376 |  3355 | `case PH7_OP_LOAD_IDX: {` |
|   432798 |  3356 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   432798 |  3357 | `	ph7_hashmap *pMap = 0;` |
|        - |  3358 | `	ph7_value *pIdx;` |
|   432798 |  3359 | `	pIdx = 0;` |
|   432798 |  3360 | `	if( pInstr->iP1 == 0 ){` |
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
|   432798 |  3377 | `		pIdx = pTos;` |
|   432798 |  3378 | `		pTos--;` |
|        - |  3379 | `	}` |
|   432798 |  3380 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
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
|    92124 |  3405 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3406 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3407 | `			ph7_value *pObj;` |
|      ! 0 |  3408 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3409 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3410 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3411 | `			}` |
|      ! 0 |  3412 | `		}` |
|      ! 0 |  3413 | `	}` |
|    92124 |  3414 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    92124 |  3415 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    92124 |  3416 | `		if( pInstr->iP2 ){` |
|        - |  3417 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3418 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3419 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3420 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3421 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3422 | `		}` |
|        - |  3423 | `		/* Point to the hashmap */` |
|    92124 |  3424 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    92124 |  3425 | `		if( pIdx ){` |
|        - |  3426 | `			/* Load the desired entry */` |
|    92124 |  3427 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    46061 |  3428 | `		}` |
|    92124 |  3429 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3430 | `			/* Create a new empty entry */` |
|      265 |  3431 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3432 | `			if( rc == SXRET_OK ){` |
|        - |  3433 | `				/* Point to the last inserted entry */` |
|      265 |  3434 | `				pNode = pMap->pLast;` |
|      132 |  3435 | `			}` |
|      132 |  3436 | `		}` |
|    46061 |  3437 | `	}` |
|    92124 |  3438 | `	if( pIdx ){` |
|    92124 |  3439 | `		PH7_MemObjRelease(pIdx);` |
|    46061 |  3440 | `	}` |
|    92124 |  3441 | `	if( rc == SXRET_OK ){` |
|        - |  3442 | `		/* Load entry contents */` |
|    42170 |  3443 | `		if( pMap->iRef < 2 ){` |
|        - |  3444 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3445 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3446 | `			 */` |
|       24 |  3447 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3448 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3449 | `		}else{` |
|    42148 |  3450 | `			pTos->nIdx = pNode->nValIdx;` |
|    42148 |  3451 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    42148 |  3452 | `			PH7_HashmapUnref(pMap);` |
|        - |  3453 | `		}` |
|    21086 |  3454 | `	}else{` |
|        - |  3455 | `		/* No such entry,load NULL */` |
|    49956 |  3456 | `		PH7_MemObjRelease(pTos);` |
|    49956 |  3457 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3458 | `	}` |
|    92124 |  3459 | `	break;` |
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
|   114925 |  3537 | `case PH7_OP_STORE: {` |
|        - |  3538 | `	ph7_value *pObj;` |
|        - |  3539 | `	SyString sName;` |
|        - |  3540 | `#ifdef UNTRUST` |
|        - |  3541 | `	if( pTos < pStack ){` |
|        - |  3542 | `		goto Abort;` |
|        - |  3543 | `	}` |
|        - |  3544 | `#endif` |
|   229852 |  3545 | `	if( pInstr->iP2 ){` |
|        - |  3546 | `		sxu32 nIdx;` |
|        - |  3547 | `		/* Member store operation */` |
|     2974 |  3548 | `		nIdx = pTos->nIdx;` |
|     2974 |  3549 | `		VmPopOperand(&pTos,1);` |
|     2974 |  3550 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3551 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3552 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3553 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3554 | `		}else{` |
|        - |  3555 | `			/* Point to the desired memory object */` |
|     2970 |  3556 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2970 |  3557 | `			if( pObj ){` |
|        - |  3558 | `				/* Perform the store operation */` |
|     2970 |  3559 | `				PH7_MemObjStore(pTos,pObj);` |
|     1484 |  3560 | `			}` |
|        - |  3561 | `		}` |
|   116413 |  3562 | `		break;` |
|   226880 |  3563 | `	}else if( pInstr->p3 == 0 ){` |
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
|   226874 |  3577 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3578 | `	}` |
|        - |  3579 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   226880 |  3580 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   226880 |  3581 | `	if( pObj == 0 ){` |
|      ! 0 |  3582 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3583 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3584 | `		goto Abort;` |
|        - |  3585 | `	}` |
|   226880 |  3586 | `	if( !pInstr->p3 ){` |
|        7 |  3587 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3588 | `	}` |
|        - |  3589 | `	/* Perform the store operation */` |
|   226880 |  3590 | `	PH7_MemObjStore(pTos,pObj);` |
|   226880 |  3591 | `	break;` |
|        - |  3592 | `				   }` |
|        - |  3593 | `/*` |
|        - |  3594 | ` * STORE_IDX:   P1 * P3` |
|        - |  3595 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3596 | ` *` |
|        - |  3597 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3598 | ` */` |
|    82600 |  3599 | `case PH7_OP_STORE_IDX:` |
|        - |  3600 | `case PH7_OP_STORE_IDX_REF: {` |
|   165202 |  3601 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3602 | `	ph7_value *pKey;` |
|        - |  3603 | `	sxu32 nIdx;` |
|   165202 |  3604 | `	if( pInstr->iP1 ){` |
|        - |  3605 | `		/* Key is next on stack */` |
|    57694 |  3606 | `		pKey = pTos;` |
|    57694 |  3607 | `		pTos--;` |
|    28848 |  3608 | `	}else{` |
|   107510 |  3609 | `		pKey = 0;` |
|        - |  3610 | `	}` |
|   165202 |  3611 | `	nIdx = pTos->nIdx;` |
|   165202 |  3612 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3613 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3614 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3615 | `		 * checking true sharing count, then re-add after separation. */` |
|   165150 |  3616 | `		if( nIdx != SXU32_HIGH ){` |
|   165150 |  3617 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   247724 |  3618 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   165150 |  3619 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3620 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3621 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3622 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3623 | `				 * refcounts if the backing array was already separated. */` |
|   165150 |  3624 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   165150 |  3625 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   165150 |  3626 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   165150 |  3627 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   165150 |  3628 | `					pTos->x.pOther = pMap;` |
|    82576 |  3629 | `				}else{` |
|        - |  3630 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3631 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3632 | `					pMap = pCur;` |
|        - |  3633 | `				}` |
|    82576 |  3634 | `			}else{` |
|      ! 0 |  3635 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3636 | `			}` |
|    82576 |  3637 | `		}else{` |
|      ! 0 |  3638 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3639 | `		}` |
|   165150 |  3640 | `		if( pMap->iRef < 2 ){` |
|        - |  3641 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3642 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3643 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3644 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3645 | `			pMap->iRef = 2;` |
|      ! 0 |  3646 | `		}` |
|    82576 |  3647 | `	}else{` |
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
|   165150 |  3702 | `	VmPopOperand(&pTos,1);` |
|        - |  3703 | `	/* Phase#2: Perform the insertion */` |
|   165150 |  3704 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3705 | `		/* Insertion by reference */` |
|       15 |  3706 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3707 | `	}else{` |
|   165136 |  3708 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3709 | `	}` |
|   165150 |  3710 | `	if( pKey ){` |
|    57644 |  3711 | `		PH7_MemObjRelease(pKey);` |
|    28821 |  3712 | `	}` |
|   165150 |  3713 | `	break;` |
|        - |  3714 | `					   }` |
|        - |  3715 | `/*` |
|        - |  3716 | ` * INCR: P1 * *` |
|        - |  3717 | ` *` |
|        - |  3718 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3719 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3720 | ` * the stack and increment after that.` |
|        - |  3721 | ` */` |
|   151360 |  3722 | `case PH7_OP_INCR:` |
|        - |  3723 | `#ifdef UNTRUST` |
|        - |  3724 | `	if( pTos < pStack ){` |
|        - |  3725 | `		goto Abort;` |
|        - |  3726 | `	}` |
|        - |  3727 | `#endif` |
|   302766 |  3728 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302766 |  3729 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3730 | `			ph7_value *pObj;` |
|   302766 |  3731 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3732 | `				/* Force a numeric cast */` |
|   302766 |  3733 | `				PH7_MemObjToNumeric(pObj);` |
|   302766 |  3734 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3735 | `					pObj->rVal++;` |
|        - |  3736 | `					/* Try to get an integer representation */` |
|      ! 0 |  3737 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3738 | `				}else{` |
|   302766 |  3739 | `					pObj->x.iVal++;` |
|   302766 |  3740 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3741 | `				}` |
|   302766 |  3742 | `				if( pInstr->iP1 ){` |
|        - |  3743 | `					/* Pre-icrement */` |
|       71 |  3744 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3745 | `				}` |
|   151404 |  3746 | `			}` |
|   151406 |  3747 | `		}else{` |
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
|   151404 |  3762 | `	}` |
|   302766 |  3763 | `	break;` |
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
|    23997 |  3818 | `case PH7_OP_UMINUS:` |
|        - |  3819 | `#ifdef UNTRUST` |
|        - |  3820 | `	if( pTos < pStack ){` |
|        - |  3821 | `		goto Abort;` |
|        - |  3822 | `	}` |
|        - |  3823 | `#endif` |
|        - |  3824 | `	/* Force a numeric (integer,real or both) cast */` |
|    47996 |  3825 | `	PH7_MemObjToNumeric(pTos);` |
|    47996 |  3826 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3827 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3828 | `	}` |
|    47996 |  3829 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    47966 |  3830 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23982 |  3831 | `	}` |
|    47996 |  3832 | `	break;` |
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
|    40265 |  3859 | `case PH7_OP_LNOT:` |
|        - |  3860 | `#ifdef UNTRUST` |
|        - |  3861 | `	if( pTos < pStack ){` |
|        - |  3862 | `		goto Abort;` |
|        - |  3863 | `	}` |
|        - |  3864 | `#endif` |
|        - |  3865 | `	/* Force a boolean cast */` |
|    80576 |  3866 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3867 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3868 | `	}` |
|    80576 |  3869 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    80576 |  3870 | `	break;` |
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
|      446 |  3950 | `case PH7_OP_ADD:{` |
|      894 |  3951 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3952 | `#ifdef UNTRUST` |
|        - |  3953 | `	if( pNos < pStack ){` |
|        - |  3954 | `		goto Abort;` |
|        - |  3955 | `	}` |
|        - |  3956 | `#endif` |
|        - |  3957 | `	/* Perform the addition */` |
|      894 |  3958 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      894 |  3959 | `	VmPopOperand(&pTos,1);` |
|      894 |  3960 | `	break;` |
|        - |  3961 | `				}` |
|        - |  3962 | `/*` |
|        - |  3963 | ` * OP_ADD_STORE * * *` |
|        - |  3964 | ` *` |
|        - |  3965 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3966 | ` * and push the result back onto the stack.` |
|        - |  3967 | ` */` |
|      494 |  3968 | `case PH7_OP_ADD_STORE:{` |
|      990 |  3969 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3970 | `	ph7_value *pObj;` |
|        - |  3971 | `	sxu32 nIdx;` |
|        - |  3972 | `#ifdef UNTRUST` |
|        - |  3973 | `	if( pNos < pStack ){` |
|        - |  3974 | `		goto Abort;` |
|        - |  3975 | `	}` |
|        - |  3976 | `#endif` |
|        - |  3977 | `	/* Perform the addition */` |
|      990 |  3978 | `	nIdx = pTos->nIdx;` |
|      990 |  3979 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3980 | `	/* Peform the store operation */` |
|      990 |  3981 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3982 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      990 |  3983 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      990 |  3984 | `		PH7_MemObjStore(pTos,pObj);` |
|      494 |  3985 | `	}` |
|        - |  3986 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      990 |  3987 | `	PH7_MemObjStore(pTos,pNos);` |
|      990 |  3988 | `	VmPopOperand(&pTos,1);` |
|      990 |  3989 | `	break;` |
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
|    63823 |  4473 | `case PH7_OP_CAT:{` |
|        - |  4474 | `	ph7_value *pNos,*pCur;` |
|   127648 |  4475 | `	if( pInstr->iP1 < 1 ){` |
|   100608 |  4476 | `		pNos = &pTos[-1];` |
|    50305 |  4477 | `	}else{` |
|    27042 |  4478 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4479 | `	}` |
|        - |  4480 | `#ifdef UNTRUST` |
|        - |  4481 | `	if( pNos < pStack ){` |
|        - |  4482 | `		goto Abort;` |
|        - |  4483 | `	}` |
|        - |  4484 | `#endif` |
|        - |  4485 | `	/* Force a string cast */` |
|   127648 |  4486 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1254 |  4487 | `		PH7_MemObjToString(pNos);` |
|      626 |  4488 | `	}` |
|   127648 |  4489 | `	pCur = &pNos[1];` |
|   257334 |  4490 | `	while( pCur <= pTos ){` |
|   129688 |  4491 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50644 |  4492 | `			PH7_MemObjToString(pCur);` |
|    25321 |  4493 | `		}` |
|        - |  4494 | `		/* Perform the concatenation */` |
|   129688 |  4495 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   129650 |  4496 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    64824 |  4497 | `		}` |
|   129688 |  4498 | `		SyBlobRelease(&pCur->sBlob);` |
|   129688 |  4499 | `		pCur++;` |
|        2 |  4500 | `	}` |
|   127648 |  4501 | `	pTos = pNos;` |
|   127648 |  4502 | `	break;` |
|        - |  4503 | `				}` |
|        - |  4504 | `/*  CAT_STORE: * * *` |
|        - |  4505 | ` *` |
|        - |  4506 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4507 | ` * back.` |
|        - |  4508 | ` */` |
|     3747 |  4509 | `case PH7_OP_CAT_STORE:{` |
|     7496 |  4510 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4511 | `	ph7_value *pObj;` |
|        - |  4512 | `#ifdef UNTRUST` |
|        - |  4513 | `	if( pNos < pStack ){` |
|        - |  4514 | `		goto Abort;` |
|        - |  4515 | `	}` |
|        - |  4516 | `#endif` |
|     7496 |  4517 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4518 | `		/* Force a string cast */` |
|      ! 0 |  4519 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4520 | `	}` |
|     7496 |  4521 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4522 | `		/* Force a string cast */` |
|      ! 0 |  4523 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4524 | `	}` |
|        - |  4525 | `	/* Perform the concatenation (Reverse order) */` |
|     7496 |  4526 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7496 |  4527 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3747 |  4528 | `	}` |
|        - |  4529 | `	/* Perform the store operation */` |
|     7496 |  4530 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4531 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7496 |  4532 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7496 |  4533 | `		PH7_MemObjStore(pTos,pObj);` |
|     3747 |  4534 | `	}` |
|     7496 |  4535 | `	PH7_MemObjStore(pTos,pNos);` |
|     7496 |  4536 | `	VmPopOperand(&pTos,1);` |
|     7496 |  4537 | `	break;` |
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
|    94646 |  4551 | `case PH7_OP_LAND:` |
|        - |  4552 | `case PH7_OP_LOR: {` |
|   189338 |  4553 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4554 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4555 | `#ifdef UNTRUST` |
|        - |  4556 | `	if( pNos < pStack ){` |
|        - |  4557 | `		goto Abort;` |
|        - |  4558 | `	}` |
|        - |  4559 | `#endif` |
|        - |  4560 | `	/* Force a boolean cast */` |
|   189338 |  4561 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4562 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4563 | `	}` |
|   189338 |  4564 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4565 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4566 | `	}` |
|   189338 |  4567 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   189338 |  4568 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   189338 |  4569 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4570 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    86906 |  4571 | `		v1 = and_logic[v1*3+v2];` |
|    43476 |  4572 | `	}else{` |
|        - |  4573 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102434 |  4574 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4575 | `	}` |
|   189338 |  4576 | `	if( v1 == 2 ){` |
|      ! 0 |  4577 | `		v1 = 1;` |
|      ! 0 |  4578 | `	}` |
|   189338 |  4579 | `	VmPopOperand(&pTos,1);` |
|   189338 |  4580 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   189338 |  4581 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   189338 |  4582 | `	break;` |
|        - |  4583 | `				 }` |
|        - |  4584 | `/*` |
|        - |  4585 | ` * OP_NULLC: * * *` |
|        - |  4586 | ` * Null coalescing operator '??'.` |
|        - |  4587 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  4588 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  4589 | ` */` |
|        - |  4590 | `/*` |
|        - |  4591 | ` * OP_NULLC: * P2 *` |
|        - |  4592 | ` * Short-circuit null coalescing '??'.` |
|        - |  4593 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  4594 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  4595 | ` */` |
|       19 |  4596 | `case PH7_OP_NULLC: {` |
|        - |  4597 | `#ifdef UNTRUST` |
|        - |  4598 | `	if( pTos < pStack ){` |
|        - |  4599 | `		goto Abort;` |
|        - |  4600 | `	}` |
|        - |  4601 | `#endif` |
|       39 |  4602 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  4603 | `		/* Left is not null — keep it and skip the RHS */` |
|       17 |  4604 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  4605 | `	}else{` |
|        - |  4606 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       23 |  4607 | `		VmPopOperand(&pTos, 1);` |
|        - |  4608 | `	}` |
|       39 |  4609 | `	break;` |
|        - |  4610 |  |
|        - |  4611 | `/*` |
|        - |  4612 | ` * OP_SPREAD: * * *` |
|        - |  4613 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  4614 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  4615 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  4616 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  4617 | ` */` |
|        7 |  4618 | `case PH7_OP_SPREAD: {` |
|        - |  4619 | `#ifdef UNTRUST` |
|        - |  4620 | `	if( pTos < pStack ){` |
|        - |  4621 | `		goto Abort;` |
|        - |  4622 | `	}` |
|        - |  4623 | `#endif` |
|       15 |  4624 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  4625 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  4626 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  4627 | `		if( nEntry == 0 ){` |
|        - |  4628 | `			/* Empty array — remove from stack */` |
|        3 |  4629 | `			VmPopOperand(&pTos, 1);` |
|        3 |  4630 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  4631 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  4632 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  4633 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  4634 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  4635 | `				VM_STACK_GUARD);` |
|      ! 0 |  4636 | `		}else{` |
|        - |  4637 | `			ph7_hashmap_node *pNode2;` |
|        - |  4638 | `			ph7_value *pElem;` |
|        - |  4639 | `			sxu32 i;` |
|        - |  4640 | `			/* Overwrite TOS with first element */` |
|       13 |  4641 | `			pNode2 = pMap->pFirst;` |
|       13 |  4642 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  4643 | `			PH7_MemObjRelease(pTos);` |
|       13 |  4644 | `			if( pElem ){` |
|       13 |  4645 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  4646 | `			}` |
|       13 |  4647 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  4648 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  4649 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  4650 | `			pNode2 = pNode2->pPrev;` |
|        - |  4651 | `			/* Push remaining elements */` |
|       33 |  4652 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  4653 | `				pTos++;` |
|       21 |  4654 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  4655 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  4656 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  4657 | `				if( pElem ){` |
|       21 |  4658 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  4659 | `				}` |
|       21 |  4660 | `				pNode2 = pNode2->pPrev;` |
|       11 |  4661 | `			}` |
|       13 |  4662 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  4663 | `		}` |
|        7 |  4664 | `	}` |
|        - |  4665 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  4666 | `	break;` |
|        - |  4667 |  |
|        - |  4668 | `/* OP_LXOR: * * *` |
|        - |  4669 | ` *` |
|        - |  4670 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4671 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4672 | ` * stack.` |
|        - |  4673 | ` * According to the PHP language reference manual:` |
|        - |  4674 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4675 | ` *  TRUE,but not both.` |
|        - |  4676 | ` */` |
|        5 |  4677 | `case PH7_OP_LXOR:{` |
|       11 |  4678 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4679 | `	sxi32 v = 0;` |
|        - |  4680 | `#ifdef UNTRUST` |
|        - |  4681 | `	if( pNos < pStack ){` |
|        - |  4682 | `		goto Abort;` |
|        - |  4683 | `	}` |
|        - |  4684 | `#endif` |
|        - |  4685 | `	/* Force a boolean cast */` |
|       11 |  4686 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4687 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4688 | `	}` |
|       11 |  4689 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4690 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4691 | `	}` |
|       11 |  4692 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4693 | `		v = 1;` |
|        3 |  4694 | `	}` |
|       11 |  4695 | `	VmPopOperand(&pTos,1);` |
|       11 |  4696 | `	pTos->x.iVal = v;` |
|       11 |  4697 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4698 | `	break;` |
|        - |  4699 | `				 }` |
|        - |  4700 | `/* OP_EQ P1 P2 P3` |
|        - |  4701 | ` *` |
|        - |  4702 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4703 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4704 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4705 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4706 | ` */` |
|        - |  4707 | `/* OP_NEQ P1 P2 P3` |
|        - |  4708 | ` *` |
|        - |  4709 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4710 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4711 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4712 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4713 | ` */` |
|     3945 |  4714 | `case PH7_OP_EQ:` |
|        - |  4715 | `case PH7_OP_NEQ: {` |
|     7892 |  4716 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4717 | `	/* Perform the comparison and act accordingly */` |
|        - |  4718 | `#ifdef UNTRUST` |
|        - |  4719 | `	if( pNos < pStack ){` |
|        - |  4720 | `		goto Abort;` |
|        - |  4721 | `	}` |
|        - |  4722 | `#endif` |
|     7892 |  4723 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7892 |  4724 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4725 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7883 |  4726 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7848 |  4727 | `		rc = rc == 0;` |
|     3925 |  4728 | `	}else{` |
|       28 |  4729 | `		rc = rc != 0;` |
|        - |  4730 | `	}` |
|     7892 |  4731 | `	VmPopOperand(&pTos,1);` |
|     7892 |  4732 | `	if( !pInstr->iP2 ){` |
|        - |  4733 | `		/* Push comparison result without taking the jump */` |
|     7892 |  4734 | `		PH7_MemObjRelease(pTos);` |
|     7892 |  4735 | `		pTos->x.iVal = rc;` |
|        - |  4736 | `		/* Invalidate any prior representation */` |
|     7892 |  4737 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3947 |  4738 | `	}else{` |
|      ! 0 |  4739 | `		if( rc ){` |
|        - |  4740 | `			/* Jump to the desired location */` |
|      ! 0 |  4741 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4742 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4743 | `		}` |
|        - |  4744 | `	}` |
|     7892 |  4745 | `	break;` |
|        - |  4746 | `				 }` |
|        - |  4747 | `/* OP_TEQ P1 P2 *` |
|        - |  4748 | ` *` |
|        - |  4749 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4750 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4751 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4752 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4753 | ` */` |
|   133153 |  4754 | `case PH7_OP_TEQ: {` |
|   266308 |  4755 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4756 | `	/* Perform the comparison and act accordingly */` |
|        - |  4757 | `#ifdef UNTRUST` |
|        - |  4758 | `	if( pNos < pStack ){` |
|        - |  4759 | `		goto Abort;` |
|        - |  4760 | `	}` |
|        - |  4761 | `#endif` |
|   266308 |  4762 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   266308 |  4763 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4764 | `		rc = 0;` |
|        2 |  4765 | `	}else{` |
|   266306 |  4766 | `		rc = rc == 0;` |
|        - |  4767 | `	}` |
|   266308 |  4768 | `	VmPopOperand(&pTos,1);` |
|   266308 |  4769 | `	if( !pInstr->iP2 ){` |
|        - |  4770 | `		/* Push comparison result without taking the jump */` |
|   266308 |  4771 | `		PH7_MemObjRelease(pTos);` |
|   266308 |  4772 | `		pTos->x.iVal = rc;` |
|        - |  4773 | `		/* Invalidate any prior representation */` |
|   266308 |  4774 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   133155 |  4775 | `	}else{` |
|      ! 0 |  4776 | `		if( rc ){` |
|        - |  4777 | `			/* Jump to the desired location */` |
|      ! 0 |  4778 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4779 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4780 | `		}` |
|        - |  4781 | `	}` |
|   266308 |  4782 | `	break;` |
|        - |  4783 | `				 }` |
|        - |  4784 | `/* OP_TNE P1 P2 *` |
|        - |  4785 | ` *` |
|        - |  4786 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4787 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4788 | ` * instruction.` |
|        - |  4789 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4790 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4791 | ` *` |
|        - |  4792 | ` */` |
|   103407 |  4793 | `case PH7_OP_TNE: {` |
|   206816 |  4794 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4795 | `	/* Perform the comparison and act accordingly */` |
|        - |  4796 | `#ifdef UNTRUST` |
|        - |  4797 | `	if( pNos < pStack ){` |
|        - |  4798 | `		goto Abort;` |
|        - |  4799 | `	}` |
|        - |  4800 | `#endif` |
|   206816 |  4801 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   206816 |  4802 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4803 | `		rc = 1;` |
|        2 |  4804 | `	}else{` |
|   206814 |  4805 | `		rc = rc != 0;` |
|        - |  4806 | `	}` |
|   206816 |  4807 | `	VmPopOperand(&pTos,1);` |
|   206816 |  4808 | `	if( !pInstr->iP2 ){` |
|        - |  4809 | `		/* Push comparison result without taking the jump */` |
|   206816 |  4810 | `		PH7_MemObjRelease(pTos);` |
|   206816 |  4811 | `		pTos->x.iVal = rc;` |
|        - |  4812 | `		/* Invalidate any prior representation */` |
|   206816 |  4813 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   103409 |  4814 | `	}else{` |
|      ! 0 |  4815 | `		if( rc ){` |
|        - |  4816 | `			/* Jump to the desired location */` |
|      ! 0 |  4817 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4818 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4819 | `		}` |
|        - |  4820 | `	}` |
|   206816 |  4821 | `	break;` |
|        - |  4822 | `				 }` |
|        - |  4823 | `/* OP_LT P1 P2 P3` |
|        - |  4824 | ` *` |
|        - |  4825 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4826 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4827 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4828 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4829 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4830 | ` *` |
|        - |  4831 | ` */` |
|        - |  4832 | `/* OP_LE P1 P2 P3` |
|        - |  4833 | ` *` |
|        - |  4834 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4835 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4836 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4837 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4838 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4839 | ` *` |
|        - |  4840 | ` */` |
|   102449 |  4841 | `case PH7_OP_LT:` |
|        - |  4842 | `case PH7_OP_LE: {` |
|   204944 |  4843 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4844 | `	/* Perform the comparison and act accordingly */` |
|        - |  4845 | `#ifdef UNTRUST` |
|        - |  4846 | `	if( pNos < pStack ){` |
|        - |  4847 | `		goto Abort;` |
|        - |  4848 | `	}` |
|        - |  4849 | `#endif` |
|   204944 |  4850 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204944 |  4851 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4852 | `		rc = 0;` |
|   204940 |  4853 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      430 |  4854 | `		rc = rc < 1;` |
|      216 |  4855 | `	}else{` |
|   204508 |  4856 | `		rc = rc < 0;` |
|        - |  4857 | `	}` |
|   204944 |  4858 | `	VmPopOperand(&pTos,1);` |
|   204944 |  4859 | `	if( !pInstr->iP2 ){` |
|        - |  4860 | `		/* Push comparison result without taking the jump */` |
|   204944 |  4861 | `		PH7_MemObjRelease(pTos);` |
|   204944 |  4862 | `		pTos->x.iVal = rc;` |
|        - |  4863 | `		/* Invalidate any prior representation */` |
|   204944 |  4864 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102495 |  4865 | `	}else{` |
|      ! 0 |  4866 | `		if( rc ){` |
|        - |  4867 | `			/* Jump to the desired location */` |
|      ! 0 |  4868 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4869 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4870 | `		}` |
|        - |  4871 | `	}` |
|   204944 |  4872 | `	break;` |
|        - |  4873 | `				}` |
|        - |  4874 | `/* OP_GT P1 P2 P3` |
|        - |  4875 | ` *` |
|        - |  4876 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4877 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4878 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4879 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4880 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4881 | ` *` |
|        - |  4882 | ` */` |
|        - |  4883 | `/* OP_GE P1 P2 P3` |
|        - |  4884 | ` *` |
|        - |  4885 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4886 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4887 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4888 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4889 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4890 | ` *` |
|        - |  4891 | ` */` |
|    48771 |  4892 | `case PH7_OP_GT:` |
|        - |  4893 | `case PH7_OP_GE: {` |
|    97544 |  4894 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4895 | `	/* Perform the comparison and act accordingly */` |
|        - |  4896 | `#ifdef UNTRUST` |
|        - |  4897 | `	if( pNos < pStack ){` |
|        - |  4898 | `		goto Abort;` |
|        - |  4899 | `	}` |
|        - |  4900 | `#endif` |
|    97544 |  4901 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97544 |  4902 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4903 | `		rc = 0;` |
|    97540 |  4904 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97388 |  4905 | `		rc = rc >= 0;` |
|    48695 |  4906 | `	}else{` |
|      150 |  4907 | `		rc = rc > 0;` |
|        - |  4908 | `	}` |
|    97544 |  4909 | `	VmPopOperand(&pTos,1);` |
|    97544 |  4910 | `	if( !pInstr->iP2 ){` |
|        - |  4911 | `		/* Push comparison result without taking the jump */` |
|    97544 |  4912 | `		PH7_MemObjRelease(pTos);` |
|    97544 |  4913 | `		pTos->x.iVal = rc;` |
|        - |  4914 | `		/* Invalidate any prior representation */` |
|    97544 |  4915 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48773 |  4916 | `	}else{` |
|      ! 0 |  4917 | `		if( rc ){` |
|        - |  4918 | `			/* Jump to the desired location */` |
|      ! 0 |  4919 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4920 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4921 | `		}` |
|        - |  4922 | `	}` |
|    97544 |  4923 | `	break;` |
|        - |  4924 | `				}` |
|        - |  4925 | `/* OP_SEQ P1 P2 *` |
|        - |  4926 | ` * Strict string comparison.` |
|        - |  4927 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4928 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4929 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4930 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4931 | ` * use PH7_OP_EQ.` |
|        - |  4932 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4933 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4934 | ` */` |
|        - |  4935 | `/* OP_SNE P1 P2 *` |
|        - |  4936 | ` * Strict string comparison.` |
|        - |  4937 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4938 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4939 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4940 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4941 | ` * use PH7_OP_EQ.` |
|        - |  4942 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4943 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4944 | ` */` |
|       18 |  4945 | `case PH7_OP_SEQ:` |
|        - |  4946 | `case PH7_OP_SNE: {` |
|       38 |  4947 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4948 | `	SyString s1,s2;` |
|        - |  4949 | `	/* Perform the comparison and act accordingly */` |
|        - |  4950 | `#ifdef UNTRUST` |
|        - |  4951 | `	if( pNos < pStack ){` |
|        - |  4952 | `		goto Abort;` |
|        - |  4953 | `	}` |
|        - |  4954 | `#endif` |
|        - |  4955 | `	/* Force a string cast */` |
|       38 |  4956 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4957 | `		PH7_MemObjToString(pTos);` |
|        2 |  4958 | `	}` |
|       38 |  4959 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4960 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4961 | `	}` |
|       38 |  4962 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4963 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4964 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4965 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4966 | `		rc = rc != 0;` |
|      ! 0 |  4967 | `	}else{` |
|       38 |  4968 | `		rc = rc == 0;` |
|        - |  4969 | `	}` |
|       38 |  4970 | `	VmPopOperand(&pTos,1);` |
|       38 |  4971 | `	if( !pInstr->iP2 ){` |
|        - |  4972 | `		/* Push comparison result without taking the jump */` |
|       38 |  4973 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4974 | `		pTos->x.iVal = rc;` |
|        - |  4975 | `		/* Invalidate any prior representation */` |
|       38 |  4976 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4977 | `	}else{` |
|      ! 0 |  4978 | `		if( rc ){` |
|        - |  4979 | `			/* Jump to the desired location */` |
|      ! 0 |  4980 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4981 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4982 | `		}` |
|        - |  4983 | `	}` |
|       38 |  4984 | `	break;` |
|        - |  4985 | `				 }` |
|        - |  4986 | `/*` |
|        - |  4987 | ` * OP_LOAD_REF * * *` |
|        - |  4988 | ` * Push the index of a referenced object on the stack.` |
|        - |  4989 | ` */` |
|       57 |  4990 | `case PH7_OP_LOAD_REF: {` |
|        - |  4991 | `	sxu32 nIdx;` |
|        - |  4992 | `#ifdef UNTRUST` |
|        - |  4993 | `	if( pTos < pStack ){` |
|        - |  4994 | `		goto Abort;` |
|        - |  4995 | `	}` |
|        - |  4996 | `#endif` |
|        - |  4997 | `	/* Extract memory object index */` |
|      115 |  4998 | `	nIdx = pTos->nIdx;` |
|      115 |  4999 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5000 | `		/* Nullify the object */` |
|       95 |  5001 | `		PH7_MemObjRelease(pTos);` |
|        - |  5002 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5003 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5004 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5005 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5006 | `	}` |
|      115 |  5007 | `	break;` |
|        - |  5008 | `					  }` |
|        - |  5009 | `/*` |
|        - |  5010 | ` * OP_STORE_REF * * P3` |
|        - |  5011 | ` * Perform an assignment operation by reference.` |
|        - |  5012 | ` */` |
|       15 |  5013 | ` case PH7_OP_STORE_REF: {` |
|       32 |  5014 | `	 SyString sName = { 0 , 0 };` |
|        - |  5015 | `	 VmFrame *pFrameLocal;` |
|        - |  5016 | `	SyHashEntry *pEntry;` |
|        - |  5017 | `	sxu32 nIdx;` |
|        - |  5018 | `#ifdef UNTRUST` |
|        - |  5019 | `	if( pTos < pStack ){` |
|        - |  5020 | `		goto Abort;` |
|        - |  5021 | `	}` |
|        - |  5022 | `#endif` |
|       32 |  5023 | `	if( pInstr->p3 == 0 ){` |
|        - |  5024 | `		char *zName;` |
|        - |  5025 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5026 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5027 | `			/* Force a string cast */` |
|      ! 0 |  5028 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5029 | `		}` |
|      ! 0 |  5030 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5031 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5032 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5033 | `			if( zName ){` |
|      ! 0 |  5034 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5035 | `			}` |
|      ! 0 |  5036 | `		}` |
|      ! 0 |  5037 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5038 | `		pTos--;` |
|      ! 0 |  5039 | `	}else{` |
|       32 |  5040 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5041 | `	}` |
|       32 |  5042 | `	nIdx = pTos->nIdx;` |
|       32 |  5043 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5044 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5045 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5046 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5047 | `		}else{` |
|        - |  5048 | `			ph7_value *pObj;` |
|        - |  5049 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5050 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5051 | `			if( pObj == 0 ){` |
|      ! 0 |  5052 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5053 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5054 | `				goto Abort;` |
|        - |  5055 | `			}` |
|        - |  5056 | `			/* Perform the store operation */` |
|      ! 0 |  5057 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5058 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5059 | `		}` |
|       32 |  5060 | `	}else if( sName.nByte > 0){` |
|       32 |  5061 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5062 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5063 | `		}else{` |
|       32 |  5064 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  5065 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5066 | `			/* Query the local frame */` |
|       32 |  5067 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  5068 | `			if( pEntry ){` |
|      ! 0 |  5069 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5070 | `			}else{` |
|       32 |  5071 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  5072 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5073 | `					/* Insert in the $GLOBALS array */` |
|       28 |  5074 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  5075 | `				}` |
|       32 |  5076 | `				if( rc == SXRET_OK ){` |
|       32 |  5077 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  5078 | `				}` |
|        - |  5079 | `			}` |
|        - |  5080 | `		}` |
|       15 |  5081 | `	}` |
|       32 |  5082 | `	break;` |
|        - |  5083 | `				 }` |
|        - |  5084 | `/*` |
|        - |  5085 | ` * OP_UPLINK P1 * *` |
|        - |  5086 | ` * Link a variable to the top active VM frame.` |
|        - |  5087 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5088 | ` */` |
|       25 |  5089 | `case PH7_OP_UPLINK: {` |
|       52 |  5090 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5091 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5092 | `		SyString sName;` |
|        - |  5093 | `		/* Perform the link */` |
|      104 |  5094 | `		while( pLink <= pTos ){` |
|       54 |  5095 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5096 | `				/* Force a string cast */` |
|      ! 0 |  5097 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5098 | `			}` |
|       54 |  5099 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5100 | `			if( sName.nByte > 0 ){` |
|       54 |  5101 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5102 | `			}` |
|       54 |  5103 | `			pLink++;` |
|        2 |  5104 | `		}` |
|       25 |  5105 | `	}` |
|       52 |  5106 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5107 | `	break;` |
|        - |  5108 | `					}` |
|        - |  5109 | `/*` |
|        - |  5110 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5111 | ` * Push an exception in the corresponding container so that` |
|        - |  5112 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5113 | ` */` |
|       32 |  5114 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5115 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5116 | `	VmFrame *pFrameLocal;` |
|        - |  5117 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5118 | `	pException->iFinallyDone = 0;` |
|       66 |  5119 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5120 | `	/* Create the exception frame */` |
|       66 |  5121 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5122 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5123 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5124 | `		goto Abort;` |
|        - |  5125 | `	}` |
|        - |  5126 | `	/* Mark the special frame */` |
|       66 |  5127 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5128 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5129 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5130 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5131 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5132 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5133 | `	break;` |
|        - |  5134 | `							}` |
|        - |  5135 | `/*` |
|        - |  5136 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5137 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5138 | ` */` |
|       31 |  5139 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5140 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5141 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5142 | `		ph7_exception **apException;` |
|        - |  5143 | `		/* Pop the loaded exception */` |
|       28 |  5144 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5145 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5146 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5147 | `		}` |
|       13 |  5148 | `	}` |
|       64 |  5149 | `	pException->pFrame = 0;` |
|        - |  5150 | `	/* Leave the exception frame */` |
|       64 |  5151 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5152 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5153 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5154 | `		sxi32 rcFinally;` |
|       19 |  5155 | `		pException->iFinallyDone = 1;` |
|       19 |  5156 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  5157 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5158 | `			goto Abort;` |
|        - |  5159 | `		}` |
|        9 |  5160 | `	}` |
|       64 |  5161 | `	break;` |
|        - |  5162 | `							}` |
|        - |  5163 |  |
|        - |  5164 | `/*` |
|        - |  5165 | ` * OP_THROW * P2 *` |
|        - |  5166 | ` * Throw an user exception.` |
|        - |  5167 | ` */` |
|       18 |  5168 | `case PH7_OP_THROW: {` |
|       38 |  5169 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5170 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5171 | `#ifdef UNTRUST` |
|        - |  5172 | `	if( pTos < pStack ){` |
|        - |  5173 | `		goto Abort;` |
|        - |  5174 | `	}` |
|        - |  5175 | `#endif` |
|       38 |  5176 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5177 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5178 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5179 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5180 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5181 | `		ph7_class *pException;` |
|        - |  5182 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5183 | `		 */` |
|       38 |  5184 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5185 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5186 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5187 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5188 | `			if( rc == SXERR_ABORT ){` |
|        - |  5189 | `				/* Abort processing immediately */` |
|      ! 0 |  5190 | `				goto Abort;` |
|        - |  5191 | `			}` |
|      ! 0 |  5192 | `		}else{` |
|        - |  5193 | `			/* Throw the exception */` |
|       38 |  5194 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5195 | `			if( rc == SXERR_ABORT ){` |
|        - |  5196 | `				/* Abort processing immediately */` |
|        9 |  5197 | `				goto Abort;` |
|        - |  5198 | `			}` |
|        - |  5199 | `		}` |
|       16 |  5200 | `	}else{` |
|        - |  5201 | `		/* Expecting a class instance */` |
|      ! 0 |  5202 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5203 | `		if( rc == SXERR_ABORT ){` |
|        - |  5204 | `			/* Abort processing immediately */` |
|      ! 0 |  5205 | `			goto Abort;` |
|        - |  5206 | `		}` |
|        - |  5207 | `	}` |
|        - |  5208 | `	/* Pop the top entry */` |
|       30 |  5209 | `	VmPopOperand(&pTos,1);` |
|        - |  5210 | `	/* Perform an unconditional jump */` |
|       30 |  5211 | `	pc = nJump - 1;` |
|       30 |  5212 | `	break;` |
|        - |  5213 | `				   }` |
|        - |  5214 | `/*` |
|        - |  5215 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5216 | ` * Prepare a foreach step.` |
|        - |  5217 | ` */` |
|     4999 |  5218 | `case PH7_OP_FOREACH_INIT: {` |
|    10000 |  5219 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5220 | `	void *pName;` |
|        - |  5221 | `#ifdef UNTRUST` |
|        - |  5222 | `	if( pTos < pStack ){` |
|        - |  5223 | `		goto Abort;` |
|        - |  5224 | `	}` |
|        - |  5225 | `#endif` |
|    10000 |  5226 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5227 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5228 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5229 | `			/* Force a string cast */` |
|      ! 0 |  5230 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5231 | `		}` |
|        - |  5232 | `		/* Duplicate name */` |
|      ! 0 |  5233 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5234 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5235 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5236 | `		}` |
|      ! 0 |  5237 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5238 | `	}` |
|    10000 |  5239 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5240 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5241 | `			/* Force a string cast */` |
|      ! 0 |  5242 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5243 | `		}` |
|        - |  5244 | `		/* Duplicate name */` |
|      ! 0 |  5245 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5246 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5247 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5248 | `		}` |
|      ! 0 |  5249 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5250 | `	}` |
|        - |  5251 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10000 |  5252 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5253 | `		/* Jump out of the loop */` |
|      ! 0 |  5254 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5255 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5256 | `		}` |
|      ! 0 |  5257 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5258 | `	}else{` |
|        - |  5259 | `		ph7_foreach_step *pStep;` |
|    10000 |  5260 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10000 |  5261 | `		if( pStep == 0 ){` |
|      ! 0 |  5262 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5263 | `			/* Jump out of the loop */` |
|      ! 0 |  5264 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5265 | `		}else{` |
|        - |  5266 | `			/* Zero the structure */` |
|    10000 |  5267 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5268 | `			/* Prepare the step */` |
|    10000 |  5269 | `			pStep->iFlags = pInfo->iFlags;` |
|    10000 |  5270 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5271 | `				ph7_hashmap *pMap;` |
|        - |  5272 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5273 | `				 * source array so mutations don't affect other sharers. */` |
|     9972 |  5274 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|       10 |  5275 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|       10 |  5276 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|       10 |  5277 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5278 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5279 | `						 * variable still points at the same hashmap as` |
|        - |  5280 | `						 * the stack value. */` |
|       10 |  5281 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|       10 |  5282 | `							pCur->iRef--;` |
|       10 |  5283 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|       10 |  5284 | `							pTos->x.pOther = pBacking->x.pOther;` |
|       10 |  5285 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5286 | `						}` |
|        4 |  5287 | `					}` |
|        4 |  5288 | `				}` |
|     9972 |  5289 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5290 | `				/* Reset the internal loop cursor */` |
|     9972 |  5291 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5292 | `				/* Mark the step */` |
|     9972 |  5293 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9972 |  5294 | `				pStep->xIter.pMap = pMap;` |
|     9972 |  5295 | `				pMap->iRef++;` |
|     4987 |  5296 | `			}else{` |
|       30 |  5297 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5298 | `				ph7_class *pIteratorClass;` |
|        - |  5299 | `				/* Check if the object implements Iterator */` |
|       30 |  5300 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5301 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5302 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5303 | `					ph7_class_method *pRewind;` |
|       19 |  5304 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       19 |  5305 | `					pStep->xIter.pThis = pThis;` |
|       19 |  5306 | `					pThis->iRef++;` |
|       19 |  5307 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       19 |  5308 | `					if( pRewind ){` |
|       19 |  5309 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5310 | `					}` |
|       10 |  5311 | `				}else{` |
|        - |  5312 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5313 | `					ph7_class *pIterAggClass;` |
|       12 |  5314 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5315 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5316 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5317 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5318 | `						ph7_class_method *pGetIter;` |
|        3 |  5319 | `						int iterAggOk = 0;` |
|        3 |  5320 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5321 | `						if( pGetIter ){` |
|        - |  5322 | `							ph7_value sResult;` |
|        3 |  5323 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5324 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5325 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5326 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5327 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5328 | `									ph7_class_method *pRewind;` |
|        3 |  5329 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5330 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5331 | `									pIterObj->iRef++;` |
|        - |  5332 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5333 | `									pStep->pOwner = pThis;` |
|        3 |  5334 | `									pThis->iRef++;` |
|        3 |  5335 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5336 | `									if( pRewind ){` |
|        3 |  5337 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5338 | `									}` |
|        3 |  5339 | `									iterAggOk = 1;` |
|        1 |  5340 | `								}` |
|        1 |  5341 | `							}` |
|        3 |  5342 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5343 | `						}` |
|        3 |  5344 | `						if( !iterAggOk ){` |
|        - |  5345 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5346 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5347 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5348 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5349 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5350 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5351 | `						}` |
|        2 |  5352 | `					}else{` |
|        - |  5353 | `						/* Plain object iteration via hAttr */` |
|        9 |  5354 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5355 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5356 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5357 | `						pThis->iRef++;` |
|        - |  5358 | `					}` |
|        - |  5359 | `				}` |
|        - |  5360 | `			}` |
|        - |  5361 | `		}` |
|    10000 |  5362 | `		if( pStep ){` |
|    10000 |  5363 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5364 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5365 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5366 | `				/* Jump out of the loop */` |
|      ! 0 |  5367 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5368 | `			}` |
|     4999 |  5369 | `		}` |
|        - |  5370 | `	}` |
|    10000 |  5371 | `	VmPopOperand(&pTos,1);` |
|    10000 |  5372 | `	break;` |
|        - |  5373 | `						  }` |
|        - |  5374 | `/*` |
|        - |  5375 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5376 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5377 | ` */` |
|    80760 |  5378 | `case PH7_OP_FOREACH_STEP: {` |
|   161522 |  5379 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5380 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5381 | `	ph7_value *pValue;` |
|        - |  5382 | `	VmFrame *pFrameLocal;` |
|        - |  5383 | `	/* Peek the last step */` |
|   161522 |  5384 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   161522 |  5385 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   161522 |  5386 | `	pFrameLocal = pVm->pFrame;` |
|   161522 |  5387 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   161522 |  5388 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   161410 |  5389 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5390 | `		ph7_hashmap_node *pNode;` |
|        - |  5391 | `		/* Extract the current node value */` |
|   161410 |  5392 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   161410 |  5393 | `		if( pNode == 0 ){` |
|        - |  5394 | `			/* No more entry to process */` |
|     9970 |  5395 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9970 |  5396 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5397 | `				/* Break the reference with the last element */` |
|        7 |  5398 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5399 | `			}` |
|        - |  5400 | `			/* Automatically reset the loop cursor */` |
|     9970 |  5401 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5402 | `			/* Cleanup the mess left behind */` |
|     9970 |  5403 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9970 |  5404 | `			SySetPop(&pInfo->aStep);` |
|     9970 |  5405 | `			PH7_HashmapUnref(pMap);` |
|     4986 |  5406 | `		}else{` |
|   151442 |  5407 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5408 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5409 | `				if( pKey ){` |
|      416 |  5410 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5411 | `				}` |
|      207 |  5412 | `			}` |
|   151442 |  5413 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5414 | `				SyHashEntry *pEntry;` |
|        - |  5415 | `				/* Pass by reference */` |
|       24 |  5416 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       24 |  5417 | `				if( pEntry ){` |
|       22 |  5418 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5419 | `				}else{` |
|        4 |  5420 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5421 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5422 | `				}` |
|       13 |  5423 | `			}else{` |
|        - |  5424 | `				/* Make a copy of the entry value */` |
|   151420 |  5425 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   151420 |  5426 | `				if( pValue ){` |
|   151420 |  5427 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    75709 |  5428 | `				}` |
|        - |  5429 | `			}` |
|        2 |  5430 | `		}` |
|    80818 |  5431 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5432 | `		/* Iterator-based iteration.` |
|        - |  5433 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5434 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5435 | `		 */` |
|       89 |  5436 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5437 | `		ph7_class_method *pMethod;` |
|        - |  5438 | `		ph7_value sResult;` |
|       89 |  5439 | `		int isValid = 0;` |
|        - |  5440 | `		/* Call next() to advance — but skip on the first iteration */` |
|       89 |  5441 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       21 |  5442 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       11 |  5443 | `		}else{` |
|       69 |  5444 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       69 |  5445 | `			if( pMethod ){` |
|       69 |  5446 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5447 | `			}` |
|        - |  5448 | `		}` |
|        - |  5449 | `		/* Call valid() */` |
|       89 |  5450 | `		PH7_MemObjInit(pVm,&sResult);` |
|       89 |  5451 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       89 |  5452 | `		if( pMethod ){` |
|       89 |  5453 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       89 |  5454 | `			PH7_MemObjToBool(&sResult);` |
|       89 |  5455 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5456 | `		}` |
|       89 |  5457 | `		PH7_MemObjRelease(&sResult);` |
|       89 |  5458 | `		if( !isValid ){` |
|        - |  5459 | `			/* Iterator exhausted */` |
|       19 |  5460 | `			pc = pInstr->iP2 - 1;` |
|        - |  5461 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       19 |  5462 | `			if( pStep->pOwner ){` |
|        3 |  5463 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5464 | `			}` |
|       19 |  5465 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       19 |  5466 | `			SySetPop(&pInfo->aStep);` |
|       19 |  5467 | `			PH7_ClassInstanceUnref(pThis);` |
|       10 |  5468 | `		}else{` |
|        - |  5469 | `			/* Call current() to get value */` |
|       71 |  5470 | `			PH7_MemObjInit(pVm,&sResult);` |
|       71 |  5471 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       71 |  5472 | `			if( pMethod ){` |
|       71 |  5473 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5474 | `			}` |
|       71 |  5475 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       71 |  5476 | `			if( pValue ){` |
|       71 |  5477 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5478 | `			}` |
|       71 |  5479 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5480 | `			/* Call key() if needed */` |
|       71 |  5481 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5482 | `				ph7_value sKey;` |
|       35 |  5483 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5484 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5485 | `				if( pMethod ){` |
|       35 |  5486 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5487 | `				}` |
|       35 |  5488 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5489 | `				if( pValue ){` |
|       35 |  5490 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5491 | `				}` |
|       35 |  5492 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5493 | `			}` |
|        - |  5494 | `		}` |
|       45 |  5495 | `	}else{` |
|       25 |  5496 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5497 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5498 | `		SyHashEntry *pEntry;` |
|        - |  5499 | `		/* Point to the next attribute */` |
|       29 |  5500 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5501 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5502 | `			/* Check access permission */` |
|       31 |  5503 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5504 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5505 | `					break; /* Access is granted */` |
|        - |  5506 | `			}` |
|        1 |  5507 | `		}` |
|       25 |  5508 | `		if( pEntry == 0 ){` |
|        - |  5509 | `			/* Clean up the mess left behind */` |
|        9 |  5510 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5511 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5512 | `				/* Break the reference with the last element */` |
|        3 |  5513 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5514 | `			}` |
|        9 |  5515 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5516 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5517 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5518 | `		}else{` |
|       17 |  5519 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5520 | `			ph7_value *pAttrValue;` |
|       17 |  5521 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5522 | `				/* Fill with the current attribute name */` |
|       17 |  5523 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5524 | `				if( pKey ){` |
|       17 |  5525 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5526 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5527 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5528 | `				}` |
|        8 |  5529 | `			}` |
|        - |  5530 | `			/* Extract attribute value */` |
|       17 |  5531 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5532 | `			if( pAttrValue ){` |
|       17 |  5533 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5534 | `					/* Pass by reference */` |
|        3 |  5535 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5536 | `					if( pEntry ){` |
|        3 |  5537 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5538 | `					}else{` |
|      ! 0 |  5539 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5540 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5541 | `					}` |
|        2 |  5542 | `				}else{` |
|        - |  5543 | `					/* Make a copy of the attribute value */` |
|       15 |  5544 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5545 | `					if( pValue ){` |
|       15 |  5546 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5547 | `					}` |
|        - |  5548 | `				}` |
|        8 |  5549 | `			}` |
|        - |  5550 | `		}` |
|        - |  5551 | `	}` |
|   161522 |  5552 | `	break;` |
|        - |  5553 | `						  }` |
|        - |  5554 | `/*` |
|        - |  5555 | ` * OP_MEMBER P1 P2` |
|        - |  5556 | ` * Load class attribute/method on the stack.` |
|        - |  5557 | ` */` |
|     2198 |  5558 | `case PH7_OP_MEMBER: {` |
|        - |  5559 | `	ph7_class_instance *pThis;` |
|        - |  5560 | `	ph7_value *pNos;` |
|        - |  5561 | `	SyString sName;` |
|     4398 |  5562 | `	if( !pInstr->iP1 ){` |
|     4262 |  5563 | `		pNos = &pTos[-1];` |
|        - |  5564 | `#ifdef UNTRUST` |
|        - |  5565 | `		if( pNos < pStack ){` |
|        - |  5566 | `			goto Abort;` |
|        - |  5567 | `		}` |
|        - |  5568 | `#endif` |
|     4262 |  5569 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5570 | `			ph7_class *pClass;` |
|        - |  5571 | `			/* Class already instantiated */` |
|     4262 |  5572 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5573 | `			/* Point to the instantiated class */` |
|     4262 |  5574 | `			pClass = pThis->pClass;` |
|        - |  5575 | `			/* Extract attribute name first */` |
|     4262 |  5576 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4262 |  5577 | `			if( pInstr->iP2 ){` |
|        - |  5578 | `				/* Method call */` |
|      418 |  5579 | `				ph7_class_method *pMeth = 0;` |
|      418 |  5580 | `				if( sName.nByte > 0 ){` |
|        - |  5581 | `					/* Extract the target method */` |
|      418 |  5582 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      208 |  5583 | `				}` |
|      418 |  5584 | `				if( pMeth == 0 ){` |
|      ! 0 |  5585 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5586 | `						&pClass->sName,&sName` |
|        - |  5587 | `						);` |
|        - |  5588 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5589 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5590 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5591 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5592 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5593 | `				}else{` |
|        - |  5594 | `					/* Push method name on the stack */` |
|      418 |  5595 | `					PH7_MemObjRelease(pTos);` |
|      418 |  5596 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      418 |  5597 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5598 | `				}` |
|      418 |  5599 | `				pTos->nIdx = SXU32_HIGH;` |
|      210 |  5600 | `			}else{` |
|        - |  5601 | `				/* Attribute access */` |
|     3846 |  5602 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5603 | `				SyHashEntry *pEntry;` |
|        - |  5604 | `				/* Extract the target attribute */` |
|     3846 |  5605 | `				if( sName.nByte > 0 ){` |
|     3846 |  5606 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3846 |  5607 | `					if( pEntry ){` |
|        - |  5608 | `						/* Point to the attribute value */` |
|     3844 |  5609 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1921 |  5610 | `					}` |
|     1922 |  5611 | `				}` |
|     3846 |  5612 | `				if( pObjAttr == 0 ){` |
|        - |  5613 | `					/* No such attribute,load null */` |
|        4 |  5614 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5615 | `						&pClass->sName,&sName);` |
|        - |  5616 | `					/* Call the __get magic method if available */` |
|        3 |  5617 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5618 | `				}` |
|     3846 |  5619 | `				VmPopOperand(&pTos,1);` |
|        - |  5620 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5621 | `				 * This is due to the following case:` |
|        - |  5622 | `				 *     (new TestClass())->foo;` |
|        - |  5623 | `				 */` |
|     3846 |  5624 | `				pThis->iRef++;` |
|     3846 |  5625 | `				PH7_MemObjRelease(pTos);` |
|     3846 |  5626 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3846 |  5627 | `				if( pObjAttr ){` |
|     3844 |  5628 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5629 | `					/* Check attribute access */` |
|     3844 |  5630 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5631 | `						/* Load attribute */` |
|     3844 |  5632 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3844 |  5633 | `						if( pValue ){` |
|     3844 |  5634 | `							if( pThis->iRef < 2 ){` |
|        - |  5635 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5636 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5637 | `								 */` |
|        3 |  5638 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5639 | `							}else{` |
|        - |  5640 | `								/* Simple load */` |
|     3842 |  5641 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5642 | `							}` |
|     3844 |  5643 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3842 |  5644 | `								if( pThis->iRef > 1 ){` |
|        - |  5645 | `									/* Load attribute index */` |
|     3840 |  5646 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1919 |  5647 | `								}` |
|     1920 |  5648 | `							}` |
|     1921 |  5649 | `						}` |
|     1921 |  5650 | `					}` |
|     1921 |  5651 | `				}` |
|        - |  5652 | `				/* Safely unreference the object */` |
|     3846 |  5653 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5654 | `			}` |
|     2132 |  5655 | `		}else{` |
|      ! 0 |  5656 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5657 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5658 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5659 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5660 | `		}` |
|     2132 |  5661 | `	}else{` |
|        - |  5662 | `		/* Static member access using class name */` |
|      138 |  5663 | `		pNos = pTos;` |
|      138 |  5664 | `		pThis = 0;` |
|      138 |  5665 | `		if( !pInstr->p3 ){` |
|      126 |  5666 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      126 |  5667 | `			pNos--;` |
|        - |  5668 | `#ifdef UNTRUST` |
|        - |  5669 | `			if( pNos < pStack ){` |
|        - |  5670 | `				goto Abort;` |
|        - |  5671 | `			}` |
|        - |  5672 | `#endif` |
|       64 |  5673 | `		}else{` |
|        - |  5674 | `			/* Attribute name already computed */` |
|       14 |  5675 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5676 | `		}` |
|      138 |  5677 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      138 |  5678 | `			ph7_class *pClass = 0;` |
|      138 |  5679 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5680 | `				/* Class already instantiated */` |
|      ! 0 |  5681 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5682 | `				pClass = pThis->pClass;` |
|      ! 0 |  5683 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5684 | `			}else{` |
|        - |  5685 | `				/* Try to extract the target class */` |
|      138 |  5686 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      138 |  5687 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      138 |  5688 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5689 | `					/* Handle self/static/parent keywords */` |
|      138 |  5690 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5691 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5692 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5693 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5694 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5695 | `						}` |
|      124 |  5696 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5697 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      109 |  5698 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5699 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5700 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5701 | `							pClass = pSelf->pBase;` |
|        6 |  5702 | `						}` |
|        8 |  5703 | `					}else{` |
|       84 |  5704 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5705 | `					}` |
|       68 |  5706 | `				}` |
|        - |  5707 | `			}` |
|      138 |  5708 | `			if( pClass == 0 ){` |
|        - |  5709 | `				/* Undefined class */` |
|      ! 0 |  5710 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5711 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5712 | `					);` |
|      ! 0 |  5713 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5714 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5715 | `				}` |
|      ! 0 |  5716 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5717 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5718 | `			}else{` |
|      138 |  5719 | `				if( pInstr->iP2 ){` |
|        - |  5720 | `					/* Method call */` |
|       68 |  5721 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5722 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5723 | `						/* Extract the target method */` |
|       68 |  5724 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5725 | `					}` |
|       68 |  5726 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5727 | `						if( pMeth ){` |
|      ! 0 |  5728 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5729 | `								&pClass->sName,&sName` |
|        - |  5730 | `								);` |
|      ! 0 |  5731 | `						}else{` |
|      ! 0 |  5732 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5733 | `								&pClass->sName,&sName` |
|        - |  5734 | `								);` |
|        - |  5735 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5736 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5737 | `						}` |
|        - |  5738 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5739 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5740 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5741 | `						}` |
|      ! 0 |  5742 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5743 | `					}else{` |
|        - |  5744 | `						/* Push method name on the stack */` |
|       68 |  5745 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5746 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5747 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5748 | `					}` |
|       68 |  5749 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5750 | `				}else{` |
|        - |  5751 | `					/* Attribute access */` |
|       72 |  5752 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5753 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5754 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5755 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5756 | `						/* ::class returns the fully qualified class name */` |
|        - |  5757 | `						/* Pop the attribute name from the stack */` |
|       54 |  5758 | `						if( !pInstr->p3 ){` |
|       54 |  5759 | `							VmPopOperand(&pTos,1);` |
|       26 |  5760 | `						}` |
|       54 |  5761 | `						PH7_MemObjRelease(pTos);` |
|        - |  5762 | `						/* Load the class name */` |
|       54 |  5763 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5764 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5765 | `					}else{` |
|        - |  5766 | `						/* Extract the target attribute */` |
|       20 |  5767 | `						if( sName.nByte > 0 ){` |
|       20 |  5768 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5769 | `						}` |
|       20 |  5770 | `						if( pAttr == 0 ){` |
|        - |  5771 | `							/* No such attribute,load null */` |
|      ! 0 |  5772 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5773 | `								&pClass->sName,&sName);` |
|        - |  5774 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5775 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5776 | `						}` |
|        - |  5777 | `						/* Pop the attribute name from the stack */` |
|       20 |  5778 | `						if( !pInstr->p3 ){` |
|        7 |  5779 | `							VmPopOperand(&pTos,1);` |
|        3 |  5780 | `						}` |
|       20 |  5781 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5782 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5783 | `						if( pAttr ){` |
|       20 |  5784 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5785 | `								/* Access to a non static attribute */` |
|      ! 0 |  5786 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5787 | `									&pClass->sName,&pAttr->sName` |
|        - |  5788 | `									);` |
|      ! 0 |  5789 | `							}else{` |
|        - |  5790 | `								ph7_value *pValue;` |
|        - |  5791 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5792 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5793 | `									/* Load the desired attribute */` |
|       20 |  5794 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5795 | `									if( pValue ){` |
|       20 |  5796 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5797 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5798 | `											/* Load index number */` |
|       14 |  5799 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5800 | `										}` |
|        9 |  5801 | `									}` |
|        9 |  5802 | `								}` |
|        - |  5803 | `							}` |
|        9 |  5804 | `						}` |
|        - |  5805 | `					}` |
|        - |  5806 | `				}` |
|      138 |  5807 | `				if( pThis ){` |
|        - |  5808 | `					/* Safely unreference the object */` |
|      ! 0 |  5809 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5810 | `				}` |
|        - |  5811 | `			}` |
|       70 |  5812 | `		}else{` |
|        - |  5813 | `			/* Pop operands */` |
|      ! 0 |  5814 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5815 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5816 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5817 | `			}` |
|      ! 0 |  5818 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5819 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5820 | `		}` |
|        - |  5821 | `	}` |
|     4398 |  5822 | `	break;` |
|        - |  5823 | `					}` |
|        - |  5824 | `/*` |
|        - |  5825 | ` * OP_NEW P1 * * *` |
|        - |  5826 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5827 | ` */` |
|      323 |  5828 | `case PH7_OP_NEW: {` |
|      648 |  5829 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      648 |  5830 | `	ph7_class *pClass = 0;` |
|        - |  5831 | `	ph7_class_instance *pNew;` |
|      648 |  5832 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5833 | `		/* Try to extract the desired class */` |
|      971 |  5834 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      646 |  5835 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      323 |  5836 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5837 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5838 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5839 | `	}` |
|      648 |  5840 | `	if( pClass == 0 ){` |
|        - |  5841 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5842 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5843 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5844 | `			);` |
|        - |  5845 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5846 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5847 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5848 | `			/* Pop given arguments */` |
|      ! 0 |  5849 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5850 | `		}` |
|      ! 0 |  5851 | `		goto Abort;` |
|      ! 0 |  5852 | `	}else{` |
|        - |  5853 | `		ph7_class_method *pCons;` |
|        - |  5854 | `		/* Create a new class instance */` |
|      648 |  5855 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      648 |  5856 | `		if( pNew == 0 ){` |
|      ! 0 |  5857 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5858 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5859 | `				&pClass->sName` |
|        - |  5860 | `			);` |
|      ! 0 |  5861 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5862 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5863 | `				/* Pop given arguments */` |
|      ! 0 |  5864 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5865 | `			}` |
|      ! 0 |  5866 | `			break;` |
|        - |  5867 | `		}` |
|        - |  5868 | `		/* Check if a constructor is available */` |
|      648 |  5869 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      648 |  5870 | `		if( pCons == 0 ){` |
|      534 |  5871 | `			SyString *pName = &pClass->sName;` |
|        - |  5872 | `			/* Check for a constructor with the same base class name */` |
|      534 |  5873 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      266 |  5874 | `		}` |
|      648 |  5875 | `		if( pCons ){` |
|        - |  5876 | `			/* Call the class constructor */` |
|      116 |  5877 | `			SySetReset(&aArg);` |
|      220 |  5878 | `			while( pArg < pTos ){` |
|      106 |  5879 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      106 |  5880 | `				pArg++;` |
|        2 |  5881 | `			}` |
|      116 |  5882 | `			if( pVm->bErrReport ){` |
|        - |  5883 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5884 | `				sxu32 n;` |
|       73 |  5885 | `				n = SySetUsed(&aArg);` |
|        - |  5886 | `				/* Emit a notice for missing arguments */` |
|      137 |  5887 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       65 |  5888 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       65 |  5889 | `					if( pFuncArg ){` |
|       65 |  5890 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5891 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5892 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5893 | `						}` |
|       32 |  5894 | `					}` |
|       65 |  5895 | `					n++;` |
|        1 |  5896 | `				}` |
|       36 |  5897 | `			}` |
|      116 |  5898 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5899 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      116 |  5900 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5901 | `				pNew->iRef = 1;` |
|      ! 0 |  5902 | `			}` |
|       57 |  5903 | `		}` |
|      648 |  5904 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5905 | `			/* Pop given arguments */` |
|       98 |  5906 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       48 |  5907 | `		}` |
|      648 |  5908 | `		PH7_MemObjRelease(pTos);` |
|      648 |  5909 | `		pTos->x.pOther = pNew;` |
|      648 |  5910 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5911 | `	}` |
|      648 |  5912 | `	break;` |
|        - |  5913 | `				 }` |
|        - |  5914 | `/*` |
|        - |  5915 | ` * OP_CLONE * * *` |
|        - |  5916 | ` * Perfome a clone operation.` |
|        - |  5917 | ` */` |
|       23 |  5918 | `case PH7_OP_CLONE: {` |
|        - |  5919 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5920 | `#ifdef UNTRUST` |
|        - |  5921 | `	if( pTos < pStack ){` |
|        - |  5922 | `		goto Abort;` |
|        - |  5923 | `	}` |
|        - |  5924 | `#endif` |
|        - |  5925 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5926 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5927 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5928 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5929 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5930 | `		break;` |
|        - |  5931 | `	}` |
|        - |  5932 | `	/* Point to the source */` |
|       44 |  5933 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5934 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  5935 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  5936 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5937 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  5938 | `			&pSrc->pClass->sName);` |
|      ! 0 |  5939 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5940 | `		break;` |
|        - |  5941 | `	}` |
|        - |  5942 | `	/* Perform the clone operation */` |
|       44 |  5943 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5944 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5945 | `	if( pClone == 0 ){` |
|      ! 0 |  5946 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5947 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5948 | `	}else{` |
|        - |  5949 | `		/* Load the cloned object */` |
|       44 |  5950 | `		pTos->x.pOther = pClone;` |
|       44 |  5951 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5952 | `	}` |
|       44 |  5953 | `	break;` |
|        - |  5954 | `				   }` |
|        - |  5955 | `/*` |
|        - |  5956 | ` * OP_SWITCH * * P3` |
|        - |  5957 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5958 | ` */` |
|       18 |  5959 | `case PH7_OP_SWITCH: {` |
|       38 |  5960 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5961 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5962 | `	ph7_value sValue,sCaseValue;` |
|        - |  5963 | `	sxu32 n,nEntry;` |
|        - |  5964 | `#ifdef UNTRUST` |
|        - |  5965 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5966 | `		goto Abort;` |
|        - |  5967 | `	}` |
|        - |  5968 | `#endif` |
|        - |  5969 | `	/* Point to the case table  */` |
|       38 |  5970 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5971 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5972 | `	/* Select the appropriate case block to execute */` |
|       38 |  5973 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5974 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5975 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5976 | `		pCase = &aCase[n];` |
|       92 |  5977 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5978 | `		/* Execute the case expression first */` |
|       92 |  5979 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5980 | `		/* Compare the two expression */` |
|       92 |  5981 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5982 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5983 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5984 | `		if( rc == 0 ){` |
|        - |  5985 | `			/* Value match,jump to this block */` |
|       38 |  5986 | `			pc = pCase->nStart - 1;` |
|       38 |  5987 | `			break;` |
|        - |  5988 | `		}` |
|       29 |  5989 | `	}` |
|       38 |  5990 | `	VmPopOperand(&pTos,1);` |
|       38 |  5991 | `	if( n >= nEntry ){` |
|        - |  5992 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5993 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5994 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5995 | `		}else{` |
|        - |  5996 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5997 | `			pc = pSwitch->nOut - 1;` |
|        - |  5998 | `		}` |
|      ! 0 |  5999 | `	}` |
|       38 |  6000 | `	break;` |
|        - |  6001 | `					}` |
|        - |  6002 | `/*` |
|        - |  6003 | ` * OP_YIELD P1 P2 *` |
|        - |  6004 | ` *  Yield a value from a generator function.` |
|        - |  6005 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6006 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6007 | ` */` |
|       28 |  6008 | `case PH7_OP_YIELD: {` |
|        - |  6009 | `	ph7_generator *pGen;` |
|       57 |  6010 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6011 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6012 | `		goto Abort;` |
|        - |  6013 | `	}` |
|       57 |  6014 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       57 |  6015 | `	if( pInstr->iP2 ){` |
|        - |  6016 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6017 | `#ifdef UNTRUST` |
|        - |  6018 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6019 | `#endif` |
|        7 |  6020 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6021 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6022 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6023 | `		VmPopOperand(&pTos, 1);` |
|        - |  6024 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6025 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6026 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6027 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6028 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6029 | `			}` |
|        1 |  6030 | `		}` |
|       54 |  6031 | `	}else if( pInstr->iP1 ){` |
|        - |  6032 | `		/* yield $value */` |
|        - |  6033 | `#ifdef UNTRUST` |
|        - |  6034 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6035 | `#endif` |
|       51 |  6036 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       51 |  6037 | `		VmPopOperand(&pTos, 1);` |
|        - |  6038 | `		/* Auto-increment key */` |
|       51 |  6039 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       51 |  6040 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       51 |  6041 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       26 |  6042 | `	}else{` |
|        - |  6043 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6044 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6045 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6046 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6047 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6048 | `	}` |
|        - |  6049 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       57 |  6050 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       57 |  6051 | `	goto Suspend;` |
|        - |  6052 |  |
|        - |  6053 | `/*` |
|        - |  6054 | ` * OP_CALL P1 * *` |
|        - |  6055 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6056 | ` *  function on the stack.` |
|        - |  6057 | ` */` |
|   292927 |  6058 | `case PH7_OP_CALL: {` |
|   585900 |  6059 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6060 | `	ph7_value *pArg;` |
|   585900 |  6061 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   585900 |  6062 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6063 | `	SyHashEntry *pEntry;` |
|        - |  6064 | `	SyString sName;` |
|        - |  6065 | `	/* Extract function name */` |
|   585900 |  6066 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6067 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6068 | `			ph7_value sResult;` |
|      ! 0 |  6069 | `			SySetReset(&aArg);` |
|      ! 0 |  6070 | `			while( pArg < pTos ){` |
|      ! 0 |  6071 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6072 | `				pArg++;` |
|      ! 0 |  6073 | `			}` |
|      ! 0 |  6074 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6075 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6076 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6077 | `			SySetReset(&aArg);` |
|        - |  6078 | `			/* Pop given arguments */` |
|      ! 0 |  6079 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6080 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6081 | `			}` |
|        - |  6082 | `			/* Copy result */` |
|      ! 0 |  6083 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6084 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6085 | `		}else{` |
|        3 |  6086 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6087 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6088 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6089 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6090 | `			}else{` |
|        - |  6091 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6092 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6093 | `			}` |
|        - |  6094 | `			/* Pop given arguments */` |
|        3 |  6095 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6096 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6097 | `			}` |
|        - |  6098 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6099 | `			PH7_MemObjRelease(pTos);` |
|        - |  6100 | `		}` |
|   292654 |  6101 | `		break;` |
|        - |  6102 | `	}` |
|   585898 |  6103 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6104 | `	/* Check for a compiled function first.` |
|        - |  6105 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6106 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   585898 |  6107 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6108 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6109 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6110 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6111 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6112 | `	 * function calls inside namespaces. */` |
|   585898 |  6113 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6114 | `		const char *zFunc;` |
|        - |  6115 | `		const char *zEnd;` |
|        - |  6116 | `		const char *z;` |
|        - |  6117 | `		SyString sGlobal;` |
|       15 |  6118 | `		zFunc = sName.zString;` |
|       15 |  6119 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  6120 | `		z = zEnd;` |
|        - |  6121 | `		/* Find last namespace separator */` |
|      133 |  6122 | `		while( z > zFunc ){` |
|      133 |  6123 | `			if( z[-1] == '\\' ){` |
|       15 |  6124 | `				break;` |
|        - |  6125 | `			}` |
|      119 |  6126 | `			z--;` |
|        1 |  6127 | `		}` |
|       15 |  6128 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6129 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  6130 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  6131 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  6132 | `		}` |
|        7 |  6133 | `	}` |
|   585898 |  6134 | `	if( pEntry ){` |
|        - |  6135 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6136 | `		ph7_class_instance *pThis;` |
|        - |  6137 | `		ph7_value *pFrameStack;` |
|        - |  6138 | `		ph7_vm_func *pVmFunc;` |
|        - |  6139 | `		ph7_class *pSelf;` |
|        - |  6140 | `		VmFrame *pFrame;` |
|        - |  6141 | `		ph7_value *pObj;` |
|        - |  6142 | `		VmSlot sArg;` |
|        - |  6143 | `		sxu32 n;` |
|        - |  6144 | `		/* initialize fields */` |
|    13444 |  6145 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13444 |  6146 | `		pThis = 0;` |
|    13444 |  6147 | `		pSelf = 0;` |
|    13444 |  6148 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6149 | `			ph7_class_method *pMeth;` |
|        - |  6150 | `			/* Class method call */` |
|     1972 |  6151 | `			ph7_value *pTarget = &pTos[-1];` |
|     1972 |  6152 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6153 | `				/* Extract the 'this' pointer */` |
|     1972 |  6154 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6155 | `					/* Instance already loaded */` |
|     1900 |  6156 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1900 |  6157 | `					pThis->iRef++;` |
|     1900 |  6158 | `					pSelf = pThis->pClass;` |
|      949 |  6159 | `				}` |
|     1972 |  6160 | `				if( pSelf == 0 ){` |
|       74 |  6161 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6162 | `						/* "Late Static Binding" class name */` |
|      101 |  6163 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6164 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6165 | `					}` |
|       74 |  6166 | `					if( pSelf == 0 ){` |
|       13 |  6167 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6168 | `					}` |
|       36 |  6169 | `				}` |
|     1972 |  6170 | `				if( pThis == 0  ){` |
|       74 |  6171 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6172 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6173 | `					if( pFrameLocal->pParent ){` |
|        - |  6174 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6175 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6176 | `						if( pThis ){` |
|       13 |  6177 | `							pThis->iRef++;` |
|        6 |  6178 | `						}` |
|       28 |  6179 | `					}` |
|       36 |  6180 | `				}` |
|     1972 |  6181 | `				VmPopOperand(&pTos,1);` |
|     1972 |  6182 | `				PH7_MemObjRelease(pTos);` |
|        - |  6183 | `				/* Synchronize pointers */` |
|     1972 |  6184 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6185 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6186 | `				 * user have already computed the random generated unique class method name` |
|        - |  6187 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6188 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6189 | `				 */` |
|     1972 |  6190 | `				while( pArg < pStack ){` |
|      ! 0 |  6191 | `					pArg++;` |
|      ! 0 |  6192 | `				}` |
|     1972 |  6193 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6194 | `					/* Check if the call is allowed */` |
|     1972 |  6195 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1972 |  6196 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6197 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6198 | `							/* Pop given arguments */` |
|      ! 0 |  6199 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6200 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6201 | `							}` |
|        - |  6202 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6203 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6204 | `							break;` |
|        - |  6205 | `						}` |
|        3 |  6206 | `					}` |
|      985 |  6207 | `				}` |
|      985 |  6208 | `			}` |
|      985 |  6209 | `		}` |
|        - |  6210 | `		/* Check The recursion limit */` |
|    13444 |  6211 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6212 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6213 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6214 | `				&pVmFunc->sName);` |
|        - |  6215 | `			/* Pop given arguments */` |
|        3 |  6216 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6217 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6218 | `			}` |
|        - |  6219 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6220 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6221 | `			break;` |
|        - |  6222 | `		}` |
|    13442 |  6223 | `		if( pVmFunc->pNextName ){` |
|        - |  6224 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6225 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6226 | `		}` |
|    13442 |  6227 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6228 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6229 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6230 | `			ph7_generator *pGenerator;` |
|        - |  6231 | `			ph7_class_instance *pGenObj;` |
|        - |  6232 | `			ph7_value *pCtxAttr;` |
|        - |  6233 | `			SyString sAttrName;` |
|        - |  6234 | `			ph7_value **apCallArgs;` |
|        - |  6235 | `			int nGenArgs, iArg;` |
|        - |  6236 | `			/* Collect arguments from the operand stack */` |
|       19 |  6237 | `			nGenArgs = (int)(pTos - pArg);` |
|       19 |  6238 | `			apCallArgs = 0;` |
|       19 |  6239 | `			if( nGenArgs > 0 ){` |
|        7 |  6240 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6241 | `					nGenArgs * sizeof(ph7_value *));` |
|        5 |  6242 | `				if( apCallArgs == 0 ){` |
|        - |  6243 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6244 | `					nGenArgs = 0;` |
|      ! 0 |  6245 | `				}else{` |
|       11 |  6246 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        7 |  6247 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        4 |  6248 | `					}` |
|        - |  6249 | `				}` |
|        2 |  6250 | `			}` |
|        - |  6251 | `			/* Create execution context and generator wrapper */` |
|       19 |  6252 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       19 |  6253 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6254 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6255 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6256 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6257 | `				break;` |
|        - |  6258 | `			}` |
|       19 |  6259 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       19 |  6260 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6261 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6262 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6263 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6264 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6265 | `				break;` |
|        - |  6266 | `			}` |
|        - |  6267 | `			/* Set up the frame with arguments, closure env, $this */` |
|       19 |  6268 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       19 |  6269 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       19 |  6270 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       19 |  6271 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       19 |  6272 | `			pExecCtx->pFrame->pParent = 0;` |
|       19 |  6273 | `			if( apCallArgs ){` |
|        5 |  6274 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6275 | `			}` |
|       19 |  6276 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6277 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6278 | `				if( pThis ){` |
|      ! 0 |  6279 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6280 | `				}` |
|      ! 0 |  6281 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6282 | `					goto Abort;` |
|        - |  6283 | `				}` |
|      ! 0 |  6284 | `				break;` |
|        - |  6285 | `			}` |
|        - |  6286 | `			/* Create Generator class instance */` |
|       19 |  6287 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       19 |  6288 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6289 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6290 | `				break;` |
|        - |  6291 | `			}` |
|        - |  6292 | `			/* Store generator in __ctx attribute */` |
|       19 |  6293 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       19 |  6294 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       19 |  6295 | `			if( pCtxAttr ){` |
|       19 |  6296 | `				pCtxAttr->x.pOther = pGenerator;` |
|       19 |  6297 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6298 | `			}` |
|        - |  6299 | `			/* Pop args and function name, push Generator object */` |
|       19 |  6300 | `			PH7_MemObjRelease(pTos);` |
|       19 |  6301 | `			pTos = &pTos[-nCallArgs];` |
|       19 |  6302 | `			pTos->x.pOther = pGenObj;` |
|       19 |  6303 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       19 |  6304 | `			pGenObj->iRef++;` |
|       19 |  6305 | `			if( pThis ){` |
|      ! 0 |  6306 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6307 | `			}` |
|       19 |  6308 | `			break;` |
|        - |  6309 | `		}` |
|        - |  6310 | `		/* Extract the formal argument set */` |
|    13424 |  6311 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6312 | `		/* Create a new VM frame  */` |
|    13424 |  6313 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13424 |  6314 | `		if( rc != SXRET_OK ){` |
|        - |  6315 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6316 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6317 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6318 | `				&pVmFunc->sName);` |
|        - |  6319 | `			/* Pop given arguments */` |
|      ! 0 |  6320 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6321 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6322 | `			}` |
|        - |  6323 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6324 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6325 | `			break;` |
|        - |  6326 | `		}` |
|    13424 |  6327 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6328 | `			/* Install the '$this' variable */` |
|        - |  6329 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1910 |  6330 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1910 |  6331 | `			if( pObj ){` |
|        - |  6332 | `				/* Reflect the change */` |
|     1910 |  6333 | `				pObj->x.pOther = pThis;` |
|     1910 |  6334 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      954 |  6335 | `			}` |
|      954 |  6336 | `		}` |
|    13424 |  6337 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6338 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6339 | `			/* Install static variables */` |
|      ! 0 |  6340 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6341 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6342 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6343 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6344 | `					/* Initialize the static variables */` |
|      ! 0 |  6345 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6346 | `					if( pObj ){` |
|        - |  6347 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6348 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6349 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6350 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6351 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6352 | `						}` |
|      ! 0 |  6353 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6354 | `					}else{` |
|      ! 0 |  6355 | `						continue;` |
|        - |  6356 | `					}` |
|      ! 0 |  6357 | `				}` |
|        - |  6358 | `				/* Install in the current frame */` |
|      ! 0 |  6359 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6360 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6361 | `			}` |
|      ! 0 |  6362 | `		}` |
|        - |  6363 | `		/* Push arguments in the local frame */` |
|    13424 |  6364 | `		n = 0;` |
|    36492 |  6365 | `		while( pArg < pTos ){` |
|    23090 |  6366 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6367 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       21 |  6368 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       21 |  6369 | `				if( pObj ){` |
|        - |  6370 | `					/* Initialize as empty array */` |
|       21 |  6371 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6372 | `					{` |
|       21 |  6373 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       83 |  6374 | `						while( pArg < pTos ){` |
|        - |  6375 | `							/* Apply type coercion to each element if the variadic has a type hint */` |
|       62 |  6376 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       29 |  6377 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6378 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6379 | `								if( xCast ){` |
|       13 |  6380 | `									xCast(pArg);` |
|        6 |  6381 | `								}` |
|        6 |  6382 | `							}` |
|       63 |  6383 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       63 |  6384 | `							pArg++;` |
|        1 |  6385 | `						}` |
|        - |  6386 | `					}` |
|       21 |  6387 | `					sArg.nIdx = pObj->nIdx;` |
|       21 |  6388 | `					sArg.pUserData = 0;` |
|       21 |  6389 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       10 |  6390 | `				}` |
|       21 |  6391 | `				break; /* All remaining args consumed */` |
|        - |  6392 | `			}` |
|    23070 |  6393 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22914 |  6394 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        9 |  6395 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6396 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6397 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6398 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6399 | `						goto Abort;` |
|        - |  6400 | `					}` |
|      ! 0 |  6401 | `				}` |
|        - |  6402 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6403 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    22926 |  6404 | `				if( aFormalArg[n].nType > 0` |
|    12030 |  6405 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1132 |  6406 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6407 | `						/* Argument must be a class instance [i.e: object] */` |
|        5 |  6408 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6409 | `						ph7_class *pClass;` |
|        - |  6410 | `						/* Try to extract the desired class */` |
|        5 |  6411 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|        5 |  6412 | `						if( pClass ){` |
|        5 |  6413 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6414 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6415 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6416 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6417 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6418 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6419 | `								}` |
|      ! 0 |  6420 | `							}else{` |
|        - |  6421 | `								/* reuse pThis declared in outer scope */` |
|        5 |  6422 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6423 | `								/* Make sure the object is an instance of the given class */` |
|        5 |  6424 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6425 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6426 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6427 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6428 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6429 | `								}` |
|        - |  6430 | `							}` |
|        3 |  6431 | `						}` |
|     1130 |  6432 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6433 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6434 | `						/* Cast to the desired type */` |
|      ! 0 |  6435 | `						xCast(pArg);` |
|      ! 0 |  6436 | `					}` |
|      565 |  6437 | `				}` |
|    22916 |  6438 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6439 | `					/* Pass by reference */` |
|       54 |  6440 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6441 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6442 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6443 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6444 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6445 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6446 | `						}` |
|        - |  6447 | `						/* Switch to pass by value */` |
|      ! 0 |  6448 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6449 | `					}else{` |
|        - |  6450 | `						SyHashEntry *pRefEntry;` |
|        - |  6451 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6452 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6453 | `						if( pRefEntry == 0 ){` |
|       80 |  6454 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6455 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6456 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6457 | `							sArg.pUserData = 0;` |
|       54 |  6458 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6459 | `						}` |
|       54 |  6460 | `						pObj = 0;` |
|        - |  6461 | `					}` |
|       28 |  6462 | `				}else{` |
|        - |  6463 | `					/* Pass by value,make a copy of the given argument */` |
|    22864 |  6464 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6465 | `				}` |
|    11459 |  6466 | `			}else{` |
|        - |  6467 | `				char zName[32];` |
|        - |  6468 | `				SyString sArgName;` |
|        - |  6469 | `				/* Set a dummy name */` |
|      156 |  6470 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6471 | `				sArgName.zString = zName;` |
|        - |  6472 | `				/* Annonymous argument */` |
|      156 |  6473 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6474 | `			}` |
|    23070 |  6475 | `			if( pObj ){` |
|    23018 |  6476 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6477 | `				/* Insert argument index  */` |
|    23018 |  6478 | `				sArg.nIdx = pObj->nIdx;` |
|    23018 |  6479 | `				sArg.pUserData = 0;` |
|    23018 |  6480 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11508 |  6481 | `			}` |
|    23070 |  6482 | `			PH7_MemObjRelease(pArg);` |
|    23070 |  6483 | `			pArg++;` |
|    23070 |  6484 | `			++n;` |
|        2 |  6485 | `		}` |
|        - |  6486 | `		/* Set up closure environment */` |
|    13424 |  6487 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6488 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6489 | `			ph7_value *pValue;` |
|        - |  6490 | `			sxu32 iEnv;` |
|       11 |  6491 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6492 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6493 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6494 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6495 | `					/* Do not install null value */` |
|       11 |  6496 | `					continue;` |
|        - |  6497 | `				}` |
|       11 |  6498 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6499 | `				if( pValue == 0 ){` |
|      ! 0 |  6500 | `					continue;` |
|        - |  6501 | `				}` |
|        - |  6502 | `				/* Invalidate any prior representation */` |
|       11 |  6503 | `				PH7_MemObjRelease(pValue);` |
|        - |  6504 | `				/* Duplicate bound variable value */` |
|       11 |  6505 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6506 | `			}` |
|        5 |  6507 | `		}` |
|        - |  6508 | `		/* Process default values for remaining formal parameters */` |
|    15366 |  6509 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1970 |  6510 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6511 | `				/* Variadic parameter with no extra args — create empty array */` |
|       27 |  6512 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       27 |  6513 | `				if( pObj ){` |
|       27 |  6514 | `					PH7_MemObjToHashmap(pObj);` |
|       27 |  6515 | `					sArg.nIdx = pObj->nIdx;` |
|       27 |  6516 | `					sArg.pUserData = 0;` |
|       27 |  6517 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6518 | `				}` |
|       27 |  6519 | `				n++;` |
|       27 |  6520 | `				break; /* Variadic is always last */` |
|        - |  6521 | `			}` |
|     1944 |  6522 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1938 |  6523 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1938 |  6524 | `				if( pObj ){` |
|        - |  6525 | `					/* Evaluate the default value and extract it's result */` |
|     1938 |  6526 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1938 |  6527 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6528 | `						goto Abort;` |
|        - |  6529 | `					}` |
|        - |  6530 | `					/* Insert argument index */` |
|     1938 |  6531 | `					sArg.nIdx = pObj->nIdx;` |
|     1938 |  6532 | `					sArg.pUserData = 0;` |
|     1938 |  6533 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6534 | `					/* Make sure the default argument is of the correct type */` |
|     1938 |  6535 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6536 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6537 | `						/* Cast to the desired type */` |
|      ! 0 |  6538 | `						xCast(pObj);` |
|      ! 0 |  6539 | `					}` |
|      968 |  6540 | `				}` |
|      968 |  6541 | `			}` |
|     1944 |  6542 | `			++n;` |
|        2 |  6543 | `		}` |
|        - |  6544 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6545 | `		 * does not return anything.` |
|        - |  6546 | `		 */` |
|    13424 |  6547 | `		PH7_MemObjRelease(pTos);` |
|    13424 |  6548 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6549 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13424 |  6550 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13424 |  6551 | `		if( pFrameStack == 0 ){` |
|        - |  6552 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6553 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6554 | `				&pVmFunc->sName);` |
|      ! 0 |  6555 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6556 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6557 | `			}` |
|      ! 0 |  6558 | `			break;` |
|        - |  6559 | `		}` |
|    13424 |  6560 | `		if( pSelf ){` |
|        - |  6561 | `			/* Push class name */` |
|     1970 |  6562 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      984 |  6563 | `		}` |
|        - |  6564 | `		/* Increment nesting level */` |
|    13424 |  6565 | `		pVm->nRecursionDepth++;` |
|        - |  6566 | `		/* Execute function body */` |
|    13424 |  6567 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6568 | `		/* Decrement nesting level */` |
|    13424 |  6569 | `		pVm->nRecursionDepth--;` |
|    13424 |  6570 | `		if( pSelf ){` |
|        - |  6571 | `			/* Pop class name */` |
|     1970 |  6572 | `			(void)SySetPop(&pVm->aSelf);` |
|      984 |  6573 | `		}` |
|        - |  6574 | `		/* Cleanup the mess left behind */` |
|    13424 |  6575 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6576 | `			/* Return by reference,reflect that */` |
|        9 |  6577 | `			if( n != SXU32_HIGH ){` |
|        9 |  6578 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6579 | `				sxu32 i;` |
|        - |  6580 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6581 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6582 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6583 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6584 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6585 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6586 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6587 | `								&pVmFunc->sName);` |
|      ! 0 |  6588 | `						}` |
|      ! 0 |  6589 | `						n = SXU32_HIGH;` |
|      ! 0 |  6590 | `						break;` |
|        - |  6591 | `					}` |
|        3 |  6592 | `				}` |
|        5 |  6593 | `			}else{` |
|      ! 0 |  6594 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6595 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6596 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6597 | `						&pVmFunc->sName);` |
|      ! 0 |  6598 | `				}` |
|        - |  6599 | `			}` |
|        9 |  6600 | `			pTos->nIdx = n;` |
|        4 |  6601 | `		}` |
|        - |  6602 | `		/* Cleanup the mess left behind */` |
|    13424 |  6603 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6604 | `			/* An exception was throw in this frame */` |
|       12 |  6605 | `			pFrame = pFrame->pParent;` |
|       12 |  6606 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6607 | `				/* Pop the resutlt */` |
|       10 |  6608 | `				VmPopOperand(&pTos,1);` |
|        - |  6609 | `				/* Jump to this destination */` |
|       10 |  6610 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6611 | `				rc = PH7_OK;` |
|        6 |  6612 | `			}else{` |
|        3 |  6613 | `				if( pFrame->pParent ){` |
|        3 |  6614 | `					rc = PH7_EXCEPTION;` |
|        2 |  6615 | `				}else{` |
|        - |  6616 | `					/* Continue normal execution */` |
|      ! 0 |  6617 | `					rc = PH7_OK;` |
|        - |  6618 | `				}` |
|        - |  6619 | `			}` |
|        5 |  6620 | `		}` |
|        - |  6621 | `		/* Free the operand stack */` |
|    13424 |  6622 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6623 | `		/* Leave the frame */` |
|    13424 |  6624 | `		VmLeaveFrame(&(*pVm));` |
|    13424 |  6625 | `		if( rc == PH7_ABORT ){` |
|        - |  6626 | `			/* Abort processing immeditaley */` |
|        7 |  6627 | `			goto Abort;` |
|    13418 |  6628 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6629 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6630 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6631 | `			 * overwriting the state saved by the inner level.` |
|        - |  6632 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6633 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       39 |  6634 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       39 |  6635 | `			goto Suspend;` |
|    13380 |  6636 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6637 | `			goto Exception;` |
|        - |  6638 | `		}` |
|     6690 |  6639 | `	}else{` |
|        - |  6640 | `		ph7_user_func *pFunc;` |
|        - |  6641 | `		ph7_context sCtx;` |
|        - |  6642 | `		ph7_value sRet;` |
|        - |  6643 | `		/* Look for an installed foreign function.` |
|        - |  6644 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6645 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6646 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6647 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6648 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6649 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   572456 |  6650 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   572456 |  6651 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6652 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6653 | `			const char *zShort = sName.zString;` |
|        - |  6654 | `			sxu32 i;` |
|      217 |  6655 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6656 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6657 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6658 | `				}` |
|      102 |  6659 | `			}` |
|       15 |  6660 | `			if( zShort != sName.zString ){` |
|       15 |  6661 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6662 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6663 | `			}` |
|        7 |  6664 | `		}` |
|   572456 |  6665 | `		if( pEntry == 0 ){` |
|        - |  6666 | `			/* Call to undefined function */` |
|        5 |  6667 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6668 | `			/* Pop given arguments */` |
|        5 |  6669 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6670 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6671 | `			}` |
|        - |  6672 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6673 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6674 | `			break;` |
|        - |  6675 | `		}` |
|   572452 |  6676 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6677 | `		/* Start collecting function arguments */` |
|   572452 |  6678 | `		SySetReset(&aArg);` |
|  1536356 |  6679 | `		while( pArg < pTos ){` |
|   963906 |  6680 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   963906 |  6681 | `			pArg++;` |
|        2 |  6682 | `		}` |
|        - |  6683 | `		/* Assume a null return value */` |
|   572452 |  6684 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6685 | `		/* Init the call context */` |
|   572452 |  6686 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6687 | `		/* Call the foreign function */` |
|   572452 |  6688 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6689 | `		/* Release the call context */` |
|   572452 |  6690 | `		VmReleaseCallContext(&sCtx);` |
|   572452 |  6691 | `		if( rc == PH7_ABORT ){` |
|      463 |  6692 | `			goto Abort;` |
|   571990 |  6693 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6694 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6695 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6696 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6697 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6698 | `				goto Exception;` |
|        - |  6699 | `			}` |
|        - |  6700 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6701 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6702 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6703 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6704 | `			}` |
|        - |  6705 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6706 | `			VmPopOperand(&pTos,1);` |
|        - |  6707 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6708 | `			pFrm = pVm->pFrame;` |
|        7 |  6709 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6710 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6711 | `			}` |
|        7 |  6712 | `			break;` |
|        - |  6713 | `		}` |
|   571980 |  6714 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6715 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6716 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6717 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6718 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6719 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6720 | `			 * body), the user-function path above will handle re-saving. */` |
|       39 |  6721 | `			PH7_MemObjRelease(&sRet);` |
|       39 |  6722 | `			if( pInstr->iP1 > 0 ){` |
|       39 |  6723 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6724 | `			}` |
|        - |  6725 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6726 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       39 |  6727 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       39 |  6728 | `			goto Suspend;` |
|        - |  6729 | `		}` |
|   571942 |  6730 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6731 | `			/* Pop function name and arguments */` |
|   554302 |  6732 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   277172 |  6733 | `		}` |
|        - |  6734 | `		/* Save foreign function return value */` |
|   571942 |  6735 | `		PH7_MemObjStore(&sRet,pTos);` |
|   571942 |  6736 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6737 | `	}` |
|   585318 |  6738 | `	break;` |
|        - |  6739 | `				  }` |
|        - |  6740 | `/*` |
|        - |  6741 | ` * OP_CONSUME: P1 * *` |
|        - |  6742 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6743 | ` */` |
|    11481 |  6744 | `case PH7_OP_CONSUME: {` |
|    22964 |  6745 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    22964 |  6746 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6747 |  |
|    22964 |  6748 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    22964 |  6749 | `	pCur = pOut;` |
|        - |  6750 | `	/* Start the consume process  */` |
|    45926 |  6751 | `	while( pOut <= pTos ){` |
|        - |  6752 | `		/* Force a string cast */` |
|    22964 |  6753 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      304 |  6754 | `			PH7_MemObjToString(pOut);` |
|      151 |  6755 | `		}` |
|    22964 |  6756 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6757 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6758 | `			/* Invoke the output consumer callback */` |
|    12736 |  6759 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    12736 |  6760 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    12736 |  6761 | `			SyBlobRelease(&pOut->sBlob);` |
|    12736 |  6762 | `			if( rc == SXERR_ABORT ){` |
|        - |  6763 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6764 | `				goto Abort;` |
|        - |  6765 | `			}` |
|     6367 |  6766 | `		}` |
|    22964 |  6767 | `		pOut++;` |
|        2 |  6768 | `	}` |
|    22964 |  6769 | `	pTos = &pCur[-1];` |
|    22962 |  6770 | `	break;` |
|        - |  6771 | `					 }` |
|        - |  6772 |  |
|        - |  6773 | `		} /* Switch() */` |
|  9928774 |  6774 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6775 | `	} /* For(;;) */` |
|    16267 |  6776 | `Done:` |
|    32536 |  6777 | `	SySetRelease(&aArg);` |
|    32536 |  6778 | `	return SXRET_OK;` |
|       66 |  6779 | `Suspend:` |
|      133 |  6780 | `	SySetRelease(&aArg);` |
|      133 |  6781 | `	return PH7_SUSPEND;` |
|      238 |  6782 | `Abort:` |
|      477 |  6783 | `	SySetRelease(&aArg);` |
|     1661 |  6784 | `	while( pTos >= pStack ){` |
|     1185 |  6785 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6786 | `		pTos--;` |
|        1 |  6787 | `	}` |
|      477 |  6788 | `	return PH7_ABORT;` |
|        3 |  6789 | `Exception:` |
|        8 |  6790 | `	SySetRelease(&aArg);` |
|       22 |  6791 | `	while( pTos >= pStack ){` |
|       16 |  6792 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6793 | `		pTos--;` |
|        2 |  6794 | `	}` |
|        8 |  6795 | `	return PH7_EXCEPTION;` |
|    16576 |  6796 |  |
|        - |  6797 | `/*` |
|        - |  6798 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6799 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6800 | ` * See block-comment on that function for additional information.` |
|        - |  6801 | ` */` |
|    15082 |  6802 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6803 |  |
|        - |  6804 | `	ph7_value *pStack;` |
|        - |  6805 | `	sxi32 rc;` |
|        - |  6806 | `	/* Allocate a new operand stack */` |
|    15084 |  6807 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15084 |  6808 | `	if( pStack == 0 ){` |
|      ! 0 |  6809 | `		return SXERR_MEM;` |
|        - |  6810 | `	}` |
|        - |  6811 | `	/* Execute the program */` |
|    15084 |  6812 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6813 | `	/* Free the operand stack */` |
|    15084 |  6814 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6815 | `	/* Execution result */` |
|    15084 |  6816 | `	return rc;` |
|     7543 |  6817 |  |
|        - |  6818 | `/*` |
|        - |  6819 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6820 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6821 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6822 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6823 | ` * execution ends.` |
|        - |  6824 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6825 | ` * additional information.` |
|        - |  6826 | ` */` |
|     2578 |  6827 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6828 |  |
|        - |  6829 | `	VmShutdownCB *pEntry;` |
|        - |  6830 | `	ph7_value *apArg[10];` |
|        - |  6831 | `	sxu32 n,nEntry;` |
|        - |  6832 | `	int i;` |
|        - |  6833 | `	/* Point to the stack of registered callbacks */` |
|     2580 |  6834 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    28360 |  6835 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    25782 |  6836 | `		apArg[i] = 0;` |
|    12892 |  6837 | `	}` |
|     2582 |  6838 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6839 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6840 | `		if( pEntry ){` |
|        - |  6841 | `			/* Prepare callback arguments if any */` |
|        3 |  6842 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6843 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6844 | `					break;` |
|        - |  6845 | `				}` |
|      ! 0 |  6846 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6847 | `			}` |
|        - |  6848 | `			/* Invoke the callback */` |
|        3 |  6849 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6850 | `			/*` |
|        - |  6851 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6852 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6853 | `			 */` |
|        3 |  6854 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6855 | `			if( pEntry ){` |
|        3 |  6856 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6857 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6858 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6859 | `				}` |
|        1 |  6860 | `			}` |
|        1 |  6861 | `		}` |
|        2 |  6862 | `	}` |
|     2580 |  6863 | `	SySetReset(&pVm->aShutdown);` |
|     2580 |  6864 |  |
|        - |  6865 | `/*` |
|        - |  6866 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6867 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6868 | ` * See block-comment on that function for additional information.` |
|        - |  6869 | ` */` |
|     2586 |  6870 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6871 |  |
|        - |  6872 | `	/* Make sure we are ready to execute this program */` |
|     2588 |  6873 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6874 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6875 | `	}` |
|        - |  6876 | `	/* Set the execution magic number  */` |
|     2588 |  6877 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6878 | `	/* Execute the program */` |
|     2588 |  6879 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  6880 | `	/* Invoke any shutdown callbacks */` |
|     2584 |  6881 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6882 | `	/*` |
|        - |  6883 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6884 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6885 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6886 | `	 */` |
|     2584 |  6887 | `	return SXRET_OK;` |
|     1295 |  6888 |  |
|        - |  6889 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  6890 | `/*` |
|        - |  6891 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  6892 | ` * The context is in CREATED state and ready to be started.` |
|        - |  6893 | ` */` |
|       42 |  6894 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        1 |  6895 |  |
|        - |  6896 | `	ph7_exec_ctx *pCtx;` |
|        - |  6897 | `	ph7_value *pStack;` |
|        - |  6898 | `	VmFrame *pFrame;` |
|       43 |  6899 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       43 |  6900 | `	if( pCtx == 0 ){` |
|      ! 0 |  6901 | `		return 0;` |
|        - |  6902 | `	}` |
|       43 |  6903 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       43 |  6904 | `	pCtx->pVm = pVm;` |
|       43 |  6905 | `	pCtx->pFunc = pFunc;` |
|       43 |  6906 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       43 |  6907 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       43 |  6908 | `	pCtx->pc = 0;` |
|       43 |  6909 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       43 |  6910 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  6911 | `	/* Allocate a private operand stack */` |
|       43 |  6912 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       43 |  6913 | `	if( pStack == 0 ){` |
|      ! 0 |  6914 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6915 | `		return 0;` |
|        - |  6916 | `	}` |
|       43 |  6917 | `	pCtx->pStack = pStack;` |
|        - |  6918 | `	/* Create a detached frame for the fiber */` |
|       43 |  6919 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       43 |  6920 | `	if( pFrame == 0 ){` |
|      ! 0 |  6921 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  6922 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  6923 | `		return 0;` |
|        - |  6924 | `	}` |
|       43 |  6925 | `	pCtx->pFrame = pFrame;` |
|       43 |  6926 | `	return pCtx;` |
|       22 |  6927 |  |
|        - |  6928 | `/*` |
|        - |  6929 | ` * Start executing a fiber context for the first time.` |
|        - |  6930 | ` */` |
|       42 |  6931 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        1 |  6932 |  |
|        - |  6933 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6934 | `	sxi32 rc;` |
|       43 |  6935 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  6936 | `		return SXERR_INVALID;` |
|        - |  6937 | `	}` |
|        - |  6938 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       43 |  6939 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       43 |  6940 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  6941 | `	/* Save and set the active context */` |
|       43 |  6942 | `	pOldCtx = pVm->pActiveCtx;` |
|       43 |  6943 | `	pVm->pActiveCtx = pCtx;` |
|       43 |  6944 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       43 |  6945 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       43 |  6946 | `	pVm->nRecursionDepth++;` |
|        - |  6947 | `	/* Execute from the beginning */` |
|       64 |  6948 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  6949 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       43 |  6950 | `	pVm->nRecursionDepth--;` |
|        - |  6951 | `	/* Restore the previous context */` |
|       43 |  6952 | `	pVm->pActiveCtx = pOldCtx;` |
|       43 |  6953 | `	if( rc == PH7_SUSPEND ){` |
|        - |  6954 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       41 |  6955 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       41 |  6956 | `		pCtx->pFrame->pParent = 0;` |
|       41 |  6957 | `		if( pResult ){` |
|       23 |  6958 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  6959 | `		}` |
|       41 |  6960 | `		return SXRET_OK;` |
|        - |  6961 | `	}` |
|        - |  6962 | `	/* Detach frame */` |
|        3 |  6963 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  6964 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  6965 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  6966 | `	}` |
|        3 |  6967 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  6968 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6969 | `		return PH7_ABORT;` |
|        - |  6970 | `	}` |
|        3 |  6971 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  6972 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  6973 | `		return PH7_EXCEPTION;` |
|        - |  6974 | `	}` |
|        - |  6975 | `	/* Normal completion */` |
|        3 |  6976 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  6977 | `	if( pResult ){` |
|        3 |  6978 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  6979 | `	}` |
|        3 |  6980 | `	return SXRET_OK;` |
|       22 |  6981 |  |
|        - |  6982 | `/*` |
|        - |  6983 | ` * Resume a suspended fiber context.` |
|        - |  6984 | ` */` |
|       86 |  6985 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        1 |  6986 |  |
|        - |  6987 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  6988 | `	sxi32 rc;` |
|       87 |  6989 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  6990 | `		return SXERR_INVALID;` |
|        - |  6991 | `	}` |
|        - |  6992 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  6993 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  6994 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       87 |  6995 | `	if( pResumeValue ){` |
|       39 |  6996 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       20 |  6997 | `	}else{` |
|       49 |  6998 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  6999 | `	}` |
|       87 |  7000 | `	pCtx->nTos++;` |
|        - |  7001 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       87 |  7002 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       87 |  7003 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7004 | `	/* Save and set the active context */` |
|       87 |  7005 | `	pOldCtx = pVm->pActiveCtx;` |
|       87 |  7006 | `	pVm->pActiveCtx = pCtx;` |
|       87 |  7007 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       87 |  7008 | `	pVm->nRecursionDepth++;` |
|        - |  7009 | `	/* Resume execution from saved PC */` |
|      130 |  7010 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7011 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       87 |  7012 | `	pVm->nRecursionDepth--;` |
|        - |  7013 | `	/* Restore the previous context */` |
|       87 |  7014 | `	pVm->pActiveCtx = pOldCtx;` |
|       87 |  7015 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7016 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       55 |  7017 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       55 |  7018 | `		pCtx->pFrame->pParent = 0;` |
|       55 |  7019 | `		if( pResult ){` |
|       17 |  7020 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7021 | `		}` |
|       55 |  7022 | `		return SXRET_OK;` |
|        - |  7023 | `	}` |
|        - |  7024 | `	/* Detach frame */` |
|       33 |  7025 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       33 |  7026 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       33 |  7027 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7028 | `	}` |
|       33 |  7029 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7030 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7031 | `		return PH7_ABORT;` |
|        - |  7032 | `	}` |
|       33 |  7033 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7034 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7035 | `		return PH7_EXCEPTION;` |
|        - |  7036 | `	}` |
|        - |  7037 | `	/* Normal completion */` |
|       33 |  7038 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       33 |  7039 | `	if( pResult ){` |
|       19 |  7040 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7041 | `	}` |
|       33 |  7042 | `	return SXRET_OK;` |
|       44 |  7043 |  |
|        - |  7044 | `/*` |
|        - |  7045 | ` * Release an execution context and all its resources.` |
|        - |  7046 | ` */` |
|      ! 0 |  7047 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|      ! 0 |  7048 |  |
|      ! 0 |  7049 | `	if( pCtx == 0 ){` |
|      ! 0 |  7050 | `		return;` |
|        - |  7051 | `	}` |
|      ! 0 |  7052 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7053 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7054 | `		return;` |
|        - |  7055 | `	}` |
|      ! 0 |  7056 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7057 | `	/* Release values */` |
|      ! 0 |  7058 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|      ! 0 |  7059 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7060 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|      ! 0 |  7061 | `	if( pCtx->pFrame ){` |
|        - |  7062 | `		VmSlot *aSlot;` |
|        - |  7063 | `		sxu32 n;` |
|        - |  7064 | `		/* Free local variables */` |
|      ! 0 |  7065 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|      ! 0 |  7066 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|      ! 0 |  7067 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|      ! 0 |  7068 | `		}` |
|        - |  7069 | `		/* Remove local references */` |
|      ! 0 |  7070 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|      ! 0 |  7071 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|      ! 0 |  7072 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|      ! 0 |  7073 | `		}` |
|      ! 0 |  7074 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|      ! 0 |  7075 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|      ! 0 |  7076 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|      ! 0 |  7077 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|      ! 0 |  7078 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|      ! 0 |  7079 | `		pCtx->pFrame = 0;` |
|      ! 0 |  7080 | `	}` |
|        - |  7081 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7082 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7083 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|      ! 0 |  7084 | `	if( pCtx->pStack ){` |
|      ! 0 |  7085 | `		if( pCtx->nTos >= 0 ){` |
|      ! 0 |  7086 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|      ! 0 |  7087 | `			while( pTos >= pCtx->pStack ){` |
|      ! 0 |  7088 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7089 | `				pTos--;` |
|      ! 0 |  7090 | `			}` |
|      ! 0 |  7091 | `		}` |
|      ! 0 |  7092 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|      ! 0 |  7093 | `		pCtx->pStack = 0;` |
|      ! 0 |  7094 | `	}` |
|        - |  7095 | `	/* Free the context itself */` |
|      ! 0 |  7096 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7097 |  |
|        - |  7098 | `/*` |
|        - |  7099 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7100 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7101 | ` */` |
|       86 |  7102 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        1 |  7103 |  |
|        - |  7104 | `	ph7_class_instance *pThis;` |
|        - |  7105 | `	SyString sAttr;` |
|        - |  7106 | `	ph7_value *pAttr;` |
|       87 |  7107 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7108 | `		return 0;` |
|        - |  7109 | `	}` |
|       87 |  7110 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       87 |  7111 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7112 | `		return 0;` |
|        - |  7113 | `	}` |
|       87 |  7114 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       87 |  7115 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       87 |  7116 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       31 |  7117 | `		return 0;` |
|        - |  7118 | `	}` |
|       57 |  7119 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       44 |  7120 |  |
|        - |  7121 | `/*` |
|        - |  7122 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7123 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7124 | ` */` |
|       38 |  7125 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7126 |  |
|       39 |  7127 | `	ph7_vm *pVm = pCtx->pVm;` |
|       39 |  7128 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7129 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7130 | `			"Cannot suspend outside of a fiber");` |
|        - |  7131 | `	}` |
|       39 |  7132 | `	if( nArg > 0 ){` |
|       39 |  7133 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       20 |  7134 | `	}else{` |
|      ! 0 |  7135 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7136 | `	}` |
|       39 |  7137 | `	return PH7_SUSPEND;` |
|       20 |  7138 |  |
|        - |  7139 | `/*` |
|        - |  7140 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7141 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7142 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7143 | ` */` |
|       24 |  7144 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7145 |  |
|        - |  7146 | `	ph7_class_instance *pThis;` |
|        - |  7147 | `	ph7_value *pAttr;` |
|        - |  7148 | `	SyString sAttrName;` |
|       25 |  7149 | `	if( nArg < 2 ){` |
|      ! 0 |  7150 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7151 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7152 | `	}` |
|       25 |  7153 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7154 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7155 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7156 | `	}` |
|       25 |  7157 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       25 |  7158 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7159 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7160 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7161 | `	}` |
|        - |  7162 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       25 |  7163 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7164 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7165 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7166 | `	}` |
|        - |  7167 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       25 |  7168 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  7169 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7170 | `	if( pAttr ){` |
|       25 |  7171 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7172 | `	}` |
|       25 |  7173 | `	return PH7_OK;` |
|       13 |  7174 |  |
|        - |  7175 | `/*` |
|        - |  7176 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7177 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7178 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7179 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7180 | ` */` |
|       24 |  7181 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7182 | `	ph7_class_instance **ppThis)` |
|        1 |  7183 |  |
|       25 |  7184 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7185 | `	ph7_value *pCallable;` |
|        - |  7186 | `	SyString sAttrName;` |
|       25 |  7187 | `	*ppThis = 0;` |
|       25 |  7188 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       25 |  7189 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       25 |  7190 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7191 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7192 | `		return 0;` |
|        - |  7193 | `	}` |
|       25 |  7194 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7195 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7196 | `		SyString sName;` |
|        - |  7197 | `		SyHashEntry *pEntry;` |
|        - |  7198 | `		ph7_vm_func *pFunc;` |
|       25 |  7199 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       25 |  7200 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       25 |  7201 | `		if( pEntry == 0 ){` |
|      ! 0 |  7202 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7203 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7204 | `			return 0;` |
|        - |  7205 | `		}` |
|       25 |  7206 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       25 |  7207 | `		return pFunc;` |
|      ! 0 |  7208 | `	}else{` |
|        - |  7209 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7210 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7211 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7212 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7213 | `		if( pMethod == 0 ){` |
|      ! 0 |  7214 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7215 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7216 | `			return 0;` |
|        - |  7217 | `		}` |
|      ! 0 |  7218 | `		*ppThis = pClosure;` |
|      ! 0 |  7219 | `		return &pMethod->sFunc;` |
|        - |  7220 | `	}` |
|       13 |  7221 |  |
|        - |  7222 | `/*` |
|        - |  7223 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7224 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7225 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7226 | ` */` |
|       42 |  7227 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7228 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        1 |  7229 |  |
|       43 |  7230 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7231 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7232 | `	sxu32 nFormal, n;` |
|        - |  7233 | `	VmSlot sSlot;` |
|        - |  7234 | `	sxi32 rc;` |
|        - |  7235 | `	/* Install $this for closure/method callables */` |
|       43 |  7236 | `	if( pClosureThis ){` |
|        - |  7237 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7238 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7239 | `		if( pObj ){` |
|      ! 0 |  7240 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7241 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7242 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7243 | `		}` |
|      ! 0 |  7244 | `	}` |
|        - |  7245 | `	/* Install static variables */` |
|       43 |  7246 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7247 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7248 | `		ph7_value *pVal;` |
|      ! 0 |  7249 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7250 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7251 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7252 | `			if( pVal ){` |
|      ! 0 |  7253 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7254 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7255 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7256 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7257 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7258 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7259 | `				}` |
|      ! 0 |  7260 | `			}` |
|      ! 0 |  7261 | `		}` |
|      ! 0 |  7262 | `	}` |
|        - |  7263 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       43 |  7264 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       43 |  7265 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       53 |  7266 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7267 | `		ph7_value *pObj;` |
|       11 |  7268 | `		if( n < (sxu32)nArg ){` |
|        - |  7269 | `			/* Argument provided — install with type casting */` |
|       11 |  7270 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       11 |  7271 | `			if( pObj ){` |
|       11 |  7272 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7273 | `				/* Type casting */` |
|       11 |  7274 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7275 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7276 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7277 | `						if( xCast ){` |
|      ! 0 |  7278 | `							xCast(pObj);` |
|      ! 0 |  7279 | `						}` |
|      ! 0 |  7280 | `					}` |
|      ! 0 |  7281 | `				}` |
|       11 |  7282 | `				sSlot.nIdx = pObj->nIdx;` |
|       11 |  7283 | `				sSlot.pUserData = 0;` |
|       11 |  7284 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        6 |  7285 | `			}` |
|        5 |  7286 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7287 | `			/* Default value */` |
|      ! 0 |  7288 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7289 | `			if( pObj ){` |
|      ! 0 |  7290 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7291 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7292 | `					return rc;` |
|        - |  7293 | `				}` |
|      ! 0 |  7294 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7295 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7296 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7297 | `						if( xCast ){` |
|      ! 0 |  7298 | `							xCast(pObj);` |
|      ! 0 |  7299 | `						}` |
|      ! 0 |  7300 | `					}` |
|      ! 0 |  7301 | `				}` |
|      ! 0 |  7302 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7303 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7304 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7305 | `			}` |
|      ! 0 |  7306 | `		}` |
|        6 |  7307 | `	}` |
|        - |  7308 | `	/* Install closure environment (captured variables) */` |
|       43 |  7309 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7310 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7311 | `		ph7_value *pValue;` |
|        - |  7312 | `		sxu32 iEnv;` |
|        3 |  7313 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7314 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7315 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7316 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7317 | `				continue;` |
|        - |  7318 | `			}` |
|        5 |  7319 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7320 | `			if( pValue == 0 ){` |
|      ! 0 |  7321 | `				continue;` |
|        - |  7322 | `			}` |
|        5 |  7323 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7324 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7325 | `		}` |
|        1 |  7326 | `	}` |
|       43 |  7327 | `	return SXRET_OK;` |
|       22 |  7328 |  |
|        - |  7329 | `/*` |
|        - |  7330 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7331 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7332 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7333 | ` */` |
|       26 |  7334 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7335 |  |
|       27 |  7336 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7337 | `	ph7_class_instance *pThis;` |
|        - |  7338 | `	ph7_class_instance *pClosureThis;` |
|        - |  7339 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7340 | `	ph7_vm_func *pFunc;` |
|        - |  7341 | `	ph7_value sResult;` |
|        - |  7342 | `	ph7_value *pCtxAttr;` |
|        - |  7343 | `	SyString sAttrName;` |
|        - |  7344 | `	sxi32 rc;` |
|       27 |  7345 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7346 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7347 | `	}` |
|       27 |  7348 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7349 | `	/* Check if already started (has a __ctx) */` |
|       27 |  7350 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       27 |  7351 | `	if( pExecCtx != 0 ){` |
|        3 |  7352 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7353 | `			"Cannot start a fiber that has already been started");` |
|        - |  7354 | `	}` |
|        - |  7355 | `	/* Resolve callable */` |
|       25 |  7356 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       25 |  7357 | `	if( pFunc == 0 ){` |
|      ! 0 |  7358 | `		return PH7_EXCEPTION;` |
|        - |  7359 | `	}` |
|        - |  7360 | `	/* Create execution context now that we know the function */` |
|       25 |  7361 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       25 |  7362 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7363 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7364 | `			"Fiber::start(): out of memory");` |
|        - |  7365 | `	}` |
|        - |  7366 | `	/* Store context in $this->__ctx */` |
|       25 |  7367 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       25 |  7368 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       25 |  7369 | `	if( pCtxAttr ){` |
|       25 |  7370 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       25 |  7371 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7372 | `	}` |
|        - |  7373 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7374 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7375 | `	 * into the fiber's frame, not the caller's. */` |
|       25 |  7376 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       25 |  7377 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7378 | `	/* Unpack the args array and install into the frame */` |
|        - |  7379 | `	{` |
|       25 |  7380 | `		ph7_value **apValues = 0;` |
|       25 |  7381 | `		int nActual = 0;` |
|       25 |  7382 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       25 |  7383 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7384 | `			ph7_hashmap_node *pNode;` |
|       25 |  7385 | `			sxu32 nCount = pMap->nEntry;` |
|       25 |  7386 | `			if( nCount > 0 ){` |
|        3 |  7387 | `				sxu32 idx = 0;` |
|        4 |  7388 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7389 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7390 | `				if( apValues ){` |
|        3 |  7391 | `					pNode = pMap->pFirst;` |
|        7 |  7392 | `					while( pNode && idx < nCount ){` |
|        5 |  7393 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7394 | `						idx++;` |
|        5 |  7395 | `						pNode = pNode->pPrev;` |
|        1 |  7396 | `					}` |
|        3 |  7397 | `					nActual = (int)idx;` |
|        1 |  7398 | `				}` |
|        1 |  7399 | `			}` |
|       12 |  7400 | `		}` |
|       25 |  7401 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       25 |  7402 | `		if( apValues ){` |
|        3 |  7403 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7404 | `		}` |
|        - |  7405 | `	}` |
|        - |  7406 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       25 |  7407 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       25 |  7408 | `	pExecCtx->pFrame->pParent = 0;` |
|       25 |  7409 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7410 | `		return PH7_ABORT;` |
|        - |  7411 | `	}` |
|       25 |  7412 | `	PH7_MemObjInit(pVm, &sResult);` |
|       25 |  7413 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       25 |  7414 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7415 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7416 | `		return PH7_ABORT;` |
|        - |  7417 | `	}` |
|       25 |  7418 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7419 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7420 | `		return PH7_EXCEPTION;` |
|        - |  7421 | `	}` |
|       25 |  7422 | `	ph7_result_value(pCtx, &sResult);` |
|       25 |  7423 | `	PH7_MemObjRelease(&sResult);` |
|       25 |  7424 | `	return PH7_OK;` |
|       14 |  7425 |  |
|        - |  7426 | `/*` |
|        - |  7427 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7428 | ` */` |
|       36 |  7429 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7430 |  |
|       37 |  7431 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7432 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7433 | `	ph7_value sResult;` |
|        - |  7434 | `	ph7_value *pResumeVal;` |
|        - |  7435 | `	sxi32 rc;` |
|       37 |  7436 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7437 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7438 | `		return PH7_OK;` |
|        - |  7439 | `	}` |
|       37 |  7440 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       37 |  7441 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7442 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7443 | `		return PH7_OK;` |
|        - |  7444 | `	}` |
|       37 |  7445 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7446 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7447 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7448 | `	}` |
|       35 |  7449 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       35 |  7450 | `	PH7_MemObjInit(pVm, &sResult);` |
|       35 |  7451 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       35 |  7452 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7453 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7454 | `		return PH7_ABORT;` |
|        - |  7455 | `	}` |
|       35 |  7456 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7457 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7458 | `		return PH7_EXCEPTION;` |
|        - |  7459 | `	}` |
|       35 |  7460 | `	ph7_result_value(pCtx, &sResult);` |
|       35 |  7461 | `	PH7_MemObjRelease(&sResult);` |
|       35 |  7462 | `	return PH7_OK;` |
|       19 |  7463 |  |
|        - |  7464 | `/*` |
|        - |  7465 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7466 | ` */` |
|        6 |  7467 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7468 |  |
|        7 |  7469 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7470 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7471 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7472 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7473 | `		return PH7_OK;` |
|        - |  7474 | `	}` |
|        7 |  7475 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        7 |  7476 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7477 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7478 | `		return PH7_OK;` |
|        - |  7479 | `	}` |
|        7 |  7480 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7481 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7482 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7483 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7484 | `		}` |
|      ! 0 |  7485 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7486 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7487 | `	}` |
|        7 |  7488 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        7 |  7489 | `	return PH7_OK;` |
|        4 |  7490 |  |
|        - |  7491 | `/*` |
|        - |  7492 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7493 | ` */` |
|        6 |  7494 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7495 |  |
|        - |  7496 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7497 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7498 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7499 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7500 | `	return PH7_OK;` |
|        4 |  7501 |  |
|      ! 0 |  7502 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7503 |  |
|        - |  7504 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7505 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7506 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7507 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7508 | `	return PH7_OK;` |
|      ! 0 |  7509 |  |
|        6 |  7510 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7511 |  |
|        - |  7512 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7513 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7514 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7515 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7516 | `	return PH7_OK;` |
|        4 |  7517 |  |
|        6 |  7518 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7519 |  |
|        - |  7520 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7521 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7522 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7523 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7524 | `	return PH7_OK;` |
|        4 |  7525 |  |
|        - |  7526 | `/*` |
|        - |  7527 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7528 | ` */` |
|      ! 0 |  7529 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7530 |  |
|      ! 0 |  7531 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7532 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7533 | `	if( nArg < 1 ){` |
|      ! 0 |  7534 | `		return PH7_OK;` |
|        - |  7535 | `	}` |
|      ! 0 |  7536 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|      ! 0 |  7537 | `	if( pExecCtx ){` |
|      ! 0 |  7538 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7539 | `		/* Clear the attribute so double-free is prevented */` |
|      ! 0 |  7540 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7541 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7542 | `			SyString sAttrName;` |
|        - |  7543 | `			ph7_value *pAttr;` |
|      ! 0 |  7544 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7545 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7546 | `			if( pAttr ){` |
|      ! 0 |  7547 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7548 | `			}` |
|      ! 0 |  7549 | `		}` |
|      ! 0 |  7550 | `	}` |
|      ! 0 |  7551 | `	return PH7_OK;` |
|      ! 0 |  7552 |  |
|        - |  7553 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7554 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7555 |  |
|        - |  7556 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7557 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7558 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7559 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7560 |  |
|      ! 0 |  7561 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7562 |  |
|        - |  7563 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7564 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7565 | `	ph7_exec_ctx *pCtx;` |
|        - |  7566 | `	ph7_vm_func *pFunc;` |
|        - |  7567 | `	ph7_value *pCallable;` |
|        - |  7568 | `	ph7_value *pCtxAttr;` |
|        - |  7569 | `	SyString sAttrName;` |
|        - |  7570 | `	/* Must not already be started */` |
|      ! 0 |  7571 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7572 | `	if( pCtx != 0 ){` |
|      ! 0 |  7573 | `		return SXERR_INVALID;` |
|        - |  7574 | `	}` |
|      ! 0 |  7575 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7576 | `		return SXERR_INVALID;` |
|        - |  7577 | `	}` |
|      ! 0 |  7578 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7579 | `	/* Get the callable */` |
|      ! 0 |  7580 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7581 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7582 | `	if( pCallable == 0 ){` |
|      ! 0 |  7583 | `		return SXERR_INVALID;` |
|        - |  7584 | `	}` |
|        - |  7585 | `	/* Resolve callable */` |
|      ! 0 |  7586 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7587 | `		SyString sName;` |
|        - |  7588 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7589 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7590 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7591 | `		if( pEntry == 0 ){` |
|      ! 0 |  7592 | `			return SXERR_NOTFOUND;` |
|        - |  7593 | `		}` |
|      ! 0 |  7594 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7595 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7596 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7597 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7598 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7599 | `		if( pMethod == 0 ){` |
|      ! 0 |  7600 | `			return SXERR_INVALID;` |
|        - |  7601 | `		}` |
|      ! 0 |  7602 | `		pClosureThis = pClosure;` |
|      ! 0 |  7603 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7604 | `	}else{` |
|      ! 0 |  7605 | `		return SXERR_INVALID;` |
|        - |  7606 | `	}` |
|        - |  7607 | `	/* Create context */` |
|      ! 0 |  7608 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7609 | `	if( pCtx == 0 ){` |
|      ! 0 |  7610 | `		return SXERR_MEM;` |
|        - |  7611 | `	}` |
|        - |  7612 | `	/* Store in __ctx */` |
|      ! 0 |  7613 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7614 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7615 | `	if( pCtxAttr ){` |
|      ! 0 |  7616 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7617 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7618 | `	}` |
|        - |  7619 | `	/* Set up frame with args */` |
|      ! 0 |  7620 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7621 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7622 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7623 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7624 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7625 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7626 |  |
|      ! 0 |  7627 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7628 |  |
|      ! 0 |  7629 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7630 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7631 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7632 |  |
|      ! 0 |  7633 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7634 |  |
|      ! 0 |  7635 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7636 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7637 |  |
|      ! 0 |  7638 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7639 |  |
|      ! 0 |  7640 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7641 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7642 |  |
|      ! 0 |  7643 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7644 |  |
|      ! 0 |  7645 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7646 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7647 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7648 |  |
|        - |  7649 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7650 | `/*` |
|        - |  7651 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7652 | ` */` |
|       18 |  7653 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7654 |  |
|        - |  7655 | `	ph7_generator *pGen;` |
|       19 |  7656 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       19 |  7657 | `	if( pGen == 0 ){` |
|      ! 0 |  7658 | `		return 0;` |
|        - |  7659 | `	}` |
|       19 |  7660 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       19 |  7661 | `	pGen->pCtx = pCtx;` |
|       19 |  7662 | `	pGen->iImplicitKey = 0;` |
|       19 |  7663 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       19 |  7664 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7665 | `	/* Link the generator back to the exec context */` |
|       19 |  7666 | `	pCtx->pPrivate = pGen;` |
|       19 |  7667 | `	return pGen;` |
|       10 |  7668 |  |
|        - |  7669 | `/*` |
|        - |  7670 | ` * Release a generator and its execution context.` |
|        - |  7671 | ` */` |
|      ! 0 |  7672 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7673 |  |
|      ! 0 |  7674 | `	if( pGen == 0 ){` |
|      ! 0 |  7675 | `		return;` |
|        - |  7676 | `	}` |
|      ! 0 |  7677 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7678 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7679 | `	if( pGen->pCtx ){` |
|      ! 0 |  7680 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7681 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7682 | `		pGen->pCtx = 0;` |
|      ! 0 |  7683 | `	}` |
|      ! 0 |  7684 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7685 |  |
|        - |  7686 | `/*` |
|        - |  7687 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7688 | ` */` |
|      192 |  7689 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        1 |  7690 |  |
|        - |  7691 | `	ph7_class_instance *pThis;` |
|        - |  7692 | `	SyString sAttr;` |
|        - |  7693 | `	ph7_value *pAttr;` |
|      193 |  7694 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7695 | `		return 0;` |
|        - |  7696 | `	}` |
|      193 |  7697 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      193 |  7698 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7699 | `		return 0;` |
|        - |  7700 | `	}` |
|      193 |  7701 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      193 |  7702 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      193 |  7703 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7704 | `		return 0;` |
|        - |  7705 | `	}` |
|      193 |  7706 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       97 |  7707 |  |
|        - |  7708 | `/*` |
|        - |  7709 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7710 | ` */` |
|       18 |  7711 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7712 |  |
|        - |  7713 | `	ph7_generator *pGen;` |
|        - |  7714 | `	sxi32 rc;` |
|       19 |  7715 | `	if( nArg < 1 ) return PH7_OK;` |
|       19 |  7716 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       19 |  7717 | `	if( pGen == 0 ) return PH7_OK;` |
|       19 |  7718 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       19 |  7719 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       19 |  7720 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       19 |  7721 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7722 | `	}` |
|       19 |  7723 | `	return PH7_OK;` |
|       10 |  7724 |  |
|        - |  7725 | `/*` |
|        - |  7726 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7727 | ` */` |
|       52 |  7728 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7729 |  |
|        - |  7730 | `	ph7_generator *pGen;` |
|       53 |  7731 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       53 |  7732 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       53 |  7733 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       53 |  7734 | `	return PH7_OK;` |
|       27 |  7735 |  |
|        - |  7736 | `/*` |
|        - |  7737 | ` * Generator::current() — return the last yielded value.` |
|        - |  7738 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7739 | ` */` |
|       56 |  7740 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7741 |  |
|        - |  7742 | `	ph7_generator *pGen;` |
|        - |  7743 | `	sxi32 rc;` |
|       57 |  7744 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       57 |  7745 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       57 |  7746 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       57 |  7747 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7748 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7749 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7750 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7751 | `	}` |
|       57 |  7752 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       57 |  7753 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       29 |  7754 | `	}else{` |
|      ! 0 |  7755 | `		ph7_result_null(pCtx);` |
|        - |  7756 | `	}` |
|       57 |  7757 | `	return PH7_OK;` |
|       29 |  7758 |  |
|        - |  7759 | `/*` |
|        - |  7760 | ` * Generator::key() — return the last yielded key.` |
|        - |  7761 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7762 | ` */` |
|       12 |  7763 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7764 |  |
|        - |  7765 | `	ph7_generator *pGen;` |
|        - |  7766 | `	sxi32 rc;` |
|       13 |  7767 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7768 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7769 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7770 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7771 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7772 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7773 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7774 | `	}` |
|       13 |  7775 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7776 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7777 | `	}else{` |
|      ! 0 |  7778 | `		ph7_result_null(pCtx);` |
|        - |  7779 | `	}` |
|       13 |  7780 | `	return PH7_OK;` |
|        7 |  7781 |  |
|        - |  7782 | `/*` |
|        - |  7783 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7784 | ` */` |
|       48 |  7785 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7786 |  |
|        - |  7787 | `	ph7_generator *pGen;` |
|        - |  7788 | `	sxi32 rc;` |
|       49 |  7789 | `	if( nArg < 1 ) return PH7_OK;` |
|       49 |  7790 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       49 |  7791 | `	if( pGen == 0 ) return PH7_OK;` |
|       49 |  7792 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7793 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       49 |  7794 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       49 |  7795 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       25 |  7796 | `	}else{` |
|      ! 0 |  7797 | `		return PH7_OK;` |
|        - |  7798 | `	}` |
|       49 |  7799 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       49 |  7800 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       49 |  7801 | `	return PH7_OK;` |
|       25 |  7802 |  |
|        - |  7803 | `/*` |
|        - |  7804 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7805 | ` */` |
|        4 |  7806 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7807 |  |
|        - |  7808 | `	ph7_generator *pGen;` |
|        - |  7809 | `	ph7_value *pSendVal;` |
|        - |  7810 | `	sxi32 rc;` |
|        5 |  7811 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7812 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7813 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7814 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7815 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7816 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7817 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7818 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7819 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7820 | `	}else{` |
|      ! 0 |  7821 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7822 | `		return PH7_OK;` |
|        - |  7823 | `	}` |
|        5 |  7824 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7825 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7826 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7827 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7828 | `	}else{` |
|        3 |  7829 | `		ph7_result_null(pCtx);` |
|        - |  7830 | `	}` |
|        5 |  7831 | `	return PH7_OK;` |
|        3 |  7832 |  |
|        - |  7833 | `/*` |
|        - |  7834 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7835 | ` *` |
|        - |  7836 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7837 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7838 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7839 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7840 | ` * the exception to the caller.` |
|        - |  7841 | ` */` |
|      ! 0 |  7842 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7843 |  |
|        - |  7844 | `	ph7_generator *pGen;` |
|        - |  7845 | `	const char *zMsg;` |
|        - |  7846 | `	int nLen;` |
|      ! 0 |  7847 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7848 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7849 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7850 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7851 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7852 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7853 | `			"Cannot throw into a closed generator");` |
|        - |  7854 | `	}` |
|        - |  7855 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7856 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7857 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7858 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7859 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7860 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7861 | `	nLen = 0;` |
|      ! 0 |  7862 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7863 | `		/* Try to get the exception's message */` |
|        - |  7864 | `		SyString sAttr;` |
|        - |  7865 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  7866 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  7867 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  7868 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  7869 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  7870 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  7871 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  7872 | `		}` |
|      ! 0 |  7873 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  7874 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  7875 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  7876 | `	}` |
|      ! 0 |  7877 | `	(void)nLen;` |
|      ! 0 |  7878 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  7879 |  |
|        - |  7880 | `/*` |
|        - |  7881 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  7882 | ` */` |
|        2 |  7883 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7884 |  |
|        - |  7885 | `	ph7_generator *pGen;` |
|        3 |  7886 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7887 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  7888 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  7889 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7890 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7891 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  7892 | `	}` |
|        3 |  7893 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  7894 | `	return PH7_OK;` |
|        2 |  7895 |  |
|        - |  7896 | `/*` |
|        - |  7897 | ` * Generator::__destruct() — clean up.` |
|        - |  7898 | ` */` |
|      ! 0 |  7899 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7900 |  |
|        - |  7901 | `	ph7_generator *pGen;` |
|      ! 0 |  7902 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  7903 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7904 | `	if( pGen ){` |
|      ! 0 |  7905 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  7906 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7907 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7908 | `			SyString sAttrName;` |
|        - |  7909 | `			ph7_value *pAttr;` |
|      ! 0 |  7910 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7911 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7912 | `			if( pAttr ){` |
|      ! 0 |  7913 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  7914 | `			}` |
|      ! 0 |  7915 | `		}` |
|      ! 0 |  7916 | `	}` |
|      ! 0 |  7917 | `	return PH7_OK;` |
|      ! 0 |  7918 |  |
|        - |  7919 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  7920 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  7921 | `/*` |
|        - |  7922 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  7923 | ` * the desired message.` |
|        - |  7924 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  7925 | ` * in 'api.c' for additional information.` |
|        - |  7926 | ` */` |
|      350 |  7927 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  7928 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  7929 | `	SyString *pString /* Message to output */` |
|        - |  7930 | `	)` |
|        2 |  7931 |  |
|      352 |  7932 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  7933 | `	sxi32 rc = SXRET_OK;` |
|        - |  7934 | `	/* Call the output consumer */` |
|      352 |  7935 | `	if( pString->nByte > 0 ){` |
|      352 |  7936 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  7937 | `		VmTrackOutput(pVm, pString->nByte);` |
|      175 |  7938 | `	}` |
|      352 |  7939 | `	return rc;` |
|        2 |  7940 |  |
|        - |  7941 | `/*` |
|        - |  7942 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  7943 | ` * callback to consume the formatted message.` |
|        - |  7944 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  7945 | ` * in 'api.c' for additional information.` |
|        - |  7946 | ` */` |
|        2 |  7947 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  7948 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  7949 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  7950 | `	va_list ap           /* Variable list of arguments */` |
|        - |  7951 | `	)` |
|        1 |  7952 |  |
|        3 |  7953 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  7954 | `	sxi32 rc = SXRET_OK;` |
|        - |  7955 | `	SyBlob sWorker;` |
|        - |  7956 | `	/* Format the message and call the output consumer */` |
|        3 |  7957 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  7958 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  7959 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  7960 | `		/* Consume the formatted message */` |
|        3 |  7961 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  7962 | `	}` |
|        3 |  7963 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  7964 | `	/* Release the working buffer */` |
|        3 |  7965 | `	SyBlobRelease(&sWorker);` |
|        3 |  7966 | `	return rc;` |
|        1 |  7967 |  |
|        - |  7968 | `/*` |
|        - |  7969 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  7970 | ` * This function never fail and always return a pointer` |
|        - |  7971 | ` * to a null terminated string.` |
|        - |  7972 | ` */` |
|       12 |  7973 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  7974 |  |
|       13 |  7975 | `	const char *zOp = "Unknown     ";` |
|       13 |  7976 | `	switch(nOp){` |
|        3 |  7977 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  7978 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  7979 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  7980 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  7981 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  7982 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  7983 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  7984 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  7985 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  7986 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  7987 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  7988 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  7989 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  7990 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  7991 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  7992 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  7993 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  7994 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  7995 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  7996 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  7997 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  7998 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  7999 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8000 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8001 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8002 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8003 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8004 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8005 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8006 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8007 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8008 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8009 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8010 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8011 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8012 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8013 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8014 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8015 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8016 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8017 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8018 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8019 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8020 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8021 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8022 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8023 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8024 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8025 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8026 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8027 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8028 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8029 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8030 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8031 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8032 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8033 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8034 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8035 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8036 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8037 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8038 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8039 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8040 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8041 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8042 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8043 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8044 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8045 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8046 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8047 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8048 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8049 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8050 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8051 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8052 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8053 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8054 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8055 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8056 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8057 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8058 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8059 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8060 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8061 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8062 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8063 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8064 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8065 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8066 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8067 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8068 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8069 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8070 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8071 | `	default:` |
|      ! 0 |  8072 | `		break;` |
|        - |  8073 | `	}` |
|       13 |  8074 | `	return zOp;` |
|        1 |  8075 |  |
|        - |  8076 | `/*` |
|        - |  8077 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8078 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8079 | ` * is responsible of consuming the generated dump.` |
|        - |  8080 | ` */` |
|        2 |  8081 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8082 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8083 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8084 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8085 | `	)` |
|        1 |  8086 |  |
|        - |  8087 | `	sxi32 rc;` |
|        3 |  8088 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8089 | `	return rc;` |
|        1 |  8090 |  |
|        - |  8091 | `/*` |
|        - |  8092 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8093 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8094 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8095 | ` * in 'compile.c' for additional information.` |
|        - |  8096 | ` */` |
|        8 |  8097 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8098 |  |
|        9 |  8099 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8100 | `	/* Evaluate and expand constant value */` |
|        9 |  8101 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  8102 |  |
|        - |  8103 | `/*` |
|        - |  8104 | ` * Section:` |
|        - |  8105 | ` *  Function handling functions.` |
|        - |  8106 | ` * Status:` |
|        - |  8107 | ` *    Stable.` |
|        - |  8108 | ` */` |
|        - |  8109 | `/*` |
|        - |  8110 | ` * int func_num_args(void)` |
|        - |  8111 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8112 | ` * Parameters` |
|        - |  8113 | ` *   None.` |
|        - |  8114 | ` * Return` |
|        - |  8115 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8116 | ` *  or -1 if called from the globe scope.` |
|        - |  8117 | ` */` |
|      928 |  8118 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8119 |  |
|        - |  8120 | `	VmFrame *pFrame;` |
|        - |  8121 | `	ph7_vm *pVm;` |
|        - |  8122 | `	/* Point to the target VM */` |
|      930 |  8123 | `	pVm = pCtx->pVm;` |
|        - |  8124 | `	/* Current frame */` |
|      930 |  8125 | `	pFrame = pVm->pFrame;` |
|      930 |  8126 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      930 |  8127 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8128 | `		SXUNUSED(nArg);` |
|      ! 0 |  8129 | `		SXUNUSED(apArg);` |
|        - |  8130 | `		/* Global frame,return -1 */` |
|      ! 0 |  8131 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8132 | `		return SXRET_OK;` |
|        - |  8133 | `	}` |
|        - |  8134 | `	/* Total number of arguments passed to the enclosing function */` |
|      930 |  8135 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      930 |  8136 | `	ph7_result_int(pCtx,nArg);` |
|      930 |  8137 | `	return SXRET_OK;` |
|      466 |  8138 |  |
|        - |  8139 | `/*` |
|        - |  8140 | ` * value func_get_arg(int $arg_num)` |
|        - |  8141 | ` *   Return an item from the argument list.` |
|        - |  8142 | ` * Parameters` |
|        - |  8143 | ` *  Argument number(index start from zero).` |
|        - |  8144 | ` * Return` |
|        - |  8145 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8146 | ` */` |
|       22 |  8147 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8148 |  |
|       24 |  8149 | `	ph7_value *pObj = 0;` |
|       24 |  8150 | `	VmSlot *pSlot = 0;` |
|        - |  8151 | `	VmFrame *pFrame;` |
|        - |  8152 | `	ph7_vm *pVm;` |
|        - |  8153 | `	/* Point to the target VM */` |
|       24 |  8154 | `	pVm = pCtx->pVm;` |
|        - |  8155 | `	/* Current frame */` |
|       24 |  8156 | `	pFrame = pVm->pFrame;` |
|       24 |  8157 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8158 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8159 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8160 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8161 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8162 | `		return SXRET_OK;` |
|        - |  8163 | `	}` |
|        - |  8164 | `	/* Extract the desired index */` |
|       21 |  8165 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8166 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8167 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8168 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8169 | `		return SXRET_OK;` |
|        - |  8170 | `	}` |
|        - |  8171 | `	/* Extract the desired argument */` |
|       21 |  8172 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8173 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8174 | `			/* Return the desired argument */` |
|       21 |  8175 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8176 | `		}else{` |
|        - |  8177 | `			/* No such argument,return false */` |
|      ! 0 |  8178 | `			ph7_result_bool(pCtx,0);` |
|        - |  8179 | `		}` |
|       11 |  8180 | `	}else{` |
|        - |  8181 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8182 | `		ph7_result_bool(pCtx,0);` |
|        - |  8183 | `	}` |
|       21 |  8184 | `	return SXRET_OK;` |
|       13 |  8185 |  |
|        - |  8186 | `/*` |
|        - |  8187 | ` * array func_get_args_byref(void)` |
|        - |  8188 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8189 | ` * Parameters` |
|        - |  8190 | ` *  None.` |
|        - |  8191 | ` * Return` |
|        - |  8192 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8193 | ` *  member of the current user-defined function's argument list.` |
|        - |  8194 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8195 | ` * NOTE:` |
|        - |  8196 | ` *  Arguments are returned to the array by reference.` |
|        - |  8197 | ` */` |
|        2 |  8198 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8199 |  |
|        - |  8200 | `	ph7_value *pArray;` |
|        - |  8201 | `	VmFrame *pFrame;` |
|        - |  8202 | `	VmSlot *aSlot;` |
|        - |  8203 | `	sxu32 n;` |
|        - |  8204 | `	/* Point to the current frame */` |
|        3 |  8205 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8206 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8207 | `	if( pFrame->pParent == 0 ){` |
|        - |  8208 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8209 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8210 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8211 | `		return SXRET_OK;` |
|        - |  8212 | `	}` |
|        - |  8213 | `	/* Create a new array */` |
|        3 |  8214 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8215 | `	if( pArray == 0 ){` |
|      ! 0 |  8216 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8217 | `		SXUNUSED(apArg);` |
|      ! 0 |  8218 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8219 | `		return SXRET_OK;` |
|        - |  8220 | `	}` |
|        - |  8221 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8222 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8223 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8224 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8225 | `	}` |
|        - |  8226 | `	/* Return the freshly created array */` |
|        3 |  8227 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8228 | `	return SXRET_OK;` |
|        2 |  8229 |  |
|        - |  8230 | `/*` |
|        - |  8231 | ` * array func_get_args(void)` |
|        - |  8232 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8233 | ` * Parameters` |
|        - |  8234 | ` *  None.` |
|        - |  8235 | ` * Return` |
|        - |  8236 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8237 | ` *  member of the current user-defined function's argument list.` |
|        - |  8238 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8239 | ` */` |
|       88 |  8240 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8241 |  |
|       90 |  8242 | `	ph7_value *pObj = 0;` |
|        - |  8243 | `	ph7_value *pArray;` |
|        - |  8244 | `	VmFrame *pFrame;` |
|        - |  8245 | `	VmSlot *aSlot;` |
|        - |  8246 | `	sxu32 n;` |
|        - |  8247 | `	/* Point to the current frame */` |
|       90 |  8248 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8249 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8250 | `	if( pFrame->pParent == 0 ){` |
|        - |  8251 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8252 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8253 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8254 | `		return SXRET_OK;` |
|        - |  8255 | `	}` |
|        - |  8256 | `	/* Create a new array */` |
|       90 |  8257 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8258 | `	if( pArray == 0 ){` |
|      ! 0 |  8259 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8260 | `		SXUNUSED(apArg);` |
|      ! 0 |  8261 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8262 | `		return SXRET_OK;` |
|        - |  8263 | `	}` |
|        - |  8264 | `	/* Start filling the array with the given arguments */` |
|       90 |  8265 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8266 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8267 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8268 | `		if( pObj ){` |
|      134 |  8269 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8270 | `		}` |
|       68 |  8271 | `	}` |
|        - |  8272 | `	/* Return the freshly created array */` |
|       90 |  8273 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8274 | `	return SXRET_OK;` |
|       46 |  8275 |  |
|        - |  8276 | `/*` |
|        - |  8277 | ` * bool function_exists(string $name)` |
|        - |  8278 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8279 | ` * Parameters` |
|        - |  8280 | ` *  The name of the desired function.` |
|        - |  8281 | ` * Return` |
|        - |  8282 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8283 | ` */` |
|     1684 |  8284 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8285 |  |
|        - |  8286 | `	const char *zName;` |
|        - |  8287 | `	ph7_vm *pVm;` |
|        - |  8288 | `	int nLen;` |
|        - |  8289 | `	int res;` |
|     1686 |  8290 | `	if( nArg < 1 ){` |
|        - |  8291 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8292 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8293 | `		return SXRET_OK;` |
|        - |  8294 | `	}` |
|        - |  8295 | `	/* Point to the target VM */` |
|     1686 |  8296 | `	pVm = pCtx->pVm;` |
|        - |  8297 | `	/* Extract the function name */` |
|     1686 |  8298 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8299 | `	/* Assume the function is not defined */` |
|     1686 |  8300 | `	res = 0;` |
|        - |  8301 | `	/* Perform the lookup */` |
|     2526 |  8302 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1680 |  8303 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8304 | `			/* Function is defined */` |
|      206 |  8305 | `			res = 1;` |
|      102 |  8306 | `	}` |
|     1686 |  8307 | `	ph7_result_bool(pCtx,res);` |
|     1686 |  8308 | `	return SXRET_OK;` |
|      844 |  8309 |  |
|        - |  8310 | `/*` |
|        - |  8311 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8312 | ` * [i.e: Whether it is callable or not].` |
|        - |  8313 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8314 | ` */` |
|    16236 |  8315 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8316 |  |
|    16238 |  8317 | `	int res = 0;` |
|    16238 |  8318 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8319 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8320 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8321 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8322 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8323 | `		if( pMethod && CallInvoke ){` |
|        - |  8324 | `			ph7_value sResult;` |
|        - |  8325 | `			sxi32 rc;` |
|        - |  8326 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8327 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8328 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8329 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8330 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8331 | `			}` |
|      ! 0 |  8332 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8333 | `		}` |
|    16238 |  8334 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8335 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8336 | `		if( pMap->nEntry == 2 ){` |
|        - |  8337 | `			ph7_class *pClass;` |
|        - |  8338 | `			ph7_value *pV;` |
|        - |  8339 | `			/* Extract the target class */` |
|       12 |  8340 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8341 | `			if( pV ){` |
|       12 |  8342 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8343 | `				if( pClass ){` |
|        - |  8344 | `					ph7_class_method *pMethod;` |
|        - |  8345 | `					/* Extract the target method */` |
|       10 |  8346 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8347 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8348 | `						/* Perform the lookup */` |
|       10 |  8349 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8350 | `						if( pMethod ){` |
|        - |  8351 | `							/* Method is callable */` |
|        5 |  8352 | `							res = 1;` |
|        2 |  8353 | `						}` |
|        4 |  8354 | `					}` |
|        4 |  8355 | `				}` |
|        5 |  8356 | `			}` |
|        7 |  8357 | `		}` |
|    16225 |  8358 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8359 | `		const char *zName;` |
|        - |  8360 | `		int nLen;` |
|        - |  8361 | `		/* Extract the name */` |
|     4752 |  8362 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8363 | `		/* Perform the lookup */` |
|     4767 |  8364 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8365 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8366 | `				/* Function is callable */` |
|     4734 |  8367 | `				res = 1;` |
|     2366 |  8368 | `		}` |
|     2375 |  8369 | `	}` |
|    16238 |  8370 | `	return res;` |
|        2 |  8371 |  |
|        - |  8372 | `/*` |
|        - |  8373 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8374 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8375 | ` * Parameters` |
|        - |  8376 | ` * $name` |
|        - |  8377 | ` *    The callback function to check` |
|        - |  8378 | ` * $syntax_only` |
|        - |  8379 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8380 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8381 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8382 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8383 | ` *    a string.` |
|        - |  8384 | ` * Return` |
|        - |  8385 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8386 | ` */` |
|       14 |  8387 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8388 |  |
|        - |  8389 | `	ph7_vm *pVm;` |
|        - |  8390 | `	int res;` |
|       15 |  8391 | `	if( nArg < 1 ){` |
|        - |  8392 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8393 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8394 | `		return SXRET_OK;` |
|        - |  8395 | `	}` |
|        - |  8396 | `	/* Point to the target VM */` |
|       15 |  8397 | `	pVm = pCtx->pVm;` |
|        - |  8398 | `	/* Perform the requested operation */` |
|       15 |  8399 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8400 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8401 | `	return SXRET_OK;` |
|        8 |  8402 |  |
|        - |  8403 | `/*` |
|        - |  8404 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8405 | ` * defined below.` |
|        - |  8406 | ` */` |
|     1188 |  8407 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8408 |  |
|     1189 |  8409 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8410 | `	ph7_value sName;` |
|        - |  8411 | `	sxi32 rc;` |
|        - |  8412 | `	/* Prepare the function name for insertion */` |
|     1189 |  8413 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1189 |  8414 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8415 | `	/* Perform the insertion */` |
|     1189 |  8416 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1189 |  8417 | `	PH7_MemObjRelease(&sName);` |
|     1189 |  8418 | `	return rc;` |
|        1 |  8419 |  |
|        - |  8420 | `/*` |
|        - |  8421 | ` * array get_defined_functions(void)` |
|        - |  8422 | ` *  Returns an array of all defined functions.` |
|        - |  8423 | ` * Parameter` |
|        - |  8424 | ` *  None.` |
|        - |  8425 | ` * Return` |
|        - |  8426 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8427 | ` *  both built-in (internal) and user-defined.` |
|        - |  8428 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8429 | ` *  defined ones using $arr["user"].` |
|        - |  8430 | ` * Note:` |
|        - |  8431 | ` *  NULL is returned on failure.` |
|        - |  8432 | ` */` |
|        2 |  8433 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8434 |  |
|        - |  8435 | `	ph7_value *pArray,*pEntry;` |
|        - |  8436 | `	/* NOTE:` |
|        - |  8437 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8438 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8439 | `	 */` |
|        3 |  8440 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8441 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8442 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8443 | `		SXUNUSED(apArg);` |
|        - |  8444 | `		/* Return NULL */` |
|      ! 0 |  8445 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8446 | `		return SXRET_OK;` |
|        - |  8447 | `	}` |
|        3 |  8448 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8449 | `	if( pEntry == 0 ){` |
|        - |  8450 | `		/* Return NULL */` |
|      ! 0 |  8451 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8452 | `		return SXRET_OK;` |
|        - |  8453 | `	}` |
|        - |  8454 | `	/* Fill with the appropriate information */` |
|        3 |  8455 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8456 | `	/* Create the 'internal' index */` |
|        3 |  8457 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8458 | `	/* Create the user-func array */` |
|        3 |  8459 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8460 | `	if( pEntry == 0 ){` |
|        - |  8461 | `		/* Return NULL */` |
|      ! 0 |  8462 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8463 | `		return SXRET_OK;` |
|        - |  8464 | `	}` |
|        - |  8465 | `	/* Fill with the appropriate information */` |
|        3 |  8466 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8467 | `	/* Create the 'user' index */` |
|        3 |  8468 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8469 | `	/* Return the multi-dimensional array */` |
|        3 |  8470 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8471 | `	return SXRET_OK;` |
|        2 |  8472 |  |
|        - |  8473 | `/*` |
|        - |  8474 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8475 | ` *  Register a function for execution on shutdown.` |
|        - |  8476 | ` * Note` |
|        - |  8477 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8478 | ` *  be called in the same order as they were registered.` |
|        - |  8479 | ` * Parameters` |
|        - |  8480 | ` *  $callback` |
|        - |  8481 | ` *   The shutdown callback to register.` |
|        - |  8482 | ` * $param` |
|        - |  8483 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8484 | ` * Return` |
|        - |  8485 | ` *  Nothing.` |
|        - |  8486 | ` */` |
|        2 |  8487 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8488 |  |
|        - |  8489 | `	VmShutdownCB sEntry;` |
|        - |  8490 | `	int i,j;` |
|        3 |  8491 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8492 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8493 | `		return PH7_OK;` |
|        - |  8494 | `	}` |
|        - |  8495 | `	/* Zero the Entry */` |
|        3 |  8496 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8497 | `	/* Initialize fields */` |
|        3 |  8498 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8499 | `	/* Save the callback name for later invocation name */` |
|        3 |  8500 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8501 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8502 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8503 | `	}` |
|        - |  8504 | `	/* Copy arguments */` |
|        3 |  8505 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8506 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8507 | `			/* Limit reached */` |
|      ! 0 |  8508 | `			break;` |
|        - |  8509 | `		}` |
|      ! 0 |  8510 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8511 | `	}` |
|        3 |  8512 | `	sEntry.nArg = j;` |
|        - |  8513 | `	/* Install the callback */` |
|        3 |  8514 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8515 | `	return PH7_OK;` |
|        2 |  8516 |  |
|        - |  8517 | `/*` |
|        - |  8518 | ` * Section:` |
|        - |  8519 | ` *  Class handling functions.` |
|        - |  8520 | ` * Status:` |
|        - |  8521 | ` *    Stable.` |
|        - |  8522 | ` */` |
|        - |  8523 | `/*` |
|        - |  8524 | ` * Extract the top active class. NULL is returned` |
|        - |  8525 | ` * if the class stack is empty.` |
|        - |  8526 | ` */` |
|      560 |  8527 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8528 |  |
|      562 |  8529 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8530 | `	ph7_class **apClass;` |
|      562 |  8531 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8532 | `		/* Empty stack,return NULL */` |
|       15 |  8533 | `		return 0;` |
|        - |  8534 | `	}` |
|        - |  8535 | `	/* Peek the last entry */` |
|      548 |  8536 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      548 |  8537 | `	return apClass[pSet->nUsed - 1];` |
|      282 |  8538 |  |
|        - |  8539 | `/*` |
|        - |  8540 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8541 | ` *   Get the class that declared the currently executing method.` |
|        - |  8542 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8543 | ` *` |
|        - |  8544 | ` * Parameters` |
|        - |  8545 | ` *   pVm: Target VM` |
|        - |  8546 | ` *` |
|        - |  8547 | ` * Return` |
|        - |  8548 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8549 | ` *   - Not executing within a class method` |
|        - |  8550 | ` *` |
|        - |  8551 | ` * Note` |
|        - |  8552 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8553 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8554 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8555 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8556 | ` *   declaring class.` |
|        - |  8557 | ` */` |
|       52 |  8558 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8559 |  |
|       54 |  8560 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8561 | `	ph7_vm_func *pVmFunc;` |
|        - |  8562 |  |
|        - |  8563 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  8564 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8565 |  |
|        - |  8566 | `	/* Check if we're in a method context */` |
|       54 |  8567 | `	if( pFrame->pParent ){` |
|       50 |  8568 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  8569 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8570 | `			/* Return the declaring class */` |
|       50 |  8571 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8572 | `		}` |
|      ! 0 |  8573 | `	}` |
|        - |  8574 |  |
|        5 |  8575 | `	return 0;` |
|       28 |  8576 |  |
|        - |  8577 |  |
|        - |  8578 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8579 | `/*` |
|        - |  8580 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8581 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8582 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8583 | ` * return value indicates failure.` |
|        - |  8584 | ` */` |
|     1488 |  8585 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8586 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8587 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8588 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8589 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8590 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8591 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8592 | `	)` |
|        2 |  8593 |  |
|        - |  8594 | `	ph7_value *aStack;` |
|        - |  8595 | `	VmInstr aInstr[2];` |
|        - |  8596 | `	int iCursor;` |
|        - |  8597 | `	int i;` |
|        - |  8598 | `	/* Create a new operand stack */` |
|     1490 |  8599 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1490 |  8600 | `	if( aStack == 0 ){` |
|      ! 0 |  8601 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8602 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8603 | `		return SXERR_MEM;` |
|        - |  8604 | `	}` |
|        - |  8605 | `	/* Fill the operand stack with the given arguments */` |
|     2096 |  8606 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      608 |  8607 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8608 | `		/*` |
|        - |  8609 | `		 * Symisc eXtension:` |
|        - |  8610 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8611 | `		 */` |
|      608 |  8612 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      305 |  8613 | `	}` |
|     1490 |  8614 | `	iCursor = nArg + 1;` |
|     1490 |  8615 | `	if( pThis ){` |
|        - |  8616 | `		/*` |
|        - |  8617 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8618 | `		 */` |
|     1484 |  8619 | `		pThis->iRef++; /* Increment reference count */` |
|     1484 |  8620 | `		aStack[i].x.pOther = pThis;` |
|     1484 |  8621 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      741 |  8622 | `	}` |
|     1490 |  8623 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1490 |  8624 | `	i++;` |
|        - |  8625 | `	/* Push method name */` |
|     1490 |  8626 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1490 |  8627 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1490 |  8628 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1490 |  8629 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8630 | `	/* Emit the CALL istruction */` |
|     1490 |  8631 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1490 |  8632 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1490 |  8633 | `	aInstr[0].iP2 = 0;` |
|     1490 |  8634 | `	aInstr[0].p3  = 0;` |
|        - |  8635 | `	/* Emit the DONE instruction */` |
|     1490 |  8636 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1490 |  8637 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1490 |  8638 | `	aInstr[1].iP2 = 0;` |
|     1490 |  8639 | `	aInstr[1].p3  = 0;` |
|        - |  8640 | `	/* Execute the method body (if available) */` |
|     1490 |  8641 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8642 | `	/* Clean up the mess left behind */` |
|     1490 |  8643 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1490 |  8644 | `	return PH7_OK;` |
|      746 |  8645 |  |
|        - |  8646 | `/*` |
|        - |  8647 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8648 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8649 | ` * in the apArg[] array.` |
|        - |  8650 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8651 | ` * return value indicates failure.` |
|        - |  8652 | ` */` |
|      930 |  8653 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8654 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8655 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8656 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8657 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8658 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8659 | `	)` |
|        2 |  8660 |  |
|        - |  8661 | `	ph7_value *aStack;` |
|        - |  8662 | `	VmInstr aInstr[2];` |
|        - |  8663 | `	int i;` |
|      932 |  8664 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8665 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  8666 | `		if( pResult ){` |
|        - |  8667 | `			/* Assume a null return value */` |
|      ! 0 |  8668 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8669 | `		}` |
|      471 |  8670 | `		return SXERR_INVALID;` |
|        - |  8671 | `	}` |
|      462 |  8672 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8673 | `		/* Class method */` |
|       11 |  8674 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8675 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8676 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8677 | `		ph7_class *pClass = 0;` |
|        - |  8678 | `		ph7_value *pValue;` |
|        - |  8679 | `		sxi32 rc;` |
|       11 |  8680 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8681 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8682 | `			if( pResult ){` |
|        - |  8683 | `				/* Assume a null return value */` |
|      ! 0 |  8684 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8685 | `			}` |
|      ! 0 |  8686 | `			return SXRET_OK;` |
|        - |  8687 | `		}` |
|        - |  8688 | `		/* Extract the class name or an instance of it */` |
|       11 |  8689 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8690 | `		if( pValue ){` |
|       11 |  8691 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8692 | `		}` |
|       11 |  8693 | `		if( pClass == 0 ){` |
|        - |  8694 | `			/* No such class,return NULL */` |
|      ! 0 |  8695 | `			if( pResult ){` |
|      ! 0 |  8696 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8697 | `			}` |
|      ! 0 |  8698 | `			return SXRET_OK;` |
|        - |  8699 | `		}` |
|       11 |  8700 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8701 | `			/* Point to the class instance */` |
|        5 |  8702 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8703 | `		}` |
|        - |  8704 | `		/* Try to extract the method */` |
|       11 |  8705 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8706 | `		if( pValue ){` |
|       11 |  8707 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8708 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8709 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8710 | `			}` |
|        5 |  8711 | `		}` |
|       11 |  8712 | `		if( pMethod == 0 ){` |
|        - |  8713 | `			/* No such method,return NULL */` |
|      ! 0 |  8714 | `			if( pResult ){` |
|      ! 0 |  8715 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8716 | `			}` |
|      ! 0 |  8717 | `			return SXRET_OK;` |
|        - |  8718 | `		}` |
|        - |  8719 | `		/* Call the class method */` |
|       11 |  8720 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8721 | `		return rc;` |
|        - |  8722 | `	}` |
|        - |  8723 | `	/* Create a new operand stack */` |
|      452 |  8724 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      452 |  8725 | `	if( aStack == 0 ){` |
|      ! 0 |  8726 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8727 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8728 | `		if( pResult ){` |
|        - |  8729 | `			/* Assume a null return value */` |
|      ! 0 |  8730 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8731 | `		}` |
|      ! 0 |  8732 | `		return SXERR_MEM;` |
|        - |  8733 | `	}` |
|        - |  8734 | `	/* Fill the operand stack with the given arguments */` |
|     1478 |  8735 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1028 |  8736 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8737 | `		/*` |
|        - |  8738 | `		 * Symisc eXtension:` |
|        - |  8739 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8740 | `		 */` |
|     1028 |  8741 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      515 |  8742 | `	}` |
|        - |  8743 | `	/* Push the function name */` |
|      452 |  8744 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      452 |  8745 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8746 | `	/* Emit the CALL istruction */` |
|      452 |  8747 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      452 |  8748 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      452 |  8749 | `	aInstr[0].iP2 = 0;` |
|      452 |  8750 | `	aInstr[0].p3  = 0;` |
|        - |  8751 | `	/* Emit the DONE instruction */` |
|      452 |  8752 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      452 |  8753 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      452 |  8754 | `	aInstr[1].iP2 = 0;` |
|      452 |  8755 | `	aInstr[1].p3  = 0;` |
|        - |  8756 | `	/* Execute the function body (if available) */` |
|      452 |  8757 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8758 | `	/* Clean up the mess left behind */` |
|      452 |  8759 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      452 |  8760 | `	return PH7_OK;` |
|      467 |  8761 |  |
|        - |  8762 | `/*` |
|        - |  8763 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8764 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8765 | ` * parameter.` |
|        - |  8766 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8767 | ` * return value indicates failure.` |
|        - |  8768 | ` */` |
|      236 |  8769 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8770 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8771 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8772 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8773 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8774 | `	)` |
|        1 |  8775 |  |
|        - |  8776 | `	ph7_value *pArg;` |
|        - |  8777 | `	SySet aArg;` |
|        - |  8778 | `	va_list ap;` |
|        - |  8779 | `	sxi32 rc;` |
|      237 |  8780 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8781 | `	/* Copy arguments one after one */` |
|      237 |  8782 | `	va_start(ap,pResult);` |
|      393 |  8783 | `	for(;;){` |
|      787 |  8784 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8785 | `		if( pArg == 0 ){` |
|      237 |  8786 | `			break;` |
|        - |  8787 | `		}` |
|      551 |  8788 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8789 | `	}` |
|        - |  8790 | `	/* Call the core routine */` |
|      237 |  8791 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8792 | `	/* Cleanup */` |
|      237 |  8793 | `	SySetRelease(&aArg);` |
|      237 |  8794 | `	return rc;` |
|        1 |  8795 |  |
|        - |  8796 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8797 | `/*` |
|        - |  8798 | ` * bool defined(string $name)` |
|        - |  8799 | ` *  Checks whether a given named constant exists.` |
|        - |  8800 | ` * Parameter:` |
|        - |  8801 | ` *  Name of the desired constant.` |
|        - |  8802 | ` * Return` |
|        - |  8803 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8804 | ` */` |
|       14 |  8805 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8806 |  |
|        - |  8807 | `	const char *zName;` |
|       16 |  8808 | `	int nLen = 0;` |
|       16 |  8809 | `	int res = 0;` |
|       16 |  8810 | `	if( nArg < 1 ){` |
|        - |  8811 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8812 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8813 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8814 | `		return SXRET_OK;` |
|        - |  8815 | `	}` |
|        - |  8816 | `	/* Extract constant name */` |
|       16 |  8817 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8818 | `	/* Perform the lookup */` |
|       16 |  8819 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8820 | `		/* Already defined */` |
|       10 |  8821 | `		res = 1;` |
|        4 |  8822 | `	}` |
|       16 |  8823 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8824 | `	return SXRET_OK;` |
|        9 |  8825 |  |
|        - |  8826 | `/*` |
|        - |  8827 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8828 | ` * below.` |
|        - |  8829 | ` */` |
|        8 |  8830 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8831 |  |
|       10 |  8832 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8833 | `	/* Expand constant value */` |
|       10 |  8834 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  8835 |  |
|        - |  8836 | `/*` |
|        - |  8837 | ` * bool define(string $constant_name,expression value)` |
|        - |  8838 | ` *  Defines a named constant at runtime.` |
|        - |  8839 | ` * Parameter:` |
|        - |  8840 | ` *  $constant_name` |
|        - |  8841 | ` *   The name of the constant` |
|        - |  8842 | ` *  $value` |
|        - |  8843 | ` *   Constant value` |
|        - |  8844 | ` * Return:` |
|        - |  8845 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8846 | ` */` |
|       10 |  8847 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8848 |  |
|        - |  8849 | `	const char *zName;  /* Constant name */` |
|        - |  8850 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  8851 | `	int nLen = 0;       /* Name length */` |
|        - |  8852 | `	sxi32 rc;` |
|       12 |  8853 | `	if( nArg < 2 ){` |
|        - |  8854 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8855 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8856 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8857 | `		return SXRET_OK;` |
|        - |  8858 | `	}` |
|       12 |  8859 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8860 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8861 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8862 | `		return SXRET_OK;` |
|        - |  8863 | `	}` |
|        - |  8864 | `	/* Extract constant name */` |
|       12 |  8865 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8866 | `	if( nLen < 1 ){` |
|      ! 0 |  8867 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  8868 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8869 | `		return SXRET_OK;` |
|        - |  8870 | `	}` |
|        - |  8871 | `	/* Duplicate constant value */` |
|       12 |  8872 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  8873 | `	if( pValue == 0 ){` |
|      ! 0 |  8874 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8875 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8876 | `		return SXRET_OK;` |
|        - |  8877 | `	}` |
|        - |  8878 | `	/* Initialize the memory object */` |
|       12 |  8879 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  8880 | `	/* Register the constant */` |
|       12 |  8881 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  8882 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8883 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  8884 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  8885 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8886 | `		return SXRET_OK;` |
|        - |  8887 | `	}` |
|        - |  8888 | `	/* Duplicate constant value */` |
|       12 |  8889 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  8890 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  8891 | `		/* Lower case the constant name */` |
|      ! 0 |  8892 | `		char *zCur = (char *)zName;` |
|      ! 0 |  8893 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  8894 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  8895 | `				/* UTF-8 stream */` |
|      ! 0 |  8896 | `				zCur++;` |
|      ! 0 |  8897 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  8898 | `					zCur++;` |
|      ! 0 |  8899 | `				}` |
|      ! 0 |  8900 | `				continue;` |
|        - |  8901 | `			}` |
|      ! 0 |  8902 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  8903 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  8904 | `				zCur[0] = (char)c;` |
|      ! 0 |  8905 | `			}` |
|      ! 0 |  8906 | `			zCur++;` |
|      ! 0 |  8907 | `		}` |
|        - |  8908 | `		/* Finally,register the constant */` |
|      ! 0 |  8909 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  8910 | `	}` |
|        - |  8911 | `	/* All done,return TRUE */` |
|       12 |  8912 | `	ph7_result_bool(pCtx,1);` |
|       12 |  8913 | `	return SXRET_OK;` |
|        7 |  8914 |  |
|        - |  8915 | `/*` |
|        - |  8916 | ` * value constant(string $name)` |
|        - |  8917 | ` *  Returns the value of a constant` |
|        - |  8918 | ` * Parameter` |
|        - |  8919 | ` *  $name` |
|        - |  8920 | ` *    Name of the constant.` |
|        - |  8921 | ` * Return` |
|        - |  8922 | ` *  Constant value or NULL if not defined.` |
|        - |  8923 | ` */` |
|        8 |  8924 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8925 |  |
|        - |  8926 | `	SyHashEntry *pEntry;` |
|        - |  8927 | `	ph7_constant *pCons;` |
|        - |  8928 | `	const char *zName; /* Constant name */` |
|        - |  8929 | `	ph7_value sVal;    /* Constant value */` |
|        - |  8930 | `	int nLen;` |
|       10 |  8931 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8932 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  8933 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  8934 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8935 | `		return SXRET_OK;` |
|        - |  8936 | `	}` |
|        - |  8937 | `	/* Extract the constant name */` |
|       10 |  8938 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8939 | `	/* Perform the query */` |
|       10 |  8940 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  8941 | `	if( pEntry == 0 ){` |
|        3 |  8942 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  8943 | `		ph7_result_null(pCtx);` |
|        3 |  8944 | `		return SXRET_OK;` |
|        - |  8945 | `	}` |
|        8 |  8946 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  8947 | `	/* Point to the structure that describe the constant */` |
|        8 |  8948 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  8949 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  8950 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  8951 | `	/* Return that value */` |
|        8 |  8952 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  8953 | `	/* Cleanup */` |
|        8 |  8954 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  8955 | `	return SXRET_OK;` |
|        6 |  8956 |  |
|        - |  8957 | `/*` |
|        - |  8958 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  8959 | ` * defined below.` |
|        - |  8960 | ` */` |
|      444 |  8961 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8962 |  |
|      445 |  8963 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8964 | `	ph7_value sName;` |
|        - |  8965 | `	sxi32 rc;` |
|        - |  8966 | `	/* Prepare the constant name for insertion */` |
|      445 |  8967 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      445 |  8968 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8969 | `	/* Perform the insertion */` |
|      445 |  8970 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      445 |  8971 | `	PH7_MemObjRelease(&sName);` |
|      445 |  8972 | `	return rc;` |
|        1 |  8973 |  |
|        - |  8974 | `/*` |
|        - |  8975 | ` * array get_defined_constants(void)` |
|        - |  8976 | ` *  Returns an associative array with the names of all defined` |
|        - |  8977 | ` *  constants.` |
|        - |  8978 | ` * Parameters` |
|        - |  8979 | ` *  NONE.` |
|        - |  8980 | ` * Returns` |
|        - |  8981 | ` *  Returns the names of all the constants currently defined.` |
|        - |  8982 | ` */` |
|        2 |  8983 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8984 |  |
|        - |  8985 | `	ph7_value *pArray;` |
|        - |  8986 | `	/* Create the array first*/` |
|        3 |  8987 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8988 | `	if( pArray == 0 ){` |
|      ! 0 |  8989 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8990 | `		SXUNUSED(apArg);` |
|        - |  8991 | `		/* Return NULL */` |
|      ! 0 |  8992 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8993 | `		return SXRET_OK;` |
|        - |  8994 | `	}` |
|        - |  8995 | `	/* Fill the array with the defined constants */` |
|        3 |  8996 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  8997 | `	/* Return the created array */` |
|        3 |  8998 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8999 | `	return SXRET_OK;` |
|        2 |  9000 |  |
|        - |  9001 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9002 | `/*` |
|        - |  9003 | ` * Section:` |
|        - |  9004 | ` *  Random numbers/string generators.` |
|        - |  9005 | ` * Status:` |
|        - |  9006 | ` *    Stable.` |
|        - |  9007 | ` */` |
|        - |  9008 | `/*` |
|        - |  9009 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9010 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9011 | ` * used by te SQLite3 library.` |
|        - |  9012 | ` */` |
|     2658 |  9013 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9014 |  |
|        - |  9015 | `	sxu32 iNum;` |
|     2660 |  9016 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2660 |  9017 | `	return iNum;` |
|        2 |  9018 |  |
|        - |  9019 | `/*` |
|        - |  9020 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9021 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9022 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9023 | ` * by te SQLite3 library.` |
|        - |  9024 | ` */` |
|   137192 |  9025 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9026 |  |
|        - |  9027 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9028 | `	int i;` |
|        - |  9029 | `	/* Generate a binary string first */` |
|   137194 |  9030 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9031 | `	/* Turn the binary string into english based alphabet */` |
|  1509282 |  9032 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1372090 |  9033 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   686046 |  9034 | `	 }` |
|   137194 |  9035 |  |
|        - |  9036 | `/*` |
|        - |  9037 | ` * int rand()` |
|        - |  9038 | ` * int mt_rand()` |
|        - |  9039 | ` * int rand(int $min,int $max)` |
|        - |  9040 | ` * int mt_rand(int $min,int $max)` |
|        - |  9041 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9042 | ` * Parameter` |
|        - |  9043 | ` *  $min` |
|        - |  9044 | ` *    The lowest value to return (default: 0)` |
|        - |  9045 | ` *  $max` |
|        - |  9046 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9047 | ` * Return` |
|        - |  9048 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9049 | ` * Note:` |
|        - |  9050 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9051 | ` *  by te SQLite3 library.` |
|        - |  9052 | ` */` |
|       20 |  9053 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9054 |  |
|        - |  9055 | `	sxu32 iNum;` |
|        - |  9056 | `	/* Generate the random number */` |
|       21 |  9057 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9058 | `	if( nArg > 1 ){` |
|        - |  9059 | `		sxu32 iMin,iMax;` |
|        3 |  9060 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9061 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9062 | `		if( iMin < iMax ){` |
|        3 |  9063 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9064 | `			if( iDiv > 0 ){` |
|        3 |  9065 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9066 | `			}` |
|        1 |  9067 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9068 | `			iNum %= iMax;` |
|      ! 0 |  9069 | `		}` |
|        1 |  9070 | `	}` |
|        - |  9071 | `	/* Return the number */` |
|       21 |  9072 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9073 | `	return SXRET_OK;` |
|        1 |  9074 |  |
|        - |  9075 | `/*` |
|        - |  9076 | ` * int getrandmax(void)` |
|        - |  9077 | ` * int mt_getrandmax(void)` |
|        - |  9078 | ` * int rc4_getrandmax(void)` |
|        - |  9079 | ` *   Show largest possible random value` |
|        - |  9080 | ` * Return` |
|        - |  9081 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9082 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9083 | ` * Note:` |
|        - |  9084 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9085 | ` *  by te SQLite3 library.` |
|        - |  9086 | ` */` |
|        4 |  9087 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9088 |  |
|        2 |  9089 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9090 | `	SXUNUSED(apArg);` |
|        5 |  9091 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9092 | `	return SXRET_OK;` |
|        1 |  9093 |  |
|        - |  9094 | `/*` |
|        - |  9095 | ` * string rand_str()` |
|        - |  9096 | ` * string rand_str(int $len)` |
|        - |  9097 | ` *  Generate a random string (English alphabet).` |
|        - |  9098 | ` * Parameter` |
|        - |  9099 | ` *  $len` |
|        - |  9100 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9101 | ` * Return` |
|        - |  9102 | ` *   A pseudo random string.` |
|        - |  9103 | ` * Note:` |
|        - |  9104 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9105 | ` *  by te SQLite3 library.` |
|        - |  9106 | ` *  This function is a symisc extension.` |
|        - |  9107 | ` */` |
|      120 |  9108 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9109 |  |
|        - |  9110 | `	char zString[1024];` |
|      122 |  9111 | `	int iLen = 0x10;` |
|      122 |  9112 | `	if( nArg > 0 ){` |
|        - |  9113 | `		/* Get the desired length */` |
|      122 |  9114 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9115 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9116 | `			/* Default length */` |
|        3 |  9117 | `			iLen = 0x10;` |
|        1 |  9118 | `		}` |
|       60 |  9119 | `	}` |
|        - |  9120 | `	/* Generate the random string */` |
|      122 |  9121 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9122 | `	/* Return the generated string */` |
|      122 |  9123 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9124 | `	return SXRET_OK;` |
|        2 |  9125 |  |
|        - |  9126 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9127 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9128 | `/* Unique ID private data */` |
|        - |  9129 | `struct unique_id_data` |
|        - |  9130 |  |
|        - |  9131 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9132 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9133 | `};` |
|        - |  9134 | `/*` |
|        - |  9135 | ` * Binary to hex consumer callback.` |
|        - |  9136 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9137 | ` * defined below.` |
|        - |  9138 | ` */` |
|      192 |  9139 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9140 |  |
|      193 |  9141 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9142 | `	sxu32 nBuflen;` |
|        - |  9143 | `	/* Extract result buffer length */` |
|      193 |  9144 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9145 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9146 | `			/*` |
|        - |  9147 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9148 | `			 * string will be 13 characters long` |
|        - |  9149 | `			 */` |
|       25 |  9150 | `		return SXERR_ABORT;` |
|        - |  9151 | `	}` |
|      169 |  9152 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9153 | `		return SXERR_ABORT;` |
|        - |  9154 | `	}` |
|        - |  9155 | `	/* Safely Consume the hex stream */` |
|      169 |  9156 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9157 | `	return SXRET_OK;` |
|       97 |  9158 |  |
|        - |  9159 | `/*` |
|        - |  9160 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9161 | ` *  Generate a unique ID` |
|        - |  9162 | ` * Parameter` |
|        - |  9163 | ` * $prefix` |
|        - |  9164 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9165 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9166 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9167 | ` * $more_entropy` |
|        - |  9168 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9169 | ` *  that the result will be unique.` |
|        - |  9170 | ` * Return` |
|        - |  9171 | ` *  Returns the unique identifier, as a string.` |
|        - |  9172 | ` */` |
|       24 |  9173 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9174 |  |
|        - |  9175 | `	struct unique_id_data sUniq;` |
|        - |  9176 | `	unsigned char zDigest[20];` |
|       25 |  9177 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9178 | `	const char *zPrefix;` |
|        - |  9179 | `	SHA1Context sCtx;` |
|        - |  9180 | `	char zRandom[7];` |
|        - |  9181 | `	int nPrefix;` |
|        - |  9182 | `	int entropy;` |
|        - |  9183 | `	/* Generate a random string first */` |
|       25 |  9184 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9185 | `	/* Initialize fields */` |
|       25 |  9186 | `	zPrefix = 0;` |
|       25 |  9187 | `	nPrefix = 0;` |
|       25 |  9188 | `	entropy = 0;` |
|       25 |  9189 | `	if( nArg > 0 ){` |
|        - |  9190 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9191 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9192 | `		if( nArg > 1 ){` |
|      ! 0 |  9193 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9194 | `		}` |
|      ! 0 |  9195 | `	}` |
|       25 |  9196 | `	SHA1Init(&sCtx);` |
|        - |  9197 | `	/* Generate the random ID */` |
|       25 |  9198 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9199 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9200 | `	}` |
|        - |  9201 | `	/* Append the random ID */` |
|       25 |  9202 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9203 | `	/* Append the random string */` |
|       25 |  9204 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9205 | `	/* Increment the number */` |
|       25 |  9206 | `	pVm->unique_id++;` |
|       25 |  9207 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9208 | `	/* Hexify the digest */` |
|       25 |  9209 | `	sUniq.pCtx = pCtx;` |
|       25 |  9210 | `	sUniq.entropy = entropy;` |
|       25 |  9211 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9212 | `	/* All done */` |
|       25 |  9213 | `	return PH7_OK;` |
|        1 |  9214 |  |
|        - |  9215 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9216 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9217 | `/*` |
|        - |  9218 | ` * Section:` |
|        - |  9219 | ` *  Language construct implementation as foreign functions.` |
|        - |  9220 | ` * Status:` |
|        - |  9221 | ` *    Stable.` |
|        - |  9222 | ` */` |
|        - |  9223 | `/*` |
|        - |  9224 | ` * void echo($string...)` |
|        - |  9225 | ` *  Output one or more messages.` |
|        - |  9226 | ` * Parameters` |
|        - |  9227 | ` *  $string` |
|        - |  9228 | ` *   Message to output.` |
|        - |  9229 | ` * Return` |
|        - |  9230 | ` *  NULL.` |
|        - |  9231 | ` */` |
|      ! 0 |  9232 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9233 |  |
|        - |  9234 | `	const char *zData;` |
|      ! 0 |  9235 | `	int nDataLen = 0;` |
|        - |  9236 | `	ph7_vm *pVm;` |
|        - |  9237 | `	int i,rc;` |
|        - |  9238 | `	/* Point to the target VM */` |
|      ! 0 |  9239 | `	pVm = pCtx->pVm;` |
|        - |  9240 | `	/* Output */` |
|      ! 0 |  9241 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9242 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9243 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9244 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9245 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9246 | `			if( rc == SXERR_ABORT ){` |
|        - |  9247 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9248 | `				return PH7_ABORT;` |
|        - |  9249 | `			}` |
|      ! 0 |  9250 | `		}` |
|      ! 0 |  9251 | `	}` |
|      ! 0 |  9252 | `	return SXRET_OK;` |
|      ! 0 |  9253 |  |
|        - |  9254 | `/*` |
|        - |  9255 | ` * int print($string...)` |
|        - |  9256 | ` *  Output one or more messages.` |
|        - |  9257 | ` * Parameters` |
|        - |  9258 | ` *  $string` |
|        - |  9259 | ` *   Message to output.` |
|        - |  9260 | ` * Return` |
|        - |  9261 | ` *  1 always.` |
|        - |  9262 | ` */` |
|        2 |  9263 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9264 |  |
|        - |  9265 | `	const char *zData;` |
|        3 |  9266 | `	int nDataLen = 0;` |
|        - |  9267 | `	ph7_vm *pVm;` |
|        - |  9268 | `	int i,rc;` |
|        - |  9269 | `	/* Point to the target VM */` |
|        3 |  9270 | `	pVm = pCtx->pVm;` |
|        - |  9271 | `	/* Output */` |
|        5 |  9272 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9273 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9274 | `		if( nDataLen > 0 ){` |
|        3 |  9275 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9276 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9277 | `			if( rc == SXERR_ABORT ){` |
|        - |  9278 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9279 | `				return PH7_ABORT;` |
|        - |  9280 | `			}` |
|        1 |  9281 | `		}` |
|        2 |  9282 | `	}` |
|        - |  9283 | `	/* Return 1 */` |
|        3 |  9284 | `	ph7_result_int(pCtx,1);` |
|        3 |  9285 | `	return SXRET_OK;` |
|        2 |  9286 |  |
|        - |  9287 | `/*` |
|        - |  9288 | ` * void exit(string $msg)` |
|        - |  9289 | ` * void exit(int $status)` |
|        - |  9290 | ` * void die(string $ms)` |
|        - |  9291 | ` * void die(int $status)` |
|        - |  9292 | ` *   Output a message and terminate program execution.` |
|        - |  9293 | ` * Parameter` |
|        - |  9294 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9295 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9296 | ` *  and not printed` |
|        - |  9297 | ` * Return` |
|        - |  9298 | ` *  NULL` |
|        - |  9299 | ` */` |
|      ! 0 |  9300 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9301 |  |
|      ! 0 |  9302 | `	if( nArg > 0 ){` |
|      ! 0 |  9303 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9304 | `			const char *zData;` |
|      ! 0 |  9305 | `			int iLen = 0;` |
|        - |  9306 | `			/* Print exit message */` |
|      ! 0 |  9307 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9308 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9309 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9310 | `			sxi32 iExitStatus;` |
|        - |  9311 | `			/* Record exit status code */` |
|      ! 0 |  9312 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9313 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9314 | `		}` |
|      ! 0 |  9315 | `	}` |
|        - |  9316 | `	/* Check if we are in an included file */` |
|      ! 0 |  9317 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9318 | `		/* Exit the entire process */` |
|      ! 0 |  9319 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9320 | `	}` |
|        - |  9321 | `	/* Abort processing immediately */` |
|      ! 0 |  9322 | `	return PH7_ABORT;` |
|      ! 0 |  9323 |  |
|        - |  9324 | `/*` |
|        - |  9325 | ` * bool isset($var,...)` |
|        - |  9326 | ` *  Finds out whether a variable is set.` |
|        - |  9327 | ` * Parameters` |
|        - |  9328 | ` *  One or more variable to check.` |
|        - |  9329 | ` * Return` |
|        - |  9330 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9331 | ` */` |
|    74094 |  9332 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9333 |  |
|        - |  9334 | `	ph7_value *pObj;` |
|    74096 |  9335 | `	int res = 0;` |
|        - |  9336 | `	int i;` |
|    74096 |  9337 | `	if( nArg < 1 ){` |
|        - |  9338 | `		/* Missing arguments,return false */` |
|      ! 0 |  9339 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9340 | `		return SXRET_OK;` |
|        - |  9341 | `	}` |
|        - |  9342 | `	/* Iterate over available arguments */` |
|    97726 |  9343 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    74096 |  9344 | `		pObj = apArg[i];` |
|    74096 |  9345 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    49948 |  9346 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9347 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9348 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9349 | `			}` |
|    24973 |  9350 | `		}` |
|    74096 |  9351 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    74096 |  9352 | `		if( !res ){` |
|        - |  9353 | `			/* Variable not set,return FALSE */` |
|    50466 |  9354 | `			ph7_result_bool(pCtx,0);` |
|    50466 |  9355 | `			return SXRET_OK;` |
|        - |  9356 | `		}` |
|    11817 |  9357 | `	}` |
|        - |  9358 | `	/* All given variable are set,return TRUE */` |
|    23632 |  9359 | `	ph7_result_bool(pCtx,1);` |
|    23632 |  9360 | `	return SXRET_OK;` |
|    37049 |  9361 |  |
|        - |  9362 | `/*` |
|        - |  9363 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9364 | ` * frame,the reference table and discard it's contents.` |
|        - |  9365 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9366 | ` */` |
|  2987836 |  9367 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9368 |  |
|        - |  9369 | `	ph7_value *pObj;` |
|        - |  9370 | `	VmRefObj *pRef;` |
|  2987838 |  9371 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2987838 |  9372 | `	if( pObj ){` |
|        - |  9373 | `		/* Release the object */` |
|  2987838 |  9374 | `		PH7_MemObjRelease(pObj);` |
|  1493918 |  9375 | `	}` |
|        - |  9376 | `	/* Remove old reference links */` |
|  2987838 |  9377 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2987838 |  9378 | `	if( pRef ){` |
|  2987832 |  9379 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9380 | `		/* Unlink from the reference table */` |
|  2987832 |  9381 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2987832 |  9382 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9383 | `			VmSlot sFree;` |
|        - |  9384 | `			/* Restore to the free list */` |
|  2987826 |  9385 | `			sFree.nIdx = nObjIdx;` |
|  2987826 |  9386 | `			sFree.pUserData = 0;` |
|  2987826 |  9387 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1493912 |  9388 | `		}` |
|  1493915 |  9389 | `	}` |
|  2987838 |  9390 | `	return SXRET_OK;` |
|        2 |  9391 |  |
|        - |  9392 | `/*` |
|        - |  9393 | ` * void unset($var,...)` |
|        - |  9394 | ` *   Unset one or more given variable.` |
|        - |  9395 | ` * Parameters` |
|        - |  9396 | ` *  One or more variable to unset.` |
|        - |  9397 | ` * Return` |
|        - |  9398 | ` *  Nothing.` |
|        - |  9399 | ` */` |
|     6678 |  9400 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9401 |  |
|        - |  9402 | `	ph7_value *pObj;` |
|        - |  9403 | `	ph7_vm *pVm;` |
|        - |  9404 | `	int i;` |
|        - |  9405 | `	/* Point to the target VM */` |
|     6680 |  9406 | `	pVm = pCtx->pVm;` |
|        - |  9407 | `	/* Iterate and unset */` |
|    13358 |  9408 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6680 |  9409 | `		pObj = apArg[i];` |
|     6680 |  9410 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9411 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9412 | `				/* Throw an error */` |
|      ! 0 |  9413 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9414 | `			}` |
|      ! 0 |  9415 | `		}else{` |
|     6680 |  9416 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9417 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6680 |  9418 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6674 |  9419 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3336 |  9420 | `			}` |
|        - |  9421 | `		}` |
|     3341 |  9422 | `	}` |
|     6680 |  9423 | `	return SXRET_OK;` |
|        2 |  9424 |  |
|        - |  9425 | `/*` |
|        - |  9426 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9427 | ` */` |
|      110 |  9428 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9429 |  |
|      111 |  9430 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9431 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9432 | `	ph7_value *pObj;` |
|        - |  9433 | `	sxu32 nIdx;` |
|        - |  9434 | `	/* Extract the memory object */` |
|      111 |  9435 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9436 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9437 | `	if( pObj ){` |
|      111 |  9438 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9439 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9440 | `				SyString sName;` |
|        - |  9441 | `				ph7_value sKey;` |
|        - |  9442 | `				/* Perform the insertion */` |
|      109 |  9443 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9444 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9445 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9446 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9447 | `			}` |
|       54 |  9448 | `		}` |
|       55 |  9449 | `	}` |
|      111 |  9450 | `	return SXRET_OK;` |
|        1 |  9451 |  |
|        - |  9452 | `/*` |
|        - |  9453 | ` * array get_defined_vars(void)` |
|        - |  9454 | ` *  Returns an array of all defined variables.` |
|        - |  9455 | ` * Parameter` |
|        - |  9456 | ` *  None` |
|        - |  9457 | ` * Return` |
|        - |  9458 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9459 | ` */` |
|        2 |  9460 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9461 |  |
|        3 |  9462 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9463 | `	ph7_value *pArray;` |
|        - |  9464 | `	/* Create a new array */` |
|        3 |  9465 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9466 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9467 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9468 | `		SXUNUSED(apArg);` |
|        - |  9469 | `		/* Return NULL */` |
|      ! 0 |  9470 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9471 | `		return SXRET_OK;` |
|        - |  9472 | `	}` |
|        - |  9473 | `	/* Superglobals first */` |
|        3 |  9474 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9475 | `	/* Then variable defined in the current frame */` |
|        3 |  9476 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9477 | `	/* Finally,return the created array */` |
|        3 |  9478 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9479 | `	return SXRET_OK;` |
|        2 |  9480 |  |
|        - |  9481 | `/*` |
|        - |  9482 | ` * bool gettype($var)` |
|        - |  9483 | ` *  Get the type of a variable` |
|        - |  9484 | ` * Parameters` |
|        - |  9485 | ` *   $var` |
|        - |  9486 | ` *    The variable being type checked.` |
|        - |  9487 | ` * Return` |
|        - |  9488 | ` *   String representation of the given variable type.` |
|        - |  9489 | ` */` |
|       32 |  9490 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9491 |  |
|       34 |  9492 | `	const char *zType = "Empty";` |
|       34 |  9493 | `	if( nArg > 0 ){` |
|       34 |  9494 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9495 | `	}` |
|        - |  9496 | `	/* Return the variable type */` |
|       34 |  9497 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9498 | `	return SXRET_OK;` |
|        2 |  9499 |  |
|        - |  9500 | `/*` |
|        - |  9501 | ` * string get_resource_type(resource $handle)` |
|        - |  9502 | ` *  This function gets the type of the given resource.` |
|        - |  9503 | ` * Parameters` |
|        - |  9504 | ` *  $handle` |
|        - |  9505 | ` *  The evaluated resource handle.` |
|        - |  9506 | ` * Return` |
|        - |  9507 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9508 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9509 | ` *  the return value will be the string Unknown.` |
|        - |  9510 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9511 | ` *  is not a resource.` |
|        - |  9512 | ` */` |
|        2 |  9513 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9514 |  |
|        3 |  9515 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9516 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9517 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9518 | `		return PH7_OK;` |
|        - |  9519 | `	}` |
|        3 |  9520 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9521 | `	return SXRET_OK;` |
|        2 |  9522 |  |
|        - |  9523 | `/*` |
|        - |  9524 | ` * void var_dump(expression,....)` |
|        - |  9525 | ` *   var_dump � Dumps information about a variable` |
|        - |  9526 | ` * Parameters` |
|        - |  9527 | ` *   One or more expression to dump.` |
|        - |  9528 | ` * Returns` |
|        - |  9529 | ` *  Nothing.` |
|        - |  9530 | ` */` |
|      218 |  9531 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9532 |  |
|        - |  9533 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9534 | `	int i;` |
|      220 |  9535 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9536 | `	/* Dump one or more expressions */` |
|      444 |  9537 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9538 | `		ph7_value *pObj = apArg[i];` |
|        - |  9539 | `		/* Reset the working buffer */` |
|      226 |  9540 | `		SyBlobReset(&sDump);` |
|        - |  9541 | `		/* Dump the given expression */` |
|      226 |  9542 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9543 | `		/* Output */` |
|      226 |  9544 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9545 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9546 | `		}` |
|      114 |  9547 | `	}` |
|        - |  9548 | `	/* Release the working buffer */` |
|      220 |  9549 | `	SyBlobRelease(&sDump);` |
|      220 |  9550 | `	return SXRET_OK;` |
|        2 |  9551 |  |
|        - |  9552 | `/*` |
|        - |  9553 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9554 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9555 | ` * Parameters` |
|        - |  9556 | ` *   expression: Expression to dump` |
|        - |  9557 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9558 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9559 | ` *            print_r() will return the information rather than print it.` |
|        - |  9560 | ` * Return` |
|        - |  9561 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9562 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9563 | ` */` |
|       16 |  9564 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9565 |  |
|       17 |  9566 | `	int ret_string = 0;` |
|        - |  9567 | `	SyBlob sDump;` |
|       17 |  9568 | `	if( nArg < 1 ){` |
|        - |  9569 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9570 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9571 | `		return SXRET_OK;` |
|        - |  9572 | `	}` |
|       17 |  9573 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9574 | `	if ( nArg > 1 ){` |
|        - |  9575 | `		/* Where to redirect output */` |
|       11 |  9576 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9577 | `	}` |
|        - |  9578 | `	/* Generate dump */` |
|       17 |  9579 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9580 | `	if( !ret_string ){` |
|        - |  9581 | `		/* Output dump */` |
|        7 |  9582 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9583 | `		/* Return true */` |
|        7 |  9584 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9585 | `	}else{` |
|        - |  9586 | `		/* Generated dump as return value */` |
|       11 |  9587 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9588 | `	}` |
|        - |  9589 | `	/* Release the working buffer */` |
|       17 |  9590 | `	SyBlobRelease(&sDump);` |
|       17 |  9591 | `	return SXRET_OK;` |
|        9 |  9592 |  |
|        - |  9593 | `/*` |
|        - |  9594 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9595 | ` * Same job as print_r. (see coment above)` |
|        - |  9596 | ` */` |
|        2 |  9597 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9598 |  |
|        3 |  9599 | `	int ret_string = 0;` |
|        - |  9600 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9601 | `	if( nArg < 1 ){` |
|        - |  9602 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9603 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9604 | `		return SXRET_OK;` |
|        - |  9605 | `	}` |
|        3 |  9606 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9607 | `	if ( nArg > 1 ){` |
|        - |  9608 | `		/* Where to redirect output */` |
|        3 |  9609 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9610 | `	}` |
|        - |  9611 | `	/* Generate dump */` |
|        3 |  9612 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9613 | `	if( !ret_string ){` |
|        - |  9614 | `		/* Output dump */` |
|      ! 0 |  9615 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9616 | `		/* Return NULL */` |
|      ! 0 |  9617 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9618 | `	}else{` |
|        - |  9619 | `		/* Generated dump as return value */` |
|        3 |  9620 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9621 | `	}` |
|        - |  9622 | `	/* Release the working buffer */` |
|        3 |  9623 | `	SyBlobRelease(&sDump);` |
|        3 |  9624 | `	return SXRET_OK;` |
|        2 |  9625 |  |
|        - |  9626 | `/*` |
|        - |  9627 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9628 | ` *  Set/get the various assert flags.` |
|        - |  9629 | ` * Parameter` |
|        - |  9630 | ` * $what` |
|        - |  9631 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9632 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9633 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9634 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9635 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9636 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9637 | ` * $value` |
|        - |  9638 | ` *   An optional new value for the option.` |
|        - |  9639 | ` * Return` |
|        - |  9640 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9641 | ` */` |
|       30 |  9642 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9643 |  |
|       32 |  9644 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9645 | `	int iOption;` |
|        - |  9646 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  9647 | `	if( nArg < 1 ){` |
|        3 |  9648 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9649 | `			"ArgumentCountError",` |
|        - |  9650 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9651 | `			);` |
|        - |  9652 | `	}` |
|        - |  9653 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  9654 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  9655 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9656 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9657 | `			"TypeError",` |
|        - |  9658 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9659 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9660 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9661 | `			);` |
|        - |  9662 | `	}` |
|       30 |  9663 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9664 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9665 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9666 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  9667 | `	switch( iOption ){` |
|        6 |  9668 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9669 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  9670 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  9671 | `		if( nArg > 1 ){` |
|        5 |  9672 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9673 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9674 | `			}else{` |
|        3 |  9675 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9676 | `			}` |
|        2 |  9677 | `		}` |
|       14 |  9678 | `		break;` |
|        1 |  9679 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9680 | `		/* Return old callback or null */` |
|        3 |  9681 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9682 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9683 | `		}else{` |
|        3 |  9684 | `			ph7_result_null(pCtx);` |
|        - |  9685 | `		}` |
|        3 |  9686 | `		if( nArg > 1 ){` |
|      ! 0 |  9687 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9688 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9689 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9690 | `			}else{` |
|      ! 0 |  9691 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9692 | `			}` |
|      ! 0 |  9693 | `		}` |
|        3 |  9694 | `		break;` |
|        5 |  9695 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9696 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9697 | `		if( nArg > 1 ){` |
|        5 |  9698 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9699 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9700 | `			}else{` |
|        3 |  9701 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9702 | `			}` |
|        2 |  9703 | `		}` |
|       11 |  9704 | `		break;` |
|      ! 0 |  9705 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9706 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9707 | `		break;` |
|        1 |  9708 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9709 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9710 | `		break;` |
|      ! 0 |  9711 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9712 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9713 | `		break;` |
|        1 |  9714 | `	default:` |
|        - |  9715 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9716 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9717 | `			"ValueError",` |
|        - |  9718 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9719 | `			);` |
|        - |  9720 | `	}` |
|       28 |  9721 | `	return PH7_OK;` |
|       17 |  9722 |  |
|        - |  9723 | `/*` |
|        - |  9724 | ` * bool assert(mixed $assertion)` |
|        - |  9725 | ` *  Checks if assertion is FALSE.` |
|        - |  9726 | ` * Parameter` |
|        - |  9727 | ` *  $assertion` |
|        - |  9728 | ` *    The assertion to test.` |
|        - |  9729 | ` * Return` |
|        - |  9730 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9731 | ` */` |
|       26 |  9732 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9733 |  |
|       28 |  9734 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9735 | `	int iFlags,iResult;` |
|        - |  9736 | `	const char *zDesc;` |
|        - |  9737 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  9738 | `	if( nArg < 1 ){` |
|        3 |  9739 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9740 | `			"ArgumentCountError",` |
|        - |  9741 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9742 | `			);` |
|        - |  9743 | `	}` |
|       26 |  9744 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  9745 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9746 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9747 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9748 | `		return PH7_OK;` |
|        - |  9749 | `	}` |
|        - |  9750 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  9751 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  9752 | `	if( !iResult ){` |
|        - |  9753 | `		/* Assertion failed */` |
|        - |  9754 | `		/* Extract optional description */` |
|       13 |  9755 | `		zDesc = 0;` |
|       13 |  9756 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9757 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9758 | `		}` |
|       13 |  9759 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9760 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9761 | `			ph7_value sFile,sLine;` |
|        - |  9762 | `			ph7_value *apCbArg[3];` |
|        - |  9763 | `			SyString *pFile;` |
|        - |  9764 | `			/* Extract the processed script */` |
|      ! 0 |  9765 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9766 | `			if( pFile == 0 ){` |
|      ! 0 |  9767 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9768 | `			}` |
|        - |  9769 | `			/* Invoke the callback */` |
|      ! 0 |  9770 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9771 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9772 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9773 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9774 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9775 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9776 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9777 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9778 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9779 | `		}` |
|       13 |  9780 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9781 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9782 | `			return PH7_ABORT;` |
|        - |  9783 | `		}` |
|        - |  9784 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9785 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9786 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9787 | `				"AssertionError",` |
|        - |  9788 | `				"%s",` |
|        1 |  9789 | `				zDesc` |
|        - |  9790 | `				);` |
|      ! 0 |  9791 | `		}else{` |
|       11 |  9792 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9793 | `				"AssertionError",` |
|        - |  9794 | `				"assert(false)"` |
|        - |  9795 | `				);` |
|        - |  9796 | `		}` |
|        - |  9797 | `	}` |
|        - |  9798 | `	/* Assertion passed */` |
|       14 |  9799 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9800 | `	return PH7_OK;` |
|       15 |  9801 |  |
|        - |  9802 | `/*` |
|        - |  9803 | ` * Section:` |
|        - |  9804 | ` *  Error reporting functions.` |
|        - |  9805 | ` * Status:` |
|        - |  9806 | ` *    Stable.` |
|        - |  9807 | ` */` |
|        - |  9808 | `/*` |
|        - |  9809 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9810 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9811 | ` * Parameters` |
|        - |  9812 | ` *  $error_msg` |
|        - |  9813 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9814 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9815 | ` * $error_type` |
|        - |  9816 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9817 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9818 | ` * Return` |
|        - |  9819 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9820 | ` */` |
|       12 |  9821 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9822 |  |
|       14 |  9823 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9824 | `	int rc = PH7_OK;` |
|       14 |  9825 | `	if( nArg > 0 ){` |
|        - |  9826 | `		const char *zErr;` |
|        - |  9827 | `		int nLen;` |
|        - |  9828 | `		/* Extract the error message */` |
|       12 |  9829 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9830 | `		if( nArg > 1 ){` |
|        - |  9831 | `			/* Extract the error type */` |
|       12 |  9832 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9833 | `			switch( nErr ){` |
|        1 |  9834 | `			case 1:   /* E_ERROR */` |
|        - |  9835 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9836 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9837 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9838 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9839 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9840 | `				break;` |
|        1 |  9841 | `			case 2:   /* E_WARNING */` |
|        - |  9842 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9843 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9844 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9845 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9846 | `				break;` |
|        3 |  9847 | `			default:` |
|        8 |  9848 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9849 | `				break;` |
|        - |  9850 | `			}` |
|        5 |  9851 | `		}` |
|        - |  9852 | `		/* Report error */` |
|       12 |  9853 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9854 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9855 | `			return rc;` |
|        - |  9856 | `		}` |
|        - |  9857 | `		/* Return true */` |
|       12 |  9858 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9859 | `	}else{` |
|        - |  9860 | `		/* Missing arguments,return FALSE */` |
|        3 |  9861 | `		ph7_result_bool(pCtx,0);` |
|        - |  9862 | `	}` |
|       14 |  9863 | `	return rc;` |
|        8 |  9864 |  |
|        - |  9865 | `/*` |
|        - |  9866 | ` * int error_reporting([int $level])` |
|        - |  9867 | ` *  Sets which PHP errors are reported.` |
|        - |  9868 | ` * Parameters` |
|        - |  9869 | ` *  $level` |
|        - |  9870 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  9871 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  9872 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  9873 | ` *   levels will not always behave as expected.` |
|        - |  9874 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  9875 | ` *   in the predefined constants.` |
|        - |  9876 | ` * Return` |
|        - |  9877 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  9878 | ` *   parameter is given.` |
|        - |  9879 | ` */` |
|       42 |  9880 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9881 |  |
|       44 |  9882 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9883 | `	int nOld;` |
|        - |  9884 | `	/* Extract the old reporting level */` |
|       44 |  9885 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       44 |  9886 | `	if( nArg > 0 ){` |
|        - |  9887 | `		int nNew;` |
|        - |  9888 | `		/* Extract the desired error reporting level */` |
|       36 |  9889 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       36 |  9890 | `		if( !nNew ){` |
|        - |  9891 | `			/* Do not report errors at all */` |
|        5 |  9892 | `			pVm->bErrReport = 0;` |
|        3 |  9893 | `		}else{` |
|        - |  9894 | `			/* Report all errors */` |
|       32 |  9895 | `			pVm->bErrReport = 1;` |
|        - |  9896 | `		}` |
|       17 |  9897 | `	}` |
|        - |  9898 | `	/* Return the old level */` |
|       44 |  9899 | `	ph7_result_int(pCtx,nOld);` |
|       44 |  9900 | `	return PH7_OK;` |
|        2 |  9901 |  |
|        - |  9902 | `/*` |
|        - |  9903 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  9904 | ` *  Send an error message somewhere.` |
|        - |  9905 | ` * Parameter` |
|        - |  9906 | ` *  $message` |
|        - |  9907 | ` *   The error message that should be logged.` |
|        - |  9908 | ` *  $message_type` |
|        - |  9909 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  9910 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  9911 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  9912 | ` *       This is the default option.` |
|        - |  9913 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  9914 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  9915 | ` *    2  No longer an option.` |
|        - |  9916 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  9917 | ` *       to the end of the message string.` |
|        - |  9918 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  9919 | ` *  $destination` |
|        - |  9920 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  9921 | ` *  $extra_headers` |
|        - |  9922 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  9923 | ` * Return` |
|        - |  9924 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9925 | ` * NOTE:` |
|        - |  9926 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  9927 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  9928 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  9929 | ` *  Otherwise this function is no-op.` |
|        - |  9930 | ` */` |
|        4 |  9931 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9932 |  |
|        - |  9933 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  9934 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  9935 | `	int iType = 0;` |
|        5 |  9936 | `	if( nArg < 1 ){` |
|        - |  9937 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  9938 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9939 | `		return PH7_OK;` |
|        - |  9940 | `	}` |
|        5 |  9941 | `	if( pVm->xErrLog  ){` |
|        - |  9942 | `		/* Invoke the user callback */` |
|      ! 0 |  9943 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  9944 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  9945 | `		if( nArg > 1 ){` |
|      ! 0 |  9946 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  9947 | `			if( nArg > 2 ){` |
|      ! 0 |  9948 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  9949 | `				if( nArg > 3 ){` |
|      ! 0 |  9950 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  9951 | `				}` |
|      ! 0 |  9952 | `			}` |
|      ! 0 |  9953 | `		}` |
|      ! 0 |  9954 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  9955 | `	}` |
|        - |  9956 | `	/* Retun TRUE */` |
|        5 |  9957 | `	ph7_result_bool(pCtx,1);` |
|        5 |  9958 | `	return PH7_OK;` |
|        3 |  9959 |  |
|        - |  9960 | `/*` |
|        - |  9961 | ` * bool restore_exception_handler(void)` |
|        - |  9962 | ` *  Restores the previously defined exception handler function.` |
|        - |  9963 | ` * Parameter` |
|        - |  9964 | ` *  None` |
|        - |  9965 | ` * Return` |
|        - |  9966 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  9967 | ` */` |
|        4 |  9968 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9969 |  |
|        5 |  9970 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9971 | `	ph7_value *pOld,*pNew;` |
|        - |  9972 | `	/* Point to the old and the new handler */` |
|        5 |  9973 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  9974 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  9975 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  9976 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  9977 | `		SXUNUSED(apArg);` |
|        - |  9978 | `		/* No installed handler,return FALSE */` |
|        5 |  9979 | `		ph7_result_bool(pCtx,0);` |
|        5 |  9980 | `		return PH7_OK;` |
|        - |  9981 | `	}` |
|        - |  9982 | `	/* Copy the old handler */` |
|      ! 0 |  9983 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  9984 | `	PH7_MemObjRelease(pOld);` |
|        - |  9985 | `	/* Return TRUE */` |
|      ! 0 |  9986 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  9987 | `	return PH7_OK;` |
|        3 |  9988 |  |
|        - |  9989 | `/*` |
|        - |  9990 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  9991 | ` *  Sets a user-defined exception handler function.` |
|        - |  9992 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  9993 | ` * NOTE` |
|        - |  9994 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  9995 | ` *  the satndard PHP engine.` |
|        - |  9996 | ` * Parameters` |
|        - |  9997 | ` *  $exception_handler` |
|        - |  9998 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  9999 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10000 | ` *   that was thrown.` |
|        - | 10001 | ` *  Note:` |
|        - | 10002 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10003 | ` * Return` |
|        - | 10004 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10005 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10006 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10007 | ` */` |
|        4 | 10008 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10009 |  |
|        6 | 10010 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10011 | `	ph7_value *pOld,*pNew;` |
|        - | 10012 | `	/* Point to the old and the new handler */` |
|        6 | 10013 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10014 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10015 | `	/* Return the old handler */` |
|        6 | 10016 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10017 | `	if( nArg > 0 ){` |
|        6 | 10018 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10019 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10020 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10021 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10022 | `		}else{` |
|        6 | 10023 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10024 | `			/* Install the new handler */` |
|        6 | 10025 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10026 | `		}` |
|        2 | 10027 | `	}` |
|        6 | 10028 | `	return PH7_OK;` |
|        2 | 10029 |  |
|        - | 10030 | `/*` |
|        - | 10031 | ` * bool restore_error_handler(void)` |
|        - | 10032 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10033 | ` * Parameters:` |
|        - | 10034 | ` *  None.` |
|        - | 10035 | ` * Return` |
|        - | 10036 | ` *  Always TRUE.` |
|        - | 10037 | ` */` |
|        4 | 10038 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10039 |  |
|        5 | 10040 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10041 | `	ph7_value *pOld,*pNew;` |
|        - | 10042 | `	/* Point to the old and the new handler */` |
|        5 | 10043 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10044 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10045 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10046 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10047 | `		SXUNUSED(apArg);` |
|        - | 10048 | `		/* No installed callback,return FALSE */` |
|        5 | 10049 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10050 | `		return PH7_OK;` |
|        - | 10051 | `	}` |
|        - | 10052 | `	/* Copy the old callback */` |
|      ! 0 | 10053 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10054 | `	PH7_MemObjRelease(pOld);` |
|        - | 10055 | `	/* Return TRUE */` |
|      ! 0 | 10056 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10057 | `	return PH7_OK;` |
|        3 | 10058 |  |
|        - | 10059 | `/*` |
|        - | 10060 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10061 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10062 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10063 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10064 | ` *  Sets a user-defined error handler function.` |
|        - | 10065 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10066 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10067 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10068 | ` *  conditions (using trigger_error()).` |
|        - | 10069 | ` * Parameters` |
|        - | 10070 | ` *  $error_handler` |
|        - | 10071 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10072 | ` *   describing the error.` |
|        - | 10073 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10074 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10075 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10076 | ` *   The function can be shown as:` |
|        - | 10077 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10078 | ` *     errno` |
|        - | 10079 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10080 | ` *   errstr` |
|        - | 10081 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10082 | ` *   errfile` |
|        - | 10083 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10084 | ` *     was raised in, as a string.` |
|        - | 10085 | ` *  Note:` |
|        - | 10086 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10087 | ` * Return` |
|        - | 10088 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10089 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10090 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10091 | ` */` |
|     8822 | 10092 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10093 |  |
|     8824 | 10094 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10095 | `	ph7_value *pOld,*pNew;` |
|        - | 10096 | `	/* Point to the old and the new handler */` |
|     8824 | 10097 | `	pOld = &pVm->aErrCB[0];` |
|     8824 | 10098 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10099 | `	/* Return the old handler */` |
|     8824 | 10100 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8824 | 10101 | `	if( nArg > 0 ){` |
|     8824 | 10102 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10103 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4411 | 10104 | `			PH7_MemObjRelease(pNew);` |
|     4411 | 10105 | `			ph7_result_bool(pCtx,1);` |
|     2206 | 10106 | `		}else{` |
|     4414 | 10107 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10108 | `			/* Install the new handler */` |
|     4414 | 10109 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10110 | `		}` |
|     4411 | 10111 | `	}` |
|     8824 | 10112 | `	return PH7_OK;` |
|        2 | 10113 |  |
|        - | 10114 | `/*` |
|        - | 10115 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10116 | ` *  Generates a backtrace.` |
|        - | 10117 | ` * Paramaeter` |
|        - | 10118 | ` *  $options` |
|        - | 10119 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10120 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10121 | ` *   all the function/method arguments, to save memory.` |
|        - | 10122 | ` * $limit` |
|        - | 10123 | ` *   (Not Used)` |
|        - | 10124 | ` * Return` |
|        - | 10125 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10126 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10127 | ` *          Name        Type      Description` |
|        - | 10128 | ` *          ------      ------     -----------` |
|        - | 10129 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10130 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10131 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10132 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10133 | ` *          object      object    The current object.` |
|        - | 10134 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10135 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10136 | ` */` |
|      514 | 10137 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10138 |  |
|      516 | 10139 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10140 | `	ph7_value *pArray;` |
|        - | 10141 | `	ph7_class *pClass;` |
|        - | 10142 | `	ph7_value *pValue;` |
|        - | 10143 | `	SyString *pFile;` |
|        - | 10144 | `	/* Create a new array */` |
|      516 | 10145 | `	pArray = ph7_context_new_array(pCtx);` |
|      516 | 10146 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      516 | 10147 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10148 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10149 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10150 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10151 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10152 | `		SXUNUSED(apArg);` |
|      ! 0 | 10153 | `		return PH7_OK;` |
|        - | 10154 | `	}` |
|        - | 10155 | `	/* Dump running function name and it's arguments  */` |
|      516 | 10156 | `	if( pVm->pFrame->pParent ){` |
|      516 | 10157 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10158 | `		ph7_vm_func *pFunc;` |
|        - | 10159 | `		ph7_value *pArg;` |
|      516 | 10160 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      516 | 10161 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      516 | 10162 | `		if( pFrame->pParent && pFunc ){` |
|      516 | 10163 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      516 | 10164 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      516 | 10165 | `			ph7_value_reset_string_cursor(pValue);` |
|      257 | 10166 | `		}` |
|        - | 10167 | `		/* Function arguments */` |
|      516 | 10168 | `		pArg = ph7_context_new_array(pCtx);` |
|      516 | 10169 | `		if( pArg  ){` |
|        - | 10170 | `			ph7_value *pObj;` |
|        - | 10171 | `			VmSlot *aSlot;` |
|        - | 10172 | `			sxu32 n;` |
|        - | 10173 | `			/* Start filling the array with the given arguments */` |
|      516 | 10174 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2050 | 10175 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1536 | 10176 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1536 | 10177 | `				if( pObj ){` |
|     1536 | 10178 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      767 | 10179 | `				}` |
|      769 | 10180 | `			}` |
|        - | 10181 | `			/* Save the array */` |
|      516 | 10182 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      257 | 10183 | `		}` |
|      257 | 10184 | `	}` |
|      516 | 10185 | `	ph7_value_int(pValue,1);` |
|        - | 10186 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10187 | `	 * line numbers at run-time. )` |
|        - | 10188 | `	 */` |
|      516 | 10189 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10190 | `	/* Current processed script */` |
|      516 | 10191 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      516 | 10192 | `	if( pFile ){` |
|      516 | 10193 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      516 | 10194 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      516 | 10195 | `		ph7_value_reset_string_cursor(pValue);` |
|      257 | 10196 | `	}` |
|        - | 10197 | `	/* Top class */` |
|      516 | 10198 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      516 | 10199 | `	if( pClass ){` |
|      512 | 10200 | `		ph7_value_reset_string_cursor(pValue);` |
|      512 | 10201 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      512 | 10202 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      255 | 10203 | `	}` |
|        - | 10204 | `	/* Return the freshly created array */` |
|      516 | 10205 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10206 | `	/*` |
|        - | 10207 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10208 | `	 * as soon we return from this function.` |
|        - | 10209 | `	 */` |
|      516 | 10210 | `	return PH7_OK;` |
|      259 | 10211 |  |
|        - | 10212 | `/*` |
|        - | 10213 | ` * Generate a small backtrace.` |
|        - | 10214 | ` * Store the generated dump in the given BLOB` |
|        - | 10215 | ` */` |
|        4 | 10216 | `static int VmMiniBacktrace(` |
|        - | 10217 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10218 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10219 | `	)` |
|        1 | 10220 |  |
|        5 | 10221 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10222 | `	ph7_vm_func *pFunc;` |
|        - | 10223 | `	ph7_class *pClass;` |
|        - | 10224 | `	SyString *pFile;` |
|        - | 10225 | `	/* Called function */` |
|        5 | 10226 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10227 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10228 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10229 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10230 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10231 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10232 | `	}else{` |
|      ! 0 | 10233 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10234 | `	}` |
|        5 | 10235 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10236 | `	/* Current processed script */` |
|        5 | 10237 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10238 | `	if( pFile ){` |
|        5 | 10239 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10240 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10241 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10242 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10243 | `	}` |
|        - | 10244 | `	/* Top class */` |
|        5 | 10245 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10246 | `	if( pClass ){` |
|      ! 0 | 10247 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10248 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10249 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10250 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10251 | `	}` |
|        5 | 10252 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10253 | `	/* All done */` |
|        5 | 10254 | `	return SXRET_OK;` |
|        1 | 10255 |  |
|        - | 10256 | `/*` |
|        - | 10257 | ` * void debug_print_backtrace()` |
|        - | 10258 | ` *  Prints a backtrace` |
|        - | 10259 | ` * Parameters` |
|        - | 10260 | ` * None` |
|        - | 10261 | ` * Return` |
|        - | 10262 | ` * NULL` |
|        - | 10263 | ` */` |
|        2 | 10264 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10265 |  |
|        3 | 10266 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10267 | `	SyBlob sDump;` |
|        3 | 10268 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10269 | `	/* Generate the backtrace */` |
|        3 | 10270 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10271 | `	/* Output backtrace */` |
|        3 | 10272 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10273 | `	/* All done,cleanup */` |
|        3 | 10274 | `	SyBlobRelease(&sDump);` |
|        1 | 10275 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10276 | `	SXUNUSED(apArg);` |
|        3 | 10277 | `	return PH7_OK;` |
|        1 | 10278 |  |
|        - | 10279 | `/*` |
|        - | 10280 | ` * string debug_string_backtrace()` |
|        - | 10281 | ` *  Generate a backtrace` |
|        - | 10282 | ` * Parameters` |
|        - | 10283 | ` * None` |
|        - | 10284 | ` * Return` |
|        - | 10285 | ` *  A mini backtrace().` |
|        - | 10286 | ` * Note that this is a symisc extension.` |
|        - | 10287 | ` */` |
|        2 | 10288 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10289 |  |
|        3 | 10290 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10291 | `	SyBlob sDump;` |
|        3 | 10292 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10293 | `	/* Generate the backtrace */` |
|        3 | 10294 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10295 | `	/* Return the backtrace */` |
|        3 | 10296 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10297 | `	/* All done,cleanup */` |
|        3 | 10298 | `	SyBlobRelease(&sDump);` |
|        1 | 10299 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10300 | `	SXUNUSED(apArg);` |
|        3 | 10301 | `	return PH7_OK;` |
|        1 | 10302 |  |
|        - | 10303 | `/*` |
|        - | 10304 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10305 | ` * exception is triggered.` |
|        - | 10306 | ` */` |
|      472 | 10307 | `static sxi32 VmUncaughtException(` |
|        - | 10308 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10309 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10310 | `	)` |
|        1 | 10311 |  |
|        - | 10312 | `	ph7_value *apArg[2],sArg;` |
|      473 | 10313 | `	int nArg = 1;` |
|        - | 10314 | `	sxi32 rc;` |
|      473 | 10315 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10316 | `		/* Nesting limit reached */` |
|      ! 0 | 10317 | `		return SXRET_OK;` |
|        - | 10318 | `	}` |
|        - | 10319 | `	/* Call any exception handler if available */` |
|      473 | 10320 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 | 10321 | `	if( pThis ){` |
|        - | 10322 | `		/* Load the exception instance */` |
|      473 | 10323 | `		sArg.x.pOther = pThis;` |
|      473 | 10324 | `		pThis->iRef++;` |
|      473 | 10325 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 | 10326 | `	}else{` |
|      ! 0 | 10327 | `		nArg = 0;` |
|        - | 10328 | `	}` |
|      473 | 10329 | `	apArg[0] = &sArg;` |
|        - | 10330 | `	/* Call the exception handler if available */` |
|      473 | 10331 | `	pVm->nExceptDepth++;` |
|      473 | 10332 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 | 10333 | `	pVm->nExceptDepth--;` |
|      473 | 10334 | `	if( rc != SXRET_OK ){` |
|        - | 10335 | `		SyBlob sMsgBuf;` |
|      471 | 10336 | `		const char *zClass = "Exception";` |
|      471 | 10337 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10338 | `		const char *zMsg;` |
|        - | 10339 | `		sxu32 nMsg;` |
|        - | 10340 | `		const char *zFuncName;` |
|        - | 10341 | `		int nFuncLen;` |
|      471 | 10342 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 | 10343 | `		if( pThis ){` |
|        - | 10344 | `			ph7_class_method *pGetMessage;` |
|        - | 10345 | `			ph7_value sMsg;` |
|        - | 10346 | `			const char *zTmp;` |
|        - | 10347 | `			int nTmp;` |
|      471 | 10348 | `			zClass = pThis->pClass->sName.zString;` |
|      471 | 10349 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 | 10350 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 | 10351 | `			if( pGetMessage ){` |
|      471 | 10352 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 | 10353 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 | 10354 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 | 10355 | `					if( zTmp && nTmp > 0 ){` |
|      471 | 10356 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 | 10357 | `					}` |
|      235 | 10358 | `				}` |
|      471 | 10359 | `				PH7_MemObjRelease(&sMsg);` |
|      235 | 10360 | `			}` |
|      235 | 10361 | `		}` |
|      471 | 10362 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10363 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10364 | `		}` |
|      471 | 10365 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 | 10366 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 | 10367 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 | 10368 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 | 10369 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10370 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 | 10371 | `		rc = SXERR_ABORT;` |
|      235 | 10372 | `	}` |
|      473 | 10373 | `	PH7_MemObjRelease(&sArg);` |
|      473 | 10374 | `	return rc;` |
|      237 | 10375 |  |
|        - | 10376 | `/*` |
|        - | 10377 | ` * Throw a user exception.` |
|        - | 10378 | ` *` |
|        - | 10379 | ` * Exception dispatch follows this sequence:` |
|        - | 10380 | ` *` |
|        - | 10381 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10382 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10383 | ` *` |
|        - | 10384 | ` * 2. If NO catch matches:` |
|        - | 10385 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10386 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10387 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10388 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10389 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10390 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10391 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10392 | ` *` |
|        - | 10393 | ` * 3. If a catch DOES match:` |
|        - | 10394 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10395 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10396 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10397 | ` *       finally block.` |
|        - | 10398 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10399 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10400 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10401 | ` *       in pPendingException (step 2c).` |
|        - | 10402 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10403 | ` *    d. Run finally (if present).` |
|        - | 10404 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10405 | ` *       that handlers are restored and finally has run.` |
|        - | 10406 | ` */` |
|      514 | 10407 | `static sxi32 VmThrowException(` |
|        - | 10408 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10409 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10410 | `	)` |
|        2 | 10411 |  |
|        - | 10412 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10413 | `	ph7_exception **apException;` |
|        - | 10414 | `	ph7_exception *pException;` |
|        - | 10415 | `	/* Point to the stack of loaded exceptions */` |
|      516 | 10416 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      516 | 10417 | `	pException = 0;` |
|      516 | 10418 | `	pCatch = 0;` |
|      516 | 10419 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10420 | `		ph7_exception_block *aCatch;` |
|        - | 10421 | `		ph7_class *pClass;` |
|        - | 10422 | `		sxu32 j;` |
|        - | 10423 | `		/* Locate the appropriate block to execute */` |
|       40 | 10424 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10425 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10426 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10427 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10428 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10429 | `			/* Extract the target class */` |
|       38 | 10430 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10431 | `			if( pClass == 0 ){` |
|        - | 10432 | `				/* No such class */` |
|      ! 0 | 10433 | `				continue;` |
|        - | 10434 | `			}` |
|       38 | 10435 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10436 | `				/* Catch block found,break immeditaley */` |
|       38 | 10437 | `				pCatch = &aCatch[j];` |
|       38 | 10438 | `				break;` |
|        - | 10439 | `			}` |
|      ! 0 | 10440 | `		}` |
|       19 | 10441 | `	}` |
|        - | 10442 | `	/* Execute the cached block if available */` |
|      516 | 10443 | `	if( pCatch == 0 ){` |
|        - | 10444 | `		sxi32 rc;` |
|        - | 10445 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 | 10446 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10447 | `			pException->iFinallyDone = 1;` |
|        3 | 10448 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10449 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10450 | `				return SXERR_ABORT;` |
|        - | 10451 | `			}` |
|        1 | 10452 | `		}` |
|        - | 10453 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 | 10454 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10455 | `			/* Re-throw to the outer handler */` |
|        3 | 10456 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10457 | `		}` |
|        - | 10458 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10459 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10460 | `		 * exception instead of reporting it uncaught.` |
|        - | 10461 | `		 */` |
|      478 | 10462 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10463 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10464 | `			 * by looking for a catch frame on the stack.` |
|        - | 10465 | `			 */` |
|      478 | 10466 | `			VmFrame *pF = pVm->pFrame;` |
|      478 | 10467 | `			int inCatch = 0;` |
|      956 | 10468 | `			while( pF ){` |
|      484 | 10469 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 | 10470 | `					inCatch = 1;` |
|        6 | 10471 | `					break;` |
|        - | 10472 | `				}` |
|      479 | 10473 | `				pF = pF->pParent;` |
|        1 | 10474 | `			}` |
|      478 | 10475 | `			if( inCatch ){` |
|        - | 10476 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 | 10477 | `				pThis->iRef++;` |
|        6 | 10478 | `				pVm->pPendingException = pThis;` |
|        6 | 10479 | `				return SXRET_OK;` |
|        - | 10480 | `			}` |
|      236 | 10481 | `		}` |
|        - | 10482 | `		/* Truly uncaught */` |
|      473 | 10483 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 | 10484 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10485 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10486 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10487 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10488 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10489 | `			}` |
|      ! 0 | 10490 | `		}` |
|      473 | 10491 | `		return rc;` |
|      ! 0 | 10492 | `	}else{` |
|       38 | 10493 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10494 | `		ph7_exception **apSaved = 0;` |
|        - | 10495 | `		sxu32 nSavedCount;` |
|        - | 10496 | `		sxi32 rc;` |
|       38 | 10497 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10498 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10499 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10500 | `		}` |
|        - | 10501 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10502 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10503 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10504 | `		 */` |
|       38 | 10505 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10506 | `		if( nSavedCount > 0 ){` |
|       11 | 10507 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10508 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 | 10509 | `			if( apSaved ){` |
|       11 | 10510 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10511 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 | 10512 | `				SySetReset(&pVm->aException);` |
|        3 | 10513 | `			}` |
|        3 | 10514 | `		}` |
|        - | 10515 | `		/* Create a private frame first */` |
|       38 | 10516 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10517 | `		if( rc == SXRET_OK ){` |
|       38 | 10518 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10519 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10520 | `			if( pObj ){` |
|       38 | 10521 | `				pThis->iRef++;` |
|       38 | 10522 | `				pObj->x.pOther = pThis;` |
|       38 | 10523 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10524 | `			}` |
|        - | 10525 | `			/* Execute the catch block */` |
|       38 | 10526 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10527 | `			/* Leave the frame */` |
|       38 | 10528 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10529 | `		}` |
|        - | 10530 | `		/* Restore the outer exception handlers */` |
|       38 | 10531 | `		if( apSaved ){` |
|        - | 10532 | `			sxu32 k;` |
|        - | 10533 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10534 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10535 | `			 * Restore the original outer entries.` |
|        - | 10536 | `			 */` |
|        8 | 10537 | `			SySetReset(&pVm->aException);` |
|       14 | 10538 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 | 10539 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 10540 | `			}` |
|        8 | 10541 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10542 | `		}` |
|        - | 10543 | `		/* Execute the finally block after catch */` |
|       38 | 10544 | `		if( pException->iHasFinally ){` |
|       11 | 10545 | `			pException->iFinallyDone = 1;` |
|        - | 10546 | `			{` |
|       11 | 10547 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 | 10548 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10549 | `					return SXERR_ABORT;` |
|        - | 10550 | `				}` |
|        - | 10551 | `			}` |
|        5 | 10552 | `		}` |
|       38 | 10553 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10554 | `			return SXERR_ABORT;` |
|        - | 10555 | `		}` |
|        - | 10556 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10557 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10558 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10559 | `		 */` |
|       38 | 10560 | `		if( pVm->pPendingException ){` |
|        6 | 10561 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 | 10562 | `			pVm->pPendingException = 0;` |
|        6 | 10563 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10564 | `		}` |
|        - | 10565 | `	}` |
|        - | 10566 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10567 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10568 | `	 */` |
|       34 | 10569 | `	return SXRET_OK;` |
|      259 | 10570 |  |
|        - | 10571 | `/*` |
|        - | 10572 | ` * Section:` |
|        - | 10573 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10574 | ` * Status:` |
|        - | 10575 | ` *    Stable.` |
|        - | 10576 | ` */` |
|        - | 10577 | `/*` |
|        - | 10578 | ` * string ph7version(void)` |
|        - | 10579 | ` *  Returns the running version of the PH7 version.` |
|        - | 10580 | ` * Parameters` |
|        - | 10581 | ` *  None` |
|        - | 10582 | ` * Return` |
|        - | 10583 | ` * Current PH7 version.` |
|        - | 10584 | ` */` |
|        2 | 10585 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10586 |  |
|        1 | 10587 | `	SXUNUSED(nArg);` |
|        1 | 10588 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10589 | `	/* Current engine version */` |
|        3 | 10590 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10591 | `	return PH7_OK;` |
|        1 | 10592 |  |
|        - | 10593 | `/*` |
|        - | 10594 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10595 | ` */` |
|        - | 10596 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10597 | ` "<html><head>"\` |
|        - | 10598 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10599 | ` "<style type=\"text/css\">"\` |
|        - | 10600 | ` "div {"\` |
|        - | 10601 | `     "border: 1px solid #cccccc;"\` |
|        - | 10602 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10603 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10604 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10605 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10606 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10607 | `     "-o-border-radius: 10px;"\` |
|        - | 10608 | `     "border-radius: 10px;"\` |
|        - | 10609 | `     "padding-left: 2em;"\` |
|        - | 10610 | `     "background-color: white;"\` |
|        - | 10611 | `     "margin-left: auto;"\` |
|        - | 10612 | `     "font-family: verdana;"\` |
|        - | 10613 | `     "padding-right: 2em;"\` |
|        - | 10614 | `     "margin-right: auto;"\` |
|        - | 10615 | `     "}"\` |
|        - | 10616 | `     "body {"\` |
|        - | 10617 | `     "padding: 0.2em;"\` |
|        - | 10618 | `     "font-style: normal;"\` |
|        - | 10619 | `     "font-size: medium;"\` |
|        - | 10620 | `     "background-color: #f2f2f2;"\` |
|        - | 10621 | `     "}"\` |
|        - | 10622 | `     "hr {"\` |
|        - | 10623 | `     "border-style: solid none none;"\` |
|        - | 10624 | `     "border-width: 1px medium medium;"\` |
|        - | 10625 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10626 | `     "height: 1px;"\` |
|        - | 10627 | `     "}"\` |
|        - | 10628 | `     "a {"\` |
|        - | 10629 | `     "color: #3366cc;"\` |
|        - | 10630 | `     "text-decoration: none;"\` |
|        - | 10631 | `     "}"\` |
|        - | 10632 | `     "a:hover {"\` |
|        - | 10633 | `     "color: #999999;"\` |
|        - | 10634 | `     "}"\` |
|        - | 10635 | `     "a:active {"\` |
|        - | 10636 | `     "color: #663399;"\` |
|        - | 10637 | `     "}"\` |
|        - | 10638 | `     "h1 {"\` |
|        - | 10639 | `     "margin: 0;"\` |
|        - | 10640 | `     "padding: 0;"\` |
|        - | 10641 | `     "font-family: Verdana;"\` |
|        - | 10642 | `     "font-weight: bold;"\` |
|        - | 10643 | `     "font-style: normal;"\` |
|        - | 10644 | `     "font-size: medium;"\` |
|        - | 10645 | `     "text-transform: capitalize;"\` |
|        - | 10646 | `     "color: #0a328c;"\` |
|        - | 10647 | `     "}"\` |
|        - | 10648 | `     "p {"\` |
|        - | 10649 | `     "margin: 0 auto;"\` |
|        - | 10650 | `     "font-size: medium;"\` |
|        - | 10651 | `     "font-style: normal;"\` |
|        - | 10652 | `     "font-family: verdana;"\` |
|        - | 10653 | `     "}"\` |
|        - | 10654 | `"</style></head><body>"\` |
|        - | 10655 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10656 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10657 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10658 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10659 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10660 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10661 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10662 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10663 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10664 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10665 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10666 |  |
|        - | 10667 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10668 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10669 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10670 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10671 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10672 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10673 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10674 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10675 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10676 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10677 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10678 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10679 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10680 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10681 |  |
|        - | 10682 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10683 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10684 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10685 | `"&nbsp;*<br>"\` |
|        - | 10686 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10687 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10688 | `"&nbsp;* are met:<br>"\` |
|        - | 10689 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10690 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10691 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10692 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10693 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10694 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10695 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10696 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10697 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10698 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10699 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10700 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10701 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10702 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10703 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10704 | `"&nbsp;*<br>"\` |
|        - | 10705 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10706 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10707 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10708 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10709 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10710 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10711 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10712 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10713 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10714 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10715 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10716 | `"&nbsp;*/<br>"\` |
|        - | 10717 | `"</span></small></small></p>"\` |
|        - | 10718 | `"</div></body></html>"` |
|        - | 10719 | `/*` |
|        - | 10720 | ` * bool ph7credits(void)` |
|        - | 10721 | ` * bool ph7info(void)` |
|        - | 10722 | ` * bool ph7copyright(void)` |
|        - | 10723 | ` *  Prints out the credits for PH7 engine` |
|        - | 10724 | ` * Parameters` |
|        - | 10725 | ` *  None` |
|        - | 10726 | ` * Return` |
|        - | 10727 | ` *  Always TRUE` |
|        - | 10728 | ` */` |
|        2 | 10729 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10730 |  |
|        3 | 10731 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10732 | `	/* Expand the HTML page above*/` |
|        3 | 10733 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10734 | `	ph7_context_output_format(` |
|        1 | 10735 | `		pCtx,` |
|        - | 10736 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10737 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10738 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10739 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10740 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10741 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10742 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10743 | `#ifdef __WINNT__` |
|        - | 10744 | `		"Windows NT"` |
|        - | 10745 | `#elif defined(__UNIXES__)` |
|        - | 10746 | `		"UNIX-Like"` |
|        - | 10747 | `#else` |
|        - | 10748 | `		"Other OS"` |
|        - | 10749 | `#endif` |
|        - | 10750 | `		);` |
|        3 | 10751 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10752 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10753 | `	SXUNUSED(apArg);` |
|        - | 10754 | `	/* Return TRUE */` |
|        - | 10755 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10756 | `	return PH7_OK;` |
|        1 | 10757 |  |
|        - | 10758 | `/*` |
|        - | 10759 | ` * Section:` |
|        - | 10760 | ` *    URL related routines.` |
|        - | 10761 | ` * Status:` |
|        - | 10762 | ` *    Stable.` |
|        - | 10763 | ` */` |
|        - | 10764 | `/*` |
|        - | 10765 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10766 | ` *  Parse a URL and return its fields.` |
|        - | 10767 | ` * Parameters` |
|        - | 10768 | ` *  $url` |
|        - | 10769 | ` *   The URL to parse.` |
|        - | 10770 | ` * $component` |
|        - | 10771 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10772 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10773 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10774 | ` *  in which case the return value will be an integer).` |
|        - | 10775 | ` * Return` |
|        - | 10776 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10777 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10778 | ` *  this array are:` |
|        - | 10779 | ` *   scheme - e.g. http` |
|        - | 10780 | ` *   host` |
|        - | 10781 | ` *   port` |
|        - | 10782 | ` *   user` |
|        - | 10783 | ` *   pass` |
|        - | 10784 | ` *   path` |
|        - | 10785 | ` *   query - after the question mark ?` |
|        - | 10786 | ` *   fragment - after the hashmark #` |
|        - | 10787 | ` * Note:` |
|        - | 10788 | ` *  FALSE is returned on failure.` |
|        - | 10789 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10790 | ` *  with the standard PHP engine.` |
|        - | 10791 | ` */` |
|       28 | 10792 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10793 |  |
|        - | 10794 | `	const char *zStr; /* Input string */` |
|        - | 10795 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10796 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10797 | `	int nLen;` |
|        - | 10798 | `	sxi32 rc;` |
|       29 | 10799 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10800 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10801 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10802 | `		return PH7_OK;` |
|        - | 10803 | `	}` |
|        - | 10804 | `	/* Extract the given URI */` |
|       29 | 10805 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10806 | `	if( nLen < 1 ){` |
|        - | 10807 | `		/* Nothing to process,return FALSE */` |
|        3 | 10808 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10809 | `		return PH7_OK;` |
|        - | 10810 | `	}` |
|        - | 10811 | `	/* Get a parse */` |
|       27 | 10812 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10813 | `	if( rc != SXRET_OK ){` |
|        - | 10814 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10815 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10816 | `		return PH7_OK;` |
|        - | 10817 | `	}` |
|       27 | 10818 | `	if( nArg > 1 ){` |
|      ! 0 | 10819 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10820 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10821 | `		switch(nComponent){` |
|      ! 0 | 10822 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10823 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10824 | `			if( pComp->nByte < 1 ){` |
|        - | 10825 | `				/* No available value,return NULL */` |
|      ! 0 | 10826 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10827 | `			}else{` |
|      ! 0 | 10828 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10829 | `			}` |
|      ! 0 | 10830 | `			break;` |
|      ! 0 | 10831 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10832 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10833 | `			if( pComp->nByte < 1 ){` |
|        - | 10834 | `				/* No available value,return NULL */` |
|      ! 0 | 10835 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10836 | `			}else{` |
|      ! 0 | 10837 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10838 | `			}` |
|      ! 0 | 10839 | `			break;` |
|      ! 0 | 10840 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10841 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10842 | `			if( pComp->nByte < 1 ){` |
|        - | 10843 | `				/* No available value,return NULL */` |
|      ! 0 | 10844 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10845 | `			}else{` |
|      ! 0 | 10846 | `				int iPort = 0;` |
|        - | 10847 | `				/* Cast the value to integer */` |
|      ! 0 | 10848 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10849 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10850 | `			}` |
|      ! 0 | 10851 | `			break;` |
|      ! 0 | 10852 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10853 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10854 | `			if( pComp->nByte < 1 ){` |
|        - | 10855 | `				/* No available value,return NULL */` |
|      ! 0 | 10856 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10857 | `			}else{` |
|      ! 0 | 10858 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10859 | `			}` |
|      ! 0 | 10860 | `			break;` |
|      ! 0 | 10861 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 10862 | `			pComp = &sURI.sPass;` |
|      ! 0 | 10863 | `			if( pComp->nByte < 1 ){` |
|        - | 10864 | `				/* No available value,return NULL */` |
|      ! 0 | 10865 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10866 | `			}else{` |
|      ! 0 | 10867 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10868 | `			}` |
|      ! 0 | 10869 | `			break;` |
|      ! 0 | 10870 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 10871 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 10872 | `			if( pComp->nByte < 1 ){` |
|        - | 10873 | `				/* No available value,return NULL */` |
|      ! 0 | 10874 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10875 | `			}else{` |
|      ! 0 | 10876 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10877 | `			}` |
|      ! 0 | 10878 | `			break;` |
|      ! 0 | 10879 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 10880 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 10881 | `			if( pComp->nByte < 1 ){` |
|        - | 10882 | `				/* No available value,return NULL */` |
|      ! 0 | 10883 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10884 | `			}else{` |
|      ! 0 | 10885 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10886 | `			}` |
|      ! 0 | 10887 | `			break;` |
|      ! 0 | 10888 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 10889 | `			pComp = &sURI.sPath;` |
|      ! 0 | 10890 | `			if( pComp->nByte < 1 ){` |
|        - | 10891 | `				/* No available value,return NULL */` |
|      ! 0 | 10892 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10893 | `			}else{` |
|      ! 0 | 10894 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10895 | `			}` |
|      ! 0 | 10896 | `			break;` |
|      ! 0 | 10897 | `		default:` |
|        - | 10898 | `			/* No such entry,return NULL */` |
|      ! 0 | 10899 | `			ph7_result_null(pCtx);` |
|      ! 0 | 10900 | `			break;` |
|        - | 10901 | `		}` |
|      ! 0 | 10902 | `	}else{` |
|        - | 10903 | `		ph7_value *pArray,*pValue;` |
|        - | 10904 | `		/* Return an associative array */` |
|       27 | 10905 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 10906 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 10907 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10908 | `			/* Out of memory */` |
|      ! 0 | 10909 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 10910 | `			/* Return false */` |
|      ! 0 | 10911 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 10912 | `			return PH7_OK;` |
|        - | 10913 | `		}` |
|        - | 10914 | `		/* Fill the array */` |
|       27 | 10915 | `		pComp = &sURI.sScheme;` |
|       27 | 10916 | `		if( pComp->nByte > 0 ){` |
|       19 | 10917 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 10918 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 10919 | `		}` |
|        - | 10920 | `		/* Reset the string cursor */` |
|       27 | 10921 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10922 | `		pComp = &sURI.sHost;` |
|       27 | 10923 | `		if( pComp->nByte > 0 ){` |
|       25 | 10924 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 10925 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 10926 | `		}` |
|        - | 10927 | `		/* Reset the string cursor */` |
|       27 | 10928 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10929 | `		pComp = &sURI.sPort;` |
|       27 | 10930 | `		if( pComp->nByte > 0 ){` |
|       11 | 10931 | `			int iPort = 0;/* cc warning */` |
|        - | 10932 | `			/* Convert to integer */` |
|       11 | 10933 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 10934 | `			ph7_value_int(pValue,iPort);` |
|       11 | 10935 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 10936 | `		}` |
|        - | 10937 | `		/* Reset the string cursor */` |
|       27 | 10938 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10939 | `		pComp = &sURI.sUser;` |
|       27 | 10940 | `		if( pComp->nByte > 0 ){` |
|        7 | 10941 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10942 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 10943 | `		}` |
|        - | 10944 | `		/* Reset the string cursor */` |
|       27 | 10945 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10946 | `		pComp = &sURI.sPass;` |
|       27 | 10947 | `		if( pComp->nByte > 0 ){` |
|        7 | 10948 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 10949 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 10950 | `		}` |
|        - | 10951 | `		/* Reset the string cursor */` |
|       27 | 10952 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10953 | `		pComp = &sURI.sPath;` |
|       27 | 10954 | `		if( pComp->nByte > 0 ){` |
|       17 | 10955 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 10956 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 10957 | `		}` |
|        - | 10958 | `		/* Reset the string cursor */` |
|       27 | 10959 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10960 | `		pComp = &sURI.sQuery;` |
|       27 | 10961 | `		if( pComp->nByte > 0 ){` |
|        5 | 10962 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10963 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 10964 | `		}` |
|        - | 10965 | `		/* Reset the string cursor */` |
|       27 | 10966 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 10967 | `		pComp = &sURI.sFragment;` |
|       27 | 10968 | `		if( pComp->nByte > 0 ){` |
|        5 | 10969 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 10970 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 10971 | `		}` |
|        - | 10972 | `		/* Return the created array */` |
|       27 | 10973 | `		ph7_result_value(pCtx,pArray);` |
|        - | 10974 | `		/* NOTE:` |
|        - | 10975 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 10976 | `		 * automatically as soon we return from this function.` |
|        - | 10977 | `		 */` |
|        - | 10978 | `	}` |
|        - | 10979 | `	/* All done */` |
|       27 | 10980 | `	return PH7_OK;` |
|       15 | 10981 |  |
|        - | 10982 | `/*` |
|        - | 10983 | ` * Section:` |
|        - | 10984 | ` *   Array related routines.` |
|        - | 10985 | ` * Status:` |
|        - | 10986 | ` *    Stable.` |
|        - | 10987 | ` * Note 2012-5-21 01:04:15:` |
|        - | 10988 | ` *  Array related functions that need access to the underlying` |
|        - | 10989 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 10990 | ` */` |
|        - | 10991 | `/*` |
|        - | 10992 | ` * The [compact()] function store it's state information in an instance` |
|        - | 10993 | ` * of the following structure.` |
|        - | 10994 | ` */` |
|        - | 10995 | `struct compact_data` |
|        - | 10996 |  |
|        - | 10997 | `	ph7_value *pArray;  /* Target array */` |
|        - | 10998 | `	int nRecCount;      /* Recursion count */` |
|        - | 10999 | `};` |
|        - | 11000 | `/*` |
|        - | 11001 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11002 | ` */` |
|      ! 0 | 11003 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11004 |  |
|      ! 0 | 11005 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11006 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11007 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11008 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11009 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11010 | `		SyString sVar;` |
|      ! 0 | 11011 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11012 | `		if( sVar.nByte > 0 ){` |
|        - | 11013 | `			/* Query the current frame */` |
|      ! 0 | 11014 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11015 | `			/* ^` |
|        - | 11016 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11017 | `			 */` |
|      ! 0 | 11018 | `			if( pKey ){` |
|        - | 11019 | `				/* Perform the insertion */` |
|      ! 0 | 11020 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11021 | `			}` |
|      ! 0 | 11022 | `		}` |
|      ! 0 | 11023 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11024 | `		int rc;` |
|        - | 11025 | `		/* Recursively traverse this array */` |
|      ! 0 | 11026 | `		pData->nRecCount++;` |
|      ! 0 | 11027 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11028 | `		pData->nRecCount--;` |
|      ! 0 | 11029 | `		return rc;` |
|        - | 11030 | `	}` |
|      ! 0 | 11031 | `	return SXRET_OK;` |
|      ! 0 | 11032 |  |
|        - | 11033 | `/*` |
|        - | 11034 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11035 | ` *  Create array containing variables and their values.` |
|        - | 11036 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11037 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11038 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11039 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11040 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11041 | ` * Parameters` |
|        - | 11042 | ` *  $varname` |
|        - | 11043 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11044 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11045 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11046 | ` *   it recursively.` |
|        - | 11047 | ` * Return` |
|        - | 11048 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11049 | ` */` |
|        2 | 11050 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11051 |  |
|        - | 11052 | `	ph7_value *pArray,*pObj;` |
|        3 | 11053 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11054 | `	const char *zName;` |
|        - | 11055 | `	SyString sVar;` |
|        - | 11056 | `	int i,nLen;` |
|        3 | 11057 | `	if( nArg < 1 ){` |
|        - | 11058 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11059 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11060 | `		return PH7_OK;` |
|        - | 11061 | `	}` |
|        - | 11062 | `	/* Create the array */` |
|        3 | 11063 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11064 | `	if( pArray == 0 ){` |
|        - | 11065 | `		/* Out of memory */` |
|      ! 0 | 11066 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11067 | `		/* Return NULL */` |
|      ! 0 | 11068 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11069 | `		return PH7_OK;` |
|        - | 11070 | `	}` |
|        - | 11071 | `	/* Perform the requested operation */` |
|        7 | 11072 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11073 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11074 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11075 | `				struct compact_data sData;` |
|      ! 0 | 11076 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11077 | `				/* Recursively walk the array */` |
|      ! 0 | 11078 | `				sData.nRecCount = 0;` |
|      ! 0 | 11079 | `				sData.pArray = pArray;` |
|      ! 0 | 11080 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11081 | `			}` |
|      ! 0 | 11082 | `		}else{` |
|        - | 11083 | `			/* Extract variable name */` |
|        5 | 11084 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11085 | `			if( nLen > 0 ){` |
|        5 | 11086 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11087 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11088 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11089 | `				if( pObj ){` |
|        5 | 11090 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11091 | `				}` |
|        2 | 11092 | `			}` |
|        - | 11093 | `		}` |
|        3 | 11094 | `	}` |
|        - | 11095 | `	/* Return the array */` |
|        3 | 11096 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11097 | `	return PH7_OK;` |
|        2 | 11098 |  |
|        - | 11099 | `/*` |
|        - | 11100 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11101 | ` * of the following structure.` |
|        - | 11102 | ` */` |
|        - | 11103 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11104 | `struct extract_aux_data` |
|        - | 11105 |  |
|        - | 11106 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11107 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11108 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11109 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11110 | `	int iFlags;           /* Control flags */` |
|        - | 11111 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11112 | `};` |
|        - | 11113 | `/* Forward declaration */` |
|        - | 11114 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11115 | `/*` |
|        - | 11116 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11117 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11118 | ` * Parameters` |
|        - | 11119 | ` * $var_array` |
|        - | 11120 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11121 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11122 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11123 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11124 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11125 | ` * $extract_type` |
|        - | 11126 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11127 | ` *  It can be one of the following values:` |
|        - | 11128 | ` *   EXTR_OVERWRITE` |
|        - | 11129 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11130 | ` *   EXTR_SKIP` |
|        - | 11131 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11132 | ` *   EXTR_PREFIX_SAME` |
|        - | 11133 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11134 | ` *   EXTR_PREFIX_ALL` |
|        - | 11135 | ` *       Prefix all variable names with prefix.` |
|        - | 11136 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11137 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11138 | ` *   EXTR_IF_EXISTS` |
|        - | 11139 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11140 | ` *       otherwise do nothing.` |
|        - | 11141 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11142 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11143 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11144 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11145 | ` *      the current symbol table.` |
|        - | 11146 | ` * $prefix` |
|        - | 11147 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11148 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11149 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11150 | ` *  underscore character.` |
|        - | 11151 | ` * Return` |
|        - | 11152 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11153 | ` */` |
|        4 | 11154 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11155 |  |
|        - | 11156 | `	extract_aux_data sAux;` |
|        - | 11157 | `	ph7_hashmap *pMap;` |
|        5 | 11158 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11159 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11160 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11161 | `		return PH7_OK;` |
|        - | 11162 | `	}` |
|        - | 11163 | `	/* Point to the target hashmap */` |
|        5 | 11164 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11165 | `	if( pMap->nEntry < 1 ){` |
|        - | 11166 | `		/* Empty map,return  0 */` |
|      ! 0 | 11167 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11168 | `		return PH7_OK;` |
|        - | 11169 | `	}` |
|        - | 11170 | `	/* Prepare the aux data */` |
|        5 | 11171 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11172 | `	if( nArg > 1 ){` |
|        3 | 11173 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11174 | `		if( nArg > 2 ){` |
|      ! 0 | 11175 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11176 | `		}` |
|        1 | 11177 | `	}` |
|        5 | 11178 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11179 | `	/* Invoke the worker callback */` |
|        5 | 11180 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11181 | `	/* Number of variables successfully imported */` |
|        5 | 11182 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11183 | `	return PH7_OK;` |
|        3 | 11184 |  |
|        - | 11185 | `/*` |
|        - | 11186 | ` * Worker callback for the [extract()] function defined` |
|        - | 11187 | ` * below.` |
|        - | 11188 | ` */` |
|        8 | 11189 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11190 |  |
|        9 | 11191 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11192 | `	int iFlags = pAux->iFlags;` |
|        9 | 11193 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11194 | `	ph7_value *pObj;` |
|        - | 11195 | `	SyString sVar;` |
|        9 | 11196 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11197 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11198 | `	}` |
|        - | 11199 | `	/* Perform a string cast */` |
|        9 | 11200 | `	PH7_MemObjToString(pKey);` |
|        9 | 11201 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11202 | `		/* Unavailable variable name */` |
|      ! 0 | 11203 | `		return SXRET_OK;` |
|        - | 11204 | `	}` |
|        9 | 11205 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11206 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11207 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11208 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11209 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11210 | `			);` |
|      ! 0 | 11211 | `	}else{` |
|       13 | 11212 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11213 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11214 | `	}` |
|        9 | 11215 | `	sVar.zString = pAux->zWorker;` |
|        - | 11216 | `	/* Try to extract the variable */` |
|        9 | 11217 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11218 | `	if( pObj ){` |
|        - | 11219 | `		/* Collision */` |
|        5 | 11220 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11221 | `			return SXRET_OK;` |
|        - | 11222 | `		}` |
|        5 | 11223 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11224 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11225 | `				/* Already prefixed */` |
|      ! 0 | 11226 | `				return SXRET_OK;` |
|        - | 11227 | `			}` |
|      ! 0 | 11228 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11229 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11230 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11231 | `				);` |
|      ! 0 | 11232 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11233 | `		}` |
|        3 | 11234 | `	}else{` |
|        - | 11235 | `		/* Create the variable */` |
|        5 | 11236 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11237 | `	}` |
|        9 | 11238 | `	if( pObj ){` |
|        - | 11239 | `		/* Overwrite the old value */` |
|        9 | 11240 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11241 | `		/* Increment counter */` |
|        9 | 11242 | `		pAux->iCount++;` |
|        4 | 11243 | `	}` |
|        9 | 11244 | `	return SXRET_OK;` |
|        5 | 11245 |  |
|        - | 11246 | `/*` |
|        - | 11247 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11248 | ` * defined below.` |
|        - | 11249 | ` */` |
|        2 | 11250 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11251 |  |
|        3 | 11252 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11253 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11254 | `	ph7_value *pObj;` |
|        - | 11255 | `	SyString sVar;` |
|        - | 11256 | `	/* Perform a string cast */` |
|        3 | 11257 | `	PH7_MemObjToString(pKey);` |
|        3 | 11258 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11259 | `		/* Unavailable variable name */` |
|      ! 0 | 11260 | `		return SXRET_OK;` |
|        - | 11261 | `	}` |
|        3 | 11262 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11263 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11264 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11265 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11266 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11267 | `			);` |
|        2 | 11268 | `	}else{` |
|      ! 0 | 11269 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11270 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11271 | `	}` |
|        3 | 11272 | `	sVar.zString = pAux->zWorker;` |
|        - | 11273 | `	/* Extract the variable */` |
|        3 | 11274 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11275 | `	if( pObj ){` |
|        3 | 11276 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11277 | `	}` |
|        3 | 11278 | `	return SXRET_OK;` |
|        2 | 11279 |  |
|        - | 11280 | `/*` |
|        - | 11281 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11282 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11283 | ` * Parameters` |
|        - | 11284 | ` * $types` |
|        - | 11285 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11286 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11287 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11288 | ` *  POST includes the POST uploaded file information.` |
|        - | 11289 | ` *  Note:` |
|        - | 11290 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11291 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11292 | ` * $prefix` |
|        - | 11293 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11294 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11295 | ` *  variable named $pref_userid.` |
|        - | 11296 | ` * Return` |
|        - | 11297 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11298 | ` */` |
|        2 | 11299 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11300 |  |
|        - | 11301 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11302 | `	extract_aux_data sAux;` |
|        - | 11303 | `	int nLen,nPrefixLen;` |
|        - | 11304 | `	ph7_value *pSuper;` |
|        - | 11305 | `	ph7_vm *pVm;` |
|        - | 11306 | `	/* By default import only $_GET variables  */` |
|        3 | 11307 | `	zImport = "G";` |
|        3 | 11308 | `	nLen = (int)sizeof(char);` |
|        3 | 11309 | `	zPrefix = 0;` |
|        3 | 11310 | `	nPrefixLen = 0;` |
|        3 | 11311 | `	if( nArg > 0 ){` |
|        3 | 11312 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11313 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11314 | `		}` |
|        3 | 11315 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11316 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11317 | `		}` |
|        1 | 11318 | `	}` |
|        - | 11319 | `	/* Point to the underlying VM */` |
|        3 | 11320 | `	pVm = pCtx->pVm;` |
|        - | 11321 | `	/* Initialize the aux data */` |
|        3 | 11322 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11323 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11324 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11325 | `	sAux.pVm = pVm;` |
|        - | 11326 | `	/* Extract */` |
|        3 | 11327 | `	zEnd = &zImport[nLen];` |
|        5 | 11328 | `	while( zImport < zEnd ){` |
|        3 | 11329 | `		int c = zImport[0];` |
|        3 | 11330 | `		pSuper = 0;` |
|        3 | 11331 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11332 | `			/* Import $_GET variables */` |
|        3 | 11333 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11334 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11335 | `			/* Import $_POST variables */` |
|      ! 0 | 11336 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11337 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11338 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11339 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11340 | `		}` |
|        3 | 11341 | `		if( pSuper ){` |
|        - | 11342 | `			/* Iterate throw array entries */` |
|        3 | 11343 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11344 | `		}` |
|        - | 11345 | `		/* Advance the cursor */` |
|        3 | 11346 | `		zImport++;` |
|        1 | 11347 | `	}` |
|        - | 11348 | `	/* All done,return TRUE*/` |
|        3 | 11349 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11350 | `	return PH7_OK;` |
|        1 | 11351 |  |
|        - | 11352 | `/*` |
|        - | 11353 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11354 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11355 | ` * information.` |
|        - | 11356 | ` */` |
|    10496 | 11357 | `static sxi32 VmEvalChunk(` |
|        - | 11358 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11359 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11360 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11361 | `	int iFlags,         /* Compile flag */` |
|        - | 11362 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11363 | `	)` |
|        2 | 11364 |  |
|        - | 11365 | `	SySet *pByteCode,aByteCode;` |
|        - | 11366 | `	SyBlob sSavedNs;` |
|    10498 | 11367 | `	ProcConsumer xErr = 0;` |
|    10498 | 11368 | `	void *pErrData = 0;` |
|        - | 11369 | `	/* Initialize bytecode container */` |
|    10498 | 11370 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10498 | 11371 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11372 | `	/* Reset the code generator */` |
|    10498 | 11373 | `	if( bTrueReturn ){` |
|        - | 11374 | `		/* Included file,log compile-time errors */` |
|     7637 | 11375 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7637 | 11376 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3818 | 11377 | `	}` |
|    10498 | 11378 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11379 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11380 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11381 | `	 * the caller's namespace is restored. */` |
|    10498 | 11382 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10498 | 11383 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10498 | 11384 | `	if( bTrueReturn ){` |
|        - | 11385 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7637 | 11386 | `		SyBlobReset(&pVm->sNamespace);` |
|     3818 | 11387 | `	}` |
|        - | 11388 | `	/* Swap bytecode container */` |
|    10498 | 11389 | `	pByteCode = pVm->pByteContainer;` |
|    10498 | 11390 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11391 | `	/* Compile the chunk */` |
|    10498 | 11392 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15746 | 11393 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11394 | `		/* Compilation error,return false */` |
|        3 | 11395 | `		if( pCtx ){` |
|        3 | 11396 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11397 | `		}` |
|        2 | 11398 | `	}else{` |
|        - | 11399 | `		/* Mount any newly defined classes */` |
|        - | 11400 | `		SyHashEntry *pEntry;` |
|        - | 11401 | `		ph7_class *pClass;` |
|        - | 11402 | `		ph7_value sResult; /* Return value */` |
|        - | 11403 | `		sxi32 rc;` |
|    10496 | 11404 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   316975 | 11405 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   301234 | 11406 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11407 | `			/* Only mount classes that haven't been mounted yet */` |
|   301234 | 11408 | `			if( !pClass->bMounted ){` |
|    75298 | 11409 | `				rc = VmMountUserClass(pVm,pClass);` |
|    75298 | 11410 | `				if( rc != SXRET_OK ){` |
|        - | 11411 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11412 | `					if( pCtx ){` |
|      ! 0 | 11413 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11414 | `					}` |
|      ! 0 | 11415 | `					goto Cleanup;` |
|        - | 11416 | `				}` |
|    37648 | 11417 | `			}` |
|        2 | 11418 | `		}` |
|    10496 | 11419 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11420 | `			/* Out of memory */` |
|      ! 0 | 11421 | `			if( pCtx ){` |
|      ! 0 | 11422 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11423 | `			}` |
|      ! 0 | 11424 | `			goto Cleanup;` |
|        - | 11425 | `		}` |
|    10496 | 11426 | `		if( bTrueReturn ){` |
|        - | 11427 | `			/* Assume a boolean true return value */` |
|     7637 | 11428 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3819 | 11429 | `		}else{` |
|        - | 11430 | `			/* Assume a null return value */` |
|     2860 | 11431 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11432 | `		}` |
|        - | 11433 | `		/* Execute the compiled chunk */` |
|    10496 | 11434 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10496 | 11435 | `		if( pCtx ){` |
|        - | 11436 | `			/* Set the execution result */` |
|     7650 | 11437 | `			ph7_result_value(pCtx,&sResult);` |
|     3824 | 11438 | `		}` |
|    10496 | 11439 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11440 | `	}` |
|     5248 | 11441 | `Cleanup:` |
|        - | 11442 | `	/* Cleanup the mess left behind */` |
|    10498 | 11443 | `	pVm->pByteContainer = pByteCode;` |
|    10498 | 11444 | `	SySetRelease(&aByteCode);` |
|        - | 11445 | `	/* Restore caller's namespace state */` |
|    10498 | 11446 | `	SyBlobReset(&pVm->sNamespace);` |
|    10498 | 11447 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10498 | 11448 | `	SyBlobRelease(&sSavedNs);` |
|    10498 | 11449 | `	return SXRET_OK;` |
|        2 | 11450 |  |
|        - | 11451 | `/*` |
|        - | 11452 | ` * value eval(string $code)` |
|        - | 11453 | ` *   Evaluate a string as PHP code.` |
|        - | 11454 | ` * Parameter` |
|        - | 11455 | ` *  code: PHP code to evaluate.` |
|        - | 11456 | ` * Return` |
|        - | 11457 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11458 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11459 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11460 | ` */` |
|       16 | 11461 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11462 |  |
|        - | 11463 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 | 11464 | `	if( nArg < 1 ){` |
|        - | 11465 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11466 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11467 | `		return SXRET_OK;` |
|        - | 11468 | `	}` |
|        - | 11469 | `	/* Chunk to evaluate */` |
|       18 | 11470 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 | 11471 | `	if( sChunk.nByte < 1 ){` |
|        - | 11472 | `		/* Empty string,return NULL */` |
|        3 | 11473 | `		ph7_result_null(pCtx);` |
|        3 | 11474 | `		return SXRET_OK;` |
|        - | 11475 | `	}` |
|        - | 11476 | `	/* Eval the chunk */` |
|       16 | 11477 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 | 11478 | `	return SXRET_OK;` |
|       10 | 11479 |  |
|        - | 11480 | `/*` |
|        - | 11481 | ` * Check if a file path is already included.` |
|        - | 11482 | ` */` |
|    15268 | 11483 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 | 11484 |  |
|        - | 11485 | `	SyString *aEntries;` |
|        - | 11486 | `	sxu32 n;` |
|    15269 | 11487 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11488 | `	/* Perform a linear search */` |
| 58267061 | 11489 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 58251799 | 11490 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11491 | `			/* Already included */` |
|        7 | 11492 | `			return TRUE;` |
|        - | 11493 | `		}` |
| 29125897 | 11494 | `	}` |
|    15263 | 11495 | `	return FALSE;` |
|     7635 | 11496 |  |
|        - | 11497 | `/*` |
|        - | 11498 | ` * Push a file path in the appropriate VM container.` |
|        - | 11499 | ` */` |
|    18106 | 11500 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11501 |  |
|        - | 11502 | `	SyString sPath;` |
|        - | 11503 | `	char *zDup;` |
|        - | 11504 | `#ifdef __WINNT__` |
|        - | 11505 | `	char *zCur;` |
|        - | 11506 | `#endif` |
|        - | 11507 | `	sxi32 rc;` |
|    18108 | 11508 | `	if( nLen < 0 ){` |
|     2840 | 11509 | `		nLen = SyStrlen(zPath);` |
|     1419 | 11510 | `	}` |
|        - | 11511 | `	/* Duplicate the file path first */` |
|    18108 | 11512 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18108 | 11513 | `	if( zDup == 0 ){` |
|      ! 0 | 11514 | `		return SXERR_MEM;` |
|        - | 11515 | `	}` |
|        - | 11516 | `#ifdef __WINNT__` |
|        - | 11517 | `	/* Normalize path on windows` |
|        - | 11518 | `	 * Example:` |
|        - | 11519 | `	 *    Path/To/File.php` |
|        - | 11520 | `	 * becomes` |
|        - | 11521 | `	 *   path\to\file.php` |
|        - | 11522 | `	 */` |
|        2 | 11523 | `	zCur = zDup;` |
|        2 | 11524 | `	while( zCur[0] != 0 ){` |
|        2 | 11525 | `		if( zCur[0] == '/' ){` |
|        2 | 11526 | `			zCur[0] = '\\';` |
|        2 | 11527 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11528 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11529 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11530 | `		}` |
|        2 | 11531 | `		zCur++;` |
|        2 | 11532 | `	}` |
|        - | 11533 | `#endif` |
|        - | 11534 | `	/* Install the file path */` |
|    18108 | 11535 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18108 | 11536 | `	if( !bMain ){` |
|    15269 | 11537 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11538 | `			/* Already included */` |
|        7 | 11539 | `			*pNew = 0;` |
|        4 | 11540 | `		}else{` |
|        - | 11541 | `			/* Insert in the corresponding container */` |
|    15263 | 11542 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15263 | 11543 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11544 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11545 | `				return rc;` |
|        - | 11546 | `			}` |
|    15263 | 11547 | `			*pNew = 1;` |
|        - | 11548 | `		}` |
|     7634 | 11549 | `	}` |
|    18108 | 11550 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18108 | 11551 | `	return SXRET_OK;` |
|     9055 | 11552 |  |
|        - | 11553 | `/*` |
|        - | 11554 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11555 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11556 | ` * indicates failure.` |
|        - | 11557 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11558 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11559 | ` * operations.` |
|        - | 11560 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11561 | ` * this function is a no-op.` |
|        - | 11562 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11563 | ` * constructs for more information.` |
|        - | 11564 | ` */` |
|     7642 | 11565 | `static sxi32 VmExecIncludedFile(` |
|        - | 11566 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11567 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11568 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11569 | `	 )` |
|        2 | 11570 |  |
|        - | 11571 | `	sxi32 rc;` |
|        - | 11572 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11573 | `	const ph7_io_stream *pStream;` |
|        - | 11574 | `	SyBlob sContents;` |
|        - | 11575 | `	void *pHandle;` |
|        - | 11576 | `	ph7_vm *pVm;` |
|        - | 11577 | `	int isNew;` |
|        - | 11578 | `	/* Initialize fields */` |
|     7644 | 11579 | `	pVm = pCtx->pVm;` |
|     7644 | 11580 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7644 | 11581 | `	isNew = 0;` |
|        - | 11582 | `	/* Extract the associated stream */` |
|     7644 | 11583 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11584 | `	/*` |
|        - | 11585 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11586 | `	 * in a read-only mode.` |
|        - | 11587 | `	 */` |
|     7644 | 11588 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7644 | 11589 | `	if( pHandle == 0 ){` |
|        3 | 11590 | `		return SXERR_IO;` |
|        - | 11591 | `	}` |
|     7641 | 11592 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7641 | 11593 | `	if( IncludeOnce && !isNew ){` |
|        - | 11594 | `		/* Already included */` |
|        5 | 11595 | `		rc = SXERR_EXISTS;` |
|        3 | 11596 | `	}else{` |
|        - | 11597 | `		/* Read the whole file contents */` |
|     7637 | 11598 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7637 | 11599 | `		if( rc == SXRET_OK ){` |
|        - | 11600 | `			SyString sScript;` |
|        - | 11601 | `			/* Compile and execute the script */` |
|     7637 | 11602 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7637 | 11603 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3818 | 11604 | `		}` |
|        - | 11605 | `	}` |
|        - | 11606 | `	/* Pop from the set of included file */` |
|     7641 | 11607 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11608 | `	/* Close the handle */` |
|     7641 | 11609 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11610 | `	/* Release the working buffer */` |
|     7641 | 11611 | `	SyBlobRelease(&sContents);` |
|        - | 11612 | `#else` |
|        - | 11613 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11614 | `	SXUNUSED(pPath);` |
|        - | 11615 | `	SXUNUSED(IncludeOnce);` |
|        - | 11616 | `	rc = SXERR_IO;` |
|        - | 11617 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7641 | 11618 | `	return rc;` |
|     3823 | 11619 |  |
|        - | 11620 | `/*` |
|        - | 11621 | ` * string get_include_path(void)` |
|        - | 11622 | ` *  Gets the current include_path configuration option.` |
|        - | 11623 | ` * Parameter` |
|        - | 11624 | ` *  None` |
|        - | 11625 | ` * Return` |
|        - | 11626 | ` *  Included paths as a string` |
|        - | 11627 | ` */` |
|        2 | 11628 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11629 |  |
|        3 | 11630 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11631 | `	SyString *aEntry;` |
|        - | 11632 | `	int dir_sep;` |
|        - | 11633 | `	sxu32 n;` |
|        - | 11634 | `#ifdef __WINNT__` |
|        1 | 11635 | `	dir_sep = ';';` |
|        - | 11636 | `#else` |
|        - | 11637 | `	/* Assume UNIX path separator */` |
|        2 | 11638 | `	dir_sep = ':';` |
|        - | 11639 | `#endif` |
|        1 | 11640 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11641 | `	SXUNUSED(apArg);` |
|        - | 11642 | `	/* Point to the list of import paths */` |
|        3 | 11643 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11644 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11645 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11646 | `		if( n > 0 ){` |
|        - | 11647 | `			/* Append dir seprator */` |
|      ! 0 | 11648 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11649 | `		}` |
|        - | 11650 | `		/* Append path */` |
|        3 | 11651 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11652 | `	}` |
|        3 | 11653 | `	return PH7_OK;` |
|        1 | 11654 |  |
|        - | 11655 | `/*` |
|        - | 11656 | ` * string get_get_included_files(void)` |
|        - | 11657 | ` *  Gets the current include_path configuration option.` |
|        - | 11658 | ` * Parameter` |
|        - | 11659 | ` *  None` |
|        - | 11660 | ` * Return` |
|        - | 11661 | ` *  Included paths as a string` |
|        - | 11662 | ` */` |
|        2 | 11663 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11664 |  |
|        3 | 11665 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11666 | `	ph7_value *pArray,*pWorker;` |
|        - | 11667 | `	SyString *pEntry;` |
|        - | 11668 | `	int c,d;` |
|        - | 11669 | `	/* Create an array and a working value */` |
|        3 | 11670 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11671 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11672 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11673 | `		/* Out of memory,return null */` |
|      ! 0 | 11674 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11675 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11676 | `		SXUNUSED(apArg);` |
|      ! 0 | 11677 | `		return PH7_OK;` |
|        - | 11678 | `	}` |
|        3 | 11679 | `	c = d = '/';` |
|        - | 11680 | `#ifdef __WINNT__` |
|        1 | 11681 | `	d = '\\';` |
|        - | 11682 | `#endif` |
|        - | 11683 | `	/* Iterate throw entries */` |
|        3 | 11684 | `	SySetResetCursor(pFiles);` |
|     3689 | 11685 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11686 | `		const char *zBase,*zEnd;` |
|        - | 11687 | `		int iLen;` |
|        - | 11688 | `		/* reset the string cursor */` |
|     3687 | 11689 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11690 | `		/* Extract base name */` |
|     3687 | 11691 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11692 | `		/* Ignore trailing '/' */` |
|     5530 | 11693 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11694 | `			zEnd--;` |
|      ! 0 | 11695 | `		}` |
|     3687 | 11696 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113770 | 11697 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108241 | 11698 | `			zEnd--;` |
|        1 | 11699 | `		}` |
|     3687 | 11700 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3687 | 11701 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11702 | `		/* Copy entry name */` |
|     3687 | 11703 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11704 | `		/* Perform the insertion */` |
|     3687 | 11705 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11706 | `	}` |
|        - | 11707 | `	/* All done,return the created array */` |
|        3 | 11708 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11709 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11710 | `	 * by the engine as soon we return from this foreign` |
|        - | 11711 | `	 * function.` |
|        - | 11712 | `	 */` |
|        3 | 11713 | `	return PH7_OK;` |
|        2 | 11714 |  |
|        - | 11715 | `/*` |
|        - | 11716 | ` * include:` |
|        - | 11717 | ` * According to the PHP reference manual.` |
|        - | 11718 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11719 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11720 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11721 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11722 | ` *  and the current working directory before failing. The include()` |
|        - | 11723 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11724 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11725 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11726 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11727 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11728 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11729 | ` *  directory to find the requested file.` |
|        - | 11730 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11731 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11732 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11733 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11734 | ` */` |
|     7630 | 11735 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11736 |  |
|        - | 11737 | `	SyString sFile;` |
|        - | 11738 | `	sxi32 rc;` |
|     7632 | 11739 | `	if( nArg < 1 ){` |
|        - | 11740 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11741 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11742 | `		return SXRET_OK;` |
|        - | 11743 | `	}` |
|        - | 11744 | `	/* File to include */` |
|     7632 | 11745 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7632 | 11746 | `	if( sFile.nByte < 1 ){` |
|        - | 11747 | `		/* Empty string,return NULL */` |
|      ! 0 | 11748 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11749 | `		return SXRET_OK;` |
|        - | 11750 | `	}` |
|        - | 11751 | `	/* Open,compile and execute the desired script */` |
|     7632 | 11752 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7632 | 11753 | `	if( rc != SXRET_OK ){` |
|        - | 11754 | `		/* Emit a warning and return false */` |
|        3 | 11755 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11756 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11757 | `	}` |
|     7632 | 11758 | `	return SXRET_OK;` |
|     3817 | 11759 |  |
|        - | 11760 | `/*` |
|        - | 11761 | ` * include_once:` |
|        - | 11762 | ` *  According to the PHP reference manual.` |
|        - | 11763 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11764 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11765 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11766 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11767 | ` *   just once.` |
|        - | 11768 | ` */` |
|        4 | 11769 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11770 |  |
|        - | 11771 | `	SyString sFile;` |
|        - | 11772 | `	sxi32 rc;` |
|        5 | 11773 | `	if( nArg < 1 ){` |
|        - | 11774 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11775 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11776 | `		return SXRET_OK;` |
|        - | 11777 | `	}` |
|        - | 11778 | `	/* File to include */` |
|        5 | 11779 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11780 | `	if( sFile.nByte < 1 ){` |
|        - | 11781 | `		/* Empty string,return NULL */` |
|      ! 0 | 11782 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11783 | `		return SXRET_OK;` |
|        - | 11784 | `	}` |
|        - | 11785 | `	/* Open,compile and execute the desired script */` |
|        5 | 11786 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11787 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11788 | `		/* File already included,return TRUE */` |
|        3 | 11789 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11790 | `		return SXRET_OK;` |
|        - | 11791 | `	}` |
|        3 | 11792 | `	if( rc != SXRET_OK ){` |
|        - | 11793 | `		/* Emit a warning and return false */` |
|      ! 0 | 11794 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11795 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11796 | ` 	}` |
|        3 | 11797 | `	return SXRET_OK;` |
|        3 | 11798 |  |
|        - | 11799 | `/*` |
|        - | 11800 | ` * require.` |
|        - | 11801 | ` *  According to the PHP reference manual.` |
|        - | 11802 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11803 | ` *   also produce a fatal level error.` |
|        - | 11804 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11805 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11806 | ` */` |
|        4 | 11807 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11808 |  |
|        - | 11809 | `	SyString sFile;` |
|        - | 11810 | `	sxi32 rc;` |
|        5 | 11811 | `	if( nArg < 1 ){` |
|        - | 11812 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11813 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11814 | `		return SXRET_OK;` |
|        - | 11815 | `	}` |
|        - | 11816 | `	/* File to include */` |
|        5 | 11817 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11818 | `	if( sFile.nByte < 1 ){` |
|        - | 11819 | `		/* Empty string,return NULL */` |
|      ! 0 | 11820 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11821 | `		return SXRET_OK;` |
|        - | 11822 | `	}` |
|        - | 11823 | `	/* Open,compile and execute the desired script */` |
|        5 | 11824 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 11825 | `	if( rc != SXRET_OK ){` |
|        - | 11826 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11827 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11828 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11829 | `		return PH7_ABORT;` |
|        - | 11830 | `	}` |
|        5 | 11831 | `	return SXRET_OK;` |
|        3 | 11832 |  |
|        - | 11833 | `/*` |
|        - | 11834 | ` * require_once:` |
|        - | 11835 | ` *  According to the PHP reference manual.` |
|        - | 11836 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11837 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11838 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11839 | ` *   and how it differs from its non _once siblings.` |
|        - | 11840 | ` */` |
|        4 | 11841 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11842 |  |
|        - | 11843 | `	SyString sFile;` |
|        - | 11844 | `	sxi32 rc;` |
|        5 | 11845 | `	if( nArg < 1 ){` |
|        - | 11846 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11847 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11848 | `		return SXRET_OK;` |
|        - | 11849 | `	}` |
|        - | 11850 | `	/* File to include */` |
|        5 | 11851 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11852 | `	if( sFile.nByte < 1 ){` |
|        - | 11853 | `		/* Empty string,return NULL */` |
|      ! 0 | 11854 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11855 | `		return SXRET_OK;` |
|        - | 11856 | `	}` |
|        - | 11857 | `	/* Open,compile and execute the desired script */` |
|        5 | 11858 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11859 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11860 | `		/* File already included,return TRUE */` |
|        3 | 11861 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11862 | `		return SXRET_OK;` |
|        - | 11863 | `	}` |
|        3 | 11864 | `	if( rc != SXRET_OK ){` |
|        - | 11865 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11866 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11867 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11868 | `		return PH7_ABORT;` |
|        - | 11869 | `	}` |
|        3 | 11870 | `	return SXRET_OK;` |
|        3 | 11871 |  |
|        - | 11872 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 11873 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 11874 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 11875 | `/* Table of built-in VM functions. */` |
|        - | 11876 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 11877 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 11878 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 11879 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 11880 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 11881 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 11882 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 11883 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 11884 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 11885 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 11886 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 11887 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 11888 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 11889 | `	    /* Constants management */` |
|        - | 11890 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 11891 | `	{ "define",   vm_builtin_define               },` |
|        - | 11892 | `	{ "constant", vm_builtin_constant             },` |
|        - | 11893 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 11894 | `	   /* Class/Object functions */` |
|        - | 11895 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 11896 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 11897 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 11898 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 11899 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 11900 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 11901 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 11902 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 11903 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 11904 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 11905 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 11906 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 11907 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 11908 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 11909 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 11910 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 11911 | `	   /* Random numbers/strings generators */` |
|        - | 11912 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 11913 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 11914 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 11915 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 11916 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 11917 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11918 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11919 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 11920 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11921 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11922 | `	   /* Language constructs functions */` |
|        - | 11923 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 11924 | `	{ "print", vm_builtin_print                   },` |
|        - | 11925 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 11926 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 11927 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 11928 | `	  /* Variable handling functions */` |
|        - | 11929 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 11930 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 11931 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 11932 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 11933 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 11934 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 11935 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 11936 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 11937 | `	  /* Ouput control functions */` |
|        - | 11938 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 11939 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 11940 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 11941 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 11942 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 11943 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 11944 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 11945 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 11946 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 11947 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 11948 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 11949 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 11950 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 11951 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 11952 | `	  /* Assertion functions */` |
|        - | 11953 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 11954 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 11955 | `	  /* Error reporting functions */` |
|        - | 11956 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 11957 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 11958 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 11959 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 11960 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 11961 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 11962 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 11963 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 11964 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 11965 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 11966 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 11967 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 11968 | `	  /* Release info */` |
|        - | 11969 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 11970 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 11971 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 11972 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 11973 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 11974 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 11975 | `	  /* hashmap */` |
|        - | 11976 | `	{"compact",          vm_builtin_compact       },` |
|        - | 11977 | `	{"extract",          vm_builtin_extract       },` |
|        - | 11978 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 11979 | `	  /* URL related function */` |
|        - | 11980 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 11981 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 11982 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11983 | `	   /* XML processing functions */` |
|        - | 11984 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 11985 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 11986 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 11987 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 11988 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 11989 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 11990 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 11991 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 11992 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 11993 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 11994 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 11995 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 11996 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 11997 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 11998 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 11999 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12000 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12001 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12002 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12003 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12004 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12005 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12006 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12007 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12008 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12009 | `	   /* Command line processing */` |
|        - | 12010 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12011 | `	   /* JSON encoding/decoding */` |
|        - | 12012 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12013 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12014 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12015 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12016 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12017 | `	   /* Files/URI inclusion facility */` |
|        - | 12018 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12019 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12020 | `	{ "include",      vm_builtin_include          },` |
|        - | 12021 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12022 | `	{ "require",      vm_builtin_require          },` |
|        - | 12023 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12024 | `};` |
|        - | 12025 | `/*` |
|        - | 12026 | ` * Register the built-in VM functions defined above.` |
|        - | 12027 | ` */` |
|     2586 | 12028 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12029 |  |
|        - | 12030 | `	sxi32 rc;` |
|        - | 12031 | `	sxu32 n;` |
|   323252 | 12032 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12033 | `		/* Note that these special functions have access` |
|        - | 12034 | `		 * to the underlying virtual machine as their` |
|        - | 12035 | `		 * private data.` |
|        - | 12036 | `		 */` |
|   320666 | 12037 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   320666 | 12038 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12039 | `			return rc;` |
|        - | 12040 | `		}` |
|   160334 | 12041 | `	}` |
|     2588 | 12042 | `	return SXRET_OK;` |
|     1295 | 12043 |  |
|        - | 12044 | `/*` |
|        - | 12045 | ` * Check if the given name refer to an installed class.` |
|        - | 12046 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12047 | ` */` |
|    30046 | 12048 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12049 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12050 | `	const char *zName,  /* Name of the target class */` |
|        - | 12051 | `	sxu32 nByte,        /* zName length */` |
|        - | 12052 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12053 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12054 | `						 */` |
|        - | 12055 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12056 | `	)` |
|        2 | 12057 |  |
|        - | 12058 | `	SyHashEntry *pEntry;` |
|        - | 12059 | `	ph7_class *pClass;` |
|    15023 | 12060 | `	SXUNUSED(iNest);` |
|        - | 12061 | `	/* Exact class lookup.` |
|        - | 12062 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12063 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    30048 | 12064 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    30048 | 12065 | `	if( pEntry == 0 ){` |
|       10 | 12066 | `		return 0;` |
|        - | 12067 | `	}` |
|    30040 | 12068 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    30040 | 12069 | `	if( !iLoadable ){` |
|    28846 | 12070 | `		return pClass;` |
|        - | 12071 | `	}` |
|        - | 12072 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1196 | 12073 | `	while(pClass){` |
|     1196 | 12074 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1196 | 12075 | `			return pClass;` |
|        - | 12076 | `		}` |
|      ! 0 | 12077 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12078 | `	}` |
|      ! 0 | 12079 | `	return 0;` |
|    15025 | 12080 |  |
|        - | 12081 | `/*` |
|        - | 12082 | ` * Reference Table Implementation` |
|        - | 12083 | ` * Status: stable <chm@symisc.net>` |
|        - | 12084 | ` * Intro` |
|        - | 12085 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12086 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12087 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12088 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12089 | ` *  Refer to the official for more information on this powerful` |
|        - | 12090 | ` *  extension.` |
|        - | 12091 | ` */` |
|        - | 12092 | `/*` |
|        - | 12093 | ` * Allocate a new reference entry.` |
|        - | 12094 | ` */` |
|  3024580 | 12095 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12096 |  |
|        - | 12097 | `	VmRefObj *pRef;` |
|        - | 12098 | `	/* Allocate a new instance */` |
|  3024582 | 12099 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3024582 | 12100 | `	if( pRef == 0 ){` |
|      ! 0 | 12101 | `		return 0;` |
|        - | 12102 | `	}` |
|        - | 12103 | `	/* Zero the structure */` |
|  3024582 | 12104 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12105 | `	/* Initialize fields */` |
|  3024582 | 12106 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3024582 | 12107 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3024582 | 12108 | `	pRef->nIdx = nIdx;` |
|  3024582 | 12109 | `	return pRef;` |
|  1512292 | 12110 |  |
|        - | 12111 | `/*` |
|        - | 12112 | ` * Default hash function used by the reference table` |
|        - | 12113 | ` * for lookup/insertion operations.` |
|        - | 12114 | ` */` |
| 16750871 | 12115 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12116 |  |
|        - | 12117 | `	/* Calculate the hash based on the memory object index */` |
| 16750873 | 12118 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12119 |  |
|        - | 12120 | `/*` |
|        - | 12121 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12122 | ` * in the reference table.` |
|        - | 12123 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12124 | ` * otherwise.` |
|        - | 12125 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12126 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12127 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12128 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12129 | ` * Refer to the official for more information on this powerful` |
|        - | 12130 | ` * extension.` |
|        - | 12131 | ` */` |
|  9023340 | 12132 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12133 |  |
|        - | 12134 | `	VmRefObj *pRef;` |
|        - | 12135 | `	sxu32 nBucket;` |
|        - | 12136 | `	/* Point to the appropriate bucket */` |
|  9023342 | 12137 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12138 | `	/* Perform the lookup */` |
|  9023342 | 12139 | `	pRef = pVm->apRefObj[nBucket];` |
| 19360352 | 12140 | `	for(;;){` |
| 38711437 | 12141 | `		if( pRef == 0 ){` |
|  3104618 | 12142 | `			break;` |
|        - | 12143 | `		}` |
| 35606821 | 12144 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12145 | `			/* Entry found */` |
|  5918726 | 12146 | `			return pRef;` |
|        - | 12147 | `		}` |
|        - | 12148 | `		/* Point to the next entry */` |
| 29688097 | 12149 | `		pRef = pRef->pNextCollide;` |
|        2 | 12150 | `	}` |
|        - | 12151 | `	/* No such entry,return NULL */` |
|  3104618 | 12152 | `	return 0;` |
|  4511672 | 12153 |  |
|        - | 12154 | `/*` |
|        - | 12155 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12156 | ` *` |
|        - | 12157 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12158 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12159 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12160 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12161 | ` * Refer to the official for more information on this powerful` |
|        - | 12162 | ` * extension.` |
|        - | 12163 | ` */` |
|  3024580 | 12164 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12165 |  |
|        - | 12166 | `	sxu32 nBucket;` |
|  3024582 | 12167 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12168 | `		VmRefObj **apNew;` |
|        - | 12169 | `		sxu32 nNew;` |
|        - | 12170 | `		/* Allocate a larger table */` |
|     4392 | 12171 | `		nNew = pVm->nRefSize << 1;` |
|     4392 | 12172 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4392 | 12173 | `		if( apNew ){` |
|     4392 | 12174 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12175 | `			sxu32 n;` |
|        - | 12176 | `			/* Zero the structure */` |
|     4392 | 12177 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12178 | `			/* Rehash all referenced entries */` |
|  2844482 | 12179 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12180 | `				/* Remove old collision links */` |
|  2840092 | 12181 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12182 | `				/* Point to the appropriate bucket */` |
|  2840092 | 12183 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12184 | `				/* Insert the entry  */` |
|  2840092 | 12185 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2840092 | 12186 | `				if( apNew[nBucket] ){` |
|  2298896 | 12187 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12188 | `				}` |
|  2840092 | 12189 | `				apNew[nBucket] = pEntry;` |
|        - | 12190 | `				/* Point to the next entry */` |
|  2840092 | 12191 | `				pEntry = pEntry->pNext;` |
|  1420047 | 12192 | `			}` |
|        - | 12193 | `			/* Release the old table */` |
|     4392 | 12194 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12195 | `			/* Install the new one */` |
|     4392 | 12196 | `			pVm->apRefObj = apNew;` |
|     4392 | 12197 | `			pVm->nRefSize = nNew;` |
|     2195 | 12198 | `		}` |
|     2195 | 12199 | `	}` |
|        - | 12200 | `	/* Point to the appropriate bucket */` |
|  3024582 | 12201 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12202 | `	/* Insert the entry */` |
|  3024582 | 12203 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3024582 | 12204 | `	if( pVm->apRefObj[nBucket] ){` |
|  2507667 | 12205 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1253851 | 12206 | `	}` |
|  3024582 | 12207 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3024582 | 12208 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3024582 | 12209 | `	pVm->nRefUsed++;` |
|  3024582 | 12210 | `	return SXRET_OK;` |
|        2 | 12211 |  |
|        - | 12212 | `/*` |
|        - | 12213 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12214 | ` * the reference table.` |
|        - | 12215 | ` * This function is invoked when the user perform an unset` |
|        - | 12216 | ` * call [i.e: unset($var); ].` |
|        - | 12217 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12218 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12219 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12220 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12221 | ` * Refer to the official for more information on this powerful` |
|        - | 12222 | ` * extension.` |
|        - | 12223 | ` */` |
|  2987830 | 12224 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12225 |  |
|        - | 12226 | `	ph7_hashmap_node **apNode;` |
|        - | 12227 | `	SyHashEntry **apEntry;` |
|        - | 12228 | `	sxu32 n;` |
|        - | 12229 | `	/* Point to the reference table */` |
|  2987832 | 12230 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2987832 | 12231 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12232 | `	/* Unlink the entry from the reference table */` |
|  3073734 | 12233 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    85904 | 12234 | `		if( apEntry[n] ){` |
|    85854 | 12235 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    42926 | 12236 | `		}` |
|    42953 | 12237 | `	}` |
|  5892428 | 12238 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2904598 | 12239 | `		if( apNode[n] ){` |
|     6794 | 12240 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3396 | 12241 | `		}` |
|  1452300 | 12242 | `	}` |
|  2987832 | 12243 | `	if( pRef->pPrevCollide ){` |
|  1124971 | 12244 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   562611 | 12245 | `	}else{` |
|  1862863 | 12246 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12247 | `	}` |
|  2987832 | 12248 | `	if( pRef->pNextCollide ){` |
|  1696844 | 12249 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   848408 | 12250 | `	}` |
|  2987832 | 12251 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12252 | `	/* Release the node */` |
|  2987832 | 12253 | `	SySetRelease(&pRef->aReference);` |
|  2987832 | 12254 | `	SySetRelease(&pRef->aArrEntries);` |
|  2987832 | 12255 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2987832 | 12256 | `	pVm->nRefUsed--;` |
|  2987832 | 12257 | `	return SXRET_OK;` |
|        2 | 12258 |  |
|        - | 12259 | `/*` |
|        - | 12260 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12261 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12262 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12263 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12264 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12265 | ` * Refer to the official for more information on this powerful` |
|        - | 12266 | ` * extension.` |
|        - | 12267 | ` */` |
|  3057608 | 12268 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12269 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12270 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12271 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12272 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12273 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12274 | `	)` |
|        2 | 12275 |  |
|  3057610 | 12276 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12277 | `	VmRefObj *pRef;` |
|        - | 12278 | `	/* Check if the referenced object already exists */` |
|  3057610 | 12279 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3057610 | 12280 | `	if( pRef == 0 ){` |
|        - | 12281 | `		/* Create a new entry */` |
|  3024582 | 12282 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3024582 | 12283 | `		if( pRef == 0 ){` |
|      ! 0 | 12284 | `			return SXERR_MEM;` |
|        - | 12285 | `		}` |
|  3024582 | 12286 | `		pRef->iFlags = iFlags;` |
|        - | 12287 | `		/* Install the entry */` |
|  3024582 | 12288 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1512290 | 12289 | `	}` |
|  3057610 | 12290 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3057610 | 12291 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12292 | `		VmSlot sRef;` |
|        - | 12293 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12294 | `		 * be deleted when we leave this frame.` |
|        - | 12295 | `		 */` |
|    80122 | 12296 | `		sRef.nIdx = nIdx;` |
|    80122 | 12297 | `		sRef.pUserData = pEntry;` |
|    80122 | 12298 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12299 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12300 | `		}` |
|    40060 | 12301 | `	}` |
|  3057610 | 12302 | `	if( pEntry ){` |
|        - | 12303 | `		/* Address of the hash-entry */` |
|   112958 | 12304 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    56478 | 12305 | `	}` |
|  3057610 | 12306 | `	if( pMapEntry ){` |
|        - | 12307 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2939656 | 12308 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1469827 | 12309 | `	}` |
|  3057610 | 12310 | `	return SXRET_OK;` |
|  1528806 | 12311 |  |
|        - | 12312 | `/*` |
|        - | 12313 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12314 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12315 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12316 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12317 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12318 | ` * Refer to the official for more information on this powerful` |
|        - | 12319 | ` * extension.` |
|        - | 12320 | ` */` |
|  2977896 | 12321 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12322 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12323 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12324 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12325 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12326 | `	)` |
|        2 | 12327 |  |
|        - | 12328 | `	VmRefObj *pRef;` |
|        - | 12329 | `	sxu32 n;` |
|        - | 12330 | `	/* Check if the referenced object already exists */` |
|  2977898 | 12331 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2977898 | 12332 | `	if( pRef == 0 ){` |
|        - | 12333 | `		/* Not such entry */` |
|    80032 | 12334 | `		return SXERR_NOTFOUND;` |
|        - | 12335 | `	}` |
|        - | 12336 | `	/* Remove the desired entry */` |
|  2897868 | 12337 | `	if( pEntry ){` |
|        - | 12338 | `		SyHashEntry **apEntry;` |
|       56 | 12339 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12340 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12341 | `			if( apEntry[n] == pEntry ){` |
|        - | 12342 | `				/* Nullify the entry */` |
|       56 | 12343 | `				apEntry[n] = 0;` |
|        - | 12344 | `				/*` |
|        - | 12345 | `				 * NOTE:` |
|        - | 12346 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12347 | `				 * we avoid wasting spaces.` |
|        - | 12348 | `				 */` |
|       27 | 12349 | `			}` |
|       79 | 12350 | `		}` |
|       27 | 12351 | `	}` |
|  2897868 | 12352 | `	if( pMapEntry ){` |
|        - | 12353 | `		ph7_hashmap_node **apNode;` |
|  2897814 | 12354 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5795720 | 12355 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2897908 | 12356 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12357 | `				/* nullify the entry */` |
|  2897814 | 12358 | `				apNode[n] = 0;` |
|  1448906 | 12359 | `			}` |
|  1448955 | 12360 | `		}` |
|  1448906 | 12361 | `	}` |
|  2897868 | 12362 | `	return SXRET_OK;` |
|  1488950 | 12363 |  |
|        - | 12364 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12365 | `/*` |
|        - | 12366 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12367 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12368 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12369 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12370 | ` * For more information on how to register IO stream devices,please` |
|        - | 12371 | ` * refer to the official documentation.` |
|        - | 12372 | ` */` |
|    23780 | 12373 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12374 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12375 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12376 | `	int nByte              /* *pzDevice length*/` |
|        - | 12377 | `	)` |
|        2 | 12378 |  |
|        - | 12379 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12380 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12381 | `	SyString sDev,sCur;` |
|        - | 12382 | `	sxu32 n,nEntry;` |
|        - | 12383 | `	int rc;` |
|        - | 12384 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23782 | 12385 | `	zNext = zCur = zIn = *pzDevice;` |
|    23782 | 12386 | `	zEnd = &zIn[nByte];` |
|  1518234 | 12387 | `	while( zIn < zEnd ){` |
|  1494456 | 12388 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12389 | `			/* Got one */` |
|        3 | 12390 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12391 | `			break;` |
|        - | 12392 | `		}` |
|        - | 12393 | `		/* Advance the cursor */` |
|  1494454 | 12394 | `		zIn++;` |
|        2 | 12395 | `	}` |
|    23782 | 12396 | `	if( zIn >= zEnd ){` |
|        - | 12397 | `		/* No such scheme,return the default stream */` |
|    23780 | 12398 | `		return pVm->pDefStream;` |
|        - | 12399 | `	}` |
|        3 | 12400 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12401 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12402 | `	SyStringFullTrim(&sDev);` |
|        - | 12403 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12404 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12405 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12406 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12407 | `		pStream = apStream[n];` |
|        3 | 12408 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12409 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12410 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12411 | `		if( rc == 0 ){` |
|        - | 12412 | `			/* Stream device found */` |
|        3 | 12413 | `			*pzDevice = zNext;` |
|        3 | 12414 | `			return pStream;` |
|        - | 12415 | `		}` |
|      ! 0 | 12416 | `	}` |
|        - | 12417 | `	/* No such stream,return NULL */` |
|      ! 0 | 12418 | `	return 0;` |
|    11892 | 12419 |  |
|        - | 12420 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12421 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12422 |  |
