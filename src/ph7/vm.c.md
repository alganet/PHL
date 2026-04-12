# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5529/7179 lines (77.02%)

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
|   838208 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   838210 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   838176 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   838166 |   104 | `	return FALSE;` |
|   419128 |   105 |  |
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
|   547300 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   547302 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   547302 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   547298 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   547298 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   547298 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   547298 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   547298 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   547298 |   152 | `	pCons->xExpand = xExpand;` |
|   547298 |   153 | `	pCons->pUserData = pUserData;` |
|   547298 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   547298 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   547298 |   161 | `	return SXRET_OK;` |
|   273652 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1203270 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1203272 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1203272 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1203272 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1203272 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1203272 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1203272 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1203272 |   195 | `	pFunc->pVm   = pVm;` |
|  1203272 |   196 | `	pFunc->xFunc = xFunc;` |
|  1203272 |   197 | `	pFunc->pUserData = pUserData;` |
|  1203272 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1203272 |   200 | `	*ppOut = pFunc;` |
|  1203272 |   201 | `	return SXRET_OK;` |
|   601637 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1205792 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1205794 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1205794 |   223 | `	if( pEntry ){` |
|     2524 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2524 |   225 | `		pFunc->pUserData = pUserData;` |
|     2524 |   226 | `		pFunc->xFunc = xFunc;` |
|     2524 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2524 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1203272 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1203272 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1203272 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1203272 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1203272 |   243 | `	return SXRET_OK;` |
|   602898 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   172266 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   172268 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   172268 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   172268 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   172268 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   172268 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   172268 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   270 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   172268 |   271 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   172268 |   272 | `	pFunc->iFlags = iFlags;` |
|   172268 |   273 | `	pFunc->pUserData = pUserData;` |
|   172268 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   172268 |   275 | `	return SXRET_OK;` |
|        2 |   276 |  |
|        - |   277 | `/*` |
|        - |   278 | ` * Namespace-aware function lookup.` |
|        - |   279 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   280 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   281 | ` */` |
|        - |   282 | `/*` |
|        - |   283 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   284 | ` */` |
|   676966 |   285 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   286 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   287 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   288 | `	SyString *pName     /* Function name */` |
|        - |   289 | `	)` |
|        2 |   290 |  |
|        - |   291 | `	SyHashEntry *pEntry;` |
|        - |   292 | `	sxi32 rc;` |
|   676968 |   293 | `	if( pName == 0 ){` |
|        - |   294 | `		/* Use the built-in name */` |
|    37264 |   295 | `		pName = &pFunc->sName;` |
|    18631 |   296 | `	}` |
|        - |   297 | `	/* Check for duplicates (functions with the same name) first */` |
|   676968 |   298 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   676968 |   299 | `	if( pEntry ){` |
|   527392 |   300 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   527392 |   301 | `		if( pLink != pFunc ){` |
|        - |   302 | `			/* Link */` |
|      188 |   303 | `			pFunc->pNextName = pLink;` |
|      188 |   304 | `			pEntry->pUserData = pFunc;` |
|       93 |   305 | `		}` |
|   527392 |   306 | `		return SXRET_OK;` |
|        - |   307 | `	}` |
|        - |   308 | `	/* First time seen */` |
|   149578 |   309 | `	pFunc->pNextName = 0;` |
|   149578 |   310 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   149578 |   311 | `	return rc;` |
|   338485 |   312 |  |
|        - |   313 | `/*` |
|        - |   314 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   315 | ` */` |
|    48334 |   316 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   317 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   318 | `	ph7_class *pClass /* Target Class */` |
|        - |   319 | `	)` |
|        2 |   320 |  |
|    48336 |   321 | `	SyString *pName = &pClass->sName;` |
|        - |   322 | `	SyHashEntry *pEntry;` |
|        - |   323 | `	sxi32 rc;` |
|        - |   324 | `	/* Check for duplicates */` |
|    48336 |   325 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    48336 |   326 | `	if( pEntry ){` |
|       31 |   327 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   328 | `		/* Link entry with the same name */` |
|       31 |   329 | `		pClass->pNextName = pLink;` |
|       31 |   330 | `		pEntry->pUserData = pClass;` |
|       31 |   331 | `		return SXRET_OK;` |
|        - |   332 | `	}` |
|    48306 |   333 | `	pClass->pNextName = 0;` |
|        - |   334 | `	/* Perform a simple hashtable insertion */` |
|    48306 |   335 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    48306 |   336 | `	return rc;` |
|    24169 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Instruction builder interface.` |
|        - |   340 | ` */` |
|  3473520 |   341 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3473522 |   353 | `	sInstr.iOp = (sxu8)iOp;` |
|  3473522 |   354 | `	sInstr.iP1 = iP1;` |
|  3473522 |   355 | `	sInstr.iP2 = iP2;` |
|  3473522 |   356 | `	sInstr.p3  = p3;` |
|  3473522 |   357 | `	if( pIndex ){` |
|        - |   358 | `		/* Instruction index in the bytecode array */` |
|   200110 |   359 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   100054 |   360 | `	}` |
|        - |   361 | `	/* Finally,record the instruction */` |
|  3473522 |   362 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3473522 |   363 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   364 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   365 | `		/* Fall throw */` |
|      ! 0 |   366 | `	}` |
|  3473522 |   367 | `	return rc;` |
|        2 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Swap the current bytecode container with the given one.` |
|        - |   371 | ` */` |
|   412880 |   372 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   373 |  |
|   412882 |   374 | `	if( pContainer == 0 ){` |
|        - |   375 | `		/* Point to the default container */` |
|      ! 0 |   376 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   377 | `	}else{` |
|        - |   378 | `		/* Change container */` |
|   412882 |   379 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   380 | `	}` |
|   412882 |   381 | `	return SXRET_OK;` |
|        2 |   382 |  |
|        - |   383 | `/*` |
|        - |   384 | ` * Return the current bytecode container.` |
|        - |   385 | ` */` |
|   206440 |   386 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   387 |  |
|   206442 |   388 | `	return pVm->pByteContainer;` |
|        2 |   389 |  |
|        - |   390 | `/*` |
|        - |   391 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   392 | ` */` |
|   197230 |   393 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   394 |  |
|        - |   395 | `	VmInstr *pInstr;` |
|   197232 |   396 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   197232 |   397 | `	return pInstr;` |
|        2 |   398 |  |
|        - |   399 | `/*` |
|        - |   400 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   401 | ` */` |
|  1040960 |   402 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   403 |  |
|  1040962 |   404 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Pop the last VM instruction.` |
|        - |   408 | ` */` |
|   187750 |   409 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   410 |  |
|   187752 |   411 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   412 |  |
|        - |   413 | `/*` |
|        - |   414 | ` * Peek the last VM instruction.` |
|        - |   415 | ` */` |
|   673498 |   416 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   417 |  |
|   673500 |   418 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   419 |  |
|    29124 |   420 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   421 |  |
|        - |   422 | `	VmInstr *aInstr;` |
|        - |   423 | `	sxu32 n;` |
|    29126 |   424 | `	n = SySetUsed(pVm->pByteContainer);` |
|    29126 |   425 | `	if( n < 2 ){` |
|      ! 0 |   426 | `		return 0;` |
|        - |   427 | `	}` |
|    29126 |   428 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    29126 |   429 | `	return &aInstr[n - 2];` |
|    14564 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Allocate a new virtual machine frame.` |
|        - |   433 | ` */` |
|    17828 |   434 | `static VmFrame * VmNewFrame(` |
|        - |   435 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   436 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   437 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   438 | `	)` |
|        2 |   439 |  |
|        - |   440 | `	VmFrame *pFrame;` |
|        - |   441 | `	/* Allocate a new vm frame */` |
|    17830 |   442 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    17830 |   443 | `	if( pFrame == 0 ){` |
|      ! 0 |   444 | `		return 0;` |
|        - |   445 | `	}` |
|        - |   446 | `	/* Zero the structure */` |
|    17830 |   447 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   448 | `	/* Initialize frame fields */` |
|    17830 |   449 | `	pFrame->pUserData = pUserData;` |
|    17830 |   450 | `	pFrame->pThis = pThis;` |
|    17830 |   451 | `	pFrame->pVm = pVm;` |
|    17830 |   452 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    17830 |   453 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    17830 |   454 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    17830 |   455 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    17830 |   456 | `	return pFrame;` |
|     8916 |   457 |  |
|        - |   458 | `/* Forward declaration */` |
|        - |   459 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   460 | `/*` |
|        - |   461 | ` * Enter a VM frame.` |
|        - |   462 | ` */` |
|    17786 |   463 | `static sxi32 VmEnterFrame(` |
|        - |   464 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   465 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   466 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   467 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   468 | `	)` |
|        2 |   469 |  |
|        - |   470 | `	VmFrame *pFrame;` |
|        - |   471 | `	/* Allocate a new frame */` |
|    17788 |   472 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    17788 |   473 | `	if( pFrame == 0 ){` |
|      ! 0 |   474 | `		return SXERR_MEM;` |
|        - |   475 | `	}` |
|        - |   476 | `	/* Link to the list of active VM frame */` |
|    17788 |   477 | `	pFrame->pParent = pVm->pFrame;` |
|    17788 |   478 | `	pVm->pFrame = pFrame;` |
|    17788 |   479 | `	if( ppFrame ){` |
|        - |   480 | `		/* Write a pointer to the new VM frame */` |
|    14986 |   481 | `		*ppFrame = pFrame;` |
|     7492 |   482 | `	}` |
|    17788 |   483 | `	return SXRET_OK;` |
|     8895 |   484 |  |
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
|    14984 |   528 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   529 |  |
|    14986 |   530 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    14986 |   531 | `	if( pCurFrame ){` |
|        - |   532 | `		/* Unlink from the list of active VM frame */` |
|    14986 |   533 | `		pVm->pFrame = pCurFrame->pParent;` |
|    14986 |   534 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   535 | `			VmSlot  *aSlot;` |
|        - |   536 | `			sxu32 n;` |
|        - |   537 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    14828 |   538 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   101390 |   539 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   540 | `				/* Unset the local variable */` |
|    86564 |   541 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    43283 |   542 | `			}` |
|        - |   543 | `			/* Remove local reference */` |
|    14828 |   544 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   101446 |   545 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    86620 |   546 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    43311 |   547 | `			}` |
|     7413 |   548 | `		}` |
|        - |   549 | `		/* Release internal containers */` |
|    14986 |   550 | `		SyHashRelease(&pCurFrame->hVar);` |
|    14986 |   551 | `		SySetRelease(&pCurFrame->sArg);` |
|    14986 |   552 | `		SySetRelease(&pCurFrame->sLocal);` |
|    14986 |   553 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   554 | `		/* Release the whole structure */` |
|    14986 |   555 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     7492 |   556 | `	}` |
|    14986 |   557 |  |
|        - |   558 | `/*` |
|        - |   559 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   560 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   561 | ` * should be skipped when looking for the real execution context.` |
|        - |   562 | ` */` |
|  6632538 |   563 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   564 |  |
|  6633402 |   565 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      864 |   566 | `		pFrame = pFrame->pParent;` |
|        2 |   567 | `	}` |
|  6632540 |   568 | `	return pFrame;` |
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
|   132670 |   688 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   689 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   690 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   691 | `	)` |
|        2 |   692 |  |
|        - |   693 | `	ph7_class_method *pMeth;` |
|        - |   694 | `	ph7_class_attr *pAttr;` |
|        - |   695 | `	SyHashEntry *pEntry;` |
|        - |   696 | `	sxi32 rc;` |
|        - |   697 | `	/* Reset the loop cursor */` |
|   132672 |   698 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   699 | `	/* Process only static and constant attribute */` |
|   558769 |   700 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   701 | `		/* Extract the current attribute */` |
|   359764 |   702 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   359764 |   703 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|   132672 |   748 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   749 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   750 | `		 */` |
|    57578 |   751 | `		return SXRET_OK;` |
|        - |   752 | `	}` |
|        - |   753 | `	/* Create constructor alias if not yet done */` |
|    75096 |   754 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   755 | `		/* User constructor with the same base class name */` |
|     5800 |   756 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5800 |   757 | `		if( pEntry ){` |
|      ! 0 |   758 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   759 | `			/* Create the alias */` |
|      ! 0 |   760 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   761 | `		}` |
|     2899 |   762 | `	}` |
|        - |   763 | `	/* Install the methods now */` |
|    75096 |   764 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   752355 |   765 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   639714 |   766 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   639714 |   767 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   639706 |   768 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   639706 |   769 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   770 | `				return rc;` |
|        - |   771 | `			}` |
|   319852 |   772 | `		}` |
|        2 |   773 | `	}` |
|        - |   774 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    75096 |   775 | `	pClass->bMounted = TRUE;` |
|    75096 |   776 | `	return SXRET_OK;` |
|    66337 |   777 |  |
|        - |   778 | `/*` |
|        - |   779 | ` * Allocate a private frame for attributes of the given` |
|        - |   780 | ` * class instance (Object in the PHP jargon).` |
|        - |   781 | ` */` |
|     1464 |   782 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   783 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   784 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   785 | `	)` |
|        2 |   786 |  |
|     1466 |   787 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   788 | `	ph7_class_attr *pAttr;` |
|        - |   789 | `	SyHashEntry *pEntry;` |
|        - |   790 | `	sxi32 rc;` |
|        - |   791 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1466 |   792 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     5954 |   793 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   794 | `		VmClassAttr *pVmAttr;` |
|        - |   795 | `		/* Extract the current attribute */` |
|     4490 |   796 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     4490 |   797 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     4490 |   798 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   799 | `			return SXERR_MEM;` |
|        - |   800 | `		}` |
|     4490 |   801 | `		pVmAttr->pAttr = pAttr;` |
|     4490 |   802 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   803 | `			ph7_value *pMemObj;` |
|        - |   804 | `			/* Reserve a memory object for this attribute */` |
|     4466 |   805 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     4466 |   806 | `			if( pMemObj == 0 ){` |
|      ! 0 |   807 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   808 | `				return SXERR_MEM;` |
|        - |   809 | `			}` |
|     4466 |   810 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     4466 |   811 | `			pVmAttr->iState = 0;` |
|     4466 |   812 | `			pVmAttr->pOwner = pClass;` |
|     4466 |   813 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   814 | `				/* Initialize attribute default value (any complex expression) */` |
|     1524 |   815 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     3705 |   816 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   817 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   818 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       28 |   819 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       13 |   820 | `			}` |
|     4466 |   821 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     4466 |   822 | `			if( rc != SXRET_OK ){` |
|        - |   823 | `				VmSlot sSlot;` |
|        - |   824 | `				/* Restore memory object */` |
|      ! 0 |   825 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   826 | `				sSlot.pUserData = 0;` |
|      ! 0 |   827 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|        - |   831 | `			/* Install attribute in the reference table */` |
|     4466 |   832 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   833 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   834 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   835 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     4466 |   836 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|     2234 |   848 | `		}else{` |
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
|     1466 |   860 | `	return SXRET_OK;` |
|      734 |   861 |  |
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
|   398598 |   873 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   874 |  |
|        - |   875 | `	ph7_value *pObj;` |
|        - |   876 | `	sxi32 rc;` |
|   398600 |   877 | `	if( pIndex ){` |
|        - |   878 | `		/* Object index in the object table */` |
|   390194 |   879 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   195096 |   880 | `	}` |
|        - |   881 | `	/* Reserve a slot for the new object */` |
|   398600 |   882 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   398600 |   883 | `	if( rc != SXRET_OK ){` |
|        - |   884 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   885 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   886 | `		 */` |
|      ! 0 |   887 | `		return 0;` |
|        - |   888 | `	}` |
|   398600 |   889 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   398600 |   890 | `	return pObj;` |
|   199301 |   891 |  |
|        - |   892 | `/*` |
|        - |   893 | ` * Reserve a memory object.` |
|        - |   894 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   895 | ` */` |
|  2146126 |   896 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   897 |  |
|        - |   898 | `	ph7_value *pObj;` |
|        - |   899 | `	sxi32 rc;` |
|  2146128 |   900 | `	if( pIndex ){` |
|        - |   901 | `		/* Object index in the object table */` |
|  2146128 |   902 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1073063 |   903 | `	}` |
|        - |   904 | `	/* Reserve a slot for the new object */` |
|  2146128 |   905 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2146128 |   906 | `	if( rc != SXRET_OK ){` |
|        - |   907 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   908 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   909 | `		 */` |
|      ! 0 |   910 | `		return 0;` |
|        - |   911 | `	}` |
|  2146128 |   912 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2146128 |   913 | `	return pObj;` |
|  1073065 |   914 |  |
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
|        - |   933 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   934 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   935 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   936 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   937 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   938 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   939 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   940 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   941 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   942 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   943 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   944 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   945 | `/*` |
|        - |   946 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   947 | ` * directly as foreign functions.` |
|        - |   948 | ` */` |
|        - |   949 | `#define PH7_BUILTIN_LIB \` |
|        - |   950 | `	"class Exception { "\` |
|        - |   951 | `    "protected $message = 'Unknown exception';"\` |
|        - |   952 | `    "protected $code = 0;"\` |
|        - |   953 | `    "protected $file;"\` |
|        - |   954 | `    "protected $line;"\` |
|        - |   955 | `    "protected $trace;"\` |
|        - |   956 | `    "protected $previous;"\` |
|        - |   957 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   958 | `	"   if( isset($message) ){"\` |
|        - |   959 | `	"	  $this->message = $message;"\` |
|        - |   960 | `	"   }"\` |
|        - |   961 | `	"   $this->code = $code;"\` |
|        - |   962 | `	"   $this->file = __FILE__;"\` |
|        - |   963 | `	"   $this->line = __LINE__;"\` |
|        - |   964 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   965 | `	"   if( isset($previous) ){"\` |
|        - |   966 | `	"     $this->previous = $previous;"\` |
|        - |   967 | `	"   }"\` |
|        - |   968 | `	"}"\` |
|        - |   969 | `	"public function getMessage(){"\` |
|        - |   970 | `	"   return $this->message;"\` |
|        - |   971 | `	"}"\` |
|        - |   972 | `	" public function getCode(){"\` |
|        - |   973 | `	"  return $this->code;"\` |
|        - |   974 | `	"}"\` |
|        - |   975 | `	"public function getFile(){"\` |
|        - |   976 | `	"  return $this->file;"\` |
|        - |   977 | `	"}"\` |
|        - |   978 | `	"public function getLine(){"\` |
|        - |   979 | `	"  return $this->line;"\` |
|        - |   980 | `	"}"\` |
|        - |   981 | `	"public function getTrace(){"\` |
|        - |   982 | `	"   return $this->trace;"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"public function getTraceAsString(){"\` |
|        - |   985 | `	"  return debug_string_backtrace();"\` |
|        - |   986 | `	"}"\` |
|        - |   987 | `	"public function getPrevious(){"\` |
|        - |   988 | `	"    return $this->previous;"\` |
|        - |   989 | `	"}"\` |
|        - |   990 | `	"public function __toString(){"\` |
|        - |   991 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   992 | `    "}"\` |
|        - |   993 | `	"}"\` |
|        - |   994 | `	"class Error extends Exception { }"\` |
|        - |   995 | `	"class TypeError extends Error { }"\` |
|        - |   996 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   997 | `	"class ValueError extends Error { }"\` |
|        - |   998 | `	"class FiberError extends Error { }"\` |
|        - |   999 | `	"class AssertionError extends Error { }"\` |
|        - |  1000 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1001 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1002 | `	"class ErrorException extends Exception { "\` |
|        - |  1003 | `	"protected $severity;"\` |
|        - |  1004 | `	"public function __construct(string $message = null,"\` |
|        - |  1005 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |  1006 | `	"   if( isset($message) ){"\` |
|        - |  1007 | `	"	  $this->message = $message;"\` |
|        - |  1008 | `	"   }"\` |
|        - |  1009 | `	"   $this->severity = $severity;"\` |
|        - |  1010 | `	"   $this->code = $code;"\` |
|        - |  1011 | `	"   $this->file = $filename;"\` |
|        - |  1012 | `	"   $this->line = $lineno;"\` |
|        - |  1013 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1014 | `	"   if( isset($previous) ){"\` |
|        - |  1015 | `	"     $this->previous = $previous;"\` |
|        - |  1016 | `	"   }"\` |
|        - |  1017 | `	"}"\` |
|        - |  1018 | `	"public function getSeverity(){"\` |
|        - |  1019 | `	"   return $this->severity;"\` |
|        - |  1020 | `    "}"\` |
|        - |  1021 | `	"}"\` |
|        - |  1022 | `	"interface Iterator {"\` |
|        - |  1023 | `	"public function current();"\` |
|        - |  1024 | `	"public function key();"\` |
|        - |  1025 | `	"public function next();"\` |
|        - |  1026 | `	"public function rewind();"\` |
|        - |  1027 | `	"public function valid();"\` |
|        - |  1028 | `	"}"\` |
|        - |  1029 | `	"interface IteratorAggregate {"\` |
|        - |  1030 | `	"public function getIterator();"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"interface Serializable {"\` |
|        - |  1033 | `	"public function serialize();"\` |
|        - |  1034 | `	"public function unserialize(string $serialized);"\` |
|        - |  1035 | `	"}"\` |
|        - |  1036 | `	"/* Directory releated IO */"\` |
|        - |  1037 | `	"class Directory {"\` |
|        - |  1038 | `	"public $handle = null;"\` |
|        - |  1039 | `	"public $path  = null;"\` |
|        - |  1040 | `	"public function __construct(string $path)"\` |
|        - |  1041 | `	"{"\` |
|        - |  1042 | `	"   $this->handle = opendir($path);"\` |
|        - |  1043 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1044 | `	"      $this->path = $path;"\` |
|        - |  1045 | `	"   }"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"public function __destruct()"\` |
|        - |  1048 | `	"{"\` |
|        - |  1049 | `	"  if( $this->handle != null ){"\` |
|        - |  1050 | `	"       closedir($this->handle);"\` |
|        - |  1051 | `	"  }"\` |
|        - |  1052 | `	"}"\` |
|        - |  1053 | `	"public function read()"\` |
|        - |  1054 | `	"{"\` |
|        - |  1055 | `	"    return readdir($this->handle);"\` |
|        - |  1056 | `	"}"\` |
|        - |  1057 | `	"public function rewind()"\` |
|        - |  1058 | `	"{"\` |
|        - |  1059 | `	"    rewinddir($this->handle);"\` |
|        - |  1060 | `	"}"\` |
|        - |  1061 | `	"public function close()"\` |
|        - |  1062 | `	"{"\` |
|        - |  1063 | `	"    closedir($this->handle);"\` |
|        - |  1064 | `	"    $this->handle = null;"\` |
|        - |  1065 | `	"}"\` |
|        - |  1066 | `	"}"\` |
|        - |  1067 | `	"class Fiber {"\` |
|        - |  1068 | `	"  private $__ctx;"\` |
|        - |  1069 | `	"  private $__callable;"\` |
|        - |  1070 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1071 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1072 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1073 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1074 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1075 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1076 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1077 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1078 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1079 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1080 | `	"}"\` |
|        - |  1081 | `	"class Generator implements Iterator {"\` |
|        - |  1082 | `	"  private $__ctx;"\` |
|        - |  1083 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1084 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1085 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1086 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1087 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1088 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1089 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1090 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1091 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1092 | `	"}"\` |
|        - |  1093 | `	"class stdClass{"\` |
|        - |  1094 | `	"  public $value;"\` |
|        - |  1095 | `	" /* Magic methods */"\` |
|        - |  1096 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1097 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1098 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1099 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1100 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1101 | `	"}"\` |
|        - |  1102 | `	"function dir(string $path){"\` |
|        - |  1103 | `	"   return new Directory($path);"\` |
|        - |  1104 | `	"}"\` |
|        - |  1105 | `	"function Dir(string $path){"\` |
|        - |  1106 | `	"   return new Directory($path);"\` |
|        - |  1107 | `	"}"\` |
|        - |  1108 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1109 | `    "{"\` |
|        - |  1110 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1111 | `	"  $aDir = array();"\` |
|        - |  1112 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1113 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1114 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1115 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1116 | `	"   }"\` |
|        - |  1117 | `	"  closedir($pHandle);"\` |
|        - |  1118 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1119 | `	"      rsort($aDir);"\` |
|        - |  1120 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1121 | `	"      sort($aDir);"\` |
|        - |  1122 | `	"  }"\` |
|        - |  1123 | `	"  return $aDir;"\` |
|        - |  1124 | `	"}"\` |
|        - |  1125 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1126 | `	"/* Open the target directory */"\` |
|        - |  1127 | `	"$zDir = dirname($pattern);"\` |
|        - |  1128 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1129 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1130 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1131 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1132 | `	"	return FALSE;"\` |
|        - |  1133 | `	"}"\` |
|        - |  1134 | `	"$pattern = basename($pattern);"\` |
|        - |  1135 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1136 | `	"/* Loop throw available entries */"\` |
|        - |  1137 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1138 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1139 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1140 | `	"	if( $rc ){"\` |
|        - |  1141 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1142 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1143 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1144 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1145 | `	"		  }"\` |
|        - |  1146 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1147 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1148 | `	"		 continue;"\` |
|        - |  1149 | `	"	   }"\` |
|        - |  1150 | `	"	   /* Add the entry */"\` |
|        - |  1151 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1152 | `	"	}"\` |
|        - |  1153 | `	" }"\` |
|        - |  1154 | `	"/* Close the handle */"\` |
|        - |  1155 | `	"closedir($pHandle);"\` |
|        - |  1156 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1157 | `	"  /* Sort the array */"\` |
|        - |  1158 | `	"  sort($pArray);"\` |
|        - |  1159 | `	"}"\` |
|        - |  1160 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1161 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1162 | `	"  $pArray[] = $pattern;"\` |
|        - |  1163 | `	"}"\` |
|        - |  1164 | `	"/* Return the created array */"\` |
|        - |  1165 | `	"return $pArray;"\` |
|        - |  1166 | `   "}"\` |
|        - |  1167 | `   "/* Creates a temporary file */"\` |
|        - |  1168 | `   "function tmpfile(){"\` |
|        - |  1169 | `   "  /* Extract the temp directory */"\` |
|        - |  1170 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1171 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1172 | `   "    /* Use the current dir */"\` |
|        - |  1173 | `   "    $zTempDir = '.';"\` |
|        - |  1174 | `   "  }"\` |
|        - |  1175 | `   "  /* Create the file */"\` |
|        - |  1176 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1177 | `   "  return $pHandle;"\` |
|        - |  1178 | `   "}"\` |
|        - |  1179 | `   "/* Creates a temporary filename */"\` |
|        - |  1180 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1181 | `   "{"\` |
|        - |  1182 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1183 | `   "}"\` |
|        - |  1184 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1185 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1186 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1187 | `   "/* Copy arguments */"\` |
|        - |  1188 | `   "$nArgs = func_num_args();"\` |
|        - |  1189 | `   "$pNew = array();"\` |
|        - |  1190 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1191 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1192 | `    "}"\` |
|        - |  1193 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1194 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1195 | `	"/* Erase */"\` |
|        - |  1196 | `	"array_erase($pArray);"\` |
|        - |  1197 | `	"/* Unshift */"\` |
|        - |  1198 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1199 | `	"return sizeof($pArray);"\` |
|        - |  1200 | `    "}"\` |
|        - |  1201 | `	"function array_merge_recursive(){"\` |
|        - |  1202 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1203 | `    "$arrays = func_get_args();"\` |
|        - |  1204 | `    "$narrays = count($arrays);"\` |
|        - |  1205 | `    "$ret = array();"\` |
|        - |  1206 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1207 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1208 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1209 | `	 " }"\` |
|        - |  1210 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1211 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1212 | `     "  if( $keyIsInt ) {"\` |
|        - |  1213 | `     "   $ret[] = $value;"\` |
|        - |  1214 | `     "  } else {"\` |
|        - |  1215 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1216 | `     "    $cur = $ret[$key];"\` |
|        - |  1217 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1218 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1219 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1220 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1221 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1222 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1223 | `     "    } else {"\` |
|        - |  1224 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1225 | `     "    }"\` |
|        - |  1226 | `     "   } else {"\` |
|        - |  1227 | `     "    $ret[$key] = $value;"\` |
|        - |  1228 | `     "   }"\` |
|        - |  1229 | `     "  }"\` |
|        - |  1230 | `     " }"\` |
|        - |  1231 | `	 " }"\` |
|        - |  1232 | `	 " return $ret;"\` |
|        - |  1233 | `    "}"\` |
|        - |  1234 | `	"function max(){"\` |
|        - |  1235 | `    "  $pArgs = func_get_args();"\` |
|        - |  1236 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1237 | `	"  return null;"\` |
|        - |  1238 | `    " }"\` |
|        - |  1239 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1240 | `    " $pArg = $pArgs[0];"\` |
|        - |  1241 | `	" if( !is_array($pArg) ){"\` |
|        - |  1242 | `	"   return $pArg; "\` |
|        - |  1243 | `	" }"\` |
|        - |  1244 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1245 | `	"   return null;"\` |
|        - |  1246 | `	" }"\` |
|        - |  1247 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1248 | `	" reset($pArg);"\` |
|        - |  1249 | `	" $max = current($pArg);"\` |
|        - |  1250 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1251 | `	"   if( $val > $max ){"\` |
|        - |  1252 | `	"     $max = $val;"\` |
|        - |  1253 | `    " }"\` |
|        - |  1254 | `	" }"\` |
|        - |  1255 | `	" return $max;"\` |
|        - |  1256 | `    " }"\` |
|        - |  1257 | `    " $max = $pArgs[0];"\` |
|        - |  1258 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1259 | `    " $val = $pArgs[$i];"\` |
|        - |  1260 | `	"if( $val > $max ){"\` |
|        - |  1261 | `	" $max = $val;"\` |
|        - |  1262 | `	"}"\` |
|        - |  1263 | `    " }"\` |
|        - |  1264 | `	" return $max;"\` |
|        - |  1265 | `    "}"\` |
|        - |  1266 | `	"function min(){"\` |
|        - |  1267 | `    "  $pArgs = func_get_args();"\` |
|        - |  1268 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1269 | `	"  return null;"\` |
|        - |  1270 | `    " }"\` |
|        - |  1271 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1272 | `    " $pArg = $pArgs[0];"\` |
|        - |  1273 | `	" if( !is_array($pArg) ){"\` |
|        - |  1274 | `	"   return $pArg; "\` |
|        - |  1275 | `	" }"\` |
|        - |  1276 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1277 | `	"   return null;"\` |
|        - |  1278 | `	" }"\` |
|        - |  1279 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1280 | `	" reset($pArg);"\` |
|        - |  1281 | `	" $min = current($pArg);"\` |
|        - |  1282 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1283 | `	"   if( $val < $min ){"\` |
|        - |  1284 | `	"     $min = $val;"\` |
|        - |  1285 | `    " }"\` |
|        - |  1286 | `	" }"\` |
|        - |  1287 | `	" return $min;"\` |
|        - |  1288 | `    " }"\` |
|        - |  1289 | `    " $min = $pArgs[0];"\` |
|        - |  1290 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1291 | `    " $val = $pArgs[$i];"\` |
|        - |  1292 | `	"if( $val < $min ){"\` |
|        - |  1293 | `	" $min = $val;"\` |
|        - |  1294 | `	" }"\` |
|        - |  1295 | `    " }"\` |
|        - |  1296 | `	" return $min;"\` |
|        - |  1297 | `	"}"\` |
|        - |  1298 | `	"function fileowner(string $file){"\` |
|        - |  1299 | `    " $a = stat($file);"\` |
|        - |  1300 | `	" if( !is_array($a) ){"\` |
|        - |  1301 | `	"	return false;"\` |
|        - |  1302 | `	" }"\` |
|        - |  1303 | `	" return $a['uid'];"\` |
|        - |  1304 | `    "}"\` |
|        - |  1305 | `    "function filegroup(string $file){"\` |
|        - |  1306 | `	" $a = stat($file);"\` |
|        - |  1307 | `	" if( !is_array($a) ){"\` |
|        - |  1308 | `	"	return false;"\` |
|        - |  1309 | `	" }"\` |
|        - |  1310 | `	" return $a['gid'];"\` |
|        - |  1311 | `    "}"\` |
|        - |  1312 | `	 "function fileinode(string $file){"\` |
|        - |  1313 | `	" $a = stat($file);"\` |
|        - |  1314 | `	" if( !is_array($a) ){"\` |
|        - |  1315 | `	"	return false;"\` |
|        - |  1316 | `	" }"\` |
|        - |  1317 | `	" return $a['ino'];"\` |
|        - |  1318 | `    "}"` |
|        - |  1319 |  |
|        - |  1320 | `/*` |
|        - |  1321 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1322 | ` * start compiling the target PHP program.` |
|        - |  1323 | ` */` |
|     2802 |  1324 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1325 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1326 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1327 | `	 )` |
|        2 |  1328 |  |
|        - |  1329 | `	SyString sBuiltin;` |
|        - |  1330 | `	ph7_value *pObj;` |
|        - |  1331 | `	sxi32 rc;` |
|        - |  1332 | `	/* Zero the structure */` |
|     2804 |  1333 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1334 | `	/* Initialize VM fields */` |
|     2804 |  1335 | `	pVm->pEngine = &(*pEngine);` |
|     2804 |  1336 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1337 | `	/* Instructions containers */` |
|     2804 |  1338 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2804 |  1339 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2804 |  1340 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1341 | `	/* Object containers */` |
|     2804 |  1342 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2804 |  1343 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1344 | `	/* Virtual machine internal containers */` |
|     2804 |  1345 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2804 |  1346 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2804 |  1347 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2804 |  1348 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2804 |  1349 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2804 |  1350 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2804 |  1351 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2804 |  1352 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2804 |  1353 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2804 |  1354 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2804 |  1355 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2804 |  1356 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2804 |  1357 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2804 |  1358 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2804 |  1359 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2804 |  1360 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2804 |  1361 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2804 |  1362 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2804 |  1363 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2804 |  1364 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2804 |  1365 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2804 |  1366 | `	pVm->pPendingException = 0;` |
|        - |  1367 | `	/* Configuration containers */` |
|     2804 |  1368 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2804 |  1369 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2804 |  1370 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2804 |  1371 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2804 |  1372 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2804 |  1373 | `	pVm->iResponseStatus = 200;` |
|     2804 |  1374 | `	pVm->bHeadersSent = 0;` |
|     2804 |  1375 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1376 | `	/* Error callbacks containers */` |
|     2804 |  1377 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2804 |  1378 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2804 |  1379 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2804 |  1380 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2804 |  1381 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1382 | `	/* Set a default recursion limit */` |
|        - |  1383 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2804 |  1384 | `	pVm->nMaxDepth = 32;` |
|        - |  1385 | `#else` |
|        - |  1386 | `	pVm->nMaxDepth = 16;` |
|        - |  1387 | `#endif` |
|        - |  1388 | `	/* Default assertion flags */` |
|     2804 |  1389 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1390 | `	/* JSON return status */` |
|     2804 |  1391 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1392 | `	/* PRNG context */` |
|     2804 |  1393 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1394 | `	/* Install the null constant */` |
|     2804 |  1395 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2804 |  1396 | `	if( pObj == 0 ){` |
|      ! 0 |  1397 | `		rc = SXERR_MEM;` |
|      ! 0 |  1398 | `		goto Err;` |
|        - |  1399 | `	}` |
|     2804 |  1400 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1401 | `	/* Install the boolean TRUE constant */` |
|     2804 |  1402 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2804 |  1403 | `	if( pObj == 0 ){` |
|      ! 0 |  1404 | `		rc = SXERR_MEM;` |
|      ! 0 |  1405 | `		goto Err;` |
|        - |  1406 | `	}` |
|     2804 |  1407 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1408 | `	/* Install the boolean FALSE constant */` |
|     2804 |  1409 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2804 |  1410 | `	if( pObj == 0 ){` |
|      ! 0 |  1411 | `		rc = SXERR_MEM;` |
|      ! 0 |  1412 | `		goto Err;` |
|        - |  1413 | `	}` |
|     2804 |  1414 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1415 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1416 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1417 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2804 |  1418 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2804 |  1419 | `	if( pObj == 0 ){` |
|      ! 0 |  1420 | `		rc = SXERR_MEM;` |
|      ! 0 |  1421 | `		goto Err;` |
|        - |  1422 | `	}` |
|     2804 |  1423 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1424 | `	/* Create the global frame */` |
|     2804 |  1425 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2804 |  1426 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1427 | `		goto Err;` |
|        - |  1428 | `	}` |
|        - |  1429 | `	/* Initialize the code generator */` |
|     2804 |  1430 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2804 |  1431 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1432 | `		goto Err;` |
|        - |  1433 | `	}` |
|        - |  1434 | `	/* VM correctly initialized,set the magic number */` |
|     2804 |  1435 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2804 |  1436 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1437 | `	/* Compile the built-in library */` |
|     2804 |  1438 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1439 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2804 |  1440 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1441 | `	/* Register Fiber internal C functions */` |
|     2804 |  1442 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2804 |  1443 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2804 |  1444 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2804 |  1445 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2804 |  1446 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2804 |  1447 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2804 |  1448 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2804 |  1449 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2804 |  1450 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2804 |  1451 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1452 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2804 |  1453 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2804 |  1454 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2804 |  1455 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2804 |  1456 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2804 |  1457 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2804 |  1458 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2804 |  1459 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2804 |  1460 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2804 |  1461 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2804 |  1462 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1463 | `	/* Reset the code generator */` |
|     2804 |  1464 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2804 |  1465 | `	return SXRET_OK;` |
|      ! 0 |  1466 | `Err:` |
|      ! 0 |  1467 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1468 | `	return rc;` |
|     1403 |  1469 |  |
|        - |  1470 | `/*` |
|        - |  1471 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1472 | ` * routine which store the output in an internal blob.` |
|        - |  1473 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1474 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1475 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1476 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1477 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1478 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1479 | ` * to finish executing and extracting the output.` |
|        - |  1480 | ` */` |
|       38 |  1481 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1482 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1483 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1484 | `	void *pUserData     /* User private data */` |
|        - |  1485 | `	)` |
|      ! 0 |  1486 |  |
|        - |  1487 | `	 sxi32 rc;` |
|        - |  1488 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1489 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1490 | `	 return rc;` |
|      ! 0 |  1491 |  |
|        - |  1492 | `/*` |
|        - |  1493 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1494 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1495 | ` */` |
|    15780 |  1496 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1497 |  |
|    15782 |  1498 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    15782 |  1499 | `	if( xCons != VmObConsumer ){` |
|     6864 |  1500 | `		pVm->nOutputLen += nLen;` |
|     6864 |  1501 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      882 |  1502 | `			pVm->bHeadersSent = 1;` |
|      440 |  1503 | `		}` |
|     3431 |  1504 | `	}` |
|    15782 |  1505 |  |
|        - |  1506 | `#define VM_STACK_GUARD 16` |
|        - |  1507 | `/*` |
|        - |  1508 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1509 | ` * our compiled PHP program.` |
|        - |  1510 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1511 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1512 | ` */` |
|    36172 |  1513 | `static ph7_value * VmNewOperandStack(` |
|        - |  1514 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1515 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1516 | `	)` |
|        2 |  1517 |  |
|        - |  1518 | `	ph7_value *pStack;` |
|        - |  1519 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1520 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1521 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1522 | `  ** on the maximum stack depth required.` |
|        - |  1523 | `  **` |
|        - |  1524 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1525 | `  */` |
|    36174 |  1526 | `	nInstr += VM_STACK_GUARD;` |
|    36174 |  1527 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    36174 |  1528 | `	if( pStack == 0 ){` |
|      ! 0 |  1529 | `		return 0;` |
|        - |  1530 | `	}` |
|        - |  1531 | `	/* Initialize the operand stack */` |
|  2329828 |  1532 | `	while( nInstr > 0 ){` |
|  2293656 |  1533 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2293656 |  1534 | `		--nInstr;` |
|        2 |  1535 | `	}` |
|        - |  1536 | `	/* Ready for bytecode execution */` |
|    36174 |  1537 | `	return pStack;` |
|    18088 |  1538 |  |
|        - |  1539 | `/* Forward declaration */` |
|        - |  1540 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1541 | `/*` |
|        - |  1542 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1543 | ` * This routine gets called by the PH7 engine after` |
|        - |  1544 | ` * successful compilation of the target PHP program.` |
|        - |  1545 | ` */` |
|     2522 |  1546 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1547 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1548 | `	)` |
|        2 |  1549 |  |
|        - |  1550 | `	SyHashEntry *pEntry;` |
|        - |  1551 | `	sxi32 rc;` |
|     2524 |  1552 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1553 | `		/* Initialize your VM first */` |
|      ! 0 |  1554 | `		return SXERR_CORRUPT;` |
|        - |  1555 | `	}` |
|        - |  1556 | `	/* Mark the VM ready for byte-code execution */` |
|     2524 |  1557 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1558 | `	/* Release the code generator now we have compiled our program */` |
|     2524 |  1559 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1560 | `	/* Emit the DONE instruction */` |
|     2524 |  1561 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2524 |  1562 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1563 | `		return SXERR_MEM;` |
|        - |  1564 | `	}` |
|        - |  1565 | `	/* Script return value */` |
|     2524 |  1566 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1567 | `	/* Allocate a new operand stack */` |
|     2524 |  1568 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2524 |  1569 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1570 | `		return SXERR_MEM;` |
|        - |  1571 | `	}` |
|        - |  1572 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1573 | `	 * private data. */` |
|     2524 |  1574 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2524 |  1575 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1576 | `	/* Allocate the reference table */` |
|     2524 |  1577 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2524 |  1578 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2524 |  1579 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1580 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1581 | `		return SXERR_MEM;` |
|        - |  1582 | `	}` |
|        - |  1583 | `	/* Zero the reference table */` |
|     2524 |  1584 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1585 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2524 |  1586 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2524 |  1587 | `	if( rc != SXRET_OK ){` |
|        - |  1588 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1589 | `		return rc;` |
|        - |  1590 | `	}` |
|        - |  1591 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2524 |  1592 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2524 |  1593 | `	if( rc != SXRET_OK ){` |
|        - |  1594 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1595 | `		return rc;` |
|        - |  1596 | `	}` |
|        - |  1597 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2524 |  1598 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1599 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2524 |  1600 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1601 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2524 |  1602 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1603 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1604 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2524 |  1605 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2524 |  1606 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1607 | `#endif` |
|        - |  1608 | `	/* Initialize and install static and constants class attributes */` |
|     2524 |  1609 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    45644 |  1610 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    43122 |  1611 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    43122 |  1612 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1613 | `			return rc;` |
|        - |  1614 | `		}` |
|        2 |  1615 | `	}` |
|        - |  1616 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2524 |  1617 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1618 | `	/* VM is ready for bytecode execution */` |
|     2524 |  1619 | `	return SXRET_OK;` |
|     1263 |  1620 |  |
|        - |  1621 | `/*` |
|        - |  1622 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1623 | ` */` |
|      ! 0 |  1624 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1625 |  |
|      ! 0 |  1626 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1627 | `		return SXERR_CORRUPT;` |
|        - |  1628 | `	}` |
|        - |  1629 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1630 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1631 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1632 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1633 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1634 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1635 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1636 | `	pVm->bHttpContext = 0;` |
|        - |  1637 | `	/* Set the ready flag */` |
|      ! 0 |  1638 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1639 | `	return SXRET_OK;` |
|      ! 0 |  1640 |  |
|        - |  1641 | `/*` |
|        - |  1642 | ` * Release a Virtual Machine.` |
|        - |  1643 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1644 | ` */` |
|     2514 |  1645 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1646 |  |
|        - |  1647 | `	/* Set the stale magic number */` |
|     2516 |  1648 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1649 | `	/* Release the private memory subsystem */` |
|     2516 |  1650 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2516 |  1651 | `	return SXRET_OK;` |
|        2 |  1652 |  |
|        - |  1653 | `/*` |
|        - |  1654 | ` * Initialize a foreign function call context.` |
|        - |  1655 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1656 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1657 | ` * functions.` |
|        - |  1658 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1659 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1660 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1661 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1662 | ` */` |
|   620144 |  1663 | `static sxi32 VmInitCallContext(` |
|        - |  1664 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1665 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1666 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1667 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1668 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1669 | `	)` |
|        2 |  1670 |  |
|   620146 |  1671 | `	pOut->pFunc = pFunc;` |
|   620146 |  1672 | `	pOut->pVm   = pVm;` |
|   620146 |  1673 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   620146 |  1674 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1675 | `	/* Assume a null return value */` |
|   620146 |  1676 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   620146 |  1677 | `	pOut->pRet = pRet;` |
|   620146 |  1678 | `	pOut->iFlags = iFlags;` |
|   620146 |  1679 | `	return SXRET_OK;` |
|        2 |  1680 |  |
|        - |  1681 | `/*` |
|        - |  1682 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1683 | ` * left behind.` |
|        - |  1684 | ` */` |
|   620144 |  1685 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1686 |  |
|        - |  1687 | `	sxu32 n;` |
|   620146 |  1688 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7528 |  1689 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    21640 |  1690 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    14114 |  1691 | `			if( apObj[n] == 0 ){` |
|        - |  1692 | `				/* Already released */` |
|      298 |  1693 | `				continue;` |
|        - |  1694 | `			}` |
|    13818 |  1695 | `			PH7_MemObjRelease(apObj[n]);` |
|    13818 |  1696 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6910 |  1697 | `		}` |
|     7528 |  1698 | `		SySetRelease(&pCtx->sVar);` |
|     3763 |  1699 | `	}` |
|   620146 |  1700 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1701 | `		ph7_aux_data *aAux;` |
|        - |  1702 | `		void *pChunk;` |
|        - |  1703 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1704 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1705 | `		 */` |
|        9 |  1706 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1707 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1708 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1709 | `			/* Release the chunk */` |
|       25 |  1710 | `			if( pChunk ){` |
|       25 |  1711 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1712 | `			}` |
|       13 |  1713 | `		}` |
|        9 |  1714 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1715 | `	}` |
|   620146 |  1716 |  |
|        - |  1717 | `/*` |
|        - |  1718 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1719 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1720 | ` */` |
|      296 |  1721 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1722 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1723 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1724 | `	)` |
|        2 |  1725 |  |
|      298 |  1726 | `	if( pValue == 0 ){` |
|        - |  1727 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1728 | `		return;` |
|        - |  1729 | `	}` |
|      298 |  1730 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1731 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1732 | `		sxu32 n;` |
|     1054 |  1733 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1734 | `			if( apObj[n] == pValue ){` |
|      298 |  1735 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1736 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1737 | `				/* Mark as released */` |
|      298 |  1738 | `				apObj[n] = 0;` |
|      298 |  1739 | `				break;` |
|        - |  1740 | `			}` |
|      380 |  1741 | `		}` |
|      148 |  1742 | `	}` |
|      150 |  1743 |  |
|        - |  1744 | `/*` |
|        - |  1745 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1746 | ` */` |
|  3569028 |  1747 | `static void VmPopOperand(` |
|        - |  1748 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1749 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1750 | `	)` |
|        2 |  1751 |  |
|  3569030 |  1752 | `	ph7_value *pTos = *ppTos;` |
|  7592276 |  1753 | `	while( nPop > 0 ){` |
|  4023248 |  1754 | `		PH7_MemObjRelease(pTos);` |
|  4023248 |  1755 | `		pTos--;` |
|  4023248 |  1756 | `		nPop--;` |
|        2 |  1757 | `	}` |
|        - |  1758 | `	/* Top of the stack */` |
|  3569030 |  1759 | `	*ppTos = pTos;` |
|  3569030 |  1760 |  |
|        - |  1761 | `/*` |
|        - |  1762 | ` * Reserve a memory object.` |
|        - |  1763 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1764 | ` */` |
|  3098456 |  1765 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1766 |  |
|  3098458 |  1767 | `	ph7_value *pObj = 0;` |
|        - |  1768 | `	VmSlot *pSlot;` |
|        - |  1769 | `	sxu32 nIdx;` |
|        - |  1770 | `	/* Check for a free slot */` |
|  3098458 |  1771 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3098458 |  1772 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3098458 |  1773 | `	if( pSlot ){` |
|   952332 |  1774 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   952332 |  1775 | `		nIdx = pSlot->nIdx;` |
|   476165 |  1776 | `	}` |
|  3098458 |  1777 | `	if( pObj == 0 ){` |
|        - |  1778 | `		/* Reserve a new memory object */` |
|  2146128 |  1779 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2146128 |  1780 | `		if( pObj == 0 ){` |
|      ! 0 |  1781 | `			return 0;` |
|        - |  1782 | `		}` |
|  1073063 |  1783 | `	}` |
|        - |  1784 | `	/* Set a null default value */` |
|  3098458 |  1785 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3098458 |  1786 | `	pObj->nIdx = nIdx;` |
|  3098458 |  1787 | `	return pObj;` |
|  1549230 |  1788 |  |
|        - |  1789 | `/*` |
|        - |  1790 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1791 | ` */` |
|    32466 |  1792 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1793 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1794 | `	const char *zKey,  /* Entry key */` |
|        - |  1795 | `	sxu32 nByte,       /* Key length */` |
|        - |  1796 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1797 | `	)` |
|        2 |  1798 |  |
|        - |  1799 | `	ph7_value sKey;` |
|        - |  1800 | `	sxi32 rc;` |
|    32468 |  1801 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    32468 |  1802 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1803 | `	/* Perform the insertion */` |
|    32468 |  1804 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    32468 |  1805 | `	PH7_MemObjRelease(&sKey);` |
|    32468 |  1806 | `	return rc;` |
|        2 |  1807 |  |
|        - |  1808 | `/*` |
|        - |  1809 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1810 | ` * Return a pointer to the variable value on success.` |
|        - |  1811 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1812 | ` */` |
|  3322324 |  1813 | `static ph7_value * VmExtractMemObj(` |
|        - |  1814 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1815 | `	const SyString *pName, /* Variable name */` |
|        - |  1816 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1817 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1818 | `	)` |
|        2 |  1819 |  |
|  3322326 |  1820 | `	int bNullify = FALSE;` |
|        - |  1821 | `	SyHashEntry *pEntry;` |
|        - |  1822 | `	VmFrame *pFrame;` |
|        - |  1823 | `	ph7_value *pObj;` |
|        - |  1824 | `	sxu32 nIdx;` |
|        - |  1825 | `	sxi32 rc;` |
|        - |  1826 | `	/* Point to the top active frame */` |
|  3322326 |  1827 | `	pFrame = pVm->pFrame;` |
|  3322326 |  1828 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1829 | `	/* Perform the lookup */` |
|  3322326 |  1830 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1831 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1832 | `		pName = &sAnnon;` |
|        - |  1833 | `		/* Always nullify the object */` |
|      ! 0 |  1834 | `		bNullify = TRUE;` |
|      ! 0 |  1835 | `		bDup = FALSE;` |
|      ! 0 |  1836 | `	}` |
|        - |  1837 | `	/* Check the superglobals table first */` |
|  3322326 |  1838 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3322326 |  1839 | `	if( pEntry == 0 ){` |
|        - |  1840 | `		/* Query the top active frame */` |
|  3322286 |  1841 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3322286 |  1842 | `		if( pEntry == 0 ){` |
|    93932 |  1843 | `			char *zName = (char *)pName->zString;` |
|        - |  1844 | `			VmSlot sLocal;` |
|    93932 |  1845 | `			if( !bCreate ){` |
|        - |  1846 | `				/* Do not create the variable,return NULL instead */` |
|      116 |  1847 | `				return 0;` |
|        - |  1848 | `			}` |
|        - |  1849 | `			/* No such variable,automatically create a new one and install` |
|        - |  1850 | `			 * it in the current frame.` |
|        - |  1851 | `			 */` |
|    93818 |  1852 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    93818 |  1853 | `			if( pObj == 0 ){` |
|      ! 0 |  1854 | `				return 0;` |
|        - |  1855 | `			}` |
|    93818 |  1856 | `			nIdx = pObj->nIdx;` |
|    93818 |  1857 | `			if( bDup ){` |
|        - |  1858 | `				/* Duplicate name */` |
|      168 |  1859 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1860 | `				if( zName == 0 ){` |
|      ! 0 |  1861 | `					return 0;` |
|        - |  1862 | `				}` |
|       83 |  1863 | `			}` |
|        - |  1864 | `			/* Link to the top active VM frame */` |
|    93818 |  1865 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    93818 |  1866 | `			if( rc != SXRET_OK ){` |
|        - |  1867 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1868 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1869 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1870 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1871 | `				return 0;` |
|        - |  1872 | `			}` |
|    93818 |  1873 | `			if( pFrame->pParent != 0 ){` |
|        - |  1874 | `				/* Local variable */` |
|    86600 |  1875 | `				sLocal.nIdx = nIdx;` |
|    86600 |  1876 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    43301 |  1877 | `			}else{` |
|        - |  1878 | `				/* Register in the $GLOBALS array */` |
|     7220 |  1879 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1880 | `			}` |
|        - |  1881 | `			/* Install in the reference table */` |
|    93818 |  1882 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1883 | `			/* Save object index */` |
|    93818 |  1884 | `			pObj->nIdx = nIdx;` |
|    46910 |  1885 | `		}else{` |
|        - |  1886 | `			/* Extract variable contents */` |
|  3228356 |  1887 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3228356 |  1888 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3228356 |  1889 | `			if( bNullify && pObj ){` |
|      ! 0 |  1890 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1891 | `			}` |
|        - |  1892 | `		}` |
|  1661197 |  1893 | `	}else{` |
|        - |  1894 | `		/* Superglobal */` |
|       42 |  1895 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1896 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1897 | `	}` |
|  3322212 |  1898 | `	return pObj;` |
|  1661274 |  1899 |  |
|        - |  1900 | `/*` |
|        - |  1901 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1902 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1903 | ` */` |
|     2826 |  1904 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1905 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1906 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1907 | `	sxu32 nByte        /* zName length */` |
|        - |  1908 | `	)` |
|        2 |  1909 |  |
|        - |  1910 | `	SyHashEntry *pEntry;` |
|        - |  1911 | `	ph7_value *pValue;` |
|        - |  1912 | `	sxu32 nIdx;` |
|        - |  1913 | `	/* Query the superglobal table */` |
|     2828 |  1914 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2828 |  1915 | `	if( pEntry == 0 ){` |
|        - |  1916 | `		/* No such entry */` |
|      ! 0 |  1917 | `		return 0;` |
|        - |  1918 | `	}` |
|        - |  1919 | `	/* Extract the superglobal index in the global object pool */` |
|     2828 |  1920 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1921 | `	/* Extract the variable value  */` |
|     2828 |  1922 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2828 |  1923 | `	return pValue;` |
|     1415 |  1924 |  |
|        - |  1925 | `/*` |
|        - |  1926 | ` * Perform a raw hashmap insertion.` |
|        - |  1927 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1928 | ` */` |
|     2856 |  1929 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1930 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1931 | `	const char *zKey,   /* Entry key */` |
|        - |  1932 | `	int nKeylen,        /* zKey length*/` |
|        - |  1933 | `	const char *zData,  /* Entry data */` |
|        - |  1934 | `	int nLen            /* zData length */` |
|        - |  1935 | `	)` |
|        2 |  1936 |  |
|        - |  1937 | `	ph7_value sKey,sValue;` |
|        - |  1938 | `	sxi32 rc;` |
|     2858 |  1939 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2858 |  1940 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2858 |  1941 | `	if( zKey ){` |
|     2836 |  1942 | `		if( nKeylen < 0 ){` |
|     2784 |  1943 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1391 |  1944 | `		}` |
|     2836 |  1945 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1417 |  1946 | `	}` |
|     2858 |  1947 | `	if( zData ){` |
|     2858 |  1948 | `		if( nLen < 0 ){` |
|        - |  1949 | `			/* Compute length automatically */` |
|      144 |  1950 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1951 | `		}` |
|     2858 |  1952 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1428 |  1953 | `	}` |
|        - |  1954 | `	/* Perform the insertion */` |
|     2858 |  1955 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2858 |  1956 | `	PH7_MemObjRelease(&sKey);` |
|     2858 |  1957 | `	PH7_MemObjRelease(&sValue);` |
|     2858 |  1958 | `	return rc;` |
|        2 |  1959 |  |
|        - |  1960 | `/*` |
|        - |  1961 | ` * Configure a working virtual machine instance.` |
|        - |  1962 | ` *` |
|        - |  1963 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1964 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1965 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1966 | ` * The second argument to this function is an integer configuration option` |
|        - |  1967 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1968 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1969 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1970 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1971 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1972 | ` */` |
|    40682 |  1973 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1974 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1975 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1976 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1977 | `	)` |
|        2 |  1978 |  |
|    40684 |  1979 | `	sxi32 rc = SXRET_OK;` |
|    40684 |  1980 | `	switch(nOp){` |
|     1253 |  1981 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2508 |  1982 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2508 |  1983 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1984 | `		/* VM output consumer callback */` |
|        - |  1985 | `#ifdef UNTRUST` |
|        - |  1986 | `		if( xConsumer == 0 ){` |
|        - |  1987 | `			rc = SXERR_CORRUPT;` |
|        - |  1988 | `			break;` |
|        - |  1989 | `		}` |
|        - |  1990 | `#endif` |
|        - |  1991 | `		/* Install the output consumer */` |
|     2508 |  1992 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2508 |  1993 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2508 |  1994 | `		break;` |
|        - |  1995 | `							   }` |
|     1261 |  1996 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1997 | `		/* Import path */` |
|        - |  1998 | `		  const char *zPath;` |
|        - |  1999 | `		  SyString sPath;` |
|     2524 |  2000 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2001 | `#if defined(UNTRUST)` |
|        - |  2002 | `		  if( zPath == 0 ){` |
|        - |  2003 | `			  rc = SXERR_EMPTY;` |
|        - |  2004 | `			  break;` |
|        - |  2005 | `		  }` |
|        - |  2006 | `#endif` |
|     2524 |  2007 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2008 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2009 | `#ifdef __WINNT__` |
|        2 |  2010 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2011 | `#endif` |
|     5046 |  2012 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2013 | `		  /* Remove leading and trailing white spaces */` |
|     2524 |  2014 | `		  SyStringFullTrim(&sPath);` |
|     2524 |  2015 | `		  if( sPath.nByte > 0 ){` |
|        - |  2016 | `			  /* Store the path in the corresponding conatiner */` |
|     2524 |  2017 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1261 |  2018 | `		  }` |
|     2524 |  2019 | `		  break;` |
|        - |  2020 | `									 }` |
|     1261 |  2021 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2022 | `		/* Run-Time Error report */` |
|     2524 |  2023 | `		pVm->bErrReport = 1;` |
|     2524 |  2024 | `		break;` |
|      ! 0 |  2025 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2026 | `		/* Recursion depth */` |
|      ! 0 |  2027 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2028 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2029 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2030 | `		}` |
|      ! 0 |  2031 | `		break;` |
|        - |  2032 | `									   }` |
|      ! 0 |  2033 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2034 | `		/* VM output length in bytes */` |
|      ! 0 |  2035 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2036 | `#ifdef UNTRUST` |
|        - |  2037 | `		if( pOut == 0 ){` |
|        - |  2038 | `			rc = SXERR_CORRUPT;` |
|        - |  2039 | `			break;` |
|        - |  2040 | `		}` |
|        - |  2041 | `#endif` |
|      ! 0 |  2042 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2043 | `		break;` |
|        - |  2044 | `							   }` |
|        - |  2045 |  |
|    12610 |  2046 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2047 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2048 | `		/* Create a new superglobal/global variable */` |
|    25222 |  2049 | `		const char *zName = va_arg(ap,const char *);` |
|    25222 |  2050 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2051 | `		SyHashEntry *pEntry;` |
|        - |  2052 | `		ph7_value *pObj;` |
|        - |  2053 | `		sxu32 nByte;` |
|        - |  2054 | `		sxu32 nIdx;` |
|        - |  2055 | `#ifdef UNTRUST` |
|        - |  2056 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2057 | `			rc = SXERR_CORRUPT;` |
|        - |  2058 | `			break;` |
|        - |  2059 | `		}` |
|        - |  2060 | `#endif` |
|    25222 |  2061 | `		nByte = SyStrlen(zName);` |
|    25222 |  2062 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2063 | `			/* Check if the superglobal is already installed */` |
|    25222 |  2064 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12612 |  2065 | `		}else{` |
|        - |  2066 | `			/* Query the top active VM frame */` |
|      ! 0 |  2067 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2068 | `		}` |
|    25222 |  2069 | `		if( pEntry ){` |
|        - |  2070 | `			/* Variable already installed */` |
|      ! 0 |  2071 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2072 | `			/* Extract contents */` |
|      ! 0 |  2073 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2074 | `			if( pObj ){` |
|        - |  2075 | `				/* Overwrite old contents */` |
|      ! 0 |  2076 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2077 | `			}` |
|      ! 0 |  2078 | `		}else{` |
|        - |  2079 | `			/* Install a new variable */` |
|    25222 |  2080 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    25222 |  2081 | `			if( pObj == 0 ){` |
|      ! 0 |  2082 | `				rc = SXERR_MEM;` |
|      ! 0 |  2083 | `				break;` |
|        - |  2084 | `			}` |
|    25222 |  2085 | `			nIdx = pObj->nIdx;` |
|        - |  2086 | `			/* Copy value */` |
|    25222 |  2087 | `			PH7_MemObjStore(pValue,pObj);` |
|    25222 |  2088 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2089 | `				/* Install the superglobal */` |
|    25222 |  2090 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12612 |  2091 | `			}else{` |
|        - |  2092 | `				/* Install in the current frame */` |
|      ! 0 |  2093 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2094 | `			}` |
|    25222 |  2095 | `			if( rc == SXRET_OK ){` |
|        - |  2096 | `				SyHashEntry *pRef;` |
|    25222 |  2097 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    25222 |  2098 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12612 |  2099 | `				}else{` |
|      ! 0 |  2100 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2101 | `				}` |
|        - |  2102 | `				/* Install in the reference table */` |
|    25222 |  2103 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    25222 |  2104 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2105 | `					/* Register in the $GLOBALS array */` |
|    25222 |  2106 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12610 |  2107 | `				}` |
|    12610 |  2108 | `			}` |
|        - |  2109 | `		}` |
|    25222 |  2110 | `		break;` |
|        - |  2111 | `									}` |
|     1391 |  2112 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2113 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2114 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2115 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2116 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2117 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2118 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2784 |  2119 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2784 |  2120 | `		const char *zValue = va_arg(ap,const char *);` |
|     2784 |  2121 | `		int nLen = va_arg(ap,int);` |
|        - |  2122 | `		ph7_hashmap *pMap;` |
|        - |  2123 | `		ph7_value *pValue;` |
|     2784 |  2124 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2125 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2126 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2783 |  2127 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2128 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2129 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2782 |  2130 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2131 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2132 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2782 |  2133 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2134 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2135 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2782 |  2136 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2137 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2138 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2782 |  2139 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2140 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2141 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2142 | `		}else{` |
|        - |  2143 | `			/* Extract the $_SERVER superglobal */` |
|     2782 |  2144 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2145 | `		}` |
|     2784 |  2146 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2147 | `			/* No such entry */` |
|      ! 0 |  2148 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2149 | `			break;` |
|        - |  2150 | `		}` |
|        - |  2151 | `		/* Point to the hashmap */` |
|     2784 |  2152 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2153 | `		/* Perform the insertion */` |
|     2784 |  2154 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2784 |  2155 | `		break;` |
|        - |  2156 | `								   }` |
|       11 |  2157 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2158 | `		/* Script arguments */` |
|       24 |  2159 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2160 | `		ph7_hashmap *pMap;` |
|        - |  2161 | `		ph7_value *pValue;` |
|        - |  2162 | `		sxu32 n;` |
|       24 |  2163 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2164 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2165 | `			break;` |
|        - |  2166 | `		}` |
|        - |  2167 | `		/* Extract the $argv array */` |
|       24 |  2168 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2169 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2170 | `			/* No such entry */` |
|      ! 0 |  2171 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2172 | `			break;` |
|        - |  2173 | `		}` |
|        - |  2174 | `		/* Point to the hashmap */` |
|       24 |  2175 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2176 | `		/* Perform the insertion */` |
|       24 |  2177 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2178 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2179 | `		if( rc == SXRET_OK ){` |
|       24 |  2180 | `			if( pMap->nEntry > 1 ){` |
|        - |  2181 | `				/* Append space separator first */` |
|       18 |  2182 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2183 | `			}` |
|       24 |  2184 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2185 | `		}` |
|       24 |  2186 | `		break;` |
|        - |  2187 | `								  }` |
|      ! 0 |  2188 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2189 | `		/* error_log() consumer */` |
|      ! 0 |  2190 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2191 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2192 | `		break;` |
|        - |  2193 | `										}` |
|      ! 0 |  2194 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2195 | `		/* Script return value */` |
|      ! 0 |  2196 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2197 | `#ifdef UNTRUST` |
|        - |  2198 | `		if( ppValue == 0 ){` |
|        - |  2199 | `			rc = SXERR_CORRUPT;` |
|        - |  2200 | `			break;` |
|        - |  2201 | `		}` |
|        - |  2202 | `#endif` |
|      ! 0 |  2203 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2204 | `		break;` |
|        - |  2205 | `								   }` |
|     2522 |  2206 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2207 | `		/* Register an IO stream device */` |
|     5046 |  2208 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2209 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7566 |  2210 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5046 |  2211 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2212 | `				/* Invalid stream */` |
|      ! 0 |  2213 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2214 | `				break;` |
|        - |  2215 | `		}` |
|     5046 |  2216 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2217 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2524 |  2218 | `			pVm->pDefStream = pStream;` |
|     1261 |  2219 | `		}` |
|        - |  2220 | `		/* Insert in the appropriate container */` |
|     5046 |  2221 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5046 |  2222 | `		break;` |
|        - |  2223 | `								  }` |
|        8 |  2224 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2225 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2226 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2227 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2228 | `#ifdef UNTRUST` |
|        - |  2229 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2230 | `			rc = SXERR_CORRUPT;` |
|        - |  2231 | `			break;` |
|        - |  2232 | `		}` |
|        - |  2233 | `#endif` |
|       16 |  2234 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2235 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2236 | `		break;` |
|        - |  2237 | `									   }` |
|        8 |  2238 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2239 | `		/* Raw HTTP request*/` |
|       16 |  2240 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2241 | `		int nByte = va_arg(ap,int);` |
|       16 |  2242 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2243 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2244 | `			break;` |
|        - |  2245 | `		}` |
|       16 |  2246 | `		if( nByte < 0 ){` |
|        - |  2247 | `			/* Compute length automatically */` |
|      ! 0 |  2248 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2249 | `		}` |
|        - |  2250 | `		/* Process the request */` |
|       16 |  2251 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2252 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2253 | `		if( rc == SXRET_OK ){` |
|       16 |  2254 | `			pVm->bHttpContext = 1;` |
|        8 |  2255 | `		}` |
|       16 |  2256 | `		break;` |
|        - |  2257 | `									}` |
|        8 |  2258 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2259 | `		/* Extract HTTP response status code */` |
|       16 |  2260 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2261 | `		if( pStatus ){` |
|       16 |  2262 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2263 | `		}` |
|       16 |  2264 | `		break;` |
|        - |  2265 | `										}` |
|        8 |  2266 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2267 | `		/* Iterate response headers via callback */` |
|        - |  2268 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2269 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2270 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2271 | `		if( xCallback ){` |
|       16 |  2272 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2273 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2274 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2275 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2276 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2277 | `							   pUserData);` |
|       12 |  2278 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2279 | `					break;` |
|        - |  2280 | `				}` |
|        6 |  2281 | `			}` |
|        8 |  2282 | `		}` |
|       16 |  2283 | `		break;` |
|        - |  2284 | `										 }` |
|      ! 0 |  2285 | `	default:` |
|        - |  2286 | `		/* Unknown configuration option */` |
|      ! 0 |  2287 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2288 | `		break;` |
|        - |  2289 | `	}` |
|    40684 |  2290 | `	return rc;` |
|        2 |  2291 |  |
|        - |  2292 | `/* Forward declaration */` |
|        - |  2293 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2294 | `/*` |
|        - |  2295 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2296 | ` * format.` |
|        - |  2297 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2298 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2299 | ` * (STDOUT).` |
|        - |  2300 | ` */` |
|        2 |  2301 | `static sxi32 VmByteCodeDump(` |
|        - |  2302 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2303 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2304 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2305 | `	)` |
|        1 |  2306 |  |
|        - |  2307 | `	static const char zDump[] = {` |
|        - |  2308 | `		"====================================================\n"` |
|        - |  2309 | `		"PH7 VM Dump\n"` |
|        - |  2310 | `		"====================================================\n"` |
|        - |  2311 | `	};` |
|        - |  2312 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2313 | `	sxi32 rc = SXRET_OK;` |
|        - |  2314 | `	sxu32 n;` |
|        - |  2315 | `	/* Point to the PH7 instructions */` |
|        3 |  2316 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2317 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2318 | `	n = 0;` |
|        3 |  2319 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2320 | `	/* Dump instructions */` |
|        7 |  2321 | `	for(;;){` |
|       15 |  2322 | `		if( pInstr >= pEnd ){` |
|        - |  2323 | `			/* No more instructions */` |
|        3 |  2324 | `			break;` |
|        - |  2325 | `		}` |
|        - |  2326 | `		/* Format and call the consumer callback */` |
|       19 |  2327 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2328 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2329 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2330 | `		if( rc != SXRET_OK ){` |
|        - |  2331 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2332 | `			return rc;` |
|        - |  2333 | `		}` |
|       13 |  2334 | `		++n;` |
|       13 |  2335 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2336 | `	}` |
|        3 |  2337 | `	return rc;` |
|        2 |  2338 |  |
|        - |  2339 | `/* Forward declaration */` |
|        - |  2340 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2341 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2342 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2343 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2344 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2345 | `/*` |
|        - |  2346 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2347 | ` * consumer callback.` |
|        - |  2348 | ` */` |
|      558 |  2349 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2350 |  |
|      559 |  2351 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      559 |  2352 | `	sxi32 rc = SXRET_OK;` |
|        - |  2353 | `	/* Append a new line */` |
|        - |  2354 | `#ifdef __WINNT__` |
|        1 |  2355 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2356 | `#else` |
|      558 |  2357 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2358 | `#endif` |
|        - |  2359 | `	/* Invoke the output consumer callback */` |
|      559 |  2360 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      559 |  2361 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      559 |  2362 | `	return rc;` |
|        1 |  2363 |  |
|        - |  2364 | `/*` |
|        - |  2365 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2366 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2367 | ` * information.` |
|        - |  2368 | ` */` |
|      134 |  2369 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2370 |  |
|      136 |  2371 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2372 | `		ph7_value apArg[4];` |
|        - |  2373 | `		ph7_value *apArgPtr[4];` |
|        - |  2374 | `		ph7_value sResult;` |
|        - |  2375 | `		SyString sErr;` |
|        - |  2376 | `		/* Prepare arguments */` |
|       61 |  2377 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2378 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2379 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2380 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2381 | `		if( pFile ){` |
|       61 |  2382 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2383 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2384 | `		}else{` |
|      ! 0 |  2385 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2386 | `		}` |
|       61 |  2387 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2388 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2389 | `		/* Set up pointer array */` |
|       61 |  2390 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2391 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2392 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2393 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2394 | `		/* Call the handler */` |
|       61 |  2395 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2396 | `		/* Check return value */` |
|       61 |  2397 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2398 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2399 | `		}` |
|        - |  2400 | `		/* Release */` |
|       61 |  2401 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2402 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2403 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2404 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2405 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2406 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2407 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2408 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2409 | `	}` |
|        - |  2410 | `	/* No handler, always call error handler */` |
|       75 |  2411 | `	return TRUE;` |
|       69 |  2412 |  |
|       98 |  2413 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2414 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2415 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2416 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2417 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2418 | `	)` |
|        2 |  2419 |  |
|      100 |  2420 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2421 | `	SyString *pFile;` |
|        - |  2422 | `	char *zErr;` |
|      100 |  2423 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2424 | `	if( !pVm->bErrReport ){` |
|        - |  2425 | `		/* Don't bother reporting errors */` |
|        3 |  2426 | `		return SXRET_OK;` |
|        - |  2427 | `	}` |
|        - |  2428 | `	/* Reset the working buffer */` |
|       98 |  2429 | `	SyBlobReset(pWorker);` |
|        - |  2430 | `	/* Peek the processed file if available */` |
|       98 |  2431 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2432 | `	if( pFile ){` |
|        - |  2433 | `		/* Append file name */` |
|       98 |  2434 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2435 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2436 | `	}` |
|        - |  2437 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2438 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2439 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2440 | `	 * E_DEPRECATED). */` |
|       98 |  2441 | `	zErr = "Error:  ";` |
|       98 |  2442 | `	switch(iErr){` |
|       19 |  2443 | `	case PH7_CTX_WARNING:` |
|       40 |  2444 | `		zErr = "Warning:  ";` |
|       40 |  2445 | `		break;` |
|        6 |  2446 | `	case PH7_CTX_NOTICE:` |
|       14 |  2447 | `		zErr = "Notice:  ";` |
|       12 |  2448 | `		break;` |
|       23 |  2449 | `	default:` |
|        - |  2450 | `		/* keep iErr unchanged */` |
|       46 |  2451 | `		break;` |
|        - |  2452 | `	}` |
|       98 |  2453 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2454 | `	if( pFuncName ){` |
|        - |  2455 | `		/* Append function name first */` |
|       23 |  2456 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2457 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2458 | `	}` |
|       98 |  2459 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2460 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2461 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2462 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2463 | `	}` |
|       98 |  2464 | `	return rc;` |
|       51 |  2465 |  |
|        - |  2466 | `/*` |
|        - |  2467 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2468 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2469 | ` * information.` |
|        - |  2470 | ` */` |
|       38 |  2471 | `static sxi32 VmThrowErrorAp(` |
|        - |  2472 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2473 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2474 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2475 | `	const char *zFormat, /* Format message */` |
|        - |  2476 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2477 | `	)` |
|        2 |  2478 |  |
|       40 |  2479 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2480 | `	SyBlob sMsg;` |
|        - |  2481 | `	SyString *pFile;` |
|        - |  2482 | `	char *zErr;` |
|       40 |  2483 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2484 | `	if( !pVm->bErrReport ){` |
|        - |  2485 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2486 | `		return SXRET_OK;` |
|        - |  2487 | `	}` |
|        - |  2488 | `	/* Reset the working buffer */` |
|       40 |  2489 | `	SyBlobReset(pWorker);` |
|        - |  2490 | `	/* Peek the processed file if available */` |
|       40 |  2491 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2492 | `	if( pFile ){` |
|        - |  2493 | `		/* Append file name */` |
|       40 |  2494 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2495 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2496 | `	}` |
|        - |  2497 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2498 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2499 | `	 * the correct errno value. */` |
|       40 |  2500 | `	zErr = "Error:  ";` |
|       40 |  2501 | `	switch(iErr){` |
|        4 |  2502 | `	case PH7_CTX_WARNING:` |
|        9 |  2503 | `		zErr = "Warning:  ";` |
|        9 |  2504 | `		break;` |
|        3 |  2505 | `	case PH7_CTX_NOTICE:` |
|        7 |  2506 | `		zErr = "Notice:  ";` |
|        6 |  2507 | `		break;` |
|       12 |  2508 | `	default:` |
|        - |  2509 | `		/* do not change iErr */` |
|       24 |  2510 | `		break;` |
|        - |  2511 | `	}` |
|       40 |  2512 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2513 | `	if( pFuncName ){` |
|        - |  2514 | `		/* Append function name first */` |
|       26 |  2515 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2516 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2517 | `	}` |
|        - |  2518 | `	/* Format the raw message */` |
|       40 |  2519 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2520 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2521 | `	/* Check if a user error handler is installed */` |
|       40 |  2522 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2523 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2524 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2525 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2526 | `	}` |
|       40 |  2527 | `	SyBlobRelease(&sMsg);` |
|       40 |  2528 | `	return rc;` |
|       21 |  2529 |  |
|        - |  2530 | `/*` |
|        - |  2531 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2532 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2533 | ` * possible.` |
|        - |  2534 | ` */` |
|       36 |  2535 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2536 |  |
|        - |  2537 | `	ph7_class *pClass;` |
|       37 |  2538 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2539 | `	ph7_class_instance *pThis;` |
|        - |  2540 | `	ph7_class_method *pCons;` |
|        - |  2541 | `	ph7_value sArg;` |
|        - |  2542 | `	ph7_value *apArg[1];` |
|        - |  2543 | `	SyBlob sMsg;` |
|        - |  2544 | `	SyString sMsgStr;` |
|        - |  2545 | `	VmFrame *pFrame;` |
|        - |  2546 | `	sxi32 rc;` |
|       37 |  2547 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       37 |  2548 | `	if( pClass == 0 ){` |
|      ! 0 |  2549 | `		return PH7_ABORT;` |
|        - |  2550 | `	}` |
|       37 |  2551 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       37 |  2552 | `	if( pThis == 0 ){` |
|      ! 0 |  2553 | `		return PH7_ABORT;` |
|        - |  2554 | `	}` |
|       37 |  2555 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2556 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2557 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2558 | `	{` |
|       37 |  2559 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       37 |  2560 | `		if( pOwner ){` |
|       37 |  2561 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       18 |  2562 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       19 |  2563 | `		}else{` |
|      ! 0 |  2564 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2565 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2566 | `		}` |
|        - |  2567 | `	}` |
|       37 |  2568 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       37 |  2569 | `	if( pCons ){` |
|       37 |  2570 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       37 |  2571 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       37 |  2572 | `		apArg[0] = &sArg;` |
|       37 |  2573 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       37 |  2574 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  2575 | `	}` |
|       37 |  2576 | `	SyBlobRelease(&sMsg);` |
|       37 |  2577 | `	pFrame = pVm->pFrame;` |
|       37 |  2578 | `	if( pFrame ){` |
|       37 |  2579 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       37 |  2580 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  2581 | `	}` |
|       37 |  2582 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       37 |  2583 | `	PH7_ClassInstanceUnref(pThis);` |
|       37 |  2584 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2585 | `		return PH7_ABORT;` |
|        - |  2586 | `	}` |
|       37 |  2587 | `	return PH7_EXCEPTION;` |
|       19 |  2588 |  |
|        - |  2589 |  |
|        - |  2590 | `/*` |
|        - |  2591 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2592 | ` */` |
|        4 |  2593 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2594 |  |
|        - |  2595 | `	ph7_class *pErrClass;` |
|        - |  2596 | `	ph7_class_instance *pThis;` |
|        - |  2597 | `	ph7_class_method *pCons;` |
|        - |  2598 | `	ph7_value sArg;` |
|        - |  2599 | `	ph7_value *apArg[1];` |
|        - |  2600 | `	SyBlob sMsg;` |
|        - |  2601 | `	SyString sMsgStr;` |
|        - |  2602 | `	VmFrame *pFrame;` |
|        - |  2603 | `	sxi32 rc;` |
|        5 |  2604 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2605 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2606 | `		return PH7_ABORT;` |
|        - |  2607 | `	}` |
|        5 |  2608 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2609 | `	if( pThis == 0 ){` |
|      ! 0 |  2610 | `		return PH7_ABORT;` |
|        - |  2611 | `	}` |
|        5 |  2612 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2613 | `	{` |
|        5 |  2614 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2615 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2616 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2617 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2618 | `	}` |
|        5 |  2619 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2620 | `	if( pCons ){` |
|        5 |  2621 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2622 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2623 | `		apArg[0] = &sArg;` |
|        5 |  2624 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2625 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2626 | `	}` |
|        5 |  2627 | `	SyBlobRelease(&sMsg);` |
|        5 |  2628 | `	pFrame = pVm->pFrame;` |
|        5 |  2629 | `	if( pFrame ){` |
|        5 |  2630 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2631 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2632 | `	}` |
|        5 |  2633 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2634 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2635 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2636 | `		return PH7_ABORT;` |
|        - |  2637 | `	}` |
|        5 |  2638 | `	return PH7_EXCEPTION;` |
|        3 |  2639 |  |
|        - |  2640 |  |
|        - |  2641 | `/*` |
|        - |  2642 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2643 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2644 | ` * For class types, instanceof is verified.` |
|        - |  2645 | ` *` |
|        - |  2646 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2647 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2648 | ` */` |
|        - |  2649 | `/*` |
|        - |  2650 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2651 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2652 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2653 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2654 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2655 | ` */` |
|       16 |  2656 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2657 |  |
|        - |  2658 | `	const char *z, *zEnd, *zTail;` |
|        - |  2659 | `	sxu32 n;` |
|        - |  2660 | `	sxu8 bReal;` |
|        - |  2661 | `	sxi32 rc;` |
|       18 |  2662 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2663 | `		return 0;` |
|        - |  2664 | `	}` |
|       18 |  2665 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2666 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2667 | `	zEnd = z + n;` |
|       18 |  2668 | `	if( n == 0 ){` |
|      ! 0 |  2669 | `		return 0;` |
|        - |  2670 | `	}` |
|       18 |  2671 | `	zTail = 0;` |
|       18 |  2672 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2673 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        5 |  2674 | `		return 0;` |
|        - |  2675 | `	}` |
|        - |  2676 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       14 |  2677 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2678 | `		zTail++;` |
|      ! 0 |  2679 | `	}` |
|       14 |  2680 | `	return zTail == zEnd ? 1 : 0;` |
|       10 |  2681 |  |
|        - |  2682 |  |
|        - |  2683 | `/*` |
|        - |  2684 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2685 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2686 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2687 | ` *   0 if it's not strictly numeric.` |
|        - |  2688 | ` */` |
|       16 |  2689 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2690 |  |
|        - |  2691 | `	const char *z, *zEnd, *zTail;` |
|        - |  2692 | `	sxu32 n;` |
|       18 |  2693 | `	sxu8 bReal = 0;` |
|        - |  2694 | `	sxi32 rc;` |
|       18 |  2695 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2696 | `		return 0;` |
|        - |  2697 | `	}` |
|       18 |  2698 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2699 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2700 | `	zEnd = z + n;` |
|       18 |  2701 | `	if( n == 0 ) return 0;` |
|       18 |  2702 | `	zTail = 0;` |
|       18 |  2703 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2704 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2705 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2706 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2707 | `	return bReal ? 2 : 1;` |
|       10 |  2708 |  |
|        - |  2709 |  |
|        - |  2710 | `/*` |
|        - |  2711 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts* using` |
|        - |  2712 | ` * PHP 8 weak-mode union semantics. Returns SXRET_OK on accept (pValue may` |
|        - |  2713 | ` * have been mutated by the cast), SXERR_INVALID on reject. Caller is` |
|        - |  2714 | ` * responsible for the actual TypeError throw.` |
|        - |  2715 | ` *` |
|        - |  2716 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2717 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2718 | ` */` |
|       78 |  2719 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable)` |
|        2 |  2720 |  |
|        - |  2721 | `	sxu32 i;` |
|        - |  2722 | `	ph7_type_alt *aAlts;` |
|        - |  2723 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2724 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|       80 |  2725 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2726 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2727 | `	}` |
|       68 |  2728 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       68 |  2729 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       68 |  2730 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      200 |  2731 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      134 |  2732 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      110 |  2733 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      110 |  2734 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      110 |  2735 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       56 |  2736 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       34 |  2737 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2738 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       68 |  2739 | `	}` |
|        - |  2740 | `	/* Object handling */` |
|       68 |  2741 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2742 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2743 | `		if( bHasClassAlt ){` |
|       14 |  2744 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2745 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2746 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2747 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2748 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2749 | `			}` |
|       26 |  2750 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2751 | `				ph7_class *pExpected;` |
|        - |  2752 | `				SyString *pCN;` |
|       22 |  2753 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2754 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2755 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2756 | `					pExpected = pSelfNow;` |
|       22 |  2757 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2758 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2759 | `				}else{` |
|       22 |  2760 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2761 | `				}` |
|       22 |  2762 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2763 | `					return SXRET_OK;` |
|        - |  2764 | `				}` |
|        8 |  2765 | `			}` |
|        2 |  2766 | `		}` |
|        9 |  2767 | `		return SXERR_INVALID;` |
|        - |  2768 | `	}` |
|        - |  2769 | `	/* Array handling */` |
|       52 |  2770 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2771 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2772 | `	}` |
|        - |  2773 | `	/* Scalar handling — exact match first */` |
|       46 |  2774 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       16 |  2775 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2776 | `	}` |
|       32 |  2777 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        3 |  2778 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2779 | `	}` |
|       30 |  2780 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       30 |  2781 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2782 | `	}` |
|       18 |  2783 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2784 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2785 | `	}` |
|        - |  2786 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2787 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2788 | `	 * to match PHP's union RFC. */` |
|        - |  2789 | `	{` |
|       18 |  2790 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2791 | `		if( bHasInt ){` |
|        - |  2792 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2793 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2794 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2795 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2796 | `				return SXRET_OK;` |
|        - |  2797 | `			}` |
|       18 |  2798 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2799 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2800 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2801 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2802 | `					return SXRET_OK;` |
|        - |  2803 | `				}` |
|      ! 0 |  2804 | `			}` |
|       18 |  2805 | `			if( kind == 1 ){` |
|        9 |  2806 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2807 | `				return SXRET_OK;` |
|        - |  2808 | `			}` |
|        4 |  2809 | `		}` |
|       10 |  2810 | `		if( bHasFloat ){` |
|       10 |  2811 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2812 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2813 | `				return SXRET_OK;` |
|        - |  2814 | `			}` |
|       10 |  2815 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2816 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2817 | `				return SXRET_OK;` |
|        - |  2818 | `			}` |
|        1 |  2819 | `		}` |
|        3 |  2820 | `		if( bHasString ){` |
|      ! 0 |  2821 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2822 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2823 | `				return SXRET_OK;` |
|        - |  2824 | `			}` |
|      ! 0 |  2825 | `		}` |
|        3 |  2826 | `		if( bHasBool ){` |
|      ! 0 |  2827 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2828 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2829 | `				return SXRET_OK;` |
|        - |  2830 | `			}` |
|      ! 0 |  2831 | `		}` |
|        - |  2832 | `	}` |
|        3 |  2833 | `	return SXERR_INVALID;` |
|       41 |  2834 |  |
|        - |  2835 |  |
|        - |  2836 | `/*` |
|        - |  2837 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  2838 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  2839 | ` */` |
|       16 |  2840 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  2841 |  |
|       17 |  2842 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       25 |  2843 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       16 |  2844 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       17 |  2845 | `	return zBuf;` |
|        1 |  2846 |  |
|        - |  2847 |  |
|    11978 |  2848 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  2849 |  |
|        - |  2850 | `	SyHashEntry *pSlot;` |
|        - |  2851 | `	VmClassAttr *pVmAttr;` |
|        - |  2852 | `	ph7_class_attr *pAttr;` |
|    11980 |  2853 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    11980 |  2854 | `	if( pSlot == 0 ){` |
|    11834 |  2855 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  2856 | `	}` |
|      148 |  2857 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      148 |  2858 | `	pAttr = pVmAttr->pAttr;` |
|      148 |  2859 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  2860 | `		return SXRET_OK;` |
|        - |  2861 | `	}` |
|        - |  2862 | `	/* Union type: dispatch to the shared coercion helper. */` |
|      148 |  2863 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  2864 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  2865 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0);` |
|       16 |  2866 | `		if( rc == SXRET_OK ){` |
|        9 |  2867 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  2868 | `			return SXRET_OK;` |
|        - |  2869 | `		}` |
|        7 |  2870 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2871 | `			char zBuf[128];` |
|        4 |  2872 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  2873 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2874 | `		}` |
|        5 |  2875 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2876 | `	}` |
|        - |  2877 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      134 |  2878 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       10 |  2879 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        8 |  2880 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        8 |  2881 | `			return SXRET_OK;` |
|        - |  2882 | `		}` |
|        3 |  2883 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  2884 | `	}` |
|        - |  2885 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  2886 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  2887 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      126 |  2888 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  2889 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  2890 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  2891 | `			return SXRET_OK;` |
|        - |  2892 | `		}` |
|        7 |  2893 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2894 | `	}` |
|      116 |  2895 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  2896 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  2897 | `		 * currently active on the self-stack. */` |
|       20 |  2898 | `		ph7_class *pExpected = 0;` |
|       20 |  2899 | `		SyString *pClassName = &pAttr->sClass;` |
|       20 |  2900 | `		ph7_class *pSelfNow = 0;` |
|       20 |  2901 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2902 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2903 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2904 | `		}` |
|       20 |  2905 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  2906 | `			pExpected = pSelfNow;` |
|       18 |  2907 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  2908 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2909 | `		}else{` |
|       16 |  2910 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  2911 | `		}` |
|       20 |  2912 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  2913 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2914 | `		}` |
|       20 |  2915 | `		if( pExpected ){` |
|       16 |  2916 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       16 |  2917 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  2918 | `				char zBuf[128];` |
|        7 |  2919 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2920 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2921 | `			}` |
|        5 |  2922 | `		}` |
|       16 |  2923 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       16 |  2924 | `		return SXRET_OK;` |
|        - |  2925 | `	}` |
|        - |  2926 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  2927 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|       98 |  2928 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2929 | `		char zBuf[128];` |
|        7 |  2930 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2931 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2932 | `	}` |
|       94 |  2933 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  2934 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  2935 | `		if( xCast ){` |
|        - |  2936 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  2937 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  2938 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2939 | `			}` |
|       24 |  2940 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  2941 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2942 | `			}` |
|        - |  2943 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  2944 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  2945 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  2946 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  2947 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  2948 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  2949 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  2950 | `			}` |
|       12 |  2951 | `			xCast(pValue);` |
|        5 |  2952 | `		}` |
|        5 |  2953 | `	}` |
|       80 |  2954 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       80 |  2955 | `	return SXRET_OK;` |
|     5991 |  2956 |  |
|        - |  2957 |  |
|        - |  2958 | `/*` |
|        - |  2959 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2960 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2961 | ` * information.` |
|        - |  2962 | ` * ------------------------------------` |
|        - |  2963 | ` * Simple boring wrapper function.` |
|        - |  2964 | ` * ------------------------------------` |
|        - |  2965 | ` */` |
|       14 |  2966 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2967 |  |
|        - |  2968 | `	va_list ap;` |
|        - |  2969 | `	sxi32 rc;` |
|       15 |  2970 | `	va_start(ap,zFormat);` |
|       15 |  2971 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2972 | `	va_end(ap);` |
|       15 |  2973 | `	return rc;` |
|        1 |  2974 |  |
|        - |  2975 | `/*` |
|        - |  2976 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  2977 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  2978 | ` */` |
|       30 |  2979 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  2980 |  |
|        - |  2981 | `	ph7_class *pClass;` |
|        - |  2982 | `	ph7_class_instance *pThis;` |
|        - |  2983 | `	ph7_class_method *pCons;` |
|        - |  2984 | `	ph7_value sArg;` |
|        - |  2985 | `	ph7_value *apArg[1];` |
|        - |  2986 | `	SyBlob sMsg;` |
|        - |  2987 | `	SyString sMsgStr;` |
|        - |  2988 | `	VmFrame *pFrame;` |
|        - |  2989 | `	sxi32 rc;` |
|       31 |  2990 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       31 |  2991 | `	if( pClass == 0 ){` |
|      ! 0 |  2992 | `		return PH7_ABORT;` |
|        - |  2993 | `	}` |
|       31 |  2994 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       31 |  2995 | `	if( pThis == 0 ){` |
|      ! 0 |  2996 | `		return PH7_ABORT;` |
|        - |  2997 | `	}` |
|       31 |  2998 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       31 |  2999 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       15 |  3000 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       31 |  3001 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       31 |  3002 | `	if( pCons ){` |
|       31 |  3003 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       31 |  3004 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       31 |  3005 | `		apArg[0] = &sArg;` |
|       31 |  3006 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       31 |  3007 | `		PH7_MemObjRelease(&sArg);` |
|       15 |  3008 | `	}` |
|       31 |  3009 | `	SyBlobRelease(&sMsg);` |
|       31 |  3010 | `	pFrame = pVm->pFrame;` |
|       31 |  3011 | `	if( pFrame ){` |
|       31 |  3012 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       31 |  3013 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       15 |  3014 | `	}` |
|       31 |  3015 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       31 |  3016 | `	PH7_ClassInstanceUnref(pThis);` |
|       31 |  3017 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3018 | `		return PH7_ABORT;` |
|        - |  3019 | `	}` |
|       31 |  3020 | `	return PH7_EXCEPTION;` |
|       16 |  3021 |  |
|        - |  3022 | `/*` |
|        - |  3023 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3024 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3025 | ` * information.` |
|        - |  3026 | ` * ------------------------------------` |
|        - |  3027 | ` * Simple boring wrapper function.` |
|        - |  3028 | ` * ------------------------------------` |
|        - |  3029 | ` */` |
|       24 |  3030 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3031 |  |
|        - |  3032 | `	sxi32 rc;` |
|       26 |  3033 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3034 | `	return rc;` |
|        2 |  3035 |  |
|        - |  3036 | `/*` |
|        - |  3037 | ` * Resolve function context from the current frame.` |
|        - |  3038 | ` */` |
|      954 |  3039 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3040 |  |
|        - |  3041 | `	VmFrame *pFrame;` |
|        - |  3042 | `	ph7_vm_func *pFunc;` |
|      955 |  3043 | `	*pzFuncName = 0;` |
|      955 |  3044 | `	*pnFuncLen = 0;` |
|      955 |  3045 | `	pFrame = pVm->pFrame;` |
|      955 |  3046 | `	if( pFrame == 0 ){` |
|      ! 0 |  3047 | `		return;` |
|        - |  3048 | `	}` |
|      955 |  3049 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      955 |  3050 | `	if( pFrame->pParent == 0 ){` |
|      947 |  3051 | `		return;` |
|        - |  3052 | `	}` |
|        9 |  3053 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        9 |  3054 | `	if( pFunc == 0 ){` |
|      ! 0 |  3055 | `		return;` |
|        - |  3056 | `	}` |
|        9 |  3057 | `	*pzFuncName = pFunc->sName.zString;` |
|        9 |  3058 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      478 |  3059 |  |
|        - |  3060 | `/*` |
|        - |  3061 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3062 | ` */` |
|      482 |  3063 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3064 |  |
|        - |  3065 | `	SyBlob sOut;` |
|        - |  3066 | `	SyString *pFile;` |
|      483 |  3067 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3068 | `		return PH7_OK;` |
|        - |  3069 | `	}` |
|      483 |  3070 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3071 | `		zClass = "Exception";` |
|      ! 0 |  3072 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3073 | `	}` |
|      483 |  3074 | `	if( zMsg == 0 ){` |
|      ! 0 |  3075 | `		zMsg = "Unknown exception";` |
|      ! 0 |  3076 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  3077 | `	}` |
|      483 |  3078 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      477 |  3079 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      238 |  3080 | `	}` |
|      483 |  3081 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      483 |  3082 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      483 |  3083 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      483 |  3084 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      483 |  3085 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      483 |  3086 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      483 |  3087 | `	if( pFile ){` |
|      483 |  3088 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      483 |  3089 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  3090 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      241 |  3091 | `	}` |
|      483 |  3092 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      483 |  3093 | `	if( pFile ){` |
|      483 |  3094 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      483 |  3095 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  3096 | `		if( zFuncName && nFuncLen > 0 ){` |
|        9 |  3097 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        5 |  3098 | `		}else{` |
|      475 |  3099 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3100 | `		}` |
|      241 |  3101 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3102 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3103 | `	}else{` |
|      ! 0 |  3104 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3105 | `	}` |
|      483 |  3106 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      483 |  3107 | `	if( pFile ){` |
|      483 |  3108 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      483 |  3109 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      483 |  3110 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  3111 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      241 |  3112 | `	}` |
|      483 |  3113 | `	VmCallErrorHandler(pVm,&sOut);` |
|      483 |  3114 | `	SyBlobRelease(&sOut);` |
|      483 |  3115 | `	return PH7_ABORT;` |
|      242 |  3116 |  |
|        - |  3117 | `/*` |
|        - |  3118 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3119 | ` */` |
|      480 |  3120 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3121 |  |
|        - |  3122 | `	ph7_vm *pVm;` |
|        - |  3123 | `	ph7_class *pClass;` |
|        - |  3124 | `	ph7_class_instance *pThis;` |
|        - |  3125 | `	ph7_class_method *pCons;` |
|        - |  3126 | `	ph7_value sArg;` |
|        - |  3127 | `	ph7_value *apArg[1];` |
|        - |  3128 | `	SyBlob sMsg;` |
|        - |  3129 | `	SyString sMsgStr;` |
|        - |  3130 | `	VmFrame *pFrame;` |
|        - |  3131 | `	va_list ap;` |
|        - |  3132 | `	sxi32 rc;` |
|        - |  3133 |  |
|      482 |  3134 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3135 | `		return PH7_ABORT;` |
|        - |  3136 | `	}` |
|      482 |  3137 | `	pVm = pCtx->pVm;` |
|      482 |  3138 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3139 | `		zClass = "Error";` |
|      ! 0 |  3140 | `	}` |
|      482 |  3141 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      482 |  3142 | `	if( pClass == 0 ){` |
|      ! 0 |  3143 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3144 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3145 | `			zClass` |
|        - |  3146 | `			);` |
|        - |  3147 | `	}` |
|      482 |  3148 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      482 |  3149 | `	if( pThis == 0 ){` |
|      ! 0 |  3150 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3151 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3152 | `			);` |
|        - |  3153 | `	}` |
|        - |  3154 |  |
|      482 |  3155 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      482 |  3156 | `	va_start(ap,zFormat);` |
|      482 |  3157 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      482 |  3158 | `	va_end(ap);` |
|        - |  3159 |  |
|      482 |  3160 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      482 |  3161 | `	if( pCons ){` |
|      482 |  3162 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      482 |  3163 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      482 |  3164 | `		apArg[0] = &sArg;` |
|      482 |  3165 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      482 |  3166 | `		PH7_MemObjRelease(&sArg);` |
|      240 |  3167 | `	}` |
|      482 |  3168 | `	SyBlobRelease(&sMsg);` |
|        - |  3169 |  |
|      482 |  3170 | `	pFrame = pVm->pFrame;` |
|      482 |  3171 | `	if( pFrame ){` |
|      482 |  3172 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      482 |  3173 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      240 |  3174 | `	}` |
|      482 |  3175 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      482 |  3176 | `	PH7_ClassInstanceUnref(pThis);` |
|      482 |  3177 | `	if( rc == SXERR_ABORT ){` |
|      471 |  3178 | `		return PH7_ABORT;` |
|        - |  3179 | `	}` |
|       12 |  3180 | `	return PH7_EXCEPTION;` |
|      242 |  3181 |  |
|        - |  3182 | `/*` |
|        - |  3183 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3184 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3185 | ` */` |
|      ! 0 |  3186 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3187 |  |
|        - |  3188 | `	ph7_vm *pVm;` |
|        - |  3189 | `	SyBlob sMsg;` |
|      ! 0 |  3190 | `	const char *zFuncName = 0;` |
|      ! 0 |  3191 | `	int nFuncLen = 0;` |
|        - |  3192 | `	va_list ap;` |
|        - |  3193 | `	sxi32 rc;` |
|        - |  3194 |  |
|      ! 0 |  3195 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3196 | `		return PH7_OK;` |
|        - |  3197 | `	}` |
|      ! 0 |  3198 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3199 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3200 | `		zClass = "Error";` |
|      ! 0 |  3201 | `	}` |
|        - |  3202 |  |
|      ! 0 |  3203 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3204 |  |
|      ! 0 |  3205 | `	va_start(ap,zFormat);` |
|      ! 0 |  3206 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3207 | `	va_end(ap);` |
|        - |  3208 |  |
|      ! 0 |  3209 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3210 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3211 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3212 | `	}` |
|      ! 0 |  3213 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3214 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3215 | `	}` |
|      ! 0 |  3216 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3217 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3218 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3219 | `	return rc;` |
|      ! 0 |  3220 |  |
|        - |  3221 | `/*` |
|        - |  3222 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3223 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3224 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3225 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3226 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3227 | ` * when VmByteCodeExec returns.` |
|        - |  3228 | ` */` |
|      132 |  3229 | `static sxi32 VmSuspendCtx(` |
|        - |  3230 | `	ph7_vm *pVm,` |
|        - |  3231 | `	ph7_exec_ctx *pCtx,` |
|        - |  3232 | `	sxi32 pc,` |
|        - |  3233 | `	sxi32 nTos` |
|        - |  3234 | `	)` |
|        2 |  3235 |  |
|       66 |  3236 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      134 |  3237 | `	pCtx->pc = pc;` |
|      134 |  3238 | `	pCtx->nTos = nTos;` |
|      134 |  3239 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      134 |  3240 | `	return PH7_SUSPEND;` |
|        2 |  3241 |  |
|        - |  3242 | `/*` |
|        - |  3243 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3244 | ` *` |
|        - |  3245 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3246 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3247 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3248 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3249 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3250 | ` * then the program execution is halted.` |
|        - |  3251 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3252 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3253 | ` * or to reset the VM to it's initial state.` |
|        - |  3254 | ` */` |
|    36258 |  3255 | `static sxi32 VmByteCodeExec(` |
|        - |  3256 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3257 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3258 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3259 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3260 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3261 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3262 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3263 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3264 | `	)` |
|        2 |  3265 |  |
|        - |  3266 | `	VmInstr *pInstr;` |
|        - |  3267 | `	ph7_value *pTos;` |
|        - |  3268 | `	SySet aArg;` |
|        - |  3269 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3270 | `	sxi32 pc;` |
|        - |  3271 | `	sxi32 rc;` |
|        - |  3272 | `	/* Argument container */` |
|    36260 |  3273 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    36260 |  3274 | `	if( nTos < 0 ){` |
|    34086 |  3275 | `		pTos = &pStack[-1];` |
|    17044 |  3276 | `	}else{` |
|     2176 |  3277 | `		pTos = &pStack[nTos];` |
|        - |  3278 | `	}` |
|    36260 |  3279 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    36260 |  3280 | `	pc = nPc;` |
|        - |  3281 | `/*` |
|        - |  3282 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3283 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3284 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3285 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3286 | ` */` |
|        - |  3287 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3288 | `	{ \` |
|        - |  3289 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3290 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3291 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3292 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3293 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3294 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3295 | `				break; \` |
|        - |  3296 | `			} \` |
|        - |  3297 | `			goto Exception; \` |
|        - |  3298 | `		} \` |
|        - |  3299 | `	}` |
|        - |  3300 | `	/* Execute as much as we can */` |
|  5340792 |  3301 | `	for(;;){` |
|        - |  3302 | `		/* Fetch the instruction to execute */` |
| 10680882 |  3303 | `		pInstr = &aInstr[pc];` |
| 10680882 |  3304 | `		rc = SXRET_OK;` |
|        - |  3305 | `/*` |
|        - |  3306 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3307 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3308 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3309 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3310 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3311 | ` */` |
| 10680882 |  3312 | `		switch(pInstr->iOp){` |
|        - |  3313 | `/*` |
|        - |  3314 | ` * DONE: P1 * *` |
|        - |  3315 | ` *` |
|        - |  3316 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3317 | ` * and return immediately.` |
|        - |  3318 | ` */` |
|    17811 |  3319 | `case PH7_OP_DONE:` |
|    35624 |  3320 | `	if( pInstr->iP1 ){` |
|        - |  3321 | `#ifdef UNTRUST` |
|        - |  3322 | `		if( pTos < pStack ){` |
|        - |  3323 | `			goto Abort;` |
|        - |  3324 | `		}` |
|        - |  3325 | `#endif` |
|    20804 |  3326 | `		if( pLastRef ){` |
|    13380 |  3327 | `			*pLastRef = pTos->nIdx;` |
|     6689 |  3328 | `		}` |
|    20804 |  3329 | `		if( pResult ){` |
|        - |  3330 | `			/* Execution result */` |
|    19750 |  3331 | `			PH7_MemObjStore(pTos,pResult);` |
|     9874 |  3332 | `		}` |
|    20804 |  3333 | `		VmPopOperand(&pTos,1);` |
|    25223 |  3334 | `	}else if( pLastRef ){` |
|        - |  3335 | `		/* Nothing referenced */` |
|     1240 |  3336 | `		*pLastRef = SXU32_HIGH;` |
|      619 |  3337 | `	}` |
|        - |  3338 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3339 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3340 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3341 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3342 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3343 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3344 | `	 * block can override it.` |
|        - |  3345 | `	 */` |
|    35626 |  3346 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3347 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3348 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3349 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3350 | `		pExc->pFrame = 0;` |
|        3 |  3351 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3352 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3353 | `			pExc->iFinallyDone = 1;` |
|        - |  3354 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3355 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3356 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3357 | `				goto Abort;` |
|        - |  3358 | `			}` |
|        1 |  3359 | `		}` |
|        1 |  3360 | `	}` |
|    35624 |  3361 | `	goto Done;` |
|        - |  3362 | `/*` |
|        - |  3363 | ` * HALT: P1 * *` |
|        - |  3364 | ` *` |
|        - |  3365 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3366 | ` * and abort immediately.` |
|        - |  3367 | ` */` |
|        4 |  3368 | `case PH7_OP_HALT:` |
|        9 |  3369 | `	if( pInstr->iP1 ){` |
|        - |  3370 | `#ifdef UNTRUST` |
|        - |  3371 | `		if( pTos < pStack ){` |
|        - |  3372 | `			goto Abort;` |
|        - |  3373 | `		}` |
|        - |  3374 | `#endif` |
|        9 |  3375 | `		if( pLastRef ){` |
|      ! 0 |  3376 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3377 | `		}` |
|        9 |  3378 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3379 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3380 | `				/* Output the exit message */` |
|        7 |  3381 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3382 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3383 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3384 | `			}` |
|        7 |  3385 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3386 | `			/* Record exit status */` |
|        5 |  3387 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3388 | `		}` |
|        9 |  3389 | `		VmPopOperand(&pTos,1);` |
|        4 |  3390 | `	}else if( pLastRef ){` |
|        - |  3391 | `		/* Nothing referenced */` |
|      ! 0 |  3392 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3393 | `	}` |
|        - |  3394 | `	/* Check if we're in an included file context */` |
|        9 |  3395 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3396 | `		/* Terminate the entire process */` |
|        9 |  3397 | `		exit(pVm->iExitStatus);` |
|        - |  3398 | `	}` |
|      ! 0 |  3399 | `	goto Abort;` |
|        - |  3400 | `/*` |
|        - |  3401 | ` * JMP: * P2 *` |
|        - |  3402 | ` *` |
|        - |  3403 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3404 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3405 | ` */` |
|   229412 |  3406 | `case PH7_OP_JMP:` |
|   458870 |  3407 | `	pc = pInstr->iP2 - 1;` |
|   458870 |  3408 | `	break;` |
|        - |  3409 | `/*` |
|        - |  3410 | ` * JZ: P1 P2 *` |
|        - |  3411 | ` *` |
|        - |  3412 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3413 | ` * entry in the stack if P1 is zero.` |
|        - |  3414 | ` */` |
|   540218 |  3415 | `case PH7_OP_JZ:` |
|        - |  3416 | `#ifdef UNTRUST` |
|        - |  3417 | `	if( pTos < pStack ){` |
|        - |  3418 | `		goto Abort;` |
|        - |  3419 | `	}` |
|        - |  3420 | `#endif` |
|        - |  3421 | `	/* Get a boolean value */` |
|  1080526 |  3422 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  3423 | `		PH7_MemObjToBool(pTos);` |
|       80 |  3424 | `	}` |
|  1080526 |  3425 | `	if( !pTos->x.iVal ){` |
|        - |  3426 | `		/* Take the jump */` |
|   550600 |  3427 | `		pc = pInstr->iP2 - 1;` |
|   275299 |  3428 | `	}` |
|  1080526 |  3429 | `	if( !pInstr->iP1 ){` |
|   858016 |  3430 | `		VmPopOperand(&pTos,1);` |
|   429029 |  3431 | `	}` |
|  1080526 |  3432 | `	break;` |
|        - |  3433 | `/*` |
|        - |  3434 | ` * JNZ: P1 P2 *` |
|        - |  3435 | ` *` |
|        - |  3436 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3437 | ` * entry in the stack if P1 is zero.` |
|        - |  3438 | ` */` |
|    56630 |  3439 | `case PH7_OP_JNZ:` |
|        - |  3440 | `#ifdef UNTRUST` |
|        - |  3441 | `	if( pTos < pStack ){` |
|        - |  3442 | `		goto Abort;` |
|        - |  3443 | `	}` |
|        - |  3444 | `#endif` |
|        - |  3445 | `	/* Get a boolean value */` |
|   113262 |  3446 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3447 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3448 | `	}` |
|   113262 |  3449 | `	if( pTos->x.iVal ){` |
|        - |  3450 | `		/* Take the jump */` |
|     4922 |  3451 | `		pc = pInstr->iP2 - 1;` |
|     2460 |  3452 | `	}` |
|   113262 |  3453 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3454 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3455 | `	}` |
|   113262 |  3456 | `	break;` |
|        - |  3457 | `/*` |
|        - |  3458 | ` * NOOP: * * *` |
|        - |  3459 | ` *` |
|        - |  3460 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  3461 | ` * destination.` |
|        - |  3462 | ` */` |
|      ! 0 |  3463 | `case PH7_OP_NOOP:` |
|      ! 0 |  3464 | `	break;` |
|        - |  3465 | `/*` |
|        - |  3466 | ` * POP: P1 * *` |
|        - |  3467 | ` *` |
|        - |  3468 | ` * Pop P1 elements from the operand stack.` |
|        - |  3469 | ` */` |
|   418199 |  3470 | `case PH7_OP_POP: {` |
|   836444 |  3471 | `	sxi32 n = pInstr->iP1;` |
|   836444 |  3472 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3473 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3474 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3475 | `	}` |
|   836444 |  3476 | `	VmPopOperand(&pTos,n);` |
|   836444 |  3477 | `	break;` |
|        - |  3478 | `				 }` |
|        - |  3479 | `/*` |
|        - |  3480 | ` * DUP: * * *` |
|        - |  3481 | ` *` |
|        - |  3482 | ` * Duplicate the top of the stack.` |
|        - |  3483 | ` */` |
|       41 |  3484 | `case PH7_OP_DUP:` |
|        - |  3485 | `#ifdef UNTRUST` |
|        - |  3486 | `	if( pTos < pStack ){` |
|        - |  3487 | `		goto Abort;` |
|        - |  3488 | `	}` |
|        - |  3489 | `#endif` |
|       84 |  3490 | `	pTos++;` |
|       84 |  3491 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  3492 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  3493 | `	break;` |
|        - |  3494 | `/*` |
|        - |  3495 | ` * NSSWITCH: * * P3` |
|        - |  3496 | ` *` |
|        - |  3497 | ` * Switch the active namespace at runtime.` |
|        - |  3498 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  3499 | ` */` |
|     6995 |  3500 | `case PH7_OP_NSSWITCH:` |
|    13992 |  3501 | `	SyBlobReset(&pVm->sNamespace);` |
|    13992 |  3502 | `	if( pInstr->p3 ){` |
|       96 |  3503 | `		const char *zNs = (const char *)pInstr->p3;` |
|       96 |  3504 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       47 |  3505 | `	}` |
|        - |  3506 | `	/* Clear namespace-scoped use-const imports */` |
|    13992 |  3507 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    13992 |  3508 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    13992 |  3509 | `	break;` |
|        - |  3510 | `/* OP_USECONST P1 * P3` |
|        - |  3511 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  3512 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  3513 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  3514 | ` */` |
|        7 |  3515 | `case PH7_OP_USECONST: {` |
|       16 |  3516 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  3517 | `	if( azPair ){` |
|       16 |  3518 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  3519 | `	}` |
|       16 |  3520 | `	break;` |
|        - |  3521 | `				}` |
|        - |  3522 | `/*` |
|        - |  3523 | ` * CVT_INT: * * *` |
|        - |  3524 | ` *` |
|        - |  3525 | ` * Force the top of the stack to be an integer.` |
|        - |  3526 | ` */` |
|       77 |  3527 | `case PH7_OP_CVT_INT:` |
|        - |  3528 | `#ifdef UNTRUST` |
|        - |  3529 | `	if( pTos < pStack ){` |
|        - |  3530 | `		goto Abort;` |
|        - |  3531 | `	}` |
|        - |  3532 | `#endif` |
|      156 |  3533 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      109 |  3534 | `		PH7_MemObjToInteger(pTos);` |
|       54 |  3535 | `	}` |
|        - |  3536 | `	/* Invalidate any prior representation */` |
|      156 |  3537 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      156 |  3538 | `	break;` |
|        - |  3539 | `/*` |
|        - |  3540 | ` * CVT_REAL: * * *` |
|        - |  3541 | ` *` |
|        - |  3542 | ` * Force the top of the stack to be a real.` |
|        - |  3543 | ` */` |
|        4 |  3544 | `case PH7_OP_CVT_REAL:` |
|        - |  3545 | `#ifdef UNTRUST` |
|        - |  3546 | `	if( pTos < pStack ){` |
|        - |  3547 | `		goto Abort;` |
|        - |  3548 | `	}` |
|        - |  3549 | `#endif` |
|        9 |  3550 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3551 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3552 | `	}` |
|        - |  3553 | `	/* Invalidate any prior representation */` |
|        9 |  3554 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3555 | `	break;` |
|        - |  3556 | `/*` |
|        - |  3557 | ` * CVT_STR: * * *` |
|        - |  3558 | ` *` |
|        - |  3559 | ` * Force the top of the stack to be a string.` |
|        - |  3560 | ` */` |
|      146 |  3561 | `case PH7_OP_CVT_STR:` |
|        - |  3562 | `#ifdef UNTRUST` |
|        - |  3563 | `	if( pTos < pStack ){` |
|        - |  3564 | `		goto Abort;` |
|        - |  3565 | `	}` |
|        - |  3566 | `#endif` |
|      294 |  3567 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3568 | `		PH7_MemObjToString(pTos);` |
|      146 |  3569 | `	}` |
|      294 |  3570 | `	break;` |
|        - |  3571 | `/*` |
|        - |  3572 | ` * CVT_BOOL: * * *` |
|        - |  3573 | ` *` |
|        - |  3574 | ` * Force the top of the stack to be a boolean.` |
|        - |  3575 | ` */` |
|        5 |  3576 | `case PH7_OP_CVT_BOOL:` |
|        - |  3577 | `#ifdef UNTRUST` |
|        - |  3578 | `	if( pTos < pStack ){` |
|        - |  3579 | `		goto Abort;` |
|        - |  3580 | `	}` |
|        - |  3581 | `#endif` |
|       11 |  3582 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3583 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3584 | `	}` |
|       11 |  3585 | `	break;` |
|        - |  3586 | `/*` |
|        - |  3587 | ` * CVT_NULL: * * *` |
|        - |  3588 | ` *` |
|        - |  3589 | ` * Nullify the top of the stack.` |
|        - |  3590 | ` */` |
|        3 |  3591 | `case PH7_OP_CVT_NULL:` |
|        - |  3592 | `#ifdef UNTRUST` |
|        - |  3593 | `	if( pTos < pStack ){` |
|        - |  3594 | `		goto Abort;` |
|        - |  3595 | `	}` |
|        - |  3596 | `#endif` |
|        7 |  3597 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3598 | `	break;` |
|        - |  3599 | `/*` |
|        - |  3600 | ` * CVT_NUMC: * * *` |
|        - |  3601 | ` *` |
|        - |  3602 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3603 | ` */` |
|      ! 0 |  3604 | `case PH7_OP_CVT_NUMC:` |
|        - |  3605 | `#ifdef UNTRUST` |
|        - |  3606 | `	if( pTos < pStack ){` |
|        - |  3607 | `		goto Abort;` |
|        - |  3608 | `	}` |
|        - |  3609 | `#endif` |
|        - |  3610 | `	/* Force a numeric cast */` |
|      ! 0 |  3611 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3612 | `	break;` |
|        - |  3613 | `/*` |
|        - |  3614 | ` * CVT_ARRAY: * * *` |
|        - |  3615 | ` *` |
|        - |  3616 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3617 | ` */` |
|       10 |  3618 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3619 | `#ifdef UNTRUST` |
|        - |  3620 | `	if( pTos < pStack ){` |
|        - |  3621 | `		goto Abort;` |
|        - |  3622 | `	}` |
|        - |  3623 | `#endif` |
|        - |  3624 | `	/* Force a hashmap cast */` |
|       21 |  3625 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3626 | `	if( rc != SXRET_OK ){` |
|        - |  3627 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3628 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3629 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3630 | `	}` |
|       21 |  3631 | `	break;` |
|        - |  3632 | `/*` |
|        - |  3633 | ` * CVT_OBJ: * * *` |
|        - |  3634 | ` *` |
|        - |  3635 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3636 | ` */` |
|        8 |  3637 | `case PH7_OP_CVT_OBJ:` |
|        - |  3638 | `#ifdef UNTRUST` |
|        - |  3639 | `	if( pTos < pStack ){` |
|        - |  3640 | `		goto Abort;` |
|        - |  3641 | `	}` |
|        - |  3642 | `#endif` |
|       17 |  3643 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3644 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3645 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3646 | `	}` |
|       17 |  3647 | `	break;` |
|        - |  3648 | `/*` |
|        - |  3649 | ` * ERR_CTRL * * *` |
|        - |  3650 | ` *` |
|        - |  3651 | ` * Error control operator.` |
|        - |  3652 | ` */` |
|    14131 |  3653 | `case PH7_OP_ERR_CTRL:` |
|        - |  3654 | `	/*` |
|        - |  3655 | `	 * TICKET 1433-038:` |
|        - |  3656 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3657 | `	 * use the public API,to control error output.` |
|        - |  3658 | `	 */` |
|    28262 |  3659 | `	break;` |
|        - |  3660 | `/*` |
|        - |  3661 | ` * IS_A * * *` |
|        - |  3662 | ` *` |
|        - |  3663 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3664 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3665 | ` * holding a class name or an object).` |
|        - |  3666 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3667 | ` */` |
|       23 |  3668 | `case PH7_OP_IS_A:{` |
|       48 |  3669 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3670 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3671 | `#ifdef UNTRUST` |
|        - |  3672 | `	if( pNos < pStack ){` |
|        - |  3673 | `		goto Abort;` |
|        - |  3674 | `	}` |
|        - |  3675 | `#endif` |
|       48 |  3676 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3677 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3678 | `		ph7_class *pClass = 0;` |
|        - |  3679 | `		/* Extract the target class */` |
|       46 |  3680 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3681 | `			/* Instance already loaded */` |
|      ! 0 |  3682 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3683 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3684 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3685 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3686 | `			/* Handle self/static/parent keywords */` |
|       46 |  3687 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3688 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3689 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3690 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3691 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3692 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3693 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3694 | `					pClass = pSelf->pBase;` |
|        2 |  3695 | `				}` |
|        3 |  3696 | `			}else{` |
|       36 |  3697 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3698 | `			}` |
|       22 |  3699 | `		}` |
|       46 |  3700 | `		if( pClass ){` |
|        - |  3701 | `			/* Perform the query */` |
|       46 |  3702 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3703 | `		}` |
|       22 |  3704 | `	}` |
|        - |  3705 | `	/* Push result */` |
|       48 |  3706 | `	VmPopOperand(&pTos,1);` |
|       48 |  3707 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3708 | `	pTos->x.iVal = iRes;` |
|       48 |  3709 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3710 | `	break;` |
|        - |  3711 | `				 }` |
|        - |  3712 |  |
|        - |  3713 | `/*` |
|        - |  3714 | ` * LOADC P1 P2 *` |
|        - |  3715 | ` *` |
|        - |  3716 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3717 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3718 | ` */` |
|   904036 |  3719 | `case PH7_OP_LOADC: {` |
|        - |  3720 | `	ph7_value *pObj;` |
|        - |  3721 | `	/* Reserve a room */` |
|  1808118 |  3722 | `	pTos++;` |
|  2703415 |  3723 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1808118 |  3724 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3725 | `			SyHashEntry *pEntry;` |
|        - |  3726 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3727 | `			{` |
|        - |  3728 | `				SyHashEntry *pConstImport;` |
|    26354 |  3729 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    17568 |  3730 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17570 |  3731 | `				if( pConstImport ){` |
|       11 |  3732 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3733 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3734 | `					if( pEntry ){` |
|       11 |  3735 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3736 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3737 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3738 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3739 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3740 | `						break;` |
|        - |  3741 | `					}` |
|        - |  3742 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3743 | `				}` |
|        - |  3744 | `			}` |
|        - |  3745 | `			/* Candidate for expansion via user defined callbacks */` |
|    17560 |  3746 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17560 |  3747 | `			if( pEntry ){` |
|    17556 |  3748 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3749 | `				/* Set a NULL default value */` |
|    17556 |  3750 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    17556 |  3751 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3752 | `				/* Invoke the callback and deal with the expanded value */` |
|    17556 |  3753 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3754 | `				/* Mark as constant */` |
|    17556 |  3755 | `				pTos->nIdx = SXU32_HIGH;` |
|    17556 |  3756 | `				break;` |
|        - |  3757 | `			}` |
|        - |  3758 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3759 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3760 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3761 | `			{` |
|        6 |  3762 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3763 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3764 | `				sxu32 j;` |
|        6 |  3765 | `				int isQualified = 0;` |
|       32 |  3766 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3767 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3768 | `				}` |
|        6 |  3769 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3770 | `					/* Try current_namespace\name */` |
|      ! 0 |  3771 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3772 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3773 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3774 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3775 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3776 | `					if( pEntry ){` |
|      ! 0 |  3777 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3778 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3779 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3780 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3781 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3782 | `						break;` |
|        - |  3783 | `					}` |
|        - |  3784 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3785 | `				}` |
|        6 |  3786 | `				if( isQualified ){` |
|        - |  3787 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3788 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3789 | `					SyBlob sErr;` |
|        3 |  3790 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3791 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3792 | `					if( pErrFile ){` |
|        3 |  3793 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3794 | `					}` |
|        3 |  3795 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3796 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3797 | `					SyBlobRelease(&sErr);` |
|        3 |  3798 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3799 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3800 | `					goto LoadC_Done;` |
|        - |  3801 | `				}` |
|        - |  3802 | `			}` |
|        1 |  3803 | `		}` |
|  1790552 |  3804 | `		PH7_MemObjLoad(pObj,pTos);` |
|   895299 |  3805 | `	}else{` |
|        - |  3806 | `		/* Set a NULL value */` |
|      ! 0 |  3807 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3808 | `	}` |
|   895254 |  3809 | `LoadC_Done:` |
|        - |  3810 | `	/* Mark as constant */` |
|  1790554 |  3811 | `	pTos->nIdx = SXU32_HIGH;` |
|  1790554 |  3812 | `	break;` |
|        - |  3813 | `				  }` |
|        - |  3814 | `/*` |
|        - |  3815 | ` * LOAD: P1 * P3` |
|        - |  3816 | ` *` |
|        - |  3817 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3818 | ` * from the P3 operand.` |
|        - |  3819 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3820 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3821 | ` */` |
|  1439557 |  3822 | `case PH7_OP_LOAD:{` |
|        - |  3823 | `	ph7_value *pObj;` |
|        - |  3824 | `	SyString sName;` |
|  2879336 |  3825 | `	if( pInstr->p3 == 0 ){` |
|        - |  3826 | `		/* Take the variable name from the top of the stack */` |
|        - |  3827 | `#ifdef UNTRUST` |
|        - |  3828 | `		if( pTos < pStack ){` |
|        - |  3829 | `			goto Abort;` |
|        - |  3830 | `		}` |
|        - |  3831 | `#endif` |
|        - |  3832 | `		/* Force a string cast */` |
|       19 |  3833 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3834 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3835 | `		}` |
|       19 |  3836 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3837 | `	}else{` |
|  2879318 |  3838 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3839 | `		/* Reserve a room for the target object */` |
|  2879318 |  3840 | `		pTos++;` |
|        - |  3841 | `	}` |
|        - |  3842 | `	/* Extract the requested memory object */` |
|  2879336 |  3843 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2879336 |  3844 | `	if( pObj == 0 ){` |
|       28 |  3845 | `		if( pInstr->iP1 ){` |
|        - |  3846 | `			/* Variable not found,load NULL */` |
|       28 |  3847 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3848 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3849 | `			}else{` |
|       28 |  3850 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3851 | `			}` |
|       28 |  3852 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1439572 |  3853 | `			break;` |
|      ! 0 |  3854 | `		}else{` |
|        - |  3855 | `			/* Fatal error */` |
|      ! 0 |  3856 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3857 | `			goto Abort;` |
|        - |  3858 | `		}` |
|        - |  3859 | `	}` |
|        - |  3860 | `	/* Load variable contents */` |
|  2879310 |  3861 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2879310 |  3862 | `	pTos->nIdx = pObj->nIdx;` |
|  2879310 |  3863 | `	break;` |
|        - |  3864 | `				   }` |
|        - |  3865 | `/*` |
|        - |  3866 | ` * LOAD_MAP P1 * *` |
|        - |  3867 | ` *` |
|        - |  3868 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3869 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3870 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3871 | ` */` |
|    20157 |  3872 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3873 | `	ph7_hashmap *pMap;` |
|        - |  3874 | `	/* Allocate a new hashmap instance */` |
|    40316 |  3875 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    40316 |  3876 | `	if( pMap == 0 ){` |
|      ! 0 |  3877 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3878 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3879 | `		goto Abort;` |
|        - |  3880 | `	}` |
|    40316 |  3881 | `	if( pInstr->iP1 > 0 ){` |
|     2358 |  3882 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3883 | `		/* Perform the insertion */` |
|     7216 |  3884 | `		while( pEntry < pTos ){` |
|     4860 |  3885 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3886 | `				/* Insertion by reference */` |
|      142 |  3887 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3888 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3889 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3890 | `					);` |
|       48 |  3891 | `			}else{` |
|        - |  3892 | `				/* Standard insertion */` |
|     7148 |  3893 | `				PH7_HashmapInsert(pMap,` |
|     4764 |  3894 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2382 |  3895 | `					&pEntry[1]` |
|        - |  3896 | `				);` |
|        - |  3897 | `			}` |
|        - |  3898 | `			/* Next pair on the stack */` |
|     4860 |  3899 | `			pEntry += 2;` |
|        2 |  3900 | `		}` |
|        - |  3901 | `		/* Pop P1 elements */` |
|     2358 |  3902 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1178 |  3903 | `	}` |
|        - |  3904 | `	/* Push the hashmap */` |
|    40316 |  3905 | `	pTos++;` |
|    40316 |  3906 | `	pTos->nIdx = SXU32_HIGH;` |
|    40316 |  3907 | `	pTos->x.pOther = pMap;` |
|    40316 |  3908 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    40316 |  3909 | `	break;` |
|        - |  3910 | `					  }` |
|        - |  3911 | `/*` |
|        - |  3912 | ` * LOAD_LIST: P1 * *` |
|        - |  3913 | ` *` |
|        - |  3914 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3915 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3916 | ` * Caveats:` |
|        - |  3917 | ` *  This implementation support only a single nesting level.` |
|        - |  3918 | ` */` |
|       48 |  3919 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3920 | `	ph7_value *pEntry;` |
|       98 |  3921 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3922 | `		/* Empty list,break immediately */` |
|      ! 0 |  3923 | `		break;` |
|        - |  3924 | `	}` |
|       98 |  3925 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3926 | `#ifdef UNTRUST` |
|        - |  3927 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3928 | `		goto Abort;` |
|        - |  3929 | `	}` |
|        - |  3930 | `#endif` |
|       98 |  3931 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  3932 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3933 | `		ph7_hashmap_node *pNode;` |
|        - |  3934 | `		ph7_value sKey,*pObj;` |
|        - |  3935 | `		/* Start Copying */` |
|       91 |  3936 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  3937 | `		while( pEntry <= pTos ){` |
|      193 |  3938 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  3939 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  3940 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  3941 | `					if( rc == SXRET_OK ){` |
|        - |  3942 | `						/* Store node value */` |
|      165 |  3943 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  3944 | `					}else{` |
|        - |  3945 | `						/* Undefined array key */` |
|        - |  3946 | `						char zMsg[128];` |
|      ! 0 |  3947 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  3948 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3949 | `						PH7_MemObjRelease(pObj);` |
|        - |  3950 | `					}` |
|       82 |  3951 | `				}` |
|       82 |  3952 | `			}` |
|      193 |  3953 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  3954 | `			pEntry++;` |
|        1 |  3955 | `		}` |
|       46 |  3956 | `	}else{` |
|        - |  3957 | `		/* Source is not an array */` |
|        - |  3958 | `		ph7_value *pObj;` |
|       18 |  3959 | `		while( pEntry <= pTos ){` |
|       12 |  3960 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  3961 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  3962 | `					PH7_MemObjRelease(pObj);` |
|        5 |  3963 | `				}` |
|        5 |  3964 | `			}` |
|       12 |  3965 | `			pEntry++;` |
|        2 |  3966 | `		}` |
|        8 |  3967 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  3968 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  3969 | `			const char *zType = "unknown";` |
|        3 |  3970 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  3971 | `			char zMsg[256];` |
|        3 |  3972 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  3973 | `				zType = "string";` |
|        1 |  3974 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  3975 | `				zType = "int";` |
|      ! 0 |  3976 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3977 | `				zType = "float";` |
|      ! 0 |  3978 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3979 | `				zType = "object";` |
|      ! 0 |  3980 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  3981 | `				zType = "resource";` |
|      ! 0 |  3982 | `			}` |
|        3 |  3983 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  3984 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  3985 | `		}` |
|        - |  3986 | `	}` |
|       98 |  3987 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  3988 | `	break;` |
|        - |  3989 | `					   }` |
|        - |  3990 | `/*` |
|        - |  3991 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3992 | ` *` |
|        - |  3993 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3994 | ` * from the stack.` |
|        - |  3995 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3996 | ` * instead.` |
|        - |  3997 | ` */` |
|   231387 |  3998 | `case PH7_OP_LOAD_IDX: {` |
|   462820 |  3999 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   462820 |  4000 | `	ph7_hashmap *pMap = 0;` |
|        - |  4001 | `	ph7_value *pIdx;` |
|   462820 |  4002 | `	pIdx = 0;` |
|   462820 |  4003 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4004 | `		if( !pInstr->iP2){` |
|        - |  4005 | `			/* No available index,load NULL */` |
|      ! 0 |  4006 | `			if( pTos >= pStack ){` |
|      ! 0 |  4007 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4008 | `			}else{` |
|        - |  4009 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4010 | `				pTos++;` |
|      ! 0 |  4011 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4012 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4013 | `			}` |
|        - |  4014 | `			/* Emit a notice */` |
|      ! 0 |  4015 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4016 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4017 | `			break;` |
|        - |  4018 | `		}` |
|      ! 0 |  4019 | `	}else{` |
|   462820 |  4020 | `		pIdx = pTos;` |
|   462820 |  4021 | `		pTos--;` |
|        - |  4022 | `	}` |
|   462820 |  4023 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4024 | `		/* String access */` |
|   362270 |  4025 | `		if( pIdx ){` |
|        - |  4026 | `			sxu32 nOfft;` |
|   362270 |  4027 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4028 | `				/* Force an int cast */` |
|      ! 0 |  4029 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4030 | `			}` |
|   362270 |  4031 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   362270 |  4032 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4033 | `				/* Invalid offset,load null */` |
|      ! 0 |  4034 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4035 | `			}else{` |
|   362270 |  4036 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   362270 |  4037 | `				int c = zData[nOfft];` |
|   362270 |  4038 | `				PH7_MemObjRelease(pTos);` |
|   362270 |  4039 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   362270 |  4040 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4041 | `			}` |
|   181158 |  4042 | `		}else{` |
|        - |  4043 | `			/* No available index,load NULL */` |
|      ! 0 |  4044 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4045 | `		}` |
|   362270 |  4046 | `		break;` |
|        - |  4047 | `	}` |
|   100552 |  4048 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4049 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4050 | `			ph7_value *pObj;` |
|        3 |  4051 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4052 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4053 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4054 | `			}` |
|        1 |  4055 | `		}` |
|        1 |  4056 | `	}` |
|   100552 |  4057 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   100552 |  4058 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   100552 |  4059 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4060 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4061 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4062 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4063 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      881 |  4064 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      440 |  4065 | `		}` |
|        - |  4066 | `		/* Point to the hashmap */` |
|   100552 |  4067 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   100552 |  4068 | `		if( pIdx ){` |
|        - |  4069 | `			/* Load the desired entry */` |
|   100552 |  4070 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    50275 |  4071 | `		}` |
|   100552 |  4072 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4073 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4074 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4075 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4076 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4077 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4078 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4079 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4080 | `			 * correct for the outermost write. */` |
|       19 |  4081 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4082 | `			if( !needWrite && pNode ){` |
|       13 |  4083 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4084 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4085 | `					needWrite = 1;` |
|        3 |  4086 | `				}` |
|        6 |  4087 | `			}` |
|       19 |  4088 | `			if( needWrite ){` |
|       13 |  4089 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4090 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4091 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4092 | `					 * into the new map's storage. */` |
|        7 |  4093 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4094 | `					if( pIdx ){` |
|        7 |  4095 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4096 | `					}` |
|        3 |  4097 | `				}` |
|        6 |  4098 | `			}` |
|        9 |  4099 | `		}` |
|   100552 |  4100 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4101 | `			/* Create a new empty entry */` |
|      273 |  4102 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4103 | `			if( rc == SXRET_OK ){` |
|        - |  4104 | `				/* Point to the last inserted entry */` |
|      273 |  4105 | `				pNode = pMap->pLast;` |
|      136 |  4106 | `			}` |
|      136 |  4107 | `		}` |
|    50275 |  4108 | `	}` |
|   100552 |  4109 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4110 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4111 | `		char zMsg[128];` |
|      ! 0 |  4112 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4113 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4114 | `		}` |
|      ! 0 |  4115 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4116 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4117 | `	}` |
|   100552 |  4118 | `	if( pIdx ){` |
|   100552 |  4119 | `		PH7_MemObjRelease(pIdx);` |
|    50275 |  4120 | `	}` |
|   100552 |  4121 | `	if( rc == SXRET_OK ){` |
|        - |  4122 | `		/* Load entry contents */` |
|    45328 |  4123 | `		if( pMap->iRef < 2 ){` |
|        - |  4124 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4125 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4126 | `			 */` |
|       24 |  4127 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4128 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4129 | `		}else{` |
|    45306 |  4130 | `			pTos->nIdx = pNode->nValIdx;` |
|    45306 |  4131 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    45306 |  4132 | `			PH7_HashmapUnref(pMap);` |
|        - |  4133 | `		}` |
|    22665 |  4134 | `	}else{` |
|        - |  4135 | `		/* No such entry,load NULL */` |
|    55226 |  4136 | `		PH7_MemObjRelease(pTos);` |
|    55226 |  4137 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4138 | `	}` |
|   100552 |  4139 | `	break;` |
|        - |  4140 | `					  }` |
|        - |  4141 | `/*` |
|        - |  4142 | ` * LOAD_CLOSURE * * P3` |
|        - |  4143 | ` *` |
|        - |  4144 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4145 | ` * name in the stack.` |
|        - |  4146 | ` */` |
|       44 |  4147 | `case PH7_OP_LOAD_CLOSURE:{` |
|       89 |  4148 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       89 |  4149 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4150 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4151 | `		ph7_vm_func *pClosure;` |
|        - |  4152 | `		char *zName;` |
|        - |  4153 | `		sxu32 mLen;` |
|        - |  4154 | `		sxu32 n;` |
|        - |  4155 | `		/* Create a new VM function */` |
|       89 |  4156 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4157 | `		/* Generate an unique closure name */` |
|       89 |  4158 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       89 |  4159 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4160 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4161 | `			goto Abort;` |
|        - |  4162 | `		}` |
|       89 |  4163 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       89 |  4164 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4165 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4166 | `		}` |
|        - |  4167 | `		/* Zero the stucture */` |
|       89 |  4168 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4169 | `		/* Perform a structure assignment on read-only items */` |
|       89 |  4170 | `		pClosure->aArgs = pFunc->aArgs;` |
|       89 |  4171 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       89 |  4172 | `		pClosure->aStatic = pFunc->aStatic;` |
|       89 |  4173 | `		pClosure->iFlags = pFunc->iFlags;` |
|       89 |  4174 | `		pClosure->pUserData = pFunc->pUserData;` |
|       89 |  4175 | `		pClosure->sSignature = pFunc->sSignature;` |
|       89 |  4176 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       89 |  4177 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       89 |  4178 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       89 |  4179 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       89 |  4180 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4181 | `		/* Register the closure */` |
|       89 |  4182 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4183 | `		/* Set up closure environment */` |
|       89 |  4184 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       89 |  4185 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      241 |  4186 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4187 | `			ph7_value *pValue;` |
|      153 |  4188 | `			pEnv = &aEnv[n];` |
|      153 |  4189 | `			sEnv.sName  = pEnv->sName;` |
|      153 |  4190 | `			sEnv.iFlags = pEnv->iFlags;` |
|      153 |  4191 | `			sEnv.nIdx = SXU32_HIGH;` |
|      153 |  4192 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      153 |  4193 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4194 | `				/* Pass by reference */` |
|      ! 0 |  4195 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4196 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4197 | `					);` |
|      ! 0 |  4198 | `			}` |
|        - |  4199 | `			/* Standard pass by value */` |
|      153 |  4200 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      153 |  4201 | `			if( pValue ){` |
|        - |  4202 | `				/* Copy imported value */` |
|       69 |  4203 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       34 |  4204 | `			}` |
|        - |  4205 | `			/* Insert the imported variable */` |
|      153 |  4206 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       77 |  4207 | `		}` |
|        - |  4208 | `		/* Finally,load the closure name on the stack */` |
|       89 |  4209 | `		pTos++;` |
|       89 |  4210 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       44 |  4211 | `	}` |
|       89 |  4212 | `	break;` |
|        - |  4213 | `						 }` |
|        - |  4214 | `/*` |
|        - |  4215 | ` * STORE * P2 P3` |
|        - |  4216 | ` *` |
|        - |  4217 | ` * Perform a store (Assignment) operation.` |
|        - |  4218 | ` */` |
|   124856 |  4219 | `case PH7_OP_STORE: {` |
|        - |  4220 | `	ph7_value *pObj;` |
|        - |  4221 | `	SyString sName;` |
|        - |  4222 | `#ifdef UNTRUST` |
|        - |  4223 | `	if( pTos < pStack ){` |
|        - |  4224 | `		goto Abort;` |
|        - |  4225 | `	}` |
|        - |  4226 | `#endif` |
|   249714 |  4227 | `	if( pInstr->iP2 ){` |
|        - |  4228 | `		sxu32 nIdx;` |
|        - |  4229 | `		sxi32 rcT;` |
|        - |  4230 | `		/* Member store operation */` |
|     3616 |  4231 | `		nIdx = pTos->nIdx;` |
|     3616 |  4232 | `		VmPopOperand(&pTos,1);` |
|     3616 |  4233 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4234 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4235 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4236 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4237 | `		}else{` |
|        - |  4238 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4239 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     3612 |  4240 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     3612 |  4241 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4242 | `				goto Abort;` |
|        - |  4243 | `			}` |
|     3612 |  4244 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4245 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4246 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4247 | `				 * propagate out of the VM loop. */` |
|       35 |  4248 | `				VmPopOperand(&pTos,1);` |
|        - |  4249 | `				{` |
|       35 |  4250 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       35 |  4251 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       35 |  4252 | `						pc = pFrm2->iExceptionJump - 1;` |
|   124874 |  4253 | `						break;` |
|        - |  4254 | `					}` |
|        - |  4255 | `				}` |
|      ! 0 |  4256 | `				goto Exception;` |
|        - |  4257 | `			}` |
|        - |  4258 | `			/* Point to the desired memory object */` |
|     3578 |  4259 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3578 |  4260 | `			if( pObj ){` |
|        - |  4261 | `				/* Perform the store operation */` |
|     3578 |  4262 | `				PH7_MemObjStore(pTos,pObj);` |
|     1788 |  4263 | `			}` |
|        - |  4264 | `		}` |
|     3582 |  4265 | `		break;` |
|   246100 |  4266 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4267 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4268 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4269 | `			/* Force a string cast */` |
|      ! 0 |  4270 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4271 | `		}` |
|        7 |  4272 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4273 | `		pTos--;` |
|        - |  4274 | `#ifdef UNTRUST` |
|        - |  4275 | `		if( pTos < pStack  ){` |
|        - |  4276 | `			goto Abort;` |
|        - |  4277 | `		}` |
|        - |  4278 | `#endif` |
|        4 |  4279 | `	}else{` |
|   246094 |  4280 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4281 | `	}` |
|        - |  4282 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   246100 |  4283 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   246100 |  4284 | `	if( pObj == 0 ){` |
|      ! 0 |  4285 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4286 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4287 | `		goto Abort;` |
|        - |  4288 | `	}` |
|   246100 |  4289 | `	if( !pInstr->p3 ){` |
|        7 |  4290 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4291 | `	}` |
|        - |  4292 | `	/* Perform the store operation */` |
|   246100 |  4293 | `	PH7_MemObjStore(pTos,pObj);` |
|   246100 |  4294 | `	break;` |
|        - |  4295 | `				   }` |
|        - |  4296 | `/*` |
|        - |  4297 | ` * STORE_IDX:   P1 * P3` |
|        - |  4298 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4299 | ` *` |
|        - |  4300 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4301 | ` */` |
|    88323 |  4302 | `case PH7_OP_STORE_IDX:` |
|        - |  4303 | `case PH7_OP_STORE_IDX_REF: {` |
|   176648 |  4304 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4305 | `	ph7_value *pKey;` |
|        - |  4306 | `	sxu32 nIdx;` |
|   176648 |  4307 | `	if( pInstr->iP1 ){` |
|        - |  4308 | `		/* Key is next on stack */` |
|    59884 |  4309 | `		pKey = pTos;` |
|    59884 |  4310 | `		pTos--;` |
|    29943 |  4311 | `	}else{` |
|   116766 |  4312 | `		pKey = 0;` |
|        - |  4313 | `	}` |
|   176648 |  4314 | `	nIdx = pTos->nIdx;` |
|   176648 |  4315 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4316 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4317 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4318 | `		 * checking true sharing count, then re-add after separation. */` |
|   176596 |  4319 | `		if( nIdx != SXU32_HIGH ){` |
|   176596 |  4320 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   264893 |  4321 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   176596 |  4322 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4323 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4324 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4325 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4326 | `				 * refcounts if the backing array was already separated. */` |
|   176596 |  4327 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   176596 |  4328 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   176596 |  4329 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   176596 |  4330 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   176596 |  4331 | `					pTos->x.pOther = pMap;` |
|    88299 |  4332 | `				}else{` |
|        - |  4333 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4334 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4335 | `					pMap = pCur;` |
|        - |  4336 | `				}` |
|    88299 |  4337 | `			}else{` |
|      ! 0 |  4338 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4339 | `			}` |
|    88299 |  4340 | `		}else{` |
|      ! 0 |  4341 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4342 | `		}` |
|   176596 |  4343 | `		if( pMap->iRef < 2 ){` |
|        - |  4344 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4345 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4346 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4347 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4348 | `			pMap->iRef = 2;` |
|      ! 0 |  4349 | `		}` |
|    88299 |  4350 | `	}else{` |
|        - |  4351 | `		ph7_value *pObj;` |
|       53 |  4352 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4353 | `		if( pObj == 0 ){` |
|      ! 0 |  4354 | `			if( pKey ){` |
|      ! 0 |  4355 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4356 | `			}` |
|      ! 0 |  4357 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4358 | `			break;` |
|        - |  4359 | `		}` |
|        - |  4360 | `		/* Phase#1: Load the array */` |
|       53 |  4361 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4362 | `			VmPopOperand(&pTos,1);` |
|       53 |  4363 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4364 | `				/* Force a string cast */` |
|      ! 0 |  4365 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4366 | `			}` |
|       53 |  4367 | `			if( pKey == 0 ){` |
|        - |  4368 | `				/* Append string */` |
|        3 |  4369 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4370 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4371 | `				}` |
|        2 |  4372 | `			}else{` |
|        - |  4373 | `				sxu32 nOfft;` |
|       51 |  4374 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4375 | `					/* Force an int cast */` |
|       51 |  4376 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4377 | `				}` |
|       51 |  4378 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4379 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4380 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4381 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4382 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4383 | `				}else{` |
|      ! 0 |  4384 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4385 | `						/* Perform an append operation */` |
|      ! 0 |  4386 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4387 | `					}` |
|        - |  4388 | `				}` |
|        - |  4389 | `			}` |
|       53 |  4390 | `			if( pKey ){` |
|       51 |  4391 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4392 | `			}` |
|       53 |  4393 | `			break;` |
|      ! 0 |  4394 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4395 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4396 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4397 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4398 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4399 | `				goto Abort;` |
|        - |  4400 | `			}` |
|      ! 0 |  4401 | `		}` |
|        - |  4402 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4403 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4404 | `	}` |
|   176596 |  4405 | `	VmPopOperand(&pTos,1);` |
|        - |  4406 | `	/* Phase#2: Perform the insertion */` |
|   176596 |  4407 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4408 | `		/* Insertion by reference */` |
|       15 |  4409 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4410 | `	}else{` |
|   176582 |  4411 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4412 | `	}` |
|   176596 |  4413 | `	if( pKey ){` |
|    59834 |  4414 | `		PH7_MemObjRelease(pKey);` |
|    29916 |  4415 | `	}` |
|   176596 |  4416 | `	break;` |
|        - |  4417 | `					   }` |
|        - |  4418 | `/*` |
|        - |  4419 | ` * INCR: P1 * *` |
|        - |  4420 | ` *` |
|        - |  4421 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4422 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4423 | ` * the stack and increment after that.` |
|        - |  4424 | ` */` |
|   158515 |  4425 | `case PH7_OP_INCR:` |
|        - |  4426 | `#ifdef UNTRUST` |
|        - |  4427 | `	if( pTos < pStack ){` |
|        - |  4428 | `		goto Abort;` |
|        - |  4429 | `	}` |
|        - |  4430 | `#endif` |
|   317076 |  4431 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   317076 |  4432 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4433 | `			ph7_value *pObj;` |
|   317076 |  4434 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4435 | `				/* Force a numeric cast */` |
|   317076 |  4436 | `				PH7_MemObjToNumeric(pObj);` |
|   317076 |  4437 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4438 | `					pObj->rVal++;` |
|        - |  4439 | `					/* Try to get an integer representation */` |
|      ! 0 |  4440 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4441 | `				}else{` |
|   317076 |  4442 | `					pObj->x.iVal++;` |
|   317076 |  4443 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4444 | `				}` |
|   317076 |  4445 | `				if( pInstr->iP1 ){` |
|        - |  4446 | `					/* Pre-icrement */` |
|       77 |  4447 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4448 | `				}` |
|   158559 |  4449 | `			}` |
|   158561 |  4450 | `		}else{` |
|      ! 0 |  4451 | `			if( pInstr->iP1 ){` |
|        - |  4452 | `				/* Force a numeric cast */` |
|      ! 0 |  4453 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  4454 | `				/* Pre-increment */` |
|      ! 0 |  4455 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4456 | `					pTos->rVal++;` |
|        - |  4457 | `					/* Try to get an integer representation */` |
|      ! 0 |  4458 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4459 | `				}else{` |
|      ! 0 |  4460 | `					pTos->x.iVal++;` |
|      ! 0 |  4461 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4462 | `				}` |
|      ! 0 |  4463 | `			}` |
|        - |  4464 | `		}` |
|   158559 |  4465 | `	}` |
|   317076 |  4466 | `	break;` |
|        - |  4467 | `/*` |
|        - |  4468 | ` * DECR: P1 * *` |
|        - |  4469 | ` *` |
|        - |  4470 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  4471 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  4472 | ` * and decrement after that.` |
|        - |  4473 | ` */` |
|        2 |  4474 | `case PH7_OP_DECR:` |
|        - |  4475 | `#ifdef UNTRUST` |
|        - |  4476 | `	if( pTos < pStack ){` |
|        - |  4477 | `		goto Abort;` |
|        - |  4478 | `	}` |
|        - |  4479 | `#endif` |
|        5 |  4480 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  4481 | `		/* Force a numeric cast */` |
|        5 |  4482 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  4483 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4484 | `			ph7_value *pObj;` |
|        5 |  4485 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4486 | `				/* Force a numeric cast */` |
|        5 |  4487 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  4488 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4489 | `					pObj->rVal--;` |
|        - |  4490 | `					/* Try to get an integer representation */` |
|      ! 0 |  4491 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4492 | `				}else{` |
|        5 |  4493 | `					pObj->x.iVal--;` |
|        5 |  4494 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4495 | `				}` |
|        5 |  4496 | `				if( pInstr->iP1 ){` |
|        - |  4497 | `					/* Pre-icrement */` |
|      ! 0 |  4498 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  4499 | `				}` |
|        2 |  4500 | `			}` |
|        3 |  4501 | `		}else{` |
|      ! 0 |  4502 | `			if( pInstr->iP1 ){` |
|        - |  4503 | `				/* Pre-increment */` |
|      ! 0 |  4504 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4505 | `					pTos->rVal--;` |
|        - |  4506 | `					/* Try to get an integer representation */` |
|      ! 0 |  4507 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4508 | `				}else{` |
|      ! 0 |  4509 | `					pTos->x.iVal--;` |
|      ! 0 |  4510 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4511 | `				}` |
|      ! 0 |  4512 | `			}` |
|        - |  4513 | `		}` |
|        2 |  4514 | `	}` |
|        5 |  4515 | `	break;` |
|        - |  4516 | `/*` |
|        - |  4517 | ` * UMINUS: * * *` |
|        - |  4518 | ` *` |
|        - |  4519 | ` * Perform a unary minus operation.` |
|        - |  4520 | ` */` |
|    26191 |  4521 | `case PH7_OP_UMINUS:` |
|        - |  4522 | `#ifdef UNTRUST` |
|        - |  4523 | `	if( pTos < pStack ){` |
|        - |  4524 | `		goto Abort;` |
|        - |  4525 | `	}` |
|        - |  4526 | `#endif` |
|        - |  4527 | `	/* Force a numeric (integer,real or both) cast */` |
|    52384 |  4528 | `	PH7_MemObjToNumeric(pTos);` |
|    52384 |  4529 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  4530 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  4531 | `	}` |
|    52384 |  4532 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    52354 |  4533 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    26176 |  4534 | `	}` |
|    52384 |  4535 | `	break;` |
|        - |  4536 | `/*` |
|        - |  4537 | ` * UPLUS: * * *` |
|        - |  4538 | ` *` |
|        - |  4539 | ` * Perform a unary plus operation.` |
|        - |  4540 | ` */` |
|       18 |  4541 | `case PH7_OP_UPLUS:` |
|        - |  4542 | `#ifdef UNTRUST` |
|        - |  4543 | `	if( pTos < pStack ){` |
|        - |  4544 | `		goto Abort;` |
|        - |  4545 | `	}` |
|        - |  4546 | `#endif` |
|        - |  4547 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  4548 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  4549 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4550 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  4551 | `	}` |
|       37 |  4552 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  4553 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  4554 | `	}` |
|       37 |  4555 | `	break;` |
|        - |  4556 | `/*` |
|        - |  4557 | ` * OP_LNOT: * * *` |
|        - |  4558 | ` *` |
|        - |  4559 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  4560 | ` * with its complement.` |
|        - |  4561 | ` */` |
|    42114 |  4562 | `case PH7_OP_LNOT:` |
|        - |  4563 | `#ifdef UNTRUST` |
|        - |  4564 | `	if( pTos < pStack ){` |
|        - |  4565 | `		goto Abort;` |
|        - |  4566 | `	}` |
|        - |  4567 | `#endif` |
|        - |  4568 | `	/* Force a boolean cast */` |
|    84274 |  4569 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  4570 | `		PH7_MemObjToBool(pTos);` |
|       10 |  4571 | `	}` |
|    84274 |  4572 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    84274 |  4573 | `	break;` |
|        - |  4574 | `/*` |
|        - |  4575 | ` * OP_BITNOT: * * *` |
|        - |  4576 | ` *` |
|        - |  4577 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  4578 | ` * with its ones-complement.` |
|        - |  4579 | ` */` |
|       13 |  4580 | `case PH7_OP_BITNOT:` |
|        - |  4581 | `#ifdef UNTRUST` |
|        - |  4582 | `	if( pTos < pStack ){` |
|        - |  4583 | `		goto Abort;` |
|        - |  4584 | `	}` |
|        - |  4585 | `#endif` |
|        - |  4586 | `	/* Force an integer cast */` |
|       28 |  4587 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4588 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4589 | `	}` |
|       28 |  4590 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  4591 | `	break;` |
|        - |  4592 | `/* OP_MUL * * *` |
|        - |  4593 | ` * OP_MUL_STORE * * *` |
|        - |  4594 | ` *` |
|        - |  4595 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  4596 | ` * and push the result back onto the stack.` |
|        - |  4597 | ` */` |
|     1272 |  4598 | `case PH7_OP_MUL:` |
|        - |  4599 | `case PH7_OP_MUL_STORE: {` |
|     2546 |  4600 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4601 | `	/* Force the operand to be numeric */` |
|        - |  4602 | `#ifdef UNTRUST` |
|        - |  4603 | `	if( pNos < pStack ){` |
|        - |  4604 | `		goto Abort;` |
|        - |  4605 | `	}` |
|        - |  4606 | `#endif` |
|     2546 |  4607 | `	PH7_MemObjToNumeric(pTos);` |
|     2546 |  4608 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  4609 | `	/* Perform the requested operation */` |
|     2546 |  4610 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4611 | `		/* Floating point arithemic */` |
|        - |  4612 | `		ph7_real a,b,r;` |
|       19 |  4613 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  4614 | `			PH7_MemObjToReal(pTos);` |
|        4 |  4615 | `		}` |
|       19 |  4616 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4617 | `			PH7_MemObjToReal(pNos);` |
|        3 |  4618 | `		}` |
|       19 |  4619 | `		a = pNos->rVal;` |
|       19 |  4620 | `		b = pTos->rVal;` |
|       19 |  4621 | `		r = a * b;` |
|        - |  4622 | `		/* Push the result */` |
|       19 |  4623 | `		pNos->rVal = r;` |
|       19 |  4624 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4625 | `		/* Try to get an integer representation */` |
|       19 |  4626 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  4627 | `	}else{` |
|        - |  4628 | `		/* Integer arithmetic */` |
|        - |  4629 | `		sxi64 a,b,r;` |
|     2528 |  4630 | `		a = pNos->x.iVal;` |
|     2528 |  4631 | `		b = pTos->x.iVal;` |
|     2528 |  4632 | `		r = a * b;` |
|        - |  4633 | `		/* Push the result */` |
|     2528 |  4634 | `		pNos->x.iVal = r;` |
|     2528 |  4635 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4636 | `	}` |
|     2546 |  4637 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4638 | `		ph7_value *pObj;` |
|       32 |  4639 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4640 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  4641 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  4642 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  4643 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  4644 | `		}` |
|       15 |  4645 | `	}` |
|     2546 |  4646 | `	VmPopOperand(&pTos,1);` |
|     2546 |  4647 | `	break;` |
|        - |  4648 | `				 }` |
|        - |  4649 | `/* OP_ADD * * *` |
|        - |  4650 | ` *` |
|        - |  4651 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4652 | ` * and push the result back onto the stack.` |
|        - |  4653 | ` */` |
|      487 |  4654 | `case PH7_OP_ADD:{` |
|      976 |  4655 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4656 | `#ifdef UNTRUST` |
|        - |  4657 | `	if( pNos < pStack ){` |
|        - |  4658 | `		goto Abort;` |
|        - |  4659 | `	}` |
|        - |  4660 | `#endif` |
|        - |  4661 | `	/* Perform the addition */` |
|      976 |  4662 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      976 |  4663 | `	VmPopOperand(&pTos,1);` |
|      976 |  4664 | `	break;` |
|        - |  4665 | `				}` |
|        - |  4666 | `/*` |
|        - |  4667 | ` * OP_ADD_STORE * * *` |
|        - |  4668 | ` *` |
|        - |  4669 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4670 | ` * and push the result back onto the stack.` |
|        - |  4671 | ` */` |
|      497 |  4672 | `case PH7_OP_ADD_STORE:{` |
|      996 |  4673 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4674 | `	ph7_value *pObj;` |
|        - |  4675 | `	sxu32 nIdx;` |
|        - |  4676 | `#ifdef UNTRUST` |
|        - |  4677 | `	if( pNos < pStack ){` |
|        - |  4678 | `		goto Abort;` |
|        - |  4679 | `	}` |
|        - |  4680 | `#endif` |
|        - |  4681 | `	/* Perform the addition */` |
|      996 |  4682 | `	nIdx = pTos->nIdx;` |
|      996 |  4683 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4684 | `	/* Peform the store operation */` |
|      996 |  4685 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4686 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      996 |  4687 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      996 |  4688 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|      996 |  4689 | `		PH7_MemObjStore(pTos,pObj);` |
|      497 |  4690 | `	}` |
|        - |  4691 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      996 |  4692 | `	PH7_MemObjStore(pTos,pNos);` |
|      996 |  4693 | `	VmPopOperand(&pTos,1);` |
|      996 |  4694 | `	break;` |
|        - |  4695 | `				}` |
|        - |  4696 | `/* OP_SUB * * *` |
|        - |  4697 | ` *` |
|        - |  4698 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4699 | ` * first (what was next on the stack) from the second (the` |
|        - |  4700 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4701 | ` */` |
|      302 |  4702 | `case PH7_OP_SUB: {` |
|      606 |  4703 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4704 | `#ifdef UNTRUST` |
|        - |  4705 | `	if( pNos < pStack ){` |
|        - |  4706 | `		goto Abort;` |
|        - |  4707 | `	}` |
|        - |  4708 | `#endif` |
|      606 |  4709 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4710 | `		/* Floating point arithemic */` |
|        - |  4711 | `		ph7_real a,b,r;` |
|       95 |  4712 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4713 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4714 | `		}` |
|       95 |  4715 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4716 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4717 | `		}` |
|       95 |  4718 | `		a = pNos->rVal;` |
|       95 |  4719 | `		b = pTos->rVal;` |
|       95 |  4720 | `		r = a - b;` |
|        - |  4721 | `		/* Push the result */` |
|       95 |  4722 | `		pNos->rVal = r;` |
|       95 |  4723 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4724 | `		/* Try to get an integer representation */` |
|       95 |  4725 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4726 | `	}else{` |
|        - |  4727 | `		/* Integer arithmetic */` |
|        - |  4728 | `		sxi64 a,b,r;` |
|      512 |  4729 | `		a = pNos->x.iVal;` |
|      512 |  4730 | `		b = pTos->x.iVal;` |
|      512 |  4731 | `		r = a - b;` |
|        - |  4732 | `		/* Push the result */` |
|      512 |  4733 | `		pNos->x.iVal = r;` |
|      512 |  4734 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4735 | `	}` |
|      606 |  4736 | `	VmPopOperand(&pTos,1);` |
|      606 |  4737 | `	break;` |
|        - |  4738 | `				 }` |
|        - |  4739 | `/* OP_SUB_STORE * * *` |
|        - |  4740 | ` *` |
|        - |  4741 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4742 | ` * first (what was next on the stack) from the second (the` |
|        - |  4743 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4744 | ` */` |
|        4 |  4745 | `case PH7_OP_SUB_STORE: {` |
|       10 |  4746 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4747 | `	ph7_value *pObj;` |
|        - |  4748 | `#ifdef UNTRUST` |
|        - |  4749 | `	if( pNos < pStack ){` |
|        - |  4750 | `		goto Abort;` |
|        - |  4751 | `	}` |
|        - |  4752 | `#endif` |
|       10 |  4753 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4754 | `		/* Floating point arithemic */` |
|        - |  4755 | `		ph7_real a,b,r;` |
|      ! 0 |  4756 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4757 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4758 | `		}` |
|      ! 0 |  4759 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4760 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4761 | `		}` |
|      ! 0 |  4762 | `		a = pTos->rVal;` |
|      ! 0 |  4763 | `		b = pNos->rVal;` |
|      ! 0 |  4764 | `		r = a - b;` |
|        - |  4765 | `		/* Push the result */` |
|      ! 0 |  4766 | `		pNos->rVal = r;` |
|      ! 0 |  4767 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4768 | `		/* Try to get an integer representation */` |
|      ! 0 |  4769 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4770 | `	}else{` |
|        - |  4771 | `		/* Integer arithmetic */` |
|        - |  4772 | `		sxi64 a,b,r;` |
|       10 |  4773 | `		a = pTos->x.iVal;` |
|       10 |  4774 | `		b = pNos->x.iVal;` |
|       10 |  4775 | `		r = a - b;` |
|        - |  4776 | `		/* Push the result */` |
|       10 |  4777 | `		pNos->x.iVal = r;` |
|       10 |  4778 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4779 | `	}` |
|       10 |  4780 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4781 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  4782 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  4783 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  4784 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  4785 | `	}` |
|       10 |  4786 | `	VmPopOperand(&pTos,1);` |
|       10 |  4787 | `	break;` |
|        - |  4788 | `				 }` |
|        - |  4789 |  |
|        - |  4790 | `/*` |
|        - |  4791 | ` * OP_MOD * * *` |
|        - |  4792 | ` *` |
|        - |  4793 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4794 | ` * first (what was next on the stack) from the second (the` |
|        - |  4795 | ` * top of the stack) and push the remainder after division` |
|        - |  4796 | ` * onto the stack.` |
|        - |  4797 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4798 | ` */` |
|      307 |  4799 | `case PH7_OP_MOD:{` |
|      616 |  4800 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4801 | `	sxi64 a,b,r;` |
|        - |  4802 | `#ifdef UNTRUST` |
|        - |  4803 | `	if( pNos < pStack ){` |
|        - |  4804 | `		goto Abort;` |
|        - |  4805 | `	}` |
|        - |  4806 | `#endif` |
|        - |  4807 | `	/* Force the operands to be integer */` |
|      616 |  4808 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4809 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4810 | `	}` |
|      616 |  4811 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4812 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4813 | `	}` |
|        - |  4814 | `	/* Perform the requested operation */` |
|      616 |  4815 | `	a = pNos->x.iVal;` |
|      616 |  4816 | `	b = pTos->x.iVal;` |
|      616 |  4817 | `	if( b == 0 ){` |
|        3 |  4818 | `		r = 0;` |
|        3 |  4819 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4820 | `		/* goto Abort; */` |
|        2 |  4821 | `	}else{` |
|      613 |  4822 | `		r = a%b;` |
|        - |  4823 | `	}` |
|        - |  4824 | `	/* Push the result */` |
|      616 |  4825 | `	pNos->x.iVal = r;` |
|      616 |  4826 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      616 |  4827 | `	VmPopOperand(&pTos,1);` |
|      616 |  4828 | `	break;` |
|        - |  4829 | `				}` |
|        - |  4830 | `/*` |
|        - |  4831 | ` * OP_MOD_STORE * * *` |
|        - |  4832 | ` *` |
|        - |  4833 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4834 | ` * first (what was next on the stack) from the second (the` |
|        - |  4835 | ` * top of the stack) and push the remainder after division` |
|        - |  4836 | ` * onto the stack.` |
|        - |  4837 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4838 | ` */` |
|        1 |  4839 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4840 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4841 | `	ph7_value *pObj;` |
|        - |  4842 | `	sxi64 a,b,r;` |
|        - |  4843 | `#ifdef UNTRUST` |
|        - |  4844 | `	if( pNos < pStack ){` |
|        - |  4845 | `		goto Abort;` |
|        - |  4846 | `	}` |
|        - |  4847 | `#endif` |
|        - |  4848 | `	/* Force the operands to be integer */` |
|        3 |  4849 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4850 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4851 | `	}` |
|        3 |  4852 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4853 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4854 | `	}` |
|        - |  4855 | `	/* Perform the requested operation */` |
|        3 |  4856 | `	a = pTos->x.iVal;` |
|        3 |  4857 | `	b = pNos->x.iVal;` |
|        3 |  4858 | `	if( b == 0 ){` |
|      ! 0 |  4859 | `		r = 0;` |
|      ! 0 |  4860 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4861 | `		/* goto Abort; */` |
|      ! 0 |  4862 | `	}else{` |
|        3 |  4863 | `		r = a%b;` |
|        - |  4864 | `	}` |
|        - |  4865 | `	/* Push the result */` |
|        3 |  4866 | `	pNos->x.iVal = r;` |
|        3 |  4867 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4868 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4869 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4870 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4871 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  4872 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4873 | `	}` |
|        3 |  4874 | `	VmPopOperand(&pTos,1);` |
|        3 |  4875 | `	break;` |
|        - |  4876 | `				}` |
|        - |  4877 | `/*` |
|        - |  4878 | ` * OP_DIV * * *` |
|        - |  4879 | ` *` |
|        - |  4880 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4881 | ` * first (what was next on the stack) from the second (the` |
|        - |  4882 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4883 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4884 | ` */` |
|       30 |  4885 | `case PH7_OP_DIV:{` |
|       62 |  4886 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4887 | `	ph7_real a,b,r;` |
|        - |  4888 | `#ifdef UNTRUST` |
|        - |  4889 | `	if( pNos < pStack ){` |
|        - |  4890 | `		goto Abort;` |
|        - |  4891 | `	}` |
|        - |  4892 | `#endif` |
|        - |  4893 | `	/* Force the operands to be real */` |
|       62 |  4894 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       58 |  4895 | `		PH7_MemObjToReal(pTos);` |
|       28 |  4896 | `	}` |
|       62 |  4897 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       24 |  4898 | `		PH7_MemObjToReal(pNos);` |
|       11 |  4899 | `	}` |
|        - |  4900 | `	/* Perform the requested operation */` |
|       62 |  4901 | `	a = pNos->rVal;` |
|       62 |  4902 | `	b = pTos->rVal;` |
|       62 |  4903 | `	if( b == 0 ){` |
|        - |  4904 | `		/* Division by zero */` |
|        3 |  4905 | `		pNos->rVal = 0;` |
|        3 |  4906 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4907 | `		/* goto Abort; */` |
|        2 |  4908 | `	}else{` |
|       59 |  4909 | `		r = a/b;` |
|        - |  4910 | `		/* Push the result */` |
|       59 |  4911 | `		pNos->rVal = r;` |
|       59 |  4912 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4913 | `		/* Try to get an integer representation */` |
|       59 |  4914 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4915 | `	}` |
|       62 |  4916 | `	VmPopOperand(&pTos,1);` |
|       62 |  4917 | `	break;` |
|        - |  4918 | `				}` |
|        - |  4919 | `/*` |
|        - |  4920 | ` * OP_DIV_STORE * * *` |
|        - |  4921 | ` *` |
|        - |  4922 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4923 | ` * first (what was next on the stack) from the second (the` |
|        - |  4924 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4925 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4926 | ` */` |
|        2 |  4927 | `case PH7_OP_DIV_STORE:{` |
|        5 |  4928 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4929 | `	ph7_value *pObj;` |
|        - |  4930 | `	ph7_real a,b,r;` |
|        - |  4931 | `#ifdef UNTRUST` |
|        - |  4932 | `	if( pNos < pStack ){` |
|        - |  4933 | `		goto Abort;` |
|        - |  4934 | `	}` |
|        - |  4935 | `#endif` |
|        - |  4936 | `	/* Force the operands to be real */` |
|        5 |  4937 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4938 | `		PH7_MemObjToReal(pTos);` |
|        2 |  4939 | `	}` |
|        5 |  4940 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4941 | `		PH7_MemObjToReal(pNos);` |
|        2 |  4942 | `	}` |
|        - |  4943 | `	/* Perform the requested operation */` |
|        5 |  4944 | `	a = pTos->rVal;` |
|        5 |  4945 | `	b = pNos->rVal;` |
|        5 |  4946 | `	if( b == 0 ){` |
|        - |  4947 | `		/* Division by zero */` |
|      ! 0 |  4948 | `		r = 0;` |
|      ! 0 |  4949 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4950 | `		/* goto Abort; */` |
|      ! 0 |  4951 | `	}else{` |
|        5 |  4952 | `		r = a/b;` |
|        - |  4953 | `		/* Push the result */` |
|        5 |  4954 | `		pNos->rVal = r;` |
|        5 |  4955 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4956 | `		/* Try to get an integer representation */` |
|        5 |  4957 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4958 | `	}` |
|        5 |  4959 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4960 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  4961 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  4962 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  4963 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  4964 | `	}` |
|        5 |  4965 | `	VmPopOperand(&pTos,1);` |
|        5 |  4966 | `	break;` |
|        - |  4967 | `				}` |
|        - |  4968 | `/* OP_BAND * * *` |
|        - |  4969 | ` *` |
|        - |  4970 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4971 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4972 | ` * two elements.` |
|        - |  4973 | `*/` |
|        - |  4974 | `/* OP_BOR * * *` |
|        - |  4975 | ` *` |
|        - |  4976 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4977 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4978 | ` * two elements.` |
|        - |  4979 | ` */` |
|        - |  4980 | `/* OP_BXOR * * *` |
|        - |  4981 | ` *` |
|        - |  4982 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4983 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4984 | ` * two elements.` |
|        - |  4985 | ` */` |
|       44 |  4986 | `case PH7_OP_BAND:` |
|        - |  4987 | `case PH7_OP_BOR:` |
|        - |  4988 | `case PH7_OP_BXOR:{` |
|       90 |  4989 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4990 | `	sxi64 a,b,r;` |
|        - |  4991 | `#ifdef UNTRUST` |
|        - |  4992 | `	if( pNos < pStack ){` |
|        - |  4993 | `		goto Abort;` |
|        - |  4994 | `	}` |
|        - |  4995 | `#endif` |
|        - |  4996 | `	/* Force the operands to be integer */` |
|       90 |  4997 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4998 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4999 | `	}` |
|       90 |  5000 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5001 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5002 | `	}` |
|        - |  5003 | `	/* Perform the requested operation */` |
|       90 |  5004 | `	a = pNos->x.iVal;` |
|       90 |  5005 | `	b = pTos->x.iVal;` |
|       90 |  5006 | `	switch(pInstr->iOp){` |
|        7 |  5007 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5008 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5009 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5010 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5011 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5012 | `	case PH7_OP_BAND:` |
|       62 |  5013 | `	default:          r = a&b; break;` |
|        - |  5014 | `	}` |
|        - |  5015 | `	/* Push the result */` |
|       90 |  5016 | `	pNos->x.iVal = r;` |
|       90 |  5017 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5018 | `	VmPopOperand(&pTos,1);` |
|       90 |  5019 | `	break;` |
|        - |  5020 | `				 }` |
|        - |  5021 | `/* OP_BAND_STORE * * *` |
|        - |  5022 | ` *` |
|        - |  5023 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5024 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5025 | ` * two elements.` |
|        - |  5026 | `*/` |
|        - |  5027 | `/* OP_BOR_STORE * * *` |
|        - |  5028 | ` *` |
|        - |  5029 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5030 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5031 | ` * two elements.` |
|        - |  5032 | ` */` |
|        - |  5033 | `/* OP_BXOR_STORE * * *` |
|        - |  5034 | ` *` |
|        - |  5035 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5036 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5037 | ` * two elements.` |
|        - |  5038 | ` */` |
|       10 |  5039 | `case PH7_OP_BAND_STORE:` |
|        - |  5040 | `case PH7_OP_BOR_STORE:` |
|        - |  5041 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5042 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5043 | `	ph7_value *pObj;` |
|        - |  5044 | `	sxi64 a,b,r;` |
|        - |  5045 | `#ifdef UNTRUST` |
|        - |  5046 | `	if( pNos < pStack ){` |
|        - |  5047 | `		goto Abort;` |
|        - |  5048 | `	}` |
|        - |  5049 | `#endif` |
|        - |  5050 | `	/* Force the operands to be integer */` |
|       21 |  5051 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5052 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5053 | `	}` |
|       21 |  5054 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5055 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5056 | `	}` |
|        - |  5057 | `	/* Perform the requested operation */` |
|       21 |  5058 | `	a = pTos->x.iVal;` |
|       21 |  5059 | `	b = pNos->x.iVal;` |
|       21 |  5060 | `	switch(pInstr->iOp){` |
|        3 |  5061 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5062 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5063 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5064 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5065 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5066 | `	case PH7_OP_BAND:` |
|        7 |  5067 | `	default:          r = a&b; break;` |
|        - |  5068 | `	}` |
|        - |  5069 | `	/* Push the result */` |
|       21 |  5070 | `	pNos->x.iVal = r;` |
|       21 |  5071 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5072 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5073 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5074 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5075 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5076 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5077 | `	}` |
|       21 |  5078 | `	VmPopOperand(&pTos,1);` |
|       21 |  5079 | `	break;` |
|        - |  5080 | `				 }` |
|        - |  5081 | `/* OP_SHL * * *` |
|        - |  5082 | ` *` |
|        - |  5083 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5084 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5085 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5086 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5087 | ` */` |
|        - |  5088 | `/* OP_SHR * * *` |
|        - |  5089 | ` *` |
|        - |  5090 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5091 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5092 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5093 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5094 | ` */` |
|       12 |  5095 | `case PH7_OP_SHL:` |
|        - |  5096 | `case PH7_OP_SHR: {` |
|       25 |  5097 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5098 | `	sxi64 a,r;` |
|        - |  5099 | `	sxi32 b;` |
|        - |  5100 | `#ifdef UNTRUST` |
|        - |  5101 | `	if( pNos < pStack ){` |
|        - |  5102 | `		goto Abort;` |
|        - |  5103 | `	}` |
|        - |  5104 | `#endif` |
|        - |  5105 | `	/* Force the operands to be integer */` |
|       25 |  5106 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5107 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5108 | `	}` |
|       25 |  5109 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5110 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5111 | `	}` |
|        - |  5112 | `	/* Perform the requested operation */` |
|       25 |  5113 | `	a = pNos->x.iVal;` |
|       25 |  5114 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5115 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5116 | `		r = a << b;` |
|        8 |  5117 | `	}else{` |
|       11 |  5118 | `		r = a >> b;` |
|        - |  5119 | `	}` |
|        - |  5120 | `	/* Push the result */` |
|       25 |  5121 | `	pNos->x.iVal = r;` |
|       25 |  5122 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5123 | `	VmPopOperand(&pTos,1);` |
|       25 |  5124 | `	break;` |
|        - |  5125 | `				 }` |
|        - |  5126 | `/*  OP_SHL_STORE * * *` |
|        - |  5127 | ` *` |
|        - |  5128 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5129 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5130 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5131 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5132 | ` */` |
|        - |  5133 | `/* OP_SHR_STORE * * *` |
|        - |  5134 | ` *` |
|        - |  5135 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5136 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5137 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5138 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5139 | ` */` |
|        9 |  5140 | `case PH7_OP_SHL_STORE:` |
|        - |  5141 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5142 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5143 | `	ph7_value *pObj;` |
|        - |  5144 | `	sxi64 a,r;` |
|        - |  5145 | `	sxi32 b;` |
|        - |  5146 | `#ifdef UNTRUST` |
|        - |  5147 | `	if( pNos < pStack ){` |
|        - |  5148 | `		goto Abort;` |
|        - |  5149 | `	}` |
|        - |  5150 | `#endif` |
|        - |  5151 | `	/* Force the operands to be integer */` |
|       19 |  5152 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5153 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5154 | `	}` |
|       19 |  5155 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5156 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5157 | `	}` |
|        - |  5158 | `	/* Perform the requested operation */` |
|       19 |  5159 | `	a = pTos->x.iVal;` |
|       19 |  5160 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5161 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5162 | `		r = a << b;` |
|        5 |  5163 | `	}else{` |
|       11 |  5164 | `		r = a >> b;` |
|        - |  5165 | `	}` |
|        - |  5166 | `	/* Push the result */` |
|       19 |  5167 | `	pNos->x.iVal = r;` |
|       19 |  5168 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5169 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5170 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5171 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5172 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5173 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5174 | `	}` |
|       19 |  5175 | `	VmPopOperand(&pTos,1);` |
|       19 |  5176 | `	break;` |
|        - |  5177 | `				 }` |
|        - |  5178 | `/* CAT:  P1 * *` |
|        - |  5179 | ` *` |
|        - |  5180 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5181 | ` * back.` |
|        - |  5182 | ` */` |
|    66567 |  5183 | `case PH7_OP_CAT:{` |
|        - |  5184 | `	ph7_value *pNos,*pCur;` |
|   133136 |  5185 | `	if( pInstr->iP1 < 1 ){` |
|   105958 |  5186 | `		pNos = &pTos[-1];` |
|    52980 |  5187 | `	}else{` |
|    27180 |  5188 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5189 | `	}` |
|        - |  5190 | `#ifdef UNTRUST` |
|        - |  5191 | `	if( pNos < pStack ){` |
|        - |  5192 | `		goto Abort;` |
|        - |  5193 | `	}` |
|        - |  5194 | `#endif` |
|        - |  5195 | `	/* Force a string cast */` |
|   133136 |  5196 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1636 |  5197 | `		PH7_MemObjToString(pNos);` |
|      817 |  5198 | `	}` |
|   133136 |  5199 | `	pCur = &pNos[1];` |
|   268520 |  5200 | `	while( pCur <= pTos ){` |
|   135386 |  5201 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50764 |  5202 | `			PH7_MemObjToString(pCur);` |
|    25381 |  5203 | `		}` |
|        - |  5204 | `		/* Perform the concatenation */` |
|   135386 |  5205 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   135344 |  5206 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    67671 |  5207 | `		}` |
|   135386 |  5208 | `		SyBlobRelease(&pCur->sBlob);` |
|   135386 |  5209 | `		pCur++;` |
|        2 |  5210 | `	}` |
|   133136 |  5211 | `	pTos = pNos;` |
|   133136 |  5212 | `	break;` |
|        - |  5213 | `				}` |
|        - |  5214 | `/*  CAT_STORE: * * *` |
|        - |  5215 | ` *` |
|        - |  5216 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5217 | ` * back.` |
|        - |  5218 | ` */` |
|     3632 |  5219 | `case PH7_OP_CAT_STORE:{` |
|     7266 |  5220 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5221 | `	ph7_value *pObj;` |
|        - |  5222 | `#ifdef UNTRUST` |
|        - |  5223 | `	if( pNos < pStack ){` |
|        - |  5224 | `		goto Abort;` |
|        - |  5225 | `	}` |
|        - |  5226 | `#endif` |
|     7266 |  5227 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5228 | `		/* Force a string cast */` |
|        3 |  5229 | `		PH7_MemObjToString(pTos);` |
|        1 |  5230 | `	}` |
|     7266 |  5231 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5232 | `		/* Force a string cast */` |
|      ! 0 |  5233 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5234 | `	}` |
|        - |  5235 | `	/* Perform the concatenation (Reverse order) */` |
|     7266 |  5236 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7266 |  5237 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3632 |  5238 | `	}` |
|        - |  5239 | `	/* Perform the store operation */` |
|     7266 |  5240 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5241 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7266 |  5242 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7266 |  5243 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7264 |  5244 | `		PH7_MemObjStore(pTos,pObj);` |
|     3631 |  5245 | `	}` |
|     7264 |  5246 | `	PH7_MemObjStore(pTos,pNos);` |
|     7264 |  5247 | `	VmPopOperand(&pTos,1);` |
|     7264 |  5248 | `	break;` |
|        - |  5249 | `				}` |
|        - |  5250 | `/* OP_AND: * * *` |
|        - |  5251 | ` *` |
|        - |  5252 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5253 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5254 | ` * stack.` |
|        - |  5255 | ` */` |
|        - |  5256 | `/* OP_OR: * * *` |
|        - |  5257 | ` *` |
|        - |  5258 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5259 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5260 | ` * stack.` |
|        - |  5261 | ` */` |
|   100447 |  5262 | `case PH7_OP_LAND:` |
|        - |  5263 | `case PH7_OP_LOR: {` |
|   200940 |  5264 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5265 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5266 | `#ifdef UNTRUST` |
|        - |  5267 | `	if( pNos < pStack ){` |
|        - |  5268 | `		goto Abort;` |
|        - |  5269 | `	}` |
|        - |  5270 | `#endif` |
|        - |  5271 | `	/* Force a boolean cast */` |
|   200940 |  5272 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5273 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5274 | `	}` |
|   200940 |  5275 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5276 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5277 | `	}` |
|   200940 |  5278 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   200940 |  5279 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   200940 |  5280 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5281 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    92600 |  5282 | `		v1 = and_logic[v1*3+v2];` |
|    46323 |  5283 | `	}else{` |
|        - |  5284 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   108342 |  5285 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5286 | `	}` |
|   200940 |  5287 | `	if( v1 == 2 ){` |
|      ! 0 |  5288 | `		v1 = 1;` |
|      ! 0 |  5289 | `	}` |
|   200940 |  5290 | `	VmPopOperand(&pTos,1);` |
|   200940 |  5291 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   200940 |  5292 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   200940 |  5293 | `	break;` |
|        - |  5294 | `				 }` |
|        - |  5295 | `/*` |
|        - |  5296 | ` * OP_NULLC: * * *` |
|        - |  5297 | ` * Null coalescing operator '??'.` |
|        - |  5298 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5299 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5300 | ` */` |
|        - |  5301 | `/*` |
|        - |  5302 | ` * OP_NULLC: * P2 *` |
|        - |  5303 | ` * Short-circuit null coalescing '??'.` |
|        - |  5304 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  5305 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  5306 | ` */` |
|       19 |  5307 | `case PH7_OP_NULLC: {` |
|        - |  5308 | `#ifdef UNTRUST` |
|        - |  5309 | `	if( pTos < pStack ){` |
|        - |  5310 | `		goto Abort;` |
|        - |  5311 | `	}` |
|        - |  5312 | `#endif` |
|       40 |  5313 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5314 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  5315 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  5316 | `	}else{` |
|        - |  5317 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  5318 | `		VmPopOperand(&pTos, 1);` |
|        - |  5319 | `	}` |
|       40 |  5320 | `	break;` |
|        - |  5321 |  |
|        - |  5322 | `/*` |
|        - |  5323 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  5324 | ` * Null coalescing assignment short-circuit.` |
|        - |  5325 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  5326 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  5327 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  5328 | ` */` |
|       23 |  5329 | `case PH7_OP_NULLC_JMP: {` |
|        - |  5330 | `#ifdef UNTRUST` |
|        - |  5331 | `	if( pTos < pStack ){` |
|        - |  5332 | `		goto Abort;` |
|        - |  5333 | `	}` |
|        - |  5334 | `#endif` |
|       47 |  5335 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  5336 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  5337 | `	}` |
|       47 |  5338 | `	break;` |
|        - |  5339 |  |
|        - |  5340 | `/*` |
|        - |  5341 | ` * OP_NULLC_STORE: * * *` |
|        - |  5342 | ` * Null coalescing assignment store.` |
|        - |  5343 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  5344 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  5345 | ` * expression result.` |
|        - |  5346 | ` */` |
|       14 |  5347 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  5348 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5349 | `	ph7_value *pObj;` |
|        - |  5350 | `	sxu32 nIdx;` |
|        - |  5351 | `#ifdef UNTRUST` |
|        - |  5352 | `	if( pNos < pStack ){` |
|        - |  5353 | `		goto Abort;` |
|        - |  5354 | `	}` |
|        - |  5355 | `#endif` |
|       29 |  5356 | `	nIdx = pNos->nIdx;` |
|       29 |  5357 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5358 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5359 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  5360 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  5361 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  5362 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  5363 | `	}` |
|       29 |  5364 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  5365 | `	VmPopOperand(&pTos,1);` |
|       29 |  5366 | `	break;` |
|        - |  5367 |  |
|        - |  5368 | `/*` |
|        - |  5369 | ` * OP_SPREAD: * * *` |
|        - |  5370 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  5371 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  5372 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  5373 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  5374 | ` */` |
|        7 |  5375 | `case PH7_OP_SPREAD: {` |
|        - |  5376 | `#ifdef UNTRUST` |
|        - |  5377 | `	if( pTos < pStack ){` |
|        - |  5378 | `		goto Abort;` |
|        - |  5379 | `	}` |
|        - |  5380 | `#endif` |
|       15 |  5381 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  5382 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  5383 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  5384 | `		if( nEntry == 0 ){` |
|        - |  5385 | `			/* Empty array — remove from stack */` |
|        3 |  5386 | `			VmPopOperand(&pTos, 1);` |
|        3 |  5387 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  5388 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  5389 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  5390 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  5391 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  5392 | `				VM_STACK_GUARD);` |
|      ! 0 |  5393 | `		}else{` |
|        - |  5394 | `			ph7_hashmap_node *pNode2;` |
|        - |  5395 | `			ph7_value *pElem;` |
|        - |  5396 | `			sxu32 i;` |
|        - |  5397 | `			/* Overwrite TOS with first element */` |
|       13 |  5398 | `			pNode2 = pMap->pFirst;` |
|       13 |  5399 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  5400 | `			PH7_MemObjRelease(pTos);` |
|       13 |  5401 | `			if( pElem ){` |
|       13 |  5402 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  5403 | `			}` |
|       13 |  5404 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5405 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  5406 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  5407 | `			pNode2 = pNode2->pPrev;` |
|        - |  5408 | `			/* Push remaining elements */` |
|       33 |  5409 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  5410 | `				pTos++;` |
|       21 |  5411 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  5412 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  5413 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  5414 | `				if( pElem ){` |
|       21 |  5415 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  5416 | `				}` |
|       21 |  5417 | `				pNode2 = pNode2->pPrev;` |
|       11 |  5418 | `			}` |
|       13 |  5419 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  5420 | `		}` |
|        7 |  5421 | `	}` |
|        - |  5422 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  5423 | `	break;` |
|        - |  5424 |  |
|        - |  5425 | `/* OP_LXOR: * * *` |
|        - |  5426 | ` *` |
|        - |  5427 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  5428 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5429 | ` * stack.` |
|        - |  5430 | ` * According to the PHP language reference manual:` |
|        - |  5431 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  5432 | ` *  TRUE,but not both.` |
|        - |  5433 | ` */` |
|        5 |  5434 | `case PH7_OP_LXOR:{` |
|       11 |  5435 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  5436 | `	sxi32 v = 0;` |
|        - |  5437 | `#ifdef UNTRUST` |
|        - |  5438 | `	if( pNos < pStack ){` |
|        - |  5439 | `		goto Abort;` |
|        - |  5440 | `	}` |
|        - |  5441 | `#endif` |
|        - |  5442 | `	/* Force a boolean cast */` |
|       11 |  5443 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5444 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  5445 | `	}` |
|       11 |  5446 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5447 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5448 | `	}` |
|       11 |  5449 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  5450 | `		v = 1;` |
|        3 |  5451 | `	}` |
|       11 |  5452 | `	VmPopOperand(&pTos,1);` |
|       11 |  5453 | `	pTos->x.iVal = v;` |
|       11 |  5454 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  5455 | `	break;` |
|        - |  5456 | `				 }` |
|        - |  5457 | `/* OP_EQ P1 P2 P3` |
|        - |  5458 | ` *` |
|        - |  5459 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  5460 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5461 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5462 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5463 | ` */` |
|        - |  5464 | `/* OP_NEQ P1 P2 P3` |
|        - |  5465 | ` *` |
|        - |  5466 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  5467 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5468 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5469 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5470 | ` */` |
|     4191 |  5471 | `case PH7_OP_EQ:` |
|        - |  5472 | `case PH7_OP_NEQ: {` |
|     8384 |  5473 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5474 | `	/* Perform the comparison and act accordingly */` |
|        - |  5475 | `#ifdef UNTRUST` |
|        - |  5476 | `	if( pNos < pStack ){` |
|        - |  5477 | `		goto Abort;` |
|        - |  5478 | `	}` |
|        - |  5479 | `#endif` |
|     8384 |  5480 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8384 |  5481 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  5482 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8375 |  5483 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8340 |  5484 | `		rc = rc == 0;` |
|     4171 |  5485 | `	}else{` |
|       28 |  5486 | `		rc = rc != 0;` |
|        - |  5487 | `	}` |
|     8384 |  5488 | `	VmPopOperand(&pTos,1);` |
|     8384 |  5489 | `	if( !pInstr->iP2 ){` |
|        - |  5490 | `		/* Push comparison result without taking the jump */` |
|     8384 |  5491 | `		PH7_MemObjRelease(pTos);` |
|     8384 |  5492 | `		pTos->x.iVal = rc;` |
|        - |  5493 | `		/* Invalidate any prior representation */` |
|     8384 |  5494 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4193 |  5495 | `	}else{` |
|      ! 0 |  5496 | `		if( rc ){` |
|        - |  5497 | `			/* Jump to the desired location */` |
|      ! 0 |  5498 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5499 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5500 | `		}` |
|        - |  5501 | `	}` |
|     8384 |  5502 | `	break;` |
|        - |  5503 | `				 }` |
|        - |  5504 | `/* OP_TEQ P1 P2 *` |
|        - |  5505 | ` *` |
|        - |  5506 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  5507 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5508 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5509 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5510 | ` */` |
|   144805 |  5511 | `case PH7_OP_TEQ: {` |
|   289612 |  5512 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5513 | `	/* Perform the comparison and act accordingly */` |
|        - |  5514 | `#ifdef UNTRUST` |
|        - |  5515 | `	if( pNos < pStack ){` |
|        - |  5516 | `		goto Abort;` |
|        - |  5517 | `	}` |
|        - |  5518 | `#endif` |
|   289612 |  5519 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   289612 |  5520 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5521 | `		rc = 0;` |
|        2 |  5522 | `	}else{` |
|   289610 |  5523 | `		rc = rc == 0;` |
|        - |  5524 | `	}` |
|   289612 |  5525 | `	VmPopOperand(&pTos,1);` |
|   289612 |  5526 | `	if( !pInstr->iP2 ){` |
|        - |  5527 | `		/* Push comparison result without taking the jump */` |
|   289612 |  5528 | `		PH7_MemObjRelease(pTos);` |
|   289612 |  5529 | `		pTos->x.iVal = rc;` |
|        - |  5530 | `		/* Invalidate any prior representation */` |
|   289612 |  5531 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   144807 |  5532 | `	}else{` |
|      ! 0 |  5533 | `		if( rc ){` |
|        - |  5534 | `			/* Jump to the desired location */` |
|      ! 0 |  5535 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5536 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5537 | `		}` |
|        - |  5538 | `	}` |
|   289612 |  5539 | `	break;` |
|        - |  5540 | `				 }` |
|        - |  5541 | `/* OP_TNE P1 P2 *` |
|        - |  5542 | ` *` |
|        - |  5543 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  5544 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  5545 | ` * instruction.` |
|        - |  5546 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5547 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5548 | ` *` |
|        - |  5549 | ` */` |
|   111794 |  5550 | `case PH7_OP_TNE: {` |
|   223590 |  5551 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5552 | `	/* Perform the comparison and act accordingly */` |
|        - |  5553 | `#ifdef UNTRUST` |
|        - |  5554 | `	if( pNos < pStack ){` |
|        - |  5555 | `		goto Abort;` |
|        - |  5556 | `	}` |
|        - |  5557 | `#endif` |
|   223590 |  5558 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   223590 |  5559 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5560 | `		rc = 1;` |
|        2 |  5561 | `	}else{` |
|   223588 |  5562 | `		rc = rc != 0;` |
|        - |  5563 | `	}` |
|   223590 |  5564 | `	VmPopOperand(&pTos,1);` |
|   223590 |  5565 | `	if( !pInstr->iP2 ){` |
|        - |  5566 | `		/* Push comparison result without taking the jump */` |
|   223590 |  5567 | `		PH7_MemObjRelease(pTos);` |
|   223590 |  5568 | `		pTos->x.iVal = rc;` |
|        - |  5569 | `		/* Invalidate any prior representation */` |
|   223590 |  5570 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   111796 |  5571 | `	}else{` |
|      ! 0 |  5572 | `		if( rc ){` |
|        - |  5573 | `			/* Jump to the desired location */` |
|      ! 0 |  5574 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5575 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5576 | `		}` |
|        - |  5577 | `	}` |
|   223590 |  5578 | `	break;` |
|        - |  5579 | `				 }` |
|        - |  5580 | `/* OP_LT P1 P2 P3` |
|        - |  5581 | ` *` |
|        - |  5582 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5583 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5584 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5585 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5586 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5587 | ` *` |
|        - |  5588 | ` */` |
|        - |  5589 | `/* OP_LE P1 P2 P3` |
|        - |  5590 | ` *` |
|        - |  5591 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5592 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5593 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5594 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5595 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5596 | ` *` |
|        - |  5597 | ` */` |
|   106678 |  5598 | `case PH7_OP_LT:` |
|        - |  5599 | `case PH7_OP_LE: {` |
|   213402 |  5600 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5601 | `	/* Perform the comparison and act accordingly */` |
|        - |  5602 | `#ifdef UNTRUST` |
|        - |  5603 | `	if( pNos < pStack ){` |
|        - |  5604 | `		goto Abort;` |
|        - |  5605 | `	}` |
|        - |  5606 | `#endif` |
|   213402 |  5607 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   213402 |  5608 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5609 | `		rc = 0;` |
|   213398 |  5610 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      594 |  5611 | `		rc = rc < 1;` |
|      298 |  5612 | `	}else{` |
|   212802 |  5613 | `		rc = rc < 0;` |
|        - |  5614 | `	}` |
|   213402 |  5615 | `	VmPopOperand(&pTos,1);` |
|   213402 |  5616 | `	if( !pInstr->iP2 ){` |
|        - |  5617 | `		/* Push comparison result without taking the jump */` |
|   213402 |  5618 | `		PH7_MemObjRelease(pTos);` |
|   213402 |  5619 | `		pTos->x.iVal = rc;` |
|        - |  5620 | `		/* Invalidate any prior representation */` |
|   213402 |  5621 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   106724 |  5622 | `	}else{` |
|      ! 0 |  5623 | `		if( rc ){` |
|        - |  5624 | `			/* Jump to the desired location */` |
|      ! 0 |  5625 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5626 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5627 | `		}` |
|        - |  5628 | `	}` |
|   213402 |  5629 | `	break;` |
|        - |  5630 | `				}` |
|        - |  5631 | `/* OP_GT P1 P2 P3` |
|        - |  5632 | ` *` |
|        - |  5633 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5634 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5635 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5636 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5637 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5638 | ` *` |
|        - |  5639 | ` */` |
|        - |  5640 | `/* OP_GE P1 P2 P3` |
|        - |  5641 | ` *` |
|        - |  5642 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5643 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5644 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5645 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5646 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5647 | ` *` |
|        - |  5648 | ` */` |
|    51589 |  5649 | `case PH7_OP_GT:` |
|        - |  5650 | `case PH7_OP_GE: {` |
|   103180 |  5651 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5652 | `	/* Perform the comparison and act accordingly */` |
|        - |  5653 | `#ifdef UNTRUST` |
|        - |  5654 | `	if( pNos < pStack ){` |
|        - |  5655 | `		goto Abort;` |
|        - |  5656 | `	}` |
|        - |  5657 | `#endif` |
|   103180 |  5658 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   103180 |  5659 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5660 | `		rc = 0;` |
|   103176 |  5661 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   103014 |  5662 | `		rc = rc >= 0;` |
|    51508 |  5663 | `	}else{` |
|      160 |  5664 | `		rc = rc > 0;` |
|        - |  5665 | `	}` |
|   103180 |  5666 | `	VmPopOperand(&pTos,1);` |
|   103180 |  5667 | `	if( !pInstr->iP2 ){` |
|        - |  5668 | `		/* Push comparison result without taking the jump */` |
|   103180 |  5669 | `		PH7_MemObjRelease(pTos);` |
|   103180 |  5670 | `		pTos->x.iVal = rc;` |
|        - |  5671 | `		/* Invalidate any prior representation */` |
|   103180 |  5672 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    51591 |  5673 | `	}else{` |
|      ! 0 |  5674 | `		if( rc ){` |
|        - |  5675 | `			/* Jump to the desired location */` |
|      ! 0 |  5676 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5677 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5678 | `		}` |
|        - |  5679 | `	}` |
|   103180 |  5680 | `	break;` |
|        - |  5681 | `				}` |
|        - |  5682 | `/* OP_SPACESHIP * * *` |
|        - |  5683 | ` *` |
|        - |  5684 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5685 | ` *   -1 if left < right` |
|        - |  5686 | ` *    0 if left == right` |
|        - |  5687 | ` *    1 if left > right` |
|        - |  5688 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5689 | ` */` |
|       25 |  5690 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5691 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5692 | `#ifdef UNTRUST` |
|        - |  5693 | `	if( pNos < pStack ){` |
|        - |  5694 | `		goto Abort;` |
|        - |  5695 | `	}` |
|        - |  5696 | `#endif` |
|       51 |  5697 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5698 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5699 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5700 | `		rc = 1;` |
|        4 |  5701 | `	}else{` |
|        - |  5702 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5703 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5704 | `	}` |
|       51 |  5705 | `	VmPopOperand(&pTos,1);` |
|       51 |  5706 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5707 | `	pTos->x.iVal = rc;` |
|       51 |  5708 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5709 | `	break;` |
|        - |  5710 | `				}` |
|        - |  5711 | `/* OP_SEQ P1 P2 *` |
|        - |  5712 | ` * Strict string comparison.` |
|        - |  5713 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5714 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5715 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5716 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5717 | ` * use PH7_OP_EQ.` |
|        - |  5718 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5719 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5720 | ` */` |
|        - |  5721 | `/* OP_SNE P1 P2 *` |
|        - |  5722 | ` * Strict string comparison.` |
|        - |  5723 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5724 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5725 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5726 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5727 | ` * use PH7_OP_EQ.` |
|        - |  5728 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5729 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5730 | ` */` |
|       18 |  5731 | `case PH7_OP_SEQ:` |
|        - |  5732 | `case PH7_OP_SNE: {` |
|       38 |  5733 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5734 | `	SyString s1,s2;` |
|        - |  5735 | `	/* Perform the comparison and act accordingly */` |
|        - |  5736 | `#ifdef UNTRUST` |
|        - |  5737 | `	if( pNos < pStack ){` |
|        - |  5738 | `		goto Abort;` |
|        - |  5739 | `	}` |
|        - |  5740 | `#endif` |
|        - |  5741 | `	/* Force a string cast */` |
|       38 |  5742 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5743 | `		PH7_MemObjToString(pTos);` |
|        2 |  5744 | `	}` |
|       38 |  5745 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5746 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5747 | `	}` |
|       38 |  5748 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5749 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5750 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5751 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5752 | `		rc = rc != 0;` |
|      ! 0 |  5753 | `	}else{` |
|       38 |  5754 | `		rc = rc == 0;` |
|        - |  5755 | `	}` |
|       38 |  5756 | `	VmPopOperand(&pTos,1);` |
|       38 |  5757 | `	if( !pInstr->iP2 ){` |
|        - |  5758 | `		/* Push comparison result without taking the jump */` |
|       38 |  5759 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5760 | `		pTos->x.iVal = rc;` |
|        - |  5761 | `		/* Invalidate any prior representation */` |
|       38 |  5762 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5763 | `	}else{` |
|      ! 0 |  5764 | `		if( rc ){` |
|        - |  5765 | `			/* Jump to the desired location */` |
|      ! 0 |  5766 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5767 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5768 | `		}` |
|        - |  5769 | `	}` |
|       38 |  5770 | `	break;` |
|        - |  5771 | `				 }` |
|        - |  5772 | `/*` |
|        - |  5773 | ` * OP_LOAD_REF * * *` |
|        - |  5774 | ` * Push the index of a referenced object on the stack.` |
|        - |  5775 | ` */` |
|       57 |  5776 | `case PH7_OP_LOAD_REF: {` |
|        - |  5777 | `	sxu32 nIdx;` |
|        - |  5778 | `#ifdef UNTRUST` |
|        - |  5779 | `	if( pTos < pStack ){` |
|        - |  5780 | `		goto Abort;` |
|        - |  5781 | `	}` |
|        - |  5782 | `#endif` |
|        - |  5783 | `	/* Extract memory object index */` |
|      115 |  5784 | `	nIdx = pTos->nIdx;` |
|      115 |  5785 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5786 | `		/* Nullify the object */` |
|       95 |  5787 | `		PH7_MemObjRelease(pTos);` |
|        - |  5788 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5789 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5790 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5791 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5792 | `	}` |
|      115 |  5793 | `	break;` |
|        - |  5794 | `					  }` |
|        - |  5795 | `/*` |
|        - |  5796 | ` * OP_STORE_REF * * P3` |
|        - |  5797 | ` * Perform an assignment operation by reference.` |
|        - |  5798 | ` */` |
|       16 |  5799 | ` case PH7_OP_STORE_REF: {` |
|       34 |  5800 | `	 SyString sName = { 0 , 0 };` |
|        - |  5801 | `	 VmFrame *pFrameLocal;` |
|        - |  5802 | `	SyHashEntry *pEntry;` |
|        - |  5803 | `	sxu32 nIdx;` |
|        - |  5804 | `#ifdef UNTRUST` |
|        - |  5805 | `	if( pTos < pStack ){` |
|        - |  5806 | `		goto Abort;` |
|        - |  5807 | `	}` |
|        - |  5808 | `#endif` |
|       34 |  5809 | `	if( pInstr->p3 == 0 ){` |
|        - |  5810 | `		char *zName;` |
|        - |  5811 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5812 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5813 | `			/* Force a string cast */` |
|      ! 0 |  5814 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5815 | `		}` |
|      ! 0 |  5816 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5817 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5818 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5819 | `			if( zName ){` |
|      ! 0 |  5820 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5821 | `			}` |
|      ! 0 |  5822 | `		}` |
|      ! 0 |  5823 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5824 | `		pTos--;` |
|      ! 0 |  5825 | `	}else{` |
|       34 |  5826 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5827 | `	}` |
|       34 |  5828 | `	nIdx = pTos->nIdx;` |
|       34 |  5829 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5830 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5831 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5832 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5833 | `		}else{` |
|        - |  5834 | `			ph7_value *pObj;` |
|        - |  5835 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5836 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5837 | `			if( pObj == 0 ){` |
|      ! 0 |  5838 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5839 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5840 | `				goto Abort;` |
|        - |  5841 | `			}` |
|        - |  5842 | `			/* Perform the store operation */` |
|      ! 0 |  5843 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5844 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5845 | `		}` |
|       34 |  5846 | `	}else if( sName.nByte > 0){` |
|       34 |  5847 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5848 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5849 | `		}else{` |
|       34 |  5850 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  5851 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5852 | `			/* Query the local frame */` |
|       34 |  5853 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  5854 | `			if( pEntry ){` |
|      ! 0 |  5855 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5856 | `			}else{` |
|       34 |  5857 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  5858 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5859 | `					/* Insert in the $GLOBALS array */` |
|       30 |  5860 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  5861 | `				}` |
|       34 |  5862 | `				if( rc == SXRET_OK ){` |
|       34 |  5863 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  5864 | `				}` |
|        - |  5865 | `			}` |
|        - |  5866 | `		}` |
|       16 |  5867 | `	}` |
|       34 |  5868 | `	break;` |
|        - |  5869 | `				 }` |
|        - |  5870 | `/*` |
|        - |  5871 | ` * OP_UPLINK P1 * *` |
|        - |  5872 | ` * Link a variable to the top active VM frame.` |
|        - |  5873 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5874 | ` */` |
|       25 |  5875 | `case PH7_OP_UPLINK: {` |
|       52 |  5876 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5877 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5878 | `		SyString sName;` |
|        - |  5879 | `		/* Perform the link */` |
|      104 |  5880 | `		while( pLink <= pTos ){` |
|       54 |  5881 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5882 | `				/* Force a string cast */` |
|      ! 0 |  5883 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5884 | `			}` |
|       54 |  5885 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5886 | `			if( sName.nByte > 0 ){` |
|       54 |  5887 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5888 | `			}` |
|       54 |  5889 | `			pLink++;` |
|        2 |  5890 | `		}` |
|       25 |  5891 | `	}` |
|       52 |  5892 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5893 | `	break;` |
|        - |  5894 | `					}` |
|        - |  5895 | `/*` |
|        - |  5896 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5897 | ` * Push an exception in the corresponding container so that` |
|        - |  5898 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5899 | ` */` |
|       79 |  5900 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      160 |  5901 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5902 | `	VmFrame *pFrameLocal;` |
|        - |  5903 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      160 |  5904 | `	pException->iFinallyDone = 0;` |
|      160 |  5905 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5906 | `	/* Create the exception frame */` |
|      160 |  5907 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      160 |  5908 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5909 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5910 | `		goto Abort;` |
|        - |  5911 | `	}` |
|        - |  5912 | `	/* Mark the special frame */` |
|      160 |  5913 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      160 |  5914 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5915 | `	/* Point to the frame that trigger the exception */` |
|      160 |  5916 | `	pFrameLocal = pFrameLocal->pParent;` |
|      160 |  5917 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      160 |  5918 | `	pException->pFrame = pFrameLocal;` |
|      160 |  5919 | `	break;` |
|        - |  5920 | `							}` |
|        - |  5921 | `/*` |
|        - |  5922 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5923 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5924 | ` */` |
|       78 |  5925 | `case PH7_OP_POP_EXCEPTION: {` |
|      158 |  5926 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      158 |  5927 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5928 | `		ph7_exception **apException;` |
|        - |  5929 | `		/* Pop the loaded exception */` |
|       28 |  5930 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5931 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5932 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5933 | `		}` |
|       13 |  5934 | `	}` |
|      158 |  5935 | `	pException->pFrame = 0;` |
|        - |  5936 | `	/* Leave the exception frame */` |
|      158 |  5937 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5938 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      158 |  5939 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5940 | `		sxi32 rcFinally;` |
|       20 |  5941 | `		pException->iFinallyDone = 1;` |
|       20 |  5942 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5943 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5944 | `			goto Abort;` |
|        - |  5945 | `		}` |
|        9 |  5946 | `	}` |
|      158 |  5947 | `	break;` |
|        - |  5948 | `							}` |
|        - |  5949 |  |
|        - |  5950 | `/*` |
|        - |  5951 | ` * OP_THROW * P2 *` |
|        - |  5952 | ` * Throw an user exception.` |
|        - |  5953 | ` */` |
|       30 |  5954 | `case PH7_OP_THROW: {` |
|       62 |  5955 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       62 |  5956 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5957 | `#ifdef UNTRUST` |
|        - |  5958 | `	if( pTos < pStack ){` |
|        - |  5959 | `		goto Abort;` |
|        - |  5960 | `	}` |
|        - |  5961 | `#endif` |
|       62 |  5962 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5963 | `	/* Tell the upper layer that an exception was thrown */` |
|       62 |  5964 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       62 |  5965 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       62 |  5966 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5967 | `		ph7_class *pException;` |
|        - |  5968 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5969 | `		 */` |
|       62 |  5970 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       62 |  5971 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5972 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5973 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5974 | `			if( rc == SXERR_ABORT ){` |
|        - |  5975 | `				/* Abort processing immediately */` |
|      ! 0 |  5976 | `				goto Abort;` |
|        - |  5977 | `			}` |
|      ! 0 |  5978 | `		}else{` |
|        - |  5979 | `			/* Throw the exception */` |
|       62 |  5980 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       62 |  5981 | `			if( rc == SXERR_ABORT ){` |
|        - |  5982 | `				/* Abort processing immediately */` |
|        9 |  5983 | `				goto Abort;` |
|        - |  5984 | `			}` |
|        - |  5985 | `		}` |
|       28 |  5986 | `	}else{` |
|        - |  5987 | `		/* Expecting a class instance */` |
|      ! 0 |  5988 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5989 | `		if( rc == SXERR_ABORT ){` |
|        - |  5990 | `			/* Abort processing immediately */` |
|      ! 0 |  5991 | `			goto Abort;` |
|        - |  5992 | `		}` |
|        - |  5993 | `	}` |
|        - |  5994 | `	/* Pop the top entry */` |
|       54 |  5995 | `	VmPopOperand(&pTos,1);` |
|        - |  5996 | `	/* Perform an unconditional jump */` |
|       54 |  5997 | `	pc = nJump - 1;` |
|       54 |  5998 | `	break;` |
|        - |  5999 | `				   }` |
|        - |  6000 | `/*` |
|        - |  6001 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6002 | ` * Prepare a foreach step.` |
|        - |  6003 | ` */` |
|     5454 |  6004 | `case PH7_OP_FOREACH_INIT: {` |
|    10910 |  6005 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6006 | `	void *pName;` |
|        - |  6007 | `#ifdef UNTRUST` |
|        - |  6008 | `	if( pTos < pStack ){` |
|        - |  6009 | `		goto Abort;` |
|        - |  6010 | `	}` |
|        - |  6011 | `#endif` |
|    10910 |  6012 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6013 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6014 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6015 | `			/* Force a string cast */` |
|      ! 0 |  6016 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6017 | `		}` |
|        - |  6018 | `		/* Duplicate name */` |
|      ! 0 |  6019 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6020 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6021 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6022 | `		}` |
|      ! 0 |  6023 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6024 | `	}` |
|    10910 |  6025 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6026 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6027 | `			/* Force a string cast */` |
|      ! 0 |  6028 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6029 | `		}` |
|        - |  6030 | `		/* Duplicate name */` |
|      ! 0 |  6031 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6032 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6033 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6034 | `		}` |
|      ! 0 |  6035 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6036 | `	}` |
|        - |  6037 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10910 |  6038 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6039 | `		/* Jump out of the loop */` |
|      ! 0 |  6040 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6041 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6042 | `		}` |
|      ! 0 |  6043 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6044 | `	}else{` |
|        - |  6045 | `		ph7_foreach_step *pStep;` |
|    10910 |  6046 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10910 |  6047 | `		if( pStep == 0 ){` |
|      ! 0 |  6048 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6049 | `			/* Jump out of the loop */` |
|      ! 0 |  6050 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6051 | `		}else{` |
|        - |  6052 | `			/* Zero the structure */` |
|    10910 |  6053 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6054 | `			/* Prepare the step */` |
|    10910 |  6055 | `			pStep->iFlags = pInfo->iFlags;` |
|    10910 |  6056 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6057 | `				ph7_hashmap *pMap;` |
|        - |  6058 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6059 | `				 * source array so mutations don't affect other sharers. */` |
|    10882 |  6060 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6061 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6062 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6063 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6064 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6065 | `						 * variable still points at the same hashmap as` |
|        - |  6066 | `						 * the stack value. */` |
|        9 |  6067 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6068 | `							pCur->iRef--;` |
|        9 |  6069 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6070 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6071 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6072 | `						}` |
|        4 |  6073 | `					}` |
|        4 |  6074 | `				}` |
|    10882 |  6075 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6076 | `				/* Reset the internal loop cursor */` |
|    10882 |  6077 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6078 | `				/* Mark the step */` |
|    10882 |  6079 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10882 |  6080 | `				pStep->xIter.pMap = pMap;` |
|    10882 |  6081 | `				pMap->iRef++;` |
|     5442 |  6082 | `			}else{` |
|       30 |  6083 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6084 | `				ph7_class *pIteratorClass;` |
|        - |  6085 | `				/* Check if the object implements Iterator */` |
|       30 |  6086 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  6087 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6088 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6089 | `					ph7_class_method *pRewind;` |
|       20 |  6090 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  6091 | `					pStep->xIter.pThis = pThis;` |
|       20 |  6092 | `					pThis->iRef++;` |
|       20 |  6093 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  6094 | `					if( pRewind ){` |
|       20 |  6095 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  6096 | `					}` |
|       11 |  6097 | `				}else{` |
|        - |  6098 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6099 | `					ph7_class *pIterAggClass;` |
|       12 |  6100 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6101 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6102 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6103 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6104 | `						ph7_class_method *pGetIter;` |
|        3 |  6105 | `						int iterAggOk = 0;` |
|        3 |  6106 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6107 | `						if( pGetIter ){` |
|        - |  6108 | `							ph7_value sResult;` |
|        3 |  6109 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6110 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6111 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6112 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6113 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6114 | `									ph7_class_method *pRewind;` |
|        3 |  6115 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6116 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6117 | `									pIterObj->iRef++;` |
|        - |  6118 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6119 | `									pStep->pOwner = pThis;` |
|        3 |  6120 | `									pThis->iRef++;` |
|        3 |  6121 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6122 | `									if( pRewind ){` |
|        3 |  6123 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6124 | `									}` |
|        3 |  6125 | `									iterAggOk = 1;` |
|        1 |  6126 | `								}` |
|        1 |  6127 | `							}` |
|        3 |  6128 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6129 | `						}` |
|        3 |  6130 | `						if( !iterAggOk ){` |
|        - |  6131 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6132 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6133 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6134 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6135 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6136 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6137 | `						}` |
|        2 |  6138 | `					}else{` |
|        - |  6139 | `						/* Plain object iteration via hAttr */` |
|        9 |  6140 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6141 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6142 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6143 | `						pThis->iRef++;` |
|        - |  6144 | `					}` |
|        - |  6145 | `				}` |
|        - |  6146 | `			}` |
|        - |  6147 | `		}` |
|    10910 |  6148 | `		if( pStep ){` |
|    10910 |  6149 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6150 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6151 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6152 | `				/* Jump out of the loop */` |
|      ! 0 |  6153 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6154 | `			}` |
|     5454 |  6155 | `		}` |
|        - |  6156 | `	}` |
|    10910 |  6157 | `	VmPopOperand(&pTos,1);` |
|    10910 |  6158 | `	break;` |
|        - |  6159 | `						  }` |
|        - |  6160 | `/*` |
|        - |  6161 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6162 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6163 | ` */` |
|    88909 |  6164 | `case PH7_OP_FOREACH_STEP: {` |
|   177820 |  6165 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6166 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6167 | `	ph7_value *pValue;` |
|        - |  6168 | `	VmFrame *pFrameLocal;` |
|        - |  6169 | `	/* Peek the last step */` |
|   177820 |  6170 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   177820 |  6171 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   177820 |  6172 | `	pFrameLocal = pVm->pFrame;` |
|   177820 |  6173 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   177820 |  6174 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   177708 |  6175 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6176 | `		ph7_hashmap_node *pNode;` |
|        - |  6177 | `		/* Extract the current node value */` |
|   177708 |  6178 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   177708 |  6179 | `		if( pNode == 0 ){` |
|        - |  6180 | `			/* No more entry to process */` |
|    10880 |  6181 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10880 |  6182 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6183 | `				/* Break the reference with the last element */` |
|        7 |  6184 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6185 | `			}` |
|        - |  6186 | `			/* Automatically reset the loop cursor */` |
|    10880 |  6187 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6188 | `			/* Cleanup the mess left behind */` |
|    10880 |  6189 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10880 |  6190 | `			SySetPop(&pInfo->aStep);` |
|    10880 |  6191 | `			PH7_HashmapUnref(pMap);` |
|     5441 |  6192 | `		}else{` |
|   166830 |  6193 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  6194 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  6195 | `				if( pKey ){` |
|      416 |  6196 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  6197 | `				}` |
|      207 |  6198 | `			}` |
|   166830 |  6199 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6200 | `				SyHashEntry *pEntry;` |
|        - |  6201 | `				/* Pass by reference */` |
|       23 |  6202 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6203 | `				if( pEntry ){` |
|       21 |  6204 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6205 | `				}else{` |
|        4 |  6206 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6207 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6208 | `				}` |
|       12 |  6209 | `			}else{` |
|        - |  6210 | `				/* Make a copy of the entry value */` |
|   166808 |  6211 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   166808 |  6212 | `				if( pValue ){` |
|   166808 |  6213 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    83403 |  6214 | `				}` |
|        - |  6215 | `			}` |
|        2 |  6216 | `		}` |
|    88967 |  6217 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6218 | `		/* Iterator-based iteration.` |
|        - |  6219 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6220 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6221 | `		 */` |
|       90 |  6222 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6223 | `		ph7_class_method *pMethod;` |
|        - |  6224 | `		ph7_value sResult;` |
|       90 |  6225 | `		int isValid = 0;` |
|        - |  6226 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  6227 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  6228 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  6229 | `		}else{` |
|       70 |  6230 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  6231 | `			if( pMethod ){` |
|       70 |  6232 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  6233 | `			}` |
|        - |  6234 | `		}` |
|        - |  6235 | `		/* Call valid() */` |
|       90 |  6236 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  6237 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  6238 | `		if( pMethod ){` |
|       90 |  6239 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  6240 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  6241 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  6242 | `		}` |
|       90 |  6243 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  6244 | `		if( !isValid ){` |
|        - |  6245 | `			/* Iterator exhausted */` |
|       20 |  6246 | `			pc = pInstr->iP2 - 1;` |
|        - |  6247 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  6248 | `			if( pStep->pOwner ){` |
|        3 |  6249 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6250 | `			}` |
|       20 |  6251 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  6252 | `			SySetPop(&pInfo->aStep);` |
|       20 |  6253 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  6254 | `		}else{` |
|        - |  6255 | `			/* Call current() to get value */` |
|       72 |  6256 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  6257 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  6258 | `			if( pMethod ){` |
|       72 |  6259 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  6260 | `			}` |
|       72 |  6261 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  6262 | `			if( pValue ){` |
|       72 |  6263 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  6264 | `			}` |
|       72 |  6265 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6266 | `			/* Call key() if needed */` |
|       72 |  6267 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6268 | `				ph7_value sKey;` |
|       35 |  6269 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6270 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6271 | `				if( pMethod ){` |
|       35 |  6272 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6273 | `				}` |
|       35 |  6274 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6275 | `				if( pValue ){` |
|       35 |  6276 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6277 | `				}` |
|       35 |  6278 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6279 | `			}` |
|        - |  6280 | `		}` |
|       46 |  6281 | `	}else{` |
|       25 |  6282 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6283 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6284 | `		SyHashEntry *pEntry;` |
|        - |  6285 | `		/* Point to the next attribute */` |
|       29 |  6286 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6287 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6288 | `			/* Check access permission */` |
|       31 |  6289 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6290 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6291 | `					break; /* Access is granted */` |
|        - |  6292 | `			}` |
|        1 |  6293 | `		}` |
|       25 |  6294 | `		if( pEntry == 0 ){` |
|        - |  6295 | `			/* Clean up the mess left behind */` |
|        9 |  6296 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6297 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6298 | `				/* Break the reference with the last element */` |
|        3 |  6299 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6300 | `			}` |
|        9 |  6301 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6302 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6303 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6304 | `		}else{` |
|       17 |  6305 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6306 | `			ph7_value *pAttrValue;` |
|       17 |  6307 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6308 | `				/* Fill with the current attribute name */` |
|       17 |  6309 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6310 | `				if( pKey ){` |
|       17 |  6311 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6312 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6313 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6314 | `				}` |
|        8 |  6315 | `			}` |
|        - |  6316 | `			/* Extract attribute value */` |
|       17 |  6317 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  6318 | `			if( pAttrValue ){` |
|       17 |  6319 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6320 | `					/* Pass by reference */` |
|        3 |  6321 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  6322 | `					if( pEntry ){` |
|        3 |  6323 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  6324 | `					}else{` |
|      ! 0 |  6325 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  6326 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  6327 | `					}` |
|        2 |  6328 | `				}else{` |
|        - |  6329 | `					/* Make a copy of the attribute value */` |
|       15 |  6330 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  6331 | `					if( pValue ){` |
|       15 |  6332 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  6333 | `					}` |
|        - |  6334 | `				}` |
|        8 |  6335 | `			}` |
|        - |  6336 | `		}` |
|        - |  6337 | `	}` |
|   177820 |  6338 | `	break;` |
|        - |  6339 | `						  }` |
|        - |  6340 | `/*` |
|        - |  6341 | ` * OP_MEMBER P1 P2` |
|        - |  6342 | ` * Load class attribute/method on the stack.` |
|        - |  6343 | ` */` |
|     2792 |  6344 | `case PH7_OP_MEMBER: {` |
|        - |  6345 | `	ph7_class_instance *pThis;` |
|        - |  6346 | `	ph7_value *pNos;` |
|        - |  6347 | `	SyString sName;` |
|     5586 |  6348 | `	if( !pInstr->iP1 ){` |
|     5368 |  6349 | `		pNos = &pTos[-1];` |
|        - |  6350 | `#ifdef UNTRUST` |
|        - |  6351 | `		if( pNos < pStack ){` |
|        - |  6352 | `			goto Abort;` |
|        - |  6353 | `		}` |
|        - |  6354 | `#endif` |
|     5368 |  6355 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6356 | `			ph7_class *pClass;` |
|        - |  6357 | `			/* Class already instantiated */` |
|     5368 |  6358 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  6359 | `			/* Point to the instantiated class */` |
|     5368 |  6360 | `			pClass = pThis->pClass;` |
|        - |  6361 | `			/* Extract attribute name first */` |
|     5368 |  6362 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     5368 |  6363 | `			if( pInstr->iP2 ){` |
|        - |  6364 | `				/* Method call */` |
|      564 |  6365 | `				ph7_class_method *pMeth = 0;` |
|      564 |  6366 | `				if( sName.nByte > 0 ){` |
|        - |  6367 | `					/* Extract the target method */` |
|      564 |  6368 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      281 |  6369 | `				}` |
|      564 |  6370 | `				if( pMeth == 0 ){` |
|      ! 0 |  6371 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  6372 | `						&pClass->sName,&sName` |
|        - |  6373 | `						);` |
|        - |  6374 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  6375 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  6376 | `					/* Pop the method name from the stack */` |
|      ! 0 |  6377 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6378 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  6379 | `				}else{` |
|        - |  6380 | `					/* Push method name on the stack */` |
|      564 |  6381 | `					PH7_MemObjRelease(pTos);` |
|      564 |  6382 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      564 |  6383 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6384 | `				}` |
|      564 |  6385 | `				pTos->nIdx = SXU32_HIGH;` |
|      283 |  6386 | `			}else{` |
|        - |  6387 | `				/* Attribute access */` |
|     4806 |  6388 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  6389 | `				SyHashEntry *pEntry;` |
|        - |  6390 | `				/* Extract the target attribute */` |
|     4806 |  6391 | `				if( sName.nByte > 0 ){` |
|     4806 |  6392 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     4806 |  6393 | `					if( pEntry ){` |
|        - |  6394 | `						/* Point to the attribute value */` |
|     4804 |  6395 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2401 |  6396 | `					}` |
|     2402 |  6397 | `				}` |
|     4806 |  6398 | `				if( pObjAttr == 0 ){` |
|        - |  6399 | `					/* No such attribute,load null */` |
|        4 |  6400 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  6401 | `						&pClass->sName,&sName);` |
|        - |  6402 | `					/* Call the __get magic method if available */` |
|        3 |  6403 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  6404 | `				}` |
|     4806 |  6405 | `				VmPopOperand(&pTos,1);` |
|        - |  6406 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  6407 | `				 * This is due to the following case:` |
|        - |  6408 | `				 *     (new TestClass())->foo;` |
|        - |  6409 | `				 */` |
|     4806 |  6410 | `				pThis->iRef++;` |
|     4806 |  6411 | `				PH7_MemObjRelease(pTos);` |
|     4806 |  6412 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     4806 |  6413 | `				if( pObjAttr ){` |
|     4804 |  6414 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  6415 | `					/* Check attribute access */` |
|     4804 |  6416 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  6417 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  6418 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  6419 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  6420 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  6421 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     4802 |  6422 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     2420 |  6423 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       36 |  6424 | `							VmInstr *pNext = pInstr + 1;` |
|       36 |  6425 | `							int bIsLhs = 0;` |
|       36 |  6426 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       34 |  6427 | `								bIsLhs = 1;` |
|       16 |  6428 | `							}` |
|       36 |  6429 | `							if( !bIsLhs ){` |
|        3 |  6430 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  6431 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  6432 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  6433 | `									goto Abort;` |
|        - |  6434 | `								}` |
|        - |  6435 | `								{` |
|        3 |  6436 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6437 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6438 | `										pc = pFrm2->iExceptionJump - 1;` |
|     2792 |  6439 | `										break;` |
|        - |  6440 | `									}` |
|        - |  6441 | `								}` |
|      ! 0 |  6442 | `								goto Exception;` |
|        - |  6443 | `							}` |
|       16 |  6444 | `						}` |
|        - |  6445 | `						/* Load attribute */` |
|     4802 |  6446 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     4802 |  6447 | `						if( pValue ){` |
|     4802 |  6448 | `							if( pThis->iRef < 2 ){` |
|        - |  6449 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  6450 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  6451 | `								 */` |
|        7 |  6452 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  6453 | `							}else{` |
|        - |  6454 | `								/* Simple load */` |
|     4796 |  6455 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  6456 | `							}` |
|     4802 |  6457 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     4800 |  6458 | `								if( pThis->iRef > 1 ){` |
|        - |  6459 | `									/* Load attribute index */` |
|     4794 |  6460 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2396 |  6461 | `								}` |
|     2399 |  6462 | `							}` |
|     2400 |  6463 | `						}` |
|     2402 |  6464 | `					}else{` |
|        - |  6465 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  6466 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  6467 | `						char zMsg[256];` |
|      ! 0 |  6468 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6469 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6470 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6471 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  6472 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6473 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6474 | `						goto Abort;` |
|        - |  6475 | `					}` |
|     2400 |  6476 | `				}` |
|        - |  6477 | `				/* Safely unreference the object */` |
|     4804 |  6478 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  6479 | `			}` |
|     2684 |  6480 | `		}else{` |
|      ! 0 |  6481 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  6482 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6483 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6484 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  6485 | `		}` |
|     2684 |  6486 | `	}else{` |
|        - |  6487 | `		/* Static member access using class name */` |
|      220 |  6488 | `		pNos = pTos;` |
|      220 |  6489 | `		pThis = 0;` |
|      220 |  6490 | `		if( !pInstr->p3 ){` |
|      186 |  6491 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      186 |  6492 | `			pNos--;` |
|        - |  6493 | `#ifdef UNTRUST` |
|        - |  6494 | `			if( pNos < pStack ){` |
|        - |  6495 | `				goto Abort;` |
|        - |  6496 | `			}` |
|        - |  6497 | `#endif` |
|       94 |  6498 | `		}else{` |
|        - |  6499 | `			/* Attribute name already computed */` |
|       36 |  6500 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6501 | `		}` |
|      220 |  6502 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      220 |  6503 | `			ph7_class *pClass = 0;` |
|      220 |  6504 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6505 | `				/* Class already instantiated */` |
|        5 |  6506 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  6507 | `				pClass = pThis->pClass;` |
|        5 |  6508 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  6509 | `			}else{` |
|        - |  6510 | `				/* Try to extract the target class */` |
|      216 |  6511 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      216 |  6512 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      216 |  6513 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  6514 | `					/* Handle self/static/parent keywords */` |
|      216 |  6515 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  6516 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  6517 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  6518 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  6519 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  6520 | `						}` |
|      186 |  6521 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  6522 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      155 |  6523 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       26 |  6524 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       26 |  6525 | `						if( pSelf && pSelf->pBase ){` |
|       26 |  6526 | `							pClass = pSelf->pBase;` |
|       12 |  6527 | `						}` |
|       14 |  6528 | `					}else{` |
|      106 |  6529 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  6530 | `					}` |
|      107 |  6531 | `				}` |
|        - |  6532 | `			}` |
|      220 |  6533 | `			if( pClass == 0 ){` |
|        - |  6534 | `				/* Undefined class */` |
|      ! 0 |  6535 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  6536 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  6537 | `					);` |
|      ! 0 |  6538 | `				if( !pInstr->p3 ){` |
|      ! 0 |  6539 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6540 | `				}` |
|      ! 0 |  6541 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6542 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  6543 | `			}else{` |
|      220 |  6544 | `				if( pInstr->iP2 ){` |
|        - |  6545 | `					/* Method call */` |
|       82 |  6546 | `					ph7_class_method *pMeth = 0;` |
|       82 |  6547 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  6548 | `						/* Extract the target method */` |
|       82 |  6549 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       40 |  6550 | `					}` |
|       82 |  6551 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  6552 | `						if( pMeth ){` |
|      ! 0 |  6553 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  6554 | `								&pClass->sName,&sName` |
|        - |  6555 | `								);` |
|      ! 0 |  6556 | `						}else{` |
|      ! 0 |  6557 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6558 | `								&pClass->sName,&sName` |
|        - |  6559 | `								);` |
|        - |  6560 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  6561 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  6562 | `						}` |
|        - |  6563 | `						/* Pop the method name from the stack */` |
|      ! 0 |  6564 | `						if( !pInstr->p3 ){` |
|      ! 0 |  6565 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  6566 | `						}` |
|      ! 0 |  6567 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  6568 | `					}else{` |
|        - |  6569 | `						/* Push method name on the stack */` |
|       82 |  6570 | `						PH7_MemObjRelease(pTos);` |
|       82 |  6571 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       82 |  6572 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6573 | `					}` |
|       82 |  6574 | `					pTos->nIdx = SXU32_HIGH;` |
|       42 |  6575 | `				}else{` |
|        - |  6576 | `					/* Attribute access */` |
|      140 |  6577 | `					ph7_class_attr *pAttr = 0;` |
|        - |  6578 | `					/* Check for special ::class pseudo-constant */` |
|      186 |  6579 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  6580 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  6581 | `						/* ::class returns the fully qualified class name */` |
|        - |  6582 | `						/* Pop the attribute name from the stack */` |
|       60 |  6583 | `						if( !pInstr->p3 ){` |
|       60 |  6584 | `							VmPopOperand(&pTos,1);` |
|       29 |  6585 | `						}` |
|       60 |  6586 | `						PH7_MemObjRelease(pTos);` |
|        - |  6587 | `						/* Load the class name */` |
|       60 |  6588 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  6589 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  6590 | `					}else{` |
|        - |  6591 | `						/* Extract the target attribute */` |
|       82 |  6592 | `						if( sName.nByte > 0 ){` |
|       82 |  6593 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       40 |  6594 | `						}` |
|       82 |  6595 | `						if( pAttr == 0 ){` |
|        - |  6596 | `							/* No such attribute,load null */` |
|      ! 0 |  6597 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6598 | `								&pClass->sName,&sName);` |
|        - |  6599 | `							/* Call the __get magic method if available */` |
|      ! 0 |  6600 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  6601 | `						}` |
|        - |  6602 | `						/* Pop the attribute name from the stack */` |
|       82 |  6603 | `						if( !pInstr->p3 ){` |
|       48 |  6604 | `							VmPopOperand(&pTos,1);` |
|       23 |  6605 | `						}` |
|       82 |  6606 | `						PH7_MemObjRelease(pTos);` |
|       82 |  6607 | `						pTos->nIdx = SXU32_HIGH;` |
|       82 |  6608 | `						if( pAttr ){` |
|       82 |  6609 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  6610 | `								/* Access to a non static attribute */` |
|      ! 0 |  6611 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6612 | `									&pClass->sName,&pAttr->sName` |
|        - |  6613 | `									);` |
|      ! 0 |  6614 | `							}else{` |
|        - |  6615 | `								ph7_value *pValue;` |
|        - |  6616 | `								/* Check if the access to the attribute is allowed */` |
|       82 |  6617 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  6618 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  6619 | `									 * Same LHS-of-store peek as the instance path. */` |
|       76 |  6620 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       51 |  6621 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       35 |  6622 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       22 |  6623 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       24 |  6624 | `										if( pS ){` |
|       24 |  6625 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       24 |  6626 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        5 |  6627 | `												VmInstr *pNext = pInstr + 1;` |
|        5 |  6628 | `												int bIsLhs = 0;` |
|        5 |  6629 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        3 |  6630 | `													bIsLhs = 1;` |
|        1 |  6631 | `												}` |
|        5 |  6632 | `												if( !bIsLhs ){` |
|        3 |  6633 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  6634 | `													if( pThis ){` |
|      ! 0 |  6635 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6636 | `													}` |
|        3 |  6637 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  6638 | `														goto Abort;` |
|        - |  6639 | `													}` |
|        - |  6640 | `													{` |
|        3 |  6641 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6642 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6643 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  6644 | `															break;` |
|        - |  6645 | `														}` |
|        - |  6646 | `													}` |
|      ! 0 |  6647 | `													goto Exception;` |
|        - |  6648 | `												}` |
|        1 |  6649 | `											}` |
|       10 |  6650 | `										}` |
|       10 |  6651 | `									}` |
|        - |  6652 | `									/* Load the desired attribute */` |
|       76 |  6653 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       76 |  6654 | `									if( pValue ){` |
|       76 |  6655 | `										PH7_MemObjLoad(pValue,pTos);` |
|       76 |  6656 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  6657 | `											/* Load index number */` |
|       34 |  6658 | `											pTos->nIdx = pAttr->nIdx;` |
|       16 |  6659 | `										}` |
|       37 |  6660 | `									}` |
|       39 |  6661 | `								}else{` |
|        - |  6662 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  6663 | `									char zMsg[256];` |
|        5 |  6664 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  6665 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  6666 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  6667 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  6668 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  6669 | `									}else{` |
|      ! 0 |  6670 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6671 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6672 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  6673 | `									}` |
|        5 |  6674 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  6675 | `									goto Abort;` |
|        - |  6676 | `								}` |
|        - |  6677 | `							}` |
|       37 |  6678 | `						}` |
|        - |  6679 | `					}` |
|        - |  6680 | `				}` |
|      214 |  6681 | `				if( pThis ){` |
|        - |  6682 | `					/* Safely unreference the object */` |
|        5 |  6683 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  6684 | `				}` |
|        - |  6685 | `			}` |
|      108 |  6686 | `		}else{` |
|        - |  6687 | `			/* Pop operands */` |
|      ! 0 |  6688 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  6689 | `			if( !pInstr->p3 ){` |
|      ! 0 |  6690 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  6691 | `			}` |
|      ! 0 |  6692 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6693 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6694 | `		}` |
|        - |  6695 | `	}` |
|     5578 |  6696 | `	break;` |
|        - |  6697 | `					}` |
|        - |  6698 | `/*` |
|        - |  6699 | ` * OP_NEW P1 * * *` |
|        - |  6700 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  6701 | ` */` |
|      419 |  6702 | `case PH7_OP_NEW: {` |
|      840 |  6703 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      840 |  6704 | `	ph7_class *pClass = 0;` |
|        - |  6705 | `	ph7_class_instance *pNew;` |
|      840 |  6706 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  6707 | `		/* Try to extract the desired class */` |
|     1259 |  6708 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      838 |  6709 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      419 |  6710 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6711 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6712 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6713 | `	}` |
|      840 |  6714 | `	if( pClass == 0 ){` |
|        - |  6715 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6716 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6717 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6718 | `			);` |
|        - |  6719 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6720 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6721 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6722 | `			/* Pop given arguments */` |
|      ! 0 |  6723 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6724 | `		}` |
|      ! 0 |  6725 | `		goto Abort;` |
|      ! 0 |  6726 | `	}else{` |
|        - |  6727 | `		ph7_class_method *pCons;` |
|        - |  6728 | `		/* Create a new class instance */` |
|      840 |  6729 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      840 |  6730 | `		if( pNew == 0 ){` |
|      ! 0 |  6731 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6732 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6733 | `				&pClass->sName` |
|        - |  6734 | `			);` |
|      ! 0 |  6735 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6736 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6737 | `				/* Pop given arguments */` |
|      ! 0 |  6738 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6739 | `			}` |
|      ! 0 |  6740 | `			break;` |
|        - |  6741 | `		}` |
|        - |  6742 | `		/* Check if a constructor is available */` |
|      840 |  6743 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      840 |  6744 | `		if( pCons == 0 ){` |
|      694 |  6745 | `			SyString *pName = &pClass->sName;` |
|        - |  6746 | `			/* Check for a constructor with the same base class name */` |
|      694 |  6747 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      346 |  6748 | `		}` |
|      840 |  6749 | `		if( pCons ){` |
|        - |  6750 | `			/* Call the class constructor */` |
|      148 |  6751 | `			SySetReset(&aArg);` |
|      286 |  6752 | `			while( pArg < pTos ){` |
|      140 |  6753 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      140 |  6754 | `				pArg++;` |
|        2 |  6755 | `			}` |
|      148 |  6756 | `			if( pVm->bErrReport ){` |
|        - |  6757 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6758 | `				sxu32 n;` |
|       57 |  6759 | `				n = SySetUsed(&aArg);` |
|        - |  6760 | `				/* Emit a notice for missing arguments */` |
|      101 |  6761 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6762 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6763 | `					if( pFuncArg ){` |
|       45 |  6764 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6765 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6766 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6767 | `						}` |
|       22 |  6768 | `					}` |
|       45 |  6769 | `					n++;` |
|        1 |  6770 | `				}` |
|       28 |  6771 | `			}` |
|      148 |  6772 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6773 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      148 |  6774 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6775 | `				pNew->iRef = 1;` |
|      ! 0 |  6776 | `			}` |
|       73 |  6777 | `		}` |
|      840 |  6778 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6779 | `			/* Pop given arguments */` |
|      130 |  6780 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       64 |  6781 | `		}` |
|      840 |  6782 | `		PH7_MemObjRelease(pTos);` |
|      840 |  6783 | `		pTos->x.pOther = pNew;` |
|      840 |  6784 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6785 | `	}` |
|      840 |  6786 | `	break;` |
|        - |  6787 | `				 }` |
|        - |  6788 | `/*` |
|        - |  6789 | ` * OP_CLONE * * *` |
|        - |  6790 | ` * Perfome a clone operation.` |
|        - |  6791 | ` */` |
|       23 |  6792 | `case PH7_OP_CLONE: {` |
|        - |  6793 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6794 | `#ifdef UNTRUST` |
|        - |  6795 | `	if( pTos < pStack ){` |
|        - |  6796 | `		goto Abort;` |
|        - |  6797 | `	}` |
|        - |  6798 | `#endif` |
|        - |  6799 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6800 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6801 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6802 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6803 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6804 | `		break;` |
|        - |  6805 | `	}` |
|        - |  6806 | `	/* Point to the source */` |
|       44 |  6807 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6808 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6809 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6810 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6811 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6812 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6813 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6814 | `		break;` |
|        - |  6815 | `	}` |
|        - |  6816 | `	/* Perform the clone operation */` |
|       44 |  6817 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6818 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6819 | `	if( pClone == 0 ){` |
|      ! 0 |  6820 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6821 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6822 | `	}else{` |
|        - |  6823 | `		/* Load the cloned object */` |
|       44 |  6824 | `		pTos->x.pOther = pClone;` |
|       44 |  6825 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6826 | `	}` |
|       44 |  6827 | `	break;` |
|        - |  6828 | `				   }` |
|        - |  6829 | `/*` |
|        - |  6830 | ` * OP_SWITCH * * P3` |
|        - |  6831 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6832 | ` */` |
|       26 |  6833 | `case PH7_OP_SWITCH: {` |
|       54 |  6834 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6835 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6836 | `	ph7_value sValue,sCaseValue;` |
|        - |  6837 | `	sxu32 n,nEntry;` |
|        - |  6838 | `#ifdef UNTRUST` |
|        - |  6839 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6840 | `		goto Abort;` |
|        - |  6841 | `	}` |
|        - |  6842 | `#endif` |
|        - |  6843 | `	/* Point to the case table  */` |
|       54 |  6844 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  6845 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6846 | `	/* Select the appropriate case block to execute */` |
|       54 |  6847 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  6848 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  6849 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  6850 | `		pCase = &aCase[n];` |
|      130 |  6851 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6852 | `		/* Execute the case expression first */` |
|      130 |  6853 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6854 | `		/* Compare the two expression */` |
|      130 |  6855 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  6856 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  6857 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  6858 | `		if( rc == 0 ){` |
|        - |  6859 | `			/* Value match,jump to this block */` |
|       52 |  6860 | `			pc = pCase->nStart - 1;` |
|       52 |  6861 | `			break;` |
|        - |  6862 | `		}` |
|       41 |  6863 | `	}` |
|       54 |  6864 | `	VmPopOperand(&pTos,1);` |
|       54 |  6865 | `	if( n >= nEntry ){` |
|        - |  6866 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  6867 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  6868 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  6869 | `		}else{` |
|        - |  6870 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6871 | `			pc = pSwitch->nOut - 1;` |
|        - |  6872 | `		}` |
|        1 |  6873 | `	}` |
|       54 |  6874 | `	break;` |
|        - |  6875 | `					}` |
|        - |  6876 | `/*` |
|        - |  6877 | ` * OP_YIELD P1 P2 *` |
|        - |  6878 | ` *  Yield a value from a generator function.` |
|        - |  6879 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6880 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6881 | ` */` |
|       28 |  6882 | `case PH7_OP_YIELD: {` |
|        - |  6883 | `	ph7_generator *pGen;` |
|       58 |  6884 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6885 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6886 | `		goto Abort;` |
|        - |  6887 | `	}` |
|       58 |  6888 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6889 | `	if( pInstr->iP2 ){` |
|        - |  6890 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6891 | `#ifdef UNTRUST` |
|        - |  6892 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6893 | `#endif` |
|        7 |  6894 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6895 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6896 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6897 | `		VmPopOperand(&pTos, 1);` |
|        - |  6898 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6899 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6900 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6901 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6902 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6903 | `			}` |
|        1 |  6904 | `		}` |
|       55 |  6905 | `	}else if( pInstr->iP1 ){` |
|        - |  6906 | `		/* yield $value */` |
|        - |  6907 | `#ifdef UNTRUST` |
|        - |  6908 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6909 | `#endif` |
|       52 |  6910 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6911 | `		VmPopOperand(&pTos, 1);` |
|        - |  6912 | `		/* Auto-increment key */` |
|       52 |  6913 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6914 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6915 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6916 | `	}else{` |
|        - |  6917 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6918 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6919 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6920 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6921 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6922 | `	}` |
|        - |  6923 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6924 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6925 | `	goto Suspend;` |
|        - |  6926 |  |
|        - |  6927 | `/*` |
|        - |  6928 | ` * OP_CALL P1 * *` |
|        - |  6929 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6930 | ` *  function on the stack.` |
|        - |  6931 | ` */` |
|   317411 |  6932 | `case PH7_OP_CALL: {` |
|   634868 |  6933 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6934 | `	ph7_value *pArg;` |
|   634868 |  6935 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   634868 |  6936 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6937 | `	SyHashEntry *pEntry;` |
|        - |  6938 | `	SyString sName;` |
|        - |  6939 | `	/* Extract function name */` |
|   634868 |  6940 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6941 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6942 | `			ph7_value sResult;` |
|      ! 0 |  6943 | `			SySetReset(&aArg);` |
|      ! 0 |  6944 | `			while( pArg < pTos ){` |
|      ! 0 |  6945 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6946 | `				pArg++;` |
|      ! 0 |  6947 | `			}` |
|      ! 0 |  6948 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6949 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6950 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6951 | `			SySetReset(&aArg);` |
|        - |  6952 | `			/* Pop given arguments */` |
|      ! 0 |  6953 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6954 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6955 | `			}` |
|        - |  6956 | `			/* Copy result */` |
|      ! 0 |  6957 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6958 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6959 | `		}else{` |
|        3 |  6960 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6961 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6962 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6963 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6964 | `			}else{` |
|        - |  6965 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6966 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6967 | `			}` |
|        - |  6968 | `			/* Pop given arguments */` |
|        3 |  6969 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6970 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6971 | `			}` |
|        - |  6972 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6973 | `			PH7_MemObjRelease(pTos);` |
|        - |  6974 | `		}` |
|   317133 |  6975 | `		break;` |
|        - |  6976 | `	}` |
|   634866 |  6977 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6978 | `	/* Check for a compiled function first.` |
|        - |  6979 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6980 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   634866 |  6981 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6982 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6983 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6984 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6985 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6986 | `	 * function calls inside namespaces. */` |
|   634866 |  6987 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6988 | `		const char *zFunc;` |
|        - |  6989 | `		const char *zEnd;` |
|        - |  6990 | `		const char *z;` |
|        - |  6991 | `		SyString sGlobal;` |
|       20 |  6992 | `		zFunc = sName.zString;` |
|       20 |  6993 | `		zEnd  = zFunc + sName.nByte;` |
|       20 |  6994 | `		z = zEnd;` |
|        - |  6995 | `		/* Find last namespace separator */` |
|      174 |  6996 | `		while( z > zFunc ){` |
|      174 |  6997 | `			if( z[-1] == '\\' ){` |
|       20 |  6998 | `				break;` |
|        - |  6999 | `			}` |
|      156 |  7000 | `			z--;` |
|        2 |  7001 | `		}` |
|       20 |  7002 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7003 | `			/* Retry lookup using the unqualified/global function name */` |
|       20 |  7004 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       20 |  7005 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        9 |  7006 | `		}` |
|        9 |  7007 | `	}` |
|   634866 |  7008 | `	if( pEntry ){` |
|        - |  7009 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7010 | `		ph7_class_instance *pThis;` |
|        - |  7011 | `		ph7_value *pFrameStack;` |
|        - |  7012 | `		ph7_vm_func *pVmFunc;` |
|        - |  7013 | `		ph7_class *pSelf;` |
|        - |  7014 | `		VmFrame *pFrame;` |
|        - |  7015 | `		ph7_value *pObj;` |
|        - |  7016 | `		VmSlot sArg;` |
|        - |  7017 | `		sxu32 n;` |
|        - |  7018 | `		/* initialize fields */` |
|    14718 |  7019 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    14718 |  7020 | `		pThis = 0;` |
|    14718 |  7021 | `		pSelf = 0;` |
|    14718 |  7022 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7023 | `			ph7_class_method *pMeth;` |
|        - |  7024 | `			/* Class method call */` |
|     2254 |  7025 | `			ph7_value *pTarget = &pTos[-1];` |
|     2254 |  7026 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7027 | `				/* Extract the 'this' pointer */` |
|     2254 |  7028 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7029 | `					/* Instance already loaded */` |
|     2168 |  7030 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2168 |  7031 | `					pThis->iRef++;` |
|     2168 |  7032 | `					pSelf = pThis->pClass;` |
|     1083 |  7033 | `				}` |
|     2254 |  7034 | `				if( pSelf == 0 ){` |
|       88 |  7035 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7036 | `						/* "Late Static Binding" class name */` |
|      122 |  7037 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       40 |  7038 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       40 |  7039 | `					}` |
|       88 |  7040 | `					if( pSelf == 0 ){` |
|       19 |  7041 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        9 |  7042 | `					}` |
|       43 |  7043 | `				}` |
|     2254 |  7044 | `				if( pThis == 0  ){` |
|       88 |  7045 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       88 |  7046 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       88 |  7047 | `					if( pFrameLocal->pParent ){` |
|        - |  7048 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       64 |  7049 | `						pThis = pFrameLocal->pThis;` |
|       64 |  7050 | `						if( pThis ){` |
|       19 |  7051 | `							pThis->iRef++;` |
|        9 |  7052 | `						}` |
|       31 |  7053 | `					}` |
|       43 |  7054 | `				}` |
|     2254 |  7055 | `				VmPopOperand(&pTos,1);` |
|     2254 |  7056 | `				PH7_MemObjRelease(pTos);` |
|        - |  7057 | `				/* Synchronize pointers */` |
|     2254 |  7058 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7059 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7060 | `				 * user have already computed the random generated unique class method name` |
|        - |  7061 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7062 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7063 | `				 */` |
|     2254 |  7064 | `				while( pArg < pStack ){` |
|      ! 0 |  7065 | `					pArg++;` |
|      ! 0 |  7066 | `				}` |
|     2254 |  7067 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7068 | `					/* Check if the call is allowed */` |
|     2254 |  7069 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2254 |  7070 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7071 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7072 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7073 | `							char zMsg[256];` |
|      ! 0 |  7074 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7075 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7076 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7077 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7078 | `							/* Pop given arguments */` |
|      ! 0 |  7079 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7080 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7081 | `							}` |
|      ! 0 |  7082 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7083 | `							goto Abort;` |
|        - |  7084 | `						}` |
|        6 |  7085 | `					}` |
|     1126 |  7086 | `				}` |
|     1126 |  7087 | `			}` |
|     1126 |  7088 | `		}` |
|        - |  7089 | `		/* Check The recursion limit */` |
|    14718 |  7090 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7091 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7092 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7093 | `				&pVmFunc->sName);` |
|        - |  7094 | `			/* Pop given arguments */` |
|        3 |  7095 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7096 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7097 | `			}` |
|        - |  7098 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7099 | `			PH7_MemObjRelease(pTos);` |
|       12 |  7100 | `			break;` |
|        - |  7101 | `		}` |
|    14716 |  7102 | `		if( pVmFunc->pNextName ){` |
|        - |  7103 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7104 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7105 | `		}` |
|    14716 |  7106 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7107 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7108 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7109 | `			ph7_generator *pGenerator;` |
|        - |  7110 | `			ph7_class_instance *pGenObj;` |
|        - |  7111 | `			ph7_value *pCtxAttr;` |
|        - |  7112 | `			SyString sAttrName;` |
|        - |  7113 | `			ph7_value **apCallArgs;` |
|        - |  7114 | `			int nGenArgs, iArg;` |
|        - |  7115 | `			/* Collect arguments from the operand stack */` |
|       20 |  7116 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  7117 | `			apCallArgs = 0;` |
|       20 |  7118 | `			if( nGenArgs > 0 ){` |
|        8 |  7119 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  7120 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  7121 | `				if( apCallArgs == 0 ){` |
|        - |  7122 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7123 | `					nGenArgs = 0;` |
|      ! 0 |  7124 | `				}else{` |
|       12 |  7125 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  7126 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  7127 | `					}` |
|        - |  7128 | `				}` |
|        2 |  7129 | `			}` |
|        - |  7130 | `			/* Create execution context and generator wrapper */` |
|       20 |  7131 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  7132 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  7133 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7134 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7135 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7136 | `				break;` |
|        - |  7137 | `			}` |
|       20 |  7138 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  7139 | `			if( pGenerator == 0 ){` |
|      ! 0 |  7140 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  7141 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7142 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7143 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7144 | `				break;` |
|        - |  7145 | `			}` |
|        - |  7146 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  7147 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  7148 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  7149 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  7150 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  7151 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  7152 | `			if( apCallArgs ){` |
|        6 |  7153 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  7154 | `			}` |
|       20 |  7155 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7156 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7157 | `				if( pThis ){` |
|      ! 0 |  7158 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7159 | `				}` |
|      ! 0 |  7160 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7161 | `					goto Abort;` |
|        - |  7162 | `				}` |
|      ! 0 |  7163 | `				break;` |
|        - |  7164 | `			}` |
|        - |  7165 | `			/* Create Generator class instance */` |
|       20 |  7166 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  7167 | `			if( pGenObj == 0 ){` |
|      ! 0 |  7168 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7169 | `				break;` |
|        - |  7170 | `			}` |
|        - |  7171 | `			/* Store generator in __ctx attribute */` |
|       20 |  7172 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  7173 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  7174 | `			if( pCtxAttr ){` |
|       20 |  7175 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  7176 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  7177 | `			}` |
|        - |  7178 | `			/* Pop args and function name, push Generator object */` |
|       20 |  7179 | `			PH7_MemObjRelease(pTos);` |
|       20 |  7180 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  7181 | `			pTos->x.pOther = pGenObj;` |
|       20 |  7182 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  7183 | `			pGenObj->iRef++;` |
|       20 |  7184 | `			if( pThis ){` |
|      ! 0 |  7185 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7186 | `			}` |
|       20 |  7187 | `			break;` |
|        - |  7188 | `		}` |
|        - |  7189 | `		/* Extract the formal argument set */` |
|    14698 |  7190 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  7191 | `		/* Create a new VM frame  */` |
|    14698 |  7192 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    14698 |  7193 | `		if( rc != SXRET_OK ){` |
|        - |  7194 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7195 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7196 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7197 | `				&pVmFunc->sName);` |
|        - |  7198 | `			/* Pop given arguments */` |
|      ! 0 |  7199 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7200 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7201 | `			}` |
|        - |  7202 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7203 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7204 | `			break;` |
|        - |  7205 | `		}` |
|    14698 |  7206 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  7207 | `			/* Install the '$this' variable */` |
|        - |  7208 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2184 |  7209 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2184 |  7210 | `			if( pObj ){` |
|        - |  7211 | `				/* Reflect the change */` |
|     2184 |  7212 | `				pObj->x.pOther = pThis;` |
|     2184 |  7213 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1091 |  7214 | `			}` |
|     1091 |  7215 | `		}` |
|    14698 |  7216 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  7217 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  7218 | `			/* Install static variables */` |
|      ! 0 |  7219 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  7220 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  7221 | `				pStatic = &aStatic[n];` |
|      ! 0 |  7222 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  7223 | `					/* Initialize the static variables */` |
|      ! 0 |  7224 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  7225 | `					if( pObj ){` |
|        - |  7226 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  7227 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  7228 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  7229 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  7230 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  7231 | `						}` |
|      ! 0 |  7232 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  7233 | `					}else{` |
|      ! 0 |  7234 | `						continue;` |
|        - |  7235 | `					}` |
|      ! 0 |  7236 | `				}` |
|        - |  7237 | `				/* Install in the current frame */` |
|      ! 0 |  7238 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  7239 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  7240 | `			}` |
|      ! 0 |  7241 | `		}` |
|        - |  7242 | `		/* Push arguments in the local frame */` |
|    14698 |  7243 | `		n = 0;` |
|    39488 |  7244 | `		while( pArg < pTos ){` |
|    24852 |  7245 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  7246 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       34 |  7247 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       34 |  7248 | `				if( pObj ){` |
|        - |  7249 | `					/* Initialize as empty array */` |
|       34 |  7250 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7251 | `					{` |
|       34 |  7252 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      130 |  7253 | `						while( pArg < pTos ){` |
|        - |  7254 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  7255 | `							 *` |
|        - |  7256 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  7257 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  7258 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  7259 | `							 * non-union variadic path below has the same limitation;` |
|        - |  7260 | `							 * fixing both wants a separate counter for elements` |
|        - |  7261 | `							 * already packed into the variadic array. */` |
|      100 |  7262 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  7263 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  7264 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       16 |  7265 | `								if( rcU != SXRET_OK ){` |
|        - |  7266 | `									const char *zGiven;` |
|        - |  7267 | `									char zBuf[128];` |
|        3 |  7268 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7269 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  7270 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7271 | `										zGiven = "null";` |
|      ! 0 |  7272 | `									}else{` |
|        3 |  7273 | `										zGiven = ph7_type_name(pArg);` |
|        - |  7274 | `									}` |
|        3 |  7275 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  7276 | `										&aFormalArg[n].sName,` |
|        2 |  7277 | `										SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|        2 |  7278 | `											? aFormalArg[n].sTypeName.zString : "union",` |
|        1 |  7279 | `										zGiven);` |
|        3 |  7280 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7281 | `										goto Abort;` |
|        - |  7282 | `									}` |
|        3 |  7283 | `									PH7_MemObjRelease(pTos);` |
|        3 |  7284 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  7285 | `									pFrameStack = 0;` |
|        3 |  7286 | `									rc = PH7_EXCEPTION;` |
|        3 |  7287 | `									goto SkipFuncBody;` |
|        - |  7288 | `								}` |
|       14 |  7289 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  7290 | `								pArg++;` |
|       14 |  7291 | `								continue;` |
|        - |  7292 | `							}` |
|        - |  7293 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  7294 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      100 |  7295 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  7296 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  7297 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  7298 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7299 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  7300 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7301 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  7302 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7303 | `										goto Abort;` |
|        - |  7304 | `									}` |
|        - |  7305 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  7306 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  7307 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7308 | `									pFrameStack = 0;` |
|      ! 0 |  7309 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  7310 | `									goto SkipFuncBody;` |
|      ! 0 |  7311 | `								}else{` |
|       13 |  7312 | `									ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  7313 | `									if( xCast ){` |
|       13 |  7314 | `										xCast(pArg);` |
|        6 |  7315 | `									}` |
|        - |  7316 | `								}` |
|        6 |  7317 | `							}` |
|       86 |  7318 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       86 |  7319 | `							pArg++;` |
|        2 |  7320 | `						}` |
|        - |  7321 | `					}` |
|       32 |  7322 | `					sArg.nIdx = pObj->nIdx;` |
|       32 |  7323 | `					sArg.pUserData = 0;` |
|       32 |  7324 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       15 |  7325 | `				}` |
|       32 |  7326 | `				break; /* All remaining args consumed */` |
|        - |  7327 | `			}` |
|    24820 |  7328 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    24664 |  7329 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       24 |  7330 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  7331 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  7332 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  7333 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7334 | `						goto Abort;` |
|        - |  7335 | `					}` |
|      ! 0 |  7336 | `				}` |
|        - |  7337 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    24666 |  7338 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       77 |  7339 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       50 |  7340 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       52 |  7341 | `					if( rcU != SXRET_OK ){` |
|        - |  7342 | `						const char *zGiven;` |
|        - |  7343 | `						char zBuf[128];` |
|       19 |  7344 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  7345 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  7346 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  7347 | `							zGiven = "null";` |
|        5 |  7348 | `						}else{` |
|        5 |  7349 | `							zGiven = ph7_type_name(pArg);` |
|        - |  7350 | `						}` |
|       19 |  7351 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  7352 | `							&aFormalArg[n].sName,` |
|       18 |  7353 | `							SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|       18 |  7354 | `								? aFormalArg[n].sTypeName.zString : "union",` |
|        9 |  7355 | `							zGiven);` |
|       19 |  7356 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  7357 | `							goto Abort;` |
|        - |  7358 | `						}` |
|       19 |  7359 | `						PH7_MemObjRelease(pTos);` |
|       19 |  7360 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  7361 | `						pFrameStack = 0;` |
|       19 |  7362 | `						rc = PH7_EXCEPTION;` |
|       19 |  7363 | `						goto SkipFuncBody;` |
|        - |  7364 | `					}` |
|       17 |  7365 | `				}else` |
|        - |  7366 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  7367 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    24636 |  7368 | `				if( aFormalArg[n].nType > 0` |
|    12924 |  7369 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1210 |  7370 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  7371 | `						/* Argument must be a class instance [i.e: object] */` |
|       20 |  7372 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  7373 | `						ph7_class *pClass;` |
|        - |  7374 | `						/* Try to extract the desired class */` |
|       20 |  7375 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  7376 | `						if( pClass ){` |
|       20 |  7377 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7378 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7379 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7380 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7381 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7382 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7383 | `								}` |
|      ! 0 |  7384 | `							}else{` |
|        - |  7385 | `								/* reuse pThis declared in outer scope */` |
|       20 |  7386 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  7387 | `								/* Make sure the object is an instance of the given class */` |
|       20 |  7388 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  7389 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7390 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7391 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7392 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7393 | `								}` |
|        - |  7394 | `							}` |
|       11 |  7395 | `						}` |
|     1201 |  7396 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       11 |  7397 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7398 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  7399 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  7400 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  7401 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7402 | `								goto Abort;` |
|        - |  7403 | `							}` |
|        - |  7404 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  7405 | `							PH7_MemObjRelease(pTos);` |
|       11 |  7406 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  7407 | `							pFrameStack = 0;` |
|       11 |  7408 | `							rc = PH7_EXCEPTION;` |
|       11 |  7409 | `							goto SkipFuncBody;` |
|      ! 0 |  7410 | `						}else{` |
|      ! 0 |  7411 | `							ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7412 | `							/* Cast to the desired type */` |
|      ! 0 |  7413 | `							xCast(pArg);` |
|        - |  7414 | `						}` |
|      ! 0 |  7415 | `					}` |
|      599 |  7416 | `				}` |
|    24638 |  7417 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  7418 | `					/* Pass by reference */` |
|       54 |  7419 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  7420 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  7421 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  7422 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7423 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7424 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7425 | `						}` |
|        - |  7426 | `						/* Switch to pass by value */` |
|      ! 0 |  7427 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7428 | `					}else{` |
|        - |  7429 | `						SyHashEntry *pRefEntry;` |
|        - |  7430 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  7431 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  7432 | `						if( pRefEntry == 0 ){` |
|       80 |  7433 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  7434 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  7435 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  7436 | `							sArg.pUserData = 0;` |
|       54 |  7437 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  7438 | `						}` |
|       54 |  7439 | `						pObj = 0;` |
|        - |  7440 | `					}` |
|       28 |  7441 | `				}else{` |
|        - |  7442 | `					/* Pass by value,make a copy of the given argument */` |
|    24586 |  7443 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7444 | `				}` |
|    12320 |  7445 | `			}else{` |
|        - |  7446 | `				char zName[32];` |
|        - |  7447 | `				SyString sArgName;` |
|        - |  7448 | `				/* Set a dummy name */` |
|      156 |  7449 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  7450 | `				sArgName.zString = zName;` |
|        - |  7451 | `				/* Annonymous argument */` |
|      156 |  7452 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  7453 | `			}` |
|    24792 |  7454 | `			if( pObj ){` |
|    24740 |  7455 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  7456 | `				/* Insert argument index  */` |
|    24740 |  7457 | `				sArg.nIdx = pObj->nIdx;` |
|    24740 |  7458 | `				sArg.pUserData = 0;` |
|    24740 |  7459 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    12369 |  7460 | `			}` |
|    24792 |  7461 | `			PH7_MemObjRelease(pArg);` |
|    24792 |  7462 | `			pArg++;` |
|    24792 |  7463 | `			++n;` |
|        2 |  7464 | `		}` |
|        - |  7465 | `		/* Set up closure environment */` |
|    14668 |  7466 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7467 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  7468 | `			ph7_value *pValue;` |
|        - |  7469 | `			sxu32 iEnv;` |
|      111 |  7470 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      287 |  7471 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      177 |  7472 | `				pEnv = &aEnv[iEnv];` |
|      177 |  7473 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  7474 | `					/* Do not install null value */` |
|      105 |  7475 | `					continue;` |
|        - |  7476 | `				}` |
|       73 |  7477 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       73 |  7478 | `				if( pValue == 0 ){` |
|      ! 0 |  7479 | `					continue;` |
|        - |  7480 | `				}` |
|        - |  7481 | `				/* Invalidate any prior representation */` |
|       73 |  7482 | `				PH7_MemObjRelease(pValue);` |
|        - |  7483 | `				/* Duplicate bound variable value */` |
|       73 |  7484 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       37 |  7485 | `			}` |
|       55 |  7486 | `		}` |
|        - |  7487 | `		/* Process default values for remaining formal parameters */` |
|    16836 |  7488 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2208 |  7489 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7490 | `				/* Variadic parameter with no extra args — create empty array */` |
|       40 |  7491 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  7492 | `				if( pObj ){` |
|       40 |  7493 | `					PH7_MemObjToHashmap(pObj);` |
|       40 |  7494 | `					sArg.nIdx = pObj->nIdx;` |
|       40 |  7495 | `					sArg.pUserData = 0;` |
|       40 |  7496 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       19 |  7497 | `				}` |
|       40 |  7498 | `				n++;` |
|       40 |  7499 | `				break; /* Variadic is always last */` |
|        - |  7500 | `			}` |
|     2170 |  7501 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2164 |  7502 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2164 |  7503 | `				if( pObj ){` |
|        - |  7504 | `					/* Evaluate the default value and extract it's result */` |
|     2164 |  7505 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2164 |  7506 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7507 | `						goto Abort;` |
|        - |  7508 | `					}` |
|        - |  7509 | `					/* Insert argument index */` |
|     2164 |  7510 | `					sArg.nIdx = pObj->nIdx;` |
|     2164 |  7511 | `					sArg.pUserData = 0;` |
|     2164 |  7512 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  7513 | `					/* Make sure the default argument is of the correct type */` |
|     2162 |  7514 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1504 |  7515 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  7516 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7517 | `						/* Cast to the desired type */` |
|      ! 0 |  7518 | `						xCast(pObj);` |
|      ! 0 |  7519 | `					}` |
|     1081 |  7520 | `				}` |
|     1081 |  7521 | `			}` |
|     2170 |  7522 | `			++n;` |
|        2 |  7523 | `		}` |
|        - |  7524 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  7525 | `		 * does not return anything.` |
|        - |  7526 | `		 */` |
|    14668 |  7527 | `		PH7_MemObjRelease(pTos);` |
|    14668 |  7528 | `		pTos = &pTos[-nCallArgs];` |
|        - |  7529 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    14668 |  7530 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    14668 |  7531 | `		if( pFrameStack == 0 ){` |
|        - |  7532 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7533 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7534 | `				&pVmFunc->sName);` |
|      ! 0 |  7535 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7536 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7537 | `			}` |
|      ! 0 |  7538 | `			break;` |
|        - |  7539 | `		}` |
|     7333 |  7540 | `SkipFuncBody:` |
|    14698 |  7541 | `		if( pSelf ){` |
|        - |  7542 | `			/* Push class name */` |
|     2252 |  7543 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1125 |  7544 | `		}` |
|        - |  7545 | `		/* Increment nesting level */` |
|    14698 |  7546 | `		pVm->nRecursionDepth++;` |
|    14698 |  7547 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  7548 | `			/* Execute function body */` |
|    14668 |  7549 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|     7333 |  7550 | `		}` |
|        - |  7551 | `		/* Decrement nesting level */` |
|    14698 |  7552 | `		pVm->nRecursionDepth--;` |
|    14698 |  7553 | `		if( pSelf ){` |
|        - |  7554 | `			/* Pop class name */` |
|     2252 |  7555 | `			(void)SySetPop(&pVm->aSelf);` |
|     1125 |  7556 | `		}` |
|        - |  7557 | `		/* Cleanup the mess left behind */` |
|    14698 |  7558 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  7559 | `			/* Return by reference,reflect that */` |
|        9 |  7560 | `			if( n != SXU32_HIGH ){` |
|        9 |  7561 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  7562 | `				sxu32 i;` |
|        - |  7563 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  7564 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  7565 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  7566 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  7567 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7568 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  7569 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  7570 | `								&pVmFunc->sName);` |
|      ! 0 |  7571 | `						}` |
|      ! 0 |  7572 | `						n = SXU32_HIGH;` |
|      ! 0 |  7573 | `						break;` |
|        - |  7574 | `					}` |
|        3 |  7575 | `				}` |
|        5 |  7576 | `			}else{` |
|      ! 0 |  7577 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7578 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  7579 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  7580 | `						&pVmFunc->sName);` |
|      ! 0 |  7581 | `				}` |
|        - |  7582 | `			}` |
|        9 |  7583 | `			pTos->nIdx = n;` |
|        4 |  7584 | `		}` |
|        - |  7585 | `		/* Cleanup the mess left behind */` |
|    14698 |  7586 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  7587 | `			/* An exception was throw in this frame */` |
|       42 |  7588 | `			pFrame = pFrame->pParent;` |
|       42 |  7589 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  7590 | `				/* Pop the resutlt */` |
|       40 |  7591 | `				VmPopOperand(&pTos,1);` |
|        - |  7592 | `				/* Jump to this destination */` |
|       40 |  7593 | `				pc = pFrame->iExceptionJump - 1;` |
|       40 |  7594 | `				rc = PH7_OK;` |
|       21 |  7595 | `			}else{` |
|        3 |  7596 | `				if( pFrame->pParent ){` |
|        3 |  7597 | `					rc = PH7_EXCEPTION;` |
|        2 |  7598 | `				}else{` |
|        - |  7599 | `					/* Continue normal execution */` |
|      ! 0 |  7600 | `					rc = PH7_OK;` |
|        - |  7601 | `				}` |
|        - |  7602 | `			}` |
|       20 |  7603 | `		}` |
|        - |  7604 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    14698 |  7605 | `		if( pFrameStack ){` |
|    14668 |  7606 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     7333 |  7607 | `		}` |
|        - |  7608 | `		/* Leave the frame */` |
|    14698 |  7609 | `		VmLeaveFrame(&(*pVm));` |
|    14698 |  7610 | `		if( rc == PH7_ABORT ){` |
|        - |  7611 | `			/* Abort processing immeditaley */` |
|        9 |  7612 | `			goto Abort;` |
|    14690 |  7613 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  7614 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  7615 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  7616 | `			 * overwriting the state saved by the inner level.` |
|        - |  7617 | `			 * pTos points to the result slot (not yet written).` |
|        - |  7618 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  7619 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  7620 | `			goto Suspend;` |
|    14652 |  7621 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  7622 | `			goto Exception;` |
|        - |  7623 | `		}` |
|     7326 |  7624 | `	}else{` |
|        - |  7625 | `		ph7_user_func *pFunc;` |
|        - |  7626 | `		ph7_context sCtx;` |
|        - |  7627 | `		ph7_value sRet;` |
|        - |  7628 | `		/* Look for an installed foreign function.` |
|        - |  7629 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  7630 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  7631 | `		 * extract the short name (last component after \) and try that.` |
|        - |  7632 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  7633 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  7634 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   620150 |  7635 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   620150 |  7636 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  7637 | `			/* Compiler-qualified: try short name as global fallback */` |
|       20 |  7638 | `			const char *zShort = sName.zString;` |
|        - |  7639 | `			sxu32 i;` |
|      296 |  7640 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      278 |  7641 | `				if( sName.zString[i] == '\\' ){` |
|       24 |  7642 | `					zShort = &sName.zString[i + 1];` |
|       11 |  7643 | `				}` |
|      140 |  7644 | `			}` |
|       20 |  7645 | `			if( zShort != sName.zString ){` |
|       20 |  7646 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       20 |  7647 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        9 |  7648 | `			}` |
|        9 |  7649 | `		}` |
|   620150 |  7650 | `		if( pEntry == 0 ){` |
|        - |  7651 | `			/* Call to undefined function */` |
|        5 |  7652 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  7653 | `			/* Pop given arguments */` |
|        5 |  7654 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  7655 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7656 | `			}` |
|        - |  7657 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  7658 | `			PH7_MemObjRelease(pTos);` |
|        8 |  7659 | `			break;` |
|        - |  7660 | `		}` |
|   620146 |  7661 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  7662 | `		/* Start collecting function arguments */` |
|   620146 |  7663 | `		SySetReset(&aArg);` |
|  1667680 |  7664 | `		while( pArg < pTos ){` |
|  1047536 |  7665 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1047536 |  7666 | `			pArg++;` |
|        2 |  7667 | `		}` |
|        - |  7668 | `		/* Assume a null return value */` |
|   620146 |  7669 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  7670 | `		/* Init the call context */` |
|   620146 |  7671 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  7672 | `		/* Call the foreign function */` |
|   620146 |  7673 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  7674 | `		/* Release the call context */` |
|   620146 |  7675 | `		VmReleaseCallContext(&sCtx);` |
|   620146 |  7676 | `		if( rc == PH7_ABORT ){` |
|      471 |  7677 | `			goto Abort;` |
|   619676 |  7678 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  7679 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  7680 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  7681 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  7682 | `				/* Exception was NOT caught, propagate */` |
|        5 |  7683 | `				goto Exception;` |
|        - |  7684 | `			}` |
|        - |  7685 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  7686 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  7687 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  7688 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  7689 | `			}` |
|        - |  7690 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  7691 | `			VmPopOperand(&pTos,1);` |
|        - |  7692 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  7693 | `			pFrm = pVm->pFrame;` |
|        7 |  7694 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  7695 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  7696 | `			}` |
|        7 |  7697 | `			break;` |
|        - |  7698 | `		}` |
|   619666 |  7699 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  7700 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  7701 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  7702 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  7703 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  7704 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  7705 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  7706 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  7707 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  7708 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  7709 | `			}` |
|        - |  7710 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  7711 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  7712 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  7713 | `			goto Suspend;` |
|        - |  7714 | `		}` |
|   619628 |  7715 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7716 | `			/* Pop function name and arguments */` |
|   600016 |  7717 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   300029 |  7718 | `		}` |
|        - |  7719 | `		/* Save foreign function return value */` |
|   619628 |  7720 | `		PH7_MemObjStore(&sRet,pTos);` |
|   619628 |  7721 | `		PH7_MemObjRelease(&sRet);` |
|        - |  7722 | `	}` |
|   634276 |  7723 | `	break;` |
|        - |  7724 | `				  }` |
|        - |  7725 | `/*` |
|        - |  7726 | ` * OP_CONSUME: P1 * *` |
|        - |  7727 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  7728 | ` */` |
|    12965 |  7729 | `case PH7_OP_CONSUME: {` |
|    25932 |  7730 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    25932 |  7731 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  7732 |  |
|    25932 |  7733 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    25932 |  7734 | `	pCur = pOut;` |
|        - |  7735 | `	/* Start the consume process  */` |
|    51862 |  7736 | `	while( pOut <= pTos ){` |
|        - |  7737 | `		/* Force a string cast */` |
|    25932 |  7738 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      470 |  7739 | `			PH7_MemObjToString(pOut);` |
|      234 |  7740 | `		}` |
|    25932 |  7741 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  7742 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  7743 | `			/* Invoke the output consumer callback */` |
|    14846 |  7744 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    14846 |  7745 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    14846 |  7746 | `			SyBlobRelease(&pOut->sBlob);` |
|    14846 |  7747 | `			if( rc == SXERR_ABORT ){` |
|        - |  7748 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  7749 | `				goto Abort;` |
|        - |  7750 | `			}` |
|     7422 |  7751 | `		}` |
|    25932 |  7752 | `		pOut++;` |
|        2 |  7753 | `	}` |
|    25932 |  7754 | `	pTos = &pCur[-1];` |
|    25930 |  7755 | `	break;` |
|        - |  7756 | `					 }` |
|        - |  7757 |  |
|        - |  7758 | `		} /* Switch() */` |
| 10644624 |  7759 | `		pc++; /* Next instruction in the stream */` |
|        2 |  7760 | `	} /* For(;;) */` |
|    17811 |  7761 | `Done:` |
|    35624 |  7762 | `	SySetRelease(&aArg);` |
|    35624 |  7763 | `	return SXRET_OK;` |
|       66 |  7764 | `Suspend:` |
|      134 |  7765 | `	SySetRelease(&aArg);` |
|      134 |  7766 | `	return PH7_SUSPEND;` |
|      245 |  7767 | `Abort:` |
|      491 |  7768 | `	SySetRelease(&aArg);` |
|     1697 |  7769 | `	while( pTos >= pStack ){` |
|     1207 |  7770 | `		PH7_MemObjRelease(pTos);` |
|     1207 |  7771 | `		pTos--;` |
|        1 |  7772 | `	}` |
|      491 |  7773 | `	return PH7_ABORT;` |
|        3 |  7774 | `Exception:` |
|        8 |  7775 | `	SySetRelease(&aArg);` |
|       22 |  7776 | `	while( pTos >= pStack ){` |
|       16 |  7777 | `		PH7_MemObjRelease(pTos);` |
|       16 |  7778 | `		pTos--;` |
|        2 |  7779 | `	}` |
|        8 |  7780 | `	return PH7_EXCEPTION;` |
|    18127 |  7781 |  |
|        - |  7782 | `/*` |
|        - |  7783 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  7784 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7785 | ` * See block-comment on that function for additional information.` |
|        - |  7786 | ` */` |
|    16854 |  7787 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  7788 |  |
|        - |  7789 | `	ph7_value *pStack;` |
|        - |  7790 | `	sxi32 rc;` |
|        - |  7791 | `	/* Allocate a new operand stack */` |
|    16856 |  7792 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    16856 |  7793 | `	if( pStack == 0 ){` |
|      ! 0 |  7794 | `		return SXERR_MEM;` |
|        - |  7795 | `	}` |
|        - |  7796 | `	/* Execute the program */` |
|    16856 |  7797 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  7798 | `	/* Free the operand stack */` |
|    16856 |  7799 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  7800 | `	/* Execution result */` |
|    16856 |  7801 | `	return rc;` |
|     8429 |  7802 |  |
|        - |  7803 | `/*` |
|        - |  7804 | ` * Invoke any installed shutdown callbacks.` |
|        - |  7805 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  7806 | ` * or more calls to [register_shutdown_function()].` |
|        - |  7807 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  7808 | ` * execution ends.` |
|        - |  7809 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  7810 | ` * additional information.` |
|        - |  7811 | ` */` |
|     2514 |  7812 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  7813 |  |
|        - |  7814 | `	VmShutdownCB *pEntry;` |
|        - |  7815 | `	ph7_value *apArg[10];` |
|        - |  7816 | `	sxu32 n,nEntry;` |
|        - |  7817 | `	int i;` |
|        - |  7818 | `	/* Point to the stack of registered callbacks */` |
|     2516 |  7819 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    27656 |  7820 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    25142 |  7821 | `		apArg[i] = 0;` |
|    12572 |  7822 | `	}` |
|     2518 |  7823 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  7824 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7825 | `		if( pEntry ){` |
|        - |  7826 | `			/* Prepare callback arguments if any */` |
|        3 |  7827 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  7828 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  7829 | `					break;` |
|        - |  7830 | `				}` |
|      ! 0 |  7831 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  7832 | `			}` |
|        - |  7833 | `			/* Invoke the callback */` |
|        3 |  7834 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  7835 | `			/*` |
|        - |  7836 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  7837 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  7838 | `			 */` |
|        3 |  7839 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7840 | `			if( pEntry ){` |
|        3 |  7841 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  7842 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  7843 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  7844 | `				}` |
|        1 |  7845 | `			}` |
|        1 |  7846 | `		}` |
|        2 |  7847 | `	}` |
|     2516 |  7848 | `	SySetReset(&pVm->aShutdown);` |
|     2516 |  7849 |  |
|        - |  7850 | `/*` |
|        - |  7851 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  7852 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7853 | ` * See block-comment on that function for additional information.` |
|        - |  7854 | ` */` |
|     2522 |  7855 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  7856 |  |
|        - |  7857 | `	/* Make sure we are ready to execute this program */` |
|     2524 |  7858 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  7859 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  7860 | `	}` |
|        - |  7861 | `	/* Set the execution magic number  */` |
|     2524 |  7862 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  7863 | `	/* Execute the program */` |
|     2524 |  7864 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  7865 | `	/* Invoke any shutdown callbacks */` |
|     2520 |  7866 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  7867 | `	/*` |
|        - |  7868 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  7869 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  7870 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  7871 | `	 */` |
|     2520 |  7872 | `	return SXRET_OK;` |
|     1263 |  7873 |  |
|        - |  7874 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  7875 | `/*` |
|        - |  7876 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  7877 | ` * The context is in CREATED state and ready to be started.` |
|        - |  7878 | ` */` |
|       42 |  7879 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  7880 |  |
|        - |  7881 | `	ph7_exec_ctx *pCtx;` |
|        - |  7882 | `	ph7_value *pStack;` |
|        - |  7883 | `	VmFrame *pFrame;` |
|       44 |  7884 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  7885 | `	if( pCtx == 0 ){` |
|      ! 0 |  7886 | `		return 0;` |
|        - |  7887 | `	}` |
|       44 |  7888 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  7889 | `	pCtx->pVm = pVm;` |
|       44 |  7890 | `	pCtx->pFunc = pFunc;` |
|       44 |  7891 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  7892 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  7893 | `	pCtx->pc = 0;` |
|       44 |  7894 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  7895 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  7896 | `	/* Allocate a private operand stack */` |
|       44 |  7897 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  7898 | `	if( pStack == 0 ){` |
|      ! 0 |  7899 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7900 | `		return 0;` |
|        - |  7901 | `	}` |
|       44 |  7902 | `	pCtx->pStack = pStack;` |
|        - |  7903 | `	/* Create a detached frame for the fiber */` |
|       44 |  7904 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  7905 | `	if( pFrame == 0 ){` |
|      ! 0 |  7906 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  7907 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7908 | `		return 0;` |
|        - |  7909 | `	}` |
|       44 |  7910 | `	pCtx->pFrame = pFrame;` |
|       44 |  7911 | `	return pCtx;` |
|       23 |  7912 |  |
|        - |  7913 | `/*` |
|        - |  7914 | ` * Start executing a fiber context for the first time.` |
|        - |  7915 | ` */` |
|       42 |  7916 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  7917 |  |
|        - |  7918 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7919 | `	sxi32 rc;` |
|       44 |  7920 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7921 | `		return SXERR_INVALID;` |
|        - |  7922 | `	}` |
|        - |  7923 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  7924 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  7925 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7926 | `	/* Save and set the active context */` |
|       44 |  7927 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  7928 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  7929 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  7930 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  7931 | `	pVm->nRecursionDepth++;` |
|        - |  7932 | `	/* Execute from the beginning */` |
|       65 |  7933 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  7934 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  7935 | `	pVm->nRecursionDepth--;` |
|        - |  7936 | `	/* Restore the previous context */` |
|       44 |  7937 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  7938 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7939 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  7940 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  7941 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  7942 | `		if( pResult ){` |
|       24 |  7943 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7944 | `		}` |
|       42 |  7945 | `		return SXRET_OK;` |
|        - |  7946 | `	}` |
|        - |  7947 | `	/* Detach frame */` |
|        3 |  7948 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7949 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7950 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7951 | `	}` |
|        3 |  7952 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7953 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7954 | `		return PH7_ABORT;` |
|        - |  7955 | `	}` |
|        3 |  7956 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7957 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7958 | `		return PH7_EXCEPTION;` |
|        - |  7959 | `	}` |
|        - |  7960 | `	/* Normal completion */` |
|        3 |  7961 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7962 | `	if( pResult ){` |
|        3 |  7963 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7964 | `	}` |
|        3 |  7965 | `	return SXRET_OK;` |
|       23 |  7966 |  |
|        - |  7967 | `/*` |
|        - |  7968 | ` * Resume a suspended fiber context.` |
|        - |  7969 | ` */` |
|       86 |  7970 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7971 |  |
|        - |  7972 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7973 | `	sxi32 rc;` |
|       88 |  7974 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7975 | `		return SXERR_INVALID;` |
|        - |  7976 | `	}` |
|        - |  7977 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7978 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7979 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7980 | `	if( pResumeValue ){` |
|       40 |  7981 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7982 | `	}else{` |
|       50 |  7983 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7984 | `	}` |
|       88 |  7985 | `	pCtx->nTos++;` |
|        - |  7986 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7987 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7988 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7989 | `	/* Save and set the active context */` |
|       88 |  7990 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7991 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7992 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7993 | `	pVm->nRecursionDepth++;` |
|        - |  7994 | `	/* Resume execution from saved PC */` |
|      131 |  7995 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7996 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7997 | `	pVm->nRecursionDepth--;` |
|        - |  7998 | `	/* Restore the previous context */` |
|       88 |  7999 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  8000 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8001 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  8002 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  8003 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  8004 | `		if( pResult ){` |
|       18 |  8005 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  8006 | `		}` |
|       56 |  8007 | `		return SXRET_OK;` |
|        - |  8008 | `	}` |
|        - |  8009 | `	/* Detach frame */` |
|       34 |  8010 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  8011 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  8012 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  8013 | `	}` |
|       34 |  8014 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8015 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8016 | `		return PH7_ABORT;` |
|        - |  8017 | `	}` |
|       34 |  8018 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8019 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8020 | `		return PH7_EXCEPTION;` |
|        - |  8021 | `	}` |
|        - |  8022 | `	/* Normal completion */` |
|       34 |  8023 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  8024 | `	if( pResult ){` |
|       20 |  8025 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  8026 | `	}` |
|       34 |  8027 | `	return SXRET_OK;` |
|       45 |  8028 |  |
|        - |  8029 | `/*` |
|        - |  8030 | ` * Release an execution context and all its resources.` |
|        - |  8031 | ` */` |
|        4 |  8032 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  8033 |  |
|        5 |  8034 | `	if( pCtx == 0 ){` |
|      ! 0 |  8035 | `		return;` |
|        - |  8036 | `	}` |
|        5 |  8037 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  8038 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  8039 | `		return;` |
|        - |  8040 | `	}` |
|        5 |  8041 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  8042 | `	/* Release values */` |
|        5 |  8043 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  8044 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  8045 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  8046 | `	if( pCtx->pFrame ){` |
|        - |  8047 | `		VmSlot *aSlot;` |
|        - |  8048 | `		sxu32 n;` |
|        - |  8049 | `		/* Free local variables */` |
|        5 |  8050 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  8051 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  8052 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  8053 | `		}` |
|        - |  8054 | `		/* Remove local references */` |
|        5 |  8055 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  8056 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  8057 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  8058 | `		}` |
|        5 |  8059 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  8060 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  8061 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  8062 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  8063 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  8064 | `		pCtx->pFrame = 0;` |
|        2 |  8065 | `	}` |
|        - |  8066 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  8067 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  8068 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  8069 | `	if( pCtx->pStack ){` |
|        5 |  8070 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  8071 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  8072 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  8073 | `				PH7_MemObjRelease(pTos);` |
|        5 |  8074 | `				pTos--;` |
|        1 |  8075 | `			}` |
|        2 |  8076 | `		}` |
|        5 |  8077 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  8078 | `		pCtx->pStack = 0;` |
|        2 |  8079 | `	}` |
|        - |  8080 | `	/* Free the context itself */` |
|        5 |  8081 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  8082 |  |
|        - |  8083 | `/*` |
|        - |  8084 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  8085 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  8086 | ` */` |
|       90 |  8087 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  8088 |  |
|        - |  8089 | `	ph7_class_instance *pThis;` |
|        - |  8090 | `	SyString sAttr;` |
|        - |  8091 | `	ph7_value *pAttr;` |
|       92 |  8092 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8093 | `		return 0;` |
|        - |  8094 | `	}` |
|       92 |  8095 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  8096 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  8097 | `		return 0;` |
|        - |  8098 | `	}` |
|       92 |  8099 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  8100 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  8101 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  8102 | `		return 0;` |
|        - |  8103 | `	}` |
|       62 |  8104 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  8105 |  |
|        - |  8106 | `/*` |
|        - |  8107 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  8108 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  8109 | ` */` |
|       38 |  8110 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8111 |  |
|       40 |  8112 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  8113 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  8114 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8115 | `			"Cannot suspend outside of a fiber");` |
|        - |  8116 | `	}` |
|       40 |  8117 | `	if( nArg > 0 ){` |
|       40 |  8118 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  8119 | `	}else{` |
|      ! 0 |  8120 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  8121 | `	}` |
|       40 |  8122 | `	return PH7_SUSPEND;` |
|       21 |  8123 |  |
|        - |  8124 | `/*` |
|        - |  8125 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  8126 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  8127 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  8128 | ` */` |
|       24 |  8129 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8130 |  |
|        - |  8131 | `	ph7_class_instance *pThis;` |
|        - |  8132 | `	ph7_value *pAttr;` |
|        - |  8133 | `	SyString sAttrName;` |
|       26 |  8134 | `	if( nArg < 2 ){` |
|      ! 0 |  8135 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8136 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  8137 | `	}` |
|       26 |  8138 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8139 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8140 | `			"Fiber::__construct(): invalid $this");` |
|        - |  8141 | `	}` |
|       26 |  8142 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  8143 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  8144 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8145 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  8146 | `	}` |
|        - |  8147 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  8148 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8149 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8150 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  8151 | `	}` |
|        - |  8152 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  8153 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8154 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8155 | `	if( pAttr ){` |
|       26 |  8156 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  8157 | `	}` |
|       26 |  8158 | `	return PH7_OK;` |
|       14 |  8159 |  |
|        - |  8160 | `/*` |
|        - |  8161 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  8162 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  8163 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  8164 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  8165 | ` */` |
|       24 |  8166 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  8167 | `	ph7_class_instance **ppThis)` |
|        2 |  8168 |  |
|       26 |  8169 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8170 | `	ph7_value *pCallable;` |
|        - |  8171 | `	SyString sAttrName;` |
|       26 |  8172 | `	*ppThis = 0;` |
|       26 |  8173 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8174 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  8175 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8176 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  8177 | `		return 0;` |
|        - |  8178 | `	}` |
|       26 |  8179 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  8180 | `		/* String callable — look up in user functions with overload support */` |
|        - |  8181 | `		SyString sName;` |
|        - |  8182 | `		SyHashEntry *pEntry;` |
|        - |  8183 | `		ph7_vm_func *pFunc;` |
|       26 |  8184 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  8185 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  8186 | `		if( pEntry == 0 ){` |
|      ! 0 |  8187 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  8188 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  8189 | `			return 0;` |
|        - |  8190 | `		}` |
|       26 |  8191 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  8192 | `		return pFunc;` |
|      ! 0 |  8193 | `	}else{` |
|        - |  8194 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  8195 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  8196 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  8197 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  8198 | `		if( pMethod == 0 ){` |
|      ! 0 |  8199 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8200 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  8201 | `			return 0;` |
|        - |  8202 | `		}` |
|      ! 0 |  8203 | `		*ppThis = pClosure;` |
|      ! 0 |  8204 | `		return &pMethod->sFunc;` |
|        - |  8205 | `	}` |
|       14 |  8206 |  |
|        - |  8207 | `/*` |
|        - |  8208 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  8209 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  8210 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  8211 | ` */` |
|       42 |  8212 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  8213 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  8214 |  |
|       44 |  8215 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  8216 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  8217 | `	sxu32 nFormal, n;` |
|        - |  8218 | `	VmSlot sSlot;` |
|        - |  8219 | `	sxi32 rc;` |
|        - |  8220 | `	/* Install $this for closure/method callables */` |
|       44 |  8221 | `	if( pClosureThis ){` |
|        - |  8222 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  8223 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  8224 | `		if( pObj ){` |
|      ! 0 |  8225 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  8226 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  8227 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  8228 | `		}` |
|      ! 0 |  8229 | `	}` |
|        - |  8230 | `	/* Install static variables */` |
|       44 |  8231 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  8232 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  8233 | `		ph7_value *pVal;` |
|      ! 0 |  8234 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  8235 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  8236 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  8237 | `			if( pVal ){` |
|      ! 0 |  8238 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8239 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  8240 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  8241 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  8242 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  8243 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  8244 | `				}` |
|      ! 0 |  8245 | `			}` |
|      ! 0 |  8246 | `		}` |
|      ! 0 |  8247 | `	}` |
|        - |  8248 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  8249 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  8250 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  8251 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  8252 | `		ph7_value *pObj;` |
|       12 |  8253 | `		if( n < (sxu32)nArg ){` |
|        - |  8254 | `			/* Argument provided — install with type casting */` |
|       12 |  8255 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  8256 | `			if( pObj ){` |
|       12 |  8257 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  8258 | `				/* Type casting */` |
|       12 |  8259 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8260 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8261 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8262 | `						if( xCast ){` |
|      ! 0 |  8263 | `							xCast(pObj);` |
|      ! 0 |  8264 | `						}` |
|      ! 0 |  8265 | `					}` |
|      ! 0 |  8266 | `				}` |
|       12 |  8267 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  8268 | `				sSlot.pUserData = 0;` |
|       12 |  8269 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  8270 | `			}` |
|        5 |  8271 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  8272 | `			/* Default value */` |
|      ! 0 |  8273 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  8274 | `			if( pObj ){` |
|      ! 0 |  8275 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  8276 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8277 | `					return rc;` |
|        - |  8278 | `				}` |
|      ! 0 |  8279 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8280 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8281 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8282 | `						if( xCast ){` |
|      ! 0 |  8283 | `							xCast(pObj);` |
|      ! 0 |  8284 | `						}` |
|      ! 0 |  8285 | `					}` |
|      ! 0 |  8286 | `				}` |
|      ! 0 |  8287 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  8288 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8289 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  8290 | `			}` |
|      ! 0 |  8291 | `		}` |
|        7 |  8292 | `	}` |
|        - |  8293 | `	/* Install closure environment (captured variables) */` |
|       44 |  8294 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8295 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  8296 | `		ph7_value *pValue;` |
|        - |  8297 | `		sxu32 iEnv;` |
|        3 |  8298 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  8299 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  8300 | `			pEnv = &aEnv[iEnv];` |
|        7 |  8301 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  8302 | `				continue;` |
|        - |  8303 | `			}` |
|        5 |  8304 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  8305 | `			if( pValue == 0 ){` |
|      ! 0 |  8306 | `				continue;` |
|        - |  8307 | `			}` |
|        5 |  8308 | `			PH7_MemObjRelease(pValue);` |
|        5 |  8309 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  8310 | `		}` |
|        1 |  8311 | `	}` |
|       44 |  8312 | `	return SXRET_OK;` |
|       23 |  8313 |  |
|        - |  8314 | `/*` |
|        - |  8315 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  8316 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  8317 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  8318 | ` */` |
|       26 |  8319 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8320 |  |
|       28 |  8321 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8322 | `	ph7_class_instance *pThis;` |
|        - |  8323 | `	ph7_class_instance *pClosureThis;` |
|        - |  8324 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8325 | `	ph7_vm_func *pFunc;` |
|        - |  8326 | `	ph7_value sResult;` |
|        - |  8327 | `	ph7_value *pCtxAttr;` |
|        - |  8328 | `	SyString sAttrName;` |
|        - |  8329 | `	sxi32 rc;` |
|       28 |  8330 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8331 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  8332 | `	}` |
|       28 |  8333 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8334 | `	/* Check if already started (has a __ctx) */` |
|       28 |  8335 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  8336 | `	if( pExecCtx != 0 ){` |
|        3 |  8337 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8338 | `			"Cannot start a fiber that has already been started");` |
|        - |  8339 | `	}` |
|        - |  8340 | `	/* Resolve callable */` |
|       26 |  8341 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  8342 | `	if( pFunc == 0 ){` |
|      ! 0 |  8343 | `		return PH7_EXCEPTION;` |
|        - |  8344 | `	}` |
|        - |  8345 | `	/* Create execution context now that we know the function */` |
|       26 |  8346 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  8347 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8348 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8349 | `			"Fiber::start(): out of memory");` |
|        - |  8350 | `	}` |
|        - |  8351 | `	/* Store context in $this->__ctx */` |
|       26 |  8352 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  8353 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8354 | `	if( pCtxAttr ){` |
|       26 |  8355 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  8356 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  8357 | `	}` |
|        - |  8358 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  8359 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  8360 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  8361 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  8362 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  8363 | `	/* Unpack the args array and install into the frame */` |
|        - |  8364 | `	{` |
|       26 |  8365 | `		ph7_value **apValues = 0;` |
|       26 |  8366 | `		int nActual = 0;` |
|       26 |  8367 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  8368 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  8369 | `			ph7_hashmap_node *pNode;` |
|       26 |  8370 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  8371 | `			if( nCount > 0 ){` |
|        3 |  8372 | `				sxu32 idx = 0;` |
|        4 |  8373 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  8374 | `					nCount * sizeof(ph7_value *));` |
|        3 |  8375 | `				if( apValues ){` |
|        3 |  8376 | `					pNode = pMap->pFirst;` |
|        7 |  8377 | `					while( pNode && idx < nCount ){` |
|        5 |  8378 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  8379 | `						idx++;` |
|        5 |  8380 | `						pNode = pNode->pPrev;` |
|        1 |  8381 | `					}` |
|        3 |  8382 | `					nActual = (int)idx;` |
|        1 |  8383 | `				}` |
|        1 |  8384 | `			}` |
|       12 |  8385 | `		}` |
|       26 |  8386 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  8387 | `		if( apValues ){` |
|        3 |  8388 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  8389 | `		}` |
|        - |  8390 | `	}` |
|        - |  8391 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  8392 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  8393 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  8394 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8395 | `		return PH7_ABORT;` |
|        - |  8396 | `	}` |
|       26 |  8397 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  8398 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  8399 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8400 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8401 | `		return PH7_ABORT;` |
|        - |  8402 | `	}` |
|       26 |  8403 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8404 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8405 | `		return PH7_EXCEPTION;` |
|        - |  8406 | `	}` |
|       26 |  8407 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  8408 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  8409 | `	return PH7_OK;` |
|       15 |  8410 |  |
|        - |  8411 | `/*` |
|        - |  8412 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  8413 | ` */` |
|       36 |  8414 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8415 |  |
|       38 |  8416 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8417 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8418 | `	ph7_value sResult;` |
|        - |  8419 | `	ph7_value *pResumeVal;` |
|        - |  8420 | `	sxi32 rc;` |
|       38 |  8421 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8422 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  8423 | `		return PH7_OK;` |
|        - |  8424 | `	}` |
|       38 |  8425 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  8426 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8427 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  8428 | `		return PH7_OK;` |
|        - |  8429 | `	}` |
|       38 |  8430 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8431 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8432 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  8433 | `	}` |
|       36 |  8434 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  8435 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  8436 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  8437 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8438 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8439 | `		return PH7_ABORT;` |
|        - |  8440 | `	}` |
|       36 |  8441 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8442 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8443 | `		return PH7_EXCEPTION;` |
|        - |  8444 | `	}` |
|       36 |  8445 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  8446 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  8447 | `	return PH7_OK;` |
|       20 |  8448 |  |
|        - |  8449 | `/*` |
|        - |  8450 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  8451 | ` */` |
|        6 |  8452 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8453 |  |
|        8 |  8454 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8455 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  8456 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8457 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8458 | `		return PH7_OK;` |
|        - |  8459 | `	}` |
|        8 |  8460 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  8461 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8462 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8463 | `		return PH7_OK;` |
|        - |  8464 | `	}` |
|        8 |  8465 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8466 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8467 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8468 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  8469 | `		}` |
|      ! 0 |  8470 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8471 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  8472 | `	}` |
|        8 |  8473 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  8474 | `	return PH7_OK;` |
|        5 |  8475 |  |
|        - |  8476 | `/*` |
|        - |  8477 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  8478 | ` */` |
|        6 |  8479 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8480 |  |
|        - |  8481 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8482 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8483 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8484 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  8485 | `	return PH7_OK;` |
|        4 |  8486 |  |
|      ! 0 |  8487 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8488 |  |
|        - |  8489 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  8490 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  8491 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8492 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  8493 | `	return PH7_OK;` |
|      ! 0 |  8494 |  |
|        6 |  8495 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8496 |  |
|        - |  8497 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8498 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8499 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8500 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  8501 | `	return PH7_OK;` |
|        4 |  8502 |  |
|        6 |  8503 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8504 |  |
|        - |  8505 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8506 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8507 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8508 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  8509 | `	return PH7_OK;` |
|        4 |  8510 |  |
|        - |  8511 | `/*` |
|        - |  8512 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  8513 | ` */` |
|        4 |  8514 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8515 |  |
|        5 |  8516 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8517 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  8518 | `	if( nArg < 1 ){` |
|      ! 0 |  8519 | `		return PH7_OK;` |
|        - |  8520 | `	}` |
|        5 |  8521 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  8522 | `	if( pExecCtx ){` |
|        5 |  8523 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  8524 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  8525 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  8526 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8527 | `			SyString sAttrName;` |
|        - |  8528 | `			ph7_value *pAttr;` |
|        5 |  8529 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  8530 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  8531 | `			if( pAttr ){` |
|        5 |  8532 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  8533 | `			}` |
|        2 |  8534 | `		}` |
|        2 |  8535 | `	}` |
|        5 |  8536 | `	return PH7_OK;` |
|        3 |  8537 |  |
|        - |  8538 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  8539 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  8540 |  |
|        - |  8541 | `	ph7_class_instance *pThis;` |
|      ! 0 |  8542 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  8543 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8544 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  8545 |  |
|      ! 0 |  8546 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  8547 |  |
|        - |  8548 | `	ph7_class_instance *pThis;` |
|      ! 0 |  8549 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  8550 | `	ph7_exec_ctx *pCtx;` |
|        - |  8551 | `	ph7_vm_func *pFunc;` |
|        - |  8552 | `	ph7_value *pCallable;` |
|        - |  8553 | `	ph7_value *pCtxAttr;` |
|        - |  8554 | `	SyString sAttrName;` |
|        - |  8555 | `	/* Must not already be started */` |
|      ! 0 |  8556 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8557 | `	if( pCtx != 0 ){` |
|      ! 0 |  8558 | `		return SXERR_INVALID;` |
|        - |  8559 | `	}` |
|      ! 0 |  8560 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8561 | `		return SXERR_INVALID;` |
|        - |  8562 | `	}` |
|      ! 0 |  8563 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  8564 | `	/* Get the callable */` |
|      ! 0 |  8565 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  8566 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8567 | `	if( pCallable == 0 ){` |
|      ! 0 |  8568 | `		return SXERR_INVALID;` |
|        - |  8569 | `	}` |
|        - |  8570 | `	/* Resolve callable */` |
|      ! 0 |  8571 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  8572 | `		SyString sName;` |
|        - |  8573 | `		SyHashEntry *pEntry;` |
|      ! 0 |  8574 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  8575 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  8576 | `		if( pEntry == 0 ){` |
|      ! 0 |  8577 | `			return SXERR_NOTFOUND;` |
|        - |  8578 | `		}` |
|      ! 0 |  8579 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  8580 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8581 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  8582 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  8583 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  8584 | `		if( pMethod == 0 ){` |
|      ! 0 |  8585 | `			return SXERR_INVALID;` |
|        - |  8586 | `		}` |
|      ! 0 |  8587 | `		pClosureThis = pClosure;` |
|      ! 0 |  8588 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  8589 | `	}else{` |
|      ! 0 |  8590 | `		return SXERR_INVALID;` |
|        - |  8591 | `	}` |
|        - |  8592 | `	/* Create context */` |
|      ! 0 |  8593 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  8594 | `	if( pCtx == 0 ){` |
|      ! 0 |  8595 | `		return SXERR_MEM;` |
|        - |  8596 | `	}` |
|        - |  8597 | `	/* Store in __ctx */` |
|      ! 0 |  8598 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8599 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8600 | `	if( pCtxAttr ){` |
|      ! 0 |  8601 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  8602 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  8603 | `	}` |
|        - |  8604 | `	/* Set up frame with args */` |
|      ! 0 |  8605 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  8606 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  8607 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  8608 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  8609 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  8610 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  8611 |  |
|      ! 0 |  8612 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  8613 |  |
|      ! 0 |  8614 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8615 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  8616 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  8617 |  |
|      ! 0 |  8618 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  8619 |  |
|      ! 0 |  8620 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8621 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  8622 |  |
|      ! 0 |  8623 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  8624 |  |
|      ! 0 |  8625 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8626 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  8627 |  |
|      ! 0 |  8628 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  8629 |  |
|      ! 0 |  8630 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  8631 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  8632 | `	return &pCtx->sRetValue;` |
|      ! 0 |  8633 |  |
|        - |  8634 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  8635 | `/*` |
|        - |  8636 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  8637 | ` */` |
|       18 |  8638 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  8639 |  |
|        - |  8640 | `	ph7_generator *pGen;` |
|       20 |  8641 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  8642 | `	if( pGen == 0 ){` |
|      ! 0 |  8643 | `		return 0;` |
|        - |  8644 | `	}` |
|       20 |  8645 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  8646 | `	pGen->pCtx = pCtx;` |
|       20 |  8647 | `	pGen->iImplicitKey = 0;` |
|       20 |  8648 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  8649 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  8650 | `	/* Link the generator back to the exec context */` |
|       20 |  8651 | `	pCtx->pPrivate = pGen;` |
|       20 |  8652 | `	return pGen;` |
|       11 |  8653 |  |
|        - |  8654 | `/*` |
|        - |  8655 | ` * Release a generator and its execution context.` |
|        - |  8656 | ` */` |
|      ! 0 |  8657 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  8658 |  |
|      ! 0 |  8659 | `	if( pGen == 0 ){` |
|      ! 0 |  8660 | `		return;` |
|        - |  8661 | `	}` |
|      ! 0 |  8662 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8663 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8664 | `	if( pGen->pCtx ){` |
|      ! 0 |  8665 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  8666 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  8667 | `		pGen->pCtx = 0;` |
|      ! 0 |  8668 | `	}` |
|      ! 0 |  8669 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  8670 |  |
|        - |  8671 | `/*` |
|        - |  8672 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  8673 | ` */` |
|      192 |  8674 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  8675 |  |
|        - |  8676 | `	ph7_class_instance *pThis;` |
|        - |  8677 | `	SyString sAttr;` |
|        - |  8678 | `	ph7_value *pAttr;` |
|      194 |  8679 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8680 | `		return 0;` |
|        - |  8681 | `	}` |
|      194 |  8682 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  8683 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  8684 | `		return 0;` |
|        - |  8685 | `	}` |
|      194 |  8686 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  8687 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  8688 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  8689 | `		return 0;` |
|        - |  8690 | `	}` |
|      194 |  8691 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  8692 |  |
|        - |  8693 | `/*` |
|        - |  8694 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  8695 | ` */` |
|       18 |  8696 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8697 |  |
|        - |  8698 | `	ph7_generator *pGen;` |
|        - |  8699 | `	sxi32 rc;` |
|       20 |  8700 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  8701 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  8702 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  8703 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  8704 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  8705 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  8706 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  8707 | `	}` |
|       20 |  8708 | `	return PH7_OK;` |
|       11 |  8709 |  |
|        - |  8710 | `/*` |
|        - |  8711 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  8712 | ` */` |
|       52 |  8713 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8714 |  |
|        - |  8715 | `	ph7_generator *pGen;` |
|       54 |  8716 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  8717 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  8718 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  8719 | `	return PH7_OK;` |
|       28 |  8720 |  |
|        - |  8721 | `/*` |
|        - |  8722 | ` * Generator::current() — return the last yielded value.` |
|        - |  8723 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  8724 | ` */` |
|       56 |  8725 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8726 |  |
|        - |  8727 | `	ph7_generator *pGen;` |
|        - |  8728 | `	sxi32 rc;` |
|       58 |  8729 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  8730 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  8731 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  8732 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8733 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  8734 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  8735 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  8736 | `	}` |
|       58 |  8737 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  8738 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  8739 | `	}else{` |
|      ! 0 |  8740 | `		ph7_result_null(pCtx);` |
|        - |  8741 | `	}` |
|       58 |  8742 | `	return PH7_OK;` |
|       30 |  8743 |  |
|        - |  8744 | `/*` |
|        - |  8745 | ` * Generator::key() — return the last yielded key.` |
|        - |  8746 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  8747 | ` */` |
|       12 |  8748 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8749 |  |
|        - |  8750 | `	ph7_generator *pGen;` |
|        - |  8751 | `	sxi32 rc;` |
|       13 |  8752 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  8753 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  8754 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  8755 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8756 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  8757 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  8758 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  8759 | `	}` |
|       13 |  8760 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  8761 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  8762 | `	}else{` |
|      ! 0 |  8763 | `		ph7_result_null(pCtx);` |
|        - |  8764 | `	}` |
|       13 |  8765 | `	return PH7_OK;` |
|        7 |  8766 |  |
|        - |  8767 | `/*` |
|        - |  8768 | ` * Generator::next() — advance to the next yield point.` |
|        - |  8769 | ` */` |
|       48 |  8770 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8771 |  |
|        - |  8772 | `	ph7_generator *pGen;` |
|        - |  8773 | `	sxi32 rc;` |
|       50 |  8774 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  8775 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  8776 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  8777 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8778 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  8779 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  8780 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  8781 | `	}else{` |
|      ! 0 |  8782 | `		return PH7_OK;` |
|        - |  8783 | `	}` |
|       50 |  8784 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  8785 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  8786 | `	return PH7_OK;` |
|       26 |  8787 |  |
|        - |  8788 | `/*` |
|        - |  8789 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  8790 | ` */` |
|        4 |  8791 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8792 |  |
|        - |  8793 | `	ph7_generator *pGen;` |
|        - |  8794 | `	ph7_value *pSendVal;` |
|        - |  8795 | `	sxi32 rc;` |
|        5 |  8796 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  8797 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  8798 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  8799 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  8800 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  8801 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  8802 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  8803 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  8804 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  8805 | `	}else{` |
|      ! 0 |  8806 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8807 | `		return PH7_OK;` |
|        - |  8808 | `	}` |
|        5 |  8809 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  8810 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  8811 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8812 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  8813 | `	}else{` |
|        3 |  8814 | `		ph7_result_null(pCtx);` |
|        - |  8815 | `	}` |
|        5 |  8816 | `	return PH7_OK;` |
|        3 |  8817 |  |
|        - |  8818 | `/*` |
|        - |  8819 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  8820 | ` *` |
|        - |  8821 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  8822 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  8823 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  8824 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  8825 | ` * the exception to the caller.` |
|        - |  8826 | ` */` |
|      ! 0 |  8827 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8828 |  |
|        - |  8829 | `	ph7_generator *pGen;` |
|        - |  8830 | `	const char *zMsg;` |
|        - |  8831 | `	int nLen;` |
|      ! 0 |  8832 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  8833 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8834 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  8835 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  8836 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  8837 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8838 | `			"Cannot throw into a closed generator");` |
|        - |  8839 | `	}` |
|        - |  8840 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  8841 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  8842 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  8843 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  8844 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8845 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  8846 | `	nLen = 0;` |
|      ! 0 |  8847 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  8848 | `		/* Try to get the exception's message */` |
|        - |  8849 | `		SyString sAttr;` |
|        - |  8850 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  8851 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  8852 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  8853 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  8854 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  8855 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  8856 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  8857 | `		}` |
|      ! 0 |  8858 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  8859 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  8860 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  8861 | `	}` |
|      ! 0 |  8862 | `	(void)nLen;` |
|      ! 0 |  8863 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  8864 |  |
|        - |  8865 | `/*` |
|        - |  8866 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  8867 | ` */` |
|        2 |  8868 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8869 |  |
|        - |  8870 | `	ph7_generator *pGen;` |
|        3 |  8871 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8872 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  8873 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8874 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8875 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8876 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  8877 | `	}` |
|        3 |  8878 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  8879 | `	return PH7_OK;` |
|        2 |  8880 |  |
|        - |  8881 | `/*` |
|        - |  8882 | ` * Generator::__destruct() — clean up.` |
|        - |  8883 | ` */` |
|      ! 0 |  8884 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8885 |  |
|        - |  8886 | `	ph7_generator *pGen;` |
|      ! 0 |  8887 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  8888 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8889 | `	if( pGen ){` |
|      ! 0 |  8890 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  8891 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8892 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8893 | `			SyString sAttrName;` |
|        - |  8894 | `			ph7_value *pAttr;` |
|      ! 0 |  8895 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8896 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8897 | `			if( pAttr ){` |
|      ! 0 |  8898 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  8899 | `			}` |
|      ! 0 |  8900 | `		}` |
|      ! 0 |  8901 | `	}` |
|      ! 0 |  8902 | `	return PH7_OK;` |
|      ! 0 |  8903 |  |
|        - |  8904 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  8905 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  8906 | `/*` |
|        - |  8907 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8908 | ` * the desired message.` |
|        - |  8909 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8910 | ` * in 'api.c' for additional information.` |
|        - |  8911 | ` */` |
|      370 |  8912 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8913 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8914 | `	SyString *pString /* Message to output */` |
|        - |  8915 | `	)` |
|        2 |  8916 |  |
|      372 |  8917 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  8918 | `	sxi32 rc = SXRET_OK;` |
|        - |  8919 | `	/* Call the output consumer */` |
|      372 |  8920 | `	if( pString->nByte > 0 ){` |
|      372 |  8921 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  8922 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  8923 | `	}` |
|      372 |  8924 | `	return rc;` |
|        2 |  8925 |  |
|        - |  8926 | `/*` |
|        - |  8927 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8928 | ` * callback to consume the formatted message.` |
|        - |  8929 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8930 | ` * in 'api.c' for additional information.` |
|        - |  8931 | ` */` |
|        2 |  8932 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8933 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8934 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8935 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8936 | `	)` |
|        1 |  8937 |  |
|        3 |  8938 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  8939 | `	sxi32 rc = SXRET_OK;` |
|        - |  8940 | `	SyBlob sWorker;` |
|        - |  8941 | `	/* Format the message and call the output consumer */` |
|        3 |  8942 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  8943 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8944 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8945 | `		/* Consume the formatted message */` |
|        3 |  8946 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8947 | `	}` |
|        3 |  8948 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8949 | `	/* Release the working buffer */` |
|        3 |  8950 | `	SyBlobRelease(&sWorker);` |
|        3 |  8951 | `	return rc;` |
|        1 |  8952 |  |
|        - |  8953 | `/*` |
|        - |  8954 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8955 | ` * This function never fail and always return a pointer` |
|        - |  8956 | ` * to a null terminated string.` |
|        - |  8957 | ` */` |
|       12 |  8958 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8959 |  |
|       13 |  8960 | `	const char *zOp = "Unknown     ";` |
|       13 |  8961 | `	switch(nOp){` |
|        3 |  8962 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8963 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8964 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8965 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8966 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8967 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8968 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8969 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8970 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8971 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8972 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8973 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8974 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8975 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8976 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8977 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8978 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8979 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8980 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8981 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8982 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8983 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8984 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8985 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8986 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8987 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8988 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8989 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8990 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8991 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8992 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8993 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8994 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8995 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8996 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8997 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8998 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8999 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  9000 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  9001 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  9002 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  9003 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  9004 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  9005 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  9006 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  9007 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  9008 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  9009 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  9010 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  9011 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  9012 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  9013 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  9014 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  9015 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  9016 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  9017 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  9018 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  9019 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  9020 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  9021 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  9022 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  9023 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  9024 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  9025 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  9026 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  9027 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  9028 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  9029 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  9030 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  9031 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  9032 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  9033 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  9034 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  9035 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  9036 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  9037 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  9038 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  9039 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  9040 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  9041 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  9042 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  9043 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  9044 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  9045 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  9046 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  9047 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  9048 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  9049 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  9050 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  9051 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  9052 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  9053 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  9054 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  9055 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  9056 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  9057 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  9058 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  9059 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  9060 | `	default:` |
|      ! 0 |  9061 | `		break;` |
|        - |  9062 | `	}` |
|       13 |  9063 | `	return zOp;` |
|        1 |  9064 |  |
|        - |  9065 | `/*` |
|        - |  9066 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  9067 | ` * The xConsumer() callback which is an used defined function` |
|        - |  9068 | ` * is responsible of consuming the generated dump.` |
|        - |  9069 | ` */` |
|        2 |  9070 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  9071 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  9072 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  9073 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  9074 | `	)` |
|        1 |  9075 |  |
|        - |  9076 | `	sxi32 rc;` |
|        3 |  9077 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  9078 | `	return rc;` |
|        1 |  9079 |  |
|        - |  9080 | `/*` |
|        - |  9081 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  9082 | ` * outside a class body [i.e: global or function scope].` |
|        - |  9083 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  9084 | ` * in 'compile.c' for additional information.` |
|        - |  9085 | ` */` |
|       14 |  9086 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  9087 |  |
|       15 |  9088 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  9089 | `	/* Evaluate and expand constant value */` |
|       15 |  9090 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  9091 |  |
|        - |  9092 | `/*` |
|        - |  9093 | ` * Section:` |
|        - |  9094 | ` *  Function handling functions.` |
|        - |  9095 | ` * Status:` |
|        - |  9096 | ` *    Stable.` |
|        - |  9097 | ` */` |
|        - |  9098 | `/*` |
|        - |  9099 | ` * int func_num_args(void)` |
|        - |  9100 | ` *   Returns the number of arguments passed to the function.` |
|        - |  9101 | ` * Parameters` |
|        - |  9102 | ` *   None.` |
|        - |  9103 | ` * Return` |
|        - |  9104 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  9105 | ` *  or -1 if called from the globe scope.` |
|        - |  9106 | ` */` |
|      944 |  9107 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9108 |  |
|        - |  9109 | `	VmFrame *pFrame;` |
|        - |  9110 | `	ph7_vm *pVm;` |
|        - |  9111 | `	/* Point to the target VM */` |
|      946 |  9112 | `	pVm = pCtx->pVm;` |
|        - |  9113 | `	/* Current frame */` |
|      946 |  9114 | `	pFrame = pVm->pFrame;` |
|      946 |  9115 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  9116 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  9117 | `		SXUNUSED(nArg);` |
|      ! 0 |  9118 | `		SXUNUSED(apArg);` |
|        - |  9119 | `		/* Global frame,return -1 */` |
|      ! 0 |  9120 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  9121 | `		return SXRET_OK;` |
|        - |  9122 | `	}` |
|        - |  9123 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  9124 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  9125 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  9126 | `	return SXRET_OK;` |
|      474 |  9127 |  |
|        - |  9128 | `/*` |
|        - |  9129 | ` * value func_get_arg(int $arg_num)` |
|        - |  9130 | ` *   Return an item from the argument list.` |
|        - |  9131 | ` * Parameters` |
|        - |  9132 | ` *  Argument number(index start from zero).` |
|        - |  9133 | ` * Return` |
|        - |  9134 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  9135 | ` */` |
|       22 |  9136 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9137 |  |
|       24 |  9138 | `	ph7_value *pObj = 0;` |
|       24 |  9139 | `	VmSlot *pSlot = 0;` |
|        - |  9140 | `	VmFrame *pFrame;` |
|        - |  9141 | `	ph7_vm *pVm;` |
|        - |  9142 | `	/* Point to the target VM */` |
|       24 |  9143 | `	pVm = pCtx->pVm;` |
|        - |  9144 | `	/* Current frame */` |
|       24 |  9145 | `	pFrame = pVm->pFrame;` |
|       24 |  9146 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  9147 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  9148 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  9149 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  9150 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9151 | `		return SXRET_OK;` |
|        - |  9152 | `	}` |
|        - |  9153 | `	/* Extract the desired index */` |
|       21 |  9154 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  9155 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  9156 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  9157 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9158 | `		return SXRET_OK;` |
|        - |  9159 | `	}` |
|        - |  9160 | `	/* Extract the desired argument */` |
|       21 |  9161 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  9162 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  9163 | `			/* Return the desired argument */` |
|       21 |  9164 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  9165 | `		}else{` |
|        - |  9166 | `			/* No such argument,return false */` |
|      ! 0 |  9167 | `			ph7_result_bool(pCtx,0);` |
|        - |  9168 | `		}` |
|       11 |  9169 | `	}else{` |
|        - |  9170 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  9171 | `		ph7_result_bool(pCtx,0);` |
|        - |  9172 | `	}` |
|       21 |  9173 | `	return SXRET_OK;` |
|       13 |  9174 |  |
|        - |  9175 | `/*` |
|        - |  9176 | ` * array func_get_args_byref(void)` |
|        - |  9177 | ` *   Returns an array comprising a function's argument list.` |
|        - |  9178 | ` * Parameters` |
|        - |  9179 | ` *  None.` |
|        - |  9180 | ` * Return` |
|        - |  9181 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  9182 | ` *  member of the current user-defined function's argument list.` |
|        - |  9183 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9184 | ` * NOTE:` |
|        - |  9185 | ` *  Arguments are returned to the array by reference.` |
|        - |  9186 | ` */` |
|        2 |  9187 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9188 |  |
|        - |  9189 | `	ph7_value *pArray;` |
|        - |  9190 | `	VmFrame *pFrame;` |
|        - |  9191 | `	VmSlot *aSlot;` |
|        - |  9192 | `	sxu32 n;` |
|        - |  9193 | `	/* Point to the current frame */` |
|        3 |  9194 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  9195 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  9196 | `	if( pFrame->pParent == 0 ){` |
|        - |  9197 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9198 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9199 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9200 | `		return SXRET_OK;` |
|        - |  9201 | `	}` |
|        - |  9202 | `	/* Create a new array */` |
|        3 |  9203 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9204 | `	if( pArray == 0 ){` |
|      ! 0 |  9205 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9206 | `		SXUNUSED(apArg);` |
|      ! 0 |  9207 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9208 | `		return SXRET_OK;` |
|        - |  9209 | `	}` |
|        - |  9210 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  9211 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  9212 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  9213 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  9214 | `	}` |
|        - |  9215 | `	/* Return the freshly created array */` |
|        3 |  9216 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9217 | `	return SXRET_OK;` |
|        2 |  9218 |  |
|        - |  9219 | `/*` |
|        - |  9220 | ` * array func_get_args(void)` |
|        - |  9221 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  9222 | ` * Parameters` |
|        - |  9223 | ` *  None.` |
|        - |  9224 | ` * Return` |
|        - |  9225 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  9226 | ` *  member of the current user-defined function's argument list.` |
|        - |  9227 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9228 | ` */` |
|       88 |  9229 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9230 |  |
|       90 |  9231 | `	ph7_value *pObj = 0;` |
|        - |  9232 | `	ph7_value *pArray;` |
|        - |  9233 | `	VmFrame *pFrame;` |
|        - |  9234 | `	VmSlot *aSlot;` |
|        - |  9235 | `	sxu32 n;` |
|        - |  9236 | `	/* Point to the current frame */` |
|       90 |  9237 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  9238 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  9239 | `	if( pFrame->pParent == 0 ){` |
|        - |  9240 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9241 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9242 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9243 | `		return SXRET_OK;` |
|        - |  9244 | `	}` |
|        - |  9245 | `	/* Create a new array */` |
|       90 |  9246 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  9247 | `	if( pArray == 0 ){` |
|      ! 0 |  9248 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9249 | `		SXUNUSED(apArg);` |
|      ! 0 |  9250 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9251 | `		return SXRET_OK;` |
|        - |  9252 | `	}` |
|        - |  9253 | `	/* Start filling the array with the given arguments */` |
|       90 |  9254 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  9255 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  9256 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  9257 | `		if( pObj ){` |
|      134 |  9258 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  9259 | `		}` |
|       68 |  9260 | `	}` |
|        - |  9261 | `	/* Return the freshly created array */` |
|       90 |  9262 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  9263 | `	return SXRET_OK;` |
|       46 |  9264 |  |
|        - |  9265 | `/*` |
|        - |  9266 | ` * bool function_exists(string $name)` |
|        - |  9267 | ` *  Return TRUE if the given function has been defined.` |
|        - |  9268 | ` * Parameters` |
|        - |  9269 | ` *  The name of the desired function.` |
|        - |  9270 | ` * Return` |
|        - |  9271 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  9272 | ` */` |
|     1680 |  9273 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9274 |  |
|        - |  9275 | `	const char *zName;` |
|        - |  9276 | `	ph7_vm *pVm;` |
|        - |  9277 | `	int nLen;` |
|        - |  9278 | `	int res;` |
|     1682 |  9279 | `	if( nArg < 1 ){` |
|        - |  9280 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  9281 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9282 | `		return SXRET_OK;` |
|        - |  9283 | `	}` |
|        - |  9284 | `	/* Point to the target VM */` |
|     1682 |  9285 | `	pVm = pCtx->pVm;` |
|        - |  9286 | `	/* Extract the function name */` |
|     1682 |  9287 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9288 | `	/* Assume the function is not defined */` |
|     1682 |  9289 | `	res = 0;` |
|        - |  9290 | `	/* Perform the lookup */` |
|     2520 |  9291 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1676 |  9292 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9293 | `			/* Function is defined */` |
|      206 |  9294 | `			res = 1;` |
|      102 |  9295 | `	}` |
|     1682 |  9296 | `	ph7_result_bool(pCtx,res);` |
|     1682 |  9297 | `	return SXRET_OK;` |
|      842 |  9298 |  |
|        - |  9299 | `/*` |
|        - |  9300 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9301 | ` * [i.e: Whether it is callable or not].` |
|        - |  9302 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  9303 | ` */` |
|    19196 |  9304 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  9305 |  |
|    19198 |  9306 | `	int res = 0;` |
|    19198 |  9307 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9308 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  9309 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  9310 | `		ph7_class_method *pMethod;` |
|      ! 0 |  9311 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  9312 | `		if( pMethod && CallInvoke ){` |
|        - |  9313 | `			ph7_value sResult;` |
|        - |  9314 | `			sxi32 rc;` |
|        - |  9315 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  9316 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  9317 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  9318 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  9319 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  9320 | `			}` |
|      ! 0 |  9321 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9322 | `		}` |
|    19198 |  9323 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  9324 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  9325 | `		if( pMap->nEntry == 2 ){` |
|        - |  9326 | `			ph7_class *pClass;` |
|        - |  9327 | `			ph7_value *pV;` |
|        - |  9328 | `			/* Extract the target class */` |
|       12 |  9329 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  9330 | `			if( pV ){` |
|       12 |  9331 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  9332 | `				if( pClass ){` |
|        - |  9333 | `					ph7_class_method *pMethod;` |
|        - |  9334 | `					/* Extract the target method */` |
|       10 |  9335 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  9336 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  9337 | `						/* Perform the lookup */` |
|       10 |  9338 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  9339 | `						if( pMethod ){` |
|        - |  9340 | `							/* Method is callable */` |
|        5 |  9341 | `							res = 1;` |
|        2 |  9342 | `						}` |
|        4 |  9343 | `					}` |
|        4 |  9344 | `				}` |
|        5 |  9345 | `			}` |
|        7 |  9346 | `		}` |
|    19185 |  9347 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  9348 | `		const char *zName;` |
|        - |  9349 | `		int nLen;` |
|        - |  9350 | `		/* Extract the name */` |
|     5242 |  9351 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  9352 | `		/* Perform the lookup */` |
|     5257 |  9353 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  9354 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9355 | `				/* Function is callable */` |
|     5224 |  9356 | `				res = 1;` |
|     2611 |  9357 | `		}` |
|     2620 |  9358 | `	}` |
|    19198 |  9359 | `	return res;` |
|        2 |  9360 |  |
|        - |  9361 | `/*` |
|        - |  9362 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  9363 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9364 | ` * Parameters` |
|        - |  9365 | ` * $name` |
|        - |  9366 | ` *    The callback function to check` |
|        - |  9367 | ` * $syntax_only` |
|        - |  9368 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  9369 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  9370 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  9371 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  9372 | ` *    a string.` |
|        - |  9373 | ` * Return` |
|        - |  9374 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  9375 | ` */` |
|       14 |  9376 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9377 |  |
|        - |  9378 | `	ph7_vm *pVm;` |
|        - |  9379 | `	int res;` |
|       15 |  9380 | `	if( nArg < 1 ){` |
|        - |  9381 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9382 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9383 | `		return SXRET_OK;` |
|        - |  9384 | `	}` |
|        - |  9385 | `	/* Point to the target VM */` |
|       15 |  9386 | `	pVm = pCtx->pVm;` |
|        - |  9387 | `	/* Perform the requested operation */` |
|       15 |  9388 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  9389 | `	ph7_result_bool(pCtx,res);` |
|       15 |  9390 | `	return SXRET_OK;` |
|        8 |  9391 |  |
|        - |  9392 | `/*` |
|        - |  9393 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  9394 | ` * defined below.` |
|        - |  9395 | ` */` |
|     1200 |  9396 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9397 |  |
|     1201 |  9398 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9399 | `	ph7_value sName;` |
|        - |  9400 | `	sxi32 rc;` |
|        - |  9401 | `	/* Prepare the function name for insertion */` |
|     1201 |  9402 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  9403 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9404 | `	/* Perform the insertion */` |
|     1201 |  9405 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  9406 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  9407 | `	return rc;` |
|        1 |  9408 |  |
|        - |  9409 | `/*` |
|        - |  9410 | ` * array get_defined_functions(void)` |
|        - |  9411 | ` *  Returns an array of all defined functions.` |
|        - |  9412 | ` * Parameter` |
|        - |  9413 | ` *  None.` |
|        - |  9414 | ` * Return` |
|        - |  9415 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  9416 | ` *  both built-in (internal) and user-defined.` |
|        - |  9417 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  9418 | ` *  defined ones using $arr["user"].` |
|        - |  9419 | ` * Note:` |
|        - |  9420 | ` *  NULL is returned on failure.` |
|        - |  9421 | ` */` |
|        2 |  9422 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9423 |  |
|        - |  9424 | `	ph7_value *pArray,*pEntry;` |
|        - |  9425 | `	/* NOTE:` |
|        - |  9426 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  9427 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  9428 | `	 */` |
|        3 |  9429 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9430 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9431 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9432 | `		SXUNUSED(apArg);` |
|        - |  9433 | `		/* Return NULL */` |
|      ! 0 |  9434 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9435 | `		return SXRET_OK;` |
|        - |  9436 | `	}` |
|        3 |  9437 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9438 | `	if( pEntry == 0 ){` |
|        - |  9439 | `		/* Return NULL */` |
|      ! 0 |  9440 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9441 | `		return SXRET_OK;` |
|        - |  9442 | `	}` |
|        - |  9443 | `	/* Fill with the appropriate information */` |
|        3 |  9444 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  9445 | `	/* Create the 'internal' index */` |
|        3 |  9446 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  9447 | `	/* Create the user-func array */` |
|        3 |  9448 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9449 | `	if( pEntry == 0 ){` |
|        - |  9450 | `		/* Return NULL */` |
|      ! 0 |  9451 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9452 | `		return SXRET_OK;` |
|        - |  9453 | `	}` |
|        - |  9454 | `	/* Fill with the appropriate information */` |
|        3 |  9455 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  9456 | `	/* Create the 'user' index */` |
|        3 |  9457 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  9458 | `	/* Return the multi-dimensional array */` |
|        3 |  9459 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9460 | `	return SXRET_OK;` |
|        2 |  9461 |  |
|        - |  9462 | `/*` |
|        - |  9463 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  9464 | ` *  Register a function for execution on shutdown.` |
|        - |  9465 | ` * Note` |
|        - |  9466 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  9467 | ` *  be called in the same order as they were registered.` |
|        - |  9468 | ` * Parameters` |
|        - |  9469 | ` *  $callback` |
|        - |  9470 | ` *   The shutdown callback to register.` |
|        - |  9471 | ` * $param` |
|        - |  9472 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  9473 | ` * Return` |
|        - |  9474 | ` *  Nothing.` |
|        - |  9475 | ` */` |
|        2 |  9476 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9477 |  |
|        - |  9478 | `	VmShutdownCB sEntry;` |
|        - |  9479 | `	int i,j;` |
|        3 |  9480 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  9481 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  9482 | `		return PH7_OK;` |
|        - |  9483 | `	}` |
|        - |  9484 | `	/* Zero the Entry */` |
|        3 |  9485 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  9486 | `	/* Initialize fields */` |
|        3 |  9487 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  9488 | `	/* Save the callback name for later invocation name */` |
|        3 |  9489 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  9490 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  9491 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  9492 | `	}` |
|        - |  9493 | `	/* Copy arguments */` |
|        3 |  9494 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  9495 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  9496 | `			/* Limit reached */` |
|      ! 0 |  9497 | `			break;` |
|        - |  9498 | `		}` |
|      ! 0 |  9499 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  9500 | `	}` |
|        3 |  9501 | `	sEntry.nArg = j;` |
|        - |  9502 | `	/* Install the callback */` |
|        3 |  9503 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  9504 | `	return PH7_OK;` |
|        2 |  9505 |  |
|        - |  9506 | `/*` |
|        - |  9507 | ` * Section:` |
|        - |  9508 | ` *  Class handling functions.` |
|        - |  9509 | ` * Status:` |
|        - |  9510 | ` *    Stable.` |
|        - |  9511 | ` */` |
|        - |  9512 | `/*` |
|        - |  9513 | ` * Extract the top active class. NULL is returned` |
|        - |  9514 | ` * if the class stack is empty.` |
|        - |  9515 | ` */` |
|      672 |  9516 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  9517 |  |
|      674 |  9518 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  9519 | `	ph7_class **apClass;` |
|      674 |  9520 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  9521 | `		/* Empty stack,return NULL */` |
|       15 |  9522 | `		return 0;` |
|        - |  9523 | `	}` |
|        - |  9524 | `	/* Peek the last entry */` |
|      660 |  9525 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      660 |  9526 | `	return apClass[pSet->nUsed - 1];` |
|      338 |  9527 |  |
|        - |  9528 | `/*` |
|        - |  9529 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  9530 | ` *   Get the class that declared the currently executing method.` |
|        - |  9531 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  9532 | ` *` |
|        - |  9533 | ` * Parameters` |
|        - |  9534 | ` *   pVm: Target VM` |
|        - |  9535 | ` *` |
|        - |  9536 | ` * Return` |
|        - |  9537 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  9538 | ` *   - Not executing within a class method` |
|        - |  9539 | ` *` |
|        - |  9540 | ` * Note` |
|        - |  9541 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  9542 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  9543 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  9544 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  9545 | ` *   declaring class.` |
|        - |  9546 | ` */` |
|       96 |  9547 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  9548 |  |
|       98 |  9549 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  9550 | `	ph7_vm_func *pVmFunc;` |
|        - |  9551 |  |
|        - |  9552 | `	/* Skip exception frames to find the actual method frame */` |
|       98 |  9553 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  9554 |  |
|        - |  9555 | `	/* Check if we're in a method context */` |
|       98 |  9556 | `	if( pFrame->pParent ){` |
|       94 |  9557 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       94 |  9558 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  9559 | `			/* Return the declaring class */` |
|       94 |  9560 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  9561 | `		}` |
|      ! 0 |  9562 | `	}` |
|        - |  9563 |  |
|        5 |  9564 | `	return 0;` |
|       50 |  9565 |  |
|        - |  9566 |  |
|        - |  9567 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  9568 | `/*` |
|        - |  9569 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  9570 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  9571 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  9572 | ` * return value indicates failure.` |
|        - |  9573 | ` */` |
|     1610 |  9574 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  9575 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  9576 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  9577 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  9578 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  9579 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  9580 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  9581 | `	)` |
|        2 |  9582 |  |
|        - |  9583 | `	ph7_value *aStack;` |
|        - |  9584 | `	VmInstr aInstr[2];` |
|        - |  9585 | `	int iCursor;` |
|        - |  9586 | `	int i;` |
|        - |  9587 | `	/* Create a new operand stack */` |
|     1612 |  9588 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1612 |  9589 | `	if( aStack == 0 ){` |
|      ! 0 |  9590 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  9591 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  9592 | `		return SXERR_MEM;` |
|        - |  9593 | `	}` |
|        - |  9594 | `	/* Fill the operand stack with the given arguments */` |
|     2330 |  9595 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      720 |  9596 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  9597 | `		/*` |
|        - |  9598 | `		 * Symisc eXtension:` |
|        - |  9599 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  9600 | `		 */` |
|      720 |  9601 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      361 |  9602 | `	}` |
|     1612 |  9603 | `	iCursor = nArg + 1;` |
|     1612 |  9604 | `	if( pThis ){` |
|        - |  9605 | `		/*` |
|        - |  9606 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  9607 | `		 */` |
|     1606 |  9608 | `		pThis->iRef++; /* Increment reference count */` |
|     1606 |  9609 | `		aStack[i].x.pOther = pThis;` |
|     1606 |  9610 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      802 |  9611 | `	}` |
|     1612 |  9612 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1612 |  9613 | `	i++;` |
|        - |  9614 | `	/* Push method name */` |
|     1612 |  9615 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1612 |  9616 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1612 |  9617 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1612 |  9618 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  9619 | `	/* Emit the CALL istruction */` |
|     1612 |  9620 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1612 |  9621 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1612 |  9622 | `	aInstr[0].iP2 = 0;` |
|     1612 |  9623 | `	aInstr[0].p3  = 0;` |
|        - |  9624 | `	/* Emit the DONE instruction */` |
|     1612 |  9625 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1612 |  9626 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1612 |  9627 | `	aInstr[1].iP2 = 0;` |
|     1612 |  9628 | `	aInstr[1].p3  = 0;` |
|        - |  9629 | `	/* Execute the method body (if available) */` |
|     1612 |  9630 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  9631 | `	/* Clean up the mess left behind */` |
|     1612 |  9632 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1612 |  9633 | `	return PH7_OK;` |
|      807 |  9634 |  |
|        - |  9635 | `/*` |
|        - |  9636 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  9637 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  9638 | ` * in the apArg[] array.` |
|        - |  9639 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  9640 | ` * return value indicates failure.` |
|        - |  9641 | ` */` |
|      966 |  9642 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  9643 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  9644 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  9645 | `	int nArg,          /* Total number of given arguments */` |
|        - |  9646 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  9647 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  9648 | `	)` |
|        2 |  9649 |  |
|        - |  9650 | `	ph7_value *aStack;` |
|        - |  9651 | `	VmInstr aInstr[2];` |
|        - |  9652 | `	int i;` |
|      968 |  9653 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  9654 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 |  9655 | `		if( pResult ){` |
|        - |  9656 | `			/* Assume a null return value */` |
|      ! 0 |  9657 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  9658 | `		}` |
|      479 |  9659 | `		return SXERR_INVALID;` |
|        - |  9660 | `	}` |
|      490 |  9661 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  9662 | `		/* Class method */` |
|       11 |  9663 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  9664 | `		ph7_class_method *pMethod = 0;` |
|       11 |  9665 | `		ph7_class_instance *pThis = 0;` |
|       11 |  9666 | `		ph7_class *pClass = 0;` |
|        - |  9667 | `		ph7_value *pValue;` |
|        - |  9668 | `		sxi32 rc;` |
|       11 |  9669 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  9670 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  9671 | `			if( pResult ){` |
|        - |  9672 | `				/* Assume a null return value */` |
|      ! 0 |  9673 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9674 | `			}` |
|      ! 0 |  9675 | `			return SXRET_OK;` |
|        - |  9676 | `		}` |
|        - |  9677 | `		/* Extract the class name or an instance of it */` |
|       11 |  9678 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  9679 | `		if( pValue ){` |
|       11 |  9680 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  9681 | `		}` |
|       11 |  9682 | `		if( pClass == 0 ){` |
|        - |  9683 | `			/* No such class,return NULL */` |
|      ! 0 |  9684 | `			if( pResult ){` |
|      ! 0 |  9685 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9686 | `			}` |
|      ! 0 |  9687 | `			return SXRET_OK;` |
|        - |  9688 | `		}` |
|       11 |  9689 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9690 | `			/* Point to the class instance */` |
|        5 |  9691 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  9692 | `		}` |
|        - |  9693 | `		/* Try to extract the method */` |
|       11 |  9694 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  9695 | `		if( pValue ){` |
|       11 |  9696 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  9697 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  9698 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  9699 | `			}` |
|        5 |  9700 | `		}` |
|       11 |  9701 | `		if( pMethod == 0 ){` |
|        - |  9702 | `			/* No such method,return NULL */` |
|      ! 0 |  9703 | `			if( pResult ){` |
|      ! 0 |  9704 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9705 | `			}` |
|      ! 0 |  9706 | `			return SXRET_OK;` |
|        - |  9707 | `		}` |
|        - |  9708 | `		/* Call the class method */` |
|       11 |  9709 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  9710 | `		return rc;` |
|        - |  9711 | `	}` |
|        - |  9712 | `	/* Create a new operand stack */` |
|      480 |  9713 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      480 |  9714 | `	if( aStack == 0 ){` |
|      ! 0 |  9715 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  9716 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  9717 | `		if( pResult ){` |
|        - |  9718 | `			/* Assume a null return value */` |
|      ! 0 |  9719 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  9720 | `		}` |
|      ! 0 |  9721 | `		return SXERR_MEM;` |
|        - |  9722 | `	}` |
|        - |  9723 | `	/* Fill the operand stack with the given arguments */` |
|     1534 |  9724 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1056 |  9725 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  9726 | `		/*` |
|        - |  9727 | `		 * Symisc eXtension:` |
|        - |  9728 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  9729 | `		 */` |
|     1056 |  9730 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      529 |  9731 | `	}` |
|        - |  9732 | `	/* Push the function name */` |
|      480 |  9733 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      480 |  9734 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  9735 | `	/* Emit the CALL istruction */` |
|      480 |  9736 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      480 |  9737 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      480 |  9738 | `	aInstr[0].iP2 = 0;` |
|      480 |  9739 | `	aInstr[0].p3  = 0;` |
|        - |  9740 | `	/* Emit the DONE instruction */` |
|      480 |  9741 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      480 |  9742 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      480 |  9743 | `	aInstr[1].iP2 = 0;` |
|      480 |  9744 | `	aInstr[1].p3  = 0;` |
|        - |  9745 | `	/* Execute the function body (if available) */` |
|      480 |  9746 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  9747 | `	/* Clean up the mess left behind */` |
|      480 |  9748 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      480 |  9749 | `	return PH7_OK;` |
|      485 |  9750 |  |
|        - |  9751 | `/*` |
|        - |  9752 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  9753 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  9754 | ` * parameter.` |
|        - |  9755 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  9756 | ` * return value indicates failure.` |
|        - |  9757 | ` */` |
|      236 |  9758 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  9759 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  9760 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  9761 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  9762 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  9763 | `	)` |
|        1 |  9764 |  |
|        - |  9765 | `	ph7_value *pArg;` |
|        - |  9766 | `	SySet aArg;` |
|        - |  9767 | `	va_list ap;` |
|        - |  9768 | `	sxi32 rc;` |
|      237 |  9769 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  9770 | `	/* Copy arguments one after one */` |
|      237 |  9771 | `	va_start(ap,pResult);` |
|      393 |  9772 | `	for(;;){` |
|      787 |  9773 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  9774 | `		if( pArg == 0 ){` |
|      237 |  9775 | `			break;` |
|        - |  9776 | `		}` |
|      551 |  9777 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  9778 | `	}` |
|        - |  9779 | `	/* Call the core routine */` |
|      237 |  9780 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  9781 | `	/* Cleanup */` |
|      237 |  9782 | `	SySetRelease(&aArg);` |
|      237 |  9783 | `	return rc;` |
|        1 |  9784 |  |
|        - |  9785 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  9786 | `/*` |
|        - |  9787 | ` * bool defined(string $name)` |
|        - |  9788 | ` *  Checks whether a given named constant exists.` |
|        - |  9789 | ` * Parameter:` |
|        - |  9790 | ` *  Name of the desired constant.` |
|        - |  9791 | ` * Return` |
|        - |  9792 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  9793 | ` */` |
|       14 |  9794 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9795 |  |
|        - |  9796 | `	const char *zName;` |
|       16 |  9797 | `	int nLen = 0;` |
|       16 |  9798 | `	int res = 0;` |
|       16 |  9799 | `	if( nArg < 1 ){` |
|        - |  9800 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  9801 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  9802 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9803 | `		return SXRET_OK;` |
|        - |  9804 | `	}` |
|        - |  9805 | `	/* Extract constant name */` |
|       16 |  9806 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9807 | `	/* Perform the lookup */` |
|       16 |  9808 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9809 | `		/* Already defined */` |
|       10 |  9810 | `		res = 1;` |
|        4 |  9811 | `	}` |
|       16 |  9812 | `	ph7_result_bool(pCtx,res);` |
|       16 |  9813 | `	return SXRET_OK;` |
|        9 |  9814 |  |
|        - |  9815 | `/*` |
|        - |  9816 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  9817 | ` * below.` |
|        - |  9818 | ` */` |
|       10 |  9819 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  9820 |  |
|       12 |  9821 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  9822 | `	/* Expand constant value */` |
|       12 |  9823 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 |  9824 |  |
|        - |  9825 | `/*` |
|        - |  9826 | ` * bool define(string $constant_name,expression value)` |
|        - |  9827 | ` *  Defines a named constant at runtime.` |
|        - |  9828 | ` * Parameter:` |
|        - |  9829 | ` *  $constant_name` |
|        - |  9830 | ` *   The name of the constant` |
|        - |  9831 | ` *  $value` |
|        - |  9832 | ` *   Constant value` |
|        - |  9833 | ` * Return:` |
|        - |  9834 | ` *   TRUE on success,FALSE on failure.` |
|        - |  9835 | ` */` |
|       12 |  9836 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9837 |  |
|        - |  9838 | `	const char *zName;  /* Constant name */` |
|        - |  9839 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 |  9840 | `	int nLen = 0;       /* Name length */` |
|        - |  9841 | `	sxi32 rc;` |
|       14 |  9842 | `	if( nArg < 2 ){` |
|        - |  9843 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  9844 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  9845 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9846 | `		return SXRET_OK;` |
|        - |  9847 | `	}` |
|       14 |  9848 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  9849 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  9850 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9851 | `		return SXRET_OK;` |
|        - |  9852 | `	}` |
|        - |  9853 | `	/* Extract constant name */` |
|       14 |  9854 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 |  9855 | `	if( nLen < 1 ){` |
|      ! 0 |  9856 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  9857 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9858 | `		return SXRET_OK;` |
|        - |  9859 | `	}` |
|        - |  9860 | `	/* Duplicate constant value */` |
|       14 |  9861 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 |  9862 | `	if( pValue == 0 ){` |
|      ! 0 |  9863 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9864 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9865 | `		return SXRET_OK;` |
|        - |  9866 | `	}` |
|        - |  9867 | `	/* Initialize the memory object */` |
|       14 |  9868 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  9869 | `	/* Register the constant */` |
|       14 |  9870 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 |  9871 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9872 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  9873 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9874 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9875 | `		return SXRET_OK;` |
|        - |  9876 | `	}` |
|        - |  9877 | `	/* Duplicate constant value */` |
|       14 |  9878 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 |  9879 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  9880 | `		/* Lower case the constant name */` |
|      ! 0 |  9881 | `		char *zCur = (char *)zName;` |
|      ! 0 |  9882 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  9883 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  9884 | `				/* UTF-8 stream */` |
|      ! 0 |  9885 | `				zCur++;` |
|      ! 0 |  9886 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  9887 | `					zCur++;` |
|      ! 0 |  9888 | `				}` |
|      ! 0 |  9889 | `				continue;` |
|        - |  9890 | `			}` |
|      ! 0 |  9891 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  9892 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  9893 | `				zCur[0] = (char)c;` |
|      ! 0 |  9894 | `			}` |
|      ! 0 |  9895 | `			zCur++;` |
|      ! 0 |  9896 | `		}` |
|        - |  9897 | `		/* Finally,register the constant */` |
|      ! 0 |  9898 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  9899 | `	}` |
|        - |  9900 | `	/* All done,return TRUE */` |
|       14 |  9901 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9902 | `	return SXRET_OK;` |
|        8 |  9903 |  |
|        - |  9904 | `/*` |
|        - |  9905 | ` * value constant(string $name)` |
|        - |  9906 | ` *  Returns the value of a constant` |
|        - |  9907 | ` * Parameter` |
|        - |  9908 | ` *  $name` |
|        - |  9909 | ` *    Name of the constant.` |
|        - |  9910 | ` * Return` |
|        - |  9911 | ` *  Constant value or NULL if not defined.` |
|        - |  9912 | ` */` |
|        8 |  9913 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9914 |  |
|        - |  9915 | `	SyHashEntry *pEntry;` |
|        - |  9916 | `	ph7_constant *pCons;` |
|        - |  9917 | `	const char *zName; /* Constant name */` |
|        - |  9918 | `	ph7_value sVal;    /* Constant value */` |
|        - |  9919 | `	int nLen;` |
|       10 |  9920 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9921 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  9922 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  9923 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9924 | `		return SXRET_OK;` |
|        - |  9925 | `	}` |
|        - |  9926 | `	/* Extract the constant name */` |
|       10 |  9927 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9928 | `	/* Perform the query */` |
|       10 |  9929 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  9930 | `	if( pEntry == 0 ){` |
|        3 |  9931 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  9932 | `		ph7_result_null(pCtx);` |
|        3 |  9933 | `		return SXRET_OK;` |
|        - |  9934 | `	}` |
|        8 |  9935 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  9936 | `	/* Point to the structure that describe the constant */` |
|        8 |  9937 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  9938 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  9939 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  9940 | `	/* Return that value */` |
|        8 |  9941 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  9942 | `	/* Cleanup */` |
|        8 |  9943 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  9944 | `	return SXRET_OK;` |
|        6 |  9945 |  |
|        - |  9946 | `/*` |
|        - |  9947 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9948 | ` * defined below.` |
|        - |  9949 | ` */` |
|      452 |  9950 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9951 |  |
|      453 |  9952 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9953 | `	ph7_value sName;` |
|        - |  9954 | `	sxi32 rc;` |
|        - |  9955 | `	/* Prepare the constant name for insertion */` |
|      453 |  9956 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 |  9957 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9958 | `	/* Perform the insertion */` |
|      453 |  9959 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 |  9960 | `	PH7_MemObjRelease(&sName);` |
|      453 |  9961 | `	return rc;` |
|        1 |  9962 |  |
|        - |  9963 | `/*` |
|        - |  9964 | ` * array get_defined_constants(void)` |
|        - |  9965 | ` *  Returns an associative array with the names of all defined` |
|        - |  9966 | ` *  constants.` |
|        - |  9967 | ` * Parameters` |
|        - |  9968 | ` *  NONE.` |
|        - |  9969 | ` * Returns` |
|        - |  9970 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9971 | ` */` |
|        2 |  9972 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9973 |  |
|        - |  9974 | `	ph7_value *pArray;` |
|        - |  9975 | `	/* Create the array first*/` |
|        3 |  9976 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9977 | `	if( pArray == 0 ){` |
|      ! 0 |  9978 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9979 | `		SXUNUSED(apArg);` |
|        - |  9980 | `		/* Return NULL */` |
|      ! 0 |  9981 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9982 | `		return SXRET_OK;` |
|        - |  9983 | `	}` |
|        - |  9984 | `	/* Fill the array with the defined constants */` |
|        3 |  9985 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9986 | `	/* Return the created array */` |
|        3 |  9987 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9988 | `	return SXRET_OK;` |
|        2 |  9989 |  |
|        - |  9990 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9991 | `/*` |
|        - |  9992 | ` * Section:` |
|        - |  9993 | ` *  Random numbers/string generators.` |
|        - |  9994 | ` * Status:` |
|        - |  9995 | ` *    Stable.` |
|        - |  9996 | ` */` |
|        - |  9997 | `/*` |
|        - |  9998 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9999 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 10000 | ` * used by te SQLite3 library.` |
|        - | 10001 | ` */` |
|     2595 | 10002 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 10003 |  |
|        - | 10004 | `	sxu32 iNum;` |
|     2597 | 10005 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2597 | 10006 | `	return iNum;` |
|        2 | 10007 |  |
|        - | 10008 | `/*` |
|        - | 10009 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 10010 | ` * Note that the generated string is NOT null terminated.` |
|        - | 10011 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 10012 | ` * by te SQLite3 library.` |
|        - | 10013 | ` */` |
|   135136 | 10014 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 10015 |  |
|        - | 10016 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 10017 | `	int i;` |
|        - | 10018 | `	/* Generate a binary string first */` |
|   135138 | 10019 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 10020 | `	/* Turn the binary string into english based alphabet */` |
|  1486666 | 10021 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1351530 | 10022 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   675766 | 10023 | `	 }` |
|   135138 | 10024 |  |
|        - | 10025 | `/*` |
|        - | 10026 | ` * int rand()` |
|        - | 10027 | ` * int mt_rand()` |
|        - | 10028 | ` * int rand(int $min,int $max)` |
|        - | 10029 | ` * int mt_rand(int $min,int $max)` |
|        - | 10030 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 10031 | ` * Parameter` |
|        - | 10032 | ` *  $min` |
|        - | 10033 | ` *    The lowest value to return (default: 0)` |
|        - | 10034 | ` *  $max` |
|        - | 10035 | ` *   The highest value to return (default: getrandmax())` |
|        - | 10036 | ` * Return` |
|        - | 10037 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 10038 | ` * Note:` |
|        - | 10039 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10040 | ` *  by te SQLite3 library.` |
|        - | 10041 | ` */` |
|       20 | 10042 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10043 |  |
|        - | 10044 | `	sxu32 iNum;` |
|        - | 10045 | `	/* Generate the random number */` |
|       21 | 10046 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 10047 | `	if( nArg > 1 ){` |
|        - | 10048 | `		sxu32 iMin,iMax;` |
|        3 | 10049 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 10050 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 10051 | `		if( iMin < iMax ){` |
|        3 | 10052 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 10053 | `			if( iDiv > 0 ){` |
|        3 | 10054 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 10055 | `			}` |
|        1 | 10056 | `		}else if(iMax > 0 ){` |
|      ! 0 | 10057 | `			iNum %= iMax;` |
|      ! 0 | 10058 | `		}` |
|        1 | 10059 | `	}` |
|        - | 10060 | `	/* Return the number */` |
|       21 | 10061 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 10062 | `	return SXRET_OK;` |
|        1 | 10063 |  |
|        - | 10064 | `/*` |
|        - | 10065 | ` * int getrandmax(void)` |
|        - | 10066 | ` * int mt_getrandmax(void)` |
|        - | 10067 | ` * int rc4_getrandmax(void)` |
|        - | 10068 | ` *   Show largest possible random value` |
|        - | 10069 | ` * Return` |
|        - | 10070 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 10071 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 10072 | ` * Note:` |
|        - | 10073 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10074 | ` *  by te SQLite3 library.` |
|        - | 10075 | ` */` |
|        4 | 10076 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10077 |  |
|        2 | 10078 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 10079 | `	SXUNUSED(apArg);` |
|        5 | 10080 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 10081 | `	return SXRET_OK;` |
|        1 | 10082 |  |
|        - | 10083 | `/*` |
|        - | 10084 | ` * string rand_str()` |
|        - | 10085 | ` * string rand_str(int $len)` |
|        - | 10086 | ` *  Generate a random string (English alphabet).` |
|        - | 10087 | ` * Parameter` |
|        - | 10088 | ` *  $len` |
|        - | 10089 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 10090 | ` * Return` |
|        - | 10091 | ` *   A pseudo random string.` |
|        - | 10092 | ` * Note:` |
|        - | 10093 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10094 | ` *  by te SQLite3 library.` |
|        - | 10095 | ` *  This function is a symisc extension.` |
|        - | 10096 | ` */` |
|      120 | 10097 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10098 |  |
|        - | 10099 | `	char zString[1024];` |
|      122 | 10100 | `	int iLen = 0x10;` |
|      122 | 10101 | `	if( nArg > 0 ){` |
|        - | 10102 | `		/* Get the desired length */` |
|      122 | 10103 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 10104 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 10105 | `			/* Default length */` |
|        3 | 10106 | `			iLen = 0x10;` |
|        1 | 10107 | `		}` |
|       60 | 10108 | `	}` |
|        - | 10109 | `	/* Generate the random string */` |
|      122 | 10110 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 10111 | `	/* Return the generated string */` |
|      122 | 10112 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 10113 | `	return SXRET_OK;` |
|        2 | 10114 |  |
|        - | 10115 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10116 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10117 | `/* Unique ID private data */` |
|        - | 10118 | `struct unique_id_data` |
|        - | 10119 |  |
|        - | 10120 | `	ph7_context *pCtx; /* Call context */` |
|        - | 10121 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 10122 | `};` |
|        - | 10123 | `/*` |
|        - | 10124 | ` * Binary to hex consumer callback.` |
|        - | 10125 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 10126 | ` * defined below.` |
|        - | 10127 | ` */` |
|      192 | 10128 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 10129 |  |
|      193 | 10130 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 10131 | `	sxu32 nBuflen;` |
|        - | 10132 | `	/* Extract result buffer length */` |
|      193 | 10133 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 10134 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 10135 | `			/*` |
|        - | 10136 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 10137 | `			 * string will be 13 characters long` |
|        - | 10138 | `			 */` |
|       25 | 10139 | `		return SXERR_ABORT;` |
|        - | 10140 | `	}` |
|      169 | 10141 | `	if( nBuflen > 22 ){` |
|      ! 0 | 10142 | `		return SXERR_ABORT;` |
|        - | 10143 | `	}` |
|        - | 10144 | `	/* Safely Consume the hex stream */` |
|      169 | 10145 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 10146 | `	return SXRET_OK;` |
|       97 | 10147 |  |
|        - | 10148 | `/*` |
|        - | 10149 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 10150 | ` *  Generate a unique ID` |
|        - | 10151 | ` * Parameter` |
|        - | 10152 | ` * $prefix` |
|        - | 10153 | ` *  Append this prefix to the generated unique ID.` |
|        - | 10154 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 10155 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 10156 | ` * $more_entropy` |
|        - | 10157 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 10158 | ` *  that the result will be unique.` |
|        - | 10159 | ` * Return` |
|        - | 10160 | ` *  Returns the unique identifier, as a string.` |
|        - | 10161 | ` */` |
|       24 | 10162 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10163 |  |
|        - | 10164 | `	struct unique_id_data sUniq;` |
|        - | 10165 | `	unsigned char zDigest[20];` |
|       25 | 10166 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10167 | `	const char *zPrefix;` |
|        - | 10168 | `	SHA1Context sCtx;` |
|        - | 10169 | `	char zRandom[7];` |
|        - | 10170 | `	int nPrefix;` |
|        - | 10171 | `	int entropy;` |
|        - | 10172 | `	/* Generate a random string first */` |
|       25 | 10173 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 10174 | `	/* Initialize fields */` |
|       25 | 10175 | `	zPrefix = 0;` |
|       25 | 10176 | `	nPrefix = 0;` |
|       25 | 10177 | `	entropy = 0;` |
|       25 | 10178 | `	if( nArg > 0 ){` |
|        - | 10179 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 10180 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 10181 | `		if( nArg > 1 ){` |
|      ! 0 | 10182 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 10183 | `		}` |
|      ! 0 | 10184 | `	}` |
|       25 | 10185 | `	SHA1Init(&sCtx);` |
|        - | 10186 | `	/* Generate the random ID */` |
|       25 | 10187 | `	if( nPrefix > 0 ){` |
|      ! 0 | 10188 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 10189 | `	}` |
|        - | 10190 | `	/* Append the random ID */` |
|       25 | 10191 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 10192 | `	/* Append the random string */` |
|       25 | 10193 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 10194 | `	/* Increment the number */` |
|       25 | 10195 | `	pVm->unique_id++;` |
|       25 | 10196 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 10197 | `	/* Hexify the digest */` |
|       25 | 10198 | `	sUniq.pCtx = pCtx;` |
|       25 | 10199 | `	sUniq.entropy = entropy;` |
|       25 | 10200 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 10201 | `	/* All done */` |
|       25 | 10202 | `	return PH7_OK;` |
|        1 | 10203 |  |
|        - | 10204 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10205 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10206 | `/*` |
|        - | 10207 | ` * Section:` |
|        - | 10208 | ` *  Language construct implementation as foreign functions.` |
|        - | 10209 | ` * Status:` |
|        - | 10210 | ` *    Stable.` |
|        - | 10211 | ` */` |
|        - | 10212 | `/*` |
|        - | 10213 | ` * void echo($string...)` |
|        - | 10214 | ` *  Output one or more messages.` |
|        - | 10215 | ` * Parameters` |
|        - | 10216 | ` *  $string` |
|        - | 10217 | ` *   Message to output.` |
|        - | 10218 | ` * Return` |
|        - | 10219 | ` *  NULL.` |
|        - | 10220 | ` */` |
|      ! 0 | 10221 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10222 |  |
|        - | 10223 | `	const char *zData;` |
|      ! 0 | 10224 | `	int nDataLen = 0;` |
|        - | 10225 | `	ph7_vm *pVm;` |
|        - | 10226 | `	int i,rc;` |
|        - | 10227 | `	/* Point to the target VM */` |
|      ! 0 | 10228 | `	pVm = pCtx->pVm;` |
|        - | 10229 | `	/* Output */` |
|      ! 0 | 10230 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 10231 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 10232 | `		if( nDataLen > 0 ){` |
|      ! 0 | 10233 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 10234 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 10235 | `			if( rc == SXERR_ABORT ){` |
|        - | 10236 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10237 | `				return PH7_ABORT;` |
|        - | 10238 | `			}` |
|      ! 0 | 10239 | `		}` |
|      ! 0 | 10240 | `	}` |
|      ! 0 | 10241 | `	return SXRET_OK;` |
|      ! 0 | 10242 |  |
|        - | 10243 | `/*` |
|        - | 10244 | ` * int print($string...)` |
|        - | 10245 | ` *  Output one or more messages.` |
|        - | 10246 | ` * Parameters` |
|        - | 10247 | ` *  $string` |
|        - | 10248 | ` *   Message to output.` |
|        - | 10249 | ` * Return` |
|        - | 10250 | ` *  1 always.` |
|        - | 10251 | ` */` |
|        2 | 10252 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10253 |  |
|        - | 10254 | `	const char *zData;` |
|        3 | 10255 | `	int nDataLen = 0;` |
|        - | 10256 | `	ph7_vm *pVm;` |
|        - | 10257 | `	int i,rc;` |
|        - | 10258 | `	/* Point to the target VM */` |
|        3 | 10259 | `	pVm = pCtx->pVm;` |
|        - | 10260 | `	/* Output */` |
|        5 | 10261 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 10262 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 10263 | `		if( nDataLen > 0 ){` |
|        3 | 10264 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 10265 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 10266 | `			if( rc == SXERR_ABORT ){` |
|        - | 10267 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10268 | `				return PH7_ABORT;` |
|        - | 10269 | `			}` |
|        1 | 10270 | `		}` |
|        2 | 10271 | `	}` |
|        - | 10272 | `	/* Return 1 */` |
|        3 | 10273 | `	ph7_result_int(pCtx,1);` |
|        3 | 10274 | `	return SXRET_OK;` |
|        2 | 10275 |  |
|        - | 10276 | `/*` |
|        - | 10277 | ` * void exit(string $msg)` |
|        - | 10278 | ` * void exit(int $status)` |
|        - | 10279 | ` * void die(string $ms)` |
|        - | 10280 | ` * void die(int $status)` |
|        - | 10281 | ` *   Output a message and terminate program execution.` |
|        - | 10282 | ` * Parameter` |
|        - | 10283 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 10284 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 10285 | ` *  and not printed` |
|        - | 10286 | ` * Return` |
|        - | 10287 | ` *  NULL` |
|        - | 10288 | ` */` |
|      ! 0 | 10289 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10290 |  |
|      ! 0 | 10291 | `	if( nArg > 0 ){` |
|      ! 0 | 10292 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 10293 | `			const char *zData;` |
|      ! 0 | 10294 | `			int iLen = 0;` |
|        - | 10295 | `			/* Print exit message */` |
|      ! 0 | 10296 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 10297 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 10298 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 10299 | `			sxi32 iExitStatus;` |
|        - | 10300 | `			/* Record exit status code */` |
|      ! 0 | 10301 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 10302 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 10303 | `		}` |
|      ! 0 | 10304 | `	}` |
|        - | 10305 | `	/* Check if we are in an included file */` |
|      ! 0 | 10306 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 10307 | `		/* Exit the entire process */` |
|      ! 0 | 10308 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 10309 | `	}` |
|        - | 10310 | `	/* Abort processing immediately */` |
|      ! 0 | 10311 | `	return PH7_ABORT;` |
|      ! 0 | 10312 |  |
|        - | 10313 | `/*` |
|        - | 10314 | ` * bool isset($var,...)` |
|        - | 10315 | ` *  Finds out whether a variable is set.` |
|        - | 10316 | ` * Parameters` |
|        - | 10317 | ` *  One or more variable to check.` |
|        - | 10318 | ` * Return` |
|        - | 10319 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 10320 | ` */` |
|    81310 | 10321 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10322 |  |
|        - | 10323 | `	ph7_value *pObj;` |
|    81312 | 10324 | `	int res = 0;` |
|        - | 10325 | `	int i;` |
|    81312 | 10326 | `	if( nArg < 1 ){` |
|        - | 10327 | `		/* Missing arguments,return false */` |
|      ! 0 | 10328 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 10329 | `		return SXRET_OK;` |
|        - | 10330 | `	}` |
|        - | 10331 | `	/* Iterate over available arguments */` |
|   106784 | 10332 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    81312 | 10333 | `		pObj = apArg[i];` |
|    81312 | 10334 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    55220 | 10335 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10336 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 10337 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 10338 | `			}` |
|    27609 | 10339 | `		}` |
|    81312 | 10340 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    81312 | 10341 | `		if( !res ){` |
|        - | 10342 | `			/* Variable not set,return FALSE */` |
|    55840 | 10343 | `			ph7_result_bool(pCtx,0);` |
|    55840 | 10344 | `			return SXRET_OK;` |
|        - | 10345 | `		}` |
|    12738 | 10346 | `	}` |
|        - | 10347 | `	/* All given variable are set,return TRUE */` |
|    25474 | 10348 | `	ph7_result_bool(pCtx,1);` |
|    25474 | 10349 | `	return SXRET_OK;` |
|    40657 | 10350 |  |
|        - | 10351 | `/*` |
|        - | 10352 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 10353 | ` * frame,the reference table and discard it's contents.` |
|        - | 10354 | ` * This function never fail and always return SXRET_OK.` |
|        - | 10355 | ` */` |
|  3059624 | 10356 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 10357 |  |
|        - | 10358 | `	ph7_value *pObj;` |
|        - | 10359 | `	VmRefObj *pRef;` |
|  3059626 | 10360 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3059626 | 10361 | `	if( pObj ){` |
|        - | 10362 | `		/* Release the object */` |
|  3059626 | 10363 | `		PH7_MemObjRelease(pObj);` |
|  1529812 | 10364 | `	}` |
|        - | 10365 | `	/* Remove old reference links */` |
|  3059626 | 10366 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3059626 | 10367 | `	if( pRef ){` |
|  3059620 | 10368 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 10369 | `		/* Unlink from the reference table */` |
|  3059620 | 10370 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3059620 | 10371 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 10372 | `			VmSlot sFree;` |
|        - | 10373 | `			/* Restore to the free list */` |
|  3059614 | 10374 | `			sFree.nIdx = nObjIdx;` |
|  3059614 | 10375 | `			sFree.pUserData = 0;` |
|  3059614 | 10376 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1529806 | 10377 | `		}` |
|  1529809 | 10378 | `	}` |
|  3059626 | 10379 | `	return SXRET_OK;` |
|        2 | 10380 |  |
|        - | 10381 | `/*` |
|        - | 10382 | ` * void unset($var,...)` |
|        - | 10383 | ` *   Unset one or more given variable.` |
|        - | 10384 | ` * Parameters` |
|        - | 10385 | ` *  One or more variable to unset.` |
|        - | 10386 | ` * Return` |
|        - | 10387 | ` *  Nothing.` |
|        - | 10388 | ` */` |
|     7114 | 10389 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10390 |  |
|        - | 10391 | `	ph7_value *pObj;` |
|        - | 10392 | `	ph7_vm *pVm;` |
|        - | 10393 | `	int i;` |
|        - | 10394 | `	/* Point to the target VM */` |
|     7116 | 10395 | `	pVm = pCtx->pVm;` |
|        - | 10396 | `	/* Iterate and unset */` |
|    14230 | 10397 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7116 | 10398 | `		pObj = apArg[i];` |
|     7116 | 10399 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 10400 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10401 | `				/* Throw an error */` |
|      ! 0 | 10402 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 10403 | `			}` |
|      ! 0 | 10404 | `		}else{` |
|     7116 | 10405 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 10406 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7116 | 10407 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7110 | 10408 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3554 | 10409 | `			}` |
|        - | 10410 | `		}` |
|     3559 | 10411 | `	}` |
|     7116 | 10412 | `	return SXRET_OK;` |
|        2 | 10413 |  |
|        - | 10414 | `/*` |
|        - | 10415 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 10416 | ` */` |
|      110 | 10417 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10418 |  |
|      111 | 10419 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 10420 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10421 | `	ph7_value *pObj;` |
|        - | 10422 | `	sxu32 nIdx;` |
|        - | 10423 | `	/* Extract the memory object */` |
|      111 | 10424 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 10425 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 10426 | `	if( pObj ){` |
|      111 | 10427 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 10428 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 10429 | `				SyString sName;` |
|        - | 10430 | `				ph7_value sKey;` |
|        - | 10431 | `				/* Perform the insertion */` |
|      109 | 10432 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 10433 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 10434 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 10435 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 10436 | `			}` |
|       54 | 10437 | `		}` |
|       55 | 10438 | `	}` |
|      111 | 10439 | `	return SXRET_OK;` |
|        1 | 10440 |  |
|        - | 10441 | `/*` |
|        - | 10442 | ` * array get_defined_vars(void)` |
|        - | 10443 | ` *  Returns an array of all defined variables.` |
|        - | 10444 | ` * Parameter` |
|        - | 10445 | ` *  None` |
|        - | 10446 | ` * Return` |
|        - | 10447 | ` *  An array with all the variables defined in the current scope.` |
|        - | 10448 | ` */` |
|        2 | 10449 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10450 |  |
|        3 | 10451 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10452 | `	ph7_value *pArray;` |
|        - | 10453 | `	/* Create a new array */` |
|        3 | 10454 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10455 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10456 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10457 | `		SXUNUSED(apArg);` |
|        - | 10458 | `		/* Return NULL */` |
|      ! 0 | 10459 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10460 | `		return SXRET_OK;` |
|        - | 10461 | `	}` |
|        - | 10462 | `	/* Superglobals first */` |
|        3 | 10463 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 10464 | `	/* Then variable defined in the current frame */` |
|        3 | 10465 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 10466 | `	/* Finally,return the created array */` |
|        3 | 10467 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10468 | `	return SXRET_OK;` |
|        2 | 10469 |  |
|        - | 10470 | `/*` |
|        - | 10471 | ` * bool gettype($var)` |
|        - | 10472 | ` *  Get the type of a variable` |
|        - | 10473 | ` * Parameters` |
|        - | 10474 | ` *   $var` |
|        - | 10475 | ` *    The variable being type checked.` |
|        - | 10476 | ` * Return` |
|        - | 10477 | ` *   String representation of the given variable type.` |
|        - | 10478 | ` */` |
|       32 | 10479 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10480 |  |
|       34 | 10481 | `	const char *zType = "Empty";` |
|       34 | 10482 | `	if( nArg > 0 ){` |
|       34 | 10483 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 10484 | `	}` |
|        - | 10485 | `	/* Return the variable type */` |
|       34 | 10486 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 10487 | `	return SXRET_OK;` |
|        2 | 10488 |  |
|        - | 10489 | `/*` |
|        - | 10490 | ` * string get_resource_type(resource $handle)` |
|        - | 10491 | ` *  This function gets the type of the given resource.` |
|        - | 10492 | ` * Parameters` |
|        - | 10493 | ` *  $handle` |
|        - | 10494 | ` *  The evaluated resource handle.` |
|        - | 10495 | ` * Return` |
|        - | 10496 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 10497 | ` *  representing its type. If the type is not identified by this function` |
|        - | 10498 | ` *  the return value will be the string Unknown.` |
|        - | 10499 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 10500 | ` *  is not a resource.` |
|        - | 10501 | ` */` |
|        2 | 10502 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10503 |  |
|        3 | 10504 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 10505 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 10506 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10507 | `		return PH7_OK;` |
|        - | 10508 | `	}` |
|        3 | 10509 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 10510 | `	return SXRET_OK;` |
|        2 | 10511 |  |
|        - | 10512 | `/*` |
|        - | 10513 | ` * void var_dump(expression,....)` |
|        - | 10514 | ` *   var_dump � Dumps information about a variable` |
|        - | 10515 | ` * Parameters` |
|        - | 10516 | ` *   One or more expression to dump.` |
|        - | 10517 | ` * Returns` |
|        - | 10518 | ` *  Nothing.` |
|        - | 10519 | ` */` |
|      218 | 10520 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10521 |  |
|        - | 10522 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 10523 | `	int i;` |
|      220 | 10524 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 10525 | `	/* Dump one or more expressions */` |
|      444 | 10526 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 10527 | `		ph7_value *pObj = apArg[i];` |
|        - | 10528 | `		/* Reset the working buffer */` |
|      226 | 10529 | `		SyBlobReset(&sDump);` |
|        - | 10530 | `		/* Dump the given expression */` |
|      226 | 10531 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 10532 | `		/* Output */` |
|      226 | 10533 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 10534 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 10535 | `		}` |
|      114 | 10536 | `	}` |
|        - | 10537 | `	/* Release the working buffer */` |
|      220 | 10538 | `	SyBlobRelease(&sDump);` |
|      220 | 10539 | `	return SXRET_OK;` |
|        2 | 10540 |  |
|        - | 10541 | `/*` |
|        - | 10542 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 10543 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 10544 | ` * Parameters` |
|        - | 10545 | ` *   expression: Expression to dump` |
|        - | 10546 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 10547 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 10548 | ` *            print_r() will return the information rather than print it.` |
|        - | 10549 | ` * Return` |
|        - | 10550 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 10551 | ` *  Otherwise, the return value is TRUE.` |
|        - | 10552 | ` */` |
|       16 | 10553 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10554 |  |
|       17 | 10555 | `	int ret_string = 0;` |
|        - | 10556 | `	SyBlob sDump;` |
|       17 | 10557 | `	if( nArg < 1 ){` |
|        - | 10558 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 10559 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10560 | `		return SXRET_OK;` |
|        - | 10561 | `	}` |
|       17 | 10562 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 10563 | `	if ( nArg > 1 ){` |
|        - | 10564 | `		/* Where to redirect output */` |
|       11 | 10565 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 10566 | `	}` |
|        - | 10567 | `	/* Generate dump */` |
|       17 | 10568 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 10569 | `	if( !ret_string ){` |
|        - | 10570 | `		/* Output dump */` |
|        7 | 10571 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10572 | `		/* Return true */` |
|        7 | 10573 | `		ph7_result_bool(pCtx,1);` |
|        4 | 10574 | `	}else{` |
|        - | 10575 | `		/* Generated dump as return value */` |
|       11 | 10576 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10577 | `	}` |
|        - | 10578 | `	/* Release the working buffer */` |
|       17 | 10579 | `	SyBlobRelease(&sDump);` |
|       17 | 10580 | `	return SXRET_OK;` |
|        9 | 10581 |  |
|        - | 10582 | `/*` |
|        - | 10583 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 10584 | ` * Same job as print_r. (see coment above)` |
|        - | 10585 | ` */` |
|        2 | 10586 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10587 |  |
|        3 | 10588 | `	int ret_string = 0;` |
|        - | 10589 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 10590 | `	if( nArg < 1 ){` |
|        - | 10591 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 10592 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10593 | `		return SXRET_OK;` |
|        - | 10594 | `	}` |
|        3 | 10595 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 10596 | `	if ( nArg > 1 ){` |
|        - | 10597 | `		/* Where to redirect output */` |
|        3 | 10598 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 10599 | `	}` |
|        - | 10600 | `	/* Generate dump */` |
|        3 | 10601 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 10602 | `	if( !ret_string ){` |
|        - | 10603 | `		/* Output dump */` |
|      ! 0 | 10604 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10605 | `		/* Return NULL */` |
|      ! 0 | 10606 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10607 | `	}else{` |
|        - | 10608 | `		/* Generated dump as return value */` |
|        3 | 10609 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10610 | `	}` |
|        - | 10611 | `	/* Release the working buffer */` |
|        3 | 10612 | `	SyBlobRelease(&sDump);` |
|        3 | 10613 | `	return SXRET_OK;` |
|        2 | 10614 |  |
|        - | 10615 | `/*` |
|        - | 10616 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 10617 | ` *  Set/get the various assert flags.` |
|        - | 10618 | ` * Parameter` |
|        - | 10619 | ` * $what` |
|        - | 10620 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 10621 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 10622 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 10623 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 10624 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 10625 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 10626 | ` * $value` |
|        - | 10627 | ` *   An optional new value for the option.` |
|        - | 10628 | ` * Return` |
|        - | 10629 | ` *  Old setting on success or FALSE on failure.` |
|        - | 10630 | ` */` |
|       28 | 10631 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10632 |  |
|       30 | 10633 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10634 | `	int iOption;` |
|        - | 10635 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 10636 | `	if( nArg < 1 ){` |
|        3 | 10637 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10638 | `			"ArgumentCountError",` |
|        - | 10639 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 10640 | `			);` |
|        - | 10641 | `	}` |
|        - | 10642 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 10643 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 10644 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 10645 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10646 | `			"TypeError",` |
|        - | 10647 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 10648 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 10649 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 10650 | `			);` |
|        - | 10651 | `	}` |
|       28 | 10652 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 10653 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 10654 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 10655 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 10656 | `	switch( iOption ){` |
|        5 | 10657 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 10658 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 10659 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 10660 | `		if( nArg > 1 ){` |
|        5 | 10661 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 10662 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 10663 | `			}else{` |
|        3 | 10664 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 10665 | `			}` |
|        2 | 10666 | `		}` |
|       12 | 10667 | `		break;` |
|        1 | 10668 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 10669 | `		/* Return old callback or null */` |
|        3 | 10670 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 10671 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 10672 | `		}else{` |
|        3 | 10673 | `			ph7_result_null(pCtx);` |
|        - | 10674 | `		}` |
|        3 | 10675 | `		if( nArg > 1 ){` |
|      ! 0 | 10676 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 10677 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 10678 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 10679 | `			}else{` |
|      ! 0 | 10680 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 10681 | `			}` |
|      ! 0 | 10682 | `		}` |
|        3 | 10683 | `		break;` |
|        5 | 10684 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 10685 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 10686 | `		if( nArg > 1 ){` |
|        5 | 10687 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 10688 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 10689 | `			}else{` |
|        3 | 10690 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 10691 | `			}` |
|        2 | 10692 | `		}` |
|       11 | 10693 | `		break;` |
|      ! 0 | 10694 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 10695 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 10696 | `		break;` |
|        1 | 10697 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 10698 | `		ph7_result_int(pCtx, 1);` |
|        3 | 10699 | `		break;` |
|      ! 0 | 10700 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 10701 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 10702 | `		break;` |
|        1 | 10703 | `	default:` |
|        - | 10704 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 10705 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10706 | `			"ValueError",` |
|        - | 10707 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 10708 | `			);` |
|        - | 10709 | `	}` |
|       26 | 10710 | `	return PH7_OK;` |
|       16 | 10711 |  |
|        - | 10712 | `/*` |
|        - | 10713 | ` * bool assert(mixed $assertion)` |
|        - | 10714 | ` *  Checks if assertion is FALSE.` |
|        - | 10715 | ` * Parameter` |
|        - | 10716 | ` *  $assertion` |
|        - | 10717 | ` *    The assertion to test.` |
|        - | 10718 | ` * Return` |
|        - | 10719 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 10720 | ` */` |
|       24 | 10721 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10722 |  |
|       26 | 10723 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10724 | `	int iFlags,iResult;` |
|        - | 10725 | `	const char *zDesc;` |
|        - | 10726 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 10727 | `	if( nArg < 1 ){` |
|        3 | 10728 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10729 | `			"ArgumentCountError",` |
|        - | 10730 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 10731 | `			);` |
|        - | 10732 | `	}` |
|       24 | 10733 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 10734 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 10735 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 10736 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 10737 | `		return PH7_OK;` |
|        - | 10738 | `	}` |
|        - | 10739 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 10740 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 10741 | `	if( !iResult ){` |
|        - | 10742 | `		/* Assertion failed */` |
|        - | 10743 | `		/* Extract optional description */` |
|       13 | 10744 | `		zDesc = 0;` |
|       13 | 10745 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10746 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 10747 | `		}` |
|       13 | 10748 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 10749 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 10750 | `			ph7_value sFile,sLine;` |
|        - | 10751 | `			ph7_value *apCbArg[3];` |
|        - | 10752 | `			SyString *pFile;` |
|        - | 10753 | `			/* Extract the processed script */` |
|      ! 0 | 10754 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 10755 | `			if( pFile == 0 ){` |
|      ! 0 | 10756 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 10757 | `			}` |
|        - | 10758 | `			/* Invoke the callback */` |
|      ! 0 | 10759 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 10760 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 10761 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 10762 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 10763 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 10764 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 10765 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 10766 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 10767 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 10768 | `		}` |
|       13 | 10769 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 10770 | `			/* Abort VM execution immediately */` |
|      ! 0 | 10771 | `			return PH7_ABORT;` |
|        - | 10772 | `		}` |
|        - | 10773 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 10774 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 10775 | `			return PH7_VmThrowException(pCtx,` |
|        - | 10776 | `				"AssertionError",` |
|        - | 10777 | `				"%s",` |
|        1 | 10778 | `				zDesc` |
|        - | 10779 | `				);` |
|      ! 0 | 10780 | `		}else{` |
|       11 | 10781 | `			return PH7_VmThrowException(pCtx,` |
|        - | 10782 | `				"AssertionError",` |
|        - | 10783 | `				"assert(false)"` |
|        - | 10784 | `				);` |
|        - | 10785 | `		}` |
|        - | 10786 | `	}` |
|        - | 10787 | `	/* Assertion passed */` |
|       11 | 10788 | `	ph7_result_bool(pCtx,1);` |
|       11 | 10789 | `	return PH7_OK;` |
|       14 | 10790 |  |
|        - | 10791 | `/*` |
|        - | 10792 | ` * Section:` |
|        - | 10793 | ` *  Error reporting functions.` |
|        - | 10794 | ` * Status:` |
|        - | 10795 | ` *    Stable.` |
|        - | 10796 | ` */` |
|        - | 10797 | `/*` |
|        - | 10798 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 10799 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 10800 | ` * Parameters` |
|        - | 10801 | ` *  $error_msg` |
|        - | 10802 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 10803 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 10804 | ` * $error_type` |
|        - | 10805 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 10806 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 10807 | ` * Return` |
|        - | 10808 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 10809 | ` */` |
|       12 | 10810 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10811 |  |
|       14 | 10812 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 10813 | `	int rc = PH7_OK;` |
|       14 | 10814 | `	if( nArg > 0 ){` |
|        - | 10815 | `		const char *zErr;` |
|        - | 10816 | `		int nLen;` |
|        - | 10817 | `		/* Extract the error message */` |
|       12 | 10818 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 10819 | `		if( nArg > 1 ){` |
|        - | 10820 | `			/* Extract the error type */` |
|       12 | 10821 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 10822 | `			switch( nErr ){` |
|        1 | 10823 | `			case 1:   /* E_ERROR */` |
|        - | 10824 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 10825 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 10826 | `			case 256: /* E_USER_ERROR */` |
|        3 | 10827 | `				nErr = PH7_CTX_ERR;` |
|        3 | 10828 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 10829 | `				break;` |
|        1 | 10830 | `			case 2:   /* E_WARNING */` |
|        - | 10831 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 10832 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 10833 | `			case 512: /* E_USER_WARNING */` |
|        3 | 10834 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 10835 | `				break;` |
|        3 | 10836 | `			default:` |
|        8 | 10837 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 10838 | `				break;` |
|        - | 10839 | `			}` |
|        5 | 10840 | `		}` |
|        - | 10841 | `		/* Report error */` |
|       12 | 10842 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 10843 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 10844 | `			return rc;` |
|        - | 10845 | `		}` |
|        - | 10846 | `		/* Return true */` |
|       12 | 10847 | `		ph7_result_bool(pCtx,1);` |
|        7 | 10848 | `	}else{` |
|        - | 10849 | `		/* Missing arguments,return FALSE */` |
|        3 | 10850 | `		ph7_result_bool(pCtx,0);` |
|        - | 10851 | `	}` |
|       14 | 10852 | `	return rc;` |
|        8 | 10853 |  |
|        - | 10854 | `/*` |
|        - | 10855 | ` * int error_reporting([int $level])` |
|        - | 10856 | ` *  Sets which PHP errors are reported.` |
|        - | 10857 | ` * Parameters` |
|        - | 10858 | ` *  $level` |
|        - | 10859 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 10860 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 10861 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 10862 | ` *   levels will not always behave as expected.` |
|        - | 10863 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 10864 | ` *   in the predefined constants.` |
|        - | 10865 | ` * Return` |
|        - | 10866 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 10867 | ` *   parameter is given.` |
|        - | 10868 | ` */` |
|       38 | 10869 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10870 |  |
|       40 | 10871 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10872 | `	int nOld;` |
|        - | 10873 | `	/* Extract the old reporting level */` |
|       40 | 10874 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 10875 | `	if( nArg > 0 ){` |
|        - | 10876 | `		int nNew;` |
|        - | 10877 | `		/* Extract the desired error reporting level */` |
|       32 | 10878 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 10879 | `		if( !nNew ){` |
|        - | 10880 | `			/* Do not report errors at all */` |
|        5 | 10881 | `			pVm->bErrReport = 0;` |
|        3 | 10882 | `		}else{` |
|        - | 10883 | `			/* Report all errors */` |
|       28 | 10884 | `			pVm->bErrReport = 1;` |
|        - | 10885 | `		}` |
|       15 | 10886 | `	}` |
|        - | 10887 | `	/* Return the old level */` |
|       40 | 10888 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 10889 | `	return PH7_OK;` |
|        2 | 10890 |  |
|        - | 10891 | `/*` |
|        - | 10892 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 10893 | ` *  Send an error message somewhere.` |
|        - | 10894 | ` * Parameter` |
|        - | 10895 | ` *  $message` |
|        - | 10896 | ` *   The error message that should be logged.` |
|        - | 10897 | ` *  $message_type` |
|        - | 10898 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 10899 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 10900 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 10901 | ` *       This is the default option.` |
|        - | 10902 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 10903 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 10904 | ` *    2  No longer an option.` |
|        - | 10905 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 10906 | ` *       to the end of the message string.` |
|        - | 10907 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 10908 | ` *  $destination` |
|        - | 10909 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 10910 | ` *  $extra_headers` |
|        - | 10911 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 10912 | ` * Return` |
|        - | 10913 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10914 | ` * NOTE:` |
|        - | 10915 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 10916 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 10917 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 10918 | ` *  Otherwise this function is no-op.` |
|        - | 10919 | ` */` |
|        4 | 10920 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10921 |  |
|        - | 10922 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 10923 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 10924 | `	int iType = 0;` |
|        5 | 10925 | `	if( nArg < 1 ){` |
|        - | 10926 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 10927 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10928 | `		return PH7_OK;` |
|        - | 10929 | `	}` |
|        5 | 10930 | `	if( pVm->xErrLog  ){` |
|        - | 10931 | `		/* Invoke the user callback */` |
|      ! 0 | 10932 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 10933 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 10934 | `		if( nArg > 1 ){` |
|      ! 0 | 10935 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 10936 | `			if( nArg > 2 ){` |
|      ! 0 | 10937 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 10938 | `				if( nArg > 3 ){` |
|      ! 0 | 10939 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 10940 | `				}` |
|      ! 0 | 10941 | `			}` |
|      ! 0 | 10942 | `		}` |
|      ! 0 | 10943 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 10944 | `	}` |
|        - | 10945 | `	/* Retun TRUE */` |
|        5 | 10946 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10947 | `	return PH7_OK;` |
|        3 | 10948 |  |
|        - | 10949 | `/*` |
|        - | 10950 | ` * bool restore_exception_handler(void)` |
|        - | 10951 | ` *  Restores the previously defined exception handler function.` |
|        - | 10952 | ` * Parameter` |
|        - | 10953 | ` *  None` |
|        - | 10954 | ` * Return` |
|        - | 10955 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10956 | ` */` |
|        4 | 10957 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10958 |  |
|        5 | 10959 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10960 | `	ph7_value *pOld,*pNew;` |
|        - | 10961 | `	/* Point to the old and the new handler */` |
|        5 | 10962 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10963 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10964 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10965 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10966 | `		SXUNUSED(apArg);` |
|        - | 10967 | `		/* No installed handler,return FALSE */` |
|        5 | 10968 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10969 | `		return PH7_OK;` |
|        - | 10970 | `	}` |
|        - | 10971 | `	/* Copy the old handler */` |
|      ! 0 | 10972 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10973 | `	PH7_MemObjRelease(pOld);` |
|        - | 10974 | `	/* Return TRUE */` |
|      ! 0 | 10975 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10976 | `	return PH7_OK;` |
|        3 | 10977 |  |
|        - | 10978 | `/*` |
|        - | 10979 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10980 | ` *  Sets a user-defined exception handler function.` |
|        - | 10981 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10982 | ` * NOTE` |
|        - | 10983 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10984 | ` *  the satndard PHP engine.` |
|        - | 10985 | ` * Parameters` |
|        - | 10986 | ` *  $exception_handler` |
|        - | 10987 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10988 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10989 | ` *   that was thrown.` |
|        - | 10990 | ` *  Note:` |
|        - | 10991 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10992 | ` * Return` |
|        - | 10993 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10994 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10995 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10996 | ` */` |
|        4 | 10997 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10998 |  |
|        6 | 10999 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11000 | `	ph7_value *pOld,*pNew;` |
|        - | 11001 | `	/* Point to the old and the new handler */` |
|        6 | 11002 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 11003 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 11004 | `	/* Return the old handler */` |
|        6 | 11005 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 11006 | `	if( nArg > 0 ){` |
|        6 | 11007 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11008 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 11009 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 11010 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 11011 | `		}else{` |
|        6 | 11012 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11013 | `			/* Install the new handler */` |
|        6 | 11014 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11015 | `		}` |
|        2 | 11016 | `	}` |
|        6 | 11017 | `	return PH7_OK;` |
|        2 | 11018 |  |
|        - | 11019 | `/*` |
|        - | 11020 | ` * bool restore_error_handler(void)` |
|        - | 11021 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11022 | ` * Parameters:` |
|        - | 11023 | ` *  None.` |
|        - | 11024 | ` * Return` |
|        - | 11025 | ` *  Always TRUE.` |
|        - | 11026 | ` */` |
|        4 | 11027 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11028 |  |
|        5 | 11029 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11030 | `	ph7_value *pOld,*pNew;` |
|        - | 11031 | `	/* Point to the old and the new handler */` |
|        5 | 11032 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 11033 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 11034 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 11035 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 11036 | `		SXUNUSED(apArg);` |
|        - | 11037 | `		/* No installed callback,return FALSE */` |
|        5 | 11038 | `		ph7_result_bool(pCtx,0);` |
|        5 | 11039 | `		return PH7_OK;` |
|        - | 11040 | `	}` |
|        - | 11041 | `	/* Copy the old callback */` |
|      ! 0 | 11042 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11043 | `	PH7_MemObjRelease(pOld);` |
|        - | 11044 | `	/* Return TRUE */` |
|      ! 0 | 11045 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11046 | `	return PH7_OK;` |
|        3 | 11047 |  |
|        - | 11048 | `/*` |
|        - | 11049 | ` * value set_error_handler(callable $error_handler)` |
|        - | 11050 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11051 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11052 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11053 | ` *  Sets a user-defined error handler function.` |
|        - | 11054 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 11055 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 11056 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 11057 | ` *  conditions (using trigger_error()).` |
|        - | 11058 | ` * Parameters` |
|        - | 11059 | ` *  $error_handler` |
|        - | 11060 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 11061 | ` *   describing the error.` |
|        - | 11062 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 11063 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 11064 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 11065 | ` *   The function can be shown as:` |
|        - | 11066 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 11067 | ` *     errno` |
|        - | 11068 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 11069 | ` *   errstr` |
|        - | 11070 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 11071 | ` *   errfile` |
|        - | 11072 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 11073 | ` *     was raised in, as a string.` |
|        - | 11074 | ` *  Note:` |
|        - | 11075 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11076 | ` * Return` |
|        - | 11077 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 11078 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11079 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11080 | ` */` |
|     9746 | 11081 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11082 |  |
|     9748 | 11083 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11084 | `	ph7_value *pOld,*pNew;` |
|        - | 11085 | `	/* Point to the old and the new handler */` |
|     9748 | 11086 | `	pOld = &pVm->aErrCB[0];` |
|     9748 | 11087 | `	pNew = &pVm->aErrCB[1];` |
|        - | 11088 | `	/* Return the old handler */` |
|     9748 | 11089 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9748 | 11090 | `	if( nArg > 0 ){` |
|     9748 | 11091 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11092 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4873 | 11093 | `			PH7_MemObjRelease(pNew);` |
|     4873 | 11094 | `			ph7_result_bool(pCtx,1);` |
|     2437 | 11095 | `		}else{` |
|     4876 | 11096 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11097 | `			/* Install the new handler */` |
|     4876 | 11098 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11099 | `		}` |
|     4873 | 11100 | `	}` |
|     9748 | 11101 | `	return PH7_OK;` |
|        2 | 11102 |  |
|        - | 11103 | `/*` |
|        - | 11104 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 11105 | ` *  Generates a backtrace.` |
|        - | 11106 | ` * Paramaeter` |
|        - | 11107 | ` *  $options` |
|        - | 11108 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 11109 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 11110 | ` *   all the function/method arguments, to save memory.` |
|        - | 11111 | ` * $limit` |
|        - | 11112 | ` *   (Not Used)` |
|        - | 11113 | ` * Return` |
|        - | 11114 | ` *  An array.The possible returned elements are as follows:` |
|        - | 11115 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 11116 | ` *          Name        Type      Description` |
|        - | 11117 | ` *          ------      ------     -----------` |
|        - | 11118 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 11119 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 11120 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 11121 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 11122 | ` *          object      object    The current object.` |
|        - | 11123 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 11124 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 11125 | ` */` |
|      614 | 11126 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11127 |  |
|      616 | 11128 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11129 | `	ph7_value *pArray;` |
|        - | 11130 | `	ph7_class *pClass;` |
|        - | 11131 | `	ph7_value *pValue;` |
|        - | 11132 | `	SyString *pFile;` |
|        - | 11133 | `	/* Create a new array */` |
|      616 | 11134 | `	pArray = ph7_context_new_array(pCtx);` |
|      616 | 11135 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      616 | 11136 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11137 | `		/* Out of memory,return NULL */` |
|      ! 0 | 11138 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11139 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11140 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11141 | `		SXUNUSED(apArg);` |
|      ! 0 | 11142 | `		return PH7_OK;` |
|        - | 11143 | `	}` |
|        - | 11144 | `	/* Dump running function name and it's arguments  */` |
|      616 | 11145 | `	if( pVm->pFrame->pParent ){` |
|      616 | 11146 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 11147 | `		ph7_vm_func *pFunc;` |
|        - | 11148 | `		ph7_value *pArg;` |
|      616 | 11149 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      616 | 11150 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      616 | 11151 | `		if( pFrame->pParent && pFunc ){` |
|      616 | 11152 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      616 | 11153 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      616 | 11154 | `			ph7_value_reset_string_cursor(pValue);` |
|      307 | 11155 | `		}` |
|        - | 11156 | `		/* Function arguments */` |
|      616 | 11157 | `		pArg = ph7_context_new_array(pCtx);` |
|      616 | 11158 | `		if( pArg  ){` |
|        - | 11159 | `			ph7_value *pObj;` |
|        - | 11160 | `			VmSlot *aSlot;` |
|        - | 11161 | `			sxu32 n;` |
|        - | 11162 | `			/* Start filling the array with the given arguments */` |
|      616 | 11163 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2450 | 11164 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1836 | 11165 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1836 | 11166 | `				if( pObj ){` |
|     1836 | 11167 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      917 | 11168 | `				}` |
|      919 | 11169 | `			}` |
|        - | 11170 | `			/* Save the array */` |
|      616 | 11171 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      307 | 11172 | `		}` |
|      307 | 11173 | `	}` |
|      616 | 11174 | `	ph7_value_int(pValue,1);` |
|        - | 11175 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 11176 | `	 * line numbers at run-time. )` |
|        - | 11177 | `	 */` |
|      616 | 11178 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 11179 | `	/* Current processed script */` |
|      616 | 11180 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      616 | 11181 | `	if( pFile ){` |
|      616 | 11182 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      616 | 11183 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      616 | 11184 | `		ph7_value_reset_string_cursor(pValue);` |
|      307 | 11185 | `	}` |
|        - | 11186 | `	/* Top class */` |
|      616 | 11187 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      616 | 11188 | `	if( pClass ){` |
|      612 | 11189 | `		ph7_value_reset_string_cursor(pValue);` |
|      612 | 11190 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      612 | 11191 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      305 | 11192 | `	}` |
|        - | 11193 | `	/* Return the freshly created array */` |
|      616 | 11194 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11195 | `	/*` |
|        - | 11196 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 11197 | `	 * as soon we return from this function.` |
|        - | 11198 | `	 */` |
|      616 | 11199 | `	return PH7_OK;` |
|      309 | 11200 |  |
|        - | 11201 | `/*` |
|        - | 11202 | ` * Generate a small backtrace.` |
|        - | 11203 | ` * Store the generated dump in the given BLOB` |
|        - | 11204 | ` */` |
|        4 | 11205 | `static int VmMiniBacktrace(` |
|        - | 11206 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11207 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 11208 | `	)` |
|        1 | 11209 |  |
|        5 | 11210 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11211 | `	ph7_vm_func *pFunc;` |
|        - | 11212 | `	ph7_class *pClass;` |
|        - | 11213 | `	SyString *pFile;` |
|        - | 11214 | `	/* Called function */` |
|        5 | 11215 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 11216 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 11217 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11218 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 11219 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 11220 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 11221 | `	}else{` |
|      ! 0 | 11222 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 11223 | `	}` |
|        5 | 11224 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 11225 | `	/* Current processed script */` |
|        5 | 11226 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 11227 | `	if( pFile ){` |
|        5 | 11228 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11229 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 11230 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 11231 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 11232 | `	}` |
|        - | 11233 | `	/* Top class */` |
|        5 | 11234 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 11235 | `	if( pClass ){` |
|      ! 0 | 11236 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 11237 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 11238 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 11239 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 11240 | `	}` |
|        5 | 11241 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 11242 | `	/* All done */` |
|        5 | 11243 | `	return SXRET_OK;` |
|        1 | 11244 |  |
|        - | 11245 | `/*` |
|        - | 11246 | ` * void debug_print_backtrace()` |
|        - | 11247 | ` *  Prints a backtrace` |
|        - | 11248 | ` * Parameters` |
|        - | 11249 | ` * None` |
|        - | 11250 | ` * Return` |
|        - | 11251 | ` * NULL` |
|        - | 11252 | ` */` |
|        2 | 11253 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11254 |  |
|        3 | 11255 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11256 | `	SyBlob sDump;` |
|        3 | 11257 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11258 | `	/* Generate the backtrace */` |
|        3 | 11259 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11260 | `	/* Output backtrace */` |
|        3 | 11261 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11262 | `	/* All done,cleanup */` |
|        3 | 11263 | `	SyBlobRelease(&sDump);` |
|        1 | 11264 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11265 | `	SXUNUSED(apArg);` |
|        3 | 11266 | `	return PH7_OK;` |
|        1 | 11267 |  |
|        - | 11268 | `/*` |
|        - | 11269 | ` * string debug_string_backtrace()` |
|        - | 11270 | ` *  Generate a backtrace` |
|        - | 11271 | ` * Parameters` |
|        - | 11272 | ` * None` |
|        - | 11273 | ` * Return` |
|        - | 11274 | ` *  A mini backtrace().` |
|        - | 11275 | ` * Note that this is a symisc extension.` |
|        - | 11276 | ` */` |
|        2 | 11277 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11278 |  |
|        3 | 11279 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11280 | `	SyBlob sDump;` |
|        3 | 11281 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11282 | `	/* Generate the backtrace */` |
|        3 | 11283 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11284 | `	/* Return the backtrace */` |
|        3 | 11285 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 11286 | `	/* All done,cleanup */` |
|        3 | 11287 | `	SyBlobRelease(&sDump);` |
|        1 | 11288 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11289 | `	SXUNUSED(apArg);` |
|        3 | 11290 | `	return PH7_OK;` |
|        1 | 11291 |  |
|        - | 11292 | `/*` |
|        - | 11293 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 11294 | ` * exception is triggered.` |
|        - | 11295 | ` */` |
|      480 | 11296 | `static sxi32 VmUncaughtException(` |
|        - | 11297 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11298 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11299 | `	)` |
|        1 | 11300 |  |
|        - | 11301 | `	ph7_value *apArg[2],sArg;` |
|      481 | 11302 | `	int nArg = 1;` |
|        - | 11303 | `	sxi32 rc;` |
|      481 | 11304 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 11305 | `		/* Nesting limit reached */` |
|      ! 0 | 11306 | `		return SXRET_OK;` |
|        - | 11307 | `	}` |
|        - | 11308 | `	/* Call any exception handler if available */` |
|      481 | 11309 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 11310 | `	if( pThis ){` |
|        - | 11311 | `		/* Load the exception instance */` |
|      481 | 11312 | `		sArg.x.pOther = pThis;` |
|      481 | 11313 | `		pThis->iRef++;` |
|      481 | 11314 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 11315 | `	}else{` |
|      ! 0 | 11316 | `		nArg = 0;` |
|        - | 11317 | `	}` |
|      481 | 11318 | `	apArg[0] = &sArg;` |
|        - | 11319 | `	/* Call the exception handler if available */` |
|      481 | 11320 | `	pVm->nExceptDepth++;` |
|      481 | 11321 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 11322 | `	pVm->nExceptDepth--;` |
|      481 | 11323 | `	if( rc != SXRET_OK ){` |
|        - | 11324 | `		SyBlob sMsgBuf;` |
|      479 | 11325 | `		const char *zClass = "Exception";` |
|      479 | 11326 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 11327 | `		const char *zMsg;` |
|        - | 11328 | `		sxu32 nMsg;` |
|        - | 11329 | `		const char *zFuncName;` |
|        - | 11330 | `		int nFuncLen;` |
|      479 | 11331 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 11332 | `		if( pThis ){` |
|        - | 11333 | `			ph7_class_method *pGetMessage;` |
|        - | 11334 | `			ph7_value sMsg;` |
|        - | 11335 | `			const char *zTmp;` |
|        - | 11336 | `			int nTmp;` |
|      479 | 11337 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 11338 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 11339 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 11340 | `			if( pGetMessage ){` |
|      479 | 11341 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 11342 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 11343 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 11344 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 11345 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 11346 | `					}` |
|      239 | 11347 | `				}` |
|      479 | 11348 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 11349 | `			}` |
|      239 | 11350 | `		}` |
|      479 | 11351 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 11352 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 11353 | `		}` |
|      479 | 11354 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 11355 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 11356 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 11357 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 11358 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 11359 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 11360 | `		rc = SXERR_ABORT;` |
|      239 | 11361 | `	}` |
|      481 | 11362 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 11363 | `	return rc;` |
|      241 | 11364 |  |
|        - | 11365 | `/*` |
|        - | 11366 | ` * Throw a user exception.` |
|        - | 11367 | ` *` |
|        - | 11368 | ` * Exception dispatch follows this sequence:` |
|        - | 11369 | ` *` |
|        - | 11370 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 11371 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 11372 | ` *` |
|        - | 11373 | ` * 2. If NO catch matches:` |
|        - | 11374 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 11375 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 11376 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 11377 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 11378 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 11379 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 11380 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 11381 | ` *` |
|        - | 11382 | ` * 3. If a catch DOES match:` |
|        - | 11383 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 11384 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 11385 | ` *       inside the catch body from immediately propagating past our` |
|        - | 11386 | ` *       finally block.` |
|        - | 11387 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 11388 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 11389 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 11390 | ` *       in pPendingException (step 2c).` |
|        - | 11391 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 11392 | ` *    d. Run finally (if present).` |
|        - | 11393 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 11394 | ` *       that handlers are restored and finally has run.` |
|        - | 11395 | ` */` |
|      618 | 11396 | `static sxi32 VmThrowException(` |
|        - | 11397 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 11398 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11399 | `	)` |
|        2 | 11400 |  |
|        - | 11401 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 11402 | `	ph7_exception **apException;` |
|        - | 11403 | `	ph7_exception *pException;` |
|        - | 11404 | `	/* Point to the stack of loaded exceptions */` |
|      620 | 11405 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      620 | 11406 | `	pException = 0;` |
|      620 | 11407 | `	pCatch = 0;` |
|      620 | 11408 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11409 | `		ph7_exception_block *aCatch;` |
|        - | 11410 | `		ph7_class *pClass;` |
|        - | 11411 | `		SyString *aNames;` |
|        - | 11412 | `		sxu32 nNames;` |
|        - | 11413 | `		int matched;` |
|        - | 11414 | `		sxu32 j,k;` |
|        - | 11415 | `		/* Locate the appropriate block to execute */` |
|      134 | 11416 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      134 | 11417 | `		(void)SySetPop(&pVm->aException);` |
|      134 | 11418 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      136 | 11419 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 11420 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      134 | 11421 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      134 | 11422 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      134 | 11423 | `			matched = 0;` |
|      148 | 11424 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 11425 | `				/* Extract the target class */` |
|      146 | 11426 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,TRUE,0);` |
|      146 | 11427 | `				if( pClass == 0 ){` |
|        - | 11428 | `					/* No such class */` |
|      ! 0 | 11429 | `					continue;` |
|        - | 11430 | `				}` |
|      146 | 11431 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      132 | 11432 | `					matched = 1;` |
|      132 | 11433 | `					break;` |
|        - | 11434 | `				}` |
|        8 | 11435 | `			}` |
|      134 | 11436 | `			if( matched ){` |
|        - | 11437 | `				/* Catch block found,break immediately */` |
|      132 | 11438 | `				pCatch = &aCatch[j];` |
|      132 | 11439 | `				break;` |
|        - | 11440 | `			}` |
|        2 | 11441 | `		}` |
|       66 | 11442 | `	}` |
|        - | 11443 | `	/* Execute the cached block if available */` |
|      620 | 11444 | `	if( pCatch == 0 ){` |
|        - | 11445 | `		sxi32 rc;` |
|        - | 11446 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      490 | 11447 | `		if( pException && pException->iHasFinally ){` |
|        3 | 11448 | `			pException->iFinallyDone = 1;` |
|        3 | 11449 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 11450 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11451 | `				return SXERR_ABORT;` |
|        - | 11452 | `			}` |
|        1 | 11453 | `		}` |
|        - | 11454 | `		/* Check if there is an outer exception handler on the stack */` |
|      490 | 11455 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11456 | `			/* Re-throw to the outer handler */` |
|        3 | 11457 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 11458 | `		}` |
|        - | 11459 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 11460 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 11461 | `		 * exception instead of reporting it uncaught.` |
|        - | 11462 | `		 */` |
|      488 | 11463 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 11464 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 11465 | `			 * by looking for a catch frame on the stack.` |
|        - | 11466 | `			 */` |
|      488 | 11467 | `			VmFrame *pF = pVm->pFrame;` |
|      488 | 11468 | `			int inCatch = 0;` |
|      974 | 11469 | `			while( pF ){` |
|      494 | 11470 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        7 | 11471 | `					inCatch = 1;` |
|        7 | 11472 | `					break;` |
|        - | 11473 | `				}` |
|      487 | 11474 | `				pF = pF->pParent;` |
|        1 | 11475 | `			}` |
|      488 | 11476 | `			if( inCatch ){` |
|        - | 11477 | `				/* Defer — will be re-thrown after finally runs */` |
|        7 | 11478 | `				pThis->iRef++;` |
|        7 | 11479 | `				pVm->pPendingException = pThis;` |
|        7 | 11480 | `				return SXRET_OK;` |
|        - | 11481 | `			}` |
|      240 | 11482 | `		}` |
|        - | 11483 | `		/* Truly uncaught */` |
|      481 | 11484 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 11485 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 11486 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 11487 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 11488 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 11489 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 11490 | `			}` |
|      ! 0 | 11491 | `		}` |
|      481 | 11492 | `		return rc;` |
|      ! 0 | 11493 | `	}else{` |
|      132 | 11494 | `		VmFrame *pFrame = pVm->pFrame;` |
|      132 | 11495 | `		ph7_exception **apSaved = 0;` |
|        - | 11496 | `		sxu32 nSavedCount;` |
|        - | 11497 | `		sxi32 rc;` |
|      132 | 11498 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      132 | 11499 | `		if( pException->pFrame == pFrame ){` |
|       88 | 11500 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       43 | 11501 | `		}` |
|        - | 11502 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 11503 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 11504 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 11505 | `		 */` |
|      132 | 11506 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      132 | 11507 | `		if( nSavedCount > 0 ){` |
|       13 | 11508 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 | 11509 | `				nSavedCount * sizeof(ph7_exception *));` |
|        9 | 11510 | `			if( apSaved ){` |
|       13 | 11511 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        4 | 11512 | `					nSavedCount * sizeof(ph7_exception *));` |
|        9 | 11513 | `				SySetReset(&pVm->aException);` |
|        4 | 11514 | `			}` |
|        4 | 11515 | `		}` |
|        - | 11516 | `		/* Create a private frame first */` |
|      132 | 11517 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      132 | 11518 | `		if( rc == SXRET_OK ){` |
|      132 | 11519 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      132 | 11520 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      132 | 11521 | `			if( pObj ){` |
|      132 | 11522 | `				pThis->iRef++;` |
|      132 | 11523 | `				pObj->x.pOther = pThis;` |
|      132 | 11524 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       65 | 11525 | `			}` |
|        - | 11526 | `			/* Execute the catch block */` |
|      132 | 11527 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 11528 | `			/* Leave the frame */` |
|      132 | 11529 | `			VmLeaveFrame(&(*pVm));` |
|       65 | 11530 | `		}` |
|        - | 11531 | `		/* Restore the outer exception handlers */` |
|      132 | 11532 | `		if( apSaved ){` |
|        - | 11533 | `			sxu32 k;` |
|        - | 11534 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 11535 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 11536 | `			 * Restore the original outer entries.` |
|        - | 11537 | `			 */` |
|        9 | 11538 | `			SySetReset(&pVm->aException);` |
|       17 | 11539 | `			for(k = 0; k < nSavedCount; k++){` |
|        9 | 11540 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 11541 | `			}` |
|        9 | 11542 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        4 | 11543 | `		}` |
|        - | 11544 | `		/* Execute the finally block after catch */` |
|      132 | 11545 | `		if( pException->iHasFinally ){` |
|       16 | 11546 | `			pException->iFinallyDone = 1;` |
|        - | 11547 | `			{` |
|       16 | 11548 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 11549 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 11550 | `					return SXERR_ABORT;` |
|        - | 11551 | `				}` |
|        - | 11552 | `			}` |
|        7 | 11553 | `		}` |
|      132 | 11554 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 11555 | `			return SXERR_ABORT;` |
|        - | 11556 | `		}` |
|        - | 11557 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 11558 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 11559 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 11560 | `		 */` |
|      132 | 11561 | `		if( pVm->pPendingException ){` |
|        7 | 11562 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        7 | 11563 | `			pVm->pPendingException = 0;` |
|        7 | 11564 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 11565 | `		}` |
|        - | 11566 | `	}` |
|        - | 11567 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 11568 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 11569 | `	 */` |
|      126 | 11570 | `	return SXRET_OK;` |
|      311 | 11571 |  |
|        - | 11572 | `/*` |
|        - | 11573 | ` * Section:` |
|        - | 11574 | ` *  Version,Credits and Copyright related functions.` |
|        - | 11575 | ` * Status:` |
|        - | 11576 | ` *    Stable.` |
|        - | 11577 | ` */` |
|        - | 11578 | `/*` |
|        - | 11579 | ` * string ph7version(void)` |
|        - | 11580 | ` *  Returns the running version of the PH7 version.` |
|        - | 11581 | ` * Parameters` |
|        - | 11582 | ` *  None` |
|        - | 11583 | ` * Return` |
|        - | 11584 | ` * Current PH7 version.` |
|        - | 11585 | ` */` |
|        2 | 11586 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11587 |  |
|        1 | 11588 | `	SXUNUSED(nArg);` |
|        1 | 11589 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 11590 | `	/* Current engine version */` |
|        3 | 11591 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 11592 | `	return PH7_OK;` |
|        1 | 11593 |  |
|        - | 11594 | `/*` |
|        - | 11595 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 11596 | ` */` |
|        - | 11597 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 11598 | ` "<html><head>"\` |
|        - | 11599 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 11600 | ` "<style type=\"text/css\">"\` |
|        - | 11601 | ` "div {"\` |
|        - | 11602 | `     "border: 1px solid #cccccc;"\` |
|        - | 11603 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 11604 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 11605 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 11606 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 11607 | `     "-webkit-border-radius: 10px;"\` |
|        - | 11608 | `     "-o-border-radius: 10px;"\` |
|        - | 11609 | `     "border-radius: 10px;"\` |
|        - | 11610 | `     "padding-left: 2em;"\` |
|        - | 11611 | `     "background-color: white;"\` |
|        - | 11612 | `     "margin-left: auto;"\` |
|        - | 11613 | `     "font-family: verdana;"\` |
|        - | 11614 | `     "padding-right: 2em;"\` |
|        - | 11615 | `     "margin-right: auto;"\` |
|        - | 11616 | `     "}"\` |
|        - | 11617 | `     "body {"\` |
|        - | 11618 | `     "padding: 0.2em;"\` |
|        - | 11619 | `     "font-style: normal;"\` |
|        - | 11620 | `     "font-size: medium;"\` |
|        - | 11621 | `     "background-color: #f2f2f2;"\` |
|        - | 11622 | `     "}"\` |
|        - | 11623 | `     "hr {"\` |
|        - | 11624 | `     "border-style: solid none none;"\` |
|        - | 11625 | `     "border-width: 1px medium medium;"\` |
|        - | 11626 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 11627 | `     "height: 1px;"\` |
|        - | 11628 | `     "}"\` |
|        - | 11629 | `     "a {"\` |
|        - | 11630 | `     "color: #3366cc;"\` |
|        - | 11631 | `     "text-decoration: none;"\` |
|        - | 11632 | `     "}"\` |
|        - | 11633 | `     "a:hover {"\` |
|        - | 11634 | `     "color: #999999;"\` |
|        - | 11635 | `     "}"\` |
|        - | 11636 | `     "a:active {"\` |
|        - | 11637 | `     "color: #663399;"\` |
|        - | 11638 | `     "}"\` |
|        - | 11639 | `     "h1 {"\` |
|        - | 11640 | `     "margin: 0;"\` |
|        - | 11641 | `     "padding: 0;"\` |
|        - | 11642 | `     "font-family: Verdana;"\` |
|        - | 11643 | `     "font-weight: bold;"\` |
|        - | 11644 | `     "font-style: normal;"\` |
|        - | 11645 | `     "font-size: medium;"\` |
|        - | 11646 | `     "text-transform: capitalize;"\` |
|        - | 11647 | `     "color: #0a328c;"\` |
|        - | 11648 | `     "}"\` |
|        - | 11649 | `     "p {"\` |
|        - | 11650 | `     "margin: 0 auto;"\` |
|        - | 11651 | `     "font-size: medium;"\` |
|        - | 11652 | `     "font-style: normal;"\` |
|        - | 11653 | `     "font-family: verdana;"\` |
|        - | 11654 | `     "}"\` |
|        - | 11655 | `"</style></head><body>"\` |
|        - | 11656 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 11657 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 11658 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 11659 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 11660 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 11661 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 11662 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 11663 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 11664 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 11665 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 11666 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 11667 |  |
|        - | 11668 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11669 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 11670 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 11671 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 11672 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11673 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 11674 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 11675 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 11676 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 11677 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 11678 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11679 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 11680 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 11681 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 11682 |  |
|        - | 11683 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 11684 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 11685 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 11686 | `"&nbsp;*<br>"\` |
|        - | 11687 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 11688 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 11689 | `"&nbsp;* are met:<br>"\` |
|        - | 11690 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 11691 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 11692 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 11693 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 11694 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 11695 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 11696 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 11697 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 11698 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 11699 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 11700 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 11701 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 11702 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 11703 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 11704 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 11705 | `"&nbsp;*<br>"\` |
|        - | 11706 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 11707 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 11708 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 11709 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 11710 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 11711 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 11712 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 11713 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 11714 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 11715 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 11716 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 11717 | `"&nbsp;*/<br>"\` |
|        - | 11718 | `"</span></small></small></p>"\` |
|        - | 11719 | `"</div></body></html>"` |
|        - | 11720 | `/*` |
|        - | 11721 | ` * bool ph7credits(void)` |
|        - | 11722 | ` * bool ph7info(void)` |
|        - | 11723 | ` * bool ph7copyright(void)` |
|        - | 11724 | ` *  Prints out the credits for PH7 engine` |
|        - | 11725 | ` * Parameters` |
|        - | 11726 | ` *  None` |
|        - | 11727 | ` * Return` |
|        - | 11728 | ` *  Always TRUE` |
|        - | 11729 | ` */` |
|        2 | 11730 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11731 |  |
|        3 | 11732 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 11733 | `	/* Expand the HTML page above*/` |
|        3 | 11734 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 11735 | `	ph7_context_output_format(` |
|        1 | 11736 | `		pCtx,` |
|        - | 11737 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 11738 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 11739 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 11740 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 11741 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 11742 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 11743 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 11744 | `#ifdef __WINNT__` |
|        - | 11745 | `		"Windows NT"` |
|        - | 11746 | `#elif defined(__UNIXES__)` |
|        - | 11747 | `		"UNIX-Like"` |
|        - | 11748 | `#else` |
|        - | 11749 | `		"Other OS"` |
|        - | 11750 | `#endif` |
|        - | 11751 | `		);` |
|        3 | 11752 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 11753 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11754 | `	SXUNUSED(apArg);` |
|        - | 11755 | `	/* Return TRUE */` |
|        - | 11756 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 11757 | `	return PH7_OK;` |
|        1 | 11758 |  |
|        - | 11759 | `/*` |
|        - | 11760 | ` * Section:` |
|        - | 11761 | ` *    URL related routines.` |
|        - | 11762 | ` * Status:` |
|        - | 11763 | ` *    Stable.` |
|        - | 11764 | ` */` |
|        - | 11765 | `/*` |
|        - | 11766 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 11767 | ` *  Parse a URL and return its fields.` |
|        - | 11768 | ` * Parameters` |
|        - | 11769 | ` *  $url` |
|        - | 11770 | ` *   The URL to parse.` |
|        - | 11771 | ` * $component` |
|        - | 11772 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 11773 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 11774 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 11775 | ` *  in which case the return value will be an integer).` |
|        - | 11776 | ` * Return` |
|        - | 11777 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 11778 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 11779 | ` *  this array are:` |
|        - | 11780 | ` *   scheme - e.g. http` |
|        - | 11781 | ` *   host` |
|        - | 11782 | ` *   port` |
|        - | 11783 | ` *   user` |
|        - | 11784 | ` *   pass` |
|        - | 11785 | ` *   path` |
|        - | 11786 | ` *   query - after the question mark ?` |
|        - | 11787 | ` *   fragment - after the hashmark #` |
|        - | 11788 | ` * Note:` |
|        - | 11789 | ` *  FALSE is returned on failure.` |
|        - | 11790 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 11791 | ` *  with the standard PHP engine.` |
|        - | 11792 | ` */` |
|       28 | 11793 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11794 |  |
|        - | 11795 | `	const char *zStr; /* Input string */` |
|        - | 11796 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 11797 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 11798 | `	int nLen;` |
|        - | 11799 | `	sxi32 rc;` |
|       29 | 11800 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11801 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11802 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11803 | `		return PH7_OK;` |
|        - | 11804 | `	}` |
|        - | 11805 | `	/* Extract the given URI */` |
|       29 | 11806 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 11807 | `	if( nLen < 1 ){` |
|        - | 11808 | `		/* Nothing to process,return FALSE */` |
|        3 | 11809 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11810 | `		return PH7_OK;` |
|        - | 11811 | `	}` |
|        - | 11812 | `	/* Get a parse */` |
|       27 | 11813 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 11814 | `	if( rc != SXRET_OK ){` |
|        - | 11815 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 11816 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11817 | `		return PH7_OK;` |
|        - | 11818 | `	}` |
|       27 | 11819 | `	if( nArg > 1 ){` |
|      ! 0 | 11820 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 11821 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 11822 | `		switch(nComponent){` |
|      ! 0 | 11823 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 11824 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 11825 | `			if( pComp->nByte < 1 ){` |
|        - | 11826 | `				/* No available value,return NULL */` |
|      ! 0 | 11827 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11828 | `			}else{` |
|      ! 0 | 11829 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11830 | `			}` |
|      ! 0 | 11831 | `			break;` |
|      ! 0 | 11832 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 11833 | `			pComp = &sURI.sHost;` |
|      ! 0 | 11834 | `			if( pComp->nByte < 1 ){` |
|        - | 11835 | `				/* No available value,return NULL */` |
|      ! 0 | 11836 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11837 | `			}else{` |
|      ! 0 | 11838 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11839 | `			}` |
|      ! 0 | 11840 | `			break;` |
|      ! 0 | 11841 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 11842 | `			pComp = &sURI.sPort;` |
|      ! 0 | 11843 | `			if( pComp->nByte < 1 ){` |
|        - | 11844 | `				/* No available value,return NULL */` |
|      ! 0 | 11845 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11846 | `			}else{` |
|      ! 0 | 11847 | `				int iPort = 0;` |
|        - | 11848 | `				/* Cast the value to integer */` |
|      ! 0 | 11849 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 11850 | `				ph7_result_int(pCtx,iPort);` |
|        - | 11851 | `			}` |
|      ! 0 | 11852 | `			break;` |
|      ! 0 | 11853 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 11854 | `			pComp = &sURI.sUser;` |
|      ! 0 | 11855 | `			if( pComp->nByte < 1 ){` |
|        - | 11856 | `				/* No available value,return NULL */` |
|      ! 0 | 11857 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11858 | `			}else{` |
|      ! 0 | 11859 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11860 | `			}` |
|      ! 0 | 11861 | `			break;` |
|      ! 0 | 11862 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 11863 | `			pComp = &sURI.sPass;` |
|      ! 0 | 11864 | `			if( pComp->nByte < 1 ){` |
|        - | 11865 | `				/* No available value,return NULL */` |
|      ! 0 | 11866 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11867 | `			}else{` |
|      ! 0 | 11868 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11869 | `			}` |
|      ! 0 | 11870 | `			break;` |
|      ! 0 | 11871 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 11872 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 11873 | `			if( pComp->nByte < 1 ){` |
|        - | 11874 | `				/* No available value,return NULL */` |
|      ! 0 | 11875 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11876 | `			}else{` |
|      ! 0 | 11877 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11878 | `			}` |
|      ! 0 | 11879 | `			break;` |
|      ! 0 | 11880 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 11881 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 11882 | `			if( pComp->nByte < 1 ){` |
|        - | 11883 | `				/* No available value,return NULL */` |
|      ! 0 | 11884 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11885 | `			}else{` |
|      ! 0 | 11886 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11887 | `			}` |
|      ! 0 | 11888 | `			break;` |
|      ! 0 | 11889 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 11890 | `			pComp = &sURI.sPath;` |
|      ! 0 | 11891 | `			if( pComp->nByte < 1 ){` |
|        - | 11892 | `				/* No available value,return NULL */` |
|      ! 0 | 11893 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11894 | `			}else{` |
|      ! 0 | 11895 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11896 | `			}` |
|      ! 0 | 11897 | `			break;` |
|      ! 0 | 11898 | `		default:` |
|        - | 11899 | `			/* No such entry,return NULL */` |
|      ! 0 | 11900 | `			ph7_result_null(pCtx);` |
|      ! 0 | 11901 | `			break;` |
|        - | 11902 | `		}` |
|      ! 0 | 11903 | `	}else{` |
|        - | 11904 | `		ph7_value *pArray,*pValue;` |
|        - | 11905 | `		/* Return an associative array */` |
|       27 | 11906 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 11907 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 11908 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11909 | `			/* Out of memory */` |
|      ! 0 | 11910 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11911 | `			/* Return false */` |
|      ! 0 | 11912 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 11913 | `			return PH7_OK;` |
|        - | 11914 | `		}` |
|        - | 11915 | `		/* Fill the array */` |
|       27 | 11916 | `		pComp = &sURI.sScheme;` |
|       27 | 11917 | `		if( pComp->nByte > 0 ){` |
|       19 | 11918 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 11919 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 11920 | `		}` |
|        - | 11921 | `		/* Reset the string cursor */` |
|       27 | 11922 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11923 | `		pComp = &sURI.sHost;` |
|       27 | 11924 | `		if( pComp->nByte > 0 ){` |
|       25 | 11925 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 11926 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 11927 | `		}` |
|        - | 11928 | `		/* Reset the string cursor */` |
|       27 | 11929 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11930 | `		pComp = &sURI.sPort;` |
|       27 | 11931 | `		if( pComp->nByte > 0 ){` |
|       11 | 11932 | `			int iPort = 0;/* cc warning */` |
|        - | 11933 | `			/* Convert to integer */` |
|       11 | 11934 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 11935 | `			ph7_value_int(pValue,iPort);` |
|       11 | 11936 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 11937 | `		}` |
|        - | 11938 | `		/* Reset the string cursor */` |
|       27 | 11939 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11940 | `		pComp = &sURI.sUser;` |
|       27 | 11941 | `		if( pComp->nByte > 0 ){` |
|        7 | 11942 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11943 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 11944 | `		}` |
|        - | 11945 | `		/* Reset the string cursor */` |
|       27 | 11946 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11947 | `		pComp = &sURI.sPass;` |
|       27 | 11948 | `		if( pComp->nByte > 0 ){` |
|        7 | 11949 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11950 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 11951 | `		}` |
|        - | 11952 | `		/* Reset the string cursor */` |
|       27 | 11953 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11954 | `		pComp = &sURI.sPath;` |
|       27 | 11955 | `		if( pComp->nByte > 0 ){` |
|       17 | 11956 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 11957 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 11958 | `		}` |
|        - | 11959 | `		/* Reset the string cursor */` |
|       27 | 11960 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11961 | `		pComp = &sURI.sQuery;` |
|       27 | 11962 | `		if( pComp->nByte > 0 ){` |
|        5 | 11963 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11964 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11965 | `		}` |
|        - | 11966 | `		/* Reset the string cursor */` |
|       27 | 11967 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11968 | `		pComp = &sURI.sFragment;` |
|       27 | 11969 | `		if( pComp->nByte > 0 ){` |
|        5 | 11970 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11971 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11972 | `		}` |
|        - | 11973 | `		/* Return the created array */` |
|       27 | 11974 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11975 | `		/* NOTE:` |
|        - | 11976 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11977 | `		 * automatically as soon we return from this function.` |
|        - | 11978 | `		 */` |
|        - | 11979 | `	}` |
|        - | 11980 | `	/* All done */` |
|       27 | 11981 | `	return PH7_OK;` |
|       15 | 11982 |  |
|        - | 11983 | `/*` |
|        - | 11984 | ` * Section:` |
|        - | 11985 | ` *   Array related routines.` |
|        - | 11986 | ` * Status:` |
|        - | 11987 | ` *    Stable.` |
|        - | 11988 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11989 | ` *  Array related functions that need access to the underlying` |
|        - | 11990 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11991 | ` */` |
|        - | 11992 | `/*` |
|        - | 11993 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11994 | ` * of the following structure.` |
|        - | 11995 | ` */` |
|        - | 11996 | `struct compact_data` |
|        - | 11997 |  |
|        - | 11998 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11999 | `	int nRecCount;      /* Recursion count */` |
|        - | 12000 | `};` |
|        - | 12001 | `/*` |
|        - | 12002 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 12003 | ` */` |
|      ! 0 | 12004 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 12005 |  |
|      ! 0 | 12006 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 12007 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 12008 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12009 | `	/* Act according to the hashmap value */` |
|      ! 0 | 12010 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 12011 | `		SyString sVar;` |
|      ! 0 | 12012 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 12013 | `		if( sVar.nByte > 0 ){` |
|        - | 12014 | `			/* Query the current frame */` |
|      ! 0 | 12015 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 12016 | `			/* ^` |
|        - | 12017 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 12018 | `			 */` |
|      ! 0 | 12019 | `			if( pKey ){` |
|        - | 12020 | `				/* Perform the insertion */` |
|      ! 0 | 12021 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 12022 | `			}` |
|      ! 0 | 12023 | `		}` |
|      ! 0 | 12024 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 12025 | `		int rc;` |
|        - | 12026 | `		/* Recursively traverse this array */` |
|      ! 0 | 12027 | `		pData->nRecCount++;` |
|      ! 0 | 12028 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 12029 | `		pData->nRecCount--;` |
|      ! 0 | 12030 | `		return rc;` |
|        - | 12031 | `	}` |
|      ! 0 | 12032 | `	return SXRET_OK;` |
|      ! 0 | 12033 |  |
|        - | 12034 | `/*` |
|        - | 12035 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 12036 | ` *  Create array containing variables and their values.` |
|        - | 12037 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 12038 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 12039 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 12040 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 12041 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 12042 | ` * Parameters` |
|        - | 12043 | ` *  $varname` |
|        - | 12044 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 12045 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 12046 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 12047 | ` *   it recursively.` |
|        - | 12048 | ` * Return` |
|        - | 12049 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 12050 | ` */` |
|        2 | 12051 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12052 |  |
|        - | 12053 | `	ph7_value *pArray,*pObj;` |
|        3 | 12054 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12055 | `	const char *zName;` |
|        - | 12056 | `	SyString sVar;` |
|        - | 12057 | `	int i,nLen;` |
|        3 | 12058 | `	if( nArg < 1 ){` |
|        - | 12059 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 12060 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12061 | `		return PH7_OK;` |
|        - | 12062 | `	}` |
|        - | 12063 | `	/* Create the array */` |
|        3 | 12064 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12065 | `	if( pArray == 0 ){` |
|        - | 12066 | `		/* Out of memory */` |
|      ! 0 | 12067 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12068 | `		/* Return NULL */` |
|      ! 0 | 12069 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12070 | `		return PH7_OK;` |
|        - | 12071 | `	}` |
|        - | 12072 | `	/* Perform the requested operation */` |
|        7 | 12073 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 12074 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 12075 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 12076 | `				struct compact_data sData;` |
|      ! 0 | 12077 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 12078 | `				/* Recursively walk the array */` |
|      ! 0 | 12079 | `				sData.nRecCount = 0;` |
|      ! 0 | 12080 | `				sData.pArray = pArray;` |
|      ! 0 | 12081 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 12082 | `			}` |
|      ! 0 | 12083 | `		}else{` |
|        - | 12084 | `			/* Extract variable name */` |
|        5 | 12085 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 12086 | `			if( nLen > 0 ){` |
|        5 | 12087 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 12088 | `				/* Check if the variable is available in the current frame */` |
|        5 | 12089 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 12090 | `				if( pObj ){` |
|        5 | 12091 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 12092 | `				}` |
|        2 | 12093 | `			}` |
|        - | 12094 | `		}` |
|        3 | 12095 | `	}` |
|        - | 12096 | `	/* Return the array */` |
|        3 | 12097 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12098 | `	return PH7_OK;` |
|        2 | 12099 |  |
|        - | 12100 | `/*` |
|        - | 12101 | ` * The [extract()] function store it's state information in an instance` |
|        - | 12102 | ` * of the following structure.` |
|        - | 12103 | ` */` |
|        - | 12104 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 12105 | `struct extract_aux_data` |
|        - | 12106 |  |
|        - | 12107 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 12108 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 12109 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 12110 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 12111 | `	int iFlags;           /* Control flags */` |
|        - | 12112 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 12113 | `};` |
|        - | 12114 | `/* Forward declaration */` |
|        - | 12115 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 12116 | `/*` |
|        - | 12117 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 12118 | ` *   Import variables into the current symbol table from an array.` |
|        - | 12119 | ` * Parameters` |
|        - | 12120 | ` * $var_array` |
|        - | 12121 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 12122 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 12123 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 12124 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 12125 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 12126 | ` * $extract_type` |
|        - | 12127 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 12128 | ` *  It can be one of the following values:` |
|        - | 12129 | ` *   EXTR_OVERWRITE` |
|        - | 12130 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 12131 | ` *   EXTR_SKIP` |
|        - | 12132 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 12133 | ` *   EXTR_PREFIX_SAME` |
|        - | 12134 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 12135 | ` *   EXTR_PREFIX_ALL` |
|        - | 12136 | ` *       Prefix all variable names with prefix.` |
|        - | 12137 | ` *   EXTR_PREFIX_INVALID` |
|        - | 12138 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 12139 | ` *   EXTR_IF_EXISTS` |
|        - | 12140 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 12141 | ` *       otherwise do nothing.` |
|        - | 12142 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 12143 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 12144 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 12145 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 12146 | ` *      the current symbol table.` |
|        - | 12147 | ` * $prefix` |
|        - | 12148 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 12149 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 12150 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 12151 | ` *  underscore character.` |
|        - | 12152 | ` * Return` |
|        - | 12153 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 12154 | ` */` |
|        4 | 12155 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12156 |  |
|        - | 12157 | `	extract_aux_data sAux;` |
|        - | 12158 | `	ph7_hashmap *pMap;` |
|        5 | 12159 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 12160 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 12161 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12162 | `		return PH7_OK;` |
|        - | 12163 | `	}` |
|        - | 12164 | `	/* Point to the target hashmap */` |
|        5 | 12165 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 12166 | `	if( pMap->nEntry < 1 ){` |
|        - | 12167 | `		/* Empty map,return  0 */` |
|      ! 0 | 12168 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12169 | `		return PH7_OK;` |
|        - | 12170 | `	}` |
|        - | 12171 | `	/* Prepare the aux data */` |
|        5 | 12172 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 12173 | `	if( nArg > 1 ){` |
|        3 | 12174 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 12175 | `		if( nArg > 2 ){` |
|      ! 0 | 12176 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 12177 | `		}` |
|        1 | 12178 | `	}` |
|        5 | 12179 | `	sAux.pVm = pCtx->pVm;` |
|        - | 12180 | `	/* Invoke the worker callback */` |
|        5 | 12181 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 12182 | `	/* Number of variables successfully imported */` |
|        5 | 12183 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 12184 | `	return PH7_OK;` |
|        3 | 12185 |  |
|        - | 12186 | `/*` |
|        - | 12187 | ` * Worker callback for the [extract()] function defined` |
|        - | 12188 | ` * below.` |
|        - | 12189 | ` */` |
|        8 | 12190 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12191 |  |
|        9 | 12192 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 12193 | `	int iFlags = pAux->iFlags;` |
|        9 | 12194 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12195 | `	ph7_value *pObj;` |
|        - | 12196 | `	SyString sVar;` |
|        9 | 12197 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 12198 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 12199 | `	}` |
|        - | 12200 | `	/* Perform a string cast */` |
|        9 | 12201 | `	PH7_MemObjToString(pKey);` |
|        9 | 12202 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12203 | `		/* Unavailable variable name */` |
|      ! 0 | 12204 | `		return SXRET_OK;` |
|        - | 12205 | `	}` |
|        9 | 12206 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 12207 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 12208 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12209 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12210 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12211 | `			);` |
|      ! 0 | 12212 | `	}else{` |
|       13 | 12213 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 12214 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12215 | `	}` |
|        9 | 12216 | `	sVar.zString = pAux->zWorker;` |
|        - | 12217 | `	/* Try to extract the variable */` |
|        9 | 12218 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 12219 | `	if( pObj ){` |
|        - | 12220 | `		/* Collision */` |
|        5 | 12221 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 12222 | `			return SXRET_OK;` |
|        - | 12223 | `		}` |
|        5 | 12224 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 12225 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 12226 | `				/* Already prefixed */` |
|      ! 0 | 12227 | `				return SXRET_OK;` |
|        - | 12228 | `			}` |
|      ! 0 | 12229 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12230 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12231 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12232 | `				);` |
|      ! 0 | 12233 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 12234 | `		}` |
|        3 | 12235 | `	}else{` |
|        - | 12236 | `		/* Create the variable */` |
|        5 | 12237 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 12238 | `	}` |
|        9 | 12239 | `	if( pObj ){` |
|        - | 12240 | `		/* Overwrite the old value */` |
|        9 | 12241 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 12242 | `		/* Increment counter */` |
|        9 | 12243 | `		pAux->iCount++;` |
|        4 | 12244 | `	}` |
|        9 | 12245 | `	return SXRET_OK;` |
|        5 | 12246 |  |
|        - | 12247 | `/*` |
|        - | 12248 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 12249 | ` * defined below.` |
|        - | 12250 | ` */` |
|        2 | 12251 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12252 |  |
|        3 | 12253 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 12254 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12255 | `	ph7_value *pObj;` |
|        - | 12256 | `	SyString sVar;` |
|        - | 12257 | `	/* Perform a string cast */` |
|        3 | 12258 | `	PH7_MemObjToString(pKey);` |
|        3 | 12259 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12260 | `		/* Unavailable variable name */` |
|      ! 0 | 12261 | `		return SXRET_OK;` |
|        - | 12262 | `	}` |
|        3 | 12263 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 12264 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 12265 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 12266 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 12267 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12268 | `			);` |
|        2 | 12269 | `	}else{` |
|      ! 0 | 12270 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 12271 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12272 | `	}` |
|        3 | 12273 | `	sVar.zString = pAux->zWorker;` |
|        - | 12274 | `	/* Extract the variable */` |
|        3 | 12275 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 12276 | `	if( pObj ){` |
|        3 | 12277 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 12278 | `	}` |
|        3 | 12279 | `	return SXRET_OK;` |
|        2 | 12280 |  |
|        - | 12281 | `/*` |
|        - | 12282 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 12283 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 12284 | ` * Parameters` |
|        - | 12285 | ` * $types` |
|        - | 12286 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 12287 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 12288 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 12289 | ` *  POST includes the POST uploaded file information.` |
|        - | 12290 | ` *  Note:` |
|        - | 12291 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 12292 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 12293 | ` * $prefix` |
|        - | 12294 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 12295 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 12296 | ` *  variable named $pref_userid.` |
|        - | 12297 | ` * Return` |
|        - | 12298 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12299 | ` */` |
|        2 | 12300 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12301 |  |
|        - | 12302 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 12303 | `	extract_aux_data sAux;` |
|        - | 12304 | `	int nLen,nPrefixLen;` |
|        - | 12305 | `	ph7_value *pSuper;` |
|        - | 12306 | `	ph7_vm *pVm;` |
|        - | 12307 | `	/* By default import only $_GET variables  */` |
|        3 | 12308 | `	zImport = "G";` |
|        3 | 12309 | `	nLen = (int)sizeof(char);` |
|        3 | 12310 | `	zPrefix = 0;` |
|        3 | 12311 | `	nPrefixLen = 0;` |
|        3 | 12312 | `	if( nArg > 0 ){` |
|        3 | 12313 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 12314 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 12315 | `		}` |
|        3 | 12316 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12317 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 12318 | `		}` |
|        1 | 12319 | `	}` |
|        - | 12320 | `	/* Point to the underlying VM */` |
|        3 | 12321 | `	pVm = pCtx->pVm;` |
|        - | 12322 | `	/* Initialize the aux data */` |
|        3 | 12323 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 12324 | `	sAux.zPrefix = zPrefix;` |
|        3 | 12325 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 12326 | `	sAux.pVm = pVm;` |
|        - | 12327 | `	/* Extract */` |
|        3 | 12328 | `	zEnd = &zImport[nLen];` |
|        5 | 12329 | `	while( zImport < zEnd ){` |
|        3 | 12330 | `		int c = zImport[0];` |
|        3 | 12331 | `		pSuper = 0;` |
|        3 | 12332 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 12333 | `			/* Import $_GET variables */` |
|        3 | 12334 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 12335 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 12336 | `			/* Import $_POST variables */` |
|      ! 0 | 12337 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 12338 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 12339 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 12340 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 12341 | `		}` |
|        3 | 12342 | `		if( pSuper ){` |
|        - | 12343 | `			/* Iterate throw array entries */` |
|        3 | 12344 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 12345 | `		}` |
|        - | 12346 | `		/* Advance the cursor */` |
|        3 | 12347 | `		zImport++;` |
|        1 | 12348 | `	}` |
|        - | 12349 | `	/* All done,return TRUE*/` |
|        3 | 12350 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12351 | `	return PH7_OK;` |
|        1 | 12352 |  |
|        - | 12353 | `/*` |
|        - | 12354 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 12355 | ` * Refer to the eval() language construct implementation for more` |
|        - | 12356 | ` * information.` |
|        - | 12357 | ` */` |
|    11374 | 12358 | `static sxi32 VmEvalChunk(` |
|        - | 12359 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 12360 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 12361 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 12362 | `	int iFlags,         /* Compile flag */` |
|        - | 12363 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 12364 | `	)` |
|        2 | 12365 |  |
|        - | 12366 | `	SySet *pByteCode,aByteCode;` |
|        - | 12367 | `	SyBlob sSavedNs;` |
|    11376 | 12368 | `	ProcConsumer xErr = 0;` |
|    11376 | 12369 | `	void *pErrData = 0;` |
|        - | 12370 | `	/* Initialize bytecode container */` |
|    11376 | 12371 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    11376 | 12372 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 12373 | `	/* Reset the code generator */` |
|    11376 | 12374 | `	if( bTrueReturn ){` |
|        - | 12375 | `		/* Included file,log compile-time errors */` |
|     8554 | 12376 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8554 | 12377 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4276 | 12378 | `	}` |
|    11376 | 12379 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 12380 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 12381 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 12382 | `	 * the caller's namespace is restored. */` |
|    11376 | 12383 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    11376 | 12384 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    11376 | 12385 | `	if( bTrueReturn ){` |
|        - | 12386 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8554 | 12387 | `		SyBlobReset(&pVm->sNamespace);` |
|     4276 | 12388 | `	}` |
|        - | 12389 | `	/* Swap bytecode container */` |
|    11376 | 12390 | `	pByteCode = pVm->pByteContainer;` |
|    11376 | 12391 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 12392 | `	/* Compile the chunk */` |
|    11376 | 12393 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    17063 | 12394 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 12395 | `		/* Compilation error,return false */` |
|        3 | 12396 | `		if( pCtx ){` |
|        3 | 12397 | `			ph7_result_bool(pCtx,0);` |
|        1 | 12398 | `		}` |
|        2 | 12399 | `	}else{` |
|        - | 12400 | `		/* Mount any newly defined classes */` |
|        - | 12401 | `		SyHashEntry *pEntry;` |
|        - | 12402 | `		ph7_class *pClass;` |
|        - | 12403 | `		ph7_value sResult; /* Return value */` |
|        - | 12404 | `		sxi32 rc;` |
|    11374 | 12405 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   463118 | 12406 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   446060 | 12407 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 12408 | `			/* Only mount classes that haven't been mounted yet */` |
|   446060 | 12409 | `			if( !pClass->bMounted ){` |
|    89552 | 12410 | `				rc = VmMountUserClass(pVm,pClass);` |
|    89552 | 12411 | `				if( rc != SXRET_OK ){` |
|        - | 12412 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 12413 | `					if( pCtx ){` |
|      ! 0 | 12414 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 12415 | `					}` |
|      ! 0 | 12416 | `					goto Cleanup;` |
|        - | 12417 | `				}` |
|    44775 | 12418 | `			}` |
|        2 | 12419 | `		}` |
|    11374 | 12420 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 12421 | `			/* Out of memory */` |
|      ! 0 | 12422 | `			if( pCtx ){` |
|      ! 0 | 12423 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 12424 | `			}` |
|      ! 0 | 12425 | `			goto Cleanup;` |
|        - | 12426 | `		}` |
|    11374 | 12427 | `		if( bTrueReturn ){` |
|        - | 12428 | `			/* Assume a boolean true return value */` |
|     8554 | 12429 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4278 | 12430 | `		}else{` |
|        - | 12431 | `			/* Assume a null return value */` |
|     2822 | 12432 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 12433 | `		}` |
|        - | 12434 | `		/* Execute the compiled chunk */` |
|    11374 | 12435 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    11374 | 12436 | `		if( pCtx ){` |
|        - | 12437 | `			/* Set the execution result */` |
|     8572 | 12438 | `			ph7_result_value(pCtx,&sResult);` |
|     4285 | 12439 | `		}` |
|    11374 | 12440 | `		PH7_MemObjRelease(&sResult);` |
|        - | 12441 | `	}` |
|     5687 | 12442 | `Cleanup:` |
|        - | 12443 | `	/* Cleanup the mess left behind */` |
|    11376 | 12444 | `	pVm->pByteContainer = pByteCode;` |
|    11376 | 12445 | `	SySetRelease(&aByteCode);` |
|        - | 12446 | `	/* Restore caller's namespace state */` |
|    11376 | 12447 | `	SyBlobReset(&pVm->sNamespace);` |
|    11376 | 12448 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    11376 | 12449 | `	SyBlobRelease(&sSavedNs);` |
|    11376 | 12450 | `	return SXRET_OK;` |
|        2 | 12451 |  |
|        - | 12452 | `/*` |
|        - | 12453 | ` * value eval(string $code)` |
|        - | 12454 | ` *   Evaluate a string as PHP code.` |
|        - | 12455 | ` * Parameter` |
|        - | 12456 | ` *  code: PHP code to evaluate.` |
|        - | 12457 | ` * Return` |
|        - | 12458 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 12459 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 12460 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 12461 | ` */` |
|       22 | 12462 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12463 |  |
|        - | 12464 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 12465 | `	if( nArg < 1 ){` |
|        - | 12466 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12467 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12468 | `		return SXRET_OK;` |
|        - | 12469 | `	}` |
|        - | 12470 | `	/* Chunk to evaluate */` |
|       24 | 12471 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 12472 | `	if( sChunk.nByte < 1 ){` |
|        - | 12473 | `		/* Empty string,return NULL */` |
|        3 | 12474 | `		ph7_result_null(pCtx);` |
|        3 | 12475 | `		return SXRET_OK;` |
|        - | 12476 | `	}` |
|        - | 12477 | `	/* Eval the chunk */` |
|       22 | 12478 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 12479 | `	return SXRET_OK;` |
|       13 | 12480 |  |
|        - | 12481 | `/*` |
|        - | 12482 | ` * Check if a file path is already included.` |
|        - | 12483 | ` */` |
|    17100 | 12484 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 12485 |  |
|        - | 12486 | `	SyString *aEntries;` |
|        - | 12487 | `	sxu32 n;` |
|    17102 | 12488 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 12489 | `	/* Perform a linear search */` |
| 73054002 | 12490 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 73036908 | 12491 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 12492 | `			/* Already included */` |
|        7 | 12493 | `			return TRUE;` |
|        - | 12494 | `		}` |
| 36518452 | 12495 | `	}` |
|    17096 | 12496 | `	return FALSE;` |
|     8552 | 12497 |  |
|        - | 12498 | `/*` |
|        - | 12499 | ` * Push a file path in the appropriate VM container.` |
|        - | 12500 | ` */` |
|    19894 | 12501 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 12502 |  |
|        - | 12503 | `	SyString sPath;` |
|        - | 12504 | `	char *zDup;` |
|        - | 12505 | `#ifdef __WINNT__` |
|        - | 12506 | `	char *zCur;` |
|        - | 12507 | `#endif` |
|        - | 12508 | `	sxi32 rc;` |
|    19896 | 12509 | `	if( nLen < 0 ){` |
|     2796 | 12510 | `		nLen = SyStrlen(zPath);` |
|     1397 | 12511 | `	}` |
|        - | 12512 | `	/* Duplicate the file path first */` |
|    19896 | 12513 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    19896 | 12514 | `	if( zDup == 0 ){` |
|      ! 0 | 12515 | `		return SXERR_MEM;` |
|        - | 12516 | `	}` |
|        - | 12517 | `#ifdef __WINNT__` |
|        - | 12518 | `	/* Normalize path on windows` |
|        - | 12519 | `	 * Example:` |
|        - | 12520 | `	 *    Path/To/File.php` |
|        - | 12521 | `	 * becomes` |
|        - | 12522 | `	 *   path\to\file.php` |
|        - | 12523 | `	 */` |
|        2 | 12524 | `	zCur = zDup;` |
|        2 | 12525 | `	while( zCur[0] != 0 ){` |
|        2 | 12526 | `		if( zCur[0] == '/' ){` |
|        2 | 12527 | `			zCur[0] = '\\';` |
|        2 | 12528 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 12529 | `			int c = SyToLower(zCur[0]);` |
|        1 | 12530 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 12531 | `		}` |
|        2 | 12532 | `		zCur++;` |
|        2 | 12533 | `	}` |
|        - | 12534 | `#endif` |
|        - | 12535 | `	/* Install the file path */` |
|    19896 | 12536 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    19896 | 12537 | `	if( !bMain ){` |
|    17102 | 12538 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 12539 | `			/* Already included */` |
|        7 | 12540 | `			*pNew = 0;` |
|        4 | 12541 | `		}else{` |
|        - | 12542 | `			/* Insert in the corresponding container */` |
|    17096 | 12543 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    17096 | 12544 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 12545 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 12546 | `				return rc;` |
|        - | 12547 | `			}` |
|    17096 | 12548 | `			*pNew = 1;` |
|        - | 12549 | `		}` |
|     8550 | 12550 | `	}` |
|    19896 | 12551 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    19896 | 12552 | `	return SXRET_OK;` |
|     9949 | 12553 |  |
|        - | 12554 | `/*` |
|        - | 12555 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 12556 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 12557 | ` * indicates failure.` |
|        - | 12558 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 12559 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 12560 | ` * operations.` |
|        - | 12561 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 12562 | ` * this function is a no-op.` |
|        - | 12563 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 12564 | ` * constructs for more information.` |
|        - | 12565 | ` */` |
|     8562 | 12566 | `static sxi32 VmExecIncludedFile(` |
|        - | 12567 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 12568 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 12569 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 12570 | `	 )` |
|        2 | 12571 |  |
|        - | 12572 | `	sxi32 rc;` |
|        - | 12573 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12574 | `	const ph7_io_stream *pStream;` |
|        - | 12575 | `	SyBlob sContents;` |
|        - | 12576 | `	void *pHandle;` |
|        - | 12577 | `	ph7_vm *pVm;` |
|        - | 12578 | `	int isNew;` |
|        - | 12579 | `	/* Initialize fields */` |
|     8564 | 12580 | `	pVm = pCtx->pVm;` |
|     8564 | 12581 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8564 | 12582 | `	isNew = 0;` |
|        - | 12583 | `	/* Extract the associated stream */` |
|     8564 | 12584 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 12585 | `	/*` |
|        - | 12586 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 12587 | `	 * in a read-only mode.` |
|        - | 12588 | `	 */` |
|     8564 | 12589 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8564 | 12590 | `	if( pHandle == 0 ){` |
|        8 | 12591 | `		return SXERR_IO;` |
|        - | 12592 | `	}` |
|     8558 | 12593 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8558 | 12594 | `	if( IncludeOnce && !isNew ){` |
|        - | 12595 | `		/* Already included */` |
|        5 | 12596 | `		rc = SXERR_EXISTS;` |
|        3 | 12597 | `	}else{` |
|        - | 12598 | `		/* Read the whole file contents */` |
|     8554 | 12599 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8554 | 12600 | `		if( rc == SXRET_OK ){` |
|        - | 12601 | `			SyString sScript;` |
|        - | 12602 | `			/* Compile and execute the script */` |
|     8554 | 12603 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8554 | 12604 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4276 | 12605 | `		}` |
|        - | 12606 | `	}` |
|        - | 12607 | `	/* Pop from the set of included file */` |
|     8558 | 12608 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 12609 | `	/* Close the handle */` |
|     8558 | 12610 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 12611 | `	/* Release the working buffer */` |
|     8558 | 12612 | `	SyBlobRelease(&sContents);` |
|        - | 12613 | `#else` |
|        - | 12614 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 12615 | `	SXUNUSED(pPath);` |
|        - | 12616 | `	SXUNUSED(IncludeOnce);` |
|        - | 12617 | `	rc = SXERR_IO;` |
|        - | 12618 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8558 | 12619 | `	return rc;` |
|     4283 | 12620 |  |
|        - | 12621 | `/*` |
|        - | 12622 | ` * string get_include_path(void)` |
|        - | 12623 | ` *  Gets the current include_path configuration option.` |
|        - | 12624 | ` * Parameter` |
|        - | 12625 | ` *  None` |
|        - | 12626 | ` * Return` |
|        - | 12627 | ` *  Included paths as a string` |
|        - | 12628 | ` */` |
|        2 | 12629 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12630 |  |
|        3 | 12631 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12632 | `	SyString *aEntry;` |
|        - | 12633 | `	int dir_sep;` |
|        - | 12634 | `	sxu32 n;` |
|        - | 12635 | `#ifdef __WINNT__` |
|        1 | 12636 | `	dir_sep = ';';` |
|        - | 12637 | `#else` |
|        - | 12638 | `	/* Assume UNIX path separator */` |
|        2 | 12639 | `	dir_sep = ':';` |
|        - | 12640 | `#endif` |
|        1 | 12641 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12642 | `	SXUNUSED(apArg);` |
|        - | 12643 | `	/* Point to the list of import paths */` |
|        3 | 12644 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 12645 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 12646 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 12647 | `		if( n > 0 ){` |
|        - | 12648 | `			/* Append dir seprator */` |
|      ! 0 | 12649 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 12650 | `		}` |
|        - | 12651 | `		/* Append path */` |
|        3 | 12652 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 12653 | `	}` |
|        3 | 12654 | `	return PH7_OK;` |
|        1 | 12655 |  |
|        - | 12656 | `/*` |
|        - | 12657 | ` * string get_get_included_files(void)` |
|        - | 12658 | ` *  Gets the current include_path configuration option.` |
|        - | 12659 | ` * Parameter` |
|        - | 12660 | ` *  None` |
|        - | 12661 | ` * Return` |
|        - | 12662 | ` *  Included paths as a string` |
|        - | 12663 | ` */` |
|        2 | 12664 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12665 |  |
|        3 | 12666 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 12667 | `	ph7_value *pArray,*pWorker;` |
|        - | 12668 | `	SyString *pEntry;` |
|        - | 12669 | `	int c,d;` |
|        - | 12670 | `	/* Create an array and a working value */` |
|        3 | 12671 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 12672 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 12673 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 12674 | `		/* Out of memory,return null */` |
|      ! 0 | 12675 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12676 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12677 | `		SXUNUSED(apArg);` |
|      ! 0 | 12678 | `		return PH7_OK;` |
|        - | 12679 | `	}` |
|        3 | 12680 | `	c = d = '/';` |
|        - | 12681 | `#ifdef __WINNT__` |
|        1 | 12682 | `	d = '\\';` |
|        - | 12683 | `#endif` |
|        - | 12684 | `	/* Iterate throw entries */` |
|        3 | 12685 | `	SySetResetCursor(pFiles);` |
|     3839 | 12686 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 12687 | `		const char *zBase,*zEnd;` |
|        - | 12688 | `		int iLen;` |
|        - | 12689 | `		/* reset the string cursor */` |
|     3837 | 12690 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 12691 | `		/* Extract base name */` |
|     3837 | 12692 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 12693 | `		/* Ignore trailing '/' */` |
|     5755 | 12694 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 12695 | `			zEnd--;` |
|      ! 0 | 12696 | `		}` |
|     3837 | 12697 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 12698 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 12699 | `			zEnd--;` |
|        1 | 12700 | `		}` |
|     3837 | 12701 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 12702 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 12703 | `		/* Copy entry name */` |
|     3837 | 12704 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 12705 | `		/* Perform the insertion */` |
|     3837 | 12706 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 12707 | `	}` |
|        - | 12708 | `	/* All done,return the created array */` |
|        3 | 12709 | `	ph7_result_value(pCtx,pArray);` |
|        - | 12710 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 12711 | `	 * by the engine as soon we return from this foreign` |
|        - | 12712 | `	 * function.` |
|        - | 12713 | `	 */` |
|        3 | 12714 | `	return PH7_OK;` |
|        2 | 12715 |  |
|        - | 12716 | `/*` |
|        - | 12717 | ` * include:` |
|        - | 12718 | ` * According to the PHP reference manual.` |
|        - | 12719 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 12720 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 12721 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 12722 | ` *  include() will finally check in the calling script's own directory` |
|        - | 12723 | ` *  and the current working directory before failing. The include()` |
|        - | 12724 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 12725 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 12726 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 12727 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 12728 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 12729 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 12730 | ` *  directory to find the requested file.` |
|        - | 12731 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 12732 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 12733 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 12734 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 12735 | ` */` |
|     8544 | 12736 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12737 |  |
|        - | 12738 | `	SyString sFile;` |
|        - | 12739 | `	sxi32 rc;` |
|     8546 | 12740 | `	if( nArg < 1 ){` |
|        - | 12741 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12742 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12743 | `		return SXRET_OK;` |
|        - | 12744 | `	}` |
|        - | 12745 | `	/* File to include */` |
|     8546 | 12746 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8546 | 12747 | `	if( sFile.nByte < 1 ){` |
|        - | 12748 | `		/* Empty string,return NULL */` |
|      ! 0 | 12749 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12750 | `		return SXRET_OK;` |
|        - | 12751 | `	}` |
|        - | 12752 | `	/* Open,compile and execute the desired script */` |
|     8546 | 12753 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8546 | 12754 | `	if( rc != SXRET_OK ){` |
|        - | 12755 | `		/* Emit a warning and return false */` |
|        3 | 12756 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 12757 | `		ph7_result_bool(pCtx,0);` |
|        1 | 12758 | `	}` |
|     8546 | 12759 | `	return SXRET_OK;` |
|     4274 | 12760 |  |
|        - | 12761 | `/*` |
|        - | 12762 | ` * include_once:` |
|        - | 12763 | ` *  According to the PHP reference manual.` |
|        - | 12764 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 12765 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 12766 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 12767 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 12768 | ` *   just once.` |
|        - | 12769 | ` */` |
|        4 | 12770 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12771 |  |
|        - | 12772 | `	SyString sFile;` |
|        - | 12773 | `	sxi32 rc;` |
|        5 | 12774 | `	if( nArg < 1 ){` |
|        - | 12775 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12776 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12777 | `		return SXRET_OK;` |
|        - | 12778 | `	}` |
|        - | 12779 | `	/* File to include */` |
|        5 | 12780 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12781 | `	if( sFile.nByte < 1 ){` |
|        - | 12782 | `		/* Empty string,return NULL */` |
|      ! 0 | 12783 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12784 | `		return SXRET_OK;` |
|        - | 12785 | `	}` |
|        - | 12786 | `	/* Open,compile and execute the desired script */` |
|        5 | 12787 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12788 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12789 | `		/* File already included,return TRUE */` |
|        3 | 12790 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12791 | `		return SXRET_OK;` |
|        - | 12792 | `	}` |
|        3 | 12793 | `	if( rc != SXRET_OK ){` |
|        - | 12794 | `		/* Emit a warning and return false */` |
|      ! 0 | 12795 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12796 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12797 | ` 	}` |
|        3 | 12798 | `	return SXRET_OK;` |
|        3 | 12799 |  |
|        - | 12800 | `/*` |
|        - | 12801 | ` * require.` |
|        - | 12802 | ` *  According to the PHP reference manual.` |
|        - | 12803 | ` *   require() is identical to include() except upon failure it will` |
|        - | 12804 | ` *   also produce a fatal level error.` |
|        - | 12805 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 12806 | ` *   emits a warning  which allows the script to continue.` |
|        - | 12807 | ` */` |
|        6 | 12808 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12809 |  |
|        - | 12810 | `	SyString sFile;` |
|        - | 12811 | `	sxi32 rc;` |
|        8 | 12812 | `	if( nArg < 1 ){` |
|        - | 12813 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12814 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12815 | `		return SXRET_OK;` |
|        - | 12816 | `	}` |
|        - | 12817 | `	/* File to include */` |
|        8 | 12818 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 12819 | `	if( sFile.nByte < 1 ){` |
|        - | 12820 | `		/* Empty string,return NULL */` |
|      ! 0 | 12821 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12822 | `		return SXRET_OK;` |
|        - | 12823 | `	}` |
|        - | 12824 | `	/* Open,compile and execute the desired script */` |
|        8 | 12825 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 12826 | `	if( rc != SXRET_OK ){` |
|        - | 12827 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12828 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12829 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12830 | `		return PH7_ABORT;` |
|        - | 12831 | `	}` |
|        8 | 12832 | `	return SXRET_OK;` |
|        5 | 12833 |  |
|        - | 12834 | `/*` |
|        - | 12835 | ` * require_once:` |
|        - | 12836 | ` *  According to the PHP reference manual.` |
|        - | 12837 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 12838 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 12839 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 12840 | ` *   and how it differs from its non _once siblings.` |
|        - | 12841 | ` */` |
|        4 | 12842 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12843 |  |
|        - | 12844 | `	SyString sFile;` |
|        - | 12845 | `	sxi32 rc;` |
|        5 | 12846 | `	if( nArg < 1 ){` |
|        - | 12847 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12848 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12849 | `		return SXRET_OK;` |
|        - | 12850 | `	}` |
|        - | 12851 | `	/* File to include */` |
|        5 | 12852 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12853 | `	if( sFile.nByte < 1 ){` |
|        - | 12854 | `		/* Empty string,return NULL */` |
|      ! 0 | 12855 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12856 | `		return SXRET_OK;` |
|        - | 12857 | `	}` |
|        - | 12858 | `	/* Open,compile and execute the desired script */` |
|        5 | 12859 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12860 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12861 | `		/* File already included,return TRUE */` |
|        3 | 12862 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12863 | `		return SXRET_OK;` |
|        - | 12864 | `	}` |
|        3 | 12865 | `	if( rc != SXRET_OK ){` |
|        - | 12866 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12867 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12868 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12869 | `		return PH7_ABORT;` |
|        - | 12870 | `	}` |
|        3 | 12871 | `	return SXRET_OK;` |
|        3 | 12872 |  |
|        - | 12873 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 12874 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 12875 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 12876 | `/*` |
|        - | 12877 | ` * Section:` |
|        - | 12878 | ` *  SPL Autoloading functions.` |
|        - | 12879 | ` * Status:` |
|        - | 12880 | ` *  Stable.` |
|        - | 12881 | ` */` |
|        - | 12882 | `/*` |
|        - | 12883 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 12884 | ` *  Register given function as __autoload() implementation.` |
|        - | 12885 | ` * Parameters` |
|        - | 12886 | ` *  callback` |
|        - | 12887 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 12888 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 12889 | ` *  throw` |
|        - | 12890 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 12891 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 12892 | ` *  prepend` |
|        - | 12893 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 12894 | ` *   autoload stack instead of appending it.` |
|        - | 12895 | ` * Return` |
|        - | 12896 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12897 | ` */` |
|       34 | 12898 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12899 |  |
|        - | 12900 | `	VmAutoloadCB sEntry;` |
|       36 | 12901 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 12902 | `	int iPrepend = 0;` |
|        - | 12903 | `	sxu32 n;` |
|       36 | 12904 | `	if( nArg < 1 ){` |
|        - | 12905 | `		/* No callback provided — register default spl_autoload.` |
|        - | 12906 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 12907 | `		/* Check for duplicates first */` |
|        9 | 12908 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 12909 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 12910 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 12911 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 12912 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 12913 | `				ph7_result_bool(pCtx,1);` |
|        5 | 12914 | `				return SXRET_OK;` |
|        - | 12915 | `			}` |
|      ! 0 | 12916 | `		}` |
|        5 | 12917 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 12918 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 12919 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 12920 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 12921 | `		ph7_result_bool(pCtx,1);` |
|        5 | 12922 | `		return SXRET_OK;` |
|        - | 12923 | `	}` |
|        - | 12924 | `	/* Validate that the callback is callable */` |
|       28 | 12925 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 12926 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 12927 | `		if( nArg >= 2 ){` |
|      ! 0 | 12928 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12929 | `		}` |
|      ! 0 | 12930 | `		if( iThrow ){` |
|      ! 0 | 12931 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 12932 | `				"Argument is not callable");` |
|      ! 0 | 12933 | `		}` |
|      ! 0 | 12934 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12935 | `		return SXRET_OK;` |
|        - | 12936 | `	}` |
|        - | 12937 | `	/* Check for duplicates */` |
|       46 | 12938 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 12939 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 12940 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12941 | `			/* Already registered */` |
|      ! 0 | 12942 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12943 | `			return SXRET_OK;` |
|        - | 12944 | `		}` |
|       11 | 12945 | `	}` |
|        - | 12946 | `	/* Check prepend flag */` |
|       28 | 12947 | `	if( nArg >= 3 ){` |
|        3 | 12948 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 12949 | `	}` |
|        - | 12950 | `	/* Store the callback */` |
|       28 | 12951 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 12952 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 12953 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 12954 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 12955 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 12956 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 12957 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 12958 | `		VmAutoloadCB *aBase;` |
|        3 | 12959 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12960 | `		/* Rotate: move last entry to front */` |
|        3 | 12961 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12962 | `		if( aBase ){` |
|        - | 12963 | `			VmAutoloadCB sTemp;` |
|        - | 12964 | `			sxu32 i;` |
|        3 | 12965 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12966 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12967 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12968 | `			}` |
|        3 | 12969 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12970 | `		}` |
|        2 | 12971 | `	}else{` |
|       26 | 12972 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12973 | `	}` |
|       28 | 12974 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12975 | `	return SXRET_OK;` |
|       19 | 12976 |  |
|        - | 12977 | `/*` |
|        - | 12978 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12979 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12980 | ` * Parameters` |
|        - | 12981 | ` *  callback` |
|        - | 12982 | ` *   The autoload function being unregistered.` |
|        - | 12983 | ` * Return` |
|        - | 12984 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12985 | ` */` |
|       32 | 12986 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12987 |  |
|       34 | 12988 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12989 | `	sxu32 n,nEntry;` |
|       34 | 12990 | `	if( nArg < 1 ){` |
|      ! 0 | 12991 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12992 | `		return SXRET_OK;` |
|        - | 12993 | `	}` |
|       34 | 12994 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12995 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12996 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12997 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12998 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12999 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 13000 | `			sxu32 i;` |
|       32 | 13001 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 13002 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 13003 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 13004 | `			}` |
|        - | 13005 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 13006 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 13007 | `			ph7_result_bool(pCtx,1);` |
|       32 | 13008 | `			return SXRET_OK;` |
|        - | 13009 | `		}` |
|        3 | 13010 | `	}` |
|        3 | 13011 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13012 | `	return SXRET_OK;` |
|       18 | 13013 |  |
|        - | 13014 | `/*` |
|        - | 13015 | ` * array spl_autoload_functions(void)` |
|        - | 13016 | ` *  Return all registered __autoload() functions.` |
|        - | 13017 | ` * Return` |
|        - | 13018 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 13019 | ` *  an empty array is returned.` |
|        - | 13020 | ` */` |
|       20 | 13021 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13022 |  |
|       21 | 13023 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13024 | `	ph7_value *pArray;` |
|        - | 13025 | `	sxu32 n,nEntry;` |
|       10 | 13026 | `	SXUNUSED(nArg);` |
|       10 | 13027 | `	SXUNUSED(apArg);` |
|       21 | 13028 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 13029 | `	if( pArray == 0 ){` |
|      ! 0 | 13030 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13031 | `		return SXRET_OK;` |
|        - | 13032 | `	}` |
|       21 | 13033 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 13034 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 13035 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 13036 | `		if( pEntry ){` |
|       15 | 13037 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 13038 | `		}` |
|        8 | 13039 | `	}` |
|       21 | 13040 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 13041 | `	return SXRET_OK;` |
|       11 | 13042 |  |
|        - | 13043 | `/*` |
|        - | 13044 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 13045 | ` *  Default implementation of __autoload().` |
|        - | 13046 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 13047 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 13048 | ` * Parameters` |
|        - | 13049 | ` *  class` |
|        - | 13050 | ` *   The class name being searched.` |
|        - | 13051 | ` *  file_extensions` |
|        - | 13052 | ` *   Comma-separated list of file extensions to try.` |
|        - | 13053 | ` */` |
|        2 | 13054 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13055 |  |
|        - | 13056 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 13057 | `	SyBlob sPath;` |
|        - | 13058 | `	int nClass;` |
|        - | 13059 | `	sxi32 rc;` |
|        3 | 13060 | `	if( nArg < 1 ){` |
|      ! 0 | 13061 | `		return SXRET_OK;` |
|        - | 13062 | `	}` |
|        3 | 13063 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 13064 | `	if( nClass < 1 ){` |
|      ! 0 | 13065 | `		return SXRET_OK;` |
|        - | 13066 | `	}` |
|        - | 13067 | `	/* Default extensions */` |
|        3 | 13068 | `	zExt = ".php,.inc";` |
|        3 | 13069 | `	if( nArg >= 2 ){` |
|        - | 13070 | `		int nExt;` |
|      ! 0 | 13071 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 13072 | `		if( nExt < 1 ){` |
|      ! 0 | 13073 | `			zExt = ".php,.inc";` |
|      ! 0 | 13074 | `		}` |
|      ! 0 | 13075 | `	}` |
|        3 | 13076 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 13077 | `	/* Iterate over comma-separated extensions */` |
|        3 | 13078 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 13079 | `	zCur = zExt;` |
|        7 | 13080 | `	while( zCur < zEnd ){` |
|        - | 13081 | `		const char *zComma;` |
|        - | 13082 | `		SyString sFile;` |
|        - | 13083 | `		int i;` |
|        - | 13084 | `		/* Find next comma or end */` |
|        5 | 13085 | `		zComma = zCur;` |
|       21 | 13086 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 13087 | `			zComma++;` |
|        1 | 13088 | `		}` |
|        - | 13089 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 13090 | `		SyBlobReset(&sPath);` |
|       69 | 13091 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 13092 | `			char c = zClass[i];` |
|       65 | 13093 | `			if( c == '\\' ){` |
|      ! 0 | 13094 | `				c = '/';` |
|       65 | 13095 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 13096 | `				c = c + ('a' - 'A');` |
|        6 | 13097 | `			}` |
|       65 | 13098 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 13099 | `		}` |
|        - | 13100 | `		/* Append extension */` |
|        5 | 13101 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 13102 | `		/* Try to include the file */` |
|        5 | 13103 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 13104 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 13105 | `		if( rc == SXRET_OK ){` |
|        - | 13106 | `			/* File included successfully */` |
|      ! 0 | 13107 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 13108 | `			return SXRET_OK;` |
|        - | 13109 | `		}` |
|        - | 13110 | `		/* Move past the comma */` |
|        5 | 13111 | `		zCur = zComma;` |
|        5 | 13112 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 13113 | `			zCur++;` |
|        1 | 13114 | `		}` |
|        1 | 13115 | `	}` |
|        3 | 13116 | `	SyBlobRelease(&sPath);` |
|        3 | 13117 | `	return SXRET_OK;` |
|        2 | 13118 |  |
|        - | 13119 | `/* Table of built-in VM functions. */` |
|        - | 13120 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13121 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13122 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13123 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13124 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13125 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13126 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13127 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13128 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13129 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13130 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13131 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13132 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13133 | `	    /* Constants management */` |
|        - | 13134 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13135 | `	{ "define",   vm_builtin_define               },` |
|        - | 13136 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13137 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13138 | `	   /* Class/Object functions */` |
|        - | 13139 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13140 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13141 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13142 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13143 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13144 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13145 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13146 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13147 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13148 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13149 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13150 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13151 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13152 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13153 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13154 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13155 | `	   /* SPL Autoloading */` |
|        - | 13156 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 13157 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 13158 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 13159 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 13160 | `	   /* Random numbers/strings generators */` |
|        - | 13161 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13162 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13163 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13164 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13165 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13166 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13167 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13168 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13169 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13170 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13171 | `	   /* Language constructs functions */` |
|        - | 13172 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13173 | `	{ "print", vm_builtin_print                   },` |
|        - | 13174 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13175 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13176 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13177 | `	  /* Variable handling functions */` |
|        - | 13178 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13179 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13180 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13181 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13182 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13183 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13184 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13185 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13186 | `	  /* Ouput control functions */` |
|        - | 13187 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13188 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13189 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13190 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13191 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13192 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13193 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13194 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13195 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13196 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13197 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13198 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13199 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13200 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13201 | `	  /* Assertion functions */` |
|        - | 13202 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13203 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13204 | `	  /* Error reporting functions */` |
|        - | 13205 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13206 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13207 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13208 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13209 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13210 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13211 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13212 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13213 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13214 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13215 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13216 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13217 | `	  /* Release info */` |
|        - | 13218 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13219 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13220 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13221 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13222 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13223 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13224 | `	  /* hashmap */` |
|        - | 13225 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13226 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13227 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13228 | `	  /* URL related function */` |
|        - | 13229 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13230 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13231 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13232 | `	   /* XML processing functions */` |
|        - | 13233 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13234 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13235 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 13236 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 13237 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 13238 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 13239 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13240 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13241 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13242 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13243 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13244 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 13245 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 13246 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 13247 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 13248 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 13249 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 13250 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 13251 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 13252 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 13253 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 13254 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13255 | `	   /* UTF-8 encoding/decoding */` |
|        - | 13256 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 13257 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 13258 | `	   /* Command line processing */` |
|        - | 13259 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 13260 | `	   /* JSON encoding/decoding */` |
|        - | 13261 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 13262 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 13263 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 13264 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 13265 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 13266 | `	   /* Files/URI inclusion facility */` |
|        - | 13267 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 13268 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 13269 | `	{ "include",      vm_builtin_include          },` |
|        - | 13270 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 13271 | `	{ "require",      vm_builtin_require          },` |
|        - | 13272 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 13273 | `};` |
|        - | 13274 | `/*` |
|        - | 13275 | ` * Register the built-in VM functions defined above.` |
|        - | 13276 | ` */` |
|     2522 | 13277 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 13278 |  |
|        - | 13279 | `	sxi32 rc;` |
|        - | 13280 | `	sxu32 n;` |
|   325340 | 13281 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 13282 | `		/* Note that these special functions have access` |
|        - | 13283 | `		 * to the underlying virtual machine as their` |
|        - | 13284 | `		 * private data.` |
|        - | 13285 | `		 */` |
|   322818 | 13286 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   322818 | 13287 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13288 | `			return rc;` |
|        - | 13289 | `		}` |
|   161410 | 13290 | `	}` |
|     2524 | 13291 | `	return SXRET_OK;` |
|     1263 | 13292 |  |
|        - | 13293 | `/*` |
|        - | 13294 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 13295 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 13296 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 13297 | ` */` |
|    35740 | 13298 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 13299 |  |
|    35742 | 13300 | `	if( !iLoadable ){` |
|    34098 | 13301 | `		return pClass;` |
|        - | 13302 | `	}` |
|     1646 | 13303 | `	while(pClass){` |
|     1646 | 13304 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1646 | 13305 | `			return pClass;` |
|        - | 13306 | `		}` |
|      ! 0 | 13307 | `		pClass = pClass->pNextName;` |
|      ! 0 | 13308 | `	}` |
|      ! 0 | 13309 | `	return 0;` |
|    17872 | 13310 |  |
|        - | 13311 | `/*` |
|        - | 13312 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 13313 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 13314 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 13315 | ` * registered in the VM's class table.` |
|        - | 13316 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 13317 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 13318 | ` */` |
|       36 | 13319 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13320 |  |
|        - | 13321 | `	VmAutoloadCB *pEntry;` |
|        - | 13322 | `	ph7_value sArg,sResult;` |
|        - | 13323 | `	SyHashEntry *pHashEntry;` |
|        - | 13324 | `	ph7_class *pClass;` |
|        - | 13325 | `	sxu32 n,nEntry;` |
|       38 | 13326 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13327 | `	if( nEntry < 1 ){` |
|       24 | 13328 | `		return 0;` |
|        - | 13329 | `	}` |
|        - | 13330 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 13331 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 13332 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 13333 | `	}` |
|        - | 13334 | `	/* Mark this class as being autoloaded */` |
|       14 | 13335 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 13336 | `	/* Prepare the class name argument */` |
|       14 | 13337 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 13338 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 13339 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 13340 | `	pClass = 0;` |
|       28 | 13341 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 13342 | `		ph7_value *apArg[1];` |
|       24 | 13343 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 13344 | `		if( pEntry == 0 ){` |
|      ! 0 | 13345 | `			continue;` |
|        - | 13346 | `		}` |
|       24 | 13347 | `		apArg[0] = &sArg;` |
|       24 | 13348 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 13349 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 13350 | `			continue;` |
|        - | 13351 | `		}` |
|        - | 13352 | `		/* Check if the class is now available */` |
|       24 | 13353 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 13354 | `		if( pHashEntry ){` |
|       10 | 13355 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 13356 | `			if( pClass ){` |
|       10 | 13357 | `				break;` |
|        - | 13358 | `			}` |
|      ! 0 | 13359 | `		}` |
|        9 | 13360 | `	}` |
|       14 | 13361 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 13362 | `	PH7_MemObjRelease(&sResult);` |
|        - | 13363 | `	/* Remove reentrancy guard */` |
|       14 | 13364 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 13365 | `	return pClass;` |
|       20 | 13366 |  |
|        - | 13367 | `/*` |
|        - | 13368 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 13369 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 13370 | ` */` |
|       18 | 13371 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13372 |  |
|       20 | 13373 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 13374 |  |
|        - | 13375 | `/*` |
|        - | 13376 | ` * Check if the given name refer to an installed class.` |
|        - | 13377 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 13378 | ` */` |
|    35750 | 13379 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 13380 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 13381 | `	const char *zName,  /* Name of the target class */` |
|        - | 13382 | `	sxu32 nByte,        /* zName length */` |
|        - | 13383 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 13384 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 13385 | `						 */` |
|        - | 13386 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 13387 | `	)` |
|        2 | 13388 |  |
|        - | 13389 | `	SyHashEntry *pEntry;` |
|        - | 13390 | `	ph7_class *pClass;` |
|    17875 | 13391 | `	SXUNUSED(iNest);` |
|        - | 13392 | `	/* Exact class lookup.` |
|        - | 13393 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 13394 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    35752 | 13395 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    35752 | 13396 | `	if( pEntry == 0 ){` |
|        - | 13397 | `		/* Class not found in hash table — try autoload before giving up */` |
|       20 | 13398 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 13399 | `	}` |
|    35734 | 13400 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    35734 | 13401 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    17877 | 13402 |  |
|        - | 13403 | `/*` |
|        - | 13404 | ` * Reference Table Implementation` |
|        - | 13405 | ` * Status: stable <chm@symisc.net>` |
|        - | 13406 | ` * Intro` |
|        - | 13407 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 13408 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 13409 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 13410 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 13411 | ` *  Refer to the official for more information on this powerful` |
|        - | 13412 | ` *  extension.` |
|        - | 13413 | ` */` |
|        - | 13414 | `/*` |
|        - | 13415 | ` * Allocate a new reference entry.` |
|        - | 13416 | ` */` |
|  3095934 | 13417 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 13418 |  |
|        - | 13419 | `	VmRefObj *pRef;` |
|        - | 13420 | `	/* Allocate a new instance */` |
|  3095936 | 13421 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3095936 | 13422 | `	if( pRef == 0 ){` |
|      ! 0 | 13423 | `		return 0;` |
|        - | 13424 | `	}` |
|        - | 13425 | `	/* Zero the structure */` |
|  3095936 | 13426 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 13427 | `	/* Initialize fields */` |
|  3095936 | 13428 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3095936 | 13429 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3095936 | 13430 | `	pRef->nIdx = nIdx;` |
|  3095936 | 13431 | `	return pRef;` |
|  1547969 | 13432 |  |
|        - | 13433 | `/*` |
|        - | 13434 | ` * Default hash function used by the reference table` |
|        - | 13435 | ` * for lookup/insertion operations.` |
|        - | 13436 | ` */` |
| 17064354 | 13437 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 13438 |  |
|        - | 13439 | `	/* Calculate the hash based on the memory object index */` |
| 17064356 | 13440 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 13441 |  |
|        - | 13442 | `/*` |
|        - | 13443 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 13444 | ` * in the reference table.` |
|        - | 13445 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 13446 | ` * otherwise.` |
|        - | 13447 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13448 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13449 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13450 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13451 | ` * Refer to the official for more information on this powerful` |
|        - | 13452 | ` * extension.` |
|        - | 13453 | ` */` |
|  9236750 | 13454 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 13455 |  |
|        - | 13456 | `	VmRefObj *pRef;` |
|        - | 13457 | `	sxu32 nBucket;` |
|        - | 13458 | `	/* Point to the appropriate bucket */` |
|  9236752 | 13459 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 13460 | `	/* Perform the lookup */` |
|  9236752 | 13461 | `	pRef = pVm->apRefObj[nBucket];` |
| 20089381 | 13462 | `	for(;;){` |
| 40164481 | 13463 | `		if( pRef == 0 ){` |
|  3182512 | 13464 | `			break;` |
|        - | 13465 | `		}` |
| 36981971 | 13466 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 13467 | `			/* Entry found */` |
|  6054242 | 13468 | `			return pRef;` |
|        - | 13469 | `		}` |
|        - | 13470 | `		/* Point to the next entry */` |
| 30927731 | 13471 | `		pRef = pRef->pNextCollide;` |
|        2 | 13472 | `	}` |
|        - | 13473 | `	/* No such entry,return NULL */` |
|  3182512 | 13474 | `	return 0;` |
|  4618377 | 13475 |  |
|        - | 13476 | `/*` |
|        - | 13477 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13478 | ` *` |
|        - | 13479 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13480 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13481 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13482 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13483 | ` * Refer to the official for more information on this powerful` |
|        - | 13484 | ` * extension.` |
|        - | 13485 | ` */` |
|  3095934 | 13486 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13487 |  |
|        - | 13488 | `	sxu32 nBucket;` |
|  3095936 | 13489 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 13490 | `		VmRefObj **apNew;` |
|        - | 13491 | `		sxu32 nNew;` |
|        - | 13492 | `		/* Allocate a larger table */` |
|     4318 | 13493 | `		nNew = pVm->nRefSize << 1;` |
|     4318 | 13494 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4318 | 13495 | `		if( apNew ){` |
|     4318 | 13496 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 13497 | `			sxu32 n;` |
|        - | 13498 | `			/* Zero the structure */` |
|     4318 | 13499 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 13500 | `			/* Rehash all referenced entries */` |
|  2844290 | 13501 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 13502 | `				/* Remove old collision links */` |
|  2839974 | 13503 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 13504 | `				/* Point to the appropriate bucket */` |
|  2839974 | 13505 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 13506 | `				/* Insert the entry  */` |
|  2839974 | 13507 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2839974 | 13508 | `				if( apNew[nBucket] ){` |
|  2298896 | 13509 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 13510 | `				}` |
|  2839974 | 13511 | `				apNew[nBucket] = pEntry;` |
|        - | 13512 | `				/* Point to the next entry */` |
|  2839974 | 13513 | `				pEntry = pEntry->pNext;` |
|  1419988 | 13514 | `			}` |
|        - | 13515 | `			/* Release the old table */` |
|     4318 | 13516 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 13517 | `			/* Install the new one */` |
|     4318 | 13518 | `			pVm->apRefObj = apNew;` |
|     4318 | 13519 | `			pVm->nRefSize = nNew;` |
|     2158 | 13520 | `		}` |
|     2158 | 13521 | `	}` |
|        - | 13522 | `	/* Point to the appropriate bucket */` |
|  3095936 | 13523 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 13524 | `	/* Insert the entry */` |
|  3095936 | 13525 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3095936 | 13526 | `	if( pVm->apRefObj[nBucket] ){` |
|  2548602 | 13527 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1274298 | 13528 | `	}` |
|  3095936 | 13529 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3095936 | 13530 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3095936 | 13531 | `	pVm->nRefUsed++;` |
|  3095936 | 13532 | `	return SXRET_OK;` |
|        2 | 13533 |  |
|        - | 13534 | `/*` |
|        - | 13535 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 13536 | ` * the reference table.` |
|        - | 13537 | ` * This function is invoked when the user perform an unset` |
|        - | 13538 | ` * call [i.e: unset($var); ].` |
|        - | 13539 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13540 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13541 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13542 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13543 | ` * Refer to the official for more information on this powerful` |
|        - | 13544 | ` * extension.` |
|        - | 13545 | ` */` |
|  3059618 | 13546 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 13547 |  |
|        - | 13548 | `	ph7_hashmap_node **apNode;` |
|        - | 13549 | `	SyHashEntry **apEntry;` |
|        - | 13550 | `	sxu32 n;` |
|        - | 13551 | `	/* Point to the reference table */` |
|  3059620 | 13552 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3059620 | 13553 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 13554 | `	/* Unlink the entry from the reference table */` |
|  3152500 | 13555 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    92882 | 13556 | `		if( apEntry[n] ){` |
|    92832 | 13557 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    46415 | 13558 | `		}` |
|    46442 | 13559 | `	}` |
|  6028752 | 13560 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2969134 | 13561 | `		if( apNode[n] ){` |
|     7232 | 13562 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3615 | 13563 | `		}` |
|  1484568 | 13564 | `	}` |
|  3059620 | 13565 | `	if( pRef->pPrevCollide ){` |
|  1167922 | 13566 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   584296 | 13567 | `	}else{` |
|  1891700 | 13568 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 13569 | `	}` |
|  3059620 | 13570 | `	if( pRef->pNextCollide ){` |
|  1736590 | 13571 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   868266 | 13572 | `	}` |
|  3059620 | 13573 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 13574 | `	/* Release the node */` |
|  3059620 | 13575 | `	SySetRelease(&pRef->aReference);` |
|  3059620 | 13576 | `	SySetRelease(&pRef->aArrEntries);` |
|  3059620 | 13577 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3059620 | 13578 | `	pVm->nRefUsed--;` |
|  3059620 | 13579 | `	return SXRET_OK;` |
|        2 | 13580 |  |
|        - | 13581 | `/*` |
|        - | 13582 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13583 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13584 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13585 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13586 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13587 | ` * Refer to the official for more information on this powerful` |
|        - | 13588 | ` * extension.` |
|        - | 13589 | ` */` |
|  3128594 | 13590 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 13591 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 13592 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 13593 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 13594 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 13595 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 13596 | `	)` |
|        2 | 13597 |  |
|  3128596 | 13598 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13599 | `	VmRefObj *pRef;` |
|        - | 13600 | `	/* Check if the referenced object already exists */` |
|  3128596 | 13601 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3128596 | 13602 | `	if( pRef == 0 ){` |
|        - | 13603 | `		/* Create a new entry */` |
|  3095936 | 13604 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3095936 | 13605 | `		if( pRef == 0 ){` |
|      ! 0 | 13606 | `			return SXERR_MEM;` |
|        - | 13607 | `		}` |
|  3095936 | 13608 | `		pRef->iFlags = iFlags;` |
|        - | 13609 | `		/* Install the entry */` |
|  3095936 | 13610 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1547967 | 13611 | `	}` |
|  3128596 | 13612 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3128596 | 13613 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 13614 | `		VmSlot sRef;` |
|        - | 13615 | `		/* Local frame,record referenced entry so that it can` |
|        - | 13616 | `		 * be deleted when we leave this frame.` |
|        - | 13617 | `		 */` |
|    86656 | 13618 | `		sRef.nIdx = nIdx;` |
|    86656 | 13619 | `		sRef.pUserData = pEntry;` |
|    86656 | 13620 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 13621 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 13622 | `		}` |
|    43327 | 13623 | `	}` |
|  3128596 | 13624 | `	if( pEntry ){` |
|        - | 13625 | `		/* Address of the hash-entry */` |
|   119122 | 13626 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    59560 | 13627 | `	}` |
|  3128596 | 13628 | `	if( pMapEntry ){` |
|        - | 13629 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3003520 | 13630 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1501759 | 13631 | `	}` |
|  3128596 | 13632 | `	return SXRET_OK;` |
|  1564299 | 13633 |  |
|        - | 13634 | `/*` |
|        - | 13635 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 13636 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13637 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13638 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13639 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13640 | ` * Refer to the official for more information on this powerful` |
|        - | 13641 | ` * extension.` |
|        - | 13642 | ` */` |
|  3048532 | 13643 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 13644 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 13645 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 13646 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 13647 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 13648 | `	)` |
|        2 | 13649 |  |
|        - | 13650 | `	VmRefObj *pRef;` |
|        - | 13651 | `	sxu32 n;` |
|        - | 13652 | `	/* Check if the referenced object already exists */` |
|  3048534 | 13653 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3048534 | 13654 | `	if( pRef == 0 ){` |
|        - | 13655 | `		/* Not such entry */` |
|    86572 | 13656 | `		return SXERR_NOTFOUND;` |
|        - | 13657 | `	}` |
|        - | 13658 | `	/* Remove the desired entry */` |
|  2961964 | 13659 | `	if( pEntry ){` |
|        - | 13660 | `		SyHashEntry **apEntry;` |
|       56 | 13661 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 13662 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 13663 | `			if( apEntry[n] == pEntry ){` |
|        - | 13664 | `				/* Nullify the entry */` |
|       56 | 13665 | `				apEntry[n] = 0;` |
|        - | 13666 | `				/*` |
|        - | 13667 | `				 * NOTE:` |
|        - | 13668 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 13669 | `				 * we avoid wasting spaces.` |
|        - | 13670 | `				 */` |
|       27 | 13671 | `			}` |
|       79 | 13672 | `		}` |
|       27 | 13673 | `	}` |
|  2961964 | 13674 | `	if( pMapEntry ){` |
|        - | 13675 | `		ph7_hashmap_node **apNode;` |
|  2961910 | 13676 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5923912 | 13677 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2962004 | 13678 | `			if( apNode[n] == pMapEntry ){` |
|        - | 13679 | `				/* nullify the entry */` |
|  2961910 | 13680 | `				apNode[n] = 0;` |
|  1480954 | 13681 | `			}` |
|  1481003 | 13682 | `		}` |
|  1480954 | 13683 | `	}` |
|  2961964 | 13684 | `	return SXRET_OK;` |
|  1524268 | 13685 |  |
|        - | 13686 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 13687 | `/*` |
|        - | 13688 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 13689 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 13690 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 13691 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 13692 | ` * For more information on how to register IO stream devices,please` |
|        - | 13693 | ` * refer to the official documentation.` |
|        - | 13694 | ` */` |
|    26024 | 13695 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 13696 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 13697 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 13698 | `	int nByte              /* *pzDevice length*/` |
|        - | 13699 | `	)` |
|        2 | 13700 |  |
|        - | 13701 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 13702 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 13703 | `	SyString sDev,sCur;` |
|        - | 13704 | `	sxu32 n,nEntry;` |
|        - | 13705 | `	int rc;` |
|        - | 13706 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    26026 | 13707 | `	zNext = zCur = zIn = *pzDevice;` |
|    26026 | 13708 | `	zEnd = &zIn[nByte];` |
|  1656236 | 13709 | `	while( zIn < zEnd ){` |
|  1630214 | 13710 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 13711 | `			/* Got one */` |
|        3 | 13712 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 13713 | `			break;` |
|        - | 13714 | `		}` |
|        - | 13715 | `		/* Advance the cursor */` |
|  1630212 | 13716 | `		zIn++;` |
|        2 | 13717 | `	}` |
|    26026 | 13718 | `	if( zIn >= zEnd ){` |
|        - | 13719 | `		/* No such scheme,return the default stream */` |
|    26024 | 13720 | `		return pVm->pDefStream;` |
|        - | 13721 | `	}` |
|        3 | 13722 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 13723 | `	/* Remove leading and trailing white spaces */` |
|        3 | 13724 | `	SyStringFullTrim(&sDev);` |
|        - | 13725 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 13726 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 13727 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 13728 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 13729 | `		pStream = apStream[n];` |
|        3 | 13730 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 13731 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 13732 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 13733 | `		if( rc == 0 ){` |
|        - | 13734 | `			/* Stream device found */` |
|        3 | 13735 | `			*pzDevice = zNext;` |
|        3 | 13736 | `			return pStream;` |
|        - | 13737 | `		}` |
|      ! 0 | 13738 | `	}` |
|        - | 13739 | `	/* No such stream,return NULL */` |
|      ! 0 | 13740 | `	return 0;` |
|    13014 | 13741 |  |
|        - | 13742 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 13743 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 13744 |  |
