# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5175/6759 lines (76.56%)

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
|   811424 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   811426 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   811392 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   811382 |   104 | `	return FALSE;` |
|   405736 |   105 |  |
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
|   521260 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   521262 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   521262 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   521258 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   521258 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   521258 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   521258 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   521258 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   521258 |   152 | `	pCons->xExpand = xExpand;` |
|   521258 |   153 | `	pCons->pUserData = pUserData;` |
|   521258 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   521258 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   521258 |   161 | `	return SXRET_OK;` |
|   260632 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1146004 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1146006 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1146006 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1146006 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1146006 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1146006 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1146006 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1146006 |   195 | `	pFunc->pVm   = pVm;` |
|  1146006 |   196 | `	pFunc->xFunc = xFunc;` |
|  1146006 |   197 | `	pFunc->pUserData = pUserData;` |
|  1146006 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1146006 |   200 | `	*ppOut = pFunc;` |
|  1146006 |   201 | `	return SXRET_OK;` |
|   573004 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1148406 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1148408 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1148408 |   223 | `	if( pEntry ){` |
|     2404 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2404 |   225 | `		pFunc->pUserData = pUserData;` |
|     2404 |   226 | `		pFunc->xFunc = xFunc;` |
|     2404 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2404 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1146006 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1146006 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1146006 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1146006 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1146006 |   243 | `	return SXRET_OK;` |
|   574205 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   163934 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   163936 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   163936 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   163936 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   163936 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   163936 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   163936 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   163936 |   270 | `	pFunc->iFlags = iFlags;` |
|   163936 |   271 | `	pFunc->pUserData = pUserData;` |
|   163936 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   163936 |   273 | `	return SXRET_OK;` |
|        2 |   274 |  |
|        - |   275 | `/*` |
|        - |   276 | ` * Namespace-aware function lookup.` |
|        - |   277 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   278 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   279 | ` */` |
|        - |   280 | `/*` |
|        - |   281 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   282 | ` */` |
|   644592 |   283 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   284 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   285 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   286 | `	SyString *pName     /* Function name */` |
|        - |   287 | `	)` |
|        2 |   288 |  |
|        - |   289 | `	SyHashEntry *pEntry;` |
|        - |   290 | `	sxi32 rc;` |
|   644594 |   291 | `	if( pName == 0 ){` |
|        - |   292 | `		/* Use the built-in name */` |
|    35384 |   293 | `		pName = &pFunc->sName;` |
|    17691 |   294 | `	}` |
|        - |   295 | `	/* Check for duplicates (functions with the same name) first */` |
|   644594 |   296 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   644594 |   297 | `	if( pEntry ){` |
|   502272 |   298 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   502272 |   299 | `		if( pLink != pFunc ){` |
|        - |   300 | `			/* Link */` |
|      188 |   301 | `			pFunc->pNextName = pLink;` |
|      188 |   302 | `			pEntry->pUserData = pFunc;` |
|       93 |   303 | `		}` |
|   502272 |   304 | `		return SXRET_OK;` |
|        - |   305 | `	}` |
|        - |   306 | `	/* First time seen */` |
|   142324 |   307 | `	pFunc->pNextName = 0;` |
|   142324 |   308 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   142324 |   309 | `	return rc;` |
|   322298 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   313 | ` */` |
|    45920 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   315 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   316 | `	ph7_class *pClass /* Target Class */` |
|        - |   317 | `	)` |
|        2 |   318 |  |
|    45922 |   319 | `	SyString *pName = &pClass->sName;` |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|        - |   322 | `	/* Check for duplicates */` |
|    45922 |   323 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    45922 |   324 | `	if( pEntry ){` |
|       31 |   325 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   326 | `		/* Link entry with the same name */` |
|       31 |   327 | `		pClass->pNextName = pLink;` |
|       31 |   328 | `		pEntry->pUserData = pClass;` |
|       31 |   329 | `		return SXRET_OK;` |
|        - |   330 | `	}` |
|    45892 |   331 | `	pClass->pNextName = 0;` |
|        - |   332 | `	/* Perform a simple hashtable insertion */` |
|    45892 |   333 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    45892 |   334 | `	return rc;` |
|    22962 |   335 |  |
|        - |   336 | `/*` |
|        - |   337 | ` * Instruction builder interface.` |
|        - |   338 | ` */` |
|  3309296 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3309298 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3309298 |   352 | `	sInstr.iP1 = iP1;` |
|  3309298 |   353 | `	sInstr.iP2 = iP2;` |
|  3309298 |   354 | `	sInstr.p3  = p3;` |
|  3309298 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   190800 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    95399 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3309298 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3309298 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3309298 |   365 | `	return rc;` |
|        2 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Swap the current bytecode container with the given one.` |
|        - |   369 | ` */` |
|   392668 |   370 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   371 |  |
|   392670 |   372 | `	if( pContainer == 0 ){` |
|        - |   373 | `		/* Point to the default container */` |
|      ! 0 |   374 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   375 | `	}else{` |
|        - |   376 | `		/* Change container */` |
|   392670 |   377 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   378 | `	}` |
|   392670 |   379 | `	return SXRET_OK;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Return the current bytecode container.` |
|        - |   383 | ` */` |
|   196334 |   384 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   385 |  |
|   196336 |   386 | `	return pVm->pByteContainer;` |
|        2 |   387 |  |
|        - |   388 | `/*` |
|        - |   389 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   390 | ` */` |
|   188054 |   391 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   392 |  |
|        - |   393 | `	VmInstr *pInstr;` |
|   188056 |   394 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   188056 |   395 | `	return pInstr;` |
|        2 |   396 |  |
|        - |   397 | `/*` |
|        - |   398 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   399 | ` */` |
|   991544 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|   991546 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   178792 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   178794 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   640820 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   640822 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   417 |  |
|    27540 |   418 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   419 |  |
|        - |   420 | `	VmInstr *aInstr;` |
|        - |   421 | `	sxu32 n;` |
|    27542 |   422 | `	n = SySetUsed(pVm->pByteContainer);` |
|    27542 |   423 | `	if( n < 2 ){` |
|      ! 0 |   424 | `		return 0;` |
|        - |   425 | `	}` |
|    27542 |   426 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    27542 |   427 | `	return &aInstr[n - 2];` |
|    13772 |   428 |  |
|        - |   429 | `/*` |
|        - |   430 | ` * Allocate a new virtual machine frame.` |
|        - |   431 | ` */` |
|    16682 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    16684 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16684 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    16684 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    16684 |   447 | `	pFrame->pUserData = pUserData;` |
|    16684 |   448 | `	pFrame->pThis = pThis;` |
|    16684 |   449 | `	pFrame->pVm = pVm;` |
|    16684 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16684 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16684 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16684 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16684 |   454 | `	return pFrame;` |
|     8343 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    16640 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    16642 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    16642 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    16642 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    16642 |   476 | `	pVm->pFrame = pFrame;` |
|    16642 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    13974 |   479 | `		*ppFrame = pFrame;` |
|     6986 |   480 | `	}` |
|    16642 |   481 | `	return SXRET_OK;` |
|     8322 |   482 |  |
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
|    13972 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    13974 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13974 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    13974 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13974 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13876 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    95964 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    82090 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    41046 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    13876 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    96020 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    82146 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    41074 |   545 | `			}` |
|     6937 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    13974 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13974 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    13974 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13974 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    13974 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6986 |   554 | `	}` |
|    13974 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6498730 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6499212 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      482 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6498732 |   566 | `	return pFrame;` |
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
|   125796 |   686 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   687 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   688 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   689 | `	)` |
|        2 |   690 |  |
|        - |   691 | `	ph7_class_method *pMeth;` |
|        - |   692 | `	ph7_class_attr *pAttr;` |
|        - |   693 | `	SyHashEntry *pEntry;` |
|        - |   694 | `	sxi32 rc;` |
|        - |   695 | `	/* Reset the loop cursor */` |
|   125798 |   696 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   697 | `	/* Process only static and constant attribute */` |
|   530162 |   698 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   699 | `		/* Extract the current attribute */` |
|   341468 |   700 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   341468 |   701 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   702 | `			ph7_value *pMemObj;` |
|        - |   703 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1328 |   704 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1328 |   705 | `			if( pMemObj == 0 ){` |
|      ! 0 |   706 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   707 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   708 | `					&pClass->sName,&pAttr->sName` |
|        - |   709 | `					);` |
|      ! 0 |   710 | `				return SXERR_MEM;` |
|        - |   711 | `			}` |
|     1328 |   712 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   713 | `				/* Initialize attribute default value (any complex expression) */` |
|     1328 |   714 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      663 |   715 | `			}` |
|        - |   716 | `			/* Record attribute index */` |
|     1328 |   717 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   718 | `			/* Install static attribute in the reference table */` |
|     1328 |   719 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      663 |   720 | `		}` |
|        2 |   721 | `	}` |
|        - |   722 | `	/* Install class methods */` |
|   125798 |   723 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   724 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   725 | `		 */` |
|    54394 |   726 | `		return SXRET_OK;` |
|        - |   727 | `	}` |
|        - |   728 | `	/* Create constructor alias if not yet done */` |
|    71406 |   729 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   730 | `		/* User constructor with the same base class name */` |
|     5416 |   731 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5416 |   732 | `		if( pEntry ){` |
|      ! 0 |   733 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   734 | `			/* Create the alias */` |
|      ! 0 |   735 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   736 | `		}` |
|     2707 |   737 | `	}` |
|        - |   738 | `	/* Install the methods now */` |
|    71406 |   739 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   716326 |   740 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   609220 |   741 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   609220 |   742 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   609212 |   743 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   609212 |   744 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   745 | `				return rc;` |
|        - |   746 | `			}` |
|   304605 |   747 | `		}` |
|        2 |   748 | `	}` |
|        - |   749 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    71406 |   750 | `	pClass->bMounted = TRUE;` |
|    71406 |   751 | `	return SXRET_OK;` |
|    62900 |   752 |  |
|        - |   753 | `/*` |
|        - |   754 | ` * Allocate a private frame for attributes of the given` |
|        - |   755 | ` * class instance (Object in the PHP jargon).` |
|        - |   756 | ` */` |
|     1282 |   757 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   758 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   759 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   760 | `	)` |
|        2 |   761 |  |
|     1284 |   762 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   763 | `	ph7_class_attr *pAttr;` |
|        - |   764 | `	SyHashEntry *pEntry;` |
|        - |   765 | `	sxi32 rc;` |
|        - |   766 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1284 |   767 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     5276 |   768 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   769 | `		VmClassAttr *pVmAttr;` |
|        - |   770 | `		/* Extract the current attribute */` |
|     3994 |   771 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3994 |   772 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3994 |   773 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   774 | `			return SXERR_MEM;` |
|        - |   775 | `		}` |
|     3994 |   776 | `		pVmAttr->pAttr = pAttr;` |
|     3994 |   777 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   778 | `			ph7_value *pMemObj;` |
|        - |   779 | `			/* Reserve a memory object for this attribute */` |
|     3970 |   780 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3970 |   781 | `			if( pMemObj == 0 ){` |
|      ! 0 |   782 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   783 | `				return SXERR_MEM;` |
|        - |   784 | `			}` |
|     3970 |   785 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3970 |   786 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   787 | `				/* Initialize attribute default value (any complex expression) */` |
|     1294 |   788 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      646 |   789 | `			}` |
|     3970 |   790 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3970 |   791 | `			if( rc != SXRET_OK ){` |
|        - |   792 | `				VmSlot sSlot;` |
|        - |   793 | `				/* Restore memory object */` |
|      ! 0 |   794 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   795 | `				sSlot.pUserData = 0;` |
|      ! 0 |   796 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   797 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   798 | `				return SXERR_MEM;` |
|        - |   799 | `			}` |
|        - |   800 | `			/* Install attribute in the reference table */` |
|     3970 |   801 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1986 |   802 | `		}else{` |
|        - |   803 | `			/* Install static/constant attribute */` |
|       26 |   804 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   805 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   806 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   807 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   808 | `				return SXERR_MEM;` |
|        - |   809 | `			}` |
|        - |   810 | `		}` |
|        2 |   811 | `	}` |
|     1284 |   812 | `	return SXRET_OK;` |
|      643 |   813 |  |
|        - |   814 | `/* Forward declaration */` |
|        - |   815 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   816 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   817 | `/*` |
|        - |   818 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   819 | ` */` |
|        - |   820 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   821 | `/*` |
|        - |   822 | ` * Reserve a constant memory object.` |
|        - |   823 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   824 | ` */` |
|   379976 |   825 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   826 |  |
|        - |   827 | `	ph7_value *pObj;` |
|        - |   828 | `	sxi32 rc;` |
|   379978 |   829 | `	if( pIndex ){` |
|        - |   830 | `		/* Object index in the object table */` |
|   371974 |   831 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   185986 |   832 | `	}` |
|        - |   833 | `	/* Reserve a slot for the new object */` |
|   379978 |   834 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   379978 |   835 | `	if( rc != SXRET_OK ){` |
|        - |   836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   838 | `		 */` |
|      ! 0 |   839 | `		return 0;` |
|        - |   840 | `	}` |
|   379978 |   841 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   379978 |   842 | `	return pObj;` |
|   189990 |   843 |  |
|        - |   844 | `/*` |
|        - |   845 | ` * Reserve a memory object.` |
|        - |   846 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   847 | ` */` |
|  2143462 |   848 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   849 |  |
|        - |   850 | `	ph7_value *pObj;` |
|        - |   851 | `	sxi32 rc;` |
|  2143464 |   852 | `	if( pIndex ){` |
|        - |   853 | `		/* Object index in the object table */` |
|  2143464 |   854 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1071731 |   855 | `	}` |
|        - |   856 | `	/* Reserve a slot for the new object */` |
|  2143464 |   857 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2143464 |   858 | `	if( rc != SXRET_OK ){` |
|        - |   859 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   860 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   861 | `		 */` |
|      ! 0 |   862 | `		return 0;` |
|        - |   863 | `	}` |
|  2143464 |   864 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2143464 |   865 | `	return pObj;` |
|  1071733 |   866 |  |
|        - |   867 | `/* Forward declaration */` |
|        - |   868 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   869 | `/* Forward declarations for Fiber C functions */` |
|        - |   870 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   871 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   872 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   873 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   874 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   875 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   876 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   877 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   878 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   879 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   880 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   881 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   882 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   883 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   884 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   885 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   886 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   887 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   888 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   889 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   890 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   891 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   892 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   893 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   894 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   895 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   896 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   897 | `/*` |
|        - |   898 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   899 | ` * directly as foreign functions.` |
|        - |   900 | ` */` |
|        - |   901 | `#define PH7_BUILTIN_LIB \` |
|        - |   902 | `	"class Exception { "\` |
|        - |   903 | `    "protected $message = 'Unknown exception';"\` |
|        - |   904 | `    "protected $code = 0;"\` |
|        - |   905 | `    "protected $file;"\` |
|        - |   906 | `    "protected $line;"\` |
|        - |   907 | `    "protected $trace;"\` |
|        - |   908 | `    "protected $previous;"\` |
|        - |   909 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   910 | `	"   if( isset($message) ){"\` |
|        - |   911 | `	"	  $this->message = $message;"\` |
|        - |   912 | `	"   }"\` |
|        - |   913 | `	"   $this->code = $code;"\` |
|        - |   914 | `	"   $this->file = __FILE__;"\` |
|        - |   915 | `	"   $this->line = __LINE__;"\` |
|        - |   916 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   917 | `	"   if( isset($previous) ){"\` |
|        - |   918 | `	"     $this->previous = $previous;"\` |
|        - |   919 | `	"   }"\` |
|        - |   920 | `	"}"\` |
|        - |   921 | `	"public function getMessage(){"\` |
|        - |   922 | `	"   return $this->message;"\` |
|        - |   923 | `	"}"\` |
|        - |   924 | `	" public function getCode(){"\` |
|        - |   925 | `	"  return $this->code;"\` |
|        - |   926 | `	"}"\` |
|        - |   927 | `	"public function getFile(){"\` |
|        - |   928 | `	"  return $this->file;"\` |
|        - |   929 | `	"}"\` |
|        - |   930 | `	"public function getLine(){"\` |
|        - |   931 | `	"  return $this->line;"\` |
|        - |   932 | `	"}"\` |
|        - |   933 | `	"public function getTrace(){"\` |
|        - |   934 | `	"   return $this->trace;"\` |
|        - |   935 | `	"}"\` |
|        - |   936 | `	"public function getTraceAsString(){"\` |
|        - |   937 | `	"  return debug_string_backtrace();"\` |
|        - |   938 | `	"}"\` |
|        - |   939 | `	"public function getPrevious(){"\` |
|        - |   940 | `	"    return $this->previous;"\` |
|        - |   941 | `	"}"\` |
|        - |   942 | `	"public function __toString(){"\` |
|        - |   943 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   944 | `    "}"\` |
|        - |   945 | `	"}"\` |
|        - |   946 | `	"class Error extends Exception { }"\` |
|        - |   947 | `	"class TypeError extends Error { }"\` |
|        - |   948 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   949 | `	"class ValueError extends Error { }"\` |
|        - |   950 | `	"class FiberError extends Error { }"\` |
|        - |   951 | `	"class AssertionError extends Error { }"\` |
|        - |   952 | `	"class ArithmeticError extends Error { }"\` |
|        - |   953 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |   954 | `	"class ErrorException extends Exception { "\` |
|        - |   955 | `	"protected $severity;"\` |
|        - |   956 | `	"public function __construct(string $message = null,"\` |
|        - |   957 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   958 | `	"   if( isset($message) ){"\` |
|        - |   959 | `	"	  $this->message = $message;"\` |
|        - |   960 | `	"   }"\` |
|        - |   961 | `	"   $this->severity = $severity;"\` |
|        - |   962 | `	"   $this->code = $code;"\` |
|        - |   963 | `	"   $this->file = $filename;"\` |
|        - |   964 | `	"   $this->line = $lineno;"\` |
|        - |   965 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   966 | `	"   if( isset($previous) ){"\` |
|        - |   967 | `	"     $this->previous = $previous;"\` |
|        - |   968 | `	"   }"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"public function getSeverity(){"\` |
|        - |   971 | `	"   return $this->severity;"\` |
|        - |   972 | `    "}"\` |
|        - |   973 | `	"}"\` |
|        - |   974 | `	"interface Iterator {"\` |
|        - |   975 | `	"public function current();"\` |
|        - |   976 | `	"public function key();"\` |
|        - |   977 | `	"public function next();"\` |
|        - |   978 | `	"public function rewind();"\` |
|        - |   979 | `	"public function valid();"\` |
|        - |   980 | `	"}"\` |
|        - |   981 | `	"interface IteratorAggregate {"\` |
|        - |   982 | `	"public function getIterator();"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"interface Serializable {"\` |
|        - |   985 | `	"public function serialize();"\` |
|        - |   986 | `	"public function unserialize(string $serialized);"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"/* Directory releated IO */"\` |
|        - |   989 | `	"class Directory {"\` |
|        - |   990 | `	"public $handle = null;"\` |
|        - |   991 | `	"public $path  = null;"\` |
|        - |   992 | `	"public function __construct(string $path)"\` |
|        - |   993 | `	"{"\` |
|        - |   994 | `	"   $this->handle = opendir($path);"\` |
|        - |   995 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   996 | `	"      $this->path = $path;"\` |
|        - |   997 | `	"   }"\` |
|        - |   998 | `	"}"\` |
|        - |   999 | `	"public function __destruct()"\` |
|        - |  1000 | `	"{"\` |
|        - |  1001 | `	"  if( $this->handle != null ){"\` |
|        - |  1002 | `	"       closedir($this->handle);"\` |
|        - |  1003 | `	"  }"\` |
|        - |  1004 | `	"}"\` |
|        - |  1005 | `	"public function read()"\` |
|        - |  1006 | `	"{"\` |
|        - |  1007 | `	"    return readdir($this->handle);"\` |
|        - |  1008 | `	"}"\` |
|        - |  1009 | `	"public function rewind()"\` |
|        - |  1010 | `	"{"\` |
|        - |  1011 | `	"    rewinddir($this->handle);"\` |
|        - |  1012 | `	"}"\` |
|        - |  1013 | `	"public function close()"\` |
|        - |  1014 | `	"{"\` |
|        - |  1015 | `	"    closedir($this->handle);"\` |
|        - |  1016 | `	"    $this->handle = null;"\` |
|        - |  1017 | `	"}"\` |
|        - |  1018 | `	"}"\` |
|        - |  1019 | `	"class Fiber {"\` |
|        - |  1020 | `	"  private $__ctx;"\` |
|        - |  1021 | `	"  private $__callable;"\` |
|        - |  1022 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1023 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1024 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1025 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1026 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1027 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1028 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1029 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1030 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1031 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1032 | `	"}"\` |
|        - |  1033 | `	"class Generator implements Iterator {"\` |
|        - |  1034 | `	"  private $__ctx;"\` |
|        - |  1035 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1036 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1037 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1038 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1039 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1040 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1041 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1042 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1043 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1044 | `	"}"\` |
|        - |  1045 | `	"class stdClass{"\` |
|        - |  1046 | `	"  public $value;"\` |
|        - |  1047 | `	" /* Magic methods */"\` |
|        - |  1048 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1049 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1050 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1051 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1052 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1053 | `	"}"\` |
|        - |  1054 | `	"function dir(string $path){"\` |
|        - |  1055 | `	"   return new Directory($path);"\` |
|        - |  1056 | `	"}"\` |
|        - |  1057 | `	"function Dir(string $path){"\` |
|        - |  1058 | `	"   return new Directory($path);"\` |
|        - |  1059 | `	"}"\` |
|        - |  1060 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1061 | `    "{"\` |
|        - |  1062 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1063 | `	"  $aDir = array();"\` |
|        - |  1064 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1065 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1066 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1067 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1068 | `	"   }"\` |
|        - |  1069 | `	"  closedir($pHandle);"\` |
|        - |  1070 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1071 | `	"      rsort($aDir);"\` |
|        - |  1072 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1073 | `	"      sort($aDir);"\` |
|        - |  1074 | `	"  }"\` |
|        - |  1075 | `	"  return $aDir;"\` |
|        - |  1076 | `	"}"\` |
|        - |  1077 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1078 | `	"/* Open the target directory */"\` |
|        - |  1079 | `	"$zDir = dirname($pattern);"\` |
|        - |  1080 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1081 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1082 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1083 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1084 | `	"	return FALSE;"\` |
|        - |  1085 | `	"}"\` |
|        - |  1086 | `	"$pattern = basename($pattern);"\` |
|        - |  1087 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1088 | `	"/* Loop throw available entries */"\` |
|        - |  1089 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1090 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1091 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1092 | `	"	if( $rc ){"\` |
|        - |  1093 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1094 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1095 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1096 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1097 | `	"		  }"\` |
|        - |  1098 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1099 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1100 | `	"		 continue;"\` |
|        - |  1101 | `	"	   }"\` |
|        - |  1102 | `	"	   /* Add the entry */"\` |
|        - |  1103 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1104 | `	"	}"\` |
|        - |  1105 | `	" }"\` |
|        - |  1106 | `	"/* Close the handle */"\` |
|        - |  1107 | `	"closedir($pHandle);"\` |
|        - |  1108 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1109 | `	"  /* Sort the array */"\` |
|        - |  1110 | `	"  sort($pArray);"\` |
|        - |  1111 | `	"}"\` |
|        - |  1112 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1113 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1114 | `	"  $pArray[] = $pattern;"\` |
|        - |  1115 | `	"}"\` |
|        - |  1116 | `	"/* Return the created array */"\` |
|        - |  1117 | `	"return $pArray;"\` |
|        - |  1118 | `   "}"\` |
|        - |  1119 | `   "/* Creates a temporary file */"\` |
|        - |  1120 | `   "function tmpfile(){"\` |
|        - |  1121 | `   "  /* Extract the temp directory */"\` |
|        - |  1122 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1123 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1124 | `   "    /* Use the current dir */"\` |
|        - |  1125 | `   "    $zTempDir = '.';"\` |
|        - |  1126 | `   "  }"\` |
|        - |  1127 | `   "  /* Create the file */"\` |
|        - |  1128 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1129 | `   "  return $pHandle;"\` |
|        - |  1130 | `   "}"\` |
|        - |  1131 | `   "/* Creates a temporary filename */"\` |
|        - |  1132 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1133 | `   "{"\` |
|        - |  1134 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1135 | `   "}"\` |
|        - |  1136 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1137 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1138 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1139 | `   "/* Copy arguments */"\` |
|        - |  1140 | `   "$nArgs = func_num_args();"\` |
|        - |  1141 | `   "$pNew = array();"\` |
|        - |  1142 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1143 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1144 | `    "}"\` |
|        - |  1145 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1146 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1147 | `	"/* Erase */"\` |
|        - |  1148 | `	"array_erase($pArray);"\` |
|        - |  1149 | `	"/* Unshift */"\` |
|        - |  1150 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1151 | `	"return sizeof($pArray);"\` |
|        - |  1152 | `    "}"\` |
|        - |  1153 | `	"function array_merge_recursive(){"\` |
|        - |  1154 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1155 | `    "$arrays = func_get_args();"\` |
|        - |  1156 | `    "$narrays = count($arrays);"\` |
|        - |  1157 | `    "$ret = array();"\` |
|        - |  1158 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1159 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1160 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1161 | `	 " }"\` |
|        - |  1162 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1163 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1164 | `     "  if( $keyIsInt ) {"\` |
|        - |  1165 | `     "   $ret[] = $value;"\` |
|        - |  1166 | `     "  } else {"\` |
|        - |  1167 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1168 | `     "    $cur = $ret[$key];"\` |
|        - |  1169 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1170 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1171 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1172 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1173 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1174 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1175 | `     "    } else {"\` |
|        - |  1176 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1177 | `     "    }"\` |
|        - |  1178 | `     "   } else {"\` |
|        - |  1179 | `     "    $ret[$key] = $value;"\` |
|        - |  1180 | `     "   }"\` |
|        - |  1181 | `     "  }"\` |
|        - |  1182 | `     " }"\` |
|        - |  1183 | `	 " }"\` |
|        - |  1184 | `	 " return $ret;"\` |
|        - |  1185 | `    "}"\` |
|        - |  1186 | `	"function max(){"\` |
|        - |  1187 | `    "  $pArgs = func_get_args();"\` |
|        - |  1188 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1189 | `	"  return null;"\` |
|        - |  1190 | `    " }"\` |
|        - |  1191 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1192 | `    " $pArg = $pArgs[0];"\` |
|        - |  1193 | `	" if( !is_array($pArg) ){"\` |
|        - |  1194 | `	"   return $pArg; "\` |
|        - |  1195 | `	" }"\` |
|        - |  1196 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1197 | `	"   return null;"\` |
|        - |  1198 | `	" }"\` |
|        - |  1199 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1200 | `	" reset($pArg);"\` |
|        - |  1201 | `	" $max = current($pArg);"\` |
|        - |  1202 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1203 | `	"   if( $val > $max ){"\` |
|        - |  1204 | `	"     $max = $val;"\` |
|        - |  1205 | `    " }"\` |
|        - |  1206 | `	" }"\` |
|        - |  1207 | `	" return $max;"\` |
|        - |  1208 | `    " }"\` |
|        - |  1209 | `    " $max = $pArgs[0];"\` |
|        - |  1210 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1211 | `    " $val = $pArgs[$i];"\` |
|        - |  1212 | `	"if( $val > $max ){"\` |
|        - |  1213 | `	" $max = $val;"\` |
|        - |  1214 | `	"}"\` |
|        - |  1215 | `    " }"\` |
|        - |  1216 | `	" return $max;"\` |
|        - |  1217 | `    "}"\` |
|        - |  1218 | `	"function min(){"\` |
|        - |  1219 | `    "  $pArgs = func_get_args();"\` |
|        - |  1220 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1221 | `	"  return null;"\` |
|        - |  1222 | `    " }"\` |
|        - |  1223 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1224 | `    " $pArg = $pArgs[0];"\` |
|        - |  1225 | `	" if( !is_array($pArg) ){"\` |
|        - |  1226 | `	"   return $pArg; "\` |
|        - |  1227 | `	" }"\` |
|        - |  1228 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1229 | `	"   return null;"\` |
|        - |  1230 | `	" }"\` |
|        - |  1231 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1232 | `	" reset($pArg);"\` |
|        - |  1233 | `	" $min = current($pArg);"\` |
|        - |  1234 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1235 | `	"   if( $val < $min ){"\` |
|        - |  1236 | `	"     $min = $val;"\` |
|        - |  1237 | `    " }"\` |
|        - |  1238 | `	" }"\` |
|        - |  1239 | `	" return $min;"\` |
|        - |  1240 | `    " }"\` |
|        - |  1241 | `    " $min = $pArgs[0];"\` |
|        - |  1242 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1243 | `    " $val = $pArgs[$i];"\` |
|        - |  1244 | `	"if( $val < $min ){"\` |
|        - |  1245 | `	" $min = $val;"\` |
|        - |  1246 | `	" }"\` |
|        - |  1247 | `    " }"\` |
|        - |  1248 | `	" return $min;"\` |
|        - |  1249 | `	"}"\` |
|        - |  1250 | `	"function fileowner(string $file){"\` |
|        - |  1251 | `    " $a = stat($file);"\` |
|        - |  1252 | `	" if( !is_array($a) ){"\` |
|        - |  1253 | `	"	return false;"\` |
|        - |  1254 | `	" }"\` |
|        - |  1255 | `	" return $a['uid'];"\` |
|        - |  1256 | `    "}"\` |
|        - |  1257 | `    "function filegroup(string $file){"\` |
|        - |  1258 | `	" $a = stat($file);"\` |
|        - |  1259 | `	" if( !is_array($a) ){"\` |
|        - |  1260 | `	"	return false;"\` |
|        - |  1261 | `	" }"\` |
|        - |  1262 | `	" return $a['gid'];"\` |
|        - |  1263 | `    "}"\` |
|        - |  1264 | `	 "function fileinode(string $file){"\` |
|        - |  1265 | `	" $a = stat($file);"\` |
|        - |  1266 | `	" if( !is_array($a) ){"\` |
|        - |  1267 | `	"	return false;"\` |
|        - |  1268 | `	" }"\` |
|        - |  1269 | `	" return $a['ino'];"\` |
|        - |  1270 | `    "}"` |
|        - |  1271 |  |
|        - |  1272 | `/*` |
|        - |  1273 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1274 | ` * start compiling the target PHP program.` |
|        - |  1275 | ` */` |
|     2668 |  1276 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1277 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1278 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1279 | `	 )` |
|        2 |  1280 |  |
|        - |  1281 | `	SyString sBuiltin;` |
|        - |  1282 | `	ph7_value *pObj;` |
|        - |  1283 | `	sxi32 rc;` |
|        - |  1284 | `	/* Zero the structure */` |
|     2670 |  1285 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1286 | `	/* Initialize VM fields */` |
|     2670 |  1287 | `	pVm->pEngine = &(*pEngine);` |
|     2670 |  1288 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1289 | `	/* Instructions containers */` |
|     2670 |  1290 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2670 |  1291 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2670 |  1292 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1293 | `	/* Object containers */` |
|     2670 |  1294 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2670 |  1295 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1296 | `	/* Virtual machine internal containers */` |
|     2670 |  1297 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2670 |  1298 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2670 |  1299 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2670 |  1300 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2670 |  1301 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2670 |  1302 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2670 |  1303 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2670 |  1304 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2670 |  1305 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2670 |  1306 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2670 |  1307 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2670 |  1308 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2670 |  1309 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2670 |  1310 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2670 |  1311 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2670 |  1312 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2670 |  1313 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2670 |  1314 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2670 |  1315 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2670 |  1316 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2670 |  1317 | `	pVm->pPendingException = 0;` |
|        - |  1318 | `	/* Configuration containers */` |
|     2670 |  1319 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2670 |  1320 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2670 |  1321 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2670 |  1322 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2670 |  1323 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2670 |  1324 | `	pVm->iResponseStatus = 200;` |
|     2670 |  1325 | `	pVm->bHeadersSent = 0;` |
|     2670 |  1326 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1327 | `	/* Error callbacks containers */` |
|     2670 |  1328 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2670 |  1329 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2670 |  1330 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2670 |  1331 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2670 |  1332 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1333 | `	/* Set a default recursion limit */` |
|        - |  1334 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2670 |  1335 | `	pVm->nMaxDepth = 32;` |
|        - |  1336 | `#else` |
|        - |  1337 | `	pVm->nMaxDepth = 16;` |
|        - |  1338 | `#endif` |
|        - |  1339 | `	/* Default assertion flags */` |
|     2670 |  1340 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1341 | `	/* JSON return status */` |
|     2670 |  1342 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1343 | `	/* PRNG context */` |
|     2670 |  1344 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1345 | `	/* Install the null constant */` |
|     2670 |  1346 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2670 |  1347 | `	if( pObj == 0 ){` |
|      ! 0 |  1348 | `		rc = SXERR_MEM;` |
|      ! 0 |  1349 | `		goto Err;` |
|        - |  1350 | `	}` |
|     2670 |  1351 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1352 | `	/* Install the boolean TRUE constant */` |
|     2670 |  1353 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2670 |  1354 | `	if( pObj == 0 ){` |
|      ! 0 |  1355 | `		rc = SXERR_MEM;` |
|      ! 0 |  1356 | `		goto Err;` |
|        - |  1357 | `	}` |
|     2670 |  1358 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1359 | `	/* Install the boolean FALSE constant */` |
|     2670 |  1360 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2670 |  1361 | `	if( pObj == 0 ){` |
|      ! 0 |  1362 | `		rc = SXERR_MEM;` |
|      ! 0 |  1363 | `		goto Err;` |
|        - |  1364 | `	}` |
|     2670 |  1365 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1366 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1367 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1368 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2670 |  1369 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2670 |  1370 | `	if( pObj == 0 ){` |
|      ! 0 |  1371 | `		rc = SXERR_MEM;` |
|      ! 0 |  1372 | `		goto Err;` |
|        - |  1373 | `	}` |
|     2670 |  1374 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1375 | `	/* Create the global frame */` |
|     2670 |  1376 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2670 |  1377 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1378 | `		goto Err;` |
|        - |  1379 | `	}` |
|        - |  1380 | `	/* Initialize the code generator */` |
|     2670 |  1381 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2670 |  1382 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1383 | `		goto Err;` |
|        - |  1384 | `	}` |
|        - |  1385 | `	/* VM correctly initialized,set the magic number */` |
|     2670 |  1386 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2670 |  1387 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1388 | `	/* Compile the built-in library */` |
|     2670 |  1389 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1390 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2670 |  1391 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1392 | `	/* Register Fiber internal C functions */` |
|     2670 |  1393 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2670 |  1394 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2670 |  1395 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2670 |  1396 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2670 |  1397 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2670 |  1398 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2670 |  1399 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2670 |  1400 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2670 |  1401 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2670 |  1402 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1403 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2670 |  1404 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2670 |  1405 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2670 |  1406 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2670 |  1407 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2670 |  1408 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2670 |  1409 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2670 |  1410 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2670 |  1411 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2670 |  1412 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2670 |  1413 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1414 | `	/* Reset the code generator */` |
|     2670 |  1415 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2670 |  1416 | `	return SXRET_OK;` |
|      ! 0 |  1417 | `Err:` |
|      ! 0 |  1418 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1419 | `	return rc;` |
|     1336 |  1420 |  |
|        - |  1421 | `/*` |
|        - |  1422 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1423 | ` * routine which store the output in an internal blob.` |
|        - |  1424 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1425 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1426 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1427 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1428 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1429 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1430 | ` * to finish executing and extracting the output.` |
|        - |  1431 | ` */` |
|       38 |  1432 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1433 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1434 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1435 | `	void *pUserData     /* User private data */` |
|        - |  1436 | `	)` |
|      ! 0 |  1437 |  |
|        - |  1438 | `	 sxi32 rc;` |
|        - |  1439 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1440 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1441 | `	 return rc;` |
|      ! 0 |  1442 |  |
|        - |  1443 | `/*` |
|        - |  1444 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1445 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1446 | ` */` |
|    14754 |  1447 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1448 |  |
|    14756 |  1449 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    14756 |  1450 | `	if( xCons != VmObConsumer ){` |
|     6452 |  1451 | `		pVm->nOutputLen += nLen;` |
|     6452 |  1452 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      836 |  1453 | `			pVm->bHeadersSent = 1;` |
|      417 |  1454 | `		}` |
|     3225 |  1455 | `	}` |
|    14756 |  1456 |  |
|        - |  1457 | `#define VM_STACK_GUARD 16` |
|        - |  1458 | `/*` |
|        - |  1459 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1460 | ` * our compiled PHP program.` |
|        - |  1461 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1462 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1463 | ` */` |
|    34120 |  1464 | `static ph7_value * VmNewOperandStack(` |
|        - |  1465 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1466 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1467 | `	)` |
|        2 |  1468 |  |
|        - |  1469 | `	ph7_value *pStack;` |
|        - |  1470 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1471 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1472 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1473 | `  ** on the maximum stack depth required.` |
|        - |  1474 | `  **` |
|        - |  1475 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1476 | `  */` |
|    34122 |  1477 | `	nInstr += VM_STACK_GUARD;` |
|    34122 |  1478 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    34122 |  1479 | `	if( pStack == 0 ){` |
|      ! 0 |  1480 | `		return 0;` |
|        - |  1481 | `	}` |
|        - |  1482 | `	/* Initialize the operand stack */` |
|  2130366 |  1483 | `	while( nInstr > 0 ){` |
|  2096246 |  1484 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2096246 |  1485 | `		--nInstr;` |
|        2 |  1486 | `	}` |
|        - |  1487 | `	/* Ready for bytecode execution */` |
|    34122 |  1488 | `	return pStack;` |
|    17062 |  1489 |  |
|        - |  1490 | `/* Forward declaration */` |
|        - |  1491 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1492 | `/*` |
|        - |  1493 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1494 | ` * This routine gets called by the PH7 engine after` |
|        - |  1495 | ` * successful compilation of the target PHP program.` |
|        - |  1496 | ` */` |
|     2402 |  1497 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1498 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1499 | `	)` |
|        2 |  1500 |  |
|        - |  1501 | `	SyHashEntry *pEntry;` |
|        - |  1502 | `	sxi32 rc;` |
|     2404 |  1503 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1504 | `		/* Initialize your VM first */` |
|      ! 0 |  1505 | `		return SXERR_CORRUPT;` |
|        - |  1506 | `	}` |
|        - |  1507 | `	/* Mark the VM ready for byte-code execution */` |
|     2404 |  1508 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1509 | `	/* Release the code generator now we have compiled our program */` |
|     2404 |  1510 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1511 | `	/* Emit the DONE instruction */` |
|     2404 |  1512 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2404 |  1513 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1514 | `		return SXERR_MEM;` |
|        - |  1515 | `	}` |
|        - |  1516 | `	/* Script return value */` |
|     2404 |  1517 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1518 | `	/* Allocate a new operand stack */` |
|     2404 |  1519 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2404 |  1520 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1521 | `		return SXERR_MEM;` |
|        - |  1522 | `	}` |
|        - |  1523 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1524 | `	 * private data. */` |
|     2404 |  1525 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2404 |  1526 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1527 | `	/* Allocate the reference table */` |
|     2404 |  1528 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2404 |  1529 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2404 |  1530 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1531 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1532 | `		return SXERR_MEM;` |
|        - |  1533 | `	}` |
|        - |  1534 | `	/* Zero the reference table */` |
|     2404 |  1535 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1536 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2404 |  1537 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2404 |  1538 | `	if( rc != SXRET_OK ){` |
|        - |  1539 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1540 | `		return rc;` |
|        - |  1541 | `	}` |
|        - |  1542 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2404 |  1543 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2404 |  1544 | `	if( rc != SXRET_OK ){` |
|        - |  1545 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1546 | `		return rc;` |
|        - |  1547 | `	}` |
|        - |  1548 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2404 |  1549 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1550 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2404 |  1551 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1552 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2404 |  1553 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1554 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1555 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2404 |  1556 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2404 |  1557 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1558 | `#endif` |
|        - |  1559 | `	/* Initialize and install static and constants class attributes */` |
|     2404 |  1560 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    43420 |  1561 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    41018 |  1562 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    41018 |  1563 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1564 | `			return rc;` |
|        - |  1565 | `		}` |
|        2 |  1566 | `	}` |
|        - |  1567 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2404 |  1568 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1569 | `	/* VM is ready for bytecode execution */` |
|     2404 |  1570 | `	return SXRET_OK;` |
|     1203 |  1571 |  |
|        - |  1572 | `/*` |
|        - |  1573 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1574 | ` */` |
|      ! 0 |  1575 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1576 |  |
|      ! 0 |  1577 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1578 | `		return SXERR_CORRUPT;` |
|        - |  1579 | `	}` |
|        - |  1580 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1581 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1582 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1583 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1584 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1585 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1586 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1587 | `	pVm->bHttpContext = 0;` |
|        - |  1588 | `	/* Set the ready flag */` |
|      ! 0 |  1589 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1590 | `	return SXRET_OK;` |
|      ! 0 |  1591 |  |
|        - |  1592 | `/*` |
|        - |  1593 | ` * Release a Virtual Machine.` |
|        - |  1594 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1595 | ` */` |
|     2394 |  1596 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1597 |  |
|        - |  1598 | `	/* Set the stale magic number */` |
|     2396 |  1599 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1600 | `	/* Release the private memory subsystem */` |
|     2396 |  1601 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2396 |  1602 | `	return SXRET_OK;` |
|        2 |  1603 |  |
|        - |  1604 | `/*` |
|        - |  1605 | ` * Initialize a foreign function call context.` |
|        - |  1606 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1607 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1608 | ` * functions.` |
|        - |  1609 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1610 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1611 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1612 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1613 | ` */` |
|   597898 |  1614 | `static sxi32 VmInitCallContext(` |
|        - |  1615 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1616 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1617 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1618 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1619 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1620 | `	)` |
|        2 |  1621 |  |
|   597900 |  1622 | `	pOut->pFunc = pFunc;` |
|   597900 |  1623 | `	pOut->pVm   = pVm;` |
|   597900 |  1624 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   597900 |  1625 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1626 | `	/* Assume a null return value */` |
|   597900 |  1627 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   597900 |  1628 | `	pOut->pRet = pRet;` |
|   597900 |  1629 | `	pOut->iFlags = iFlags;` |
|   597900 |  1630 | `	return SXRET_OK;` |
|        2 |  1631 |  |
|        - |  1632 | `/*` |
|        - |  1633 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1634 | ` * left behind.` |
|        - |  1635 | ` */` |
|   597898 |  1636 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1637 |  |
|        - |  1638 | `	sxu32 n;` |
|   597900 |  1639 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7262 |  1640 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    20784 |  1641 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13524 |  1642 | `			if( apObj[n] == 0 ){` |
|        - |  1643 | `				/* Already released */` |
|      298 |  1644 | `				continue;` |
|        - |  1645 | `			}` |
|    13228 |  1646 | `			PH7_MemObjRelease(apObj[n]);` |
|    13228 |  1647 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6615 |  1648 | `		}` |
|     7262 |  1649 | `		SySetRelease(&pCtx->sVar);` |
|     3630 |  1650 | `	}` |
|   597900 |  1651 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1652 | `		ph7_aux_data *aAux;` |
|        - |  1653 | `		void *pChunk;` |
|        - |  1654 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1655 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1656 | `		 */` |
|        9 |  1657 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1658 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1659 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1660 | `			/* Release the chunk */` |
|       25 |  1661 | `			if( pChunk ){` |
|       25 |  1662 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1663 | `			}` |
|       13 |  1664 | `		}` |
|        9 |  1665 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1666 | `	}` |
|   597900 |  1667 |  |
|        - |  1668 | `/*` |
|        - |  1669 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1670 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1671 | ` */` |
|      296 |  1672 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1673 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1674 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1675 | `	)` |
|        2 |  1676 |  |
|      298 |  1677 | `	if( pValue == 0 ){` |
|        - |  1678 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1679 | `		return;` |
|        - |  1680 | `	}` |
|      298 |  1681 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1682 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1683 | `		sxu32 n;` |
|     1054 |  1684 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1685 | `			if( apObj[n] == pValue ){` |
|      298 |  1686 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1687 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1688 | `				/* Mark as released */` |
|      298 |  1689 | `				apObj[n] = 0;` |
|      298 |  1690 | `				break;` |
|        - |  1691 | `			}` |
|      380 |  1692 | `		}` |
|      148 |  1693 | `	}` |
|      150 |  1694 |  |
|        - |  1695 | `/*` |
|        - |  1696 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1697 | ` */` |
|  3452214 |  1698 | `static void VmPopOperand(` |
|        - |  1699 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1700 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1701 | `	)` |
|        2 |  1702 |  |
|  3452216 |  1703 | `	ph7_value *pTos = *ppTos;` |
|  7341734 |  1704 | `	while( nPop > 0 ){` |
|  3889520 |  1705 | `		PH7_MemObjRelease(pTos);` |
|  3889520 |  1706 | `		pTos--;` |
|  3889520 |  1707 | `		nPop--;` |
|        2 |  1708 | `	}` |
|        - |  1709 | `	/* Top of the stack */` |
|  3452216 |  1710 | `	*ppTos = pTos;` |
|  3452216 |  1711 |  |
|        - |  1712 | `/*` |
|        - |  1713 | ` * Reserve a memory object.` |
|        - |  1714 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1715 | ` */` |
|  3080956 |  1716 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1717 |  |
|  3080958 |  1718 | `	ph7_value *pObj = 0;` |
|        - |  1719 | `	VmSlot *pSlot;` |
|        - |  1720 | `	sxu32 nIdx;` |
|        - |  1721 | `	/* Check for a free slot */` |
|  3080958 |  1722 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3080958 |  1723 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3080958 |  1724 | `	if( pSlot ){` |
|   937496 |  1725 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   937496 |  1726 | `		nIdx = pSlot->nIdx;` |
|   468747 |  1727 | `	}` |
|  3080958 |  1728 | `	if( pObj == 0 ){` |
|        - |  1729 | `		/* Reserve a new memory object */` |
|  2143464 |  1730 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2143464 |  1731 | `		if( pObj == 0 ){` |
|      ! 0 |  1732 | `			return 0;` |
|        - |  1733 | `		}` |
|  1071731 |  1734 | `	}` |
|        - |  1735 | `	/* Set a null default value */` |
|  3080958 |  1736 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3080958 |  1737 | `	pObj->nIdx = nIdx;` |
|  3080958 |  1738 | `	return pObj;` |
|  1540480 |  1739 |  |
|        - |  1740 | `/*` |
|        - |  1741 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1742 | ` */` |
|    31028 |  1743 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1744 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1745 | `	const char *zKey,  /* Entry key */` |
|        - |  1746 | `	sxu32 nByte,       /* Key length */` |
|        - |  1747 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1748 | `	)` |
|        2 |  1749 |  |
|        - |  1750 | `	ph7_value sKey;` |
|        - |  1751 | `	sxi32 rc;` |
|    31030 |  1752 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    31030 |  1753 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1754 | `	/* Perform the insertion */` |
|    31030 |  1755 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    31030 |  1756 | `	PH7_MemObjRelease(&sKey);` |
|    31030 |  1757 | `	return rc;` |
|        2 |  1758 |  |
|        - |  1759 | `/*` |
|        - |  1760 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1761 | ` * Return a pointer to the variable value on success.` |
|        - |  1762 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1763 | ` */` |
|  3214646 |  1764 | `static ph7_value * VmExtractMemObj(` |
|        - |  1765 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1766 | `	const SyString *pName, /* Variable name */` |
|        - |  1767 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1768 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1769 | `	)` |
|        2 |  1770 |  |
|  3214648 |  1771 | `	int bNullify = FALSE;` |
|        - |  1772 | `	SyHashEntry *pEntry;` |
|        - |  1773 | `	VmFrame *pFrame;` |
|        - |  1774 | `	ph7_value *pObj;` |
|        - |  1775 | `	sxu32 nIdx;` |
|        - |  1776 | `	sxi32 rc;` |
|        - |  1777 | `	/* Point to the top active frame */` |
|  3214648 |  1778 | `	pFrame = pVm->pFrame;` |
|  3214648 |  1779 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1780 | `	/* Perform the lookup */` |
|  3214648 |  1781 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1782 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1783 | `		pName = &sAnnon;` |
|        - |  1784 | `		/* Always nullify the object */` |
|      ! 0 |  1785 | `		bNullify = TRUE;` |
|      ! 0 |  1786 | `		bDup = FALSE;` |
|      ! 0 |  1787 | `	}` |
|        - |  1788 | `	/* Check the superglobals table first */` |
|  3214648 |  1789 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3214648 |  1790 | `	if( pEntry == 0 ){` |
|        - |  1791 | `		/* Query the top active frame */` |
|  3214608 |  1792 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3214608 |  1793 | `		if( pEntry == 0 ){` |
|    89146 |  1794 | `			char *zName = (char *)pName->zString;` |
|        - |  1795 | `			VmSlot sLocal;` |
|    89146 |  1796 | `			if( !bCreate ){` |
|        - |  1797 | `				/* Do not create the variable,return NULL instead */` |
|       42 |  1798 | `				return 0;` |
|        - |  1799 | `			}` |
|        - |  1800 | `			/* No such variable,automatically create a new one and install` |
|        - |  1801 | `			 * it in the current frame.` |
|        - |  1802 | `			 */` |
|    89106 |  1803 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    89106 |  1804 | `			if( pObj == 0 ){` |
|      ! 0 |  1805 | `				return 0;` |
|        - |  1806 | `			}` |
|    89106 |  1807 | `			nIdx = pObj->nIdx;` |
|    89106 |  1808 | `			if( bDup ){` |
|        - |  1809 | `				/* Duplicate name */` |
|      168 |  1810 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1811 | `				if( zName == 0 ){` |
|      ! 0 |  1812 | `					return 0;` |
|        - |  1813 | `				}` |
|       83 |  1814 | `			}` |
|        - |  1815 | `			/* Link to the top active VM frame */` |
|    89106 |  1816 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    89106 |  1817 | `			if( rc != SXRET_OK ){` |
|        - |  1818 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1819 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1820 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1821 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1822 | `				return 0;` |
|        - |  1823 | `			}` |
|    89106 |  1824 | `			if( pFrame->pParent != 0 ){` |
|        - |  1825 | `				/* Local variable */` |
|    82126 |  1826 | `				sLocal.nIdx = nIdx;` |
|    82126 |  1827 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    41064 |  1828 | `			}else{` |
|        - |  1829 | `				/* Register in the $GLOBALS array */` |
|     6982 |  1830 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1831 | `			}` |
|        - |  1832 | `			/* Install in the reference table */` |
|    89106 |  1833 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1834 | `			/* Save object index */` |
|    89106 |  1835 | `			pObj->nIdx = nIdx;` |
|    44554 |  1836 | `		}else{` |
|        - |  1837 | `			/* Extract variable contents */` |
|  3125464 |  1838 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3125464 |  1839 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3125464 |  1840 | `			if( bNullify && pObj ){` |
|      ! 0 |  1841 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1842 | `			}` |
|        - |  1843 | `		}` |
|  1607395 |  1844 | `	}else{` |
|        - |  1845 | `		/* Superglobal */` |
|       42 |  1846 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1847 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1848 | `	}` |
|  3214608 |  1849 | `	return pObj;` |
|  1607435 |  1850 |  |
|        - |  1851 | `/*` |
|        - |  1852 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1853 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1854 | ` */` |
|     2706 |  1855 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1856 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1857 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1858 | `	sxu32 nByte        /* zName length */` |
|        - |  1859 | `	)` |
|        2 |  1860 |  |
|        - |  1861 | `	SyHashEntry *pEntry;` |
|        - |  1862 | `	ph7_value *pValue;` |
|        - |  1863 | `	sxu32 nIdx;` |
|        - |  1864 | `	/* Query the superglobal table */` |
|     2708 |  1865 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2708 |  1866 | `	if( pEntry == 0 ){` |
|        - |  1867 | `		/* No such entry */` |
|      ! 0 |  1868 | `		return 0;` |
|        - |  1869 | `	}` |
|        - |  1870 | `	/* Extract the superglobal index in the global object pool */` |
|     2708 |  1871 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1872 | `	/* Extract the variable value  */` |
|     2708 |  1873 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2708 |  1874 | `	return pValue;` |
|     1355 |  1875 |  |
|        - |  1876 | `/*` |
|        - |  1877 | ` * Perform a raw hashmap insertion.` |
|        - |  1878 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1879 | ` */` |
|     2736 |  1880 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1881 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1882 | `	const char *zKey,   /* Entry key */` |
|        - |  1883 | `	int nKeylen,        /* zKey length*/` |
|        - |  1884 | `	const char *zData,  /* Entry data */` |
|        - |  1885 | `	int nLen            /* zData length */` |
|        - |  1886 | `	)` |
|        2 |  1887 |  |
|        - |  1888 | `	ph7_value sKey,sValue;` |
|        - |  1889 | `	sxi32 rc;` |
|     2738 |  1890 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2738 |  1891 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2738 |  1892 | `	if( zKey ){` |
|     2716 |  1893 | `		if( nKeylen < 0 ){` |
|     2664 |  1894 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1331 |  1895 | `		}` |
|     2716 |  1896 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1357 |  1897 | `	}` |
|     2738 |  1898 | `	if( zData ){` |
|     2738 |  1899 | `		if( nLen < 0 ){` |
|        - |  1900 | `			/* Compute length automatically */` |
|      144 |  1901 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1902 | `		}` |
|     2738 |  1903 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1368 |  1904 | `	}` |
|        - |  1905 | `	/* Perform the insertion */` |
|     2738 |  1906 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2738 |  1907 | `	PH7_MemObjRelease(&sKey);` |
|     2738 |  1908 | `	PH7_MemObjRelease(&sValue);` |
|     2738 |  1909 | `	return rc;` |
|        2 |  1910 |  |
|        - |  1911 | `/*` |
|        - |  1912 | ` * Configure a working virtual machine instance.` |
|        - |  1913 | ` *` |
|        - |  1914 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1915 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1916 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1917 | ` * The second argument to this function is an integer configuration option` |
|        - |  1918 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1919 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1920 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1921 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1922 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1923 | ` */` |
|    38762 |  1924 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1925 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1926 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1927 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1928 | `	)` |
|        2 |  1929 |  |
|    38764 |  1930 | `	sxi32 rc = SXRET_OK;` |
|    38764 |  1931 | `	switch(nOp){` |
|     1193 |  1932 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2388 |  1933 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2388 |  1934 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1935 | `		/* VM output consumer callback */` |
|        - |  1936 | `#ifdef UNTRUST` |
|        - |  1937 | `		if( xConsumer == 0 ){` |
|        - |  1938 | `			rc = SXERR_CORRUPT;` |
|        - |  1939 | `			break;` |
|        - |  1940 | `		}` |
|        - |  1941 | `#endif` |
|        - |  1942 | `		/* Install the output consumer */` |
|     2388 |  1943 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2388 |  1944 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2388 |  1945 | `		break;` |
|        - |  1946 | `							   }` |
|     1201 |  1947 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1948 | `		/* Import path */` |
|        - |  1949 | `		  const char *zPath;` |
|        - |  1950 | `		  SyString sPath;` |
|     2404 |  1951 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1952 | `#if defined(UNTRUST)` |
|        - |  1953 | `		  if( zPath == 0 ){` |
|        - |  1954 | `			  rc = SXERR_EMPTY;` |
|        - |  1955 | `			  break;` |
|        - |  1956 | `		  }` |
|        - |  1957 | `#endif` |
|     2404 |  1958 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1959 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1960 | `#ifdef __WINNT__` |
|        2 |  1961 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1962 | `#endif` |
|     4806 |  1963 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1964 | `		  /* Remove leading and trailing white spaces */` |
|     2404 |  1965 | `		  SyStringFullTrim(&sPath);` |
|     2404 |  1966 | `		  if( sPath.nByte > 0 ){` |
|        - |  1967 | `			  /* Store the path in the corresponding conatiner */` |
|     2404 |  1968 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1201 |  1969 | `		  }` |
|     2404 |  1970 | `		  break;` |
|        - |  1971 | `									 }` |
|     1201 |  1972 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1973 | `		/* Run-Time Error report */` |
|     2404 |  1974 | `		pVm->bErrReport = 1;` |
|     2404 |  1975 | `		break;` |
|      ! 0 |  1976 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1977 | `		/* Recursion depth */` |
|      ! 0 |  1978 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1979 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1980 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1981 | `		}` |
|      ! 0 |  1982 | `		break;` |
|        - |  1983 | `									   }` |
|      ! 0 |  1984 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1985 | `		/* VM output length in bytes */` |
|      ! 0 |  1986 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1987 | `#ifdef UNTRUST` |
|        - |  1988 | `		if( pOut == 0 ){` |
|        - |  1989 | `			rc = SXERR_CORRUPT;` |
|        - |  1990 | `			break;` |
|        - |  1991 | `		}` |
|        - |  1992 | `#endif` |
|      ! 0 |  1993 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1994 | `		break;` |
|        - |  1995 | `							   }` |
|        - |  1996 |  |
|    12010 |  1997 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1998 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1999 | `		/* Create a new superglobal/global variable */` |
|    24022 |  2000 | `		const char *zName = va_arg(ap,const char *);` |
|    24022 |  2001 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2002 | `		SyHashEntry *pEntry;` |
|        - |  2003 | `		ph7_value *pObj;` |
|        - |  2004 | `		sxu32 nByte;` |
|        - |  2005 | `		sxu32 nIdx;` |
|        - |  2006 | `#ifdef UNTRUST` |
|        - |  2007 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2008 | `			rc = SXERR_CORRUPT;` |
|        - |  2009 | `			break;` |
|        - |  2010 | `		}` |
|        - |  2011 | `#endif` |
|    24022 |  2012 | `		nByte = SyStrlen(zName);` |
|    24022 |  2013 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2014 | `			/* Check if the superglobal is already installed */` |
|    24022 |  2015 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12012 |  2016 | `		}else{` |
|        - |  2017 | `			/* Query the top active VM frame */` |
|      ! 0 |  2018 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2019 | `		}` |
|    24022 |  2020 | `		if( pEntry ){` |
|        - |  2021 | `			/* Variable already installed */` |
|      ! 0 |  2022 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2023 | `			/* Extract contents */` |
|      ! 0 |  2024 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2025 | `			if( pObj ){` |
|        - |  2026 | `				/* Overwrite old contents */` |
|      ! 0 |  2027 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2028 | `			}` |
|      ! 0 |  2029 | `		}else{` |
|        - |  2030 | `			/* Install a new variable */` |
|    24022 |  2031 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    24022 |  2032 | `			if( pObj == 0 ){` |
|      ! 0 |  2033 | `				rc = SXERR_MEM;` |
|      ! 0 |  2034 | `				break;` |
|        - |  2035 | `			}` |
|    24022 |  2036 | `			nIdx = pObj->nIdx;` |
|        - |  2037 | `			/* Copy value */` |
|    24022 |  2038 | `			PH7_MemObjStore(pValue,pObj);` |
|    24022 |  2039 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2040 | `				/* Install the superglobal */` |
|    24022 |  2041 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12012 |  2042 | `			}else{` |
|        - |  2043 | `				/* Install in the current frame */` |
|      ! 0 |  2044 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2045 | `			}` |
|    24022 |  2046 | `			if( rc == SXRET_OK ){` |
|        - |  2047 | `				SyHashEntry *pRef;` |
|    24022 |  2048 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    24022 |  2049 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12012 |  2050 | `				}else{` |
|      ! 0 |  2051 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2052 | `				}` |
|        - |  2053 | `				/* Install in the reference table */` |
|    24022 |  2054 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    24022 |  2055 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2056 | `					/* Register in the $GLOBALS array */` |
|    24022 |  2057 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12010 |  2058 | `				}` |
|    12010 |  2059 | `			}` |
|        - |  2060 | `		}` |
|    24022 |  2061 | `		break;` |
|        - |  2062 | `									}` |
|     1331 |  2063 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2064 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2065 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2066 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2067 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2068 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2069 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2664 |  2070 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2664 |  2071 | `		const char *zValue = va_arg(ap,const char *);` |
|     2664 |  2072 | `		int nLen = va_arg(ap,int);` |
|        - |  2073 | `		ph7_hashmap *pMap;` |
|        - |  2074 | `		ph7_value *pValue;` |
|     2664 |  2075 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2076 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2077 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2663 |  2078 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2079 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2080 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2662 |  2081 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2082 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2083 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2662 |  2084 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2085 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2086 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2662 |  2087 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2088 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2089 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2662 |  2090 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2091 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2092 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2093 | `		}else{` |
|        - |  2094 | `			/* Extract the $_SERVER superglobal */` |
|     2662 |  2095 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2096 | `		}` |
|     2664 |  2097 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2098 | `			/* No such entry */` |
|      ! 0 |  2099 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2100 | `			break;` |
|        - |  2101 | `		}` |
|        - |  2102 | `		/* Point to the hashmap */` |
|     2664 |  2103 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2104 | `		/* Perform the insertion */` |
|     2664 |  2105 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2664 |  2106 | `		break;` |
|        - |  2107 | `								   }` |
|       11 |  2108 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2109 | `		/* Script arguments */` |
|       24 |  2110 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2111 | `		ph7_hashmap *pMap;` |
|        - |  2112 | `		ph7_value *pValue;` |
|        - |  2113 | `		sxu32 n;` |
|       24 |  2114 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2115 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2116 | `			break;` |
|        - |  2117 | `		}` |
|        - |  2118 | `		/* Extract the $argv array */` |
|       24 |  2119 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2120 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2121 | `			/* No such entry */` |
|      ! 0 |  2122 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2123 | `			break;` |
|        - |  2124 | `		}` |
|        - |  2125 | `		/* Point to the hashmap */` |
|       24 |  2126 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2127 | `		/* Perform the insertion */` |
|       24 |  2128 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2129 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2130 | `		if( rc == SXRET_OK ){` |
|       24 |  2131 | `			if( pMap->nEntry > 1 ){` |
|        - |  2132 | `				/* Append space separator first */` |
|       18 |  2133 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2134 | `			}` |
|       24 |  2135 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2136 | `		}` |
|       24 |  2137 | `		break;` |
|        - |  2138 | `								  }` |
|      ! 0 |  2139 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2140 | `		/* error_log() consumer */` |
|      ! 0 |  2141 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2142 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2143 | `		break;` |
|        - |  2144 | `										}` |
|      ! 0 |  2145 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2146 | `		/* Script return value */` |
|      ! 0 |  2147 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2148 | `#ifdef UNTRUST` |
|        - |  2149 | `		if( ppValue == 0 ){` |
|        - |  2150 | `			rc = SXERR_CORRUPT;` |
|        - |  2151 | `			break;` |
|        - |  2152 | `		}` |
|        - |  2153 | `#endif` |
|      ! 0 |  2154 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2155 | `		break;` |
|        - |  2156 | `								   }` |
|     2402 |  2157 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2158 | `		/* Register an IO stream device */` |
|     4806 |  2159 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2160 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7206 |  2161 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4806 |  2162 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2163 | `				/* Invalid stream */` |
|      ! 0 |  2164 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2165 | `				break;` |
|        - |  2166 | `		}` |
|     4806 |  2167 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2168 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2404 |  2169 | `			pVm->pDefStream = pStream;` |
|     1201 |  2170 | `		}` |
|        - |  2171 | `		/* Insert in the appropriate container */` |
|     4806 |  2172 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4806 |  2173 | `		break;` |
|        - |  2174 | `								  }` |
|        8 |  2175 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2176 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2177 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2178 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2179 | `#ifdef UNTRUST` |
|        - |  2180 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2181 | `			rc = SXERR_CORRUPT;` |
|        - |  2182 | `			break;` |
|        - |  2183 | `		}` |
|        - |  2184 | `#endif` |
|       16 |  2185 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2186 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2187 | `		break;` |
|        - |  2188 | `									   }` |
|        8 |  2189 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2190 | `		/* Raw HTTP request*/` |
|       16 |  2191 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2192 | `		int nByte = va_arg(ap,int);` |
|       16 |  2193 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2194 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2195 | `			break;` |
|        - |  2196 | `		}` |
|       16 |  2197 | `		if( nByte < 0 ){` |
|        - |  2198 | `			/* Compute length automatically */` |
|      ! 0 |  2199 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2200 | `		}` |
|        - |  2201 | `		/* Process the request */` |
|       16 |  2202 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2203 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2204 | `		if( rc == SXRET_OK ){` |
|       16 |  2205 | `			pVm->bHttpContext = 1;` |
|        8 |  2206 | `		}` |
|       16 |  2207 | `		break;` |
|        - |  2208 | `									}` |
|        8 |  2209 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2210 | `		/* Extract HTTP response status code */` |
|       16 |  2211 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2212 | `		if( pStatus ){` |
|       16 |  2213 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2214 | `		}` |
|       16 |  2215 | `		break;` |
|        - |  2216 | `										}` |
|        8 |  2217 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2218 | `		/* Iterate response headers via callback */` |
|        - |  2219 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2220 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2221 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2222 | `		if( xCallback ){` |
|       16 |  2223 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2224 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2225 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2226 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2227 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2228 | `							   pUserData);` |
|       12 |  2229 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2230 | `					break;` |
|        - |  2231 | `				}` |
|        6 |  2232 | `			}` |
|        8 |  2233 | `		}` |
|       16 |  2234 | `		break;` |
|        - |  2235 | `										 }` |
|      ! 0 |  2236 | `	default:` |
|        - |  2237 | `		/* Unknown configuration option */` |
|      ! 0 |  2238 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2239 | `		break;` |
|        - |  2240 | `	}` |
|    38764 |  2241 | `	return rc;` |
|        2 |  2242 |  |
|        - |  2243 | `/* Forward declaration */` |
|        - |  2244 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2245 | `/*` |
|        - |  2246 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2247 | ` * format.` |
|        - |  2248 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2249 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2250 | ` * (STDOUT).` |
|        - |  2251 | ` */` |
|        2 |  2252 | `static sxi32 VmByteCodeDump(` |
|        - |  2253 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2254 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2255 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2256 | `	)` |
|        1 |  2257 |  |
|        - |  2258 | `	static const char zDump[] = {` |
|        - |  2259 | `		"====================================================\n"` |
|        - |  2260 | `		"PH7 VM Dump\n"` |
|        - |  2261 | `		"====================================================\n"` |
|        - |  2262 | `	};` |
|        - |  2263 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2264 | `	sxi32 rc = SXRET_OK;` |
|        - |  2265 | `	sxu32 n;` |
|        - |  2266 | `	/* Point to the PH7 instructions */` |
|        3 |  2267 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2268 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2269 | `	n = 0;` |
|        3 |  2270 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2271 | `	/* Dump instructions */` |
|        7 |  2272 | `	for(;;){` |
|       15 |  2273 | `		if( pInstr >= pEnd ){` |
|        - |  2274 | `			/* No more instructions */` |
|        3 |  2275 | `			break;` |
|        - |  2276 | `		}` |
|        - |  2277 | `		/* Format and call the consumer callback */` |
|       19 |  2278 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2279 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2280 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2281 | `		if( rc != SXRET_OK ){` |
|        - |  2282 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2283 | `			return rc;` |
|        - |  2284 | `		}` |
|       13 |  2285 | `		++n;` |
|       13 |  2286 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2287 | `	}` |
|        3 |  2288 | `	return rc;` |
|        2 |  2289 |  |
|        - |  2290 | `/* Forward declaration */` |
|        - |  2291 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2292 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2293 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2294 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2295 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2296 | `/*` |
|        - |  2297 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2298 | ` * consumer callback.` |
|        - |  2299 | ` */` |
|      558 |  2300 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2301 |  |
|      559 |  2302 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      559 |  2303 | `	sxi32 rc = SXRET_OK;` |
|        - |  2304 | `	/* Append a new line */` |
|        - |  2305 | `#ifdef __WINNT__` |
|        1 |  2306 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2307 | `#else` |
|      558 |  2308 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2309 | `#endif` |
|        - |  2310 | `	/* Invoke the output consumer callback */` |
|      559 |  2311 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      559 |  2312 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      559 |  2313 | `	return rc;` |
|        1 |  2314 |  |
|        - |  2315 | `/*` |
|        - |  2316 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2317 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2318 | ` * information.` |
|        - |  2319 | ` */` |
|      134 |  2320 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2321 |  |
|      136 |  2322 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2323 | `		ph7_value apArg[4];` |
|        - |  2324 | `		ph7_value *apArgPtr[4];` |
|        - |  2325 | `		ph7_value sResult;` |
|        - |  2326 | `		SyString sErr;` |
|        - |  2327 | `		/* Prepare arguments */` |
|       61 |  2328 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2329 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2330 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2331 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2332 | `		if( pFile ){` |
|       61 |  2333 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2334 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2335 | `		}else{` |
|      ! 0 |  2336 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2337 | `		}` |
|       61 |  2338 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2339 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2340 | `		/* Set up pointer array */` |
|       61 |  2341 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2342 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2343 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2344 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2345 | `		/* Call the handler */` |
|       61 |  2346 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2347 | `		/* Check return value */` |
|       61 |  2348 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2349 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2350 | `		}` |
|        - |  2351 | `		/* Release */` |
|       61 |  2352 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2353 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2354 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2355 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2356 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2357 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2358 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2359 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2360 | `	}` |
|        - |  2361 | `	/* No handler, always call error handler */` |
|       75 |  2362 | `	return TRUE;` |
|       69 |  2363 |  |
|       98 |  2364 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2365 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2366 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2367 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2368 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2369 | `	)` |
|        2 |  2370 |  |
|      100 |  2371 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2372 | `	SyString *pFile;` |
|        - |  2373 | `	char *zErr;` |
|      100 |  2374 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2375 | `	if( !pVm->bErrReport ){` |
|        - |  2376 | `		/* Don't bother reporting errors */` |
|        3 |  2377 | `		return SXRET_OK;` |
|        - |  2378 | `	}` |
|        - |  2379 | `	/* Reset the working buffer */` |
|       98 |  2380 | `	SyBlobReset(pWorker);` |
|        - |  2381 | `	/* Peek the processed file if available */` |
|       98 |  2382 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2383 | `	if( pFile ){` |
|        - |  2384 | `		/* Append file name */` |
|       98 |  2385 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2386 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2387 | `	}` |
|        - |  2388 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2389 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2390 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2391 | `	 * E_DEPRECATED). */` |
|       98 |  2392 | `	zErr = "Error:  ";` |
|       98 |  2393 | `	switch(iErr){` |
|       19 |  2394 | `	case PH7_CTX_WARNING:` |
|       40 |  2395 | `		zErr = "Warning:  ";` |
|       40 |  2396 | `		break;` |
|        6 |  2397 | `	case PH7_CTX_NOTICE:` |
|       14 |  2398 | `		zErr = "Notice:  ";` |
|       12 |  2399 | `		break;` |
|       23 |  2400 | `	default:` |
|        - |  2401 | `		/* keep iErr unchanged */` |
|       46 |  2402 | `		break;` |
|        - |  2403 | `	}` |
|       98 |  2404 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2405 | `	if( pFuncName ){` |
|        - |  2406 | `		/* Append function name first */` |
|       23 |  2407 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2408 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2409 | `	}` |
|       98 |  2410 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2411 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2412 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2413 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2414 | `	}` |
|       98 |  2415 | `	return rc;` |
|       51 |  2416 |  |
|        - |  2417 | `/*` |
|        - |  2418 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2419 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2420 | ` * information.` |
|        - |  2421 | ` */` |
|       38 |  2422 | `static sxi32 VmThrowErrorAp(` |
|        - |  2423 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2424 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2425 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2426 | `	const char *zFormat, /* Format message */` |
|        - |  2427 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2428 | `	)` |
|        2 |  2429 |  |
|       40 |  2430 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2431 | `	SyBlob sMsg;` |
|        - |  2432 | `	SyString *pFile;` |
|        - |  2433 | `	char *zErr;` |
|       40 |  2434 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2435 | `	if( !pVm->bErrReport ){` |
|        - |  2436 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2437 | `		return SXRET_OK;` |
|        - |  2438 | `	}` |
|        - |  2439 | `	/* Reset the working buffer */` |
|       40 |  2440 | `	SyBlobReset(pWorker);` |
|        - |  2441 | `	/* Peek the processed file if available */` |
|       40 |  2442 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2443 | `	if( pFile ){` |
|        - |  2444 | `		/* Append file name */` |
|       40 |  2445 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2446 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2447 | `	}` |
|        - |  2448 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2449 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2450 | `	 * the correct errno value. */` |
|       40 |  2451 | `	zErr = "Error:  ";` |
|       40 |  2452 | `	switch(iErr){` |
|        4 |  2453 | `	case PH7_CTX_WARNING:` |
|        9 |  2454 | `		zErr = "Warning:  ";` |
|        9 |  2455 | `		break;` |
|        3 |  2456 | `	case PH7_CTX_NOTICE:` |
|        7 |  2457 | `		zErr = "Notice:  ";` |
|        6 |  2458 | `		break;` |
|       12 |  2459 | `	default:` |
|        - |  2460 | `		/* do not change iErr */` |
|       24 |  2461 | `		break;` |
|        - |  2462 | `	}` |
|       40 |  2463 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2464 | `	if( pFuncName ){` |
|        - |  2465 | `		/* Append function name first */` |
|       26 |  2466 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2467 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2468 | `	}` |
|        - |  2469 | `	/* Format the raw message */` |
|       40 |  2470 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2471 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2472 | `	/* Check if a user error handler is installed */` |
|       40 |  2473 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2474 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2475 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2476 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2477 | `	}` |
|       40 |  2478 | `	SyBlobRelease(&sMsg);` |
|       40 |  2479 | `	return rc;` |
|       21 |  2480 |  |
|        - |  2481 | `/*` |
|        - |  2482 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2483 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2484 | ` * information.` |
|        - |  2485 | ` * ------------------------------------` |
|        - |  2486 | ` * Simple boring wrapper function.` |
|        - |  2487 | ` * ------------------------------------` |
|        - |  2488 | ` */` |
|       14 |  2489 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2490 |  |
|        - |  2491 | `	va_list ap;` |
|        - |  2492 | `	sxi32 rc;` |
|       15 |  2493 | `	va_start(ap,zFormat);` |
|       15 |  2494 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2495 | `	va_end(ap);` |
|       15 |  2496 | `	return rc;` |
|        1 |  2497 |  |
|        - |  2498 | `/*` |
|        - |  2499 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  2500 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  2501 | ` */` |
|       10 |  2502 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  2503 |  |
|        - |  2504 | `	ph7_class *pClass;` |
|        - |  2505 | `	ph7_class_instance *pThis;` |
|        - |  2506 | `	ph7_class_method *pCons;` |
|        - |  2507 | `	ph7_value sArg;` |
|        - |  2508 | `	ph7_value *apArg[1];` |
|        - |  2509 | `	SyBlob sMsg;` |
|        - |  2510 | `	SyString sMsgStr;` |
|        - |  2511 | `	VmFrame *pFrame;` |
|        - |  2512 | `	sxi32 rc;` |
|       11 |  2513 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       11 |  2514 | `	if( pClass == 0 ){` |
|      ! 0 |  2515 | `		return PH7_ABORT;` |
|        - |  2516 | `	}` |
|       11 |  2517 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       11 |  2518 | `	if( pThis == 0 ){` |
|      ! 0 |  2519 | `		return PH7_ABORT;` |
|        - |  2520 | `	}` |
|       11 |  2521 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       11 |  2522 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|        5 |  2523 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       11 |  2524 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       11 |  2525 | `	if( pCons ){` |
|       11 |  2526 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       11 |  2527 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       11 |  2528 | `		apArg[0] = &sArg;` |
|       11 |  2529 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       11 |  2530 | `		PH7_MemObjRelease(&sArg);` |
|        5 |  2531 | `	}` |
|       11 |  2532 | `	SyBlobRelease(&sMsg);` |
|       11 |  2533 | `	pFrame = pVm->pFrame;` |
|       11 |  2534 | `	if( pFrame ){` |
|       11 |  2535 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       11 |  2536 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        5 |  2537 | `	}` |
|       11 |  2538 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       11 |  2539 | `	PH7_ClassInstanceUnref(pThis);` |
|       11 |  2540 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2541 | `		return PH7_ABORT;` |
|        - |  2542 | `	}` |
|       11 |  2543 | `	return PH7_EXCEPTION;` |
|        6 |  2544 |  |
|        - |  2545 | `/*` |
|        - |  2546 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2547 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2548 | ` * information.` |
|        - |  2549 | ` * ------------------------------------` |
|        - |  2550 | ` * Simple boring wrapper function.` |
|        - |  2551 | ` * ------------------------------------` |
|        - |  2552 | ` */` |
|       24 |  2553 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2554 |  |
|        - |  2555 | `	sxi32 rc;` |
|       26 |  2556 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2557 | `	return rc;` |
|        2 |  2558 |  |
|        - |  2559 | `/*` |
|        - |  2560 | ` * Resolve function context from the current frame.` |
|        - |  2561 | ` */` |
|      954 |  2562 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2563 |  |
|        - |  2564 | `	VmFrame *pFrame;` |
|        - |  2565 | `	ph7_vm_func *pFunc;` |
|      955 |  2566 | `	*pzFuncName = 0;` |
|      955 |  2567 | `	*pnFuncLen = 0;` |
|      955 |  2568 | `	pFrame = pVm->pFrame;` |
|      955 |  2569 | `	if( pFrame == 0 ){` |
|      ! 0 |  2570 | `		return;` |
|        - |  2571 | `	}` |
|      955 |  2572 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      955 |  2573 | `	if( pFrame->pParent == 0 ){` |
|      947 |  2574 | `		return;` |
|        - |  2575 | `	}` |
|        9 |  2576 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        9 |  2577 | `	if( pFunc == 0 ){` |
|      ! 0 |  2578 | `		return;` |
|        - |  2579 | `	}` |
|        9 |  2580 | `	*pzFuncName = pFunc->sName.zString;` |
|        9 |  2581 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      478 |  2582 |  |
|        - |  2583 | `/*` |
|        - |  2584 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2585 | ` */` |
|      482 |  2586 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2587 |  |
|        - |  2588 | `	SyBlob sOut;` |
|        - |  2589 | `	SyString *pFile;` |
|      483 |  2590 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2591 | `		return PH7_OK;` |
|        - |  2592 | `	}` |
|      483 |  2593 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2594 | `		zClass = "Exception";` |
|      ! 0 |  2595 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2596 | `	}` |
|      483 |  2597 | `	if( zMsg == 0 ){` |
|      ! 0 |  2598 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2599 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2600 | `	}` |
|      483 |  2601 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      477 |  2602 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      238 |  2603 | `	}` |
|      483 |  2604 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      483 |  2605 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      483 |  2606 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      483 |  2607 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      483 |  2608 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      483 |  2609 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      483 |  2610 | `	if( pFile ){` |
|      483 |  2611 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      483 |  2612 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2613 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      241 |  2614 | `	}` |
|      483 |  2615 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      483 |  2616 | `	if( pFile ){` |
|      483 |  2617 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      483 |  2618 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2619 | `		if( zFuncName && nFuncLen > 0 ){` |
|        9 |  2620 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        5 |  2621 | `		}else{` |
|      475 |  2622 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2623 | `		}` |
|      241 |  2624 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2625 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2626 | `	}else{` |
|      ! 0 |  2627 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2628 | `	}` |
|      483 |  2629 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      483 |  2630 | `	if( pFile ){` |
|      483 |  2631 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      483 |  2632 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      483 |  2633 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2634 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      241 |  2635 | `	}` |
|      483 |  2636 | `	VmCallErrorHandler(pVm,&sOut);` |
|      483 |  2637 | `	SyBlobRelease(&sOut);` |
|      483 |  2638 | `	return PH7_ABORT;` |
|      242 |  2639 |  |
|        - |  2640 | `/*` |
|        - |  2641 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2642 | ` */` |
|      480 |  2643 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2644 |  |
|        - |  2645 | `	ph7_vm *pVm;` |
|        - |  2646 | `	ph7_class *pClass;` |
|        - |  2647 | `	ph7_class_instance *pThis;` |
|        - |  2648 | `	ph7_class_method *pCons;` |
|        - |  2649 | `	ph7_value sArg;` |
|        - |  2650 | `	ph7_value *apArg[1];` |
|        - |  2651 | `	SyBlob sMsg;` |
|        - |  2652 | `	SyString sMsgStr;` |
|        - |  2653 | `	VmFrame *pFrame;` |
|        - |  2654 | `	va_list ap;` |
|        - |  2655 | `	sxi32 rc;` |
|        - |  2656 |  |
|      482 |  2657 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2658 | `		return PH7_ABORT;` |
|        - |  2659 | `	}` |
|      482 |  2660 | `	pVm = pCtx->pVm;` |
|      482 |  2661 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2662 | `		zClass = "Error";` |
|      ! 0 |  2663 | `	}` |
|      482 |  2664 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      482 |  2665 | `	if( pClass == 0 ){` |
|      ! 0 |  2666 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2667 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2668 | `			zClass` |
|        - |  2669 | `			);` |
|        - |  2670 | `	}` |
|      482 |  2671 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      482 |  2672 | `	if( pThis == 0 ){` |
|      ! 0 |  2673 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2674 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2675 | `			);` |
|        - |  2676 | `	}` |
|        - |  2677 |  |
|      482 |  2678 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      482 |  2679 | `	va_start(ap,zFormat);` |
|      482 |  2680 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      482 |  2681 | `	va_end(ap);` |
|        - |  2682 |  |
|      482 |  2683 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      482 |  2684 | `	if( pCons ){` |
|      482 |  2685 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      482 |  2686 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      482 |  2687 | `		apArg[0] = &sArg;` |
|      482 |  2688 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      482 |  2689 | `		PH7_MemObjRelease(&sArg);` |
|      240 |  2690 | `	}` |
|      482 |  2691 | `	SyBlobRelease(&sMsg);` |
|        - |  2692 |  |
|      482 |  2693 | `	pFrame = pVm->pFrame;` |
|      482 |  2694 | `	if( pFrame ){` |
|      482 |  2695 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      482 |  2696 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      240 |  2697 | `	}` |
|      482 |  2698 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      482 |  2699 | `	PH7_ClassInstanceUnref(pThis);` |
|      482 |  2700 | `	if( rc == SXERR_ABORT ){` |
|      471 |  2701 | `		return PH7_ABORT;` |
|        - |  2702 | `	}` |
|       12 |  2703 | `	return PH7_EXCEPTION;` |
|      242 |  2704 |  |
|        - |  2705 | `/*` |
|        - |  2706 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2707 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2708 | ` */` |
|      ! 0 |  2709 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2710 |  |
|        - |  2711 | `	ph7_vm *pVm;` |
|        - |  2712 | `	SyBlob sMsg;` |
|      ! 0 |  2713 | `	const char *zFuncName = 0;` |
|      ! 0 |  2714 | `	int nFuncLen = 0;` |
|        - |  2715 | `	va_list ap;` |
|        - |  2716 | `	sxi32 rc;` |
|        - |  2717 |  |
|      ! 0 |  2718 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2719 | `		return PH7_OK;` |
|        - |  2720 | `	}` |
|      ! 0 |  2721 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2722 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2723 | `		zClass = "Error";` |
|      ! 0 |  2724 | `	}` |
|        - |  2725 |  |
|      ! 0 |  2726 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2727 |  |
|      ! 0 |  2728 | `	va_start(ap,zFormat);` |
|      ! 0 |  2729 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2730 | `	va_end(ap);` |
|        - |  2731 |  |
|      ! 0 |  2732 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2733 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2734 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2735 | `	}` |
|      ! 0 |  2736 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2737 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2738 | `	}` |
|      ! 0 |  2739 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2740 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2741 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2742 | `	return rc;` |
|      ! 0 |  2743 |  |
|        - |  2744 | `/*` |
|        - |  2745 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2746 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2747 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2748 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2749 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2750 | ` * when VmByteCodeExec returns.` |
|        - |  2751 | ` */` |
|      132 |  2752 | `static sxi32 VmSuspendCtx(` |
|        - |  2753 | `	ph7_vm *pVm,` |
|        - |  2754 | `	ph7_exec_ctx *pCtx,` |
|        - |  2755 | `	sxi32 pc,` |
|        - |  2756 | `	sxi32 nTos` |
|        - |  2757 | `	)` |
|        2 |  2758 |  |
|       66 |  2759 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      134 |  2760 | `	pCtx->pc = pc;` |
|      134 |  2761 | `	pCtx->nTos = nTos;` |
|      134 |  2762 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      134 |  2763 | `	return PH7_SUSPEND;` |
|        2 |  2764 |  |
|        - |  2765 | `/*` |
|        - |  2766 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2767 | ` *` |
|        - |  2768 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2769 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2770 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2771 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2772 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2773 | ` * then the program execution is halted.` |
|        - |  2774 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2775 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2776 | ` * or to reset the VM to it's initial state.` |
|        - |  2777 | ` */` |
|    34206 |  2778 | `static sxi32 VmByteCodeExec(` |
|        - |  2779 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2780 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2781 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2782 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2783 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2784 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2785 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2786 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2787 | `	)` |
|        2 |  2788 |  |
|        - |  2789 | `	VmInstr *pInstr;` |
|        - |  2790 | `	ph7_value *pTos;` |
|        - |  2791 | `	SySet aArg;` |
|        - |  2792 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2793 | `	sxi32 pc;` |
|        - |  2794 | `	sxi32 rc;` |
|        - |  2795 | `	/* Argument container */` |
|    34208 |  2796 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    34208 |  2797 | `	if( nTos < 0 ){` |
|    32108 |  2798 | `		pTos = &pStack[-1];` |
|    16055 |  2799 | `	}else{` |
|     2102 |  2800 | `		pTos = &pStack[nTos];` |
|        - |  2801 | `	}` |
|    34208 |  2802 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    34208 |  2803 | `	pc = nPc;` |
|        - |  2804 | `	/* Execute as much as we can */` |
|  5165846 |  2805 | `	for(;;){` |
|        - |  2806 | `		/* Fetch the instruction to execute */` |
| 10330990 |  2807 | `		pInstr = &aInstr[pc];` |
| 10330990 |  2808 | `		rc = SXRET_OK;` |
|        - |  2809 | `/*` |
|        - |  2810 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2811 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2812 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2813 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2814 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2815 | ` */` |
| 10330990 |  2816 | `		switch(pInstr->iOp){` |
|        - |  2817 | `/*` |
|        - |  2818 | ` * DONE: P1 * *` |
|        - |  2819 | ` *` |
|        - |  2820 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2821 | ` * and return immediately.` |
|        - |  2822 | ` */` |
|    16785 |  2823 | `case PH7_OP_DONE:` |
|    33572 |  2824 | `	if( pInstr->iP1 ){` |
|        - |  2825 | `#ifdef UNTRUST` |
|        - |  2826 | `		if( pTos < pStack ){` |
|        - |  2827 | `			goto Abort;` |
|        - |  2828 | `		}` |
|        - |  2829 | `#endif` |
|    19478 |  2830 | `		if( pLastRef ){` |
|    12650 |  2831 | `			*pLastRef = pTos->nIdx;` |
|     6324 |  2832 | `		}` |
|    19478 |  2833 | `		if( pResult ){` |
|        - |  2834 | `			/* Execution result */` |
|    18492 |  2835 | `			PH7_MemObjStore(pTos,pResult);` |
|     9245 |  2836 | `		}` |
|    19478 |  2837 | `		VmPopOperand(&pTos,1);` |
|    23834 |  2838 | `	}else if( pLastRef ){` |
|        - |  2839 | `		/* Nothing referenced */` |
|     1098 |  2840 | `		*pLastRef = SXU32_HIGH;` |
|      548 |  2841 | `	}` |
|        - |  2842 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2843 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2844 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2845 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2846 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2847 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2848 | `	 * block can override it.` |
|        - |  2849 | `	 */` |
|    33574 |  2850 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2851 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2852 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2853 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2854 | `		pExc->pFrame = 0;` |
|        3 |  2855 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2856 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2857 | `			pExc->iFinallyDone = 1;` |
|        - |  2858 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2859 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2860 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2861 | `				goto Abort;` |
|        - |  2862 | `			}` |
|        1 |  2863 | `		}` |
|        1 |  2864 | `	}` |
|    33572 |  2865 | `	goto Done;` |
|        - |  2866 | `/*` |
|        - |  2867 | ` * HALT: P1 * *` |
|        - |  2868 | ` *` |
|        - |  2869 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2870 | ` * and abort immediately.` |
|        - |  2871 | ` */` |
|        4 |  2872 | `case PH7_OP_HALT:` |
|        9 |  2873 | `	if( pInstr->iP1 ){` |
|        - |  2874 | `#ifdef UNTRUST` |
|        - |  2875 | `		if( pTos < pStack ){` |
|        - |  2876 | `			goto Abort;` |
|        - |  2877 | `		}` |
|        - |  2878 | `#endif` |
|        9 |  2879 | `		if( pLastRef ){` |
|      ! 0 |  2880 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2881 | `		}` |
|        9 |  2882 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2883 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2884 | `				/* Output the exit message */` |
|        7 |  2885 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2886 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2887 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2888 | `			}` |
|        7 |  2889 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2890 | `			/* Record exit status */` |
|        5 |  2891 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2892 | `		}` |
|        9 |  2893 | `		VmPopOperand(&pTos,1);` |
|        4 |  2894 | `	}else if( pLastRef ){` |
|        - |  2895 | `		/* Nothing referenced */` |
|      ! 0 |  2896 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2897 | `	}` |
|        - |  2898 | `	/* Check if we're in an included file context */` |
|        9 |  2899 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2900 | `		/* Terminate the entire process */` |
|        9 |  2901 | `		exit(pVm->iExitStatus);` |
|        - |  2902 | `	}` |
|      ! 0 |  2903 | `	goto Abort;` |
|        - |  2904 | `/*` |
|        - |  2905 | ` * JMP: * P2 *` |
|        - |  2906 | ` *` |
|        - |  2907 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2908 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2909 | ` */` |
|   222583 |  2910 | `case PH7_OP_JMP:` |
|   445212 |  2911 | `	pc = pInstr->iP2 - 1;` |
|   445212 |  2912 | `	break;` |
|        - |  2913 | `/*` |
|        - |  2914 | ` * JZ: P1 P2 *` |
|        - |  2915 | ` *` |
|        - |  2916 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2917 | ` * entry in the stack if P1 is zero.` |
|        - |  2918 | ` */` |
|   521671 |  2919 | `case PH7_OP_JZ:` |
|        - |  2920 | `#ifdef UNTRUST` |
|        - |  2921 | `	if( pTos < pStack ){` |
|        - |  2922 | `		goto Abort;` |
|        - |  2923 | `	}` |
|        - |  2924 | `#endif` |
|        - |  2925 | `	/* Get a boolean value */` |
|  1043432 |  2926 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  2927 | `		PH7_MemObjToBool(pTos);` |
|       80 |  2928 | `	}` |
|  1043432 |  2929 | `	if( !pTos->x.iVal ){` |
|        - |  2930 | `		/* Take the jump */` |
|   529354 |  2931 | `		pc = pInstr->iP2 - 1;` |
|   264676 |  2932 | `	}` |
|  1043432 |  2933 | `	if( !pInstr->iP1 ){` |
|   828884 |  2934 | `		VmPopOperand(&pTos,1);` |
|   414463 |  2935 | `	}` |
|  1043432 |  2936 | `	break;` |
|        - |  2937 | `/*` |
|        - |  2938 | ` * JNZ: P1 P2 *` |
|        - |  2939 | ` *` |
|        - |  2940 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2941 | ` * entry in the stack if P1 is zero.` |
|        - |  2942 | ` */` |
|    55121 |  2943 | `case PH7_OP_JNZ:` |
|        - |  2944 | `#ifdef UNTRUST` |
|        - |  2945 | `	if( pTos < pStack ){` |
|        - |  2946 | `		goto Abort;` |
|        - |  2947 | `	}` |
|        - |  2948 | `#endif` |
|        - |  2949 | `	/* Get a boolean value */` |
|   110244 |  2950 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2951 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2952 | `	}` |
|   110244 |  2953 | `	if( pTos->x.iVal ){` |
|        - |  2954 | `		/* Take the jump */` |
|     4718 |  2955 | `		pc = pInstr->iP2 - 1;` |
|     2358 |  2956 | `	}` |
|   110244 |  2957 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2958 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2959 | `	}` |
|   110244 |  2960 | `	break;` |
|        - |  2961 | `/*` |
|        - |  2962 | ` * NOOP: * * *` |
|        - |  2963 | ` *` |
|        - |  2964 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2965 | ` * destination.` |
|        - |  2966 | ` */` |
|      ! 0 |  2967 | `case PH7_OP_NOOP:` |
|      ! 0 |  2968 | `	break;` |
|        - |  2969 | `/*` |
|        - |  2970 | ` * POP: P1 * *` |
|        - |  2971 | ` *` |
|        - |  2972 | ` * Pop P1 elements from the operand stack.` |
|        - |  2973 | ` */` |
|   405490 |  2974 | `case PH7_OP_POP: {` |
|   811026 |  2975 | `	sxi32 n = pInstr->iP1;` |
|   811026 |  2976 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2977 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       11 |  2978 | `		n = (sxi32)(pTos - pStack);` |
|        5 |  2979 | `	}` |
|   811026 |  2980 | `	VmPopOperand(&pTos,n);` |
|   811026 |  2981 | `	break;` |
|        - |  2982 | `				 }` |
|        - |  2983 | `/*` |
|        - |  2984 | ` * DUP: * * *` |
|        - |  2985 | ` *` |
|        - |  2986 | ` * Duplicate the top of the stack.` |
|        - |  2987 | ` */` |
|       41 |  2988 | `case PH7_OP_DUP:` |
|        - |  2989 | `#ifdef UNTRUST` |
|        - |  2990 | `	if( pTos < pStack ){` |
|        - |  2991 | `		goto Abort;` |
|        - |  2992 | `	}` |
|        - |  2993 | `#endif` |
|       84 |  2994 | `	pTos++;` |
|       84 |  2995 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  2996 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  2997 | `	break;` |
|        - |  2998 | `/*` |
|        - |  2999 | ` * NSSWITCH: * * P3` |
|        - |  3000 | ` *` |
|        - |  3001 | ` * Switch the active namespace at runtime.` |
|        - |  3002 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  3003 | ` */` |
|     6731 |  3004 | `case PH7_OP_NSSWITCH:` |
|    13464 |  3005 | `	SyBlobReset(&pVm->sNamespace);` |
|    13464 |  3006 | `	if( pInstr->p3 ){` |
|       92 |  3007 | `		const char *zNs = (const char *)pInstr->p3;` |
|       92 |  3008 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       45 |  3009 | `	}` |
|        - |  3010 | `	/* Clear namespace-scoped use-const imports */` |
|    13464 |  3011 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    13464 |  3012 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    13464 |  3013 | `	break;` |
|        - |  3014 | `/* OP_USECONST P1 * P3` |
|        - |  3015 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  3016 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  3017 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  3018 | ` */` |
|        7 |  3019 | `case PH7_OP_USECONST: {` |
|       16 |  3020 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  3021 | `	if( azPair ){` |
|       16 |  3022 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  3023 | `	}` |
|       16 |  3024 | `	break;` |
|        - |  3025 | `				}` |
|        - |  3026 | `/*` |
|        - |  3027 | ` * CVT_INT: * * *` |
|        - |  3028 | ` *` |
|        - |  3029 | ` * Force the top of the stack to be an integer.` |
|        - |  3030 | ` */` |
|       77 |  3031 | `case PH7_OP_CVT_INT:` |
|        - |  3032 | `#ifdef UNTRUST` |
|        - |  3033 | `	if( pTos < pStack ){` |
|        - |  3034 | `		goto Abort;` |
|        - |  3035 | `	}` |
|        - |  3036 | `#endif` |
|      156 |  3037 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      109 |  3038 | `		PH7_MemObjToInteger(pTos);` |
|       54 |  3039 | `	}` |
|        - |  3040 | `	/* Invalidate any prior representation */` |
|      156 |  3041 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      156 |  3042 | `	break;` |
|        - |  3043 | `/*` |
|        - |  3044 | ` * CVT_REAL: * * *` |
|        - |  3045 | ` *` |
|        - |  3046 | ` * Force the top of the stack to be a real.` |
|        - |  3047 | ` */` |
|        4 |  3048 | `case PH7_OP_CVT_REAL:` |
|        - |  3049 | `#ifdef UNTRUST` |
|        - |  3050 | `	if( pTos < pStack ){` |
|        - |  3051 | `		goto Abort;` |
|        - |  3052 | `	}` |
|        - |  3053 | `#endif` |
|        9 |  3054 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3055 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3056 | `	}` |
|        - |  3057 | `	/* Invalidate any prior representation */` |
|        9 |  3058 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3059 | `	break;` |
|        - |  3060 | `/*` |
|        - |  3061 | ` * CVT_STR: * * *` |
|        - |  3062 | ` *` |
|        - |  3063 | ` * Force the top of the stack to be a string.` |
|        - |  3064 | ` */` |
|      146 |  3065 | `case PH7_OP_CVT_STR:` |
|        - |  3066 | `#ifdef UNTRUST` |
|        - |  3067 | `	if( pTos < pStack ){` |
|        - |  3068 | `		goto Abort;` |
|        - |  3069 | `	}` |
|        - |  3070 | `#endif` |
|      294 |  3071 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3072 | `		PH7_MemObjToString(pTos);` |
|      146 |  3073 | `	}` |
|      294 |  3074 | `	break;` |
|        - |  3075 | `/*` |
|        - |  3076 | ` * CVT_BOOL: * * *` |
|        - |  3077 | ` *` |
|        - |  3078 | ` * Force the top of the stack to be a boolean.` |
|        - |  3079 | ` */` |
|        5 |  3080 | `case PH7_OP_CVT_BOOL:` |
|        - |  3081 | `#ifdef UNTRUST` |
|        - |  3082 | `	if( pTos < pStack ){` |
|        - |  3083 | `		goto Abort;` |
|        - |  3084 | `	}` |
|        - |  3085 | `#endif` |
|       11 |  3086 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3087 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3088 | `	}` |
|       11 |  3089 | `	break;` |
|        - |  3090 | `/*` |
|        - |  3091 | ` * CVT_NULL: * * *` |
|        - |  3092 | ` *` |
|        - |  3093 | ` * Nullify the top of the stack.` |
|        - |  3094 | ` */` |
|        3 |  3095 | `case PH7_OP_CVT_NULL:` |
|        - |  3096 | `#ifdef UNTRUST` |
|        - |  3097 | `	if( pTos < pStack ){` |
|        - |  3098 | `		goto Abort;` |
|        - |  3099 | `	}` |
|        - |  3100 | `#endif` |
|        7 |  3101 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3102 | `	break;` |
|        - |  3103 | `/*` |
|        - |  3104 | ` * CVT_NUMC: * * *` |
|        - |  3105 | ` *` |
|        - |  3106 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3107 | ` */` |
|      ! 0 |  3108 | `case PH7_OP_CVT_NUMC:` |
|        - |  3109 | `#ifdef UNTRUST` |
|        - |  3110 | `	if( pTos < pStack ){` |
|        - |  3111 | `		goto Abort;` |
|        - |  3112 | `	}` |
|        - |  3113 | `#endif` |
|        - |  3114 | `	/* Force a numeric cast */` |
|      ! 0 |  3115 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3116 | `	break;` |
|        - |  3117 | `/*` |
|        - |  3118 | ` * CVT_ARRAY: * * *` |
|        - |  3119 | ` *` |
|        - |  3120 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3121 | ` */` |
|       10 |  3122 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3123 | `#ifdef UNTRUST` |
|        - |  3124 | `	if( pTos < pStack ){` |
|        - |  3125 | `		goto Abort;` |
|        - |  3126 | `	}` |
|        - |  3127 | `#endif` |
|        - |  3128 | `	/* Force a hashmap cast */` |
|       21 |  3129 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3130 | `	if( rc != SXRET_OK ){` |
|        - |  3131 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3132 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3133 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3134 | `	}` |
|       21 |  3135 | `	break;` |
|        - |  3136 | `/*` |
|        - |  3137 | ` * CVT_OBJ: * * *` |
|        - |  3138 | ` *` |
|        - |  3139 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3140 | ` */` |
|        8 |  3141 | `case PH7_OP_CVT_OBJ:` |
|        - |  3142 | `#ifdef UNTRUST` |
|        - |  3143 | `	if( pTos < pStack ){` |
|        - |  3144 | `		goto Abort;` |
|        - |  3145 | `	}` |
|        - |  3146 | `#endif` |
|       17 |  3147 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3148 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3149 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3150 | `	}` |
|       17 |  3151 | `	break;` |
|        - |  3152 | `/*` |
|        - |  3153 | ` * ERR_CTRL * * *` |
|        - |  3154 | ` *` |
|        - |  3155 | ` * Error control operator.` |
|        - |  3156 | ` */` |
|    13553 |  3157 | `case PH7_OP_ERR_CTRL:` |
|        - |  3158 | `	/*` |
|        - |  3159 | `	 * TICKET 1433-038:` |
|        - |  3160 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3161 | `	 * use the public API,to control error output.` |
|        - |  3162 | `	 */` |
|    27106 |  3163 | `	break;` |
|        - |  3164 | `/*` |
|        - |  3165 | ` * IS_A * * *` |
|        - |  3166 | ` *` |
|        - |  3167 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3168 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3169 | ` * holding a class name or an object).` |
|        - |  3170 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3171 | ` */` |
|       23 |  3172 | `case PH7_OP_IS_A:{` |
|       48 |  3173 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3174 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3175 | `#ifdef UNTRUST` |
|        - |  3176 | `	if( pNos < pStack ){` |
|        - |  3177 | `		goto Abort;` |
|        - |  3178 | `	}` |
|        - |  3179 | `#endif` |
|       48 |  3180 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3181 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3182 | `		ph7_class *pClass = 0;` |
|        - |  3183 | `		/* Extract the target class */` |
|       46 |  3184 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3185 | `			/* Instance already loaded */` |
|      ! 0 |  3186 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3187 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3188 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3189 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3190 | `			/* Handle self/static/parent keywords */` |
|       46 |  3191 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3192 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3193 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3194 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3195 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3196 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3197 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3198 | `					pClass = pSelf->pBase;` |
|        2 |  3199 | `				}` |
|        3 |  3200 | `			}else{` |
|       36 |  3201 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3202 | `			}` |
|       22 |  3203 | `		}` |
|       46 |  3204 | `		if( pClass ){` |
|        - |  3205 | `			/* Perform the query */` |
|       46 |  3206 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3207 | `		}` |
|       22 |  3208 | `	}` |
|        - |  3209 | `	/* Push result */` |
|       48 |  3210 | `	VmPopOperand(&pTos,1);` |
|       48 |  3211 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3212 | `	pTos->x.iVal = iRes;` |
|       48 |  3213 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3214 | `	break;` |
|        - |  3215 | `				 }` |
|        - |  3216 |  |
|        - |  3217 | `/*` |
|        - |  3218 | ` * LOADC P1 P2 *` |
|        - |  3219 | ` *` |
|        - |  3220 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3221 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3222 | ` */` |
|   871340 |  3223 | `case PH7_OP_LOADC: {` |
|        - |  3224 | `	ph7_value *pObj;` |
|        - |  3225 | `	/* Reserve a room */` |
|  1742726 |  3226 | `	pTos++;` |
|  2605628 |  3227 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1742726 |  3228 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3229 | `			SyHashEntry *pEntry;` |
|        - |  3230 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3231 | `			{` |
|        - |  3232 | `				SyHashEntry *pConstImport;` |
|    25451 |  3233 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    16966 |  3234 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16968 |  3235 | `				if( pConstImport ){` |
|       11 |  3236 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3237 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3238 | `					if( pEntry ){` |
|       11 |  3239 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3240 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3241 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3242 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3243 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3244 | `						break;` |
|        - |  3245 | `					}` |
|        - |  3246 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3247 | `				}` |
|        - |  3248 | `			}` |
|        - |  3249 | `			/* Candidate for expansion via user defined callbacks */` |
|    16958 |  3250 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16958 |  3251 | `			if( pEntry ){` |
|    16954 |  3252 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3253 | `				/* Set a NULL default value */` |
|    16954 |  3254 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16954 |  3255 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3256 | `				/* Invoke the callback and deal with the expanded value */` |
|    16954 |  3257 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3258 | `				/* Mark as constant */` |
|    16954 |  3259 | `				pTos->nIdx = SXU32_HIGH;` |
|    16954 |  3260 | `				break;` |
|        - |  3261 | `			}` |
|        - |  3262 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3263 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3264 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3265 | `			{` |
|        6 |  3266 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3267 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3268 | `				sxu32 j;` |
|        6 |  3269 | `				int isQualified = 0;` |
|       32 |  3270 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3271 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3272 | `				}` |
|        6 |  3273 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3274 | `					/* Try current_namespace\name */` |
|      ! 0 |  3275 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3276 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3277 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3278 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3279 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3280 | `					if( pEntry ){` |
|      ! 0 |  3281 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3282 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3283 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3284 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3285 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3286 | `						break;` |
|        - |  3287 | `					}` |
|        - |  3288 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3289 | `				}` |
|        6 |  3290 | `				if( isQualified ){` |
|        - |  3291 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3292 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3293 | `					SyBlob sErr;` |
|        3 |  3294 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3295 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3296 | `					if( pErrFile ){` |
|        3 |  3297 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3298 | `					}` |
|        3 |  3299 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3300 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3301 | `					SyBlobRelease(&sErr);` |
|        3 |  3302 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3303 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3304 | `					goto LoadC_Done;` |
|        - |  3305 | `				}` |
|        - |  3306 | `			}` |
|        1 |  3307 | `		}` |
|  1725762 |  3308 | `		PH7_MemObjLoad(pObj,pTos);` |
|   862904 |  3309 | `	}else{` |
|        - |  3310 | `		/* Set a NULL value */` |
|      ! 0 |  3311 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3312 | `	}` |
|   862859 |  3313 | `LoadC_Done:` |
|        - |  3314 | `	/* Mark as constant */` |
|  1725764 |  3315 | `	pTos->nIdx = SXU32_HIGH;` |
|  1725764 |  3316 | `	break;` |
|        - |  3317 | `				  }` |
|        - |  3318 | `/*` |
|        - |  3319 | ` * LOAD: P1 * P3` |
|        - |  3320 | ` *` |
|        - |  3321 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3322 | ` * from the P3 operand.` |
|        - |  3323 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3324 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3325 | ` */` |
|  1394978 |  3326 | `case PH7_OP_LOAD:{` |
|        - |  3327 | `	ph7_value *pObj;` |
|        - |  3328 | `	SyString sName;` |
|  2790178 |  3329 | `	if( pInstr->p3 == 0 ){` |
|        - |  3330 | `		/* Take the variable name from the top of the stack */` |
|        - |  3331 | `#ifdef UNTRUST` |
|        - |  3332 | `		if( pTos < pStack ){` |
|        - |  3333 | `			goto Abort;` |
|        - |  3334 | `		}` |
|        - |  3335 | `#endif` |
|        - |  3336 | `		/* Force a string cast */` |
|       19 |  3337 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3338 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3339 | `		}` |
|       19 |  3340 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3341 | `	}else{` |
|  2790160 |  3342 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3343 | `		/* Reserve a room for the target object */` |
|  2790160 |  3344 | `		pTos++;` |
|        - |  3345 | `	}` |
|        - |  3346 | `	/* Extract the requested memory object */` |
|  2790178 |  3347 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2790178 |  3348 | `	if( pObj == 0 ){` |
|       28 |  3349 | `		if( pInstr->iP1 ){` |
|        - |  3350 | `			/* Variable not found,load NULL */` |
|       28 |  3351 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3352 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3353 | `			}else{` |
|       28 |  3354 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3355 | `			}` |
|       28 |  3356 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1394993 |  3357 | `			break;` |
|      ! 0 |  3358 | `		}else{` |
|        - |  3359 | `			/* Fatal error */` |
|      ! 0 |  3360 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3361 | `			goto Abort;` |
|        - |  3362 | `		}` |
|        - |  3363 | `	}` |
|        - |  3364 | `	/* Load variable contents */` |
|  2790152 |  3365 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2790152 |  3366 | `	pTos->nIdx = pObj->nIdx;` |
|  2790152 |  3367 | `	break;` |
|        - |  3368 | `				   }` |
|        - |  3369 | `/*` |
|        - |  3370 | ` * LOAD_MAP P1 * *` |
|        - |  3371 | ` *` |
|        - |  3372 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3373 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3374 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3375 | ` */` |
|    19433 |  3376 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3377 | `	ph7_hashmap *pMap;` |
|        - |  3378 | `	/* Allocate a new hashmap instance */` |
|    38868 |  3379 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    38868 |  3380 | `	if( pMap == 0 ){` |
|      ! 0 |  3381 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3382 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3383 | `		goto Abort;` |
|        - |  3384 | `	}` |
|    38868 |  3385 | `	if( pInstr->iP1 > 0 ){` |
|     2340 |  3386 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3387 | `		/* Perform the insertion */` |
|     7160 |  3388 | `		while( pEntry < pTos ){` |
|     4822 |  3389 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3390 | `				/* Insertion by reference */` |
|      142 |  3391 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3392 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3393 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3394 | `					);` |
|       48 |  3395 | `			}else{` |
|        - |  3396 | `				/* Standard insertion */` |
|     7091 |  3397 | `				PH7_HashmapInsert(pMap,` |
|     4726 |  3398 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2363 |  3399 | `					&pEntry[1]` |
|        - |  3400 | `				);` |
|        - |  3401 | `			}` |
|        - |  3402 | `			/* Next pair on the stack */` |
|     4822 |  3403 | `			pEntry += 2;` |
|        2 |  3404 | `		}` |
|        - |  3405 | `		/* Pop P1 elements */` |
|     2340 |  3406 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1169 |  3407 | `	}` |
|        - |  3408 | `	/* Push the hashmap */` |
|    38868 |  3409 | `	pTos++;` |
|    38868 |  3410 | `	pTos->nIdx = SXU32_HIGH;` |
|    38868 |  3411 | `	pTos->x.pOther = pMap;` |
|    38868 |  3412 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    38868 |  3413 | `	break;` |
|        - |  3414 | `					  }` |
|        - |  3415 | `/*` |
|        - |  3416 | ` * LOAD_LIST: P1 * *` |
|        - |  3417 | ` *` |
|        - |  3418 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3419 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3420 | ` * Caveats:` |
|        - |  3421 | ` *  This implementation support only a single nesting level.` |
|        - |  3422 | ` */` |
|       48 |  3423 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3424 | `	ph7_value *pEntry;` |
|       98 |  3425 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3426 | `		/* Empty list,break immediately */` |
|      ! 0 |  3427 | `		break;` |
|        - |  3428 | `	}` |
|       98 |  3429 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3430 | `#ifdef UNTRUST` |
|        - |  3431 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3432 | `		goto Abort;` |
|        - |  3433 | `	}` |
|        - |  3434 | `#endif` |
|       98 |  3435 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  3436 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3437 | `		ph7_hashmap_node *pNode;` |
|        - |  3438 | `		ph7_value sKey,*pObj;` |
|        - |  3439 | `		/* Start Copying */` |
|       91 |  3440 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  3441 | `		while( pEntry <= pTos ){` |
|      193 |  3442 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  3443 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  3444 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  3445 | `					if( rc == SXRET_OK ){` |
|        - |  3446 | `						/* Store node value */` |
|      165 |  3447 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  3448 | `					}else{` |
|        - |  3449 | `						/* Undefined array key */` |
|        - |  3450 | `						char zMsg[128];` |
|      ! 0 |  3451 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  3452 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3453 | `						PH7_MemObjRelease(pObj);` |
|        - |  3454 | `					}` |
|       82 |  3455 | `				}` |
|       82 |  3456 | `			}` |
|      193 |  3457 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  3458 | `			pEntry++;` |
|        1 |  3459 | `		}` |
|       46 |  3460 | `	}else{` |
|        - |  3461 | `		/* Source is not an array */` |
|        - |  3462 | `		ph7_value *pObj;` |
|       18 |  3463 | `		while( pEntry <= pTos ){` |
|       12 |  3464 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  3465 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  3466 | `					PH7_MemObjRelease(pObj);` |
|        5 |  3467 | `				}` |
|        5 |  3468 | `			}` |
|       12 |  3469 | `			pEntry++;` |
|        2 |  3470 | `		}` |
|        8 |  3471 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  3472 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  3473 | `			const char *zType = "unknown";` |
|        3 |  3474 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  3475 | `			char zMsg[256];` |
|        3 |  3476 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  3477 | `				zType = "string";` |
|        1 |  3478 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  3479 | `				zType = "int";` |
|      ! 0 |  3480 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3481 | `				zType = "float";` |
|      ! 0 |  3482 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3483 | `				zType = "object";` |
|      ! 0 |  3484 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  3485 | `				zType = "resource";` |
|      ! 0 |  3486 | `			}` |
|        3 |  3487 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  3488 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  3489 | `		}` |
|        - |  3490 | `	}` |
|       98 |  3491 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  3492 | `	break;` |
|        - |  3493 | `					   }` |
|        - |  3494 | `/*` |
|        - |  3495 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3496 | ` *` |
|        - |  3497 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3498 | ` * from the stack.` |
|        - |  3499 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3500 | ` * instead.` |
|        - |  3501 | ` */` |
|   223608 |  3502 | `case PH7_OP_LOAD_IDX: {` |
|   447262 |  3503 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   447262 |  3504 | `	ph7_hashmap *pMap = 0;` |
|        - |  3505 | `	ph7_value *pIdx;` |
|   447262 |  3506 | `	pIdx = 0;` |
|   447262 |  3507 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3508 | `		if( !pInstr->iP2){` |
|        - |  3509 | `			/* No available index,load NULL */` |
|      ! 0 |  3510 | `			if( pTos >= pStack ){` |
|      ! 0 |  3511 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3512 | `			}else{` |
|        - |  3513 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3514 | `				pTos++;` |
|      ! 0 |  3515 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3516 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3517 | `			}` |
|        - |  3518 | `			/* Emit a notice */` |
|      ! 0 |  3519 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3520 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3521 | `			break;` |
|        - |  3522 | `		}` |
|      ! 0 |  3523 | `	}else{` |
|   447262 |  3524 | `		pIdx = pTos;` |
|   447262 |  3525 | `		pTos--;` |
|        - |  3526 | `	}` |
|   447262 |  3527 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3528 | `		/* String access */` |
|   350590 |  3529 | `		if( pIdx ){` |
|        - |  3530 | `			sxu32 nOfft;` |
|   350590 |  3531 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3532 | `				/* Force an int cast */` |
|      ! 0 |  3533 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3534 | `			}` |
|   350590 |  3535 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   350590 |  3536 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3537 | `				/* Invalid offset,load null */` |
|      ! 0 |  3538 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3539 | `			}else{` |
|   350590 |  3540 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   350590 |  3541 | `				int c = zData[nOfft];` |
|   350590 |  3542 | `				PH7_MemObjRelease(pTos);` |
|   350590 |  3543 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   350590 |  3544 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3545 | `			}` |
|   175318 |  3546 | `		}else{` |
|        - |  3547 | `			/* No available index,load NULL */` |
|      ! 0 |  3548 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3549 | `		}` |
|   350590 |  3550 | `		break;` |
|        - |  3551 | `	}` |
|    96674 |  3552 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3553 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3554 | `			ph7_value *pObj;` |
|        3 |  3555 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3556 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  3557 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  3558 | `			}` |
|        1 |  3559 | `		}` |
|        1 |  3560 | `	}` |
|    96674 |  3561 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    96674 |  3562 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    96674 |  3563 | `		if( pInstr->iP2 == 1 ){` |
|        - |  3564 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3565 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3566 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3567 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      881 |  3568 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      440 |  3569 | `		}` |
|        - |  3570 | `		/* Point to the hashmap */` |
|    96674 |  3571 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    96674 |  3572 | `		if( pIdx ){` |
|        - |  3573 | `			/* Load the desired entry */` |
|    96674 |  3574 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    48336 |  3575 | `		}` |
|    96674 |  3576 | `		if( pInstr->iP2 == 3 ){` |
|        - |  3577 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  3578 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  3579 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  3580 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  3581 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  3582 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  3583 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  3584 | `			 * correct for the outermost write. */` |
|       19 |  3585 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  3586 | `			if( !needWrite && pNode ){` |
|       13 |  3587 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  3588 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  3589 | `					needWrite = 1;` |
|        3 |  3590 | `				}` |
|        6 |  3591 | `			}` |
|       19 |  3592 | `			if( needWrite ){` |
|       13 |  3593 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  3594 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  3595 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  3596 | `					 * into the new map's storage. */` |
|        7 |  3597 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  3598 | `					if( pIdx ){` |
|        7 |  3599 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  3600 | `					}` |
|        3 |  3601 | `				}` |
|        6 |  3602 | `			}` |
|        9 |  3603 | `		}` |
|    96674 |  3604 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  3605 | `			/* Create a new empty entry */` |
|      273 |  3606 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  3607 | `			if( rc == SXRET_OK ){` |
|        - |  3608 | `				/* Point to the last inserted entry */` |
|      273 |  3609 | `				pNode = pMap->pLast;` |
|      136 |  3610 | `			}` |
|      136 |  3611 | `		}` |
|    48336 |  3612 | `	}` |
|    96674 |  3613 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  3614 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  3615 | `		char zMsg[128];` |
|      ! 0 |  3616 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3617 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3618 | `		}` |
|      ! 0 |  3619 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  3620 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3621 | `	}` |
|    96674 |  3622 | `	if( pIdx ){` |
|    96674 |  3623 | `		PH7_MemObjRelease(pIdx);` |
|    48336 |  3624 | `	}` |
|    96674 |  3625 | `	if( rc == SXRET_OK ){` |
|        - |  3626 | `		/* Load entry contents */` |
|    43906 |  3627 | `		if( pMap->iRef < 2 ){` |
|        - |  3628 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3629 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3630 | `			 */` |
|       24 |  3631 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3632 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3633 | `		}else{` |
|    43884 |  3634 | `			pTos->nIdx = pNode->nValIdx;` |
|    43884 |  3635 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    43884 |  3636 | `			PH7_HashmapUnref(pMap);` |
|        - |  3637 | `		}` |
|    21954 |  3638 | `	}else{` |
|        - |  3639 | `		/* No such entry,load NULL */` |
|    52770 |  3640 | `		PH7_MemObjRelease(pTos);` |
|    52770 |  3641 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3642 | `	}` |
|    96674 |  3643 | `	break;` |
|        - |  3644 | `					  }` |
|        - |  3645 | `/*` |
|        - |  3646 | ` * LOAD_CLOSURE * * P3` |
|        - |  3647 | ` *` |
|        - |  3648 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3649 | ` * name in the stack.` |
|        - |  3650 | ` */` |
|        5 |  3651 | `case PH7_OP_LOAD_CLOSURE:{` |
|       11 |  3652 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       11 |  3653 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3654 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3655 | `		ph7_vm_func *pClosure;` |
|        - |  3656 | `		char *zName;` |
|        - |  3657 | `		sxu32 mLen;` |
|        - |  3658 | `		sxu32 n;` |
|        - |  3659 | `		/* Create a new VM function */` |
|       11 |  3660 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3661 | `		/* Generate an unique closure name */` |
|       11 |  3662 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       11 |  3663 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3664 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3665 | `			goto Abort;` |
|        - |  3666 | `		}` |
|       11 |  3667 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       11 |  3668 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3669 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3670 | `		}` |
|        - |  3671 | `		/* Zero the stucture */` |
|       11 |  3672 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3673 | `		/* Perform a structure assignment on read-only items */` |
|       11 |  3674 | `		pClosure->aArgs = pFunc->aArgs;` |
|       11 |  3675 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       11 |  3676 | `		pClosure->aStatic = pFunc->aStatic;` |
|       11 |  3677 | `		pClosure->iFlags = pFunc->iFlags;` |
|       11 |  3678 | `		pClosure->pUserData = pFunc->pUserData;` |
|       11 |  3679 | `		pClosure->sSignature = pFunc->sSignature;` |
|       11 |  3680 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       11 |  3681 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       11 |  3682 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3683 | `		/* Register the closure */` |
|       11 |  3684 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3685 | `		/* Set up closure environment */` |
|       11 |  3686 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       11 |  3687 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       35 |  3688 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3689 | `			ph7_value *pValue;` |
|       25 |  3690 | `			pEnv = &aEnv[n];` |
|       25 |  3691 | `			sEnv.sName  = pEnv->sName;` |
|       25 |  3692 | `			sEnv.iFlags = pEnv->iFlags;` |
|       25 |  3693 | `			sEnv.nIdx = SXU32_HIGH;` |
|       25 |  3694 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       25 |  3695 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3696 | `				/* Pass by reference */` |
|      ! 0 |  3697 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3698 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3699 | `					);` |
|      ! 0 |  3700 | `			}` |
|        - |  3701 | `			/* Standard pass by value */` |
|       25 |  3702 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       25 |  3703 | `			if( pValue ){` |
|        - |  3704 | `				/* Copy imported value */` |
|       15 |  3705 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        7 |  3706 | `			}` |
|        - |  3707 | `			/* Insert the imported variable */` |
|       25 |  3708 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       13 |  3709 | `		}` |
|        - |  3710 | `		/* Finally,load the closure name on the stack */` |
|       11 |  3711 | `		pTos++;` |
|       11 |  3712 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        5 |  3713 | `	}` |
|       11 |  3714 | `	break;` |
|        - |  3715 | `						 }` |
|        - |  3716 | `/*` |
|        - |  3717 | ` * STORE * P2 P3` |
|        - |  3718 | ` *` |
|        - |  3719 | ` * Perform a store (Assignment) operation.` |
|        - |  3720 | ` */` |
|   119616 |  3721 | `case PH7_OP_STORE: {` |
|        - |  3722 | `	ph7_value *pObj;` |
|        - |  3723 | `	SyString sName;` |
|        - |  3724 | `#ifdef UNTRUST` |
|        - |  3725 | `	if( pTos < pStack ){` |
|        - |  3726 | `		goto Abort;` |
|        - |  3727 | `	}` |
|        - |  3728 | `#endif` |
|   239234 |  3729 | `	if( pInstr->iP2 ){` |
|        - |  3730 | `		sxu32 nIdx;` |
|        - |  3731 | `		/* Member store operation */` |
|     3178 |  3732 | `		nIdx = pTos->nIdx;` |
|     3178 |  3733 | `		VmPopOperand(&pTos,1);` |
|     3178 |  3734 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3735 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3736 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3737 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3738 | `		}else{` |
|        - |  3739 | `			/* Point to the desired memory object */` |
|     3174 |  3740 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3174 |  3741 | `			if( pObj ){` |
|        - |  3742 | `				/* Perform the store operation */` |
|     3174 |  3743 | `				PH7_MemObjStore(pTos,pObj);` |
|     1586 |  3744 | `			}` |
|        - |  3745 | `		}` |
|   121206 |  3746 | `		break;` |
|   236058 |  3747 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3748 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3749 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3750 | `			/* Force a string cast */` |
|      ! 0 |  3751 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3752 | `		}` |
|        7 |  3753 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3754 | `		pTos--;` |
|        - |  3755 | `#ifdef UNTRUST` |
|        - |  3756 | `		if( pTos < pStack  ){` |
|        - |  3757 | `			goto Abort;` |
|        - |  3758 | `		}` |
|        - |  3759 | `#endif` |
|        4 |  3760 | `	}else{` |
|   236052 |  3761 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3762 | `	}` |
|        - |  3763 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   236058 |  3764 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   236058 |  3765 | `	if( pObj == 0 ){` |
|      ! 0 |  3766 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3767 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3768 | `		goto Abort;` |
|        - |  3769 | `	}` |
|   236058 |  3770 | `	if( !pInstr->p3 ){` |
|        7 |  3771 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3772 | `	}` |
|        - |  3773 | `	/* Perform the store operation */` |
|   236058 |  3774 | `	PH7_MemObjStore(pTos,pObj);` |
|   236058 |  3775 | `	break;` |
|        - |  3776 | `				   }` |
|        - |  3777 | `/*` |
|        - |  3778 | ` * STORE_IDX:   P1 * P3` |
|        - |  3779 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3780 | ` *` |
|        - |  3781 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3782 | ` */` |
|    85921 |  3783 | `case PH7_OP_STORE_IDX:` |
|        - |  3784 | `case PH7_OP_STORE_IDX_REF: {` |
|   171844 |  3785 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3786 | `	ph7_value *pKey;` |
|        - |  3787 | `	sxu32 nIdx;` |
|   171844 |  3788 | `	if( pInstr->iP1 ){` |
|        - |  3789 | `		/* Key is next on stack */` |
|    58868 |  3790 | `		pKey = pTos;` |
|    58868 |  3791 | `		pTos--;` |
|    29435 |  3792 | `	}else{` |
|   112978 |  3793 | `		pKey = 0;` |
|        - |  3794 | `	}` |
|   171844 |  3795 | `	nIdx = pTos->nIdx;` |
|   171844 |  3796 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3797 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3798 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3799 | `		 * checking true sharing count, then re-add after separation. */` |
|   171792 |  3800 | `		if( nIdx != SXU32_HIGH ){` |
|   171792 |  3801 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   257687 |  3802 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   171792 |  3803 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3804 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3805 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3806 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3807 | `				 * refcounts if the backing array was already separated. */` |
|   171792 |  3808 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   171792 |  3809 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   171792 |  3810 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   171792 |  3811 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   171792 |  3812 | `					pTos->x.pOther = pMap;` |
|    85897 |  3813 | `				}else{` |
|        - |  3814 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3815 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3816 | `					pMap = pCur;` |
|        - |  3817 | `				}` |
|    85897 |  3818 | `			}else{` |
|      ! 0 |  3819 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3820 | `			}` |
|    85897 |  3821 | `		}else{` |
|      ! 0 |  3822 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3823 | `		}` |
|   171792 |  3824 | `		if( pMap->iRef < 2 ){` |
|        - |  3825 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3826 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3827 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3828 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3829 | `			pMap->iRef = 2;` |
|      ! 0 |  3830 | `		}` |
|    85897 |  3831 | `	}else{` |
|        - |  3832 | `		ph7_value *pObj;` |
|       53 |  3833 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3834 | `		if( pObj == 0 ){` |
|      ! 0 |  3835 | `			if( pKey ){` |
|      ! 0 |  3836 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3837 | `			}` |
|      ! 0 |  3838 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3839 | `			break;` |
|        - |  3840 | `		}` |
|        - |  3841 | `		/* Phase#1: Load the array */` |
|       53 |  3842 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3843 | `			VmPopOperand(&pTos,1);` |
|       53 |  3844 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3845 | `				/* Force a string cast */` |
|      ! 0 |  3846 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3847 | `			}` |
|       53 |  3848 | `			if( pKey == 0 ){` |
|        - |  3849 | `				/* Append string */` |
|        3 |  3850 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3851 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3852 | `				}` |
|        2 |  3853 | `			}else{` |
|        - |  3854 | `				sxu32 nOfft;` |
|       51 |  3855 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3856 | `					/* Force an int cast */` |
|       51 |  3857 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3858 | `				}` |
|       51 |  3859 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3860 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3861 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3862 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3863 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3864 | `				}else{` |
|      ! 0 |  3865 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3866 | `						/* Perform an append operation */` |
|      ! 0 |  3867 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3868 | `					}` |
|        - |  3869 | `				}` |
|        - |  3870 | `			}` |
|       53 |  3871 | `			if( pKey ){` |
|       51 |  3872 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3873 | `			}` |
|       53 |  3874 | `			break;` |
|      ! 0 |  3875 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3876 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3877 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3878 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3879 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3880 | `				goto Abort;` |
|        - |  3881 | `			}` |
|      ! 0 |  3882 | `		}` |
|        - |  3883 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3884 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3885 | `	}` |
|   171792 |  3886 | `	VmPopOperand(&pTos,1);` |
|        - |  3887 | `	/* Phase#2: Perform the insertion */` |
|   171792 |  3888 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3889 | `		/* Insertion by reference */` |
|       15 |  3890 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3891 | `	}else{` |
|   171778 |  3892 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3893 | `	}` |
|   171792 |  3894 | `	if( pKey ){` |
|    58818 |  3895 | `		PH7_MemObjRelease(pKey);` |
|    29408 |  3896 | `	}` |
|   171792 |  3897 | `	break;` |
|        - |  3898 | `					   }` |
|        - |  3899 | `/*` |
|        - |  3900 | ` * INCR: P1 * *` |
|        - |  3901 | ` *` |
|        - |  3902 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3903 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3904 | ` * the stack and increment after that.` |
|        - |  3905 | ` */` |
|   155066 |  3906 | `case PH7_OP_INCR:` |
|        - |  3907 | `#ifdef UNTRUST` |
|        - |  3908 | `	if( pTos < pStack ){` |
|        - |  3909 | `		goto Abort;` |
|        - |  3910 | `	}` |
|        - |  3911 | `#endif` |
|   310178 |  3912 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   310178 |  3913 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3914 | `			ph7_value *pObj;` |
|   310178 |  3915 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3916 | `				/* Force a numeric cast */` |
|   310178 |  3917 | `				PH7_MemObjToNumeric(pObj);` |
|   310178 |  3918 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3919 | `					pObj->rVal++;` |
|        - |  3920 | `					/* Try to get an integer representation */` |
|      ! 0 |  3921 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3922 | `				}else{` |
|   310178 |  3923 | `					pObj->x.iVal++;` |
|   310178 |  3924 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3925 | `				}` |
|   310178 |  3926 | `				if( pInstr->iP1 ){` |
|        - |  3927 | `					/* Pre-icrement */` |
|       71 |  3928 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3929 | `				}` |
|   155110 |  3930 | `			}` |
|   155112 |  3931 | `		}else{` |
|      ! 0 |  3932 | `			if( pInstr->iP1 ){` |
|        - |  3933 | `				/* Force a numeric cast */` |
|      ! 0 |  3934 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3935 | `				/* Pre-increment */` |
|      ! 0 |  3936 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3937 | `					pTos->rVal++;` |
|        - |  3938 | `					/* Try to get an integer representation */` |
|      ! 0 |  3939 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3940 | `				}else{` |
|      ! 0 |  3941 | `					pTos->x.iVal++;` |
|      ! 0 |  3942 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3943 | `				}` |
|      ! 0 |  3944 | `			}` |
|        - |  3945 | `		}` |
|   155110 |  3946 | `	}` |
|   310178 |  3947 | `	break;` |
|        - |  3948 | `/*` |
|        - |  3949 | ` * DECR: P1 * *` |
|        - |  3950 | ` *` |
|        - |  3951 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3952 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3953 | ` * and decrement after that.` |
|        - |  3954 | ` */` |
|        2 |  3955 | `case PH7_OP_DECR:` |
|        - |  3956 | `#ifdef UNTRUST` |
|        - |  3957 | `	if( pTos < pStack ){` |
|        - |  3958 | `		goto Abort;` |
|        - |  3959 | `	}` |
|        - |  3960 | `#endif` |
|        5 |  3961 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3962 | `		/* Force a numeric cast */` |
|        5 |  3963 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3964 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3965 | `			ph7_value *pObj;` |
|        5 |  3966 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3967 | `				/* Force a numeric cast */` |
|        5 |  3968 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3969 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3970 | `					pObj->rVal--;` |
|        - |  3971 | `					/* Try to get an integer representation */` |
|      ! 0 |  3972 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3973 | `				}else{` |
|        5 |  3974 | `					pObj->x.iVal--;` |
|        5 |  3975 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3976 | `				}` |
|        5 |  3977 | `				if( pInstr->iP1 ){` |
|        - |  3978 | `					/* Pre-icrement */` |
|      ! 0 |  3979 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3980 | `				}` |
|        2 |  3981 | `			}` |
|        3 |  3982 | `		}else{` |
|      ! 0 |  3983 | `			if( pInstr->iP1 ){` |
|        - |  3984 | `				/* Pre-increment */` |
|      ! 0 |  3985 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3986 | `					pTos->rVal--;` |
|        - |  3987 | `					/* Try to get an integer representation */` |
|      ! 0 |  3988 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3989 | `				}else{` |
|      ! 0 |  3990 | `					pTos->x.iVal--;` |
|      ! 0 |  3991 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3992 | `				}` |
|      ! 0 |  3993 | `			}` |
|        - |  3994 | `		}` |
|        2 |  3995 | `	}` |
|        5 |  3996 | `	break;` |
|        - |  3997 | `/*` |
|        - |  3998 | ` * UMINUS: * * *` |
|        - |  3999 | ` *` |
|        - |  4000 | ` * Perform a unary minus operation.` |
|        - |  4001 | ` */` |
|    25175 |  4002 | `case PH7_OP_UMINUS:` |
|        - |  4003 | `#ifdef UNTRUST` |
|        - |  4004 | `	if( pTos < pStack ){` |
|        - |  4005 | `		goto Abort;` |
|        - |  4006 | `	}` |
|        - |  4007 | `#endif` |
|        - |  4008 | `	/* Force a numeric (integer,real or both) cast */` |
|    50352 |  4009 | `	PH7_MemObjToNumeric(pTos);` |
|    50352 |  4010 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  4011 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  4012 | `	}` |
|    50352 |  4013 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    50322 |  4014 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    25160 |  4015 | `	}` |
|    50352 |  4016 | `	break;` |
|        - |  4017 | `/*` |
|        - |  4018 | ` * UPLUS: * * *` |
|        - |  4019 | ` *` |
|        - |  4020 | ` * Perform a unary plus operation.` |
|        - |  4021 | ` */` |
|       18 |  4022 | `case PH7_OP_UPLUS:` |
|        - |  4023 | `#ifdef UNTRUST` |
|        - |  4024 | `	if( pTos < pStack ){` |
|        - |  4025 | `		goto Abort;` |
|        - |  4026 | `	}` |
|        - |  4027 | `#endif` |
|        - |  4028 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  4029 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  4030 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4031 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  4032 | `	}` |
|       37 |  4033 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  4034 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  4035 | `	}` |
|       37 |  4036 | `	break;` |
|        - |  4037 | `/*` |
|        - |  4038 | ` * OP_LNOT: * * *` |
|        - |  4039 | ` *` |
|        - |  4040 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  4041 | ` * with its complement.` |
|        - |  4042 | ` */` |
|    41024 |  4043 | `case PH7_OP_LNOT:` |
|        - |  4044 | `#ifdef UNTRUST` |
|        - |  4045 | `	if( pTos < pStack ){` |
|        - |  4046 | `		goto Abort;` |
|        - |  4047 | `	}` |
|        - |  4048 | `#endif` |
|        - |  4049 | `	/* Force a boolean cast */` |
|    82094 |  4050 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  4051 | `		PH7_MemObjToBool(pTos);` |
|       10 |  4052 | `	}` |
|    82094 |  4053 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    82094 |  4054 | `	break;` |
|        - |  4055 | `/*` |
|        - |  4056 | ` * OP_BITNOT: * * *` |
|        - |  4057 | ` *` |
|        - |  4058 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  4059 | ` * with its ones-complement.` |
|        - |  4060 | ` */` |
|       13 |  4061 | `case PH7_OP_BITNOT:` |
|        - |  4062 | `#ifdef UNTRUST` |
|        - |  4063 | `	if( pTos < pStack ){` |
|        - |  4064 | `		goto Abort;` |
|        - |  4065 | `	}` |
|        - |  4066 | `#endif` |
|        - |  4067 | `	/* Force an integer cast */` |
|       28 |  4068 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4069 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4070 | `	}` |
|       28 |  4071 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  4072 | `	break;` |
|        - |  4073 | `/* OP_MUL * * *` |
|        - |  4074 | ` * OP_MUL_STORE * * *` |
|        - |  4075 | ` *` |
|        - |  4076 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  4077 | ` * and push the result back onto the stack.` |
|        - |  4078 | ` */` |
|     1255 |  4079 | `case PH7_OP_MUL:` |
|        - |  4080 | `case PH7_OP_MUL_STORE: {` |
|     2512 |  4081 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4082 | `	/* Force the operand to be numeric */` |
|        - |  4083 | `#ifdef UNTRUST` |
|        - |  4084 | `	if( pNos < pStack ){` |
|        - |  4085 | `		goto Abort;` |
|        - |  4086 | `	}` |
|        - |  4087 | `#endif` |
|     2512 |  4088 | `	PH7_MemObjToNumeric(pTos);` |
|     2512 |  4089 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  4090 | `	/* Perform the requested operation */` |
|     2512 |  4091 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4092 | `		/* Floating point arithemic */` |
|        - |  4093 | `		ph7_real a,b,r;` |
|       17 |  4094 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4095 | `			PH7_MemObjToReal(pTos);` |
|        3 |  4096 | `		}` |
|       17 |  4097 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4098 | `			PH7_MemObjToReal(pNos);` |
|        3 |  4099 | `		}` |
|       17 |  4100 | `		a = pNos->rVal;` |
|       17 |  4101 | `		b = pTos->rVal;` |
|       17 |  4102 | `		r = a * b;` |
|        - |  4103 | `		/* Push the result */` |
|       17 |  4104 | `		pNos->rVal = r;` |
|       17 |  4105 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4106 | `		/* Try to get an integer representation */` |
|       17 |  4107 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  4108 | `	}else{` |
|        - |  4109 | `		/* Integer arithmetic */` |
|        - |  4110 | `		sxi64 a,b,r;` |
|     2496 |  4111 | `		a = pNos->x.iVal;` |
|     2496 |  4112 | `		b = pTos->x.iVal;` |
|     2496 |  4113 | `		r = a * b;` |
|        - |  4114 | `		/* Push the result */` |
|     2496 |  4115 | `		pNos->x.iVal = r;` |
|     2496 |  4116 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4117 | `	}` |
|     2512 |  4118 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4119 | `		ph7_value *pObj;` |
|       29 |  4120 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4121 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       29 |  4122 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       29 |  4123 | `			PH7_MemObjStore(pNos,pObj);` |
|       14 |  4124 | `		}` |
|       14 |  4125 | `	}` |
|     2512 |  4126 | `	VmPopOperand(&pTos,1);` |
|     2512 |  4127 | `	break;` |
|        - |  4128 | `				 }` |
|        - |  4129 | `/* OP_ADD * * *` |
|        - |  4130 | ` *` |
|        - |  4131 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4132 | ` * and push the result back onto the stack.` |
|        - |  4133 | ` */` |
|      464 |  4134 | `case PH7_OP_ADD:{` |
|      930 |  4135 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4136 | `#ifdef UNTRUST` |
|        - |  4137 | `	if( pNos < pStack ){` |
|        - |  4138 | `		goto Abort;` |
|        - |  4139 | `	}` |
|        - |  4140 | `#endif` |
|        - |  4141 | `	/* Perform the addition */` |
|      930 |  4142 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      930 |  4143 | `	VmPopOperand(&pTos,1);` |
|      930 |  4144 | `	break;` |
|        - |  4145 | `				}` |
|        - |  4146 | `/*` |
|        - |  4147 | ` * OP_ADD_STORE * * *` |
|        - |  4148 | ` *` |
|        - |  4149 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4150 | ` * and push the result back onto the stack.` |
|        - |  4151 | ` */` |
|      496 |  4152 | `case PH7_OP_ADD_STORE:{` |
|      994 |  4153 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4154 | `	ph7_value *pObj;` |
|        - |  4155 | `	sxu32 nIdx;` |
|        - |  4156 | `#ifdef UNTRUST` |
|        - |  4157 | `	if( pNos < pStack ){` |
|        - |  4158 | `		goto Abort;` |
|        - |  4159 | `	}` |
|        - |  4160 | `#endif` |
|        - |  4161 | `	/* Perform the addition */` |
|      994 |  4162 | `	nIdx = pTos->nIdx;` |
|      994 |  4163 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4164 | `	/* Peform the store operation */` |
|      994 |  4165 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4166 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      994 |  4167 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      994 |  4168 | `		PH7_MemObjStore(pTos,pObj);` |
|      496 |  4169 | `	}` |
|        - |  4170 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      994 |  4171 | `	PH7_MemObjStore(pTos,pNos);` |
|      994 |  4172 | `	VmPopOperand(&pTos,1);` |
|      994 |  4173 | `	break;` |
|        - |  4174 | `				}` |
|        - |  4175 | `/* OP_SUB * * *` |
|        - |  4176 | ` *` |
|        - |  4177 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4178 | ` * first (what was next on the stack) from the second (the` |
|        - |  4179 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4180 | ` */` |
|      302 |  4181 | `case PH7_OP_SUB: {` |
|      606 |  4182 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4183 | `#ifdef UNTRUST` |
|        - |  4184 | `	if( pNos < pStack ){` |
|        - |  4185 | `		goto Abort;` |
|        - |  4186 | `	}` |
|        - |  4187 | `#endif` |
|      606 |  4188 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4189 | `		/* Floating point arithemic */` |
|        - |  4190 | `		ph7_real a,b,r;` |
|       95 |  4191 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4192 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4193 | `		}` |
|       95 |  4194 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4195 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4196 | `		}` |
|       95 |  4197 | `		a = pNos->rVal;` |
|       95 |  4198 | `		b = pTos->rVal;` |
|       95 |  4199 | `		r = a - b;` |
|        - |  4200 | `		/* Push the result */` |
|       95 |  4201 | `		pNos->rVal = r;` |
|       95 |  4202 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4203 | `		/* Try to get an integer representation */` |
|       95 |  4204 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4205 | `	}else{` |
|        - |  4206 | `		/* Integer arithmetic */` |
|        - |  4207 | `		sxi64 a,b,r;` |
|      512 |  4208 | `		a = pNos->x.iVal;` |
|      512 |  4209 | `		b = pTos->x.iVal;` |
|      512 |  4210 | `		r = a - b;` |
|        - |  4211 | `		/* Push the result */` |
|      512 |  4212 | `		pNos->x.iVal = r;` |
|      512 |  4213 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4214 | `	}` |
|      606 |  4215 | `	VmPopOperand(&pTos,1);` |
|      606 |  4216 | `	break;` |
|        - |  4217 | `				 }` |
|        - |  4218 | `/* OP_SUB_STORE * * *` |
|        - |  4219 | ` *` |
|        - |  4220 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4221 | ` * first (what was next on the stack) from the second (the` |
|        - |  4222 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4223 | ` */` |
|        3 |  4224 | `case PH7_OP_SUB_STORE: {` |
|        7 |  4225 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4226 | `	ph7_value *pObj;` |
|        - |  4227 | `#ifdef UNTRUST` |
|        - |  4228 | `	if( pNos < pStack ){` |
|        - |  4229 | `		goto Abort;` |
|        - |  4230 | `	}` |
|        - |  4231 | `#endif` |
|        7 |  4232 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4233 | `		/* Floating point arithemic */` |
|        - |  4234 | `		ph7_real a,b,r;` |
|      ! 0 |  4235 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4236 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4237 | `		}` |
|      ! 0 |  4238 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4239 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4240 | `		}` |
|      ! 0 |  4241 | `		a = pTos->rVal;` |
|      ! 0 |  4242 | `		b = pNos->rVal;` |
|      ! 0 |  4243 | `		r = a - b;` |
|        - |  4244 | `		/* Push the result */` |
|      ! 0 |  4245 | `		pNos->rVal = r;` |
|      ! 0 |  4246 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4247 | `		/* Try to get an integer representation */` |
|      ! 0 |  4248 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4249 | `	}else{` |
|        - |  4250 | `		/* Integer arithmetic */` |
|        - |  4251 | `		sxi64 a,b,r;` |
|        7 |  4252 | `		a = pTos->x.iVal;` |
|        7 |  4253 | `		b = pNos->x.iVal;` |
|        7 |  4254 | `		r = a - b;` |
|        - |  4255 | `		/* Push the result */` |
|        7 |  4256 | `		pNos->x.iVal = r;` |
|        7 |  4257 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4258 | `	}` |
|        7 |  4259 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4260 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        7 |  4261 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        7 |  4262 | `		PH7_MemObjStore(pNos,pObj);` |
|        3 |  4263 | `	}` |
|        7 |  4264 | `	VmPopOperand(&pTos,1);` |
|        7 |  4265 | `	break;` |
|        - |  4266 | `				 }` |
|        - |  4267 |  |
|        - |  4268 | `/*` |
|        - |  4269 | ` * OP_MOD * * *` |
|        - |  4270 | ` *` |
|        - |  4271 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4272 | ` * first (what was next on the stack) from the second (the` |
|        - |  4273 | ` * top of the stack) and push the remainder after division` |
|        - |  4274 | ` * onto the stack.` |
|        - |  4275 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4276 | ` */` |
|      307 |  4277 | `case PH7_OP_MOD:{` |
|      616 |  4278 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4279 | `	sxi64 a,b,r;` |
|        - |  4280 | `#ifdef UNTRUST` |
|        - |  4281 | `	if( pNos < pStack ){` |
|        - |  4282 | `		goto Abort;` |
|        - |  4283 | `	}` |
|        - |  4284 | `#endif` |
|        - |  4285 | `	/* Force the operands to be integer */` |
|      616 |  4286 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4287 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4288 | `	}` |
|      616 |  4289 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4290 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4291 | `	}` |
|        - |  4292 | `	/* Perform the requested operation */` |
|      616 |  4293 | `	a = pNos->x.iVal;` |
|      616 |  4294 | `	b = pTos->x.iVal;` |
|      616 |  4295 | `	if( b == 0 ){` |
|        3 |  4296 | `		r = 0;` |
|        3 |  4297 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4298 | `		/* goto Abort; */` |
|        2 |  4299 | `	}else{` |
|      613 |  4300 | `		r = a%b;` |
|        - |  4301 | `	}` |
|        - |  4302 | `	/* Push the result */` |
|      616 |  4303 | `	pNos->x.iVal = r;` |
|      616 |  4304 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      616 |  4305 | `	VmPopOperand(&pTos,1);` |
|      616 |  4306 | `	break;` |
|        - |  4307 | `				}` |
|        - |  4308 | `/*` |
|        - |  4309 | ` * OP_MOD_STORE * * *` |
|        - |  4310 | ` *` |
|        - |  4311 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4312 | ` * first (what was next on the stack) from the second (the` |
|        - |  4313 | ` * top of the stack) and push the remainder after division` |
|        - |  4314 | ` * onto the stack.` |
|        - |  4315 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4316 | ` */` |
|        1 |  4317 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4318 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4319 | `	ph7_value *pObj;` |
|        - |  4320 | `	sxi64 a,b,r;` |
|        - |  4321 | `#ifdef UNTRUST` |
|        - |  4322 | `	if( pNos < pStack ){` |
|        - |  4323 | `		goto Abort;` |
|        - |  4324 | `	}` |
|        - |  4325 | `#endif` |
|        - |  4326 | `	/* Force the operands to be integer */` |
|        3 |  4327 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4328 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4329 | `	}` |
|        3 |  4330 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4331 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4332 | `	}` |
|        - |  4333 | `	/* Perform the requested operation */` |
|        3 |  4334 | `	a = pTos->x.iVal;` |
|        3 |  4335 | `	b = pNos->x.iVal;` |
|        3 |  4336 | `	if( b == 0 ){` |
|      ! 0 |  4337 | `		r = 0;` |
|      ! 0 |  4338 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4339 | `		/* goto Abort; */` |
|      ! 0 |  4340 | `	}else{` |
|        3 |  4341 | `		r = a%b;` |
|        - |  4342 | `	}` |
|        - |  4343 | `	/* Push the result */` |
|        3 |  4344 | `	pNos->x.iVal = r;` |
|        3 |  4345 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4346 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4347 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4348 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4349 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4350 | `	}` |
|        3 |  4351 | `	VmPopOperand(&pTos,1);` |
|        3 |  4352 | `	break;` |
|        - |  4353 | `				}` |
|        - |  4354 | `/*` |
|        - |  4355 | ` * OP_DIV * * *` |
|        - |  4356 | ` *` |
|        - |  4357 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4358 | ` * first (what was next on the stack) from the second (the` |
|        - |  4359 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4360 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4361 | ` */` |
|       30 |  4362 | `case PH7_OP_DIV:{` |
|       62 |  4363 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4364 | `	ph7_real a,b,r;` |
|        - |  4365 | `#ifdef UNTRUST` |
|        - |  4366 | `	if( pNos < pStack ){` |
|        - |  4367 | `		goto Abort;` |
|        - |  4368 | `	}` |
|        - |  4369 | `#endif` |
|        - |  4370 | `	/* Force the operands to be real */` |
|       62 |  4371 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       58 |  4372 | `		PH7_MemObjToReal(pTos);` |
|       28 |  4373 | `	}` |
|       62 |  4374 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       24 |  4375 | `		PH7_MemObjToReal(pNos);` |
|       11 |  4376 | `	}` |
|        - |  4377 | `	/* Perform the requested operation */` |
|       62 |  4378 | `	a = pNos->rVal;` |
|       62 |  4379 | `	b = pTos->rVal;` |
|       62 |  4380 | `	if( b == 0 ){` |
|        - |  4381 | `		/* Division by zero */` |
|        3 |  4382 | `		pNos->rVal = 0;` |
|        3 |  4383 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4384 | `		/* goto Abort; */` |
|        2 |  4385 | `	}else{` |
|       59 |  4386 | `		r = a/b;` |
|        - |  4387 | `		/* Push the result */` |
|       59 |  4388 | `		pNos->rVal = r;` |
|       59 |  4389 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4390 | `		/* Try to get an integer representation */` |
|       59 |  4391 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4392 | `	}` |
|       62 |  4393 | `	VmPopOperand(&pTos,1);` |
|       62 |  4394 | `	break;` |
|        - |  4395 | `				}` |
|        - |  4396 | `/*` |
|        - |  4397 | ` * OP_DIV_STORE * * *` |
|        - |  4398 | ` *` |
|        - |  4399 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4400 | ` * first (what was next on the stack) from the second (the` |
|        - |  4401 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4402 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4403 | ` */` |
|        2 |  4404 | `case PH7_OP_DIV_STORE:{` |
|        5 |  4405 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4406 | `	ph7_value *pObj;` |
|        - |  4407 | `	ph7_real a,b,r;` |
|        - |  4408 | `#ifdef UNTRUST` |
|        - |  4409 | `	if( pNos < pStack ){` |
|        - |  4410 | `		goto Abort;` |
|        - |  4411 | `	}` |
|        - |  4412 | `#endif` |
|        - |  4413 | `	/* Force the operands to be real */` |
|        5 |  4414 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4415 | `		PH7_MemObjToReal(pTos);` |
|        2 |  4416 | `	}` |
|        5 |  4417 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4418 | `		PH7_MemObjToReal(pNos);` |
|        2 |  4419 | `	}` |
|        - |  4420 | `	/* Perform the requested operation */` |
|        5 |  4421 | `	a = pTos->rVal;` |
|        5 |  4422 | `	b = pNos->rVal;` |
|        5 |  4423 | `	if( b == 0 ){` |
|        - |  4424 | `		/* Division by zero */` |
|      ! 0 |  4425 | `		r = 0;` |
|      ! 0 |  4426 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4427 | `		/* goto Abort; */` |
|      ! 0 |  4428 | `	}else{` |
|        5 |  4429 | `		r = a/b;` |
|        - |  4430 | `		/* Push the result */` |
|        5 |  4431 | `		pNos->rVal = r;` |
|        5 |  4432 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4433 | `		/* Try to get an integer representation */` |
|        5 |  4434 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4435 | `	}` |
|        5 |  4436 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4437 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  4438 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  4439 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  4440 | `	}` |
|        5 |  4441 | `	VmPopOperand(&pTos,1);` |
|        5 |  4442 | `	break;` |
|        - |  4443 | `				}` |
|        - |  4444 | `/* OP_BAND * * *` |
|        - |  4445 | ` *` |
|        - |  4446 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4447 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4448 | ` * two elements.` |
|        - |  4449 | `*/` |
|        - |  4450 | `/* OP_BOR * * *` |
|        - |  4451 | ` *` |
|        - |  4452 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4453 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4454 | ` * two elements.` |
|        - |  4455 | ` */` |
|        - |  4456 | `/* OP_BXOR * * *` |
|        - |  4457 | ` *` |
|        - |  4458 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4459 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4460 | ` * two elements.` |
|        - |  4461 | ` */` |
|       44 |  4462 | `case PH7_OP_BAND:` |
|        - |  4463 | `case PH7_OP_BOR:` |
|        - |  4464 | `case PH7_OP_BXOR:{` |
|       90 |  4465 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4466 | `	sxi64 a,b,r;` |
|        - |  4467 | `#ifdef UNTRUST` |
|        - |  4468 | `	if( pNos < pStack ){` |
|        - |  4469 | `		goto Abort;` |
|        - |  4470 | `	}` |
|        - |  4471 | `#endif` |
|        - |  4472 | `	/* Force the operands to be integer */` |
|       90 |  4473 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4474 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4475 | `	}` |
|       90 |  4476 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4477 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4478 | `	}` |
|        - |  4479 | `	/* Perform the requested operation */` |
|       90 |  4480 | `	a = pNos->x.iVal;` |
|       90 |  4481 | `	b = pTos->x.iVal;` |
|       90 |  4482 | `	switch(pInstr->iOp){` |
|        7 |  4483 | `	case PH7_OP_BOR_STORE:` |
|       15 |  4484 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  4485 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  4486 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  4487 | `	case PH7_OP_BAND_STORE:` |
|       30 |  4488 | `	case PH7_OP_BAND:` |
|       62 |  4489 | `	default:          r = a&b; break;` |
|        - |  4490 | `	}` |
|        - |  4491 | `	/* Push the result */` |
|       90 |  4492 | `	pNos->x.iVal = r;` |
|       90 |  4493 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  4494 | `	VmPopOperand(&pTos,1);` |
|       90 |  4495 | `	break;` |
|        - |  4496 | `				 }` |
|        - |  4497 | `/* OP_BAND_STORE * * *` |
|        - |  4498 | ` *` |
|        - |  4499 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4500 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4501 | ` * two elements.` |
|        - |  4502 | `*/` |
|        - |  4503 | `/* OP_BOR_STORE * * *` |
|        - |  4504 | ` *` |
|        - |  4505 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4506 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4507 | ` * two elements.` |
|        - |  4508 | ` */` |
|        - |  4509 | `/* OP_BXOR_STORE * * *` |
|        - |  4510 | ` *` |
|        - |  4511 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4512 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4513 | ` * two elements.` |
|        - |  4514 | ` */` |
|       10 |  4515 | `case PH7_OP_BAND_STORE:` |
|        - |  4516 | `case PH7_OP_BOR_STORE:` |
|        - |  4517 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  4518 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4519 | `	ph7_value *pObj;` |
|        - |  4520 | `	sxi64 a,b,r;` |
|        - |  4521 | `#ifdef UNTRUST` |
|        - |  4522 | `	if( pNos < pStack ){` |
|        - |  4523 | `		goto Abort;` |
|        - |  4524 | `	}` |
|        - |  4525 | `#endif` |
|        - |  4526 | `	/* Force the operands to be integer */` |
|       21 |  4527 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4528 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4529 | `	}` |
|       21 |  4530 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4531 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4532 | `	}` |
|        - |  4533 | `	/* Perform the requested operation */` |
|       21 |  4534 | `	a = pTos->x.iVal;` |
|       21 |  4535 | `	b = pNos->x.iVal;` |
|       21 |  4536 | `	switch(pInstr->iOp){` |
|        3 |  4537 | `	case PH7_OP_BOR_STORE:` |
|        7 |  4538 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  4539 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  4540 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  4541 | `	case PH7_OP_BAND_STORE:` |
|        3 |  4542 | `	case PH7_OP_BAND:` |
|        7 |  4543 | `	default:          r = a&b; break;` |
|        - |  4544 | `	}` |
|        - |  4545 | `	/* Push the result */` |
|       21 |  4546 | `	pNos->x.iVal = r;` |
|       21 |  4547 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  4548 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4549 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  4550 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  4551 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  4552 | `	}` |
|       21 |  4553 | `	VmPopOperand(&pTos,1);` |
|       21 |  4554 | `	break;` |
|        - |  4555 | `				 }` |
|        - |  4556 | `/* OP_SHL * * *` |
|        - |  4557 | ` *` |
|        - |  4558 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4559 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4560 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4561 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4562 | ` */` |
|        - |  4563 | `/* OP_SHR * * *` |
|        - |  4564 | ` *` |
|        - |  4565 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4566 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4567 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4568 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4569 | ` */` |
|       12 |  4570 | `case PH7_OP_SHL:` |
|        - |  4571 | `case PH7_OP_SHR: {` |
|       25 |  4572 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4573 | `	sxi64 a,r;` |
|        - |  4574 | `	sxi32 b;` |
|        - |  4575 | `#ifdef UNTRUST` |
|        - |  4576 | `	if( pNos < pStack ){` |
|        - |  4577 | `		goto Abort;` |
|        - |  4578 | `	}` |
|        - |  4579 | `#endif` |
|        - |  4580 | `	/* Force the operands to be integer */` |
|       25 |  4581 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4582 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4583 | `	}` |
|       25 |  4584 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4585 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4586 | `	}` |
|        - |  4587 | `	/* Perform the requested operation */` |
|       25 |  4588 | `	a = pNos->x.iVal;` |
|       25 |  4589 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  4590 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  4591 | `		r = a << b;` |
|        8 |  4592 | `	}else{` |
|       11 |  4593 | `		r = a >> b;` |
|        - |  4594 | `	}` |
|        - |  4595 | `	/* Push the result */` |
|       25 |  4596 | `	pNos->x.iVal = r;` |
|       25 |  4597 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  4598 | `	VmPopOperand(&pTos,1);` |
|       25 |  4599 | `	break;` |
|        - |  4600 | `				 }` |
|        - |  4601 | `/*  OP_SHL_STORE * * *` |
|        - |  4602 | ` *` |
|        - |  4603 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4604 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4605 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4606 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4607 | ` */` |
|        - |  4608 | `/* OP_SHR_STORE * * *` |
|        - |  4609 | ` *` |
|        - |  4610 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4611 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4612 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4613 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4614 | ` */` |
|        9 |  4615 | `case PH7_OP_SHL_STORE:` |
|        - |  4616 | `case PH7_OP_SHR_STORE: {` |
|       19 |  4617 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4618 | `	ph7_value *pObj;` |
|        - |  4619 | `	sxi64 a,r;` |
|        - |  4620 | `	sxi32 b;` |
|        - |  4621 | `#ifdef UNTRUST` |
|        - |  4622 | `	if( pNos < pStack ){` |
|        - |  4623 | `		goto Abort;` |
|        - |  4624 | `	}` |
|        - |  4625 | `#endif` |
|        - |  4626 | `	/* Force the operands to be integer */` |
|       19 |  4627 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4628 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4629 | `	}` |
|       19 |  4630 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4631 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4632 | `	}` |
|        - |  4633 | `	/* Perform the requested operation */` |
|       19 |  4634 | `	a = pTos->x.iVal;` |
|       19 |  4635 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  4636 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  4637 | `		r = a << b;` |
|        5 |  4638 | `	}else{` |
|       11 |  4639 | `		r = a >> b;` |
|        - |  4640 | `	}` |
|        - |  4641 | `	/* Push the result */` |
|       19 |  4642 | `	pNos->x.iVal = r;` |
|       19 |  4643 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4644 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4645 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  4646 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  4647 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  4648 | `	}` |
|       19 |  4649 | `	VmPopOperand(&pTos,1);` |
|       19 |  4650 | `	break;` |
|        - |  4651 | `				 }` |
|        - |  4652 | `/* CAT:  P1 * *` |
|        - |  4653 | ` *` |
|        - |  4654 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4655 | ` * back.` |
|        - |  4656 | ` */` |
|    64991 |  4657 | `case PH7_OP_CAT:{` |
|        - |  4658 | `	ph7_value *pNos,*pCur;` |
|   129984 |  4659 | `	if( pInstr->iP1 < 1 ){` |
|   102868 |  4660 | `		pNos = &pTos[-1];` |
|    51435 |  4661 | `	}else{` |
|    27118 |  4662 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4663 | `	}` |
|        - |  4664 | `#ifdef UNTRUST` |
|        - |  4665 | `	if( pNos < pStack ){` |
|        - |  4666 | `		goto Abort;` |
|        - |  4667 | `	}` |
|        - |  4668 | `#endif` |
|        - |  4669 | `	/* Force a string cast */` |
|   129984 |  4670 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1626 |  4671 | `		PH7_MemObjToString(pNos);` |
|      812 |  4672 | `	}` |
|   129984 |  4673 | `	pCur = &pNos[1];` |
|   262148 |  4674 | `	while( pCur <= pTos ){` |
|   132166 |  4675 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50734 |  4676 | `			PH7_MemObjToString(pCur);` |
|    25366 |  4677 | `		}` |
|        - |  4678 | `		/* Perform the concatenation */` |
|   132166 |  4679 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   132126 |  4680 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    66062 |  4681 | `		}` |
|   132166 |  4682 | `		SyBlobRelease(&pCur->sBlob);` |
|   132166 |  4683 | `		pCur++;` |
|        2 |  4684 | `	}` |
|   129984 |  4685 | `	pTos = pNos;` |
|   129984 |  4686 | `	break;` |
|        - |  4687 | `				}` |
|        - |  4688 | `/*  CAT_STORE: * * *` |
|        - |  4689 | ` *` |
|        - |  4690 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4691 | ` * back.` |
|        - |  4692 | ` */` |
|     3496 |  4693 | `case PH7_OP_CAT_STORE:{` |
|     6994 |  4694 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4695 | `	ph7_value *pObj;` |
|        - |  4696 | `#ifdef UNTRUST` |
|        - |  4697 | `	if( pNos < pStack ){` |
|        - |  4698 | `		goto Abort;` |
|        - |  4699 | `	}` |
|        - |  4700 | `#endif` |
|     6994 |  4701 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4702 | `		/* Force a string cast */` |
|      ! 0 |  4703 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4704 | `	}` |
|     6994 |  4705 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4706 | `		/* Force a string cast */` |
|      ! 0 |  4707 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4708 | `	}` |
|        - |  4709 | `	/* Perform the concatenation (Reverse order) */` |
|     6994 |  4710 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6994 |  4711 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3496 |  4712 | `	}` |
|        - |  4713 | `	/* Perform the store operation */` |
|     6994 |  4714 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4715 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6994 |  4716 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6994 |  4717 | `		PH7_MemObjStore(pTos,pObj);` |
|     3496 |  4718 | `	}` |
|     6994 |  4719 | `	PH7_MemObjStore(pTos,pNos);` |
|     6994 |  4720 | `	VmPopOperand(&pTos,1);` |
|     6994 |  4721 | `	break;` |
|        - |  4722 | `				}` |
|        - |  4723 | `/* OP_AND: * * *` |
|        - |  4724 | ` *` |
|        - |  4725 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4726 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4727 | ` * stack.` |
|        - |  4728 | ` */` |
|        - |  4729 | `/* OP_OR: * * *` |
|        - |  4730 | ` *` |
|        - |  4731 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4732 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4733 | ` * stack.` |
|        - |  4734 | ` */` |
|    97666 |  4735 | `case PH7_OP_LAND:` |
|        - |  4736 | `case PH7_OP_LOR: {` |
|   195378 |  4737 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4738 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4739 | `#ifdef UNTRUST` |
|        - |  4740 | `	if( pNos < pStack ){` |
|        - |  4741 | `		goto Abort;` |
|        - |  4742 | `	}` |
|        - |  4743 | `#endif` |
|        - |  4744 | `	/* Force a boolean cast */` |
|   195378 |  4745 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4746 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4747 | `	}` |
|   195378 |  4748 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4749 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4750 | `	}` |
|   195378 |  4751 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   195378 |  4752 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   195378 |  4753 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4754 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    89852 |  4755 | `		v1 = and_logic[v1*3+v2];` |
|    44949 |  4756 | `	}else{` |
|        - |  4757 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   105528 |  4758 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4759 | `	}` |
|   195378 |  4760 | `	if( v1 == 2 ){` |
|      ! 0 |  4761 | `		v1 = 1;` |
|      ! 0 |  4762 | `	}` |
|   195378 |  4763 | `	VmPopOperand(&pTos,1);` |
|   195378 |  4764 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   195378 |  4765 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   195378 |  4766 | `	break;` |
|        - |  4767 | `				 }` |
|        - |  4768 | `/*` |
|        - |  4769 | ` * OP_NULLC: * * *` |
|        - |  4770 | ` * Null coalescing operator '??'.` |
|        - |  4771 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  4772 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  4773 | ` */` |
|        - |  4774 | `/*` |
|        - |  4775 | ` * OP_NULLC: * P2 *` |
|        - |  4776 | ` * Short-circuit null coalescing '??'.` |
|        - |  4777 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  4778 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  4779 | ` */` |
|       19 |  4780 | `case PH7_OP_NULLC: {` |
|        - |  4781 | `#ifdef UNTRUST` |
|        - |  4782 | `	if( pTos < pStack ){` |
|        - |  4783 | `		goto Abort;` |
|        - |  4784 | `	}` |
|        - |  4785 | `#endif` |
|       40 |  4786 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  4787 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  4788 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  4789 | `	}else{` |
|        - |  4790 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  4791 | `		VmPopOperand(&pTos, 1);` |
|        - |  4792 | `	}` |
|       40 |  4793 | `	break;` |
|        - |  4794 |  |
|        - |  4795 | `/*` |
|        - |  4796 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  4797 | ` * Null coalescing assignment short-circuit.` |
|        - |  4798 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  4799 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  4800 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  4801 | ` */` |
|       23 |  4802 | `case PH7_OP_NULLC_JMP: {` |
|        - |  4803 | `#ifdef UNTRUST` |
|        - |  4804 | `	if( pTos < pStack ){` |
|        - |  4805 | `		goto Abort;` |
|        - |  4806 | `	}` |
|        - |  4807 | `#endif` |
|       47 |  4808 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  4809 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  4810 | `	}` |
|       47 |  4811 | `	break;` |
|        - |  4812 |  |
|        - |  4813 | `/*` |
|        - |  4814 | ` * OP_NULLC_STORE: * * *` |
|        - |  4815 | ` * Null coalescing assignment store.` |
|        - |  4816 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  4817 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  4818 | ` * expression result.` |
|        - |  4819 | ` */` |
|       14 |  4820 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  4821 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4822 | `	ph7_value *pObj;` |
|        - |  4823 | `	sxu32 nIdx;` |
|        - |  4824 | `#ifdef UNTRUST` |
|        - |  4825 | `	if( pNos < pStack ){` |
|        - |  4826 | `		goto Abort;` |
|        - |  4827 | `	}` |
|        - |  4828 | `#endif` |
|       29 |  4829 | `	nIdx = pNos->nIdx;` |
|       29 |  4830 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4831 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4832 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  4833 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  4834 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  4835 | `	}` |
|       29 |  4836 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  4837 | `	VmPopOperand(&pTos,1);` |
|       29 |  4838 | `	break;` |
|        - |  4839 |  |
|        - |  4840 | `/*` |
|        - |  4841 | ` * OP_SPREAD: * * *` |
|        - |  4842 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  4843 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  4844 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  4845 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  4846 | ` */` |
|        7 |  4847 | `case PH7_OP_SPREAD: {` |
|        - |  4848 | `#ifdef UNTRUST` |
|        - |  4849 | `	if( pTos < pStack ){` |
|        - |  4850 | `		goto Abort;` |
|        - |  4851 | `	}` |
|        - |  4852 | `#endif` |
|       15 |  4853 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  4854 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  4855 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  4856 | `		if( nEntry == 0 ){` |
|        - |  4857 | `			/* Empty array — remove from stack */` |
|        3 |  4858 | `			VmPopOperand(&pTos, 1);` |
|        3 |  4859 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  4860 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  4861 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  4862 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  4863 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  4864 | `				VM_STACK_GUARD);` |
|      ! 0 |  4865 | `		}else{` |
|        - |  4866 | `			ph7_hashmap_node *pNode2;` |
|        - |  4867 | `			ph7_value *pElem;` |
|        - |  4868 | `			sxu32 i;` |
|        - |  4869 | `			/* Overwrite TOS with first element */` |
|       13 |  4870 | `			pNode2 = pMap->pFirst;` |
|       13 |  4871 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  4872 | `			PH7_MemObjRelease(pTos);` |
|       13 |  4873 | `			if( pElem ){` |
|       13 |  4874 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  4875 | `			}` |
|       13 |  4876 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  4877 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  4878 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  4879 | `			pNode2 = pNode2->pPrev;` |
|        - |  4880 | `			/* Push remaining elements */` |
|       33 |  4881 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  4882 | `				pTos++;` |
|       21 |  4883 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  4884 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  4885 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  4886 | `				if( pElem ){` |
|       21 |  4887 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  4888 | `				}` |
|       21 |  4889 | `				pNode2 = pNode2->pPrev;` |
|       11 |  4890 | `			}` |
|       13 |  4891 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  4892 | `		}` |
|        7 |  4893 | `	}` |
|        - |  4894 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  4895 | `	break;` |
|        - |  4896 |  |
|        - |  4897 | `/* OP_LXOR: * * *` |
|        - |  4898 | ` *` |
|        - |  4899 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4900 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4901 | ` * stack.` |
|        - |  4902 | ` * According to the PHP language reference manual:` |
|        - |  4903 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4904 | ` *  TRUE,but not both.` |
|        - |  4905 | ` */` |
|        5 |  4906 | `case PH7_OP_LXOR:{` |
|       11 |  4907 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4908 | `	sxi32 v = 0;` |
|        - |  4909 | `#ifdef UNTRUST` |
|        - |  4910 | `	if( pNos < pStack ){` |
|        - |  4911 | `		goto Abort;` |
|        - |  4912 | `	}` |
|        - |  4913 | `#endif` |
|        - |  4914 | `	/* Force a boolean cast */` |
|       11 |  4915 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4916 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4917 | `	}` |
|       11 |  4918 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4919 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4920 | `	}` |
|       11 |  4921 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4922 | `		v = 1;` |
|        3 |  4923 | `	}` |
|       11 |  4924 | `	VmPopOperand(&pTos,1);` |
|       11 |  4925 | `	pTos->x.iVal = v;` |
|       11 |  4926 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4927 | `	break;` |
|        - |  4928 | `				 }` |
|        - |  4929 | `/* OP_EQ P1 P2 P3` |
|        - |  4930 | ` *` |
|        - |  4931 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4932 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4933 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4934 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4935 | ` */` |
|        - |  4936 | `/* OP_NEQ P1 P2 P3` |
|        - |  4937 | ` *` |
|        - |  4938 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4939 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4940 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4941 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4942 | ` */` |
|     4089 |  4943 | `case PH7_OP_EQ:` |
|        - |  4944 | `case PH7_OP_NEQ: {` |
|     8180 |  4945 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4946 | `	/* Perform the comparison and act accordingly */` |
|        - |  4947 | `#ifdef UNTRUST` |
|        - |  4948 | `	if( pNos < pStack ){` |
|        - |  4949 | `		goto Abort;` |
|        - |  4950 | `	}` |
|        - |  4951 | `#endif` |
|     8180 |  4952 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8180 |  4953 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  4954 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8171 |  4955 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8136 |  4956 | `		rc = rc == 0;` |
|     4069 |  4957 | `	}else{` |
|       28 |  4958 | `		rc = rc != 0;` |
|        - |  4959 | `	}` |
|     8180 |  4960 | `	VmPopOperand(&pTos,1);` |
|     8180 |  4961 | `	if( !pInstr->iP2 ){` |
|        - |  4962 | `		/* Push comparison result without taking the jump */` |
|     8180 |  4963 | `		PH7_MemObjRelease(pTos);` |
|     8180 |  4964 | `		pTos->x.iVal = rc;` |
|        - |  4965 | `		/* Invalidate any prior representation */` |
|     8180 |  4966 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4091 |  4967 | `	}else{` |
|      ! 0 |  4968 | `		if( rc ){` |
|        - |  4969 | `			/* Jump to the desired location */` |
|      ! 0 |  4970 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4971 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4972 | `		}` |
|        - |  4973 | `	}` |
|     8180 |  4974 | `	break;` |
|        - |  4975 | `				 }` |
|        - |  4976 | `/* OP_TEQ P1 P2 *` |
|        - |  4977 | ` *` |
|        - |  4978 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4979 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4980 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4981 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4982 | ` */` |
|   138642 |  4983 | `case PH7_OP_TEQ: {` |
|   277286 |  4984 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4985 | `	/* Perform the comparison and act accordingly */` |
|        - |  4986 | `#ifdef UNTRUST` |
|        - |  4987 | `	if( pNos < pStack ){` |
|        - |  4988 | `		goto Abort;` |
|        - |  4989 | `	}` |
|        - |  4990 | `#endif` |
|   277286 |  4991 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   277286 |  4992 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4993 | `		rc = 0;` |
|        2 |  4994 | `	}else{` |
|   277284 |  4995 | `		rc = rc == 0;` |
|        - |  4996 | `	}` |
|   277286 |  4997 | `	VmPopOperand(&pTos,1);` |
|   277286 |  4998 | `	if( !pInstr->iP2 ){` |
|        - |  4999 | `		/* Push comparison result without taking the jump */` |
|   277286 |  5000 | `		PH7_MemObjRelease(pTos);` |
|   277286 |  5001 | `		pTos->x.iVal = rc;` |
|        - |  5002 | `		/* Invalidate any prior representation */` |
|   277286 |  5003 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   138644 |  5004 | `	}else{` |
|      ! 0 |  5005 | `		if( rc ){` |
|        - |  5006 | `			/* Jump to the desired location */` |
|      ! 0 |  5007 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5008 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5009 | `		}` |
|        - |  5010 | `	}` |
|   277286 |  5011 | `	break;` |
|        - |  5012 | `				 }` |
|        - |  5013 | `/* OP_TNE P1 P2 *` |
|        - |  5014 | ` *` |
|        - |  5015 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  5016 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  5017 | ` * instruction.` |
|        - |  5018 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5019 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5020 | ` *` |
|        - |  5021 | ` */` |
|   108107 |  5022 | `case PH7_OP_TNE: {` |
|   216216 |  5023 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5024 | `	/* Perform the comparison and act accordingly */` |
|        - |  5025 | `#ifdef UNTRUST` |
|        - |  5026 | `	if( pNos < pStack ){` |
|        - |  5027 | `		goto Abort;` |
|        - |  5028 | `	}` |
|        - |  5029 | `#endif` |
|   216216 |  5030 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   216216 |  5031 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5032 | `		rc = 1;` |
|        2 |  5033 | `	}else{` |
|   216214 |  5034 | `		rc = rc != 0;` |
|        - |  5035 | `	}` |
|   216216 |  5036 | `	VmPopOperand(&pTos,1);` |
|   216216 |  5037 | `	if( !pInstr->iP2 ){` |
|        - |  5038 | `		/* Push comparison result without taking the jump */` |
|   216216 |  5039 | `		PH7_MemObjRelease(pTos);` |
|   216216 |  5040 | `		pTos->x.iVal = rc;` |
|        - |  5041 | `		/* Invalidate any prior representation */` |
|   216216 |  5042 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   108109 |  5043 | `	}else{` |
|      ! 0 |  5044 | `		if( rc ){` |
|        - |  5045 | `			/* Jump to the desired location */` |
|      ! 0 |  5046 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5047 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5048 | `		}` |
|        - |  5049 | `	}` |
|   216216 |  5050 | `	break;` |
|        - |  5051 | `				 }` |
|        - |  5052 | `/* OP_LT P1 P2 P3` |
|        - |  5053 | ` *` |
|        - |  5054 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5055 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5056 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5057 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5058 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5059 | ` *` |
|        - |  5060 | ` */` |
|        - |  5061 | `/* OP_LE P1 P2 P3` |
|        - |  5062 | ` *` |
|        - |  5063 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5064 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5065 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5066 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5067 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5068 | ` *` |
|        - |  5069 | ` */` |
|   104589 |  5070 | `case PH7_OP_LT:` |
|        - |  5071 | `case PH7_OP_LE: {` |
|   209224 |  5072 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5073 | `	/* Perform the comparison and act accordingly */` |
|        - |  5074 | `#ifdef UNTRUST` |
|        - |  5075 | `	if( pNos < pStack ){` |
|        - |  5076 | `		goto Abort;` |
|        - |  5077 | `	}` |
|        - |  5078 | `#endif` |
|   209224 |  5079 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   209224 |  5080 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5081 | `		rc = 0;` |
|   209220 |  5082 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      434 |  5083 | `		rc = rc < 1;` |
|      218 |  5084 | `	}else{` |
|   208784 |  5085 | `		rc = rc < 0;` |
|        - |  5086 | `	}` |
|   209224 |  5087 | `	VmPopOperand(&pTos,1);` |
|   209224 |  5088 | `	if( !pInstr->iP2 ){` |
|        - |  5089 | `		/* Push comparison result without taking the jump */` |
|   209224 |  5090 | `		PH7_MemObjRelease(pTos);` |
|   209224 |  5091 | `		pTos->x.iVal = rc;` |
|        - |  5092 | `		/* Invalidate any prior representation */` |
|   209224 |  5093 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   104635 |  5094 | `	}else{` |
|      ! 0 |  5095 | `		if( rc ){` |
|        - |  5096 | `			/* Jump to the desired location */` |
|      ! 0 |  5097 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5098 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5099 | `		}` |
|        - |  5100 | `	}` |
|   209224 |  5101 | `	break;` |
|        - |  5102 | `				}` |
|        - |  5103 | `/* OP_GT P1 P2 P3` |
|        - |  5104 | ` *` |
|        - |  5105 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5106 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5107 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5108 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5109 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5110 | ` *` |
|        - |  5111 | ` */` |
|        - |  5112 | `/* OP_GE P1 P2 P3` |
|        - |  5113 | ` *` |
|        - |  5114 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5115 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5116 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5117 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5118 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5119 | ` *` |
|        - |  5120 | ` */` |
|    50238 |  5121 | `case PH7_OP_GT:` |
|        - |  5122 | `case PH7_OP_GE: {` |
|   100478 |  5123 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5124 | `	/* Perform the comparison and act accordingly */` |
|        - |  5125 | `#ifdef UNTRUST` |
|        - |  5126 | `	if( pNos < pStack ){` |
|        - |  5127 | `		goto Abort;` |
|        - |  5128 | `	}` |
|        - |  5129 | `#endif` |
|   100478 |  5130 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   100478 |  5131 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5132 | `		rc = 0;` |
|   100474 |  5133 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   100316 |  5134 | `		rc = rc >= 0;` |
|    50159 |  5135 | `	}else{` |
|      156 |  5136 | `		rc = rc > 0;` |
|        - |  5137 | `	}` |
|   100478 |  5138 | `	VmPopOperand(&pTos,1);` |
|   100478 |  5139 | `	if( !pInstr->iP2 ){` |
|        - |  5140 | `		/* Push comparison result without taking the jump */` |
|   100478 |  5141 | `		PH7_MemObjRelease(pTos);` |
|   100478 |  5142 | `		pTos->x.iVal = rc;` |
|        - |  5143 | `		/* Invalidate any prior representation */` |
|   100478 |  5144 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    50240 |  5145 | `	}else{` |
|      ! 0 |  5146 | `		if( rc ){` |
|        - |  5147 | `			/* Jump to the desired location */` |
|      ! 0 |  5148 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5149 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5150 | `		}` |
|        - |  5151 | `	}` |
|   100478 |  5152 | `	break;` |
|        - |  5153 | `				}` |
|        - |  5154 | `/* OP_SPACESHIP * * *` |
|        - |  5155 | ` *` |
|        - |  5156 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5157 | ` *   -1 if left < right` |
|        - |  5158 | ` *    0 if left == right` |
|        - |  5159 | ` *    1 if left > right` |
|        - |  5160 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5161 | ` */` |
|       25 |  5162 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5163 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5164 | `#ifdef UNTRUST` |
|        - |  5165 | `	if( pNos < pStack ){` |
|        - |  5166 | `		goto Abort;` |
|        - |  5167 | `	}` |
|        - |  5168 | `#endif` |
|       51 |  5169 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5170 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5171 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5172 | `		rc = 1;` |
|        4 |  5173 | `	}else{` |
|        - |  5174 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5175 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5176 | `	}` |
|       51 |  5177 | `	VmPopOperand(&pTos,1);` |
|       51 |  5178 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5179 | `	pTos->x.iVal = rc;` |
|       51 |  5180 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5181 | `	break;` |
|        - |  5182 | `				}` |
|        - |  5183 | `/* OP_SEQ P1 P2 *` |
|        - |  5184 | ` * Strict string comparison.` |
|        - |  5185 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5186 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5187 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5188 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5189 | ` * use PH7_OP_EQ.` |
|        - |  5190 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5191 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5192 | ` */` |
|        - |  5193 | `/* OP_SNE P1 P2 *` |
|        - |  5194 | ` * Strict string comparison.` |
|        - |  5195 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5196 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5197 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5198 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5199 | ` * use PH7_OP_EQ.` |
|        - |  5200 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5201 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5202 | ` */` |
|       18 |  5203 | `case PH7_OP_SEQ:` |
|        - |  5204 | `case PH7_OP_SNE: {` |
|       38 |  5205 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5206 | `	SyString s1,s2;` |
|        - |  5207 | `	/* Perform the comparison and act accordingly */` |
|        - |  5208 | `#ifdef UNTRUST` |
|        - |  5209 | `	if( pNos < pStack ){` |
|        - |  5210 | `		goto Abort;` |
|        - |  5211 | `	}` |
|        - |  5212 | `#endif` |
|        - |  5213 | `	/* Force a string cast */` |
|       38 |  5214 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5215 | `		PH7_MemObjToString(pTos);` |
|        2 |  5216 | `	}` |
|       38 |  5217 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5218 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5219 | `	}` |
|       38 |  5220 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5221 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5222 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5223 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5224 | `		rc = rc != 0;` |
|      ! 0 |  5225 | `	}else{` |
|       38 |  5226 | `		rc = rc == 0;` |
|        - |  5227 | `	}` |
|       38 |  5228 | `	VmPopOperand(&pTos,1);` |
|       38 |  5229 | `	if( !pInstr->iP2 ){` |
|        - |  5230 | `		/* Push comparison result without taking the jump */` |
|       38 |  5231 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5232 | `		pTos->x.iVal = rc;` |
|        - |  5233 | `		/* Invalidate any prior representation */` |
|       38 |  5234 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5235 | `	}else{` |
|      ! 0 |  5236 | `		if( rc ){` |
|        - |  5237 | `			/* Jump to the desired location */` |
|      ! 0 |  5238 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5239 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5240 | `		}` |
|        - |  5241 | `	}` |
|       38 |  5242 | `	break;` |
|        - |  5243 | `				 }` |
|        - |  5244 | `/*` |
|        - |  5245 | ` * OP_LOAD_REF * * *` |
|        - |  5246 | ` * Push the index of a referenced object on the stack.` |
|        - |  5247 | ` */` |
|       57 |  5248 | `case PH7_OP_LOAD_REF: {` |
|        - |  5249 | `	sxu32 nIdx;` |
|        - |  5250 | `#ifdef UNTRUST` |
|        - |  5251 | `	if( pTos < pStack ){` |
|        - |  5252 | `		goto Abort;` |
|        - |  5253 | `	}` |
|        - |  5254 | `#endif` |
|        - |  5255 | `	/* Extract memory object index */` |
|      115 |  5256 | `	nIdx = pTos->nIdx;` |
|      115 |  5257 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5258 | `		/* Nullify the object */` |
|       95 |  5259 | `		PH7_MemObjRelease(pTos);` |
|        - |  5260 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5261 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5262 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5263 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5264 | `	}` |
|      115 |  5265 | `	break;` |
|        - |  5266 | `					  }` |
|        - |  5267 | `/*` |
|        - |  5268 | ` * OP_STORE_REF * * P3` |
|        - |  5269 | ` * Perform an assignment operation by reference.` |
|        - |  5270 | ` */` |
|       16 |  5271 | ` case PH7_OP_STORE_REF: {` |
|       34 |  5272 | `	 SyString sName = { 0 , 0 };` |
|        - |  5273 | `	 VmFrame *pFrameLocal;` |
|        - |  5274 | `	SyHashEntry *pEntry;` |
|        - |  5275 | `	sxu32 nIdx;` |
|        - |  5276 | `#ifdef UNTRUST` |
|        - |  5277 | `	if( pTos < pStack ){` |
|        - |  5278 | `		goto Abort;` |
|        - |  5279 | `	}` |
|        - |  5280 | `#endif` |
|       34 |  5281 | `	if( pInstr->p3 == 0 ){` |
|        - |  5282 | `		char *zName;` |
|        - |  5283 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5284 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5285 | `			/* Force a string cast */` |
|      ! 0 |  5286 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5287 | `		}` |
|      ! 0 |  5288 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5289 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5290 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5291 | `			if( zName ){` |
|      ! 0 |  5292 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5293 | `			}` |
|      ! 0 |  5294 | `		}` |
|      ! 0 |  5295 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5296 | `		pTos--;` |
|      ! 0 |  5297 | `	}else{` |
|       34 |  5298 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5299 | `	}` |
|       34 |  5300 | `	nIdx = pTos->nIdx;` |
|       34 |  5301 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5302 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5303 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5304 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5305 | `		}else{` |
|        - |  5306 | `			ph7_value *pObj;` |
|        - |  5307 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5308 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5309 | `			if( pObj == 0 ){` |
|      ! 0 |  5310 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5311 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5312 | `				goto Abort;` |
|        - |  5313 | `			}` |
|        - |  5314 | `			/* Perform the store operation */` |
|      ! 0 |  5315 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5316 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5317 | `		}` |
|       34 |  5318 | `	}else if( sName.nByte > 0){` |
|       34 |  5319 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5320 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5321 | `		}else{` |
|       34 |  5322 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  5323 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5324 | `			/* Query the local frame */` |
|       34 |  5325 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  5326 | `			if( pEntry ){` |
|      ! 0 |  5327 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5328 | `			}else{` |
|       34 |  5329 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  5330 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5331 | `					/* Insert in the $GLOBALS array */` |
|       30 |  5332 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  5333 | `				}` |
|       34 |  5334 | `				if( rc == SXRET_OK ){` |
|       34 |  5335 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  5336 | `				}` |
|        - |  5337 | `			}` |
|        - |  5338 | `		}` |
|       16 |  5339 | `	}` |
|       34 |  5340 | `	break;` |
|        - |  5341 | `				 }` |
|        - |  5342 | `/*` |
|        - |  5343 | ` * OP_UPLINK P1 * *` |
|        - |  5344 | ` * Link a variable to the top active VM frame.` |
|        - |  5345 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5346 | ` */` |
|       25 |  5347 | `case PH7_OP_UPLINK: {` |
|       52 |  5348 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5349 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5350 | `		SyString sName;` |
|        - |  5351 | `		/* Perform the link */` |
|      104 |  5352 | `		while( pLink <= pTos ){` |
|       54 |  5353 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5354 | `				/* Force a string cast */` |
|      ! 0 |  5355 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5356 | `			}` |
|       54 |  5357 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5358 | `			if( sName.nByte > 0 ){` |
|       54 |  5359 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5360 | `			}` |
|       54 |  5361 | `			pLink++;` |
|        2 |  5362 | `		}` |
|       25 |  5363 | `	}` |
|       52 |  5364 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5365 | `	break;` |
|        - |  5366 | `					}` |
|        - |  5367 | `/*` |
|        - |  5368 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5369 | ` * Push an exception in the corresponding container so that` |
|        - |  5370 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5371 | ` */` |
|       49 |  5372 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      100 |  5373 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5374 | `	VmFrame *pFrameLocal;` |
|        - |  5375 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      100 |  5376 | `	pException->iFinallyDone = 0;` |
|      100 |  5377 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5378 | `	/* Create the exception frame */` |
|      100 |  5379 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      100 |  5380 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5381 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5382 | `		goto Abort;` |
|        - |  5383 | `	}` |
|        - |  5384 | `	/* Mark the special frame */` |
|      100 |  5385 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      100 |  5386 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5387 | `	/* Point to the frame that trigger the exception */` |
|      100 |  5388 | `	pFrameLocal = pFrameLocal->pParent;` |
|      100 |  5389 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      100 |  5390 | `	pException->pFrame = pFrameLocal;` |
|      100 |  5391 | `	break;` |
|        - |  5392 | `							}` |
|        - |  5393 | `/*` |
|        - |  5394 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5395 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5396 | ` */` |
|       48 |  5397 | `case PH7_OP_POP_EXCEPTION: {` |
|       98 |  5398 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       98 |  5399 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5400 | `		ph7_exception **apException;` |
|        - |  5401 | `		/* Pop the loaded exception */` |
|       28 |  5402 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5403 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5404 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5405 | `		}` |
|       13 |  5406 | `	}` |
|       98 |  5407 | `	pException->pFrame = 0;` |
|        - |  5408 | `	/* Leave the exception frame */` |
|       98 |  5409 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5410 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       98 |  5411 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5412 | `		sxi32 rcFinally;` |
|       20 |  5413 | `		pException->iFinallyDone = 1;` |
|       20 |  5414 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5415 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5416 | `			goto Abort;` |
|        - |  5417 | `		}` |
|        9 |  5418 | `	}` |
|       98 |  5419 | `	break;` |
|        - |  5420 | `							}` |
|        - |  5421 |  |
|        - |  5422 | `/*` |
|        - |  5423 | ` * OP_THROW * P2 *` |
|        - |  5424 | ` * Throw an user exception.` |
|        - |  5425 | ` */` |
|       30 |  5426 | `case PH7_OP_THROW: {` |
|       62 |  5427 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       62 |  5428 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5429 | `#ifdef UNTRUST` |
|        - |  5430 | `	if( pTos < pStack ){` |
|        - |  5431 | `		goto Abort;` |
|        - |  5432 | `	}` |
|        - |  5433 | `#endif` |
|       62 |  5434 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5435 | `	/* Tell the upper layer that an exception was thrown */` |
|       62 |  5436 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       62 |  5437 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       62 |  5438 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5439 | `		ph7_class *pException;` |
|        - |  5440 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5441 | `		 */` |
|       62 |  5442 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       62 |  5443 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5444 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5445 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5446 | `			if( rc == SXERR_ABORT ){` |
|        - |  5447 | `				/* Abort processing immediately */` |
|      ! 0 |  5448 | `				goto Abort;` |
|        - |  5449 | `			}` |
|      ! 0 |  5450 | `		}else{` |
|        - |  5451 | `			/* Throw the exception */` |
|       62 |  5452 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       62 |  5453 | `			if( rc == SXERR_ABORT ){` |
|        - |  5454 | `				/* Abort processing immediately */` |
|        9 |  5455 | `				goto Abort;` |
|        - |  5456 | `			}` |
|        - |  5457 | `		}` |
|       28 |  5458 | `	}else{` |
|        - |  5459 | `		/* Expecting a class instance */` |
|      ! 0 |  5460 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5461 | `		if( rc == SXERR_ABORT ){` |
|        - |  5462 | `			/* Abort processing immediately */` |
|      ! 0 |  5463 | `			goto Abort;` |
|        - |  5464 | `		}` |
|        - |  5465 | `	}` |
|        - |  5466 | `	/* Pop the top entry */` |
|       54 |  5467 | `	VmPopOperand(&pTos,1);` |
|        - |  5468 | `	/* Perform an unconditional jump */` |
|       54 |  5469 | `	pc = nJump - 1;` |
|       54 |  5470 | `	break;` |
|        - |  5471 | `				   }` |
|        - |  5472 | `/*` |
|        - |  5473 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5474 | ` * Prepare a foreach step.` |
|        - |  5475 | ` */` |
|     5248 |  5476 | `case PH7_OP_FOREACH_INIT: {` |
|    10498 |  5477 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5478 | `	void *pName;` |
|        - |  5479 | `#ifdef UNTRUST` |
|        - |  5480 | `	if( pTos < pStack ){` |
|        - |  5481 | `		goto Abort;` |
|        - |  5482 | `	}` |
|        - |  5483 | `#endif` |
|    10498 |  5484 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5485 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5486 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5487 | `			/* Force a string cast */` |
|      ! 0 |  5488 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5489 | `		}` |
|        - |  5490 | `		/* Duplicate name */` |
|      ! 0 |  5491 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5492 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5493 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5494 | `		}` |
|      ! 0 |  5495 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5496 | `	}` |
|    10498 |  5497 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5498 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5499 | `			/* Force a string cast */` |
|      ! 0 |  5500 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5501 | `		}` |
|        - |  5502 | `		/* Duplicate name */` |
|      ! 0 |  5503 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5504 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5505 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5506 | `		}` |
|      ! 0 |  5507 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5508 | `	}` |
|        - |  5509 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10498 |  5510 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5511 | `		/* Jump out of the loop */` |
|      ! 0 |  5512 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5513 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5514 | `		}` |
|      ! 0 |  5515 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5516 | `	}else{` |
|        - |  5517 | `		ph7_foreach_step *pStep;` |
|    10498 |  5518 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10498 |  5519 | `		if( pStep == 0 ){` |
|      ! 0 |  5520 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5521 | `			/* Jump out of the loop */` |
|      ! 0 |  5522 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5523 | `		}else{` |
|        - |  5524 | `			/* Zero the structure */` |
|    10498 |  5525 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5526 | `			/* Prepare the step */` |
|    10498 |  5527 | `			pStep->iFlags = pInfo->iFlags;` |
|    10498 |  5528 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5529 | `				ph7_hashmap *pMap;` |
|        - |  5530 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5531 | `				 * source array so mutations don't affect other sharers. */` |
|    10470 |  5532 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  5533 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  5534 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  5535 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5536 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5537 | `						 * variable still points at the same hashmap as` |
|        - |  5538 | `						 * the stack value. */` |
|        9 |  5539 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  5540 | `							pCur->iRef--;` |
|        9 |  5541 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  5542 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  5543 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5544 | `						}` |
|        4 |  5545 | `					}` |
|        4 |  5546 | `				}` |
|    10470 |  5547 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5548 | `				/* Reset the internal loop cursor */` |
|    10470 |  5549 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5550 | `				/* Mark the step */` |
|    10470 |  5551 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10470 |  5552 | `				pStep->xIter.pMap = pMap;` |
|    10470 |  5553 | `				pMap->iRef++;` |
|     5236 |  5554 | `			}else{` |
|       30 |  5555 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5556 | `				ph7_class *pIteratorClass;` |
|        - |  5557 | `				/* Check if the object implements Iterator */` |
|       30 |  5558 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5559 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5560 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5561 | `					ph7_class_method *pRewind;` |
|       20 |  5562 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  5563 | `					pStep->xIter.pThis = pThis;` |
|       20 |  5564 | `					pThis->iRef++;` |
|       20 |  5565 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  5566 | `					if( pRewind ){` |
|       20 |  5567 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5568 | `					}` |
|       11 |  5569 | `				}else{` |
|        - |  5570 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5571 | `					ph7_class *pIterAggClass;` |
|       12 |  5572 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5573 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5574 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5575 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5576 | `						ph7_class_method *pGetIter;` |
|        3 |  5577 | `						int iterAggOk = 0;` |
|        3 |  5578 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5579 | `						if( pGetIter ){` |
|        - |  5580 | `							ph7_value sResult;` |
|        3 |  5581 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5582 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5583 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5584 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5585 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5586 | `									ph7_class_method *pRewind;` |
|        3 |  5587 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5588 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5589 | `									pIterObj->iRef++;` |
|        - |  5590 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5591 | `									pStep->pOwner = pThis;` |
|        3 |  5592 | `									pThis->iRef++;` |
|        3 |  5593 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5594 | `									if( pRewind ){` |
|        3 |  5595 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5596 | `									}` |
|        3 |  5597 | `									iterAggOk = 1;` |
|        1 |  5598 | `								}` |
|        1 |  5599 | `							}` |
|        3 |  5600 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5601 | `						}` |
|        3 |  5602 | `						if( !iterAggOk ){` |
|        - |  5603 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5604 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5605 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5606 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5607 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5608 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5609 | `						}` |
|        2 |  5610 | `					}else{` |
|        - |  5611 | `						/* Plain object iteration via hAttr */` |
|        9 |  5612 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5613 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5614 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5615 | `						pThis->iRef++;` |
|        - |  5616 | `					}` |
|        - |  5617 | `				}` |
|        - |  5618 | `			}` |
|        - |  5619 | `		}` |
|    10498 |  5620 | `		if( pStep ){` |
|    10498 |  5621 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5622 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5623 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5624 | `				/* Jump out of the loop */` |
|      ! 0 |  5625 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5626 | `			}` |
|     5248 |  5627 | `		}` |
|        - |  5628 | `	}` |
|    10498 |  5629 | `	VmPopOperand(&pTos,1);` |
|    10498 |  5630 | `	break;` |
|        - |  5631 | `						  }` |
|        - |  5632 | `/*` |
|        - |  5633 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5634 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5635 | ` */` |
|    85379 |  5636 | `case PH7_OP_FOREACH_STEP: {` |
|   170760 |  5637 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5638 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5639 | `	ph7_value *pValue;` |
|        - |  5640 | `	VmFrame *pFrameLocal;` |
|        - |  5641 | `	/* Peek the last step */` |
|   170760 |  5642 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   170760 |  5643 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   170760 |  5644 | `	pFrameLocal = pVm->pFrame;` |
|   170760 |  5645 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   170760 |  5646 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   170648 |  5647 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5648 | `		ph7_hashmap_node *pNode;` |
|        - |  5649 | `		/* Extract the current node value */` |
|   170648 |  5650 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   170648 |  5651 | `		if( pNode == 0 ){` |
|        - |  5652 | `			/* No more entry to process */` |
|    10468 |  5653 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10468 |  5654 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5655 | `				/* Break the reference with the last element */` |
|        7 |  5656 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5657 | `			}` |
|        - |  5658 | `			/* Automatically reset the loop cursor */` |
|    10468 |  5659 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5660 | `			/* Cleanup the mess left behind */` |
|    10468 |  5661 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10468 |  5662 | `			SySetPop(&pInfo->aStep);` |
|    10468 |  5663 | `			PH7_HashmapUnref(pMap);` |
|     5235 |  5664 | `		}else{` |
|   160182 |  5665 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5666 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5667 | `				if( pKey ){` |
|      416 |  5668 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5669 | `				}` |
|      207 |  5670 | `			}` |
|   160182 |  5671 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5672 | `				SyHashEntry *pEntry;` |
|        - |  5673 | `				/* Pass by reference */` |
|       23 |  5674 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  5675 | `				if( pEntry ){` |
|       23 |  5676 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5677 | `				}else{` |
|      ! 0 |  5678 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5679 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5680 | `				}` |
|       12 |  5681 | `			}else{` |
|        - |  5682 | `				/* Make a copy of the entry value */` |
|   160160 |  5683 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   160160 |  5684 | `				if( pValue ){` |
|   160160 |  5685 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    80079 |  5686 | `				}` |
|        - |  5687 | `			}` |
|        2 |  5688 | `		}` |
|    85437 |  5689 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5690 | `		/* Iterator-based iteration.` |
|        - |  5691 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5692 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5693 | `		 */` |
|       90 |  5694 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5695 | `		ph7_class_method *pMethod;` |
|        - |  5696 | `		ph7_value sResult;` |
|       90 |  5697 | `		int isValid = 0;` |
|        - |  5698 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  5699 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  5700 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  5701 | `		}else{` |
|       70 |  5702 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  5703 | `			if( pMethod ){` |
|       70 |  5704 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5705 | `			}` |
|        - |  5706 | `		}` |
|        - |  5707 | `		/* Call valid() */` |
|       90 |  5708 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  5709 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  5710 | `		if( pMethod ){` |
|       90 |  5711 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  5712 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  5713 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5714 | `		}` |
|       90 |  5715 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  5716 | `		if( !isValid ){` |
|        - |  5717 | `			/* Iterator exhausted */` |
|       20 |  5718 | `			pc = pInstr->iP2 - 1;` |
|        - |  5719 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  5720 | `			if( pStep->pOwner ){` |
|        3 |  5721 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5722 | `			}` |
|       20 |  5723 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  5724 | `			SySetPop(&pInfo->aStep);` |
|       20 |  5725 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  5726 | `		}else{` |
|        - |  5727 | `			/* Call current() to get value */` |
|       72 |  5728 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  5729 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  5730 | `			if( pMethod ){` |
|       72 |  5731 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5732 | `			}` |
|       72 |  5733 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  5734 | `			if( pValue ){` |
|       72 |  5735 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5736 | `			}` |
|       72 |  5737 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5738 | `			/* Call key() if needed */` |
|       72 |  5739 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5740 | `				ph7_value sKey;` |
|       35 |  5741 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5742 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5743 | `				if( pMethod ){` |
|       35 |  5744 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5745 | `				}` |
|       35 |  5746 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5747 | `				if( pValue ){` |
|       35 |  5748 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5749 | `				}` |
|       35 |  5750 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5751 | `			}` |
|        - |  5752 | `		}` |
|       46 |  5753 | `	}else{` |
|       25 |  5754 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5755 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5756 | `		SyHashEntry *pEntry;` |
|        - |  5757 | `		/* Point to the next attribute */` |
|       29 |  5758 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5759 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5760 | `			/* Check access permission */` |
|       31 |  5761 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5762 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5763 | `					break; /* Access is granted */` |
|        - |  5764 | `			}` |
|        1 |  5765 | `		}` |
|       25 |  5766 | `		if( pEntry == 0 ){` |
|        - |  5767 | `			/* Clean up the mess left behind */` |
|        9 |  5768 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5769 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5770 | `				/* Break the reference with the last element */` |
|        3 |  5771 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5772 | `			}` |
|        9 |  5773 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5774 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5775 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5776 | `		}else{` |
|       17 |  5777 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5778 | `			ph7_value *pAttrValue;` |
|       17 |  5779 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5780 | `				/* Fill with the current attribute name */` |
|       17 |  5781 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5782 | `				if( pKey ){` |
|       17 |  5783 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5784 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5785 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5786 | `				}` |
|        8 |  5787 | `			}` |
|        - |  5788 | `			/* Extract attribute value */` |
|       17 |  5789 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5790 | `			if( pAttrValue ){` |
|       17 |  5791 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5792 | `					/* Pass by reference */` |
|        3 |  5793 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5794 | `					if( pEntry ){` |
|        3 |  5795 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5796 | `					}else{` |
|      ! 0 |  5797 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5798 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5799 | `					}` |
|        2 |  5800 | `				}else{` |
|        - |  5801 | `					/* Make a copy of the attribute value */` |
|       15 |  5802 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5803 | `					if( pValue ){` |
|       15 |  5804 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5805 | `					}` |
|        - |  5806 | `				}` |
|        8 |  5807 | `			}` |
|        - |  5808 | `		}` |
|        - |  5809 | `	}` |
|   170760 |  5810 | `	break;` |
|        - |  5811 | `						  }` |
|        - |  5812 | `/*` |
|        - |  5813 | ` * OP_MEMBER P1 P2` |
|        - |  5814 | ` * Load class attribute/method on the stack.` |
|        - |  5815 | ` */` |
|     2386 |  5816 | `case PH7_OP_MEMBER: {` |
|        - |  5817 | `	ph7_class_instance *pThis;` |
|        - |  5818 | `	ph7_value *pNos;` |
|        - |  5819 | `	SyString sName;` |
|     4774 |  5820 | `	if( !pInstr->iP1 ){` |
|     4584 |  5821 | `		pNos = &pTos[-1];` |
|        - |  5822 | `#ifdef UNTRUST` |
|        - |  5823 | `		if( pNos < pStack ){` |
|        - |  5824 | `			goto Abort;` |
|        - |  5825 | `		}` |
|        - |  5826 | `#endif` |
|     4584 |  5827 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5828 | `			ph7_class *pClass;` |
|        - |  5829 | `			/* Class already instantiated */` |
|     4584 |  5830 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5831 | `			/* Point to the instantiated class */` |
|     4584 |  5832 | `			pClass = pThis->pClass;` |
|        - |  5833 | `			/* Extract attribute name first */` |
|     4584 |  5834 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4584 |  5835 | `			if( pInstr->iP2 ){` |
|        - |  5836 | `				/* Method call */` |
|      488 |  5837 | `				ph7_class_method *pMeth = 0;` |
|      488 |  5838 | `				if( sName.nByte > 0 ){` |
|        - |  5839 | `					/* Extract the target method */` |
|      488 |  5840 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      243 |  5841 | `				}` |
|      488 |  5842 | `				if( pMeth == 0 ){` |
|      ! 0 |  5843 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5844 | `						&pClass->sName,&sName` |
|        - |  5845 | `						);` |
|        - |  5846 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5847 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5848 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5849 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5850 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5851 | `				}else{` |
|        - |  5852 | `					/* Push method name on the stack */` |
|      488 |  5853 | `					PH7_MemObjRelease(pTos);` |
|      488 |  5854 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      488 |  5855 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5856 | `				}` |
|      488 |  5857 | `				pTos->nIdx = SXU32_HIGH;` |
|      245 |  5858 | `			}else{` |
|        - |  5859 | `				/* Attribute access */` |
|     4098 |  5860 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5861 | `				SyHashEntry *pEntry;` |
|        - |  5862 | `				/* Extract the target attribute */` |
|     4098 |  5863 | `				if( sName.nByte > 0 ){` |
|     4098 |  5864 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     4098 |  5865 | `					if( pEntry ){` |
|        - |  5866 | `						/* Point to the attribute value */` |
|     4096 |  5867 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2047 |  5868 | `					}` |
|     2048 |  5869 | `				}` |
|     4098 |  5870 | `				if( pObjAttr == 0 ){` |
|        - |  5871 | `					/* No such attribute,load null */` |
|        4 |  5872 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5873 | `						&pClass->sName,&sName);` |
|        - |  5874 | `					/* Call the __get magic method if available */` |
|        3 |  5875 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5876 | `				}` |
|     4098 |  5877 | `				VmPopOperand(&pTos,1);` |
|        - |  5878 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5879 | `				 * This is due to the following case:` |
|        - |  5880 | `				 *     (new TestClass())->foo;` |
|        - |  5881 | `				 */` |
|     4098 |  5882 | `				pThis->iRef++;` |
|     4098 |  5883 | `				PH7_MemObjRelease(pTos);` |
|     4098 |  5884 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     4098 |  5885 | `				if( pObjAttr ){` |
|     4096 |  5886 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5887 | `					/* Check attribute access */` |
|     4096 |  5888 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  5889 | `						/* Load attribute */` |
|     4096 |  5890 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     4096 |  5891 | `						if( pValue ){` |
|     4096 |  5892 | `							if( pThis->iRef < 2 ){` |
|        - |  5893 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5894 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5895 | `								 */` |
|        3 |  5896 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5897 | `							}else{` |
|        - |  5898 | `								/* Simple load */` |
|     4094 |  5899 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5900 | `							}` |
|     4096 |  5901 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     4094 |  5902 | `								if( pThis->iRef > 1 ){` |
|        - |  5903 | `									/* Load attribute index */` |
|     4092 |  5904 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2045 |  5905 | `								}` |
|     2046 |  5906 | `							}` |
|     2047 |  5907 | `						}` |
|     2049 |  5908 | `					}else{` |
|        - |  5909 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  5910 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  5911 | `						char zMsg[256];` |
|      ! 0 |  5912 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  5913 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  5914 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  5915 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  5916 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5917 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  5918 | `						goto Abort;` |
|        - |  5919 | `					}` |
|     2047 |  5920 | `				}` |
|        - |  5921 | `				/* Safely unreference the object */` |
|     4098 |  5922 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5923 | `			}` |
|     2293 |  5924 | `		}else{` |
|      ! 0 |  5925 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5926 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5927 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5928 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5929 | `		}` |
|     2293 |  5930 | `	}else{` |
|        - |  5931 | `		/* Static member access using class name */` |
|      192 |  5932 | `		pNos = pTos;` |
|      192 |  5933 | `		pThis = 0;` |
|      192 |  5934 | `		if( !pInstr->p3 ){` |
|      180 |  5935 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      180 |  5936 | `			pNos--;` |
|        - |  5937 | `#ifdef UNTRUST` |
|        - |  5938 | `			if( pNos < pStack ){` |
|        - |  5939 | `				goto Abort;` |
|        - |  5940 | `			}` |
|        - |  5941 | `#endif` |
|       91 |  5942 | `		}else{` |
|        - |  5943 | `			/* Attribute name already computed */` |
|       14 |  5944 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5945 | `		}` |
|      192 |  5946 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      192 |  5947 | `			ph7_class *pClass = 0;` |
|      192 |  5948 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5949 | `				/* Class already instantiated */` |
|        5 |  5950 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  5951 | `				pClass = pThis->pClass;` |
|        5 |  5952 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  5953 | `			}else{` |
|        - |  5954 | `				/* Try to extract the target class */` |
|      188 |  5955 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      188 |  5956 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      188 |  5957 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5958 | `					/* Handle self/static/parent keywords */` |
|      188 |  5959 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       56 |  5960 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       56 |  5961 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5962 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5963 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5964 | `						}` |
|      161 |  5965 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  5966 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      133 |  5967 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       26 |  5968 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       26 |  5969 | `						if( pSelf && pSelf->pBase ){` |
|       26 |  5970 | `							pClass = pSelf->pBase;` |
|       12 |  5971 | `						}` |
|       14 |  5972 | `					}else{` |
|       84 |  5973 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5974 | `					}` |
|       93 |  5975 | `				}` |
|        - |  5976 | `			}` |
|      192 |  5977 | `			if( pClass == 0 ){` |
|        - |  5978 | `				/* Undefined class */` |
|      ! 0 |  5979 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5980 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5981 | `					);` |
|      ! 0 |  5982 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5983 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5984 | `				}` |
|      ! 0 |  5985 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5986 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5987 | `			}else{` |
|      192 |  5988 | `				if( pInstr->iP2 ){` |
|        - |  5989 | `					/* Method call */` |
|       76 |  5990 | `					ph7_class_method *pMeth = 0;` |
|       76 |  5991 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5992 | `						/* Extract the target method */` |
|       76 |  5993 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       37 |  5994 | `					}` |
|       76 |  5995 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5996 | `						if( pMeth ){` |
|      ! 0 |  5997 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5998 | `								&pClass->sName,&sName` |
|        - |  5999 | `								);` |
|      ! 0 |  6000 | `						}else{` |
|      ! 0 |  6001 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6002 | `								&pClass->sName,&sName` |
|        - |  6003 | `								);` |
|        - |  6004 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  6005 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  6006 | `						}` |
|        - |  6007 | `						/* Pop the method name from the stack */` |
|      ! 0 |  6008 | `						if( !pInstr->p3 ){` |
|      ! 0 |  6009 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  6010 | `						}` |
|      ! 0 |  6011 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  6012 | `					}else{` |
|        - |  6013 | `						/* Push method name on the stack */` |
|       76 |  6014 | `						PH7_MemObjRelease(pTos);` |
|       76 |  6015 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       76 |  6016 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6017 | `					}` |
|       76 |  6018 | `					pTos->nIdx = SXU32_HIGH;` |
|       39 |  6019 | `				}else{` |
|        - |  6020 | `					/* Attribute access */` |
|      118 |  6021 | `					ph7_class_attr *pAttr = 0;` |
|        - |  6022 | `					/* Check for special ::class pseudo-constant */` |
|      153 |  6023 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       70 |  6024 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  6025 | `						/* ::class returns the fully qualified class name */` |
|        - |  6026 | `						/* Pop the attribute name from the stack */` |
|       60 |  6027 | `						if( !pInstr->p3 ){` |
|       60 |  6028 | `							VmPopOperand(&pTos,1);` |
|       29 |  6029 | `						}` |
|       60 |  6030 | `						PH7_MemObjRelease(pTos);` |
|        - |  6031 | `						/* Load the class name */` |
|       60 |  6032 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  6033 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  6034 | `					}else{` |
|        - |  6035 | `						/* Extract the target attribute */` |
|       60 |  6036 | `						if( sName.nByte > 0 ){` |
|       60 |  6037 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       29 |  6038 | `						}` |
|       60 |  6039 | `						if( pAttr == 0 ){` |
|        - |  6040 | `							/* No such attribute,load null */` |
|      ! 0 |  6041 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6042 | `								&pClass->sName,&sName);` |
|        - |  6043 | `							/* Call the __get magic method if available */` |
|      ! 0 |  6044 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  6045 | `						}` |
|        - |  6046 | `						/* Pop the attribute name from the stack */` |
|       60 |  6047 | `						if( !pInstr->p3 ){` |
|       48 |  6048 | `							VmPopOperand(&pTos,1);` |
|       23 |  6049 | `						}` |
|       60 |  6050 | `						PH7_MemObjRelease(pTos);` |
|       60 |  6051 | `						pTos->nIdx = SXU32_HIGH;` |
|       60 |  6052 | `						if( pAttr ){` |
|       60 |  6053 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  6054 | `								/* Access to a non static attribute */` |
|      ! 0 |  6055 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6056 | `									&pClass->sName,&pAttr->sName` |
|        - |  6057 | `									);` |
|      ! 0 |  6058 | `							}else{` |
|        - |  6059 | `								ph7_value *pValue;` |
|        - |  6060 | `								/* Check if the access to the attribute is allowed */` |
|       60 |  6061 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  6062 | `									/* Load the desired attribute */` |
|       56 |  6063 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       56 |  6064 | `									if( pValue ){` |
|       56 |  6065 | `										PH7_MemObjLoad(pValue,pTos);` |
|       56 |  6066 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  6067 | `											/* Load index number */` |
|       14 |  6068 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  6069 | `										}` |
|       27 |  6070 | `									}` |
|       29 |  6071 | `								}else{` |
|        - |  6072 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  6073 | `									char zMsg[256];` |
|        5 |  6074 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  6075 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  6076 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  6077 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  6078 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  6079 | `									}else{` |
|      ! 0 |  6080 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6081 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6082 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  6083 | `									}` |
|        5 |  6084 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  6085 | `									goto Abort;` |
|        - |  6086 | `								}` |
|        - |  6087 | `							}` |
|       27 |  6088 | `						}` |
|        - |  6089 | `					}` |
|        - |  6090 | `				}` |
|      188 |  6091 | `				if( pThis ){` |
|        - |  6092 | `					/* Safely unreference the object */` |
|        5 |  6093 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  6094 | `				}` |
|        - |  6095 | `			}` |
|       95 |  6096 | `		}else{` |
|        - |  6097 | `			/* Pop operands */` |
|      ! 0 |  6098 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  6099 | `			if( !pInstr->p3 ){` |
|      ! 0 |  6100 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  6101 | `			}` |
|      ! 0 |  6102 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6103 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6104 | `		}` |
|        - |  6105 | `	}` |
|     4770 |  6106 | `	break;` |
|        - |  6107 | `					}` |
|        - |  6108 | `/*` |
|        - |  6109 | ` * OP_NEW P1 * * *` |
|        - |  6110 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  6111 | ` */` |
|      358 |  6112 | `case PH7_OP_NEW: {` |
|      718 |  6113 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      718 |  6114 | `	ph7_class *pClass = 0;` |
|        - |  6115 | `	ph7_class_instance *pNew;` |
|      718 |  6116 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  6117 | `		/* Try to extract the desired class */` |
|     1076 |  6118 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      716 |  6119 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      358 |  6120 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6121 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6122 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6123 | `	}` |
|      718 |  6124 | `	if( pClass == 0 ){` |
|        - |  6125 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6126 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6127 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6128 | `			);` |
|        - |  6129 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6130 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6131 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6132 | `			/* Pop given arguments */` |
|      ! 0 |  6133 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6134 | `		}` |
|      ! 0 |  6135 | `		goto Abort;` |
|      ! 0 |  6136 | `	}else{` |
|        - |  6137 | `		ph7_class_method *pCons;` |
|        - |  6138 | `		/* Create a new class instance */` |
|      718 |  6139 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      718 |  6140 | `		if( pNew == 0 ){` |
|      ! 0 |  6141 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6142 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6143 | `				&pClass->sName` |
|        - |  6144 | `			);` |
|      ! 0 |  6145 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6146 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6147 | `				/* Pop given arguments */` |
|      ! 0 |  6148 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6149 | `			}` |
|      ! 0 |  6150 | `			break;` |
|        - |  6151 | `		}` |
|        - |  6152 | `		/* Check if a constructor is available */` |
|      718 |  6153 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      718 |  6154 | `		if( pCons == 0 ){` |
|      580 |  6155 | `			SyString *pName = &pClass->sName;` |
|        - |  6156 | `			/* Check for a constructor with the same base class name */` |
|      580 |  6157 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      289 |  6158 | `		}` |
|      718 |  6159 | `		if( pCons ){` |
|        - |  6160 | `			/* Call the class constructor */` |
|      140 |  6161 | `			SySetReset(&aArg);` |
|      270 |  6162 | `			while( pArg < pTos ){` |
|      132 |  6163 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      132 |  6164 | `				pArg++;` |
|        2 |  6165 | `			}` |
|      140 |  6166 | `			if( pVm->bErrReport ){` |
|        - |  6167 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6168 | `				sxu32 n;` |
|       57 |  6169 | `				n = SySetUsed(&aArg);` |
|        - |  6170 | `				/* Emit a notice for missing arguments */` |
|      101 |  6171 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6172 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6173 | `					if( pFuncArg ){` |
|       45 |  6174 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6175 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6176 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6177 | `						}` |
|       22 |  6178 | `					}` |
|       45 |  6179 | `					n++;` |
|        1 |  6180 | `				}` |
|       28 |  6181 | `			}` |
|      140 |  6182 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6183 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      140 |  6184 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6185 | `				pNew->iRef = 1;` |
|      ! 0 |  6186 | `			}` |
|       69 |  6187 | `		}` |
|      718 |  6188 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6189 | `			/* Pop given arguments */` |
|      122 |  6190 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       60 |  6191 | `		}` |
|      718 |  6192 | `		PH7_MemObjRelease(pTos);` |
|      718 |  6193 | `		pTos->x.pOther = pNew;` |
|      718 |  6194 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6195 | `	}` |
|      718 |  6196 | `	break;` |
|        - |  6197 | `				 }` |
|        - |  6198 | `/*` |
|        - |  6199 | ` * OP_CLONE * * *` |
|        - |  6200 | ` * Perfome a clone operation.` |
|        - |  6201 | ` */` |
|       23 |  6202 | `case PH7_OP_CLONE: {` |
|        - |  6203 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6204 | `#ifdef UNTRUST` |
|        - |  6205 | `	if( pTos < pStack ){` |
|        - |  6206 | `		goto Abort;` |
|        - |  6207 | `	}` |
|        - |  6208 | `#endif` |
|        - |  6209 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6210 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6211 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6212 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6213 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6214 | `		break;` |
|        - |  6215 | `	}` |
|        - |  6216 | `	/* Point to the source */` |
|       44 |  6217 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6218 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6219 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6220 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6221 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6222 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6223 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6224 | `		break;` |
|        - |  6225 | `	}` |
|        - |  6226 | `	/* Perform the clone operation */` |
|       44 |  6227 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6228 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6229 | `	if( pClone == 0 ){` |
|      ! 0 |  6230 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6231 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6232 | `	}else{` |
|        - |  6233 | `		/* Load the cloned object */` |
|       44 |  6234 | `		pTos->x.pOther = pClone;` |
|       44 |  6235 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6236 | `	}` |
|       44 |  6237 | `	break;` |
|        - |  6238 | `				   }` |
|        - |  6239 | `/*` |
|        - |  6240 | ` * OP_SWITCH * * P3` |
|        - |  6241 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6242 | ` */` |
|       26 |  6243 | `case PH7_OP_SWITCH: {` |
|       54 |  6244 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6245 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6246 | `	ph7_value sValue,sCaseValue;` |
|        - |  6247 | `	sxu32 n,nEntry;` |
|        - |  6248 | `#ifdef UNTRUST` |
|        - |  6249 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6250 | `		goto Abort;` |
|        - |  6251 | `	}` |
|        - |  6252 | `#endif` |
|        - |  6253 | `	/* Point to the case table  */` |
|       54 |  6254 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  6255 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6256 | `	/* Select the appropriate case block to execute */` |
|       54 |  6257 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  6258 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  6259 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  6260 | `		pCase = &aCase[n];` |
|      130 |  6261 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6262 | `		/* Execute the case expression first */` |
|      130 |  6263 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6264 | `		/* Compare the two expression */` |
|      130 |  6265 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  6266 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  6267 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  6268 | `		if( rc == 0 ){` |
|        - |  6269 | `			/* Value match,jump to this block */` |
|       52 |  6270 | `			pc = pCase->nStart - 1;` |
|       52 |  6271 | `			break;` |
|        - |  6272 | `		}` |
|       41 |  6273 | `	}` |
|       54 |  6274 | `	VmPopOperand(&pTos,1);` |
|       54 |  6275 | `	if( n >= nEntry ){` |
|        - |  6276 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  6277 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  6278 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  6279 | `		}else{` |
|        - |  6280 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6281 | `			pc = pSwitch->nOut - 1;` |
|        - |  6282 | `		}` |
|        1 |  6283 | `	}` |
|       54 |  6284 | `	break;` |
|        - |  6285 | `					}` |
|        - |  6286 | `/*` |
|        - |  6287 | ` * OP_YIELD P1 P2 *` |
|        - |  6288 | ` *  Yield a value from a generator function.` |
|        - |  6289 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6290 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6291 | ` */` |
|       28 |  6292 | `case PH7_OP_YIELD: {` |
|        - |  6293 | `	ph7_generator *pGen;` |
|       58 |  6294 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6295 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6296 | `		goto Abort;` |
|        - |  6297 | `	}` |
|       58 |  6298 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6299 | `	if( pInstr->iP2 ){` |
|        - |  6300 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6301 | `#ifdef UNTRUST` |
|        - |  6302 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6303 | `#endif` |
|        7 |  6304 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6305 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6306 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6307 | `		VmPopOperand(&pTos, 1);` |
|        - |  6308 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6309 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6310 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6311 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6312 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6313 | `			}` |
|        1 |  6314 | `		}` |
|       55 |  6315 | `	}else if( pInstr->iP1 ){` |
|        - |  6316 | `		/* yield $value */` |
|        - |  6317 | `#ifdef UNTRUST` |
|        - |  6318 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6319 | `#endif` |
|       52 |  6320 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6321 | `		VmPopOperand(&pTos, 1);` |
|        - |  6322 | `		/* Auto-increment key */` |
|       52 |  6323 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6324 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6325 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6326 | `	}else{` |
|        - |  6327 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6328 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6329 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6330 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6331 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6332 | `	}` |
|        - |  6333 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6334 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6335 | `	goto Suspend;` |
|        - |  6336 |  |
|        - |  6337 | `/*` |
|        - |  6338 | ` * OP_CALL P1 * *` |
|        - |  6339 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6340 | ` *  function on the stack.` |
|        - |  6341 | ` */` |
|   305842 |  6342 | `case PH7_OP_CALL: {` |
|   611730 |  6343 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6344 | `	ph7_value *pArg;` |
|   611730 |  6345 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   611730 |  6346 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6347 | `	SyHashEntry *pEntry;` |
|        - |  6348 | `	SyString sName;` |
|        - |  6349 | `	/* Extract function name */` |
|   611730 |  6350 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6351 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6352 | `			ph7_value sResult;` |
|      ! 0 |  6353 | `			SySetReset(&aArg);` |
|      ! 0 |  6354 | `			while( pArg < pTos ){` |
|      ! 0 |  6355 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6356 | `				pArg++;` |
|      ! 0 |  6357 | `			}` |
|      ! 0 |  6358 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6359 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6360 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6361 | `			SySetReset(&aArg);` |
|        - |  6362 | `			/* Pop given arguments */` |
|      ! 0 |  6363 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6364 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6365 | `			}` |
|        - |  6366 | `			/* Copy result */` |
|      ! 0 |  6367 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6368 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6369 | `		}else{` |
|        3 |  6370 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6371 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6372 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6373 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6374 | `			}else{` |
|        - |  6375 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6376 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6377 | `			}` |
|        - |  6378 | `			/* Pop given arguments */` |
|        3 |  6379 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6380 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6381 | `			}` |
|        - |  6382 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6383 | `			PH7_MemObjRelease(pTos);` |
|        - |  6384 | `		}` |
|   305564 |  6385 | `		break;` |
|        - |  6386 | `	}` |
|   611728 |  6387 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6388 | `	/* Check for a compiled function first.` |
|        - |  6389 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6390 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   611728 |  6391 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6392 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6393 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6394 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6395 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6396 | `	 * function calls inside namespaces. */` |
|   611728 |  6397 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6398 | `		const char *zFunc;` |
|        - |  6399 | `		const char *zEnd;` |
|        - |  6400 | `		const char *z;` |
|        - |  6401 | `		SyString sGlobal;` |
|       18 |  6402 | `		zFunc = sName.zString;` |
|       18 |  6403 | `		zEnd  = zFunc + sName.nByte;` |
|       18 |  6404 | `		z = zEnd;` |
|        - |  6405 | `		/* Find last namespace separator */` |
|      154 |  6406 | `		while( z > zFunc ){` |
|      154 |  6407 | `			if( z[-1] == '\\' ){` |
|       18 |  6408 | `				break;` |
|        - |  6409 | `			}` |
|      138 |  6410 | `			z--;` |
|        2 |  6411 | `		}` |
|       18 |  6412 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6413 | `			/* Retry lookup using the unqualified/global function name */` |
|       18 |  6414 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       18 |  6415 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        8 |  6416 | `		}` |
|        8 |  6417 | `	}` |
|   611728 |  6418 | `	if( pEntry ){` |
|        - |  6419 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6420 | `		ph7_class_instance *pThis;` |
|        - |  6421 | `		ph7_value *pFrameStack;` |
|        - |  6422 | `		ph7_vm_func *pVmFunc;` |
|        - |  6423 | `		ph7_class *pSelf;` |
|        - |  6424 | `		VmFrame *pFrame;` |
|        - |  6425 | `		ph7_value *pObj;` |
|        - |  6426 | `		VmSlot sArg;` |
|        - |  6427 | `		sxu32 n;` |
|        - |  6428 | `		/* initialize fields */` |
|    13826 |  6429 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13826 |  6430 | `		pThis = 0;` |
|    13826 |  6431 | `		pSelf = 0;` |
|    13826 |  6432 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6433 | `			ph7_class_method *pMeth;` |
|        - |  6434 | `			/* Class method call */` |
|     2104 |  6435 | `			ph7_value *pTarget = &pTos[-1];` |
|     2104 |  6436 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6437 | `				/* Extract the 'this' pointer */` |
|     2104 |  6438 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6439 | `					/* Instance already loaded */` |
|     2024 |  6440 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2024 |  6441 | `					pThis->iRef++;` |
|     2024 |  6442 | `					pSelf = pThis->pClass;` |
|     1011 |  6443 | `				}` |
|     2104 |  6444 | `				if( pSelf == 0 ){` |
|       82 |  6445 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6446 | `						/* "Late Static Binding" class name */` |
|      113 |  6447 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       37 |  6448 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       37 |  6449 | `					}` |
|       82 |  6450 | `					if( pSelf == 0 ){` |
|       19 |  6451 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        9 |  6452 | `					}` |
|       40 |  6453 | `				}` |
|     2104 |  6454 | `				if( pThis == 0  ){` |
|       82 |  6455 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       82 |  6456 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       82 |  6457 | `					if( pFrameLocal->pParent ){` |
|        - |  6458 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       64 |  6459 | `						pThis = pFrameLocal->pThis;` |
|       64 |  6460 | `						if( pThis ){` |
|       19 |  6461 | `							pThis->iRef++;` |
|        9 |  6462 | `						}` |
|       31 |  6463 | `					}` |
|       40 |  6464 | `				}` |
|     2104 |  6465 | `				VmPopOperand(&pTos,1);` |
|     2104 |  6466 | `				PH7_MemObjRelease(pTos);` |
|        - |  6467 | `				/* Synchronize pointers */` |
|     2104 |  6468 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6469 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6470 | `				 * user have already computed the random generated unique class method name` |
|        - |  6471 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6472 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6473 | `				 */` |
|     2104 |  6474 | `				while( pArg < pStack ){` |
|      ! 0 |  6475 | `					pArg++;` |
|      ! 0 |  6476 | `				}` |
|     2104 |  6477 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6478 | `					/* Check if the call is allowed */` |
|     2104 |  6479 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2104 |  6480 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  6481 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  6482 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  6483 | `							char zMsg[256];` |
|      ! 0 |  6484 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6485 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  6486 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  6487 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  6488 | `							/* Pop given arguments */` |
|      ! 0 |  6489 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6490 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6491 | `							}` |
|      ! 0 |  6492 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6493 | `							goto Abort;` |
|        - |  6494 | `						}` |
|        6 |  6495 | `					}` |
|     1051 |  6496 | `				}` |
|     1051 |  6497 | `			}` |
|     1051 |  6498 | `		}` |
|        - |  6499 | `		/* Check The recursion limit */` |
|    13826 |  6500 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6501 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6502 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6503 | `				&pVmFunc->sName);` |
|        - |  6504 | `			/* Pop given arguments */` |
|        3 |  6505 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6506 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6507 | `			}` |
|        - |  6508 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6509 | `			PH7_MemObjRelease(pTos);` |
|       12 |  6510 | `			break;` |
|        - |  6511 | `		}` |
|    13824 |  6512 | `		if( pVmFunc->pNextName ){` |
|        - |  6513 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  6514 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  6515 | `		}` |
|    13824 |  6516 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6517 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6518 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6519 | `			ph7_generator *pGenerator;` |
|        - |  6520 | `			ph7_class_instance *pGenObj;` |
|        - |  6521 | `			ph7_value *pCtxAttr;` |
|        - |  6522 | `			SyString sAttrName;` |
|        - |  6523 | `			ph7_value **apCallArgs;` |
|        - |  6524 | `			int nGenArgs, iArg;` |
|        - |  6525 | `			/* Collect arguments from the operand stack */` |
|       20 |  6526 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6527 | `			apCallArgs = 0;` |
|       20 |  6528 | `			if( nGenArgs > 0 ){` |
|        8 |  6529 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6530 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6531 | `				if( apCallArgs == 0 ){` |
|        - |  6532 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6533 | `					nGenArgs = 0;` |
|      ! 0 |  6534 | `				}else{` |
|       12 |  6535 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6536 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6537 | `					}` |
|        - |  6538 | `				}` |
|        2 |  6539 | `			}` |
|        - |  6540 | `			/* Create execution context and generator wrapper */` |
|       20 |  6541 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6542 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6543 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6544 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6545 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6546 | `				break;` |
|        - |  6547 | `			}` |
|       20 |  6548 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6549 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6550 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6551 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6552 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6553 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6554 | `				break;` |
|        - |  6555 | `			}` |
|        - |  6556 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6557 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6558 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6559 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6560 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6561 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6562 | `			if( apCallArgs ){` |
|        6 |  6563 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6564 | `			}` |
|       20 |  6565 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6566 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6567 | `				if( pThis ){` |
|      ! 0 |  6568 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6569 | `				}` |
|      ! 0 |  6570 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6571 | `					goto Abort;` |
|        - |  6572 | `				}` |
|      ! 0 |  6573 | `				break;` |
|        - |  6574 | `			}` |
|        - |  6575 | `			/* Create Generator class instance */` |
|       20 |  6576 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6577 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6578 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6579 | `				break;` |
|        - |  6580 | `			}` |
|        - |  6581 | `			/* Store generator in __ctx attribute */` |
|       20 |  6582 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  6583 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  6584 | `			if( pCtxAttr ){` |
|       20 |  6585 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  6586 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6587 | `			}` |
|        - |  6588 | `			/* Pop args and function name, push Generator object */` |
|       20 |  6589 | `			PH7_MemObjRelease(pTos);` |
|       20 |  6590 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  6591 | `			pTos->x.pOther = pGenObj;` |
|       20 |  6592 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  6593 | `			pGenObj->iRef++;` |
|       20 |  6594 | `			if( pThis ){` |
|      ! 0 |  6595 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6596 | `			}` |
|       20 |  6597 | `			break;` |
|        - |  6598 | `		}` |
|        - |  6599 | `		/* Extract the formal argument set */` |
|    13806 |  6600 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6601 | `		/* Create a new VM frame  */` |
|    13806 |  6602 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13806 |  6603 | `		if( rc != SXRET_OK ){` |
|        - |  6604 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6605 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6606 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6607 | `				&pVmFunc->sName);` |
|        - |  6608 | `			/* Pop given arguments */` |
|      ! 0 |  6609 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6610 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6611 | `			}` |
|        - |  6612 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6613 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6614 | `			break;` |
|        - |  6615 | `		}` |
|    13806 |  6616 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6617 | `			/* Install the '$this' variable */` |
|        - |  6618 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2040 |  6619 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2040 |  6620 | `			if( pObj ){` |
|        - |  6621 | `				/* Reflect the change */` |
|     2040 |  6622 | `				pObj->x.pOther = pThis;` |
|     2040 |  6623 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1019 |  6624 | `			}` |
|     1019 |  6625 | `		}` |
|    13806 |  6626 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6627 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6628 | `			/* Install static variables */` |
|      ! 0 |  6629 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6630 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6631 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6632 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6633 | `					/* Initialize the static variables */` |
|      ! 0 |  6634 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6635 | `					if( pObj ){` |
|        - |  6636 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6637 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6638 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6639 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6640 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6641 | `						}` |
|      ! 0 |  6642 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6643 | `					}else{` |
|      ! 0 |  6644 | `						continue;` |
|        - |  6645 | `					}` |
|      ! 0 |  6646 | `				}` |
|        - |  6647 | `				/* Install in the current frame */` |
|      ! 0 |  6648 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6649 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6650 | `			}` |
|      ! 0 |  6651 | `		}` |
|        - |  6652 | `		/* Push arguments in the local frame */` |
|    13806 |  6653 | `		n = 0;` |
|    37296 |  6654 | `		while( pArg < pTos ){` |
|    23528 |  6655 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6656 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       28 |  6657 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       28 |  6658 | `				if( pObj ){` |
|        - |  6659 | `					/* Initialize as empty array */` |
|       28 |  6660 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6661 | `					{` |
|       28 |  6662 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      104 |  6663 | `						while( pArg < pTos ){` |
|        - |  6664 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  6665 | `							 * Nullable types (?type) allow null through without coercion. */` |
|       92 |  6666 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  6667 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  6668 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6669 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  6670 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  6671 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  6672 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  6673 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  6674 | `										goto Abort;` |
|        - |  6675 | `									}` |
|        - |  6676 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  6677 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  6678 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  6679 | `									pFrameStack = 0;` |
|      ! 0 |  6680 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  6681 | `									goto SkipFuncBody;` |
|      ! 0 |  6682 | `								}else{` |
|       13 |  6683 | `									ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6684 | `									if( xCast ){` |
|       13 |  6685 | `										xCast(pArg);` |
|        6 |  6686 | `									}` |
|        - |  6687 | `								}` |
|        6 |  6688 | `							}` |
|       78 |  6689 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       78 |  6690 | `							pArg++;` |
|        2 |  6691 | `						}` |
|        - |  6692 | `					}` |
|       28 |  6693 | `					sArg.nIdx = pObj->nIdx;` |
|       28 |  6694 | `					sArg.pUserData = 0;` |
|       28 |  6695 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6696 | `				}` |
|       28 |  6697 | `				break; /* All remaining args consumed */` |
|        - |  6698 | `			}` |
|    23502 |  6699 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    23346 |  6700 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       11 |  6701 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6702 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6703 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6704 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6705 | `						goto Abort;` |
|        - |  6706 | `					}` |
|      ! 0 |  6707 | `				}` |
|        - |  6708 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6709 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    23360 |  6710 | `				if( aFormalArg[n].nType > 0` |
|    12272 |  6711 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1182 |  6712 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6713 | `						/* Argument must be a class instance [i.e: object] */` |
|       16 |  6714 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6715 | `						ph7_class *pClass;` |
|        - |  6716 | `						/* Try to extract the desired class */` |
|       16 |  6717 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       16 |  6718 | `						if( pClass ){` |
|       16 |  6719 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6720 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6721 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6722 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6723 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6724 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6725 | `								}` |
|      ! 0 |  6726 | `							}else{` |
|        - |  6727 | `								/* reuse pThis declared in outer scope */` |
|       16 |  6728 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6729 | `								/* Make sure the object is an instance of the given class */` |
|       16 |  6730 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6731 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6732 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6733 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6734 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6735 | `								}` |
|        - |  6736 | `							}` |
|        9 |  6737 | `						}` |
|     1175 |  6738 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       11 |  6739 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  6740 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  6741 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  6742 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  6743 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  6744 | `								goto Abort;` |
|        - |  6745 | `							}` |
|        - |  6746 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  6747 | `							PH7_MemObjRelease(pTos);` |
|       11 |  6748 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  6749 | `							pFrameStack = 0;` |
|       11 |  6750 | `							rc = PH7_EXCEPTION;` |
|       11 |  6751 | `							goto SkipFuncBody;` |
|      ! 0 |  6752 | `						}else{` |
|      ! 0 |  6753 | `							ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6754 | `							/* Cast to the desired type */` |
|      ! 0 |  6755 | `							xCast(pArg);` |
|        - |  6756 | `						}` |
|      ! 0 |  6757 | `					}` |
|      585 |  6758 | `				}` |
|    23338 |  6759 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6760 | `					/* Pass by reference */` |
|       54 |  6761 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6762 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6763 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6764 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6765 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6766 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6767 | `						}` |
|        - |  6768 | `						/* Switch to pass by value */` |
|      ! 0 |  6769 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6770 | `					}else{` |
|        - |  6771 | `						SyHashEntry *pRefEntry;` |
|        - |  6772 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6773 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6774 | `						if( pRefEntry == 0 ){` |
|       80 |  6775 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6776 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6777 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6778 | `							sArg.pUserData = 0;` |
|       54 |  6779 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6780 | `						}` |
|       54 |  6781 | `						pObj = 0;` |
|        - |  6782 | `					}` |
|       28 |  6783 | `				}else{` |
|        - |  6784 | `					/* Pass by value,make a copy of the given argument */` |
|    23286 |  6785 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6786 | `				}` |
|    11670 |  6787 | `			}else{` |
|        - |  6788 | `				char zName[32];` |
|        - |  6789 | `				SyString sArgName;` |
|        - |  6790 | `				/* Set a dummy name */` |
|      156 |  6791 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6792 | `				sArgName.zString = zName;` |
|        - |  6793 | `				/* Annonymous argument */` |
|      156 |  6794 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6795 | `			}` |
|    23492 |  6796 | `			if( pObj ){` |
|    23440 |  6797 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6798 | `				/* Insert argument index  */` |
|    23440 |  6799 | `				sArg.nIdx = pObj->nIdx;` |
|    23440 |  6800 | `				sArg.pUserData = 0;` |
|    23440 |  6801 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11719 |  6802 | `			}` |
|    23492 |  6803 | `			PH7_MemObjRelease(pArg);` |
|    23492 |  6804 | `			pArg++;` |
|    23492 |  6805 | `			++n;` |
|        2 |  6806 | `		}` |
|        - |  6807 | `		/* Set up closure environment */` |
|    13796 |  6808 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6809 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6810 | `			ph7_value *pValue;` |
|        - |  6811 | `			sxu32 iEnv;` |
|       13 |  6812 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       39 |  6813 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       27 |  6814 | `				pEnv = &aEnv[iEnv];` |
|       27 |  6815 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6816 | `					/* Do not install null value */` |
|       13 |  6817 | `					continue;` |
|        - |  6818 | `				}` |
|       15 |  6819 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       15 |  6820 | `				if( pValue == 0 ){` |
|      ! 0 |  6821 | `					continue;` |
|        - |  6822 | `				}` |
|        - |  6823 | `				/* Invalidate any prior representation */` |
|       15 |  6824 | `				PH7_MemObjRelease(pValue);` |
|        - |  6825 | `				/* Duplicate bound variable value */` |
|       15 |  6826 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        8 |  6827 | `			}` |
|        6 |  6828 | `		}` |
|        - |  6829 | `		/* Process default values for remaining formal parameters */` |
|    15836 |  6830 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2074 |  6831 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6832 | `				/* Variadic parameter with no extra args — create empty array */` |
|       34 |  6833 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       34 |  6834 | `				if( pObj ){` |
|       34 |  6835 | `					PH7_MemObjToHashmap(pObj);` |
|       34 |  6836 | `					sArg.nIdx = pObj->nIdx;` |
|       34 |  6837 | `					sArg.pUserData = 0;` |
|       34 |  6838 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       16 |  6839 | `				}` |
|       34 |  6840 | `				n++;` |
|       34 |  6841 | `				break; /* Variadic is always last */` |
|        - |  6842 | `			}` |
|     2042 |  6843 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2036 |  6844 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2036 |  6845 | `				if( pObj ){` |
|        - |  6846 | `					/* Evaluate the default value and extract it's result */` |
|     2036 |  6847 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2036 |  6848 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6849 | `						goto Abort;` |
|        - |  6850 | `					}` |
|        - |  6851 | `					/* Insert argument index */` |
|     2036 |  6852 | `					sArg.nIdx = pObj->nIdx;` |
|     2036 |  6853 | `					sArg.pUserData = 0;` |
|     2036 |  6854 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6855 | `					/* Make sure the default argument is of the correct type */` |
|     2034 |  6856 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1444 |  6857 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6858 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6859 | `						/* Cast to the desired type */` |
|      ! 0 |  6860 | `						xCast(pObj);` |
|      ! 0 |  6861 | `					}` |
|     1017 |  6862 | `				}` |
|     1017 |  6863 | `			}` |
|     2042 |  6864 | `			++n;` |
|        2 |  6865 | `		}` |
|        - |  6866 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6867 | `		 * does not return anything.` |
|        - |  6868 | `		 */` |
|    13796 |  6869 | `		PH7_MemObjRelease(pTos);` |
|    13796 |  6870 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6871 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13796 |  6872 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13796 |  6873 | `		if( pFrameStack == 0 ){` |
|        - |  6874 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6875 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6876 | `				&pVmFunc->sName);` |
|      ! 0 |  6877 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6878 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6879 | `			}` |
|      ! 0 |  6880 | `			break;` |
|        - |  6881 | `		}` |
|     6897 |  6882 | `SkipFuncBody:` |
|    13806 |  6883 | `		if( pSelf ){` |
|        - |  6884 | `			/* Push class name */` |
|     2102 |  6885 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1050 |  6886 | `		}` |
|        - |  6887 | `		/* Increment nesting level */` |
|    13806 |  6888 | `		pVm->nRecursionDepth++;` |
|    13806 |  6889 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  6890 | `			/* Execute function body */` |
|    13796 |  6891 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|     6897 |  6892 | `		}` |
|        - |  6893 | `		/* Decrement nesting level */` |
|    13806 |  6894 | `		pVm->nRecursionDepth--;` |
|    13806 |  6895 | `		if( pSelf ){` |
|        - |  6896 | `			/* Pop class name */` |
|     2102 |  6897 | `			(void)SySetPop(&pVm->aSelf);` |
|     1050 |  6898 | `		}` |
|        - |  6899 | `		/* Cleanup the mess left behind */` |
|    13806 |  6900 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6901 | `			/* Return by reference,reflect that */` |
|        9 |  6902 | `			if( n != SXU32_HIGH ){` |
|        9 |  6903 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6904 | `				sxu32 i;` |
|        - |  6905 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6906 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6907 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6908 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6909 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6910 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6911 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6912 | `								&pVmFunc->sName);` |
|      ! 0 |  6913 | `						}` |
|      ! 0 |  6914 | `						n = SXU32_HIGH;` |
|      ! 0 |  6915 | `						break;` |
|        - |  6916 | `					}` |
|        3 |  6917 | `				}` |
|        5 |  6918 | `			}else{` |
|      ! 0 |  6919 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6920 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6921 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6922 | `						&pVmFunc->sName);` |
|      ! 0 |  6923 | `				}` |
|        - |  6924 | `			}` |
|        9 |  6925 | `			pTos->nIdx = n;` |
|        4 |  6926 | `		}` |
|        - |  6927 | `		/* Cleanup the mess left behind */` |
|    13806 |  6928 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6929 | `			/* An exception was throw in this frame */` |
|       22 |  6930 | `			pFrame = pFrame->pParent;` |
|       22 |  6931 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6932 | `				/* Pop the resutlt */` |
|       20 |  6933 | `				VmPopOperand(&pTos,1);` |
|        - |  6934 | `				/* Jump to this destination */` |
|       20 |  6935 | `				pc = pFrame->iExceptionJump - 1;` |
|       20 |  6936 | `				rc = PH7_OK;` |
|       11 |  6937 | `			}else{` |
|        3 |  6938 | `				if( pFrame->pParent ){` |
|        3 |  6939 | `					rc = PH7_EXCEPTION;` |
|        2 |  6940 | `				}else{` |
|        - |  6941 | `					/* Continue normal execution */` |
|      ! 0 |  6942 | `					rc = PH7_OK;` |
|        - |  6943 | `				}` |
|        - |  6944 | `			}` |
|       10 |  6945 | `		}` |
|        - |  6946 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    13806 |  6947 | `		if( pFrameStack ){` |
|    13796 |  6948 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     6897 |  6949 | `		}` |
|        - |  6950 | `		/* Leave the frame */` |
|    13806 |  6951 | `		VmLeaveFrame(&(*pVm));` |
|    13806 |  6952 | `		if( rc == PH7_ABORT ){` |
|        - |  6953 | `			/* Abort processing immeditaley */` |
|        9 |  6954 | `			goto Abort;` |
|    13798 |  6955 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6956 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6957 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6958 | `			 * overwriting the state saved by the inner level.` |
|        - |  6959 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6960 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  6961 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  6962 | `			goto Suspend;` |
|    13760 |  6963 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6964 | `			goto Exception;` |
|        - |  6965 | `		}` |
|     6880 |  6966 | `	}else{` |
|        - |  6967 | `		ph7_user_func *pFunc;` |
|        - |  6968 | `		ph7_context sCtx;` |
|        - |  6969 | `		ph7_value sRet;` |
|        - |  6970 | `		/* Look for an installed foreign function.` |
|        - |  6971 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6972 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6973 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6974 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6975 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6976 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   597904 |  6977 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   597904 |  6978 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6979 | `			/* Compiler-qualified: try short name as global fallback */` |
|       18 |  6980 | `			const char *zShort = sName.zString;` |
|        - |  6981 | `			sxu32 i;` |
|      262 |  6982 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      246 |  6983 | `				if( sName.zString[i] == '\\' ){` |
|       22 |  6984 | `					zShort = &sName.zString[i + 1];` |
|       10 |  6985 | `				}` |
|      124 |  6986 | `			}` |
|       18 |  6987 | `			if( zShort != sName.zString ){` |
|       18 |  6988 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       18 |  6989 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        8 |  6990 | `			}` |
|        8 |  6991 | `		}` |
|   597904 |  6992 | `		if( pEntry == 0 ){` |
|        - |  6993 | `			/* Call to undefined function */` |
|        5 |  6994 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6995 | `			/* Pop given arguments */` |
|        5 |  6996 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6997 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6998 | `			}` |
|        - |  6999 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  7000 | `			PH7_MemObjRelease(pTos);` |
|        8 |  7001 | `			break;` |
|        - |  7002 | `		}` |
|   597900 |  7003 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  7004 | `		/* Start collecting function arguments */` |
|   597900 |  7005 | `		SySetReset(&aArg);` |
|  1606926 |  7006 | `		while( pArg < pTos ){` |
|  1009028 |  7007 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1009028 |  7008 | `			pArg++;` |
|        2 |  7009 | `		}` |
|        - |  7010 | `		/* Assume a null return value */` |
|   597900 |  7011 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  7012 | `		/* Init the call context */` |
|   597900 |  7013 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  7014 | `		/* Call the foreign function */` |
|   597900 |  7015 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  7016 | `		/* Release the call context */` |
|   597900 |  7017 | `		VmReleaseCallContext(&sCtx);` |
|   597900 |  7018 | `		if( rc == PH7_ABORT ){` |
|      471 |  7019 | `			goto Abort;` |
|   597430 |  7020 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  7021 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  7022 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  7023 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  7024 | `				/* Exception was NOT caught, propagate */` |
|        5 |  7025 | `				goto Exception;` |
|        - |  7026 | `			}` |
|        - |  7027 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  7028 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  7029 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  7030 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  7031 | `			}` |
|        - |  7032 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  7033 | `			VmPopOperand(&pTos,1);` |
|        - |  7034 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  7035 | `			pFrm = pVm->pFrame;` |
|        7 |  7036 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  7037 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  7038 | `			}` |
|        7 |  7039 | `			break;` |
|        - |  7040 | `		}` |
|   597420 |  7041 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  7042 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  7043 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  7044 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  7045 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  7046 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  7047 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  7048 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  7049 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  7050 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  7051 | `			}` |
|        - |  7052 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  7053 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  7054 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  7055 | `			goto Suspend;` |
|        - |  7056 | `		}` |
|   597382 |  7057 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7058 | `			/* Pop function name and arguments */` |
|   578370 |  7059 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   289206 |  7060 | `		}` |
|        - |  7061 | `		/* Save foreign function return value */` |
|   597382 |  7062 | `		PH7_MemObjStore(&sRet,pTos);` |
|   597382 |  7063 | `		PH7_MemObjRelease(&sRet);` |
|        - |  7064 | `	}` |
|   611138 |  7065 | `	break;` |
|        - |  7066 | `				  }` |
|        - |  7067 | `/*` |
|        - |  7068 | ` * OP_CONSUME: P1 * *` |
|        - |  7069 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  7070 | ` */` |
|    12254 |  7071 | `case PH7_OP_CONSUME: {` |
|    24510 |  7072 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    24510 |  7073 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  7074 |  |
|    24510 |  7075 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    24510 |  7076 | `	pCur = pOut;` |
|        - |  7077 | `	/* Start the consume process  */` |
|    49018 |  7078 | `	while( pOut <= pTos ){` |
|        - |  7079 | `		/* Force a string cast */` |
|    24510 |  7080 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  7081 | `			PH7_MemObjToString(pOut);` |
|      149 |  7082 | `		}` |
|    24510 |  7083 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  7084 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  7085 | `			/* Invoke the output consumer callback */` |
|    13820 |  7086 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    13820 |  7087 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    13820 |  7088 | `			SyBlobRelease(&pOut->sBlob);` |
|    13820 |  7089 | `			if( rc == SXERR_ABORT ){` |
|        - |  7090 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  7091 | `				goto Abort;` |
|        - |  7092 | `			}` |
|     6909 |  7093 | `		}` |
|    24510 |  7094 | `		pOut++;` |
|        2 |  7095 | `	}` |
|    24510 |  7096 | `	pTos = &pCur[-1];` |
|    24508 |  7097 | `	break;` |
|        - |  7098 | `					 }` |
|        - |  7099 |  |
|        - |  7100 | `		} /* Switch() */` |
| 10296784 |  7101 | `		pc++; /* Next instruction in the stream */` |
|        2 |  7102 | `	} /* For(;;) */` |
|    16785 |  7103 | `Done:` |
|    33572 |  7104 | `	SySetRelease(&aArg);` |
|    33572 |  7105 | `	return SXRET_OK;` |
|       66 |  7106 | `Suspend:` |
|      134 |  7107 | `	SySetRelease(&aArg);` |
|      134 |  7108 | `	return PH7_SUSPEND;` |
|      245 |  7109 | `Abort:` |
|      491 |  7110 | `	SySetRelease(&aArg);` |
|     1697 |  7111 | `	while( pTos >= pStack ){` |
|     1207 |  7112 | `		PH7_MemObjRelease(pTos);` |
|     1207 |  7113 | `		pTos--;` |
|        1 |  7114 | `	}` |
|      491 |  7115 | `	return PH7_ABORT;` |
|        3 |  7116 | `Exception:` |
|        8 |  7117 | `	SySetRelease(&aArg);` |
|       22 |  7118 | `	while( pTos >= pStack ){` |
|       16 |  7119 | `		PH7_MemObjRelease(pTos);` |
|       16 |  7120 | `		pTos--;` |
|        2 |  7121 | `	}` |
|        8 |  7122 | `	return PH7_EXCEPTION;` |
|    17101 |  7123 |  |
|        - |  7124 | `/*` |
|        - |  7125 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  7126 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7127 | ` * See block-comment on that function for additional information.` |
|        - |  7128 | ` */` |
|    15868 |  7129 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  7130 |  |
|        - |  7131 | `	ph7_value *pStack;` |
|        - |  7132 | `	sxi32 rc;` |
|        - |  7133 | `	/* Allocate a new operand stack */` |
|    15870 |  7134 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15870 |  7135 | `	if( pStack == 0 ){` |
|      ! 0 |  7136 | `		return SXERR_MEM;` |
|        - |  7137 | `	}` |
|        - |  7138 | `	/* Execute the program */` |
|    15870 |  7139 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  7140 | `	/* Free the operand stack */` |
|    15870 |  7141 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  7142 | `	/* Execution result */` |
|    15870 |  7143 | `	return rc;` |
|     7936 |  7144 |  |
|        - |  7145 | `/*` |
|        - |  7146 | ` * Invoke any installed shutdown callbacks.` |
|        - |  7147 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  7148 | ` * or more calls to [register_shutdown_function()].` |
|        - |  7149 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  7150 | ` * execution ends.` |
|        - |  7151 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  7152 | ` * additional information.` |
|        - |  7153 | ` */` |
|     2394 |  7154 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  7155 |  |
|        - |  7156 | `	VmShutdownCB *pEntry;` |
|        - |  7157 | `	ph7_value *apArg[10];` |
|        - |  7158 | `	sxu32 n,nEntry;` |
|        - |  7159 | `	int i;` |
|        - |  7160 | `	/* Point to the stack of registered callbacks */` |
|     2396 |  7161 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    26336 |  7162 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23942 |  7163 | `		apArg[i] = 0;` |
|    11972 |  7164 | `	}` |
|     2398 |  7165 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  7166 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7167 | `		if( pEntry ){` |
|        - |  7168 | `			/* Prepare callback arguments if any */` |
|        3 |  7169 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  7170 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  7171 | `					break;` |
|        - |  7172 | `				}` |
|      ! 0 |  7173 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  7174 | `			}` |
|        - |  7175 | `			/* Invoke the callback */` |
|        3 |  7176 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  7177 | `			/*` |
|        - |  7178 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  7179 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  7180 | `			 */` |
|        3 |  7181 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7182 | `			if( pEntry ){` |
|        3 |  7183 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  7184 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  7185 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  7186 | `				}` |
|        1 |  7187 | `			}` |
|        1 |  7188 | `		}` |
|        2 |  7189 | `	}` |
|     2396 |  7190 | `	SySetReset(&pVm->aShutdown);` |
|     2396 |  7191 |  |
|        - |  7192 | `/*` |
|        - |  7193 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  7194 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7195 | ` * See block-comment on that function for additional information.` |
|        - |  7196 | ` */` |
|     2402 |  7197 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  7198 |  |
|        - |  7199 | `	/* Make sure we are ready to execute this program */` |
|     2404 |  7200 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  7201 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  7202 | `	}` |
|        - |  7203 | `	/* Set the execution magic number  */` |
|     2404 |  7204 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  7205 | `	/* Execute the program */` |
|     2404 |  7206 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  7207 | `	/* Invoke any shutdown callbacks */` |
|     2400 |  7208 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  7209 | `	/*` |
|        - |  7210 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  7211 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  7212 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  7213 | `	 */` |
|     2400 |  7214 | `	return SXRET_OK;` |
|     1203 |  7215 |  |
|        - |  7216 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  7217 | `/*` |
|        - |  7218 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  7219 | ` * The context is in CREATED state and ready to be started.` |
|        - |  7220 | ` */` |
|       42 |  7221 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  7222 |  |
|        - |  7223 | `	ph7_exec_ctx *pCtx;` |
|        - |  7224 | `	ph7_value *pStack;` |
|        - |  7225 | `	VmFrame *pFrame;` |
|       44 |  7226 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  7227 | `	if( pCtx == 0 ){` |
|      ! 0 |  7228 | `		return 0;` |
|        - |  7229 | `	}` |
|       44 |  7230 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  7231 | `	pCtx->pVm = pVm;` |
|       44 |  7232 | `	pCtx->pFunc = pFunc;` |
|       44 |  7233 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  7234 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  7235 | `	pCtx->pc = 0;` |
|       44 |  7236 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  7237 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  7238 | `	/* Allocate a private operand stack */` |
|       44 |  7239 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  7240 | `	if( pStack == 0 ){` |
|      ! 0 |  7241 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7242 | `		return 0;` |
|        - |  7243 | `	}` |
|       44 |  7244 | `	pCtx->pStack = pStack;` |
|        - |  7245 | `	/* Create a detached frame for the fiber */` |
|       44 |  7246 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  7247 | `	if( pFrame == 0 ){` |
|      ! 0 |  7248 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  7249 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7250 | `		return 0;` |
|        - |  7251 | `	}` |
|       44 |  7252 | `	pCtx->pFrame = pFrame;` |
|       44 |  7253 | `	return pCtx;` |
|       23 |  7254 |  |
|        - |  7255 | `/*` |
|        - |  7256 | ` * Start executing a fiber context for the first time.` |
|        - |  7257 | ` */` |
|       42 |  7258 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  7259 |  |
|        - |  7260 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7261 | `	sxi32 rc;` |
|       44 |  7262 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7263 | `		return SXERR_INVALID;` |
|        - |  7264 | `	}` |
|        - |  7265 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  7266 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  7267 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7268 | `	/* Save and set the active context */` |
|       44 |  7269 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  7270 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  7271 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  7272 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  7273 | `	pVm->nRecursionDepth++;` |
|        - |  7274 | `	/* Execute from the beginning */` |
|       65 |  7275 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  7276 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  7277 | `	pVm->nRecursionDepth--;` |
|        - |  7278 | `	/* Restore the previous context */` |
|       44 |  7279 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  7280 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7281 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  7282 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  7283 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  7284 | `		if( pResult ){` |
|       24 |  7285 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7286 | `		}` |
|       42 |  7287 | `		return SXRET_OK;` |
|        - |  7288 | `	}` |
|        - |  7289 | `	/* Detach frame */` |
|        3 |  7290 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7291 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7292 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7293 | `	}` |
|        3 |  7294 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7295 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7296 | `		return PH7_ABORT;` |
|        - |  7297 | `	}` |
|        3 |  7298 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7299 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7300 | `		return PH7_EXCEPTION;` |
|        - |  7301 | `	}` |
|        - |  7302 | `	/* Normal completion */` |
|        3 |  7303 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7304 | `	if( pResult ){` |
|        3 |  7305 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7306 | `	}` |
|        3 |  7307 | `	return SXRET_OK;` |
|       23 |  7308 |  |
|        - |  7309 | `/*` |
|        - |  7310 | ` * Resume a suspended fiber context.` |
|        - |  7311 | ` */` |
|       86 |  7312 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7313 |  |
|        - |  7314 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7315 | `	sxi32 rc;` |
|       88 |  7316 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7317 | `		return SXERR_INVALID;` |
|        - |  7318 | `	}` |
|        - |  7319 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7320 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7321 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7322 | `	if( pResumeValue ){` |
|       40 |  7323 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7324 | `	}else{` |
|       50 |  7325 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7326 | `	}` |
|       88 |  7327 | `	pCtx->nTos++;` |
|        - |  7328 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7329 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7330 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7331 | `	/* Save and set the active context */` |
|       88 |  7332 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7333 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7334 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7335 | `	pVm->nRecursionDepth++;` |
|        - |  7336 | `	/* Resume execution from saved PC */` |
|      131 |  7337 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7338 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7339 | `	pVm->nRecursionDepth--;` |
|        - |  7340 | `	/* Restore the previous context */` |
|       88 |  7341 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7342 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7343 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7344 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7345 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7346 | `		if( pResult ){` |
|       18 |  7347 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7348 | `		}` |
|       56 |  7349 | `		return SXRET_OK;` |
|        - |  7350 | `	}` |
|        - |  7351 | `	/* Detach frame */` |
|       34 |  7352 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7353 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7354 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7355 | `	}` |
|       34 |  7356 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7357 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7358 | `		return PH7_ABORT;` |
|        - |  7359 | `	}` |
|       34 |  7360 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7361 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7362 | `		return PH7_EXCEPTION;` |
|        - |  7363 | `	}` |
|        - |  7364 | `	/* Normal completion */` |
|       34 |  7365 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7366 | `	if( pResult ){` |
|       20 |  7367 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7368 | `	}` |
|       34 |  7369 | `	return SXRET_OK;` |
|       45 |  7370 |  |
|        - |  7371 | `/*` |
|        - |  7372 | ` * Release an execution context and all its resources.` |
|        - |  7373 | ` */` |
|        4 |  7374 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7375 |  |
|        5 |  7376 | `	if( pCtx == 0 ){` |
|      ! 0 |  7377 | `		return;` |
|        - |  7378 | `	}` |
|        5 |  7379 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7380 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7381 | `		return;` |
|        - |  7382 | `	}` |
|        5 |  7383 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7384 | `	/* Release values */` |
|        5 |  7385 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7386 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7387 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7388 | `	if( pCtx->pFrame ){` |
|        - |  7389 | `		VmSlot *aSlot;` |
|        - |  7390 | `		sxu32 n;` |
|        - |  7391 | `		/* Free local variables */` |
|        5 |  7392 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7393 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7394 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7395 | `		}` |
|        - |  7396 | `		/* Remove local references */` |
|        5 |  7397 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7398 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7399 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7400 | `		}` |
|        5 |  7401 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7402 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7403 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7404 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7405 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7406 | `		pCtx->pFrame = 0;` |
|        2 |  7407 | `	}` |
|        - |  7408 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7409 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7410 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7411 | `	if( pCtx->pStack ){` |
|        5 |  7412 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7413 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7414 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7415 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7416 | `				pTos--;` |
|        1 |  7417 | `			}` |
|        2 |  7418 | `		}` |
|        5 |  7419 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7420 | `		pCtx->pStack = 0;` |
|        2 |  7421 | `	}` |
|        - |  7422 | `	/* Free the context itself */` |
|        5 |  7423 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7424 |  |
|        - |  7425 | `/*` |
|        - |  7426 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7427 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7428 | ` */` |
|       90 |  7429 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7430 |  |
|        - |  7431 | `	ph7_class_instance *pThis;` |
|        - |  7432 | `	SyString sAttr;` |
|        - |  7433 | `	ph7_value *pAttr;` |
|       92 |  7434 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7435 | `		return 0;` |
|        - |  7436 | `	}` |
|       92 |  7437 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7438 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7439 | `		return 0;` |
|        - |  7440 | `	}` |
|       92 |  7441 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7442 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7443 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7444 | `		return 0;` |
|        - |  7445 | `	}` |
|       62 |  7446 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7447 |  |
|        - |  7448 | `/*` |
|        - |  7449 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7450 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7451 | ` */` |
|       38 |  7452 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7453 |  |
|       40 |  7454 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7455 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7456 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7457 | `			"Cannot suspend outside of a fiber");` |
|        - |  7458 | `	}` |
|       40 |  7459 | `	if( nArg > 0 ){` |
|       40 |  7460 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7461 | `	}else{` |
|      ! 0 |  7462 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7463 | `	}` |
|       40 |  7464 | `	return PH7_SUSPEND;` |
|       21 |  7465 |  |
|        - |  7466 | `/*` |
|        - |  7467 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7468 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7469 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7470 | ` */` |
|       24 |  7471 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7472 |  |
|        - |  7473 | `	ph7_class_instance *pThis;` |
|        - |  7474 | `	ph7_value *pAttr;` |
|        - |  7475 | `	SyString sAttrName;` |
|       26 |  7476 | `	if( nArg < 2 ){` |
|      ! 0 |  7477 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7478 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7479 | `	}` |
|       26 |  7480 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7481 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7482 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7483 | `	}` |
|       26 |  7484 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7485 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7486 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7487 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7488 | `	}` |
|        - |  7489 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7490 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7491 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7492 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7493 | `	}` |
|        - |  7494 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7495 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7496 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7497 | `	if( pAttr ){` |
|       26 |  7498 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7499 | `	}` |
|       26 |  7500 | `	return PH7_OK;` |
|       14 |  7501 |  |
|        - |  7502 | `/*` |
|        - |  7503 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7504 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7505 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7506 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7507 | ` */` |
|       24 |  7508 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7509 | `	ph7_class_instance **ppThis)` |
|        2 |  7510 |  |
|       26 |  7511 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7512 | `	ph7_value *pCallable;` |
|        - |  7513 | `	SyString sAttrName;` |
|       26 |  7514 | `	*ppThis = 0;` |
|       26 |  7515 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7516 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7517 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7518 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7519 | `		return 0;` |
|        - |  7520 | `	}` |
|       26 |  7521 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7522 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7523 | `		SyString sName;` |
|        - |  7524 | `		SyHashEntry *pEntry;` |
|        - |  7525 | `		ph7_vm_func *pFunc;` |
|       26 |  7526 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7527 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7528 | `		if( pEntry == 0 ){` |
|      ! 0 |  7529 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7530 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7531 | `			return 0;` |
|        - |  7532 | `		}` |
|       26 |  7533 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7534 | `		return pFunc;` |
|      ! 0 |  7535 | `	}else{` |
|        - |  7536 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7537 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7538 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7539 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7540 | `		if( pMethod == 0 ){` |
|      ! 0 |  7541 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7542 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7543 | `			return 0;` |
|        - |  7544 | `		}` |
|      ! 0 |  7545 | `		*ppThis = pClosure;` |
|      ! 0 |  7546 | `		return &pMethod->sFunc;` |
|        - |  7547 | `	}` |
|       14 |  7548 |  |
|        - |  7549 | `/*` |
|        - |  7550 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7551 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7552 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7553 | ` */` |
|       42 |  7554 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7555 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7556 |  |
|       44 |  7557 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7558 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7559 | `	sxu32 nFormal, n;` |
|        - |  7560 | `	VmSlot sSlot;` |
|        - |  7561 | `	sxi32 rc;` |
|        - |  7562 | `	/* Install $this for closure/method callables */` |
|       44 |  7563 | `	if( pClosureThis ){` |
|        - |  7564 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7565 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7566 | `		if( pObj ){` |
|      ! 0 |  7567 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7568 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7569 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7570 | `		}` |
|      ! 0 |  7571 | `	}` |
|        - |  7572 | `	/* Install static variables */` |
|       44 |  7573 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7574 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7575 | `		ph7_value *pVal;` |
|      ! 0 |  7576 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7577 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7578 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7579 | `			if( pVal ){` |
|      ! 0 |  7580 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7581 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7582 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7583 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7584 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7585 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7586 | `				}` |
|      ! 0 |  7587 | `			}` |
|      ! 0 |  7588 | `		}` |
|      ! 0 |  7589 | `	}` |
|        - |  7590 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  7591 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  7592 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  7593 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7594 | `		ph7_value *pObj;` |
|       12 |  7595 | `		if( n < (sxu32)nArg ){` |
|        - |  7596 | `			/* Argument provided — install with type casting */` |
|       12 |  7597 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  7598 | `			if( pObj ){` |
|       12 |  7599 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7600 | `				/* Type casting */` |
|       12 |  7601 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7602 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7603 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7604 | `						if( xCast ){` |
|      ! 0 |  7605 | `							xCast(pObj);` |
|      ! 0 |  7606 | `						}` |
|      ! 0 |  7607 | `					}` |
|      ! 0 |  7608 | `				}` |
|       12 |  7609 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  7610 | `				sSlot.pUserData = 0;` |
|       12 |  7611 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  7612 | `			}` |
|        5 |  7613 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7614 | `			/* Default value */` |
|      ! 0 |  7615 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7616 | `			if( pObj ){` |
|      ! 0 |  7617 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7618 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7619 | `					return rc;` |
|        - |  7620 | `				}` |
|      ! 0 |  7621 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7622 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7623 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7624 | `						if( xCast ){` |
|      ! 0 |  7625 | `							xCast(pObj);` |
|      ! 0 |  7626 | `						}` |
|      ! 0 |  7627 | `					}` |
|      ! 0 |  7628 | `				}` |
|      ! 0 |  7629 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7630 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7631 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7632 | `			}` |
|      ! 0 |  7633 | `		}` |
|        7 |  7634 | `	}` |
|        - |  7635 | `	/* Install closure environment (captured variables) */` |
|       44 |  7636 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7637 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7638 | `		ph7_value *pValue;` |
|        - |  7639 | `		sxu32 iEnv;` |
|        3 |  7640 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7641 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7642 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7643 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7644 | `				continue;` |
|        - |  7645 | `			}` |
|        5 |  7646 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7647 | `			if( pValue == 0 ){` |
|      ! 0 |  7648 | `				continue;` |
|        - |  7649 | `			}` |
|        5 |  7650 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7651 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7652 | `		}` |
|        1 |  7653 | `	}` |
|       44 |  7654 | `	return SXRET_OK;` |
|       23 |  7655 |  |
|        - |  7656 | `/*` |
|        - |  7657 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7658 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7659 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7660 | ` */` |
|       26 |  7661 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7662 |  |
|       28 |  7663 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7664 | `	ph7_class_instance *pThis;` |
|        - |  7665 | `	ph7_class_instance *pClosureThis;` |
|        - |  7666 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7667 | `	ph7_vm_func *pFunc;` |
|        - |  7668 | `	ph7_value sResult;` |
|        - |  7669 | `	ph7_value *pCtxAttr;` |
|        - |  7670 | `	SyString sAttrName;` |
|        - |  7671 | `	sxi32 rc;` |
|       28 |  7672 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7673 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7674 | `	}` |
|       28 |  7675 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7676 | `	/* Check if already started (has a __ctx) */` |
|       28 |  7677 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  7678 | `	if( pExecCtx != 0 ){` |
|        3 |  7679 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7680 | `			"Cannot start a fiber that has already been started");` |
|        - |  7681 | `	}` |
|        - |  7682 | `	/* Resolve callable */` |
|       26 |  7683 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  7684 | `	if( pFunc == 0 ){` |
|      ! 0 |  7685 | `		return PH7_EXCEPTION;` |
|        - |  7686 | `	}` |
|        - |  7687 | `	/* Create execution context now that we know the function */` |
|       26 |  7688 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  7689 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7690 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7691 | `			"Fiber::start(): out of memory");` |
|        - |  7692 | `	}` |
|        - |  7693 | `	/* Store context in $this->__ctx */` |
|       26 |  7694 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  7695 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7696 | `	if( pCtxAttr ){` |
|       26 |  7697 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  7698 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7699 | `	}` |
|        - |  7700 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7701 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7702 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  7703 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  7704 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7705 | `	/* Unpack the args array and install into the frame */` |
|        - |  7706 | `	{` |
|       26 |  7707 | `		ph7_value **apValues = 0;` |
|       26 |  7708 | `		int nActual = 0;` |
|       26 |  7709 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  7710 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7711 | `			ph7_hashmap_node *pNode;` |
|       26 |  7712 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  7713 | `			if( nCount > 0 ){` |
|        3 |  7714 | `				sxu32 idx = 0;` |
|        4 |  7715 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7716 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7717 | `				if( apValues ){` |
|        3 |  7718 | `					pNode = pMap->pFirst;` |
|        7 |  7719 | `					while( pNode && idx < nCount ){` |
|        5 |  7720 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7721 | `						idx++;` |
|        5 |  7722 | `						pNode = pNode->pPrev;` |
|        1 |  7723 | `					}` |
|        3 |  7724 | `					nActual = (int)idx;` |
|        1 |  7725 | `				}` |
|        1 |  7726 | `			}` |
|       12 |  7727 | `		}` |
|       26 |  7728 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  7729 | `		if( apValues ){` |
|        3 |  7730 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7731 | `		}` |
|        - |  7732 | `	}` |
|        - |  7733 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  7734 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  7735 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  7736 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7737 | `		return PH7_ABORT;` |
|        - |  7738 | `	}` |
|       26 |  7739 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  7740 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  7741 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7742 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7743 | `		return PH7_ABORT;` |
|        - |  7744 | `	}` |
|       26 |  7745 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7746 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7747 | `		return PH7_EXCEPTION;` |
|        - |  7748 | `	}` |
|       26 |  7749 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  7750 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  7751 | `	return PH7_OK;` |
|       15 |  7752 |  |
|        - |  7753 | `/*` |
|        - |  7754 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7755 | ` */` |
|       36 |  7756 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7757 |  |
|       38 |  7758 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7759 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7760 | `	ph7_value sResult;` |
|        - |  7761 | `	ph7_value *pResumeVal;` |
|        - |  7762 | `	sxi32 rc;` |
|       38 |  7763 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7764 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7765 | `		return PH7_OK;` |
|        - |  7766 | `	}` |
|       38 |  7767 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  7768 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7769 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7770 | `		return PH7_OK;` |
|        - |  7771 | `	}` |
|       38 |  7772 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7773 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7774 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7775 | `	}` |
|       36 |  7776 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  7777 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  7778 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  7779 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7780 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7781 | `		return PH7_ABORT;` |
|        - |  7782 | `	}` |
|       36 |  7783 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7784 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7785 | `		return PH7_EXCEPTION;` |
|        - |  7786 | `	}` |
|       36 |  7787 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  7788 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  7789 | `	return PH7_OK;` |
|       20 |  7790 |  |
|        - |  7791 | `/*` |
|        - |  7792 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7793 | ` */` |
|        6 |  7794 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7795 |  |
|        8 |  7796 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7797 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  7798 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7799 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7800 | `		return PH7_OK;` |
|        - |  7801 | `	}` |
|        8 |  7802 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  7803 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7804 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7805 | `		return PH7_OK;` |
|        - |  7806 | `	}` |
|        8 |  7807 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7808 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7809 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7810 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7811 | `		}` |
|      ! 0 |  7812 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7813 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7814 | `	}` |
|        8 |  7815 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  7816 | `	return PH7_OK;` |
|        5 |  7817 |  |
|        - |  7818 | `/*` |
|        - |  7819 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7820 | ` */` |
|        6 |  7821 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7822 |  |
|        - |  7823 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7824 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7825 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7826 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7827 | `	return PH7_OK;` |
|        4 |  7828 |  |
|      ! 0 |  7829 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7830 |  |
|        - |  7831 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7832 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7833 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7834 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7835 | `	return PH7_OK;` |
|      ! 0 |  7836 |  |
|        6 |  7837 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7838 |  |
|        - |  7839 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7840 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7841 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7842 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7843 | `	return PH7_OK;` |
|        4 |  7844 |  |
|        6 |  7845 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7846 |  |
|        - |  7847 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7848 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7849 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7850 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7851 | `	return PH7_OK;` |
|        4 |  7852 |  |
|        - |  7853 | `/*` |
|        - |  7854 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7855 | ` */` |
|        4 |  7856 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7857 |  |
|        5 |  7858 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7859 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  7860 | `	if( nArg < 1 ){` |
|      ! 0 |  7861 | `		return PH7_OK;` |
|        - |  7862 | `	}` |
|        5 |  7863 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  7864 | `	if( pExecCtx ){` |
|        5 |  7865 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7866 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  7867 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  7868 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7869 | `			SyString sAttrName;` |
|        - |  7870 | `			ph7_value *pAttr;` |
|        5 |  7871 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  7872 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  7873 | `			if( pAttr ){` |
|        5 |  7874 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  7875 | `			}` |
|        2 |  7876 | `		}` |
|        2 |  7877 | `	}` |
|        5 |  7878 | `	return PH7_OK;` |
|        3 |  7879 |  |
|        - |  7880 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7881 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7882 |  |
|        - |  7883 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7884 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7885 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7886 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7887 |  |
|      ! 0 |  7888 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7889 |  |
|        - |  7890 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7891 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7892 | `	ph7_exec_ctx *pCtx;` |
|        - |  7893 | `	ph7_vm_func *pFunc;` |
|        - |  7894 | `	ph7_value *pCallable;` |
|        - |  7895 | `	ph7_value *pCtxAttr;` |
|        - |  7896 | `	SyString sAttrName;` |
|        - |  7897 | `	/* Must not already be started */` |
|      ! 0 |  7898 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7899 | `	if( pCtx != 0 ){` |
|      ! 0 |  7900 | `		return SXERR_INVALID;` |
|        - |  7901 | `	}` |
|      ! 0 |  7902 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7903 | `		return SXERR_INVALID;` |
|        - |  7904 | `	}` |
|      ! 0 |  7905 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7906 | `	/* Get the callable */` |
|      ! 0 |  7907 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7908 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7909 | `	if( pCallable == 0 ){` |
|      ! 0 |  7910 | `		return SXERR_INVALID;` |
|        - |  7911 | `	}` |
|        - |  7912 | `	/* Resolve callable */` |
|      ! 0 |  7913 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7914 | `		SyString sName;` |
|        - |  7915 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7916 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7917 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7918 | `		if( pEntry == 0 ){` |
|      ! 0 |  7919 | `			return SXERR_NOTFOUND;` |
|        - |  7920 | `		}` |
|      ! 0 |  7921 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7922 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7923 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7924 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7925 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7926 | `		if( pMethod == 0 ){` |
|      ! 0 |  7927 | `			return SXERR_INVALID;` |
|        - |  7928 | `		}` |
|      ! 0 |  7929 | `		pClosureThis = pClosure;` |
|      ! 0 |  7930 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7931 | `	}else{` |
|      ! 0 |  7932 | `		return SXERR_INVALID;` |
|        - |  7933 | `	}` |
|        - |  7934 | `	/* Create context */` |
|      ! 0 |  7935 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7936 | `	if( pCtx == 0 ){` |
|      ! 0 |  7937 | `		return SXERR_MEM;` |
|        - |  7938 | `	}` |
|        - |  7939 | `	/* Store in __ctx */` |
|      ! 0 |  7940 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7941 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7942 | `	if( pCtxAttr ){` |
|      ! 0 |  7943 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7944 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7945 | `	}` |
|        - |  7946 | `	/* Set up frame with args */` |
|      ! 0 |  7947 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7948 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7949 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7950 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7951 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7952 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7953 |  |
|      ! 0 |  7954 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7955 |  |
|      ! 0 |  7956 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7957 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7958 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7959 |  |
|      ! 0 |  7960 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7961 |  |
|      ! 0 |  7962 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7963 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7964 |  |
|      ! 0 |  7965 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7966 |  |
|      ! 0 |  7967 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7968 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7969 |  |
|      ! 0 |  7970 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7971 |  |
|      ! 0 |  7972 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7973 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7974 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7975 |  |
|        - |  7976 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7977 | `/*` |
|        - |  7978 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7979 | ` */` |
|       18 |  7980 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  7981 |  |
|        - |  7982 | `	ph7_generator *pGen;` |
|       20 |  7983 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  7984 | `	if( pGen == 0 ){` |
|      ! 0 |  7985 | `		return 0;` |
|        - |  7986 | `	}` |
|       20 |  7987 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  7988 | `	pGen->pCtx = pCtx;` |
|       20 |  7989 | `	pGen->iImplicitKey = 0;` |
|       20 |  7990 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  7991 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7992 | `	/* Link the generator back to the exec context */` |
|       20 |  7993 | `	pCtx->pPrivate = pGen;` |
|       20 |  7994 | `	return pGen;` |
|       11 |  7995 |  |
|        - |  7996 | `/*` |
|        - |  7997 | ` * Release a generator and its execution context.` |
|        - |  7998 | ` */` |
|      ! 0 |  7999 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  8000 |  |
|      ! 0 |  8001 | `	if( pGen == 0 ){` |
|      ! 0 |  8002 | `		return;` |
|        - |  8003 | `	}` |
|      ! 0 |  8004 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8005 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8006 | `	if( pGen->pCtx ){` |
|      ! 0 |  8007 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  8008 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  8009 | `		pGen->pCtx = 0;` |
|      ! 0 |  8010 | `	}` |
|      ! 0 |  8011 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  8012 |  |
|        - |  8013 | `/*` |
|        - |  8014 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  8015 | ` */` |
|      192 |  8016 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  8017 |  |
|        - |  8018 | `	ph7_class_instance *pThis;` |
|        - |  8019 | `	SyString sAttr;` |
|        - |  8020 | `	ph7_value *pAttr;` |
|      194 |  8021 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8022 | `		return 0;` |
|        - |  8023 | `	}` |
|      194 |  8024 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  8025 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  8026 | `		return 0;` |
|        - |  8027 | `	}` |
|      194 |  8028 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  8029 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  8030 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  8031 | `		return 0;` |
|        - |  8032 | `	}` |
|      194 |  8033 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  8034 |  |
|        - |  8035 | `/*` |
|        - |  8036 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  8037 | ` */` |
|       18 |  8038 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8039 |  |
|        - |  8040 | `	ph7_generator *pGen;` |
|        - |  8041 | `	sxi32 rc;` |
|       20 |  8042 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  8043 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  8044 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  8045 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  8046 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  8047 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  8048 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  8049 | `	}` |
|       20 |  8050 | `	return PH7_OK;` |
|       11 |  8051 |  |
|        - |  8052 | `/*` |
|        - |  8053 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  8054 | ` */` |
|       52 |  8055 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8056 |  |
|        - |  8057 | `	ph7_generator *pGen;` |
|       54 |  8058 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  8059 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  8060 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  8061 | `	return PH7_OK;` |
|       28 |  8062 |  |
|        - |  8063 | `/*` |
|        - |  8064 | ` * Generator::current() — return the last yielded value.` |
|        - |  8065 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  8066 | ` */` |
|       56 |  8067 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8068 |  |
|        - |  8069 | `	ph7_generator *pGen;` |
|        - |  8070 | `	sxi32 rc;` |
|       58 |  8071 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  8072 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  8073 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  8074 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8075 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  8076 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  8077 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  8078 | `	}` |
|       58 |  8079 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  8080 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  8081 | `	}else{` |
|      ! 0 |  8082 | `		ph7_result_null(pCtx);` |
|        - |  8083 | `	}` |
|       58 |  8084 | `	return PH7_OK;` |
|       30 |  8085 |  |
|        - |  8086 | `/*` |
|        - |  8087 | ` * Generator::key() — return the last yielded key.` |
|        - |  8088 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  8089 | ` */` |
|       12 |  8090 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8091 |  |
|        - |  8092 | `	ph7_generator *pGen;` |
|        - |  8093 | `	sxi32 rc;` |
|       13 |  8094 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  8095 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  8096 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  8097 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8098 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  8099 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  8100 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  8101 | `	}` |
|       13 |  8102 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  8103 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  8104 | `	}else{` |
|      ! 0 |  8105 | `		ph7_result_null(pCtx);` |
|        - |  8106 | `	}` |
|       13 |  8107 | `	return PH7_OK;` |
|        7 |  8108 |  |
|        - |  8109 | `/*` |
|        - |  8110 | ` * Generator::next() — advance to the next yield point.` |
|        - |  8111 | ` */` |
|       48 |  8112 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8113 |  |
|        - |  8114 | `	ph7_generator *pGen;` |
|        - |  8115 | `	sxi32 rc;` |
|       50 |  8116 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  8117 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  8118 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  8119 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8120 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  8121 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  8122 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  8123 | `	}else{` |
|      ! 0 |  8124 | `		return PH7_OK;` |
|        - |  8125 | `	}` |
|       50 |  8126 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  8127 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  8128 | `	return PH7_OK;` |
|       26 |  8129 |  |
|        - |  8130 | `/*` |
|        - |  8131 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  8132 | ` */` |
|        4 |  8133 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8134 |  |
|        - |  8135 | `	ph7_generator *pGen;` |
|        - |  8136 | `	ph7_value *pSendVal;` |
|        - |  8137 | `	sxi32 rc;` |
|        5 |  8138 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  8139 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  8140 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  8141 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  8142 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  8143 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  8144 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  8145 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  8146 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  8147 | `	}else{` |
|      ! 0 |  8148 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8149 | `		return PH7_OK;` |
|        - |  8150 | `	}` |
|        5 |  8151 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  8152 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  8153 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8154 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  8155 | `	}else{` |
|        3 |  8156 | `		ph7_result_null(pCtx);` |
|        - |  8157 | `	}` |
|        5 |  8158 | `	return PH7_OK;` |
|        3 |  8159 |  |
|        - |  8160 | `/*` |
|        - |  8161 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  8162 | ` *` |
|        - |  8163 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  8164 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  8165 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  8166 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  8167 | ` * the exception to the caller.` |
|        - |  8168 | ` */` |
|      ! 0 |  8169 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8170 |  |
|        - |  8171 | `	ph7_generator *pGen;` |
|        - |  8172 | `	const char *zMsg;` |
|        - |  8173 | `	int nLen;` |
|      ! 0 |  8174 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  8175 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8176 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  8177 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  8178 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  8179 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8180 | `			"Cannot throw into a closed generator");` |
|        - |  8181 | `	}` |
|        - |  8182 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  8183 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  8184 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  8185 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  8186 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8187 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  8188 | `	nLen = 0;` |
|      ! 0 |  8189 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  8190 | `		/* Try to get the exception's message */` |
|        - |  8191 | `		SyString sAttr;` |
|        - |  8192 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  8193 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  8194 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  8195 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  8196 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  8197 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  8198 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  8199 | `		}` |
|      ! 0 |  8200 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  8201 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  8202 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  8203 | `	}` |
|      ! 0 |  8204 | `	(void)nLen;` |
|      ! 0 |  8205 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  8206 |  |
|        - |  8207 | `/*` |
|        - |  8208 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  8209 | ` */` |
|        2 |  8210 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8211 |  |
|        - |  8212 | `	ph7_generator *pGen;` |
|        3 |  8213 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8214 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  8215 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8216 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8217 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8218 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  8219 | `	}` |
|        3 |  8220 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  8221 | `	return PH7_OK;` |
|        2 |  8222 |  |
|        - |  8223 | `/*` |
|        - |  8224 | ` * Generator::__destruct() — clean up.` |
|        - |  8225 | ` */` |
|      ! 0 |  8226 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8227 |  |
|        - |  8228 | `	ph7_generator *pGen;` |
|      ! 0 |  8229 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  8230 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8231 | `	if( pGen ){` |
|      ! 0 |  8232 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  8233 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8234 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8235 | `			SyString sAttrName;` |
|        - |  8236 | `			ph7_value *pAttr;` |
|      ! 0 |  8237 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8238 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8239 | `			if( pAttr ){` |
|      ! 0 |  8240 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  8241 | `			}` |
|      ! 0 |  8242 | `		}` |
|      ! 0 |  8243 | `	}` |
|      ! 0 |  8244 | `	return PH7_OK;` |
|      ! 0 |  8245 |  |
|        - |  8246 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  8247 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  8248 | `/*` |
|        - |  8249 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8250 | ` * the desired message.` |
|        - |  8251 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8252 | ` * in 'api.c' for additional information.` |
|        - |  8253 | ` */` |
|      370 |  8254 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8255 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8256 | `	SyString *pString /* Message to output */` |
|        - |  8257 | `	)` |
|        2 |  8258 |  |
|      372 |  8259 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  8260 | `	sxi32 rc = SXRET_OK;` |
|        - |  8261 | `	/* Call the output consumer */` |
|      372 |  8262 | `	if( pString->nByte > 0 ){` |
|      372 |  8263 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  8264 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  8265 | `	}` |
|      372 |  8266 | `	return rc;` |
|        2 |  8267 |  |
|        - |  8268 | `/*` |
|        - |  8269 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8270 | ` * callback to consume the formatted message.` |
|        - |  8271 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8272 | ` * in 'api.c' for additional information.` |
|        - |  8273 | ` */` |
|        2 |  8274 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8275 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8276 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8277 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8278 | `	)` |
|        1 |  8279 |  |
|        3 |  8280 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  8281 | `	sxi32 rc = SXRET_OK;` |
|        - |  8282 | `	SyBlob sWorker;` |
|        - |  8283 | `	/* Format the message and call the output consumer */` |
|        3 |  8284 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  8285 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8286 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8287 | `		/* Consume the formatted message */` |
|        3 |  8288 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8289 | `	}` |
|        3 |  8290 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8291 | `	/* Release the working buffer */` |
|        3 |  8292 | `	SyBlobRelease(&sWorker);` |
|        3 |  8293 | `	return rc;` |
|        1 |  8294 |  |
|        - |  8295 | `/*` |
|        - |  8296 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8297 | ` * This function never fail and always return a pointer` |
|        - |  8298 | ` * to a null terminated string.` |
|        - |  8299 | ` */` |
|       12 |  8300 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8301 |  |
|       13 |  8302 | `	const char *zOp = "Unknown     ";` |
|       13 |  8303 | `	switch(nOp){` |
|        3 |  8304 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8305 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8306 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8307 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8308 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8309 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8310 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8311 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8312 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8313 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8314 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8315 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8316 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8317 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8318 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8319 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8320 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8321 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8322 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8323 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8324 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8325 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8326 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8327 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8328 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8329 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8330 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8331 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8332 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8333 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8334 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8335 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8336 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8337 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8338 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8339 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8340 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8341 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8342 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8343 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8344 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8345 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8346 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8347 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8348 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8349 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8350 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8351 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8352 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8353 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8354 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8355 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8356 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  8357 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8358 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8359 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8360 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  8361 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  8362 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8363 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8364 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8365 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8366 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8367 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8368 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8369 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8370 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8371 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8372 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8373 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8374 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8375 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8376 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8377 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8378 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8379 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8380 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8381 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8382 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8383 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8384 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8385 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8386 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8387 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8388 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8389 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8390 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8391 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8392 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8393 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8394 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8395 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8396 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8397 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8398 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8399 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8400 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8401 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8402 | `	default:` |
|      ! 0 |  8403 | `		break;` |
|        - |  8404 | `	}` |
|       13 |  8405 | `	return zOp;` |
|        1 |  8406 |  |
|        - |  8407 | `/*` |
|        - |  8408 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8409 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8410 | ` * is responsible of consuming the generated dump.` |
|        - |  8411 | ` */` |
|        2 |  8412 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8413 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8414 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8415 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8416 | `	)` |
|        1 |  8417 |  |
|        - |  8418 | `	sxi32 rc;` |
|        3 |  8419 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8420 | `	return rc;` |
|        1 |  8421 |  |
|        - |  8422 | `/*` |
|        - |  8423 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8424 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8425 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8426 | ` * in 'compile.c' for additional information.` |
|        - |  8427 | ` */` |
|       14 |  8428 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8429 |  |
|       15 |  8430 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8431 | `	/* Evaluate and expand constant value */` |
|       15 |  8432 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  8433 |  |
|        - |  8434 | `/*` |
|        - |  8435 | ` * Section:` |
|        - |  8436 | ` *  Function handling functions.` |
|        - |  8437 | ` * Status:` |
|        - |  8438 | ` *    Stable.` |
|        - |  8439 | ` */` |
|        - |  8440 | `/*` |
|        - |  8441 | ` * int func_num_args(void)` |
|        - |  8442 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8443 | ` * Parameters` |
|        - |  8444 | ` *   None.` |
|        - |  8445 | ` * Return` |
|        - |  8446 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8447 | ` *  or -1 if called from the globe scope.` |
|        - |  8448 | ` */` |
|      944 |  8449 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8450 |  |
|        - |  8451 | `	VmFrame *pFrame;` |
|        - |  8452 | `	ph7_vm *pVm;` |
|        - |  8453 | `	/* Point to the target VM */` |
|      946 |  8454 | `	pVm = pCtx->pVm;` |
|        - |  8455 | `	/* Current frame */` |
|      946 |  8456 | `	pFrame = pVm->pFrame;` |
|      946 |  8457 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  8458 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8459 | `		SXUNUSED(nArg);` |
|      ! 0 |  8460 | `		SXUNUSED(apArg);` |
|        - |  8461 | `		/* Global frame,return -1 */` |
|      ! 0 |  8462 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8463 | `		return SXRET_OK;` |
|        - |  8464 | `	}` |
|        - |  8465 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  8466 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  8467 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  8468 | `	return SXRET_OK;` |
|      474 |  8469 |  |
|        - |  8470 | `/*` |
|        - |  8471 | ` * value func_get_arg(int $arg_num)` |
|        - |  8472 | ` *   Return an item from the argument list.` |
|        - |  8473 | ` * Parameters` |
|        - |  8474 | ` *  Argument number(index start from zero).` |
|        - |  8475 | ` * Return` |
|        - |  8476 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8477 | ` */` |
|       22 |  8478 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8479 |  |
|       24 |  8480 | `	ph7_value *pObj = 0;` |
|       24 |  8481 | `	VmSlot *pSlot = 0;` |
|        - |  8482 | `	VmFrame *pFrame;` |
|        - |  8483 | `	ph7_vm *pVm;` |
|        - |  8484 | `	/* Point to the target VM */` |
|       24 |  8485 | `	pVm = pCtx->pVm;` |
|        - |  8486 | `	/* Current frame */` |
|       24 |  8487 | `	pFrame = pVm->pFrame;` |
|       24 |  8488 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8489 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8490 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8491 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8492 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8493 | `		return SXRET_OK;` |
|        - |  8494 | `	}` |
|        - |  8495 | `	/* Extract the desired index */` |
|       21 |  8496 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8497 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8498 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8499 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8500 | `		return SXRET_OK;` |
|        - |  8501 | `	}` |
|        - |  8502 | `	/* Extract the desired argument */` |
|       21 |  8503 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8504 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8505 | `			/* Return the desired argument */` |
|       21 |  8506 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8507 | `		}else{` |
|        - |  8508 | `			/* No such argument,return false */` |
|      ! 0 |  8509 | `			ph7_result_bool(pCtx,0);` |
|        - |  8510 | `		}` |
|       11 |  8511 | `	}else{` |
|        - |  8512 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8513 | `		ph7_result_bool(pCtx,0);` |
|        - |  8514 | `	}` |
|       21 |  8515 | `	return SXRET_OK;` |
|       13 |  8516 |  |
|        - |  8517 | `/*` |
|        - |  8518 | ` * array func_get_args_byref(void)` |
|        - |  8519 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8520 | ` * Parameters` |
|        - |  8521 | ` *  None.` |
|        - |  8522 | ` * Return` |
|        - |  8523 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8524 | ` *  member of the current user-defined function's argument list.` |
|        - |  8525 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8526 | ` * NOTE:` |
|        - |  8527 | ` *  Arguments are returned to the array by reference.` |
|        - |  8528 | ` */` |
|        2 |  8529 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8530 |  |
|        - |  8531 | `	ph7_value *pArray;` |
|        - |  8532 | `	VmFrame *pFrame;` |
|        - |  8533 | `	VmSlot *aSlot;` |
|        - |  8534 | `	sxu32 n;` |
|        - |  8535 | `	/* Point to the current frame */` |
|        3 |  8536 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8537 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8538 | `	if( pFrame->pParent == 0 ){` |
|        - |  8539 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8540 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8541 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8542 | `		return SXRET_OK;` |
|        - |  8543 | `	}` |
|        - |  8544 | `	/* Create a new array */` |
|        3 |  8545 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8546 | `	if( pArray == 0 ){` |
|      ! 0 |  8547 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8548 | `		SXUNUSED(apArg);` |
|      ! 0 |  8549 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8550 | `		return SXRET_OK;` |
|        - |  8551 | `	}` |
|        - |  8552 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8553 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8554 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8555 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8556 | `	}` |
|        - |  8557 | `	/* Return the freshly created array */` |
|        3 |  8558 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8559 | `	return SXRET_OK;` |
|        2 |  8560 |  |
|        - |  8561 | `/*` |
|        - |  8562 | ` * array func_get_args(void)` |
|        - |  8563 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8564 | ` * Parameters` |
|        - |  8565 | ` *  None.` |
|        - |  8566 | ` * Return` |
|        - |  8567 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8568 | ` *  member of the current user-defined function's argument list.` |
|        - |  8569 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8570 | ` */` |
|       88 |  8571 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8572 |  |
|       90 |  8573 | `	ph7_value *pObj = 0;` |
|        - |  8574 | `	ph7_value *pArray;` |
|        - |  8575 | `	VmFrame *pFrame;` |
|        - |  8576 | `	VmSlot *aSlot;` |
|        - |  8577 | `	sxu32 n;` |
|        - |  8578 | `	/* Point to the current frame */` |
|       90 |  8579 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8580 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8581 | `	if( pFrame->pParent == 0 ){` |
|        - |  8582 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8583 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8584 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8585 | `		return SXRET_OK;` |
|        - |  8586 | `	}` |
|        - |  8587 | `	/* Create a new array */` |
|       90 |  8588 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8589 | `	if( pArray == 0 ){` |
|      ! 0 |  8590 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8591 | `		SXUNUSED(apArg);` |
|      ! 0 |  8592 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8593 | `		return SXRET_OK;` |
|        - |  8594 | `	}` |
|        - |  8595 | `	/* Start filling the array with the given arguments */` |
|       90 |  8596 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8597 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8598 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8599 | `		if( pObj ){` |
|      134 |  8600 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8601 | `		}` |
|       68 |  8602 | `	}` |
|        - |  8603 | `	/* Return the freshly created array */` |
|       90 |  8604 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8605 | `	return SXRET_OK;` |
|       46 |  8606 |  |
|        - |  8607 | `/*` |
|        - |  8608 | ` * bool function_exists(string $name)` |
|        - |  8609 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8610 | ` * Parameters` |
|        - |  8611 | ` *  The name of the desired function.` |
|        - |  8612 | ` * Return` |
|        - |  8613 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8614 | ` */` |
|     1684 |  8615 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8616 |  |
|        - |  8617 | `	const char *zName;` |
|        - |  8618 | `	ph7_vm *pVm;` |
|        - |  8619 | `	int nLen;` |
|        - |  8620 | `	int res;` |
|     1686 |  8621 | `	if( nArg < 1 ){` |
|        - |  8622 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8623 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8624 | `		return SXRET_OK;` |
|        - |  8625 | `	}` |
|        - |  8626 | `	/* Point to the target VM */` |
|     1686 |  8627 | `	pVm = pCtx->pVm;` |
|        - |  8628 | `	/* Extract the function name */` |
|     1686 |  8629 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8630 | `	/* Assume the function is not defined */` |
|     1686 |  8631 | `	res = 0;` |
|        - |  8632 | `	/* Perform the lookup */` |
|     2526 |  8633 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1680 |  8634 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8635 | `			/* Function is defined */` |
|      206 |  8636 | `			res = 1;` |
|      102 |  8637 | `	}` |
|     1686 |  8638 | `	ph7_result_bool(pCtx,res);` |
|     1686 |  8639 | `	return SXRET_OK;` |
|      844 |  8640 |  |
|        - |  8641 | `/*` |
|        - |  8642 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8643 | ` * [i.e: Whether it is callable or not].` |
|        - |  8644 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8645 | ` */` |
|    18312 |  8646 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8647 |  |
|    18314 |  8648 | `	int res = 0;` |
|    18314 |  8649 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8650 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8651 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8652 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8653 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8654 | `		if( pMethod && CallInvoke ){` |
|        - |  8655 | `			ph7_value sResult;` |
|        - |  8656 | `			sxi32 rc;` |
|        - |  8657 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8658 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8659 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8660 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8661 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8662 | `			}` |
|      ! 0 |  8663 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8664 | `		}` |
|    18314 |  8665 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8666 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8667 | `		if( pMap->nEntry == 2 ){` |
|        - |  8668 | `			ph7_class *pClass;` |
|        - |  8669 | `			ph7_value *pV;` |
|        - |  8670 | `			/* Extract the target class */` |
|       12 |  8671 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8672 | `			if( pV ){` |
|       12 |  8673 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8674 | `				if( pClass ){` |
|        - |  8675 | `					ph7_class_method *pMethod;` |
|        - |  8676 | `					/* Extract the target method */` |
|       10 |  8677 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8678 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8679 | `						/* Perform the lookup */` |
|       10 |  8680 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8681 | `						if( pMethod ){` |
|        - |  8682 | `							/* Method is callable */` |
|        5 |  8683 | `							res = 1;` |
|        2 |  8684 | `						}` |
|        4 |  8685 | `					}` |
|        4 |  8686 | `				}` |
|        5 |  8687 | `			}` |
|        7 |  8688 | `		}` |
|    18301 |  8689 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8690 | `		const char *zName;` |
|        - |  8691 | `		int nLen;` |
|        - |  8692 | `		/* Extract the name */` |
|     5106 |  8693 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8694 | `		/* Perform the lookup */` |
|     5121 |  8695 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8696 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8697 | `				/* Function is callable */` |
|     5088 |  8698 | `				res = 1;` |
|     2543 |  8699 | `		}` |
|     2552 |  8700 | `	}` |
|    18314 |  8701 | `	return res;` |
|        2 |  8702 |  |
|        - |  8703 | `/*` |
|        - |  8704 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8705 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8706 | ` * Parameters` |
|        - |  8707 | ` * $name` |
|        - |  8708 | ` *    The callback function to check` |
|        - |  8709 | ` * $syntax_only` |
|        - |  8710 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8711 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8712 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8713 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8714 | ` *    a string.` |
|        - |  8715 | ` * Return` |
|        - |  8716 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8717 | ` */` |
|       14 |  8718 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8719 |  |
|        - |  8720 | `	ph7_vm *pVm;` |
|        - |  8721 | `	int res;` |
|       15 |  8722 | `	if( nArg < 1 ){` |
|        - |  8723 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8724 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8725 | `		return SXRET_OK;` |
|        - |  8726 | `	}` |
|        - |  8727 | `	/* Point to the target VM */` |
|       15 |  8728 | `	pVm = pCtx->pVm;` |
|        - |  8729 | `	/* Perform the requested operation */` |
|       15 |  8730 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8731 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8732 | `	return SXRET_OK;` |
|        8 |  8733 |  |
|        - |  8734 | `/*` |
|        - |  8735 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8736 | ` * defined below.` |
|        - |  8737 | ` */` |
|     1200 |  8738 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8739 |  |
|     1201 |  8740 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8741 | `	ph7_value sName;` |
|        - |  8742 | `	sxi32 rc;` |
|        - |  8743 | `	/* Prepare the function name for insertion */` |
|     1201 |  8744 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  8745 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8746 | `	/* Perform the insertion */` |
|     1201 |  8747 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  8748 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  8749 | `	return rc;` |
|        1 |  8750 |  |
|        - |  8751 | `/*` |
|        - |  8752 | ` * array get_defined_functions(void)` |
|        - |  8753 | ` *  Returns an array of all defined functions.` |
|        - |  8754 | ` * Parameter` |
|        - |  8755 | ` *  None.` |
|        - |  8756 | ` * Return` |
|        - |  8757 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8758 | ` *  both built-in (internal) and user-defined.` |
|        - |  8759 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8760 | ` *  defined ones using $arr["user"].` |
|        - |  8761 | ` * Note:` |
|        - |  8762 | ` *  NULL is returned on failure.` |
|        - |  8763 | ` */` |
|        2 |  8764 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8765 |  |
|        - |  8766 | `	ph7_value *pArray,*pEntry;` |
|        - |  8767 | `	/* NOTE:` |
|        - |  8768 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8769 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8770 | `	 */` |
|        3 |  8771 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8772 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8773 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8774 | `		SXUNUSED(apArg);` |
|        - |  8775 | `		/* Return NULL */` |
|      ! 0 |  8776 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8777 | `		return SXRET_OK;` |
|        - |  8778 | `	}` |
|        3 |  8779 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8780 | `	if( pEntry == 0 ){` |
|        - |  8781 | `		/* Return NULL */` |
|      ! 0 |  8782 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8783 | `		return SXRET_OK;` |
|        - |  8784 | `	}` |
|        - |  8785 | `	/* Fill with the appropriate information */` |
|        3 |  8786 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8787 | `	/* Create the 'internal' index */` |
|        3 |  8788 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8789 | `	/* Create the user-func array */` |
|        3 |  8790 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8791 | `	if( pEntry == 0 ){` |
|        - |  8792 | `		/* Return NULL */` |
|      ! 0 |  8793 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8794 | `		return SXRET_OK;` |
|        - |  8795 | `	}` |
|        - |  8796 | `	/* Fill with the appropriate information */` |
|        3 |  8797 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8798 | `	/* Create the 'user' index */` |
|        3 |  8799 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8800 | `	/* Return the multi-dimensional array */` |
|        3 |  8801 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8802 | `	return SXRET_OK;` |
|        2 |  8803 |  |
|        - |  8804 | `/*` |
|        - |  8805 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8806 | ` *  Register a function for execution on shutdown.` |
|        - |  8807 | ` * Note` |
|        - |  8808 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8809 | ` *  be called in the same order as they were registered.` |
|        - |  8810 | ` * Parameters` |
|        - |  8811 | ` *  $callback` |
|        - |  8812 | ` *   The shutdown callback to register.` |
|        - |  8813 | ` * $param` |
|        - |  8814 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8815 | ` * Return` |
|        - |  8816 | ` *  Nothing.` |
|        - |  8817 | ` */` |
|        2 |  8818 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8819 |  |
|        - |  8820 | `	VmShutdownCB sEntry;` |
|        - |  8821 | `	int i,j;` |
|        3 |  8822 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8823 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8824 | `		return PH7_OK;` |
|        - |  8825 | `	}` |
|        - |  8826 | `	/* Zero the Entry */` |
|        3 |  8827 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8828 | `	/* Initialize fields */` |
|        3 |  8829 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8830 | `	/* Save the callback name for later invocation name */` |
|        3 |  8831 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8832 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8833 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8834 | `	}` |
|        - |  8835 | `	/* Copy arguments */` |
|        3 |  8836 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8837 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8838 | `			/* Limit reached */` |
|      ! 0 |  8839 | `			break;` |
|        - |  8840 | `		}` |
|      ! 0 |  8841 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8842 | `	}` |
|        3 |  8843 | `	sEntry.nArg = j;` |
|        - |  8844 | `	/* Install the callback */` |
|        3 |  8845 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8846 | `	return PH7_OK;` |
|        2 |  8847 |  |
|        - |  8848 | `/*` |
|        - |  8849 | ` * Section:` |
|        - |  8850 | ` *  Class handling functions.` |
|        - |  8851 | ` * Status:` |
|        - |  8852 | ` *    Stable.` |
|        - |  8853 | ` */` |
|        - |  8854 | `/*` |
|        - |  8855 | ` * Extract the top active class. NULL is returned` |
|        - |  8856 | ` * if the class stack is empty.` |
|        - |  8857 | ` */` |
|      612 |  8858 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8859 |  |
|      614 |  8860 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8861 | `	ph7_class **apClass;` |
|      614 |  8862 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8863 | `		/* Empty stack,return NULL */` |
|       15 |  8864 | `		return 0;` |
|        - |  8865 | `	}` |
|        - |  8866 | `	/* Peek the last entry */` |
|      600 |  8867 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      600 |  8868 | `	return apClass[pSet->nUsed - 1];` |
|      308 |  8869 |  |
|        - |  8870 | `/*` |
|        - |  8871 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8872 | ` *   Get the class that declared the currently executing method.` |
|        - |  8873 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8874 | ` *` |
|        - |  8875 | ` * Parameters` |
|        - |  8876 | ` *   pVm: Target VM` |
|        - |  8877 | ` *` |
|        - |  8878 | ` * Return` |
|        - |  8879 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8880 | ` *   - Not executing within a class method` |
|        - |  8881 | ` *` |
|        - |  8882 | ` * Note` |
|        - |  8883 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8884 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8885 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8886 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8887 | ` *   declaring class.` |
|        - |  8888 | ` */` |
|       90 |  8889 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8890 |  |
|       92 |  8891 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8892 | `	ph7_vm_func *pVmFunc;` |
|        - |  8893 |  |
|        - |  8894 | `	/* Skip exception frames to find the actual method frame */` |
|       92 |  8895 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8896 |  |
|        - |  8897 | `	/* Check if we're in a method context */` |
|       92 |  8898 | `	if( pFrame->pParent ){` |
|       88 |  8899 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       88 |  8900 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8901 | `			/* Return the declaring class */` |
|       88 |  8902 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8903 | `		}` |
|      ! 0 |  8904 | `	}` |
|        - |  8905 |  |
|        5 |  8906 | `	return 0;` |
|       47 |  8907 |  |
|        - |  8908 |  |
|        - |  8909 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8910 | `/*` |
|        - |  8911 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8912 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8913 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8914 | ` * return value indicates failure.` |
|        - |  8915 | ` */` |
|     1542 |  8916 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8917 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8918 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8919 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8920 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8921 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8922 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8923 | `	)` |
|        2 |  8924 |  |
|        - |  8925 | `	ph7_value *aStack;` |
|        - |  8926 | `	VmInstr aInstr[2];` |
|        - |  8927 | `	int iCursor;` |
|        - |  8928 | `	int i;` |
|        - |  8929 | `	/* Create a new operand stack */` |
|     1544 |  8930 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1544 |  8931 | `	if( aStack == 0 ){` |
|      ! 0 |  8932 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8933 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8934 | `		return SXERR_MEM;` |
|        - |  8935 | `	}` |
|        - |  8936 | `	/* Fill the operand stack with the given arguments */` |
|     2194 |  8937 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      652 |  8938 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8939 | `		/*` |
|        - |  8940 | `		 * Symisc eXtension:` |
|        - |  8941 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8942 | `		 */` |
|      652 |  8943 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      327 |  8944 | `	}` |
|     1544 |  8945 | `	iCursor = nArg + 1;` |
|     1544 |  8946 | `	if( pThis ){` |
|        - |  8947 | `		/*` |
|        - |  8948 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8949 | `		 */` |
|     1538 |  8950 | `		pThis->iRef++; /* Increment reference count */` |
|     1538 |  8951 | `		aStack[i].x.pOther = pThis;` |
|     1538 |  8952 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      768 |  8953 | `	}` |
|     1544 |  8954 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1544 |  8955 | `	i++;` |
|        - |  8956 | `	/* Push method name */` |
|     1544 |  8957 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1544 |  8958 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1544 |  8959 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1544 |  8960 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8961 | `	/* Emit the CALL istruction */` |
|     1544 |  8962 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1544 |  8963 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1544 |  8964 | `	aInstr[0].iP2 = 0;` |
|     1544 |  8965 | `	aInstr[0].p3  = 0;` |
|        - |  8966 | `	/* Emit the DONE instruction */` |
|     1544 |  8967 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1544 |  8968 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1544 |  8969 | `	aInstr[1].iP2 = 0;` |
|     1544 |  8970 | `	aInstr[1].p3  = 0;` |
|        - |  8971 | `	/* Execute the method body (if available) */` |
|     1544 |  8972 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8973 | `	/* Clean up the mess left behind */` |
|     1544 |  8974 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1544 |  8975 | `	return PH7_OK;` |
|      773 |  8976 |  |
|        - |  8977 | `/*` |
|        - |  8978 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8979 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8980 | ` * in the apArg[] array.` |
|        - |  8981 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8982 | ` * return value indicates failure.` |
|        - |  8983 | ` */` |
|      960 |  8984 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8985 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8986 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8987 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8988 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8989 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8990 | `	)` |
|        2 |  8991 |  |
|        - |  8992 | `	ph7_value *aStack;` |
|        - |  8993 | `	VmInstr aInstr[2];` |
|        - |  8994 | `	int i;` |
|      962 |  8995 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8996 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 |  8997 | `		if( pResult ){` |
|        - |  8998 | `			/* Assume a null return value */` |
|      ! 0 |  8999 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  9000 | `		}` |
|      479 |  9001 | `		return SXERR_INVALID;` |
|        - |  9002 | `	}` |
|      484 |  9003 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  9004 | `		/* Class method */` |
|       11 |  9005 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  9006 | `		ph7_class_method *pMethod = 0;` |
|       11 |  9007 | `		ph7_class_instance *pThis = 0;` |
|       11 |  9008 | `		ph7_class *pClass = 0;` |
|        - |  9009 | `		ph7_value *pValue;` |
|        - |  9010 | `		sxi32 rc;` |
|       11 |  9011 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  9012 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  9013 | `			if( pResult ){` |
|        - |  9014 | `				/* Assume a null return value */` |
|      ! 0 |  9015 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9016 | `			}` |
|      ! 0 |  9017 | `			return SXRET_OK;` |
|        - |  9018 | `		}` |
|        - |  9019 | `		/* Extract the class name or an instance of it */` |
|       11 |  9020 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  9021 | `		if( pValue ){` |
|       11 |  9022 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  9023 | `		}` |
|       11 |  9024 | `		if( pClass == 0 ){` |
|        - |  9025 | `			/* No such class,return NULL */` |
|      ! 0 |  9026 | `			if( pResult ){` |
|      ! 0 |  9027 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9028 | `			}` |
|      ! 0 |  9029 | `			return SXRET_OK;` |
|        - |  9030 | `		}` |
|       11 |  9031 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9032 | `			/* Point to the class instance */` |
|        5 |  9033 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  9034 | `		}` |
|        - |  9035 | `		/* Try to extract the method */` |
|       11 |  9036 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  9037 | `		if( pValue ){` |
|       11 |  9038 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  9039 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  9040 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  9041 | `			}` |
|        5 |  9042 | `		}` |
|       11 |  9043 | `		if( pMethod == 0 ){` |
|        - |  9044 | `			/* No such method,return NULL */` |
|      ! 0 |  9045 | `			if( pResult ){` |
|      ! 0 |  9046 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  9047 | `			}` |
|      ! 0 |  9048 | `			return SXRET_OK;` |
|        - |  9049 | `		}` |
|        - |  9050 | `		/* Call the class method */` |
|       11 |  9051 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  9052 | `		return rc;` |
|        - |  9053 | `	}` |
|        - |  9054 | `	/* Create a new operand stack */` |
|      474 |  9055 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      474 |  9056 | `	if( aStack == 0 ){` |
|      ! 0 |  9057 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  9058 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  9059 | `		if( pResult ){` |
|        - |  9060 | `			/* Assume a null return value */` |
|      ! 0 |  9061 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  9062 | `		}` |
|      ! 0 |  9063 | `		return SXERR_MEM;` |
|        - |  9064 | `	}` |
|        - |  9065 | `	/* Fill the operand stack with the given arguments */` |
|     1522 |  9066 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1050 |  9067 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  9068 | `		/*` |
|        - |  9069 | `		 * Symisc eXtension:` |
|        - |  9070 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  9071 | `		 */` |
|     1050 |  9072 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      526 |  9073 | `	}` |
|        - |  9074 | `	/* Push the function name */` |
|      474 |  9075 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      474 |  9076 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  9077 | `	/* Emit the CALL istruction */` |
|      474 |  9078 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      474 |  9079 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      474 |  9080 | `	aInstr[0].iP2 = 0;` |
|      474 |  9081 | `	aInstr[0].p3  = 0;` |
|        - |  9082 | `	/* Emit the DONE instruction */` |
|      474 |  9083 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      474 |  9084 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      474 |  9085 | `	aInstr[1].iP2 = 0;` |
|      474 |  9086 | `	aInstr[1].p3  = 0;` |
|        - |  9087 | `	/* Execute the function body (if available) */` |
|      474 |  9088 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  9089 | `	/* Clean up the mess left behind */` |
|      474 |  9090 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      474 |  9091 | `	return PH7_OK;` |
|      482 |  9092 |  |
|        - |  9093 | `/*` |
|        - |  9094 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  9095 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  9096 | ` * parameter.` |
|        - |  9097 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  9098 | ` * return value indicates failure.` |
|        - |  9099 | ` */` |
|      236 |  9100 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  9101 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  9102 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  9103 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  9104 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  9105 | `	)` |
|        1 |  9106 |  |
|        - |  9107 | `	ph7_value *pArg;` |
|        - |  9108 | `	SySet aArg;` |
|        - |  9109 | `	va_list ap;` |
|        - |  9110 | `	sxi32 rc;` |
|      237 |  9111 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  9112 | `	/* Copy arguments one after one */` |
|      237 |  9113 | `	va_start(ap,pResult);` |
|      393 |  9114 | `	for(;;){` |
|      787 |  9115 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  9116 | `		if( pArg == 0 ){` |
|      237 |  9117 | `			break;` |
|        - |  9118 | `		}` |
|      551 |  9119 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  9120 | `	}` |
|        - |  9121 | `	/* Call the core routine */` |
|      237 |  9122 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  9123 | `	/* Cleanup */` |
|      237 |  9124 | `	SySetRelease(&aArg);` |
|      237 |  9125 | `	return rc;` |
|        1 |  9126 |  |
|        - |  9127 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  9128 | `/*` |
|        - |  9129 | ` * bool defined(string $name)` |
|        - |  9130 | ` *  Checks whether a given named constant exists.` |
|        - |  9131 | ` * Parameter:` |
|        - |  9132 | ` *  Name of the desired constant.` |
|        - |  9133 | ` * Return` |
|        - |  9134 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  9135 | ` */` |
|       14 |  9136 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9137 |  |
|        - |  9138 | `	const char *zName;` |
|       16 |  9139 | `	int nLen = 0;` |
|       16 |  9140 | `	int res = 0;` |
|       16 |  9141 | `	if( nArg < 1 ){` |
|        - |  9142 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  9143 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  9144 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9145 | `		return SXRET_OK;` |
|        - |  9146 | `	}` |
|        - |  9147 | `	/* Extract constant name */` |
|       16 |  9148 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9149 | `	/* Perform the lookup */` |
|       16 |  9150 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9151 | `		/* Already defined */` |
|       10 |  9152 | `		res = 1;` |
|        4 |  9153 | `	}` |
|       16 |  9154 | `	ph7_result_bool(pCtx,res);` |
|       16 |  9155 | `	return SXRET_OK;` |
|        9 |  9156 |  |
|        - |  9157 | `/*` |
|        - |  9158 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  9159 | ` * below.` |
|        - |  9160 | ` */` |
|       10 |  9161 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  9162 |  |
|       12 |  9163 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  9164 | `	/* Expand constant value */` |
|       12 |  9165 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 |  9166 |  |
|        - |  9167 | `/*` |
|        - |  9168 | ` * bool define(string $constant_name,expression value)` |
|        - |  9169 | ` *  Defines a named constant at runtime.` |
|        - |  9170 | ` * Parameter:` |
|        - |  9171 | ` *  $constant_name` |
|        - |  9172 | ` *   The name of the constant` |
|        - |  9173 | ` *  $value` |
|        - |  9174 | ` *   Constant value` |
|        - |  9175 | ` * Return:` |
|        - |  9176 | ` *   TRUE on success,FALSE on failure.` |
|        - |  9177 | ` */` |
|       12 |  9178 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9179 |  |
|        - |  9180 | `	const char *zName;  /* Constant name */` |
|        - |  9181 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 |  9182 | `	int nLen = 0;       /* Name length */` |
|        - |  9183 | `	sxi32 rc;` |
|       14 |  9184 | `	if( nArg < 2 ){` |
|        - |  9185 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  9186 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  9187 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9188 | `		return SXRET_OK;` |
|        - |  9189 | `	}` |
|       14 |  9190 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  9191 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  9192 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9193 | `		return SXRET_OK;` |
|        - |  9194 | `	}` |
|        - |  9195 | `	/* Extract constant name */` |
|       14 |  9196 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 |  9197 | `	if( nLen < 1 ){` |
|      ! 0 |  9198 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  9199 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9200 | `		return SXRET_OK;` |
|        - |  9201 | `	}` |
|        - |  9202 | `	/* Duplicate constant value */` |
|       14 |  9203 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 |  9204 | `	if( pValue == 0 ){` |
|      ! 0 |  9205 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9206 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9207 | `		return SXRET_OK;` |
|        - |  9208 | `	}` |
|        - |  9209 | `	/* Initialize the memory object */` |
|       14 |  9210 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  9211 | `	/* Register the constant */` |
|       14 |  9212 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 |  9213 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9214 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  9215 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9216 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9217 | `		return SXRET_OK;` |
|        - |  9218 | `	}` |
|        - |  9219 | `	/* Duplicate constant value */` |
|       14 |  9220 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 |  9221 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  9222 | `		/* Lower case the constant name */` |
|      ! 0 |  9223 | `		char *zCur = (char *)zName;` |
|      ! 0 |  9224 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  9225 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  9226 | `				/* UTF-8 stream */` |
|      ! 0 |  9227 | `				zCur++;` |
|      ! 0 |  9228 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  9229 | `					zCur++;` |
|      ! 0 |  9230 | `				}` |
|      ! 0 |  9231 | `				continue;` |
|        - |  9232 | `			}` |
|      ! 0 |  9233 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  9234 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  9235 | `				zCur[0] = (char)c;` |
|      ! 0 |  9236 | `			}` |
|      ! 0 |  9237 | `			zCur++;` |
|      ! 0 |  9238 | `		}` |
|        - |  9239 | `		/* Finally,register the constant */` |
|      ! 0 |  9240 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  9241 | `	}` |
|        - |  9242 | `	/* All done,return TRUE */` |
|       14 |  9243 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9244 | `	return SXRET_OK;` |
|        8 |  9245 |  |
|        - |  9246 | `/*` |
|        - |  9247 | ` * value constant(string $name)` |
|        - |  9248 | ` *  Returns the value of a constant` |
|        - |  9249 | ` * Parameter` |
|        - |  9250 | ` *  $name` |
|        - |  9251 | ` *    Name of the constant.` |
|        - |  9252 | ` * Return` |
|        - |  9253 | ` *  Constant value or NULL if not defined.` |
|        - |  9254 | ` */` |
|        8 |  9255 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9256 |  |
|        - |  9257 | `	SyHashEntry *pEntry;` |
|        - |  9258 | `	ph7_constant *pCons;` |
|        - |  9259 | `	const char *zName; /* Constant name */` |
|        - |  9260 | `	ph7_value sVal;    /* Constant value */` |
|        - |  9261 | `	int nLen;` |
|       10 |  9262 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9263 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  9264 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  9265 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9266 | `		return SXRET_OK;` |
|        - |  9267 | `	}` |
|        - |  9268 | `	/* Extract the constant name */` |
|       10 |  9269 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9270 | `	/* Perform the query */` |
|       10 |  9271 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  9272 | `	if( pEntry == 0 ){` |
|        3 |  9273 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  9274 | `		ph7_result_null(pCtx);` |
|        3 |  9275 | `		return SXRET_OK;` |
|        - |  9276 | `	}` |
|        8 |  9277 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  9278 | `	/* Point to the structure that describe the constant */` |
|        8 |  9279 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  9280 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  9281 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  9282 | `	/* Return that value */` |
|        8 |  9283 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  9284 | `	/* Cleanup */` |
|        8 |  9285 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  9286 | `	return SXRET_OK;` |
|        6 |  9287 |  |
|        - |  9288 | `/*` |
|        - |  9289 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9290 | ` * defined below.` |
|        - |  9291 | ` */` |
|      452 |  9292 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9293 |  |
|      453 |  9294 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9295 | `	ph7_value sName;` |
|        - |  9296 | `	sxi32 rc;` |
|        - |  9297 | `	/* Prepare the constant name for insertion */` |
|      453 |  9298 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 |  9299 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9300 | `	/* Perform the insertion */` |
|      453 |  9301 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 |  9302 | `	PH7_MemObjRelease(&sName);` |
|      453 |  9303 | `	return rc;` |
|        1 |  9304 |  |
|        - |  9305 | `/*` |
|        - |  9306 | ` * array get_defined_constants(void)` |
|        - |  9307 | ` *  Returns an associative array with the names of all defined` |
|        - |  9308 | ` *  constants.` |
|        - |  9309 | ` * Parameters` |
|        - |  9310 | ` *  NONE.` |
|        - |  9311 | ` * Returns` |
|        - |  9312 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9313 | ` */` |
|        2 |  9314 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9315 |  |
|        - |  9316 | `	ph7_value *pArray;` |
|        - |  9317 | `	/* Create the array first*/` |
|        3 |  9318 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9319 | `	if( pArray == 0 ){` |
|      ! 0 |  9320 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9321 | `		SXUNUSED(apArg);` |
|        - |  9322 | `		/* Return NULL */` |
|      ! 0 |  9323 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9324 | `		return SXRET_OK;` |
|        - |  9325 | `	}` |
|        - |  9326 | `	/* Fill the array with the defined constants */` |
|        3 |  9327 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9328 | `	/* Return the created array */` |
|        3 |  9329 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9330 | `	return SXRET_OK;` |
|        2 |  9331 |  |
|        - |  9332 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9333 | `/*` |
|        - |  9334 | ` * Section:` |
|        - |  9335 | ` *  Random numbers/string generators.` |
|        - |  9336 | ` * Status:` |
|        - |  9337 | ` *    Stable.` |
|        - |  9338 | ` */` |
|        - |  9339 | `/*` |
|        - |  9340 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9341 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9342 | ` * used by te SQLite3 library.` |
|        - |  9343 | ` */` |
|     2474 |  9344 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9345 |  |
|        - |  9346 | `	sxu32 iNum;` |
|     2476 |  9347 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2476 |  9348 | `	return iNum;` |
|        2 |  9349 |  |
|        - |  9350 | `/*` |
|        - |  9351 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9352 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9353 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9354 | ` * by te SQLite3 library.` |
|        - |  9355 | ` */` |
|   128690 |  9356 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9357 |  |
|        - |  9358 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9359 | `	int i;` |
|        - |  9360 | `	/* Generate a binary string first */` |
|   128692 |  9361 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9362 | `	/* Turn the binary string into english based alphabet */` |
|  1415760 |  9363 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1287070 |  9364 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   643536 |  9365 | `	 }` |
|   128692 |  9366 |  |
|        - |  9367 | `/*` |
|        - |  9368 | ` * int rand()` |
|        - |  9369 | ` * int mt_rand()` |
|        - |  9370 | ` * int rand(int $min,int $max)` |
|        - |  9371 | ` * int mt_rand(int $min,int $max)` |
|        - |  9372 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9373 | ` * Parameter` |
|        - |  9374 | ` *  $min` |
|        - |  9375 | ` *    The lowest value to return (default: 0)` |
|        - |  9376 | ` *  $max` |
|        - |  9377 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9378 | ` * Return` |
|        - |  9379 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9380 | ` * Note:` |
|        - |  9381 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9382 | ` *  by te SQLite3 library.` |
|        - |  9383 | ` */` |
|       20 |  9384 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9385 |  |
|        - |  9386 | `	sxu32 iNum;` |
|        - |  9387 | `	/* Generate the random number */` |
|       21 |  9388 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9389 | `	if( nArg > 1 ){` |
|        - |  9390 | `		sxu32 iMin,iMax;` |
|        3 |  9391 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9392 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9393 | `		if( iMin < iMax ){` |
|        3 |  9394 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9395 | `			if( iDiv > 0 ){` |
|        3 |  9396 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9397 | `			}` |
|        1 |  9398 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9399 | `			iNum %= iMax;` |
|      ! 0 |  9400 | `		}` |
|        1 |  9401 | `	}` |
|        - |  9402 | `	/* Return the number */` |
|       21 |  9403 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9404 | `	return SXRET_OK;` |
|        1 |  9405 |  |
|        - |  9406 | `/*` |
|        - |  9407 | ` * int getrandmax(void)` |
|        - |  9408 | ` * int mt_getrandmax(void)` |
|        - |  9409 | ` * int rc4_getrandmax(void)` |
|        - |  9410 | ` *   Show largest possible random value` |
|        - |  9411 | ` * Return` |
|        - |  9412 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9413 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9414 | ` * Note:` |
|        - |  9415 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9416 | ` *  by te SQLite3 library.` |
|        - |  9417 | ` */` |
|        4 |  9418 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9419 |  |
|        2 |  9420 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9421 | `	SXUNUSED(apArg);` |
|        5 |  9422 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9423 | `	return SXRET_OK;` |
|        1 |  9424 |  |
|        - |  9425 | `/*` |
|        - |  9426 | ` * string rand_str()` |
|        - |  9427 | ` * string rand_str(int $len)` |
|        - |  9428 | ` *  Generate a random string (English alphabet).` |
|        - |  9429 | ` * Parameter` |
|        - |  9430 | ` *  $len` |
|        - |  9431 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9432 | ` * Return` |
|        - |  9433 | ` *   A pseudo random string.` |
|        - |  9434 | ` * Note:` |
|        - |  9435 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9436 | ` *  by te SQLite3 library.` |
|        - |  9437 | ` *  This function is a symisc extension.` |
|        - |  9438 | ` */` |
|      120 |  9439 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9440 |  |
|        - |  9441 | `	char zString[1024];` |
|      122 |  9442 | `	int iLen = 0x10;` |
|      122 |  9443 | `	if( nArg > 0 ){` |
|        - |  9444 | `		/* Get the desired length */` |
|      122 |  9445 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9446 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9447 | `			/* Default length */` |
|        3 |  9448 | `			iLen = 0x10;` |
|        1 |  9449 | `		}` |
|       60 |  9450 | `	}` |
|        - |  9451 | `	/* Generate the random string */` |
|      122 |  9452 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9453 | `	/* Return the generated string */` |
|      122 |  9454 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9455 | `	return SXRET_OK;` |
|        2 |  9456 |  |
|        - |  9457 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9458 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9459 | `/* Unique ID private data */` |
|        - |  9460 | `struct unique_id_data` |
|        - |  9461 |  |
|        - |  9462 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9463 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9464 | `};` |
|        - |  9465 | `/*` |
|        - |  9466 | ` * Binary to hex consumer callback.` |
|        - |  9467 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9468 | ` * defined below.` |
|        - |  9469 | ` */` |
|      192 |  9470 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9471 |  |
|      193 |  9472 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9473 | `	sxu32 nBuflen;` |
|        - |  9474 | `	/* Extract result buffer length */` |
|      193 |  9475 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9476 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9477 | `			/*` |
|        - |  9478 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9479 | `			 * string will be 13 characters long` |
|        - |  9480 | `			 */` |
|       25 |  9481 | `		return SXERR_ABORT;` |
|        - |  9482 | `	}` |
|      169 |  9483 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9484 | `		return SXERR_ABORT;` |
|        - |  9485 | `	}` |
|        - |  9486 | `	/* Safely Consume the hex stream */` |
|      169 |  9487 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9488 | `	return SXRET_OK;` |
|       97 |  9489 |  |
|        - |  9490 | `/*` |
|        - |  9491 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9492 | ` *  Generate a unique ID` |
|        - |  9493 | ` * Parameter` |
|        - |  9494 | ` * $prefix` |
|        - |  9495 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9496 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9497 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9498 | ` * $more_entropy` |
|        - |  9499 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9500 | ` *  that the result will be unique.` |
|        - |  9501 | ` * Return` |
|        - |  9502 | ` *  Returns the unique identifier, as a string.` |
|        - |  9503 | ` */` |
|       24 |  9504 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9505 |  |
|        - |  9506 | `	struct unique_id_data sUniq;` |
|        - |  9507 | `	unsigned char zDigest[20];` |
|       25 |  9508 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9509 | `	const char *zPrefix;` |
|        - |  9510 | `	SHA1Context sCtx;` |
|        - |  9511 | `	char zRandom[7];` |
|        - |  9512 | `	int nPrefix;` |
|        - |  9513 | `	int entropy;` |
|        - |  9514 | `	/* Generate a random string first */` |
|       25 |  9515 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9516 | `	/* Initialize fields */` |
|       25 |  9517 | `	zPrefix = 0;` |
|       25 |  9518 | `	nPrefix = 0;` |
|       25 |  9519 | `	entropy = 0;` |
|       25 |  9520 | `	if( nArg > 0 ){` |
|        - |  9521 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9522 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9523 | `		if( nArg > 1 ){` |
|      ! 0 |  9524 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9525 | `		}` |
|      ! 0 |  9526 | `	}` |
|       25 |  9527 | `	SHA1Init(&sCtx);` |
|        - |  9528 | `	/* Generate the random ID */` |
|       25 |  9529 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9530 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9531 | `	}` |
|        - |  9532 | `	/* Append the random ID */` |
|       25 |  9533 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9534 | `	/* Append the random string */` |
|       25 |  9535 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9536 | `	/* Increment the number */` |
|       25 |  9537 | `	pVm->unique_id++;` |
|       25 |  9538 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9539 | `	/* Hexify the digest */` |
|       25 |  9540 | `	sUniq.pCtx = pCtx;` |
|       25 |  9541 | `	sUniq.entropy = entropy;` |
|       25 |  9542 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9543 | `	/* All done */` |
|       25 |  9544 | `	return PH7_OK;` |
|        1 |  9545 |  |
|        - |  9546 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9547 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9548 | `/*` |
|        - |  9549 | ` * Section:` |
|        - |  9550 | ` *  Language construct implementation as foreign functions.` |
|        - |  9551 | ` * Status:` |
|        - |  9552 | ` *    Stable.` |
|        - |  9553 | ` */` |
|        - |  9554 | `/*` |
|        - |  9555 | ` * void echo($string...)` |
|        - |  9556 | ` *  Output one or more messages.` |
|        - |  9557 | ` * Parameters` |
|        - |  9558 | ` *  $string` |
|        - |  9559 | ` *   Message to output.` |
|        - |  9560 | ` * Return` |
|        - |  9561 | ` *  NULL.` |
|        - |  9562 | ` */` |
|      ! 0 |  9563 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9564 |  |
|        - |  9565 | `	const char *zData;` |
|      ! 0 |  9566 | `	int nDataLen = 0;` |
|        - |  9567 | `	ph7_vm *pVm;` |
|        - |  9568 | `	int i,rc;` |
|        - |  9569 | `	/* Point to the target VM */` |
|      ! 0 |  9570 | `	pVm = pCtx->pVm;` |
|        - |  9571 | `	/* Output */` |
|      ! 0 |  9572 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9573 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9574 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9575 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9576 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9577 | `			if( rc == SXERR_ABORT ){` |
|        - |  9578 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9579 | `				return PH7_ABORT;` |
|        - |  9580 | `			}` |
|      ! 0 |  9581 | `		}` |
|      ! 0 |  9582 | `	}` |
|      ! 0 |  9583 | `	return SXRET_OK;` |
|      ! 0 |  9584 |  |
|        - |  9585 | `/*` |
|        - |  9586 | ` * int print($string...)` |
|        - |  9587 | ` *  Output one or more messages.` |
|        - |  9588 | ` * Parameters` |
|        - |  9589 | ` *  $string` |
|        - |  9590 | ` *   Message to output.` |
|        - |  9591 | ` * Return` |
|        - |  9592 | ` *  1 always.` |
|        - |  9593 | ` */` |
|        2 |  9594 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9595 |  |
|        - |  9596 | `	const char *zData;` |
|        3 |  9597 | `	int nDataLen = 0;` |
|        - |  9598 | `	ph7_vm *pVm;` |
|        - |  9599 | `	int i,rc;` |
|        - |  9600 | `	/* Point to the target VM */` |
|        3 |  9601 | `	pVm = pCtx->pVm;` |
|        - |  9602 | `	/* Output */` |
|        5 |  9603 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9604 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9605 | `		if( nDataLen > 0 ){` |
|        3 |  9606 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9607 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9608 | `			if( rc == SXERR_ABORT ){` |
|        - |  9609 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9610 | `				return PH7_ABORT;` |
|        - |  9611 | `			}` |
|        1 |  9612 | `		}` |
|        2 |  9613 | `	}` |
|        - |  9614 | `	/* Return 1 */` |
|        3 |  9615 | `	ph7_result_int(pCtx,1);` |
|        3 |  9616 | `	return SXRET_OK;` |
|        2 |  9617 |  |
|        - |  9618 | `/*` |
|        - |  9619 | ` * void exit(string $msg)` |
|        - |  9620 | ` * void exit(int $status)` |
|        - |  9621 | ` * void die(string $ms)` |
|        - |  9622 | ` * void die(int $status)` |
|        - |  9623 | ` *   Output a message and terminate program execution.` |
|        - |  9624 | ` * Parameter` |
|        - |  9625 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9626 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9627 | ` *  and not printed` |
|        - |  9628 | ` * Return` |
|        - |  9629 | ` *  NULL` |
|        - |  9630 | ` */` |
|      ! 0 |  9631 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9632 |  |
|      ! 0 |  9633 | `	if( nArg > 0 ){` |
|      ! 0 |  9634 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9635 | `			const char *zData;` |
|      ! 0 |  9636 | `			int iLen = 0;` |
|        - |  9637 | `			/* Print exit message */` |
|      ! 0 |  9638 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9639 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9640 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9641 | `			sxi32 iExitStatus;` |
|        - |  9642 | `			/* Record exit status code */` |
|      ! 0 |  9643 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9644 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9645 | `		}` |
|      ! 0 |  9646 | `	}` |
|        - |  9647 | `	/* Check if we are in an included file */` |
|      ! 0 |  9648 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9649 | `		/* Exit the entire process */` |
|      ! 0 |  9650 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9651 | `	}` |
|        - |  9652 | `	/* Abort processing immediately */` |
|      ! 0 |  9653 | `	return PH7_ABORT;` |
|      ! 0 |  9654 |  |
|        - |  9655 | `/*` |
|        - |  9656 | ` * bool isset($var,...)` |
|        - |  9657 | ` *  Finds out whether a variable is set.` |
|        - |  9658 | ` * Parameters` |
|        - |  9659 | ` *  One or more variable to check.` |
|        - |  9660 | ` * Return` |
|        - |  9661 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9662 | ` */` |
|    77926 |  9663 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9664 |  |
|        - |  9665 | `	ph7_value *pObj;` |
|    77928 |  9666 | `	int res = 0;` |
|        - |  9667 | `	int i;` |
|    77928 |  9668 | `	if( nArg < 1 ){` |
|        - |  9669 | `		/* Missing arguments,return false */` |
|      ! 0 |  9670 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9671 | `		return SXRET_OK;` |
|        - |  9672 | `	}` |
|        - |  9673 | `	/* Iterate over available arguments */` |
|   102532 |  9674 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    77928 |  9675 | `		pObj = apArg[i];` |
|    77928 |  9676 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    52764 |  9677 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9678 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9679 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9680 | `			}` |
|    26381 |  9681 | `		}` |
|    77928 |  9682 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    77928 |  9683 | `		if( !res ){` |
|        - |  9684 | `			/* Variable not set,return FALSE */` |
|    53324 |  9685 | `			ph7_result_bool(pCtx,0);` |
|    53324 |  9686 | `			return SXRET_OK;` |
|        - |  9687 | `		}` |
|    12304 |  9688 | `	}` |
|        - |  9689 | `	/* All given variable are set,return TRUE */` |
|    24606 |  9690 | `	ph7_result_bool(pCtx,1);` |
|    24606 |  9691 | `	return SXRET_OK;` |
|    38965 |  9692 |  |
|        - |  9693 | `/*` |
|        - |  9694 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9695 | ` * frame,the reference table and discard it's contents.` |
|        - |  9696 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9697 | ` */` |
|  3044002 |  9698 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9699 |  |
|        - |  9700 | `	ph7_value *pObj;` |
|        - |  9701 | `	VmRefObj *pRef;` |
|  3044004 |  9702 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3044004 |  9703 | `	if( pObj ){` |
|        - |  9704 | `		/* Release the object */` |
|  3044004 |  9705 | `		PH7_MemObjRelease(pObj);` |
|  1522001 |  9706 | `	}` |
|        - |  9707 | `	/* Remove old reference links */` |
|  3044004 |  9708 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3044004 |  9709 | `	if( pRef ){` |
|  3043998 |  9710 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9711 | `		/* Unlink from the reference table */` |
|  3043998 |  9712 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3043998 |  9713 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9714 | `			VmSlot sFree;` |
|        - |  9715 | `			/* Restore to the free list */` |
|  3043992 |  9716 | `			sFree.nIdx = nObjIdx;` |
|  3043992 |  9717 | `			sFree.pUserData = 0;` |
|  3043992 |  9718 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1521995 |  9719 | `		}` |
|  1521998 |  9720 | `	}` |
|  3044004 |  9721 | `	return SXRET_OK;` |
|        2 |  9722 |  |
|        - |  9723 | `/*` |
|        - |  9724 | ` * void unset($var,...)` |
|        - |  9725 | ` *   Unset one or more given variable.` |
|        - |  9726 | ` * Parameters` |
|        - |  9727 | ` *  One or more variable to unset.` |
|        - |  9728 | ` * Return` |
|        - |  9729 | ` *  Nothing.` |
|        - |  9730 | ` */` |
|     6902 |  9731 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9732 |  |
|        - |  9733 | `	ph7_value *pObj;` |
|        - |  9734 | `	ph7_vm *pVm;` |
|        - |  9735 | `	int i;` |
|        - |  9736 | `	/* Point to the target VM */` |
|     6904 |  9737 | `	pVm = pCtx->pVm;` |
|        - |  9738 | `	/* Iterate and unset */` |
|    13806 |  9739 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6904 |  9740 | `		pObj = apArg[i];` |
|     6904 |  9741 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9742 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9743 | `				/* Throw an error */` |
|      ! 0 |  9744 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9745 | `			}` |
|      ! 0 |  9746 | `		}else{` |
|     6904 |  9747 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9748 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6904 |  9749 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6898 |  9750 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3448 |  9751 | `			}` |
|        - |  9752 | `		}` |
|     3453 |  9753 | `	}` |
|     6904 |  9754 | `	return SXRET_OK;` |
|        2 |  9755 |  |
|        - |  9756 | `/*` |
|        - |  9757 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9758 | ` */` |
|      110 |  9759 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9760 |  |
|      111 |  9761 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9762 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9763 | `	ph7_value *pObj;` |
|        - |  9764 | `	sxu32 nIdx;` |
|        - |  9765 | `	/* Extract the memory object */` |
|      111 |  9766 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9767 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9768 | `	if( pObj ){` |
|      111 |  9769 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9770 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9771 | `				SyString sName;` |
|        - |  9772 | `				ph7_value sKey;` |
|        - |  9773 | `				/* Perform the insertion */` |
|      109 |  9774 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9775 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9776 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9777 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9778 | `			}` |
|       54 |  9779 | `		}` |
|       55 |  9780 | `	}` |
|      111 |  9781 | `	return SXRET_OK;` |
|        1 |  9782 |  |
|        - |  9783 | `/*` |
|        - |  9784 | ` * array get_defined_vars(void)` |
|        - |  9785 | ` *  Returns an array of all defined variables.` |
|        - |  9786 | ` * Parameter` |
|        - |  9787 | ` *  None` |
|        - |  9788 | ` * Return` |
|        - |  9789 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9790 | ` */` |
|        2 |  9791 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9792 |  |
|        3 |  9793 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9794 | `	ph7_value *pArray;` |
|        - |  9795 | `	/* Create a new array */` |
|        3 |  9796 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9797 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9798 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9799 | `		SXUNUSED(apArg);` |
|        - |  9800 | `		/* Return NULL */` |
|      ! 0 |  9801 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9802 | `		return SXRET_OK;` |
|        - |  9803 | `	}` |
|        - |  9804 | `	/* Superglobals first */` |
|        3 |  9805 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9806 | `	/* Then variable defined in the current frame */` |
|        3 |  9807 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9808 | `	/* Finally,return the created array */` |
|        3 |  9809 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9810 | `	return SXRET_OK;` |
|        2 |  9811 |  |
|        - |  9812 | `/*` |
|        - |  9813 | ` * bool gettype($var)` |
|        - |  9814 | ` *  Get the type of a variable` |
|        - |  9815 | ` * Parameters` |
|        - |  9816 | ` *   $var` |
|        - |  9817 | ` *    The variable being type checked.` |
|        - |  9818 | ` * Return` |
|        - |  9819 | ` *   String representation of the given variable type.` |
|        - |  9820 | ` */` |
|       32 |  9821 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9822 |  |
|       34 |  9823 | `	const char *zType = "Empty";` |
|       34 |  9824 | `	if( nArg > 0 ){` |
|       34 |  9825 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9826 | `	}` |
|        - |  9827 | `	/* Return the variable type */` |
|       34 |  9828 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9829 | `	return SXRET_OK;` |
|        2 |  9830 |  |
|        - |  9831 | `/*` |
|        - |  9832 | ` * string get_resource_type(resource $handle)` |
|        - |  9833 | ` *  This function gets the type of the given resource.` |
|        - |  9834 | ` * Parameters` |
|        - |  9835 | ` *  $handle` |
|        - |  9836 | ` *  The evaluated resource handle.` |
|        - |  9837 | ` * Return` |
|        - |  9838 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9839 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9840 | ` *  the return value will be the string Unknown.` |
|        - |  9841 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9842 | ` *  is not a resource.` |
|        - |  9843 | ` */` |
|        2 |  9844 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9845 |  |
|        3 |  9846 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9847 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9848 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9849 | `		return PH7_OK;` |
|        - |  9850 | `	}` |
|        3 |  9851 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9852 | `	return SXRET_OK;` |
|        2 |  9853 |  |
|        - |  9854 | `/*` |
|        - |  9855 | ` * void var_dump(expression,....)` |
|        - |  9856 | ` *   var_dump � Dumps information about a variable` |
|        - |  9857 | ` * Parameters` |
|        - |  9858 | ` *   One or more expression to dump.` |
|        - |  9859 | ` * Returns` |
|        - |  9860 | ` *  Nothing.` |
|        - |  9861 | ` */` |
|      218 |  9862 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9863 |  |
|        - |  9864 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9865 | `	int i;` |
|      220 |  9866 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9867 | `	/* Dump one or more expressions */` |
|      444 |  9868 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9869 | `		ph7_value *pObj = apArg[i];` |
|        - |  9870 | `		/* Reset the working buffer */` |
|      226 |  9871 | `		SyBlobReset(&sDump);` |
|        - |  9872 | `		/* Dump the given expression */` |
|      226 |  9873 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9874 | `		/* Output */` |
|      226 |  9875 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9876 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9877 | `		}` |
|      114 |  9878 | `	}` |
|        - |  9879 | `	/* Release the working buffer */` |
|      220 |  9880 | `	SyBlobRelease(&sDump);` |
|      220 |  9881 | `	return SXRET_OK;` |
|        2 |  9882 |  |
|        - |  9883 | `/*` |
|        - |  9884 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9885 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9886 | ` * Parameters` |
|        - |  9887 | ` *   expression: Expression to dump` |
|        - |  9888 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9889 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9890 | ` *            print_r() will return the information rather than print it.` |
|        - |  9891 | ` * Return` |
|        - |  9892 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9893 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9894 | ` */` |
|       16 |  9895 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9896 |  |
|       17 |  9897 | `	int ret_string = 0;` |
|        - |  9898 | `	SyBlob sDump;` |
|       17 |  9899 | `	if( nArg < 1 ){` |
|        - |  9900 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9901 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9902 | `		return SXRET_OK;` |
|        - |  9903 | `	}` |
|       17 |  9904 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9905 | `	if ( nArg > 1 ){` |
|        - |  9906 | `		/* Where to redirect output */` |
|       11 |  9907 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9908 | `	}` |
|        - |  9909 | `	/* Generate dump */` |
|       17 |  9910 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9911 | `	if( !ret_string ){` |
|        - |  9912 | `		/* Output dump */` |
|        7 |  9913 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9914 | `		/* Return true */` |
|        7 |  9915 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9916 | `	}else{` |
|        - |  9917 | `		/* Generated dump as return value */` |
|       11 |  9918 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9919 | `	}` |
|        - |  9920 | `	/* Release the working buffer */` |
|       17 |  9921 | `	SyBlobRelease(&sDump);` |
|       17 |  9922 | `	return SXRET_OK;` |
|        9 |  9923 |  |
|        - |  9924 | `/*` |
|        - |  9925 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9926 | ` * Same job as print_r. (see coment above)` |
|        - |  9927 | ` */` |
|        2 |  9928 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9929 |  |
|        3 |  9930 | `	int ret_string = 0;` |
|        - |  9931 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9932 | `	if( nArg < 1 ){` |
|        - |  9933 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9934 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9935 | `		return SXRET_OK;` |
|        - |  9936 | `	}` |
|        3 |  9937 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9938 | `	if ( nArg > 1 ){` |
|        - |  9939 | `		/* Where to redirect output */` |
|        3 |  9940 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9941 | `	}` |
|        - |  9942 | `	/* Generate dump */` |
|        3 |  9943 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9944 | `	if( !ret_string ){` |
|        - |  9945 | `		/* Output dump */` |
|      ! 0 |  9946 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9947 | `		/* Return NULL */` |
|      ! 0 |  9948 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9949 | `	}else{` |
|        - |  9950 | `		/* Generated dump as return value */` |
|        3 |  9951 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9952 | `	}` |
|        - |  9953 | `	/* Release the working buffer */` |
|        3 |  9954 | `	SyBlobRelease(&sDump);` |
|        3 |  9955 | `	return SXRET_OK;` |
|        2 |  9956 |  |
|        - |  9957 | `/*` |
|        - |  9958 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9959 | ` *  Set/get the various assert flags.` |
|        - |  9960 | ` * Parameter` |
|        - |  9961 | ` * $what` |
|        - |  9962 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9963 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9964 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9965 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9966 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9967 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9968 | ` * $value` |
|        - |  9969 | ` *   An optional new value for the option.` |
|        - |  9970 | ` * Return` |
|        - |  9971 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9972 | ` */` |
|       28 |  9973 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9974 |  |
|       30 |  9975 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9976 | `	int iOption;` |
|        - |  9977 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 |  9978 | `	if( nArg < 1 ){` |
|        3 |  9979 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9980 | `			"ArgumentCountError",` |
|        - |  9981 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9982 | `			);` |
|        - |  9983 | `	}` |
|        - |  9984 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 |  9985 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 |  9986 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9987 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9988 | `			"TypeError",` |
|        - |  9989 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9990 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9991 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9992 | `			);` |
|        - |  9993 | `	}` |
|       28 |  9994 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9995 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9996 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9997 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 |  9998 | `	switch( iOption ){` |
|        5 |  9999 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 10000 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 10001 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 10002 | `		if( nArg > 1 ){` |
|        5 | 10003 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 10004 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 10005 | `			}else{` |
|        3 | 10006 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 10007 | `			}` |
|        2 | 10008 | `		}` |
|       12 | 10009 | `		break;` |
|        1 | 10010 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 10011 | `		/* Return old callback or null */` |
|        3 | 10012 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 10013 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 10014 | `		}else{` |
|        3 | 10015 | `			ph7_result_null(pCtx);` |
|        - | 10016 | `		}` |
|        3 | 10017 | `		if( nArg > 1 ){` |
|      ! 0 | 10018 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 10019 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 10020 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 10021 | `			}else{` |
|      ! 0 | 10022 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 10023 | `			}` |
|      ! 0 | 10024 | `		}` |
|        3 | 10025 | `		break;` |
|        5 | 10026 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 10027 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 10028 | `		if( nArg > 1 ){` |
|        5 | 10029 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 10030 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 10031 | `			}else{` |
|        3 | 10032 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 10033 | `			}` |
|        2 | 10034 | `		}` |
|       11 | 10035 | `		break;` |
|      ! 0 | 10036 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 10037 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 10038 | `		break;` |
|        1 | 10039 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 10040 | `		ph7_result_int(pCtx, 1);` |
|        3 | 10041 | `		break;` |
|      ! 0 | 10042 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 10043 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 10044 | `		break;` |
|        1 | 10045 | `	default:` |
|        - | 10046 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 10047 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10048 | `			"ValueError",` |
|        - | 10049 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 10050 | `			);` |
|        - | 10051 | `	}` |
|       26 | 10052 | `	return PH7_OK;` |
|       16 | 10053 |  |
|        - | 10054 | `/*` |
|        - | 10055 | ` * bool assert(mixed $assertion)` |
|        - | 10056 | ` *  Checks if assertion is FALSE.` |
|        - | 10057 | ` * Parameter` |
|        - | 10058 | ` *  $assertion` |
|        - | 10059 | ` *    The assertion to test.` |
|        - | 10060 | ` * Return` |
|        - | 10061 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 10062 | ` */` |
|       24 | 10063 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10064 |  |
|       26 | 10065 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10066 | `	int iFlags,iResult;` |
|        - | 10067 | `	const char *zDesc;` |
|        - | 10068 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 10069 | `	if( nArg < 1 ){` |
|        3 | 10070 | `		return PH7_VmThrowException(pCtx,` |
|        - | 10071 | `			"ArgumentCountError",` |
|        - | 10072 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 10073 | `			);` |
|        - | 10074 | `	}` |
|       24 | 10075 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 10076 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 10077 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 10078 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 10079 | `		return PH7_OK;` |
|        - | 10080 | `	}` |
|        - | 10081 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 10082 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 10083 | `	if( !iResult ){` |
|        - | 10084 | `		/* Assertion failed */` |
|        - | 10085 | `		/* Extract optional description */` |
|       13 | 10086 | `		zDesc = 0;` |
|       13 | 10087 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 10088 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 10089 | `		}` |
|       13 | 10090 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 10091 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 10092 | `			ph7_value sFile,sLine;` |
|        - | 10093 | `			ph7_value *apCbArg[3];` |
|        - | 10094 | `			SyString *pFile;` |
|        - | 10095 | `			/* Extract the processed script */` |
|      ! 0 | 10096 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 10097 | `			if( pFile == 0 ){` |
|      ! 0 | 10098 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 10099 | `			}` |
|        - | 10100 | `			/* Invoke the callback */` |
|      ! 0 | 10101 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 10102 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 10103 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 10104 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 10105 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 10106 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 10107 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 10108 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 10109 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 10110 | `		}` |
|       13 | 10111 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 10112 | `			/* Abort VM execution immediately */` |
|      ! 0 | 10113 | `			return PH7_ABORT;` |
|        - | 10114 | `		}` |
|        - | 10115 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 10116 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 10117 | `			return PH7_VmThrowException(pCtx,` |
|        - | 10118 | `				"AssertionError",` |
|        - | 10119 | `				"%s",` |
|        1 | 10120 | `				zDesc` |
|        - | 10121 | `				);` |
|      ! 0 | 10122 | `		}else{` |
|       11 | 10123 | `			return PH7_VmThrowException(pCtx,` |
|        - | 10124 | `				"AssertionError",` |
|        - | 10125 | `				"assert(false)"` |
|        - | 10126 | `				);` |
|        - | 10127 | `		}` |
|        - | 10128 | `	}` |
|        - | 10129 | `	/* Assertion passed */` |
|       11 | 10130 | `	ph7_result_bool(pCtx,1);` |
|       11 | 10131 | `	return PH7_OK;` |
|       14 | 10132 |  |
|        - | 10133 | `/*` |
|        - | 10134 | ` * Section:` |
|        - | 10135 | ` *  Error reporting functions.` |
|        - | 10136 | ` * Status:` |
|        - | 10137 | ` *    Stable.` |
|        - | 10138 | ` */` |
|        - | 10139 | `/*` |
|        - | 10140 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 10141 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 10142 | ` * Parameters` |
|        - | 10143 | ` *  $error_msg` |
|        - | 10144 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 10145 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 10146 | ` * $error_type` |
|        - | 10147 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 10148 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 10149 | ` * Return` |
|        - | 10150 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 10151 | ` */` |
|       12 | 10152 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10153 |  |
|       14 | 10154 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 10155 | `	int rc = PH7_OK;` |
|       14 | 10156 | `	if( nArg > 0 ){` |
|        - | 10157 | `		const char *zErr;` |
|        - | 10158 | `		int nLen;` |
|        - | 10159 | `		/* Extract the error message */` |
|       12 | 10160 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 10161 | `		if( nArg > 1 ){` |
|        - | 10162 | `			/* Extract the error type */` |
|       12 | 10163 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 10164 | `			switch( nErr ){` |
|        1 | 10165 | `			case 1:   /* E_ERROR */` |
|        - | 10166 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 10167 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 10168 | `			case 256: /* E_USER_ERROR */` |
|        3 | 10169 | `				nErr = PH7_CTX_ERR;` |
|        3 | 10170 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 10171 | `				break;` |
|        1 | 10172 | `			case 2:   /* E_WARNING */` |
|        - | 10173 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 10174 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 10175 | `			case 512: /* E_USER_WARNING */` |
|        3 | 10176 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 10177 | `				break;` |
|        3 | 10178 | `			default:` |
|        8 | 10179 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 10180 | `				break;` |
|        - | 10181 | `			}` |
|        5 | 10182 | `		}` |
|        - | 10183 | `		/* Report error */` |
|       12 | 10184 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 10185 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 10186 | `			return rc;` |
|        - | 10187 | `		}` |
|        - | 10188 | `		/* Return true */` |
|       12 | 10189 | `		ph7_result_bool(pCtx,1);` |
|        7 | 10190 | `	}else{` |
|        - | 10191 | `		/* Missing arguments,return FALSE */` |
|        3 | 10192 | `		ph7_result_bool(pCtx,0);` |
|        - | 10193 | `	}` |
|       14 | 10194 | `	return rc;` |
|        8 | 10195 |  |
|        - | 10196 | `/*` |
|        - | 10197 | ` * int error_reporting([int $level])` |
|        - | 10198 | ` *  Sets which PHP errors are reported.` |
|        - | 10199 | ` * Parameters` |
|        - | 10200 | ` *  $level` |
|        - | 10201 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 10202 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 10203 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 10204 | ` *   levels will not always behave as expected.` |
|        - | 10205 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 10206 | ` *   in the predefined constants.` |
|        - | 10207 | ` * Return` |
|        - | 10208 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 10209 | ` *   parameter is given.` |
|        - | 10210 | ` */` |
|       38 | 10211 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10212 |  |
|       40 | 10213 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10214 | `	int nOld;` |
|        - | 10215 | `	/* Extract the old reporting level */` |
|       40 | 10216 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 10217 | `	if( nArg > 0 ){` |
|        - | 10218 | `		int nNew;` |
|        - | 10219 | `		/* Extract the desired error reporting level */` |
|       32 | 10220 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 10221 | `		if( !nNew ){` |
|        - | 10222 | `			/* Do not report errors at all */` |
|        5 | 10223 | `			pVm->bErrReport = 0;` |
|        3 | 10224 | `		}else{` |
|        - | 10225 | `			/* Report all errors */` |
|       28 | 10226 | `			pVm->bErrReport = 1;` |
|        - | 10227 | `		}` |
|       15 | 10228 | `	}` |
|        - | 10229 | `	/* Return the old level */` |
|       40 | 10230 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 10231 | `	return PH7_OK;` |
|        2 | 10232 |  |
|        - | 10233 | `/*` |
|        - | 10234 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 10235 | ` *  Send an error message somewhere.` |
|        - | 10236 | ` * Parameter` |
|        - | 10237 | ` *  $message` |
|        - | 10238 | ` *   The error message that should be logged.` |
|        - | 10239 | ` *  $message_type` |
|        - | 10240 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 10241 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 10242 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 10243 | ` *       This is the default option.` |
|        - | 10244 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 10245 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 10246 | ` *    2  No longer an option.` |
|        - | 10247 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 10248 | ` *       to the end of the message string.` |
|        - | 10249 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 10250 | ` *  $destination` |
|        - | 10251 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 10252 | ` *  $extra_headers` |
|        - | 10253 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 10254 | ` * Return` |
|        - | 10255 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10256 | ` * NOTE:` |
|        - | 10257 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 10258 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 10259 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 10260 | ` *  Otherwise this function is no-op.` |
|        - | 10261 | ` */` |
|        4 | 10262 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10263 |  |
|        - | 10264 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 10265 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 10266 | `	int iType = 0;` |
|        5 | 10267 | `	if( nArg < 1 ){` |
|        - | 10268 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 10269 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10270 | `		return PH7_OK;` |
|        - | 10271 | `	}` |
|        5 | 10272 | `	if( pVm->xErrLog  ){` |
|        - | 10273 | `		/* Invoke the user callback */` |
|      ! 0 | 10274 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 10275 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 10276 | `		if( nArg > 1 ){` |
|      ! 0 | 10277 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 10278 | `			if( nArg > 2 ){` |
|      ! 0 | 10279 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 10280 | `				if( nArg > 3 ){` |
|      ! 0 | 10281 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 10282 | `				}` |
|      ! 0 | 10283 | `			}` |
|      ! 0 | 10284 | `		}` |
|      ! 0 | 10285 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 10286 | `	}` |
|        - | 10287 | `	/* Retun TRUE */` |
|        5 | 10288 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10289 | `	return PH7_OK;` |
|        3 | 10290 |  |
|        - | 10291 | `/*` |
|        - | 10292 | ` * bool restore_exception_handler(void)` |
|        - | 10293 | ` *  Restores the previously defined exception handler function.` |
|        - | 10294 | ` * Parameter` |
|        - | 10295 | ` *  None` |
|        - | 10296 | ` * Return` |
|        - | 10297 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10298 | ` */` |
|        4 | 10299 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10300 |  |
|        5 | 10301 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10302 | `	ph7_value *pOld,*pNew;` |
|        - | 10303 | `	/* Point to the old and the new handler */` |
|        5 | 10304 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10305 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10306 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10307 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10308 | `		SXUNUSED(apArg);` |
|        - | 10309 | `		/* No installed handler,return FALSE */` |
|        5 | 10310 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10311 | `		return PH7_OK;` |
|        - | 10312 | `	}` |
|        - | 10313 | `	/* Copy the old handler */` |
|      ! 0 | 10314 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10315 | `	PH7_MemObjRelease(pOld);` |
|        - | 10316 | `	/* Return TRUE */` |
|      ! 0 | 10317 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10318 | `	return PH7_OK;` |
|        3 | 10319 |  |
|        - | 10320 | `/*` |
|        - | 10321 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10322 | ` *  Sets a user-defined exception handler function.` |
|        - | 10323 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10324 | ` * NOTE` |
|        - | 10325 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10326 | ` *  the satndard PHP engine.` |
|        - | 10327 | ` * Parameters` |
|        - | 10328 | ` *  $exception_handler` |
|        - | 10329 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10330 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10331 | ` *   that was thrown.` |
|        - | 10332 | ` *  Note:` |
|        - | 10333 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10334 | ` * Return` |
|        - | 10335 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10336 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10337 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10338 | ` */` |
|        4 | 10339 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10340 |  |
|        6 | 10341 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10342 | `	ph7_value *pOld,*pNew;` |
|        - | 10343 | `	/* Point to the old and the new handler */` |
|        6 | 10344 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10345 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10346 | `	/* Return the old handler */` |
|        6 | 10347 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10348 | `	if( nArg > 0 ){` |
|        6 | 10349 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10350 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10351 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10352 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10353 | `		}else{` |
|        6 | 10354 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10355 | `			/* Install the new handler */` |
|        6 | 10356 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10357 | `		}` |
|        2 | 10358 | `	}` |
|        6 | 10359 | `	return PH7_OK;` |
|        2 | 10360 |  |
|        - | 10361 | `/*` |
|        - | 10362 | ` * bool restore_error_handler(void)` |
|        - | 10363 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10364 | ` * Parameters:` |
|        - | 10365 | ` *  None.` |
|        - | 10366 | ` * Return` |
|        - | 10367 | ` *  Always TRUE.` |
|        - | 10368 | ` */` |
|        4 | 10369 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10370 |  |
|        5 | 10371 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10372 | `	ph7_value *pOld,*pNew;` |
|        - | 10373 | `	/* Point to the old and the new handler */` |
|        5 | 10374 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10375 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10376 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10377 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10378 | `		SXUNUSED(apArg);` |
|        - | 10379 | `		/* No installed callback,return FALSE */` |
|        5 | 10380 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10381 | `		return PH7_OK;` |
|        - | 10382 | `	}` |
|        - | 10383 | `	/* Copy the old callback */` |
|      ! 0 | 10384 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10385 | `	PH7_MemObjRelease(pOld);` |
|        - | 10386 | `	/* Return TRUE */` |
|      ! 0 | 10387 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10388 | `	return PH7_OK;` |
|        3 | 10389 |  |
|        - | 10390 | `/*` |
|        - | 10391 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10392 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10393 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10394 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10395 | ` *  Sets a user-defined error handler function.` |
|        - | 10396 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10397 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10398 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10399 | ` *  conditions (using trigger_error()).` |
|        - | 10400 | ` * Parameters` |
|        - | 10401 | ` *  $error_handler` |
|        - | 10402 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10403 | ` *   describing the error.` |
|        - | 10404 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10405 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10406 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10407 | ` *   The function can be shown as:` |
|        - | 10408 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10409 | ` *     errno` |
|        - | 10410 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10411 | ` *   errstr` |
|        - | 10412 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10413 | ` *   errfile` |
|        - | 10414 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10415 | ` *     was raised in, as a string.` |
|        - | 10416 | ` *  Note:` |
|        - | 10417 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10418 | ` * Return` |
|        - | 10419 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10420 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10421 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10422 | ` */` |
|     9478 | 10423 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10424 |  |
|     9480 | 10425 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10426 | `	ph7_value *pOld,*pNew;` |
|        - | 10427 | `	/* Point to the old and the new handler */` |
|     9480 | 10428 | `	pOld = &pVm->aErrCB[0];` |
|     9480 | 10429 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10430 | `	/* Return the old handler */` |
|     9480 | 10431 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9480 | 10432 | `	if( nArg > 0 ){` |
|     9480 | 10433 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10434 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4739 | 10435 | `			PH7_MemObjRelease(pNew);` |
|     4739 | 10436 | `			ph7_result_bool(pCtx,1);` |
|     2370 | 10437 | `		}else{` |
|     4742 | 10438 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10439 | `			/* Install the new handler */` |
|     4742 | 10440 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10441 | `		}` |
|     4739 | 10442 | `	}` |
|     9480 | 10443 | `	return PH7_OK;` |
|        2 | 10444 |  |
|        - | 10445 | `/*` |
|        - | 10446 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10447 | ` *  Generates a backtrace.` |
|        - | 10448 | ` * Paramaeter` |
|        - | 10449 | ` *  $options` |
|        - | 10450 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10451 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10452 | ` *   all the function/method arguments, to save memory.` |
|        - | 10453 | ` * $limit` |
|        - | 10454 | ` *   (Not Used)` |
|        - | 10455 | ` * Return` |
|        - | 10456 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10457 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10458 | ` *          Name        Type      Description` |
|        - | 10459 | ` *          ------      ------     -----------` |
|        - | 10460 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10461 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10462 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10463 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10464 | ` *          object      object    The current object.` |
|        - | 10465 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10466 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10467 | ` */` |
|      554 | 10468 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10469 |  |
|      556 | 10470 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10471 | `	ph7_value *pArray;` |
|        - | 10472 | `	ph7_class *pClass;` |
|        - | 10473 | `	ph7_value *pValue;` |
|        - | 10474 | `	SyString *pFile;` |
|        - | 10475 | `	/* Create a new array */` |
|      556 | 10476 | `	pArray = ph7_context_new_array(pCtx);` |
|      556 | 10477 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      556 | 10478 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10479 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10480 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10481 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10482 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10483 | `		SXUNUSED(apArg);` |
|      ! 0 | 10484 | `		return PH7_OK;` |
|        - | 10485 | `	}` |
|        - | 10486 | `	/* Dump running function name and it's arguments  */` |
|      556 | 10487 | `	if( pVm->pFrame->pParent ){` |
|      556 | 10488 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10489 | `		ph7_vm_func *pFunc;` |
|        - | 10490 | `		ph7_value *pArg;` |
|      556 | 10491 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      556 | 10492 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      556 | 10493 | `		if( pFrame->pParent && pFunc ){` |
|      556 | 10494 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      556 | 10495 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      556 | 10496 | `			ph7_value_reset_string_cursor(pValue);` |
|      277 | 10497 | `		}` |
|        - | 10498 | `		/* Function arguments */` |
|      556 | 10499 | `		pArg = ph7_context_new_array(pCtx);` |
|      556 | 10500 | `		if( pArg  ){` |
|        - | 10501 | `			ph7_value *pObj;` |
|        - | 10502 | `			VmSlot *aSlot;` |
|        - | 10503 | `			sxu32 n;` |
|        - | 10504 | `			/* Start filling the array with the given arguments */` |
|      556 | 10505 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2210 | 10506 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1656 | 10507 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1656 | 10508 | `				if( pObj ){` |
|     1656 | 10509 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      827 | 10510 | `				}` |
|      829 | 10511 | `			}` |
|        - | 10512 | `			/* Save the array */` |
|      556 | 10513 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      277 | 10514 | `		}` |
|      277 | 10515 | `	}` |
|      556 | 10516 | `	ph7_value_int(pValue,1);` |
|        - | 10517 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10518 | `	 * line numbers at run-time. )` |
|        - | 10519 | `	 */` |
|      556 | 10520 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10521 | `	/* Current processed script */` |
|      556 | 10522 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      556 | 10523 | `	if( pFile ){` |
|      556 | 10524 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      556 | 10525 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      556 | 10526 | `		ph7_value_reset_string_cursor(pValue);` |
|      277 | 10527 | `	}` |
|        - | 10528 | `	/* Top class */` |
|      556 | 10529 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      556 | 10530 | `	if( pClass ){` |
|      552 | 10531 | `		ph7_value_reset_string_cursor(pValue);` |
|      552 | 10532 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      552 | 10533 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      275 | 10534 | `	}` |
|        - | 10535 | `	/* Return the freshly created array */` |
|      556 | 10536 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10537 | `	/*` |
|        - | 10538 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10539 | `	 * as soon we return from this function.` |
|        - | 10540 | `	 */` |
|      556 | 10541 | `	return PH7_OK;` |
|      279 | 10542 |  |
|        - | 10543 | `/*` |
|        - | 10544 | ` * Generate a small backtrace.` |
|        - | 10545 | ` * Store the generated dump in the given BLOB` |
|        - | 10546 | ` */` |
|        4 | 10547 | `static int VmMiniBacktrace(` |
|        - | 10548 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10549 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10550 | `	)` |
|        1 | 10551 |  |
|        5 | 10552 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10553 | `	ph7_vm_func *pFunc;` |
|        - | 10554 | `	ph7_class *pClass;` |
|        - | 10555 | `	SyString *pFile;` |
|        - | 10556 | `	/* Called function */` |
|        5 | 10557 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10558 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10559 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10560 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10561 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10562 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10563 | `	}else{` |
|      ! 0 | 10564 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10565 | `	}` |
|        5 | 10566 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10567 | `	/* Current processed script */` |
|        5 | 10568 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10569 | `	if( pFile ){` |
|        5 | 10570 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10571 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10572 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10573 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10574 | `	}` |
|        - | 10575 | `	/* Top class */` |
|        5 | 10576 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10577 | `	if( pClass ){` |
|      ! 0 | 10578 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10579 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10580 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10581 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10582 | `	}` |
|        5 | 10583 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10584 | `	/* All done */` |
|        5 | 10585 | `	return SXRET_OK;` |
|        1 | 10586 |  |
|        - | 10587 | `/*` |
|        - | 10588 | ` * void debug_print_backtrace()` |
|        - | 10589 | ` *  Prints a backtrace` |
|        - | 10590 | ` * Parameters` |
|        - | 10591 | ` * None` |
|        - | 10592 | ` * Return` |
|        - | 10593 | ` * NULL` |
|        - | 10594 | ` */` |
|        2 | 10595 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10596 |  |
|        3 | 10597 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10598 | `	SyBlob sDump;` |
|        3 | 10599 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10600 | `	/* Generate the backtrace */` |
|        3 | 10601 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10602 | `	/* Output backtrace */` |
|        3 | 10603 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10604 | `	/* All done,cleanup */` |
|        3 | 10605 | `	SyBlobRelease(&sDump);` |
|        1 | 10606 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10607 | `	SXUNUSED(apArg);` |
|        3 | 10608 | `	return PH7_OK;` |
|        1 | 10609 |  |
|        - | 10610 | `/*` |
|        - | 10611 | ` * string debug_string_backtrace()` |
|        - | 10612 | ` *  Generate a backtrace` |
|        - | 10613 | ` * Parameters` |
|        - | 10614 | ` * None` |
|        - | 10615 | ` * Return` |
|        - | 10616 | ` *  A mini backtrace().` |
|        - | 10617 | ` * Note that this is a symisc extension.` |
|        - | 10618 | ` */` |
|        2 | 10619 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10620 |  |
|        3 | 10621 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10622 | `	SyBlob sDump;` |
|        3 | 10623 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10624 | `	/* Generate the backtrace */` |
|        3 | 10625 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10626 | `	/* Return the backtrace */` |
|        3 | 10627 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10628 | `	/* All done,cleanup */` |
|        3 | 10629 | `	SyBlobRelease(&sDump);` |
|        1 | 10630 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10631 | `	SXUNUSED(apArg);` |
|        3 | 10632 | `	return PH7_OK;` |
|        1 | 10633 |  |
|        - | 10634 | `/*` |
|        - | 10635 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10636 | ` * exception is triggered.` |
|        - | 10637 | ` */` |
|      480 | 10638 | `static sxi32 VmUncaughtException(` |
|        - | 10639 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10640 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10641 | `	)` |
|        1 | 10642 |  |
|        - | 10643 | `	ph7_value *apArg[2],sArg;` |
|      481 | 10644 | `	int nArg = 1;` |
|        - | 10645 | `	sxi32 rc;` |
|      481 | 10646 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10647 | `		/* Nesting limit reached */` |
|      ! 0 | 10648 | `		return SXRET_OK;` |
|        - | 10649 | `	}` |
|        - | 10650 | `	/* Call any exception handler if available */` |
|      481 | 10651 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 10652 | `	if( pThis ){` |
|        - | 10653 | `		/* Load the exception instance */` |
|      481 | 10654 | `		sArg.x.pOther = pThis;` |
|      481 | 10655 | `		pThis->iRef++;` |
|      481 | 10656 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 10657 | `	}else{` |
|      ! 0 | 10658 | `		nArg = 0;` |
|        - | 10659 | `	}` |
|      481 | 10660 | `	apArg[0] = &sArg;` |
|        - | 10661 | `	/* Call the exception handler if available */` |
|      481 | 10662 | `	pVm->nExceptDepth++;` |
|      481 | 10663 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 10664 | `	pVm->nExceptDepth--;` |
|      481 | 10665 | `	if( rc != SXRET_OK ){` |
|        - | 10666 | `		SyBlob sMsgBuf;` |
|      479 | 10667 | `		const char *zClass = "Exception";` |
|      479 | 10668 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10669 | `		const char *zMsg;` |
|        - | 10670 | `		sxu32 nMsg;` |
|        - | 10671 | `		const char *zFuncName;` |
|        - | 10672 | `		int nFuncLen;` |
|      479 | 10673 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 10674 | `		if( pThis ){` |
|        - | 10675 | `			ph7_class_method *pGetMessage;` |
|        - | 10676 | `			ph7_value sMsg;` |
|        - | 10677 | `			const char *zTmp;` |
|        - | 10678 | `			int nTmp;` |
|      479 | 10679 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 10680 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 10681 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 10682 | `			if( pGetMessage ){` |
|      479 | 10683 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 10684 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 10685 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 10686 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 10687 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 10688 | `					}` |
|      239 | 10689 | `				}` |
|      479 | 10690 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 10691 | `			}` |
|      239 | 10692 | `		}` |
|      479 | 10693 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10694 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10695 | `		}` |
|      479 | 10696 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 10697 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 10698 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 10699 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 10700 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10701 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 10702 | `		rc = SXERR_ABORT;` |
|      239 | 10703 | `	}` |
|      481 | 10704 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 10705 | `	return rc;` |
|      241 | 10706 |  |
|        - | 10707 | `/*` |
|        - | 10708 | ` * Throw a user exception.` |
|        - | 10709 | ` *` |
|        - | 10710 | ` * Exception dispatch follows this sequence:` |
|        - | 10711 | ` *` |
|        - | 10712 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10713 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10714 | ` *` |
|        - | 10715 | ` * 2. If NO catch matches:` |
|        - | 10716 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10717 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10718 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10719 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10720 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10721 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10722 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10723 | ` *` |
|        - | 10724 | ` * 3. If a catch DOES match:` |
|        - | 10725 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10726 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10727 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10728 | ` *       finally block.` |
|        - | 10729 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10730 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10731 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10732 | ` *       in pPendingException (step 2c).` |
|        - | 10733 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10734 | ` *    d. Run finally (if present).` |
|        - | 10735 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10736 | ` *       that handlers are restored and finally has run.` |
|        - | 10737 | ` */` |
|      558 | 10738 | `static sxi32 VmThrowException(` |
|        - | 10739 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10740 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10741 | `	)` |
|        2 | 10742 |  |
|        - | 10743 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10744 | `	ph7_exception **apException;` |
|        - | 10745 | `	ph7_exception *pException;` |
|        - | 10746 | `	/* Point to the stack of loaded exceptions */` |
|      560 | 10747 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      560 | 10748 | `	pException = 0;` |
|      560 | 10749 | `	pCatch = 0;` |
|      560 | 10750 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10751 | `		ph7_exception_block *aCatch;` |
|        - | 10752 | `		ph7_class *pClass;` |
|        - | 10753 | `		SyString *aNames;` |
|        - | 10754 | `		sxu32 nNames;` |
|        - | 10755 | `		int matched;` |
|        - | 10756 | `		sxu32 j,k;` |
|        - | 10757 | `		/* Locate the appropriate block to execute */` |
|       74 | 10758 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       74 | 10759 | `		(void)SySetPop(&pVm->aException);` |
|       74 | 10760 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       76 | 10761 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 10762 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|       74 | 10763 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|       74 | 10764 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|       74 | 10765 | `			matched = 0;` |
|       88 | 10766 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 10767 | `				/* Extract the target class */` |
|       86 | 10768 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,TRUE,0);` |
|       86 | 10769 | `				if( pClass == 0 ){` |
|        - | 10770 | `					/* No such class */` |
|      ! 0 | 10771 | `					continue;` |
|        - | 10772 | `				}` |
|       86 | 10773 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|       72 | 10774 | `					matched = 1;` |
|       72 | 10775 | `					break;` |
|        - | 10776 | `				}` |
|        8 | 10777 | `			}` |
|       74 | 10778 | `			if( matched ){` |
|        - | 10779 | `				/* Catch block found,break immediately */` |
|       72 | 10780 | `				pCatch = &aCatch[j];` |
|       72 | 10781 | `				break;` |
|        - | 10782 | `			}` |
|        2 | 10783 | `		}` |
|       36 | 10784 | `	}` |
|        - | 10785 | `	/* Execute the cached block if available */` |
|      560 | 10786 | `	if( pCatch == 0 ){` |
|        - | 10787 | `		sxi32 rc;` |
|        - | 10788 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      490 | 10789 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10790 | `			pException->iFinallyDone = 1;` |
|        3 | 10791 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10792 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10793 | `				return SXERR_ABORT;` |
|        - | 10794 | `			}` |
|        1 | 10795 | `		}` |
|        - | 10796 | `		/* Check if there is an outer exception handler on the stack */` |
|      490 | 10797 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10798 | `			/* Re-throw to the outer handler */` |
|        3 | 10799 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10800 | `		}` |
|        - | 10801 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10802 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10803 | `		 * exception instead of reporting it uncaught.` |
|        - | 10804 | `		 */` |
|      488 | 10805 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10806 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10807 | `			 * by looking for a catch frame on the stack.` |
|        - | 10808 | `			 */` |
|      488 | 10809 | `			VmFrame *pF = pVm->pFrame;` |
|      488 | 10810 | `			int inCatch = 0;` |
|      974 | 10811 | `			while( pF ){` |
|      494 | 10812 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        7 | 10813 | `					inCatch = 1;` |
|        7 | 10814 | `					break;` |
|        - | 10815 | `				}` |
|      487 | 10816 | `				pF = pF->pParent;` |
|        1 | 10817 | `			}` |
|      488 | 10818 | `			if( inCatch ){` |
|        - | 10819 | `				/* Defer — will be re-thrown after finally runs */` |
|        7 | 10820 | `				pThis->iRef++;` |
|        7 | 10821 | `				pVm->pPendingException = pThis;` |
|        7 | 10822 | `				return SXRET_OK;` |
|        - | 10823 | `			}` |
|      240 | 10824 | `		}` |
|        - | 10825 | `		/* Truly uncaught */` |
|      481 | 10826 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 10827 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10828 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10829 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10830 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10831 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10832 | `			}` |
|      ! 0 | 10833 | `		}` |
|      481 | 10834 | `		return rc;` |
|      ! 0 | 10835 | `	}else{` |
|       72 | 10836 | `		VmFrame *pFrame = pVm->pFrame;` |
|       72 | 10837 | `		ph7_exception **apSaved = 0;` |
|        - | 10838 | `		sxu32 nSavedCount;` |
|        - | 10839 | `		sxi32 rc;` |
|       72 | 10840 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       72 | 10841 | `		if( pException->pFrame == pFrame ){` |
|       48 | 10842 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       23 | 10843 | `		}` |
|        - | 10844 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10845 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10846 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10847 | `		 */` |
|       72 | 10848 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       72 | 10849 | `		if( nSavedCount > 0 ){` |
|       13 | 10850 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 | 10851 | `				nSavedCount * sizeof(ph7_exception *));` |
|        9 | 10852 | `			if( apSaved ){` |
|       13 | 10853 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        4 | 10854 | `					nSavedCount * sizeof(ph7_exception *));` |
|        9 | 10855 | `				SySetReset(&pVm->aException);` |
|        4 | 10856 | `			}` |
|        4 | 10857 | `		}` |
|        - | 10858 | `		/* Create a private frame first */` |
|       72 | 10859 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       72 | 10860 | `		if( rc == SXRET_OK ){` |
|       72 | 10861 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       72 | 10862 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       72 | 10863 | `			if( pObj ){` |
|       72 | 10864 | `				pThis->iRef++;` |
|       72 | 10865 | `				pObj->x.pOther = pThis;` |
|       72 | 10866 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       35 | 10867 | `			}` |
|        - | 10868 | `			/* Execute the catch block */` |
|       72 | 10869 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10870 | `			/* Leave the frame */` |
|       72 | 10871 | `			VmLeaveFrame(&(*pVm));` |
|       35 | 10872 | `		}` |
|        - | 10873 | `		/* Restore the outer exception handlers */` |
|       72 | 10874 | `		if( apSaved ){` |
|        - | 10875 | `			sxu32 k;` |
|        - | 10876 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10877 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10878 | `			 * Restore the original outer entries.` |
|        - | 10879 | `			 */` |
|        9 | 10880 | `			SySetReset(&pVm->aException);` |
|       17 | 10881 | `			for(k = 0; k < nSavedCount; k++){` |
|        9 | 10882 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 10883 | `			}` |
|        9 | 10884 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        4 | 10885 | `		}` |
|        - | 10886 | `		/* Execute the finally block after catch */` |
|       72 | 10887 | `		if( pException->iHasFinally ){` |
|       16 | 10888 | `			pException->iFinallyDone = 1;` |
|        - | 10889 | `			{` |
|       16 | 10890 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 10891 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10892 | `					return SXERR_ABORT;` |
|        - | 10893 | `				}` |
|        - | 10894 | `			}` |
|        7 | 10895 | `		}` |
|       72 | 10896 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10897 | `			return SXERR_ABORT;` |
|        - | 10898 | `		}` |
|        - | 10899 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10900 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10901 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10902 | `		 */` |
|       72 | 10903 | `		if( pVm->pPendingException ){` |
|        7 | 10904 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        7 | 10905 | `			pVm->pPendingException = 0;` |
|        7 | 10906 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10907 | `		}` |
|        - | 10908 | `	}` |
|        - | 10909 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10910 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10911 | `	 */` |
|       66 | 10912 | `	return SXRET_OK;` |
|      281 | 10913 |  |
|        - | 10914 | `/*` |
|        - | 10915 | ` * Section:` |
|        - | 10916 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10917 | ` * Status:` |
|        - | 10918 | ` *    Stable.` |
|        - | 10919 | ` */` |
|        - | 10920 | `/*` |
|        - | 10921 | ` * string ph7version(void)` |
|        - | 10922 | ` *  Returns the running version of the PH7 version.` |
|        - | 10923 | ` * Parameters` |
|        - | 10924 | ` *  None` |
|        - | 10925 | ` * Return` |
|        - | 10926 | ` * Current PH7 version.` |
|        - | 10927 | ` */` |
|        2 | 10928 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10929 |  |
|        1 | 10930 | `	SXUNUSED(nArg);` |
|        1 | 10931 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10932 | `	/* Current engine version */` |
|        3 | 10933 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10934 | `	return PH7_OK;` |
|        1 | 10935 |  |
|        - | 10936 | `/*` |
|        - | 10937 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10938 | ` */` |
|        - | 10939 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10940 | ` "<html><head>"\` |
|        - | 10941 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10942 | ` "<style type=\"text/css\">"\` |
|        - | 10943 | ` "div {"\` |
|        - | 10944 | `     "border: 1px solid #cccccc;"\` |
|        - | 10945 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10946 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10947 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10948 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10949 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10950 | `     "-o-border-radius: 10px;"\` |
|        - | 10951 | `     "border-radius: 10px;"\` |
|        - | 10952 | `     "padding-left: 2em;"\` |
|        - | 10953 | `     "background-color: white;"\` |
|        - | 10954 | `     "margin-left: auto;"\` |
|        - | 10955 | `     "font-family: verdana;"\` |
|        - | 10956 | `     "padding-right: 2em;"\` |
|        - | 10957 | `     "margin-right: auto;"\` |
|        - | 10958 | `     "}"\` |
|        - | 10959 | `     "body {"\` |
|        - | 10960 | `     "padding: 0.2em;"\` |
|        - | 10961 | `     "font-style: normal;"\` |
|        - | 10962 | `     "font-size: medium;"\` |
|        - | 10963 | `     "background-color: #f2f2f2;"\` |
|        - | 10964 | `     "}"\` |
|        - | 10965 | `     "hr {"\` |
|        - | 10966 | `     "border-style: solid none none;"\` |
|        - | 10967 | `     "border-width: 1px medium medium;"\` |
|        - | 10968 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10969 | `     "height: 1px;"\` |
|        - | 10970 | `     "}"\` |
|        - | 10971 | `     "a {"\` |
|        - | 10972 | `     "color: #3366cc;"\` |
|        - | 10973 | `     "text-decoration: none;"\` |
|        - | 10974 | `     "}"\` |
|        - | 10975 | `     "a:hover {"\` |
|        - | 10976 | `     "color: #999999;"\` |
|        - | 10977 | `     "}"\` |
|        - | 10978 | `     "a:active {"\` |
|        - | 10979 | `     "color: #663399;"\` |
|        - | 10980 | `     "}"\` |
|        - | 10981 | `     "h1 {"\` |
|        - | 10982 | `     "margin: 0;"\` |
|        - | 10983 | `     "padding: 0;"\` |
|        - | 10984 | `     "font-family: Verdana;"\` |
|        - | 10985 | `     "font-weight: bold;"\` |
|        - | 10986 | `     "font-style: normal;"\` |
|        - | 10987 | `     "font-size: medium;"\` |
|        - | 10988 | `     "text-transform: capitalize;"\` |
|        - | 10989 | `     "color: #0a328c;"\` |
|        - | 10990 | `     "}"\` |
|        - | 10991 | `     "p {"\` |
|        - | 10992 | `     "margin: 0 auto;"\` |
|        - | 10993 | `     "font-size: medium;"\` |
|        - | 10994 | `     "font-style: normal;"\` |
|        - | 10995 | `     "font-family: verdana;"\` |
|        - | 10996 | `     "}"\` |
|        - | 10997 | `"</style></head><body>"\` |
|        - | 10998 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10999 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 11000 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 11001 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 11002 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 11003 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 11004 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 11005 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 11006 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 11007 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 11008 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 11009 |  |
|        - | 11010 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11011 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 11012 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 11013 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 11014 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11015 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 11016 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 11017 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 11018 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 11019 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 11020 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 11021 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 11022 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 11023 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 11024 |  |
|        - | 11025 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 11026 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 11027 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 11028 | `"&nbsp;*<br>"\` |
|        - | 11029 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 11030 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 11031 | `"&nbsp;* are met:<br>"\` |
|        - | 11032 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 11033 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 11034 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 11035 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 11036 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 11037 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 11038 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 11039 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 11040 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 11041 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 11042 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 11043 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 11044 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 11045 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 11046 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 11047 | `"&nbsp;*<br>"\` |
|        - | 11048 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 11049 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 11050 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 11051 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 11052 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 11053 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 11054 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 11055 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 11056 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 11057 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 11058 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 11059 | `"&nbsp;*/<br>"\` |
|        - | 11060 | `"</span></small></small></p>"\` |
|        - | 11061 | `"</div></body></html>"` |
|        - | 11062 | `/*` |
|        - | 11063 | ` * bool ph7credits(void)` |
|        - | 11064 | ` * bool ph7info(void)` |
|        - | 11065 | ` * bool ph7copyright(void)` |
|        - | 11066 | ` *  Prints out the credits for PH7 engine` |
|        - | 11067 | ` * Parameters` |
|        - | 11068 | ` *  None` |
|        - | 11069 | ` * Return` |
|        - | 11070 | ` *  Always TRUE` |
|        - | 11071 | ` */` |
|        2 | 11072 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11073 |  |
|        3 | 11074 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 11075 | `	/* Expand the HTML page above*/` |
|        3 | 11076 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 11077 | `	ph7_context_output_format(` |
|        1 | 11078 | `		pCtx,` |
|        - | 11079 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 11080 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 11081 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 11082 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 11083 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 11084 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 11085 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 11086 | `#ifdef __WINNT__` |
|        - | 11087 | `		"Windows NT"` |
|        - | 11088 | `#elif defined(__UNIXES__)` |
|        - | 11089 | `		"UNIX-Like"` |
|        - | 11090 | `#else` |
|        - | 11091 | `		"Other OS"` |
|        - | 11092 | `#endif` |
|        - | 11093 | `		);` |
|        3 | 11094 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 11095 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11096 | `	SXUNUSED(apArg);` |
|        - | 11097 | `	/* Return TRUE */` |
|        - | 11098 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 11099 | `	return PH7_OK;` |
|        1 | 11100 |  |
|        - | 11101 | `/*` |
|        - | 11102 | ` * Section:` |
|        - | 11103 | ` *    URL related routines.` |
|        - | 11104 | ` * Status:` |
|        - | 11105 | ` *    Stable.` |
|        - | 11106 | ` */` |
|        - | 11107 | `/*` |
|        - | 11108 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 11109 | ` *  Parse a URL and return its fields.` |
|        - | 11110 | ` * Parameters` |
|        - | 11111 | ` *  $url` |
|        - | 11112 | ` *   The URL to parse.` |
|        - | 11113 | ` * $component` |
|        - | 11114 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 11115 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 11116 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 11117 | ` *  in which case the return value will be an integer).` |
|        - | 11118 | ` * Return` |
|        - | 11119 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 11120 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 11121 | ` *  this array are:` |
|        - | 11122 | ` *   scheme - e.g. http` |
|        - | 11123 | ` *   host` |
|        - | 11124 | ` *   port` |
|        - | 11125 | ` *   user` |
|        - | 11126 | ` *   pass` |
|        - | 11127 | ` *   path` |
|        - | 11128 | ` *   query - after the question mark ?` |
|        - | 11129 | ` *   fragment - after the hashmark #` |
|        - | 11130 | ` * Note:` |
|        - | 11131 | ` *  FALSE is returned on failure.` |
|        - | 11132 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 11133 | ` *  with the standard PHP engine.` |
|        - | 11134 | ` */` |
|       28 | 11135 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11136 |  |
|        - | 11137 | `	const char *zStr; /* Input string */` |
|        - | 11138 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 11139 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 11140 | `	int nLen;` |
|        - | 11141 | `	sxi32 rc;` |
|       29 | 11142 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11143 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 11144 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11145 | `		return PH7_OK;` |
|        - | 11146 | `	}` |
|        - | 11147 | `	/* Extract the given URI */` |
|       29 | 11148 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 11149 | `	if( nLen < 1 ){` |
|        - | 11150 | `		/* Nothing to process,return FALSE */` |
|        3 | 11151 | `		ph7_result_bool(pCtx,0);` |
|        3 | 11152 | `		return PH7_OK;` |
|        - | 11153 | `	}` |
|        - | 11154 | `	/* Get a parse */` |
|       27 | 11155 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 11156 | `	if( rc != SXRET_OK ){` |
|        - | 11157 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 11158 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11159 | `		return PH7_OK;` |
|        - | 11160 | `	}` |
|       27 | 11161 | `	if( nArg > 1 ){` |
|      ! 0 | 11162 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 11163 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 11164 | `		switch(nComponent){` |
|      ! 0 | 11165 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 11166 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 11167 | `			if( pComp->nByte < 1 ){` |
|        - | 11168 | `				/* No available value,return NULL */` |
|      ! 0 | 11169 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11170 | `			}else{` |
|      ! 0 | 11171 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11172 | `			}` |
|      ! 0 | 11173 | `			break;` |
|      ! 0 | 11174 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 11175 | `			pComp = &sURI.sHost;` |
|      ! 0 | 11176 | `			if( pComp->nByte < 1 ){` |
|        - | 11177 | `				/* No available value,return NULL */` |
|      ! 0 | 11178 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11179 | `			}else{` |
|      ! 0 | 11180 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11181 | `			}` |
|      ! 0 | 11182 | `			break;` |
|      ! 0 | 11183 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 11184 | `			pComp = &sURI.sPort;` |
|      ! 0 | 11185 | `			if( pComp->nByte < 1 ){` |
|        - | 11186 | `				/* No available value,return NULL */` |
|      ! 0 | 11187 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11188 | `			}else{` |
|      ! 0 | 11189 | `				int iPort = 0;` |
|        - | 11190 | `				/* Cast the value to integer */` |
|      ! 0 | 11191 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 11192 | `				ph7_result_int(pCtx,iPort);` |
|        - | 11193 | `			}` |
|      ! 0 | 11194 | `			break;` |
|      ! 0 | 11195 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 11196 | `			pComp = &sURI.sUser;` |
|      ! 0 | 11197 | `			if( pComp->nByte < 1 ){` |
|        - | 11198 | `				/* No available value,return NULL */` |
|      ! 0 | 11199 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11200 | `			}else{` |
|      ! 0 | 11201 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11202 | `			}` |
|      ! 0 | 11203 | `			break;` |
|      ! 0 | 11204 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 11205 | `			pComp = &sURI.sPass;` |
|      ! 0 | 11206 | `			if( pComp->nByte < 1 ){` |
|        - | 11207 | `				/* No available value,return NULL */` |
|      ! 0 | 11208 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11209 | `			}else{` |
|      ! 0 | 11210 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11211 | `			}` |
|      ! 0 | 11212 | `			break;` |
|      ! 0 | 11213 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 11214 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 11215 | `			if( pComp->nByte < 1 ){` |
|        - | 11216 | `				/* No available value,return NULL */` |
|      ! 0 | 11217 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11218 | `			}else{` |
|      ! 0 | 11219 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11220 | `			}` |
|      ! 0 | 11221 | `			break;` |
|      ! 0 | 11222 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 11223 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 11224 | `			if( pComp->nByte < 1 ){` |
|        - | 11225 | `				/* No available value,return NULL */` |
|      ! 0 | 11226 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11227 | `			}else{` |
|      ! 0 | 11228 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11229 | `			}` |
|      ! 0 | 11230 | `			break;` |
|      ! 0 | 11231 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 11232 | `			pComp = &sURI.sPath;` |
|      ! 0 | 11233 | `			if( pComp->nByte < 1 ){` |
|        - | 11234 | `				/* No available value,return NULL */` |
|      ! 0 | 11235 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11236 | `			}else{` |
|      ! 0 | 11237 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11238 | `			}` |
|      ! 0 | 11239 | `			break;` |
|      ! 0 | 11240 | `		default:` |
|        - | 11241 | `			/* No such entry,return NULL */` |
|      ! 0 | 11242 | `			ph7_result_null(pCtx);` |
|      ! 0 | 11243 | `			break;` |
|        - | 11244 | `		}` |
|      ! 0 | 11245 | `	}else{` |
|        - | 11246 | `		ph7_value *pArray,*pValue;` |
|        - | 11247 | `		/* Return an associative array */` |
|       27 | 11248 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 11249 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 11250 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11251 | `			/* Out of memory */` |
|      ! 0 | 11252 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11253 | `			/* Return false */` |
|      ! 0 | 11254 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 11255 | `			return PH7_OK;` |
|        - | 11256 | `		}` |
|        - | 11257 | `		/* Fill the array */` |
|       27 | 11258 | `		pComp = &sURI.sScheme;` |
|       27 | 11259 | `		if( pComp->nByte > 0 ){` |
|       19 | 11260 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 11261 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 11262 | `		}` |
|        - | 11263 | `		/* Reset the string cursor */` |
|       27 | 11264 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11265 | `		pComp = &sURI.sHost;` |
|       27 | 11266 | `		if( pComp->nByte > 0 ){` |
|       25 | 11267 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 11268 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 11269 | `		}` |
|        - | 11270 | `		/* Reset the string cursor */` |
|       27 | 11271 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11272 | `		pComp = &sURI.sPort;` |
|       27 | 11273 | `		if( pComp->nByte > 0 ){` |
|       11 | 11274 | `			int iPort = 0;/* cc warning */` |
|        - | 11275 | `			/* Convert to integer */` |
|       11 | 11276 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 11277 | `			ph7_value_int(pValue,iPort);` |
|       11 | 11278 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 11279 | `		}` |
|        - | 11280 | `		/* Reset the string cursor */` |
|       27 | 11281 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11282 | `		pComp = &sURI.sUser;` |
|       27 | 11283 | `		if( pComp->nByte > 0 ){` |
|        7 | 11284 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11285 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 11286 | `		}` |
|        - | 11287 | `		/* Reset the string cursor */` |
|       27 | 11288 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11289 | `		pComp = &sURI.sPass;` |
|       27 | 11290 | `		if( pComp->nByte > 0 ){` |
|        7 | 11291 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11292 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 11293 | `		}` |
|        - | 11294 | `		/* Reset the string cursor */` |
|       27 | 11295 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11296 | `		pComp = &sURI.sPath;` |
|       27 | 11297 | `		if( pComp->nByte > 0 ){` |
|       17 | 11298 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 11299 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 11300 | `		}` |
|        - | 11301 | `		/* Reset the string cursor */` |
|       27 | 11302 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11303 | `		pComp = &sURI.sQuery;` |
|       27 | 11304 | `		if( pComp->nByte > 0 ){` |
|        5 | 11305 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11306 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11307 | `		}` |
|        - | 11308 | `		/* Reset the string cursor */` |
|       27 | 11309 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11310 | `		pComp = &sURI.sFragment;` |
|       27 | 11311 | `		if( pComp->nByte > 0 ){` |
|        5 | 11312 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11313 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11314 | `		}` |
|        - | 11315 | `		/* Return the created array */` |
|       27 | 11316 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11317 | `		/* NOTE:` |
|        - | 11318 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11319 | `		 * automatically as soon we return from this function.` |
|        - | 11320 | `		 */` |
|        - | 11321 | `	}` |
|        - | 11322 | `	/* All done */` |
|       27 | 11323 | `	return PH7_OK;` |
|       15 | 11324 |  |
|        - | 11325 | `/*` |
|        - | 11326 | ` * Section:` |
|        - | 11327 | ` *   Array related routines.` |
|        - | 11328 | ` * Status:` |
|        - | 11329 | ` *    Stable.` |
|        - | 11330 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11331 | ` *  Array related functions that need access to the underlying` |
|        - | 11332 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11333 | ` */` |
|        - | 11334 | `/*` |
|        - | 11335 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11336 | ` * of the following structure.` |
|        - | 11337 | ` */` |
|        - | 11338 | `struct compact_data` |
|        - | 11339 |  |
|        - | 11340 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11341 | `	int nRecCount;      /* Recursion count */` |
|        - | 11342 | `};` |
|        - | 11343 | `/*` |
|        - | 11344 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11345 | ` */` |
|      ! 0 | 11346 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11347 |  |
|      ! 0 | 11348 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11349 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11350 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11351 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11352 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11353 | `		SyString sVar;` |
|      ! 0 | 11354 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11355 | `		if( sVar.nByte > 0 ){` |
|        - | 11356 | `			/* Query the current frame */` |
|      ! 0 | 11357 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11358 | `			/* ^` |
|        - | 11359 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11360 | `			 */` |
|      ! 0 | 11361 | `			if( pKey ){` |
|        - | 11362 | `				/* Perform the insertion */` |
|      ! 0 | 11363 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11364 | `			}` |
|      ! 0 | 11365 | `		}` |
|      ! 0 | 11366 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11367 | `		int rc;` |
|        - | 11368 | `		/* Recursively traverse this array */` |
|      ! 0 | 11369 | `		pData->nRecCount++;` |
|      ! 0 | 11370 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11371 | `		pData->nRecCount--;` |
|      ! 0 | 11372 | `		return rc;` |
|        - | 11373 | `	}` |
|      ! 0 | 11374 | `	return SXRET_OK;` |
|      ! 0 | 11375 |  |
|        - | 11376 | `/*` |
|        - | 11377 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11378 | ` *  Create array containing variables and their values.` |
|        - | 11379 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11380 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11381 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11382 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11383 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11384 | ` * Parameters` |
|        - | 11385 | ` *  $varname` |
|        - | 11386 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11387 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11388 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11389 | ` *   it recursively.` |
|        - | 11390 | ` * Return` |
|        - | 11391 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11392 | ` */` |
|        2 | 11393 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11394 |  |
|        - | 11395 | `	ph7_value *pArray,*pObj;` |
|        3 | 11396 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11397 | `	const char *zName;` |
|        - | 11398 | `	SyString sVar;` |
|        - | 11399 | `	int i,nLen;` |
|        3 | 11400 | `	if( nArg < 1 ){` |
|        - | 11401 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11402 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11403 | `		return PH7_OK;` |
|        - | 11404 | `	}` |
|        - | 11405 | `	/* Create the array */` |
|        3 | 11406 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11407 | `	if( pArray == 0 ){` |
|        - | 11408 | `		/* Out of memory */` |
|      ! 0 | 11409 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11410 | `		/* Return NULL */` |
|      ! 0 | 11411 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11412 | `		return PH7_OK;` |
|        - | 11413 | `	}` |
|        - | 11414 | `	/* Perform the requested operation */` |
|        7 | 11415 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11416 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11417 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11418 | `				struct compact_data sData;` |
|      ! 0 | 11419 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11420 | `				/* Recursively walk the array */` |
|      ! 0 | 11421 | `				sData.nRecCount = 0;` |
|      ! 0 | 11422 | `				sData.pArray = pArray;` |
|      ! 0 | 11423 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11424 | `			}` |
|      ! 0 | 11425 | `		}else{` |
|        - | 11426 | `			/* Extract variable name */` |
|        5 | 11427 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11428 | `			if( nLen > 0 ){` |
|        5 | 11429 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11430 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11431 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11432 | `				if( pObj ){` |
|        5 | 11433 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11434 | `				}` |
|        2 | 11435 | `			}` |
|        - | 11436 | `		}` |
|        3 | 11437 | `	}` |
|        - | 11438 | `	/* Return the array */` |
|        3 | 11439 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11440 | `	return PH7_OK;` |
|        2 | 11441 |  |
|        - | 11442 | `/*` |
|        - | 11443 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11444 | ` * of the following structure.` |
|        - | 11445 | ` */` |
|        - | 11446 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11447 | `struct extract_aux_data` |
|        - | 11448 |  |
|        - | 11449 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11450 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11451 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11452 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11453 | `	int iFlags;           /* Control flags */` |
|        - | 11454 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11455 | `};` |
|        - | 11456 | `/* Forward declaration */` |
|        - | 11457 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11458 | `/*` |
|        - | 11459 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11460 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11461 | ` * Parameters` |
|        - | 11462 | ` * $var_array` |
|        - | 11463 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11464 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11465 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11466 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11467 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11468 | ` * $extract_type` |
|        - | 11469 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11470 | ` *  It can be one of the following values:` |
|        - | 11471 | ` *   EXTR_OVERWRITE` |
|        - | 11472 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11473 | ` *   EXTR_SKIP` |
|        - | 11474 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11475 | ` *   EXTR_PREFIX_SAME` |
|        - | 11476 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11477 | ` *   EXTR_PREFIX_ALL` |
|        - | 11478 | ` *       Prefix all variable names with prefix.` |
|        - | 11479 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11480 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11481 | ` *   EXTR_IF_EXISTS` |
|        - | 11482 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11483 | ` *       otherwise do nothing.` |
|        - | 11484 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11485 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11486 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11487 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11488 | ` *      the current symbol table.` |
|        - | 11489 | ` * $prefix` |
|        - | 11490 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11491 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11492 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11493 | ` *  underscore character.` |
|        - | 11494 | ` * Return` |
|        - | 11495 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11496 | ` */` |
|        4 | 11497 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11498 |  |
|        - | 11499 | `	extract_aux_data sAux;` |
|        - | 11500 | `	ph7_hashmap *pMap;` |
|        5 | 11501 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11502 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11503 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11504 | `		return PH7_OK;` |
|        - | 11505 | `	}` |
|        - | 11506 | `	/* Point to the target hashmap */` |
|        5 | 11507 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11508 | `	if( pMap->nEntry < 1 ){` |
|        - | 11509 | `		/* Empty map,return  0 */` |
|      ! 0 | 11510 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11511 | `		return PH7_OK;` |
|        - | 11512 | `	}` |
|        - | 11513 | `	/* Prepare the aux data */` |
|        5 | 11514 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11515 | `	if( nArg > 1 ){` |
|        3 | 11516 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11517 | `		if( nArg > 2 ){` |
|      ! 0 | 11518 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11519 | `		}` |
|        1 | 11520 | `	}` |
|        5 | 11521 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11522 | `	/* Invoke the worker callback */` |
|        5 | 11523 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11524 | `	/* Number of variables successfully imported */` |
|        5 | 11525 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11526 | `	return PH7_OK;` |
|        3 | 11527 |  |
|        - | 11528 | `/*` |
|        - | 11529 | ` * Worker callback for the [extract()] function defined` |
|        - | 11530 | ` * below.` |
|        - | 11531 | ` */` |
|        8 | 11532 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11533 |  |
|        9 | 11534 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11535 | `	int iFlags = pAux->iFlags;` |
|        9 | 11536 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11537 | `	ph7_value *pObj;` |
|        - | 11538 | `	SyString sVar;` |
|        9 | 11539 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11540 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11541 | `	}` |
|        - | 11542 | `	/* Perform a string cast */` |
|        9 | 11543 | `	PH7_MemObjToString(pKey);` |
|        9 | 11544 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11545 | `		/* Unavailable variable name */` |
|      ! 0 | 11546 | `		return SXRET_OK;` |
|        - | 11547 | `	}` |
|        9 | 11548 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11549 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11550 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11551 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11552 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11553 | `			);` |
|      ! 0 | 11554 | `	}else{` |
|       13 | 11555 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11556 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11557 | `	}` |
|        9 | 11558 | `	sVar.zString = pAux->zWorker;` |
|        - | 11559 | `	/* Try to extract the variable */` |
|        9 | 11560 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11561 | `	if( pObj ){` |
|        - | 11562 | `		/* Collision */` |
|        5 | 11563 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11564 | `			return SXRET_OK;` |
|        - | 11565 | `		}` |
|        5 | 11566 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11567 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11568 | `				/* Already prefixed */` |
|      ! 0 | 11569 | `				return SXRET_OK;` |
|        - | 11570 | `			}` |
|      ! 0 | 11571 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11572 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11573 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11574 | `				);` |
|      ! 0 | 11575 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11576 | `		}` |
|        3 | 11577 | `	}else{` |
|        - | 11578 | `		/* Create the variable */` |
|        5 | 11579 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11580 | `	}` |
|        9 | 11581 | `	if( pObj ){` |
|        - | 11582 | `		/* Overwrite the old value */` |
|        9 | 11583 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11584 | `		/* Increment counter */` |
|        9 | 11585 | `		pAux->iCount++;` |
|        4 | 11586 | `	}` |
|        9 | 11587 | `	return SXRET_OK;` |
|        5 | 11588 |  |
|        - | 11589 | `/*` |
|        - | 11590 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11591 | ` * defined below.` |
|        - | 11592 | ` */` |
|        2 | 11593 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11594 |  |
|        3 | 11595 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11596 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11597 | `	ph7_value *pObj;` |
|        - | 11598 | `	SyString sVar;` |
|        - | 11599 | `	/* Perform a string cast */` |
|        3 | 11600 | `	PH7_MemObjToString(pKey);` |
|        3 | 11601 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11602 | `		/* Unavailable variable name */` |
|      ! 0 | 11603 | `		return SXRET_OK;` |
|        - | 11604 | `	}` |
|        3 | 11605 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11606 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11607 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11608 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11609 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11610 | `			);` |
|        2 | 11611 | `	}else{` |
|      ! 0 | 11612 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11613 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11614 | `	}` |
|        3 | 11615 | `	sVar.zString = pAux->zWorker;` |
|        - | 11616 | `	/* Extract the variable */` |
|        3 | 11617 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11618 | `	if( pObj ){` |
|        3 | 11619 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11620 | `	}` |
|        3 | 11621 | `	return SXRET_OK;` |
|        2 | 11622 |  |
|        - | 11623 | `/*` |
|        - | 11624 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11625 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11626 | ` * Parameters` |
|        - | 11627 | ` * $types` |
|        - | 11628 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11629 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11630 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11631 | ` *  POST includes the POST uploaded file information.` |
|        - | 11632 | ` *  Note:` |
|        - | 11633 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11634 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11635 | ` * $prefix` |
|        - | 11636 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11637 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11638 | ` *  variable named $pref_userid.` |
|        - | 11639 | ` * Return` |
|        - | 11640 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11641 | ` */` |
|        2 | 11642 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11643 |  |
|        - | 11644 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11645 | `	extract_aux_data sAux;` |
|        - | 11646 | `	int nLen,nPrefixLen;` |
|        - | 11647 | `	ph7_value *pSuper;` |
|        - | 11648 | `	ph7_vm *pVm;` |
|        - | 11649 | `	/* By default import only $_GET variables  */` |
|        3 | 11650 | `	zImport = "G";` |
|        3 | 11651 | `	nLen = (int)sizeof(char);` |
|        3 | 11652 | `	zPrefix = 0;` |
|        3 | 11653 | `	nPrefixLen = 0;` |
|        3 | 11654 | `	if( nArg > 0 ){` |
|        3 | 11655 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11656 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11657 | `		}` |
|        3 | 11658 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11659 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11660 | `		}` |
|        1 | 11661 | `	}` |
|        - | 11662 | `	/* Point to the underlying VM */` |
|        3 | 11663 | `	pVm = pCtx->pVm;` |
|        - | 11664 | `	/* Initialize the aux data */` |
|        3 | 11665 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11666 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11667 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11668 | `	sAux.pVm = pVm;` |
|        - | 11669 | `	/* Extract */` |
|        3 | 11670 | `	zEnd = &zImport[nLen];` |
|        5 | 11671 | `	while( zImport < zEnd ){` |
|        3 | 11672 | `		int c = zImport[0];` |
|        3 | 11673 | `		pSuper = 0;` |
|        3 | 11674 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11675 | `			/* Import $_GET variables */` |
|        3 | 11676 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11677 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11678 | `			/* Import $_POST variables */` |
|      ! 0 | 11679 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11680 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11681 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11682 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11683 | `		}` |
|        3 | 11684 | `		if( pSuper ){` |
|        - | 11685 | `			/* Iterate throw array entries */` |
|        3 | 11686 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11687 | `		}` |
|        - | 11688 | `		/* Advance the cursor */` |
|        3 | 11689 | `		zImport++;` |
|        1 | 11690 | `	}` |
|        - | 11691 | `	/* All done,return TRUE*/` |
|        3 | 11692 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11693 | `	return PH7_OK;` |
|        1 | 11694 |  |
|        - | 11695 | `/*` |
|        - | 11696 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11697 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11698 | ` * information.` |
|        - | 11699 | ` */` |
|    10970 | 11700 | `static sxi32 VmEvalChunk(` |
|        - | 11701 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11702 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11703 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11704 | `	int iFlags,         /* Compile flag */` |
|        - | 11705 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11706 | `	)` |
|        2 | 11707 |  |
|        - | 11708 | `	SySet *pByteCode,aByteCode;` |
|        - | 11709 | `	SyBlob sSavedNs;` |
|    10972 | 11710 | `	ProcConsumer xErr = 0;` |
|    10972 | 11711 | `	void *pErrData = 0;` |
|        - | 11712 | `	/* Initialize bytecode container */` |
|    10972 | 11713 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10972 | 11714 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11715 | `	/* Reset the code generator */` |
|    10972 | 11716 | `	if( bTrueReturn ){` |
|        - | 11717 | `		/* Included file,log compile-time errors */` |
|     8284 | 11718 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8284 | 11719 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4141 | 11720 | `	}` |
|    10972 | 11721 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11722 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11723 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11724 | `	 * the caller's namespace is restored. */` |
|    10972 | 11725 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10972 | 11726 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10972 | 11727 | `	if( bTrueReturn ){` |
|        - | 11728 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8284 | 11729 | `		SyBlobReset(&pVm->sNamespace);` |
|     4141 | 11730 | `	}` |
|        - | 11731 | `	/* Swap bytecode container */` |
|    10972 | 11732 | `	pByteCode = pVm->pByteContainer;` |
|    10972 | 11733 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11734 | `	/* Compile the chunk */` |
|    10972 | 11735 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    16457 | 11736 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11737 | `		/* Compilation error,return false */` |
|        3 | 11738 | `		if( pCtx ){` |
|        3 | 11739 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11740 | `		}` |
|        2 | 11741 | `	}else{` |
|        - | 11742 | `		/* Mount any newly defined classes */` |
|        - | 11743 | `		SyHashEntry *pEntry;` |
|        - | 11744 | `		ph7_class *pClass;` |
|        - | 11745 | `		ph7_value sResult; /* Return value */` |
|        - | 11746 | `		sxi32 rc;` |
|    10970 | 11747 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   413018 | 11748 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   396566 | 11749 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11750 | `			/* Only mount classes that haven't been mounted yet */` |
|   396566 | 11751 | `			if( !pClass->bMounted ){` |
|    84782 | 11752 | `				rc = VmMountUserClass(pVm,pClass);` |
|    84782 | 11753 | `				if( rc != SXRET_OK ){` |
|        - | 11754 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11755 | `					if( pCtx ){` |
|      ! 0 | 11756 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11757 | `					}` |
|      ! 0 | 11758 | `					goto Cleanup;` |
|        - | 11759 | `				}` |
|    42390 | 11760 | `			}` |
|        2 | 11761 | `		}` |
|    10970 | 11762 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11763 | `			/* Out of memory */` |
|      ! 0 | 11764 | `			if( pCtx ){` |
|      ! 0 | 11765 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11766 | `			}` |
|      ! 0 | 11767 | `			goto Cleanup;` |
|        - | 11768 | `		}` |
|    10970 | 11769 | `		if( bTrueReturn ){` |
|        - | 11770 | `			/* Assume a boolean true return value */` |
|     8284 | 11771 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4143 | 11772 | `		}else{` |
|        - | 11773 | `			/* Assume a null return value */` |
|     2688 | 11774 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11775 | `		}` |
|        - | 11776 | `		/* Execute the compiled chunk */` |
|    10970 | 11777 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10970 | 11778 | `		if( pCtx ){` |
|        - | 11779 | `			/* Set the execution result */` |
|     8302 | 11780 | `			ph7_result_value(pCtx,&sResult);` |
|     4150 | 11781 | `		}` |
|    10970 | 11782 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11783 | `	}` |
|     5485 | 11784 | `Cleanup:` |
|        - | 11785 | `	/* Cleanup the mess left behind */` |
|    10972 | 11786 | `	pVm->pByteContainer = pByteCode;` |
|    10972 | 11787 | `	SySetRelease(&aByteCode);` |
|        - | 11788 | `	/* Restore caller's namespace state */` |
|    10972 | 11789 | `	SyBlobReset(&pVm->sNamespace);` |
|    10972 | 11790 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10972 | 11791 | `	SyBlobRelease(&sSavedNs);` |
|    10972 | 11792 | `	return SXRET_OK;` |
|        2 | 11793 |  |
|        - | 11794 | `/*` |
|        - | 11795 | ` * value eval(string $code)` |
|        - | 11796 | ` *   Evaluate a string as PHP code.` |
|        - | 11797 | ` * Parameter` |
|        - | 11798 | ` *  code: PHP code to evaluate.` |
|        - | 11799 | ` * Return` |
|        - | 11800 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11801 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11802 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11803 | ` */` |
|       22 | 11804 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11805 |  |
|        - | 11806 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 11807 | `	if( nArg < 1 ){` |
|        - | 11808 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11809 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11810 | `		return SXRET_OK;` |
|        - | 11811 | `	}` |
|        - | 11812 | `	/* Chunk to evaluate */` |
|       24 | 11813 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 11814 | `	if( sChunk.nByte < 1 ){` |
|        - | 11815 | `		/* Empty string,return NULL */` |
|        3 | 11816 | `		ph7_result_null(pCtx);` |
|        3 | 11817 | `		return SXRET_OK;` |
|        - | 11818 | `	}` |
|        - | 11819 | `	/* Eval the chunk */` |
|       22 | 11820 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 11821 | `	return SXRET_OK;` |
|       13 | 11822 |  |
|        - | 11823 | `/*` |
|        - | 11824 | ` * Check if a file path is already included.` |
|        - | 11825 | ` */` |
|    16560 | 11826 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 11827 |  |
|        - | 11828 | `	SyString *aEntries;` |
|        - | 11829 | `	sxu32 n;` |
|    16562 | 11830 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11831 | `	/* Perform a linear search */` |
| 68512332 | 11832 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 68495778 | 11833 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11834 | `			/* Already included */` |
|        7 | 11835 | `			return TRUE;` |
|        - | 11836 | `		}` |
| 34247887 | 11837 | `	}` |
|    16556 | 11838 | `	return FALSE;` |
|     8282 | 11839 |  |
|        - | 11840 | `/*` |
|        - | 11841 | ` * Push a file path in the appropriate VM container.` |
|        - | 11842 | ` */` |
|    19220 | 11843 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11844 |  |
|        - | 11845 | `	SyString sPath;` |
|        - | 11846 | `	char *zDup;` |
|        - | 11847 | `#ifdef __WINNT__` |
|        - | 11848 | `	char *zCur;` |
|        - | 11849 | `#endif` |
|        - | 11850 | `	sxi32 rc;` |
|    19222 | 11851 | `	if( nLen < 0 ){` |
|     2662 | 11852 | `		nLen = SyStrlen(zPath);` |
|     1330 | 11853 | `	}` |
|        - | 11854 | `	/* Duplicate the file path first */` |
|    19222 | 11855 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    19222 | 11856 | `	if( zDup == 0 ){` |
|      ! 0 | 11857 | `		return SXERR_MEM;` |
|        - | 11858 | `	}` |
|        - | 11859 | `#ifdef __WINNT__` |
|        - | 11860 | `	/* Normalize path on windows` |
|        - | 11861 | `	 * Example:` |
|        - | 11862 | `	 *    Path/To/File.php` |
|        - | 11863 | `	 * becomes` |
|        - | 11864 | `	 *   path\to\file.php` |
|        - | 11865 | `	 */` |
|        2 | 11866 | `	zCur = zDup;` |
|        2 | 11867 | `	while( zCur[0] != 0 ){` |
|        2 | 11868 | `		if( zCur[0] == '/' ){` |
|        2 | 11869 | `			zCur[0] = '\\';` |
|        2 | 11870 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11871 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11872 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11873 | `		}` |
|        2 | 11874 | `		zCur++;` |
|        2 | 11875 | `	}` |
|        - | 11876 | `#endif` |
|        - | 11877 | `	/* Install the file path */` |
|    19222 | 11878 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    19222 | 11879 | `	if( !bMain ){` |
|    16562 | 11880 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11881 | `			/* Already included */` |
|        7 | 11882 | `			*pNew = 0;` |
|        4 | 11883 | `		}else{` |
|        - | 11884 | `			/* Insert in the corresponding container */` |
|    16556 | 11885 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16556 | 11886 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11887 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11888 | `				return rc;` |
|        - | 11889 | `			}` |
|    16556 | 11890 | `			*pNew = 1;` |
|        - | 11891 | `		}` |
|     8280 | 11892 | `	}` |
|    19222 | 11893 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    19222 | 11894 | `	return SXRET_OK;` |
|     9612 | 11895 |  |
|        - | 11896 | `/*` |
|        - | 11897 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11898 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11899 | ` * indicates failure.` |
|        - | 11900 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11901 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11902 | ` * operations.` |
|        - | 11903 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11904 | ` * this function is a no-op.` |
|        - | 11905 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11906 | ` * constructs for more information.` |
|        - | 11907 | ` */` |
|     8292 | 11908 | `static sxi32 VmExecIncludedFile(` |
|        - | 11909 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11910 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11911 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11912 | `	 )` |
|        2 | 11913 |  |
|        - | 11914 | `	sxi32 rc;` |
|        - | 11915 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11916 | `	const ph7_io_stream *pStream;` |
|        - | 11917 | `	SyBlob sContents;` |
|        - | 11918 | `	void *pHandle;` |
|        - | 11919 | `	ph7_vm *pVm;` |
|        - | 11920 | `	int isNew;` |
|        - | 11921 | `	/* Initialize fields */` |
|     8294 | 11922 | `	pVm = pCtx->pVm;` |
|     8294 | 11923 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8294 | 11924 | `	isNew = 0;` |
|        - | 11925 | `	/* Extract the associated stream */` |
|     8294 | 11926 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11927 | `	/*` |
|        - | 11928 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11929 | `	 * in a read-only mode.` |
|        - | 11930 | `	 */` |
|     8294 | 11931 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8294 | 11932 | `	if( pHandle == 0 ){` |
|        8 | 11933 | `		return SXERR_IO;` |
|        - | 11934 | `	}` |
|     8288 | 11935 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8288 | 11936 | `	if( IncludeOnce && !isNew ){` |
|        - | 11937 | `		/* Already included */` |
|        5 | 11938 | `		rc = SXERR_EXISTS;` |
|        3 | 11939 | `	}else{` |
|        - | 11940 | `		/* Read the whole file contents */` |
|     8284 | 11941 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8284 | 11942 | `		if( rc == SXRET_OK ){` |
|        - | 11943 | `			SyString sScript;` |
|        - | 11944 | `			/* Compile and execute the script */` |
|     8284 | 11945 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8284 | 11946 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4141 | 11947 | `		}` |
|        - | 11948 | `	}` |
|        - | 11949 | `	/* Pop from the set of included file */` |
|     8288 | 11950 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11951 | `	/* Close the handle */` |
|     8288 | 11952 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11953 | `	/* Release the working buffer */` |
|     8288 | 11954 | `	SyBlobRelease(&sContents);` |
|        - | 11955 | `#else` |
|        - | 11956 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11957 | `	SXUNUSED(pPath);` |
|        - | 11958 | `	SXUNUSED(IncludeOnce);` |
|        - | 11959 | `	rc = SXERR_IO;` |
|        - | 11960 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8288 | 11961 | `	return rc;` |
|     4148 | 11962 |  |
|        - | 11963 | `/*` |
|        - | 11964 | ` * string get_include_path(void)` |
|        - | 11965 | ` *  Gets the current include_path configuration option.` |
|        - | 11966 | ` * Parameter` |
|        - | 11967 | ` *  None` |
|        - | 11968 | ` * Return` |
|        - | 11969 | ` *  Included paths as a string` |
|        - | 11970 | ` */` |
|        2 | 11971 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11972 |  |
|        3 | 11973 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11974 | `	SyString *aEntry;` |
|        - | 11975 | `	int dir_sep;` |
|        - | 11976 | `	sxu32 n;` |
|        - | 11977 | `#ifdef __WINNT__` |
|        1 | 11978 | `	dir_sep = ';';` |
|        - | 11979 | `#else` |
|        - | 11980 | `	/* Assume UNIX path separator */` |
|        2 | 11981 | `	dir_sep = ':';` |
|        - | 11982 | `#endif` |
|        1 | 11983 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11984 | `	SXUNUSED(apArg);` |
|        - | 11985 | `	/* Point to the list of import paths */` |
|        3 | 11986 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11987 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11988 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11989 | `		if( n > 0 ){` |
|        - | 11990 | `			/* Append dir seprator */` |
|      ! 0 | 11991 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11992 | `		}` |
|        - | 11993 | `		/* Append path */` |
|        3 | 11994 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11995 | `	}` |
|        3 | 11996 | `	return PH7_OK;` |
|        1 | 11997 |  |
|        - | 11998 | `/*` |
|        - | 11999 | ` * string get_get_included_files(void)` |
|        - | 12000 | ` *  Gets the current include_path configuration option.` |
|        - | 12001 | ` * Parameter` |
|        - | 12002 | ` *  None` |
|        - | 12003 | ` * Return` |
|        - | 12004 | ` *  Included paths as a string` |
|        - | 12005 | ` */` |
|        2 | 12006 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12007 |  |
|        3 | 12008 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 12009 | `	ph7_value *pArray,*pWorker;` |
|        - | 12010 | `	SyString *pEntry;` |
|        - | 12011 | `	int c,d;` |
|        - | 12012 | `	/* Create an array and a working value */` |
|        3 | 12013 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 12014 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 12015 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 12016 | `		/* Out of memory,return null */` |
|      ! 0 | 12017 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12018 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12019 | `		SXUNUSED(apArg);` |
|      ! 0 | 12020 | `		return PH7_OK;` |
|        - | 12021 | `	}` |
|        3 | 12022 | `	c = d = '/';` |
|        - | 12023 | `#ifdef __WINNT__` |
|        1 | 12024 | `	d = '\\';` |
|        - | 12025 | `#endif` |
|        - | 12026 | `	/* Iterate throw entries */` |
|        3 | 12027 | `	SySetResetCursor(pFiles);` |
|     3839 | 12028 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 12029 | `		const char *zBase,*zEnd;` |
|        - | 12030 | `		int iLen;` |
|        - | 12031 | `		/* reset the string cursor */` |
|     3837 | 12032 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 12033 | `		/* Extract base name */` |
|     3837 | 12034 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 12035 | `		/* Ignore trailing '/' */` |
|     5755 | 12036 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 12037 | `			zEnd--;` |
|      ! 0 | 12038 | `		}` |
|     3837 | 12039 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 12040 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 12041 | `			zEnd--;` |
|        1 | 12042 | `		}` |
|     3837 | 12043 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 12044 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 12045 | `		/* Copy entry name */` |
|     3837 | 12046 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 12047 | `		/* Perform the insertion */` |
|     3837 | 12048 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 12049 | `	}` |
|        - | 12050 | `	/* All done,return the created array */` |
|        3 | 12051 | `	ph7_result_value(pCtx,pArray);` |
|        - | 12052 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 12053 | `	 * by the engine as soon we return from this foreign` |
|        - | 12054 | `	 * function.` |
|        - | 12055 | `	 */` |
|        3 | 12056 | `	return PH7_OK;` |
|        2 | 12057 |  |
|        - | 12058 | `/*` |
|        - | 12059 | ` * include:` |
|        - | 12060 | ` * According to the PHP reference manual.` |
|        - | 12061 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 12062 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 12063 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 12064 | ` *  include() will finally check in the calling script's own directory` |
|        - | 12065 | ` *  and the current working directory before failing. The include()` |
|        - | 12066 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 12067 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 12068 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 12069 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 12070 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 12071 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 12072 | ` *  directory to find the requested file.` |
|        - | 12073 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 12074 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 12075 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 12076 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 12077 | ` */` |
|     8274 | 12078 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12079 |  |
|        - | 12080 | `	SyString sFile;` |
|        - | 12081 | `	sxi32 rc;` |
|     8276 | 12082 | `	if( nArg < 1 ){` |
|        - | 12083 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12084 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12085 | `		return SXRET_OK;` |
|        - | 12086 | `	}` |
|        - | 12087 | `	/* File to include */` |
|     8276 | 12088 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8276 | 12089 | `	if( sFile.nByte < 1 ){` |
|        - | 12090 | `		/* Empty string,return NULL */` |
|      ! 0 | 12091 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12092 | `		return SXRET_OK;` |
|        - | 12093 | `	}` |
|        - | 12094 | `	/* Open,compile and execute the desired script */` |
|     8276 | 12095 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8276 | 12096 | `	if( rc != SXRET_OK ){` |
|        - | 12097 | `		/* Emit a warning and return false */` |
|        3 | 12098 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 12099 | `		ph7_result_bool(pCtx,0);` |
|        1 | 12100 | `	}` |
|     8276 | 12101 | `	return SXRET_OK;` |
|     4139 | 12102 |  |
|        - | 12103 | `/*` |
|        - | 12104 | ` * include_once:` |
|        - | 12105 | ` *  According to the PHP reference manual.` |
|        - | 12106 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 12107 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 12108 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 12109 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 12110 | ` *   just once.` |
|        - | 12111 | ` */` |
|        4 | 12112 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12113 |  |
|        - | 12114 | `	SyString sFile;` |
|        - | 12115 | `	sxi32 rc;` |
|        5 | 12116 | `	if( nArg < 1 ){` |
|        - | 12117 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12118 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12119 | `		return SXRET_OK;` |
|        - | 12120 | `	}` |
|        - | 12121 | `	/* File to include */` |
|        5 | 12122 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12123 | `	if( sFile.nByte < 1 ){` |
|        - | 12124 | `		/* Empty string,return NULL */` |
|      ! 0 | 12125 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12126 | `		return SXRET_OK;` |
|        - | 12127 | `	}` |
|        - | 12128 | `	/* Open,compile and execute the desired script */` |
|        5 | 12129 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12130 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12131 | `		/* File already included,return TRUE */` |
|        3 | 12132 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12133 | `		return SXRET_OK;` |
|        - | 12134 | `	}` |
|        3 | 12135 | `	if( rc != SXRET_OK ){` |
|        - | 12136 | `		/* Emit a warning and return false */` |
|      ! 0 | 12137 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12138 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12139 | ` 	}` |
|        3 | 12140 | `	return SXRET_OK;` |
|        3 | 12141 |  |
|        - | 12142 | `/*` |
|        - | 12143 | ` * require.` |
|        - | 12144 | ` *  According to the PHP reference manual.` |
|        - | 12145 | ` *   require() is identical to include() except upon failure it will` |
|        - | 12146 | ` *   also produce a fatal level error.` |
|        - | 12147 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 12148 | ` *   emits a warning  which allows the script to continue.` |
|        - | 12149 | ` */` |
|        6 | 12150 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12151 |  |
|        - | 12152 | `	SyString sFile;` |
|        - | 12153 | `	sxi32 rc;` |
|        8 | 12154 | `	if( nArg < 1 ){` |
|        - | 12155 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12156 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12157 | `		return SXRET_OK;` |
|        - | 12158 | `	}` |
|        - | 12159 | `	/* File to include */` |
|        8 | 12160 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 12161 | `	if( sFile.nByte < 1 ){` |
|        - | 12162 | `		/* Empty string,return NULL */` |
|      ! 0 | 12163 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12164 | `		return SXRET_OK;` |
|        - | 12165 | `	}` |
|        - | 12166 | `	/* Open,compile and execute the desired script */` |
|        8 | 12167 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 12168 | `	if( rc != SXRET_OK ){` |
|        - | 12169 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12170 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12171 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12172 | `		return PH7_ABORT;` |
|        - | 12173 | `	}` |
|        8 | 12174 | `	return SXRET_OK;` |
|        5 | 12175 |  |
|        - | 12176 | `/*` |
|        - | 12177 | ` * require_once:` |
|        - | 12178 | ` *  According to the PHP reference manual.` |
|        - | 12179 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 12180 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 12181 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 12182 | ` *   and how it differs from its non _once siblings.` |
|        - | 12183 | ` */` |
|        4 | 12184 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12185 |  |
|        - | 12186 | `	SyString sFile;` |
|        - | 12187 | `	sxi32 rc;` |
|        5 | 12188 | `	if( nArg < 1 ){` |
|        - | 12189 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12190 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12191 | `		return SXRET_OK;` |
|        - | 12192 | `	}` |
|        - | 12193 | `	/* File to include */` |
|        5 | 12194 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12195 | `	if( sFile.nByte < 1 ){` |
|        - | 12196 | `		/* Empty string,return NULL */` |
|      ! 0 | 12197 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12198 | `		return SXRET_OK;` |
|        - | 12199 | `	}` |
|        - | 12200 | `	/* Open,compile and execute the desired script */` |
|        5 | 12201 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12202 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12203 | `		/* File already included,return TRUE */` |
|        3 | 12204 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12205 | `		return SXRET_OK;` |
|        - | 12206 | `	}` |
|        3 | 12207 | `	if( rc != SXRET_OK ){` |
|        - | 12208 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12209 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12210 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12211 | `		return PH7_ABORT;` |
|        - | 12212 | `	}` |
|        3 | 12213 | `	return SXRET_OK;` |
|        3 | 12214 |  |
|        - | 12215 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 12216 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 12217 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 12218 | `/*` |
|        - | 12219 | ` * Section:` |
|        - | 12220 | ` *  SPL Autoloading functions.` |
|        - | 12221 | ` * Status:` |
|        - | 12222 | ` *  Stable.` |
|        - | 12223 | ` */` |
|        - | 12224 | `/*` |
|        - | 12225 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 12226 | ` *  Register given function as __autoload() implementation.` |
|        - | 12227 | ` * Parameters` |
|        - | 12228 | ` *  callback` |
|        - | 12229 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 12230 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 12231 | ` *  throw` |
|        - | 12232 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 12233 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 12234 | ` *  prepend` |
|        - | 12235 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 12236 | ` *   autoload stack instead of appending it.` |
|        - | 12237 | ` * Return` |
|        - | 12238 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12239 | ` */` |
|       34 | 12240 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12241 |  |
|        - | 12242 | `	VmAutoloadCB sEntry;` |
|       36 | 12243 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 12244 | `	int iPrepend = 0;` |
|        - | 12245 | `	sxu32 n;` |
|       36 | 12246 | `	if( nArg < 1 ){` |
|        - | 12247 | `		/* No callback provided — register default spl_autoload.` |
|        - | 12248 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 12249 | `		/* Check for duplicates first */` |
|        9 | 12250 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 12251 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 12252 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 12253 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 12254 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 12255 | `				ph7_result_bool(pCtx,1);` |
|        5 | 12256 | `				return SXRET_OK;` |
|        - | 12257 | `			}` |
|      ! 0 | 12258 | `		}` |
|        5 | 12259 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 12260 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 12261 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 12262 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 12263 | `		ph7_result_bool(pCtx,1);` |
|        5 | 12264 | `		return SXRET_OK;` |
|        - | 12265 | `	}` |
|        - | 12266 | `	/* Validate that the callback is callable */` |
|       28 | 12267 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 12268 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 12269 | `		if( nArg >= 2 ){` |
|      ! 0 | 12270 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12271 | `		}` |
|      ! 0 | 12272 | `		if( iThrow ){` |
|      ! 0 | 12273 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 12274 | `				"Argument is not callable");` |
|      ! 0 | 12275 | `		}` |
|      ! 0 | 12276 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12277 | `		return SXRET_OK;` |
|        - | 12278 | `	}` |
|        - | 12279 | `	/* Check for duplicates */` |
|       46 | 12280 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 12281 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 12282 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12283 | `			/* Already registered */` |
|      ! 0 | 12284 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12285 | `			return SXRET_OK;` |
|        - | 12286 | `		}` |
|       11 | 12287 | `	}` |
|        - | 12288 | `	/* Check prepend flag */` |
|       28 | 12289 | `	if( nArg >= 3 ){` |
|        3 | 12290 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 12291 | `	}` |
|        - | 12292 | `	/* Store the callback */` |
|       28 | 12293 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 12294 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 12295 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 12296 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 12297 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 12298 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 12299 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 12300 | `		VmAutoloadCB *aBase;` |
|        3 | 12301 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12302 | `		/* Rotate: move last entry to front */` |
|        3 | 12303 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12304 | `		if( aBase ){` |
|        - | 12305 | `			VmAutoloadCB sTemp;` |
|        - | 12306 | `			sxu32 i;` |
|        3 | 12307 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12308 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12309 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12310 | `			}` |
|        3 | 12311 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12312 | `		}` |
|        2 | 12313 | `	}else{` |
|       26 | 12314 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12315 | `	}` |
|       28 | 12316 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12317 | `	return SXRET_OK;` |
|       19 | 12318 |  |
|        - | 12319 | `/*` |
|        - | 12320 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12321 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12322 | ` * Parameters` |
|        - | 12323 | ` *  callback` |
|        - | 12324 | ` *   The autoload function being unregistered.` |
|        - | 12325 | ` * Return` |
|        - | 12326 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12327 | ` */` |
|       32 | 12328 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12329 |  |
|       34 | 12330 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12331 | `	sxu32 n,nEntry;` |
|       34 | 12332 | `	if( nArg < 1 ){` |
|      ! 0 | 12333 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12334 | `		return SXRET_OK;` |
|        - | 12335 | `	}` |
|       34 | 12336 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12337 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12338 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12339 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12340 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12341 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12342 | `			sxu32 i;` |
|       32 | 12343 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12344 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12345 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12346 | `			}` |
|        - | 12347 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12348 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12349 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12350 | `			return SXRET_OK;` |
|        - | 12351 | `		}` |
|        3 | 12352 | `	}` |
|        3 | 12353 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12354 | `	return SXRET_OK;` |
|       18 | 12355 |  |
|        - | 12356 | `/*` |
|        - | 12357 | ` * array spl_autoload_functions(void)` |
|        - | 12358 | ` *  Return all registered __autoload() functions.` |
|        - | 12359 | ` * Return` |
|        - | 12360 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12361 | ` *  an empty array is returned.` |
|        - | 12362 | ` */` |
|       20 | 12363 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12364 |  |
|       21 | 12365 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12366 | `	ph7_value *pArray;` |
|        - | 12367 | `	sxu32 n,nEntry;` |
|       10 | 12368 | `	SXUNUSED(nArg);` |
|       10 | 12369 | `	SXUNUSED(apArg);` |
|       21 | 12370 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12371 | `	if( pArray == 0 ){` |
|      ! 0 | 12372 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12373 | `		return SXRET_OK;` |
|        - | 12374 | `	}` |
|       21 | 12375 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12376 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12377 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12378 | `		if( pEntry ){` |
|       15 | 12379 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12380 | `		}` |
|        8 | 12381 | `	}` |
|       21 | 12382 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12383 | `	return SXRET_OK;` |
|       11 | 12384 |  |
|        - | 12385 | `/*` |
|        - | 12386 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12387 | ` *  Default implementation of __autoload().` |
|        - | 12388 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12389 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12390 | ` * Parameters` |
|        - | 12391 | ` *  class` |
|        - | 12392 | ` *   The class name being searched.` |
|        - | 12393 | ` *  file_extensions` |
|        - | 12394 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12395 | ` */` |
|        2 | 12396 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12397 |  |
|        - | 12398 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12399 | `	SyBlob sPath;` |
|        - | 12400 | `	int nClass;` |
|        - | 12401 | `	sxi32 rc;` |
|        3 | 12402 | `	if( nArg < 1 ){` |
|      ! 0 | 12403 | `		return SXRET_OK;` |
|        - | 12404 | `	}` |
|        3 | 12405 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12406 | `	if( nClass < 1 ){` |
|      ! 0 | 12407 | `		return SXRET_OK;` |
|        - | 12408 | `	}` |
|        - | 12409 | `	/* Default extensions */` |
|        3 | 12410 | `	zExt = ".php,.inc";` |
|        3 | 12411 | `	if( nArg >= 2 ){` |
|        - | 12412 | `		int nExt;` |
|      ! 0 | 12413 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12414 | `		if( nExt < 1 ){` |
|      ! 0 | 12415 | `			zExt = ".php,.inc";` |
|      ! 0 | 12416 | `		}` |
|      ! 0 | 12417 | `	}` |
|        3 | 12418 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12419 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12420 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12421 | `	zCur = zExt;` |
|        7 | 12422 | `	while( zCur < zEnd ){` |
|        - | 12423 | `		const char *zComma;` |
|        - | 12424 | `		SyString sFile;` |
|        - | 12425 | `		int i;` |
|        - | 12426 | `		/* Find next comma or end */` |
|        5 | 12427 | `		zComma = zCur;` |
|       21 | 12428 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12429 | `			zComma++;` |
|        1 | 12430 | `		}` |
|        - | 12431 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12432 | `		SyBlobReset(&sPath);` |
|       69 | 12433 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12434 | `			char c = zClass[i];` |
|       65 | 12435 | `			if( c == '\\' ){` |
|      ! 0 | 12436 | `				c = '/';` |
|       65 | 12437 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12438 | `				c = c + ('a' - 'A');` |
|        6 | 12439 | `			}` |
|       65 | 12440 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12441 | `		}` |
|        - | 12442 | `		/* Append extension */` |
|        5 | 12443 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12444 | `		/* Try to include the file */` |
|        5 | 12445 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12446 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12447 | `		if( rc == SXRET_OK ){` |
|        - | 12448 | `			/* File included successfully */` |
|      ! 0 | 12449 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12450 | `			return SXRET_OK;` |
|        - | 12451 | `		}` |
|        - | 12452 | `		/* Move past the comma */` |
|        5 | 12453 | `		zCur = zComma;` |
|        5 | 12454 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12455 | `			zCur++;` |
|        1 | 12456 | `		}` |
|        1 | 12457 | `	}` |
|        3 | 12458 | `	SyBlobRelease(&sPath);` |
|        3 | 12459 | `	return SXRET_OK;` |
|        2 | 12460 |  |
|        - | 12461 | `/* Table of built-in VM functions. */` |
|        - | 12462 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12463 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12464 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12465 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12466 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12467 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12468 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12469 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12470 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12471 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12472 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12473 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12474 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12475 | `	    /* Constants management */` |
|        - | 12476 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12477 | `	{ "define",   vm_builtin_define               },` |
|        - | 12478 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12479 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12480 | `	   /* Class/Object functions */` |
|        - | 12481 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12482 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12483 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12484 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12485 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12486 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12487 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12488 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12489 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12490 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12491 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12492 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12493 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12494 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12495 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12496 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12497 | `	   /* SPL Autoloading */` |
|        - | 12498 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12499 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12500 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12501 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12502 | `	   /* Random numbers/strings generators */` |
|        - | 12503 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12504 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12505 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12506 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12507 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12508 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12509 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12510 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12511 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12512 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12513 | `	   /* Language constructs functions */` |
|        - | 12514 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12515 | `	{ "print", vm_builtin_print                   },` |
|        - | 12516 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12517 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12518 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12519 | `	  /* Variable handling functions */` |
|        - | 12520 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12521 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12522 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12523 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12524 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12525 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12526 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12527 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12528 | `	  /* Ouput control functions */` |
|        - | 12529 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12530 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12531 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12532 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12533 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12534 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12535 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12536 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12537 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12538 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12539 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12540 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12541 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12542 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12543 | `	  /* Assertion functions */` |
|        - | 12544 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12545 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12546 | `	  /* Error reporting functions */` |
|        - | 12547 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12548 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12549 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12550 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12551 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12552 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12553 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12554 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12555 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12556 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12557 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12558 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12559 | `	  /* Release info */` |
|        - | 12560 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12561 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12562 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12563 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12564 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12565 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12566 | `	  /* hashmap */` |
|        - | 12567 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12568 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12569 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12570 | `	  /* URL related function */` |
|        - | 12571 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12572 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12573 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12574 | `	   /* XML processing functions */` |
|        - | 12575 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12576 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12577 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12578 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12579 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12580 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12581 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 12582 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 12583 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 12584 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 12585 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 12586 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 12587 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 12588 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 12589 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 12590 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12591 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12592 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12593 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12594 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12595 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12596 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12597 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12598 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12599 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12600 | `	   /* Command line processing */` |
|        - | 12601 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12602 | `	   /* JSON encoding/decoding */` |
|        - | 12603 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12604 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12605 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12606 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12607 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12608 | `	   /* Files/URI inclusion facility */` |
|        - | 12609 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12610 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12611 | `	{ "include",      vm_builtin_include          },` |
|        - | 12612 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12613 | `	{ "require",      vm_builtin_require          },` |
|        - | 12614 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12615 | `};` |
|        - | 12616 | `/*` |
|        - | 12617 | ` * Register the built-in VM functions defined above.` |
|        - | 12618 | ` */` |
|     2402 | 12619 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12620 |  |
|        - | 12621 | `	sxi32 rc;` |
|        - | 12622 | `	sxu32 n;` |
|   309860 | 12623 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12624 | `		/* Note that these special functions have access` |
|        - | 12625 | `		 * to the underlying virtual machine as their` |
|        - | 12626 | `		 * private data.` |
|        - | 12627 | `		 */` |
|   307458 | 12628 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   307458 | 12629 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12630 | `			return rc;` |
|        - | 12631 | `		}` |
|   153730 | 12632 | `	}` |
|     2404 | 12633 | `	return SXRET_OK;` |
|     1203 | 12634 |  |
|        - | 12635 | `/*` |
|        - | 12636 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 12637 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 12638 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 12639 | ` */` |
|    33818 | 12640 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 12641 |  |
|    33820 | 12642 | `	if( !iLoadable ){` |
|    32456 | 12643 | `		return pClass;` |
|        - | 12644 | `	}` |
|     1366 | 12645 | `	while(pClass){` |
|     1366 | 12646 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1366 | 12647 | `			return pClass;` |
|        - | 12648 | `		}` |
|      ! 0 | 12649 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12650 | `	}` |
|      ! 0 | 12651 | `	return 0;` |
|    16911 | 12652 |  |
|        - | 12653 | `/*` |
|        - | 12654 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 12655 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 12656 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 12657 | ` * registered in the VM's class table.` |
|        - | 12658 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 12659 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 12660 | ` */` |
|       36 | 12661 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12662 |  |
|        - | 12663 | `	VmAutoloadCB *pEntry;` |
|        - | 12664 | `	ph7_value sArg,sResult;` |
|        - | 12665 | `	SyHashEntry *pHashEntry;` |
|        - | 12666 | `	ph7_class *pClass;` |
|        - | 12667 | `	sxu32 n,nEntry;` |
|       38 | 12668 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12669 | `	if( nEntry < 1 ){` |
|       24 | 12670 | `		return 0;` |
|        - | 12671 | `	}` |
|        - | 12672 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 12673 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 12674 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 12675 | `	}` |
|        - | 12676 | `	/* Mark this class as being autoloaded */` |
|       14 | 12677 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 12678 | `	/* Prepare the class name argument */` |
|       14 | 12679 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 12680 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 12681 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 12682 | `	pClass = 0;` |
|       28 | 12683 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 12684 | `		ph7_value *apArg[1];` |
|       24 | 12685 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 12686 | `		if( pEntry == 0 ){` |
|      ! 0 | 12687 | `			continue;` |
|        - | 12688 | `		}` |
|       24 | 12689 | `		apArg[0] = &sArg;` |
|       24 | 12690 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 12691 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 12692 | `			continue;` |
|        - | 12693 | `		}` |
|        - | 12694 | `		/* Check if the class is now available */` |
|       24 | 12695 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 12696 | `		if( pHashEntry ){` |
|       10 | 12697 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 12698 | `			if( pClass ){` |
|       10 | 12699 | `				break;` |
|        - | 12700 | `			}` |
|      ! 0 | 12701 | `		}` |
|        9 | 12702 | `	}` |
|       14 | 12703 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 12704 | `	PH7_MemObjRelease(&sResult);` |
|        - | 12705 | `	/* Remove reentrancy guard */` |
|       14 | 12706 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 12707 | `	return pClass;` |
|       20 | 12708 |  |
|        - | 12709 | `/*` |
|        - | 12710 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 12711 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 12712 | ` */` |
|       18 | 12713 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12714 |  |
|       20 | 12715 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 12716 |  |
|        - | 12717 | `/*` |
|        - | 12718 | ` * Check if the given name refer to an installed class.` |
|        - | 12719 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12720 | ` */` |
|    33828 | 12721 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12722 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12723 | `	const char *zName,  /* Name of the target class */` |
|        - | 12724 | `	sxu32 nByte,        /* zName length */` |
|        - | 12725 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12726 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12727 | `						 */` |
|        - | 12728 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12729 | `	)` |
|        2 | 12730 |  |
|        - | 12731 | `	SyHashEntry *pEntry;` |
|        - | 12732 | `	ph7_class *pClass;` |
|    16914 | 12733 | `	SXUNUSED(iNest);` |
|        - | 12734 | `	/* Exact class lookup.` |
|        - | 12735 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12736 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    33830 | 12737 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    33830 | 12738 | `	if( pEntry == 0 ){` |
|        - | 12739 | `		/* Class not found in hash table — try autoload before giving up */` |
|       20 | 12740 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 12741 | `	}` |
|    33812 | 12742 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    33812 | 12743 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    16916 | 12744 |  |
|        - | 12745 | `/*` |
|        - | 12746 | ` * Reference Table Implementation` |
|        - | 12747 | ` * Status: stable <chm@symisc.net>` |
|        - | 12748 | ` * Intro` |
|        - | 12749 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12750 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12751 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12752 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12753 | ` *  Refer to the official for more information on this powerful` |
|        - | 12754 | ` *  extension.` |
|        - | 12755 | ` */` |
|        - | 12756 | `/*` |
|        - | 12757 | ` * Allocate a new reference entry.` |
|        - | 12758 | ` */` |
|  3078554 | 12759 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12760 |  |
|        - | 12761 | `	VmRefObj *pRef;` |
|        - | 12762 | `	/* Allocate a new instance */` |
|  3078556 | 12763 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3078556 | 12764 | `	if( pRef == 0 ){` |
|      ! 0 | 12765 | `		return 0;` |
|        - | 12766 | `	}` |
|        - | 12767 | `	/* Zero the structure */` |
|  3078556 | 12768 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12769 | `	/* Initialize fields */` |
|  3078556 | 12770 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3078556 | 12771 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3078556 | 12772 | `	pRef->nIdx = nIdx;` |
|  3078556 | 12773 | `	return pRef;` |
|  1539279 | 12774 |  |
|        - | 12775 | `/*` |
|        - | 12776 | ` * Default hash function used by the reference table` |
|        - | 12777 | ` * for lookup/insertion operations.` |
|        - | 12778 | ` */` |
| 16979130 | 12779 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12780 |  |
|        - | 12781 | `	/* Calculate the hash based on the memory object index */` |
| 16979132 | 12782 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12783 |  |
|        - | 12784 | `/*` |
|        - | 12785 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12786 | ` * in the reference table.` |
|        - | 12787 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12788 | ` * otherwise.` |
|        - | 12789 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12790 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12791 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12792 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12793 | ` * Refer to the official for more information on this powerful` |
|        - | 12794 | ` * extension.` |
|        - | 12795 | ` */` |
|  9187362 | 12796 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12797 |  |
|        - | 12798 | `	VmRefObj *pRef;` |
|        - | 12799 | `	sxu32 nBucket;` |
|        - | 12800 | `	/* Point to the appropriate bucket */` |
|  9187364 | 12801 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12802 | `	/* Perform the lookup */` |
|  9187364 | 12803 | `	pRef = pVm->apRefObj[nBucket];` |
| 20017766 | 12804 | `	for(;;){` |
| 40024873 | 12805 | `		if( pRef == 0 ){` |
|  3160658 | 12806 | `			break;` |
|        - | 12807 | `		}` |
| 36864217 | 12808 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12809 | `			/* Entry found */` |
|  6026708 | 12810 | `			return pRef;` |
|        - | 12811 | `		}` |
|        - | 12812 | `		/* Point to the next entry */` |
| 30837511 | 12813 | `		pRef = pRef->pNextCollide;` |
|        2 | 12814 | `	}` |
|        - | 12815 | `	/* No such entry,return NULL */` |
|  3160658 | 12816 | `	return 0;` |
|  4593683 | 12817 |  |
|        - | 12818 | `/*` |
|        - | 12819 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12820 | ` *` |
|        - | 12821 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12822 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12823 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12824 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12825 | ` * Refer to the official for more information on this powerful` |
|        - | 12826 | ` * extension.` |
|        - | 12827 | ` */` |
|  3078554 | 12828 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12829 |  |
|        - | 12830 | `	sxu32 nBucket;` |
|  3078556 | 12831 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12832 | `		VmRefObj **apNew;` |
|        - | 12833 | `		sxu32 nNew;` |
|        - | 12834 | `		/* Allocate a larger table */` |
|     4078 | 12835 | `		nNew = pVm->nRefSize << 1;` |
|     4078 | 12836 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4078 | 12837 | `		if( apNew ){` |
|     4078 | 12838 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12839 | `			sxu32 n;` |
|        - | 12840 | `			/* Zero the structure */` |
|     4078 | 12841 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12842 | `			/* Rehash all referenced entries */` |
|  2841504 | 12843 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12844 | `				/* Remove old collision links */` |
|  2837428 | 12845 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12846 | `				/* Point to the appropriate bucket */` |
|  2837428 | 12847 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12848 | `				/* Insert the entry  */` |
|  2837428 | 12849 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2837428 | 12850 | `				if( apNew[nBucket] ){` |
|  2298896 | 12851 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12852 | `				}` |
|  2837428 | 12853 | `				apNew[nBucket] = pEntry;` |
|        - | 12854 | `				/* Point to the next entry */` |
|  2837428 | 12855 | `				pEntry = pEntry->pNext;` |
|  1418715 | 12856 | `			}` |
|        - | 12857 | `			/* Release the old table */` |
|     4078 | 12858 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12859 | `			/* Install the new one */` |
|     4078 | 12860 | `			pVm->apRefObj = apNew;` |
|     4078 | 12861 | `			pVm->nRefSize = nNew;` |
|     2038 | 12862 | `		}` |
|     2038 | 12863 | `	}` |
|        - | 12864 | `	/* Point to the appropriate bucket */` |
|  3078556 | 12865 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12866 | `	/* Insert the entry */` |
|  3078556 | 12867 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3078556 | 12868 | `	if( pVm->apRefObj[nBucket] ){` |
|  2534690 | 12869 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1267076 | 12870 | `	}` |
|  3078556 | 12871 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3078556 | 12872 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3078556 | 12873 | `	pVm->nRefUsed++;` |
|  3078556 | 12874 | `	return SXRET_OK;` |
|        2 | 12875 |  |
|        - | 12876 | `/*` |
|        - | 12877 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12878 | ` * the reference table.` |
|        - | 12879 | ` * This function is invoked when the user perform an unset` |
|        - | 12880 | ` * call [i.e: unset($var); ].` |
|        - | 12881 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12882 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12883 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12884 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12885 | ` * Refer to the official for more information on this powerful` |
|        - | 12886 | ` * extension.` |
|        - | 12887 | ` */` |
|  3043996 | 12888 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12889 |  |
|        - | 12890 | `	ph7_hashmap_node **apNode;` |
|        - | 12891 | `	SyHashEntry **apEntry;` |
|        - | 12892 | `	sxu32 n;` |
|        - | 12893 | `	/* Point to the reference table */` |
|  3043998 | 12894 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3043998 | 12895 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12896 | `	/* Unlink the entry from the reference table */` |
|  3132192 | 12897 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    88196 | 12898 | `		if( apEntry[n] ){` |
|    88146 | 12899 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    44072 | 12900 | `		}` |
|    44099 | 12901 | `	}` |
|  6002442 | 12902 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2958446 | 12903 | `		if( apNode[n] ){` |
|     7020 | 12904 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3509 | 12905 | `		}` |
|  1479224 | 12906 | `	}` |
|  3043998 | 12907 | `	if( pRef->pPrevCollide ){` |
|  1168210 | 12908 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   584586 | 12909 | `	}else{` |
|  1875790 | 12910 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12911 | `	}` |
|  3043998 | 12912 | `	if( pRef->pNextCollide ){` |
|  1722082 | 12913 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   860652 | 12914 | `	}` |
|  3043998 | 12915 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12916 | `	/* Release the node */` |
|  3043998 | 12917 | `	SySetRelease(&pRef->aReference);` |
|  3043998 | 12918 | `	SySetRelease(&pRef->aArrEntries);` |
|  3043998 | 12919 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3043998 | 12920 | `	pVm->nRefUsed--;` |
|  3043998 | 12921 | `	return SXRET_OK;` |
|        2 | 12922 |  |
|        - | 12923 | `/*` |
|        - | 12924 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12925 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12926 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12927 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12928 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12929 | ` * Refer to the official for more information on this powerful` |
|        - | 12930 | ` * extension.` |
|        - | 12931 | ` */` |
|  3109776 | 12932 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12933 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12934 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12935 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12936 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12937 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12938 | `	)` |
|        2 | 12939 |  |
|  3109778 | 12940 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12941 | `	VmRefObj *pRef;` |
|        - | 12942 | `	/* Check if the referenced object already exists */` |
|  3109778 | 12943 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3109778 | 12944 | `	if( pRef == 0 ){` |
|        - | 12945 | `		/* Create a new entry */` |
|  3078556 | 12946 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3078556 | 12947 | `		if( pRef == 0 ){` |
|      ! 0 | 12948 | `			return SXERR_MEM;` |
|        - | 12949 | `		}` |
|  3078556 | 12950 | `		pRef->iFlags = iFlags;` |
|        - | 12951 | `		/* Install the entry */` |
|  3078556 | 12952 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1539277 | 12953 | `	}` |
|  3109778 | 12954 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3109778 | 12955 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12956 | `		VmSlot sRef;` |
|        - | 12957 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12958 | `		 * be deleted when we leave this frame.` |
|        - | 12959 | `		 */` |
|    82182 | 12960 | `		sRef.nIdx = nIdx;` |
|    82182 | 12961 | `		sRef.pUserData = pEntry;` |
|    82182 | 12962 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12963 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12964 | `		}` |
|    41090 | 12965 | `	}` |
|  3109778 | 12966 | `	if( pEntry ){` |
|        - | 12967 | `		/* Address of the hash-entry */` |
|   113210 | 12968 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    56604 | 12969 | `	}` |
|  3109778 | 12970 | `	if( pMapEntry ){` |
|        - | 12971 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2991276 | 12972 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1495637 | 12973 | `	}` |
|  3109778 | 12974 | `	return SXRET_OK;` |
|  1554890 | 12975 |  |
|        - | 12976 | `/*` |
|        - | 12977 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12978 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12979 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12980 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12981 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12982 | ` * Refer to the official for more information on this powerful` |
|        - | 12983 | ` * extension.` |
|        - | 12984 | ` */` |
|  3033584 | 12985 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12986 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12987 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12988 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12989 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12990 | `	)` |
|        2 | 12991 |  |
|        - | 12992 | `	VmRefObj *pRef;` |
|        - | 12993 | `	sxu32 n;` |
|        - | 12994 | `	/* Check if the referenced object already exists */` |
|  3033586 | 12995 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3033586 | 12996 | `	if( pRef == 0 ){` |
|        - | 12997 | `		/* Not such entry */` |
|    82098 | 12998 | `		return SXERR_NOTFOUND;` |
|        - | 12999 | `	}` |
|        - | 13000 | `	/* Remove the desired entry */` |
|  2951490 | 13001 | `	if( pEntry ){` |
|        - | 13002 | `		SyHashEntry **apEntry;` |
|       56 | 13003 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 13004 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 13005 | `			if( apEntry[n] == pEntry ){` |
|        - | 13006 | `				/* Nullify the entry */` |
|       56 | 13007 | `				apEntry[n] = 0;` |
|        - | 13008 | `				/*` |
|        - | 13009 | `				 * NOTE:` |
|        - | 13010 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 13011 | `				 * we avoid wasting spaces.` |
|        - | 13012 | `				 */` |
|       27 | 13013 | `			}` |
|       79 | 13014 | `		}` |
|       27 | 13015 | `	}` |
|  2951490 | 13016 | `	if( pMapEntry ){` |
|        - | 13017 | `		ph7_hashmap_node **apNode;` |
|  2951436 | 13018 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5902964 | 13019 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2951530 | 13020 | `			if( apNode[n] == pMapEntry ){` |
|        - | 13021 | `				/* nullify the entry */` |
|  2951436 | 13022 | `				apNode[n] = 0;` |
|  1475717 | 13023 | `			}` |
|  1475766 | 13024 | `		}` |
|  1475717 | 13025 | `	}` |
|  2951490 | 13026 | `	return SXRET_OK;` |
|  1516794 | 13027 |  |
|        - | 13028 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 13029 | `/*` |
|        - | 13030 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 13031 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 13032 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 13033 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 13034 | ` * For more information on how to register IO stream devices,please` |
|        - | 13035 | ` * refer to the official documentation.` |
|        - | 13036 | ` */` |
|    25146 | 13037 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 13038 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 13039 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 13040 | `	int nByte              /* *pzDevice length*/` |
|        - | 13041 | `	)` |
|        2 | 13042 |  |
|        - | 13043 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 13044 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 13045 | `	SyString sDev,sCur;` |
|        - | 13046 | `	sxu32 n,nEntry;` |
|        - | 13047 | `	int rc;` |
|        - | 13048 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    25148 | 13049 | `	zNext = zCur = zIn = *pzDevice;` |
|    25148 | 13050 | `	zEnd = &zIn[nByte];` |
|  1601041 | 13051 | `	while( zIn < zEnd ){` |
|  1575897 | 13052 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 13053 | `			/* Got one */` |
|        3 | 13054 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 13055 | `			break;` |
|        - | 13056 | `		}` |
|        - | 13057 | `		/* Advance the cursor */` |
|  1575895 | 13058 | `		zIn++;` |
|        2 | 13059 | `	}` |
|    25148 | 13060 | `	if( zIn >= zEnd ){` |
|        - | 13061 | `		/* No such scheme,return the default stream */` |
|    25146 | 13062 | `		return pVm->pDefStream;` |
|        - | 13063 | `	}` |
|        3 | 13064 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 13065 | `	/* Remove leading and trailing white spaces */` |
|        3 | 13066 | `	SyStringFullTrim(&sDev);` |
|        - | 13067 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 13068 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 13069 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 13070 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 13071 | `		pStream = apStream[n];` |
|        3 | 13072 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 13073 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 13074 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 13075 | `		if( rc == 0 ){` |
|        - | 13076 | `			/* Stream device found */` |
|        3 | 13077 | `			*pzDevice = zNext;` |
|        3 | 13078 | `			return pStream;` |
|        - | 13079 | `		}` |
|      ! 0 | 13080 | `	}` |
|        - | 13081 | `	/* No such stream,return NULL */` |
|      ! 0 | 13082 | `	return 0;` |
|    12575 | 13083 |  |
|        - | 13084 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 13085 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 13086 |  |
