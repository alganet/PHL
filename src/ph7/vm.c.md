# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5056/6632 lines (76.24%)

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
|   794548 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   794550 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   794516 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   794506 |   104 | `	return FALSE;` |
|   397298 |   105 |  |
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
|   507806 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   507808 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   507808 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   507804 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   507804 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   507804 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   507804 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   507804 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   507804 |   152 | `	pCons->xExpand = xExpand;` |
|   507804 |   153 | `	pCons->pUserData = pUserData;` |
|   507804 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   507804 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   507804 |   161 | `	return SXRET_OK;` |
|   253905 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1116478 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1116480 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1116480 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1116480 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1116480 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1116480 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1116480 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1116480 |   195 | `	pFunc->pVm   = pVm;` |
|  1116480 |   196 | `	pFunc->xFunc = xFunc;` |
|  1116480 |   197 | `	pFunc->pUserData = pUserData;` |
|  1116480 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1116480 |   200 | `	*ppOut = pFunc;` |
|  1116480 |   201 | `	return SXRET_OK;` |
|   558241 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1118818 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1118820 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1118820 |   223 | `	if( pEntry ){` |
|     2342 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2342 |   225 | `		pFunc->pUserData = pUserData;` |
|     2342 |   226 | `		pFunc->xFunc = xFunc;` |
|     2342 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2342 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1116480 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1116480 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1116480 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1116480 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1116480 |   243 | `	return SXRET_OK;` |
|   559411 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   159836 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   159838 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   159838 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   159838 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   159838 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   159838 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   159838 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   159838 |   270 | `	pFunc->iFlags = iFlags;` |
|   159838 |   271 | `	pFunc->pUserData = pUserData;` |
|   159838 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   159838 |   273 | `	return SXRET_OK;` |
|        2 |   274 |  |
|        - |   275 | `/*` |
|        - |   276 | ` * Namespace-aware function lookup.` |
|        - |   277 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   278 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   279 | ` */` |
|        - |   280 | `/*` |
|        - |   281 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   282 | ` */` |
|   627986 |   283 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   284 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   285 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   286 | `	SyString *pName     /* Function name */` |
|        - |   287 | `	)` |
|        2 |   288 |  |
|        - |   289 | `	SyHashEntry *pEntry;` |
|        - |   290 | `	sxi32 rc;` |
|   627988 |   291 | `	if( pName == 0 ){` |
|        - |   292 | `		/* Use the built-in name */` |
|    34482 |   293 | `		pName = &pFunc->sName;` |
|    17240 |   294 | `	}` |
|        - |   295 | `	/* Check for duplicates (functions with the same name) first */` |
|   627988 |   296 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   627988 |   297 | `	if( pEntry ){` |
|   489232 |   298 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   489232 |   299 | `		if( pLink != pFunc ){` |
|        - |   300 | `			/* Link */` |
|      184 |   301 | `			pFunc->pNextName = pLink;` |
|      184 |   302 | `			pEntry->pUserData = pFunc;` |
|       91 |   303 | `		}` |
|   489232 |   304 | `		return SXRET_OK;` |
|        - |   305 | `	}` |
|        - |   306 | `	/* First time seen */` |
|   138758 |   307 | `	pFunc->pNextName = 0;` |
|   138758 |   308 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   138758 |   309 | `	return rc;` |
|   313995 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   313 | ` */` |
|    44732 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   315 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   316 | `	ph7_class *pClass /* Target Class */` |
|        - |   317 | `	)` |
|        2 |   318 |  |
|    44734 |   319 | `	SyString *pName = &pClass->sName;` |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|        - |   322 | `	/* Check for duplicates */` |
|    44734 |   323 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    44734 |   324 | `	if( pEntry ){` |
|       31 |   325 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   326 | `		/* Link entry with the same name */` |
|       31 |   327 | `		pClass->pNextName = pLink;` |
|       31 |   328 | `		pEntry->pUserData = pClass;` |
|       31 |   329 | `		return SXRET_OK;` |
|        - |   330 | `	}` |
|    44704 |   331 | `	pClass->pNextName = 0;` |
|        - |   332 | `	/* Perform a simple hashtable insertion */` |
|    44704 |   333 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    44704 |   334 | `	return rc;` |
|    22368 |   335 |  |
|        - |   336 | `/*` |
|        - |   337 | ` * Instruction builder interface.` |
|        - |   338 | ` */` |
|  3227136 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3227138 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3227138 |   352 | `	sInstr.iP1 = iP1;` |
|  3227138 |   353 | `	sInstr.iP2 = iP2;` |
|  3227138 |   354 | `	sInstr.p3  = p3;` |
|  3227138 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   186138 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    93068 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3227138 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3227138 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3227138 |   365 | `	return rc;` |
|        2 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Swap the current bytecode container with the given one.` |
|        - |   369 | ` */` |
|   382732 |   370 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   371 |  |
|   382734 |   372 | `	if( pContainer == 0 ){` |
|        - |   373 | `		/* Point to the default container */` |
|      ! 0 |   374 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   375 | `	}else{` |
|        - |   376 | `		/* Change container */` |
|   382734 |   377 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   378 | `	}` |
|   382734 |   379 | `	return SXRET_OK;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Return the current bytecode container.` |
|        - |   383 | ` */` |
|   191366 |   384 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   385 |  |
|   191368 |   386 | `	return pVm->pByteContainer;` |
|        2 |   387 |  |
|        - |   388 | `/*` |
|        - |   389 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   390 | ` */` |
|   183458 |   391 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   392 |  |
|        - |   393 | `	VmInstr *pInstr;` |
|   183460 |   394 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   183460 |   395 | `	return pInstr;` |
|        2 |   396 |  |
|        - |   397 | `/*` |
|        - |   398 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   399 | ` */` |
|   967038 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|   967040 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   174382 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   174384 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   624768 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   624770 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   417 |  |
|    26756 |   418 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   419 |  |
|        - |   420 | `	VmInstr *aInstr;` |
|        - |   421 | `	sxu32 n;` |
|    26758 |   422 | `	n = SySetUsed(pVm->pByteContainer);` |
|    26758 |   423 | `	if( n < 2 ){` |
|      ! 0 |   424 | `		return 0;` |
|        - |   425 | `	}` |
|    26758 |   426 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    26758 |   427 | `	return &aInstr[n - 2];` |
|    13380 |   428 |  |
|        - |   429 | `/*` |
|        - |   430 | ` * Allocate a new virtual machine frame.` |
|        - |   431 | ` */` |
|    16170 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    16172 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16172 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    16172 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    16172 |   447 | `	pFrame->pUserData = pUserData;` |
|    16172 |   448 | `	pFrame->pThis = pThis;` |
|    16172 |   449 | `	pFrame->pVm = pVm;` |
|    16172 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16172 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16172 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16172 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16172 |   454 | `	return pFrame;` |
|     8087 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    16128 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    16130 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    16130 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    16130 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    16130 |   476 | `	pVm->pFrame = pFrame;` |
|    16130 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    13528 |   479 | `		*ppFrame = pFrame;` |
|     6763 |   480 | `	}` |
|    16130 |   481 | `	return SXRET_OK;` |
|     8066 |   482 |  |
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
|    13526 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    13528 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13528 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    13528 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13528 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13464 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    93672 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    80210 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    40106 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    13464 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    93728 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    80266 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    40134 |   545 | `			}` |
|     6731 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    13528 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13528 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    13528 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13528 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    13528 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6763 |   554 | `	}` |
|    13528 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6419268 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6419546 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      278 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6419270 |   566 | `	return pFrame;` |
|        2 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Compare two functions signature and return the comparison result.` |
|        - |   570 | ` */` |
|      818 |   571 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   572 |  |
|      819 |   573 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   574 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   575 | `	const char *zSin = pSecond->zString;` |
|      819 |   576 | `	const char *zFin = pFirst->zString;` |
|      819 |   577 | `	const char *zPtr = zFin;` |
|      409 |   578 | `	for(;;){` |
|      819 |   579 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   580 | `			break;` |
|        - |   581 | `		}` |
|      ! 0 |   582 | `		if( zFin[0] != zSin[0] ){` |
|        - |   583 | `			/* mismatch */` |
|      ! 0 |   584 | `			break;` |
|        - |   585 | `		}` |
|      ! 0 |   586 | `		zFin++;` |
|      ! 0 |   587 | `		zSin++;` |
|      ! 0 |   588 | `	}` |
|      819 |   589 | `	return (int)(zFin-zPtr);` |
|        1 |   590 |  |
|        - |   591 | `/*` |
|        - |   592 | ` * Select the appropriate VM function for the current call context.` |
|        - |   593 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   594 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   595 | ` * Refer to the official documentation for more information.` |
|        - |   596 | ` */` |
|      132 |   597 | `static ph7_vm_func * VmOverload(` |
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
|      134 |   610 | `	pLink = pList;` |
|      134 |   611 | `	i = 0;` |
|        - |   612 | `	/* Put functions expecting the same number of passed arguments */` |
|     1062 |   613 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1000 |   614 | `		if( pLink == 0 ){` |
|       72 |   615 | `			break;` |
|        - |   616 | `		}` |
|      930 |   617 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   618 | `			/* Candidate for overloading */` |
|      884 |   619 | `			apSet[i++] = pLink;` |
|      441 |   620 | `		}` |
|        - |   621 | `		/* Point to the next entry */` |
|      930 |   622 | `		pLink = pLink->pNextName;` |
|        2 |   623 | `	}` |
|      134 |   624 | `	if( i < 1 ){` |
|        - |   625 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   626 | `		return pList;` |
|        - |   627 | `	}` |
|      134 |   628 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   629 | `		/* Return the only candidate */` |
|       32 |   630 | `		return apSet[0];` |
|        - |   631 | `	}` |
|        - |   632 | `	/* Calculate function signature */` |
|      103 |   633 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   634 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   635 | `		int c = 'n'; /* null */` |
|      253 |   636 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   637 | `			/* Hashmap */` |
|       45 |   638 | `			c = 'h';` |
|      231 |   639 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   640 | `			/* bool */` |
|      ! 0 |   641 | `			c = 'b';` |
|      209 |   642 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   643 | `			/* int */` |
|        5 |   644 | `			c = 'i';` |
|      207 |   645 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   646 | `			/* String */` |
|      105 |   647 | `			c = 's';` |
|      153 |   648 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   649 | `			/* Float */` |
|      ! 0 |   650 | `			c = 'f';` |
|      101 |   651 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   652 | `			/* Class instance */` |
|      ! 0 |   653 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   654 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   655 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   656 | `			c = -1;` |
|      ! 0 |   657 | `		}` |
|      253 |   658 | `		if( c > 0 ){` |
|      253 |   659 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   660 | `		}` |
|      127 |   661 | `	}` |
|      103 |   662 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   663 | `	iTarget = 0;` |
|      103 |   664 | `	iMax = -1;` |
|        - |   665 | `	/* Select the appropriate function */` |
|      921 |   666 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   667 | `		/* Compare the two signatures */` |
|      819 |   668 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   669 | `		if( iCur > iMax ){` |
|      103 |   670 | `			iMax = iCur;` |
|      103 |   671 | `			iTarget = j;` |
|       51 |   672 | `		}` |
|      410 |   673 | `	}` |
|      103 |   674 | `	SyBlobRelease(&sSig);` |
|        - |   675 | `	/* Appropriate function for the current call context */` |
|      103 |   676 | `	return apSet[iTarget];` |
|       68 |   677 |  |
|        - |   678 | `/* Forward declaration */` |
|        - |   679 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   680 | `/*` |
|        - |   681 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   682 | ` * it can be instanciated from the executed PHP script.` |
|        - |   683 | ` */` |
|   122494 |   684 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   685 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   686 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	ph7_class_method *pMeth;` |
|        - |   690 | `	ph7_class_attr *pAttr;` |
|        - |   691 | `	SyHashEntry *pEntry;` |
|        - |   692 | `	sxi32 rc;` |
|        - |   693 | `	/* Reset the loop cursor */` |
|   122496 |   694 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   695 | `	/* Process only static and constant attribute */` |
|   516385 |   696 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   697 | `		/* Extract the current attribute */` |
|   332644 |   698 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   332644 |   699 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   700 | `			ph7_value *pMemObj;` |
|        - |   701 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1294 |   702 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1294 |   703 | `			if( pMemObj == 0 ){` |
|      ! 0 |   704 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   705 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   706 | `					&pClass->sName,&pAttr->sName` |
|        - |   707 | `					);` |
|      ! 0 |   708 | `				return SXERR_MEM;` |
|        - |   709 | `			}` |
|     1294 |   710 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   711 | `				/* Initialize attribute default value (any complex expression) */` |
|     1294 |   712 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      646 |   713 | `			}` |
|        - |   714 | `			/* Record attribute index */` |
|     1294 |   715 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   716 | `			/* Install static attribute in the reference table */` |
|     1294 |   717 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      646 |   718 | `		}` |
|        2 |   719 | `	}` |
|        - |   720 | `	/* Install class methods */` |
|   122496 |   721 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   722 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   723 | `		 */` |
|    52948 |   724 | `		return SXRET_OK;` |
|        - |   725 | `	}` |
|        - |   726 | `	/* Create constructor alias if not yet done */` |
|    69550 |   727 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   728 | `		/* User constructor with the same base class name */` |
|     5260 |   729 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5260 |   730 | `		if( pEntry ){` |
|      ! 0 |   731 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   732 | `			/* Create the alias */` |
|      ! 0 |   733 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   734 | `		}` |
|     2629 |   735 | `	}` |
|        - |   736 | `	/* Install the methods now */` |
|    69550 |   737 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   697838 |   738 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   593516 |   739 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   593516 |   740 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   593508 |   741 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   593508 |   742 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   743 | `				return rc;` |
|        - |   744 | `			}` |
|   296753 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    69550 |   748 | `	pClass->bMounted = TRUE;` |
|    69550 |   749 | `	return SXRET_OK;` |
|    61249 |   750 |  |
|        - |   751 | `/*` |
|        - |   752 | ` * Allocate a private frame for attributes of the given` |
|        - |   753 | ` * class instance (Object in the PHP jargon).` |
|        - |   754 | ` */` |
|     1214 |   755 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   756 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   757 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   758 | `	)` |
|        2 |   759 |  |
|     1216 |   760 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   761 | `	ph7_class_attr *pAttr;` |
|        - |   762 | `	SyHashEntry *pEntry;` |
|        - |   763 | `	sxi32 rc;` |
|        - |   764 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1216 |   765 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4976 |   766 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   767 | `		VmClassAttr *pVmAttr;` |
|        - |   768 | `		/* Extract the current attribute */` |
|     3762 |   769 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3762 |   770 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3762 |   771 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   772 | `			return SXERR_MEM;` |
|        - |   773 | `		}` |
|     3762 |   774 | `		pVmAttr->pAttr = pAttr;` |
|     3762 |   775 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   776 | `			ph7_value *pMemObj;` |
|        - |   777 | `			/* Reserve a memory object for this attribute */` |
|     3756 |   778 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3756 |   779 | `			if( pMemObj == 0 ){` |
|      ! 0 |   780 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   781 | `				return SXERR_MEM;` |
|        - |   782 | `			}` |
|     3756 |   783 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3756 |   784 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   785 | `				/* Initialize attribute default value (any complex expression) */` |
|     1212 |   786 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      605 |   787 | `			}` |
|     3756 |   788 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3756 |   789 | `			if( rc != SXRET_OK ){` |
|        - |   790 | `				VmSlot sSlot;` |
|        - |   791 | `				/* Restore memory object */` |
|      ! 0 |   792 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   793 | `				sSlot.pUserData = 0;` |
|      ! 0 |   794 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `			/* Install attribute in the reference table */` |
|     3756 |   799 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1879 |   800 | `		}else{` |
|        - |   801 | `			/* Install static/constant attribute */` |
|        8 |   802 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   803 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   804 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   805 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   806 | `				return SXERR_MEM;` |
|        - |   807 | `			}` |
|        - |   808 | `		}` |
|        2 |   809 | `	}` |
|     1216 |   810 | `	return SXRET_OK;` |
|      609 |   811 |  |
|        - |   812 | `/* Forward declaration */` |
|        - |   813 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   814 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   815 | `/*` |
|        - |   816 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   817 | ` */` |
|        - |   818 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   819 | `/*` |
|        - |   820 | ` * Reserve a constant memory object.` |
|        - |   821 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   822 | ` */` |
|   370262 |   823 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   824 |  |
|        - |   825 | `	ph7_value *pObj;` |
|        - |   826 | `	sxi32 rc;` |
|   370264 |   827 | `	if( pIndex ){` |
|        - |   828 | `		/* Object index in the object table */` |
|   362458 |   829 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   181228 |   830 | `	}` |
|        - |   831 | `	/* Reserve a slot for the new object */` |
|   370264 |   832 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   370264 |   833 | `	if( rc != SXRET_OK ){` |
|        - |   834 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   835 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   836 | `		 */` |
|      ! 0 |   837 | `		return 0;` |
|        - |   838 | `	}` |
|   370264 |   839 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   370264 |   840 | `	return pObj;` |
|   185133 |   841 |  |
|        - |   842 | `/*` |
|        - |   843 | ` * Reserve a memory object.` |
|        - |   844 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   845 | ` */` |
|  2142508 |   846 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   847 |  |
|        - |   848 | `	ph7_value *pObj;` |
|        - |   849 | `	sxi32 rc;` |
|  2142510 |   850 | `	if( pIndex ){` |
|        - |   851 | `		/* Object index in the object table */` |
|  2142510 |   852 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1071254 |   853 | `	}` |
|        - |   854 | `	/* Reserve a slot for the new object */` |
|  2142510 |   855 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2142510 |   856 | `	if( rc != SXRET_OK ){` |
|        - |   857 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   858 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   859 | `		 */` |
|      ! 0 |   860 | `		return 0;` |
|        - |   861 | `	}` |
|  2142510 |   862 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2142510 |   863 | `	return pObj;` |
|  1071256 |   864 |  |
|        - |   865 | `/* Forward declaration */` |
|        - |   866 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   867 | `/* Forward declarations for Fiber C functions */` |
|        - |   868 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   869 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   870 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   871 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   872 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   873 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   874 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   875 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   876 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   877 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   878 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |   879 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |   880 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   881 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |   882 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |   883 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   884 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   885 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   886 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   887 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   888 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   889 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   890 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   891 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   892 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   893 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   894 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   895 | `/*` |
|        - |   896 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   897 | ` * directly as foreign functions.` |
|        - |   898 | ` */` |
|        - |   899 | `#define PH7_BUILTIN_LIB \` |
|        - |   900 | `	"class Exception { "\` |
|        - |   901 | `    "protected $message = 'Unknown exception';"\` |
|        - |   902 | `    "protected $code = 0;"\` |
|        - |   903 | `    "protected $file;"\` |
|        - |   904 | `    "protected $line;"\` |
|        - |   905 | `    "protected $trace;"\` |
|        - |   906 | `    "protected $previous;"\` |
|        - |   907 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   908 | `	"   if( isset($message) ){"\` |
|        - |   909 | `	"	  $this->message = $message;"\` |
|        - |   910 | `	"   }"\` |
|        - |   911 | `	"   $this->code = $code;"\` |
|        - |   912 | `	"   $this->file = __FILE__;"\` |
|        - |   913 | `	"   $this->line = __LINE__;"\` |
|        - |   914 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   915 | `	"   if( isset($previous) ){"\` |
|        - |   916 | `	"     $this->previous = $previous;"\` |
|        - |   917 | `	"   }"\` |
|        - |   918 | `	"}"\` |
|        - |   919 | `	"public function getMessage(){"\` |
|        - |   920 | `	"   return $this->message;"\` |
|        - |   921 | `	"}"\` |
|        - |   922 | `	" public function getCode(){"\` |
|        - |   923 | `	"  return $this->code;"\` |
|        - |   924 | `	"}"\` |
|        - |   925 | `	"public function getFile(){"\` |
|        - |   926 | `	"  return $this->file;"\` |
|        - |   927 | `	"}"\` |
|        - |   928 | `	"public function getLine(){"\` |
|        - |   929 | `	"  return $this->line;"\` |
|        - |   930 | `	"}"\` |
|        - |   931 | `	"public function getTrace(){"\` |
|        - |   932 | `	"   return $this->trace;"\` |
|        - |   933 | `	"}"\` |
|        - |   934 | `	"public function getTraceAsString(){"\` |
|        - |   935 | `	"  return debug_string_backtrace();"\` |
|        - |   936 | `	"}"\` |
|        - |   937 | `	"public function getPrevious(){"\` |
|        - |   938 | `	"    return $this->previous;"\` |
|        - |   939 | `	"}"\` |
|        - |   940 | `	"public function __toString(){"\` |
|        - |   941 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   942 | `    "}"\` |
|        - |   943 | `	"}"\` |
|        - |   944 | `	"class Error extends Exception { }"\` |
|        - |   945 | `	"class TypeError extends Error { }"\` |
|        - |   946 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   947 | `	"class ValueError extends Error { }"\` |
|        - |   948 | `	"class FiberError extends Error { }"\` |
|        - |   949 | `	"class AssertionError extends Error { }"\` |
|        - |   950 | `	"class ArithmeticError extends Error { }"\` |
|        - |   951 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |   952 | `	"class ErrorException extends Exception { "\` |
|        - |   953 | `	"protected $severity;"\` |
|        - |   954 | `	"public function __construct(string $message = null,"\` |
|        - |   955 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   956 | `	"   if( isset($message) ){"\` |
|        - |   957 | `	"	  $this->message = $message;"\` |
|        - |   958 | `	"   }"\` |
|        - |   959 | `	"   $this->severity = $severity;"\` |
|        - |   960 | `	"   $this->code = $code;"\` |
|        - |   961 | `	"   $this->file = $filename;"\` |
|        - |   962 | `	"   $this->line = $lineno;"\` |
|        - |   963 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   964 | `	"   if( isset($previous) ){"\` |
|        - |   965 | `	"     $this->previous = $previous;"\` |
|        - |   966 | `	"   }"\` |
|        - |   967 | `	"}"\` |
|        - |   968 | `	"public function getSeverity(){"\` |
|        - |   969 | `	"   return $this->severity;"\` |
|        - |   970 | `    "}"\` |
|        - |   971 | `	"}"\` |
|        - |   972 | `	"interface Iterator {"\` |
|        - |   973 | `	"public function current();"\` |
|        - |   974 | `	"public function key();"\` |
|        - |   975 | `	"public function next();"\` |
|        - |   976 | `	"public function rewind();"\` |
|        - |   977 | `	"public function valid();"\` |
|        - |   978 | `	"}"\` |
|        - |   979 | `	"interface IteratorAggregate {"\` |
|        - |   980 | `	"public function getIterator();"\` |
|        - |   981 | `	"}"\` |
|        - |   982 | `	"interface Serializable {"\` |
|        - |   983 | `	"public function serialize();"\` |
|        - |   984 | `	"public function unserialize(string $serialized);"\` |
|        - |   985 | `	"}"\` |
|        - |   986 | `	"/* Directory releated IO */"\` |
|        - |   987 | `	"class Directory {"\` |
|        - |   988 | `	"public $handle = null;"\` |
|        - |   989 | `	"public $path  = null;"\` |
|        - |   990 | `	"public function __construct(string $path)"\` |
|        - |   991 | `	"{"\` |
|        - |   992 | `	"   $this->handle = opendir($path);"\` |
|        - |   993 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   994 | `	"      $this->path = $path;"\` |
|        - |   995 | `	"   }"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"public function __destruct()"\` |
|        - |   998 | `	"{"\` |
|        - |   999 | `	"  if( $this->handle != null ){"\` |
|        - |  1000 | `	"       closedir($this->handle);"\` |
|        - |  1001 | `	"  }"\` |
|        - |  1002 | `	"}"\` |
|        - |  1003 | `	"public function read()"\` |
|        - |  1004 | `	"{"\` |
|        - |  1005 | `	"    return readdir($this->handle);"\` |
|        - |  1006 | `	"}"\` |
|        - |  1007 | `	"public function rewind()"\` |
|        - |  1008 | `	"{"\` |
|        - |  1009 | `	"    rewinddir($this->handle);"\` |
|        - |  1010 | `	"}"\` |
|        - |  1011 | `	"public function close()"\` |
|        - |  1012 | `	"{"\` |
|        - |  1013 | `	"    closedir($this->handle);"\` |
|        - |  1014 | `	"    $this->handle = null;"\` |
|        - |  1015 | `	"}"\` |
|        - |  1016 | `	"}"\` |
|        - |  1017 | `	"class Fiber {"\` |
|        - |  1018 | `	"  private $__ctx;"\` |
|        - |  1019 | `	"  private $__callable;"\` |
|        - |  1020 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1021 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1022 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1023 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1024 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1025 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1026 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1027 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1028 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1029 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1030 | `	"}"\` |
|        - |  1031 | `	"class Generator implements Iterator {"\` |
|        - |  1032 | `	"  private $__ctx;"\` |
|        - |  1033 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1034 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1035 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1036 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1037 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1038 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1039 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1040 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1041 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"class stdClass{"\` |
|        - |  1044 | `	"  public $value;"\` |
|        - |  1045 | `	" /* Magic methods */"\` |
|        - |  1046 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1047 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1048 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1049 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1050 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1051 | `	"}"\` |
|        - |  1052 | `	"function dir(string $path){"\` |
|        - |  1053 | `	"   return new Directory($path);"\` |
|        - |  1054 | `	"}"\` |
|        - |  1055 | `	"function Dir(string $path){"\` |
|        - |  1056 | `	"   return new Directory($path);"\` |
|        - |  1057 | `	"}"\` |
|        - |  1058 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1059 | `    "{"\` |
|        - |  1060 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1061 | `	"  $aDir = array();"\` |
|        - |  1062 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1063 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1064 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1065 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1066 | `	"   }"\` |
|        - |  1067 | `	"  closedir($pHandle);"\` |
|        - |  1068 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1069 | `	"      rsort($aDir);"\` |
|        - |  1070 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1071 | `	"      sort($aDir);"\` |
|        - |  1072 | `	"  }"\` |
|        - |  1073 | `	"  return $aDir;"\` |
|        - |  1074 | `	"}"\` |
|        - |  1075 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1076 | `	"/* Open the target directory */"\` |
|        - |  1077 | `	"$zDir = dirname($pattern);"\` |
|        - |  1078 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1079 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1080 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1081 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1082 | `	"	return FALSE;"\` |
|        - |  1083 | `	"}"\` |
|        - |  1084 | `	"$pattern = basename($pattern);"\` |
|        - |  1085 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1086 | `	"/* Loop throw available entries */"\` |
|        - |  1087 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1088 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1089 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1090 | `	"	if( $rc ){"\` |
|        - |  1091 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1092 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1093 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1094 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1095 | `	"		  }"\` |
|        - |  1096 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1097 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1098 | `	"		 continue;"\` |
|        - |  1099 | `	"	   }"\` |
|        - |  1100 | `	"	   /* Add the entry */"\` |
|        - |  1101 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1102 | `	"	}"\` |
|        - |  1103 | `	" }"\` |
|        - |  1104 | `	"/* Close the handle */"\` |
|        - |  1105 | `	"closedir($pHandle);"\` |
|        - |  1106 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1107 | `	"  /* Sort the array */"\` |
|        - |  1108 | `	"  sort($pArray);"\` |
|        - |  1109 | `	"}"\` |
|        - |  1110 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1111 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1112 | `	"  $pArray[] = $pattern;"\` |
|        - |  1113 | `	"}"\` |
|        - |  1114 | `	"/* Return the created array */"\` |
|        - |  1115 | `	"return $pArray;"\` |
|        - |  1116 | `   "}"\` |
|        - |  1117 | `   "/* Creates a temporary file */"\` |
|        - |  1118 | `   "function tmpfile(){"\` |
|        - |  1119 | `   "  /* Extract the temp directory */"\` |
|        - |  1120 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1121 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1122 | `   "    /* Use the current dir */"\` |
|        - |  1123 | `   "    $zTempDir = '.';"\` |
|        - |  1124 | `   "  }"\` |
|        - |  1125 | `   "  /* Create the file */"\` |
|        - |  1126 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1127 | `   "  return $pHandle;"\` |
|        - |  1128 | `   "}"\` |
|        - |  1129 | `   "/* Creates a temporary filename */"\` |
|        - |  1130 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1131 | `   "{"\` |
|        - |  1132 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1133 | `   "}"\` |
|        - |  1134 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1135 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1136 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1137 | `   "/* Copy arguments */"\` |
|        - |  1138 | `   "$nArgs = func_num_args();"\` |
|        - |  1139 | `   "$pNew = array();"\` |
|        - |  1140 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1141 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1142 | `    "}"\` |
|        - |  1143 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1144 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1145 | `	"/* Erase */"\` |
|        - |  1146 | `	"array_erase($pArray);"\` |
|        - |  1147 | `	"/* Unshift */"\` |
|        - |  1148 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1149 | `	"return sizeof($pArray);"\` |
|        - |  1150 | `    "}"\` |
|        - |  1151 | `	"function array_merge_recursive(){"\` |
|        - |  1152 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1153 | `    "$arrays = func_get_args();"\` |
|        - |  1154 | `    "$narrays = count($arrays);"\` |
|        - |  1155 | `    "$ret = array();"\` |
|        - |  1156 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1157 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1158 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1159 | `	 " }"\` |
|        - |  1160 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1161 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1162 | `     "  if( $keyIsInt ) {"\` |
|        - |  1163 | `     "   $ret[] = $value;"\` |
|        - |  1164 | `     "  } else {"\` |
|        - |  1165 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1166 | `     "    $cur = $ret[$key];"\` |
|        - |  1167 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1168 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1169 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1170 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1171 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1172 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1173 | `     "    } else {"\` |
|        - |  1174 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1175 | `     "    }"\` |
|        - |  1176 | `     "   } else {"\` |
|        - |  1177 | `     "    $ret[$key] = $value;"\` |
|        - |  1178 | `     "   }"\` |
|        - |  1179 | `     "  }"\` |
|        - |  1180 | `     " }"\` |
|        - |  1181 | `	 " }"\` |
|        - |  1182 | `	 " return $ret;"\` |
|        - |  1183 | `    "}"\` |
|        - |  1184 | `	"function max(){"\` |
|        - |  1185 | `    "  $pArgs = func_get_args();"\` |
|        - |  1186 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1187 | `	"  return null;"\` |
|        - |  1188 | `    " }"\` |
|        - |  1189 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1190 | `    " $pArg = $pArgs[0];"\` |
|        - |  1191 | `	" if( !is_array($pArg) ){"\` |
|        - |  1192 | `	"   return $pArg; "\` |
|        - |  1193 | `	" }"\` |
|        - |  1194 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1195 | `	"   return null;"\` |
|        - |  1196 | `	" }"\` |
|        - |  1197 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1198 | `	" reset($pArg);"\` |
|        - |  1199 | `	" $max = current($pArg);"\` |
|        - |  1200 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1201 | `	"   if( $val > $max ){"\` |
|        - |  1202 | `	"     $max = $val;"\` |
|        - |  1203 | `    " }"\` |
|        - |  1204 | `	" }"\` |
|        - |  1205 | `	" return $max;"\` |
|        - |  1206 | `    " }"\` |
|        - |  1207 | `    " $max = $pArgs[0];"\` |
|        - |  1208 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1209 | `    " $val = $pArgs[$i];"\` |
|        - |  1210 | `	"if( $val > $max ){"\` |
|        - |  1211 | `	" $max = $val;"\` |
|        - |  1212 | `	"}"\` |
|        - |  1213 | `    " }"\` |
|        - |  1214 | `	" return $max;"\` |
|        - |  1215 | `    "}"\` |
|        - |  1216 | `	"function min(){"\` |
|        - |  1217 | `    "  $pArgs = func_get_args();"\` |
|        - |  1218 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1219 | `	"  return null;"\` |
|        - |  1220 | `    " }"\` |
|        - |  1221 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1222 | `    " $pArg = $pArgs[0];"\` |
|        - |  1223 | `	" if( !is_array($pArg) ){"\` |
|        - |  1224 | `	"   return $pArg; "\` |
|        - |  1225 | `	" }"\` |
|        - |  1226 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1227 | `	"   return null;"\` |
|        - |  1228 | `	" }"\` |
|        - |  1229 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1230 | `	" reset($pArg);"\` |
|        - |  1231 | `	" $min = current($pArg);"\` |
|        - |  1232 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1233 | `	"   if( $val < $min ){"\` |
|        - |  1234 | `	"     $min = $val;"\` |
|        - |  1235 | `    " }"\` |
|        - |  1236 | `	" }"\` |
|        - |  1237 | `	" return $min;"\` |
|        - |  1238 | `    " }"\` |
|        - |  1239 | `    " $min = $pArgs[0];"\` |
|        - |  1240 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1241 | `    " $val = $pArgs[$i];"\` |
|        - |  1242 | `	"if( $val < $min ){"\` |
|        - |  1243 | `	" $min = $val;"\` |
|        - |  1244 | `	" }"\` |
|        - |  1245 | `    " }"\` |
|        - |  1246 | `	" return $min;"\` |
|        - |  1247 | `	"}"\` |
|        - |  1248 | `	"function fileowner(string $file){"\` |
|        - |  1249 | `    " $a = stat($file);"\` |
|        - |  1250 | `	" if( !is_array($a) ){"\` |
|        - |  1251 | `	"	return false;"\` |
|        - |  1252 | `	" }"\` |
|        - |  1253 | `	" return $a['uid'];"\` |
|        - |  1254 | `    "}"\` |
|        - |  1255 | `    "function filegroup(string $file){"\` |
|        - |  1256 | `	" $a = stat($file);"\` |
|        - |  1257 | `	" if( !is_array($a) ){"\` |
|        - |  1258 | `	"	return false;"\` |
|        - |  1259 | `	" }"\` |
|        - |  1260 | `	" return $a['gid'];"\` |
|        - |  1261 | `    "}"\` |
|        - |  1262 | `	 "function fileinode(string $file){"\` |
|        - |  1263 | `	" $a = stat($file);"\` |
|        - |  1264 | `	" if( !is_array($a) ){"\` |
|        - |  1265 | `	"	return false;"\` |
|        - |  1266 | `	" }"\` |
|        - |  1267 | `	" return $a['ino'];"\` |
|        - |  1268 | `    "}"` |
|        - |  1269 |  |
|        - |  1270 | `/*` |
|        - |  1271 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1272 | ` * start compiling the target PHP program.` |
|        - |  1273 | ` */` |
|     2602 |  1274 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1275 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1276 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1277 | `	 )` |
|        2 |  1278 |  |
|        - |  1279 | `	SyString sBuiltin;` |
|        - |  1280 | `	ph7_value *pObj;` |
|        - |  1281 | `	sxi32 rc;` |
|        - |  1282 | `	/* Zero the structure */` |
|     2604 |  1283 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1284 | `	/* Initialize VM fields */` |
|     2604 |  1285 | `	pVm->pEngine = &(*pEngine);` |
|     2604 |  1286 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1287 | `	/* Instructions containers */` |
|     2604 |  1288 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2604 |  1289 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2604 |  1290 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1291 | `	/* Object containers */` |
|     2604 |  1292 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2604 |  1293 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1294 | `	/* Virtual machine internal containers */` |
|     2604 |  1295 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2604 |  1296 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2604 |  1297 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2604 |  1298 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2604 |  1299 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2604 |  1300 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2604 |  1301 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2604 |  1302 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2604 |  1303 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2604 |  1304 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2604 |  1305 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2604 |  1306 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2604 |  1307 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2604 |  1308 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2604 |  1309 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2604 |  1310 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2604 |  1311 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2604 |  1312 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2604 |  1313 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2604 |  1314 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2604 |  1315 | `	pVm->pPendingException = 0;` |
|        - |  1316 | `	/* Configuration containers */` |
|     2604 |  1317 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2604 |  1318 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2604 |  1319 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2604 |  1320 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2604 |  1321 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2604 |  1322 | `	pVm->iResponseStatus = 200;` |
|     2604 |  1323 | `	pVm->bHeadersSent = 0;` |
|     2604 |  1324 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1325 | `	/* Error callbacks containers */` |
|     2604 |  1326 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2604 |  1327 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2604 |  1328 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2604 |  1329 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2604 |  1330 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1331 | `	/* Set a default recursion limit */` |
|        - |  1332 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2604 |  1333 | `	pVm->nMaxDepth = 32;` |
|        - |  1334 | `#else` |
|        - |  1335 | `	pVm->nMaxDepth = 16;` |
|        - |  1336 | `#endif` |
|        - |  1337 | `	/* Default assertion flags */` |
|     2604 |  1338 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1339 | `	/* JSON return status */` |
|     2604 |  1340 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1341 | `	/* PRNG context */` |
|     2604 |  1342 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1343 | `	/* Install the null constant */` |
|     2604 |  1344 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2604 |  1345 | `	if( pObj == 0 ){` |
|      ! 0 |  1346 | `		rc = SXERR_MEM;` |
|      ! 0 |  1347 | `		goto Err;` |
|        - |  1348 | `	}` |
|     2604 |  1349 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1350 | `	/* Install the boolean TRUE constant */` |
|     2604 |  1351 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2604 |  1352 | `	if( pObj == 0 ){` |
|      ! 0 |  1353 | `		rc = SXERR_MEM;` |
|      ! 0 |  1354 | `		goto Err;` |
|        - |  1355 | `	}` |
|     2604 |  1356 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1357 | `	/* Install the boolean FALSE constant */` |
|     2604 |  1358 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2604 |  1359 | `	if( pObj == 0 ){` |
|      ! 0 |  1360 | `		rc = SXERR_MEM;` |
|      ! 0 |  1361 | `		goto Err;` |
|        - |  1362 | `	}` |
|     2604 |  1363 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1364 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1365 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1366 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2604 |  1367 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2604 |  1368 | `	if( pObj == 0 ){` |
|      ! 0 |  1369 | `		rc = SXERR_MEM;` |
|      ! 0 |  1370 | `		goto Err;` |
|        - |  1371 | `	}` |
|     2604 |  1372 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1373 | `	/* Create the global frame */` |
|     2604 |  1374 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2604 |  1375 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1376 | `		goto Err;` |
|        - |  1377 | `	}` |
|        - |  1378 | `	/* Initialize the code generator */` |
|     2604 |  1379 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2604 |  1380 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1381 | `		goto Err;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* VM correctly initialized,set the magic number */` |
|     2604 |  1384 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2604 |  1385 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1386 | `	/* Compile the built-in library */` |
|     2604 |  1387 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1388 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2604 |  1389 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1390 | `	/* Register Fiber internal C functions */` |
|     2604 |  1391 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2604 |  1392 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2604 |  1393 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2604 |  1394 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2604 |  1395 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2604 |  1396 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2604 |  1397 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2604 |  1398 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2604 |  1399 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2604 |  1400 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1401 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2604 |  1402 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2604 |  1403 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2604 |  1404 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2604 |  1405 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2604 |  1406 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2604 |  1407 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2604 |  1408 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2604 |  1409 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2604 |  1410 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2604 |  1411 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1412 | `	/* Reset the code generator */` |
|     2604 |  1413 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2604 |  1414 | `	return SXRET_OK;` |
|      ! 0 |  1415 | `Err:` |
|      ! 0 |  1416 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1417 | `	return rc;` |
|     1303 |  1418 |  |
|        - |  1419 | `/*` |
|        - |  1420 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1421 | ` * routine which store the output in an internal blob.` |
|        - |  1422 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1423 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1424 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1425 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1426 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1427 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1428 | ` * to finish executing and extracting the output.` |
|        - |  1429 | ` */` |
|       38 |  1430 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1431 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1432 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1433 | `	void *pUserData     /* User private data */` |
|        - |  1434 | `	)` |
|      ! 0 |  1435 |  |
|        - |  1436 | `	 sxi32 rc;` |
|        - |  1437 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1438 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1439 | `	 return rc;` |
|      ! 0 |  1440 |  |
|        - |  1441 | `/*` |
|        - |  1442 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1443 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1444 | ` */` |
|    14226 |  1445 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1446 |  |
|    14228 |  1447 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    14228 |  1448 | `	if( xCons != VmObConsumer ){` |
|     6322 |  1449 | `		pVm->nOutputLen += nLen;` |
|     6322 |  1450 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      830 |  1451 | `			pVm->bHeadersSent = 1;` |
|      414 |  1452 | `		}` |
|     3160 |  1453 | `	}` |
|    14228 |  1454 |  |
|        - |  1455 | `#define VM_STACK_GUARD 16` |
|        - |  1456 | `/*` |
|        - |  1457 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1458 | ` * our compiled PHP program.` |
|        - |  1459 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1460 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1461 | ` */` |
|    33192 |  1462 | `static ph7_value * VmNewOperandStack(` |
|        - |  1463 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1464 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1465 | `	)` |
|        2 |  1466 |  |
|        - |  1467 | `	ph7_value *pStack;` |
|        - |  1468 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1469 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1470 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1471 | `  ** on the maximum stack depth required.` |
|        - |  1472 | `  **` |
|        - |  1473 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1474 | `  */` |
|    33194 |  1475 | `	nInstr += VM_STACK_GUARD;` |
|    33194 |  1476 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    33194 |  1477 | `	if( pStack == 0 ){` |
|      ! 0 |  1478 | `		return 0;` |
|        - |  1479 | `	}` |
|        - |  1480 | `	/* Initialize the operand stack */` |
|  2078404 |  1481 | `	while( nInstr > 0 ){` |
|  2045212 |  1482 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2045212 |  1483 | `		--nInstr;` |
|        2 |  1484 | `	}` |
|        - |  1485 | `	/* Ready for bytecode execution */` |
|    33194 |  1486 | `	return pStack;` |
|    16598 |  1487 |  |
|        - |  1488 | `/* Forward declaration */` |
|        - |  1489 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1490 | `/*` |
|        - |  1491 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1492 | ` * This routine gets called by the PH7 engine after` |
|        - |  1493 | ` * successful compilation of the target PHP program.` |
|        - |  1494 | ` */` |
|     2340 |  1495 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1496 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1497 | `	)` |
|        2 |  1498 |  |
|        - |  1499 | `	SyHashEntry *pEntry;` |
|        - |  1500 | `	sxi32 rc;` |
|     2342 |  1501 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1502 | `		/* Initialize your VM first */` |
|      ! 0 |  1503 | `		return SXERR_CORRUPT;` |
|        - |  1504 | `	}` |
|        - |  1505 | `	/* Mark the VM ready for byte-code execution */` |
|     2342 |  1506 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1507 | `	/* Release the code generator now we have compiled our program */` |
|     2342 |  1508 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1509 | `	/* Emit the DONE instruction */` |
|     2342 |  1510 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2342 |  1511 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1512 | `		return SXERR_MEM;` |
|        - |  1513 | `	}` |
|        - |  1514 | `	/* Script return value */` |
|     2342 |  1515 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1516 | `	/* Allocate a new operand stack */` |
|     2342 |  1517 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2342 |  1518 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1519 | `		return SXERR_MEM;` |
|        - |  1520 | `	}` |
|        - |  1521 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1522 | `	 * private data. */` |
|     2342 |  1523 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2342 |  1524 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1525 | `	/* Allocate the reference table */` |
|     2342 |  1526 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2342 |  1527 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2342 |  1528 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1529 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1530 | `		return SXERR_MEM;` |
|        - |  1531 | `	}` |
|        - |  1532 | `	/* Zero the reference table */` |
|     2342 |  1533 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1534 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2342 |  1535 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2342 |  1536 | `	if( rc != SXRET_OK ){` |
|        - |  1537 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1538 | `		return rc;` |
|        - |  1539 | `	}` |
|        - |  1540 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2342 |  1541 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2342 |  1542 | `	if( rc != SXRET_OK ){` |
|        - |  1543 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1544 | `		return rc;` |
|        - |  1545 | `	}` |
|        - |  1546 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2342 |  1547 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1548 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2342 |  1549 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1550 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2342 |  1551 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1552 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1553 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2342 |  1554 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2342 |  1555 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1556 | `#endif` |
|        - |  1557 | `	/* Initialize and install static and constants class attributes */` |
|     2342 |  1558 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    42298 |  1559 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    39958 |  1560 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    39958 |  1561 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1562 | `			return rc;` |
|        - |  1563 | `		}` |
|        2 |  1564 | `	}` |
|        - |  1565 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2342 |  1566 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1567 | `	/* VM is ready for bytecode execution */` |
|     2342 |  1568 | `	return SXRET_OK;` |
|     1172 |  1569 |  |
|        - |  1570 | `/*` |
|        - |  1571 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1572 | ` */` |
|      ! 0 |  1573 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1574 |  |
|      ! 0 |  1575 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1576 | `		return SXERR_CORRUPT;` |
|        - |  1577 | `	}` |
|        - |  1578 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1579 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1580 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1581 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1582 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1583 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1584 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1585 | `	pVm->bHttpContext = 0;` |
|        - |  1586 | `	/* Set the ready flag */` |
|      ! 0 |  1587 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1588 | `	return SXRET_OK;` |
|      ! 0 |  1589 |  |
|        - |  1590 | `/*` |
|        - |  1591 | ` * Release a Virtual Machine.` |
|        - |  1592 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1593 | ` */` |
|     2332 |  1594 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1595 |  |
|        - |  1596 | `	/* Set the stale magic number */` |
|     2334 |  1597 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1598 | `	/* Release the private memory subsystem */` |
|     2334 |  1599 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2334 |  1600 | `	return SXRET_OK;` |
|        2 |  1601 |  |
|        - |  1602 | `/*` |
|        - |  1603 | ` * Initialize a foreign function call context.` |
|        - |  1604 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1605 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1606 | ` * functions.` |
|        - |  1607 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1608 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1609 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1610 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1611 | ` */` |
|   585736 |  1612 | `static sxi32 VmInitCallContext(` |
|        - |  1613 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1614 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1615 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1616 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1617 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1618 | `	)` |
|        2 |  1619 |  |
|   585738 |  1620 | `	pOut->pFunc = pFunc;` |
|   585738 |  1621 | `	pOut->pVm   = pVm;` |
|   585738 |  1622 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   585738 |  1623 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1624 | `	/* Assume a null return value */` |
|   585738 |  1625 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   585738 |  1626 | `	pOut->pRet = pRet;` |
|   585738 |  1627 | `	pOut->iFlags = iFlags;` |
|   585738 |  1628 | `	return SXRET_OK;` |
|        2 |  1629 |  |
|        - |  1630 | `/*` |
|        - |  1631 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1632 | ` * left behind.` |
|        - |  1633 | ` */` |
|   585736 |  1634 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1635 |  |
|        - |  1636 | `	sxu32 n;` |
|   585738 |  1637 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7122 |  1638 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    20332 |  1639 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13212 |  1640 | `			if( apObj[n] == 0 ){` |
|        - |  1641 | `				/* Already released */` |
|      298 |  1642 | `				continue;` |
|        - |  1643 | `			}` |
|    12916 |  1644 | `			PH7_MemObjRelease(apObj[n]);` |
|    12916 |  1645 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6459 |  1646 | `		}` |
|     7122 |  1647 | `		SySetRelease(&pCtx->sVar);` |
|     3560 |  1648 | `	}` |
|   585738 |  1649 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1650 | `		ph7_aux_data *aAux;` |
|        - |  1651 | `		void *pChunk;` |
|        - |  1652 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1653 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1654 | `		 */` |
|        9 |  1655 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1656 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1657 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1658 | `			/* Release the chunk */` |
|       25 |  1659 | `			if( pChunk ){` |
|       25 |  1660 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1661 | `			}` |
|       13 |  1662 | `		}` |
|        9 |  1663 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1664 | `	}` |
|   585738 |  1665 |  |
|        - |  1666 | `/*` |
|        - |  1667 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1668 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1669 | ` */` |
|      296 |  1670 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1671 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1672 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1673 | `	)` |
|        2 |  1674 |  |
|      298 |  1675 | `	if( pValue == 0 ){` |
|        - |  1676 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1677 | `		return;` |
|        - |  1678 | `	}` |
|      298 |  1679 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1680 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1681 | `		sxu32 n;` |
|     1054 |  1682 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1683 | `			if( apObj[n] == pValue ){` |
|      298 |  1684 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1685 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1686 | `				/* Mark as released */` |
|      298 |  1687 | `				apObj[n] = 0;` |
|      298 |  1688 | `				break;` |
|        - |  1689 | `			}` |
|      380 |  1690 | `		}` |
|      148 |  1691 | `	}` |
|      150 |  1692 |  |
|        - |  1693 | `/*` |
|        - |  1694 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1695 | ` */` |
|  3382838 |  1696 | `static void VmPopOperand(` |
|        - |  1697 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1698 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1699 | `	)` |
|        2 |  1700 |  |
|  3382840 |  1701 | `	ph7_value *pTos = *ppTos;` |
|  7192594 |  1702 | `	while( nPop > 0 ){` |
|  3809756 |  1703 | `		PH7_MemObjRelease(pTos);` |
|  3809756 |  1704 | `		pTos--;` |
|  3809756 |  1705 | `		nPop--;` |
|        2 |  1706 | `	}` |
|        - |  1707 | `	/* Top of the stack */` |
|  3382840 |  1708 | `	*ppTos = pTos;` |
|  3382840 |  1709 |  |
|        - |  1710 | `/*` |
|        - |  1711 | ` * Reserve a memory object.` |
|        - |  1712 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1713 | ` */` |
|  3070794 |  1714 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1715 |  |
|  3070796 |  1716 | `	ph7_value *pObj = 0;` |
|        - |  1717 | `	VmSlot *pSlot;` |
|        - |  1718 | `	sxu32 nIdx;` |
|        - |  1719 | `	/* Check for a free slot */` |
|  3070796 |  1720 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3070796 |  1721 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3070796 |  1722 | `	if( pSlot ){` |
|   928288 |  1723 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   928288 |  1724 | `		nIdx = pSlot->nIdx;` |
|   464143 |  1725 | `	}` |
|  3070796 |  1726 | `	if( pObj == 0 ){` |
|        - |  1727 | `		/* Reserve a new memory object */` |
|  2142510 |  1728 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2142510 |  1729 | `		if( pObj == 0 ){` |
|      ! 0 |  1730 | `			return 0;` |
|        - |  1731 | `		}` |
|  1071254 |  1732 | `	}` |
|        - |  1733 | `	/* Set a null default value */` |
|  3070796 |  1734 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3070796 |  1735 | `	pObj->nIdx = nIdx;` |
|  3070796 |  1736 | `	return pObj;` |
|  1535399 |  1737 |  |
|        - |  1738 | `/*` |
|        - |  1739 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1740 | ` */` |
|    30318 |  1741 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1742 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1743 | `	const char *zKey,  /* Entry key */` |
|        - |  1744 | `	sxu32 nByte,       /* Key length */` |
|        - |  1745 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1746 | `	)` |
|        2 |  1747 |  |
|        - |  1748 | `	ph7_value sKey;` |
|        - |  1749 | `	sxi32 rc;` |
|    30320 |  1750 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    30320 |  1751 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1752 | `	/* Perform the insertion */` |
|    30320 |  1753 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    30320 |  1754 | `	PH7_MemObjRelease(&sKey);` |
|    30320 |  1755 | `	return rc;` |
|        2 |  1756 |  |
|        - |  1757 | `/*` |
|        - |  1758 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1759 | ` * Return a pointer to the variable value on success.` |
|        - |  1760 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1761 | ` */` |
|  3150668 |  1762 | `static ph7_value * VmExtractMemObj(` |
|        - |  1763 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1764 | `	const SyString *pName, /* Variable name */` |
|        - |  1765 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1766 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1767 | `	)` |
|        2 |  1768 |  |
|  3150670 |  1769 | `	int bNullify = FALSE;` |
|        - |  1770 | `	SyHashEntry *pEntry;` |
|        - |  1771 | `	VmFrame *pFrame;` |
|        - |  1772 | `	ph7_value *pObj;` |
|        - |  1773 | `	sxu32 nIdx;` |
|        - |  1774 | `	sxi32 rc;` |
|        - |  1775 | `	/* Point to the top active frame */` |
|  3150670 |  1776 | `	pFrame = pVm->pFrame;` |
|  3150670 |  1777 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1778 | `	/* Perform the lookup */` |
|  3150670 |  1779 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1780 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1781 | `		pName = &sAnnon;` |
|        - |  1782 | `		/* Always nullify the object */` |
|      ! 0 |  1783 | `		bNullify = TRUE;` |
|      ! 0 |  1784 | `		bDup = FALSE;` |
|      ! 0 |  1785 | `	}` |
|        - |  1786 | `	/* Check the superglobals table first */` |
|  3150670 |  1787 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3150670 |  1788 | `	if( pEntry == 0 ){` |
|        - |  1789 | `		/* Query the top active frame */` |
|  3150630 |  1790 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3150630 |  1791 | `		if( pEntry == 0 ){` |
|    87174 |  1792 | `			char *zName = (char *)pName->zString;` |
|        - |  1793 | `			VmSlot sLocal;` |
|    87174 |  1794 | `			if( !bCreate ){` |
|        - |  1795 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1796 | `				return 0;` |
|        - |  1797 | `			}` |
|        - |  1798 | `			/* No such variable,automatically create a new one and install` |
|        - |  1799 | `			 * it in the current frame.` |
|        - |  1800 | `			 */` |
|    87138 |  1801 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    87138 |  1802 | `			if( pObj == 0 ){` |
|      ! 0 |  1803 | `				return 0;` |
|        - |  1804 | `			}` |
|    87138 |  1805 | `			nIdx = pObj->nIdx;` |
|    87138 |  1806 | `			if( bDup ){` |
|        - |  1807 | `				/* Duplicate name */` |
|      168 |  1808 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1809 | `				if( zName == 0 ){` |
|      ! 0 |  1810 | `					return 0;` |
|        - |  1811 | `				}` |
|       83 |  1812 | `			}` |
|        - |  1813 | `			/* Link to the top active VM frame */` |
|    87138 |  1814 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    87138 |  1815 | `			if( rc != SXRET_OK ){` |
|        - |  1816 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1817 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1818 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1819 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1820 | `				return 0;` |
|        - |  1821 | `			}` |
|    87138 |  1822 | `			if( pFrame->pParent != 0 ){` |
|        - |  1823 | `				/* Local variable */` |
|    80246 |  1824 | `				sLocal.nIdx = nIdx;` |
|    80246 |  1825 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    40124 |  1826 | `			}else{` |
|        - |  1827 | `				/* Register in the $GLOBALS array */` |
|     6894 |  1828 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1829 | `			}` |
|        - |  1830 | `			/* Install in the reference table */` |
|    87138 |  1831 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1832 | `			/* Save object index */` |
|    87138 |  1833 | `			pObj->nIdx = nIdx;` |
|    43570 |  1834 | `		}else{` |
|        - |  1835 | `			/* Extract variable contents */` |
|  3063458 |  1836 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3063458 |  1837 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3063458 |  1838 | `			if( bNullify && pObj ){` |
|      ! 0 |  1839 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1840 | `			}` |
|        - |  1841 | `		}` |
|  1575408 |  1842 | `	}else{` |
|        - |  1843 | `		/* Superglobal */` |
|       42 |  1844 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1845 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1846 | `	}` |
|  3150634 |  1847 | `	return pObj;` |
|  1575446 |  1848 |  |
|        - |  1849 | `/*` |
|        - |  1850 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1851 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1852 | ` */` |
|     2644 |  1853 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1854 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1855 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1856 | `	sxu32 nByte        /* zName length */` |
|        - |  1857 | `	)` |
|        2 |  1858 |  |
|        - |  1859 | `	SyHashEntry *pEntry;` |
|        - |  1860 | `	ph7_value *pValue;` |
|        - |  1861 | `	sxu32 nIdx;` |
|        - |  1862 | `	/* Query the superglobal table */` |
|     2646 |  1863 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2646 |  1864 | `	if( pEntry == 0 ){` |
|        - |  1865 | `		/* No such entry */` |
|      ! 0 |  1866 | `		return 0;` |
|        - |  1867 | `	}` |
|        - |  1868 | `	/* Extract the superglobal index in the global object pool */` |
|     2646 |  1869 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1870 | `	/* Extract the variable value  */` |
|     2646 |  1871 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2646 |  1872 | `	return pValue;` |
|     1324 |  1873 |  |
|        - |  1874 | `/*` |
|        - |  1875 | ` * Perform a raw hashmap insertion.` |
|        - |  1876 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1877 | ` */` |
|     2674 |  1878 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1879 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1880 | `	const char *zKey,   /* Entry key */` |
|        - |  1881 | `	int nKeylen,        /* zKey length*/` |
|        - |  1882 | `	const char *zData,  /* Entry data */` |
|        - |  1883 | `	int nLen            /* zData length */` |
|        - |  1884 | `	)` |
|        2 |  1885 |  |
|        - |  1886 | `	ph7_value sKey,sValue;` |
|        - |  1887 | `	sxi32 rc;` |
|     2676 |  1888 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2676 |  1889 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2676 |  1890 | `	if( zKey ){` |
|     2654 |  1891 | `		if( nKeylen < 0 ){` |
|     2602 |  1892 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1300 |  1893 | `		}` |
|     2654 |  1894 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1326 |  1895 | `	}` |
|     2676 |  1896 | `	if( zData ){` |
|     2676 |  1897 | `		if( nLen < 0 ){` |
|        - |  1898 | `			/* Compute length automatically */` |
|      144 |  1899 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1900 | `		}` |
|     2676 |  1901 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1337 |  1902 | `	}` |
|        - |  1903 | `	/* Perform the insertion */` |
|     2676 |  1904 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2676 |  1905 | `	PH7_MemObjRelease(&sKey);` |
|     2676 |  1906 | `	PH7_MemObjRelease(&sValue);` |
|     2676 |  1907 | `	return rc;` |
|        2 |  1908 |  |
|        - |  1909 | `/*` |
|        - |  1910 | ` * Configure a working virtual machine instance.` |
|        - |  1911 | ` *` |
|        - |  1912 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1913 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1914 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1915 | ` * The second argument to this function is an integer configuration option` |
|        - |  1916 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1917 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1918 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1919 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1920 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1921 | ` */` |
|    37770 |  1922 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1923 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1924 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1925 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1926 | `	)` |
|        2 |  1927 |  |
|    37772 |  1928 | `	sxi32 rc = SXRET_OK;` |
|    37772 |  1929 | `	switch(nOp){` |
|     1162 |  1930 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2326 |  1931 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2326 |  1932 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1933 | `		/* VM output consumer callback */` |
|        - |  1934 | `#ifdef UNTRUST` |
|        - |  1935 | `		if( xConsumer == 0 ){` |
|        - |  1936 | `			rc = SXERR_CORRUPT;` |
|        - |  1937 | `			break;` |
|        - |  1938 | `		}` |
|        - |  1939 | `#endif` |
|        - |  1940 | `		/* Install the output consumer */` |
|     2326 |  1941 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2326 |  1942 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2326 |  1943 | `		break;` |
|        - |  1944 | `							   }` |
|     1170 |  1945 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1946 | `		/* Import path */` |
|        - |  1947 | `		  const char *zPath;` |
|        - |  1948 | `		  SyString sPath;` |
|     2342 |  1949 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1950 | `#if defined(UNTRUST)` |
|        - |  1951 | `		  if( zPath == 0 ){` |
|        - |  1952 | `			  rc = SXERR_EMPTY;` |
|        - |  1953 | `			  break;` |
|        - |  1954 | `		  }` |
|        - |  1955 | `#endif` |
|     2342 |  1956 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1957 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1958 | `#ifdef __WINNT__` |
|        2 |  1959 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1960 | `#endif` |
|     4682 |  1961 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1962 | `		  /* Remove leading and trailing white spaces */` |
|     2342 |  1963 | `		  SyStringFullTrim(&sPath);` |
|     2342 |  1964 | `		  if( sPath.nByte > 0 ){` |
|        - |  1965 | `			  /* Store the path in the corresponding conatiner */` |
|     2342 |  1966 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1170 |  1967 | `		  }` |
|     2342 |  1968 | `		  break;` |
|        - |  1969 | `									 }` |
|     1170 |  1970 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1971 | `		/* Run-Time Error report */` |
|     2342 |  1972 | `		pVm->bErrReport = 1;` |
|     2342 |  1973 | `		break;` |
|      ! 0 |  1974 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1975 | `		/* Recursion depth */` |
|      ! 0 |  1976 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1977 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1978 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1979 | `		}` |
|      ! 0 |  1980 | `		break;` |
|        - |  1981 | `									   }` |
|      ! 0 |  1982 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1983 | `		/* VM output length in bytes */` |
|      ! 0 |  1984 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1985 | `#ifdef UNTRUST` |
|        - |  1986 | `		if( pOut == 0 ){` |
|        - |  1987 | `			rc = SXERR_CORRUPT;` |
|        - |  1988 | `			break;` |
|        - |  1989 | `		}` |
|        - |  1990 | `#endif` |
|      ! 0 |  1991 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1992 | `		break;` |
|        - |  1993 | `							   }` |
|        - |  1994 |  |
|    11700 |  1995 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1996 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1997 | `		/* Create a new superglobal/global variable */` |
|    23402 |  1998 | `		const char *zName = va_arg(ap,const char *);` |
|    23402 |  1999 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2000 | `		SyHashEntry *pEntry;` |
|        - |  2001 | `		ph7_value *pObj;` |
|        - |  2002 | `		sxu32 nByte;` |
|        - |  2003 | `		sxu32 nIdx;` |
|        - |  2004 | `#ifdef UNTRUST` |
|        - |  2005 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2006 | `			rc = SXERR_CORRUPT;` |
|        - |  2007 | `			break;` |
|        - |  2008 | `		}` |
|        - |  2009 | `#endif` |
|    23402 |  2010 | `		nByte = SyStrlen(zName);` |
|    23402 |  2011 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2012 | `			/* Check if the superglobal is already installed */` |
|    23402 |  2013 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11702 |  2014 | `		}else{` |
|        - |  2015 | `			/* Query the top active VM frame */` |
|      ! 0 |  2016 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2017 | `		}` |
|    23402 |  2018 | `		if( pEntry ){` |
|        - |  2019 | `			/* Variable already installed */` |
|      ! 0 |  2020 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2021 | `			/* Extract contents */` |
|      ! 0 |  2022 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2023 | `			if( pObj ){` |
|        - |  2024 | `				/* Overwrite old contents */` |
|      ! 0 |  2025 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2026 | `			}` |
|      ! 0 |  2027 | `		}else{` |
|        - |  2028 | `			/* Install a new variable */` |
|    23402 |  2029 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    23402 |  2030 | `			if( pObj == 0 ){` |
|      ! 0 |  2031 | `				rc = SXERR_MEM;` |
|      ! 0 |  2032 | `				break;` |
|        - |  2033 | `			}` |
|    23402 |  2034 | `			nIdx = pObj->nIdx;` |
|        - |  2035 | `			/* Copy value */` |
|    23402 |  2036 | `			PH7_MemObjStore(pValue,pObj);` |
|    23402 |  2037 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2038 | `				/* Install the superglobal */` |
|    23402 |  2039 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11702 |  2040 | `			}else{` |
|        - |  2041 | `				/* Install in the current frame */` |
|      ! 0 |  2042 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2043 | `			}` |
|    23402 |  2044 | `			if( rc == SXRET_OK ){` |
|        - |  2045 | `				SyHashEntry *pRef;` |
|    23402 |  2046 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    23402 |  2047 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11702 |  2048 | `				}else{` |
|      ! 0 |  2049 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2050 | `				}` |
|        - |  2051 | `				/* Install in the reference table */` |
|    23402 |  2052 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    23402 |  2053 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2054 | `					/* Register in the $GLOBALS array */` |
|    23402 |  2055 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11700 |  2056 | `				}` |
|    11700 |  2057 | `			}` |
|        - |  2058 | `		}` |
|    23402 |  2059 | `		break;` |
|        - |  2060 | `									}` |
|     1300 |  2061 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2062 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2063 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2064 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2065 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2066 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2067 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2602 |  2068 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2602 |  2069 | `		const char *zValue = va_arg(ap,const char *);` |
|     2602 |  2070 | `		int nLen = va_arg(ap,int);` |
|        - |  2071 | `		ph7_hashmap *pMap;` |
|        - |  2072 | `		ph7_value *pValue;` |
|     2602 |  2073 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2074 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2075 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2601 |  2076 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2077 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2078 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2600 |  2079 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2080 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2081 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2600 |  2082 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2083 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2084 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2600 |  2085 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2086 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2087 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2600 |  2088 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2089 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2090 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2091 | `		}else{` |
|        - |  2092 | `			/* Extract the $_SERVER superglobal */` |
|     2600 |  2093 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2094 | `		}` |
|     2602 |  2095 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2096 | `			/* No such entry */` |
|      ! 0 |  2097 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2098 | `			break;` |
|        - |  2099 | `		}` |
|        - |  2100 | `		/* Point to the hashmap */` |
|     2602 |  2101 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2102 | `		/* Perform the insertion */` |
|     2602 |  2103 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2602 |  2104 | `		break;` |
|        - |  2105 | `								   }` |
|       11 |  2106 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2107 | `		/* Script arguments */` |
|       24 |  2108 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2109 | `		ph7_hashmap *pMap;` |
|        - |  2110 | `		ph7_value *pValue;` |
|        - |  2111 | `		sxu32 n;` |
|       24 |  2112 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2113 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2114 | `			break;` |
|        - |  2115 | `		}` |
|        - |  2116 | `		/* Extract the $argv array */` |
|       24 |  2117 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2118 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2119 | `			/* No such entry */` |
|      ! 0 |  2120 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2121 | `			break;` |
|        - |  2122 | `		}` |
|        - |  2123 | `		/* Point to the hashmap */` |
|       24 |  2124 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2125 | `		/* Perform the insertion */` |
|       24 |  2126 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2127 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2128 | `		if( rc == SXRET_OK ){` |
|       24 |  2129 | `			if( pMap->nEntry > 1 ){` |
|        - |  2130 | `				/* Append space separator first */` |
|       18 |  2131 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2132 | `			}` |
|       24 |  2133 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2134 | `		}` |
|       24 |  2135 | `		break;` |
|        - |  2136 | `								  }` |
|      ! 0 |  2137 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2138 | `		/* error_log() consumer */` |
|      ! 0 |  2139 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2140 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2141 | `		break;` |
|        - |  2142 | `										}` |
|      ! 0 |  2143 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2144 | `		/* Script return value */` |
|      ! 0 |  2145 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2146 | `#ifdef UNTRUST` |
|        - |  2147 | `		if( ppValue == 0 ){` |
|        - |  2148 | `			rc = SXERR_CORRUPT;` |
|        - |  2149 | `			break;` |
|        - |  2150 | `		}` |
|        - |  2151 | `#endif` |
|      ! 0 |  2152 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2153 | `		break;` |
|        - |  2154 | `								   }` |
|     2340 |  2155 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2156 | `		/* Register an IO stream device */` |
|     4682 |  2157 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2158 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7020 |  2159 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4682 |  2160 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2161 | `				/* Invalid stream */` |
|      ! 0 |  2162 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2163 | `				break;` |
|        - |  2164 | `		}` |
|     4682 |  2165 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2166 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2342 |  2167 | `			pVm->pDefStream = pStream;` |
|     1170 |  2168 | `		}` |
|        - |  2169 | `		/* Insert in the appropriate container */` |
|     4682 |  2170 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4682 |  2171 | `		break;` |
|        - |  2172 | `								  }` |
|        8 |  2173 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2174 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2175 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2176 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2177 | `#ifdef UNTRUST` |
|        - |  2178 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2179 | `			rc = SXERR_CORRUPT;` |
|        - |  2180 | `			break;` |
|        - |  2181 | `		}` |
|        - |  2182 | `#endif` |
|       16 |  2183 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2184 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2185 | `		break;` |
|        - |  2186 | `									   }` |
|        8 |  2187 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2188 | `		/* Raw HTTP request*/` |
|       16 |  2189 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2190 | `		int nByte = va_arg(ap,int);` |
|       16 |  2191 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2192 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2193 | `			break;` |
|        - |  2194 | `		}` |
|       16 |  2195 | `		if( nByte < 0 ){` |
|        - |  2196 | `			/* Compute length automatically */` |
|      ! 0 |  2197 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2198 | `		}` |
|        - |  2199 | `		/* Process the request */` |
|       16 |  2200 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2201 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2202 | `		if( rc == SXRET_OK ){` |
|       16 |  2203 | `			pVm->bHttpContext = 1;` |
|        8 |  2204 | `		}` |
|       16 |  2205 | `		break;` |
|        - |  2206 | `									}` |
|        8 |  2207 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2208 | `		/* Extract HTTP response status code */` |
|       16 |  2209 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2210 | `		if( pStatus ){` |
|       16 |  2211 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2212 | `		}` |
|       16 |  2213 | `		break;` |
|        - |  2214 | `										}` |
|        8 |  2215 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2216 | `		/* Iterate response headers via callback */` |
|        - |  2217 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2218 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2219 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2220 | `		if( xCallback ){` |
|       16 |  2221 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2222 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2223 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2224 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2225 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2226 | `							   pUserData);` |
|       12 |  2227 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2228 | `					break;` |
|        - |  2229 | `				}` |
|        6 |  2230 | `			}` |
|        8 |  2231 | `		}` |
|       16 |  2232 | `		break;` |
|        - |  2233 | `										 }` |
|      ! 0 |  2234 | `	default:` |
|        - |  2235 | `		/* Unknown configuration option */` |
|      ! 0 |  2236 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2237 | `		break;` |
|        - |  2238 | `	}` |
|    37772 |  2239 | `	return rc;` |
|        2 |  2240 |  |
|        - |  2241 | `/* Forward declaration */` |
|        - |  2242 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2243 | `/*` |
|        - |  2244 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2245 | ` * format.` |
|        - |  2246 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2247 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2248 | ` * (STDOUT).` |
|        - |  2249 | ` */` |
|        2 |  2250 | `static sxi32 VmByteCodeDump(` |
|        - |  2251 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2252 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2253 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2254 | `	)` |
|        1 |  2255 |  |
|        - |  2256 | `	static const char zDump[] = {` |
|        - |  2257 | `		"====================================================\n"` |
|        - |  2258 | `		"PH7 VM Dump\n"` |
|        - |  2259 | `		"====================================================\n"` |
|        - |  2260 | `	};` |
|        - |  2261 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2262 | `	sxi32 rc = SXRET_OK;` |
|        - |  2263 | `	sxu32 n;` |
|        - |  2264 | `	/* Point to the PH7 instructions */` |
|        3 |  2265 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2266 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2267 | `	n = 0;` |
|        3 |  2268 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2269 | `	/* Dump instructions */` |
|        7 |  2270 | `	for(;;){` |
|       15 |  2271 | `		if( pInstr >= pEnd ){` |
|        - |  2272 | `			/* No more instructions */` |
|        3 |  2273 | `			break;` |
|        - |  2274 | `		}` |
|        - |  2275 | `		/* Format and call the consumer callback */` |
|       19 |  2276 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2277 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2278 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2279 | `		if( rc != SXRET_OK ){` |
|        - |  2280 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2281 | `			return rc;` |
|        - |  2282 | `		}` |
|       13 |  2283 | `		++n;` |
|       13 |  2284 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2285 | `	}` |
|        3 |  2286 | `	return rc;` |
|        2 |  2287 |  |
|        - |  2288 | `/* Forward declaration */` |
|        - |  2289 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2290 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2291 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2292 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2293 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2294 | `/*` |
|        - |  2295 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2296 | ` * consumer callback.` |
|        - |  2297 | ` */` |
|      554 |  2298 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2299 |  |
|      555 |  2300 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      555 |  2301 | `	sxi32 rc = SXRET_OK;` |
|        - |  2302 | `	/* Append a new line */` |
|        - |  2303 | `#ifdef __WINNT__` |
|        1 |  2304 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2305 | `#else` |
|      554 |  2306 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2307 | `#endif` |
|        - |  2308 | `	/* Invoke the output consumer callback */` |
|      555 |  2309 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      555 |  2310 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      555 |  2311 | `	return rc;` |
|        1 |  2312 |  |
|        - |  2313 | `/*` |
|        - |  2314 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2315 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2316 | ` * information.` |
|        - |  2317 | ` */` |
|      134 |  2318 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2319 |  |
|      136 |  2320 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2321 | `		ph7_value apArg[4];` |
|        - |  2322 | `		ph7_value *apArgPtr[4];` |
|        - |  2323 | `		ph7_value sResult;` |
|        - |  2324 | `		SyString sErr;` |
|        - |  2325 | `		/* Prepare arguments */` |
|       61 |  2326 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2327 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2328 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2329 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2330 | `		if( pFile ){` |
|       61 |  2331 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2332 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2333 | `		}else{` |
|      ! 0 |  2334 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2335 | `		}` |
|       61 |  2336 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2337 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2338 | `		/* Set up pointer array */` |
|       61 |  2339 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2340 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2341 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2342 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2343 | `		/* Call the handler */` |
|       61 |  2344 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2345 | `		/* Check return value */` |
|       61 |  2346 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2347 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2348 | `		}` |
|        - |  2349 | `		/* Release */` |
|       61 |  2350 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2351 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2352 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2353 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2354 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2355 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2356 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2357 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2358 | `	}` |
|        - |  2359 | `	/* No handler, always call error handler */` |
|       75 |  2360 | `	return TRUE;` |
|       69 |  2361 |  |
|       98 |  2362 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2363 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2364 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2365 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2366 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2367 | `	)` |
|        2 |  2368 |  |
|      100 |  2369 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2370 | `	SyString *pFile;` |
|        - |  2371 | `	char *zErr;` |
|      100 |  2372 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2373 | `	if( !pVm->bErrReport ){` |
|        - |  2374 | `		/* Don't bother reporting errors */` |
|        3 |  2375 | `		return SXRET_OK;` |
|        - |  2376 | `	}` |
|        - |  2377 | `	/* Reset the working buffer */` |
|       98 |  2378 | `	SyBlobReset(pWorker);` |
|        - |  2379 | `	/* Peek the processed file if available */` |
|       98 |  2380 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2381 | `	if( pFile ){` |
|        - |  2382 | `		/* Append file name */` |
|       98 |  2383 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2384 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2385 | `	}` |
|        - |  2386 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2387 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2388 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2389 | `	 * E_DEPRECATED). */` |
|       98 |  2390 | `	zErr = "Error:  ";` |
|       98 |  2391 | `	switch(iErr){` |
|       19 |  2392 | `	case PH7_CTX_WARNING:` |
|       40 |  2393 | `		zErr = "Warning:  ";` |
|       40 |  2394 | `		break;` |
|        6 |  2395 | `	case PH7_CTX_NOTICE:` |
|       14 |  2396 | `		zErr = "Notice:  ";` |
|       12 |  2397 | `		break;` |
|       23 |  2398 | `	default:` |
|        - |  2399 | `		/* keep iErr unchanged */` |
|       46 |  2400 | `		break;` |
|        - |  2401 | `	}` |
|       98 |  2402 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2403 | `	if( pFuncName ){` |
|        - |  2404 | `		/* Append function name first */` |
|       23 |  2405 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2406 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2407 | `	}` |
|       98 |  2408 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2409 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2410 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2411 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2412 | `	}` |
|       98 |  2413 | `	return rc;` |
|       51 |  2414 |  |
|        - |  2415 | `/*` |
|        - |  2416 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2417 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2418 | ` * information.` |
|        - |  2419 | ` */` |
|       38 |  2420 | `static sxi32 VmThrowErrorAp(` |
|        - |  2421 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2422 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2423 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2424 | `	const char *zFormat, /* Format message */` |
|        - |  2425 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2426 | `	)` |
|        2 |  2427 |  |
|       40 |  2428 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2429 | `	SyBlob sMsg;` |
|        - |  2430 | `	SyString *pFile;` |
|        - |  2431 | `	char *zErr;` |
|       40 |  2432 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2433 | `	if( !pVm->bErrReport ){` |
|        - |  2434 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2435 | `		return SXRET_OK;` |
|        - |  2436 | `	}` |
|        - |  2437 | `	/* Reset the working buffer */` |
|       40 |  2438 | `	SyBlobReset(pWorker);` |
|        - |  2439 | `	/* Peek the processed file if available */` |
|       40 |  2440 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2441 | `	if( pFile ){` |
|        - |  2442 | `		/* Append file name */` |
|       40 |  2443 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2444 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2445 | `	}` |
|        - |  2446 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2447 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2448 | `	 * the correct errno value. */` |
|       40 |  2449 | `	zErr = "Error:  ";` |
|       40 |  2450 | `	switch(iErr){` |
|        4 |  2451 | `	case PH7_CTX_WARNING:` |
|        9 |  2452 | `		zErr = "Warning:  ";` |
|        9 |  2453 | `		break;` |
|        3 |  2454 | `	case PH7_CTX_NOTICE:` |
|        7 |  2455 | `		zErr = "Notice:  ";` |
|        6 |  2456 | `		break;` |
|       12 |  2457 | `	default:` |
|        - |  2458 | `		/* do not change iErr */` |
|       24 |  2459 | `		break;` |
|        - |  2460 | `	}` |
|       40 |  2461 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2462 | `	if( pFuncName ){` |
|        - |  2463 | `		/* Append function name first */` |
|       26 |  2464 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2465 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2466 | `	}` |
|        - |  2467 | `	/* Format the raw message */` |
|       40 |  2468 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2469 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2470 | `	/* Check if a user error handler is installed */` |
|       40 |  2471 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2472 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2473 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2474 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2475 | `	}` |
|       40 |  2476 | `	SyBlobRelease(&sMsg);` |
|       40 |  2477 | `	return rc;` |
|       21 |  2478 |  |
|        - |  2479 | `/*` |
|        - |  2480 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2481 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2482 | ` * information.` |
|        - |  2483 | ` * ------------------------------------` |
|        - |  2484 | ` * Simple boring wrapper function.` |
|        - |  2485 | ` * ------------------------------------` |
|        - |  2486 | ` */` |
|       14 |  2487 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2488 |  |
|        - |  2489 | `	va_list ap;` |
|        - |  2490 | `	sxi32 rc;` |
|       15 |  2491 | `	va_start(ap,zFormat);` |
|       15 |  2492 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2493 | `	va_end(ap);` |
|       15 |  2494 | `	return rc;` |
|        1 |  2495 |  |
|        - |  2496 | `/*` |
|        - |  2497 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2498 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2499 | ` * information.` |
|        - |  2500 | ` * ------------------------------------` |
|        - |  2501 | ` * Simple boring wrapper function.` |
|        - |  2502 | ` * ------------------------------------` |
|        - |  2503 | ` */` |
|       24 |  2504 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2505 |  |
|        - |  2506 | `	sxi32 rc;` |
|       26 |  2507 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2508 | `	return rc;` |
|        2 |  2509 |  |
|        - |  2510 | `/*` |
|        - |  2511 | ` * Resolve function context from the current frame.` |
|        - |  2512 | ` */` |
|      950 |  2513 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2514 |  |
|        - |  2515 | `	VmFrame *pFrame;` |
|        - |  2516 | `	ph7_vm_func *pFunc;` |
|      951 |  2517 | `	*pzFuncName = 0;` |
|      951 |  2518 | `	*pnFuncLen = 0;` |
|      951 |  2519 | `	pFrame = pVm->pFrame;` |
|      951 |  2520 | `	if( pFrame == 0 ){` |
|      ! 0 |  2521 | `		return;` |
|        - |  2522 | `	}` |
|      951 |  2523 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      951 |  2524 | `	if( pFrame->pParent == 0 ){` |
|      945 |  2525 | `		return;` |
|        - |  2526 | `	}` |
|        7 |  2527 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2528 | `	if( pFunc == 0 ){` |
|      ! 0 |  2529 | `		return;` |
|        - |  2530 | `	}` |
|        7 |  2531 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2532 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      476 |  2533 |  |
|        - |  2534 | `/*` |
|        - |  2535 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2536 | ` */` |
|      478 |  2537 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2538 |  |
|        - |  2539 | `	SyBlob sOut;` |
|        - |  2540 | `	SyString *pFile;` |
|      479 |  2541 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2542 | `		return PH7_OK;` |
|        - |  2543 | `	}` |
|      479 |  2544 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2545 | `		zClass = "Exception";` |
|      ! 0 |  2546 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2547 | `	}` |
|      479 |  2548 | `	if( zMsg == 0 ){` |
|      ! 0 |  2549 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2550 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2551 | `	}` |
|      479 |  2552 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      473 |  2553 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      236 |  2554 | `	}` |
|      479 |  2555 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      479 |  2556 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      479 |  2557 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      479 |  2558 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      479 |  2559 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      479 |  2560 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      479 |  2561 | `	if( pFile ){` |
|      479 |  2562 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      479 |  2563 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      479 |  2564 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      239 |  2565 | `	}` |
|      479 |  2566 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      479 |  2567 | `	if( pFile ){` |
|      479 |  2568 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      479 |  2569 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      479 |  2570 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2571 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2572 | `		}else{` |
|      473 |  2573 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2574 | `		}` |
|      239 |  2575 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2576 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2577 | `	}else{` |
|      ! 0 |  2578 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2579 | `	}` |
|      479 |  2580 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      479 |  2581 | `	if( pFile ){` |
|      479 |  2582 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      479 |  2583 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      479 |  2584 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      479 |  2585 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      239 |  2586 | `	}` |
|      479 |  2587 | `	VmCallErrorHandler(pVm,&sOut);` |
|      479 |  2588 | `	SyBlobRelease(&sOut);` |
|      479 |  2589 | `	return PH7_ABORT;` |
|      240 |  2590 |  |
|        - |  2591 | `/*` |
|        - |  2592 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2593 | ` */` |
|      480 |  2594 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2595 |  |
|        - |  2596 | `	ph7_vm *pVm;` |
|        - |  2597 | `	ph7_class *pClass;` |
|        - |  2598 | `	ph7_class_instance *pThis;` |
|        - |  2599 | `	ph7_class_method *pCons;` |
|        - |  2600 | `	ph7_value sArg;` |
|        - |  2601 | `	ph7_value *apArg[1];` |
|        - |  2602 | `	SyBlob sMsg;` |
|        - |  2603 | `	SyString sMsgStr;` |
|        - |  2604 | `	VmFrame *pFrame;` |
|        - |  2605 | `	va_list ap;` |
|        - |  2606 | `	sxi32 rc;` |
|        - |  2607 |  |
|      482 |  2608 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2609 | `		return PH7_ABORT;` |
|        - |  2610 | `	}` |
|      482 |  2611 | `	pVm = pCtx->pVm;` |
|      482 |  2612 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2613 | `		zClass = "Error";` |
|      ! 0 |  2614 | `	}` |
|      482 |  2615 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      482 |  2616 | `	if( pClass == 0 ){` |
|      ! 0 |  2617 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2618 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2619 | `			zClass` |
|        - |  2620 | `			);` |
|        - |  2621 | `	}` |
|      482 |  2622 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      482 |  2623 | `	if( pThis == 0 ){` |
|      ! 0 |  2624 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2625 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2626 | `			);` |
|        - |  2627 | `	}` |
|        - |  2628 |  |
|      482 |  2629 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      482 |  2630 | `	va_start(ap,zFormat);` |
|      482 |  2631 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      482 |  2632 | `	va_end(ap);` |
|        - |  2633 |  |
|      482 |  2634 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      482 |  2635 | `	if( pCons ){` |
|      482 |  2636 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      482 |  2637 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      482 |  2638 | `		apArg[0] = &sArg;` |
|      482 |  2639 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      482 |  2640 | `		PH7_MemObjRelease(&sArg);` |
|      240 |  2641 | `	}` |
|      482 |  2642 | `	SyBlobRelease(&sMsg);` |
|        - |  2643 |  |
|      482 |  2644 | `	pFrame = pVm->pFrame;` |
|      482 |  2645 | `	if( pFrame ){` |
|      482 |  2646 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      482 |  2647 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      240 |  2648 | `	}` |
|      482 |  2649 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      482 |  2650 | `	PH7_ClassInstanceUnref(pThis);` |
|      482 |  2651 | `	if( rc == SXERR_ABORT ){` |
|      471 |  2652 | `		return PH7_ABORT;` |
|        - |  2653 | `	}` |
|       12 |  2654 | `	return PH7_EXCEPTION;` |
|      242 |  2655 |  |
|        - |  2656 | `/*` |
|        - |  2657 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2658 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2659 | ` */` |
|      ! 0 |  2660 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2661 |  |
|        - |  2662 | `	ph7_vm *pVm;` |
|        - |  2663 | `	SyBlob sMsg;` |
|      ! 0 |  2664 | `	const char *zFuncName = 0;` |
|      ! 0 |  2665 | `	int nFuncLen = 0;` |
|        - |  2666 | `	va_list ap;` |
|        - |  2667 | `	sxi32 rc;` |
|        - |  2668 |  |
|      ! 0 |  2669 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2670 | `		return PH7_OK;` |
|        - |  2671 | `	}` |
|      ! 0 |  2672 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2673 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2674 | `		zClass = "Error";` |
|      ! 0 |  2675 | `	}` |
|        - |  2676 |  |
|      ! 0 |  2677 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2678 |  |
|      ! 0 |  2679 | `	va_start(ap,zFormat);` |
|      ! 0 |  2680 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2681 | `	va_end(ap);` |
|        - |  2682 |  |
|      ! 0 |  2683 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2684 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2685 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2686 | `	}` |
|      ! 0 |  2687 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2688 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2689 | `	}` |
|      ! 0 |  2690 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2691 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2692 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2693 | `	return rc;` |
|      ! 0 |  2694 |  |
|        - |  2695 | `/*` |
|        - |  2696 | ` * Save the execution state of a fiber/generator context.` |
|        - |  2697 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  2698 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  2699 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  2700 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  2701 | ` * when VmByteCodeExec returns.` |
|        - |  2702 | ` */` |
|      132 |  2703 | `static sxi32 VmSuspendCtx(` |
|        - |  2704 | `	ph7_vm *pVm,` |
|        - |  2705 | `	ph7_exec_ctx *pCtx,` |
|        - |  2706 | `	sxi32 pc,` |
|        - |  2707 | `	sxi32 nTos` |
|        - |  2708 | `	)` |
|        2 |  2709 |  |
|       66 |  2710 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      134 |  2711 | `	pCtx->pc = pc;` |
|      134 |  2712 | `	pCtx->nTos = nTos;` |
|      134 |  2713 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      134 |  2714 | `	return PH7_SUSPEND;` |
|        2 |  2715 |  |
|        - |  2716 | `/*` |
|        - |  2717 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2718 | ` *` |
|        - |  2719 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2720 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2721 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2722 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2723 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2724 | ` * then the program execution is halted.` |
|        - |  2725 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2726 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2727 | ` * or to reset the VM to it's initial state.` |
|        - |  2728 | ` */` |
|    33278 |  2729 | `static sxi32 VmByteCodeExec(` |
|        - |  2730 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2731 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2732 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2733 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2734 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2735 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2736 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  2737 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  2738 | `	)` |
|        2 |  2739 |  |
|        - |  2740 | `	VmInstr *pInstr;` |
|        - |  2741 | `	ph7_value *pTos;` |
|        - |  2742 | `	SySet aArg;` |
|        - |  2743 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2744 | `	sxi32 pc;` |
|        - |  2745 | `	sxi32 rc;` |
|        - |  2746 | `	/* Argument container */` |
|    33280 |  2747 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    33280 |  2748 | `	if( nTos < 0 ){` |
|    31214 |  2749 | `		pTos = &pStack[-1];` |
|    15608 |  2750 | `	}else{` |
|     2068 |  2751 | `		pTos = &pStack[nTos];` |
|        - |  2752 | `	}` |
|    33280 |  2753 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    33280 |  2754 | `	pc = nPc;` |
|        - |  2755 | `	/* Execute as much as we can */` |
|  5062125 |  2756 | `	for(;;){` |
|        - |  2757 | `		/* Fetch the instruction to execute */` |
| 10123548 |  2758 | `		pInstr = &aInstr[pc];` |
| 10123548 |  2759 | `		rc = SXRET_OK;` |
|        - |  2760 | `/*` |
|        - |  2761 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2762 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2763 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2764 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2765 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2766 | ` */` |
| 10123548 |  2767 | `		switch(pInstr->iOp){` |
|        - |  2768 | `/*` |
|        - |  2769 | ` * DONE: P1 * *` |
|        - |  2770 | ` *` |
|        - |  2771 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2772 | ` * and return immediately.` |
|        - |  2773 | ` */` |
|    16324 |  2774 | `case PH7_OP_DONE:` |
|    32650 |  2775 | `	if( pInstr->iP1 ){` |
|        - |  2776 | `#ifdef UNTRUST` |
|        - |  2777 | `		if( pTos < pStack ){` |
|        - |  2778 | `			goto Abort;` |
|        - |  2779 | `		}` |
|        - |  2780 | `#endif` |
|    18930 |  2781 | `		if( pLastRef ){` |
|    12346 |  2782 | `			*pLastRef = pTos->nIdx;` |
|     6172 |  2783 | `		}` |
|    18930 |  2784 | `		if( pResult ){` |
|        - |  2785 | `			/* Execution result */` |
|    17978 |  2786 | `			PH7_MemObjStore(pTos,pResult);` |
|     8988 |  2787 | `		}` |
|    18930 |  2788 | `		VmPopOperand(&pTos,1);` |
|    23186 |  2789 | `	}else if( pLastRef ){` |
|        - |  2790 | `		/* Nothing referenced */` |
|     1036 |  2791 | `		*pLastRef = SXU32_HIGH;` |
|      517 |  2792 | `	}` |
|        - |  2793 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2794 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2795 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2796 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2797 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2798 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2799 | `	 * block can override it.` |
|        - |  2800 | `	 */` |
|    32652 |  2801 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2802 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2803 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2804 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2805 | `		pExc->pFrame = 0;` |
|        3 |  2806 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2807 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2808 | `			pExc->iFinallyDone = 1;` |
|        - |  2809 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2810 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2811 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2812 | `				goto Abort;` |
|        - |  2813 | `			}` |
|        1 |  2814 | `		}` |
|        1 |  2815 | `	}` |
|    32650 |  2816 | `	goto Done;` |
|        - |  2817 | `/*` |
|        - |  2818 | ` * HALT: P1 * *` |
|        - |  2819 | ` *` |
|        - |  2820 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2821 | ` * and abort immediately.` |
|        - |  2822 | ` */` |
|        4 |  2823 | `case PH7_OP_HALT:` |
|        9 |  2824 | `	if( pInstr->iP1 ){` |
|        - |  2825 | `#ifdef UNTRUST` |
|        - |  2826 | `		if( pTos < pStack ){` |
|        - |  2827 | `			goto Abort;` |
|        - |  2828 | `		}` |
|        - |  2829 | `#endif` |
|        9 |  2830 | `		if( pLastRef ){` |
|      ! 0 |  2831 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2832 | `		}` |
|        9 |  2833 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2834 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2835 | `				/* Output the exit message */` |
|        7 |  2836 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2837 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2838 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  2839 | `			}` |
|        7 |  2840 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2841 | `			/* Record exit status */` |
|        5 |  2842 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2843 | `		}` |
|        9 |  2844 | `		VmPopOperand(&pTos,1);` |
|        4 |  2845 | `	}else if( pLastRef ){` |
|        - |  2846 | `		/* Nothing referenced */` |
|      ! 0 |  2847 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2848 | `	}` |
|        - |  2849 | `	/* Check if we're in an included file context */` |
|        9 |  2850 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2851 | `		/* Terminate the entire process */` |
|        9 |  2852 | `		exit(pVm->iExitStatus);` |
|        - |  2853 | `	}` |
|      ! 0 |  2854 | `	goto Abort;` |
|        - |  2855 | `/*` |
|        - |  2856 | ` * JMP: * P2 *` |
|        - |  2857 | ` *` |
|        - |  2858 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2859 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2860 | ` */` |
|   218301 |  2861 | `case PH7_OP_JMP:` |
|   436648 |  2862 | `	pc = pInstr->iP2 - 1;` |
|   436648 |  2863 | `	break;` |
|        - |  2864 | `/*` |
|        - |  2865 | ` * JZ: P1 P2 *` |
|        - |  2866 | ` *` |
|        - |  2867 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2868 | ` * entry in the stack if P1 is zero.` |
|        - |  2869 | ` */` |
|   510605 |  2870 | `case PH7_OP_JZ:` |
|        - |  2871 | `#ifdef UNTRUST` |
|        - |  2872 | `	if( pTos < pStack ){` |
|        - |  2873 | `		goto Abort;` |
|        - |  2874 | `	}` |
|        - |  2875 | `#endif` |
|        - |  2876 | `	/* Get a boolean value */` |
|  1021300 |  2877 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  2878 | `		PH7_MemObjToBool(pTos);` |
|       80 |  2879 | `	}` |
|  1021300 |  2880 | `	if( !pTos->x.iVal ){` |
|        - |  2881 | `		/* Take the jump */` |
|   516518 |  2882 | `		pc = pInstr->iP2 - 1;` |
|   258258 |  2883 | `	}` |
|  1021300 |  2884 | `	if( !pInstr->iP1 ){` |
|   811554 |  2885 | `		VmPopOperand(&pTos,1);` |
|   405798 |  2886 | `	}` |
|  1021300 |  2887 | `	break;` |
|        - |  2888 | `/*` |
|        - |  2889 | ` * JNZ: P1 P2 *` |
|        - |  2890 | ` *` |
|        - |  2891 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2892 | ` * entry in the stack if P1 is zero.` |
|        - |  2893 | ` */` |
|    54000 |  2894 | `case PH7_OP_JNZ:` |
|        - |  2895 | `#ifdef UNTRUST` |
|        - |  2896 | `	if( pTos < pStack ){` |
|        - |  2897 | `		goto Abort;` |
|        - |  2898 | `	}` |
|        - |  2899 | `#endif` |
|        - |  2900 | `	/* Get a boolean value */` |
|   108002 |  2901 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2902 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2903 | `	}` |
|   108002 |  2904 | `	if( pTos->x.iVal ){` |
|        - |  2905 | `		/* Take the jump */` |
|     4610 |  2906 | `		pc = pInstr->iP2 - 1;` |
|     2304 |  2907 | `	}` |
|   108002 |  2908 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2909 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2910 | `	}` |
|   108002 |  2911 | `	break;` |
|        - |  2912 | `/*` |
|        - |  2913 | ` * NOOP: * * *` |
|        - |  2914 | ` *` |
|        - |  2915 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2916 | ` * destination.` |
|        - |  2917 | ` */` |
|      ! 0 |  2918 | `case PH7_OP_NOOP:` |
|      ! 0 |  2919 | `	break;` |
|        - |  2920 | `/*` |
|        - |  2921 | ` * POP: P1 * *` |
|        - |  2922 | ` *` |
|        - |  2923 | ` * Pop P1 elements from the operand stack.` |
|        - |  2924 | ` */` |
|   397976 |  2925 | `case PH7_OP_POP: {` |
|   795998 |  2926 | `	sxi32 n = pInstr->iP1;` |
|   795998 |  2927 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2928 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2929 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2930 | `	}` |
|   795998 |  2931 | `	VmPopOperand(&pTos,n);` |
|   795998 |  2932 | `	break;` |
|        - |  2933 | `				 }` |
|        - |  2934 | `/*` |
|        - |  2935 | ` * DUP: * * *` |
|        - |  2936 | ` *` |
|        - |  2937 | ` * Duplicate the top of the stack.` |
|        - |  2938 | ` */` |
|       41 |  2939 | `case PH7_OP_DUP:` |
|        - |  2940 | `#ifdef UNTRUST` |
|        - |  2941 | `	if( pTos < pStack ){` |
|        - |  2942 | `		goto Abort;` |
|        - |  2943 | `	}` |
|        - |  2944 | `#endif` |
|       84 |  2945 | `	pTos++;` |
|       84 |  2946 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  2947 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  2948 | `	break;` |
|        - |  2949 | `/*` |
|        - |  2950 | ` * NSSWITCH: * * P3` |
|        - |  2951 | ` *` |
|        - |  2952 | ` * Switch the active namespace at runtime.` |
|        - |  2953 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2954 | ` */` |
|     6591 |  2955 | `case PH7_OP_NSSWITCH:` |
|    13184 |  2956 | `	SyBlobReset(&pVm->sNamespace);` |
|    13184 |  2957 | `	if( pInstr->p3 ){` |
|       90 |  2958 | `		const char *zNs = (const char *)pInstr->p3;` |
|       90 |  2959 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       44 |  2960 | `	}` |
|        - |  2961 | `	/* Clear namespace-scoped use-const imports */` |
|    13184 |  2962 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    13184 |  2963 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    13184 |  2964 | `	break;` |
|        - |  2965 | `/* OP_USECONST P1 * P3` |
|        - |  2966 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  2967 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  2968 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  2969 | ` */` |
|        7 |  2970 | `case PH7_OP_USECONST: {` |
|       16 |  2971 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  2972 | `	if( azPair ){` |
|       16 |  2973 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  2974 | `	}` |
|       16 |  2975 | `	break;` |
|        - |  2976 | `				}` |
|        - |  2977 | `/*` |
|        - |  2978 | ` * CVT_INT: * * *` |
|        - |  2979 | ` *` |
|        - |  2980 | ` * Force the top of the stack to be an integer.` |
|        - |  2981 | ` */` |
|       35 |  2982 | `case PH7_OP_CVT_INT:` |
|        - |  2983 | `#ifdef UNTRUST` |
|        - |  2984 | `	if( pTos < pStack ){` |
|        - |  2985 | `		goto Abort;` |
|        - |  2986 | `	}` |
|        - |  2987 | `#endif` |
|       72 |  2988 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2989 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2990 | `	}` |
|        - |  2991 | `	/* Invalidate any prior representation */` |
|       72 |  2992 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2993 | `	break;` |
|        - |  2994 | `/*` |
|        - |  2995 | ` * CVT_REAL: * * *` |
|        - |  2996 | ` *` |
|        - |  2997 | ` * Force the top of the stack to be a real.` |
|        - |  2998 | ` */` |
|        4 |  2999 | `case PH7_OP_CVT_REAL:` |
|        - |  3000 | `#ifdef UNTRUST` |
|        - |  3001 | `	if( pTos < pStack ){` |
|        - |  3002 | `		goto Abort;` |
|        - |  3003 | `	}` |
|        - |  3004 | `#endif` |
|        9 |  3005 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3006 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3007 | `	}` |
|        - |  3008 | `	/* Invalidate any prior representation */` |
|        9 |  3009 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3010 | `	break;` |
|        - |  3011 | `/*` |
|        - |  3012 | ` * CVT_STR: * * *` |
|        - |  3013 | ` *` |
|        - |  3014 | ` * Force the top of the stack to be a string.` |
|        - |  3015 | ` */` |
|      146 |  3016 | `case PH7_OP_CVT_STR:` |
|        - |  3017 | `#ifdef UNTRUST` |
|        - |  3018 | `	if( pTos < pStack ){` |
|        - |  3019 | `		goto Abort;` |
|        - |  3020 | `	}` |
|        - |  3021 | `#endif` |
|      294 |  3022 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3023 | `		PH7_MemObjToString(pTos);` |
|      146 |  3024 | `	}` |
|      294 |  3025 | `	break;` |
|        - |  3026 | `/*` |
|        - |  3027 | ` * CVT_BOOL: * * *` |
|        - |  3028 | ` *` |
|        - |  3029 | ` * Force the top of the stack to be a boolean.` |
|        - |  3030 | ` */` |
|        5 |  3031 | `case PH7_OP_CVT_BOOL:` |
|        - |  3032 | `#ifdef UNTRUST` |
|        - |  3033 | `	if( pTos < pStack ){` |
|        - |  3034 | `		goto Abort;` |
|        - |  3035 | `	}` |
|        - |  3036 | `#endif` |
|       11 |  3037 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3038 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3039 | `	}` |
|       11 |  3040 | `	break;` |
|        - |  3041 | `/*` |
|        - |  3042 | ` * CVT_NULL: * * *` |
|        - |  3043 | ` *` |
|        - |  3044 | ` * Nullify the top of the stack.` |
|        - |  3045 | ` */` |
|        3 |  3046 | `case PH7_OP_CVT_NULL:` |
|        - |  3047 | `#ifdef UNTRUST` |
|        - |  3048 | `	if( pTos < pStack ){` |
|        - |  3049 | `		goto Abort;` |
|        - |  3050 | `	}` |
|        - |  3051 | `#endif` |
|        7 |  3052 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3053 | `	break;` |
|        - |  3054 | `/*` |
|        - |  3055 | ` * CVT_NUMC: * * *` |
|        - |  3056 | ` *` |
|        - |  3057 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3058 | ` */` |
|      ! 0 |  3059 | `case PH7_OP_CVT_NUMC:` |
|        - |  3060 | `#ifdef UNTRUST` |
|        - |  3061 | `	if( pTos < pStack ){` |
|        - |  3062 | `		goto Abort;` |
|        - |  3063 | `	}` |
|        - |  3064 | `#endif` |
|        - |  3065 | `	/* Force a numeric cast */` |
|      ! 0 |  3066 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3067 | `	break;` |
|        - |  3068 | `/*` |
|        - |  3069 | ` * CVT_ARRAY: * * *` |
|        - |  3070 | ` *` |
|        - |  3071 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3072 | ` */` |
|       10 |  3073 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3074 | `#ifdef UNTRUST` |
|        - |  3075 | `	if( pTos < pStack ){` |
|        - |  3076 | `		goto Abort;` |
|        - |  3077 | `	}` |
|        - |  3078 | `#endif` |
|        - |  3079 | `	/* Force a hashmap cast */` |
|       21 |  3080 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3081 | `	if( rc != SXRET_OK ){` |
|        - |  3082 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3083 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3084 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3085 | `	}` |
|       21 |  3086 | `	break;` |
|        - |  3087 | `/*` |
|        - |  3088 | ` * CVT_OBJ: * * *` |
|        - |  3089 | ` *` |
|        - |  3090 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3091 | ` */` |
|        8 |  3092 | `case PH7_OP_CVT_OBJ:` |
|        - |  3093 | `#ifdef UNTRUST` |
|        - |  3094 | `	if( pTos < pStack ){` |
|        - |  3095 | `		goto Abort;` |
|        - |  3096 | `	}` |
|        - |  3097 | `#endif` |
|       17 |  3098 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3099 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3100 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3101 | `	}` |
|       17 |  3102 | `	break;` |
|        - |  3103 | `/*` |
|        - |  3104 | ` * ERR_CTRL * * *` |
|        - |  3105 | ` *` |
|        - |  3106 | ` * Error control operator.` |
|        - |  3107 | ` */` |
|    13246 |  3108 | `case PH7_OP_ERR_CTRL:` |
|        - |  3109 | `	/*` |
|        - |  3110 | `	 * TICKET 1433-038:` |
|        - |  3111 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3112 | `	 * use the public API,to control error output.` |
|        - |  3113 | `	 */` |
|    26492 |  3114 | `	break;` |
|        - |  3115 | `/*` |
|        - |  3116 | ` * IS_A * * *` |
|        - |  3117 | ` *` |
|        - |  3118 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3119 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3120 | ` * holding a class name or an object).` |
|        - |  3121 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3122 | ` */` |
|       23 |  3123 | `case PH7_OP_IS_A:{` |
|       48 |  3124 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3125 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3126 | `#ifdef UNTRUST` |
|        - |  3127 | `	if( pNos < pStack ){` |
|        - |  3128 | `		goto Abort;` |
|        - |  3129 | `	}` |
|        - |  3130 | `#endif` |
|       48 |  3131 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3132 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3133 | `		ph7_class *pClass = 0;` |
|        - |  3134 | `		/* Extract the target class */` |
|       46 |  3135 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3136 | `			/* Instance already loaded */` |
|      ! 0 |  3137 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3138 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3139 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3140 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3141 | `			/* Handle self/static/parent keywords */` |
|       46 |  3142 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3143 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3144 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3145 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3146 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3147 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3148 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3149 | `					pClass = pSelf->pBase;` |
|        2 |  3150 | `				}` |
|        3 |  3151 | `			}else{` |
|       36 |  3152 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3153 | `			}` |
|       22 |  3154 | `		}` |
|       46 |  3155 | `		if( pClass ){` |
|        - |  3156 | `			/* Perform the query */` |
|       46 |  3157 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3158 | `		}` |
|       22 |  3159 | `	}` |
|        - |  3160 | `	/* Push result */` |
|       48 |  3161 | `	VmPopOperand(&pTos,1);` |
|       48 |  3162 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3163 | `	pTos->x.iVal = iRes;` |
|       48 |  3164 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3165 | `	break;` |
|        - |  3166 | `				 }` |
|        - |  3167 |  |
|        - |  3168 | `/*` |
|        - |  3169 | ` * LOADC P1 P2 *` |
|        - |  3170 | ` *` |
|        - |  3171 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3172 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3173 | ` */` |
|   852519 |  3174 | `case PH7_OP_LOADC: {` |
|        - |  3175 | `	ph7_value *pObj;` |
|        - |  3176 | `	/* Reserve a room */` |
|  1705084 |  3177 | `	pTos++;` |
|  2549324 |  3178 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1705084 |  3179 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3180 | `			SyHashEntry *pEntry;` |
|        - |  3181 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3182 | `			{` |
|        - |  3183 | `				SyHashEntry *pConstImport;` |
|    24974 |  3184 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    16648 |  3185 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16650 |  3186 | `				if( pConstImport ){` |
|       11 |  3187 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3188 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3189 | `					if( pEntry ){` |
|       11 |  3190 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3191 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3192 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3193 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3194 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3195 | `						break;` |
|        - |  3196 | `					}` |
|        - |  3197 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3198 | `				}` |
|        - |  3199 | `			}` |
|        - |  3200 | `			/* Candidate for expansion via user defined callbacks */` |
|    16640 |  3201 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16640 |  3202 | `			if( pEntry ){` |
|    16636 |  3203 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3204 | `				/* Set a NULL default value */` |
|    16636 |  3205 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16636 |  3206 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3207 | `				/* Invoke the callback and deal with the expanded value */` |
|    16636 |  3208 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3209 | `				/* Mark as constant */` |
|    16636 |  3210 | `				pTos->nIdx = SXU32_HIGH;` |
|    16636 |  3211 | `				break;` |
|        - |  3212 | `			}` |
|        - |  3213 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3214 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3215 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3216 | `			{` |
|        6 |  3217 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3218 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3219 | `				sxu32 j;` |
|        6 |  3220 | `				int isQualified = 0;` |
|       32 |  3221 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3222 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3223 | `				}` |
|        6 |  3224 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3225 | `					/* Try current_namespace\name */` |
|      ! 0 |  3226 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3227 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3228 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3229 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3230 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3231 | `					if( pEntry ){` |
|      ! 0 |  3232 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3233 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3234 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3235 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3236 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3237 | `						break;` |
|        - |  3238 | `					}` |
|        - |  3239 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3240 | `				}` |
|        6 |  3241 | `				if( isQualified ){` |
|        - |  3242 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3243 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3244 | `					SyBlob sErr;` |
|        3 |  3245 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3246 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3247 | `					if( pErrFile ){` |
|        3 |  3248 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3249 | `					}` |
|        3 |  3250 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3251 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3252 | `					SyBlobRelease(&sErr);` |
|        3 |  3253 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3254 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3255 | `					goto LoadC_Done;` |
|        - |  3256 | `				}` |
|        - |  3257 | `			}` |
|        1 |  3258 | `		}` |
|  1688438 |  3259 | `		PH7_MemObjLoad(pObj,pTos);` |
|   844242 |  3260 | `	}else{` |
|        - |  3261 | `		/* Set a NULL value */` |
|      ! 0 |  3262 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3263 | `	}` |
|   844197 |  3264 | `LoadC_Done:` |
|        - |  3265 | `	/* Mark as constant */` |
|  1688440 |  3266 | `	pTos->nIdx = SXU32_HIGH;` |
|  1688440 |  3267 | `	break;` |
|        - |  3268 | `				  }` |
|        - |  3269 | `/*` |
|        - |  3270 | ` * LOAD: P1 * P3` |
|        - |  3271 | ` *` |
|        - |  3272 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3273 | ` * from the P3 operand.` |
|        - |  3274 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3275 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3276 | ` */` |
|  1368007 |  3277 | `case PH7_OP_LOAD:{` |
|        - |  3278 | `	ph7_value *pObj;` |
|        - |  3279 | `	SyString sName;` |
|  2736236 |  3280 | `	if( pInstr->p3 == 0 ){` |
|        - |  3281 | `		/* Take the variable name from the top of the stack */` |
|        - |  3282 | `#ifdef UNTRUST` |
|        - |  3283 | `		if( pTos < pStack ){` |
|        - |  3284 | `			goto Abort;` |
|        - |  3285 | `		}` |
|        - |  3286 | `#endif` |
|        - |  3287 | `		/* Force a string cast */` |
|       19 |  3288 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3289 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3290 | `		}` |
|       19 |  3291 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3292 | `	}else{` |
|  2736218 |  3293 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3294 | `		/* Reserve a room for the target object */` |
|  2736218 |  3295 | `		pTos++;` |
|        - |  3296 | `	}` |
|        - |  3297 | `	/* Extract the requested memory object */` |
|  2736236 |  3298 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2736236 |  3299 | `	if( pObj == 0 ){` |
|       26 |  3300 | `		if( pInstr->iP1 ){` |
|        - |  3301 | `			/* Variable not found,load NULL */` |
|       26 |  3302 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3303 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3304 | `			}else{` |
|       26 |  3305 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3306 | `			}` |
|       26 |  3307 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1368021 |  3308 | `			break;` |
|      ! 0 |  3309 | `		}else{` |
|        - |  3310 | `			/* Fatal error */` |
|      ! 0 |  3311 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3312 | `			goto Abort;` |
|        - |  3313 | `		}` |
|        - |  3314 | `	}` |
|        - |  3315 | `	/* Load variable contents */` |
|  2736212 |  3316 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2736212 |  3317 | `	pTos->nIdx = pObj->nIdx;` |
|  2736212 |  3318 | `	break;` |
|        - |  3319 | `				   }` |
|        - |  3320 | `/*` |
|        - |  3321 | ` * LOAD_MAP P1 * *` |
|        - |  3322 | ` *` |
|        - |  3323 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3324 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3325 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3326 | ` */` |
|    19039 |  3327 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3328 | `	ph7_hashmap *pMap;` |
|        - |  3329 | `	/* Allocate a new hashmap instance */` |
|    38080 |  3330 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    38080 |  3331 | `	if( pMap == 0 ){` |
|      ! 0 |  3332 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3333 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3334 | `		goto Abort;` |
|        - |  3335 | `	}` |
|    38080 |  3336 | `	if( pInstr->iP1 > 0 ){` |
|     2320 |  3337 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3338 | `		/* Perform the insertion */` |
|     7104 |  3339 | `		while( pEntry < pTos ){` |
|     4786 |  3340 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3341 | `				/* Insertion by reference */` |
|      142 |  3342 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3343 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3344 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3345 | `					);` |
|       48 |  3346 | `			}else{` |
|        - |  3347 | `				/* Standard insertion */` |
|     7037 |  3348 | `				PH7_HashmapInsert(pMap,` |
|     4690 |  3349 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2345 |  3350 | `					&pEntry[1]` |
|        - |  3351 | `				);` |
|        - |  3352 | `			}` |
|        - |  3353 | `			/* Next pair on the stack */` |
|     4786 |  3354 | `			pEntry += 2;` |
|        2 |  3355 | `		}` |
|        - |  3356 | `		/* Pop P1 elements */` |
|     2320 |  3357 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1159 |  3358 | `	}` |
|        - |  3359 | `	/* Push the hashmap */` |
|    38080 |  3360 | `	pTos++;` |
|    38080 |  3361 | `	pTos->nIdx = SXU32_HIGH;` |
|    38080 |  3362 | `	pTos->x.pOther = pMap;` |
|    38080 |  3363 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    38080 |  3364 | `	break;` |
|        - |  3365 | `					  }` |
|        - |  3366 | `/*` |
|        - |  3367 | ` * LOAD_LIST: P1 * *` |
|        - |  3368 | ` *` |
|        - |  3369 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3370 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3371 | ` * Caveats:` |
|        - |  3372 | ` *  This implementation support only a single nesting level.` |
|        - |  3373 | ` */` |
|       48 |  3374 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3375 | `	ph7_value *pEntry;` |
|       98 |  3376 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3377 | `		/* Empty list,break immediately */` |
|      ! 0 |  3378 | `		break;` |
|        - |  3379 | `	}` |
|       98 |  3380 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3381 | `#ifdef UNTRUST` |
|        - |  3382 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3383 | `		goto Abort;` |
|        - |  3384 | `	}` |
|        - |  3385 | `#endif` |
|       98 |  3386 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  3387 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3388 | `		ph7_hashmap_node *pNode;` |
|        - |  3389 | `		ph7_value sKey,*pObj;` |
|        - |  3390 | `		/* Start Copying */` |
|       91 |  3391 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  3392 | `		while( pEntry <= pTos ){` |
|      193 |  3393 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  3394 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  3395 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  3396 | `					if( rc == SXRET_OK ){` |
|        - |  3397 | `						/* Store node value */` |
|      165 |  3398 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  3399 | `					}else{` |
|        - |  3400 | `						/* Undefined array key */` |
|        - |  3401 | `						char zMsg[128];` |
|      ! 0 |  3402 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  3403 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3404 | `						PH7_MemObjRelease(pObj);` |
|        - |  3405 | `					}` |
|       82 |  3406 | `				}` |
|       82 |  3407 | `			}` |
|      193 |  3408 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  3409 | `			pEntry++;` |
|        1 |  3410 | `		}` |
|       46 |  3411 | `	}else{` |
|        - |  3412 | `		/* Source is not an array */` |
|        - |  3413 | `		ph7_value *pObj;` |
|       18 |  3414 | `		while( pEntry <= pTos ){` |
|       12 |  3415 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  3416 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  3417 | `					PH7_MemObjRelease(pObj);` |
|        5 |  3418 | `				}` |
|        5 |  3419 | `			}` |
|       12 |  3420 | `			pEntry++;` |
|        2 |  3421 | `		}` |
|        8 |  3422 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  3423 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  3424 | `			const char *zType = "unknown";` |
|        3 |  3425 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  3426 | `			char zMsg[256];` |
|        3 |  3427 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  3428 | `				zType = "string";` |
|        1 |  3429 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  3430 | `				zType = "int";` |
|      ! 0 |  3431 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3432 | `				zType = "float";` |
|      ! 0 |  3433 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3434 | `				zType = "object";` |
|      ! 0 |  3435 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  3436 | `				zType = "resource";` |
|      ! 0 |  3437 | `			}` |
|        3 |  3438 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  3439 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  3440 | `		}` |
|        - |  3441 | `	}` |
|       98 |  3442 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  3443 | `	break;` |
|        - |  3444 | `					   }` |
|        - |  3445 | `/*` |
|        - |  3446 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3447 | ` *` |
|        - |  3448 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3449 | ` * from the stack.` |
|        - |  3450 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3451 | ` * instead.` |
|        - |  3452 | ` */` |
|   219206 |  3453 | `case PH7_OP_LOAD_IDX: {` |
|   438458 |  3454 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   438458 |  3455 | `	ph7_hashmap *pMap = 0;` |
|        - |  3456 | `	ph7_value *pIdx;` |
|   438458 |  3457 | `	pIdx = 0;` |
|   438458 |  3458 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3459 | `		if( !pInstr->iP2){` |
|        - |  3460 | `			/* No available index,load NULL */` |
|      ! 0 |  3461 | `			if( pTos >= pStack ){` |
|      ! 0 |  3462 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3463 | `			}else{` |
|        - |  3464 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3465 | `				pTos++;` |
|      ! 0 |  3466 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3467 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3468 | `			}` |
|        - |  3469 | `			/* Emit a notice */` |
|      ! 0 |  3470 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3471 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3472 | `			break;` |
|        - |  3473 | `		}` |
|      ! 0 |  3474 | `	}else{` |
|   438458 |  3475 | `		pIdx = pTos;` |
|   438458 |  3476 | `		pTos--;` |
|        - |  3477 | `	}` |
|   438458 |  3478 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3479 | `		/* String access */` |
|   343918 |  3480 | `		if( pIdx ){` |
|        - |  3481 | `			sxu32 nOfft;` |
|   343918 |  3482 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3483 | `				/* Force an int cast */` |
|      ! 0 |  3484 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3485 | `			}` |
|   343918 |  3486 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   343918 |  3487 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3488 | `				/* Invalid offset,load null */` |
|      ! 0 |  3489 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3490 | `			}else{` |
|   343918 |  3491 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   343918 |  3492 | `				int c = zData[nOfft];` |
|   343918 |  3493 | `				PH7_MemObjRelease(pTos);` |
|   343918 |  3494 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   343918 |  3495 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3496 | `			}` |
|   171982 |  3497 | `		}else{` |
|        - |  3498 | `			/* No available index,load NULL */` |
|      ! 0 |  3499 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3500 | `		}` |
|   343918 |  3501 | `		break;` |
|        - |  3502 | `	}` |
|    94542 |  3503 | `	if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3504 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3505 | `			ph7_value *pObj;` |
|      ! 0 |  3506 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3507 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3508 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3509 | `			}` |
|      ! 0 |  3510 | `		}` |
|      ! 0 |  3511 | `	}` |
|    94542 |  3512 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    94542 |  3513 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    94542 |  3514 | `		if( pInstr->iP2 == 1 ){` |
|        - |  3515 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3516 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3517 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3518 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3519 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3520 | `		}` |
|        - |  3521 | `		/* Point to the hashmap */` |
|    94542 |  3522 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    94542 |  3523 | `		if( pIdx ){` |
|        - |  3524 | `			/* Load the desired entry */` |
|    94542 |  3525 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    47270 |  3526 | `		}` |
|    94542 |  3527 | `		if( rc != SXRET_OK && pInstr->iP2 == 1 ){` |
|        - |  3528 | `			/* Create a new empty entry */` |
|      265 |  3529 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3530 | `			if( rc == SXRET_OK ){` |
|        - |  3531 | `				/* Point to the last inserted entry */` |
|      265 |  3532 | `				pNode = pMap->pLast;` |
|      132 |  3533 | `			}` |
|      132 |  3534 | `		}` |
|    47270 |  3535 | `	}` |
|    94542 |  3536 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  3537 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  3538 | `		char zMsg[128];` |
|      ! 0 |  3539 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3540 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3541 | `		}` |
|      ! 0 |  3542 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  3543 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3544 | `	}` |
|    94542 |  3545 | `	if( pIdx ){` |
|    94542 |  3546 | `		PH7_MemObjRelease(pIdx);` |
|    47270 |  3547 | `	}` |
|    94542 |  3548 | `	if( rc == SXRET_OK ){` |
|        - |  3549 | `		/* Load entry contents */` |
|    43072 |  3550 | `		if( pMap->iRef < 2 ){` |
|        - |  3551 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3552 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3553 | `			 */` |
|       24 |  3554 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3555 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3556 | `		}else{` |
|    43050 |  3557 | `			pTos->nIdx = pNode->nValIdx;` |
|    43050 |  3558 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    43050 |  3559 | `			PH7_HashmapUnref(pMap);` |
|        - |  3560 | `		}` |
|    21537 |  3561 | `	}else{` |
|        - |  3562 | `		/* No such entry,load NULL */` |
|    51472 |  3563 | `		PH7_MemObjRelease(pTos);` |
|    51472 |  3564 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3565 | `	}` |
|    94542 |  3566 | `	break;` |
|        - |  3567 | `					  }` |
|        - |  3568 | `/*` |
|        - |  3569 | ` * LOAD_CLOSURE * * P3` |
|        - |  3570 | ` *` |
|        - |  3571 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3572 | ` * name in the stack.` |
|        - |  3573 | ` */` |
|        4 |  3574 | `case PH7_OP_LOAD_CLOSURE:{` |
|        9 |  3575 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        9 |  3576 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3577 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3578 | `		ph7_vm_func *pClosure;` |
|        - |  3579 | `		char *zName;` |
|        - |  3580 | `		sxu32 mLen;` |
|        - |  3581 | `		sxu32 n;` |
|        - |  3582 | `		/* Create a new VM function */` |
|        9 |  3583 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3584 | `		/* Generate an unique closure name */` |
|        9 |  3585 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        9 |  3586 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3587 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3588 | `			goto Abort;` |
|        - |  3589 | `		}` |
|        9 |  3590 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        9 |  3591 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3592 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3593 | `		}` |
|        - |  3594 | `		/* Zero the stucture */` |
|        9 |  3595 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3596 | `		/* Perform a structure assignment on read-only items */` |
|        9 |  3597 | `		pClosure->aArgs = pFunc->aArgs;` |
|        9 |  3598 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        9 |  3599 | `		pClosure->aStatic = pFunc->aStatic;` |
|        9 |  3600 | `		pClosure->iFlags = pFunc->iFlags;` |
|        9 |  3601 | `		pClosure->pUserData = pFunc->pUserData;` |
|        9 |  3602 | `		pClosure->sSignature = pFunc->sSignature;` |
|        9 |  3603 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|        9 |  3604 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|        9 |  3605 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3606 | `		/* Register the closure */` |
|        9 |  3607 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3608 | `		/* Set up closure environment */` |
|        9 |  3609 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        9 |  3610 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       27 |  3611 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3612 | `			ph7_value *pValue;` |
|       19 |  3613 | `			pEnv = &aEnv[n];` |
|       19 |  3614 | `			sEnv.sName  = pEnv->sName;` |
|       19 |  3615 | `			sEnv.iFlags = pEnv->iFlags;` |
|       19 |  3616 | `			sEnv.nIdx = SXU32_HIGH;` |
|       19 |  3617 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|       19 |  3618 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3619 | `				/* Pass by reference */` |
|      ! 0 |  3620 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3621 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3622 | `					);` |
|      ! 0 |  3623 | `			}` |
|        - |  3624 | `			/* Standard pass by value */` |
|       19 |  3625 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|       19 |  3626 | `			if( pValue ){` |
|        - |  3627 | `				/* Copy imported value */` |
|       11 |  3628 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        5 |  3629 | `			}` |
|        - |  3630 | `			/* Insert the imported variable */` |
|       19 |  3631 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       10 |  3632 | `		}` |
|        - |  3633 | `		/* Finally,load the closure name on the stack */` |
|        9 |  3634 | `		pTos++;` |
|        9 |  3635 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        4 |  3636 | `	}` |
|        9 |  3637 | `	break;` |
|        - |  3638 | `						 }` |
|        - |  3639 | `/*` |
|        - |  3640 | ` * STORE * P2 P3` |
|        - |  3641 | ` *` |
|        - |  3642 | ` * Perform a store (Assignment) operation.` |
|        - |  3643 | ` */` |
|   117024 |  3644 | `case PH7_OP_STORE: {` |
|        - |  3645 | `	ph7_value *pObj;` |
|        - |  3646 | `	SyString sName;` |
|        - |  3647 | `#ifdef UNTRUST` |
|        - |  3648 | `	if( pTos < pStack ){` |
|        - |  3649 | `		goto Abort;` |
|        - |  3650 | `	}` |
|        - |  3651 | `#endif` |
|   234050 |  3652 | `	if( pInstr->iP2 ){` |
|        - |  3653 | `		sxu32 nIdx;` |
|        - |  3654 | `		/* Member store operation */` |
|     3014 |  3655 | `		nIdx = pTos->nIdx;` |
|     3014 |  3656 | `		VmPopOperand(&pTos,1);` |
|     3014 |  3657 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3658 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3659 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3660 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3661 | `		}else{` |
|        - |  3662 | `			/* Point to the desired memory object */` |
|     3010 |  3663 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3010 |  3664 | `			if( pObj ){` |
|        - |  3665 | `				/* Perform the store operation */` |
|     3010 |  3666 | `				PH7_MemObjStore(pTos,pObj);` |
|     1504 |  3667 | `			}` |
|        - |  3668 | `		}` |
|   118532 |  3669 | `		break;` |
|   231038 |  3670 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3671 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3672 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3673 | `			/* Force a string cast */` |
|      ! 0 |  3674 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3675 | `		}` |
|        7 |  3676 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3677 | `		pTos--;` |
|        - |  3678 | `#ifdef UNTRUST` |
|        - |  3679 | `		if( pTos < pStack  ){` |
|        - |  3680 | `			goto Abort;` |
|        - |  3681 | `		}` |
|        - |  3682 | `#endif` |
|        4 |  3683 | `	}else{` |
|   231032 |  3684 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3685 | `	}` |
|        - |  3686 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   231038 |  3687 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   231038 |  3688 | `	if( pObj == 0 ){` |
|      ! 0 |  3689 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3690 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3691 | `		goto Abort;` |
|        - |  3692 | `	}` |
|   231038 |  3693 | `	if( !pInstr->p3 ){` |
|        7 |  3694 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3695 | `	}` |
|        - |  3696 | `	/* Perform the store operation */` |
|   231038 |  3697 | `	PH7_MemObjStore(pTos,pObj);` |
|   231038 |  3698 | `	break;` |
|        - |  3699 | `				   }` |
|        - |  3700 | `/*` |
|        - |  3701 | ` * STORE_IDX:   P1 * P3` |
|        - |  3702 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3703 | ` *` |
|        - |  3704 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3705 | ` */` |
|    84268 |  3706 | `case PH7_OP_STORE_IDX:` |
|        - |  3707 | `case PH7_OP_STORE_IDX_REF: {` |
|   168538 |  3708 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3709 | `	ph7_value *pKey;` |
|        - |  3710 | `	sxu32 nIdx;` |
|   168538 |  3711 | `	if( pInstr->iP1 ){` |
|        - |  3712 | `		/* Key is next on stack */` |
|    58322 |  3713 | `		pKey = pTos;` |
|    58322 |  3714 | `		pTos--;` |
|    29162 |  3715 | `	}else{` |
|   110218 |  3716 | `		pKey = 0;` |
|        - |  3717 | `	}` |
|   168538 |  3718 | `	nIdx = pTos->nIdx;` |
|   168538 |  3719 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3720 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3721 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3722 | `		 * checking true sharing count, then re-add after separation. */` |
|   168486 |  3723 | `		if( nIdx != SXU32_HIGH ){` |
|   168486 |  3724 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   252728 |  3725 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   168486 |  3726 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3727 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3728 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3729 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3730 | `				 * refcounts if the backing array was already separated. */` |
|   168486 |  3731 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   168486 |  3732 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   168486 |  3733 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   168486 |  3734 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   168486 |  3735 | `					pTos->x.pOther = pMap;` |
|    84244 |  3736 | `				}else{` |
|        - |  3737 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3738 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3739 | `					pMap = pCur;` |
|        - |  3740 | `				}` |
|    84244 |  3741 | `			}else{` |
|      ! 0 |  3742 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3743 | `			}` |
|    84244 |  3744 | `		}else{` |
|      ! 0 |  3745 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3746 | `		}` |
|   168486 |  3747 | `		if( pMap->iRef < 2 ){` |
|        - |  3748 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3749 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3750 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3751 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3752 | `			pMap->iRef = 2;` |
|      ! 0 |  3753 | `		}` |
|    84244 |  3754 | `	}else{` |
|        - |  3755 | `		ph7_value *pObj;` |
|       53 |  3756 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3757 | `		if( pObj == 0 ){` |
|      ! 0 |  3758 | `			if( pKey ){` |
|      ! 0 |  3759 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3760 | `			}` |
|      ! 0 |  3761 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3762 | `			break;` |
|        - |  3763 | `		}` |
|        - |  3764 | `		/* Phase#1: Load the array */` |
|       53 |  3765 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3766 | `			VmPopOperand(&pTos,1);` |
|       53 |  3767 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3768 | `				/* Force a string cast */` |
|      ! 0 |  3769 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3770 | `			}` |
|       53 |  3771 | `			if( pKey == 0 ){` |
|        - |  3772 | `				/* Append string */` |
|        3 |  3773 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3774 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3775 | `				}` |
|        2 |  3776 | `			}else{` |
|        - |  3777 | `				sxu32 nOfft;` |
|       51 |  3778 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3779 | `					/* Force an int cast */` |
|       51 |  3780 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3781 | `				}` |
|       51 |  3782 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3783 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3784 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3785 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3786 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3787 | `				}else{` |
|      ! 0 |  3788 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3789 | `						/* Perform an append operation */` |
|      ! 0 |  3790 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3791 | `					}` |
|        - |  3792 | `				}` |
|        - |  3793 | `			}` |
|       53 |  3794 | `			if( pKey ){` |
|       51 |  3795 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3796 | `			}` |
|       53 |  3797 | `			break;` |
|      ! 0 |  3798 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3799 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3800 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3801 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3802 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3803 | `				goto Abort;` |
|        - |  3804 | `			}` |
|      ! 0 |  3805 | `		}` |
|        - |  3806 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  3807 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  3808 | `	}` |
|   168486 |  3809 | `	VmPopOperand(&pTos,1);` |
|        - |  3810 | `	/* Phase#2: Perform the insertion */` |
|   168486 |  3811 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3812 | `		/* Insertion by reference */` |
|       15 |  3813 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3814 | `	}else{` |
|   168472 |  3815 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3816 | `	}` |
|   168486 |  3817 | `	if( pKey ){` |
|    58272 |  3818 | `		PH7_MemObjRelease(pKey);` |
|    29135 |  3819 | `	}` |
|   168486 |  3820 | `	break;` |
|        - |  3821 | `					   }` |
|        - |  3822 | `/*` |
|        - |  3823 | ` * INCR: P1 * *` |
|        - |  3824 | ` *` |
|        - |  3825 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3826 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3827 | ` * the stack and increment after that.` |
|        - |  3828 | ` */` |
|   152678 |  3829 | `case PH7_OP_INCR:` |
|        - |  3830 | `#ifdef UNTRUST` |
|        - |  3831 | `	if( pTos < pStack ){` |
|        - |  3832 | `		goto Abort;` |
|        - |  3833 | `	}` |
|        - |  3834 | `#endif` |
|   305402 |  3835 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   305402 |  3836 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3837 | `			ph7_value *pObj;` |
|   305402 |  3838 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3839 | `				/* Force a numeric cast */` |
|   305402 |  3840 | `				PH7_MemObjToNumeric(pObj);` |
|   305402 |  3841 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3842 | `					pObj->rVal++;` |
|        - |  3843 | `					/* Try to get an integer representation */` |
|      ! 0 |  3844 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3845 | `				}else{` |
|   305402 |  3846 | `					pObj->x.iVal++;` |
|   305402 |  3847 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3848 | `				}` |
|   305402 |  3849 | `				if( pInstr->iP1 ){` |
|        - |  3850 | `					/* Pre-icrement */` |
|       71 |  3851 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3852 | `				}` |
|   152722 |  3853 | `			}` |
|   152724 |  3854 | `		}else{` |
|      ! 0 |  3855 | `			if( pInstr->iP1 ){` |
|        - |  3856 | `				/* Force a numeric cast */` |
|      ! 0 |  3857 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3858 | `				/* Pre-increment */` |
|      ! 0 |  3859 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3860 | `					pTos->rVal++;` |
|        - |  3861 | `					/* Try to get an integer representation */` |
|      ! 0 |  3862 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3863 | `				}else{` |
|      ! 0 |  3864 | `					pTos->x.iVal++;` |
|      ! 0 |  3865 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3866 | `				}` |
|      ! 0 |  3867 | `			}` |
|        - |  3868 | `		}` |
|   152722 |  3869 | `	}` |
|   305402 |  3870 | `	break;` |
|        - |  3871 | `/*` |
|        - |  3872 | ` * DECR: P1 * *` |
|        - |  3873 | ` *` |
|        - |  3874 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3875 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3876 | ` * and decrement after that.` |
|        - |  3877 | ` */` |
|        2 |  3878 | `case PH7_OP_DECR:` |
|        - |  3879 | `#ifdef UNTRUST` |
|        - |  3880 | `	if( pTos < pStack ){` |
|        - |  3881 | `		goto Abort;` |
|        - |  3882 | `	}` |
|        - |  3883 | `#endif` |
|        5 |  3884 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3885 | `		/* Force a numeric cast */` |
|        5 |  3886 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3887 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3888 | `			ph7_value *pObj;` |
|        5 |  3889 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3890 | `				/* Force a numeric cast */` |
|        5 |  3891 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3892 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3893 | `					pObj->rVal--;` |
|        - |  3894 | `					/* Try to get an integer representation */` |
|      ! 0 |  3895 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3896 | `				}else{` |
|        5 |  3897 | `					pObj->x.iVal--;` |
|        5 |  3898 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3899 | `				}` |
|        5 |  3900 | `				if( pInstr->iP1 ){` |
|        - |  3901 | `					/* Pre-icrement */` |
|      ! 0 |  3902 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3903 | `				}` |
|        2 |  3904 | `			}` |
|        3 |  3905 | `		}else{` |
|      ! 0 |  3906 | `			if( pInstr->iP1 ){` |
|        - |  3907 | `				/* Pre-increment */` |
|      ! 0 |  3908 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3909 | `					pTos->rVal--;` |
|        - |  3910 | `					/* Try to get an integer representation */` |
|      ! 0 |  3911 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3912 | `				}else{` |
|      ! 0 |  3913 | `					pTos->x.iVal--;` |
|      ! 0 |  3914 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3915 | `				}` |
|      ! 0 |  3916 | `			}` |
|        - |  3917 | `		}` |
|        2 |  3918 | `	}` |
|        5 |  3919 | `	break;` |
|        - |  3920 | `/*` |
|        - |  3921 | ` * UMINUS: * * *` |
|        - |  3922 | ` *` |
|        - |  3923 | ` * Perform a unary minus operation.` |
|        - |  3924 | ` */` |
|    24632 |  3925 | `case PH7_OP_UMINUS:` |
|        - |  3926 | `#ifdef UNTRUST` |
|        - |  3927 | `	if( pTos < pStack ){` |
|        - |  3928 | `		goto Abort;` |
|        - |  3929 | `	}` |
|        - |  3930 | `#endif` |
|        - |  3931 | `	/* Force a numeric (integer,real or both) cast */` |
|    49266 |  3932 | `	PH7_MemObjToNumeric(pTos);` |
|    49266 |  3933 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  3934 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3935 | `	}` |
|    49266 |  3936 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    49236 |  3937 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    24617 |  3938 | `	}` |
|    49266 |  3939 | `	break;` |
|        - |  3940 | `/*` |
|        - |  3941 | ` * UPLUS: * * *` |
|        - |  3942 | ` *` |
|        - |  3943 | ` * Perform a unary plus operation.` |
|        - |  3944 | ` */` |
|       17 |  3945 | `case PH7_OP_UPLUS:` |
|        - |  3946 | `#ifdef UNTRUST` |
|        - |  3947 | `	if( pTos < pStack ){` |
|        - |  3948 | `		goto Abort;` |
|        - |  3949 | `	}` |
|        - |  3950 | `#endif` |
|        - |  3951 | `	/* Force a numeric (integer,real or both) cast */` |
|       35 |  3952 | `	PH7_MemObjToNumeric(pTos);` |
|       35 |  3953 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3954 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3955 | `	}` |
|       35 |  3956 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       35 |  3957 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       17 |  3958 | `	}` |
|       35 |  3959 | `	break;` |
|        - |  3960 | `/*` |
|        - |  3961 | ` * OP_LNOT: * * *` |
|        - |  3962 | ` *` |
|        - |  3963 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3964 | ` * with its complement.` |
|        - |  3965 | ` */` |
|    40523 |  3966 | `case PH7_OP_LNOT:` |
|        - |  3967 | `#ifdef UNTRUST` |
|        - |  3968 | `	if( pTos < pStack ){` |
|        - |  3969 | `		goto Abort;` |
|        - |  3970 | `	}` |
|        - |  3971 | `#endif` |
|        - |  3972 | `	/* Force a boolean cast */` |
|    81092 |  3973 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3974 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3975 | `	}` |
|    81092 |  3976 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    81092 |  3977 | `	break;` |
|        - |  3978 | `/*` |
|        - |  3979 | ` * OP_BITNOT: * * *` |
|        - |  3980 | ` *` |
|        - |  3981 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3982 | ` * with its ones-complement.` |
|        - |  3983 | ` */` |
|       13 |  3984 | `case PH7_OP_BITNOT:` |
|        - |  3985 | `#ifdef UNTRUST` |
|        - |  3986 | `	if( pTos < pStack ){` |
|        - |  3987 | `		goto Abort;` |
|        - |  3988 | `	}` |
|        - |  3989 | `#endif` |
|        - |  3990 | `	/* Force an integer cast */` |
|       28 |  3991 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3992 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3993 | `	}` |
|       28 |  3994 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  3995 | `	break;` |
|        - |  3996 | `/* OP_MUL * * *` |
|        - |  3997 | ` * OP_MUL_STORE * * *` |
|        - |  3998 | ` *` |
|        - |  3999 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  4000 | ` * and push the result back onto the stack.` |
|        - |  4001 | ` */` |
|     1249 |  4002 | `case PH7_OP_MUL:` |
|        - |  4003 | `case PH7_OP_MUL_STORE: {` |
|     2500 |  4004 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4005 | `	/* Force the operand to be numeric */` |
|        - |  4006 | `#ifdef UNTRUST` |
|        - |  4007 | `	if( pNos < pStack ){` |
|        - |  4008 | `		goto Abort;` |
|        - |  4009 | `	}` |
|        - |  4010 | `#endif` |
|     2500 |  4011 | `	PH7_MemObjToNumeric(pTos);` |
|     2500 |  4012 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  4013 | `	/* Perform the requested operation */` |
|     2500 |  4014 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4015 | `		/* Floating point arithemic */` |
|        - |  4016 | `		ph7_real a,b,r;` |
|       17 |  4017 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4018 | `			PH7_MemObjToReal(pTos);` |
|        3 |  4019 | `		}` |
|       17 |  4020 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4021 | `			PH7_MemObjToReal(pNos);` |
|        3 |  4022 | `		}` |
|       17 |  4023 | `		a = pNos->rVal;` |
|       17 |  4024 | `		b = pTos->rVal;` |
|       17 |  4025 | `		r = a * b;` |
|        - |  4026 | `		/* Push the result */` |
|       17 |  4027 | `		pNos->rVal = r;` |
|       17 |  4028 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4029 | `		/* Try to get an integer representation */` |
|       17 |  4030 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  4031 | `	}else{` |
|        - |  4032 | `		/* Integer arithmetic */` |
|        - |  4033 | `		sxi64 a,b,r;` |
|     2484 |  4034 | `		a = pNos->x.iVal;` |
|     2484 |  4035 | `		b = pTos->x.iVal;` |
|     2484 |  4036 | `		r = a * b;` |
|        - |  4037 | `		/* Push the result */` |
|     2484 |  4038 | `		pNos->x.iVal = r;` |
|     2484 |  4039 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4040 | `	}` |
|     2500 |  4041 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4042 | `		ph7_value *pObj;` |
|       27 |  4043 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4044 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       27 |  4045 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  4046 | `			PH7_MemObjStore(pNos,pObj);` |
|       13 |  4047 | `		}` |
|       13 |  4048 | `	}` |
|     2500 |  4049 | `	VmPopOperand(&pTos,1);` |
|     2500 |  4050 | `	break;` |
|        - |  4051 | `				 }` |
|        - |  4052 | `/* OP_ADD * * *` |
|        - |  4053 | ` *` |
|        - |  4054 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4055 | ` * and push the result back onto the stack.` |
|        - |  4056 | ` */` |
|      452 |  4057 | `case PH7_OP_ADD:{` |
|      906 |  4058 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4059 | `#ifdef UNTRUST` |
|        - |  4060 | `	if( pNos < pStack ){` |
|        - |  4061 | `		goto Abort;` |
|        - |  4062 | `	}` |
|        - |  4063 | `#endif` |
|        - |  4064 | `	/* Perform the addition */` |
|      906 |  4065 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      906 |  4066 | `	VmPopOperand(&pTos,1);` |
|      906 |  4067 | `	break;` |
|        - |  4068 | `				}` |
|        - |  4069 | `/*` |
|        - |  4070 | ` * OP_ADD_STORE * * *` |
|        - |  4071 | ` *` |
|        - |  4072 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4073 | ` * and push the result back onto the stack.` |
|        - |  4074 | ` */` |
|      495 |  4075 | `case PH7_OP_ADD_STORE:{` |
|      992 |  4076 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4077 | `	ph7_value *pObj;` |
|        - |  4078 | `	sxu32 nIdx;` |
|        - |  4079 | `#ifdef UNTRUST` |
|        - |  4080 | `	if( pNos < pStack ){` |
|        - |  4081 | `		goto Abort;` |
|        - |  4082 | `	}` |
|        - |  4083 | `#endif` |
|        - |  4084 | `	/* Perform the addition */` |
|      992 |  4085 | `	nIdx = pTos->nIdx;` |
|      992 |  4086 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4087 | `	/* Peform the store operation */` |
|      992 |  4088 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4089 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      992 |  4090 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      992 |  4091 | `		PH7_MemObjStore(pTos,pObj);` |
|      495 |  4092 | `	}` |
|        - |  4093 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      992 |  4094 | `	PH7_MemObjStore(pTos,pNos);` |
|      992 |  4095 | `	VmPopOperand(&pTos,1);` |
|      992 |  4096 | `	break;` |
|        - |  4097 | `				}` |
|        - |  4098 | `/* OP_SUB * * *` |
|        - |  4099 | ` *` |
|        - |  4100 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4101 | ` * first (what was next on the stack) from the second (the` |
|        - |  4102 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4103 | ` */` |
|      301 |  4104 | `case PH7_OP_SUB: {` |
|      604 |  4105 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4106 | `#ifdef UNTRUST` |
|        - |  4107 | `	if( pNos < pStack ){` |
|        - |  4108 | `		goto Abort;` |
|        - |  4109 | `	}` |
|        - |  4110 | `#endif` |
|      604 |  4111 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4112 | `		/* Floating point arithemic */` |
|        - |  4113 | `		ph7_real a,b,r;` |
|       95 |  4114 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4115 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4116 | `		}` |
|       95 |  4117 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4118 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4119 | `		}` |
|       95 |  4120 | `		a = pNos->rVal;` |
|       95 |  4121 | `		b = pTos->rVal;` |
|       95 |  4122 | `		r = a - b;` |
|        - |  4123 | `		/* Push the result */` |
|       95 |  4124 | `		pNos->rVal = r;` |
|       95 |  4125 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4126 | `		/* Try to get an integer representation */` |
|       95 |  4127 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4128 | `	}else{` |
|        - |  4129 | `		/* Integer arithmetic */` |
|        - |  4130 | `		sxi64 a,b,r;` |
|      510 |  4131 | `		a = pNos->x.iVal;` |
|      510 |  4132 | `		b = pTos->x.iVal;` |
|      510 |  4133 | `		r = a - b;` |
|        - |  4134 | `		/* Push the result */` |
|      510 |  4135 | `		pNos->x.iVal = r;` |
|      510 |  4136 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4137 | `	}` |
|      604 |  4138 | `	VmPopOperand(&pTos,1);` |
|      604 |  4139 | `	break;` |
|        - |  4140 | `				 }` |
|        - |  4141 | `/* OP_SUB_STORE * * *` |
|        - |  4142 | ` *` |
|        - |  4143 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4144 | ` * first (what was next on the stack) from the second (the` |
|        - |  4145 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4146 | ` */` |
|        2 |  4147 | `case PH7_OP_SUB_STORE: {` |
|        5 |  4148 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4149 | `	ph7_value *pObj;` |
|        - |  4150 | `#ifdef UNTRUST` |
|        - |  4151 | `	if( pNos < pStack ){` |
|        - |  4152 | `		goto Abort;` |
|        - |  4153 | `	}` |
|        - |  4154 | `#endif` |
|        5 |  4155 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4156 | `		/* Floating point arithemic */` |
|        - |  4157 | `		ph7_real a,b,r;` |
|      ! 0 |  4158 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4159 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4160 | `		}` |
|      ! 0 |  4161 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4162 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4163 | `		}` |
|      ! 0 |  4164 | `		a = pTos->rVal;` |
|      ! 0 |  4165 | `		b = pNos->rVal;` |
|      ! 0 |  4166 | `		r = a - b;` |
|        - |  4167 | `		/* Push the result */` |
|      ! 0 |  4168 | `		pNos->rVal = r;` |
|      ! 0 |  4169 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4170 | `		/* Try to get an integer representation */` |
|      ! 0 |  4171 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4172 | `	}else{` |
|        - |  4173 | `		/* Integer arithmetic */` |
|        - |  4174 | `		sxi64 a,b,r;` |
|        5 |  4175 | `		a = pTos->x.iVal;` |
|        5 |  4176 | `		b = pNos->x.iVal;` |
|        5 |  4177 | `		r = a - b;` |
|        - |  4178 | `		/* Push the result */` |
|        5 |  4179 | `		pNos->x.iVal = r;` |
|        5 |  4180 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4181 | `	}` |
|        5 |  4182 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4183 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  4184 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  4185 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  4186 | `	}` |
|        5 |  4187 | `	VmPopOperand(&pTos,1);` |
|        5 |  4188 | `	break;` |
|        - |  4189 | `				 }` |
|        - |  4190 |  |
|        - |  4191 | `/*` |
|        - |  4192 | ` * OP_MOD * * *` |
|        - |  4193 | ` *` |
|        - |  4194 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4195 | ` * first (what was next on the stack) from the second (the` |
|        - |  4196 | ` * top of the stack) and push the remainder after division` |
|        - |  4197 | ` * onto the stack.` |
|        - |  4198 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4199 | ` */` |
|      306 |  4200 | `case PH7_OP_MOD:{` |
|      614 |  4201 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4202 | `	sxi64 a,b,r;` |
|        - |  4203 | `#ifdef UNTRUST` |
|        - |  4204 | `	if( pNos < pStack ){` |
|        - |  4205 | `		goto Abort;` |
|        - |  4206 | `	}` |
|        - |  4207 | `#endif` |
|        - |  4208 | `	/* Force the operands to be integer */` |
|      614 |  4209 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4210 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4211 | `	}` |
|      614 |  4212 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4213 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4214 | `	}` |
|        - |  4215 | `	/* Perform the requested operation */` |
|      614 |  4216 | `	a = pNos->x.iVal;` |
|      614 |  4217 | `	b = pTos->x.iVal;` |
|      614 |  4218 | `	if( b == 0 ){` |
|        3 |  4219 | `		r = 0;` |
|        3 |  4220 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4221 | `		/* goto Abort; */` |
|        2 |  4222 | `	}else{` |
|      611 |  4223 | `		r = a%b;` |
|        - |  4224 | `	}` |
|        - |  4225 | `	/* Push the result */` |
|      614 |  4226 | `	pNos->x.iVal = r;` |
|      614 |  4227 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      614 |  4228 | `	VmPopOperand(&pTos,1);` |
|      614 |  4229 | `	break;` |
|        - |  4230 | `				}` |
|        - |  4231 | `/*` |
|        - |  4232 | ` * OP_MOD_STORE * * *` |
|        - |  4233 | ` *` |
|        - |  4234 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4235 | ` * first (what was next on the stack) from the second (the` |
|        - |  4236 | ` * top of the stack) and push the remainder after division` |
|        - |  4237 | ` * onto the stack.` |
|        - |  4238 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4239 | ` */` |
|        1 |  4240 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4241 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4242 | `	ph7_value *pObj;` |
|        - |  4243 | `	sxi64 a,b,r;` |
|        - |  4244 | `#ifdef UNTRUST` |
|        - |  4245 | `	if( pNos < pStack ){` |
|        - |  4246 | `		goto Abort;` |
|        - |  4247 | `	}` |
|        - |  4248 | `#endif` |
|        - |  4249 | `	/* Force the operands to be integer */` |
|        3 |  4250 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4251 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4252 | `	}` |
|        3 |  4253 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4254 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4255 | `	}` |
|        - |  4256 | `	/* Perform the requested operation */` |
|        3 |  4257 | `	a = pTos->x.iVal;` |
|        3 |  4258 | `	b = pNos->x.iVal;` |
|        3 |  4259 | `	if( b == 0 ){` |
|      ! 0 |  4260 | `		r = 0;` |
|      ! 0 |  4261 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4262 | `		/* goto Abort; */` |
|      ! 0 |  4263 | `	}else{` |
|        3 |  4264 | `		r = a%b;` |
|        - |  4265 | `	}` |
|        - |  4266 | `	/* Push the result */` |
|        3 |  4267 | `	pNos->x.iVal = r;` |
|        3 |  4268 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4269 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4270 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4271 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4272 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4273 | `	}` |
|        3 |  4274 | `	VmPopOperand(&pTos,1);` |
|        3 |  4275 | `	break;` |
|        - |  4276 | `				}` |
|        - |  4277 | `/*` |
|        - |  4278 | ` * OP_DIV * * *` |
|        - |  4279 | ` *` |
|        - |  4280 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4281 | ` * first (what was next on the stack) from the second (the` |
|        - |  4282 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4283 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4284 | ` */` |
|       29 |  4285 | `case PH7_OP_DIV:{` |
|       60 |  4286 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4287 | `	ph7_real a,b,r;` |
|        - |  4288 | `#ifdef UNTRUST` |
|        - |  4289 | `	if( pNos < pStack ){` |
|        - |  4290 | `		goto Abort;` |
|        - |  4291 | `	}` |
|        - |  4292 | `#endif` |
|        - |  4293 | `	/* Force the operands to be real */` |
|       60 |  4294 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 |  4295 | `		PH7_MemObjToReal(pTos);` |
|       27 |  4296 | `	}` |
|       60 |  4297 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       22 |  4298 | `		PH7_MemObjToReal(pNos);` |
|       10 |  4299 | `	}` |
|        - |  4300 | `	/* Perform the requested operation */` |
|       60 |  4301 | `	a = pNos->rVal;` |
|       60 |  4302 | `	b = pTos->rVal;` |
|       60 |  4303 | `	if( b == 0 ){` |
|        - |  4304 | `		/* Division by zero */` |
|        3 |  4305 | `		pNos->rVal = 0;` |
|        3 |  4306 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4307 | `		/* goto Abort; */` |
|        2 |  4308 | `	}else{` |
|       57 |  4309 | `		r = a/b;` |
|        - |  4310 | `		/* Push the result */` |
|       57 |  4311 | `		pNos->rVal = r;` |
|       57 |  4312 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4313 | `		/* Try to get an integer representation */` |
|       57 |  4314 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4315 | `	}` |
|       60 |  4316 | `	VmPopOperand(&pTos,1);` |
|       60 |  4317 | `	break;` |
|        - |  4318 | `				}` |
|        - |  4319 | `/*` |
|        - |  4320 | ` * OP_DIV_STORE * * *` |
|        - |  4321 | ` *` |
|        - |  4322 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4323 | ` * first (what was next on the stack) from the second (the` |
|        - |  4324 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4325 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4326 | ` */` |
|        1 |  4327 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4328 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4329 | `	ph7_value *pObj;` |
|        - |  4330 | `	ph7_real a,b,r;` |
|        - |  4331 | `#ifdef UNTRUST` |
|        - |  4332 | `	if( pNos < pStack ){` |
|        - |  4333 | `		goto Abort;` |
|        - |  4334 | `	}` |
|        - |  4335 | `#endif` |
|        - |  4336 | `	/* Force the operands to be real */` |
|        3 |  4337 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4338 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4339 | `	}` |
|        3 |  4340 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4341 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4342 | `	}` |
|        - |  4343 | `	/* Perform the requested operation */` |
|        3 |  4344 | `	a = pTos->rVal;` |
|        3 |  4345 | `	b = pNos->rVal;` |
|        3 |  4346 | `	if( b == 0 ){` |
|        - |  4347 | `		/* Division by zero */` |
|      ! 0 |  4348 | `		r = 0;` |
|      ! 0 |  4349 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4350 | `		/* goto Abort; */` |
|      ! 0 |  4351 | `	}else{` |
|        3 |  4352 | `		r = a/b;` |
|        - |  4353 | `		/* Push the result */` |
|        3 |  4354 | `		pNos->rVal = r;` |
|        3 |  4355 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4356 | `		/* Try to get an integer representation */` |
|        3 |  4357 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4358 | `	}` |
|        3 |  4359 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4360 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4361 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4362 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4363 | `	}` |
|        3 |  4364 | `	VmPopOperand(&pTos,1);` |
|        3 |  4365 | `	break;` |
|        - |  4366 | `				}` |
|        - |  4367 | `/* OP_BAND * * *` |
|        - |  4368 | ` *` |
|        - |  4369 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4370 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4371 | ` * two elements.` |
|        - |  4372 | `*/` |
|        - |  4373 | `/* OP_BOR * * *` |
|        - |  4374 | ` *` |
|        - |  4375 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4376 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4377 | ` * two elements.` |
|        - |  4378 | ` */` |
|        - |  4379 | `/* OP_BXOR * * *` |
|        - |  4380 | ` *` |
|        - |  4381 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4382 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4383 | ` * two elements.` |
|        - |  4384 | ` */` |
|       44 |  4385 | `case PH7_OP_BAND:` |
|        - |  4386 | `case PH7_OP_BOR:` |
|        - |  4387 | `case PH7_OP_BXOR:{` |
|       90 |  4388 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4389 | `	sxi64 a,b,r;` |
|        - |  4390 | `#ifdef UNTRUST` |
|        - |  4391 | `	if( pNos < pStack ){` |
|        - |  4392 | `		goto Abort;` |
|        - |  4393 | `	}` |
|        - |  4394 | `#endif` |
|        - |  4395 | `	/* Force the operands to be integer */` |
|       90 |  4396 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4397 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4398 | `	}` |
|       90 |  4399 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4400 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4401 | `	}` |
|        - |  4402 | `	/* Perform the requested operation */` |
|       90 |  4403 | `	a = pNos->x.iVal;` |
|       90 |  4404 | `	b = pTos->x.iVal;` |
|       90 |  4405 | `	switch(pInstr->iOp){` |
|        7 |  4406 | `	case PH7_OP_BOR_STORE:` |
|       15 |  4407 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  4408 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  4409 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  4410 | `	case PH7_OP_BAND_STORE:` |
|       30 |  4411 | `	case PH7_OP_BAND:` |
|       62 |  4412 | `	default:          r = a&b; break;` |
|        - |  4413 | `	}` |
|        - |  4414 | `	/* Push the result */` |
|       90 |  4415 | `	pNos->x.iVal = r;` |
|       90 |  4416 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  4417 | `	VmPopOperand(&pTos,1);` |
|       90 |  4418 | `	break;` |
|        - |  4419 | `				 }` |
|        - |  4420 | `/* OP_BAND_STORE * * *` |
|        - |  4421 | ` *` |
|        - |  4422 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4423 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4424 | ` * two elements.` |
|        - |  4425 | `*/` |
|        - |  4426 | `/* OP_BOR_STORE * * *` |
|        - |  4427 | ` *` |
|        - |  4428 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4429 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4430 | ` * two elements.` |
|        - |  4431 | ` */` |
|        - |  4432 | `/* OP_BXOR_STORE * * *` |
|        - |  4433 | ` *` |
|        - |  4434 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4435 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4436 | ` * two elements.` |
|        - |  4437 | ` */` |
|       10 |  4438 | `case PH7_OP_BAND_STORE:` |
|        - |  4439 | `case PH7_OP_BOR_STORE:` |
|        - |  4440 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  4441 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4442 | `	ph7_value *pObj;` |
|        - |  4443 | `	sxi64 a,b,r;` |
|        - |  4444 | `#ifdef UNTRUST` |
|        - |  4445 | `	if( pNos < pStack ){` |
|        - |  4446 | `		goto Abort;` |
|        - |  4447 | `	}` |
|        - |  4448 | `#endif` |
|        - |  4449 | `	/* Force the operands to be integer */` |
|       21 |  4450 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4451 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4452 | `	}` |
|       21 |  4453 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4454 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4455 | `	}` |
|        - |  4456 | `	/* Perform the requested operation */` |
|       21 |  4457 | `	a = pTos->x.iVal;` |
|       21 |  4458 | `	b = pNos->x.iVal;` |
|       21 |  4459 | `	switch(pInstr->iOp){` |
|        3 |  4460 | `	case PH7_OP_BOR_STORE:` |
|        7 |  4461 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  4462 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  4463 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  4464 | `	case PH7_OP_BAND_STORE:` |
|        3 |  4465 | `	case PH7_OP_BAND:` |
|        7 |  4466 | `	default:          r = a&b; break;` |
|        - |  4467 | `	}` |
|        - |  4468 | `	/* Push the result */` |
|       21 |  4469 | `	pNos->x.iVal = r;` |
|       21 |  4470 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  4471 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4472 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  4473 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  4474 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  4475 | `	}` |
|       21 |  4476 | `	VmPopOperand(&pTos,1);` |
|       21 |  4477 | `	break;` |
|        - |  4478 | `				 }` |
|        - |  4479 | `/* OP_SHL * * *` |
|        - |  4480 | ` *` |
|        - |  4481 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4482 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4483 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4484 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4485 | ` */` |
|        - |  4486 | `/* OP_SHR * * *` |
|        - |  4487 | ` *` |
|        - |  4488 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4489 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4490 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4491 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4492 | ` */` |
|       12 |  4493 | `case PH7_OP_SHL:` |
|        - |  4494 | `case PH7_OP_SHR: {` |
|       25 |  4495 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4496 | `	sxi64 a,r;` |
|        - |  4497 | `	sxi32 b;` |
|        - |  4498 | `#ifdef UNTRUST` |
|        - |  4499 | `	if( pNos < pStack ){` |
|        - |  4500 | `		goto Abort;` |
|        - |  4501 | `	}` |
|        - |  4502 | `#endif` |
|        - |  4503 | `	/* Force the operands to be integer */` |
|       25 |  4504 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4505 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4506 | `	}` |
|       25 |  4507 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4508 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4509 | `	}` |
|        - |  4510 | `	/* Perform the requested operation */` |
|       25 |  4511 | `	a = pNos->x.iVal;` |
|       25 |  4512 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  4513 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  4514 | `		r = a << b;` |
|        8 |  4515 | `	}else{` |
|       11 |  4516 | `		r = a >> b;` |
|        - |  4517 | `	}` |
|        - |  4518 | `	/* Push the result */` |
|       25 |  4519 | `	pNos->x.iVal = r;` |
|       25 |  4520 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  4521 | `	VmPopOperand(&pTos,1);` |
|       25 |  4522 | `	break;` |
|        - |  4523 | `				 }` |
|        - |  4524 | `/*  OP_SHL_STORE * * *` |
|        - |  4525 | ` *` |
|        - |  4526 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4527 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4528 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4529 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4530 | ` */` |
|        - |  4531 | `/* OP_SHR_STORE * * *` |
|        - |  4532 | ` *` |
|        - |  4533 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4534 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4535 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4536 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4537 | ` */` |
|        9 |  4538 | `case PH7_OP_SHL_STORE:` |
|        - |  4539 | `case PH7_OP_SHR_STORE: {` |
|       19 |  4540 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4541 | `	ph7_value *pObj;` |
|        - |  4542 | `	sxi64 a,r;` |
|        - |  4543 | `	sxi32 b;` |
|        - |  4544 | `#ifdef UNTRUST` |
|        - |  4545 | `	if( pNos < pStack ){` |
|        - |  4546 | `		goto Abort;` |
|        - |  4547 | `	}` |
|        - |  4548 | `#endif` |
|        - |  4549 | `	/* Force the operands to be integer */` |
|       19 |  4550 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4551 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4552 | `	}` |
|       19 |  4553 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4554 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4555 | `	}` |
|        - |  4556 | `	/* Perform the requested operation */` |
|       19 |  4557 | `	a = pTos->x.iVal;` |
|       19 |  4558 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  4559 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  4560 | `		r = a << b;` |
|        5 |  4561 | `	}else{` |
|       11 |  4562 | `		r = a >> b;` |
|        - |  4563 | `	}` |
|        - |  4564 | `	/* Push the result */` |
|       19 |  4565 | `	pNos->x.iVal = r;` |
|       19 |  4566 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4567 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4568 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  4569 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  4570 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  4571 | `	}` |
|       19 |  4572 | `	VmPopOperand(&pTos,1);` |
|       19 |  4573 | `	break;` |
|        - |  4574 | `				 }` |
|        - |  4575 | `/* CAT:  P1 * *` |
|        - |  4576 | ` *` |
|        - |  4577 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4578 | ` * back.` |
|        - |  4579 | ` */` |
|    63983 |  4580 | `case PH7_OP_CAT:{` |
|        - |  4581 | `	ph7_value *pNos,*pCur;` |
|   127968 |  4582 | `	if( pInstr->iP1 < 1 ){` |
|   100886 |  4583 | `		pNos = &pTos[-1];` |
|    50444 |  4584 | `	}else{` |
|    27084 |  4585 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4586 | `	}` |
|        - |  4587 | `#ifdef UNTRUST` |
|        - |  4588 | `	if( pNos < pStack ){` |
|        - |  4589 | `		goto Abort;` |
|        - |  4590 | `	}` |
|        - |  4591 | `#endif` |
|        - |  4592 | `	/* Force a string cast */` |
|   127968 |  4593 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1392 |  4594 | `		PH7_MemObjToString(pNos);` |
|      695 |  4595 | `	}` |
|   127968 |  4596 | `	pCur = &pNos[1];` |
|   258062 |  4597 | `	while( pCur <= pTos ){` |
|   130096 |  4598 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50724 |  4599 | `			PH7_MemObjToString(pCur);` |
|    25361 |  4600 | `		}` |
|        - |  4601 | `		/* Perform the concatenation */` |
|   130096 |  4602 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   130058 |  4603 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    65028 |  4604 | `		}` |
|   130096 |  4605 | `		SyBlobRelease(&pCur->sBlob);` |
|   130096 |  4606 | `		pCur++;` |
|        2 |  4607 | `	}` |
|   127968 |  4608 | `	pTos = pNos;` |
|   127968 |  4609 | `	break;` |
|        - |  4610 | `				}` |
|        - |  4611 | `/*  CAT_STORE: * * *` |
|        - |  4612 | ` *` |
|        - |  4613 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4614 | ` * back.` |
|        - |  4615 | ` */` |
|     3423 |  4616 | `case PH7_OP_CAT_STORE:{` |
|     6848 |  4617 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4618 | `	ph7_value *pObj;` |
|        - |  4619 | `#ifdef UNTRUST` |
|        - |  4620 | `	if( pNos < pStack ){` |
|        - |  4621 | `		goto Abort;` |
|        - |  4622 | `	}` |
|        - |  4623 | `#endif` |
|     6848 |  4624 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4625 | `		/* Force a string cast */` |
|      ! 0 |  4626 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4627 | `	}` |
|     6848 |  4628 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4629 | `		/* Force a string cast */` |
|      ! 0 |  4630 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4631 | `	}` |
|        - |  4632 | `	/* Perform the concatenation (Reverse order) */` |
|     6848 |  4633 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6848 |  4634 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3423 |  4635 | `	}` |
|        - |  4636 | `	/* Perform the store operation */` |
|     6848 |  4637 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4638 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6848 |  4639 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6848 |  4640 | `		PH7_MemObjStore(pTos,pObj);` |
|     3423 |  4641 | `	}` |
|     6848 |  4642 | `	PH7_MemObjStore(pTos,pNos);` |
|     6848 |  4643 | `	VmPopOperand(&pTos,1);` |
|     6848 |  4644 | `	break;` |
|        - |  4645 | `				}` |
|        - |  4646 | `/* OP_AND: * * *` |
|        - |  4647 | ` *` |
|        - |  4648 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4649 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4650 | ` * stack.` |
|        - |  4651 | ` */` |
|        - |  4652 | `/* OP_OR: * * *` |
|        - |  4653 | ` *` |
|        - |  4654 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4655 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4656 | ` * stack.` |
|        - |  4657 | ` */` |
|    95953 |  4658 | `case PH7_OP_LAND:` |
|        - |  4659 | `case PH7_OP_LOR: {` |
|   191952 |  4660 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4661 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4662 | `#ifdef UNTRUST` |
|        - |  4663 | `	if( pNos < pStack ){` |
|        - |  4664 | `		goto Abort;` |
|        - |  4665 | `	}` |
|        - |  4666 | `#endif` |
|        - |  4667 | `	/* Force a boolean cast */` |
|   191952 |  4668 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4669 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4670 | `	}` |
|   191952 |  4671 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4672 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4673 | `	}` |
|   191952 |  4674 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   191952 |  4675 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   191952 |  4676 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4677 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    88560 |  4678 | `		v1 = and_logic[v1*3+v2];` |
|    44303 |  4679 | `	}else{` |
|        - |  4680 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   103394 |  4681 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4682 | `	}` |
|   191952 |  4683 | `	if( v1 == 2 ){` |
|      ! 0 |  4684 | `		v1 = 1;` |
|      ! 0 |  4685 | `	}` |
|   191952 |  4686 | `	VmPopOperand(&pTos,1);` |
|   191952 |  4687 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   191952 |  4688 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   191952 |  4689 | `	break;` |
|        - |  4690 | `				 }` |
|        - |  4691 | `/*` |
|        - |  4692 | ` * OP_NULLC: * * *` |
|        - |  4693 | ` * Null coalescing operator '??'.` |
|        - |  4694 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  4695 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  4696 | ` */` |
|        - |  4697 | `/*` |
|        - |  4698 | ` * OP_NULLC: * P2 *` |
|        - |  4699 | ` * Short-circuit null coalescing '??'.` |
|        - |  4700 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  4701 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  4702 | ` */` |
|       19 |  4703 | `case PH7_OP_NULLC: {` |
|        - |  4704 | `#ifdef UNTRUST` |
|        - |  4705 | `	if( pTos < pStack ){` |
|        - |  4706 | `		goto Abort;` |
|        - |  4707 | `	}` |
|        - |  4708 | `#endif` |
|       40 |  4709 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  4710 | `		/* Left is not null — keep it and skip the RHS */` |
|       18 |  4711 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  4712 | `	}else{` |
|        - |  4713 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       24 |  4714 | `		VmPopOperand(&pTos, 1);` |
|        - |  4715 | `	}` |
|       40 |  4716 | `	break;` |
|        - |  4717 |  |
|        - |  4718 | `/*` |
|        - |  4719 | ` * OP_SPREAD: * * *` |
|        - |  4720 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  4721 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  4722 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  4723 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  4724 | ` */` |
|        7 |  4725 | `case PH7_OP_SPREAD: {` |
|        - |  4726 | `#ifdef UNTRUST` |
|        - |  4727 | `	if( pTos < pStack ){` |
|        - |  4728 | `		goto Abort;` |
|        - |  4729 | `	}` |
|        - |  4730 | `#endif` |
|       15 |  4731 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       15 |  4732 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       15 |  4733 | `		sxu32 nEntry = pMap->nEntry;` |
|       15 |  4734 | `		if( nEntry == 0 ){` |
|        - |  4735 | `			/* Empty array — remove from stack */` |
|        3 |  4736 | `			VmPopOperand(&pTos, 1);` |
|        3 |  4737 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       14 |  4738 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  4739 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  4740 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  4741 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  4742 | `				VM_STACK_GUARD);` |
|      ! 0 |  4743 | `		}else{` |
|        - |  4744 | `			ph7_hashmap_node *pNode2;` |
|        - |  4745 | `			ph7_value *pElem;` |
|        - |  4746 | `			sxu32 i;` |
|        - |  4747 | `			/* Overwrite TOS with first element */` |
|       13 |  4748 | `			pNode2 = pMap->pFirst;` |
|       13 |  4749 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       13 |  4750 | `			PH7_MemObjRelease(pTos);` |
|       13 |  4751 | `			if( pElem ){` |
|       13 |  4752 | `				PH7_MemObjLoad(pElem, pTos);` |
|        6 |  4753 | `			}` |
|       13 |  4754 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  4755 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  4756 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       13 |  4757 | `			pNode2 = pNode2->pPrev;` |
|        - |  4758 | `			/* Push remaining elements */` |
|       33 |  4759 | `			for( i = 1; i < nEntry; i++ ){` |
|       21 |  4760 | `				pTos++;` |
|       21 |  4761 | `				PH7_MemObjInit(pVm, pTos);` |
|       21 |  4762 | `				pTos->nIdx = SXU32_HIGH;` |
|       21 |  4763 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       21 |  4764 | `				if( pElem ){` |
|       21 |  4765 | `					PH7_MemObjLoad(pElem, pTos);` |
|       10 |  4766 | `				}` |
|       21 |  4767 | `				pNode2 = pNode2->pPrev;` |
|       11 |  4768 | `			}` |
|       13 |  4769 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  4770 | `		}` |
|        7 |  4771 | `	}` |
|        - |  4772 | `	/* else: not an array — leave as-is (single arg) */` |
|       15 |  4773 | `	break;` |
|        - |  4774 |  |
|        - |  4775 | `/* OP_LXOR: * * *` |
|        - |  4776 | ` *` |
|        - |  4777 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4778 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4779 | ` * stack.` |
|        - |  4780 | ` * According to the PHP language reference manual:` |
|        - |  4781 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4782 | ` *  TRUE,but not both.` |
|        - |  4783 | ` */` |
|        5 |  4784 | `case PH7_OP_LXOR:{` |
|       11 |  4785 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4786 | `	sxi32 v = 0;` |
|        - |  4787 | `#ifdef UNTRUST` |
|        - |  4788 | `	if( pNos < pStack ){` |
|        - |  4789 | `		goto Abort;` |
|        - |  4790 | `	}` |
|        - |  4791 | `#endif` |
|        - |  4792 | `	/* Force a boolean cast */` |
|       11 |  4793 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4794 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4795 | `	}` |
|       11 |  4796 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4797 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4798 | `	}` |
|       11 |  4799 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4800 | `		v = 1;` |
|        3 |  4801 | `	}` |
|       11 |  4802 | `	VmPopOperand(&pTos,1);` |
|       11 |  4803 | `	pTos->x.iVal = v;` |
|       11 |  4804 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4805 | `	break;` |
|        - |  4806 | `				 }` |
|        - |  4807 | `/* OP_EQ P1 P2 P3` |
|        - |  4808 | ` *` |
|        - |  4809 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4810 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4811 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4812 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4813 | ` */` |
|        - |  4814 | `/* OP_NEQ P1 P2 P3` |
|        - |  4815 | ` *` |
|        - |  4816 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4817 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4818 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4819 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4820 | ` */` |
|     4033 |  4821 | `case PH7_OP_EQ:` |
|        - |  4822 | `case PH7_OP_NEQ: {` |
|     8068 |  4823 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4824 | `	/* Perform the comparison and act accordingly */` |
|        - |  4825 | `#ifdef UNTRUST` |
|        - |  4826 | `	if( pNos < pStack ){` |
|        - |  4827 | `		goto Abort;` |
|        - |  4828 | `	}` |
|        - |  4829 | `#endif` |
|     8068 |  4830 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8068 |  4831 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  4832 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8059 |  4833 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8024 |  4834 | `		rc = rc == 0;` |
|     4013 |  4835 | `	}else{` |
|       28 |  4836 | `		rc = rc != 0;` |
|        - |  4837 | `	}` |
|     8068 |  4838 | `	VmPopOperand(&pTos,1);` |
|     8068 |  4839 | `	if( !pInstr->iP2 ){` |
|        - |  4840 | `		/* Push comparison result without taking the jump */` |
|     8068 |  4841 | `		PH7_MemObjRelease(pTos);` |
|     8068 |  4842 | `		pTos->x.iVal = rc;` |
|        - |  4843 | `		/* Invalidate any prior representation */` |
|     8068 |  4844 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4035 |  4845 | `	}else{` |
|      ! 0 |  4846 | `		if( rc ){` |
|        - |  4847 | `			/* Jump to the desired location */` |
|      ! 0 |  4848 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4849 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4850 | `		}` |
|        - |  4851 | `	}` |
|     8068 |  4852 | `	break;` |
|        - |  4853 | `				 }` |
|        - |  4854 | `/* OP_TEQ P1 P2 *` |
|        - |  4855 | ` *` |
|        - |  4856 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4857 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4858 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4859 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4860 | ` */` |
|   135268 |  4861 | `case PH7_OP_TEQ: {` |
|   270538 |  4862 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4863 | `	/* Perform the comparison and act accordingly */` |
|        - |  4864 | `#ifdef UNTRUST` |
|        - |  4865 | `	if( pNos < pStack ){` |
|        - |  4866 | `		goto Abort;` |
|        - |  4867 | `	}` |
|        - |  4868 | `#endif` |
|   270538 |  4869 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   270538 |  4870 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4871 | `		rc = 0;` |
|        2 |  4872 | `	}else{` |
|   270536 |  4873 | `		rc = rc == 0;` |
|        - |  4874 | `	}` |
|   270538 |  4875 | `	VmPopOperand(&pTos,1);` |
|   270538 |  4876 | `	if( !pInstr->iP2 ){` |
|        - |  4877 | `		/* Push comparison result without taking the jump */` |
|   270538 |  4878 | `		PH7_MemObjRelease(pTos);` |
|   270538 |  4879 | `		pTos->x.iVal = rc;` |
|        - |  4880 | `		/* Invalidate any prior representation */` |
|   270538 |  4881 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   135270 |  4882 | `	}else{` |
|      ! 0 |  4883 | `		if( rc ){` |
|        - |  4884 | `			/* Jump to the desired location */` |
|      ! 0 |  4885 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4886 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4887 | `		}` |
|        - |  4888 | `	}` |
|   270538 |  4889 | `	break;` |
|        - |  4890 | `				 }` |
|        - |  4891 | `/* OP_TNE P1 P2 *` |
|        - |  4892 | ` *` |
|        - |  4893 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4894 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4895 | ` * instruction.` |
|        - |  4896 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4897 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4898 | ` *` |
|        - |  4899 | ` */` |
|   105474 |  4900 | `case PH7_OP_TNE: {` |
|   210950 |  4901 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4902 | `	/* Perform the comparison and act accordingly */` |
|        - |  4903 | `#ifdef UNTRUST` |
|        - |  4904 | `	if( pNos < pStack ){` |
|        - |  4905 | `		goto Abort;` |
|        - |  4906 | `	}` |
|        - |  4907 | `#endif` |
|   210950 |  4908 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   210950 |  4909 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4910 | `		rc = 1;` |
|        2 |  4911 | `	}else{` |
|   210948 |  4912 | `		rc = rc != 0;` |
|        - |  4913 | `	}` |
|   210950 |  4914 | `	VmPopOperand(&pTos,1);` |
|   210950 |  4915 | `	if( !pInstr->iP2 ){` |
|        - |  4916 | `		/* Push comparison result without taking the jump */` |
|   210950 |  4917 | `		PH7_MemObjRelease(pTos);` |
|   210950 |  4918 | `		pTos->x.iVal = rc;` |
|        - |  4919 | `		/* Invalidate any prior representation */` |
|   210950 |  4920 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   105476 |  4921 | `	}else{` |
|      ! 0 |  4922 | `		if( rc ){` |
|        - |  4923 | `			/* Jump to the desired location */` |
|      ! 0 |  4924 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4925 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4926 | `		}` |
|        - |  4927 | `	}` |
|   210950 |  4928 | `	break;` |
|        - |  4929 | `				 }` |
|        - |  4930 | `/* OP_LT P1 P2 P3` |
|        - |  4931 | ` *` |
|        - |  4932 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4933 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4934 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4935 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4936 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4937 | ` *` |
|        - |  4938 | ` */` |
|        - |  4939 | `/* OP_LE P1 P2 P3` |
|        - |  4940 | ` *` |
|        - |  4941 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4942 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4943 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4944 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4945 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4946 | ` *` |
|        - |  4947 | ` */` |
|   103260 |  4948 | `case PH7_OP_LT:` |
|        - |  4949 | `case PH7_OP_LE: {` |
|   206566 |  4950 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4951 | `	/* Perform the comparison and act accordingly */` |
|        - |  4952 | `#ifdef UNTRUST` |
|        - |  4953 | `	if( pNos < pStack ){` |
|        - |  4954 | `		goto Abort;` |
|        - |  4955 | `	}` |
|        - |  4956 | `#endif` |
|   206566 |  4957 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   206566 |  4958 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4959 | `		rc = 0;` |
|   206562 |  4960 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      432 |  4961 | `		rc = rc < 1;` |
|      217 |  4962 | `	}else{` |
|   206128 |  4963 | `		rc = rc < 0;` |
|        - |  4964 | `	}` |
|   206566 |  4965 | `	VmPopOperand(&pTos,1);` |
|   206566 |  4966 | `	if( !pInstr->iP2 ){` |
|        - |  4967 | `		/* Push comparison result without taking the jump */` |
|   206566 |  4968 | `		PH7_MemObjRelease(pTos);` |
|   206566 |  4969 | `		pTos->x.iVal = rc;` |
|        - |  4970 | `		/* Invalidate any prior representation */` |
|   206566 |  4971 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   103306 |  4972 | `	}else{` |
|      ! 0 |  4973 | `		if( rc ){` |
|        - |  4974 | `			/* Jump to the desired location */` |
|      ! 0 |  4975 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4976 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4977 | `		}` |
|        - |  4978 | `	}` |
|   206566 |  4979 | `	break;` |
|        - |  4980 | `				}` |
|        - |  4981 | `/* OP_GT P1 P2 P3` |
|        - |  4982 | ` *` |
|        - |  4983 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4984 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4985 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4986 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4987 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4988 | ` *` |
|        - |  4989 | ` */` |
|        - |  4990 | `/* OP_GE P1 P2 P3` |
|        - |  4991 | ` *` |
|        - |  4992 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4993 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4994 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4995 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4996 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4997 | ` *` |
|        - |  4998 | ` */` |
|    49192 |  4999 | `case PH7_OP_GT:` |
|        - |  5000 | `case PH7_OP_GE: {` |
|    98386 |  5001 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5002 | `	/* Perform the comparison and act accordingly */` |
|        - |  5003 | `#ifdef UNTRUST` |
|        - |  5004 | `	if( pNos < pStack ){` |
|        - |  5005 | `		goto Abort;` |
|        - |  5006 | `	}` |
|        - |  5007 | `#endif` |
|    98386 |  5008 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    98386 |  5009 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5010 | `		rc = 0;` |
|    98382 |  5011 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    98228 |  5012 | `		rc = rc >= 0;` |
|    49115 |  5013 | `	}else{` |
|      152 |  5014 | `		rc = rc > 0;` |
|        - |  5015 | `	}` |
|    98386 |  5016 | `	VmPopOperand(&pTos,1);` |
|    98386 |  5017 | `	if( !pInstr->iP2 ){` |
|        - |  5018 | `		/* Push comparison result without taking the jump */` |
|    98386 |  5019 | `		PH7_MemObjRelease(pTos);` |
|    98386 |  5020 | `		pTos->x.iVal = rc;` |
|        - |  5021 | `		/* Invalidate any prior representation */` |
|    98386 |  5022 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    49194 |  5023 | `	}else{` |
|      ! 0 |  5024 | `		if( rc ){` |
|        - |  5025 | `			/* Jump to the desired location */` |
|      ! 0 |  5026 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5027 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5028 | `		}` |
|        - |  5029 | `	}` |
|    98386 |  5030 | `	break;` |
|        - |  5031 | `				}` |
|        - |  5032 | `/* OP_SPACESHIP * * *` |
|        - |  5033 | ` *` |
|        - |  5034 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5035 | ` *   -1 if left < right` |
|        - |  5036 | ` *    0 if left == right` |
|        - |  5037 | ` *    1 if left > right` |
|        - |  5038 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5039 | ` */` |
|       25 |  5040 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5041 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5042 | `#ifdef UNTRUST` |
|        - |  5043 | `	if( pNos < pStack ){` |
|        - |  5044 | `		goto Abort;` |
|        - |  5045 | `	}` |
|        - |  5046 | `#endif` |
|       51 |  5047 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5048 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5049 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5050 | `		rc = 1;` |
|        4 |  5051 | `	}else{` |
|        - |  5052 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5053 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5054 | `	}` |
|       51 |  5055 | `	VmPopOperand(&pTos,1);` |
|       51 |  5056 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5057 | `	pTos->x.iVal = rc;` |
|       51 |  5058 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5059 | `	break;` |
|        - |  5060 | `				}` |
|        - |  5061 | `/* OP_SEQ P1 P2 *` |
|        - |  5062 | ` * Strict string comparison.` |
|        - |  5063 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5064 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5065 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5066 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5067 | ` * use PH7_OP_EQ.` |
|        - |  5068 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5069 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5070 | ` */` |
|        - |  5071 | `/* OP_SNE P1 P2 *` |
|        - |  5072 | ` * Strict string comparison.` |
|        - |  5073 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5074 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5075 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5076 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5077 | ` * use PH7_OP_EQ.` |
|        - |  5078 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5079 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5080 | ` */` |
|       18 |  5081 | `case PH7_OP_SEQ:` |
|        - |  5082 | `case PH7_OP_SNE: {` |
|       38 |  5083 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5084 | `	SyString s1,s2;` |
|        - |  5085 | `	/* Perform the comparison and act accordingly */` |
|        - |  5086 | `#ifdef UNTRUST` |
|        - |  5087 | `	if( pNos < pStack ){` |
|        - |  5088 | `		goto Abort;` |
|        - |  5089 | `	}` |
|        - |  5090 | `#endif` |
|        - |  5091 | `	/* Force a string cast */` |
|       38 |  5092 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5093 | `		PH7_MemObjToString(pTos);` |
|        2 |  5094 | `	}` |
|       38 |  5095 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5096 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5097 | `	}` |
|       38 |  5098 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5099 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5100 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5101 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5102 | `		rc = rc != 0;` |
|      ! 0 |  5103 | `	}else{` |
|       38 |  5104 | `		rc = rc == 0;` |
|        - |  5105 | `	}` |
|       38 |  5106 | `	VmPopOperand(&pTos,1);` |
|       38 |  5107 | `	if( !pInstr->iP2 ){` |
|        - |  5108 | `		/* Push comparison result without taking the jump */` |
|       38 |  5109 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5110 | `		pTos->x.iVal = rc;` |
|        - |  5111 | `		/* Invalidate any prior representation */` |
|       38 |  5112 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5113 | `	}else{` |
|      ! 0 |  5114 | `		if( rc ){` |
|        - |  5115 | `			/* Jump to the desired location */` |
|      ! 0 |  5116 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5117 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5118 | `		}` |
|        - |  5119 | `	}` |
|       38 |  5120 | `	break;` |
|        - |  5121 | `				 }` |
|        - |  5122 | `/*` |
|        - |  5123 | ` * OP_LOAD_REF * * *` |
|        - |  5124 | ` * Push the index of a referenced object on the stack.` |
|        - |  5125 | ` */` |
|       57 |  5126 | `case PH7_OP_LOAD_REF: {` |
|        - |  5127 | `	sxu32 nIdx;` |
|        - |  5128 | `#ifdef UNTRUST` |
|        - |  5129 | `	if( pTos < pStack ){` |
|        - |  5130 | `		goto Abort;` |
|        - |  5131 | `	}` |
|        - |  5132 | `#endif` |
|        - |  5133 | `	/* Extract memory object index */` |
|      115 |  5134 | `	nIdx = pTos->nIdx;` |
|      115 |  5135 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5136 | `		/* Nullify the object */` |
|       95 |  5137 | `		PH7_MemObjRelease(pTos);` |
|        - |  5138 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5139 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5140 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5141 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5142 | `	}` |
|      115 |  5143 | `	break;` |
|        - |  5144 | `					  }` |
|        - |  5145 | `/*` |
|        - |  5146 | ` * OP_STORE_REF * * P3` |
|        - |  5147 | ` * Perform an assignment operation by reference.` |
|        - |  5148 | ` */` |
|       15 |  5149 | ` case PH7_OP_STORE_REF: {` |
|       32 |  5150 | `	 SyString sName = { 0 , 0 };` |
|        - |  5151 | `	 VmFrame *pFrameLocal;` |
|        - |  5152 | `	SyHashEntry *pEntry;` |
|        - |  5153 | `	sxu32 nIdx;` |
|        - |  5154 | `#ifdef UNTRUST` |
|        - |  5155 | `	if( pTos < pStack ){` |
|        - |  5156 | `		goto Abort;` |
|        - |  5157 | `	}` |
|        - |  5158 | `#endif` |
|       32 |  5159 | `	if( pInstr->p3 == 0 ){` |
|        - |  5160 | `		char *zName;` |
|        - |  5161 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5162 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5163 | `			/* Force a string cast */` |
|      ! 0 |  5164 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5165 | `		}` |
|      ! 0 |  5166 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5167 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5168 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5169 | `			if( zName ){` |
|      ! 0 |  5170 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5171 | `			}` |
|      ! 0 |  5172 | `		}` |
|      ! 0 |  5173 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5174 | `		pTos--;` |
|      ! 0 |  5175 | `	}else{` |
|       32 |  5176 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5177 | `	}` |
|       32 |  5178 | `	nIdx = pTos->nIdx;` |
|       32 |  5179 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5180 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5181 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5182 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5183 | `		}else{` |
|        - |  5184 | `			ph7_value *pObj;` |
|        - |  5185 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5186 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5187 | `			if( pObj == 0 ){` |
|      ! 0 |  5188 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5189 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5190 | `				goto Abort;` |
|        - |  5191 | `			}` |
|        - |  5192 | `			/* Perform the store operation */` |
|      ! 0 |  5193 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5194 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5195 | `		}` |
|       32 |  5196 | `	}else if( sName.nByte > 0){` |
|       32 |  5197 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5198 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5199 | `		}else{` |
|       32 |  5200 | `			pFrameLocal = pVm->pFrame;` |
|       32 |  5201 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5202 | `			/* Query the local frame */` |
|       32 |  5203 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       32 |  5204 | `			if( pEntry ){` |
|      ! 0 |  5205 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5206 | `			}else{` |
|       32 |  5207 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       32 |  5208 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5209 | `					/* Insert in the $GLOBALS array */` |
|       28 |  5210 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       13 |  5211 | `				}` |
|       32 |  5212 | `				if( rc == SXRET_OK ){` |
|       32 |  5213 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       15 |  5214 | `				}` |
|        - |  5215 | `			}` |
|        - |  5216 | `		}` |
|       15 |  5217 | `	}` |
|       32 |  5218 | `	break;` |
|        - |  5219 | `				 }` |
|        - |  5220 | `/*` |
|        - |  5221 | ` * OP_UPLINK P1 * *` |
|        - |  5222 | ` * Link a variable to the top active VM frame.` |
|        - |  5223 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5224 | ` */` |
|       25 |  5225 | `case PH7_OP_UPLINK: {` |
|       52 |  5226 | `	if( pVm->pFrame->pParent ){` |
|       52 |  5227 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5228 | `		SyString sName;` |
|        - |  5229 | `		/* Perform the link */` |
|      104 |  5230 | `		while( pLink <= pTos ){` |
|       54 |  5231 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5232 | `				/* Force a string cast */` |
|      ! 0 |  5233 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  5234 | `			}` |
|       54 |  5235 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  5236 | `			if( sName.nByte > 0 ){` |
|       54 |  5237 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  5238 | `			}` |
|       54 |  5239 | `			pLink++;` |
|        2 |  5240 | `		}` |
|       25 |  5241 | `	}` |
|       52 |  5242 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  5243 | `	break;` |
|        - |  5244 | `					}` |
|        - |  5245 | `/*` |
|        - |  5246 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  5247 | ` * Push an exception in the corresponding container so that` |
|        - |  5248 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  5249 | ` */` |
|       32 |  5250 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       66 |  5251 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5252 | `	VmFrame *pFrameLocal;` |
|        - |  5253 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       66 |  5254 | `	pException->iFinallyDone = 0;` |
|       66 |  5255 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5256 | `	/* Create the exception frame */` |
|       66 |  5257 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       66 |  5258 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5259 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5260 | `		goto Abort;` |
|        - |  5261 | `	}` |
|        - |  5262 | `	/* Mark the special frame */` |
|       66 |  5263 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       66 |  5264 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5265 | `	/* Point to the frame that trigger the exception */` |
|       66 |  5266 | `	pFrameLocal = pFrameLocal->pParent;` |
|       66 |  5267 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       66 |  5268 | `	pException->pFrame = pFrameLocal;` |
|       66 |  5269 | `	break;` |
|        - |  5270 | `							}` |
|        - |  5271 | `/*` |
|        - |  5272 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5273 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5274 | ` */` |
|       31 |  5275 | `case PH7_OP_POP_EXCEPTION: {` |
|       64 |  5276 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       64 |  5277 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5278 | `		ph7_exception **apException;` |
|        - |  5279 | `		/* Pop the loaded exception */` |
|       28 |  5280 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5281 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5282 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5283 | `		}` |
|       13 |  5284 | `	}` |
|       64 |  5285 | `	pException->pFrame = 0;` |
|        - |  5286 | `	/* Leave the exception frame */` |
|       64 |  5287 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5288 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       64 |  5289 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5290 | `		sxi32 rcFinally;` |
|       20 |  5291 | `		pException->iFinallyDone = 1;` |
|       20 |  5292 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5293 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5294 | `			goto Abort;` |
|        - |  5295 | `		}` |
|        9 |  5296 | `	}` |
|       64 |  5297 | `	break;` |
|        - |  5298 | `							}` |
|        - |  5299 |  |
|        - |  5300 | `/*` |
|        - |  5301 | ` * OP_THROW * P2 *` |
|        - |  5302 | ` * Throw an user exception.` |
|        - |  5303 | ` */` |
|       18 |  5304 | `case PH7_OP_THROW: {` |
|       38 |  5305 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       38 |  5306 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5307 | `#ifdef UNTRUST` |
|        - |  5308 | `	if( pTos < pStack ){` |
|        - |  5309 | `		goto Abort;` |
|        - |  5310 | `	}` |
|        - |  5311 | `#endif` |
|       38 |  5312 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5313 | `	/* Tell the upper layer that an exception was thrown */` |
|       38 |  5314 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       38 |  5315 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       38 |  5316 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5317 | `		ph7_class *pException;` |
|        - |  5318 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5319 | `		 */` |
|       38 |  5320 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       38 |  5321 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5322 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5323 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5324 | `			if( rc == SXERR_ABORT ){` |
|        - |  5325 | `				/* Abort processing immediately */` |
|      ! 0 |  5326 | `				goto Abort;` |
|        - |  5327 | `			}` |
|      ! 0 |  5328 | `		}else{` |
|        - |  5329 | `			/* Throw the exception */` |
|       38 |  5330 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       38 |  5331 | `			if( rc == SXERR_ABORT ){` |
|        - |  5332 | `				/* Abort processing immediately */` |
|        9 |  5333 | `				goto Abort;` |
|        - |  5334 | `			}` |
|        - |  5335 | `		}` |
|       16 |  5336 | `	}else{` |
|        - |  5337 | `		/* Expecting a class instance */` |
|      ! 0 |  5338 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5339 | `		if( rc == SXERR_ABORT ){` |
|        - |  5340 | `			/* Abort processing immediately */` |
|      ! 0 |  5341 | `			goto Abort;` |
|        - |  5342 | `		}` |
|        - |  5343 | `	}` |
|        - |  5344 | `	/* Pop the top entry */` |
|       30 |  5345 | `	VmPopOperand(&pTos,1);` |
|        - |  5346 | `	/* Perform an unconditional jump */` |
|       30 |  5347 | `	pc = nJump - 1;` |
|       30 |  5348 | `	break;` |
|        - |  5349 | `				   }` |
|        - |  5350 | `/*` |
|        - |  5351 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5352 | ` * Prepare a foreach step.` |
|        - |  5353 | ` */` |
|     5138 |  5354 | `case PH7_OP_FOREACH_INIT: {` |
|    10278 |  5355 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5356 | `	void *pName;` |
|        - |  5357 | `#ifdef UNTRUST` |
|        - |  5358 | `	if( pTos < pStack ){` |
|        - |  5359 | `		goto Abort;` |
|        - |  5360 | `	}` |
|        - |  5361 | `#endif` |
|    10278 |  5362 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5363 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  5364 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5365 | `			/* Force a string cast */` |
|      ! 0 |  5366 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5367 | `		}` |
|        - |  5368 | `		/* Duplicate name */` |
|      ! 0 |  5369 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5370 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5371 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5372 | `		}` |
|      ! 0 |  5373 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5374 | `	}` |
|    10278 |  5375 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  5376 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5377 | `			/* Force a string cast */` |
|      ! 0 |  5378 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5379 | `		}` |
|        - |  5380 | `		/* Duplicate name */` |
|      ! 0 |  5381 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5382 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5383 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5384 | `		}` |
|      ! 0 |  5385 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  5386 | `	}` |
|        - |  5387 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    10278 |  5388 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5389 | `		/* Jump out of the loop */` |
|      ! 0 |  5390 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5391 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5392 | `		}` |
|      ! 0 |  5393 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5394 | `	}else{` |
|        - |  5395 | `		ph7_foreach_step *pStep;` |
|    10278 |  5396 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10278 |  5397 | `		if( pStep == 0 ){` |
|      ! 0 |  5398 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5399 | `			/* Jump out of the loop */` |
|      ! 0 |  5400 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5401 | `		}else{` |
|        - |  5402 | `			/* Zero the structure */` |
|    10278 |  5403 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5404 | `			/* Prepare the step */` |
|    10278 |  5405 | `			pStep->iFlags = pInfo->iFlags;` |
|    10278 |  5406 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5407 | `				ph7_hashmap *pMap;` |
|        - |  5408 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5409 | `				 * source array so mutations don't affect other sharers. */` |
|    10250 |  5410 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  5411 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  5412 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  5413 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5414 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  5415 | `						 * variable still points at the same hashmap as` |
|        - |  5416 | `						 * the stack value. */` |
|        9 |  5417 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  5418 | `							pCur->iRef--;` |
|        9 |  5419 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  5420 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  5421 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  5422 | `						}` |
|        4 |  5423 | `					}` |
|        4 |  5424 | `				}` |
|    10250 |  5425 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5426 | `				/* Reset the internal loop cursor */` |
|    10250 |  5427 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5428 | `				/* Mark the step */` |
|    10250 |  5429 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10250 |  5430 | `				pStep->xIter.pMap = pMap;` |
|    10250 |  5431 | `				pMap->iRef++;` |
|     5126 |  5432 | `			}else{` |
|       30 |  5433 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5434 | `				ph7_class *pIteratorClass;` |
|        - |  5435 | `				/* Check if the object implements Iterator */` |
|       30 |  5436 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       39 |  5437 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5438 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5439 | `					ph7_class_method *pRewind;` |
|       20 |  5440 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       20 |  5441 | `					pStep->xIter.pThis = pThis;` |
|       20 |  5442 | `					pThis->iRef++;` |
|       20 |  5443 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       20 |  5444 | `					if( pRewind ){` |
|       20 |  5445 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        9 |  5446 | `					}` |
|       11 |  5447 | `				}else{` |
|        - |  5448 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5449 | `					ph7_class *pIterAggClass;` |
|       12 |  5450 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5451 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5452 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5453 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5454 | `						ph7_class_method *pGetIter;` |
|        3 |  5455 | `						int iterAggOk = 0;` |
|        3 |  5456 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5457 | `						if( pGetIter ){` |
|        - |  5458 | `							ph7_value sResult;` |
|        3 |  5459 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5460 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5461 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5462 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5463 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5464 | `									ph7_class_method *pRewind;` |
|        3 |  5465 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5466 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5467 | `									pIterObj->iRef++;` |
|        - |  5468 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5469 | `									pStep->pOwner = pThis;` |
|        3 |  5470 | `									pThis->iRef++;` |
|        3 |  5471 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5472 | `									if( pRewind ){` |
|        3 |  5473 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5474 | `									}` |
|        3 |  5475 | `									iterAggOk = 1;` |
|        1 |  5476 | `								}` |
|        1 |  5477 | `							}` |
|        3 |  5478 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5479 | `						}` |
|        3 |  5480 | `						if( !iterAggOk ){` |
|        - |  5481 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5482 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5483 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5484 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5485 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5486 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5487 | `						}` |
|        2 |  5488 | `					}else{` |
|        - |  5489 | `						/* Plain object iteration via hAttr */` |
|        9 |  5490 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5491 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5492 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5493 | `						pThis->iRef++;` |
|        - |  5494 | `					}` |
|        - |  5495 | `				}` |
|        - |  5496 | `			}` |
|        - |  5497 | `		}` |
|    10278 |  5498 | `		if( pStep ){` |
|    10278 |  5499 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5500 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5501 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5502 | `				/* Jump out of the loop */` |
|      ! 0 |  5503 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5504 | `			}` |
|     5138 |  5505 | `		}` |
|        - |  5506 | `	}` |
|    10278 |  5507 | `	VmPopOperand(&pTos,1);` |
|    10278 |  5508 | `	break;` |
|        - |  5509 | `						  }` |
|        - |  5510 | `/*` |
|        - |  5511 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5512 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5513 | ` */` |
|    83132 |  5514 | `case PH7_OP_FOREACH_STEP: {` |
|   166266 |  5515 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5516 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5517 | `	ph7_value *pValue;` |
|        - |  5518 | `	VmFrame *pFrameLocal;` |
|        - |  5519 | `	/* Peek the last step */` |
|   166266 |  5520 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   166266 |  5521 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   166266 |  5522 | `	pFrameLocal = pVm->pFrame;` |
|   166266 |  5523 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   166266 |  5524 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   166154 |  5525 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5526 | `		ph7_hashmap_node *pNode;` |
|        - |  5527 | `		/* Extract the current node value */` |
|   166154 |  5528 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   166154 |  5529 | `		if( pNode == 0 ){` |
|        - |  5530 | `			/* No more entry to process */` |
|    10248 |  5531 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10248 |  5532 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5533 | `				/* Break the reference with the last element */` |
|        7 |  5534 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5535 | `			}` |
|        - |  5536 | `			/* Automatically reset the loop cursor */` |
|    10248 |  5537 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5538 | `			/* Cleanup the mess left behind */` |
|    10248 |  5539 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10248 |  5540 | `			SySetPop(&pInfo->aStep);` |
|    10248 |  5541 | `			PH7_HashmapUnref(pMap);` |
|     5125 |  5542 | `		}else{` |
|   155908 |  5543 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5544 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5545 | `				if( pKey ){` |
|      416 |  5546 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5547 | `				}` |
|      207 |  5548 | `			}` |
|   155908 |  5549 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5550 | `				SyHashEntry *pEntry;` |
|        - |  5551 | `				/* Pass by reference */` |
|       23 |  5552 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  5553 | `				if( pEntry ){` |
|       23 |  5554 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       12 |  5555 | `				}else{` |
|      ! 0 |  5556 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5557 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5558 | `				}` |
|       12 |  5559 | `			}else{` |
|        - |  5560 | `				/* Make a copy of the entry value */` |
|   155886 |  5561 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   155886 |  5562 | `				if( pValue ){` |
|   155886 |  5563 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    77942 |  5564 | `				}` |
|        - |  5565 | `			}` |
|        2 |  5566 | `		}` |
|    83190 |  5567 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5568 | `		/* Iterator-based iteration.` |
|        - |  5569 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5570 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5571 | `		 */` |
|       90 |  5572 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5573 | `		ph7_class_method *pMethod;` |
|        - |  5574 | `		ph7_value sResult;` |
|       90 |  5575 | `		int isValid = 0;` |
|        - |  5576 | `		/* Call next() to advance — but skip on the first iteration */` |
|       90 |  5577 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       22 |  5578 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       12 |  5579 | `		}else{` |
|       70 |  5580 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       70 |  5581 | `			if( pMethod ){` |
|       70 |  5582 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       34 |  5583 | `			}` |
|        - |  5584 | `		}` |
|        - |  5585 | `		/* Call valid() */` |
|       90 |  5586 | `		PH7_MemObjInit(pVm,&sResult);` |
|       90 |  5587 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       90 |  5588 | `		if( pMethod ){` |
|       90 |  5589 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       90 |  5590 | `			PH7_MemObjToBool(&sResult);` |
|       90 |  5591 | `			isValid = (sResult.x.iVal != 0);` |
|       44 |  5592 | `		}` |
|       90 |  5593 | `		PH7_MemObjRelease(&sResult);` |
|       90 |  5594 | `		if( !isValid ){` |
|        - |  5595 | `			/* Iterator exhausted */` |
|       20 |  5596 | `			pc = pInstr->iP2 - 1;` |
|        - |  5597 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       20 |  5598 | `			if( pStep->pOwner ){` |
|        3 |  5599 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5600 | `			}` |
|       20 |  5601 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       20 |  5602 | `			SySetPop(&pInfo->aStep);` |
|       20 |  5603 | `			PH7_ClassInstanceUnref(pThis);` |
|       11 |  5604 | `		}else{` |
|        - |  5605 | `			/* Call current() to get value */` |
|       72 |  5606 | `			PH7_MemObjInit(pVm,&sResult);` |
|       72 |  5607 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       72 |  5608 | `			if( pMethod ){` |
|       72 |  5609 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       35 |  5610 | `			}` |
|       72 |  5611 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       72 |  5612 | `			if( pValue ){` |
|       72 |  5613 | `				PH7_MemObjStore(&sResult,pValue);` |
|       35 |  5614 | `			}` |
|       72 |  5615 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5616 | `			/* Call key() if needed */` |
|       72 |  5617 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5618 | `				ph7_value sKey;` |
|       35 |  5619 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  5620 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  5621 | `				if( pMethod ){` |
|       35 |  5622 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  5623 | `				}` |
|       35 |  5624 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  5625 | `				if( pValue ){` |
|       35 |  5626 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  5627 | `				}` |
|       35 |  5628 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  5629 | `			}` |
|        - |  5630 | `		}` |
|       46 |  5631 | `	}else{` |
|       25 |  5632 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5633 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5634 | `		SyHashEntry *pEntry;` |
|        - |  5635 | `		/* Point to the next attribute */` |
|       29 |  5636 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5637 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5638 | `			/* Check access permission */` |
|       31 |  5639 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5640 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5641 | `					break; /* Access is granted */` |
|        - |  5642 | `			}` |
|        1 |  5643 | `		}` |
|       25 |  5644 | `		if( pEntry == 0 ){` |
|        - |  5645 | `			/* Clean up the mess left behind */` |
|        9 |  5646 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5647 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5648 | `				/* Break the reference with the last element */` |
|        3 |  5649 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5650 | `			}` |
|        9 |  5651 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5652 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5653 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5654 | `		}else{` |
|       17 |  5655 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5656 | `			ph7_value *pAttrValue;` |
|       17 |  5657 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5658 | `				/* Fill with the current attribute name */` |
|       17 |  5659 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5660 | `				if( pKey ){` |
|       17 |  5661 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5662 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5663 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5664 | `				}` |
|        8 |  5665 | `			}` |
|        - |  5666 | `			/* Extract attribute value */` |
|       17 |  5667 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5668 | `			if( pAttrValue ){` |
|       17 |  5669 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5670 | `					/* Pass by reference */` |
|        3 |  5671 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5672 | `					if( pEntry ){` |
|        3 |  5673 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5674 | `					}else{` |
|      ! 0 |  5675 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5676 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5677 | `					}` |
|        2 |  5678 | `				}else{` |
|        - |  5679 | `					/* Make a copy of the attribute value */` |
|       15 |  5680 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5681 | `					if( pValue ){` |
|       15 |  5682 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5683 | `					}` |
|        - |  5684 | `				}` |
|        8 |  5685 | `			}` |
|        - |  5686 | `		}` |
|        - |  5687 | `	}` |
|   166266 |  5688 | `	break;` |
|        - |  5689 | `						  }` |
|        - |  5690 | `/*` |
|        - |  5691 | ` * OP_MEMBER P1 P2` |
|        - |  5692 | ` * Load class attribute/method on the stack.` |
|        - |  5693 | ` */` |
|     2234 |  5694 | `case PH7_OP_MEMBER: {` |
|        - |  5695 | `	ph7_class_instance *pThis;` |
|        - |  5696 | `	ph7_value *pNos;` |
|        - |  5697 | `	SyString sName;` |
|     4470 |  5698 | `	if( !pInstr->iP1 ){` |
|     4328 |  5699 | `		pNos = &pTos[-1];` |
|        - |  5700 | `#ifdef UNTRUST` |
|        - |  5701 | `		if( pNos < pStack ){` |
|        - |  5702 | `			goto Abort;` |
|        - |  5703 | `		}` |
|        - |  5704 | `#endif` |
|     4328 |  5705 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5706 | `			ph7_class *pClass;` |
|        - |  5707 | `			/* Class already instantiated */` |
|     4328 |  5708 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5709 | `			/* Point to the instantiated class */` |
|     4328 |  5710 | `			pClass = pThis->pClass;` |
|        - |  5711 | `			/* Extract attribute name first */` |
|     4328 |  5712 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4328 |  5713 | `			if( pInstr->iP2 ){` |
|        - |  5714 | `				/* Method call */` |
|      436 |  5715 | `				ph7_class_method *pMeth = 0;` |
|      436 |  5716 | `				if( sName.nByte > 0 ){` |
|        - |  5717 | `					/* Extract the target method */` |
|      436 |  5718 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      217 |  5719 | `				}` |
|      436 |  5720 | `				if( pMeth == 0 ){` |
|      ! 0 |  5721 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5722 | `						&pClass->sName,&sName` |
|        - |  5723 | `						);` |
|        - |  5724 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5725 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5726 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5727 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5728 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5729 | `				}else{` |
|        - |  5730 | `					/* Push method name on the stack */` |
|      436 |  5731 | `					PH7_MemObjRelease(pTos);` |
|      436 |  5732 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      436 |  5733 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5734 | `				}` |
|      436 |  5735 | `				pTos->nIdx = SXU32_HIGH;` |
|      219 |  5736 | `			}else{` |
|        - |  5737 | `				/* Attribute access */` |
|     3894 |  5738 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5739 | `				SyHashEntry *pEntry;` |
|        - |  5740 | `				/* Extract the target attribute */` |
|     3894 |  5741 | `				if( sName.nByte > 0 ){` |
|     3894 |  5742 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3894 |  5743 | `					if( pEntry ){` |
|        - |  5744 | `						/* Point to the attribute value */` |
|     3892 |  5745 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1945 |  5746 | `					}` |
|     1946 |  5747 | `				}` |
|     3894 |  5748 | `				if( pObjAttr == 0 ){` |
|        - |  5749 | `					/* No such attribute,load null */` |
|        4 |  5750 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5751 | `						&pClass->sName,&sName);` |
|        - |  5752 | `					/* Call the __get magic method if available */` |
|        3 |  5753 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5754 | `				}` |
|     3894 |  5755 | `				VmPopOperand(&pTos,1);` |
|        - |  5756 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5757 | `				 * This is due to the following case:` |
|        - |  5758 | `				 *     (new TestClass())->foo;` |
|        - |  5759 | `				 */` |
|     3894 |  5760 | `				pThis->iRef++;` |
|     3894 |  5761 | `				PH7_MemObjRelease(pTos);` |
|     3894 |  5762 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3894 |  5763 | `				if( pObjAttr ){` |
|     3892 |  5764 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5765 | `					/* Check attribute access */` |
|     3892 |  5766 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5767 | `						/* Load attribute */` |
|     3892 |  5768 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3892 |  5769 | `						if( pValue ){` |
|     3892 |  5770 | `							if( pThis->iRef < 2 ){` |
|        - |  5771 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5772 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5773 | `								 */` |
|        3 |  5774 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5775 | `							}else{` |
|        - |  5776 | `								/* Simple load */` |
|     3890 |  5777 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5778 | `							}` |
|     3892 |  5779 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3890 |  5780 | `								if( pThis->iRef > 1 ){` |
|        - |  5781 | `									/* Load attribute index */` |
|     3888 |  5782 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1943 |  5783 | `								}` |
|     1944 |  5784 | `							}` |
|     1945 |  5785 | `						}` |
|     1945 |  5786 | `					}` |
|     1945 |  5787 | `				}` |
|        - |  5788 | `				/* Safely unreference the object */` |
|     3894 |  5789 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5790 | `			}` |
|     2165 |  5791 | `		}else{` |
|      ! 0 |  5792 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5793 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5794 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5795 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5796 | `		}` |
|     2165 |  5797 | `	}else{` |
|        - |  5798 | `		/* Static member access using class name */` |
|      144 |  5799 | `		pNos = pTos;` |
|      144 |  5800 | `		pThis = 0;` |
|      144 |  5801 | `		if( !pInstr->p3 ){` |
|      132 |  5802 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      132 |  5803 | `			pNos--;` |
|        - |  5804 | `#ifdef UNTRUST` |
|        - |  5805 | `			if( pNos < pStack ){` |
|        - |  5806 | `				goto Abort;` |
|        - |  5807 | `			}` |
|        - |  5808 | `#endif` |
|       67 |  5809 | `		}else{` |
|        - |  5810 | `			/* Attribute name already computed */` |
|       14 |  5811 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5812 | `		}` |
|      144 |  5813 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      144 |  5814 | `			ph7_class *pClass = 0;` |
|      144 |  5815 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5816 | `				/* Class already instantiated */` |
|        5 |  5817 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  5818 | `				pClass = pThis->pClass;` |
|        5 |  5819 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  5820 | `			}else{` |
|        - |  5821 | `				/* Try to extract the target class */` |
|      140 |  5822 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      140 |  5823 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      140 |  5824 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5825 | `					/* Handle self/static/parent keywords */` |
|      140 |  5826 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       36 |  5827 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       36 |  5828 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5829 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5830 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5831 | `						}` |
|      123 |  5832 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       22 |  5833 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      103 |  5834 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       16 |  5835 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       16 |  5836 | `						if( pSelf && pSelf->pBase ){` |
|       16 |  5837 | `							pClass = pSelf->pBase;` |
|        7 |  5838 | `						}` |
|        9 |  5839 | `					}else{` |
|       72 |  5840 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5841 | `					}` |
|       69 |  5842 | `				}` |
|        - |  5843 | `			}` |
|      144 |  5844 | `			if( pClass == 0 ){` |
|        - |  5845 | `				/* Undefined class */` |
|      ! 0 |  5846 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5847 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5848 | `					);` |
|      ! 0 |  5849 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5850 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5851 | `				}` |
|      ! 0 |  5852 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5853 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5854 | `			}else{` |
|      144 |  5855 | `				if( pInstr->iP2 ){` |
|        - |  5856 | `					/* Method call */` |
|       68 |  5857 | `					ph7_class_method *pMeth = 0;` |
|       68 |  5858 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5859 | `						/* Extract the target method */` |
|       68 |  5860 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       33 |  5861 | `					}` |
|       68 |  5862 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5863 | `						if( pMeth ){` |
|      ! 0 |  5864 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5865 | `								&pClass->sName,&sName` |
|        - |  5866 | `								);` |
|      ! 0 |  5867 | `						}else{` |
|      ! 0 |  5868 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5869 | `								&pClass->sName,&sName` |
|        - |  5870 | `								);` |
|        - |  5871 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5872 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5873 | `						}` |
|        - |  5874 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5875 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5876 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5877 | `						}` |
|      ! 0 |  5878 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5879 | `					}else{` |
|        - |  5880 | `						/* Push method name on the stack */` |
|       68 |  5881 | `						PH7_MemObjRelease(pTos);` |
|       68 |  5882 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       68 |  5883 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5884 | `					}` |
|       68 |  5885 | `					pTos->nIdx = SXU32_HIGH;` |
|       35 |  5886 | `				}else{` |
|        - |  5887 | `					/* Attribute access */` |
|       78 |  5888 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5889 | `					/* Check for special ::class pseudo-constant */` |
|      113 |  5890 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       70 |  5891 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5892 | `						/* ::class returns the fully qualified class name */` |
|        - |  5893 | `						/* Pop the attribute name from the stack */` |
|       60 |  5894 | `						if( !pInstr->p3 ){` |
|       60 |  5895 | `							VmPopOperand(&pTos,1);` |
|       29 |  5896 | `						}` |
|       60 |  5897 | `						PH7_MemObjRelease(pTos);` |
|        - |  5898 | `						/* Load the class name */` |
|       60 |  5899 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  5900 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  5901 | `					}else{` |
|        - |  5902 | `						/* Extract the target attribute */` |
|       20 |  5903 | `						if( sName.nByte > 0 ){` |
|       20 |  5904 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5905 | `						}` |
|       20 |  5906 | `						if( pAttr == 0 ){` |
|        - |  5907 | `							/* No such attribute,load null */` |
|      ! 0 |  5908 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5909 | `								&pClass->sName,&sName);` |
|        - |  5910 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5911 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5912 | `						}` |
|        - |  5913 | `						/* Pop the attribute name from the stack */` |
|       20 |  5914 | `						if( !pInstr->p3 ){` |
|        7 |  5915 | `							VmPopOperand(&pTos,1);` |
|        3 |  5916 | `						}` |
|       20 |  5917 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5918 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5919 | `						if( pAttr ){` |
|       20 |  5920 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5921 | `								/* Access to a non static attribute */` |
|      ! 0 |  5922 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5923 | `									&pClass->sName,&pAttr->sName` |
|        - |  5924 | `									);` |
|      ! 0 |  5925 | `							}else{` |
|        - |  5926 | `								ph7_value *pValue;` |
|        - |  5927 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5928 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5929 | `									/* Load the desired attribute */` |
|       20 |  5930 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5931 | `									if( pValue ){` |
|       20 |  5932 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5933 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5934 | `											/* Load index number */` |
|       14 |  5935 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5936 | `										}` |
|        9 |  5937 | `									}` |
|        9 |  5938 | `								}` |
|        - |  5939 | `							}` |
|        9 |  5940 | `						}` |
|        - |  5941 | `					}` |
|        - |  5942 | `				}` |
|      144 |  5943 | `				if( pThis ){` |
|        - |  5944 | `					/* Safely unreference the object */` |
|        5 |  5945 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  5946 | `				}` |
|        - |  5947 | `			}` |
|       73 |  5948 | `		}else{` |
|        - |  5949 | `			/* Pop operands */` |
|      ! 0 |  5950 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5951 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5952 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5953 | `			}` |
|      ! 0 |  5954 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5955 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5956 | `		}` |
|        - |  5957 | `	}` |
|     4470 |  5958 | `	break;` |
|        - |  5959 | `					}` |
|        - |  5960 | `/*` |
|        - |  5961 | ` * OP_NEW P1 * * *` |
|        - |  5962 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5963 | ` */` |
|      329 |  5964 | `case PH7_OP_NEW: {` |
|      660 |  5965 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      660 |  5966 | `	ph7_class *pClass = 0;` |
|        - |  5967 | `	ph7_class_instance *pNew;` |
|      660 |  5968 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5969 | `		/* Try to extract the desired class */` |
|      989 |  5970 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      658 |  5971 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      329 |  5972 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5973 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5974 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5975 | `	}` |
|      660 |  5976 | `	if( pClass == 0 ){` |
|        - |  5977 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5978 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5979 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5980 | `			);` |
|        - |  5981 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5982 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5983 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5984 | `			/* Pop given arguments */` |
|      ! 0 |  5985 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5986 | `		}` |
|      ! 0 |  5987 | `		goto Abort;` |
|      ! 0 |  5988 | `	}else{` |
|        - |  5989 | `		ph7_class_method *pCons;` |
|        - |  5990 | `		/* Create a new class instance */` |
|      660 |  5991 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      660 |  5992 | `		if( pNew == 0 ){` |
|      ! 0 |  5993 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5994 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5995 | `				&pClass->sName` |
|        - |  5996 | `			);` |
|      ! 0 |  5997 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5998 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5999 | `				/* Pop given arguments */` |
|      ! 0 |  6000 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6001 | `			}` |
|      ! 0 |  6002 | `			break;` |
|        - |  6003 | `		}` |
|        - |  6004 | `		/* Check if a constructor is available */` |
|      660 |  6005 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      660 |  6006 | `		if( pCons == 0 ){` |
|      546 |  6007 | `			SyString *pName = &pClass->sName;` |
|        - |  6008 | `			/* Check for a constructor with the same base class name */` |
|      546 |  6009 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      272 |  6010 | `		}` |
|      660 |  6011 | `		if( pCons ){` |
|        - |  6012 | `			/* Call the class constructor */` |
|      116 |  6013 | `			SySetReset(&aArg);` |
|      220 |  6014 | `			while( pArg < pTos ){` |
|      106 |  6015 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      106 |  6016 | `				pArg++;` |
|        2 |  6017 | `			}` |
|      116 |  6018 | `			if( pVm->bErrReport ){` |
|        - |  6019 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6020 | `				sxu32 n;` |
|       57 |  6021 | `				n = SySetUsed(&aArg);` |
|        - |  6022 | `				/* Emit a notice for missing arguments */` |
|      101 |  6023 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6024 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6025 | `					if( pFuncArg ){` |
|       45 |  6026 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6027 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6028 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6029 | `						}` |
|       22 |  6030 | `					}` |
|       45 |  6031 | `					n++;` |
|        1 |  6032 | `				}` |
|       28 |  6033 | `			}` |
|      116 |  6034 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6035 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      116 |  6036 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6037 | `				pNew->iRef = 1;` |
|      ! 0 |  6038 | `			}` |
|       57 |  6039 | `		}` |
|      660 |  6040 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6041 | `			/* Pop given arguments */` |
|       98 |  6042 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       48 |  6043 | `		}` |
|      660 |  6044 | `		PH7_MemObjRelease(pTos);` |
|      660 |  6045 | `		pTos->x.pOther = pNew;` |
|      660 |  6046 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6047 | `	}` |
|      660 |  6048 | `	break;` |
|        - |  6049 | `				 }` |
|        - |  6050 | `/*` |
|        - |  6051 | ` * OP_CLONE * * *` |
|        - |  6052 | ` * Perfome a clone operation.` |
|        - |  6053 | ` */` |
|       23 |  6054 | `case PH7_OP_CLONE: {` |
|        - |  6055 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6056 | `#ifdef UNTRUST` |
|        - |  6057 | `	if( pTos < pStack ){` |
|        - |  6058 | `		goto Abort;` |
|        - |  6059 | `	}` |
|        - |  6060 | `#endif` |
|        - |  6061 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6062 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6063 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6064 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6065 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6066 | `		break;` |
|        - |  6067 | `	}` |
|        - |  6068 | `	/* Point to the source */` |
|       44 |  6069 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6070 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6071 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6072 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6073 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6074 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6075 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6076 | `		break;` |
|        - |  6077 | `	}` |
|        - |  6078 | `	/* Perform the clone operation */` |
|       44 |  6079 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6080 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6081 | `	if( pClone == 0 ){` |
|      ! 0 |  6082 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6083 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6084 | `	}else{` |
|        - |  6085 | `		/* Load the cloned object */` |
|       44 |  6086 | `		pTos->x.pOther = pClone;` |
|       44 |  6087 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6088 | `	}` |
|       44 |  6089 | `	break;` |
|        - |  6090 | `				   }` |
|        - |  6091 | `/*` |
|        - |  6092 | ` * OP_SWITCH * * P3` |
|        - |  6093 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6094 | ` */` |
|       21 |  6095 | `case PH7_OP_SWITCH: {` |
|       44 |  6096 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6097 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6098 | `	ph7_value sValue,sCaseValue;` |
|        - |  6099 | `	sxu32 n,nEntry;` |
|        - |  6100 | `#ifdef UNTRUST` |
|        - |  6101 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6102 | `		goto Abort;` |
|        - |  6103 | `	}` |
|        - |  6104 | `#endif` |
|        - |  6105 | `	/* Point to the case table  */` |
|       44 |  6106 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       44 |  6107 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6108 | `	/* Select the appropriate case block to execute */` |
|       44 |  6109 | `	PH7_MemObjInit(pVm,&sValue);` |
|       44 |  6110 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      102 |  6111 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      102 |  6112 | `		pCase = &aCase[n];` |
|      102 |  6113 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6114 | `		/* Execute the case expression first */` |
|      102 |  6115 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6116 | `		/* Compare the two expression */` |
|      102 |  6117 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      102 |  6118 | `		PH7_MemObjRelease(&sValue);` |
|      102 |  6119 | `		PH7_MemObjRelease(&sCaseValue);` |
|      102 |  6120 | `		if( rc == 0 ){` |
|        - |  6121 | `			/* Value match,jump to this block */` |
|       44 |  6122 | `			pc = pCase->nStart - 1;` |
|       44 |  6123 | `			break;` |
|        - |  6124 | `		}` |
|       31 |  6125 | `	}` |
|       44 |  6126 | `	VmPopOperand(&pTos,1);` |
|       44 |  6127 | `	if( n >= nEntry ){` |
|        - |  6128 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  6129 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  6130 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  6131 | `		}else{` |
|        - |  6132 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6133 | `			pc = pSwitch->nOut - 1;` |
|        - |  6134 | `		}` |
|      ! 0 |  6135 | `	}` |
|       44 |  6136 | `	break;` |
|        - |  6137 | `					}` |
|        - |  6138 | `/*` |
|        - |  6139 | ` * OP_YIELD P1 P2 *` |
|        - |  6140 | ` *  Yield a value from a generator function.` |
|        - |  6141 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6142 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6143 | ` */` |
|       28 |  6144 | `case PH7_OP_YIELD: {` |
|        - |  6145 | `	ph7_generator *pGen;` |
|       58 |  6146 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6147 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6148 | `		goto Abort;` |
|        - |  6149 | `	}` |
|       58 |  6150 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6151 | `	if( pInstr->iP2 ){` |
|        - |  6152 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6153 | `#ifdef UNTRUST` |
|        - |  6154 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6155 | `#endif` |
|        7 |  6156 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6157 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6158 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6159 | `		VmPopOperand(&pTos, 1);` |
|        - |  6160 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6161 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6162 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6163 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6164 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6165 | `			}` |
|        1 |  6166 | `		}` |
|       55 |  6167 | `	}else if( pInstr->iP1 ){` |
|        - |  6168 | `		/* yield $value */` |
|        - |  6169 | `#ifdef UNTRUST` |
|        - |  6170 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6171 | `#endif` |
|       52 |  6172 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6173 | `		VmPopOperand(&pTos, 1);` |
|        - |  6174 | `		/* Auto-increment key */` |
|       52 |  6175 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6176 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6177 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6178 | `	}else{` |
|        - |  6179 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6180 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6181 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6182 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6183 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6184 | `	}` |
|        - |  6185 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6186 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6187 | `	goto Suspend;` |
|        - |  6188 |  |
|        - |  6189 | `/*` |
|        - |  6190 | ` * OP_CALL P1 * *` |
|        - |  6191 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6192 | ` *  function on the stack.` |
|        - |  6193 | ` */` |
|   299572 |  6194 | `case PH7_OP_CALL: {` |
|   599190 |  6195 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6196 | `	ph7_value *pArg;` |
|   599190 |  6197 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   599190 |  6198 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6199 | `	SyHashEntry *pEntry;` |
|        - |  6200 | `	SyString sName;` |
|        - |  6201 | `	/* Extract function name */` |
|   599190 |  6202 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6203 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6204 | `			ph7_value sResult;` |
|      ! 0 |  6205 | `			SySetReset(&aArg);` |
|      ! 0 |  6206 | `			while( pArg < pTos ){` |
|      ! 0 |  6207 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6208 | `				pArg++;` |
|      ! 0 |  6209 | `			}` |
|      ! 0 |  6210 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6211 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6212 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6213 | `			SySetReset(&aArg);` |
|        - |  6214 | `			/* Pop given arguments */` |
|      ! 0 |  6215 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6216 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6217 | `			}` |
|        - |  6218 | `			/* Copy result */` |
|      ! 0 |  6219 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6220 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6221 | `		}else{` |
|        3 |  6222 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6223 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6224 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6225 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6226 | `			}else{` |
|        - |  6227 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6228 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6229 | `			}` |
|        - |  6230 | `			/* Pop given arguments */` |
|        3 |  6231 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6232 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6233 | `			}` |
|        - |  6234 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6235 | `			PH7_MemObjRelease(pTos);` |
|        - |  6236 | `		}` |
|   299295 |  6237 | `		break;` |
|        - |  6238 | `	}` |
|   599188 |  6239 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6240 | `	/* Check for a compiled function first.` |
|        - |  6241 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6242 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   599188 |  6243 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6244 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6245 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6246 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6247 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6248 | `	 * function calls inside namespaces. */` |
|   599188 |  6249 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6250 | `		const char *zFunc;` |
|        - |  6251 | `		const char *zEnd;` |
|        - |  6252 | `		const char *z;` |
|        - |  6253 | `		SyString sGlobal;` |
|       18 |  6254 | `		zFunc = sName.zString;` |
|       18 |  6255 | `		zEnd  = zFunc + sName.nByte;` |
|       18 |  6256 | `		z = zEnd;` |
|        - |  6257 | `		/* Find last namespace separator */` |
|      154 |  6258 | `		while( z > zFunc ){` |
|      154 |  6259 | `			if( z[-1] == '\\' ){` |
|       18 |  6260 | `				break;` |
|        - |  6261 | `			}` |
|      138 |  6262 | `			z--;` |
|        2 |  6263 | `		}` |
|       18 |  6264 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6265 | `			/* Retry lookup using the unqualified/global function name */` |
|       18 |  6266 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       18 |  6267 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        8 |  6268 | `		}` |
|        8 |  6269 | `	}` |
|   599188 |  6270 | `	if( pEntry ){` |
|        - |  6271 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6272 | `		ph7_class_instance *pThis;` |
|        - |  6273 | `		ph7_value *pFrameStack;` |
|        - |  6274 | `		ph7_vm_func *pVmFunc;` |
|        - |  6275 | `		ph7_class *pSelf;` |
|        - |  6276 | `		VmFrame *pFrame;` |
|        - |  6277 | `		ph7_value *pObj;` |
|        - |  6278 | `		VmSlot sArg;` |
|        - |  6279 | `		sxu32 n;` |
|        - |  6280 | `		/* initialize fields */` |
|    13448 |  6281 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13448 |  6282 | `		pThis = 0;` |
|    13448 |  6283 | `		pSelf = 0;` |
|    13448 |  6284 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6285 | `			ph7_class_method *pMeth;` |
|        - |  6286 | `			/* Class method call */` |
|     2010 |  6287 | `			ph7_value *pTarget = &pTos[-1];` |
|     2010 |  6288 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6289 | `				/* Extract the 'this' pointer */` |
|     2010 |  6290 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6291 | `					/* Instance already loaded */` |
|     1938 |  6292 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1938 |  6293 | `					pThis->iRef++;` |
|     1938 |  6294 | `					pSelf = pThis->pClass;` |
|      968 |  6295 | `				}` |
|     2010 |  6296 | `				if( pSelf == 0 ){` |
|       74 |  6297 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6298 | `						/* "Late Static Binding" class name */` |
|      101 |  6299 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       33 |  6300 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       33 |  6301 | `					}` |
|       74 |  6302 | `					if( pSelf == 0 ){` |
|       13 |  6303 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  6304 | `					}` |
|       36 |  6305 | `				}` |
|     2010 |  6306 | `				if( pThis == 0  ){` |
|       74 |  6307 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       74 |  6308 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       74 |  6309 | `					if( pFrameLocal->pParent ){` |
|        - |  6310 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       58 |  6311 | `						pThis = pFrameLocal->pThis;` |
|       58 |  6312 | `						if( pThis ){` |
|       13 |  6313 | `							pThis->iRef++;` |
|        6 |  6314 | `						}` |
|       28 |  6315 | `					}` |
|       36 |  6316 | `				}` |
|     2010 |  6317 | `				VmPopOperand(&pTos,1);` |
|     2010 |  6318 | `				PH7_MemObjRelease(pTos);` |
|        - |  6319 | `				/* Synchronize pointers */` |
|     2010 |  6320 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6321 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6322 | `				 * user have already computed the random generated unique class method name` |
|        - |  6323 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6324 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6325 | `				 */` |
|     2010 |  6326 | `				while( pArg < pStack ){` |
|      ! 0 |  6327 | `					pArg++;` |
|      ! 0 |  6328 | `				}` |
|     2010 |  6329 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6330 | `					/* Check if the call is allowed */` |
|     2010 |  6331 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2010 |  6332 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  6333 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  6334 | `							/* Pop given arguments */` |
|      ! 0 |  6335 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6336 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6337 | `							}` |
|        - |  6338 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6339 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  6340 | `							break;` |
|        - |  6341 | `						}` |
|        3 |  6342 | `					}` |
|     1004 |  6343 | `				}` |
|     1004 |  6344 | `			}` |
|     1004 |  6345 | `		}` |
|        - |  6346 | `		/* Check The recursion limit */` |
|    13448 |  6347 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6348 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6349 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6350 | `				&pVmFunc->sName);` |
|        - |  6351 | `			/* Pop given arguments */` |
|        3 |  6352 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6353 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6354 | `			}` |
|        - |  6355 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6356 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6357 | `			break;` |
|        - |  6358 | `		}` |
|    13446 |  6359 | `		if( pVmFunc->pNextName ){` |
|        - |  6360 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6361 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6362 | `		}` |
|    13446 |  6363 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6364 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6365 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6366 | `			ph7_generator *pGenerator;` |
|        - |  6367 | `			ph7_class_instance *pGenObj;` |
|        - |  6368 | `			ph7_value *pCtxAttr;` |
|        - |  6369 | `			SyString sAttrName;` |
|        - |  6370 | `			ph7_value **apCallArgs;` |
|        - |  6371 | `			int nGenArgs, iArg;` |
|        - |  6372 | `			/* Collect arguments from the operand stack */` |
|       20 |  6373 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6374 | `			apCallArgs = 0;` |
|       20 |  6375 | `			if( nGenArgs > 0 ){` |
|        8 |  6376 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6377 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6378 | `				if( apCallArgs == 0 ){` |
|        - |  6379 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6380 | `					nGenArgs = 0;` |
|      ! 0 |  6381 | `				}else{` |
|       12 |  6382 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6383 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6384 | `					}` |
|        - |  6385 | `				}` |
|        2 |  6386 | `			}` |
|        - |  6387 | `			/* Create execution context and generator wrapper */` |
|       20 |  6388 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6389 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6390 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6391 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6392 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6393 | `				break;` |
|        - |  6394 | `			}` |
|       20 |  6395 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6396 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6397 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6398 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6399 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6400 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6401 | `				break;` |
|        - |  6402 | `			}` |
|        - |  6403 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6404 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6405 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6406 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6407 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6408 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6409 | `			if( apCallArgs ){` |
|        6 |  6410 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6411 | `			}` |
|       20 |  6412 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6413 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6414 | `				if( pThis ){` |
|      ! 0 |  6415 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6416 | `				}` |
|      ! 0 |  6417 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6418 | `					goto Abort;` |
|        - |  6419 | `				}` |
|      ! 0 |  6420 | `				break;` |
|        - |  6421 | `			}` |
|        - |  6422 | `			/* Create Generator class instance */` |
|       20 |  6423 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6424 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6425 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6426 | `				break;` |
|        - |  6427 | `			}` |
|        - |  6428 | `			/* Store generator in __ctx attribute */` |
|       20 |  6429 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  6430 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  6431 | `			if( pCtxAttr ){` |
|       20 |  6432 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  6433 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6434 | `			}` |
|        - |  6435 | `			/* Pop args and function name, push Generator object */` |
|       20 |  6436 | `			PH7_MemObjRelease(pTos);` |
|       20 |  6437 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  6438 | `			pTos->x.pOther = pGenObj;` |
|       20 |  6439 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  6440 | `			pGenObj->iRef++;` |
|       20 |  6441 | `			if( pThis ){` |
|      ! 0 |  6442 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6443 | `			}` |
|       20 |  6444 | `			break;` |
|        - |  6445 | `		}` |
|        - |  6446 | `		/* Extract the formal argument set */` |
|    13428 |  6447 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6448 | `		/* Create a new VM frame  */` |
|    13428 |  6449 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13428 |  6450 | `		if( rc != SXRET_OK ){` |
|        - |  6451 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6452 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6453 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6454 | `				&pVmFunc->sName);` |
|        - |  6455 | `			/* Pop given arguments */` |
|      ! 0 |  6456 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6457 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6458 | `			}` |
|        - |  6459 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6460 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6461 | `			break;` |
|        - |  6462 | `		}` |
|    13428 |  6463 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6464 | `			/* Install the '$this' variable */` |
|        - |  6465 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1948 |  6466 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1948 |  6467 | `			if( pObj ){` |
|        - |  6468 | `				/* Reflect the change */` |
|     1948 |  6469 | `				pObj->x.pOther = pThis;` |
|     1948 |  6470 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      973 |  6471 | `			}` |
|      973 |  6472 | `		}` |
|    13428 |  6473 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6474 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6475 | `			/* Install static variables */` |
|      ! 0 |  6476 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6477 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6478 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6479 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6480 | `					/* Initialize the static variables */` |
|      ! 0 |  6481 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6482 | `					if( pObj ){` |
|        - |  6483 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6484 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6485 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6486 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6487 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6488 | `						}` |
|      ! 0 |  6489 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6490 | `					}else{` |
|      ! 0 |  6491 | `						continue;` |
|        - |  6492 | `					}` |
|      ! 0 |  6493 | `				}` |
|        - |  6494 | `				/* Install in the current frame */` |
|      ! 0 |  6495 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6496 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6497 | `			}` |
|      ! 0 |  6498 | `		}` |
|        - |  6499 | `		/* Push arguments in the local frame */` |
|    13428 |  6500 | `		n = 0;` |
|    36390 |  6501 | `		while( pArg < pTos ){` |
|    22984 |  6502 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6503 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       21 |  6504 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       21 |  6505 | `				if( pObj ){` |
|        - |  6506 | `					/* Initialize as empty array */` |
|       21 |  6507 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6508 | `					{` |
|       21 |  6509 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       83 |  6510 | `						while( pArg < pTos ){` |
|        - |  6511 | `							/* Apply type coercion to each element if the variadic has a type hint */` |
|       62 |  6512 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       29 |  6513 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6514 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6515 | `								if( xCast ){` |
|       13 |  6516 | `									xCast(pArg);` |
|        6 |  6517 | `								}` |
|        6 |  6518 | `							}` |
|       63 |  6519 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       63 |  6520 | `							pArg++;` |
|        1 |  6521 | `						}` |
|        - |  6522 | `					}` |
|       21 |  6523 | `					sArg.nIdx = pObj->nIdx;` |
|       21 |  6524 | `					sArg.pUserData = 0;` |
|       21 |  6525 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       10 |  6526 | `				}` |
|       21 |  6527 | `				break; /* All remaining args consumed */` |
|        - |  6528 | `			}` |
|    22964 |  6529 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22808 |  6530 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        9 |  6531 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6532 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6533 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6534 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6535 | `						goto Abort;` |
|        - |  6536 | `					}` |
|      ! 0 |  6537 | `				}` |
|        - |  6538 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6539 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    22820 |  6540 | `				if( aFormalArg[n].nType > 0` |
|    11985 |  6541 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1148 |  6542 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6543 | `						/* Argument must be a class instance [i.e: object] */` |
|        5 |  6544 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6545 | `						ph7_class *pClass;` |
|        - |  6546 | `						/* Try to extract the desired class */` |
|        5 |  6547 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|        5 |  6548 | `						if( pClass ){` |
|        5 |  6549 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6550 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6551 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6552 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6553 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6554 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6555 | `								}` |
|      ! 0 |  6556 | `							}else{` |
|        - |  6557 | `								/* reuse pThis declared in outer scope */` |
|        5 |  6558 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6559 | `								/* Make sure the object is an instance of the given class */` |
|        5 |  6560 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6561 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6562 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6563 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6564 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6565 | `								}` |
|        - |  6566 | `							}` |
|        3 |  6567 | `						}` |
|     1146 |  6568 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6569 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6570 | `						/* Cast to the desired type */` |
|      ! 0 |  6571 | `						xCast(pArg);` |
|      ! 0 |  6572 | `					}` |
|      573 |  6573 | `				}` |
|    22810 |  6574 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6575 | `					/* Pass by reference */` |
|       54 |  6576 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6577 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6578 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6579 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6580 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6581 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6582 | `						}` |
|        - |  6583 | `						/* Switch to pass by value */` |
|      ! 0 |  6584 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6585 | `					}else{` |
|        - |  6586 | `						SyHashEntry *pRefEntry;` |
|        - |  6587 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6588 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6589 | `						if( pRefEntry == 0 ){` |
|       80 |  6590 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6591 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6592 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6593 | `							sArg.pUserData = 0;` |
|       54 |  6594 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6595 | `						}` |
|       54 |  6596 | `						pObj = 0;` |
|        - |  6597 | `					}` |
|       28 |  6598 | `				}else{` |
|        - |  6599 | `					/* Pass by value,make a copy of the given argument */` |
|    22758 |  6600 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6601 | `				}` |
|    11406 |  6602 | `			}else{` |
|        - |  6603 | `				char zName[32];` |
|        - |  6604 | `				SyString sArgName;` |
|        - |  6605 | `				/* Set a dummy name */` |
|      156 |  6606 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6607 | `				sArgName.zString = zName;` |
|        - |  6608 | `				/* Annonymous argument */` |
|      156 |  6609 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6610 | `			}` |
|    22964 |  6611 | `			if( pObj ){` |
|    22912 |  6612 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6613 | `				/* Insert argument index  */` |
|    22912 |  6614 | `				sArg.nIdx = pObj->nIdx;` |
|    22912 |  6615 | `				sArg.pUserData = 0;` |
|    22912 |  6616 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11455 |  6617 | `			}` |
|    22964 |  6618 | `			PH7_MemObjRelease(pArg);` |
|    22964 |  6619 | `			pArg++;` |
|    22964 |  6620 | `			++n;` |
|        2 |  6621 | `		}` |
|        - |  6622 | `		/* Set up closure environment */` |
|    13428 |  6623 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6624 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6625 | `			ph7_value *pValue;` |
|        - |  6626 | `			sxu32 iEnv;` |
|       11 |  6627 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6628 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6629 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6630 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6631 | `					/* Do not install null value */` |
|       11 |  6632 | `					continue;` |
|        - |  6633 | `				}` |
|       11 |  6634 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6635 | `				if( pValue == 0 ){` |
|      ! 0 |  6636 | `					continue;` |
|        - |  6637 | `				}` |
|        - |  6638 | `				/* Invalidate any prior representation */` |
|       11 |  6639 | `				PH7_MemObjRelease(pValue);` |
|        - |  6640 | `				/* Duplicate bound variable value */` |
|       11 |  6641 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6642 | `			}` |
|        5 |  6643 | `		}` |
|        - |  6644 | `		/* Process default values for remaining formal parameters */` |
|    15402 |  6645 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2002 |  6646 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6647 | `				/* Variadic parameter with no extra args — create empty array */` |
|       27 |  6648 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       27 |  6649 | `				if( pObj ){` |
|       27 |  6650 | `					PH7_MemObjToHashmap(pObj);` |
|       27 |  6651 | `					sArg.nIdx = pObj->nIdx;` |
|       27 |  6652 | `					sArg.pUserData = 0;` |
|       27 |  6653 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6654 | `				}` |
|       27 |  6655 | `				n++;` |
|       27 |  6656 | `				break; /* Variadic is always last */` |
|        - |  6657 | `			}` |
|     1976 |  6658 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1970 |  6659 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1970 |  6660 | `				if( pObj ){` |
|        - |  6661 | `					/* Evaluate the default value and extract it's result */` |
|     1970 |  6662 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1970 |  6663 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6664 | `						goto Abort;` |
|        - |  6665 | `					}` |
|        - |  6666 | `					/* Insert argument index */` |
|     1970 |  6667 | `					sArg.nIdx = pObj->nIdx;` |
|     1970 |  6668 | `					sArg.pUserData = 0;` |
|     1970 |  6669 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6670 | `					/* Make sure the default argument is of the correct type */` |
|     1970 |  6671 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6672 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6673 | `						/* Cast to the desired type */` |
|      ! 0 |  6674 | `						xCast(pObj);` |
|      ! 0 |  6675 | `					}` |
|      984 |  6676 | `				}` |
|      984 |  6677 | `			}` |
|     1976 |  6678 | `			++n;` |
|        2 |  6679 | `		}` |
|        - |  6680 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6681 | `		 * does not return anything.` |
|        - |  6682 | `		 */` |
|    13428 |  6683 | `		PH7_MemObjRelease(pTos);` |
|    13428 |  6684 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6685 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13428 |  6686 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13428 |  6687 | `		if( pFrameStack == 0 ){` |
|        - |  6688 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6689 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6690 | `				&pVmFunc->sName);` |
|      ! 0 |  6691 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6692 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6693 | `			}` |
|      ! 0 |  6694 | `			break;` |
|        - |  6695 | `		}` |
|    13428 |  6696 | `		if( pSelf ){` |
|        - |  6697 | `			/* Push class name */` |
|     2008 |  6698 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1003 |  6699 | `		}` |
|        - |  6700 | `		/* Increment nesting level */` |
|    13428 |  6701 | `		pVm->nRecursionDepth++;` |
|        - |  6702 | `		/* Execute function body */` |
|    13428 |  6703 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6704 | `		/* Decrement nesting level */` |
|    13428 |  6705 | `		pVm->nRecursionDepth--;` |
|    13428 |  6706 | `		if( pSelf ){` |
|        - |  6707 | `			/* Pop class name */` |
|     2008 |  6708 | `			(void)SySetPop(&pVm->aSelf);` |
|     1003 |  6709 | `		}` |
|        - |  6710 | `		/* Cleanup the mess left behind */` |
|    13428 |  6711 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6712 | `			/* Return by reference,reflect that */` |
|        9 |  6713 | `			if( n != SXU32_HIGH ){` |
|        9 |  6714 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6715 | `				sxu32 i;` |
|        - |  6716 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6717 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6718 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6719 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6720 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6721 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6722 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6723 | `								&pVmFunc->sName);` |
|      ! 0 |  6724 | `						}` |
|      ! 0 |  6725 | `						n = SXU32_HIGH;` |
|      ! 0 |  6726 | `						break;` |
|        - |  6727 | `					}` |
|        3 |  6728 | `				}` |
|        5 |  6729 | `			}else{` |
|      ! 0 |  6730 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6731 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6732 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6733 | `						&pVmFunc->sName);` |
|      ! 0 |  6734 | `				}` |
|        - |  6735 | `			}` |
|        9 |  6736 | `			pTos->nIdx = n;` |
|        4 |  6737 | `		}` |
|        - |  6738 | `		/* Cleanup the mess left behind */` |
|    13428 |  6739 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6740 | `			/* An exception was throw in this frame */` |
|       12 |  6741 | `			pFrame = pFrame->pParent;` |
|       12 |  6742 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6743 | `				/* Pop the resutlt */` |
|       10 |  6744 | `				VmPopOperand(&pTos,1);` |
|        - |  6745 | `				/* Jump to this destination */` |
|       10 |  6746 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6747 | `				rc = PH7_OK;` |
|        6 |  6748 | `			}else{` |
|        3 |  6749 | `				if( pFrame->pParent ){` |
|        3 |  6750 | `					rc = PH7_EXCEPTION;` |
|        2 |  6751 | `				}else{` |
|        - |  6752 | `					/* Continue normal execution */` |
|      ! 0 |  6753 | `					rc = PH7_OK;` |
|        - |  6754 | `				}` |
|        - |  6755 | `			}` |
|        5 |  6756 | `		}` |
|        - |  6757 | `		/* Free the operand stack */` |
|    13428 |  6758 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6759 | `		/* Leave the frame */` |
|    13428 |  6760 | `		VmLeaveFrame(&(*pVm));` |
|    13428 |  6761 | `		if( rc == PH7_ABORT ){` |
|        - |  6762 | `			/* Abort processing immeditaley */` |
|        7 |  6763 | `			goto Abort;` |
|    13422 |  6764 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6765 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6766 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6767 | `			 * overwriting the state saved by the inner level.` |
|        - |  6768 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6769 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  6770 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  6771 | `			goto Suspend;` |
|    13384 |  6772 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6773 | `			goto Exception;` |
|        - |  6774 | `		}` |
|     6692 |  6775 | `	}else{` |
|        - |  6776 | `		ph7_user_func *pFunc;` |
|        - |  6777 | `		ph7_context sCtx;` |
|        - |  6778 | `		ph7_value sRet;` |
|        - |  6779 | `		/* Look for an installed foreign function.` |
|        - |  6780 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6781 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6782 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6783 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6784 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6785 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   585742 |  6786 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   585742 |  6787 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6788 | `			/* Compiler-qualified: try short name as global fallback */` |
|       18 |  6789 | `			const char *zShort = sName.zString;` |
|        - |  6790 | `			sxu32 i;` |
|      262 |  6791 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      246 |  6792 | `				if( sName.zString[i] == '\\' ){` |
|       22 |  6793 | `					zShort = &sName.zString[i + 1];` |
|       10 |  6794 | `				}` |
|      124 |  6795 | `			}` |
|       18 |  6796 | `			if( zShort != sName.zString ){` |
|       18 |  6797 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       18 |  6798 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        8 |  6799 | `			}` |
|        8 |  6800 | `		}` |
|   585742 |  6801 | `		if( pEntry == 0 ){` |
|        - |  6802 | `			/* Call to undefined function */` |
|        5 |  6803 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6804 | `			/* Pop given arguments */` |
|        5 |  6805 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6806 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6807 | `			}` |
|        - |  6808 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6809 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6810 | `			break;` |
|        - |  6811 | `		}` |
|   585738 |  6812 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6813 | `		/* Start collecting function arguments */` |
|   585738 |  6814 | `		SySetReset(&aArg);` |
|  1572590 |  6815 | `		while( pArg < pTos ){` |
|   986854 |  6816 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   986854 |  6817 | `			pArg++;` |
|        2 |  6818 | `		}` |
|        - |  6819 | `		/* Assume a null return value */` |
|   585738 |  6820 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6821 | `		/* Init the call context */` |
|   585738 |  6822 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6823 | `		/* Call the foreign function */` |
|   585738 |  6824 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6825 | `		/* Release the call context */` |
|   585738 |  6826 | `		VmReleaseCallContext(&sCtx);` |
|   585738 |  6827 | `		if( rc == PH7_ABORT ){` |
|      471 |  6828 | `			goto Abort;` |
|   585268 |  6829 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6830 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6831 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6832 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6833 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6834 | `				goto Exception;` |
|        - |  6835 | `			}` |
|        - |  6836 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6837 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6838 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6839 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6840 | `			}` |
|        - |  6841 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6842 | `			VmPopOperand(&pTos,1);` |
|        - |  6843 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6844 | `			pFrm = pVm->pFrame;` |
|        7 |  6845 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6846 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6847 | `			}` |
|        7 |  6848 | `			break;` |
|        - |  6849 | `		}` |
|   585258 |  6850 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6851 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6852 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6853 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6854 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6855 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6856 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  6857 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  6858 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  6859 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6860 | `			}` |
|        - |  6861 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6862 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  6863 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  6864 | `			goto Suspend;` |
|        - |  6865 | `		}` |
|   585220 |  6866 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6867 | `			/* Pop function name and arguments */` |
|   566540 |  6868 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   283291 |  6869 | `		}` |
|        - |  6870 | `		/* Save foreign function return value */` |
|   585220 |  6871 | `		PH7_MemObjStore(&sRet,pTos);` |
|   585220 |  6872 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6873 | `	}` |
|   598600 |  6874 | `	break;` |
|        - |  6875 | `				  }` |
|        - |  6876 | `/*` |
|        - |  6877 | ` * OP_CONSUME: P1 * *` |
|        - |  6878 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6879 | ` */` |
|    11886 |  6880 | `case PH7_OP_CONSUME: {` |
|    23774 |  6881 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    23774 |  6882 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6883 |  |
|    23774 |  6884 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    23774 |  6885 | `	pCur = pOut;` |
|        - |  6886 | `	/* Start the consume process  */` |
|    47546 |  6887 | `	while( pOut <= pTos ){` |
|        - |  6888 | `		/* Force a string cast */` |
|    23774 |  6889 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6890 | `			PH7_MemObjToString(pOut);` |
|      149 |  6891 | `		}` |
|    23774 |  6892 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6893 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6894 | `			/* Invoke the output consumer callback */` |
|    13296 |  6895 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    13296 |  6896 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    13296 |  6897 | `			SyBlobRelease(&pOut->sBlob);` |
|    13296 |  6898 | `			if( rc == SXERR_ABORT ){` |
|        - |  6899 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6900 | `				goto Abort;` |
|        - |  6901 | `			}` |
|     6647 |  6902 | `		}` |
|    23774 |  6903 | `		pOut++;` |
|        2 |  6904 | `	}` |
|    23774 |  6905 | `	pTos = &pCur[-1];` |
|    23772 |  6906 | `	break;` |
|        - |  6907 | `					 }` |
|        - |  6908 |  |
|        - |  6909 | `		} /* Switch() */` |
| 10090270 |  6910 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6911 | `	} /* For(;;) */` |
|    16324 |  6912 | `Done:` |
|    32650 |  6913 | `	SySetRelease(&aArg);` |
|    32650 |  6914 | `	return SXRET_OK;` |
|       66 |  6915 | `Suspend:` |
|      134 |  6916 | `	SySetRelease(&aArg);` |
|      134 |  6917 | `	return PH7_SUSPEND;` |
|      242 |  6918 | `Abort:` |
|      485 |  6919 | `	SySetRelease(&aArg);` |
|     1685 |  6920 | `	while( pTos >= pStack ){` |
|     1201 |  6921 | `		PH7_MemObjRelease(pTos);` |
|     1201 |  6922 | `		pTos--;` |
|        1 |  6923 | `	}` |
|      485 |  6924 | `	return PH7_ABORT;` |
|        3 |  6925 | `Exception:` |
|        8 |  6926 | `	SySetRelease(&aArg);` |
|       22 |  6927 | `	while( pTos >= pStack ){` |
|       16 |  6928 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6929 | `		pTos--;` |
|        2 |  6930 | `	}` |
|        8 |  6931 | `	return PH7_EXCEPTION;` |
|    16637 |  6932 |  |
|        - |  6933 | `/*` |
|        - |  6934 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6935 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6936 | ` * See block-comment on that function for additional information.` |
|        - |  6937 | ` */` |
|    15404 |  6938 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6939 |  |
|        - |  6940 | `	ph7_value *pStack;` |
|        - |  6941 | `	sxi32 rc;` |
|        - |  6942 | `	/* Allocate a new operand stack */` |
|    15406 |  6943 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15406 |  6944 | `	if( pStack == 0 ){` |
|      ! 0 |  6945 | `		return SXERR_MEM;` |
|        - |  6946 | `	}` |
|        - |  6947 | `	/* Execute the program */` |
|    15406 |  6948 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6949 | `	/* Free the operand stack */` |
|    15406 |  6950 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6951 | `	/* Execution result */` |
|    15406 |  6952 | `	return rc;` |
|     7704 |  6953 |  |
|        - |  6954 | `/*` |
|        - |  6955 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6956 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6957 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6958 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6959 | ` * execution ends.` |
|        - |  6960 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6961 | ` * additional information.` |
|        - |  6962 | ` */` |
|     2332 |  6963 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6964 |  |
|        - |  6965 | `	VmShutdownCB *pEntry;` |
|        - |  6966 | `	ph7_value *apArg[10];` |
|        - |  6967 | `	sxu32 n,nEntry;` |
|        - |  6968 | `	int i;` |
|        - |  6969 | `	/* Point to the stack of registered callbacks */` |
|     2334 |  6970 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25654 |  6971 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23322 |  6972 | `		apArg[i] = 0;` |
|    11662 |  6973 | `	}` |
|     2336 |  6974 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6975 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6976 | `		if( pEntry ){` |
|        - |  6977 | `			/* Prepare callback arguments if any */` |
|        3 |  6978 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6979 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6980 | `					break;` |
|        - |  6981 | `				}` |
|      ! 0 |  6982 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6983 | `			}` |
|        - |  6984 | `			/* Invoke the callback */` |
|        3 |  6985 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6986 | `			/*` |
|        - |  6987 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6988 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6989 | `			 */` |
|        3 |  6990 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6991 | `			if( pEntry ){` |
|        3 |  6992 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6993 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6994 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6995 | `				}` |
|        1 |  6996 | `			}` |
|        1 |  6997 | `		}` |
|        2 |  6998 | `	}` |
|     2334 |  6999 | `	SySetReset(&pVm->aShutdown);` |
|     2334 |  7000 |  |
|        - |  7001 | `/*` |
|        - |  7002 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  7003 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7004 | ` * See block-comment on that function for additional information.` |
|        - |  7005 | ` */` |
|     2340 |  7006 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  7007 |  |
|        - |  7008 | `	/* Make sure we are ready to execute this program */` |
|     2342 |  7009 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  7010 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  7011 | `	}` |
|        - |  7012 | `	/* Set the execution magic number  */` |
|     2342 |  7013 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  7014 | `	/* Execute the program */` |
|     2342 |  7015 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  7016 | `	/* Invoke any shutdown callbacks */` |
|     2338 |  7017 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  7018 | `	/*` |
|        - |  7019 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  7020 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  7021 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  7022 | `	 */` |
|     2338 |  7023 | `	return SXRET_OK;` |
|     1172 |  7024 |  |
|        - |  7025 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  7026 | `/*` |
|        - |  7027 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  7028 | ` * The context is in CREATED state and ready to be started.` |
|        - |  7029 | ` */` |
|       42 |  7030 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  7031 |  |
|        - |  7032 | `	ph7_exec_ctx *pCtx;` |
|        - |  7033 | `	ph7_value *pStack;` |
|        - |  7034 | `	VmFrame *pFrame;` |
|       44 |  7035 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  7036 | `	if( pCtx == 0 ){` |
|      ! 0 |  7037 | `		return 0;` |
|        - |  7038 | `	}` |
|       44 |  7039 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  7040 | `	pCtx->pVm = pVm;` |
|       44 |  7041 | `	pCtx->pFunc = pFunc;` |
|       44 |  7042 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  7043 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  7044 | `	pCtx->pc = 0;` |
|       44 |  7045 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  7046 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  7047 | `	/* Allocate a private operand stack */` |
|       44 |  7048 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  7049 | `	if( pStack == 0 ){` |
|      ! 0 |  7050 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7051 | `		return 0;` |
|        - |  7052 | `	}` |
|       44 |  7053 | `	pCtx->pStack = pStack;` |
|        - |  7054 | `	/* Create a detached frame for the fiber */` |
|       44 |  7055 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  7056 | `	if( pFrame == 0 ){` |
|      ! 0 |  7057 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  7058 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7059 | `		return 0;` |
|        - |  7060 | `	}` |
|       44 |  7061 | `	pCtx->pFrame = pFrame;` |
|       44 |  7062 | `	return pCtx;` |
|       23 |  7063 |  |
|        - |  7064 | `/*` |
|        - |  7065 | ` * Start executing a fiber context for the first time.` |
|        - |  7066 | ` */` |
|       42 |  7067 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  7068 |  |
|        - |  7069 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7070 | `	sxi32 rc;` |
|       44 |  7071 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7072 | `		return SXERR_INVALID;` |
|        - |  7073 | `	}` |
|        - |  7074 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  7075 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  7076 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7077 | `	/* Save and set the active context */` |
|       44 |  7078 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  7079 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  7080 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  7081 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  7082 | `	pVm->nRecursionDepth++;` |
|        - |  7083 | `	/* Execute from the beginning */` |
|       65 |  7084 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  7085 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  7086 | `	pVm->nRecursionDepth--;` |
|        - |  7087 | `	/* Restore the previous context */` |
|       44 |  7088 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  7089 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7090 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  7091 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  7092 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  7093 | `		if( pResult ){` |
|       24 |  7094 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7095 | `		}` |
|       42 |  7096 | `		return SXRET_OK;` |
|        - |  7097 | `	}` |
|        - |  7098 | `	/* Detach frame */` |
|        3 |  7099 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7100 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7101 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7102 | `	}` |
|        3 |  7103 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7104 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7105 | `		return PH7_ABORT;` |
|        - |  7106 | `	}` |
|        3 |  7107 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7108 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7109 | `		return PH7_EXCEPTION;` |
|        - |  7110 | `	}` |
|        - |  7111 | `	/* Normal completion */` |
|        3 |  7112 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7113 | `	if( pResult ){` |
|        3 |  7114 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7115 | `	}` |
|        3 |  7116 | `	return SXRET_OK;` |
|       23 |  7117 |  |
|        - |  7118 | `/*` |
|        - |  7119 | ` * Resume a suspended fiber context.` |
|        - |  7120 | ` */` |
|       86 |  7121 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7122 |  |
|        - |  7123 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7124 | `	sxi32 rc;` |
|       88 |  7125 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7126 | `		return SXERR_INVALID;` |
|        - |  7127 | `	}` |
|        - |  7128 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7129 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7130 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7131 | `	if( pResumeValue ){` |
|       40 |  7132 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7133 | `	}else{` |
|       50 |  7134 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7135 | `	}` |
|       88 |  7136 | `	pCtx->nTos++;` |
|        - |  7137 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7138 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7139 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7140 | `	/* Save and set the active context */` |
|       88 |  7141 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7142 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7143 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7144 | `	pVm->nRecursionDepth++;` |
|        - |  7145 | `	/* Resume execution from saved PC */` |
|      131 |  7146 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7147 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7148 | `	pVm->nRecursionDepth--;` |
|        - |  7149 | `	/* Restore the previous context */` |
|       88 |  7150 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7151 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7152 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7153 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7154 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7155 | `		if( pResult ){` |
|       18 |  7156 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7157 | `		}` |
|       56 |  7158 | `		return SXRET_OK;` |
|        - |  7159 | `	}` |
|        - |  7160 | `	/* Detach frame */` |
|       34 |  7161 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7162 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7163 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7164 | `	}` |
|       34 |  7165 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7166 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7167 | `		return PH7_ABORT;` |
|        - |  7168 | `	}` |
|       34 |  7169 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7170 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7171 | `		return PH7_EXCEPTION;` |
|        - |  7172 | `	}` |
|        - |  7173 | `	/* Normal completion */` |
|       34 |  7174 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7175 | `	if( pResult ){` |
|       20 |  7176 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7177 | `	}` |
|       34 |  7178 | `	return SXRET_OK;` |
|       45 |  7179 |  |
|        - |  7180 | `/*` |
|        - |  7181 | ` * Release an execution context and all its resources.` |
|        - |  7182 | ` */` |
|        4 |  7183 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7184 |  |
|        5 |  7185 | `	if( pCtx == 0 ){` |
|      ! 0 |  7186 | `		return;` |
|        - |  7187 | `	}` |
|        5 |  7188 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7189 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7190 | `		return;` |
|        - |  7191 | `	}` |
|        5 |  7192 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7193 | `	/* Release values */` |
|        5 |  7194 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7195 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7196 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7197 | `	if( pCtx->pFrame ){` |
|        - |  7198 | `		VmSlot *aSlot;` |
|        - |  7199 | `		sxu32 n;` |
|        - |  7200 | `		/* Free local variables */` |
|        5 |  7201 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7202 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7203 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7204 | `		}` |
|        - |  7205 | `		/* Remove local references */` |
|        5 |  7206 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7207 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7208 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7209 | `		}` |
|        5 |  7210 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7211 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7212 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7213 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7214 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7215 | `		pCtx->pFrame = 0;` |
|        2 |  7216 | `	}` |
|        - |  7217 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7218 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7219 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7220 | `	if( pCtx->pStack ){` |
|        5 |  7221 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7222 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7223 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7224 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7225 | `				pTos--;` |
|        1 |  7226 | `			}` |
|        2 |  7227 | `		}` |
|        5 |  7228 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7229 | `		pCtx->pStack = 0;` |
|        2 |  7230 | `	}` |
|        - |  7231 | `	/* Free the context itself */` |
|        5 |  7232 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7233 |  |
|        - |  7234 | `/*` |
|        - |  7235 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7236 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7237 | ` */` |
|       90 |  7238 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7239 |  |
|        - |  7240 | `	ph7_class_instance *pThis;` |
|        - |  7241 | `	SyString sAttr;` |
|        - |  7242 | `	ph7_value *pAttr;` |
|       92 |  7243 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7244 | `		return 0;` |
|        - |  7245 | `	}` |
|       92 |  7246 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7247 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7248 | `		return 0;` |
|        - |  7249 | `	}` |
|       92 |  7250 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7251 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7252 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7253 | `		return 0;` |
|        - |  7254 | `	}` |
|       62 |  7255 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7256 |  |
|        - |  7257 | `/*` |
|        - |  7258 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7259 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7260 | ` */` |
|       38 |  7261 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7262 |  |
|       40 |  7263 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7264 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7265 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7266 | `			"Cannot suspend outside of a fiber");` |
|        - |  7267 | `	}` |
|       40 |  7268 | `	if( nArg > 0 ){` |
|       40 |  7269 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7270 | `	}else{` |
|      ! 0 |  7271 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7272 | `	}` |
|       40 |  7273 | `	return PH7_SUSPEND;` |
|       21 |  7274 |  |
|        - |  7275 | `/*` |
|        - |  7276 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7277 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7278 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7279 | ` */` |
|       24 |  7280 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7281 |  |
|        - |  7282 | `	ph7_class_instance *pThis;` |
|        - |  7283 | `	ph7_value *pAttr;` |
|        - |  7284 | `	SyString sAttrName;` |
|       26 |  7285 | `	if( nArg < 2 ){` |
|      ! 0 |  7286 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7287 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7288 | `	}` |
|       26 |  7289 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7290 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7291 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7292 | `	}` |
|       26 |  7293 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7294 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7295 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7296 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7297 | `	}` |
|        - |  7298 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7299 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7300 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7301 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7302 | `	}` |
|        - |  7303 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7304 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7305 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7306 | `	if( pAttr ){` |
|       26 |  7307 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7308 | `	}` |
|       26 |  7309 | `	return PH7_OK;` |
|       14 |  7310 |  |
|        - |  7311 | `/*` |
|        - |  7312 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7313 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7314 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7315 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7316 | ` */` |
|       24 |  7317 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7318 | `	ph7_class_instance **ppThis)` |
|        2 |  7319 |  |
|       26 |  7320 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7321 | `	ph7_value *pCallable;` |
|        - |  7322 | `	SyString sAttrName;` |
|       26 |  7323 | `	*ppThis = 0;` |
|       26 |  7324 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7325 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7326 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7327 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7328 | `		return 0;` |
|        - |  7329 | `	}` |
|       26 |  7330 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7331 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7332 | `		SyString sName;` |
|        - |  7333 | `		SyHashEntry *pEntry;` |
|        - |  7334 | `		ph7_vm_func *pFunc;` |
|       26 |  7335 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7336 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7337 | `		if( pEntry == 0 ){` |
|      ! 0 |  7338 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7339 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7340 | `			return 0;` |
|        - |  7341 | `		}` |
|       26 |  7342 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7343 | `		return pFunc;` |
|      ! 0 |  7344 | `	}else{` |
|        - |  7345 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7346 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7347 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7348 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7349 | `		if( pMethod == 0 ){` |
|      ! 0 |  7350 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7351 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7352 | `			return 0;` |
|        - |  7353 | `		}` |
|      ! 0 |  7354 | `		*ppThis = pClosure;` |
|      ! 0 |  7355 | `		return &pMethod->sFunc;` |
|        - |  7356 | `	}` |
|       14 |  7357 |  |
|        - |  7358 | `/*` |
|        - |  7359 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7360 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7361 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7362 | ` */` |
|       42 |  7363 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7364 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7365 |  |
|       44 |  7366 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7367 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7368 | `	sxu32 nFormal, n;` |
|        - |  7369 | `	VmSlot sSlot;` |
|        - |  7370 | `	sxi32 rc;` |
|        - |  7371 | `	/* Install $this for closure/method callables */` |
|       44 |  7372 | `	if( pClosureThis ){` |
|        - |  7373 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7374 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7375 | `		if( pObj ){` |
|      ! 0 |  7376 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7377 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7378 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7379 | `		}` |
|      ! 0 |  7380 | `	}` |
|        - |  7381 | `	/* Install static variables */` |
|       44 |  7382 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7383 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7384 | `		ph7_value *pVal;` |
|      ! 0 |  7385 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7386 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7387 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7388 | `			if( pVal ){` |
|      ! 0 |  7389 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7390 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7391 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7392 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7393 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7394 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7395 | `				}` |
|      ! 0 |  7396 | `			}` |
|      ! 0 |  7397 | `		}` |
|      ! 0 |  7398 | `	}` |
|        - |  7399 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  7400 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  7401 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  7402 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7403 | `		ph7_value *pObj;` |
|       12 |  7404 | `		if( n < (sxu32)nArg ){` |
|        - |  7405 | `			/* Argument provided — install with type casting */` |
|       12 |  7406 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  7407 | `			if( pObj ){` |
|       12 |  7408 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7409 | `				/* Type casting */` |
|       12 |  7410 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7411 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7412 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7413 | `						if( xCast ){` |
|      ! 0 |  7414 | `							xCast(pObj);` |
|      ! 0 |  7415 | `						}` |
|      ! 0 |  7416 | `					}` |
|      ! 0 |  7417 | `				}` |
|       12 |  7418 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  7419 | `				sSlot.pUserData = 0;` |
|       12 |  7420 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  7421 | `			}` |
|        5 |  7422 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7423 | `			/* Default value */` |
|      ! 0 |  7424 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7425 | `			if( pObj ){` |
|      ! 0 |  7426 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7427 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7428 | `					return rc;` |
|        - |  7429 | `				}` |
|      ! 0 |  7430 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7431 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7432 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7433 | `						if( xCast ){` |
|      ! 0 |  7434 | `							xCast(pObj);` |
|      ! 0 |  7435 | `						}` |
|      ! 0 |  7436 | `					}` |
|      ! 0 |  7437 | `				}` |
|      ! 0 |  7438 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7439 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7440 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7441 | `			}` |
|      ! 0 |  7442 | `		}` |
|        7 |  7443 | `	}` |
|        - |  7444 | `	/* Install closure environment (captured variables) */` |
|       44 |  7445 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7446 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7447 | `		ph7_value *pValue;` |
|        - |  7448 | `		sxu32 iEnv;` |
|        3 |  7449 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7450 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7451 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7452 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7453 | `				continue;` |
|        - |  7454 | `			}` |
|        5 |  7455 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7456 | `			if( pValue == 0 ){` |
|      ! 0 |  7457 | `				continue;` |
|        - |  7458 | `			}` |
|        5 |  7459 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7460 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7461 | `		}` |
|        1 |  7462 | `	}` |
|       44 |  7463 | `	return SXRET_OK;` |
|       23 |  7464 |  |
|        - |  7465 | `/*` |
|        - |  7466 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7467 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7468 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7469 | ` */` |
|       26 |  7470 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7471 |  |
|       28 |  7472 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7473 | `	ph7_class_instance *pThis;` |
|        - |  7474 | `	ph7_class_instance *pClosureThis;` |
|        - |  7475 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7476 | `	ph7_vm_func *pFunc;` |
|        - |  7477 | `	ph7_value sResult;` |
|        - |  7478 | `	ph7_value *pCtxAttr;` |
|        - |  7479 | `	SyString sAttrName;` |
|        - |  7480 | `	sxi32 rc;` |
|       28 |  7481 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7482 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7483 | `	}` |
|       28 |  7484 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7485 | `	/* Check if already started (has a __ctx) */` |
|       28 |  7486 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  7487 | `	if( pExecCtx != 0 ){` |
|        3 |  7488 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7489 | `			"Cannot start a fiber that has already been started");` |
|        - |  7490 | `	}` |
|        - |  7491 | `	/* Resolve callable */` |
|       26 |  7492 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  7493 | `	if( pFunc == 0 ){` |
|      ! 0 |  7494 | `		return PH7_EXCEPTION;` |
|        - |  7495 | `	}` |
|        - |  7496 | `	/* Create execution context now that we know the function */` |
|       26 |  7497 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  7498 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7499 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7500 | `			"Fiber::start(): out of memory");` |
|        - |  7501 | `	}` |
|        - |  7502 | `	/* Store context in $this->__ctx */` |
|       26 |  7503 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  7504 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7505 | `	if( pCtxAttr ){` |
|       26 |  7506 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  7507 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7508 | `	}` |
|        - |  7509 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7510 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7511 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  7512 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  7513 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7514 | `	/* Unpack the args array and install into the frame */` |
|        - |  7515 | `	{` |
|       26 |  7516 | `		ph7_value **apValues = 0;` |
|       26 |  7517 | `		int nActual = 0;` |
|       26 |  7518 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  7519 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7520 | `			ph7_hashmap_node *pNode;` |
|       26 |  7521 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  7522 | `			if( nCount > 0 ){` |
|        3 |  7523 | `				sxu32 idx = 0;` |
|        4 |  7524 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7525 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7526 | `				if( apValues ){` |
|        3 |  7527 | `					pNode = pMap->pFirst;` |
|        7 |  7528 | `					while( pNode && idx < nCount ){` |
|        5 |  7529 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7530 | `						idx++;` |
|        5 |  7531 | `						pNode = pNode->pPrev;` |
|        1 |  7532 | `					}` |
|        3 |  7533 | `					nActual = (int)idx;` |
|        1 |  7534 | `				}` |
|        1 |  7535 | `			}` |
|       12 |  7536 | `		}` |
|       26 |  7537 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  7538 | `		if( apValues ){` |
|        3 |  7539 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7540 | `		}` |
|        - |  7541 | `	}` |
|        - |  7542 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  7543 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  7544 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  7545 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7546 | `		return PH7_ABORT;` |
|        - |  7547 | `	}` |
|       26 |  7548 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  7549 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  7550 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7551 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7552 | `		return PH7_ABORT;` |
|        - |  7553 | `	}` |
|       26 |  7554 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7555 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7556 | `		return PH7_EXCEPTION;` |
|        - |  7557 | `	}` |
|       26 |  7558 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  7559 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  7560 | `	return PH7_OK;` |
|       15 |  7561 |  |
|        - |  7562 | `/*` |
|        - |  7563 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7564 | ` */` |
|       36 |  7565 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7566 |  |
|       38 |  7567 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7568 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7569 | `	ph7_value sResult;` |
|        - |  7570 | `	ph7_value *pResumeVal;` |
|        - |  7571 | `	sxi32 rc;` |
|       38 |  7572 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7573 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7574 | `		return PH7_OK;` |
|        - |  7575 | `	}` |
|       38 |  7576 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  7577 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7578 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7579 | `		return PH7_OK;` |
|        - |  7580 | `	}` |
|       38 |  7581 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7582 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7583 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7584 | `	}` |
|       36 |  7585 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  7586 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  7587 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  7588 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7589 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7590 | `		return PH7_ABORT;` |
|        - |  7591 | `	}` |
|       36 |  7592 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7593 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7594 | `		return PH7_EXCEPTION;` |
|        - |  7595 | `	}` |
|       36 |  7596 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  7597 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  7598 | `	return PH7_OK;` |
|       20 |  7599 |  |
|        - |  7600 | `/*` |
|        - |  7601 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7602 | ` */` |
|        6 |  7603 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7604 |  |
|        8 |  7605 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7606 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  7607 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7608 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7609 | `		return PH7_OK;` |
|        - |  7610 | `	}` |
|        8 |  7611 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  7612 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7613 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7614 | `		return PH7_OK;` |
|        - |  7615 | `	}` |
|        8 |  7616 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7617 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7618 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7619 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7620 | `		}` |
|      ! 0 |  7621 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7622 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7623 | `	}` |
|        8 |  7624 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  7625 | `	return PH7_OK;` |
|        5 |  7626 |  |
|        - |  7627 | `/*` |
|        - |  7628 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7629 | ` */` |
|        6 |  7630 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7631 |  |
|        - |  7632 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7633 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7634 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7635 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7636 | `	return PH7_OK;` |
|        4 |  7637 |  |
|      ! 0 |  7638 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7639 |  |
|        - |  7640 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7641 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7642 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7643 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7644 | `	return PH7_OK;` |
|      ! 0 |  7645 |  |
|        6 |  7646 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7647 |  |
|        - |  7648 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7649 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7650 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7651 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7652 | `	return PH7_OK;` |
|        4 |  7653 |  |
|        6 |  7654 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7655 |  |
|        - |  7656 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7657 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7658 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7659 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7660 | `	return PH7_OK;` |
|        4 |  7661 |  |
|        - |  7662 | `/*` |
|        - |  7663 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7664 | ` */` |
|        4 |  7665 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7666 |  |
|        5 |  7667 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7668 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  7669 | `	if( nArg < 1 ){` |
|      ! 0 |  7670 | `		return PH7_OK;` |
|        - |  7671 | `	}` |
|        5 |  7672 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  7673 | `	if( pExecCtx ){` |
|        5 |  7674 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7675 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  7676 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  7677 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7678 | `			SyString sAttrName;` |
|        - |  7679 | `			ph7_value *pAttr;` |
|        5 |  7680 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  7681 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  7682 | `			if( pAttr ){` |
|        5 |  7683 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  7684 | `			}` |
|        2 |  7685 | `		}` |
|        2 |  7686 | `	}` |
|        5 |  7687 | `	return PH7_OK;` |
|        3 |  7688 |  |
|        - |  7689 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7690 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7691 |  |
|        - |  7692 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7693 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7694 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7695 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7696 |  |
|      ! 0 |  7697 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7698 |  |
|        - |  7699 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7700 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7701 | `	ph7_exec_ctx *pCtx;` |
|        - |  7702 | `	ph7_vm_func *pFunc;` |
|        - |  7703 | `	ph7_value *pCallable;` |
|        - |  7704 | `	ph7_value *pCtxAttr;` |
|        - |  7705 | `	SyString sAttrName;` |
|        - |  7706 | `	/* Must not already be started */` |
|      ! 0 |  7707 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7708 | `	if( pCtx != 0 ){` |
|      ! 0 |  7709 | `		return SXERR_INVALID;` |
|        - |  7710 | `	}` |
|      ! 0 |  7711 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7712 | `		return SXERR_INVALID;` |
|        - |  7713 | `	}` |
|      ! 0 |  7714 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7715 | `	/* Get the callable */` |
|      ! 0 |  7716 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7717 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7718 | `	if( pCallable == 0 ){` |
|      ! 0 |  7719 | `		return SXERR_INVALID;` |
|        - |  7720 | `	}` |
|        - |  7721 | `	/* Resolve callable */` |
|      ! 0 |  7722 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7723 | `		SyString sName;` |
|        - |  7724 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7725 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7726 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7727 | `		if( pEntry == 0 ){` |
|      ! 0 |  7728 | `			return SXERR_NOTFOUND;` |
|        - |  7729 | `		}` |
|      ! 0 |  7730 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7731 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7732 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7733 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7734 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7735 | `		if( pMethod == 0 ){` |
|      ! 0 |  7736 | `			return SXERR_INVALID;` |
|        - |  7737 | `		}` |
|      ! 0 |  7738 | `		pClosureThis = pClosure;` |
|      ! 0 |  7739 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7740 | `	}else{` |
|      ! 0 |  7741 | `		return SXERR_INVALID;` |
|        - |  7742 | `	}` |
|        - |  7743 | `	/* Create context */` |
|      ! 0 |  7744 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7745 | `	if( pCtx == 0 ){` |
|      ! 0 |  7746 | `		return SXERR_MEM;` |
|        - |  7747 | `	}` |
|        - |  7748 | `	/* Store in __ctx */` |
|      ! 0 |  7749 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7750 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7751 | `	if( pCtxAttr ){` |
|      ! 0 |  7752 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7753 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7754 | `	}` |
|        - |  7755 | `	/* Set up frame with args */` |
|      ! 0 |  7756 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7757 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7758 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7759 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7760 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7761 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7762 |  |
|      ! 0 |  7763 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7764 |  |
|      ! 0 |  7765 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7766 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7767 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7768 |  |
|      ! 0 |  7769 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7770 |  |
|      ! 0 |  7771 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7772 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7773 |  |
|      ! 0 |  7774 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7775 |  |
|      ! 0 |  7776 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7777 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7778 |  |
|      ! 0 |  7779 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7780 |  |
|      ! 0 |  7781 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7782 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7783 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7784 |  |
|        - |  7785 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7786 | `/*` |
|        - |  7787 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7788 | ` */` |
|       18 |  7789 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  7790 |  |
|        - |  7791 | `	ph7_generator *pGen;` |
|       20 |  7792 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  7793 | `	if( pGen == 0 ){` |
|      ! 0 |  7794 | `		return 0;` |
|        - |  7795 | `	}` |
|       20 |  7796 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  7797 | `	pGen->pCtx = pCtx;` |
|       20 |  7798 | `	pGen->iImplicitKey = 0;` |
|       20 |  7799 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  7800 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7801 | `	/* Link the generator back to the exec context */` |
|       20 |  7802 | `	pCtx->pPrivate = pGen;` |
|       20 |  7803 | `	return pGen;` |
|       11 |  7804 |  |
|        - |  7805 | `/*` |
|        - |  7806 | ` * Release a generator and its execution context.` |
|        - |  7807 | ` */` |
|      ! 0 |  7808 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7809 |  |
|      ! 0 |  7810 | `	if( pGen == 0 ){` |
|      ! 0 |  7811 | `		return;` |
|        - |  7812 | `	}` |
|      ! 0 |  7813 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7814 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7815 | `	if( pGen->pCtx ){` |
|      ! 0 |  7816 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7817 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7818 | `		pGen->pCtx = 0;` |
|      ! 0 |  7819 | `	}` |
|      ! 0 |  7820 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7821 |  |
|        - |  7822 | `/*` |
|        - |  7823 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7824 | ` */` |
|      192 |  7825 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  7826 |  |
|        - |  7827 | `	ph7_class_instance *pThis;` |
|        - |  7828 | `	SyString sAttr;` |
|        - |  7829 | `	ph7_value *pAttr;` |
|      194 |  7830 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7831 | `		return 0;` |
|        - |  7832 | `	}` |
|      194 |  7833 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  7834 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7835 | `		return 0;` |
|        - |  7836 | `	}` |
|      194 |  7837 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  7838 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  7839 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7840 | `		return 0;` |
|        - |  7841 | `	}` |
|      194 |  7842 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  7843 |  |
|        - |  7844 | `/*` |
|        - |  7845 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7846 | ` */` |
|       18 |  7847 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7848 |  |
|        - |  7849 | `	ph7_generator *pGen;` |
|        - |  7850 | `	sxi32 rc;` |
|       20 |  7851 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  7852 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  7853 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  7854 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  7855 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  7856 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  7857 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7858 | `	}` |
|       20 |  7859 | `	return PH7_OK;` |
|       11 |  7860 |  |
|        - |  7861 | `/*` |
|        - |  7862 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7863 | ` */` |
|       52 |  7864 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7865 |  |
|        - |  7866 | `	ph7_generator *pGen;` |
|       54 |  7867 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  7868 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  7869 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  7870 | `	return PH7_OK;` |
|       28 |  7871 |  |
|        - |  7872 | `/*` |
|        - |  7873 | ` * Generator::current() — return the last yielded value.` |
|        - |  7874 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7875 | ` */` |
|       56 |  7876 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7877 |  |
|        - |  7878 | `	ph7_generator *pGen;` |
|        - |  7879 | `	sxi32 rc;` |
|       58 |  7880 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7881 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  7882 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7883 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7884 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7885 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7886 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7887 | `	}` |
|       58 |  7888 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  7889 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  7890 | `	}else{` |
|      ! 0 |  7891 | `		ph7_result_null(pCtx);` |
|        - |  7892 | `	}` |
|       58 |  7893 | `	return PH7_OK;` |
|       30 |  7894 |  |
|        - |  7895 | `/*` |
|        - |  7896 | ` * Generator::key() — return the last yielded key.` |
|        - |  7897 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7898 | ` */` |
|       12 |  7899 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7900 |  |
|        - |  7901 | `	ph7_generator *pGen;` |
|        - |  7902 | `	sxi32 rc;` |
|       13 |  7903 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7904 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7905 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7906 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7907 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7908 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7909 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7910 | `	}` |
|       13 |  7911 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7912 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7913 | `	}else{` |
|      ! 0 |  7914 | `		ph7_result_null(pCtx);` |
|        - |  7915 | `	}` |
|       13 |  7916 | `	return PH7_OK;` |
|        7 |  7917 |  |
|        - |  7918 | `/*` |
|        - |  7919 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7920 | ` */` |
|       48 |  7921 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7922 |  |
|        - |  7923 | `	ph7_generator *pGen;` |
|        - |  7924 | `	sxi32 rc;` |
|       50 |  7925 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  7926 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  7927 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  7928 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7929 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  7930 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  7931 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  7932 | `	}else{` |
|      ! 0 |  7933 | `		return PH7_OK;` |
|        - |  7934 | `	}` |
|       50 |  7935 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  7936 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  7937 | `	return PH7_OK;` |
|       26 |  7938 |  |
|        - |  7939 | `/*` |
|        - |  7940 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7941 | ` */` |
|        4 |  7942 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7943 |  |
|        - |  7944 | `	ph7_generator *pGen;` |
|        - |  7945 | `	ph7_value *pSendVal;` |
|        - |  7946 | `	sxi32 rc;` |
|        5 |  7947 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7948 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7949 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7950 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7951 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7952 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7953 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7954 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7955 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7956 | `	}else{` |
|      ! 0 |  7957 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7958 | `		return PH7_OK;` |
|        - |  7959 | `	}` |
|        5 |  7960 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7961 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7962 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7963 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7964 | `	}else{` |
|        3 |  7965 | `		ph7_result_null(pCtx);` |
|        - |  7966 | `	}` |
|        5 |  7967 | `	return PH7_OK;` |
|        3 |  7968 |  |
|        - |  7969 | `/*` |
|        - |  7970 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  7971 | ` *` |
|        - |  7972 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  7973 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  7974 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  7975 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  7976 | ` * the exception to the caller.` |
|        - |  7977 | ` */` |
|      ! 0 |  7978 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7979 |  |
|        - |  7980 | `	ph7_generator *pGen;` |
|        - |  7981 | `	const char *zMsg;` |
|        - |  7982 | `	int nLen;` |
|      ! 0 |  7983 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  7984 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7985 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  7986 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  7987 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  7988 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  7989 | `			"Cannot throw into a closed generator");` |
|        - |  7990 | `	}` |
|        - |  7991 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  7992 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  7993 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  7994 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  7995 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7996 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  7997 | `	nLen = 0;` |
|      ! 0 |  7998 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  7999 | `		/* Try to get the exception's message */` |
|        - |  8000 | `		SyString sAttr;` |
|        - |  8001 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  8002 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  8003 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  8004 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  8005 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  8006 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  8007 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  8008 | `		}` |
|      ! 0 |  8009 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  8010 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  8011 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  8012 | `	}` |
|      ! 0 |  8013 | `	(void)nLen;` |
|      ! 0 |  8014 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  8015 |  |
|        - |  8016 | `/*` |
|        - |  8017 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  8018 | ` */` |
|        2 |  8019 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8020 |  |
|        - |  8021 | `	ph7_generator *pGen;` |
|        3 |  8022 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8023 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  8024 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8025 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8026 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8027 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  8028 | `	}` |
|        3 |  8029 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  8030 | `	return PH7_OK;` |
|        2 |  8031 |  |
|        - |  8032 | `/*` |
|        - |  8033 | ` * Generator::__destruct() — clean up.` |
|        - |  8034 | ` */` |
|      ! 0 |  8035 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8036 |  |
|        - |  8037 | `	ph7_generator *pGen;` |
|      ! 0 |  8038 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  8039 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8040 | `	if( pGen ){` |
|      ! 0 |  8041 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  8042 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8043 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8044 | `			SyString sAttrName;` |
|        - |  8045 | `			ph7_value *pAttr;` |
|      ! 0 |  8046 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8047 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8048 | `			if( pAttr ){` |
|      ! 0 |  8049 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  8050 | `			}` |
|      ! 0 |  8051 | `		}` |
|      ! 0 |  8052 | `	}` |
|      ! 0 |  8053 | `	return PH7_OK;` |
|      ! 0 |  8054 |  |
|        - |  8055 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  8056 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  8057 | `/*` |
|        - |  8058 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8059 | ` * the desired message.` |
|        - |  8060 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8061 | ` * in 'api.c' for additional information.` |
|        - |  8062 | ` */` |
|      370 |  8063 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8064 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8065 | `	SyString *pString /* Message to output */` |
|        - |  8066 | `	)` |
|        2 |  8067 |  |
|      372 |  8068 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  8069 | `	sxi32 rc = SXRET_OK;` |
|        - |  8070 | `	/* Call the output consumer */` |
|      372 |  8071 | `	if( pString->nByte > 0 ){` |
|      372 |  8072 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  8073 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  8074 | `	}` |
|      372 |  8075 | `	return rc;` |
|        2 |  8076 |  |
|        - |  8077 | `/*` |
|        - |  8078 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8079 | ` * callback to consume the formatted message.` |
|        - |  8080 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8081 | ` * in 'api.c' for additional information.` |
|        - |  8082 | ` */` |
|        2 |  8083 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8084 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8085 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8086 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8087 | `	)` |
|        1 |  8088 |  |
|        3 |  8089 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  8090 | `	sxi32 rc = SXRET_OK;` |
|        - |  8091 | `	SyBlob sWorker;` |
|        - |  8092 | `	/* Format the message and call the output consumer */` |
|        3 |  8093 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  8094 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8095 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8096 | `		/* Consume the formatted message */` |
|        3 |  8097 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8098 | `	}` |
|        3 |  8099 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8100 | `	/* Release the working buffer */` |
|        3 |  8101 | `	SyBlobRelease(&sWorker);` |
|        3 |  8102 | `	return rc;` |
|        1 |  8103 |  |
|        - |  8104 | `/*` |
|        - |  8105 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8106 | ` * This function never fail and always return a pointer` |
|        - |  8107 | ` * to a null terminated string.` |
|        - |  8108 | ` */` |
|       12 |  8109 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8110 |  |
|       13 |  8111 | `	const char *zOp = "Unknown     ";` |
|       13 |  8112 | `	switch(nOp){` |
|        3 |  8113 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8114 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8115 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8116 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8117 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8118 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8119 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8120 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8121 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8122 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8123 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8124 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8125 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8126 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8127 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8128 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8129 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8130 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8131 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8132 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8133 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8134 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8135 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8136 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8137 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8138 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8139 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8140 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8141 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8142 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8143 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8144 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8145 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8146 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8147 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8148 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8149 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8150 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8151 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8152 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8153 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8154 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8155 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8156 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8157 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8158 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8159 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8160 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8161 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8162 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8163 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8164 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8165 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  8166 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8167 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8168 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8169 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8170 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8171 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8172 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8173 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8174 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8175 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8176 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8177 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8178 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8179 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8180 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8181 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8182 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8183 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8184 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8185 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8186 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8187 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8188 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8189 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8190 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8191 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8192 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8193 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8194 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8195 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8196 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8197 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8198 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8199 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8200 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8201 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8202 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8203 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8204 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8205 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8206 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8207 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8208 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8209 | `	default:` |
|      ! 0 |  8210 | `		break;` |
|        - |  8211 | `	}` |
|       13 |  8212 | `	return zOp;` |
|        1 |  8213 |  |
|        - |  8214 | `/*` |
|        - |  8215 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8216 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8217 | ` * is responsible of consuming the generated dump.` |
|        - |  8218 | ` */` |
|        2 |  8219 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8220 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8221 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8222 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8223 | `	)` |
|        1 |  8224 |  |
|        - |  8225 | `	sxi32 rc;` |
|        3 |  8226 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8227 | `	return rc;` |
|        1 |  8228 |  |
|        - |  8229 | `/*` |
|        - |  8230 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8231 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8232 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8233 | ` * in 'compile.c' for additional information.` |
|        - |  8234 | ` */` |
|       14 |  8235 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8236 |  |
|       15 |  8237 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8238 | `	/* Evaluate and expand constant value */` |
|       15 |  8239 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  8240 |  |
|        - |  8241 | `/*` |
|        - |  8242 | ` * Section:` |
|        - |  8243 | ` *  Function handling functions.` |
|        - |  8244 | ` * Status:` |
|        - |  8245 | ` *    Stable.` |
|        - |  8246 | ` */` |
|        - |  8247 | `/*` |
|        - |  8248 | ` * int func_num_args(void)` |
|        - |  8249 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8250 | ` * Parameters` |
|        - |  8251 | ` *   None.` |
|        - |  8252 | ` * Return` |
|        - |  8253 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8254 | ` *  or -1 if called from the globe scope.` |
|        - |  8255 | ` */` |
|      944 |  8256 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8257 |  |
|        - |  8258 | `	VmFrame *pFrame;` |
|        - |  8259 | `	ph7_vm *pVm;` |
|        - |  8260 | `	/* Point to the target VM */` |
|      946 |  8261 | `	pVm = pCtx->pVm;` |
|        - |  8262 | `	/* Current frame */` |
|      946 |  8263 | `	pFrame = pVm->pFrame;` |
|      946 |  8264 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  8265 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8266 | `		SXUNUSED(nArg);` |
|      ! 0 |  8267 | `		SXUNUSED(apArg);` |
|        - |  8268 | `		/* Global frame,return -1 */` |
|      ! 0 |  8269 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8270 | `		return SXRET_OK;` |
|        - |  8271 | `	}` |
|        - |  8272 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  8273 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  8274 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  8275 | `	return SXRET_OK;` |
|      474 |  8276 |  |
|        - |  8277 | `/*` |
|        - |  8278 | ` * value func_get_arg(int $arg_num)` |
|        - |  8279 | ` *   Return an item from the argument list.` |
|        - |  8280 | ` * Parameters` |
|        - |  8281 | ` *  Argument number(index start from zero).` |
|        - |  8282 | ` * Return` |
|        - |  8283 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8284 | ` */` |
|       22 |  8285 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8286 |  |
|       24 |  8287 | `	ph7_value *pObj = 0;` |
|       24 |  8288 | `	VmSlot *pSlot = 0;` |
|        - |  8289 | `	VmFrame *pFrame;` |
|        - |  8290 | `	ph7_vm *pVm;` |
|        - |  8291 | `	/* Point to the target VM */` |
|       24 |  8292 | `	pVm = pCtx->pVm;` |
|        - |  8293 | `	/* Current frame */` |
|       24 |  8294 | `	pFrame = pVm->pFrame;` |
|       24 |  8295 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8296 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8297 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8298 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8299 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8300 | `		return SXRET_OK;` |
|        - |  8301 | `	}` |
|        - |  8302 | `	/* Extract the desired index */` |
|       21 |  8303 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8304 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8305 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8306 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8307 | `		return SXRET_OK;` |
|        - |  8308 | `	}` |
|        - |  8309 | `	/* Extract the desired argument */` |
|       21 |  8310 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8311 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8312 | `			/* Return the desired argument */` |
|       21 |  8313 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8314 | `		}else{` |
|        - |  8315 | `			/* No such argument,return false */` |
|      ! 0 |  8316 | `			ph7_result_bool(pCtx,0);` |
|        - |  8317 | `		}` |
|       11 |  8318 | `	}else{` |
|        - |  8319 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8320 | `		ph7_result_bool(pCtx,0);` |
|        - |  8321 | `	}` |
|       21 |  8322 | `	return SXRET_OK;` |
|       13 |  8323 |  |
|        - |  8324 | `/*` |
|        - |  8325 | ` * array func_get_args_byref(void)` |
|        - |  8326 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8327 | ` * Parameters` |
|        - |  8328 | ` *  None.` |
|        - |  8329 | ` * Return` |
|        - |  8330 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8331 | ` *  member of the current user-defined function's argument list.` |
|        - |  8332 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8333 | ` * NOTE:` |
|        - |  8334 | ` *  Arguments are returned to the array by reference.` |
|        - |  8335 | ` */` |
|        2 |  8336 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8337 |  |
|        - |  8338 | `	ph7_value *pArray;` |
|        - |  8339 | `	VmFrame *pFrame;` |
|        - |  8340 | `	VmSlot *aSlot;` |
|        - |  8341 | `	sxu32 n;` |
|        - |  8342 | `	/* Point to the current frame */` |
|        3 |  8343 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8344 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8345 | `	if( pFrame->pParent == 0 ){` |
|        - |  8346 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8347 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8348 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8349 | `		return SXRET_OK;` |
|        - |  8350 | `	}` |
|        - |  8351 | `	/* Create a new array */` |
|        3 |  8352 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8353 | `	if( pArray == 0 ){` |
|      ! 0 |  8354 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8355 | `		SXUNUSED(apArg);` |
|      ! 0 |  8356 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8357 | `		return SXRET_OK;` |
|        - |  8358 | `	}` |
|        - |  8359 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8360 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8361 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8362 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8363 | `	}` |
|        - |  8364 | `	/* Return the freshly created array */` |
|        3 |  8365 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8366 | `	return SXRET_OK;` |
|        2 |  8367 |  |
|        - |  8368 | `/*` |
|        - |  8369 | ` * array func_get_args(void)` |
|        - |  8370 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8371 | ` * Parameters` |
|        - |  8372 | ` *  None.` |
|        - |  8373 | ` * Return` |
|        - |  8374 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8375 | ` *  member of the current user-defined function's argument list.` |
|        - |  8376 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8377 | ` */` |
|       88 |  8378 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8379 |  |
|       90 |  8380 | `	ph7_value *pObj = 0;` |
|        - |  8381 | `	ph7_value *pArray;` |
|        - |  8382 | `	VmFrame *pFrame;` |
|        - |  8383 | `	VmSlot *aSlot;` |
|        - |  8384 | `	sxu32 n;` |
|        - |  8385 | `	/* Point to the current frame */` |
|       90 |  8386 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8387 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8388 | `	if( pFrame->pParent == 0 ){` |
|        - |  8389 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8390 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8391 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8392 | `		return SXRET_OK;` |
|        - |  8393 | `	}` |
|        - |  8394 | `	/* Create a new array */` |
|       90 |  8395 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8396 | `	if( pArray == 0 ){` |
|      ! 0 |  8397 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8398 | `		SXUNUSED(apArg);` |
|      ! 0 |  8399 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8400 | `		return SXRET_OK;` |
|        - |  8401 | `	}` |
|        - |  8402 | `	/* Start filling the array with the given arguments */` |
|       90 |  8403 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8404 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8405 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8406 | `		if( pObj ){` |
|      134 |  8407 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8408 | `		}` |
|       68 |  8409 | `	}` |
|        - |  8410 | `	/* Return the freshly created array */` |
|       90 |  8411 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8412 | `	return SXRET_OK;` |
|       46 |  8413 |  |
|        - |  8414 | `/*` |
|        - |  8415 | ` * bool function_exists(string $name)` |
|        - |  8416 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8417 | ` * Parameters` |
|        - |  8418 | ` *  The name of the desired function.` |
|        - |  8419 | ` * Return` |
|        - |  8420 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8421 | ` */` |
|     1684 |  8422 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8423 |  |
|        - |  8424 | `	const char *zName;` |
|        - |  8425 | `	ph7_vm *pVm;` |
|        - |  8426 | `	int nLen;` |
|        - |  8427 | `	int res;` |
|     1686 |  8428 | `	if( nArg < 1 ){` |
|        - |  8429 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8430 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8431 | `		return SXRET_OK;` |
|        - |  8432 | `	}` |
|        - |  8433 | `	/* Point to the target VM */` |
|     1686 |  8434 | `	pVm = pCtx->pVm;` |
|        - |  8435 | `	/* Extract the function name */` |
|     1686 |  8436 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8437 | `	/* Assume the function is not defined */` |
|     1686 |  8438 | `	res = 0;` |
|        - |  8439 | `	/* Perform the lookup */` |
|     2526 |  8440 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1680 |  8441 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8442 | `			/* Function is defined */` |
|      206 |  8443 | `			res = 1;` |
|      102 |  8444 | `	}` |
|     1686 |  8445 | `	ph7_result_bool(pCtx,res);` |
|     1686 |  8446 | `	return SXRET_OK;` |
|      844 |  8447 |  |
|        - |  8448 | `/*` |
|        - |  8449 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8450 | ` * [i.e: Whether it is callable or not].` |
|        - |  8451 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8452 | ` */` |
|    17762 |  8453 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8454 |  |
|    17764 |  8455 | `	int res = 0;` |
|    17764 |  8456 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8457 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8458 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8459 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8460 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8461 | `		if( pMethod && CallInvoke ){` |
|        - |  8462 | `			ph7_value sResult;` |
|        - |  8463 | `			sxi32 rc;` |
|        - |  8464 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8465 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8466 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8467 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8468 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8469 | `			}` |
|      ! 0 |  8470 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8471 | `		}` |
|    17764 |  8472 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8473 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8474 | `		if( pMap->nEntry == 2 ){` |
|        - |  8475 | `			ph7_class *pClass;` |
|        - |  8476 | `			ph7_value *pV;` |
|        - |  8477 | `			/* Extract the target class */` |
|       12 |  8478 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8479 | `			if( pV ){` |
|       12 |  8480 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8481 | `				if( pClass ){` |
|        - |  8482 | `					ph7_class_method *pMethod;` |
|        - |  8483 | `					/* Extract the target method */` |
|       10 |  8484 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8485 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8486 | `						/* Perform the lookup */` |
|       10 |  8487 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8488 | `						if( pMethod ){` |
|        - |  8489 | `							/* Method is callable */` |
|        5 |  8490 | `							res = 1;` |
|        2 |  8491 | `						}` |
|        4 |  8492 | `					}` |
|        4 |  8493 | `				}` |
|        5 |  8494 | `			}` |
|        7 |  8495 | `		}` |
|    17751 |  8496 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8497 | `		const char *zName;` |
|        - |  8498 | `		int nLen;` |
|        - |  8499 | `		/* Extract the name */` |
|     5030 |  8500 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8501 | `		/* Perform the lookup */` |
|     5045 |  8502 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8503 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8504 | `				/* Function is callable */` |
|     5012 |  8505 | `				res = 1;` |
|     2505 |  8506 | `		}` |
|     2514 |  8507 | `	}` |
|    17764 |  8508 | `	return res;` |
|        2 |  8509 |  |
|        - |  8510 | `/*` |
|        - |  8511 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8512 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8513 | ` * Parameters` |
|        - |  8514 | ` * $name` |
|        - |  8515 | ` *    The callback function to check` |
|        - |  8516 | ` * $syntax_only` |
|        - |  8517 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8518 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8519 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8520 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8521 | ` *    a string.` |
|        - |  8522 | ` * Return` |
|        - |  8523 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8524 | ` */` |
|       14 |  8525 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8526 |  |
|        - |  8527 | `	ph7_vm *pVm;` |
|        - |  8528 | `	int res;` |
|       15 |  8529 | `	if( nArg < 1 ){` |
|        - |  8530 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8531 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8532 | `		return SXRET_OK;` |
|        - |  8533 | `	}` |
|        - |  8534 | `	/* Point to the target VM */` |
|       15 |  8535 | `	pVm = pCtx->pVm;` |
|        - |  8536 | `	/* Perform the requested operation */` |
|       15 |  8537 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8538 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8539 | `	return SXRET_OK;` |
|        8 |  8540 |  |
|        - |  8541 | `/*` |
|        - |  8542 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8543 | ` * defined below.` |
|        - |  8544 | ` */` |
|     1200 |  8545 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8546 |  |
|     1201 |  8547 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8548 | `	ph7_value sName;` |
|        - |  8549 | `	sxi32 rc;` |
|        - |  8550 | `	/* Prepare the function name for insertion */` |
|     1201 |  8551 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  8552 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8553 | `	/* Perform the insertion */` |
|     1201 |  8554 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  8555 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  8556 | `	return rc;` |
|        1 |  8557 |  |
|        - |  8558 | `/*` |
|        - |  8559 | ` * array get_defined_functions(void)` |
|        - |  8560 | ` *  Returns an array of all defined functions.` |
|        - |  8561 | ` * Parameter` |
|        - |  8562 | ` *  None.` |
|        - |  8563 | ` * Return` |
|        - |  8564 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8565 | ` *  both built-in (internal) and user-defined.` |
|        - |  8566 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8567 | ` *  defined ones using $arr["user"].` |
|        - |  8568 | ` * Note:` |
|        - |  8569 | ` *  NULL is returned on failure.` |
|        - |  8570 | ` */` |
|        2 |  8571 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8572 |  |
|        - |  8573 | `	ph7_value *pArray,*pEntry;` |
|        - |  8574 | `	/* NOTE:` |
|        - |  8575 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8576 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8577 | `	 */` |
|        3 |  8578 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8579 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8580 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8581 | `		SXUNUSED(apArg);` |
|        - |  8582 | `		/* Return NULL */` |
|      ! 0 |  8583 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8584 | `		return SXRET_OK;` |
|        - |  8585 | `	}` |
|        3 |  8586 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8587 | `	if( pEntry == 0 ){` |
|        - |  8588 | `		/* Return NULL */` |
|      ! 0 |  8589 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8590 | `		return SXRET_OK;` |
|        - |  8591 | `	}` |
|        - |  8592 | `	/* Fill with the appropriate information */` |
|        3 |  8593 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8594 | `	/* Create the 'internal' index */` |
|        3 |  8595 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8596 | `	/* Create the user-func array */` |
|        3 |  8597 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8598 | `	if( pEntry == 0 ){` |
|        - |  8599 | `		/* Return NULL */` |
|      ! 0 |  8600 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8601 | `		return SXRET_OK;` |
|        - |  8602 | `	}` |
|        - |  8603 | `	/* Fill with the appropriate information */` |
|        3 |  8604 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8605 | `	/* Create the 'user' index */` |
|        3 |  8606 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8607 | `	/* Return the multi-dimensional array */` |
|        3 |  8608 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8609 | `	return SXRET_OK;` |
|        2 |  8610 |  |
|        - |  8611 | `/*` |
|        - |  8612 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8613 | ` *  Register a function for execution on shutdown.` |
|        - |  8614 | ` * Note` |
|        - |  8615 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8616 | ` *  be called in the same order as they were registered.` |
|        - |  8617 | ` * Parameters` |
|        - |  8618 | ` *  $callback` |
|        - |  8619 | ` *   The shutdown callback to register.` |
|        - |  8620 | ` * $param` |
|        - |  8621 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8622 | ` * Return` |
|        - |  8623 | ` *  Nothing.` |
|        - |  8624 | ` */` |
|        2 |  8625 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8626 |  |
|        - |  8627 | `	VmShutdownCB sEntry;` |
|        - |  8628 | `	int i,j;` |
|        3 |  8629 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8630 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8631 | `		return PH7_OK;` |
|        - |  8632 | `	}` |
|        - |  8633 | `	/* Zero the Entry */` |
|        3 |  8634 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8635 | `	/* Initialize fields */` |
|        3 |  8636 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8637 | `	/* Save the callback name for later invocation name */` |
|        3 |  8638 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8639 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8640 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8641 | `	}` |
|        - |  8642 | `	/* Copy arguments */` |
|        3 |  8643 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8644 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8645 | `			/* Limit reached */` |
|      ! 0 |  8646 | `			break;` |
|        - |  8647 | `		}` |
|      ! 0 |  8648 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8649 | `	}` |
|        3 |  8650 | `	sEntry.nArg = j;` |
|        - |  8651 | `	/* Install the callback */` |
|        3 |  8652 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8653 | `	return PH7_OK;` |
|        2 |  8654 |  |
|        - |  8655 | `/*` |
|        - |  8656 | ` * Section:` |
|        - |  8657 | ` *  Class handling functions.` |
|        - |  8658 | ` * Status:` |
|        - |  8659 | ` *    Stable.` |
|        - |  8660 | ` */` |
|        - |  8661 | `/*` |
|        - |  8662 | ` * Extract the top active class. NULL is returned` |
|        - |  8663 | ` * if the class stack is empty.` |
|        - |  8664 | ` */` |
|      574 |  8665 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8666 |  |
|      576 |  8667 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8668 | `	ph7_class **apClass;` |
|      576 |  8669 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8670 | `		/* Empty stack,return NULL */` |
|       15 |  8671 | `		return 0;` |
|        - |  8672 | `	}` |
|        - |  8673 | `	/* Peek the last entry */` |
|      562 |  8674 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      562 |  8675 | `	return apClass[pSet->nUsed - 1];` |
|      289 |  8676 |  |
|        - |  8677 | `/*` |
|        - |  8678 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8679 | ` *   Get the class that declared the currently executing method.` |
|        - |  8680 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8681 | ` *` |
|        - |  8682 | ` * Parameters` |
|        - |  8683 | ` *   pVm: Target VM` |
|        - |  8684 | ` *` |
|        - |  8685 | ` * Return` |
|        - |  8686 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8687 | ` *   - Not executing within a class method` |
|        - |  8688 | ` *` |
|        - |  8689 | ` * Note` |
|        - |  8690 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8691 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8692 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8693 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8694 | ` *   declaring class.` |
|        - |  8695 | ` */` |
|       60 |  8696 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8697 |  |
|       62 |  8698 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8699 | `	ph7_vm_func *pVmFunc;` |
|        - |  8700 |  |
|        - |  8701 | `	/* Skip exception frames to find the actual method frame */` |
|       62 |  8702 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8703 |  |
|        - |  8704 | `	/* Check if we're in a method context */` |
|       62 |  8705 | `	if( pFrame->pParent ){` |
|       58 |  8706 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       58 |  8707 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8708 | `			/* Return the declaring class */` |
|       58 |  8709 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8710 | `		}` |
|      ! 0 |  8711 | `	}` |
|        - |  8712 |  |
|        5 |  8713 | `	return 0;` |
|       32 |  8714 |  |
|        - |  8715 |  |
|        - |  8716 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8717 | `/*` |
|        - |  8718 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8719 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8720 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8721 | ` * return value indicates failure.` |
|        - |  8722 | ` */` |
|     1508 |  8723 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8724 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8725 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8726 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8727 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8728 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8729 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8730 | `	)` |
|        2 |  8731 |  |
|        - |  8732 | `	ph7_value *aStack;` |
|        - |  8733 | `	VmInstr aInstr[2];` |
|        - |  8734 | `	int iCursor;` |
|        - |  8735 | `	int i;` |
|        - |  8736 | `	/* Create a new operand stack */` |
|     1510 |  8737 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1510 |  8738 | `	if( aStack == 0 ){` |
|      ! 0 |  8739 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8740 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8741 | `		return SXERR_MEM;` |
|        - |  8742 | `	}` |
|        - |  8743 | `	/* Fill the operand stack with the given arguments */` |
|     2124 |  8744 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      616 |  8745 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8746 | `		/*` |
|        - |  8747 | `		 * Symisc eXtension:` |
|        - |  8748 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8749 | `		 */` |
|      616 |  8750 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      309 |  8751 | `	}` |
|     1510 |  8752 | `	iCursor = nArg + 1;` |
|     1510 |  8753 | `	if( pThis ){` |
|        - |  8754 | `		/*` |
|        - |  8755 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8756 | `		 */` |
|     1504 |  8757 | `		pThis->iRef++; /* Increment reference count */` |
|     1504 |  8758 | `		aStack[i].x.pOther = pThis;` |
|     1504 |  8759 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      751 |  8760 | `	}` |
|     1510 |  8761 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1510 |  8762 | `	i++;` |
|        - |  8763 | `	/* Push method name */` |
|     1510 |  8764 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1510 |  8765 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1510 |  8766 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1510 |  8767 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8768 | `	/* Emit the CALL istruction */` |
|     1510 |  8769 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1510 |  8770 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1510 |  8771 | `	aInstr[0].iP2 = 0;` |
|     1510 |  8772 | `	aInstr[0].p3  = 0;` |
|        - |  8773 | `	/* Emit the DONE instruction */` |
|     1510 |  8774 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1510 |  8775 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1510 |  8776 | `	aInstr[1].iP2 = 0;` |
|     1510 |  8777 | `	aInstr[1].p3  = 0;` |
|        - |  8778 | `	/* Execute the method body (if available) */` |
|     1510 |  8779 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8780 | `	/* Clean up the mess left behind */` |
|     1510 |  8781 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1510 |  8782 | `	return PH7_OK;` |
|      756 |  8783 |  |
|        - |  8784 | `/*` |
|        - |  8785 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8786 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8787 | ` * in the apArg[] array.` |
|        - |  8788 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8789 | ` * return value indicates failure.` |
|        - |  8790 | ` */` |
|      960 |  8791 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8792 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8793 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8794 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8795 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8796 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8797 | `	)` |
|        2 |  8798 |  |
|        - |  8799 | `	ph7_value *aStack;` |
|        - |  8800 | `	VmInstr aInstr[2];` |
|        - |  8801 | `	int i;` |
|      962 |  8802 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8803 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 |  8804 | `		if( pResult ){` |
|        - |  8805 | `			/* Assume a null return value */` |
|      ! 0 |  8806 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8807 | `		}` |
|      479 |  8808 | `		return SXERR_INVALID;` |
|        - |  8809 | `	}` |
|      484 |  8810 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8811 | `		/* Class method */` |
|       11 |  8812 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8813 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8814 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8815 | `		ph7_class *pClass = 0;` |
|        - |  8816 | `		ph7_value *pValue;` |
|        - |  8817 | `		sxi32 rc;` |
|       11 |  8818 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8819 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8820 | `			if( pResult ){` |
|        - |  8821 | `				/* Assume a null return value */` |
|      ! 0 |  8822 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8823 | `			}` |
|      ! 0 |  8824 | `			return SXRET_OK;` |
|        - |  8825 | `		}` |
|        - |  8826 | `		/* Extract the class name or an instance of it */` |
|       11 |  8827 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8828 | `		if( pValue ){` |
|       11 |  8829 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8830 | `		}` |
|       11 |  8831 | `		if( pClass == 0 ){` |
|        - |  8832 | `			/* No such class,return NULL */` |
|      ! 0 |  8833 | `			if( pResult ){` |
|      ! 0 |  8834 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8835 | `			}` |
|      ! 0 |  8836 | `			return SXRET_OK;` |
|        - |  8837 | `		}` |
|       11 |  8838 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8839 | `			/* Point to the class instance */` |
|        5 |  8840 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8841 | `		}` |
|        - |  8842 | `		/* Try to extract the method */` |
|       11 |  8843 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8844 | `		if( pValue ){` |
|       11 |  8845 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8846 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8847 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8848 | `			}` |
|        5 |  8849 | `		}` |
|       11 |  8850 | `		if( pMethod == 0 ){` |
|        - |  8851 | `			/* No such method,return NULL */` |
|      ! 0 |  8852 | `			if( pResult ){` |
|      ! 0 |  8853 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8854 | `			}` |
|      ! 0 |  8855 | `			return SXRET_OK;` |
|        - |  8856 | `		}` |
|        - |  8857 | `		/* Call the class method */` |
|       11 |  8858 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8859 | `		return rc;` |
|        - |  8860 | `	}` |
|        - |  8861 | `	/* Create a new operand stack */` |
|      474 |  8862 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      474 |  8863 | `	if( aStack == 0 ){` |
|      ! 0 |  8864 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8865 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8866 | `		if( pResult ){` |
|        - |  8867 | `			/* Assume a null return value */` |
|      ! 0 |  8868 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8869 | `		}` |
|      ! 0 |  8870 | `		return SXERR_MEM;` |
|        - |  8871 | `	}` |
|        - |  8872 | `	/* Fill the operand stack with the given arguments */` |
|     1522 |  8873 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1050 |  8874 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8875 | `		/*` |
|        - |  8876 | `		 * Symisc eXtension:` |
|        - |  8877 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8878 | `		 */` |
|     1050 |  8879 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      526 |  8880 | `	}` |
|        - |  8881 | `	/* Push the function name */` |
|      474 |  8882 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      474 |  8883 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8884 | `	/* Emit the CALL istruction */` |
|      474 |  8885 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      474 |  8886 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      474 |  8887 | `	aInstr[0].iP2 = 0;` |
|      474 |  8888 | `	aInstr[0].p3  = 0;` |
|        - |  8889 | `	/* Emit the DONE instruction */` |
|      474 |  8890 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      474 |  8891 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      474 |  8892 | `	aInstr[1].iP2 = 0;` |
|      474 |  8893 | `	aInstr[1].p3  = 0;` |
|        - |  8894 | `	/* Execute the function body (if available) */` |
|      474 |  8895 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8896 | `	/* Clean up the mess left behind */` |
|      474 |  8897 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      474 |  8898 | `	return PH7_OK;` |
|      482 |  8899 |  |
|        - |  8900 | `/*` |
|        - |  8901 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8902 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8903 | ` * parameter.` |
|        - |  8904 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8905 | ` * return value indicates failure.` |
|        - |  8906 | ` */` |
|      236 |  8907 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8908 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8909 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8910 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8911 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8912 | `	)` |
|        1 |  8913 |  |
|        - |  8914 | `	ph7_value *pArg;` |
|        - |  8915 | `	SySet aArg;` |
|        - |  8916 | `	va_list ap;` |
|        - |  8917 | `	sxi32 rc;` |
|      237 |  8918 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8919 | `	/* Copy arguments one after one */` |
|      237 |  8920 | `	va_start(ap,pResult);` |
|      393 |  8921 | `	for(;;){` |
|      787 |  8922 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8923 | `		if( pArg == 0 ){` |
|      237 |  8924 | `			break;` |
|        - |  8925 | `		}` |
|      551 |  8926 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8927 | `	}` |
|        - |  8928 | `	/* Call the core routine */` |
|      237 |  8929 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8930 | `	/* Cleanup */` |
|      237 |  8931 | `	SySetRelease(&aArg);` |
|      237 |  8932 | `	return rc;` |
|        1 |  8933 |  |
|        - |  8934 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8935 | `/*` |
|        - |  8936 | ` * bool defined(string $name)` |
|        - |  8937 | ` *  Checks whether a given named constant exists.` |
|        - |  8938 | ` * Parameter:` |
|        - |  8939 | ` *  Name of the desired constant.` |
|        - |  8940 | ` * Return` |
|        - |  8941 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8942 | ` */` |
|       14 |  8943 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8944 |  |
|        - |  8945 | `	const char *zName;` |
|       16 |  8946 | `	int nLen = 0;` |
|       16 |  8947 | `	int res = 0;` |
|       16 |  8948 | `	if( nArg < 1 ){` |
|        - |  8949 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8950 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8951 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8952 | `		return SXRET_OK;` |
|        - |  8953 | `	}` |
|        - |  8954 | `	/* Extract constant name */` |
|       16 |  8955 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8956 | `	/* Perform the lookup */` |
|       16 |  8957 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8958 | `		/* Already defined */` |
|       10 |  8959 | `		res = 1;` |
|        4 |  8960 | `	}` |
|       16 |  8961 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8962 | `	return SXRET_OK;` |
|        9 |  8963 |  |
|        - |  8964 | `/*` |
|        - |  8965 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8966 | ` * below.` |
|        - |  8967 | ` */` |
|       10 |  8968 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  8969 |  |
|       12 |  8970 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  8971 | `	/* Expand constant value */` |
|       12 |  8972 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 |  8973 |  |
|        - |  8974 | `/*` |
|        - |  8975 | ` * bool define(string $constant_name,expression value)` |
|        - |  8976 | ` *  Defines a named constant at runtime.` |
|        - |  8977 | ` * Parameter:` |
|        - |  8978 | ` *  $constant_name` |
|        - |  8979 | ` *   The name of the constant` |
|        - |  8980 | ` *  $value` |
|        - |  8981 | ` *   Constant value` |
|        - |  8982 | ` * Return:` |
|        - |  8983 | ` *   TRUE on success,FALSE on failure.` |
|        - |  8984 | ` */` |
|       12 |  8985 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8986 |  |
|        - |  8987 | `	const char *zName;  /* Constant name */` |
|        - |  8988 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 |  8989 | `	int nLen = 0;       /* Name length */` |
|        - |  8990 | `	sxi32 rc;` |
|       14 |  8991 | `	if( nArg < 2 ){` |
|        - |  8992 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  8993 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  8994 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8995 | `		return SXRET_OK;` |
|        - |  8996 | `	}` |
|       14 |  8997 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  8998 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  8999 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9000 | `		return SXRET_OK;` |
|        - |  9001 | `	}` |
|        - |  9002 | `	/* Extract constant name */` |
|       14 |  9003 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 |  9004 | `	if( nLen < 1 ){` |
|      ! 0 |  9005 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  9006 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9007 | `		return SXRET_OK;` |
|        - |  9008 | `	}` |
|        - |  9009 | `	/* Duplicate constant value */` |
|       14 |  9010 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 |  9011 | `	if( pValue == 0 ){` |
|      ! 0 |  9012 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9013 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9014 | `		return SXRET_OK;` |
|        - |  9015 | `	}` |
|        - |  9016 | `	/* Initialize the memory object */` |
|       14 |  9017 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  9018 | `	/* Register the constant */` |
|       14 |  9019 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 |  9020 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9021 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  9022 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9023 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9024 | `		return SXRET_OK;` |
|        - |  9025 | `	}` |
|        - |  9026 | `	/* Duplicate constant value */` |
|       14 |  9027 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 |  9028 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  9029 | `		/* Lower case the constant name */` |
|      ! 0 |  9030 | `		char *zCur = (char *)zName;` |
|      ! 0 |  9031 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  9032 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  9033 | `				/* UTF-8 stream */` |
|      ! 0 |  9034 | `				zCur++;` |
|      ! 0 |  9035 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  9036 | `					zCur++;` |
|      ! 0 |  9037 | `				}` |
|      ! 0 |  9038 | `				continue;` |
|        - |  9039 | `			}` |
|      ! 0 |  9040 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  9041 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  9042 | `				zCur[0] = (char)c;` |
|      ! 0 |  9043 | `			}` |
|      ! 0 |  9044 | `			zCur++;` |
|      ! 0 |  9045 | `		}` |
|        - |  9046 | `		/* Finally,register the constant */` |
|      ! 0 |  9047 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  9048 | `	}` |
|        - |  9049 | `	/* All done,return TRUE */` |
|       14 |  9050 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9051 | `	return SXRET_OK;` |
|        8 |  9052 |  |
|        - |  9053 | `/*` |
|        - |  9054 | ` * value constant(string $name)` |
|        - |  9055 | ` *  Returns the value of a constant` |
|        - |  9056 | ` * Parameter` |
|        - |  9057 | ` *  $name` |
|        - |  9058 | ` *    Name of the constant.` |
|        - |  9059 | ` * Return` |
|        - |  9060 | ` *  Constant value or NULL if not defined.` |
|        - |  9061 | ` */` |
|        8 |  9062 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9063 |  |
|        - |  9064 | `	SyHashEntry *pEntry;` |
|        - |  9065 | `	ph7_constant *pCons;` |
|        - |  9066 | `	const char *zName; /* Constant name */` |
|        - |  9067 | `	ph7_value sVal;    /* Constant value */` |
|        - |  9068 | `	int nLen;` |
|       10 |  9069 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9070 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  9071 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  9072 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9073 | `		return SXRET_OK;` |
|        - |  9074 | `	}` |
|        - |  9075 | `	/* Extract the constant name */` |
|       10 |  9076 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9077 | `	/* Perform the query */` |
|       10 |  9078 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  9079 | `	if( pEntry == 0 ){` |
|        3 |  9080 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  9081 | `		ph7_result_null(pCtx);` |
|        3 |  9082 | `		return SXRET_OK;` |
|        - |  9083 | `	}` |
|        8 |  9084 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  9085 | `	/* Point to the structure that describe the constant */` |
|        8 |  9086 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  9087 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  9088 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  9089 | `	/* Return that value */` |
|        8 |  9090 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  9091 | `	/* Cleanup */` |
|        8 |  9092 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  9093 | `	return SXRET_OK;` |
|        6 |  9094 |  |
|        - |  9095 | `/*` |
|        - |  9096 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9097 | ` * defined below.` |
|        - |  9098 | ` */` |
|      452 |  9099 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9100 |  |
|      453 |  9101 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9102 | `	ph7_value sName;` |
|        - |  9103 | `	sxi32 rc;` |
|        - |  9104 | `	/* Prepare the constant name for insertion */` |
|      453 |  9105 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 |  9106 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9107 | `	/* Perform the insertion */` |
|      453 |  9108 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 |  9109 | `	PH7_MemObjRelease(&sName);` |
|      453 |  9110 | `	return rc;` |
|        1 |  9111 |  |
|        - |  9112 | `/*` |
|        - |  9113 | ` * array get_defined_constants(void)` |
|        - |  9114 | ` *  Returns an associative array with the names of all defined` |
|        - |  9115 | ` *  constants.` |
|        - |  9116 | ` * Parameters` |
|        - |  9117 | ` *  NONE.` |
|        - |  9118 | ` * Returns` |
|        - |  9119 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9120 | ` */` |
|        2 |  9121 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9122 |  |
|        - |  9123 | `	ph7_value *pArray;` |
|        - |  9124 | `	/* Create the array first*/` |
|        3 |  9125 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9126 | `	if( pArray == 0 ){` |
|      ! 0 |  9127 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9128 | `		SXUNUSED(apArg);` |
|        - |  9129 | `		/* Return NULL */` |
|      ! 0 |  9130 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9131 | `		return SXRET_OK;` |
|        - |  9132 | `	}` |
|        - |  9133 | `	/* Fill the array with the defined constants */` |
|        3 |  9134 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9135 | `	/* Return the created array */` |
|        3 |  9136 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9137 | `	return SXRET_OK;` |
|        2 |  9138 |  |
|        - |  9139 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9140 | `/*` |
|        - |  9141 | ` * Section:` |
|        - |  9142 | ` *  Random numbers/string generators.` |
|        - |  9143 | ` * Status:` |
|        - |  9144 | ` *    Stable.` |
|        - |  9145 | ` */` |
|        - |  9146 | `/*` |
|        - |  9147 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9148 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9149 | ` * used by te SQLite3 library.` |
|        - |  9150 | ` */` |
|     2411 |  9151 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9152 |  |
|        - |  9153 | `	sxu32 iNum;` |
|     2413 |  9154 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2413 |  9155 | `	return iNum;` |
|        2 |  9156 |  |
|        - |  9157 | `/*` |
|        - |  9158 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9159 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9160 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9161 | ` * by te SQLite3 library.` |
|        - |  9162 | ` */` |
|   125494 |  9163 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9164 |  |
|        - |  9165 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9166 | `	int i;` |
|        - |  9167 | `	/* Generate a binary string first */` |
|   125496 |  9168 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9169 | `	/* Turn the binary string into english based alphabet */` |
|  1380604 |  9170 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1255110 |  9171 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   627556 |  9172 | `	 }` |
|   125496 |  9173 |  |
|        - |  9174 | `/*` |
|        - |  9175 | ` * int rand()` |
|        - |  9176 | ` * int mt_rand()` |
|        - |  9177 | ` * int rand(int $min,int $max)` |
|        - |  9178 | ` * int mt_rand(int $min,int $max)` |
|        - |  9179 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9180 | ` * Parameter` |
|        - |  9181 | ` *  $min` |
|        - |  9182 | ` *    The lowest value to return (default: 0)` |
|        - |  9183 | ` *  $max` |
|        - |  9184 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9185 | ` * Return` |
|        - |  9186 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9187 | ` * Note:` |
|        - |  9188 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9189 | ` *  by te SQLite3 library.` |
|        - |  9190 | ` */` |
|       20 |  9191 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9192 |  |
|        - |  9193 | `	sxu32 iNum;` |
|        - |  9194 | `	/* Generate the random number */` |
|       21 |  9195 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9196 | `	if( nArg > 1 ){` |
|        - |  9197 | `		sxu32 iMin,iMax;` |
|        3 |  9198 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9199 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9200 | `		if( iMin < iMax ){` |
|        3 |  9201 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9202 | `			if( iDiv > 0 ){` |
|        3 |  9203 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9204 | `			}` |
|        1 |  9205 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9206 | `			iNum %= iMax;` |
|      ! 0 |  9207 | `		}` |
|        1 |  9208 | `	}` |
|        - |  9209 | `	/* Return the number */` |
|       21 |  9210 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9211 | `	return SXRET_OK;` |
|        1 |  9212 |  |
|        - |  9213 | `/*` |
|        - |  9214 | ` * int getrandmax(void)` |
|        - |  9215 | ` * int mt_getrandmax(void)` |
|        - |  9216 | ` * int rc4_getrandmax(void)` |
|        - |  9217 | ` *   Show largest possible random value` |
|        - |  9218 | ` * Return` |
|        - |  9219 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9220 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9221 | ` * Note:` |
|        - |  9222 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9223 | ` *  by te SQLite3 library.` |
|        - |  9224 | ` */` |
|        4 |  9225 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9226 |  |
|        2 |  9227 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9228 | `	SXUNUSED(apArg);` |
|        5 |  9229 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9230 | `	return SXRET_OK;` |
|        1 |  9231 |  |
|        - |  9232 | `/*` |
|        - |  9233 | ` * string rand_str()` |
|        - |  9234 | ` * string rand_str(int $len)` |
|        - |  9235 | ` *  Generate a random string (English alphabet).` |
|        - |  9236 | ` * Parameter` |
|        - |  9237 | ` *  $len` |
|        - |  9238 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9239 | ` * Return` |
|        - |  9240 | ` *   A pseudo random string.` |
|        - |  9241 | ` * Note:` |
|        - |  9242 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9243 | ` *  by te SQLite3 library.` |
|        - |  9244 | ` *  This function is a symisc extension.` |
|        - |  9245 | ` */` |
|      120 |  9246 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9247 |  |
|        - |  9248 | `	char zString[1024];` |
|      122 |  9249 | `	int iLen = 0x10;` |
|      122 |  9250 | `	if( nArg > 0 ){` |
|        - |  9251 | `		/* Get the desired length */` |
|      122 |  9252 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9253 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9254 | `			/* Default length */` |
|        3 |  9255 | `			iLen = 0x10;` |
|        1 |  9256 | `		}` |
|       60 |  9257 | `	}` |
|        - |  9258 | `	/* Generate the random string */` |
|      122 |  9259 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9260 | `	/* Return the generated string */` |
|      122 |  9261 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9262 | `	return SXRET_OK;` |
|        2 |  9263 |  |
|        - |  9264 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9265 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9266 | `/* Unique ID private data */` |
|        - |  9267 | `struct unique_id_data` |
|        - |  9268 |  |
|        - |  9269 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9270 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9271 | `};` |
|        - |  9272 | `/*` |
|        - |  9273 | ` * Binary to hex consumer callback.` |
|        - |  9274 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9275 | ` * defined below.` |
|        - |  9276 | ` */` |
|      192 |  9277 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9278 |  |
|      193 |  9279 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9280 | `	sxu32 nBuflen;` |
|        - |  9281 | `	/* Extract result buffer length */` |
|      193 |  9282 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9283 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9284 | `			/*` |
|        - |  9285 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9286 | `			 * string will be 13 characters long` |
|        - |  9287 | `			 */` |
|       25 |  9288 | `		return SXERR_ABORT;` |
|        - |  9289 | `	}` |
|      169 |  9290 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9291 | `		return SXERR_ABORT;` |
|        - |  9292 | `	}` |
|        - |  9293 | `	/* Safely Consume the hex stream */` |
|      169 |  9294 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9295 | `	return SXRET_OK;` |
|       97 |  9296 |  |
|        - |  9297 | `/*` |
|        - |  9298 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9299 | ` *  Generate a unique ID` |
|        - |  9300 | ` * Parameter` |
|        - |  9301 | ` * $prefix` |
|        - |  9302 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9303 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9304 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9305 | ` * $more_entropy` |
|        - |  9306 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9307 | ` *  that the result will be unique.` |
|        - |  9308 | ` * Return` |
|        - |  9309 | ` *  Returns the unique identifier, as a string.` |
|        - |  9310 | ` */` |
|       24 |  9311 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9312 |  |
|        - |  9313 | `	struct unique_id_data sUniq;` |
|        - |  9314 | `	unsigned char zDigest[20];` |
|       25 |  9315 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9316 | `	const char *zPrefix;` |
|        - |  9317 | `	SHA1Context sCtx;` |
|        - |  9318 | `	char zRandom[7];` |
|        - |  9319 | `	int nPrefix;` |
|        - |  9320 | `	int entropy;` |
|        - |  9321 | `	/* Generate a random string first */` |
|       25 |  9322 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9323 | `	/* Initialize fields */` |
|       25 |  9324 | `	zPrefix = 0;` |
|       25 |  9325 | `	nPrefix = 0;` |
|       25 |  9326 | `	entropy = 0;` |
|       25 |  9327 | `	if( nArg > 0 ){` |
|        - |  9328 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9329 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9330 | `		if( nArg > 1 ){` |
|      ! 0 |  9331 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9332 | `		}` |
|      ! 0 |  9333 | `	}` |
|       25 |  9334 | `	SHA1Init(&sCtx);` |
|        - |  9335 | `	/* Generate the random ID */` |
|       25 |  9336 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9337 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9338 | `	}` |
|        - |  9339 | `	/* Append the random ID */` |
|       25 |  9340 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9341 | `	/* Append the random string */` |
|       25 |  9342 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9343 | `	/* Increment the number */` |
|       25 |  9344 | `	pVm->unique_id++;` |
|       25 |  9345 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9346 | `	/* Hexify the digest */` |
|       25 |  9347 | `	sUniq.pCtx = pCtx;` |
|       25 |  9348 | `	sUniq.entropy = entropy;` |
|       25 |  9349 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9350 | `	/* All done */` |
|       25 |  9351 | `	return PH7_OK;` |
|        1 |  9352 |  |
|        - |  9353 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9354 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9355 | `/*` |
|        - |  9356 | ` * Section:` |
|        - |  9357 | ` *  Language construct implementation as foreign functions.` |
|        - |  9358 | ` * Status:` |
|        - |  9359 | ` *    Stable.` |
|        - |  9360 | ` */` |
|        - |  9361 | `/*` |
|        - |  9362 | ` * void echo($string...)` |
|        - |  9363 | ` *  Output one or more messages.` |
|        - |  9364 | ` * Parameters` |
|        - |  9365 | ` *  $string` |
|        - |  9366 | ` *   Message to output.` |
|        - |  9367 | ` * Return` |
|        - |  9368 | ` *  NULL.` |
|        - |  9369 | ` */` |
|      ! 0 |  9370 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9371 |  |
|        - |  9372 | `	const char *zData;` |
|      ! 0 |  9373 | `	int nDataLen = 0;` |
|        - |  9374 | `	ph7_vm *pVm;` |
|        - |  9375 | `	int i,rc;` |
|        - |  9376 | `	/* Point to the target VM */` |
|      ! 0 |  9377 | `	pVm = pCtx->pVm;` |
|        - |  9378 | `	/* Output */` |
|      ! 0 |  9379 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9380 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9381 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9382 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9383 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9384 | `			if( rc == SXERR_ABORT ){` |
|        - |  9385 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9386 | `				return PH7_ABORT;` |
|        - |  9387 | `			}` |
|      ! 0 |  9388 | `		}` |
|      ! 0 |  9389 | `	}` |
|      ! 0 |  9390 | `	return SXRET_OK;` |
|      ! 0 |  9391 |  |
|        - |  9392 | `/*` |
|        - |  9393 | ` * int print($string...)` |
|        - |  9394 | ` *  Output one or more messages.` |
|        - |  9395 | ` * Parameters` |
|        - |  9396 | ` *  $string` |
|        - |  9397 | ` *   Message to output.` |
|        - |  9398 | ` * Return` |
|        - |  9399 | ` *  1 always.` |
|        - |  9400 | ` */` |
|        2 |  9401 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9402 |  |
|        - |  9403 | `	const char *zData;` |
|        3 |  9404 | `	int nDataLen = 0;` |
|        - |  9405 | `	ph7_vm *pVm;` |
|        - |  9406 | `	int i,rc;` |
|        - |  9407 | `	/* Point to the target VM */` |
|        3 |  9408 | `	pVm = pCtx->pVm;` |
|        - |  9409 | `	/* Output */` |
|        5 |  9410 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9411 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9412 | `		if( nDataLen > 0 ){` |
|        3 |  9413 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9414 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9415 | `			if( rc == SXERR_ABORT ){` |
|        - |  9416 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9417 | `				return PH7_ABORT;` |
|        - |  9418 | `			}` |
|        1 |  9419 | `		}` |
|        2 |  9420 | `	}` |
|        - |  9421 | `	/* Return 1 */` |
|        3 |  9422 | `	ph7_result_int(pCtx,1);` |
|        3 |  9423 | `	return SXRET_OK;` |
|        2 |  9424 |  |
|        - |  9425 | `/*` |
|        - |  9426 | ` * void exit(string $msg)` |
|        - |  9427 | ` * void exit(int $status)` |
|        - |  9428 | ` * void die(string $ms)` |
|        - |  9429 | ` * void die(int $status)` |
|        - |  9430 | ` *   Output a message and terminate program execution.` |
|        - |  9431 | ` * Parameter` |
|        - |  9432 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9433 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9434 | ` *  and not printed` |
|        - |  9435 | ` * Return` |
|        - |  9436 | ` *  NULL` |
|        - |  9437 | ` */` |
|      ! 0 |  9438 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9439 |  |
|      ! 0 |  9440 | `	if( nArg > 0 ){` |
|      ! 0 |  9441 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9442 | `			const char *zData;` |
|      ! 0 |  9443 | `			int iLen = 0;` |
|        - |  9444 | `			/* Print exit message */` |
|      ! 0 |  9445 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9446 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9447 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9448 | `			sxi32 iExitStatus;` |
|        - |  9449 | `			/* Record exit status code */` |
|      ! 0 |  9450 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9451 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9452 | `		}` |
|      ! 0 |  9453 | `	}` |
|        - |  9454 | `	/* Check if we are in an included file */` |
|      ! 0 |  9455 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9456 | `		/* Exit the entire process */` |
|      ! 0 |  9457 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9458 | `	}` |
|        - |  9459 | `	/* Abort processing immediately */` |
|      ! 0 |  9460 | `	return PH7_ABORT;` |
|      ! 0 |  9461 |  |
|        - |  9462 | `/*` |
|        - |  9463 | ` * bool isset($var,...)` |
|        - |  9464 | ` *  Finds out whether a variable is set.` |
|        - |  9465 | ` * Parameters` |
|        - |  9466 | ` *  One or more variable to check.` |
|        - |  9467 | ` * Return` |
|        - |  9468 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9469 | ` */` |
|    76128 |  9470 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9471 |  |
|        - |  9472 | `	ph7_value *pObj;` |
|    76130 |  9473 | `	int res = 0;` |
|        - |  9474 | `	int i;` |
|    76130 |  9475 | `	if( nArg < 1 ){` |
|        - |  9476 | `		/* Missing arguments,return false */` |
|      ! 0 |  9477 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9478 | `		return SXRET_OK;` |
|        - |  9479 | `	}` |
|        - |  9480 | `	/* Iterate over available arguments */` |
|   100268 |  9481 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    76130 |  9482 | `		pObj = apArg[i];` |
|    76130 |  9483 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    51464 |  9484 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9485 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9486 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9487 | `			}` |
|    25731 |  9488 | `		}` |
|    76130 |  9489 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    76130 |  9490 | `		if( !res ){` |
|        - |  9491 | `			/* Variable not set,return FALSE */` |
|    51992 |  9492 | `			ph7_result_bool(pCtx,0);` |
|    51992 |  9493 | `			return SXRET_OK;` |
|        - |  9494 | `		}` |
|    12071 |  9495 | `	}` |
|        - |  9496 | `	/* All given variable are set,return TRUE */` |
|    24140 |  9497 | `	ph7_result_bool(pCtx,1);` |
|    24140 |  9498 | `	return SXRET_OK;` |
|    38066 |  9499 |  |
|        - |  9500 | `/*` |
|        - |  9501 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9502 | ` * frame,the reference table and discard it's contents.` |
|        - |  9503 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9504 | ` */` |
|  3034786 |  9505 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9506 |  |
|        - |  9507 | `	ph7_value *pObj;` |
|        - |  9508 | `	VmRefObj *pRef;` |
|  3034788 |  9509 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3034788 |  9510 | `	if( pObj ){` |
|        - |  9511 | `		/* Release the object */` |
|  3034788 |  9512 | `		PH7_MemObjRelease(pObj);` |
|  1517393 |  9513 | `	}` |
|        - |  9514 | `	/* Remove old reference links */` |
|  3034788 |  9515 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3034788 |  9516 | `	if( pRef ){` |
|  3034782 |  9517 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9518 | `		/* Unlink from the reference table */` |
|  3034782 |  9519 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3034782 |  9520 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9521 | `			VmSlot sFree;` |
|        - |  9522 | `			/* Restore to the free list */` |
|  3034776 |  9523 | `			sFree.nIdx = nObjIdx;` |
|  3034776 |  9524 | `			sFree.pUserData = 0;` |
|  3034776 |  9525 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1517387 |  9526 | `		}` |
|  1517390 |  9527 | `	}` |
|  3034788 |  9528 | `	return SXRET_OK;` |
|        2 |  9529 |  |
|        - |  9530 | `/*` |
|        - |  9531 | ` * void unset($var,...)` |
|        - |  9532 | ` *   Unset one or more given variable.` |
|        - |  9533 | ` * Parameters` |
|        - |  9534 | ` *  One or more variable to unset.` |
|        - |  9535 | ` * Return` |
|        - |  9536 | ` *  Nothing.` |
|        - |  9537 | ` */` |
|     6830 |  9538 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9539 |  |
|        - |  9540 | `	ph7_value *pObj;` |
|        - |  9541 | `	ph7_vm *pVm;` |
|        - |  9542 | `	int i;` |
|        - |  9543 | `	/* Point to the target VM */` |
|     6832 |  9544 | `	pVm = pCtx->pVm;` |
|        - |  9545 | `	/* Iterate and unset */` |
|    13662 |  9546 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6832 |  9547 | `		pObj = apArg[i];` |
|     6832 |  9548 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9549 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9550 | `				/* Throw an error */` |
|      ! 0 |  9551 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9552 | `			}` |
|      ! 0 |  9553 | `		}else{` |
|     6832 |  9554 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9555 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6832 |  9556 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6826 |  9557 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3412 |  9558 | `			}` |
|        - |  9559 | `		}` |
|     3417 |  9560 | `	}` |
|     6832 |  9561 | `	return SXRET_OK;` |
|        2 |  9562 |  |
|        - |  9563 | `/*` |
|        - |  9564 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9565 | ` */` |
|      110 |  9566 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9567 |  |
|      111 |  9568 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9569 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9570 | `	ph7_value *pObj;` |
|        - |  9571 | `	sxu32 nIdx;` |
|        - |  9572 | `	/* Extract the memory object */` |
|      111 |  9573 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9574 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9575 | `	if( pObj ){` |
|      111 |  9576 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9577 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9578 | `				SyString sName;` |
|        - |  9579 | `				ph7_value sKey;` |
|        - |  9580 | `				/* Perform the insertion */` |
|      109 |  9581 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9582 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9583 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9584 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9585 | `			}` |
|       54 |  9586 | `		}` |
|       55 |  9587 | `	}` |
|      111 |  9588 | `	return SXRET_OK;` |
|        1 |  9589 |  |
|        - |  9590 | `/*` |
|        - |  9591 | ` * array get_defined_vars(void)` |
|        - |  9592 | ` *  Returns an array of all defined variables.` |
|        - |  9593 | ` * Parameter` |
|        - |  9594 | ` *  None` |
|        - |  9595 | ` * Return` |
|        - |  9596 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9597 | ` */` |
|        2 |  9598 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9599 |  |
|        3 |  9600 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9601 | `	ph7_value *pArray;` |
|        - |  9602 | `	/* Create a new array */` |
|        3 |  9603 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9604 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9605 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9606 | `		SXUNUSED(apArg);` |
|        - |  9607 | `		/* Return NULL */` |
|      ! 0 |  9608 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9609 | `		return SXRET_OK;` |
|        - |  9610 | `	}` |
|        - |  9611 | `	/* Superglobals first */` |
|        3 |  9612 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9613 | `	/* Then variable defined in the current frame */` |
|        3 |  9614 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9615 | `	/* Finally,return the created array */` |
|        3 |  9616 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9617 | `	return SXRET_OK;` |
|        2 |  9618 |  |
|        - |  9619 | `/*` |
|        - |  9620 | ` * bool gettype($var)` |
|        - |  9621 | ` *  Get the type of a variable` |
|        - |  9622 | ` * Parameters` |
|        - |  9623 | ` *   $var` |
|        - |  9624 | ` *    The variable being type checked.` |
|        - |  9625 | ` * Return` |
|        - |  9626 | ` *   String representation of the given variable type.` |
|        - |  9627 | ` */` |
|       32 |  9628 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9629 |  |
|       34 |  9630 | `	const char *zType = "Empty";` |
|       34 |  9631 | `	if( nArg > 0 ){` |
|       34 |  9632 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9633 | `	}` |
|        - |  9634 | `	/* Return the variable type */` |
|       34 |  9635 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9636 | `	return SXRET_OK;` |
|        2 |  9637 |  |
|        - |  9638 | `/*` |
|        - |  9639 | ` * string get_resource_type(resource $handle)` |
|        - |  9640 | ` *  This function gets the type of the given resource.` |
|        - |  9641 | ` * Parameters` |
|        - |  9642 | ` *  $handle` |
|        - |  9643 | ` *  The evaluated resource handle.` |
|        - |  9644 | ` * Return` |
|        - |  9645 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9646 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9647 | ` *  the return value will be the string Unknown.` |
|        - |  9648 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9649 | ` *  is not a resource.` |
|        - |  9650 | ` */` |
|        2 |  9651 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9652 |  |
|        3 |  9653 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9654 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9655 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9656 | `		return PH7_OK;` |
|        - |  9657 | `	}` |
|        3 |  9658 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9659 | `	return SXRET_OK;` |
|        2 |  9660 |  |
|        - |  9661 | `/*` |
|        - |  9662 | ` * void var_dump(expression,....)` |
|        - |  9663 | ` *   var_dump � Dumps information about a variable` |
|        - |  9664 | ` * Parameters` |
|        - |  9665 | ` *   One or more expression to dump.` |
|        - |  9666 | ` * Returns` |
|        - |  9667 | ` *  Nothing.` |
|        - |  9668 | ` */` |
|      218 |  9669 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9670 |  |
|        - |  9671 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9672 | `	int i;` |
|      220 |  9673 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9674 | `	/* Dump one or more expressions */` |
|      444 |  9675 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9676 | `		ph7_value *pObj = apArg[i];` |
|        - |  9677 | `		/* Reset the working buffer */` |
|      226 |  9678 | `		SyBlobReset(&sDump);` |
|        - |  9679 | `		/* Dump the given expression */` |
|      226 |  9680 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9681 | `		/* Output */` |
|      226 |  9682 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9683 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9684 | `		}` |
|      114 |  9685 | `	}` |
|        - |  9686 | `	/* Release the working buffer */` |
|      220 |  9687 | `	SyBlobRelease(&sDump);` |
|      220 |  9688 | `	return SXRET_OK;` |
|        2 |  9689 |  |
|        - |  9690 | `/*` |
|        - |  9691 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9692 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9693 | ` * Parameters` |
|        - |  9694 | ` *   expression: Expression to dump` |
|        - |  9695 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9696 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9697 | ` *            print_r() will return the information rather than print it.` |
|        - |  9698 | ` * Return` |
|        - |  9699 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9700 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9701 | ` */` |
|       16 |  9702 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9703 |  |
|       17 |  9704 | `	int ret_string = 0;` |
|        - |  9705 | `	SyBlob sDump;` |
|       17 |  9706 | `	if( nArg < 1 ){` |
|        - |  9707 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9708 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9709 | `		return SXRET_OK;` |
|        - |  9710 | `	}` |
|       17 |  9711 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9712 | `	if ( nArg > 1 ){` |
|        - |  9713 | `		/* Where to redirect output */` |
|       11 |  9714 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9715 | `	}` |
|        - |  9716 | `	/* Generate dump */` |
|       17 |  9717 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9718 | `	if( !ret_string ){` |
|        - |  9719 | `		/* Output dump */` |
|        7 |  9720 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9721 | `		/* Return true */` |
|        7 |  9722 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9723 | `	}else{` |
|        - |  9724 | `		/* Generated dump as return value */` |
|       11 |  9725 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9726 | `	}` |
|        - |  9727 | `	/* Release the working buffer */` |
|       17 |  9728 | `	SyBlobRelease(&sDump);` |
|       17 |  9729 | `	return SXRET_OK;` |
|        9 |  9730 |  |
|        - |  9731 | `/*` |
|        - |  9732 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9733 | ` * Same job as print_r. (see coment above)` |
|        - |  9734 | ` */` |
|        2 |  9735 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9736 |  |
|        3 |  9737 | `	int ret_string = 0;` |
|        - |  9738 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9739 | `	if( nArg < 1 ){` |
|        - |  9740 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9741 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9742 | `		return SXRET_OK;` |
|        - |  9743 | `	}` |
|        3 |  9744 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9745 | `	if ( nArg > 1 ){` |
|        - |  9746 | `		/* Where to redirect output */` |
|        3 |  9747 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9748 | `	}` |
|        - |  9749 | `	/* Generate dump */` |
|        3 |  9750 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9751 | `	if( !ret_string ){` |
|        - |  9752 | `		/* Output dump */` |
|      ! 0 |  9753 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9754 | `		/* Return NULL */` |
|      ! 0 |  9755 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9756 | `	}else{` |
|        - |  9757 | `		/* Generated dump as return value */` |
|        3 |  9758 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9759 | `	}` |
|        - |  9760 | `	/* Release the working buffer */` |
|        3 |  9761 | `	SyBlobRelease(&sDump);` |
|        3 |  9762 | `	return SXRET_OK;` |
|        2 |  9763 |  |
|        - |  9764 | `/*` |
|        - |  9765 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9766 | ` *  Set/get the various assert flags.` |
|        - |  9767 | ` * Parameter` |
|        - |  9768 | ` * $what` |
|        - |  9769 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9770 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9771 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9772 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9773 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9774 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9775 | ` * $value` |
|        - |  9776 | ` *   An optional new value for the option.` |
|        - |  9777 | ` * Return` |
|        - |  9778 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9779 | ` */` |
|       28 |  9780 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9781 |  |
|       30 |  9782 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9783 | `	int iOption;` |
|        - |  9784 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 |  9785 | `	if( nArg < 1 ){` |
|        3 |  9786 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9787 | `			"ArgumentCountError",` |
|        - |  9788 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9789 | `			);` |
|        - |  9790 | `	}` |
|        - |  9791 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 |  9792 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 |  9793 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9794 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9795 | `			"TypeError",` |
|        - |  9796 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9797 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9798 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9799 | `			);` |
|        - |  9800 | `	}` |
|       28 |  9801 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9802 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9803 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9804 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 |  9805 | `	switch( iOption ){` |
|        5 |  9806 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9807 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 |  9808 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 |  9809 | `		if( nArg > 1 ){` |
|        5 |  9810 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9811 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9812 | `			}else{` |
|        3 |  9813 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9814 | `			}` |
|        2 |  9815 | `		}` |
|       12 |  9816 | `		break;` |
|        1 |  9817 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9818 | `		/* Return old callback or null */` |
|        3 |  9819 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9820 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9821 | `		}else{` |
|        3 |  9822 | `			ph7_result_null(pCtx);` |
|        - |  9823 | `		}` |
|        3 |  9824 | `		if( nArg > 1 ){` |
|      ! 0 |  9825 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9826 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9827 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9828 | `			}else{` |
|      ! 0 |  9829 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9830 | `			}` |
|      ! 0 |  9831 | `		}` |
|        3 |  9832 | `		break;` |
|        5 |  9833 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9834 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9835 | `		if( nArg > 1 ){` |
|        5 |  9836 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9837 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9838 | `			}else{` |
|        3 |  9839 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9840 | `			}` |
|        2 |  9841 | `		}` |
|       11 |  9842 | `		break;` |
|      ! 0 |  9843 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9844 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9845 | `		break;` |
|        1 |  9846 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9847 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9848 | `		break;` |
|      ! 0 |  9849 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9850 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9851 | `		break;` |
|        1 |  9852 | `	default:` |
|        - |  9853 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9854 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9855 | `			"ValueError",` |
|        - |  9856 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9857 | `			);` |
|        - |  9858 | `	}` |
|       26 |  9859 | `	return PH7_OK;` |
|       16 |  9860 |  |
|        - |  9861 | `/*` |
|        - |  9862 | ` * bool assert(mixed $assertion)` |
|        - |  9863 | ` *  Checks if assertion is FALSE.` |
|        - |  9864 | ` * Parameter` |
|        - |  9865 | ` *  $assertion` |
|        - |  9866 | ` *    The assertion to test.` |
|        - |  9867 | ` * Return` |
|        - |  9868 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9869 | ` */` |
|       24 |  9870 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9871 |  |
|       26 |  9872 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9873 | `	int iFlags,iResult;` |
|        - |  9874 | `	const char *zDesc;` |
|        - |  9875 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 |  9876 | `	if( nArg < 1 ){` |
|        3 |  9877 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9878 | `			"ArgumentCountError",` |
|        - |  9879 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9880 | `			);` |
|        - |  9881 | `	}` |
|       24 |  9882 | `	iFlags = pVm->iAssertFlags;` |
|       24 |  9883 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9884 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9885 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9886 | `		return PH7_OK;` |
|        - |  9887 | `	}` |
|        - |  9888 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 |  9889 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 |  9890 | `	if( !iResult ){` |
|        - |  9891 | `		/* Assertion failed */` |
|        - |  9892 | `		/* Extract optional description */` |
|       13 |  9893 | `		zDesc = 0;` |
|       13 |  9894 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9895 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9896 | `		}` |
|       13 |  9897 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9898 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9899 | `			ph7_value sFile,sLine;` |
|        - |  9900 | `			ph7_value *apCbArg[3];` |
|        - |  9901 | `			SyString *pFile;` |
|        - |  9902 | `			/* Extract the processed script */` |
|      ! 0 |  9903 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9904 | `			if( pFile == 0 ){` |
|      ! 0 |  9905 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9906 | `			}` |
|        - |  9907 | `			/* Invoke the callback */` |
|      ! 0 |  9908 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9909 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9910 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9911 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9912 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9913 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9914 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9915 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9916 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9917 | `		}` |
|       13 |  9918 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9919 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9920 | `			return PH7_ABORT;` |
|        - |  9921 | `		}` |
|        - |  9922 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9923 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9924 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9925 | `				"AssertionError",` |
|        - |  9926 | `				"%s",` |
|        1 |  9927 | `				zDesc` |
|        - |  9928 | `				);` |
|      ! 0 |  9929 | `		}else{` |
|       11 |  9930 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9931 | `				"AssertionError",` |
|        - |  9932 | `				"assert(false)"` |
|        - |  9933 | `				);` |
|        - |  9934 | `		}` |
|        - |  9935 | `	}` |
|        - |  9936 | `	/* Assertion passed */` |
|       11 |  9937 | `	ph7_result_bool(pCtx,1);` |
|       11 |  9938 | `	return PH7_OK;` |
|       14 |  9939 |  |
|        - |  9940 | `/*` |
|        - |  9941 | ` * Section:` |
|        - |  9942 | ` *  Error reporting functions.` |
|        - |  9943 | ` * Status:` |
|        - |  9944 | ` *    Stable.` |
|        - |  9945 | ` */` |
|        - |  9946 | `/*` |
|        - |  9947 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9948 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9949 | ` * Parameters` |
|        - |  9950 | ` *  $error_msg` |
|        - |  9951 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9952 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9953 | ` * $error_type` |
|        - |  9954 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9955 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9956 | ` * Return` |
|        - |  9957 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9958 | ` */` |
|       12 |  9959 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9960 |  |
|       14 |  9961 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9962 | `	int rc = PH7_OK;` |
|       14 |  9963 | `	if( nArg > 0 ){` |
|        - |  9964 | `		const char *zErr;` |
|        - |  9965 | `		int nLen;` |
|        - |  9966 | `		/* Extract the error message */` |
|       12 |  9967 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9968 | `		if( nArg > 1 ){` |
|        - |  9969 | `			/* Extract the error type */` |
|       12 |  9970 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  9971 | `			switch( nErr ){` |
|        1 |  9972 | `			case 1:   /* E_ERROR */` |
|        - |  9973 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  9974 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  9975 | `			case 256: /* E_USER_ERROR */` |
|        3 |  9976 | `				nErr = PH7_CTX_ERR;` |
|        3 |  9977 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  9978 | `				break;` |
|        1 |  9979 | `			case 2:   /* E_WARNING */` |
|        - |  9980 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  9981 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  9982 | `			case 512: /* E_USER_WARNING */` |
|        3 |  9983 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  9984 | `				break;` |
|        3 |  9985 | `			default:` |
|        8 |  9986 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  9987 | `				break;` |
|        - |  9988 | `			}` |
|        5 |  9989 | `		}` |
|        - |  9990 | `		/* Report error */` |
|       12 |  9991 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  9992 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  9993 | `			return rc;` |
|        - |  9994 | `		}` |
|        - |  9995 | `		/* Return true */` |
|       12 |  9996 | `		ph7_result_bool(pCtx,1);` |
|        7 |  9997 | `	}else{` |
|        - |  9998 | `		/* Missing arguments,return FALSE */` |
|        3 |  9999 | `		ph7_result_bool(pCtx,0);` |
|        - | 10000 | `	}` |
|       14 | 10001 | `	return rc;` |
|        8 | 10002 |  |
|        - | 10003 | `/*` |
|        - | 10004 | ` * int error_reporting([int $level])` |
|        - | 10005 | ` *  Sets which PHP errors are reported.` |
|        - | 10006 | ` * Parameters` |
|        - | 10007 | ` *  $level` |
|        - | 10008 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 10009 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 10010 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 10011 | ` *   levels will not always behave as expected.` |
|        - | 10012 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 10013 | ` *   in the predefined constants.` |
|        - | 10014 | ` * Return` |
|        - | 10015 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 10016 | ` *   parameter is given.` |
|        - | 10017 | ` */` |
|       38 | 10018 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10019 |  |
|       40 | 10020 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10021 | `	int nOld;` |
|        - | 10022 | `	/* Extract the old reporting level */` |
|       40 | 10023 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 10024 | `	if( nArg > 0 ){` |
|        - | 10025 | `		int nNew;` |
|        - | 10026 | `		/* Extract the desired error reporting level */` |
|       32 | 10027 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 10028 | `		if( !nNew ){` |
|        - | 10029 | `			/* Do not report errors at all */` |
|        5 | 10030 | `			pVm->bErrReport = 0;` |
|        3 | 10031 | `		}else{` |
|        - | 10032 | `			/* Report all errors */` |
|       28 | 10033 | `			pVm->bErrReport = 1;` |
|        - | 10034 | `		}` |
|       15 | 10035 | `	}` |
|        - | 10036 | `	/* Return the old level */` |
|       40 | 10037 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 10038 | `	return PH7_OK;` |
|        2 | 10039 |  |
|        - | 10040 | `/*` |
|        - | 10041 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 10042 | ` *  Send an error message somewhere.` |
|        - | 10043 | ` * Parameter` |
|        - | 10044 | ` *  $message` |
|        - | 10045 | ` *   The error message that should be logged.` |
|        - | 10046 | ` *  $message_type` |
|        - | 10047 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 10048 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 10049 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 10050 | ` *       This is the default option.` |
|        - | 10051 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 10052 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 10053 | ` *    2  No longer an option.` |
|        - | 10054 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 10055 | ` *       to the end of the message string.` |
|        - | 10056 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 10057 | ` *  $destination` |
|        - | 10058 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 10059 | ` *  $extra_headers` |
|        - | 10060 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 10061 | ` * Return` |
|        - | 10062 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10063 | ` * NOTE:` |
|        - | 10064 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 10065 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 10066 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 10067 | ` *  Otherwise this function is no-op.` |
|        - | 10068 | ` */` |
|        4 | 10069 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10070 |  |
|        - | 10071 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 10072 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 10073 | `	int iType = 0;` |
|        5 | 10074 | `	if( nArg < 1 ){` |
|        - | 10075 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 10076 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10077 | `		return PH7_OK;` |
|        - | 10078 | `	}` |
|        5 | 10079 | `	if( pVm->xErrLog  ){` |
|        - | 10080 | `		/* Invoke the user callback */` |
|      ! 0 | 10081 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 10082 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 10083 | `		if( nArg > 1 ){` |
|      ! 0 | 10084 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 10085 | `			if( nArg > 2 ){` |
|      ! 0 | 10086 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 10087 | `				if( nArg > 3 ){` |
|      ! 0 | 10088 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 10089 | `				}` |
|      ! 0 | 10090 | `			}` |
|      ! 0 | 10091 | `		}` |
|      ! 0 | 10092 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 10093 | `	}` |
|        - | 10094 | `	/* Retun TRUE */` |
|        5 | 10095 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10096 | `	return PH7_OK;` |
|        3 | 10097 |  |
|        - | 10098 | `/*` |
|        - | 10099 | ` * bool restore_exception_handler(void)` |
|        - | 10100 | ` *  Restores the previously defined exception handler function.` |
|        - | 10101 | ` * Parameter` |
|        - | 10102 | ` *  None` |
|        - | 10103 | ` * Return` |
|        - | 10104 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10105 | ` */` |
|        4 | 10106 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10107 |  |
|        5 | 10108 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10109 | `	ph7_value *pOld,*pNew;` |
|        - | 10110 | `	/* Point to the old and the new handler */` |
|        5 | 10111 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10112 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10113 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10114 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10115 | `		SXUNUSED(apArg);` |
|        - | 10116 | `		/* No installed handler,return FALSE */` |
|        5 | 10117 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10118 | `		return PH7_OK;` |
|        - | 10119 | `	}` |
|        - | 10120 | `	/* Copy the old handler */` |
|      ! 0 | 10121 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10122 | `	PH7_MemObjRelease(pOld);` |
|        - | 10123 | `	/* Return TRUE */` |
|      ! 0 | 10124 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10125 | `	return PH7_OK;` |
|        3 | 10126 |  |
|        - | 10127 | `/*` |
|        - | 10128 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10129 | ` *  Sets a user-defined exception handler function.` |
|        - | 10130 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10131 | ` * NOTE` |
|        - | 10132 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10133 | ` *  the satndard PHP engine.` |
|        - | 10134 | ` * Parameters` |
|        - | 10135 | ` *  $exception_handler` |
|        - | 10136 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10137 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10138 | ` *   that was thrown.` |
|        - | 10139 | ` *  Note:` |
|        - | 10140 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10141 | ` * Return` |
|        - | 10142 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10143 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10144 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10145 | ` */` |
|        4 | 10146 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10147 |  |
|        6 | 10148 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10149 | `	ph7_value *pOld,*pNew;` |
|        - | 10150 | `	/* Point to the old and the new handler */` |
|        6 | 10151 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10152 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10153 | `	/* Return the old handler */` |
|        6 | 10154 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10155 | `	if( nArg > 0 ){` |
|        6 | 10156 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10157 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10158 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10159 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10160 | `		}else{` |
|        6 | 10161 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10162 | `			/* Install the new handler */` |
|        6 | 10163 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10164 | `		}` |
|        2 | 10165 | `	}` |
|        6 | 10166 | `	return PH7_OK;` |
|        2 | 10167 |  |
|        - | 10168 | `/*` |
|        - | 10169 | ` * bool restore_error_handler(void)` |
|        - | 10170 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10171 | ` * Parameters:` |
|        - | 10172 | ` *  None.` |
|        - | 10173 | ` * Return` |
|        - | 10174 | ` *  Always TRUE.` |
|        - | 10175 | ` */` |
|        4 | 10176 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10177 |  |
|        5 | 10178 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10179 | `	ph7_value *pOld,*pNew;` |
|        - | 10180 | `	/* Point to the old and the new handler */` |
|        5 | 10181 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10182 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10183 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10184 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10185 | `		SXUNUSED(apArg);` |
|        - | 10186 | `		/* No installed callback,return FALSE */` |
|        5 | 10187 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10188 | `		return PH7_OK;` |
|        - | 10189 | `	}` |
|        - | 10190 | `	/* Copy the old callback */` |
|      ! 0 | 10191 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10192 | `	PH7_MemObjRelease(pOld);` |
|        - | 10193 | `	/* Return TRUE */` |
|      ! 0 | 10194 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10195 | `	return PH7_OK;` |
|        3 | 10196 |  |
|        - | 10197 | `/*` |
|        - | 10198 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10199 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10200 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10201 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10202 | ` *  Sets a user-defined error handler function.` |
|        - | 10203 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10204 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10205 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10206 | ` *  conditions (using trigger_error()).` |
|        - | 10207 | ` * Parameters` |
|        - | 10208 | ` *  $error_handler` |
|        - | 10209 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10210 | ` *   describing the error.` |
|        - | 10211 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10212 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10213 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10214 | ` *   The function can be shown as:` |
|        - | 10215 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10216 | ` *     errno` |
|        - | 10217 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10218 | ` *   errstr` |
|        - | 10219 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10220 | ` *   errfile` |
|        - | 10221 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10222 | ` *     was raised in, as a string.` |
|        - | 10223 | ` *  Note:` |
|        - | 10224 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10225 | ` * Return` |
|        - | 10226 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10227 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10228 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10229 | ` */` |
|     9326 | 10230 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10231 |  |
|     9328 | 10232 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10233 | `	ph7_value *pOld,*pNew;` |
|        - | 10234 | `	/* Point to the old and the new handler */` |
|     9328 | 10235 | `	pOld = &pVm->aErrCB[0];` |
|     9328 | 10236 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10237 | `	/* Return the old handler */` |
|     9328 | 10238 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9328 | 10239 | `	if( nArg > 0 ){` |
|     9328 | 10240 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10241 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4663 | 10242 | `			PH7_MemObjRelease(pNew);` |
|     4663 | 10243 | `			ph7_result_bool(pCtx,1);` |
|     2332 | 10244 | `		}else{` |
|     4666 | 10245 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10246 | `			/* Install the new handler */` |
|     4666 | 10247 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10248 | `		}` |
|     4663 | 10249 | `	}` |
|     9328 | 10250 | `	return PH7_OK;` |
|        2 | 10251 |  |
|        - | 10252 | `/*` |
|        - | 10253 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10254 | ` *  Generates a backtrace.` |
|        - | 10255 | ` * Paramaeter` |
|        - | 10256 | ` *  $options` |
|        - | 10257 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10258 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10259 | ` *   all the function/method arguments, to save memory.` |
|        - | 10260 | ` * $limit` |
|        - | 10261 | ` *   (Not Used)` |
|        - | 10262 | ` * Return` |
|        - | 10263 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10264 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10265 | ` *          Name        Type      Description` |
|        - | 10266 | ` *          ------      ------     -----------` |
|        - | 10267 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10268 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10269 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10270 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10271 | ` *          object      object    The current object.` |
|        - | 10272 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10273 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10274 | ` */` |
|      522 | 10275 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10276 |  |
|      524 | 10277 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10278 | `	ph7_value *pArray;` |
|        - | 10279 | `	ph7_class *pClass;` |
|        - | 10280 | `	ph7_value *pValue;` |
|        - | 10281 | `	SyString *pFile;` |
|        - | 10282 | `	/* Create a new array */` |
|      524 | 10283 | `	pArray = ph7_context_new_array(pCtx);` |
|      524 | 10284 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      524 | 10285 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10286 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10287 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10288 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10289 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10290 | `		SXUNUSED(apArg);` |
|      ! 0 | 10291 | `		return PH7_OK;` |
|        - | 10292 | `	}` |
|        - | 10293 | `	/* Dump running function name and it's arguments  */` |
|      524 | 10294 | `	if( pVm->pFrame->pParent ){` |
|      524 | 10295 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10296 | `		ph7_vm_func *pFunc;` |
|        - | 10297 | `		ph7_value *pArg;` |
|      524 | 10298 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      524 | 10299 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      524 | 10300 | `		if( pFrame->pParent && pFunc ){` |
|      524 | 10301 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      524 | 10302 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      524 | 10303 | `			ph7_value_reset_string_cursor(pValue);` |
|      261 | 10304 | `		}` |
|        - | 10305 | `		/* Function arguments */` |
|      524 | 10306 | `		pArg = ph7_context_new_array(pCtx);` |
|      524 | 10307 | `		if( pArg  ){` |
|        - | 10308 | `			ph7_value *pObj;` |
|        - | 10309 | `			VmSlot *aSlot;` |
|        - | 10310 | `			sxu32 n;` |
|        - | 10311 | `			/* Start filling the array with the given arguments */` |
|      524 | 10312 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2082 | 10313 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1560 | 10314 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1560 | 10315 | `				if( pObj ){` |
|     1560 | 10316 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      779 | 10317 | `				}` |
|      781 | 10318 | `			}` |
|        - | 10319 | `			/* Save the array */` |
|      524 | 10320 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      261 | 10321 | `		}` |
|      261 | 10322 | `	}` |
|      524 | 10323 | `	ph7_value_int(pValue,1);` |
|        - | 10324 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10325 | `	 * line numbers at run-time. )` |
|        - | 10326 | `	 */` |
|      524 | 10327 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10328 | `	/* Current processed script */` |
|      524 | 10329 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      524 | 10330 | `	if( pFile ){` |
|      524 | 10331 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      524 | 10332 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      524 | 10333 | `		ph7_value_reset_string_cursor(pValue);` |
|      261 | 10334 | `	}` |
|        - | 10335 | `	/* Top class */` |
|      524 | 10336 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      524 | 10337 | `	if( pClass ){` |
|      520 | 10338 | `		ph7_value_reset_string_cursor(pValue);` |
|      520 | 10339 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      520 | 10340 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      259 | 10341 | `	}` |
|        - | 10342 | `	/* Return the freshly created array */` |
|      524 | 10343 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10344 | `	/*` |
|        - | 10345 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10346 | `	 * as soon we return from this function.` |
|        - | 10347 | `	 */` |
|      524 | 10348 | `	return PH7_OK;` |
|      263 | 10349 |  |
|        - | 10350 | `/*` |
|        - | 10351 | ` * Generate a small backtrace.` |
|        - | 10352 | ` * Store the generated dump in the given BLOB` |
|        - | 10353 | ` */` |
|        4 | 10354 | `static int VmMiniBacktrace(` |
|        - | 10355 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10356 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10357 | `	)` |
|        1 | 10358 |  |
|        5 | 10359 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10360 | `	ph7_vm_func *pFunc;` |
|        - | 10361 | `	ph7_class *pClass;` |
|        - | 10362 | `	SyString *pFile;` |
|        - | 10363 | `	/* Called function */` |
|        5 | 10364 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10365 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10366 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10367 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10368 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10369 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10370 | `	}else{` |
|      ! 0 | 10371 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10372 | `	}` |
|        5 | 10373 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10374 | `	/* Current processed script */` |
|        5 | 10375 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10376 | `	if( pFile ){` |
|        5 | 10377 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10378 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10379 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10380 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10381 | `	}` |
|        - | 10382 | `	/* Top class */` |
|        5 | 10383 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10384 | `	if( pClass ){` |
|      ! 0 | 10385 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10386 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10387 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10388 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10389 | `	}` |
|        5 | 10390 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10391 | `	/* All done */` |
|        5 | 10392 | `	return SXRET_OK;` |
|        1 | 10393 |  |
|        - | 10394 | `/*` |
|        - | 10395 | ` * void debug_print_backtrace()` |
|        - | 10396 | ` *  Prints a backtrace` |
|        - | 10397 | ` * Parameters` |
|        - | 10398 | ` * None` |
|        - | 10399 | ` * Return` |
|        - | 10400 | ` * NULL` |
|        - | 10401 | ` */` |
|        2 | 10402 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10403 |  |
|        3 | 10404 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10405 | `	SyBlob sDump;` |
|        3 | 10406 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10407 | `	/* Generate the backtrace */` |
|        3 | 10408 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10409 | `	/* Output backtrace */` |
|        3 | 10410 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10411 | `	/* All done,cleanup */` |
|        3 | 10412 | `	SyBlobRelease(&sDump);` |
|        1 | 10413 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10414 | `	SXUNUSED(apArg);` |
|        3 | 10415 | `	return PH7_OK;` |
|        1 | 10416 |  |
|        - | 10417 | `/*` |
|        - | 10418 | ` * string debug_string_backtrace()` |
|        - | 10419 | ` *  Generate a backtrace` |
|        - | 10420 | ` * Parameters` |
|        - | 10421 | ` * None` |
|        - | 10422 | ` * Return` |
|        - | 10423 | ` *  A mini backtrace().` |
|        - | 10424 | ` * Note that this is a symisc extension.` |
|        - | 10425 | ` */` |
|        2 | 10426 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10427 |  |
|        3 | 10428 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10429 | `	SyBlob sDump;` |
|        3 | 10430 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10431 | `	/* Generate the backtrace */` |
|        3 | 10432 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10433 | `	/* Return the backtrace */` |
|        3 | 10434 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10435 | `	/* All done,cleanup */` |
|        3 | 10436 | `	SyBlobRelease(&sDump);` |
|        1 | 10437 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10438 | `	SXUNUSED(apArg);` |
|        3 | 10439 | `	return PH7_OK;` |
|        1 | 10440 |  |
|        - | 10441 | `/*` |
|        - | 10442 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10443 | ` * exception is triggered.` |
|        - | 10444 | ` */` |
|      480 | 10445 | `static sxi32 VmUncaughtException(` |
|        - | 10446 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10447 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10448 | `	)` |
|        1 | 10449 |  |
|        - | 10450 | `	ph7_value *apArg[2],sArg;` |
|      481 | 10451 | `	int nArg = 1;` |
|        - | 10452 | `	sxi32 rc;` |
|      481 | 10453 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10454 | `		/* Nesting limit reached */` |
|      ! 0 | 10455 | `		return SXRET_OK;` |
|        - | 10456 | `	}` |
|        - | 10457 | `	/* Call any exception handler if available */` |
|      481 | 10458 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 10459 | `	if( pThis ){` |
|        - | 10460 | `		/* Load the exception instance */` |
|      481 | 10461 | `		sArg.x.pOther = pThis;` |
|      481 | 10462 | `		pThis->iRef++;` |
|      481 | 10463 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 10464 | `	}else{` |
|      ! 0 | 10465 | `		nArg = 0;` |
|        - | 10466 | `	}` |
|      481 | 10467 | `	apArg[0] = &sArg;` |
|        - | 10468 | `	/* Call the exception handler if available */` |
|      481 | 10469 | `	pVm->nExceptDepth++;` |
|      481 | 10470 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 10471 | `	pVm->nExceptDepth--;` |
|      481 | 10472 | `	if( rc != SXRET_OK ){` |
|        - | 10473 | `		SyBlob sMsgBuf;` |
|      479 | 10474 | `		const char *zClass = "Exception";` |
|      479 | 10475 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10476 | `		const char *zMsg;` |
|        - | 10477 | `		sxu32 nMsg;` |
|        - | 10478 | `		const char *zFuncName;` |
|        - | 10479 | `		int nFuncLen;` |
|      479 | 10480 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 10481 | `		if( pThis ){` |
|        - | 10482 | `			ph7_class_method *pGetMessage;` |
|        - | 10483 | `			ph7_value sMsg;` |
|        - | 10484 | `			const char *zTmp;` |
|        - | 10485 | `			int nTmp;` |
|      479 | 10486 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 10487 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 10488 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 10489 | `			if( pGetMessage ){` |
|      479 | 10490 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 10491 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 10492 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 10493 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 10494 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 10495 | `					}` |
|      239 | 10496 | `				}` |
|      479 | 10497 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 10498 | `			}` |
|      239 | 10499 | `		}` |
|      479 | 10500 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10501 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10502 | `		}` |
|      479 | 10503 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 10504 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 10505 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 10506 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 10507 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10508 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 10509 | `		rc = SXERR_ABORT;` |
|      239 | 10510 | `	}` |
|      481 | 10511 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 10512 | `	return rc;` |
|      241 | 10513 |  |
|        - | 10514 | `/*` |
|        - | 10515 | ` * Throw a user exception.` |
|        - | 10516 | ` *` |
|        - | 10517 | ` * Exception dispatch follows this sequence:` |
|        - | 10518 | ` *` |
|        - | 10519 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10520 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10521 | ` *` |
|        - | 10522 | ` * 2. If NO catch matches:` |
|        - | 10523 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10524 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10525 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10526 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10527 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10528 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10529 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10530 | ` *` |
|        - | 10531 | ` * 3. If a catch DOES match:` |
|        - | 10532 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10533 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10534 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10535 | ` *       finally block.` |
|        - | 10536 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10537 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10538 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10539 | ` *       in pPendingException (step 2c).` |
|        - | 10540 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10541 | ` *    d. Run finally (if present).` |
|        - | 10542 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10543 | ` *       that handlers are restored and finally has run.` |
|        - | 10544 | ` */` |
|      522 | 10545 | `static sxi32 VmThrowException(` |
|        - | 10546 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10547 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10548 | `	)` |
|        2 | 10549 |  |
|        - | 10550 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10551 | `	ph7_exception **apException;` |
|        - | 10552 | `	ph7_exception *pException;` |
|        - | 10553 | `	/* Point to the stack of loaded exceptions */` |
|      524 | 10554 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      524 | 10555 | `	pException = 0;` |
|      524 | 10556 | `	pCatch = 0;` |
|      524 | 10557 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10558 | `		ph7_exception_block *aCatch;` |
|        - | 10559 | `		ph7_class *pClass;` |
|        - | 10560 | `		sxu32 j;` |
|        - | 10561 | `		/* Locate the appropriate block to execute */` |
|       40 | 10562 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       40 | 10563 | `		(void)SySetPop(&pVm->aException);` |
|       40 | 10564 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       40 | 10565 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       38 | 10566 | `			SyString *pName = &aCatch[j].sClass;` |
|        - | 10567 | `			/* Extract the target class */` |
|       38 | 10568 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       38 | 10569 | `			if( pClass == 0 ){` |
|        - | 10570 | `				/* No such class */` |
|      ! 0 | 10571 | `				continue;` |
|        - | 10572 | `			}` |
|       38 | 10573 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - | 10574 | `				/* Catch block found,break immeditaley */` |
|       38 | 10575 | `				pCatch = &aCatch[j];` |
|       38 | 10576 | `				break;` |
|        - | 10577 | `			}` |
|      ! 0 | 10578 | `		}` |
|       19 | 10579 | `	}` |
|        - | 10580 | `	/* Execute the cached block if available */` |
|      524 | 10581 | `	if( pCatch == 0 ){` |
|        - | 10582 | `		sxi32 rc;` |
|        - | 10583 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      488 | 10584 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10585 | `			pException->iFinallyDone = 1;` |
|        3 | 10586 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10587 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10588 | `				return SXERR_ABORT;` |
|        - | 10589 | `			}` |
|        1 | 10590 | `		}` |
|        - | 10591 | `		/* Check if there is an outer exception handler on the stack */` |
|      488 | 10592 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10593 | `			/* Re-throw to the outer handler */` |
|        3 | 10594 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10595 | `		}` |
|        - | 10596 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10597 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10598 | `		 * exception instead of reporting it uncaught.` |
|        - | 10599 | `		 */` |
|      486 | 10600 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10601 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10602 | `			 * by looking for a catch frame on the stack.` |
|        - | 10603 | `			 */` |
|      486 | 10604 | `			VmFrame *pF = pVm->pFrame;` |
|      486 | 10605 | `			int inCatch = 0;` |
|      972 | 10606 | `			while( pF ){` |
|      492 | 10607 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        5 | 10608 | `					inCatch = 1;` |
|        5 | 10609 | `					break;` |
|        - | 10610 | `				}` |
|      487 | 10611 | `				pF = pF->pParent;` |
|        1 | 10612 | `			}` |
|      486 | 10613 | `			if( inCatch ){` |
|        - | 10614 | `				/* Defer — will be re-thrown after finally runs */` |
|        5 | 10615 | `				pThis->iRef++;` |
|        5 | 10616 | `				pVm->pPendingException = pThis;` |
|        5 | 10617 | `				return SXRET_OK;` |
|        - | 10618 | `			}` |
|      240 | 10619 | `		}` |
|        - | 10620 | `		/* Truly uncaught */` |
|      481 | 10621 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 10622 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10623 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10624 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10625 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10626 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10627 | `			}` |
|      ! 0 | 10628 | `		}` |
|      481 | 10629 | `		return rc;` |
|      ! 0 | 10630 | `	}else{` |
|       38 | 10631 | `		VmFrame *pFrame = pVm->pFrame;` |
|       38 | 10632 | `		ph7_exception **apSaved = 0;` |
|        - | 10633 | `		sxu32 nSavedCount;` |
|        - | 10634 | `		sxi32 rc;` |
|       38 | 10635 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       38 | 10636 | `		if( pException->pFrame == pFrame ){` |
|       24 | 10637 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 | 10638 | `		}` |
|        - | 10639 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10640 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10641 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10642 | `		 */` |
|       38 | 10643 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       38 | 10644 | `		if( nSavedCount > 0 ){` |
|       10 | 10645 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 | 10646 | `				nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10647 | `			if( apSaved ){` |
|       10 | 10648 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 | 10649 | `					nSavedCount * sizeof(ph7_exception *));` |
|        7 | 10650 | `				SySetReset(&pVm->aException);` |
|        3 | 10651 | `			}` |
|        3 | 10652 | `		}` |
|        - | 10653 | `		/* Create a private frame first */` |
|       38 | 10654 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       38 | 10655 | `		if( rc == SXRET_OK ){` |
|       38 | 10656 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       38 | 10657 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       38 | 10658 | `			if( pObj ){` |
|       38 | 10659 | `				pThis->iRef++;` |
|       38 | 10660 | `				pObj->x.pOther = pThis;` |
|       38 | 10661 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       18 | 10662 | `			}` |
|        - | 10663 | `			/* Execute the catch block */` |
|       38 | 10664 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10665 | `			/* Leave the frame */` |
|       38 | 10666 | `			VmLeaveFrame(&(*pVm));` |
|       18 | 10667 | `		}` |
|        - | 10668 | `		/* Restore the outer exception handlers */` |
|       38 | 10669 | `		if( apSaved ){` |
|        - | 10670 | `			sxu32 k;` |
|        - | 10671 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10672 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10673 | `			 * Restore the original outer entries.` |
|        - | 10674 | `			 */` |
|        7 | 10675 | `			SySetReset(&pVm->aException);` |
|       13 | 10676 | `			for(k = 0; k < nSavedCount; k++){` |
|        7 | 10677 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        4 | 10678 | `			}` |
|        7 | 10679 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 | 10680 | `		}` |
|        - | 10681 | `		/* Execute the finally block after catch */` |
|       38 | 10682 | `		if( pException->iHasFinally ){` |
|       12 | 10683 | `			pException->iFinallyDone = 1;` |
|        - | 10684 | `			{` |
|       12 | 10685 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       12 | 10686 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10687 | `					return SXERR_ABORT;` |
|        - | 10688 | `				}` |
|        - | 10689 | `			}` |
|        5 | 10690 | `		}` |
|       38 | 10691 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10692 | `			return SXERR_ABORT;` |
|        - | 10693 | `		}` |
|        - | 10694 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10695 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10696 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10697 | `		 */` |
|       38 | 10698 | `		if( pVm->pPendingException ){` |
|        5 | 10699 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        5 | 10700 | `			pVm->pPendingException = 0;` |
|        5 | 10701 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10702 | `		}` |
|        - | 10703 | `	}` |
|        - | 10704 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10705 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10706 | `	 */` |
|       34 | 10707 | `	return SXRET_OK;` |
|      263 | 10708 |  |
|        - | 10709 | `/*` |
|        - | 10710 | ` * Section:` |
|        - | 10711 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10712 | ` * Status:` |
|        - | 10713 | ` *    Stable.` |
|        - | 10714 | ` */` |
|        - | 10715 | `/*` |
|        - | 10716 | ` * string ph7version(void)` |
|        - | 10717 | ` *  Returns the running version of the PH7 version.` |
|        - | 10718 | ` * Parameters` |
|        - | 10719 | ` *  None` |
|        - | 10720 | ` * Return` |
|        - | 10721 | ` * Current PH7 version.` |
|        - | 10722 | ` */` |
|        2 | 10723 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10724 |  |
|        1 | 10725 | `	SXUNUSED(nArg);` |
|        1 | 10726 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10727 | `	/* Current engine version */` |
|        3 | 10728 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10729 | `	return PH7_OK;` |
|        1 | 10730 |  |
|        - | 10731 | `/*` |
|        - | 10732 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10733 | ` */` |
|        - | 10734 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10735 | ` "<html><head>"\` |
|        - | 10736 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10737 | ` "<style type=\"text/css\">"\` |
|        - | 10738 | ` "div {"\` |
|        - | 10739 | `     "border: 1px solid #cccccc;"\` |
|        - | 10740 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10741 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10742 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10743 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10744 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10745 | `     "-o-border-radius: 10px;"\` |
|        - | 10746 | `     "border-radius: 10px;"\` |
|        - | 10747 | `     "padding-left: 2em;"\` |
|        - | 10748 | `     "background-color: white;"\` |
|        - | 10749 | `     "margin-left: auto;"\` |
|        - | 10750 | `     "font-family: verdana;"\` |
|        - | 10751 | `     "padding-right: 2em;"\` |
|        - | 10752 | `     "margin-right: auto;"\` |
|        - | 10753 | `     "}"\` |
|        - | 10754 | `     "body {"\` |
|        - | 10755 | `     "padding: 0.2em;"\` |
|        - | 10756 | `     "font-style: normal;"\` |
|        - | 10757 | `     "font-size: medium;"\` |
|        - | 10758 | `     "background-color: #f2f2f2;"\` |
|        - | 10759 | `     "}"\` |
|        - | 10760 | `     "hr {"\` |
|        - | 10761 | `     "border-style: solid none none;"\` |
|        - | 10762 | `     "border-width: 1px medium medium;"\` |
|        - | 10763 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10764 | `     "height: 1px;"\` |
|        - | 10765 | `     "}"\` |
|        - | 10766 | `     "a {"\` |
|        - | 10767 | `     "color: #3366cc;"\` |
|        - | 10768 | `     "text-decoration: none;"\` |
|        - | 10769 | `     "}"\` |
|        - | 10770 | `     "a:hover {"\` |
|        - | 10771 | `     "color: #999999;"\` |
|        - | 10772 | `     "}"\` |
|        - | 10773 | `     "a:active {"\` |
|        - | 10774 | `     "color: #663399;"\` |
|        - | 10775 | `     "}"\` |
|        - | 10776 | `     "h1 {"\` |
|        - | 10777 | `     "margin: 0;"\` |
|        - | 10778 | `     "padding: 0;"\` |
|        - | 10779 | `     "font-family: Verdana;"\` |
|        - | 10780 | `     "font-weight: bold;"\` |
|        - | 10781 | `     "font-style: normal;"\` |
|        - | 10782 | `     "font-size: medium;"\` |
|        - | 10783 | `     "text-transform: capitalize;"\` |
|        - | 10784 | `     "color: #0a328c;"\` |
|        - | 10785 | `     "}"\` |
|        - | 10786 | `     "p {"\` |
|        - | 10787 | `     "margin: 0 auto;"\` |
|        - | 10788 | `     "font-size: medium;"\` |
|        - | 10789 | `     "font-style: normal;"\` |
|        - | 10790 | `     "font-family: verdana;"\` |
|        - | 10791 | `     "}"\` |
|        - | 10792 | `"</style></head><body>"\` |
|        - | 10793 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10794 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10795 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10796 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10797 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10798 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10799 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10800 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10801 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10802 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10803 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10804 |  |
|        - | 10805 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10806 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10807 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10808 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10809 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10810 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10811 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10812 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10813 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10814 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10815 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10816 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10817 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10818 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10819 |  |
|        - | 10820 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10821 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10822 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10823 | `"&nbsp;*<br>"\` |
|        - | 10824 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10825 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10826 | `"&nbsp;* are met:<br>"\` |
|        - | 10827 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10828 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10829 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10830 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10831 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10832 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10833 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10834 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10835 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10836 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10837 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10838 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10839 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10840 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10841 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10842 | `"&nbsp;*<br>"\` |
|        - | 10843 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10844 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10845 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10846 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10847 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10848 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10849 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10850 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10851 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10852 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10853 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10854 | `"&nbsp;*/<br>"\` |
|        - | 10855 | `"</span></small></small></p>"\` |
|        - | 10856 | `"</div></body></html>"` |
|        - | 10857 | `/*` |
|        - | 10858 | ` * bool ph7credits(void)` |
|        - | 10859 | ` * bool ph7info(void)` |
|        - | 10860 | ` * bool ph7copyright(void)` |
|        - | 10861 | ` *  Prints out the credits for PH7 engine` |
|        - | 10862 | ` * Parameters` |
|        - | 10863 | ` *  None` |
|        - | 10864 | ` * Return` |
|        - | 10865 | ` *  Always TRUE` |
|        - | 10866 | ` */` |
|        2 | 10867 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10868 |  |
|        3 | 10869 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10870 | `	/* Expand the HTML page above*/` |
|        3 | 10871 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10872 | `	ph7_context_output_format(` |
|        1 | 10873 | `		pCtx,` |
|        - | 10874 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10875 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10876 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10877 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10878 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10879 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10880 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10881 | `#ifdef __WINNT__` |
|        - | 10882 | `		"Windows NT"` |
|        - | 10883 | `#elif defined(__UNIXES__)` |
|        - | 10884 | `		"UNIX-Like"` |
|        - | 10885 | `#else` |
|        - | 10886 | `		"Other OS"` |
|        - | 10887 | `#endif` |
|        - | 10888 | `		);` |
|        3 | 10889 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10890 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10891 | `	SXUNUSED(apArg);` |
|        - | 10892 | `	/* Return TRUE */` |
|        - | 10893 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10894 | `	return PH7_OK;` |
|        1 | 10895 |  |
|        - | 10896 | `/*` |
|        - | 10897 | ` * Section:` |
|        - | 10898 | ` *    URL related routines.` |
|        - | 10899 | ` * Status:` |
|        - | 10900 | ` *    Stable.` |
|        - | 10901 | ` */` |
|        - | 10902 | `/*` |
|        - | 10903 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10904 | ` *  Parse a URL and return its fields.` |
|        - | 10905 | ` * Parameters` |
|        - | 10906 | ` *  $url` |
|        - | 10907 | ` *   The URL to parse.` |
|        - | 10908 | ` * $component` |
|        - | 10909 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10910 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10911 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10912 | ` *  in which case the return value will be an integer).` |
|        - | 10913 | ` * Return` |
|        - | 10914 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10915 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10916 | ` *  this array are:` |
|        - | 10917 | ` *   scheme - e.g. http` |
|        - | 10918 | ` *   host` |
|        - | 10919 | ` *   port` |
|        - | 10920 | ` *   user` |
|        - | 10921 | ` *   pass` |
|        - | 10922 | ` *   path` |
|        - | 10923 | ` *   query - after the question mark ?` |
|        - | 10924 | ` *   fragment - after the hashmark #` |
|        - | 10925 | ` * Note:` |
|        - | 10926 | ` *  FALSE is returned on failure.` |
|        - | 10927 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10928 | ` *  with the standard PHP engine.` |
|        - | 10929 | ` */` |
|       28 | 10930 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10931 |  |
|        - | 10932 | `	const char *zStr; /* Input string */` |
|        - | 10933 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10934 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10935 | `	int nLen;` |
|        - | 10936 | `	sxi32 rc;` |
|       29 | 10937 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10938 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10939 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10940 | `		return PH7_OK;` |
|        - | 10941 | `	}` |
|        - | 10942 | `	/* Extract the given URI */` |
|       29 | 10943 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10944 | `	if( nLen < 1 ){` |
|        - | 10945 | `		/* Nothing to process,return FALSE */` |
|        3 | 10946 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10947 | `		return PH7_OK;` |
|        - | 10948 | `	}` |
|        - | 10949 | `	/* Get a parse */` |
|       27 | 10950 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10951 | `	if( rc != SXRET_OK ){` |
|        - | 10952 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10953 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10954 | `		return PH7_OK;` |
|        - | 10955 | `	}` |
|       27 | 10956 | `	if( nArg > 1 ){` |
|      ! 0 | 10957 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 10958 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 10959 | `		switch(nComponent){` |
|      ! 0 | 10960 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 10961 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 10962 | `			if( pComp->nByte < 1 ){` |
|        - | 10963 | `				/* No available value,return NULL */` |
|      ! 0 | 10964 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10965 | `			}else{` |
|      ! 0 | 10966 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10967 | `			}` |
|      ! 0 | 10968 | `			break;` |
|      ! 0 | 10969 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 10970 | `			pComp = &sURI.sHost;` |
|      ! 0 | 10971 | `			if( pComp->nByte < 1 ){` |
|        - | 10972 | `				/* No available value,return NULL */` |
|      ! 0 | 10973 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10974 | `			}else{` |
|      ! 0 | 10975 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10976 | `			}` |
|      ! 0 | 10977 | `			break;` |
|      ! 0 | 10978 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 10979 | `			pComp = &sURI.sPort;` |
|      ! 0 | 10980 | `			if( pComp->nByte < 1 ){` |
|        - | 10981 | `				/* No available value,return NULL */` |
|      ! 0 | 10982 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10983 | `			}else{` |
|      ! 0 | 10984 | `				int iPort = 0;` |
|        - | 10985 | `				/* Cast the value to integer */` |
|      ! 0 | 10986 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 10987 | `				ph7_result_int(pCtx,iPort);` |
|        - | 10988 | `			}` |
|      ! 0 | 10989 | `			break;` |
|      ! 0 | 10990 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 10991 | `			pComp = &sURI.sUser;` |
|      ! 0 | 10992 | `			if( pComp->nByte < 1 ){` |
|        - | 10993 | `				/* No available value,return NULL */` |
|      ! 0 | 10994 | `				ph7_result_null(pCtx);` |
|      ! 0 | 10995 | `			}else{` |
|      ! 0 | 10996 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 10997 | `			}` |
|      ! 0 | 10998 | `			break;` |
|      ! 0 | 10999 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 11000 | `			pComp = &sURI.sPass;` |
|      ! 0 | 11001 | `			if( pComp->nByte < 1 ){` |
|        - | 11002 | `				/* No available value,return NULL */` |
|      ! 0 | 11003 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11004 | `			}else{` |
|      ! 0 | 11005 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11006 | `			}` |
|      ! 0 | 11007 | `			break;` |
|      ! 0 | 11008 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 11009 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 11010 | `			if( pComp->nByte < 1 ){` |
|        - | 11011 | `				/* No available value,return NULL */` |
|      ! 0 | 11012 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11013 | `			}else{` |
|      ! 0 | 11014 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11015 | `			}` |
|      ! 0 | 11016 | `			break;` |
|      ! 0 | 11017 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 11018 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 11019 | `			if( pComp->nByte < 1 ){` |
|        - | 11020 | `				/* No available value,return NULL */` |
|      ! 0 | 11021 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11022 | `			}else{` |
|      ! 0 | 11023 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11024 | `			}` |
|      ! 0 | 11025 | `			break;` |
|      ! 0 | 11026 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 11027 | `			pComp = &sURI.sPath;` |
|      ! 0 | 11028 | `			if( pComp->nByte < 1 ){` |
|        - | 11029 | `				/* No available value,return NULL */` |
|      ! 0 | 11030 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11031 | `			}else{` |
|      ! 0 | 11032 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11033 | `			}` |
|      ! 0 | 11034 | `			break;` |
|      ! 0 | 11035 | `		default:` |
|        - | 11036 | `			/* No such entry,return NULL */` |
|      ! 0 | 11037 | `			ph7_result_null(pCtx);` |
|      ! 0 | 11038 | `			break;` |
|        - | 11039 | `		}` |
|      ! 0 | 11040 | `	}else{` |
|        - | 11041 | `		ph7_value *pArray,*pValue;` |
|        - | 11042 | `		/* Return an associative array */` |
|       27 | 11043 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 11044 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 11045 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11046 | `			/* Out of memory */` |
|      ! 0 | 11047 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11048 | `			/* Return false */` |
|      ! 0 | 11049 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 11050 | `			return PH7_OK;` |
|        - | 11051 | `		}` |
|        - | 11052 | `		/* Fill the array */` |
|       27 | 11053 | `		pComp = &sURI.sScheme;` |
|       27 | 11054 | `		if( pComp->nByte > 0 ){` |
|       19 | 11055 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 11056 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 11057 | `		}` |
|        - | 11058 | `		/* Reset the string cursor */` |
|       27 | 11059 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11060 | `		pComp = &sURI.sHost;` |
|       27 | 11061 | `		if( pComp->nByte > 0 ){` |
|       25 | 11062 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 11063 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 11064 | `		}` |
|        - | 11065 | `		/* Reset the string cursor */` |
|       27 | 11066 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11067 | `		pComp = &sURI.sPort;` |
|       27 | 11068 | `		if( pComp->nByte > 0 ){` |
|       11 | 11069 | `			int iPort = 0;/* cc warning */` |
|        - | 11070 | `			/* Convert to integer */` |
|       11 | 11071 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 11072 | `			ph7_value_int(pValue,iPort);` |
|       11 | 11073 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 11074 | `		}` |
|        - | 11075 | `		/* Reset the string cursor */` |
|       27 | 11076 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11077 | `		pComp = &sURI.sUser;` |
|       27 | 11078 | `		if( pComp->nByte > 0 ){` |
|        7 | 11079 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11080 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 11081 | `		}` |
|        - | 11082 | `		/* Reset the string cursor */` |
|       27 | 11083 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11084 | `		pComp = &sURI.sPass;` |
|       27 | 11085 | `		if( pComp->nByte > 0 ){` |
|        7 | 11086 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11087 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 11088 | `		}` |
|        - | 11089 | `		/* Reset the string cursor */` |
|       27 | 11090 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11091 | `		pComp = &sURI.sPath;` |
|       27 | 11092 | `		if( pComp->nByte > 0 ){` |
|       17 | 11093 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 11094 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 11095 | `		}` |
|        - | 11096 | `		/* Reset the string cursor */` |
|       27 | 11097 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11098 | `		pComp = &sURI.sQuery;` |
|       27 | 11099 | `		if( pComp->nByte > 0 ){` |
|        5 | 11100 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11101 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11102 | `		}` |
|        - | 11103 | `		/* Reset the string cursor */` |
|       27 | 11104 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11105 | `		pComp = &sURI.sFragment;` |
|       27 | 11106 | `		if( pComp->nByte > 0 ){` |
|        5 | 11107 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11108 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11109 | `		}` |
|        - | 11110 | `		/* Return the created array */` |
|       27 | 11111 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11112 | `		/* NOTE:` |
|        - | 11113 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11114 | `		 * automatically as soon we return from this function.` |
|        - | 11115 | `		 */` |
|        - | 11116 | `	}` |
|        - | 11117 | `	/* All done */` |
|       27 | 11118 | `	return PH7_OK;` |
|       15 | 11119 |  |
|        - | 11120 | `/*` |
|        - | 11121 | ` * Section:` |
|        - | 11122 | ` *   Array related routines.` |
|        - | 11123 | ` * Status:` |
|        - | 11124 | ` *    Stable.` |
|        - | 11125 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11126 | ` *  Array related functions that need access to the underlying` |
|        - | 11127 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11128 | ` */` |
|        - | 11129 | `/*` |
|        - | 11130 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11131 | ` * of the following structure.` |
|        - | 11132 | ` */` |
|        - | 11133 | `struct compact_data` |
|        - | 11134 |  |
|        - | 11135 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11136 | `	int nRecCount;      /* Recursion count */` |
|        - | 11137 | `};` |
|        - | 11138 | `/*` |
|        - | 11139 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11140 | ` */` |
|      ! 0 | 11141 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11142 |  |
|      ! 0 | 11143 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11144 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11145 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11146 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11147 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11148 | `		SyString sVar;` |
|      ! 0 | 11149 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11150 | `		if( sVar.nByte > 0 ){` |
|        - | 11151 | `			/* Query the current frame */` |
|      ! 0 | 11152 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11153 | `			/* ^` |
|        - | 11154 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11155 | `			 */` |
|      ! 0 | 11156 | `			if( pKey ){` |
|        - | 11157 | `				/* Perform the insertion */` |
|      ! 0 | 11158 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11159 | `			}` |
|      ! 0 | 11160 | `		}` |
|      ! 0 | 11161 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11162 | `		int rc;` |
|        - | 11163 | `		/* Recursively traverse this array */` |
|      ! 0 | 11164 | `		pData->nRecCount++;` |
|      ! 0 | 11165 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11166 | `		pData->nRecCount--;` |
|      ! 0 | 11167 | `		return rc;` |
|        - | 11168 | `	}` |
|      ! 0 | 11169 | `	return SXRET_OK;` |
|      ! 0 | 11170 |  |
|        - | 11171 | `/*` |
|        - | 11172 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11173 | ` *  Create array containing variables and their values.` |
|        - | 11174 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11175 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11176 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11177 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11178 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11179 | ` * Parameters` |
|        - | 11180 | ` *  $varname` |
|        - | 11181 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11182 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11183 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11184 | ` *   it recursively.` |
|        - | 11185 | ` * Return` |
|        - | 11186 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11187 | ` */` |
|        2 | 11188 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11189 |  |
|        - | 11190 | `	ph7_value *pArray,*pObj;` |
|        3 | 11191 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11192 | `	const char *zName;` |
|        - | 11193 | `	SyString sVar;` |
|        - | 11194 | `	int i,nLen;` |
|        3 | 11195 | `	if( nArg < 1 ){` |
|        - | 11196 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11197 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11198 | `		return PH7_OK;` |
|        - | 11199 | `	}` |
|        - | 11200 | `	/* Create the array */` |
|        3 | 11201 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11202 | `	if( pArray == 0 ){` |
|        - | 11203 | `		/* Out of memory */` |
|      ! 0 | 11204 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11205 | `		/* Return NULL */` |
|      ! 0 | 11206 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11207 | `		return PH7_OK;` |
|        - | 11208 | `	}` |
|        - | 11209 | `	/* Perform the requested operation */` |
|        7 | 11210 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11211 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11212 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11213 | `				struct compact_data sData;` |
|      ! 0 | 11214 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11215 | `				/* Recursively walk the array */` |
|      ! 0 | 11216 | `				sData.nRecCount = 0;` |
|      ! 0 | 11217 | `				sData.pArray = pArray;` |
|      ! 0 | 11218 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11219 | `			}` |
|      ! 0 | 11220 | `		}else{` |
|        - | 11221 | `			/* Extract variable name */` |
|        5 | 11222 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11223 | `			if( nLen > 0 ){` |
|        5 | 11224 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11225 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11226 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11227 | `				if( pObj ){` |
|        5 | 11228 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11229 | `				}` |
|        2 | 11230 | `			}` |
|        - | 11231 | `		}` |
|        3 | 11232 | `	}` |
|        - | 11233 | `	/* Return the array */` |
|        3 | 11234 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11235 | `	return PH7_OK;` |
|        2 | 11236 |  |
|        - | 11237 | `/*` |
|        - | 11238 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11239 | ` * of the following structure.` |
|        - | 11240 | ` */` |
|        - | 11241 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11242 | `struct extract_aux_data` |
|        - | 11243 |  |
|        - | 11244 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11245 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11246 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11247 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11248 | `	int iFlags;           /* Control flags */` |
|        - | 11249 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11250 | `};` |
|        - | 11251 | `/* Forward declaration */` |
|        - | 11252 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11253 | `/*` |
|        - | 11254 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11255 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11256 | ` * Parameters` |
|        - | 11257 | ` * $var_array` |
|        - | 11258 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11259 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11260 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11261 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11262 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11263 | ` * $extract_type` |
|        - | 11264 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11265 | ` *  It can be one of the following values:` |
|        - | 11266 | ` *   EXTR_OVERWRITE` |
|        - | 11267 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11268 | ` *   EXTR_SKIP` |
|        - | 11269 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11270 | ` *   EXTR_PREFIX_SAME` |
|        - | 11271 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11272 | ` *   EXTR_PREFIX_ALL` |
|        - | 11273 | ` *       Prefix all variable names with prefix.` |
|        - | 11274 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11275 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11276 | ` *   EXTR_IF_EXISTS` |
|        - | 11277 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11278 | ` *       otherwise do nothing.` |
|        - | 11279 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11280 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11281 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11282 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11283 | ` *      the current symbol table.` |
|        - | 11284 | ` * $prefix` |
|        - | 11285 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11286 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11287 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11288 | ` *  underscore character.` |
|        - | 11289 | ` * Return` |
|        - | 11290 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11291 | ` */` |
|        4 | 11292 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11293 |  |
|        - | 11294 | `	extract_aux_data sAux;` |
|        - | 11295 | `	ph7_hashmap *pMap;` |
|        5 | 11296 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11297 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11298 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11299 | `		return PH7_OK;` |
|        - | 11300 | `	}` |
|        - | 11301 | `	/* Point to the target hashmap */` |
|        5 | 11302 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11303 | `	if( pMap->nEntry < 1 ){` |
|        - | 11304 | `		/* Empty map,return  0 */` |
|      ! 0 | 11305 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11306 | `		return PH7_OK;` |
|        - | 11307 | `	}` |
|        - | 11308 | `	/* Prepare the aux data */` |
|        5 | 11309 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11310 | `	if( nArg > 1 ){` |
|        3 | 11311 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11312 | `		if( nArg > 2 ){` |
|      ! 0 | 11313 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11314 | `		}` |
|        1 | 11315 | `	}` |
|        5 | 11316 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11317 | `	/* Invoke the worker callback */` |
|        5 | 11318 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11319 | `	/* Number of variables successfully imported */` |
|        5 | 11320 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11321 | `	return PH7_OK;` |
|        3 | 11322 |  |
|        - | 11323 | `/*` |
|        - | 11324 | ` * Worker callback for the [extract()] function defined` |
|        - | 11325 | ` * below.` |
|        - | 11326 | ` */` |
|        8 | 11327 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11328 |  |
|        9 | 11329 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11330 | `	int iFlags = pAux->iFlags;` |
|        9 | 11331 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11332 | `	ph7_value *pObj;` |
|        - | 11333 | `	SyString sVar;` |
|        9 | 11334 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11335 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11336 | `	}` |
|        - | 11337 | `	/* Perform a string cast */` |
|        9 | 11338 | `	PH7_MemObjToString(pKey);` |
|        9 | 11339 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11340 | `		/* Unavailable variable name */` |
|      ! 0 | 11341 | `		return SXRET_OK;` |
|        - | 11342 | `	}` |
|        9 | 11343 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11344 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11345 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11346 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11347 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11348 | `			);` |
|      ! 0 | 11349 | `	}else{` |
|       13 | 11350 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11351 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11352 | `	}` |
|        9 | 11353 | `	sVar.zString = pAux->zWorker;` |
|        - | 11354 | `	/* Try to extract the variable */` |
|        9 | 11355 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11356 | `	if( pObj ){` |
|        - | 11357 | `		/* Collision */` |
|        5 | 11358 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11359 | `			return SXRET_OK;` |
|        - | 11360 | `		}` |
|        5 | 11361 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11362 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11363 | `				/* Already prefixed */` |
|      ! 0 | 11364 | `				return SXRET_OK;` |
|        - | 11365 | `			}` |
|      ! 0 | 11366 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11367 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11368 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11369 | `				);` |
|      ! 0 | 11370 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11371 | `		}` |
|        3 | 11372 | `	}else{` |
|        - | 11373 | `		/* Create the variable */` |
|        5 | 11374 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11375 | `	}` |
|        9 | 11376 | `	if( pObj ){` |
|        - | 11377 | `		/* Overwrite the old value */` |
|        9 | 11378 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11379 | `		/* Increment counter */` |
|        9 | 11380 | `		pAux->iCount++;` |
|        4 | 11381 | `	}` |
|        9 | 11382 | `	return SXRET_OK;` |
|        5 | 11383 |  |
|        - | 11384 | `/*` |
|        - | 11385 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11386 | ` * defined below.` |
|        - | 11387 | ` */` |
|        2 | 11388 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11389 |  |
|        3 | 11390 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11391 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11392 | `	ph7_value *pObj;` |
|        - | 11393 | `	SyString sVar;` |
|        - | 11394 | `	/* Perform a string cast */` |
|        3 | 11395 | `	PH7_MemObjToString(pKey);` |
|        3 | 11396 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11397 | `		/* Unavailable variable name */` |
|      ! 0 | 11398 | `		return SXRET_OK;` |
|        - | 11399 | `	}` |
|        3 | 11400 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11401 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11402 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11403 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11404 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11405 | `			);` |
|        2 | 11406 | `	}else{` |
|      ! 0 | 11407 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11408 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11409 | `	}` |
|        3 | 11410 | `	sVar.zString = pAux->zWorker;` |
|        - | 11411 | `	/* Extract the variable */` |
|        3 | 11412 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11413 | `	if( pObj ){` |
|        3 | 11414 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11415 | `	}` |
|        3 | 11416 | `	return SXRET_OK;` |
|        2 | 11417 |  |
|        - | 11418 | `/*` |
|        - | 11419 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11420 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11421 | ` * Parameters` |
|        - | 11422 | ` * $types` |
|        - | 11423 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11424 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11425 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11426 | ` *  POST includes the POST uploaded file information.` |
|        - | 11427 | ` *  Note:` |
|        - | 11428 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11429 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11430 | ` * $prefix` |
|        - | 11431 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11432 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11433 | ` *  variable named $pref_userid.` |
|        - | 11434 | ` * Return` |
|        - | 11435 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11436 | ` */` |
|        2 | 11437 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11438 |  |
|        - | 11439 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11440 | `	extract_aux_data sAux;` |
|        - | 11441 | `	int nLen,nPrefixLen;` |
|        - | 11442 | `	ph7_value *pSuper;` |
|        - | 11443 | `	ph7_vm *pVm;` |
|        - | 11444 | `	/* By default import only $_GET variables  */` |
|        3 | 11445 | `	zImport = "G";` |
|        3 | 11446 | `	nLen = (int)sizeof(char);` |
|        3 | 11447 | `	zPrefix = 0;` |
|        3 | 11448 | `	nPrefixLen = 0;` |
|        3 | 11449 | `	if( nArg > 0 ){` |
|        3 | 11450 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11451 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11452 | `		}` |
|        3 | 11453 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11454 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11455 | `		}` |
|        1 | 11456 | `	}` |
|        - | 11457 | `	/* Point to the underlying VM */` |
|        3 | 11458 | `	pVm = pCtx->pVm;` |
|        - | 11459 | `	/* Initialize the aux data */` |
|        3 | 11460 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11461 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11462 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11463 | `	sAux.pVm = pVm;` |
|        - | 11464 | `	/* Extract */` |
|        3 | 11465 | `	zEnd = &zImport[nLen];` |
|        5 | 11466 | `	while( zImport < zEnd ){` |
|        3 | 11467 | `		int c = zImport[0];` |
|        3 | 11468 | `		pSuper = 0;` |
|        3 | 11469 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11470 | `			/* Import $_GET variables */` |
|        3 | 11471 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11472 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11473 | `			/* Import $_POST variables */` |
|      ! 0 | 11474 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11475 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11476 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11477 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11478 | `		}` |
|        3 | 11479 | `		if( pSuper ){` |
|        - | 11480 | `			/* Iterate throw array entries */` |
|        3 | 11481 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11482 | `		}` |
|        - | 11483 | `		/* Advance the cursor */` |
|        3 | 11484 | `		zImport++;` |
|        1 | 11485 | `	}` |
|        - | 11486 | `	/* All done,return TRUE*/` |
|        3 | 11487 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11488 | `	return PH7_OK;` |
|        1 | 11489 |  |
|        - | 11490 | `/*` |
|        - | 11491 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11492 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11493 | ` * information.` |
|        - | 11494 | ` */` |
|    10754 | 11495 | `static sxi32 VmEvalChunk(` |
|        - | 11496 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11497 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11498 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11499 | `	int iFlags,         /* Compile flag */` |
|        - | 11500 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11501 | `	)` |
|        2 | 11502 |  |
|        - | 11503 | `	SySet *pByteCode,aByteCode;` |
|        - | 11504 | `	SyBlob sSavedNs;` |
|    10756 | 11505 | `	ProcConsumer xErr = 0;` |
|    10756 | 11506 | `	void *pErrData = 0;` |
|        - | 11507 | `	/* Initialize bytecode container */` |
|    10756 | 11508 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10756 | 11509 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11510 | `	/* Reset the code generator */` |
|    10756 | 11511 | `	if( bTrueReturn ){` |
|        - | 11512 | `		/* Included file,log compile-time errors */` |
|     8134 | 11513 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8134 | 11514 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4066 | 11515 | `	}` |
|    10756 | 11516 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11517 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11518 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11519 | `	 * the caller's namespace is restored. */` |
|    10756 | 11520 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10756 | 11521 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10756 | 11522 | `	if( bTrueReturn ){` |
|        - | 11523 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8134 | 11524 | `		SyBlobReset(&pVm->sNamespace);` |
|     4066 | 11525 | `	}` |
|        - | 11526 | `	/* Swap bytecode container */` |
|    10756 | 11527 | `	pByteCode = pVm->pByteContainer;` |
|    10756 | 11528 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11529 | `	/* Compile the chunk */` |
|    10756 | 11530 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    16133 | 11531 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11532 | `		/* Compilation error,return false */` |
|        3 | 11533 | `		if( pCtx ){` |
|        3 | 11534 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11535 | `		}` |
|        2 | 11536 | `	}else{` |
|        - | 11537 | `		/* Mount any newly defined classes */` |
|        - | 11538 | `		SyHashEntry *pEntry;` |
|        - | 11539 | `		ph7_class *pClass;` |
|        - | 11540 | `		ph7_value sResult; /* Return value */` |
|        - | 11541 | `		sxi32 rc;` |
|    10754 | 11542 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   370010 | 11543 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   353882 | 11544 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11545 | `			/* Only mount classes that haven't been mounted yet */` |
|   353882 | 11546 | `			if( !pClass->bMounted ){` |
|    82540 | 11547 | `				rc = VmMountUserClass(pVm,pClass);` |
|    82540 | 11548 | `				if( rc != SXRET_OK ){` |
|        - | 11549 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11550 | `					if( pCtx ){` |
|      ! 0 | 11551 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11552 | `					}` |
|      ! 0 | 11553 | `					goto Cleanup;` |
|        - | 11554 | `				}` |
|    41269 | 11555 | `			}` |
|        2 | 11556 | `		}` |
|    10754 | 11557 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11558 | `			/* Out of memory */` |
|      ! 0 | 11559 | `			if( pCtx ){` |
|      ! 0 | 11560 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11561 | `			}` |
|      ! 0 | 11562 | `			goto Cleanup;` |
|        - | 11563 | `		}` |
|    10754 | 11564 | `		if( bTrueReturn ){` |
|        - | 11565 | `			/* Assume a boolean true return value */` |
|     8134 | 11566 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4068 | 11567 | `		}else{` |
|        - | 11568 | `			/* Assume a null return value */` |
|     2622 | 11569 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11570 | `		}` |
|        - | 11571 | `		/* Execute the compiled chunk */` |
|    10754 | 11572 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10754 | 11573 | `		if( pCtx ){` |
|        - | 11574 | `			/* Set the execution result */` |
|     8152 | 11575 | `			ph7_result_value(pCtx,&sResult);` |
|     4075 | 11576 | `		}` |
|    10754 | 11577 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11578 | `	}` |
|     5377 | 11579 | `Cleanup:` |
|        - | 11580 | `	/* Cleanup the mess left behind */` |
|    10756 | 11581 | `	pVm->pByteContainer = pByteCode;` |
|    10756 | 11582 | `	SySetRelease(&aByteCode);` |
|        - | 11583 | `	/* Restore caller's namespace state */` |
|    10756 | 11584 | `	SyBlobReset(&pVm->sNamespace);` |
|    10756 | 11585 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10756 | 11586 | `	SyBlobRelease(&sSavedNs);` |
|    10756 | 11587 | `	return SXRET_OK;` |
|        2 | 11588 |  |
|        - | 11589 | `/*` |
|        - | 11590 | ` * value eval(string $code)` |
|        - | 11591 | ` *   Evaluate a string as PHP code.` |
|        - | 11592 | ` * Parameter` |
|        - | 11593 | ` *  code: PHP code to evaluate.` |
|        - | 11594 | ` * Return` |
|        - | 11595 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11596 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11597 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11598 | ` */` |
|       22 | 11599 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11600 |  |
|        - | 11601 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 11602 | `	if( nArg < 1 ){` |
|        - | 11603 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11604 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11605 | `		return SXRET_OK;` |
|        - | 11606 | `	}` |
|        - | 11607 | `	/* Chunk to evaluate */` |
|       24 | 11608 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 11609 | `	if( sChunk.nByte < 1 ){` |
|        - | 11610 | `		/* Empty string,return NULL */` |
|        3 | 11611 | `		ph7_result_null(pCtx);` |
|        3 | 11612 | `		return SXRET_OK;` |
|        - | 11613 | `	}` |
|        - | 11614 | `	/* Eval the chunk */` |
|       22 | 11615 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 11616 | `	return SXRET_OK;` |
|       13 | 11617 |  |
|        - | 11618 | `/*` |
|        - | 11619 | ` * Check if a file path is already included.` |
|        - | 11620 | ` */` |
|    16260 | 11621 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 11622 |  |
|        - | 11623 | `	SyString *aEntries;` |
|        - | 11624 | `	sxu32 n;` |
|    16262 | 11625 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11626 | `	/* Perform a linear search */` |
| 66052182 | 11627 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 66035928 | 11628 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11629 | `			/* Already included */` |
|        7 | 11630 | `			return TRUE;` |
|        - | 11631 | `		}` |
| 33017962 | 11632 | `	}` |
|    16256 | 11633 | `	return FALSE;` |
|     8132 | 11634 |  |
|        - | 11635 | `/*` |
|        - | 11636 | ` * Push a file path in the appropriate VM container.` |
|        - | 11637 | ` */` |
|    18854 | 11638 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11639 |  |
|        - | 11640 | `	SyString sPath;` |
|        - | 11641 | `	char *zDup;` |
|        - | 11642 | `#ifdef __WINNT__` |
|        - | 11643 | `	char *zCur;` |
|        - | 11644 | `#endif` |
|        - | 11645 | `	sxi32 rc;` |
|    18856 | 11646 | `	if( nLen < 0 ){` |
|     2596 | 11647 | `		nLen = SyStrlen(zPath);` |
|     1297 | 11648 | `	}` |
|        - | 11649 | `	/* Duplicate the file path first */` |
|    18856 | 11650 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18856 | 11651 | `	if( zDup == 0 ){` |
|      ! 0 | 11652 | `		return SXERR_MEM;` |
|        - | 11653 | `	}` |
|        - | 11654 | `#ifdef __WINNT__` |
|        - | 11655 | `	/* Normalize path on windows` |
|        - | 11656 | `	 * Example:` |
|        - | 11657 | `	 *    Path/To/File.php` |
|        - | 11658 | `	 * becomes` |
|        - | 11659 | `	 *   path\to\file.php` |
|        - | 11660 | `	 */` |
|        2 | 11661 | `	zCur = zDup;` |
|        2 | 11662 | `	while( zCur[0] != 0 ){` |
|        2 | 11663 | `		if( zCur[0] == '/' ){` |
|        2 | 11664 | `			zCur[0] = '\\';` |
|        2 | 11665 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11666 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11667 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11668 | `		}` |
|        2 | 11669 | `		zCur++;` |
|        2 | 11670 | `	}` |
|        - | 11671 | `#endif` |
|        - | 11672 | `	/* Install the file path */` |
|    18856 | 11673 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18856 | 11674 | `	if( !bMain ){` |
|    16262 | 11675 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11676 | `			/* Already included */` |
|        7 | 11677 | `			*pNew = 0;` |
|        4 | 11678 | `		}else{` |
|        - | 11679 | `			/* Insert in the corresponding container */` |
|    16256 | 11680 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16256 | 11681 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11682 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11683 | `				return rc;` |
|        - | 11684 | `			}` |
|    16256 | 11685 | `			*pNew = 1;` |
|        - | 11686 | `		}` |
|     8130 | 11687 | `	}` |
|    18856 | 11688 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18856 | 11689 | `	return SXRET_OK;` |
|     9429 | 11690 |  |
|        - | 11691 | `/*` |
|        - | 11692 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11693 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11694 | ` * indicates failure.` |
|        - | 11695 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11696 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11697 | ` * operations.` |
|        - | 11698 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11699 | ` * this function is a no-op.` |
|        - | 11700 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11701 | ` * constructs for more information.` |
|        - | 11702 | ` */` |
|     8142 | 11703 | `static sxi32 VmExecIncludedFile(` |
|        - | 11704 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11705 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11706 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11707 | `	 )` |
|        2 | 11708 |  |
|        - | 11709 | `	sxi32 rc;` |
|        - | 11710 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11711 | `	const ph7_io_stream *pStream;` |
|        - | 11712 | `	SyBlob sContents;` |
|        - | 11713 | `	void *pHandle;` |
|        - | 11714 | `	ph7_vm *pVm;` |
|        - | 11715 | `	int isNew;` |
|        - | 11716 | `	/* Initialize fields */` |
|     8144 | 11717 | `	pVm = pCtx->pVm;` |
|     8144 | 11718 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8144 | 11719 | `	isNew = 0;` |
|        - | 11720 | `	/* Extract the associated stream */` |
|     8144 | 11721 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11722 | `	/*` |
|        - | 11723 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11724 | `	 * in a read-only mode.` |
|        - | 11725 | `	 */` |
|     8144 | 11726 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8144 | 11727 | `	if( pHandle == 0 ){` |
|        8 | 11728 | `		return SXERR_IO;` |
|        - | 11729 | `	}` |
|     8138 | 11730 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8138 | 11731 | `	if( IncludeOnce && !isNew ){` |
|        - | 11732 | `		/* Already included */` |
|        5 | 11733 | `		rc = SXERR_EXISTS;` |
|        3 | 11734 | `	}else{` |
|        - | 11735 | `		/* Read the whole file contents */` |
|     8134 | 11736 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8134 | 11737 | `		if( rc == SXRET_OK ){` |
|        - | 11738 | `			SyString sScript;` |
|        - | 11739 | `			/* Compile and execute the script */` |
|     8134 | 11740 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8134 | 11741 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4066 | 11742 | `		}` |
|        - | 11743 | `	}` |
|        - | 11744 | `	/* Pop from the set of included file */` |
|     8138 | 11745 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11746 | `	/* Close the handle */` |
|     8138 | 11747 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11748 | `	/* Release the working buffer */` |
|     8138 | 11749 | `	SyBlobRelease(&sContents);` |
|        - | 11750 | `#else` |
|        - | 11751 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11752 | `	SXUNUSED(pPath);` |
|        - | 11753 | `	SXUNUSED(IncludeOnce);` |
|        - | 11754 | `	rc = SXERR_IO;` |
|        - | 11755 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8138 | 11756 | `	return rc;` |
|     4073 | 11757 |  |
|        - | 11758 | `/*` |
|        - | 11759 | ` * string get_include_path(void)` |
|        - | 11760 | ` *  Gets the current include_path configuration option.` |
|        - | 11761 | ` * Parameter` |
|        - | 11762 | ` *  None` |
|        - | 11763 | ` * Return` |
|        - | 11764 | ` *  Included paths as a string` |
|        - | 11765 | ` */` |
|        2 | 11766 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11767 |  |
|        3 | 11768 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11769 | `	SyString *aEntry;` |
|        - | 11770 | `	int dir_sep;` |
|        - | 11771 | `	sxu32 n;` |
|        - | 11772 | `#ifdef __WINNT__` |
|        1 | 11773 | `	dir_sep = ';';` |
|        - | 11774 | `#else` |
|        - | 11775 | `	/* Assume UNIX path separator */` |
|        2 | 11776 | `	dir_sep = ':';` |
|        - | 11777 | `#endif` |
|        1 | 11778 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11779 | `	SXUNUSED(apArg);` |
|        - | 11780 | `	/* Point to the list of import paths */` |
|        3 | 11781 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11782 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11783 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11784 | `		if( n > 0 ){` |
|        - | 11785 | `			/* Append dir seprator */` |
|      ! 0 | 11786 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11787 | `		}` |
|        - | 11788 | `		/* Append path */` |
|        3 | 11789 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11790 | `	}` |
|        3 | 11791 | `	return PH7_OK;` |
|        1 | 11792 |  |
|        - | 11793 | `/*` |
|        - | 11794 | ` * string get_get_included_files(void)` |
|        - | 11795 | ` *  Gets the current include_path configuration option.` |
|        - | 11796 | ` * Parameter` |
|        - | 11797 | ` *  None` |
|        - | 11798 | ` * Return` |
|        - | 11799 | ` *  Included paths as a string` |
|        - | 11800 | ` */` |
|        2 | 11801 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11802 |  |
|        3 | 11803 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11804 | `	ph7_value *pArray,*pWorker;` |
|        - | 11805 | `	SyString *pEntry;` |
|        - | 11806 | `	int c,d;` |
|        - | 11807 | `	/* Create an array and a working value */` |
|        3 | 11808 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11809 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11810 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11811 | `		/* Out of memory,return null */` |
|      ! 0 | 11812 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11813 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11814 | `		SXUNUSED(apArg);` |
|      ! 0 | 11815 | `		return PH7_OK;` |
|        - | 11816 | `	}` |
|        3 | 11817 | `	c = d = '/';` |
|        - | 11818 | `#ifdef __WINNT__` |
|        1 | 11819 | `	d = '\\';` |
|        - | 11820 | `#endif` |
|        - | 11821 | `	/* Iterate throw entries */` |
|        3 | 11822 | `	SySetResetCursor(pFiles);` |
|     3839 | 11823 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11824 | `		const char *zBase,*zEnd;` |
|        - | 11825 | `		int iLen;` |
|        - | 11826 | `		/* reset the string cursor */` |
|     3837 | 11827 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11828 | `		/* Extract base name */` |
|     3837 | 11829 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11830 | `		/* Ignore trailing '/' */` |
|     5755 | 11831 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11832 | `			zEnd--;` |
|      ! 0 | 11833 | `		}` |
|     3837 | 11834 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 11835 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 11836 | `			zEnd--;` |
|        1 | 11837 | `		}` |
|     3837 | 11838 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 11839 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11840 | `		/* Copy entry name */` |
|     3837 | 11841 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11842 | `		/* Perform the insertion */` |
|     3837 | 11843 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11844 | `	}` |
|        - | 11845 | `	/* All done,return the created array */` |
|        3 | 11846 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11847 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11848 | `	 * by the engine as soon we return from this foreign` |
|        - | 11849 | `	 * function.` |
|        - | 11850 | `	 */` |
|        3 | 11851 | `	return PH7_OK;` |
|        2 | 11852 |  |
|        - | 11853 | `/*` |
|        - | 11854 | ` * include:` |
|        - | 11855 | ` * According to the PHP reference manual.` |
|        - | 11856 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11857 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11858 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11859 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11860 | ` *  and the current working directory before failing. The include()` |
|        - | 11861 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11862 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11863 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11864 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11865 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11866 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11867 | ` *  directory to find the requested file.` |
|        - | 11868 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11869 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11870 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11871 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11872 | ` */` |
|     8124 | 11873 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11874 |  |
|        - | 11875 | `	SyString sFile;` |
|        - | 11876 | `	sxi32 rc;` |
|     8126 | 11877 | `	if( nArg < 1 ){` |
|        - | 11878 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11879 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11880 | `		return SXRET_OK;` |
|        - | 11881 | `	}` |
|        - | 11882 | `	/* File to include */` |
|     8126 | 11883 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8126 | 11884 | `	if( sFile.nByte < 1 ){` |
|        - | 11885 | `		/* Empty string,return NULL */` |
|      ! 0 | 11886 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11887 | `		return SXRET_OK;` |
|        - | 11888 | `	}` |
|        - | 11889 | `	/* Open,compile and execute the desired script */` |
|     8126 | 11890 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8126 | 11891 | `	if( rc != SXRET_OK ){` |
|        - | 11892 | `		/* Emit a warning and return false */` |
|        3 | 11893 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11894 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11895 | `	}` |
|     8126 | 11896 | `	return SXRET_OK;` |
|     4064 | 11897 |  |
|        - | 11898 | `/*` |
|        - | 11899 | ` * include_once:` |
|        - | 11900 | ` *  According to the PHP reference manual.` |
|        - | 11901 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11902 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11903 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11904 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11905 | ` *   just once.` |
|        - | 11906 | ` */` |
|        4 | 11907 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11908 |  |
|        - | 11909 | `	SyString sFile;` |
|        - | 11910 | `	sxi32 rc;` |
|        5 | 11911 | `	if( nArg < 1 ){` |
|        - | 11912 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11913 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11914 | `		return SXRET_OK;` |
|        - | 11915 | `	}` |
|        - | 11916 | `	/* File to include */` |
|        5 | 11917 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11918 | `	if( sFile.nByte < 1 ){` |
|        - | 11919 | `		/* Empty string,return NULL */` |
|      ! 0 | 11920 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11921 | `		return SXRET_OK;` |
|        - | 11922 | `	}` |
|        - | 11923 | `	/* Open,compile and execute the desired script */` |
|        5 | 11924 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11925 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11926 | `		/* File already included,return TRUE */` |
|        3 | 11927 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11928 | `		return SXRET_OK;` |
|        - | 11929 | `	}` |
|        3 | 11930 | `	if( rc != SXRET_OK ){` |
|        - | 11931 | `		/* Emit a warning and return false */` |
|      ! 0 | 11932 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11933 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11934 | ` 	}` |
|        3 | 11935 | `	return SXRET_OK;` |
|        3 | 11936 |  |
|        - | 11937 | `/*` |
|        - | 11938 | ` * require.` |
|        - | 11939 | ` *  According to the PHP reference manual.` |
|        - | 11940 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11941 | ` *   also produce a fatal level error.` |
|        - | 11942 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11943 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11944 | ` */` |
|        6 | 11945 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11946 |  |
|        - | 11947 | `	SyString sFile;` |
|        - | 11948 | `	sxi32 rc;` |
|        8 | 11949 | `	if( nArg < 1 ){` |
|        - | 11950 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11951 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11952 | `		return SXRET_OK;` |
|        - | 11953 | `	}` |
|        - | 11954 | `	/* File to include */` |
|        8 | 11955 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 11956 | `	if( sFile.nByte < 1 ){` |
|        - | 11957 | `		/* Empty string,return NULL */` |
|      ! 0 | 11958 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11959 | `		return SXRET_OK;` |
|        - | 11960 | `	}` |
|        - | 11961 | `	/* Open,compile and execute the desired script */` |
|        8 | 11962 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 11963 | `	if( rc != SXRET_OK ){` |
|        - | 11964 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 11965 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11966 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11967 | `		return PH7_ABORT;` |
|        - | 11968 | `	}` |
|        8 | 11969 | `	return SXRET_OK;` |
|        5 | 11970 |  |
|        - | 11971 | `/*` |
|        - | 11972 | ` * require_once:` |
|        - | 11973 | ` *  According to the PHP reference manual.` |
|        - | 11974 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 11975 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 11976 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 11977 | ` *   and how it differs from its non _once siblings.` |
|        - | 11978 | ` */` |
|        4 | 11979 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11980 |  |
|        - | 11981 | `	SyString sFile;` |
|        - | 11982 | `	sxi32 rc;` |
|        5 | 11983 | `	if( nArg < 1 ){` |
|        - | 11984 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11985 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11986 | `		return SXRET_OK;` |
|        - | 11987 | `	}` |
|        - | 11988 | `	/* File to include */` |
|        5 | 11989 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11990 | `	if( sFile.nByte < 1 ){` |
|        - | 11991 | `		/* Empty string,return NULL */` |
|      ! 0 | 11992 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11993 | `		return SXRET_OK;` |
|        - | 11994 | `	}` |
|        - | 11995 | `	/* Open,compile and execute the desired script */` |
|        5 | 11996 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11997 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11998 | `		/* File already included,return TRUE */` |
|        3 | 11999 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12000 | `		return SXRET_OK;` |
|        - | 12001 | `	}` |
|        3 | 12002 | `	if( rc != SXRET_OK ){` |
|        - | 12003 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12004 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12005 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12006 | `		return PH7_ABORT;` |
|        - | 12007 | `	}` |
|        3 | 12008 | `	return SXRET_OK;` |
|        3 | 12009 |  |
|        - | 12010 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 12011 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 12012 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 12013 | `/*` |
|        - | 12014 | ` * Section:` |
|        - | 12015 | ` *  SPL Autoloading functions.` |
|        - | 12016 | ` * Status:` |
|        - | 12017 | ` *  Stable.` |
|        - | 12018 | ` */` |
|        - | 12019 | `/*` |
|        - | 12020 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 12021 | ` *  Register given function as __autoload() implementation.` |
|        - | 12022 | ` * Parameters` |
|        - | 12023 | ` *  callback` |
|        - | 12024 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 12025 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 12026 | ` *  throw` |
|        - | 12027 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 12028 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 12029 | ` *  prepend` |
|        - | 12030 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 12031 | ` *   autoload stack instead of appending it.` |
|        - | 12032 | ` * Return` |
|        - | 12033 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12034 | ` */` |
|       34 | 12035 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12036 |  |
|        - | 12037 | `	VmAutoloadCB sEntry;` |
|       36 | 12038 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 12039 | `	int iPrepend = 0;` |
|        - | 12040 | `	sxu32 n;` |
|       36 | 12041 | `	if( nArg < 1 ){` |
|        - | 12042 | `		/* No callback provided — register default spl_autoload.` |
|        - | 12043 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 12044 | `		/* Check for duplicates first */` |
|        9 | 12045 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 12046 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 12047 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 12048 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 12049 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 12050 | `				ph7_result_bool(pCtx,1);` |
|        5 | 12051 | `				return SXRET_OK;` |
|        - | 12052 | `			}` |
|      ! 0 | 12053 | `		}` |
|        5 | 12054 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 12055 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 12056 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 12057 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 12058 | `		ph7_result_bool(pCtx,1);` |
|        5 | 12059 | `		return SXRET_OK;` |
|        - | 12060 | `	}` |
|        - | 12061 | `	/* Validate that the callback is callable */` |
|       28 | 12062 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 12063 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 12064 | `		if( nArg >= 2 ){` |
|      ! 0 | 12065 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12066 | `		}` |
|      ! 0 | 12067 | `		if( iThrow ){` |
|      ! 0 | 12068 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 12069 | `				"Argument is not callable");` |
|      ! 0 | 12070 | `		}` |
|      ! 0 | 12071 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12072 | `		return SXRET_OK;` |
|        - | 12073 | `	}` |
|        - | 12074 | `	/* Check for duplicates */` |
|       46 | 12075 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 12076 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 12077 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12078 | `			/* Already registered */` |
|      ! 0 | 12079 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12080 | `			return SXRET_OK;` |
|        - | 12081 | `		}` |
|       11 | 12082 | `	}` |
|        - | 12083 | `	/* Check prepend flag */` |
|       28 | 12084 | `	if( nArg >= 3 ){` |
|        3 | 12085 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 12086 | `	}` |
|        - | 12087 | `	/* Store the callback */` |
|       28 | 12088 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 12089 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 12090 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 12091 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 12092 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 12093 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 12094 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 12095 | `		VmAutoloadCB *aBase;` |
|        3 | 12096 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12097 | `		/* Rotate: move last entry to front */` |
|        3 | 12098 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12099 | `		if( aBase ){` |
|        - | 12100 | `			VmAutoloadCB sTemp;` |
|        - | 12101 | `			sxu32 i;` |
|        3 | 12102 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12103 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12104 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12105 | `			}` |
|        3 | 12106 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12107 | `		}` |
|        2 | 12108 | `	}else{` |
|       26 | 12109 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12110 | `	}` |
|       28 | 12111 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12112 | `	return SXRET_OK;` |
|       19 | 12113 |  |
|        - | 12114 | `/*` |
|        - | 12115 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12116 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12117 | ` * Parameters` |
|        - | 12118 | ` *  callback` |
|        - | 12119 | ` *   The autoload function being unregistered.` |
|        - | 12120 | ` * Return` |
|        - | 12121 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12122 | ` */` |
|       32 | 12123 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12124 |  |
|       34 | 12125 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12126 | `	sxu32 n,nEntry;` |
|       34 | 12127 | `	if( nArg < 1 ){` |
|      ! 0 | 12128 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12129 | `		return SXRET_OK;` |
|        - | 12130 | `	}` |
|       34 | 12131 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12132 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12133 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12134 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12135 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12136 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12137 | `			sxu32 i;` |
|       32 | 12138 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12139 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12140 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12141 | `			}` |
|        - | 12142 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12143 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12144 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12145 | `			return SXRET_OK;` |
|        - | 12146 | `		}` |
|        3 | 12147 | `	}` |
|        3 | 12148 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12149 | `	return SXRET_OK;` |
|       18 | 12150 |  |
|        - | 12151 | `/*` |
|        - | 12152 | ` * array spl_autoload_functions(void)` |
|        - | 12153 | ` *  Return all registered __autoload() functions.` |
|        - | 12154 | ` * Return` |
|        - | 12155 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12156 | ` *  an empty array is returned.` |
|        - | 12157 | ` */` |
|       20 | 12158 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12159 |  |
|       21 | 12160 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12161 | `	ph7_value *pArray;` |
|        - | 12162 | `	sxu32 n,nEntry;` |
|       10 | 12163 | `	SXUNUSED(nArg);` |
|       10 | 12164 | `	SXUNUSED(apArg);` |
|       21 | 12165 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12166 | `	if( pArray == 0 ){` |
|      ! 0 | 12167 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12168 | `		return SXRET_OK;` |
|        - | 12169 | `	}` |
|       21 | 12170 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12171 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12172 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12173 | `		if( pEntry ){` |
|       15 | 12174 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12175 | `		}` |
|        8 | 12176 | `	}` |
|       21 | 12177 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12178 | `	return SXRET_OK;` |
|       11 | 12179 |  |
|        - | 12180 | `/*` |
|        - | 12181 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12182 | ` *  Default implementation of __autoload().` |
|        - | 12183 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12184 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12185 | ` * Parameters` |
|        - | 12186 | ` *  class` |
|        - | 12187 | ` *   The class name being searched.` |
|        - | 12188 | ` *  file_extensions` |
|        - | 12189 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12190 | ` */` |
|        2 | 12191 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12192 |  |
|        - | 12193 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12194 | `	SyBlob sPath;` |
|        - | 12195 | `	int nClass;` |
|        - | 12196 | `	sxi32 rc;` |
|        3 | 12197 | `	if( nArg < 1 ){` |
|      ! 0 | 12198 | `		return SXRET_OK;` |
|        - | 12199 | `	}` |
|        3 | 12200 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12201 | `	if( nClass < 1 ){` |
|      ! 0 | 12202 | `		return SXRET_OK;` |
|        - | 12203 | `	}` |
|        - | 12204 | `	/* Default extensions */` |
|        3 | 12205 | `	zExt = ".php,.inc";` |
|        3 | 12206 | `	if( nArg >= 2 ){` |
|        - | 12207 | `		int nExt;` |
|      ! 0 | 12208 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12209 | `		if( nExt < 1 ){` |
|      ! 0 | 12210 | `			zExt = ".php,.inc";` |
|      ! 0 | 12211 | `		}` |
|      ! 0 | 12212 | `	}` |
|        3 | 12213 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12214 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12215 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12216 | `	zCur = zExt;` |
|        7 | 12217 | `	while( zCur < zEnd ){` |
|        - | 12218 | `		const char *zComma;` |
|        - | 12219 | `		SyString sFile;` |
|        - | 12220 | `		int i;` |
|        - | 12221 | `		/* Find next comma or end */` |
|        5 | 12222 | `		zComma = zCur;` |
|       21 | 12223 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12224 | `			zComma++;` |
|        1 | 12225 | `		}` |
|        - | 12226 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12227 | `		SyBlobReset(&sPath);` |
|       69 | 12228 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12229 | `			char c = zClass[i];` |
|       65 | 12230 | `			if( c == '\\' ){` |
|      ! 0 | 12231 | `				c = '/';` |
|       65 | 12232 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12233 | `				c = c + ('a' - 'A');` |
|        6 | 12234 | `			}` |
|       65 | 12235 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12236 | `		}` |
|        - | 12237 | `		/* Append extension */` |
|        5 | 12238 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12239 | `		/* Try to include the file */` |
|        5 | 12240 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12241 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12242 | `		if( rc == SXRET_OK ){` |
|        - | 12243 | `			/* File included successfully */` |
|      ! 0 | 12244 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12245 | `			return SXRET_OK;` |
|        - | 12246 | `		}` |
|        - | 12247 | `		/* Move past the comma */` |
|        5 | 12248 | `		zCur = zComma;` |
|        5 | 12249 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12250 | `			zCur++;` |
|        1 | 12251 | `		}` |
|        1 | 12252 | `	}` |
|        3 | 12253 | `	SyBlobRelease(&sPath);` |
|        3 | 12254 | `	return SXRET_OK;` |
|        2 | 12255 |  |
|        - | 12256 | `/* Table of built-in VM functions. */` |
|        - | 12257 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12258 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12259 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12260 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12261 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12262 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12263 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12264 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12265 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12266 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12267 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12268 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12269 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12270 | `	    /* Constants management */` |
|        - | 12271 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12272 | `	{ "define",   vm_builtin_define               },` |
|        - | 12273 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12274 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12275 | `	   /* Class/Object functions */` |
|        - | 12276 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12277 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12278 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12279 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12280 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12281 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12282 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12283 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12284 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12285 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12286 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12287 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12288 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12289 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12290 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12291 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12292 | `	   /* SPL Autoloading */` |
|        - | 12293 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12294 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12295 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12296 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12297 | `	   /* Random numbers/strings generators */` |
|        - | 12298 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12299 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12300 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12301 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12302 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12303 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12304 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12305 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12306 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12307 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12308 | `	   /* Language constructs functions */` |
|        - | 12309 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12310 | `	{ "print", vm_builtin_print                   },` |
|        - | 12311 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12312 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12313 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12314 | `	  /* Variable handling functions */` |
|        - | 12315 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12316 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12317 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12318 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12319 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12320 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12321 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12322 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12323 | `	  /* Ouput control functions */` |
|        - | 12324 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12325 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12326 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12327 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12328 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12329 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12330 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12331 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12332 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12333 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12334 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12335 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12336 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12337 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12338 | `	  /* Assertion functions */` |
|        - | 12339 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12340 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12341 | `	  /* Error reporting functions */` |
|        - | 12342 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12343 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12344 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12345 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12346 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12347 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12348 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12349 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12350 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12351 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12352 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12353 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12354 | `	  /* Release info */` |
|        - | 12355 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12356 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12357 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12358 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12359 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12360 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12361 | `	  /* hashmap */` |
|        - | 12362 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12363 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12364 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12365 | `	  /* URL related function */` |
|        - | 12366 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12367 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12368 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12369 | `	   /* XML processing functions */` |
|        - | 12370 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12371 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12372 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12373 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12374 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12375 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12376 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 12377 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 12378 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 12379 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 12380 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 12381 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 12382 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 12383 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 12384 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 12385 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12386 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12387 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12388 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12389 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12390 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12391 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12392 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12393 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12394 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12395 | `	   /* Command line processing */` |
|        - | 12396 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12397 | `	   /* JSON encoding/decoding */` |
|        - | 12398 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12399 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12400 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12401 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12402 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12403 | `	   /* Files/URI inclusion facility */` |
|        - | 12404 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12405 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12406 | `	{ "include",      vm_builtin_include          },` |
|        - | 12407 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12408 | `	{ "require",      vm_builtin_require          },` |
|        - | 12409 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12410 | `};` |
|        - | 12411 | `/*` |
|        - | 12412 | ` * Register the built-in VM functions defined above.` |
|        - | 12413 | ` */` |
|     2340 | 12414 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12415 |  |
|        - | 12416 | `	sxi32 rc;` |
|        - | 12417 | `	sxu32 n;` |
|   301862 | 12418 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12419 | `		/* Note that these special functions have access` |
|        - | 12420 | `		 * to the underlying virtual machine as their` |
|        - | 12421 | `		 * private data.` |
|        - | 12422 | `		 */` |
|   299522 | 12423 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   299522 | 12424 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12425 | `			return rc;` |
|        - | 12426 | `		}` |
|   149762 | 12427 | `	}` |
|     2342 | 12428 | `	return SXRET_OK;` |
|     1172 | 12429 |  |
|        - | 12430 | `/*` |
|        - | 12431 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 12432 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 12433 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 12434 | ` */` |
|    32816 | 12435 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 12436 |  |
|    32818 | 12437 | `	if( !iLoadable ){` |
|    31604 | 12438 | `		return pClass;` |
|        - | 12439 | `	}` |
|     1216 | 12440 | `	while(pClass){` |
|     1216 | 12441 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1216 | 12442 | `			return pClass;` |
|        - | 12443 | `		}` |
|      ! 0 | 12444 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12445 | `	}` |
|      ! 0 | 12446 | `	return 0;` |
|    16410 | 12447 |  |
|        - | 12448 | `/*` |
|        - | 12449 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 12450 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 12451 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 12452 | ` * registered in the VM's class table.` |
|        - | 12453 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 12454 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 12455 | ` */` |
|       30 | 12456 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12457 |  |
|        - | 12458 | `	VmAutoloadCB *pEntry;` |
|        - | 12459 | `	ph7_value sArg,sResult;` |
|        - | 12460 | `	SyHashEntry *pHashEntry;` |
|        - | 12461 | `	ph7_class *pClass;` |
|        - | 12462 | `	sxu32 n,nEntry;` |
|       32 | 12463 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       32 | 12464 | `	if( nEntry < 1 ){` |
|       18 | 12465 | `		return 0;` |
|        - | 12466 | `	}` |
|        - | 12467 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 12468 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 12469 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 12470 | `	}` |
|        - | 12471 | `	/* Mark this class as being autoloaded */` |
|       14 | 12472 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 12473 | `	/* Prepare the class name argument */` |
|       14 | 12474 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 12475 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 12476 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 12477 | `	pClass = 0;` |
|       28 | 12478 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 12479 | `		ph7_value *apArg[1];` |
|       24 | 12480 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 12481 | `		if( pEntry == 0 ){` |
|      ! 0 | 12482 | `			continue;` |
|        - | 12483 | `		}` |
|       24 | 12484 | `		apArg[0] = &sArg;` |
|       24 | 12485 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 12486 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 12487 | `			continue;` |
|        - | 12488 | `		}` |
|        - | 12489 | `		/* Check if the class is now available */` |
|       24 | 12490 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 12491 | `		if( pHashEntry ){` |
|       10 | 12492 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 12493 | `			if( pClass ){` |
|       10 | 12494 | `				break;` |
|        - | 12495 | `			}` |
|      ! 0 | 12496 | `		}` |
|        9 | 12497 | `	}` |
|       14 | 12498 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 12499 | `	PH7_MemObjRelease(&sResult);` |
|        - | 12500 | `	/* Remove reentrancy guard */` |
|       14 | 12501 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 12502 | `	return pClass;` |
|       17 | 12503 |  |
|        - | 12504 | `/*` |
|        - | 12505 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 12506 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 12507 | ` */` |
|       18 | 12508 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12509 |  |
|       20 | 12510 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 12511 |  |
|        - | 12512 | `/*` |
|        - | 12513 | ` * Check if the given name refer to an installed class.` |
|        - | 12514 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12515 | ` */` |
|    32820 | 12516 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12517 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12518 | `	const char *zName,  /* Name of the target class */` |
|        - | 12519 | `	sxu32 nByte,        /* zName length */` |
|        - | 12520 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12521 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12522 | `						 */` |
|        - | 12523 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12524 | `	)` |
|        2 | 12525 |  |
|        - | 12526 | `	SyHashEntry *pEntry;` |
|        - | 12527 | `	ph7_class *pClass;` |
|    16410 | 12528 | `	SXUNUSED(iNest);` |
|        - | 12529 | `	/* Exact class lookup.` |
|        - | 12530 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12531 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    32822 | 12532 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    32822 | 12533 | `	if( pEntry == 0 ){` |
|        - | 12534 | `		/* Class not found in hash table — try autoload before giving up */` |
|       14 | 12535 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 12536 | `	}` |
|    32810 | 12537 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    32810 | 12538 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    16412 | 12539 |  |
|        - | 12540 | `/*` |
|        - | 12541 | ` * Reference Table Implementation` |
|        - | 12542 | ` * Status: stable <chm@symisc.net>` |
|        - | 12543 | ` * Intro` |
|        - | 12544 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12545 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12546 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12547 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12548 | ` *  Refer to the official for more information on this powerful` |
|        - | 12549 | ` *  extension.` |
|        - | 12550 | ` */` |
|        - | 12551 | `/*` |
|        - | 12552 | ` * Allocate a new reference entry.` |
|        - | 12553 | ` */` |
|  3068454 | 12554 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12555 |  |
|        - | 12556 | `	VmRefObj *pRef;` |
|        - | 12557 | `	/* Allocate a new instance */` |
|  3068456 | 12558 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3068456 | 12559 | `	if( pRef == 0 ){` |
|      ! 0 | 12560 | `		return 0;` |
|        - | 12561 | `	}` |
|        - | 12562 | `	/* Zero the structure */` |
|  3068456 | 12563 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12564 | `	/* Initialize fields */` |
|  3068456 | 12565 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3068456 | 12566 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3068456 | 12567 | `	pRef->nIdx = nIdx;` |
|  3068456 | 12568 | `	return pRef;` |
|  1534229 | 12569 |  |
|        - | 12570 | `/*` |
|        - | 12571 | ` * Default hash function used by the reference table` |
|        - | 12572 | ` * for lookup/insertion operations.` |
|        - | 12573 | ` */` |
| 16932393 | 12574 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12575 |  |
|        - | 12576 | `	/* Calculate the hash based on the memory object index */` |
| 16932395 | 12577 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12578 |  |
|        - | 12579 | `/*` |
|        - | 12580 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12581 | ` * in the reference table.` |
|        - | 12582 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12583 | ` * otherwise.` |
|        - | 12584 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12585 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12586 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12587 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12588 | ` * Refer to the official for more information on this powerful` |
|        - | 12589 | ` * extension.` |
|        - | 12590 | ` */` |
|  9158388 | 12591 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12592 |  |
|        - | 12593 | `	VmRefObj *pRef;` |
|        - | 12594 | `	sxu32 nBucket;` |
|        - | 12595 | `	/* Point to the appropriate bucket */` |
|  9158390 | 12596 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12597 | `	/* Perform the lookup */` |
|  9158390 | 12598 | `	pRef = pVm->apRefObj[nBucket];` |
| 19977962 | 12599 | `	for(;;){` |
| 39948275 | 12600 | `		if( pRef == 0 ){` |
|  3148678 | 12601 | `			break;` |
|        - | 12602 | `		}` |
| 36799599 | 12603 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12604 | `			/* Entry found */` |
|  6009714 | 12605 | `			return pRef;` |
|        - | 12606 | `		}` |
|        - | 12607 | `		/* Point to the next entry */` |
| 30789887 | 12608 | `		pRef = pRef->pNextCollide;` |
|        2 | 12609 | `	}` |
|        - | 12610 | `	/* No such entry,return NULL */` |
|  3148678 | 12611 | `	return 0;` |
|  4579196 | 12612 |  |
|        - | 12613 | `/*` |
|        - | 12614 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12615 | ` *` |
|        - | 12616 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12617 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12618 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12619 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12620 | ` * Refer to the official for more information on this powerful` |
|        - | 12621 | ` * extension.` |
|        - | 12622 | ` */` |
|  3068454 | 12623 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12624 |  |
|        - | 12625 | `	sxu32 nBucket;` |
|  3068456 | 12626 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12627 | `		VmRefObj **apNew;` |
|        - | 12628 | `		sxu32 nNew;` |
|        - | 12629 | `		/* Allocate a larger table */` |
|     4002 | 12630 | `		nNew = pVm->nRefSize << 1;` |
|     4002 | 12631 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4002 | 12632 | `		if( apNew ){` |
|     4002 | 12633 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12634 | `			sxu32 n;` |
|        - | 12635 | `			/* Zero the structure */` |
|     4002 | 12636 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12637 | `			/* Rehash all referenced entries */` |
|  2840880 | 12638 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12639 | `				/* Remove old collision links */` |
|  2836880 | 12640 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12641 | `				/* Point to the appropriate bucket */` |
|  2836880 | 12642 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12643 | `				/* Insert the entry  */` |
|  2836880 | 12644 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2836880 | 12645 | `				if( apNew[nBucket] ){` |
|  2298896 | 12646 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12647 | `				}` |
|  2836880 | 12648 | `				apNew[nBucket] = pEntry;` |
|        - | 12649 | `				/* Point to the next entry */` |
|  2836880 | 12650 | `				pEntry = pEntry->pNext;` |
|  1418441 | 12651 | `			}` |
|        - | 12652 | `			/* Release the old table */` |
|     4002 | 12653 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12654 | `			/* Install the new one */` |
|     4002 | 12655 | `			pVm->apRefObj = apNew;` |
|     4002 | 12656 | `			pVm->nRefSize = nNew;` |
|     2000 | 12657 | `		}` |
|     2000 | 12658 | `	}` |
|        - | 12659 | `	/* Point to the appropriate bucket */` |
|  3068456 | 12660 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12661 | `	/* Insert the entry */` |
|  3068456 | 12662 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3068456 | 12663 | `	if( pVm->apRefObj[nBucket] ){` |
|  2534972 | 12664 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1267686 | 12665 | `	}` |
|  3068456 | 12666 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3068456 | 12667 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3068456 | 12668 | `	pVm->nRefUsed++;` |
|  3068456 | 12669 | `	return SXRET_OK;` |
|        2 | 12670 |  |
|        - | 12671 | `/*` |
|        - | 12672 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12673 | ` * the reference table.` |
|        - | 12674 | ` * This function is invoked when the user perform an unset` |
|        - | 12675 | ` * call [i.e: unset($var); ].` |
|        - | 12676 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12677 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12678 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12679 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12680 | ` * Refer to the official for more information on this powerful` |
|        - | 12681 | ` * extension.` |
|        - | 12682 | ` */` |
|  3034780 | 12683 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12684 |  |
|        - | 12685 | `	ph7_hashmap_node **apNode;` |
|        - | 12686 | `	SyHashEntry **apEntry;` |
|        - | 12687 | `	sxu32 n;` |
|        - | 12688 | `	/* Point to the reference table */` |
|  3034782 | 12689 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3034782 | 12690 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12691 | `	/* Unlink the entry from the reference table */` |
|  3121022 | 12692 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    86242 | 12693 | `		if( apEntry[n] ){` |
|    86192 | 12694 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    43095 | 12695 | `		}` |
|    43122 | 12696 | `	}` |
|  5986086 | 12697 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2951306 | 12698 | `		if( apNode[n] ){` |
|     6946 | 12699 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3472 | 12700 | `		}` |
|  1475654 | 12701 | `	}` |
|  3034782 | 12702 | `	if( pRef->pPrevCollide ){` |
|  1166109 | 12703 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   583402 | 12704 | `	}else{` |
|  1868675 | 12705 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12706 | `	}` |
|  3034782 | 12707 | `	if( pRef->pNextCollide ){` |
|  1723796 | 12708 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   861999 | 12709 | `	}` |
|  3034782 | 12710 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12711 | `	/* Release the node */` |
|  3034782 | 12712 | `	SySetRelease(&pRef->aReference);` |
|  3034782 | 12713 | `	SySetRelease(&pRef->aArrEntries);` |
|  3034782 | 12714 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3034782 | 12715 | `	pVm->nRefUsed--;` |
|  3034782 | 12716 | `	return SXRET_OK;` |
|        2 | 12717 |  |
|        - | 12718 | `/*` |
|        - | 12719 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12720 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12721 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12722 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12723 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12724 | ` * Refer to the official for more information on this powerful` |
|        - | 12725 | ` * extension.` |
|        - | 12726 | ` */` |
|  3098964 | 12727 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12728 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12729 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12730 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12731 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12732 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12733 | `	)` |
|        2 | 12734 |  |
|  3098966 | 12735 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12736 | `	VmRefObj *pRef;` |
|        - | 12737 | `	/* Check if the referenced object already exists */` |
|  3098966 | 12738 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3098966 | 12739 | `	if( pRef == 0 ){` |
|        - | 12740 | `		/* Create a new entry */` |
|  3068456 | 12741 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3068456 | 12742 | `		if( pRef == 0 ){` |
|      ! 0 | 12743 | `			return SXERR_MEM;` |
|        - | 12744 | `		}` |
|  3068456 | 12745 | `		pRef->iFlags = iFlags;` |
|        - | 12746 | `		/* Install the entry */` |
|  3068456 | 12747 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1534227 | 12748 | `	}` |
|  3098966 | 12749 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3098966 | 12750 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12751 | `		VmSlot sRef;` |
|        - | 12752 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12753 | `		 * be deleted when we leave this frame.` |
|        - | 12754 | `		 */` |
|    80302 | 12755 | `		sRef.nIdx = nIdx;` |
|    80302 | 12756 | `		sRef.pUserData = pEntry;` |
|    80302 | 12757 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12758 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12759 | `		}` |
|    40150 | 12760 | `	}` |
|  3098966 | 12761 | `	if( pEntry ){` |
|        - | 12762 | `		/* Address of the hash-entry */` |
|   110620 | 12763 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    55309 | 12764 | `	}` |
|  3098966 | 12765 | `	if( pMapEntry ){` |
|        - | 12766 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2983302 | 12767 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1491650 | 12768 | `	}` |
|  3098966 | 12769 | `	return SXRET_OK;` |
|  1549484 | 12770 |  |
|        - | 12771 | `/*` |
|        - | 12772 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12773 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12774 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12775 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12776 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12777 | ` * Refer to the official for more information on this powerful` |
|        - | 12778 | ` * extension.` |
|        - | 12779 | ` */` |
|  3024638 | 12780 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12781 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12782 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12783 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12784 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12785 | `	)` |
|        2 | 12786 |  |
|        - | 12787 | `	VmRefObj *pRef;` |
|        - | 12788 | `	sxu32 n;` |
|        - | 12789 | `	/* Check if the referenced object already exists */` |
|  3024640 | 12790 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3024640 | 12791 | `	if( pRef == 0 ){` |
|        - | 12792 | `		/* Not such entry */` |
|    80218 | 12793 | `		return SXERR_NOTFOUND;` |
|        - | 12794 | `	}` |
|        - | 12795 | `	/* Remove the desired entry */` |
|  2944424 | 12796 | `	if( pEntry ){` |
|        - | 12797 | `		SyHashEntry **apEntry;` |
|       56 | 12798 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12799 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12800 | `			if( apEntry[n] == pEntry ){` |
|        - | 12801 | `				/* Nullify the entry */` |
|       56 | 12802 | `				apEntry[n] = 0;` |
|        - | 12803 | `				/*` |
|        - | 12804 | `				 * NOTE:` |
|        - | 12805 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12806 | `				 * we avoid wasting spaces.` |
|        - | 12807 | `				 */` |
|       27 | 12808 | `			}` |
|       79 | 12809 | `		}` |
|       27 | 12810 | `	}` |
|  2944424 | 12811 | `	if( pMapEntry ){` |
|        - | 12812 | `		ph7_hashmap_node **apNode;` |
|  2944370 | 12813 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5888832 | 12814 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2944464 | 12815 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12816 | `				/* nullify the entry */` |
|  2944370 | 12817 | `				apNode[n] = 0;` |
|  1472184 | 12818 | `			}` |
|  1472233 | 12819 | `		}` |
|  1472184 | 12820 | `	}` |
|  2944424 | 12821 | `	return SXRET_OK;` |
|  1512321 | 12822 |  |
|        - | 12823 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12824 | `/*` |
|        - | 12825 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12826 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12827 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12828 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12829 | ` * For more information on how to register IO stream devices,please` |
|        - | 12830 | ` * refer to the official documentation.` |
|        - | 12831 | ` */` |
|    24672 | 12832 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12833 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12834 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12835 | `	int nByte              /* *pzDevice length*/` |
|        - | 12836 | `	)` |
|        2 | 12837 |  |
|        - | 12838 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12839 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12840 | `	SyString sDev,sCur;` |
|        - | 12841 | `	sxu32 n,nEntry;` |
|        - | 12842 | `	int rc;` |
|        - | 12843 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    24674 | 12844 | `	zNext = zCur = zIn = *pzDevice;` |
|    24674 | 12845 | `	zEnd = &zIn[nByte];` |
|  1571318 | 12846 | `	while( zIn < zEnd ){` |
|  1546648 | 12847 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12848 | `			/* Got one */` |
|        3 | 12849 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12850 | `			break;` |
|        - | 12851 | `		}` |
|        - | 12852 | `		/* Advance the cursor */` |
|  1546646 | 12853 | `		zIn++;` |
|        2 | 12854 | `	}` |
|    24674 | 12855 | `	if( zIn >= zEnd ){` |
|        - | 12856 | `		/* No such scheme,return the default stream */` |
|    24672 | 12857 | `		return pVm->pDefStream;` |
|        - | 12858 | `	}` |
|        3 | 12859 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12860 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12861 | `	SyStringFullTrim(&sDev);` |
|        - | 12862 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12863 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12864 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12865 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12866 | `		pStream = apStream[n];` |
|        3 | 12867 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12868 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12869 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12870 | `		if( rc == 0 ){` |
|        - | 12871 | `			/* Stream device found */` |
|        3 | 12872 | `			*pzDevice = zNext;` |
|        3 | 12873 | `			return pStream;` |
|        - | 12874 | `		}` |
|      ! 0 | 12875 | `	}` |
|        - | 12876 | `	/* No such stream,return NULL */` |
|      ! 0 | 12877 | `	return 0;` |
|    12338 | 12878 |  |
|        - | 12879 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12880 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12881 |  |
