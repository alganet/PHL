# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5072/6661 lines (76.14%)

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
|   800272 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   800274 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   800240 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   800230 |   104 | `	return FALSE;` |
|   400160 |   105 |  |
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
|   510844 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   510846 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   510846 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   510842 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   510842 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   510842 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   510842 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   510842 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   510842 |   152 | `	pCons->xExpand = xExpand;` |
|   510842 |   153 | `	pCons->pUserData = pUserData;` |
|   510842 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   510842 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   510842 |   161 | `	return SXRET_OK;` |
|   255424 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1123242 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1123244 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1123244 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1123244 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1123244 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1123244 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1123244 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1123244 |   195 | `	pFunc->pVm   = pVm;` |
|  1123244 |   196 | `	pFunc->xFunc = xFunc;` |
|  1123244 |   197 | `	pFunc->pUserData = pUserData;` |
|  1123244 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1123244 |   200 | `	*ppOut = pFunc;` |
|  1123244 |   201 | `	return SXRET_OK;` |
|   561623 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1125596 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1125598 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1125598 |   223 | `	if( pEntry ){` |
|     2356 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2356 |   225 | `		pFunc->pUserData = pUserData;` |
|     2356 |   226 | `		pFunc->xFunc = xFunc;` |
|     2356 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2356 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1123244 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1123244 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1123244 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1123244 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1123244 |   243 | `	return SXRET_OK;` |
|   562800 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   161078 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   161080 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   161080 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   161080 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   161080 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   161080 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   161080 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   161080 |   270 | `	pFunc->iFlags = iFlags;` |
|   161080 |   271 | `	pFunc->pUserData = pUserData;` |
|   161080 |   272 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   161080 |   273 | `	return SXRET_OK;` |
|        2 |   274 |  |
|        - |   275 | `/*` |
|        - |   276 | ` * Namespace-aware function lookup.` |
|        - |   277 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   278 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   279 | ` */` |
|        - |   280 | `/*` |
|        - |   281 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   282 | ` */` |
|   632664 |   283 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   284 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   285 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   286 | `	SyString *pName     /* Function name */` |
|        - |   287 | `	)` |
|        2 |   288 |  |
|        - |   289 | `	SyHashEntry *pEntry;` |
|        - |   290 | `	sxi32 rc;` |
|   632666 |   291 | `	if( pName == 0 ){` |
|        - |   292 | `		/* Use the built-in name */` |
|    34742 |   293 | `		pName = &pFunc->sName;` |
|    17370 |   294 | `	}` |
|        - |   295 | `	/* Check for duplicates (functions with the same name) first */` |
|   632666 |   296 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   632666 |   297 | `	if( pEntry ){` |
|   492828 |   298 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   492828 |   299 | `		if( pLink != pFunc ){` |
|        - |   300 | `			/* Link */` |
|      184 |   301 | `			pFunc->pNextName = pLink;` |
|      184 |   302 | `			pEntry->pUserData = pFunc;` |
|       91 |   303 | `		}` |
|   492828 |   304 | `		return SXRET_OK;` |
|        - |   305 | `	}` |
|        - |   306 | `	/* First time seen */` |
|   139840 |   307 | `	pFunc->pNextName = 0;` |
|   139840 |   308 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   139840 |   309 | `	return rc;` |
|   316334 |   310 |  |
|        - |   311 | `/*` |
|        - |   312 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   313 | ` */` |
|    45128 |   314 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   315 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   316 | `	ph7_class *pClass /* Target Class */` |
|        - |   317 | `	)` |
|        2 |   318 |  |
|    45130 |   319 | `	SyString *pName = &pClass->sName;` |
|        - |   320 | `	SyHashEntry *pEntry;` |
|        - |   321 | `	sxi32 rc;` |
|        - |   322 | `	/* Check for duplicates */` |
|    45130 |   323 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    45130 |   324 | `	if( pEntry ){` |
|       31 |   325 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   326 | `		/* Link entry with the same name */` |
|       31 |   327 | `		pClass->pNextName = pLink;` |
|       31 |   328 | `		pEntry->pUserData = pClass;` |
|       31 |   329 | `		return SXRET_OK;` |
|        - |   330 | `	}` |
|    45100 |   331 | `	pClass->pNextName = 0;` |
|        - |   332 | `	/* Perform a simple hashtable insertion */` |
|    45100 |   333 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    45100 |   334 | `	return rc;` |
|    22566 |   335 |  |
|        - |   336 | `/*` |
|        - |   337 | ` * Instruction builder interface.` |
|        - |   338 | ` */` |
|  3251650 |   339 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3251652 |   351 | `	sInstr.iOp = (sxu8)iOp;` |
|  3251652 |   352 | `	sInstr.iP1 = iP1;` |
|  3251652 |   353 | `	sInstr.iP2 = iP2;` |
|  3251652 |   354 | `	sInstr.p3  = p3;` |
|  3251652 |   355 | `	if( pIndex ){` |
|        - |   356 | `		/* Instruction index in the bytecode array */` |
|   187554 |   357 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    93776 |   358 | `	}` |
|        - |   359 | `	/* Finally,record the instruction */` |
|  3251652 |   360 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3251652 |   361 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   362 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   363 | `		/* Fall throw */` |
|      ! 0 |   364 | `	}` |
|  3251652 |   365 | `	return rc;` |
|        2 |   366 |  |
|        - |   367 | `/*` |
|        - |   368 | ` * Swap the current bytecode container with the given one.` |
|        - |   369 | ` */` |
|   385804 |   370 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   371 |  |
|   385806 |   372 | `	if( pContainer == 0 ){` |
|        - |   373 | `		/* Point to the default container */` |
|      ! 0 |   374 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   375 | `	}else{` |
|        - |   376 | `		/* Change container */` |
|   385806 |   377 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   378 | `	}` |
|   385806 |   379 | `	return SXRET_OK;` |
|        2 |   380 |  |
|        - |   381 | `/*` |
|        - |   382 | ` * Return the current bytecode container.` |
|        - |   383 | ` */` |
|   192902 |   384 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   385 |  |
|   192904 |   386 | `	return pVm->pByteContainer;` |
|        2 |   387 |  |
|        - |   388 | `/*` |
|        - |   389 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   390 | ` */` |
|   184854 |   391 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   392 |  |
|        - |   393 | `	VmInstr *pInstr;` |
|   184856 |   394 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   184856 |   395 | `	return pInstr;` |
|        2 |   396 |  |
|        - |   397 | `/*` |
|        - |   398 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   399 | ` */` |
|   974528 |   400 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   401 |  |
|   974530 |   402 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   403 |  |
|        - |   404 | `/*` |
|        - |   405 | ` * Pop the last VM instruction.` |
|        - |   406 | ` */` |
|   175716 |   407 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   408 |  |
|   175718 |   409 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   410 |  |
|        - |   411 | `/*` |
|        - |   412 | ` * Peek the last VM instruction.` |
|        - |   413 | ` */` |
|   629780 |   414 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   415 |  |
|   629782 |   416 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   417 |  |
|    27036 |   418 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   419 |  |
|        - |   420 | `	VmInstr *aInstr;` |
|        - |   421 | `	sxu32 n;` |
|    27038 |   422 | `	n = SySetUsed(pVm->pByteContainer);` |
|    27038 |   423 | `	if( n < 2 ){` |
|      ! 0 |   424 | `		return 0;` |
|        - |   425 | `	}` |
|    27038 |   426 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    27038 |   427 | `	return &aInstr[n - 2];` |
|    13520 |   428 |  |
|        - |   429 | `/*` |
|        - |   430 | ` * Allocate a new virtual machine frame.` |
|        - |   431 | ` */` |
|    16372 |   432 | `static VmFrame * VmNewFrame(` |
|        - |   433 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   434 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   435 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   436 | `	)` |
|        2 |   437 |  |
|        - |   438 | `	VmFrame *pFrame;` |
|        - |   439 | `	/* Allocate a new vm frame */` |
|    16374 |   440 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    16374 |   441 | `	if( pFrame == 0 ){` |
|      ! 0 |   442 | `		return 0;` |
|        - |   443 | `	}` |
|        - |   444 | `	/* Zero the structure */` |
|    16374 |   445 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   446 | `	/* Initialize frame fields */` |
|    16374 |   447 | `	pFrame->pUserData = pUserData;` |
|    16374 |   448 | `	pFrame->pThis = pThis;` |
|    16374 |   449 | `	pFrame->pVm = pVm;` |
|    16374 |   450 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    16374 |   451 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    16374 |   452 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    16374 |   453 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    16374 |   454 | `	return pFrame;` |
|     8188 |   455 |  |
|        - |   456 | `/* Forward declaration */` |
|        - |   457 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   458 | `/*` |
|        - |   459 | ` * Enter a VM frame.` |
|        - |   460 | ` */` |
|    16330 |   461 | `static sxi32 VmEnterFrame(` |
|        - |   462 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   463 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   464 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   465 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   466 | `	)` |
|        2 |   467 |  |
|        - |   468 | `	VmFrame *pFrame;` |
|        - |   469 | `	/* Allocate a new frame */` |
|    16332 |   470 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    16332 |   471 | `	if( pFrame == 0 ){` |
|      ! 0 |   472 | `		return SXERR_MEM;` |
|        - |   473 | `	}` |
|        - |   474 | `	/* Link to the list of active VM frame */` |
|    16332 |   475 | `	pFrame->pParent = pVm->pFrame;` |
|    16332 |   476 | `	pVm->pFrame = pFrame;` |
|    16332 |   477 | `	if( ppFrame ){` |
|        - |   478 | `		/* Write a pointer to the new VM frame */` |
|    13710 |   479 | `		*ppFrame = pFrame;` |
|     6854 |   480 | `	}` |
|    16332 |   481 | `	return SXRET_OK;` |
|     8167 |   482 |  |
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
|    13708 |   526 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   527 |  |
|    13710 |   528 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    13710 |   529 | `	if( pCurFrame ){` |
|        - |   530 | `		/* Unlink from the list of active VM frame */` |
|    13710 |   531 | `		pVm->pFrame = pCurFrame->pParent;` |
|    13710 |   532 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   533 | `			VmSlot  *aSlot;` |
|        - |   534 | `			sxu32 n;` |
|        - |   535 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    13622 |   536 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    94468 |   537 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   538 | `				/* Unset the local variable */` |
|    80848 |   539 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    40425 |   540 | `			}` |
|        - |   541 | `			/* Remove local reference */` |
|    13622 |   542 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    94524 |   543 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    80904 |   544 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    40453 |   545 | `			}` |
|     6810 |   546 | `		}` |
|        - |   547 | `		/* Release internal containers */` |
|    13710 |   548 | `		SyHashRelease(&pCurFrame->hVar);` |
|    13710 |   549 | `		SySetRelease(&pCurFrame->sArg);` |
|    13710 |   550 | `		SySetRelease(&pCurFrame->sLocal);` |
|    13710 |   551 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   552 | `		/* Release the whole structure */` |
|    13710 |   553 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6854 |   554 | `	}` |
|    13710 |   555 |  |
|        - |   556 | `/*` |
|        - |   557 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   558 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   559 | ` * should be skipped when looking for the real execution context.` |
|        - |   560 | ` */` |
|  6446332 |   561 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   562 |  |
|  6446808 |   563 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      476 |   564 | `		pFrame = pFrame->pParent;` |
|        2 |   565 | `	}` |
|  6446334 |   566 | `	return pFrame;` |
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
|   123474 |   684 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   685 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   686 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	ph7_class_method *pMeth;` |
|        - |   690 | `	ph7_class_attr *pAttr;` |
|        - |   691 | `	SyHashEntry *pEntry;` |
|        - |   692 | `	sxi32 rc;` |
|        - |   693 | `	/* Reset the loop cursor */` |
|   123476 |   694 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   695 | `	/* Process only static and constant attribute */` |
|   520373 |   696 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   697 | `		/* Extract the current attribute */` |
|   335162 |   698 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   335162 |   699 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   700 | `			ph7_value *pMemObj;` |
|        - |   701 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1328 |   702 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1328 |   703 | `			if( pMemObj == 0 ){` |
|      ! 0 |   704 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   705 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   706 | `					&pClass->sName,&pAttr->sName` |
|        - |   707 | `					);` |
|      ! 0 |   708 | `				return SXERR_MEM;` |
|        - |   709 | `			}` |
|     1328 |   710 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   711 | `				/* Initialize attribute default value (any complex expression) */` |
|     1328 |   712 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      663 |   713 | `			}` |
|        - |   714 | `			/* Record attribute index */` |
|     1328 |   715 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   716 | `			/* Install static attribute in the reference table */` |
|     1328 |   717 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      663 |   718 | `		}` |
|        2 |   719 | `	}` |
|        - |   720 | `	/* Install class methods */` |
|   123476 |   721 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   722 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   723 | `		 */` |
|    53398 |   724 | `		return SXRET_OK;` |
|        - |   725 | `	}` |
|        - |   726 | `	/* Create constructor alias if not yet done */` |
|    70080 |   727 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   728 | `		/* User constructor with the same base class name */` |
|     5314 |   729 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5314 |   730 | `		if( pEntry ){` |
|      ! 0 |   731 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   732 | `			/* Create the alias */` |
|      ! 0 |   733 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   734 | `		}` |
|     2656 |   735 | `	}` |
|        - |   736 | `	/* Install the methods now */` |
|    70080 |   737 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   703051 |   738 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   597934 |   739 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   597934 |   740 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   597926 |   741 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   597926 |   742 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   743 | `				return rc;` |
|        - |   744 | `			}` |
|   298962 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    70080 |   748 | `	pClass->bMounted = TRUE;` |
|    70080 |   749 | `	return SXRET_OK;` |
|    61739 |   750 |  |
|        - |   751 | `/*` |
|        - |   752 | ` * Allocate a private frame for attributes of the given` |
|        - |   753 | ` * class instance (Object in the PHP jargon).` |
|        - |   754 | ` */` |
|     1250 |   755 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   756 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   757 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   758 | `	)` |
|        2 |   759 |  |
|     1252 |   760 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   761 | `	ph7_class_attr *pAttr;` |
|        - |   762 | `	SyHashEntry *pEntry;` |
|        - |   763 | `	sxi32 rc;` |
|        - |   764 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1252 |   765 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     5164 |   766 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   767 | `		VmClassAttr *pVmAttr;` |
|        - |   768 | `		/* Extract the current attribute */` |
|     3914 |   769 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3914 |   770 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3914 |   771 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   772 | `			return SXERR_MEM;` |
|        - |   773 | `		}` |
|     3914 |   774 | `		pVmAttr->pAttr = pAttr;` |
|     3914 |   775 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   776 | `			ph7_value *pMemObj;` |
|        - |   777 | `			/* Reserve a memory object for this attribute */` |
|     3890 |   778 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3890 |   779 | `			if( pMemObj == 0 ){` |
|      ! 0 |   780 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   781 | `				return SXERR_MEM;` |
|        - |   782 | `			}` |
|     3890 |   783 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3890 |   784 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   785 | `				/* Initialize attribute default value (any complex expression) */` |
|     1258 |   786 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      628 |   787 | `			}` |
|     3890 |   788 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3890 |   789 | `			if( rc != SXRET_OK ){` |
|        - |   790 | `				VmSlot sSlot;` |
|        - |   791 | `				/* Restore memory object */` |
|      ! 0 |   792 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   793 | `				sSlot.pUserData = 0;` |
|      ! 0 |   794 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `			/* Install attribute in the reference table */` |
|     3890 |   799 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1946 |   800 | `		}else{` |
|        - |   801 | `			/* Install static/constant attribute */` |
|       26 |   802 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   803 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   804 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   805 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   806 | `				return SXERR_MEM;` |
|        - |   807 | `			}` |
|        - |   808 | `		}` |
|        2 |   809 | `	}` |
|     1252 |   810 | `	return SXRET_OK;` |
|      627 |   811 |  |
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
|   373092 |   823 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   824 |  |
|        - |   825 | `	ph7_value *pObj;` |
|        - |   826 | `	sxi32 rc;` |
|   373094 |   827 | `	if( pIndex ){` |
|        - |   828 | `		/* Object index in the object table */` |
|   365228 |   829 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   182613 |   830 | `	}` |
|        - |   831 | `	/* Reserve a slot for the new object */` |
|   373094 |   832 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   373094 |   833 | `	if( rc != SXRET_OK ){` |
|        - |   834 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   835 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   836 | `		 */` |
|      ! 0 |   837 | `		return 0;` |
|        - |   838 | `	}` |
|   373094 |   839 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   373094 |   840 | `	return pObj;` |
|   186548 |   841 |  |
|        - |   842 | `/*` |
|        - |   843 | ` * Reserve a memory object.` |
|        - |   844 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   845 | ` */` |
|  2142738 |   846 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   847 |  |
|        - |   848 | `	ph7_value *pObj;` |
|        - |   849 | `	sxi32 rc;` |
|  2142740 |   850 | `	if( pIndex ){` |
|        - |   851 | `		/* Object index in the object table */` |
|  2142740 |   852 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1071369 |   853 | `	}` |
|        - |   854 | `	/* Reserve a slot for the new object */` |
|  2142740 |   855 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2142740 |   856 | `	if( rc != SXRET_OK ){` |
|        - |   857 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   858 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   859 | `		 */` |
|      ! 0 |   860 | `		return 0;` |
|        - |   861 | `	}` |
|  2142740 |   862 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2142740 |   863 | `	return pObj;` |
|  1071371 |   864 |  |
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
|     2622 |  1274 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1275 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1276 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1277 | `	 )` |
|        2 |  1278 |  |
|        - |  1279 | `	SyString sBuiltin;` |
|        - |  1280 | `	ph7_value *pObj;` |
|        - |  1281 | `	sxi32 rc;` |
|        - |  1282 | `	/* Zero the structure */` |
|     2624 |  1283 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1284 | `	/* Initialize VM fields */` |
|     2624 |  1285 | `	pVm->pEngine = &(*pEngine);` |
|     2624 |  1286 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1287 | `	/* Instructions containers */` |
|     2624 |  1288 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2624 |  1289 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2624 |  1290 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1291 | `	/* Object containers */` |
|     2624 |  1292 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2624 |  1293 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1294 | `	/* Virtual machine internal containers */` |
|     2624 |  1295 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2624 |  1296 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2624 |  1297 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2624 |  1298 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2624 |  1299 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2624 |  1300 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2624 |  1301 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2624 |  1302 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2624 |  1303 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2624 |  1304 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2624 |  1305 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2624 |  1306 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2624 |  1307 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2624 |  1308 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2624 |  1309 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2624 |  1310 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2624 |  1311 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2624 |  1312 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2624 |  1313 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2624 |  1314 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2624 |  1315 | `	pVm->pPendingException = 0;` |
|        - |  1316 | `	/* Configuration containers */` |
|     2624 |  1317 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2624 |  1318 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2624 |  1319 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2624 |  1320 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2624 |  1321 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2624 |  1322 | `	pVm->iResponseStatus = 200;` |
|     2624 |  1323 | `	pVm->bHeadersSent = 0;` |
|     2624 |  1324 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1325 | `	/* Error callbacks containers */` |
|     2624 |  1326 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2624 |  1327 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2624 |  1328 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2624 |  1329 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2624 |  1330 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1331 | `	/* Set a default recursion limit */` |
|        - |  1332 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2624 |  1333 | `	pVm->nMaxDepth = 32;` |
|        - |  1334 | `#else` |
|        - |  1335 | `	pVm->nMaxDepth = 16;` |
|        - |  1336 | `#endif` |
|        - |  1337 | `	/* Default assertion flags */` |
|     2624 |  1338 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1339 | `	/* JSON return status */` |
|     2624 |  1340 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1341 | `	/* PRNG context */` |
|     2624 |  1342 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1343 | `	/* Install the null constant */` |
|     2624 |  1344 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2624 |  1345 | `	if( pObj == 0 ){` |
|      ! 0 |  1346 | `		rc = SXERR_MEM;` |
|      ! 0 |  1347 | `		goto Err;` |
|        - |  1348 | `	}` |
|     2624 |  1349 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1350 | `	/* Install the boolean TRUE constant */` |
|     2624 |  1351 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2624 |  1352 | `	if( pObj == 0 ){` |
|      ! 0 |  1353 | `		rc = SXERR_MEM;` |
|      ! 0 |  1354 | `		goto Err;` |
|        - |  1355 | `	}` |
|     2624 |  1356 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1357 | `	/* Install the boolean FALSE constant */` |
|     2624 |  1358 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2624 |  1359 | `	if( pObj == 0 ){` |
|      ! 0 |  1360 | `		rc = SXERR_MEM;` |
|      ! 0 |  1361 | `		goto Err;` |
|        - |  1362 | `	}` |
|     2624 |  1363 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1364 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1365 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1366 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2624 |  1367 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2624 |  1368 | `	if( pObj == 0 ){` |
|      ! 0 |  1369 | `		rc = SXERR_MEM;` |
|      ! 0 |  1370 | `		goto Err;` |
|        - |  1371 | `	}` |
|     2624 |  1372 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1373 | `	/* Create the global frame */` |
|     2624 |  1374 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2624 |  1375 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1376 | `		goto Err;` |
|        - |  1377 | `	}` |
|        - |  1378 | `	/* Initialize the code generator */` |
|     2624 |  1379 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2624 |  1380 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1381 | `		goto Err;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* VM correctly initialized,set the magic number */` |
|     2624 |  1384 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2624 |  1385 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1386 | `	/* Compile the built-in library */` |
|     2624 |  1387 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1388 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2624 |  1389 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1390 | `	/* Register Fiber internal C functions */` |
|     2624 |  1391 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2624 |  1392 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2624 |  1393 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2624 |  1394 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2624 |  1395 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2624 |  1396 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2624 |  1397 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2624 |  1398 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2624 |  1399 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2624 |  1400 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1401 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2624 |  1402 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2624 |  1403 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2624 |  1404 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2624 |  1405 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2624 |  1406 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2624 |  1407 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2624 |  1408 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2624 |  1409 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2624 |  1410 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2624 |  1411 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1412 | `	/* Reset the code generator */` |
|     2624 |  1413 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2624 |  1414 | `	return SXRET_OK;` |
|      ! 0 |  1415 | `Err:` |
|      ! 0 |  1416 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1417 | `	return rc;` |
|     1313 |  1418 |  |
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
|    14336 |  1445 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1446 |  |
|    14338 |  1447 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    14338 |  1448 | `	if( xCons != VmObConsumer ){` |
|     6366 |  1449 | `		pVm->nOutputLen += nLen;` |
|     6366 |  1450 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      834 |  1451 | `			pVm->bHeadersSent = 1;` |
|      416 |  1452 | `		}` |
|     3182 |  1453 | `	}` |
|    14338 |  1454 |  |
|        - |  1455 | `#define VM_STACK_GUARD 16` |
|        - |  1456 | `/*` |
|        - |  1457 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1458 | ` * our compiled PHP program.` |
|        - |  1459 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1460 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1461 | ` */` |
|    33582 |  1462 | `static ph7_value * VmNewOperandStack(` |
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
|    33584 |  1475 | `	nInstr += VM_STACK_GUARD;` |
|    33584 |  1476 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    33584 |  1477 | `	if( pStack == 0 ){` |
|      ! 0 |  1478 | `		return 0;` |
|        - |  1479 | `	}` |
|        - |  1480 | `	/* Initialize the operand stack */` |
|  2096270 |  1481 | `	while( nInstr > 0 ){` |
|  2062688 |  1482 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2062688 |  1483 | `		--nInstr;` |
|        2 |  1484 | `	}` |
|        - |  1485 | `	/* Ready for bytecode execution */` |
|    33584 |  1486 | `	return pStack;` |
|    16793 |  1487 |  |
|        - |  1488 | `/* Forward declaration */` |
|        - |  1489 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1490 | `/*` |
|        - |  1491 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1492 | ` * This routine gets called by the PH7 engine after` |
|        - |  1493 | ` * successful compilation of the target PHP program.` |
|        - |  1494 | ` */` |
|     2354 |  1495 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1496 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1497 | `	)` |
|        2 |  1498 |  |
|        - |  1499 | `	SyHashEntry *pEntry;` |
|        - |  1500 | `	sxi32 rc;` |
|     2356 |  1501 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1502 | `		/* Initialize your VM first */` |
|      ! 0 |  1503 | `		return SXERR_CORRUPT;` |
|        - |  1504 | `	}` |
|        - |  1505 | `	/* Mark the VM ready for byte-code execution */` |
|     2356 |  1506 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1507 | `	/* Release the code generator now we have compiled our program */` |
|     2356 |  1508 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1509 | `	/* Emit the DONE instruction */` |
|     2356 |  1510 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2356 |  1511 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1512 | `		return SXERR_MEM;` |
|        - |  1513 | `	}` |
|        - |  1514 | `	/* Script return value */` |
|     2356 |  1515 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1516 | `	/* Allocate a new operand stack */` |
|     2356 |  1517 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2356 |  1518 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1519 | `		return SXERR_MEM;` |
|        - |  1520 | `	}` |
|        - |  1521 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1522 | `	 * private data. */` |
|     2356 |  1523 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2356 |  1524 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1525 | `	/* Allocate the reference table */` |
|     2356 |  1526 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2356 |  1527 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2356 |  1528 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1529 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1530 | `		return SXERR_MEM;` |
|        - |  1531 | `	}` |
|        - |  1532 | `	/* Zero the reference table */` |
|     2356 |  1533 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1534 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2356 |  1535 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2356 |  1536 | `	if( rc != SXRET_OK ){` |
|        - |  1537 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1538 | `		return rc;` |
|        - |  1539 | `	}` |
|        - |  1540 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2356 |  1541 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2356 |  1542 | `	if( rc != SXRET_OK ){` |
|        - |  1543 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1544 | `		return rc;` |
|        - |  1545 | `	}` |
|        - |  1546 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2356 |  1547 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1548 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2356 |  1549 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1550 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2356 |  1551 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1552 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1553 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2356 |  1554 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2356 |  1555 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1556 | `#endif` |
|        - |  1557 | `	/* Initialize and install static and constants class attributes */` |
|     2356 |  1558 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    42556 |  1559 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    40202 |  1560 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    40202 |  1561 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1562 | `			return rc;` |
|        - |  1563 | `		}` |
|        2 |  1564 | `	}` |
|        - |  1565 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2356 |  1566 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1567 | `	/* VM is ready for bytecode execution */` |
|     2356 |  1568 | `	return SXRET_OK;` |
|     1179 |  1569 |  |
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
|     2346 |  1594 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1595 |  |
|        - |  1596 | `	/* Set the stale magic number */` |
|     2348 |  1597 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1598 | `	/* Release the private memory subsystem */` |
|     2348 |  1599 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2348 |  1600 | `	return SXRET_OK;` |
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
|   589836 |  1612 | `static sxi32 VmInitCallContext(` |
|        - |  1613 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1614 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1615 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1616 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1617 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1618 | `	)` |
|        2 |  1619 |  |
|   589838 |  1620 | `	pOut->pFunc = pFunc;` |
|   589838 |  1621 | `	pOut->pVm   = pVm;` |
|   589838 |  1622 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   589838 |  1623 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1624 | `	/* Assume a null return value */` |
|   589838 |  1625 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   589838 |  1626 | `	pOut->pRet = pRet;` |
|   589838 |  1627 | `	pOut->iFlags = iFlags;` |
|   589838 |  1628 | `	return SXRET_OK;` |
|        2 |  1629 |  |
|        - |  1630 | `/*` |
|        - |  1631 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1632 | ` * left behind.` |
|        - |  1633 | ` */` |
|   589836 |  1634 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1635 |  |
|        - |  1636 | `	sxu32 n;` |
|   589838 |  1637 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7178 |  1638 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    20522 |  1639 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    13346 |  1640 | `			if( apObj[n] == 0 ){` |
|        - |  1641 | `				/* Already released */` |
|      298 |  1642 | `				continue;` |
|        - |  1643 | `			}` |
|    13050 |  1644 | `			PH7_MemObjRelease(apObj[n]);` |
|    13050 |  1645 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6526 |  1646 | `		}` |
|     7178 |  1647 | `		SySetRelease(&pCtx->sVar);` |
|     3588 |  1648 | `	}` |
|   589838 |  1649 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   589838 |  1665 |  |
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
|  3406670 |  1696 | `static void VmPopOperand(` |
|        - |  1697 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1698 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1699 | `	)` |
|        2 |  1700 |  |
|  3406672 |  1701 | `	ph7_value *pTos = *ppTos;` |
|  7243498 |  1702 | `	while( nPop > 0 ){` |
|  3836828 |  1703 | `		PH7_MemObjRelease(pTos);` |
|  3836828 |  1704 | `		pTos--;` |
|  3836828 |  1705 | `		nPop--;` |
|        2 |  1706 | `	}` |
|        - |  1707 | `	/* Top of the stack */` |
|  3406672 |  1708 | `	*ppTos = pTos;` |
|  3406672 |  1709 |  |
|        - |  1710 | `/*` |
|        - |  1711 | ` * Reserve a memory object.` |
|        - |  1712 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1713 | ` */` |
|  3074126 |  1714 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1715 |  |
|  3074128 |  1716 | `	ph7_value *pObj = 0;` |
|        - |  1717 | `	VmSlot *pSlot;` |
|        - |  1718 | `	sxu32 nIdx;` |
|        - |  1719 | `	/* Check for a free slot */` |
|  3074128 |  1720 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3074128 |  1721 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3074128 |  1722 | `	if( pSlot ){` |
|   931390 |  1723 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   931390 |  1724 | `		nIdx = pSlot->nIdx;` |
|   465694 |  1725 | `	}` |
|  3074128 |  1726 | `	if( pObj == 0 ){` |
|        - |  1727 | `		/* Reserve a new memory object */` |
|  2142740 |  1728 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2142740 |  1729 | `		if( pObj == 0 ){` |
|      ! 0 |  1730 | `			return 0;` |
|        - |  1731 | `		}` |
|  1071369 |  1732 | `	}` |
|        - |  1733 | `	/* Set a null default value */` |
|  3074128 |  1734 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3074128 |  1735 | `	pObj->nIdx = nIdx;` |
|  3074128 |  1736 | `	return pObj;` |
|  1537065 |  1737 |  |
|        - |  1738 | `/*` |
|        - |  1739 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1740 | ` */` |
|    30476 |  1741 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1742 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1743 | `	const char *zKey,  /* Entry key */` |
|        - |  1744 | `	sxu32 nByte,       /* Key length */` |
|        - |  1745 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1746 | `	)` |
|        2 |  1747 |  |
|        - |  1748 | `	ph7_value sKey;` |
|        - |  1749 | `	sxi32 rc;` |
|    30478 |  1750 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    30478 |  1751 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1752 | `	/* Perform the insertion */` |
|    30478 |  1753 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    30478 |  1754 | `	PH7_MemObjRelease(&sKey);` |
|    30478 |  1755 | `	return rc;` |
|        2 |  1756 |  |
|        - |  1757 | `/*` |
|        - |  1758 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1759 | ` * Return a pointer to the variable value on success.` |
|        - |  1760 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1761 | ` */` |
|  3172716 |  1762 | `static ph7_value * VmExtractMemObj(` |
|        - |  1763 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1764 | `	const SyString *pName, /* Variable name */` |
|        - |  1765 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1766 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1767 | `	)` |
|        2 |  1768 |  |
|  3172718 |  1769 | `	int bNullify = FALSE;` |
|        - |  1770 | `	SyHashEntry *pEntry;` |
|        - |  1771 | `	VmFrame *pFrame;` |
|        - |  1772 | `	ph7_value *pObj;` |
|        - |  1773 | `	sxu32 nIdx;` |
|        - |  1774 | `	sxi32 rc;` |
|        - |  1775 | `	/* Point to the top active frame */` |
|  3172718 |  1776 | `	pFrame = pVm->pFrame;` |
|  3172718 |  1777 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1778 | `	/* Perform the lookup */` |
|  3172718 |  1779 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1780 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1781 | `		pName = &sAnnon;` |
|        - |  1782 | `		/* Always nullify the object */` |
|      ! 0 |  1783 | `		bNullify = TRUE;` |
|      ! 0 |  1784 | `		bDup = FALSE;` |
|      ! 0 |  1785 | `	}` |
|        - |  1786 | `	/* Check the superglobals table first */` |
|  3172718 |  1787 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3172718 |  1788 | `	if( pEntry == 0 ){` |
|        - |  1789 | `		/* Query the top active frame */` |
|  3172678 |  1790 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3172678 |  1791 | `		if( pEntry == 0 ){` |
|    87830 |  1792 | `			char *zName = (char *)pName->zString;` |
|        - |  1793 | `			VmSlot sLocal;` |
|    87830 |  1794 | `			if( !bCreate ){` |
|        - |  1795 | `				/* Do not create the variable,return NULL instead */` |
|       38 |  1796 | `				return 0;` |
|        - |  1797 | `			}` |
|        - |  1798 | `			/* No such variable,automatically create a new one and install` |
|        - |  1799 | `			 * it in the current frame.` |
|        - |  1800 | `			 */` |
|    87794 |  1801 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    87794 |  1802 | `			if( pObj == 0 ){` |
|      ! 0 |  1803 | `				return 0;` |
|        - |  1804 | `			}` |
|    87794 |  1805 | `			nIdx = pObj->nIdx;` |
|    87794 |  1806 | `			if( bDup ){` |
|        - |  1807 | `				/* Duplicate name */` |
|      168 |  1808 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      168 |  1809 | `				if( zName == 0 ){` |
|      ! 0 |  1810 | `					return 0;` |
|        - |  1811 | `				}` |
|       83 |  1812 | `			}` |
|        - |  1813 | `			/* Link to the top active VM frame */` |
|    87794 |  1814 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    87794 |  1815 | `			if( rc != SXRET_OK ){` |
|        - |  1816 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1817 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1818 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1819 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1820 | `				return 0;` |
|        - |  1821 | `			}` |
|    87794 |  1822 | `			if( pFrame->pParent != 0 ){` |
|        - |  1823 | `				/* Local variable */` |
|    80884 |  1824 | `				sLocal.nIdx = nIdx;` |
|    80884 |  1825 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    40443 |  1826 | `			}else{` |
|        - |  1827 | `				/* Register in the $GLOBALS array */` |
|     6912 |  1828 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1829 | `			}` |
|        - |  1830 | `			/* Install in the reference table */` |
|    87794 |  1831 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1832 | `			/* Save object index */` |
|    87794 |  1833 | `			pObj->nIdx = nIdx;` |
|    43898 |  1834 | `		}else{` |
|        - |  1835 | `			/* Extract variable contents */` |
|  3084850 |  1836 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3084850 |  1837 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3084850 |  1838 | `			if( bNullify && pObj ){` |
|      ! 0 |  1839 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1840 | `			}` |
|        - |  1841 | `		}` |
|  1586432 |  1842 | `	}else{` |
|        - |  1843 | `		/* Superglobal */` |
|       42 |  1844 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1845 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1846 | `	}` |
|  3172682 |  1847 | `	return pObj;` |
|  1586470 |  1848 |  |
|        - |  1849 | `/*` |
|        - |  1850 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1851 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1852 | ` */` |
|     2658 |  1853 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1854 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1855 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1856 | `	sxu32 nByte        /* zName length */` |
|        - |  1857 | `	)` |
|        2 |  1858 |  |
|        - |  1859 | `	SyHashEntry *pEntry;` |
|        - |  1860 | `	ph7_value *pValue;` |
|        - |  1861 | `	sxu32 nIdx;` |
|        - |  1862 | `	/* Query the superglobal table */` |
|     2660 |  1863 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2660 |  1864 | `	if( pEntry == 0 ){` |
|        - |  1865 | `		/* No such entry */` |
|      ! 0 |  1866 | `		return 0;` |
|        - |  1867 | `	}` |
|        - |  1868 | `	/* Extract the superglobal index in the global object pool */` |
|     2660 |  1869 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1870 | `	/* Extract the variable value  */` |
|     2660 |  1871 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2660 |  1872 | `	return pValue;` |
|     1331 |  1873 |  |
|        - |  1874 | `/*` |
|        - |  1875 | ` * Perform a raw hashmap insertion.` |
|        - |  1876 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1877 | ` */` |
|     2688 |  1878 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1879 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1880 | `	const char *zKey,   /* Entry key */` |
|        - |  1881 | `	int nKeylen,        /* zKey length*/` |
|        - |  1882 | `	const char *zData,  /* Entry data */` |
|        - |  1883 | `	int nLen            /* zData length */` |
|        - |  1884 | `	)` |
|        2 |  1885 |  |
|        - |  1886 | `	ph7_value sKey,sValue;` |
|        - |  1887 | `	sxi32 rc;` |
|     2690 |  1888 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2690 |  1889 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2690 |  1890 | `	if( zKey ){` |
|     2668 |  1891 | `		if( nKeylen < 0 ){` |
|     2616 |  1892 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1307 |  1893 | `		}` |
|     2668 |  1894 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1333 |  1895 | `	}` |
|     2690 |  1896 | `	if( zData ){` |
|     2690 |  1897 | `		if( nLen < 0 ){` |
|        - |  1898 | `			/* Compute length automatically */` |
|      144 |  1899 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1900 | `		}` |
|     2690 |  1901 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1344 |  1902 | `	}` |
|        - |  1903 | `	/* Perform the insertion */` |
|     2690 |  1904 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2690 |  1905 | `	PH7_MemObjRelease(&sKey);` |
|     2690 |  1906 | `	PH7_MemObjRelease(&sValue);` |
|     2690 |  1907 | `	return rc;` |
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
|    37994 |  1922 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1923 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1924 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1925 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1926 | `	)` |
|        2 |  1927 |  |
|    37996 |  1928 | `	sxi32 rc = SXRET_OK;` |
|    37996 |  1929 | `	switch(nOp){` |
|     1169 |  1930 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2340 |  1931 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2340 |  1932 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1933 | `		/* VM output consumer callback */` |
|        - |  1934 | `#ifdef UNTRUST` |
|        - |  1935 | `		if( xConsumer == 0 ){` |
|        - |  1936 | `			rc = SXERR_CORRUPT;` |
|        - |  1937 | `			break;` |
|        - |  1938 | `		}` |
|        - |  1939 | `#endif` |
|        - |  1940 | `		/* Install the output consumer */` |
|     2340 |  1941 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2340 |  1942 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2340 |  1943 | `		break;` |
|        - |  1944 | `							   }` |
|     1177 |  1945 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1946 | `		/* Import path */` |
|        - |  1947 | `		  const char *zPath;` |
|        - |  1948 | `		  SyString sPath;` |
|     2356 |  1949 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1950 | `#if defined(UNTRUST)` |
|        - |  1951 | `		  if( zPath == 0 ){` |
|        - |  1952 | `			  rc = SXERR_EMPTY;` |
|        - |  1953 | `			  break;` |
|        - |  1954 | `		  }` |
|        - |  1955 | `#endif` |
|     2356 |  1956 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1957 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1958 | `#ifdef __WINNT__` |
|        2 |  1959 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1960 | `#endif` |
|     4710 |  1961 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1962 | `		  /* Remove leading and trailing white spaces */` |
|     2356 |  1963 | `		  SyStringFullTrim(&sPath);` |
|     2356 |  1964 | `		  if( sPath.nByte > 0 ){` |
|        - |  1965 | `			  /* Store the path in the corresponding conatiner */` |
|     2356 |  1966 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1177 |  1967 | `		  }` |
|     2356 |  1968 | `		  break;` |
|        - |  1969 | `									 }` |
|     1177 |  1970 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1971 | `		/* Run-Time Error report */` |
|     2356 |  1972 | `		pVm->bErrReport = 1;` |
|     2356 |  1973 | `		break;` |
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
|    11770 |  1995 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1996 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1997 | `		/* Create a new superglobal/global variable */` |
|    23542 |  1998 | `		const char *zName = va_arg(ap,const char *);` |
|    23542 |  1999 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    23542 |  2010 | `		nByte = SyStrlen(zName);` |
|    23542 |  2011 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2012 | `			/* Check if the superglobal is already installed */` |
|    23542 |  2013 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11772 |  2014 | `		}else{` |
|        - |  2015 | `			/* Query the top active VM frame */` |
|      ! 0 |  2016 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2017 | `		}` |
|    23542 |  2018 | `		if( pEntry ){` |
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
|    23542 |  2029 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    23542 |  2030 | `			if( pObj == 0 ){` |
|      ! 0 |  2031 | `				rc = SXERR_MEM;` |
|      ! 0 |  2032 | `				break;` |
|        - |  2033 | `			}` |
|    23542 |  2034 | `			nIdx = pObj->nIdx;` |
|        - |  2035 | `			/* Copy value */` |
|    23542 |  2036 | `			PH7_MemObjStore(pValue,pObj);` |
|    23542 |  2037 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2038 | `				/* Install the superglobal */` |
|    23542 |  2039 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11772 |  2040 | `			}else{` |
|        - |  2041 | `				/* Install in the current frame */` |
|      ! 0 |  2042 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2043 | `			}` |
|    23542 |  2044 | `			if( rc == SXRET_OK ){` |
|        - |  2045 | `				SyHashEntry *pRef;` |
|    23542 |  2046 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    23542 |  2047 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11772 |  2048 | `				}else{` |
|      ! 0 |  2049 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2050 | `				}` |
|        - |  2051 | `				/* Install in the reference table */` |
|    23542 |  2052 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    23542 |  2053 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2054 | `					/* Register in the $GLOBALS array */` |
|    23542 |  2055 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11770 |  2056 | `				}` |
|    11770 |  2057 | `			}` |
|        - |  2058 | `		}` |
|    23542 |  2059 | `		break;` |
|        - |  2060 | `									}` |
|     1307 |  2061 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2062 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2063 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2064 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2065 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2066 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2067 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2616 |  2068 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2616 |  2069 | `		const char *zValue = va_arg(ap,const char *);` |
|     2616 |  2070 | `		int nLen = va_arg(ap,int);` |
|        - |  2071 | `		ph7_hashmap *pMap;` |
|        - |  2072 | `		ph7_value *pValue;` |
|     2616 |  2073 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2074 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2075 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2615 |  2076 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2077 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2078 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2614 |  2079 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2080 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2081 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2614 |  2082 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2083 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2084 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2614 |  2085 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2086 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2087 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2614 |  2088 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2089 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2090 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2091 | `		}else{` |
|        - |  2092 | `			/* Extract the $_SERVER superglobal */` |
|     2614 |  2093 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2094 | `		}` |
|     2616 |  2095 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2096 | `			/* No such entry */` |
|      ! 0 |  2097 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2098 | `			break;` |
|        - |  2099 | `		}` |
|        - |  2100 | `		/* Point to the hashmap */` |
|     2616 |  2101 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2102 | `		/* Perform the insertion */` |
|     2616 |  2103 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2616 |  2104 | `		break;` |
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
|     2354 |  2155 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2156 | `		/* Register an IO stream device */` |
|     4710 |  2157 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2158 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7062 |  2159 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4710 |  2160 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2161 | `				/* Invalid stream */` |
|      ! 0 |  2162 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2163 | `				break;` |
|        - |  2164 | `		}` |
|     4710 |  2165 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2166 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2356 |  2167 | `			pVm->pDefStream = pStream;` |
|     1177 |  2168 | `		}` |
|        - |  2169 | `		/* Insert in the appropriate container */` |
|     4710 |  2170 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4710 |  2171 | `		break;` |
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
|    37996 |  2239 | `	return rc;` |
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
|      558 |  2298 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2299 |  |
|      559 |  2300 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      559 |  2301 | `	sxi32 rc = SXRET_OK;` |
|        - |  2302 | `	/* Append a new line */` |
|        - |  2303 | `#ifdef __WINNT__` |
|        1 |  2304 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2305 | `#else` |
|      558 |  2306 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2307 | `#endif` |
|        - |  2308 | `	/* Invoke the output consumer callback */` |
|      559 |  2309 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      559 |  2310 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      559 |  2311 | `	return rc;` |
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
|      954 |  2513 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2514 |  |
|        - |  2515 | `	VmFrame *pFrame;` |
|        - |  2516 | `	ph7_vm_func *pFunc;` |
|      955 |  2517 | `	*pzFuncName = 0;` |
|      955 |  2518 | `	*pnFuncLen = 0;` |
|      955 |  2519 | `	pFrame = pVm->pFrame;` |
|      955 |  2520 | `	if( pFrame == 0 ){` |
|      ! 0 |  2521 | `		return;` |
|        - |  2522 | `	}` |
|      955 |  2523 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      955 |  2524 | `	if( pFrame->pParent == 0 ){` |
|      947 |  2525 | `		return;` |
|        - |  2526 | `	}` |
|        9 |  2527 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        9 |  2528 | `	if( pFunc == 0 ){` |
|      ! 0 |  2529 | `		return;` |
|        - |  2530 | `	}` |
|        9 |  2531 | `	*pzFuncName = pFunc->sName.zString;` |
|        9 |  2532 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      478 |  2533 |  |
|        - |  2534 | `/*` |
|        - |  2535 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2536 | ` */` |
|      482 |  2537 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2538 |  |
|        - |  2539 | `	SyBlob sOut;` |
|        - |  2540 | `	SyString *pFile;` |
|      483 |  2541 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2542 | `		return PH7_OK;` |
|        - |  2543 | `	}` |
|      483 |  2544 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2545 | `		zClass = "Exception";` |
|      ! 0 |  2546 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2547 | `	}` |
|      483 |  2548 | `	if( zMsg == 0 ){` |
|      ! 0 |  2549 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2550 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2551 | `	}` |
|      483 |  2552 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      477 |  2553 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      238 |  2554 | `	}` |
|      483 |  2555 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      483 |  2556 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      483 |  2557 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      483 |  2558 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      483 |  2559 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      483 |  2560 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      483 |  2561 | `	if( pFile ){` |
|      483 |  2562 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      483 |  2563 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2564 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      241 |  2565 | `	}` |
|      483 |  2566 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      483 |  2567 | `	if( pFile ){` |
|      483 |  2568 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      483 |  2569 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2570 | `		if( zFuncName && nFuncLen > 0 ){` |
|        9 |  2571 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        5 |  2572 | `		}else{` |
|      475 |  2573 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2574 | `		}` |
|      241 |  2575 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2576 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2577 | `	}else{` |
|      ! 0 |  2578 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2579 | `	}` |
|      483 |  2580 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      483 |  2581 | `	if( pFile ){` |
|      483 |  2582 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      483 |  2583 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      483 |  2584 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      483 |  2585 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      241 |  2586 | `	}` |
|      483 |  2587 | `	VmCallErrorHandler(pVm,&sOut);` |
|      483 |  2588 | `	SyBlobRelease(&sOut);` |
|      483 |  2589 | `	return PH7_ABORT;` |
|      242 |  2590 |  |
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
|    33668 |  2729 | `static sxi32 VmByteCodeExec(` |
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
|    33670 |  2747 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    33670 |  2748 | `	if( nTos < 0 ){` |
|    31582 |  2749 | `		pTos = &pStack[-1];` |
|    15792 |  2750 | `	}else{` |
|     2090 |  2751 | `		pTos = &pStack[nTos];` |
|        - |  2752 | `	}` |
|    33670 |  2753 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    33670 |  2754 | `	pc = nPc;` |
|        - |  2755 | `	/* Execute as much as we can */` |
|  5097519 |  2756 | `	for(;;){` |
|        - |  2757 | `		/* Fetch the instruction to execute */` |
| 10194336 |  2758 | `		pInstr = &aInstr[pc];` |
| 10194336 |  2759 | `		rc = SXRET_OK;` |
|        - |  2760 | `/*` |
|        - |  2761 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2762 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2763 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2764 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2765 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2766 | ` */` |
| 10194336 |  2767 | `		switch(pInstr->iOp){` |
|        - |  2768 | `/*` |
|        - |  2769 | ` * DONE: P1 * *` |
|        - |  2770 | ` *` |
|        - |  2771 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2772 | ` * and return immediately.` |
|        - |  2773 | ` */` |
|    16516 |  2774 | `case PH7_OP_DONE:` |
|    33034 |  2775 | `	if( pInstr->iP1 ){` |
|        - |  2776 | `#ifdef UNTRUST` |
|        - |  2777 | `		if( pTos < pStack ){` |
|        - |  2778 | `			goto Abort;` |
|        - |  2779 | `		}` |
|        - |  2780 | `#endif` |
|    19170 |  2781 | `		if( pLastRef ){` |
|    12440 |  2782 | `			*pLastRef = pTos->nIdx;` |
|     6219 |  2783 | `		}` |
|    19170 |  2784 | `		if( pResult ){` |
|        - |  2785 | `			/* Execution result */` |
|    18196 |  2786 | `			PH7_MemObjStore(pTos,pResult);` |
|     9097 |  2787 | `		}` |
|    19170 |  2788 | `		VmPopOperand(&pTos,1);` |
|    23450 |  2789 | `	}else if( pLastRef ){` |
|        - |  2790 | `		/* Nothing referenced */` |
|     1074 |  2791 | `		*pLastRef = SXU32_HIGH;` |
|      536 |  2792 | `	}` |
|        - |  2793 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2794 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2795 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2796 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2797 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2798 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2799 | `	 * block can override it.` |
|        - |  2800 | `	 */` |
|    33036 |  2801 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
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
|    33034 |  2816 | `	goto Done;` |
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
|   219783 |  2861 | `case PH7_OP_JMP:` |
|   439612 |  2862 | `	pc = pInstr->iP2 - 1;` |
|   439612 |  2863 | `	break;` |
|        - |  2864 | `/*` |
|        - |  2865 | ` * JZ: P1 P2 *` |
|        - |  2866 | ` *` |
|        - |  2867 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2868 | ` * entry in the stack if P1 is zero.` |
|        - |  2869 | ` */` |
|   514408 |  2870 | `case PH7_OP_JZ:` |
|        - |  2871 | `#ifdef UNTRUST` |
|        - |  2872 | `	if( pTos < pStack ){` |
|        - |  2873 | `		goto Abort;` |
|        - |  2874 | `	}` |
|        - |  2875 | `#endif` |
|        - |  2876 | `	/* Get a boolean value */` |
|  1028906 |  2877 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      162 |  2878 | `		PH7_MemObjToBool(pTos);` |
|       80 |  2879 | `	}` |
|  1028906 |  2880 | `	if( !pTos->x.iVal ){` |
|        - |  2881 | `		/* Take the jump */` |
|   520690 |  2882 | `		pc = pInstr->iP2 - 1;` |
|   260344 |  2883 | `	}` |
|  1028906 |  2884 | `	if( !pInstr->iP1 ){` |
|   817432 |  2885 | `		VmPopOperand(&pTos,1);` |
|   408737 |  2886 | `	}` |
|  1028906 |  2887 | `	break;` |
|        - |  2888 | `/*` |
|        - |  2889 | ` * JNZ: P1 P2 *` |
|        - |  2890 | ` *` |
|        - |  2891 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2892 | ` * entry in the stack if P1 is zero.` |
|        - |  2893 | ` */` |
|    54381 |  2894 | `case PH7_OP_JNZ:` |
|        - |  2895 | `#ifdef UNTRUST` |
|        - |  2896 | `	if( pTos < pStack ){` |
|        - |  2897 | `		goto Abort;` |
|        - |  2898 | `	}` |
|        - |  2899 | `#endif` |
|        - |  2900 | `	/* Get a boolean value */` |
|   108764 |  2901 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2902 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2903 | `	}` |
|   108764 |  2904 | `	if( pTos->x.iVal ){` |
|        - |  2905 | `		/* Take the jump */` |
|     4644 |  2906 | `		pc = pInstr->iP2 - 1;` |
|     2321 |  2907 | `	}` |
|   108764 |  2908 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2909 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2910 | `	}` |
|   108764 |  2911 | `	break;` |
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
|   400518 |  2925 | `case PH7_OP_POP: {` |
|   801082 |  2926 | `	sxi32 n = pInstr->iP1;` |
|   801082 |  2927 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2928 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2929 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2930 | `	}` |
|   801082 |  2931 | `	VmPopOperand(&pTos,n);` |
|   801082 |  2932 | `	break;` |
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
|     6633 |  2955 | `case PH7_OP_NSSWITCH:` |
|    13268 |  2956 | `	SyBlobReset(&pVm->sNamespace);` |
|    13268 |  2957 | `	if( pInstr->p3 ){` |
|       92 |  2958 | `		const char *zNs = (const char *)pInstr->p3;` |
|       92 |  2959 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       45 |  2960 | `	}` |
|        - |  2961 | `	/* Clear namespace-scoped use-const imports */` |
|    13268 |  2962 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    13268 |  2963 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    13268 |  2964 | `	break;` |
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
|    13343 |  3108 | `case PH7_OP_ERR_CTRL:` |
|        - |  3109 | `	/*` |
|        - |  3110 | `	 * TICKET 1433-038:` |
|        - |  3111 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3112 | `	 * use the public API,to control error output.` |
|        - |  3113 | `	 */` |
|    26686 |  3114 | `	break;` |
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
|   858580 |  3174 | `case PH7_OP_LOADC: {` |
|        - |  3175 | `	ph7_value *pObj;` |
|        - |  3176 | `	/* Reserve a room */` |
|  1717206 |  3177 | `	pTos++;` |
|  2567452 |  3178 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1717206 |  3179 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3180 | `			SyHashEntry *pEntry;` |
|        - |  3181 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3182 | `			{` |
|        - |  3183 | `				SyHashEntry *pConstImport;` |
|    25139 |  3184 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    16758 |  3185 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16760 |  3186 | `				if( pConstImport ){` |
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
|    16750 |  3201 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16750 |  3202 | `			if( pEntry ){` |
|    16746 |  3203 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3204 | `				/* Set a NULL default value */` |
|    16746 |  3205 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    16746 |  3206 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3207 | `				/* Invoke the callback and deal with the expanded value */` |
|    16746 |  3208 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3209 | `				/* Mark as constant */` |
|    16746 |  3210 | `				pTos->nIdx = SXU32_HIGH;` |
|    16746 |  3211 | `				break;` |
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
|  1700450 |  3259 | `		PH7_MemObjLoad(pObj,pTos);` |
|   850248 |  3260 | `	}else{` |
|        - |  3261 | `		/* Set a NULL value */` |
|      ! 0 |  3262 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3263 | `	}` |
|   850203 |  3264 | `LoadC_Done:` |
|        - |  3265 | `	/* Mark as constant */` |
|  1700452 |  3266 | `	pTos->nIdx = SXU32_HIGH;` |
|  1700452 |  3267 | `	break;` |
|        - |  3268 | `				  }` |
|        - |  3269 | `/*` |
|        - |  3270 | ` * LOAD: P1 * P3` |
|        - |  3271 | ` *` |
|        - |  3272 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3273 | ` * from the P3 operand.` |
|        - |  3274 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3275 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3276 | ` */` |
|  1377435 |  3277 | `case PH7_OP_LOAD:{` |
|        - |  3278 | `	ph7_value *pObj;` |
|        - |  3279 | `	SyString sName;` |
|  2755092 |  3280 | `	if( pInstr->p3 == 0 ){` |
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
|  2755074 |  3293 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3294 | `		/* Reserve a room for the target object */` |
|  2755074 |  3295 | `		pTos++;` |
|        - |  3296 | `	}` |
|        - |  3297 | `	/* Extract the requested memory object */` |
|  2755092 |  3298 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2755092 |  3299 | `	if( pObj == 0 ){` |
|       26 |  3300 | `		if( pInstr->iP1 ){` |
|        - |  3301 | `			/* Variable not found,load NULL */` |
|       26 |  3302 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3303 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3304 | `			}else{` |
|       26 |  3305 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3306 | `			}` |
|       26 |  3307 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1377449 |  3308 | `			break;` |
|      ! 0 |  3309 | `		}else{` |
|        - |  3310 | `			/* Fatal error */` |
|      ! 0 |  3311 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3312 | `			goto Abort;` |
|        - |  3313 | `		}` |
|        - |  3314 | `	}` |
|        - |  3315 | `	/* Load variable contents */` |
|  2755068 |  3316 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2755068 |  3317 | `	pTos->nIdx = pObj->nIdx;` |
|  2755068 |  3318 | `	break;` |
|        - |  3319 | `				   }` |
|        - |  3320 | `/*` |
|        - |  3321 | ` * LOAD_MAP P1 * *` |
|        - |  3322 | ` *` |
|        - |  3323 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3324 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3325 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3326 | ` */` |
|    19158 |  3327 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3328 | `	ph7_hashmap *pMap;` |
|        - |  3329 | `	/* Allocate a new hashmap instance */` |
|    38318 |  3330 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    38318 |  3331 | `	if( pMap == 0 ){` |
|      ! 0 |  3332 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3333 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3334 | `		goto Abort;` |
|        - |  3335 | `	}` |
|    38318 |  3336 | `	if( pInstr->iP1 > 0 ){` |
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
|    38318 |  3360 | `	pTos++;` |
|    38318 |  3361 | `	pTos->nIdx = SXU32_HIGH;` |
|    38318 |  3362 | `	pTos->x.pOther = pMap;` |
|    38318 |  3363 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    38318 |  3364 | `	break;` |
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
|   220777 |  3453 | `case PH7_OP_LOAD_IDX: {` |
|   441600 |  3454 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   441600 |  3455 | `	ph7_hashmap *pMap = 0;` |
|        - |  3456 | `	ph7_value *pIdx;` |
|   441600 |  3457 | `	pIdx = 0;` |
|   441600 |  3458 | `	if( pInstr->iP1 == 0 ){` |
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
|   441600 |  3475 | `		pIdx = pTos;` |
|   441600 |  3476 | `		pTos--;` |
|        - |  3477 | `	}` |
|   441600 |  3478 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3479 | `		/* String access */` |
|   346414 |  3480 | `		if( pIdx ){` |
|        - |  3481 | `			sxu32 nOfft;` |
|   346414 |  3482 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3483 | `				/* Force an int cast */` |
|      ! 0 |  3484 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3485 | `			}` |
|   346414 |  3486 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   346414 |  3487 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3488 | `				/* Invalid offset,load null */` |
|      ! 0 |  3489 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3490 | `			}else{` |
|   346414 |  3491 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   346414 |  3492 | `				int c = zData[nOfft];` |
|   346414 |  3493 | `				PH7_MemObjRelease(pTos);` |
|   346414 |  3494 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   346414 |  3495 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3496 | `			}` |
|   173230 |  3497 | `		}else{` |
|        - |  3498 | `			/* No available index,load NULL */` |
|      ! 0 |  3499 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3500 | `		}` |
|   346414 |  3501 | `		break;` |
|        - |  3502 | `	}` |
|    95188 |  3503 | `	if( pInstr->iP2 == 1 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3504 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3505 | `			ph7_value *pObj;` |
|      ! 0 |  3506 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3507 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3508 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3509 | `			}` |
|      ! 0 |  3510 | `		}` |
|      ! 0 |  3511 | `	}` |
|    95188 |  3512 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    95188 |  3513 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|    95188 |  3514 | `		if( pInstr->iP2 == 1 ){` |
|        - |  3515 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  3516 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  3517 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  3518 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      875 |  3519 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      437 |  3520 | `		}` |
|        - |  3521 | `		/* Point to the hashmap */` |
|    95188 |  3522 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    95188 |  3523 | `		if( pIdx ){` |
|        - |  3524 | `			/* Load the desired entry */` |
|    95188 |  3525 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    47593 |  3526 | `		}` |
|    95188 |  3527 | `		if( rc != SXRET_OK && pInstr->iP2 == 1 ){` |
|        - |  3528 | `			/* Create a new empty entry */` |
|      265 |  3529 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      265 |  3530 | `			if( rc == SXRET_OK ){` |
|        - |  3531 | `				/* Point to the last inserted entry */` |
|      265 |  3532 | `				pNode = pMap->pLast;` |
|      132 |  3533 | `			}` |
|      132 |  3534 | `		}` |
|    47593 |  3535 | `	}` |
|    95188 |  3536 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  3537 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  3538 | `		char zMsg[128];` |
|      ! 0 |  3539 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3540 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3541 | `		}` |
|      ! 0 |  3542 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  3543 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  3544 | `	}` |
|    95188 |  3545 | `	if( pIdx ){` |
|    95188 |  3546 | `		PH7_MemObjRelease(pIdx);` |
|    47593 |  3547 | `	}` |
|    95188 |  3548 | `	if( rc == SXRET_OK ){` |
|        - |  3549 | `		/* Load entry contents */` |
|    43310 |  3550 | `		if( pMap->iRef < 2 ){` |
|        - |  3551 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3552 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3553 | `			 */` |
|       24 |  3554 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3555 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3556 | `		}else{` |
|    43288 |  3557 | `			pTos->nIdx = pNode->nValIdx;` |
|    43288 |  3558 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    43288 |  3559 | `			PH7_HashmapUnref(pMap);` |
|        - |  3560 | `		}` |
|    21656 |  3561 | `	}else{` |
|        - |  3562 | `		/* No such entry,load NULL */` |
|    51880 |  3563 | `		PH7_MemObjRelease(pTos);` |
|    51880 |  3564 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3565 | `	}` |
|    95188 |  3566 | `	break;` |
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
|   117861 |  3644 | `case PH7_OP_STORE: {` |
|        - |  3645 | `	ph7_value *pObj;` |
|        - |  3646 | `	SyString sName;` |
|        - |  3647 | `#ifdef UNTRUST` |
|        - |  3648 | `	if( pTos < pStack ){` |
|        - |  3649 | `		goto Abort;` |
|        - |  3650 | `	}` |
|        - |  3651 | `#endif` |
|   235724 |  3652 | `	if( pInstr->iP2 ){` |
|        - |  3653 | `		sxu32 nIdx;` |
|        - |  3654 | `		/* Member store operation */` |
|     3124 |  3655 | `		nIdx = pTos->nIdx;` |
|     3124 |  3656 | `		VmPopOperand(&pTos,1);` |
|     3124 |  3657 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3658 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3659 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3660 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3661 | `		}else{` |
|        - |  3662 | `			/* Point to the desired memory object */` |
|     3120 |  3663 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3120 |  3664 | `			if( pObj ){` |
|        - |  3665 | `				/* Perform the store operation */` |
|     3120 |  3666 | `				PH7_MemObjStore(pTos,pObj);` |
|     1559 |  3667 | `			}` |
|        - |  3668 | `		}` |
|   119424 |  3669 | `		break;` |
|   232602 |  3670 | `	}else if( pInstr->p3 == 0 ){` |
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
|   232596 |  3684 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3685 | `	}` |
|        - |  3686 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   232602 |  3687 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   232602 |  3688 | `	if( pObj == 0 ){` |
|      ! 0 |  3689 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3690 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3691 | `		goto Abort;` |
|        - |  3692 | `	}` |
|   232602 |  3693 | `	if( !pInstr->p3 ){` |
|        7 |  3694 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3695 | `	}` |
|        - |  3696 | `	/* Perform the store operation */` |
|   232602 |  3697 | `	PH7_MemObjStore(pTos,pObj);` |
|   232602 |  3698 | `	break;` |
|        - |  3699 | `				   }` |
|        - |  3700 | `/*` |
|        - |  3701 | ` * STORE_IDX:   P1 * P3` |
|        - |  3702 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3703 | ` *` |
|        - |  3704 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3705 | ` */` |
|    84784 |  3706 | `case PH7_OP_STORE_IDX:` |
|        - |  3707 | `case PH7_OP_STORE_IDX_REF: {` |
|   169570 |  3708 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3709 | `	ph7_value *pKey;` |
|        - |  3710 | `	sxu32 nIdx;` |
|   169570 |  3711 | `	if( pInstr->iP1 ){` |
|        - |  3712 | `		/* Key is next on stack */` |
|    58492 |  3713 | `		pKey = pTos;` |
|    58492 |  3714 | `		pTos--;` |
|    29247 |  3715 | `	}else{` |
|   111080 |  3716 | `		pKey = 0;` |
|        - |  3717 | `	}` |
|   169570 |  3718 | `	nIdx = pTos->nIdx;` |
|   169570 |  3719 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3720 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  3721 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  3722 | `		 * checking true sharing count, then re-add after separation. */` |
|   169518 |  3723 | `		if( nIdx != SXU32_HIGH ){` |
|   169518 |  3724 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   254276 |  3725 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   169518 |  3726 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3727 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  3728 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  3729 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  3730 | `				 * refcounts if the backing array was already separated. */` |
|   169518 |  3731 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   169518 |  3732 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   169518 |  3733 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   169518 |  3734 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   169518 |  3735 | `					pTos->x.pOther = pMap;` |
|    84760 |  3736 | `				}else{` |
|        - |  3737 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  3738 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  3739 | `					pMap = pCur;` |
|        - |  3740 | `				}` |
|    84760 |  3741 | `			}else{` |
|      ! 0 |  3742 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3743 | `			}` |
|    84760 |  3744 | `		}else{` |
|      ! 0 |  3745 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  3746 | `		}` |
|   169518 |  3747 | `		if( pMap->iRef < 2 ){` |
|        - |  3748 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  3749 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  3750 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  3751 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  3752 | `			pMap->iRef = 2;` |
|      ! 0 |  3753 | `		}` |
|    84760 |  3754 | `	}else{` |
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
|   169518 |  3809 | `	VmPopOperand(&pTos,1);` |
|        - |  3810 | `	/* Phase#2: Perform the insertion */` |
|   169518 |  3811 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3812 | `		/* Insertion by reference */` |
|       15 |  3813 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3814 | `	}else{` |
|   169504 |  3815 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3816 | `	}` |
|   169518 |  3817 | `	if( pKey ){` |
|    58442 |  3818 | `		PH7_MemObjRelease(pKey);` |
|    29220 |  3819 | `	}` |
|   169518 |  3820 | `	break;` |
|        - |  3821 | `					   }` |
|        - |  3822 | `/*` |
|        - |  3823 | ` * INCR: P1 * *` |
|        - |  3824 | ` *` |
|        - |  3825 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3826 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3827 | ` * the stack and increment after that.` |
|        - |  3828 | ` */` |
|   153589 |  3829 | `case PH7_OP_INCR:` |
|        - |  3830 | `#ifdef UNTRUST` |
|        - |  3831 | `	if( pTos < pStack ){` |
|        - |  3832 | `		goto Abort;` |
|        - |  3833 | `	}` |
|        - |  3834 | `#endif` |
|   307224 |  3835 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   307224 |  3836 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3837 | `			ph7_value *pObj;` |
|   307224 |  3838 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3839 | `				/* Force a numeric cast */` |
|   307224 |  3840 | `				PH7_MemObjToNumeric(pObj);` |
|   307224 |  3841 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3842 | `					pObj->rVal++;` |
|        - |  3843 | `					/* Try to get an integer representation */` |
|      ! 0 |  3844 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3845 | `				}else{` |
|   307224 |  3846 | `					pObj->x.iVal++;` |
|   307224 |  3847 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3848 | `				}` |
|   307224 |  3849 | `				if( pInstr->iP1 ){` |
|        - |  3850 | `					/* Pre-icrement */` |
|       71 |  3851 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3852 | `				}` |
|   153633 |  3853 | `			}` |
|   153635 |  3854 | `		}else{` |
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
|   153633 |  3869 | `	}` |
|   307224 |  3870 | `	break;` |
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
|    24802 |  3925 | `case PH7_OP_UMINUS:` |
|        - |  3926 | `#ifdef UNTRUST` |
|        - |  3927 | `	if( pTos < pStack ){` |
|        - |  3928 | `		goto Abort;` |
|        - |  3929 | `	}` |
|        - |  3930 | `#endif` |
|        - |  3931 | `	/* Force a numeric (integer,real or both) cast */` |
|    49606 |  3932 | `	PH7_MemObjToNumeric(pTos);` |
|    49606 |  3933 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  3934 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3935 | `	}` |
|    49606 |  3936 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    49576 |  3937 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    24787 |  3938 | `	}` |
|    49606 |  3939 | `	break;` |
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
|    40802 |  3966 | `case PH7_OP_LNOT:` |
|        - |  3967 | `#ifdef UNTRUST` |
|        - |  3968 | `	if( pTos < pStack ){` |
|        - |  3969 | `		goto Abort;` |
|        - |  3970 | `	}` |
|        - |  3971 | `#endif` |
|        - |  3972 | `	/* Force a boolean cast */` |
|    81650 |  3973 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3974 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3975 | `	}` |
|    81650 |  3976 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    81650 |  3977 | `	break;` |
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
|    64269 |  4580 | `case PH7_OP_CAT:{` |
|        - |  4581 | `	ph7_value *pNos,*pCur;` |
|   128540 |  4582 | `	if( pInstr->iP1 < 1 ){` |
|   101458 |  4583 | `		pNos = &pTos[-1];` |
|    50730 |  4584 | `	}else{` |
|    27084 |  4585 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4586 | `	}` |
|        - |  4587 | `#ifdef UNTRUST` |
|        - |  4588 | `	if( pNos < pStack ){` |
|        - |  4589 | `		goto Abort;` |
|        - |  4590 | `	}` |
|        - |  4591 | `#endif` |
|        - |  4592 | `	/* Force a string cast */` |
|   128540 |  4593 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1392 |  4594 | `		PH7_MemObjToString(pNos);` |
|      695 |  4595 | `	}` |
|   128540 |  4596 | `	pCur = &pNos[1];` |
|   259206 |  4597 | `	while( pCur <= pTos ){` |
|   130668 |  4598 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50724 |  4599 | `			PH7_MemObjToString(pCur);` |
|    25361 |  4600 | `		}` |
|        - |  4601 | `		/* Perform the concatenation */` |
|   130668 |  4602 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   130630 |  4603 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    65314 |  4604 | `		}` |
|   130668 |  4605 | `		SyBlobRelease(&pCur->sBlob);` |
|   130668 |  4606 | `		pCur++;` |
|        2 |  4607 | `	}` |
|   128540 |  4608 | `	pTos = pNos;` |
|   128540 |  4609 | `	break;` |
|        - |  4610 | `				}` |
|        - |  4611 | `/*  CAT_STORE: * * *` |
|        - |  4612 | ` *` |
|        - |  4613 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4614 | ` * back.` |
|        - |  4615 | ` */` |
|     3456 |  4616 | `case PH7_OP_CAT_STORE:{` |
|     6914 |  4617 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4618 | `	ph7_value *pObj;` |
|        - |  4619 | `#ifdef UNTRUST` |
|        - |  4620 | `	if( pNos < pStack ){` |
|        - |  4621 | `		goto Abort;` |
|        - |  4622 | `	}` |
|        - |  4623 | `#endif` |
|     6914 |  4624 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4625 | `		/* Force a string cast */` |
|      ! 0 |  4626 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4627 | `	}` |
|     6914 |  4628 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4629 | `		/* Force a string cast */` |
|      ! 0 |  4630 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4631 | `	}` |
|        - |  4632 | `	/* Perform the concatenation (Reverse order) */` |
|     6914 |  4633 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6914 |  4634 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3456 |  4635 | `	}` |
|        - |  4636 | `	/* Perform the store operation */` |
|     6914 |  4637 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4638 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6914 |  4639 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6914 |  4640 | `		PH7_MemObjStore(pTos,pObj);` |
|     3456 |  4641 | `	}` |
|     6914 |  4642 | `	PH7_MemObjStore(pTos,pNos);` |
|     6914 |  4643 | `	VmPopOperand(&pTos,1);` |
|     6914 |  4644 | `	break;` |
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
|    96631 |  4658 | `case PH7_OP_LAND:` |
|        - |  4659 | `case PH7_OP_LOR: {` |
|   193308 |  4660 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4661 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4662 | `#ifdef UNTRUST` |
|        - |  4663 | `	if( pNos < pStack ){` |
|        - |  4664 | `		goto Abort;` |
|        - |  4665 | `	}` |
|        - |  4666 | `#endif` |
|        - |  4667 | `	/* Force a boolean cast */` |
|   193308 |  4668 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4669 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4670 | `	}` |
|   193308 |  4671 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4672 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4673 | `	}` |
|   193308 |  4674 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   193308 |  4675 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   193308 |  4676 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4677 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    89188 |  4678 | `		v1 = and_logic[v1*3+v2];` |
|    44617 |  4679 | `	}else{` |
|        - |  4680 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   104122 |  4681 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4682 | `	}` |
|   193308 |  4683 | `	if( v1 == 2 ){` |
|      ! 0 |  4684 | `		v1 = 1;` |
|      ! 0 |  4685 | `	}` |
|   193308 |  4686 | `	VmPopOperand(&pTos,1);` |
|   193308 |  4687 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   193308 |  4688 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   193308 |  4689 | `	break;` |
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
|     4050 |  4821 | `case PH7_OP_EQ:` |
|        - |  4822 | `case PH7_OP_NEQ: {` |
|     8102 |  4823 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4824 | `	/* Perform the comparison and act accordingly */` |
|        - |  4825 | `#ifdef UNTRUST` |
|        - |  4826 | `	if( pNos < pStack ){` |
|        - |  4827 | `		goto Abort;` |
|        - |  4828 | `	}` |
|        - |  4829 | `#endif` |
|     8102 |  4830 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8102 |  4831 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  4832 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8093 |  4833 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8058 |  4834 | `		rc = rc == 0;` |
|     4030 |  4835 | `	}else{` |
|       28 |  4836 | `		rc = rc != 0;` |
|        - |  4837 | `	}` |
|     8102 |  4838 | `	VmPopOperand(&pTos,1);` |
|     8102 |  4839 | `	if( !pInstr->iP2 ){` |
|        - |  4840 | `		/* Push comparison result without taking the jump */` |
|     8102 |  4841 | `		PH7_MemObjRelease(pTos);` |
|     8102 |  4842 | `		pTos->x.iVal = rc;` |
|        - |  4843 | `		/* Invalidate any prior representation */` |
|     8102 |  4844 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4052 |  4845 | `	}else{` |
|      ! 0 |  4846 | `		if( rc ){` |
|        - |  4847 | `			/* Jump to the desired location */` |
|      ! 0 |  4848 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4849 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4850 | `		}` |
|        - |  4851 | `	}` |
|     8102 |  4852 | `	break;` |
|        - |  4853 | `				 }` |
|        - |  4854 | `/* OP_TEQ P1 P2 *` |
|        - |  4855 | ` *` |
|        - |  4856 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4857 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4858 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4859 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4860 | ` */` |
|   136356 |  4861 | `case PH7_OP_TEQ: {` |
|   272714 |  4862 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4863 | `	/* Perform the comparison and act accordingly */` |
|        - |  4864 | `#ifdef UNTRUST` |
|        - |  4865 | `	if( pNos < pStack ){` |
|        - |  4866 | `		goto Abort;` |
|        - |  4867 | `	}` |
|        - |  4868 | `#endif` |
|   272714 |  4869 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   272714 |  4870 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4871 | `		rc = 0;` |
|        2 |  4872 | `	}else{` |
|   272712 |  4873 | `		rc = rc == 0;` |
|        - |  4874 | `	}` |
|   272714 |  4875 | `	VmPopOperand(&pTos,1);` |
|   272714 |  4876 | `	if( !pInstr->iP2 ){` |
|        - |  4877 | `		/* Push comparison result without taking the jump */` |
|   272714 |  4878 | `		PH7_MemObjRelease(pTos);` |
|   272714 |  4879 | `		pTos->x.iVal = rc;` |
|        - |  4880 | `		/* Invalidate any prior representation */` |
|   272714 |  4881 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   136358 |  4882 | `	}else{` |
|      ! 0 |  4883 | `		if( rc ){` |
|        - |  4884 | `			/* Jump to the desired location */` |
|      ! 0 |  4885 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4886 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4887 | `		}` |
|        - |  4888 | `	}` |
|   272714 |  4889 | `	break;` |
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
|   106325 |  4900 | `case PH7_OP_TNE: {` |
|   212652 |  4901 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4902 | `	/* Perform the comparison and act accordingly */` |
|        - |  4903 | `#ifdef UNTRUST` |
|        - |  4904 | `	if( pNos < pStack ){` |
|        - |  4905 | `		goto Abort;` |
|        - |  4906 | `	}` |
|        - |  4907 | `#endif` |
|   212652 |  4908 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   212652 |  4909 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4910 | `		rc = 1;` |
|        2 |  4911 | `	}else{` |
|   212650 |  4912 | `		rc = rc != 0;` |
|        - |  4913 | `	}` |
|   212652 |  4914 | `	VmPopOperand(&pTos,1);` |
|   212652 |  4915 | `	if( !pInstr->iP2 ){` |
|        - |  4916 | `		/* Push comparison result without taking the jump */` |
|   212652 |  4917 | `		PH7_MemObjRelease(pTos);` |
|   212652 |  4918 | `		pTos->x.iVal = rc;` |
|        - |  4919 | `		/* Invalidate any prior representation */` |
|   212652 |  4920 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   106327 |  4921 | `	}else{` |
|      ! 0 |  4922 | `		if( rc ){` |
|        - |  4923 | `			/* Jump to the desired location */` |
|      ! 0 |  4924 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4925 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4926 | `		}` |
|        - |  4927 | `	}` |
|   212652 |  4928 | `	break;` |
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
|   103807 |  4948 | `case PH7_OP_LT:` |
|        - |  4949 | `case PH7_OP_LE: {` |
|   207660 |  4950 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4951 | `	/* Perform the comparison and act accordingly */` |
|        - |  4952 | `#ifdef UNTRUST` |
|        - |  4953 | `	if( pNos < pStack ){` |
|        - |  4954 | `		goto Abort;` |
|        - |  4955 | `	}` |
|        - |  4956 | `#endif` |
|   207660 |  4957 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   207660 |  4958 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4959 | `		rc = 0;` |
|   207656 |  4960 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      432 |  4961 | `		rc = rc < 1;` |
|      217 |  4962 | `	}else{` |
|   207222 |  4963 | `		rc = rc < 0;` |
|        - |  4964 | `	}` |
|   207660 |  4965 | `	VmPopOperand(&pTos,1);` |
|   207660 |  4966 | `	if( !pInstr->iP2 ){` |
|        - |  4967 | `		/* Push comparison result without taking the jump */` |
|   207660 |  4968 | `		PH7_MemObjRelease(pTos);` |
|   207660 |  4969 | `		pTos->x.iVal = rc;` |
|        - |  4970 | `		/* Invalidate any prior representation */` |
|   207660 |  4971 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   103853 |  4972 | `	}else{` |
|      ! 0 |  4973 | `		if( rc ){` |
|        - |  4974 | `			/* Jump to the desired location */` |
|      ! 0 |  4975 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4976 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4977 | `		}` |
|        - |  4978 | `	}` |
|   207660 |  4979 | `	break;` |
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
|    49551 |  4999 | `case PH7_OP_GT:` |
|        - |  5000 | `case PH7_OP_GE: {` |
|    99104 |  5001 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5002 | `	/* Perform the comparison and act accordingly */` |
|        - |  5003 | `#ifdef UNTRUST` |
|        - |  5004 | `	if( pNos < pStack ){` |
|        - |  5005 | `		goto Abort;` |
|        - |  5006 | `	}` |
|        - |  5007 | `#endif` |
|    99104 |  5008 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    99104 |  5009 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5010 | `		rc = 0;` |
|    99100 |  5011 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    98946 |  5012 | `		rc = rc >= 0;` |
|    49474 |  5013 | `	}else{` |
|      152 |  5014 | `		rc = rc > 0;` |
|        - |  5015 | `	}` |
|    99104 |  5016 | `	VmPopOperand(&pTos,1);` |
|    99104 |  5017 | `	if( !pInstr->iP2 ){` |
|        - |  5018 | `		/* Push comparison result without taking the jump */` |
|    99104 |  5019 | `		PH7_MemObjRelease(pTos);` |
|    99104 |  5020 | `		pTos->x.iVal = rc;` |
|        - |  5021 | `		/* Invalidate any prior representation */` |
|    99104 |  5022 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    49553 |  5023 | `	}else{` |
|      ! 0 |  5024 | `		if( rc ){` |
|        - |  5025 | `			/* Jump to the desired location */` |
|      ! 0 |  5026 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5027 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5028 | `		}` |
|        - |  5029 | `	}` |
|    99104 |  5030 | `	break;` |
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
|       44 |  5250 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       90 |  5251 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  5252 | `	VmFrame *pFrameLocal;` |
|        - |  5253 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       90 |  5254 | `	pException->iFinallyDone = 0;` |
|       90 |  5255 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  5256 | `	/* Create the exception frame */` |
|       90 |  5257 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       90 |  5258 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  5259 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  5260 | `		goto Abort;` |
|        - |  5261 | `	}` |
|        - |  5262 | `	/* Mark the special frame */` |
|       90 |  5263 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       90 |  5264 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  5265 | `	/* Point to the frame that trigger the exception */` |
|       90 |  5266 | `	pFrameLocal = pFrameLocal->pParent;` |
|       90 |  5267 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       90 |  5268 | `	pException->pFrame = pFrameLocal;` |
|       90 |  5269 | `	break;` |
|        - |  5270 | `							}` |
|        - |  5271 | `/*` |
|        - |  5272 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  5273 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  5274 | ` */` |
|       43 |  5275 | `case PH7_OP_POP_EXCEPTION: {` |
|       88 |  5276 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       88 |  5277 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  5278 | `		ph7_exception **apException;` |
|        - |  5279 | `		/* Pop the loaded exception */` |
|       28 |  5280 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  5281 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  5282 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  5283 | `		}` |
|       13 |  5284 | `	}` |
|       88 |  5285 | `	pException->pFrame = 0;` |
|        - |  5286 | `	/* Leave the exception frame */` |
|       88 |  5287 | `	VmLeaveFrame(&(*pVm));` |
|        - |  5288 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       88 |  5289 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  5290 | `		sxi32 rcFinally;` |
|       20 |  5291 | `		pException->iFinallyDone = 1;` |
|       20 |  5292 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  5293 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  5294 | `			goto Abort;` |
|        - |  5295 | `		}` |
|        9 |  5296 | `	}` |
|       88 |  5297 | `	break;` |
|        - |  5298 | `							}` |
|        - |  5299 |  |
|        - |  5300 | `/*` |
|        - |  5301 | ` * OP_THROW * P2 *` |
|        - |  5302 | ` * Throw an user exception.` |
|        - |  5303 | ` */` |
|       30 |  5304 | `case PH7_OP_THROW: {` |
|       62 |  5305 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       62 |  5306 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  5307 | `#ifdef UNTRUST` |
|        - |  5308 | `	if( pTos < pStack ){` |
|        - |  5309 | `		goto Abort;` |
|        - |  5310 | `	}` |
|        - |  5311 | `#endif` |
|       62 |  5312 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5313 | `	/* Tell the upper layer that an exception was thrown */` |
|       62 |  5314 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       62 |  5315 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       62 |  5316 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5317 | `		ph7_class *pException;` |
|        - |  5318 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  5319 | `		 */` |
|       62 |  5320 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       62 |  5321 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  5322 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  5323 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  5324 | `			if( rc == SXERR_ABORT ){` |
|        - |  5325 | `				/* Abort processing immediately */` |
|      ! 0 |  5326 | `				goto Abort;` |
|        - |  5327 | `			}` |
|      ! 0 |  5328 | `		}else{` |
|        - |  5329 | `			/* Throw the exception */` |
|       62 |  5330 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       62 |  5331 | `			if( rc == SXERR_ABORT ){` |
|        - |  5332 | `				/* Abort processing immediately */` |
|        9 |  5333 | `				goto Abort;` |
|        - |  5334 | `			}` |
|        - |  5335 | `		}` |
|       28 |  5336 | `	}else{` |
|        - |  5337 | `		/* Expecting a class instance */` |
|      ! 0 |  5338 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  5339 | `		if( rc == SXERR_ABORT ){` |
|        - |  5340 | `			/* Abort processing immediately */` |
|      ! 0 |  5341 | `			goto Abort;` |
|        - |  5342 | `		}` |
|        - |  5343 | `	}` |
|        - |  5344 | `	/* Pop the top entry */` |
|       54 |  5345 | `	VmPopOperand(&pTos,1);` |
|        - |  5346 | `	/* Perform an unconditional jump */` |
|       54 |  5347 | `	pc = nJump - 1;` |
|       54 |  5348 | `	break;` |
|        - |  5349 | `				   }` |
|        - |  5350 | `/*` |
|        - |  5351 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  5352 | ` * Prepare a foreach step.` |
|        - |  5353 | ` */` |
|     5172 |  5354 | `case PH7_OP_FOREACH_INIT: {` |
|    10346 |  5355 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5356 | `	void *pName;` |
|        - |  5357 | `#ifdef UNTRUST` |
|        - |  5358 | `	if( pTos < pStack ){` |
|        - |  5359 | `		goto Abort;` |
|        - |  5360 | `	}` |
|        - |  5361 | `#endif` |
|    10346 |  5362 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
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
|    10346 |  5375 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
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
|    10346 |  5388 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  5389 | `		/* Jump out of the loop */` |
|      ! 0 |  5390 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5391 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  5392 | `		}` |
|      ! 0 |  5393 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  5394 | `	}else{` |
|        - |  5395 | `		ph7_foreach_step *pStep;` |
|    10346 |  5396 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    10346 |  5397 | `		if( pStep == 0 ){` |
|      ! 0 |  5398 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  5399 | `			/* Jump out of the loop */` |
|      ! 0 |  5400 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5401 | `		}else{` |
|        - |  5402 | `			/* Zero the structure */` |
|    10346 |  5403 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  5404 | `			/* Prepare the step */` |
|    10346 |  5405 | `			pStep->iFlags = pInfo->iFlags;` |
|    10346 |  5406 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5407 | `				ph7_hashmap *pMap;` |
|        - |  5408 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  5409 | `				 * source array so mutations don't affect other sharers. */` |
|    10318 |  5410 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
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
|    10318 |  5425 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5426 | `				/* Reset the internal loop cursor */` |
|    10318 |  5427 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5428 | `				/* Mark the step */` |
|    10318 |  5429 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    10318 |  5430 | `				pStep->xIter.pMap = pMap;` |
|    10318 |  5431 | `				pMap->iRef++;` |
|     5160 |  5432 | `			}else{` |
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
|    10346 |  5498 | `		if( pStep ){` |
|    10346 |  5499 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5500 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5501 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5502 | `				/* Jump out of the loop */` |
|      ! 0 |  5503 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5504 | `			}` |
|     5172 |  5505 | `		}` |
|        - |  5506 | `	}` |
|    10346 |  5507 | `	VmPopOperand(&pTos,1);` |
|    10346 |  5508 | `	break;` |
|        - |  5509 | `						  }` |
|        - |  5510 | `/*` |
|        - |  5511 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5512 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5513 | ` */` |
|    83835 |  5514 | `case PH7_OP_FOREACH_STEP: {` |
|   167672 |  5515 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5516 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5517 | `	ph7_value *pValue;` |
|        - |  5518 | `	VmFrame *pFrameLocal;` |
|        - |  5519 | `	/* Peek the last step */` |
|   167672 |  5520 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   167672 |  5521 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   167672 |  5522 | `	pFrameLocal = pVm->pFrame;` |
|   167672 |  5523 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   167672 |  5524 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   167560 |  5525 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5526 | `		ph7_hashmap_node *pNode;` |
|        - |  5527 | `		/* Extract the current node value */` |
|   167560 |  5528 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   167560 |  5529 | `		if( pNode == 0 ){` |
|        - |  5530 | `			/* No more entry to process */` |
|    10316 |  5531 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    10316 |  5532 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5533 | `				/* Break the reference with the last element */` |
|        7 |  5534 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  5535 | `			}` |
|        - |  5536 | `			/* Automatically reset the loop cursor */` |
|    10316 |  5537 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5538 | `			/* Cleanup the mess left behind */` |
|    10316 |  5539 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    10316 |  5540 | `			SySetPop(&pInfo->aStep);` |
|    10316 |  5541 | `			PH7_HashmapUnref(pMap);` |
|     5159 |  5542 | `		}else{` |
|   157246 |  5543 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5544 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5545 | `				if( pKey ){` |
|      416 |  5546 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5547 | `				}` |
|      207 |  5548 | `			}` |
|   157246 |  5549 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
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
|   157224 |  5561 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   157224 |  5562 | `				if( pValue ){` |
|   157224 |  5563 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    78611 |  5564 | `				}` |
|        - |  5565 | `			}` |
|        2 |  5566 | `		}` |
|    83893 |  5567 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
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
|   167672 |  5688 | `	break;` |
|        - |  5689 | `						  }` |
|        - |  5690 | `/*` |
|        - |  5691 | ` * OP_MEMBER P1 P2` |
|        - |  5692 | ` * Load class attribute/method on the stack.` |
|        - |  5693 | ` */` |
|     2346 |  5694 | `case PH7_OP_MEMBER: {` |
|        - |  5695 | `	ph7_class_instance *pThis;` |
|        - |  5696 | `	ph7_value *pNos;` |
|        - |  5697 | `	SyString sName;` |
|     4694 |  5698 | `	if( !pInstr->iP1 ){` |
|     4506 |  5699 | `		pNos = &pTos[-1];` |
|        - |  5700 | `#ifdef UNTRUST` |
|        - |  5701 | `		if( pNos < pStack ){` |
|        - |  5702 | `			goto Abort;` |
|        - |  5703 | `		}` |
|        - |  5704 | `#endif` |
|     4506 |  5705 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5706 | `			ph7_class *pClass;` |
|        - |  5707 | `			/* Class already instantiated */` |
|     4506 |  5708 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5709 | `			/* Point to the instantiated class */` |
|     4506 |  5710 | `			pClass = pThis->pClass;` |
|        - |  5711 | `			/* Extract attribute name first */` |
|     4506 |  5712 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4506 |  5713 | `			if( pInstr->iP2 ){` |
|        - |  5714 | `				/* Method call */` |
|      478 |  5715 | `				ph7_class_method *pMeth = 0;` |
|      478 |  5716 | `				if( sName.nByte > 0 ){` |
|        - |  5717 | `					/* Extract the target method */` |
|      478 |  5718 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      238 |  5719 | `				}` |
|      478 |  5720 | `				if( pMeth == 0 ){` |
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
|      478 |  5731 | `					PH7_MemObjRelease(pTos);` |
|      478 |  5732 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      478 |  5733 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5734 | `				}` |
|      478 |  5735 | `				pTos->nIdx = SXU32_HIGH;` |
|      240 |  5736 | `			}else{` |
|        - |  5737 | `				/* Attribute access */` |
|     4030 |  5738 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5739 | `				SyHashEntry *pEntry;` |
|        - |  5740 | `				/* Extract the target attribute */` |
|     4030 |  5741 | `				if( sName.nByte > 0 ){` |
|     4030 |  5742 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     4030 |  5743 | `					if( pEntry ){` |
|        - |  5744 | `						/* Point to the attribute value */` |
|     4028 |  5745 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2013 |  5746 | `					}` |
|     2014 |  5747 | `				}` |
|     4030 |  5748 | `				if( pObjAttr == 0 ){` |
|        - |  5749 | `					/* No such attribute,load null */` |
|        4 |  5750 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5751 | `						&pClass->sName,&sName);` |
|        - |  5752 | `					/* Call the __get magic method if available */` |
|        3 |  5753 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5754 | `				}` |
|     4030 |  5755 | `				VmPopOperand(&pTos,1);` |
|        - |  5756 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5757 | `				 * This is due to the following case:` |
|        - |  5758 | `				 *     (new TestClass())->foo;` |
|        - |  5759 | `				 */` |
|     4030 |  5760 | `				pThis->iRef++;` |
|     4030 |  5761 | `				PH7_MemObjRelease(pTos);` |
|     4030 |  5762 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     4030 |  5763 | `				if( pObjAttr ){` |
|     4028 |  5764 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5765 | `					/* Check attribute access */` |
|     4028 |  5766 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  5767 | `						/* Load attribute */` |
|     4028 |  5768 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     4028 |  5769 | `						if( pValue ){` |
|     4028 |  5770 | `							if( pThis->iRef < 2 ){` |
|        - |  5771 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5772 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5773 | `								 */` |
|        3 |  5774 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5775 | `							}else{` |
|        - |  5776 | `								/* Simple load */` |
|     4026 |  5777 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5778 | `							}` |
|     4028 |  5779 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     4026 |  5780 | `								if( pThis->iRef > 1 ){` |
|        - |  5781 | `									/* Load attribute index */` |
|     4024 |  5782 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2011 |  5783 | `								}` |
|     2012 |  5784 | `							}` |
|     2013 |  5785 | `						}` |
|     2015 |  5786 | `					}else{` |
|        - |  5787 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  5788 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  5789 | `						char zMsg[256];` |
|      ! 0 |  5790 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  5791 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  5792 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  5793 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  5794 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5795 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  5796 | `						goto Abort;` |
|        - |  5797 | `					}` |
|     2013 |  5798 | `				}` |
|        - |  5799 | `				/* Safely unreference the object */` |
|     4030 |  5800 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5801 | `			}` |
|     2254 |  5802 | `		}else{` |
|      ! 0 |  5803 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5804 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5805 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5806 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5807 | `		}` |
|     2254 |  5808 | `	}else{` |
|        - |  5809 | `		/* Static member access using class name */` |
|      190 |  5810 | `		pNos = pTos;` |
|      190 |  5811 | `		pThis = 0;` |
|      190 |  5812 | `		if( !pInstr->p3 ){` |
|      178 |  5813 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      178 |  5814 | `			pNos--;` |
|        - |  5815 | `#ifdef UNTRUST` |
|        - |  5816 | `			if( pNos < pStack ){` |
|        - |  5817 | `				goto Abort;` |
|        - |  5818 | `			}` |
|        - |  5819 | `#endif` |
|       90 |  5820 | `		}else{` |
|        - |  5821 | `			/* Attribute name already computed */` |
|       14 |  5822 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5823 | `		}` |
|      190 |  5824 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      190 |  5825 | `			ph7_class *pClass = 0;` |
|      190 |  5826 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5827 | `				/* Class already instantiated */` |
|        5 |  5828 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  5829 | `				pClass = pThis->pClass;` |
|        5 |  5830 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  5831 | `			}else{` |
|        - |  5832 | `				/* Try to extract the target class */` |
|      186 |  5833 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      186 |  5834 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      186 |  5835 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5836 | `					/* Handle self/static/parent keywords */` |
|      186 |  5837 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       56 |  5838 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       56 |  5839 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5840 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5841 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5842 | `						}` |
|      159 |  5843 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  5844 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      131 |  5845 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       26 |  5846 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       26 |  5847 | `						if( pSelf && pSelf->pBase ){` |
|       26 |  5848 | `							pClass = pSelf->pBase;` |
|       12 |  5849 | `						}` |
|       14 |  5850 | `					}else{` |
|       82 |  5851 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5852 | `					}` |
|       92 |  5853 | `				}` |
|        - |  5854 | `			}` |
|      190 |  5855 | `			if( pClass == 0 ){` |
|        - |  5856 | `				/* Undefined class */` |
|      ! 0 |  5857 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5858 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5859 | `					);` |
|      ! 0 |  5860 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5861 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5862 | `				}` |
|      ! 0 |  5863 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5864 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5865 | `			}else{` |
|      190 |  5866 | `				if( pInstr->iP2 ){` |
|        - |  5867 | `					/* Method call */` |
|       74 |  5868 | `					ph7_class_method *pMeth = 0;` |
|       74 |  5869 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5870 | `						/* Extract the target method */` |
|       74 |  5871 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       36 |  5872 | `					}` |
|       74 |  5873 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5874 | `						if( pMeth ){` |
|      ! 0 |  5875 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5876 | `								&pClass->sName,&sName` |
|        - |  5877 | `								);` |
|      ! 0 |  5878 | `						}else{` |
|      ! 0 |  5879 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5880 | `								&pClass->sName,&sName` |
|        - |  5881 | `								);` |
|        - |  5882 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5883 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5884 | `						}` |
|        - |  5885 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5886 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5887 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5888 | `						}` |
|      ! 0 |  5889 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5890 | `					}else{` |
|        - |  5891 | `						/* Push method name on the stack */` |
|       74 |  5892 | `						PH7_MemObjRelease(pTos);` |
|       74 |  5893 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       74 |  5894 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5895 | `					}` |
|       74 |  5896 | `					pTos->nIdx = SXU32_HIGH;` |
|       38 |  5897 | `				}else{` |
|        - |  5898 | `					/* Attribute access */` |
|      118 |  5899 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5900 | `					/* Check for special ::class pseudo-constant */` |
|      153 |  5901 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       70 |  5902 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5903 | `						/* ::class returns the fully qualified class name */` |
|        - |  5904 | `						/* Pop the attribute name from the stack */` |
|       60 |  5905 | `						if( !pInstr->p3 ){` |
|       60 |  5906 | `							VmPopOperand(&pTos,1);` |
|       29 |  5907 | `						}` |
|       60 |  5908 | `						PH7_MemObjRelease(pTos);` |
|        - |  5909 | `						/* Load the class name */` |
|       60 |  5910 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  5911 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  5912 | `					}else{` |
|        - |  5913 | `						/* Extract the target attribute */` |
|       60 |  5914 | `						if( sName.nByte > 0 ){` |
|       60 |  5915 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       29 |  5916 | `						}` |
|       60 |  5917 | `						if( pAttr == 0 ){` |
|        - |  5918 | `							/* No such attribute,load null */` |
|      ! 0 |  5919 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5920 | `								&pClass->sName,&sName);` |
|        - |  5921 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5922 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5923 | `						}` |
|        - |  5924 | `						/* Pop the attribute name from the stack */` |
|       60 |  5925 | `						if( !pInstr->p3 ){` |
|       48 |  5926 | `							VmPopOperand(&pTos,1);` |
|       23 |  5927 | `						}` |
|       60 |  5928 | `						PH7_MemObjRelease(pTos);` |
|       60 |  5929 | `						pTos->nIdx = SXU32_HIGH;` |
|       60 |  5930 | `						if( pAttr ){` |
|       60 |  5931 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5932 | `								/* Access to a non static attribute */` |
|      ! 0 |  5933 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5934 | `									&pClass->sName,&pAttr->sName` |
|        - |  5935 | `									);` |
|      ! 0 |  5936 | `							}else{` |
|        - |  5937 | `								ph7_value *pValue;` |
|        - |  5938 | `								/* Check if the access to the attribute is allowed */` |
|       60 |  5939 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  5940 | `									/* Load the desired attribute */` |
|       56 |  5941 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       56 |  5942 | `									if( pValue ){` |
|       56 |  5943 | `										PH7_MemObjLoad(pValue,pTos);` |
|       56 |  5944 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5945 | `											/* Load index number */` |
|       14 |  5946 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5947 | `										}` |
|       27 |  5948 | `									}` |
|       29 |  5949 | `								}else{` |
|        - |  5950 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  5951 | `									char zMsg[256];` |
|        5 |  5952 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  5953 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  5954 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  5955 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  5956 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  5957 | `									}else{` |
|      ! 0 |  5958 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  5959 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  5960 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  5961 | `									}` |
|        5 |  5962 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  5963 | `									goto Abort;` |
|        - |  5964 | `								}` |
|        - |  5965 | `							}` |
|       27 |  5966 | `						}` |
|        - |  5967 | `					}` |
|        - |  5968 | `				}` |
|      186 |  5969 | `				if( pThis ){` |
|        - |  5970 | `					/* Safely unreference the object */` |
|        5 |  5971 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  5972 | `				}` |
|        - |  5973 | `			}` |
|       94 |  5974 | `		}else{` |
|        - |  5975 | `			/* Pop operands */` |
|      ! 0 |  5976 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5977 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5978 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5979 | `			}` |
|      ! 0 |  5980 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5981 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5982 | `		}` |
|        - |  5983 | `	}` |
|     4690 |  5984 | `	break;` |
|        - |  5985 | `					}` |
|        - |  5986 | `/*` |
|        - |  5987 | ` * OP_NEW P1 * * *` |
|        - |  5988 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5989 | ` */` |
|      347 |  5990 | `case PH7_OP_NEW: {` |
|      696 |  5991 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      696 |  5992 | `	ph7_class *pClass = 0;` |
|        - |  5993 | `	ph7_class_instance *pNew;` |
|      696 |  5994 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5995 | `		/* Try to extract the desired class */` |
|     1043 |  5996 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      694 |  5997 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      347 |  5998 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5999 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6000 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6001 | `	}` |
|      696 |  6002 | `	if( pClass == 0 ){` |
|        - |  6003 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6004 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6005 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6006 | `			);` |
|        - |  6007 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6008 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6009 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6010 | `			/* Pop given arguments */` |
|      ! 0 |  6011 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6012 | `		}` |
|      ! 0 |  6013 | `		goto Abort;` |
|      ! 0 |  6014 | `	}else{` |
|        - |  6015 | `		ph7_class_method *pCons;` |
|        - |  6016 | `		/* Create a new class instance */` |
|      696 |  6017 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      696 |  6018 | `		if( pNew == 0 ){` |
|      ! 0 |  6019 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6020 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6021 | `				&pClass->sName` |
|        - |  6022 | `			);` |
|      ! 0 |  6023 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6024 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6025 | `				/* Pop given arguments */` |
|      ! 0 |  6026 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6027 | `			}` |
|      ! 0 |  6028 | `			break;` |
|        - |  6029 | `		}` |
|        - |  6030 | `		/* Check if a constructor is available */` |
|      696 |  6031 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      696 |  6032 | `		if( pCons == 0 ){` |
|      560 |  6033 | `			SyString *pName = &pClass->sName;` |
|        - |  6034 | `			/* Check for a constructor with the same base class name */` |
|      560 |  6035 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      279 |  6036 | `		}` |
|      696 |  6037 | `		if( pCons ){` |
|        - |  6038 | `			/* Call the class constructor */` |
|      138 |  6039 | `			SySetReset(&aArg);` |
|      264 |  6040 | `			while( pArg < pTos ){` |
|      128 |  6041 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      128 |  6042 | `				pArg++;` |
|        2 |  6043 | `			}` |
|      138 |  6044 | `			if( pVm->bErrReport ){` |
|        - |  6045 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6046 | `				sxu32 n;` |
|       57 |  6047 | `				n = SySetUsed(&aArg);` |
|        - |  6048 | `				/* Emit a notice for missing arguments */` |
|      101 |  6049 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6050 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6051 | `					if( pFuncArg ){` |
|       45 |  6052 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6053 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6054 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6055 | `						}` |
|       22 |  6056 | `					}` |
|       45 |  6057 | `					n++;` |
|        1 |  6058 | `				}` |
|       28 |  6059 | `			}` |
|      138 |  6060 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6061 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      138 |  6062 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6063 | `				pNew->iRef = 1;` |
|      ! 0 |  6064 | `			}` |
|       68 |  6065 | `		}` |
|      696 |  6066 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6067 | `			/* Pop given arguments */` |
|      120 |  6068 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       59 |  6069 | `		}` |
|      696 |  6070 | `		PH7_MemObjRelease(pTos);` |
|      696 |  6071 | `		pTos->x.pOther = pNew;` |
|      696 |  6072 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6073 | `	}` |
|      696 |  6074 | `	break;` |
|        - |  6075 | `				 }` |
|        - |  6076 | `/*` |
|        - |  6077 | ` * OP_CLONE * * *` |
|        - |  6078 | ` * Perfome a clone operation.` |
|        - |  6079 | ` */` |
|       23 |  6080 | `case PH7_OP_CLONE: {` |
|        - |  6081 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6082 | `#ifdef UNTRUST` |
|        - |  6083 | `	if( pTos < pStack ){` |
|        - |  6084 | `		goto Abort;` |
|        - |  6085 | `	}` |
|        - |  6086 | `#endif` |
|        - |  6087 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  6088 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6089 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6090 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6091 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6092 | `		break;` |
|        - |  6093 | `	}` |
|        - |  6094 | `	/* Point to the source */` |
|       44 |  6095 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6096 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       44 |  6097 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6098 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6099 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6100 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6101 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6102 | `		break;` |
|        - |  6103 | `	}` |
|        - |  6104 | `	/* Perform the clone operation */` |
|       44 |  6105 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  6106 | `	PH7_MemObjRelease(pTos);` |
|       44 |  6107 | `	if( pClone == 0 ){` |
|      ! 0 |  6108 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6109 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6110 | `	}else{` |
|        - |  6111 | `		/* Load the cloned object */` |
|       44 |  6112 | `		pTos->x.pOther = pClone;` |
|       44 |  6113 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6114 | `	}` |
|       44 |  6115 | `	break;` |
|        - |  6116 | `				   }` |
|        - |  6117 | `/*` |
|        - |  6118 | ` * OP_SWITCH * * P3` |
|        - |  6119 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6120 | ` */` |
|       21 |  6121 | `case PH7_OP_SWITCH: {` |
|       44 |  6122 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6123 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6124 | `	ph7_value sValue,sCaseValue;` |
|        - |  6125 | `	sxu32 n,nEntry;` |
|        - |  6126 | `#ifdef UNTRUST` |
|        - |  6127 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6128 | `		goto Abort;` |
|        - |  6129 | `	}` |
|        - |  6130 | `#endif` |
|        - |  6131 | `	/* Point to the case table  */` |
|       44 |  6132 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       44 |  6133 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6134 | `	/* Select the appropriate case block to execute */` |
|       44 |  6135 | `	PH7_MemObjInit(pVm,&sValue);` |
|       44 |  6136 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      102 |  6137 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      102 |  6138 | `		pCase = &aCase[n];` |
|      102 |  6139 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6140 | `		/* Execute the case expression first */` |
|      102 |  6141 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6142 | `		/* Compare the two expression */` |
|      102 |  6143 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      102 |  6144 | `		PH7_MemObjRelease(&sValue);` |
|      102 |  6145 | `		PH7_MemObjRelease(&sCaseValue);` |
|      102 |  6146 | `		if( rc == 0 ){` |
|        - |  6147 | `			/* Value match,jump to this block */` |
|       44 |  6148 | `			pc = pCase->nStart - 1;` |
|       44 |  6149 | `			break;` |
|        - |  6150 | `		}` |
|       31 |  6151 | `	}` |
|       44 |  6152 | `	VmPopOperand(&pTos,1);` |
|       44 |  6153 | `	if( n >= nEntry ){` |
|        - |  6154 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  6155 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  6156 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  6157 | `		}else{` |
|        - |  6158 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6159 | `			pc = pSwitch->nOut - 1;` |
|        - |  6160 | `		}` |
|      ! 0 |  6161 | `	}` |
|       44 |  6162 | `	break;` |
|        - |  6163 | `					}` |
|        - |  6164 | `/*` |
|        - |  6165 | ` * OP_YIELD P1 P2 *` |
|        - |  6166 | ` *  Yield a value from a generator function.` |
|        - |  6167 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  6168 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  6169 | ` */` |
|       28 |  6170 | `case PH7_OP_YIELD: {` |
|        - |  6171 | `	ph7_generator *pGen;` |
|       58 |  6172 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  6173 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  6174 | `		goto Abort;` |
|        - |  6175 | `	}` |
|       58 |  6176 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       58 |  6177 | `	if( pInstr->iP2 ){` |
|        - |  6178 | `		/* yield $key => $value: value on top, key below */` |
|        - |  6179 | `#ifdef UNTRUST` |
|        - |  6180 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  6181 | `#endif` |
|        7 |  6182 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  6183 | `		VmPopOperand(&pTos, 1);` |
|        7 |  6184 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  6185 | `		VmPopOperand(&pTos, 1);` |
|        - |  6186 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  6187 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  6188 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  6189 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  6190 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  6191 | `			}` |
|        1 |  6192 | `		}` |
|       55 |  6193 | `	}else if( pInstr->iP1 ){` |
|        - |  6194 | `		/* yield $value */` |
|        - |  6195 | `#ifdef UNTRUST` |
|        - |  6196 | `		if( pTos < pStack ) goto Abort;` |
|        - |  6197 | `#endif` |
|       52 |  6198 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       52 |  6199 | `		VmPopOperand(&pTos, 1);` |
|        - |  6200 | `		/* Auto-increment key */` |
|       52 |  6201 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       52 |  6202 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       52 |  6203 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       27 |  6204 | `	}else{` |
|        - |  6205 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  6206 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  6207 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  6208 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  6209 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  6210 | `	}` |
|        - |  6211 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       58 |  6212 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       58 |  6213 | `	goto Suspend;` |
|        - |  6214 |  |
|        - |  6215 | `/*` |
|        - |  6216 | ` * OP_CALL P1 * *` |
|        - |  6217 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  6218 | ` *  function on the stack.` |
|        - |  6219 | ` */` |
|   301689 |  6220 | `case PH7_OP_CALL: {` |
|   603424 |  6221 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  6222 | `	ph7_value *pArg;` |
|   603424 |  6223 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   603424 |  6224 | `	pArg = &pTos[-nCallArgs];` |
|        - |  6225 | `	SyHashEntry *pEntry;` |
|        - |  6226 | `	SyString sName;` |
|        - |  6227 | `	/* Extract function name */` |
|   603424 |  6228 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  6229 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6230 | `			ph7_value sResult;` |
|      ! 0 |  6231 | `			SySetReset(&aArg);` |
|      ! 0 |  6232 | `			while( pArg < pTos ){` |
|      ! 0 |  6233 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  6234 | `				pArg++;` |
|      ! 0 |  6235 | `			}` |
|      ! 0 |  6236 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  6237 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  6238 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  6239 | `			SySetReset(&aArg);` |
|        - |  6240 | `			/* Pop given arguments */` |
|      ! 0 |  6241 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6242 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6243 | `			}` |
|        - |  6244 | `			/* Copy result */` |
|      ! 0 |  6245 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  6246 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6247 | `		}else{` |
|        3 |  6248 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  6249 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6250 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  6251 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  6252 | `			}else{` |
|        - |  6253 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  6254 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  6255 | `			}` |
|        - |  6256 | `			/* Pop given arguments */` |
|        3 |  6257 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6258 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6259 | `			}` |
|        - |  6260 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6261 | `			PH7_MemObjRelease(pTos);` |
|        - |  6262 | `		}` |
|   301411 |  6263 | `		break;` |
|        - |  6264 | `	}` |
|   603422 |  6265 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  6266 | `	/* Check for a compiled function first.` |
|        - |  6267 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  6268 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   603422 |  6269 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  6270 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  6271 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  6272 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  6273 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  6274 | `	 * function calls inside namespaces. */` |
|   603422 |  6275 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6276 | `		const char *zFunc;` |
|        - |  6277 | `		const char *zEnd;` |
|        - |  6278 | `		const char *z;` |
|        - |  6279 | `		SyString sGlobal;` |
|       18 |  6280 | `		zFunc = sName.zString;` |
|       18 |  6281 | `		zEnd  = zFunc + sName.nByte;` |
|       18 |  6282 | `		z = zEnd;` |
|        - |  6283 | `		/* Find last namespace separator */` |
|      154 |  6284 | `		while( z > zFunc ){` |
|      154 |  6285 | `			if( z[-1] == '\\' ){` |
|       18 |  6286 | `				break;` |
|        - |  6287 | `			}` |
|      138 |  6288 | `			z--;` |
|        2 |  6289 | `		}` |
|       18 |  6290 | `		if( z > zFunc && z < zEnd ){` |
|        - |  6291 | `			/* Retry lookup using the unqualified/global function name */` |
|       18 |  6292 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       18 |  6293 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        8 |  6294 | `		}` |
|        8 |  6295 | `	}` |
|   603422 |  6296 | `	if( pEntry ){` |
|        - |  6297 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  6298 | `		ph7_class_instance *pThis;` |
|        - |  6299 | `		ph7_value *pFrameStack;` |
|        - |  6300 | `		ph7_vm_func *pVmFunc;` |
|        - |  6301 | `		ph7_class *pSelf;` |
|        - |  6302 | `		VmFrame *pFrame;` |
|        - |  6303 | `		ph7_value *pObj;` |
|        - |  6304 | `		VmSlot sArg;` |
|        - |  6305 | `		sxu32 n;` |
|        - |  6306 | `		/* initialize fields */` |
|    13582 |  6307 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    13582 |  6308 | `		pThis = 0;` |
|    13582 |  6309 | `		pSelf = 0;` |
|    13582 |  6310 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  6311 | `			ph7_class_method *pMeth;` |
|        - |  6312 | `			/* Class method call */` |
|     2080 |  6313 | `			ph7_value *pTarget = &pTos[-1];` |
|     2080 |  6314 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  6315 | `				/* Extract the 'this' pointer */` |
|     2080 |  6316 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  6317 | `					/* Instance already loaded */` |
|     2002 |  6318 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2002 |  6319 | `					pThis->iRef++;` |
|     2002 |  6320 | `					pSelf = pThis->pClass;` |
|     1000 |  6321 | `				}` |
|     2080 |  6322 | `				if( pSelf == 0 ){` |
|       80 |  6323 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  6324 | `						/* "Late Static Binding" class name */` |
|      110 |  6325 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       36 |  6326 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       36 |  6327 | `					}` |
|       80 |  6328 | `					if( pSelf == 0 ){` |
|       19 |  6329 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        9 |  6330 | `					}` |
|       39 |  6331 | `				}` |
|     2080 |  6332 | `				if( pThis == 0  ){` |
|       80 |  6333 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       80 |  6334 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       80 |  6335 | `					if( pFrameLocal->pParent ){` |
|        - |  6336 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       64 |  6337 | `						pThis = pFrameLocal->pThis;` |
|       64 |  6338 | `						if( pThis ){` |
|       19 |  6339 | `							pThis->iRef++;` |
|        9 |  6340 | `						}` |
|       31 |  6341 | `					}` |
|       39 |  6342 | `				}` |
|     2080 |  6343 | `				VmPopOperand(&pTos,1);` |
|     2080 |  6344 | `				PH7_MemObjRelease(pTos);` |
|        - |  6345 | `				/* Synchronize pointers */` |
|     2080 |  6346 | `				pArg = &pTos[-nCallArgs];` |
|        - |  6347 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  6348 | `				 * user have already computed the random generated unique class method name` |
|        - |  6349 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  6350 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  6351 | `				 */` |
|     2080 |  6352 | `				while( pArg < pStack ){` |
|      ! 0 |  6353 | `					pArg++;` |
|      ! 0 |  6354 | `				}` |
|     2080 |  6355 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  6356 | `					/* Check if the call is allowed */` |
|     2080 |  6357 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2080 |  6358 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  6359 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  6360 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  6361 | `							char zMsg[256];` |
|      ! 0 |  6362 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6363 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  6364 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  6365 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  6366 | `							/* Pop given arguments */` |
|      ! 0 |  6367 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  6368 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6369 | `							}` |
|      ! 0 |  6370 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6371 | `							goto Abort;` |
|        - |  6372 | `						}` |
|        6 |  6373 | `					}` |
|     1039 |  6374 | `				}` |
|     1039 |  6375 | `			}` |
|     1039 |  6376 | `		}` |
|        - |  6377 | `		/* Check The recursion limit */` |
|    13582 |  6378 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  6379 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6380 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  6381 | `				&pVmFunc->sName);` |
|        - |  6382 | `			/* Pop given arguments */` |
|        3 |  6383 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6384 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6385 | `			}` |
|        - |  6386 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  6387 | `			PH7_MemObjRelease(pTos);` |
|       12 |  6388 | `			break;` |
|        - |  6389 | `		}` |
|    13580 |  6390 | `		if( pVmFunc->pNextName ){` |
|        - |  6391 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  6392 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  6393 | `		}` |
|    13580 |  6394 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  6395 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  6396 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  6397 | `			ph7_generator *pGenerator;` |
|        - |  6398 | `			ph7_class_instance *pGenObj;` |
|        - |  6399 | `			ph7_value *pCtxAttr;` |
|        - |  6400 | `			SyString sAttrName;` |
|        - |  6401 | `			ph7_value **apCallArgs;` |
|        - |  6402 | `			int nGenArgs, iArg;` |
|        - |  6403 | `			/* Collect arguments from the operand stack */` |
|       20 |  6404 | `			nGenArgs = (int)(pTos - pArg);` |
|       20 |  6405 | `			apCallArgs = 0;` |
|       20 |  6406 | `			if( nGenArgs > 0 ){` |
|        8 |  6407 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        2 |  6408 | `					nGenArgs * sizeof(ph7_value *));` |
|        6 |  6409 | `				if( apCallArgs == 0 ){` |
|        - |  6410 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  6411 | `					nGenArgs = 0;` |
|      ! 0 |  6412 | `				}else{` |
|       12 |  6413 | `					for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  6414 | `						apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  6415 | `					}` |
|        - |  6416 | `				}` |
|        2 |  6417 | `			}` |
|        - |  6418 | `			/* Create execution context and generator wrapper */` |
|       20 |  6419 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       20 |  6420 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  6421 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6422 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6423 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6424 | `				break;` |
|        - |  6425 | `			}` |
|       20 |  6426 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       20 |  6427 | `			if( pGenerator == 0 ){` |
|      ! 0 |  6428 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  6429 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  6430 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  6431 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  6432 | `				break;` |
|        - |  6433 | `			}` |
|        - |  6434 | `			/* Set up the frame with arguments, closure env, $this */` |
|       20 |  6435 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       20 |  6436 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       20 |  6437 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       20 |  6438 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       20 |  6439 | `			pExecCtx->pFrame->pParent = 0;` |
|       20 |  6440 | `			if( apCallArgs ){` |
|        6 |  6441 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        2 |  6442 | `			}` |
|       20 |  6443 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  6444 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6445 | `				if( pThis ){` |
|      ! 0 |  6446 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6447 | `				}` |
|      ! 0 |  6448 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6449 | `					goto Abort;` |
|        - |  6450 | `				}` |
|      ! 0 |  6451 | `				break;` |
|        - |  6452 | `			}` |
|        - |  6453 | `			/* Create Generator class instance */` |
|       20 |  6454 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       20 |  6455 | `			if( pGenObj == 0 ){` |
|      ! 0 |  6456 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  6457 | `				break;` |
|        - |  6458 | `			}` |
|        - |  6459 | `			/* Store generator in __ctx attribute */` |
|       20 |  6460 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       20 |  6461 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       20 |  6462 | `			if( pCtxAttr ){` |
|       20 |  6463 | `				pCtxAttr->x.pOther = pGenerator;` |
|       20 |  6464 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|        9 |  6465 | `			}` |
|        - |  6466 | `			/* Pop args and function name, push Generator object */` |
|       20 |  6467 | `			PH7_MemObjRelease(pTos);` |
|       20 |  6468 | `			pTos = &pTos[-nCallArgs];` |
|       20 |  6469 | `			pTos->x.pOther = pGenObj;` |
|       20 |  6470 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       20 |  6471 | `			pGenObj->iRef++;` |
|       20 |  6472 | `			if( pThis ){` |
|      ! 0 |  6473 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6474 | `			}` |
|       20 |  6475 | `			break;` |
|        - |  6476 | `		}` |
|        - |  6477 | `		/* Extract the formal argument set */` |
|    13562 |  6478 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  6479 | `		/* Create a new VM frame  */` |
|    13562 |  6480 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    13562 |  6481 | `		if( rc != SXRET_OK ){` |
|        - |  6482 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6483 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6484 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6485 | `				&pVmFunc->sName);` |
|        - |  6486 | `			/* Pop given arguments */` |
|      ! 0 |  6487 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6488 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6489 | `			}` |
|        - |  6490 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  6491 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6492 | `			break;` |
|        - |  6493 | `		}` |
|    13562 |  6494 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  6495 | `			/* Install the '$this' variable */` |
|        - |  6496 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2018 |  6497 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2018 |  6498 | `			if( pObj ){` |
|        - |  6499 | `				/* Reflect the change */` |
|     2018 |  6500 | `				pObj->x.pOther = pThis;` |
|     2018 |  6501 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1008 |  6502 | `			}` |
|     1008 |  6503 | `		}` |
|    13562 |  6504 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  6505 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  6506 | `			/* Install static variables */` |
|      ! 0 |  6507 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  6508 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  6509 | `				pStatic = &aStatic[n];` |
|      ! 0 |  6510 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  6511 | `					/* Initialize the static variables */` |
|      ! 0 |  6512 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  6513 | `					if( pObj ){` |
|        - |  6514 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  6515 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  6516 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  6517 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  6518 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  6519 | `						}` |
|      ! 0 |  6520 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  6521 | `					}else{` |
|      ! 0 |  6522 | `						continue;` |
|        - |  6523 | `					}` |
|      ! 0 |  6524 | `				}` |
|        - |  6525 | `				/* Install in the current frame */` |
|      ! 0 |  6526 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  6527 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  6528 | `			}` |
|      ! 0 |  6529 | `		}` |
|        - |  6530 | `		/* Push arguments in the local frame */` |
|    13562 |  6531 | `		n = 0;` |
|    36676 |  6532 | `		while( pArg < pTos ){` |
|    23136 |  6533 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  6534 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       21 |  6535 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       21 |  6536 | `				if( pObj ){` |
|        - |  6537 | `					/* Initialize as empty array */` |
|       21 |  6538 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  6539 | `					{` |
|       21 |  6540 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       83 |  6541 | `						while( pArg < pTos ){` |
|        - |  6542 | `							/* Apply type coercion to each element if the variadic has a type hint */` |
|       62 |  6543 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       29 |  6544 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  6545 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  6546 | `								if( xCast ){` |
|       13 |  6547 | `									xCast(pArg);` |
|        6 |  6548 | `								}` |
|        6 |  6549 | `							}` |
|       63 |  6550 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       63 |  6551 | `							pArg++;` |
|        1 |  6552 | `						}` |
|        - |  6553 | `					}` |
|       21 |  6554 | `					sArg.nIdx = pObj->nIdx;` |
|       21 |  6555 | `					sArg.pUserData = 0;` |
|       21 |  6556 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       10 |  6557 | `				}` |
|       21 |  6558 | `				break; /* All remaining args consumed */` |
|        - |  6559 | `			}` |
|    23116 |  6560 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    22960 |  6561 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        9 |  6562 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  6563 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  6564 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  6565 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6566 | `						goto Abort;` |
|        - |  6567 | `					}` |
|      ! 0 |  6568 | `				}` |
|        - |  6569 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  6570 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    22972 |  6571 | `				if( aFormalArg[n].nType > 0` |
|    12062 |  6572 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1150 |  6573 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  6574 | `						/* Argument must be a class instance [i.e: object] */` |
|        8 |  6575 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  6576 | `						ph7_class *pClass;` |
|        - |  6577 | `						/* Try to extract the desired class */` |
|        8 |  6578 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|        8 |  6579 | `						if( pClass ){` |
|        8 |  6580 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  6581 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6582 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6583 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6584 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6585 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6586 | `								}` |
|      ! 0 |  6587 | `							}else{` |
|        - |  6588 | `								/* reuse pThis declared in outer scope */` |
|        8 |  6589 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  6590 | `								/* Make sure the object is an instance of the given class */` |
|        8 |  6591 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  6592 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6593 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  6594 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  6595 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  6596 | `								}` |
|        - |  6597 | `							}` |
|        5 |  6598 | `						}` |
|     1147 |  6599 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6600 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6601 | `						/* Cast to the desired type */` |
|      ! 0 |  6602 | `						xCast(pArg);` |
|      ! 0 |  6603 | `					}` |
|      574 |  6604 | `				}` |
|    22962 |  6605 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  6606 | `					/* Pass by reference */` |
|       54 |  6607 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  6608 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  6609 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  6610 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  6611 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  6612 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  6613 | `						}` |
|        - |  6614 | `						/* Switch to pass by value */` |
|      ! 0 |  6615 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  6616 | `					}else{` |
|        - |  6617 | `						SyHashEntry *pRefEntry;` |
|        - |  6618 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  6619 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  6620 | `						if( pRefEntry == 0 ){` |
|       80 |  6621 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  6622 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  6623 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  6624 | `							sArg.pUserData = 0;` |
|       54 |  6625 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  6626 | `						}` |
|       54 |  6627 | `						pObj = 0;` |
|        - |  6628 | `					}` |
|       28 |  6629 | `				}else{` |
|        - |  6630 | `					/* Pass by value,make a copy of the given argument */` |
|    22910 |  6631 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  6632 | `				}` |
|    11482 |  6633 | `			}else{` |
|        - |  6634 | `				char zName[32];` |
|        - |  6635 | `				SyString sArgName;` |
|        - |  6636 | `				/* Set a dummy name */` |
|      156 |  6637 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      156 |  6638 | `				sArgName.zString = zName;` |
|        - |  6639 | `				/* Annonymous argument */` |
|      156 |  6640 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6641 | `			}` |
|    23116 |  6642 | `			if( pObj ){` |
|    23064 |  6643 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6644 | `				/* Insert argument index  */` |
|    23064 |  6645 | `				sArg.nIdx = pObj->nIdx;` |
|    23064 |  6646 | `				sArg.pUserData = 0;` |
|    23064 |  6647 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    11531 |  6648 | `			}` |
|    23116 |  6649 | `			PH7_MemObjRelease(pArg);` |
|    23116 |  6650 | `			pArg++;` |
|    23116 |  6651 | `			++n;` |
|        2 |  6652 | `		}` |
|        - |  6653 | `		/* Set up closure environment */` |
|    13562 |  6654 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6655 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6656 | `			ph7_value *pValue;` |
|        - |  6657 | `			sxu32 iEnv;` |
|       11 |  6658 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       31 |  6659 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       21 |  6660 | `				pEnv = &aEnv[iEnv];` |
|       21 |  6661 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6662 | `					/* Do not install null value */` |
|       11 |  6663 | `					continue;` |
|        - |  6664 | `				}` |
|       11 |  6665 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       11 |  6666 | `				if( pValue == 0 ){` |
|      ! 0 |  6667 | `					continue;` |
|        - |  6668 | `				}` |
|        - |  6669 | `				/* Invalidate any prior representation */` |
|       11 |  6670 | `				PH7_MemObjRelease(pValue);` |
|        - |  6671 | `				/* Duplicate bound variable value */` |
|       11 |  6672 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        6 |  6673 | `			}` |
|        5 |  6674 | `		}` |
|        - |  6675 | `		/* Process default values for remaining formal parameters */` |
|    15580 |  6676 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2046 |  6677 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  6678 | `				/* Variadic parameter with no extra args — create empty array */` |
|       27 |  6679 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       27 |  6680 | `				if( pObj ){` |
|       27 |  6681 | `					PH7_MemObjToHashmap(pObj);` |
|       27 |  6682 | `					sArg.nIdx = pObj->nIdx;` |
|       27 |  6683 | `					sArg.pUserData = 0;` |
|       27 |  6684 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       13 |  6685 | `				}` |
|       27 |  6686 | `				n++;` |
|       27 |  6687 | `				break; /* Variadic is always last */` |
|        - |  6688 | `			}` |
|     2020 |  6689 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2014 |  6690 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2014 |  6691 | `				if( pObj ){` |
|        - |  6692 | `					/* Evaluate the default value and extract it's result */` |
|     2014 |  6693 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2014 |  6694 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6695 | `						goto Abort;` |
|        - |  6696 | `					}` |
|        - |  6697 | `					/* Insert argument index */` |
|     2014 |  6698 | `					sArg.nIdx = pObj->nIdx;` |
|     2014 |  6699 | `					sArg.pUserData = 0;` |
|     2014 |  6700 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6701 | `					/* Make sure the default argument is of the correct type */` |
|     2014 |  6702 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6703 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6704 | `						/* Cast to the desired type */` |
|      ! 0 |  6705 | `						xCast(pObj);` |
|      ! 0 |  6706 | `					}` |
|     1006 |  6707 | `				}` |
|     1006 |  6708 | `			}` |
|     2020 |  6709 | `			++n;` |
|        2 |  6710 | `		}` |
|        - |  6711 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6712 | `		 * does not return anything.` |
|        - |  6713 | `		 */` |
|    13562 |  6714 | `		PH7_MemObjRelease(pTos);` |
|    13562 |  6715 | `		pTos = &pTos[-nCallArgs];` |
|        - |  6716 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    13562 |  6717 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    13562 |  6718 | `		if( pFrameStack == 0 ){` |
|        - |  6719 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6720 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6721 | `				&pVmFunc->sName);` |
|      ! 0 |  6722 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  6723 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  6724 | `			}` |
|      ! 0 |  6725 | `			break;` |
|        - |  6726 | `		}` |
|    13562 |  6727 | `		if( pSelf ){` |
|        - |  6728 | `			/* Push class name */` |
|     2078 |  6729 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1038 |  6730 | `		}` |
|        - |  6731 | `		/* Increment nesting level */` |
|    13562 |  6732 | `		pVm->nRecursionDepth++;` |
|        - |  6733 | `		/* Execute function body */` |
|    13562 |  6734 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|        - |  6735 | `		/* Decrement nesting level */` |
|    13562 |  6736 | `		pVm->nRecursionDepth--;` |
|    13562 |  6737 | `		if( pSelf ){` |
|        - |  6738 | `			/* Pop class name */` |
|     2078 |  6739 | `			(void)SySetPop(&pVm->aSelf);` |
|     1038 |  6740 | `		}` |
|        - |  6741 | `		/* Cleanup the mess left behind */` |
|    13562 |  6742 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6743 | `			/* Return by reference,reflect that */` |
|        9 |  6744 | `			if( n != SXU32_HIGH ){` |
|        9 |  6745 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6746 | `				sxu32 i;` |
|        - |  6747 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6748 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6749 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6750 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6751 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6752 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6753 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6754 | `								&pVmFunc->sName);` |
|      ! 0 |  6755 | `						}` |
|      ! 0 |  6756 | `						n = SXU32_HIGH;` |
|      ! 0 |  6757 | `						break;` |
|        - |  6758 | `					}` |
|        3 |  6759 | `				}` |
|        5 |  6760 | `			}else{` |
|      ! 0 |  6761 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6762 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6763 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6764 | `						&pVmFunc->sName);` |
|      ! 0 |  6765 | `				}` |
|        - |  6766 | `			}` |
|        9 |  6767 | `			pTos->nIdx = n;` |
|        4 |  6768 | `		}` |
|        - |  6769 | `		/* Cleanup the mess left behind */` |
|    13562 |  6770 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6771 | `			/* An exception was throw in this frame */` |
|       12 |  6772 | `			pFrame = pFrame->pParent;` |
|       12 |  6773 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6774 | `				/* Pop the resutlt */` |
|       10 |  6775 | `				VmPopOperand(&pTos,1);` |
|        - |  6776 | `				/* Jump to this destination */` |
|       10 |  6777 | `				pc = pFrame->iExceptionJump - 1;` |
|       10 |  6778 | `				rc = PH7_OK;` |
|        6 |  6779 | `			}else{` |
|        3 |  6780 | `				if( pFrame->pParent ){` |
|        3 |  6781 | `					rc = PH7_EXCEPTION;` |
|        2 |  6782 | `				}else{` |
|        - |  6783 | `					/* Continue normal execution */` |
|      ! 0 |  6784 | `					rc = PH7_OK;` |
|        - |  6785 | `				}` |
|        - |  6786 | `			}` |
|        5 |  6787 | `		}` |
|        - |  6788 | `		/* Free the operand stack */` |
|    13562 |  6789 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6790 | `		/* Leave the frame */` |
|    13562 |  6791 | `		VmLeaveFrame(&(*pVm));` |
|    13562 |  6792 | `		if( rc == PH7_ABORT ){` |
|        - |  6793 | `			/* Abort processing immeditaley */` |
|        9 |  6794 | `			goto Abort;` |
|    13554 |  6795 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6796 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  6797 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  6798 | `			 * overwriting the state saved by the inner level.` |
|        - |  6799 | `			 * pTos points to the result slot (not yet written).` |
|        - |  6800 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  6801 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  6802 | `			goto Suspend;` |
|    13516 |  6803 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6804 | `			goto Exception;` |
|        - |  6805 | `		}` |
|     6758 |  6806 | `	}else{` |
|        - |  6807 | `		ph7_user_func *pFunc;` |
|        - |  6808 | `		ph7_context sCtx;` |
|        - |  6809 | `		ph7_value sRet;` |
|        - |  6810 | `		/* Look for an installed foreign function.` |
|        - |  6811 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6812 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6813 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6814 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6815 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6816 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   589842 |  6817 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   589842 |  6818 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6819 | `			/* Compiler-qualified: try short name as global fallback */` |
|       18 |  6820 | `			const char *zShort = sName.zString;` |
|        - |  6821 | `			sxu32 i;` |
|      262 |  6822 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      246 |  6823 | `				if( sName.zString[i] == '\\' ){` |
|       22 |  6824 | `					zShort = &sName.zString[i + 1];` |
|       10 |  6825 | `				}` |
|      124 |  6826 | `			}` |
|       18 |  6827 | `			if( zShort != sName.zString ){` |
|       18 |  6828 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       18 |  6829 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        8 |  6830 | `			}` |
|        8 |  6831 | `		}` |
|   589842 |  6832 | `		if( pEntry == 0 ){` |
|        - |  6833 | `			/* Call to undefined function */` |
|        5 |  6834 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6835 | `			/* Pop given arguments */` |
|        5 |  6836 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6837 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6838 | `			}` |
|        - |  6839 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6840 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6841 | `			break;` |
|        - |  6842 | `		}` |
|   589838 |  6843 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6844 | `		/* Start collecting function arguments */` |
|   589838 |  6845 | `		SySetReset(&aArg);` |
|  1583912 |  6846 | `		while( pArg < pTos ){` |
|   994076 |  6847 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   994076 |  6848 | `			pArg++;` |
|        2 |  6849 | `		}` |
|        - |  6850 | `		/* Assume a null return value */` |
|   589838 |  6851 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6852 | `		/* Init the call context */` |
|   589838 |  6853 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6854 | `		/* Call the foreign function */` |
|   589838 |  6855 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6856 | `		/* Release the call context */` |
|   589838 |  6857 | `		VmReleaseCallContext(&sCtx);` |
|   589838 |  6858 | `		if( rc == PH7_ABORT ){` |
|      471 |  6859 | `			goto Abort;` |
|   589368 |  6860 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  6861 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  6862 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  6863 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6864 | `				/* Exception was NOT caught, propagate */` |
|        5 |  6865 | `				goto Exception;` |
|        - |  6866 | `			}` |
|        - |  6867 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6868 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6869 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6870 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6871 | `			}` |
|        - |  6872 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6873 | `			VmPopOperand(&pTos,1);` |
|        - |  6874 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6875 | `			pFrm = pVm->pFrame;` |
|        7 |  6876 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6877 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6878 | `			}` |
|        7 |  6879 | `			break;` |
|        - |  6880 | `		}` |
|   589358 |  6881 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  6882 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  6883 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  6884 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  6885 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  6886 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  6887 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  6888 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  6889 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  6890 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  6891 | `			}` |
|        - |  6892 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  6893 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  6894 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  6895 | `			goto Suspend;` |
|        - |  6896 | `		}` |
|   589320 |  6897 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6898 | `			/* Pop function name and arguments */` |
|   570522 |  6899 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   285282 |  6900 | `		}` |
|        - |  6901 | `		/* Save foreign function return value */` |
|   589320 |  6902 | `		PH7_MemObjStore(&sRet,pTos);` |
|   589320 |  6903 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6904 | `	}` |
|   602832 |  6905 | `	break;` |
|        - |  6906 | `				  }` |
|        - |  6907 | `/*` |
|        - |  6908 | ` * OP_CONSUME: P1 * *` |
|        - |  6909 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6910 | ` */` |
|    11970 |  6911 | `case PH7_OP_CONSUME: {` |
|    23942 |  6912 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    23942 |  6913 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6914 |  |
|    23942 |  6915 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    23942 |  6916 | `	pCur = pOut;` |
|        - |  6917 | `	/* Start the consume process  */` |
|    47882 |  6918 | `	while( pOut <= pTos ){` |
|        - |  6919 | `		/* Force a string cast */` |
|    23942 |  6920 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6921 | `			PH7_MemObjToString(pOut);` |
|      149 |  6922 | `		}` |
|    23942 |  6923 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6924 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6925 | `			/* Invoke the output consumer callback */` |
|    13402 |  6926 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    13402 |  6927 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    13402 |  6928 | `			SyBlobRelease(&pOut->sBlob);` |
|    13402 |  6929 | `			if( rc == SXERR_ABORT ){` |
|        - |  6930 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6931 | `				goto Abort;` |
|        - |  6932 | `			}` |
|     6700 |  6933 | `		}` |
|    23942 |  6934 | `		pOut++;` |
|        2 |  6935 | `	}` |
|    23942 |  6936 | `	pTos = &pCur[-1];` |
|    23940 |  6937 | `	break;` |
|        - |  6938 | `					 }` |
|        - |  6939 |  |
|        - |  6940 | `		} /* Switch() */` |
| 10160668 |  6941 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6942 | `	} /* For(;;) */` |
|    16516 |  6943 | `Done:` |
|    33034 |  6944 | `	SySetRelease(&aArg);` |
|    33034 |  6945 | `	return SXRET_OK;` |
|       66 |  6946 | `Suspend:` |
|      134 |  6947 | `	SySetRelease(&aArg);` |
|      134 |  6948 | `	return PH7_SUSPEND;` |
|      245 |  6949 | `Abort:` |
|      491 |  6950 | `	SySetRelease(&aArg);` |
|     1697 |  6951 | `	while( pTos >= pStack ){` |
|     1207 |  6952 | `		PH7_MemObjRelease(pTos);` |
|     1207 |  6953 | `		pTos--;` |
|        1 |  6954 | `	}` |
|      491 |  6955 | `	return PH7_ABORT;` |
|        3 |  6956 | `Exception:` |
|        8 |  6957 | `	SySetRelease(&aArg);` |
|       22 |  6958 | `	while( pTos >= pStack ){` |
|       16 |  6959 | `		PH7_MemObjRelease(pTos);` |
|       16 |  6960 | `		pTos--;` |
|        2 |  6961 | `	}` |
|        8 |  6962 | `	return PH7_EXCEPTION;` |
|    16832 |  6963 |  |
|        - |  6964 | `/*` |
|        - |  6965 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6966 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6967 | ` * See block-comment on that function for additional information.` |
|        - |  6968 | ` */` |
|    15624 |  6969 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6970 |  |
|        - |  6971 | `	ph7_value *pStack;` |
|        - |  6972 | `	sxi32 rc;` |
|        - |  6973 | `	/* Allocate a new operand stack */` |
|    15626 |  6974 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    15626 |  6975 | `	if( pStack == 0 ){` |
|      ! 0 |  6976 | `		return SXERR_MEM;` |
|        - |  6977 | `	}` |
|        - |  6978 | `	/* Execute the program */` |
|    15626 |  6979 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  6980 | `	/* Free the operand stack */` |
|    15626 |  6981 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6982 | `	/* Execution result */` |
|    15626 |  6983 | `	return rc;` |
|     7814 |  6984 |  |
|        - |  6985 | `/*` |
|        - |  6986 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6987 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6988 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6989 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6990 | ` * execution ends.` |
|        - |  6991 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6992 | ` * additional information.` |
|        - |  6993 | ` */` |
|     2346 |  6994 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6995 |  |
|        - |  6996 | `	VmShutdownCB *pEntry;` |
|        - |  6997 | `	ph7_value *apArg[10];` |
|        - |  6998 | `	sxu32 n,nEntry;` |
|        - |  6999 | `	int i;` |
|        - |  7000 | `	/* Point to the stack of registered callbacks */` |
|     2348 |  7001 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25808 |  7002 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    23462 |  7003 | `		apArg[i] = 0;` |
|    11732 |  7004 | `	}` |
|     2350 |  7005 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  7006 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7007 | `		if( pEntry ){` |
|        - |  7008 | `			/* Prepare callback arguments if any */` |
|        3 |  7009 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  7010 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  7011 | `					break;` |
|        - |  7012 | `				}` |
|      ! 0 |  7013 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  7014 | `			}` |
|        - |  7015 | `			/* Invoke the callback */` |
|        3 |  7016 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  7017 | `			/*` |
|        - |  7018 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  7019 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  7020 | `			 */` |
|        3 |  7021 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  7022 | `			if( pEntry ){` |
|        3 |  7023 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  7024 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  7025 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  7026 | `				}` |
|        1 |  7027 | `			}` |
|        1 |  7028 | `		}` |
|        2 |  7029 | `	}` |
|     2348 |  7030 | `	SySetReset(&pVm->aShutdown);` |
|     2348 |  7031 |  |
|        - |  7032 | `/*` |
|        - |  7033 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  7034 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  7035 | ` * See block-comment on that function for additional information.` |
|        - |  7036 | ` */` |
|     2354 |  7037 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  7038 |  |
|        - |  7039 | `	/* Make sure we are ready to execute this program */` |
|     2356 |  7040 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  7041 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  7042 | `	}` |
|        - |  7043 | `	/* Set the execution magic number  */` |
|     2356 |  7044 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  7045 | `	/* Execute the program */` |
|     2356 |  7046 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  7047 | `	/* Invoke any shutdown callbacks */` |
|     2352 |  7048 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  7049 | `	/*` |
|        - |  7050 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  7051 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  7052 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  7053 | `	 */` |
|     2352 |  7054 | `	return SXRET_OK;` |
|     1179 |  7055 |  |
|        - |  7056 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  7057 | `/*` |
|        - |  7058 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  7059 | ` * The context is in CREATED state and ready to be started.` |
|        - |  7060 | ` */` |
|       42 |  7061 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  7062 |  |
|        - |  7063 | `	ph7_exec_ctx *pCtx;` |
|        - |  7064 | `	ph7_value *pStack;` |
|        - |  7065 | `	VmFrame *pFrame;` |
|       44 |  7066 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       44 |  7067 | `	if( pCtx == 0 ){` |
|      ! 0 |  7068 | `		return 0;` |
|        - |  7069 | `	}` |
|       44 |  7070 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       44 |  7071 | `	pCtx->pVm = pVm;` |
|       44 |  7072 | `	pCtx->pFunc = pFunc;` |
|       44 |  7073 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       44 |  7074 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       44 |  7075 | `	pCtx->pc = 0;` |
|       44 |  7076 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       44 |  7077 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  7078 | `	/* Allocate a private operand stack */` |
|       44 |  7079 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       44 |  7080 | `	if( pStack == 0 ){` |
|      ! 0 |  7081 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7082 | `		return 0;` |
|        - |  7083 | `	}` |
|       44 |  7084 | `	pCtx->pStack = pStack;` |
|        - |  7085 | `	/* Create a detached frame for the fiber */` |
|       44 |  7086 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       44 |  7087 | `	if( pFrame == 0 ){` |
|      ! 0 |  7088 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  7089 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  7090 | `		return 0;` |
|        - |  7091 | `	}` |
|       44 |  7092 | `	pCtx->pFrame = pFrame;` |
|       44 |  7093 | `	return pCtx;` |
|       23 |  7094 |  |
|        - |  7095 | `/*` |
|        - |  7096 | ` * Start executing a fiber context for the first time.` |
|        - |  7097 | ` */` |
|       42 |  7098 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  7099 |  |
|        - |  7100 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7101 | `	sxi32 rc;` |
|       44 |  7102 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7103 | `		return SXERR_INVALID;` |
|        - |  7104 | `	}` |
|        - |  7105 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       44 |  7106 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       44 |  7107 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7108 | `	/* Save and set the active context */` |
|       44 |  7109 | `	pOldCtx = pVm->pActiveCtx;` |
|       44 |  7110 | `	pVm->pActiveCtx = pCtx;` |
|       44 |  7111 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       44 |  7112 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       44 |  7113 | `	pVm->nRecursionDepth++;` |
|        - |  7114 | `	/* Execute from the beginning */` |
|       65 |  7115 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       21 |  7116 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       44 |  7117 | `	pVm->nRecursionDepth--;` |
|        - |  7118 | `	/* Restore the previous context */` |
|       44 |  7119 | `	pVm->pActiveCtx = pOldCtx;` |
|       44 |  7120 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7121 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       42 |  7122 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       42 |  7123 | `		pCtx->pFrame->pParent = 0;` |
|       42 |  7124 | `		if( pResult ){` |
|       24 |  7125 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  7126 | `		}` |
|       42 |  7127 | `		return SXRET_OK;` |
|        - |  7128 | `	}` |
|        - |  7129 | `	/* Detach frame */` |
|        3 |  7130 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  7131 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  7132 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  7133 | `	}` |
|        3 |  7134 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7135 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7136 | `		return PH7_ABORT;` |
|        - |  7137 | `	}` |
|        3 |  7138 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7139 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7140 | `		return PH7_EXCEPTION;` |
|        - |  7141 | `	}` |
|        - |  7142 | `	/* Normal completion */` |
|        3 |  7143 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  7144 | `	if( pResult ){` |
|        3 |  7145 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  7146 | `	}` |
|        3 |  7147 | `	return SXRET_OK;` |
|       23 |  7148 |  |
|        - |  7149 | `/*` |
|        - |  7150 | ` * Resume a suspended fiber context.` |
|        - |  7151 | ` */` |
|       86 |  7152 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  7153 |  |
|        - |  7154 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  7155 | `	sxi32 rc;` |
|       88 |  7156 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  7157 | `		return SXERR_INVALID;` |
|        - |  7158 | `	}` |
|        - |  7159 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  7160 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  7161 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|       88 |  7162 | `	if( pResumeValue ){` |
|       40 |  7163 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  7164 | `	}else{` |
|       50 |  7165 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  7166 | `	}` |
|       88 |  7167 | `	pCtx->nTos++;` |
|        - |  7168 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|       88 |  7169 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       88 |  7170 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  7171 | `	/* Save and set the active context */` |
|       88 |  7172 | `	pOldCtx = pVm->pActiveCtx;` |
|       88 |  7173 | `	pVm->pActiveCtx = pCtx;` |
|       88 |  7174 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       88 |  7175 | `	pVm->nRecursionDepth++;` |
|        - |  7176 | `	/* Resume execution from saved PC */` |
|      131 |  7177 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       43 |  7178 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|       88 |  7179 | `	pVm->nRecursionDepth--;` |
|        - |  7180 | `	/* Restore the previous context */` |
|       88 |  7181 | `	pVm->pActiveCtx = pOldCtx;` |
|       88 |  7182 | `	if( rc == PH7_SUSPEND ){` |
|        - |  7183 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       56 |  7184 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       56 |  7185 | `		pCtx->pFrame->pParent = 0;` |
|       56 |  7186 | `		if( pResult ){` |
|       18 |  7187 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  7188 | `		}` |
|       56 |  7189 | `		return SXRET_OK;` |
|        - |  7190 | `	}` |
|        - |  7191 | `	/* Detach frame */` |
|       34 |  7192 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       34 |  7193 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       34 |  7194 | `		pCtx->pFrame->pParent = 0;` |
|       16 |  7195 | `	}` |
|       34 |  7196 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7197 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7198 | `		return PH7_ABORT;` |
|        - |  7199 | `	}` |
|       34 |  7200 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7201 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  7202 | `		return PH7_EXCEPTION;` |
|        - |  7203 | `	}` |
|        - |  7204 | `	/* Normal completion */` |
|       34 |  7205 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       34 |  7206 | `	if( pResult ){` |
|       20 |  7207 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  7208 | `	}` |
|       34 |  7209 | `	return SXRET_OK;` |
|       45 |  7210 |  |
|        - |  7211 | `/*` |
|        - |  7212 | ` * Release an execution context and all its resources.` |
|        - |  7213 | ` */` |
|        4 |  7214 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  7215 |  |
|        5 |  7216 | `	if( pCtx == 0 ){` |
|      ! 0 |  7217 | `		return;` |
|        - |  7218 | `	}` |
|        5 |  7219 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  7220 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  7221 | `		return;` |
|        - |  7222 | `	}` |
|        5 |  7223 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  7224 | `	/* Release values */` |
|        5 |  7225 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  7226 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  7227 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  7228 | `	if( pCtx->pFrame ){` |
|        - |  7229 | `		VmSlot *aSlot;` |
|        - |  7230 | `		sxu32 n;` |
|        - |  7231 | `		/* Free local variables */` |
|        5 |  7232 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  7233 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  7234 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  7235 | `		}` |
|        - |  7236 | `		/* Remove local references */` |
|        5 |  7237 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  7238 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  7239 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  7240 | `		}` |
|        5 |  7241 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  7242 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  7243 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  7244 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  7245 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  7246 | `		pCtx->pFrame = 0;` |
|        2 |  7247 | `	}` |
|        - |  7248 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  7249 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  7250 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  7251 | `	if( pCtx->pStack ){` |
|        5 |  7252 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  7253 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  7254 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  7255 | `				PH7_MemObjRelease(pTos);` |
|        5 |  7256 | `				pTos--;` |
|        1 |  7257 | `			}` |
|        2 |  7258 | `		}` |
|        5 |  7259 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  7260 | `		pCtx->pStack = 0;` |
|        2 |  7261 | `	}` |
|        - |  7262 | `	/* Free the context itself */` |
|        5 |  7263 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  7264 |  |
|        - |  7265 | `/*` |
|        - |  7266 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  7267 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  7268 | ` */` |
|       90 |  7269 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  7270 |  |
|        - |  7271 | `	ph7_class_instance *pThis;` |
|        - |  7272 | `	SyString sAttr;` |
|        - |  7273 | `	ph7_value *pAttr;` |
|       92 |  7274 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7275 | `		return 0;` |
|        - |  7276 | `	}` |
|       92 |  7277 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  7278 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  7279 | `		return 0;` |
|        - |  7280 | `	}` |
|       92 |  7281 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  7282 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  7283 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  7284 | `		return 0;` |
|        - |  7285 | `	}` |
|       62 |  7286 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  7287 |  |
|        - |  7288 | `/*` |
|        - |  7289 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  7290 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  7291 | ` */` |
|       38 |  7292 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7293 |  |
|       40 |  7294 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  7295 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  7296 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7297 | `			"Cannot suspend outside of a fiber");` |
|        - |  7298 | `	}` |
|       40 |  7299 | `	if( nArg > 0 ){` |
|       40 |  7300 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  7301 | `	}else{` |
|      ! 0 |  7302 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  7303 | `	}` |
|       40 |  7304 | `	return PH7_SUSPEND;` |
|       21 |  7305 |  |
|        - |  7306 | `/*` |
|        - |  7307 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  7308 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  7309 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  7310 | ` */` |
|       24 |  7311 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7312 |  |
|        - |  7313 | `	ph7_class_instance *pThis;` |
|        - |  7314 | `	ph7_value *pAttr;` |
|        - |  7315 | `	SyString sAttrName;` |
|       26 |  7316 | `	if( nArg < 2 ){` |
|      ! 0 |  7317 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7318 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  7319 | `	}` |
|       26 |  7320 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7321 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7322 | `			"Fiber::__construct(): invalid $this");` |
|        - |  7323 | `	}` |
|       26 |  7324 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  7325 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  7326 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7327 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  7328 | `	}` |
|        - |  7329 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  7330 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7331 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7332 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  7333 | `	}` |
|        - |  7334 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  7335 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7336 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7337 | `	if( pAttr ){` |
|       26 |  7338 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  7339 | `	}` |
|       26 |  7340 | `	return PH7_OK;` |
|       14 |  7341 |  |
|        - |  7342 | `/*` |
|        - |  7343 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  7344 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  7345 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  7346 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  7347 | ` */` |
|       24 |  7348 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  7349 | `	ph7_class_instance **ppThis)` |
|        2 |  7350 |  |
|       26 |  7351 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7352 | `	ph7_value *pCallable;` |
|        - |  7353 | `	SyString sAttrName;` |
|       26 |  7354 | `	*ppThis = 0;` |
|       26 |  7355 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  7356 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  7357 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  7358 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  7359 | `		return 0;` |
|        - |  7360 | `	}` |
|       26 |  7361 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7362 | `		/* String callable — look up in user functions with overload support */` |
|        - |  7363 | `		SyString sName;` |
|        - |  7364 | `		SyHashEntry *pEntry;` |
|        - |  7365 | `		ph7_vm_func *pFunc;` |
|       26 |  7366 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  7367 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  7368 | `		if( pEntry == 0 ){` |
|      ! 0 |  7369 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  7370 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  7371 | `			return 0;` |
|        - |  7372 | `		}` |
|       26 |  7373 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  7374 | `		return pFunc;` |
|      ! 0 |  7375 | `	}else{` |
|        - |  7376 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  7377 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7378 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7379 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7380 | `		if( pMethod == 0 ){` |
|      ! 0 |  7381 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7382 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  7383 | `			return 0;` |
|        - |  7384 | `		}` |
|      ! 0 |  7385 | `		*ppThis = pClosure;` |
|      ! 0 |  7386 | `		return &pMethod->sFunc;` |
|        - |  7387 | `	}` |
|       14 |  7388 |  |
|        - |  7389 | `/*` |
|        - |  7390 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  7391 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  7392 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  7393 | ` */` |
|       42 |  7394 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  7395 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  7396 |  |
|       44 |  7397 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  7398 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  7399 | `	sxu32 nFormal, n;` |
|        - |  7400 | `	VmSlot sSlot;` |
|        - |  7401 | `	sxi32 rc;` |
|        - |  7402 | `	/* Install $this for closure/method callables */` |
|       44 |  7403 | `	if( pClosureThis ){` |
|        - |  7404 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  7405 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  7406 | `		if( pObj ){` |
|      ! 0 |  7407 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  7408 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  7409 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  7410 | `		}` |
|      ! 0 |  7411 | `	}` |
|        - |  7412 | `	/* Install static variables */` |
|       44 |  7413 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  7414 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  7415 | `		ph7_value *pVal;` |
|      ! 0 |  7416 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  7417 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  7418 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  7419 | `			if( pVal ){` |
|      ! 0 |  7420 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7421 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  7422 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  7423 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  7424 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  7425 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  7426 | `				}` |
|      ! 0 |  7427 | `			}` |
|      ! 0 |  7428 | `		}` |
|      ! 0 |  7429 | `	}` |
|        - |  7430 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       44 |  7431 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       44 |  7432 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       54 |  7433 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  7434 | `		ph7_value *pObj;` |
|       12 |  7435 | `		if( n < (sxu32)nArg ){` |
|        - |  7436 | `			/* Argument provided — install with type casting */` |
|       12 |  7437 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       12 |  7438 | `			if( pObj ){` |
|       12 |  7439 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  7440 | `				/* Type casting */` |
|       12 |  7441 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7442 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7443 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7444 | `						if( xCast ){` |
|      ! 0 |  7445 | `							xCast(pObj);` |
|      ! 0 |  7446 | `						}` |
|      ! 0 |  7447 | `					}` |
|      ! 0 |  7448 | `				}` |
|       12 |  7449 | `				sSlot.nIdx = pObj->nIdx;` |
|       12 |  7450 | `				sSlot.pUserData = 0;` |
|       12 |  7451 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        7 |  7452 | `			}` |
|        5 |  7453 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  7454 | `			/* Default value */` |
|      ! 0 |  7455 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  7456 | `			if( pObj ){` |
|      ! 0 |  7457 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  7458 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7459 | `					return rc;` |
|        - |  7460 | `				}` |
|      ! 0 |  7461 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  7462 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7463 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7464 | `						if( xCast ){` |
|      ! 0 |  7465 | `							xCast(pObj);` |
|      ! 0 |  7466 | `						}` |
|      ! 0 |  7467 | `					}` |
|      ! 0 |  7468 | `				}` |
|      ! 0 |  7469 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  7470 | `				sSlot.pUserData = 0;` |
|      ! 0 |  7471 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  7472 | `			}` |
|      ! 0 |  7473 | `		}` |
|        7 |  7474 | `	}` |
|        - |  7475 | `	/* Install closure environment (captured variables) */` |
|       44 |  7476 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7477 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  7478 | `		ph7_value *pValue;` |
|        - |  7479 | `		sxu32 iEnv;` |
|        3 |  7480 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  7481 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  7482 | `			pEnv = &aEnv[iEnv];` |
|        7 |  7483 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  7484 | `				continue;` |
|        - |  7485 | `			}` |
|        5 |  7486 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  7487 | `			if( pValue == 0 ){` |
|      ! 0 |  7488 | `				continue;` |
|        - |  7489 | `			}` |
|        5 |  7490 | `			PH7_MemObjRelease(pValue);` |
|        5 |  7491 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  7492 | `		}` |
|        1 |  7493 | `	}` |
|       44 |  7494 | `	return SXRET_OK;` |
|       23 |  7495 |  |
|        - |  7496 | `/*` |
|        - |  7497 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  7498 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  7499 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  7500 | ` */` |
|       26 |  7501 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7502 |  |
|       28 |  7503 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7504 | `	ph7_class_instance *pThis;` |
|        - |  7505 | `	ph7_class_instance *pClosureThis;` |
|        - |  7506 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7507 | `	ph7_vm_func *pFunc;` |
|        - |  7508 | `	ph7_value sResult;` |
|        - |  7509 | `	ph7_value *pCtxAttr;` |
|        - |  7510 | `	SyString sAttrName;` |
|        - |  7511 | `	sxi32 rc;` |
|       28 |  7512 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7513 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  7514 | `	}` |
|       28 |  7515 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7516 | `	/* Check if already started (has a __ctx) */` |
|       28 |  7517 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  7518 | `	if( pExecCtx != 0 ){` |
|        3 |  7519 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7520 | `			"Cannot start a fiber that has already been started");` |
|        - |  7521 | `	}` |
|        - |  7522 | `	/* Resolve callable */` |
|       26 |  7523 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  7524 | `	if( pFunc == 0 ){` |
|      ! 0 |  7525 | `		return PH7_EXCEPTION;` |
|        - |  7526 | `	}` |
|        - |  7527 | `	/* Create execution context now that we know the function */` |
|       26 |  7528 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  7529 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7530 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7531 | `			"Fiber::start(): out of memory");` |
|        - |  7532 | `	}` |
|        - |  7533 | `	/* Store context in $this->__ctx */` |
|       26 |  7534 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  7535 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  7536 | `	if( pCtxAttr ){` |
|       26 |  7537 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  7538 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  7539 | `	}` |
|        - |  7540 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  7541 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  7542 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  7543 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  7544 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  7545 | `	/* Unpack the args array and install into the frame */` |
|        - |  7546 | `	{` |
|       26 |  7547 | `		ph7_value **apValues = 0;` |
|       26 |  7548 | `		int nActual = 0;` |
|       26 |  7549 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  7550 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  7551 | `			ph7_hashmap_node *pNode;` |
|       26 |  7552 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  7553 | `			if( nCount > 0 ){` |
|        3 |  7554 | `				sxu32 idx = 0;` |
|        4 |  7555 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  7556 | `					nCount * sizeof(ph7_value *));` |
|        3 |  7557 | `				if( apValues ){` |
|        3 |  7558 | `					pNode = pMap->pFirst;` |
|        7 |  7559 | `					while( pNode && idx < nCount ){` |
|        5 |  7560 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  7561 | `						idx++;` |
|        5 |  7562 | `						pNode = pNode->pPrev;` |
|        1 |  7563 | `					}` |
|        3 |  7564 | `					nActual = (int)idx;` |
|        1 |  7565 | `				}` |
|        1 |  7566 | `			}` |
|       12 |  7567 | `		}` |
|       26 |  7568 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  7569 | `		if( apValues ){` |
|        3 |  7570 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  7571 | `		}` |
|        - |  7572 | `	}` |
|        - |  7573 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  7574 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  7575 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  7576 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7577 | `		return PH7_ABORT;` |
|        - |  7578 | `	}` |
|       26 |  7579 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  7580 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  7581 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7582 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7583 | `		return PH7_ABORT;` |
|        - |  7584 | `	}` |
|       26 |  7585 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7586 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7587 | `		return PH7_EXCEPTION;` |
|        - |  7588 | `	}` |
|       26 |  7589 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  7590 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  7591 | `	return PH7_OK;` |
|       15 |  7592 |  |
|        - |  7593 | `/*` |
|        - |  7594 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  7595 | ` */` |
|       36 |  7596 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7597 |  |
|       38 |  7598 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7599 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  7600 | `	ph7_value sResult;` |
|        - |  7601 | `	ph7_value *pResumeVal;` |
|        - |  7602 | `	sxi32 rc;` |
|       38 |  7603 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7604 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  7605 | `		return PH7_OK;` |
|        - |  7606 | `	}` |
|       38 |  7607 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  7608 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7609 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  7610 | `		return PH7_OK;` |
|        - |  7611 | `	}` |
|       38 |  7612 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7613 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7614 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  7615 | `	}` |
|       36 |  7616 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  7617 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  7618 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  7619 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  7620 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7621 | `		return PH7_ABORT;` |
|        - |  7622 | `	}` |
|       36 |  7623 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  7624 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7625 | `		return PH7_EXCEPTION;` |
|        - |  7626 | `	}` |
|       36 |  7627 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  7628 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  7629 | `	return PH7_OK;` |
|       20 |  7630 |  |
|        - |  7631 | `/*` |
|        - |  7632 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  7633 | ` */` |
|        6 |  7634 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7635 |  |
|        8 |  7636 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7637 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  7638 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7639 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7640 | `		return PH7_OK;` |
|        - |  7641 | `	}` |
|        8 |  7642 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  7643 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  7644 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7645 | `		return PH7_OK;` |
|        - |  7646 | `	}` |
|        8 |  7647 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  7648 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7649 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7650 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  7651 | `		}` |
|      ! 0 |  7652 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  7653 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  7654 | `	}` |
|        8 |  7655 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  7656 | `	return PH7_OK;` |
|        5 |  7657 |  |
|        - |  7658 | `/*` |
|        - |  7659 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  7660 | ` */` |
|        6 |  7661 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7662 |  |
|        - |  7663 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7664 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7665 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7666 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  7667 | `	return PH7_OK;` |
|        4 |  7668 |  |
|      ! 0 |  7669 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  7670 |  |
|        - |  7671 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  7672 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  7673 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  7674 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  7675 | `	return PH7_OK;` |
|      ! 0 |  7676 |  |
|        6 |  7677 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7678 |  |
|        - |  7679 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7680 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7681 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7682 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  7683 | `	return PH7_OK;` |
|        4 |  7684 |  |
|        6 |  7685 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7686 |  |
|        - |  7687 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  7688 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  7689 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  7690 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  7691 | `	return PH7_OK;` |
|        4 |  7692 |  |
|        - |  7693 | `/*` |
|        - |  7694 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  7695 | ` */` |
|        4 |  7696 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7697 |  |
|        5 |  7698 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7699 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  7700 | `	if( nArg < 1 ){` |
|      ! 0 |  7701 | `		return PH7_OK;` |
|        - |  7702 | `	}` |
|        5 |  7703 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  7704 | `	if( pExecCtx ){` |
|        5 |  7705 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  7706 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  7707 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  7708 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  7709 | `			SyString sAttrName;` |
|        - |  7710 | `			ph7_value *pAttr;` |
|        5 |  7711 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  7712 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  7713 | `			if( pAttr ){` |
|        5 |  7714 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  7715 | `			}` |
|        2 |  7716 | `		}` |
|        2 |  7717 | `	}` |
|        5 |  7718 | `	return PH7_OK;` |
|        3 |  7719 |  |
|        - |  7720 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  7721 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  7722 |  |
|        - |  7723 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7724 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  7725 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7726 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  7727 |  |
|      ! 0 |  7728 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  7729 |  |
|        - |  7730 | `	ph7_class_instance *pThis;` |
|      ! 0 |  7731 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  7732 | `	ph7_exec_ctx *pCtx;` |
|        - |  7733 | `	ph7_vm_func *pFunc;` |
|        - |  7734 | `	ph7_value *pCallable;` |
|        - |  7735 | `	ph7_value *pCtxAttr;` |
|        - |  7736 | `	SyString sAttrName;` |
|        - |  7737 | `	/* Must not already be started */` |
|      ! 0 |  7738 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7739 | `	if( pCtx != 0 ){` |
|      ! 0 |  7740 | `		return SXERR_INVALID;` |
|        - |  7741 | `	}` |
|      ! 0 |  7742 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7743 | `		return SXERR_INVALID;` |
|        - |  7744 | `	}` |
|      ! 0 |  7745 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  7746 | `	/* Get the callable */` |
|      ! 0 |  7747 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  7748 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7749 | `	if( pCallable == 0 ){` |
|      ! 0 |  7750 | `		return SXERR_INVALID;` |
|        - |  7751 | `	}` |
|        - |  7752 | `	/* Resolve callable */` |
|      ! 0 |  7753 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  7754 | `		SyString sName;` |
|        - |  7755 | `		SyHashEntry *pEntry;` |
|      ! 0 |  7756 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  7757 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  7758 | `		if( pEntry == 0 ){` |
|      ! 0 |  7759 | `			return SXERR_NOTFOUND;` |
|        - |  7760 | `		}` |
|      ! 0 |  7761 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  7762 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7763 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  7764 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  7765 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  7766 | `		if( pMethod == 0 ){` |
|      ! 0 |  7767 | `			return SXERR_INVALID;` |
|        - |  7768 | `		}` |
|      ! 0 |  7769 | `		pClosureThis = pClosure;` |
|      ! 0 |  7770 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  7771 | `	}else{` |
|      ! 0 |  7772 | `		return SXERR_INVALID;` |
|        - |  7773 | `	}` |
|        - |  7774 | `	/* Create context */` |
|      ! 0 |  7775 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  7776 | `	if( pCtx == 0 ){` |
|      ! 0 |  7777 | `		return SXERR_MEM;` |
|        - |  7778 | `	}` |
|        - |  7779 | `	/* Store in __ctx */` |
|      ! 0 |  7780 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  7781 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  7782 | `	if( pCtxAttr ){` |
|      ! 0 |  7783 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  7784 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  7785 | `	}` |
|        - |  7786 | `	/* Set up frame with args */` |
|      ! 0 |  7787 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  7788 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  7789 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  7790 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  7791 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  7792 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  7793 |  |
|      ! 0 |  7794 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  7795 |  |
|      ! 0 |  7796 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7797 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  7798 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  7799 |  |
|      ! 0 |  7800 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7801 |  |
|      ! 0 |  7802 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7803 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  7804 |  |
|      ! 0 |  7805 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7806 |  |
|      ! 0 |  7807 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7808 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  7809 |  |
|      ! 0 |  7810 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  7811 |  |
|      ! 0 |  7812 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  7813 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  7814 | `	return &pCtx->sRetValue;` |
|      ! 0 |  7815 |  |
|        - |  7816 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  7817 | `/*` |
|        - |  7818 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  7819 | ` */` |
|       18 |  7820 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  7821 |  |
|        - |  7822 | `	ph7_generator *pGen;` |
|       20 |  7823 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       20 |  7824 | `	if( pGen == 0 ){` |
|      ! 0 |  7825 | `		return 0;` |
|        - |  7826 | `	}` |
|       20 |  7827 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       20 |  7828 | `	pGen->pCtx = pCtx;` |
|       20 |  7829 | `	pGen->iImplicitKey = 0;` |
|       20 |  7830 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       20 |  7831 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  7832 | `	/* Link the generator back to the exec context */` |
|       20 |  7833 | `	pCtx->pPrivate = pGen;` |
|       20 |  7834 | `	return pGen;` |
|       11 |  7835 |  |
|        - |  7836 | `/*` |
|        - |  7837 | ` * Release a generator and its execution context.` |
|        - |  7838 | ` */` |
|      ! 0 |  7839 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  7840 |  |
|      ! 0 |  7841 | `	if( pGen == 0 ){` |
|      ! 0 |  7842 | `		return;` |
|        - |  7843 | `	}` |
|      ! 0 |  7844 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7845 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7846 | `	if( pGen->pCtx ){` |
|      ! 0 |  7847 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  7848 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  7849 | `		pGen->pCtx = 0;` |
|      ! 0 |  7850 | `	}` |
|      ! 0 |  7851 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  7852 |  |
|        - |  7853 | `/*` |
|        - |  7854 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  7855 | ` */` |
|      192 |  7856 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  7857 |  |
|        - |  7858 | `	ph7_class_instance *pThis;` |
|        - |  7859 | `	SyString sAttr;` |
|        - |  7860 | `	ph7_value *pAttr;` |
|      194 |  7861 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7862 | `		return 0;` |
|        - |  7863 | `	}` |
|      194 |  7864 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      194 |  7865 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  7866 | `		return 0;` |
|        - |  7867 | `	}` |
|      194 |  7868 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      194 |  7869 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      194 |  7870 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  7871 | `		return 0;` |
|        - |  7872 | `	}` |
|      194 |  7873 | `	return (ph7_generator *)pAttr->x.pOther;` |
|       98 |  7874 |  |
|        - |  7875 | `/*` |
|        - |  7876 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  7877 | ` */` |
|       18 |  7878 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7879 |  |
|        - |  7880 | `	ph7_generator *pGen;` |
|        - |  7881 | `	sxi32 rc;` |
|       20 |  7882 | `	if( nArg < 1 ) return PH7_OK;` |
|       20 |  7883 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       20 |  7884 | `	if( pGen == 0 ) return PH7_OK;` |
|       20 |  7885 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       20 |  7886 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       20 |  7887 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       20 |  7888 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        9 |  7889 | `	}` |
|       20 |  7890 | `	return PH7_OK;` |
|       11 |  7891 |  |
|        - |  7892 | `/*` |
|        - |  7893 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  7894 | ` */` |
|       52 |  7895 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7896 |  |
|        - |  7897 | `	ph7_generator *pGen;` |
|       54 |  7898 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       54 |  7899 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       54 |  7900 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       54 |  7901 | `	return PH7_OK;` |
|       28 |  7902 |  |
|        - |  7903 | `/*` |
|        - |  7904 | ` * Generator::current() — return the last yielded value.` |
|        - |  7905 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7906 | ` */` |
|       56 |  7907 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7908 |  |
|        - |  7909 | `	ph7_generator *pGen;` |
|        - |  7910 | `	sxi32 rc;` |
|       58 |  7911 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7912 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       58 |  7913 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       58 |  7914 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7915 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7916 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7917 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7918 | `	}` |
|       58 |  7919 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       58 |  7920 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       30 |  7921 | `	}else{` |
|      ! 0 |  7922 | `		ph7_result_null(pCtx);` |
|        - |  7923 | `	}` |
|       58 |  7924 | `	return PH7_OK;` |
|       30 |  7925 |  |
|        - |  7926 | `/*` |
|        - |  7927 | ` * Generator::key() — return the last yielded key.` |
|        - |  7928 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  7929 | ` */` |
|       12 |  7930 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7931 |  |
|        - |  7932 | `	ph7_generator *pGen;` |
|        - |  7933 | `	sxi32 rc;` |
|       13 |  7934 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7935 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  7936 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  7937 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7938 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  7939 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  7940 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  7941 | `	}` |
|       13 |  7942 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  7943 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  7944 | `	}else{` |
|      ! 0 |  7945 | `		ph7_result_null(pCtx);` |
|        - |  7946 | `	}` |
|       13 |  7947 | `	return PH7_OK;` |
|        7 |  7948 |  |
|        - |  7949 | `/*` |
|        - |  7950 | ` * Generator::next() — advance to the next yield point.` |
|        - |  7951 | ` */` |
|       48 |  7952 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  7953 |  |
|        - |  7954 | `	ph7_generator *pGen;` |
|        - |  7955 | `	sxi32 rc;` |
|       50 |  7956 | `	if( nArg < 1 ) return PH7_OK;` |
|       50 |  7957 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       50 |  7958 | `	if( pGen == 0 ) return PH7_OK;` |
|       50 |  7959 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  7960 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       50 |  7961 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       50 |  7962 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       26 |  7963 | `	}else{` |
|      ! 0 |  7964 | `		return PH7_OK;` |
|        - |  7965 | `	}` |
|       50 |  7966 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       50 |  7967 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       50 |  7968 | `	return PH7_OK;` |
|       26 |  7969 |  |
|        - |  7970 | `/*` |
|        - |  7971 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  7972 | ` */` |
|        4 |  7973 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  7974 |  |
|        - |  7975 | `	ph7_generator *pGen;` |
|        - |  7976 | `	ph7_value *pSendVal;` |
|        - |  7977 | `	sxi32 rc;` |
|        5 |  7978 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  7979 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  7980 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  7981 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  7982 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  7983 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  7984 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  7985 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  7986 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  7987 | `	}else{` |
|      ! 0 |  7988 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7989 | `		return PH7_OK;` |
|        - |  7990 | `	}` |
|        5 |  7991 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  7992 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  7993 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  7994 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  7995 | `	}else{` |
|        3 |  7996 | `		ph7_result_null(pCtx);` |
|        - |  7997 | `	}` |
|        5 |  7998 | `	return PH7_OK;` |
|        3 |  7999 |  |
|        - |  8000 | `/*` |
|        - |  8001 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  8002 | ` *` |
|        - |  8003 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  8004 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  8005 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  8006 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  8007 | ` * the exception to the caller.` |
|        - |  8008 | ` */` |
|      ! 0 |  8009 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8010 |  |
|        - |  8011 | `	ph7_generator *pGen;` |
|        - |  8012 | `	const char *zMsg;` |
|        - |  8013 | `	int nLen;` |
|      ! 0 |  8014 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  8015 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8016 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  8017 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  8018 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  8019 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8020 | `			"Cannot throw into a closed generator");` |
|        - |  8021 | `	}` |
|        - |  8022 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  8023 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  8024 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  8025 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  8026 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8027 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  8028 | `	nLen = 0;` |
|      ! 0 |  8029 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  8030 | `		/* Try to get the exception's message */` |
|        - |  8031 | `		SyString sAttr;` |
|        - |  8032 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  8033 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  8034 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  8035 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  8036 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  8037 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  8038 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  8039 | `		}` |
|      ! 0 |  8040 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  8041 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  8042 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  8043 | `	}` |
|      ! 0 |  8044 | `	(void)nLen;` |
|      ! 0 |  8045 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  8046 |  |
|        - |  8047 | `/*` |
|        - |  8048 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  8049 | ` */` |
|        2 |  8050 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8051 |  |
|        - |  8052 | `	ph7_generator *pGen;` |
|        3 |  8053 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8054 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  8055 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  8056 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8057 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  8058 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  8059 | `	}` |
|        3 |  8060 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  8061 | `	return PH7_OK;` |
|        2 |  8062 |  |
|        - |  8063 | `/*` |
|        - |  8064 | ` * Generator::__destruct() — clean up.` |
|        - |  8065 | ` */` |
|      ! 0 |  8066 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8067 |  |
|        - |  8068 | `	ph7_generator *pGen;` |
|      ! 0 |  8069 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  8070 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  8071 | `	if( pGen ){` |
|      ! 0 |  8072 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  8073 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8074 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8075 | `			SyString sAttrName;` |
|        - |  8076 | `			ph7_value *pAttr;` |
|      ! 0 |  8077 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  8078 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  8079 | `			if( pAttr ){` |
|      ! 0 |  8080 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  8081 | `			}` |
|      ! 0 |  8082 | `		}` |
|      ! 0 |  8083 | `	}` |
|      ! 0 |  8084 | `	return PH7_OK;` |
|      ! 0 |  8085 |  |
|        - |  8086 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  8087 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  8088 | `/*` |
|        - |  8089 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  8090 | ` * the desired message.` |
|        - |  8091 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  8092 | ` * in 'api.c' for additional information.` |
|        - |  8093 | ` */` |
|      370 |  8094 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  8095 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  8096 | `	SyString *pString /* Message to output */` |
|        - |  8097 | `	)` |
|        2 |  8098 |  |
|      372 |  8099 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  8100 | `	sxi32 rc = SXRET_OK;` |
|        - |  8101 | `	/* Call the output consumer */` |
|      372 |  8102 | `	if( pString->nByte > 0 ){` |
|      372 |  8103 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  8104 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  8105 | `	}` |
|      372 |  8106 | `	return rc;` |
|        2 |  8107 |  |
|        - |  8108 | `/*` |
|        - |  8109 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  8110 | ` * callback to consume the formatted message.` |
|        - |  8111 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  8112 | ` * in 'api.c' for additional information.` |
|        - |  8113 | ` */` |
|        2 |  8114 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  8115 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  8116 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  8117 | `	va_list ap           /* Variable list of arguments */` |
|        - |  8118 | `	)` |
|        1 |  8119 |  |
|        3 |  8120 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  8121 | `	sxi32 rc = SXRET_OK;` |
|        - |  8122 | `	SyBlob sWorker;` |
|        - |  8123 | `	/* Format the message and call the output consumer */` |
|        3 |  8124 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  8125 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  8126 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  8127 | `		/* Consume the formatted message */` |
|        3 |  8128 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  8129 | `	}` |
|        3 |  8130 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  8131 | `	/* Release the working buffer */` |
|        3 |  8132 | `	SyBlobRelease(&sWorker);` |
|        3 |  8133 | `	return rc;` |
|        1 |  8134 |  |
|        - |  8135 | `/*` |
|        - |  8136 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  8137 | ` * This function never fail and always return a pointer` |
|        - |  8138 | ` * to a null terminated string.` |
|        - |  8139 | ` */` |
|       12 |  8140 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  8141 |  |
|       13 |  8142 | `	const char *zOp = "Unknown     ";` |
|       13 |  8143 | `	switch(nOp){` |
|        3 |  8144 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  8145 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  8146 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  8147 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  8148 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  8149 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  8150 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  8151 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  8152 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  8153 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  8154 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  8155 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  8156 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  8157 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  8158 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  8159 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  8160 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  8161 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  8162 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  8163 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  8164 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  8165 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  8166 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  8167 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  8168 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  8169 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  8170 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  8171 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  8172 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  8173 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  8174 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  8175 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  8176 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  8177 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  8178 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  8179 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  8180 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  8181 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  8182 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  8183 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  8184 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  8185 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  8186 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  8187 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  8188 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  8189 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  8190 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  8191 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  8192 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  8193 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  8194 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  8195 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  8196 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  8197 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  8198 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  8199 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  8200 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  8201 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  8202 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  8203 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  8204 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  8205 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  8206 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  8207 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  8208 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  8209 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  8210 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  8211 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  8212 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  8213 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  8214 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  8215 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  8216 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  8217 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  8218 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  8219 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  8220 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  8221 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  8222 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  8223 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  8224 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  8225 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  8226 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  8227 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  8228 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  8229 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  8230 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  8231 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  8232 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  8233 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  8234 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  8235 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  8236 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  8237 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  8238 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  8239 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  8240 | `	default:` |
|      ! 0 |  8241 | `		break;` |
|        - |  8242 | `	}` |
|       13 |  8243 | `	return zOp;` |
|        1 |  8244 |  |
|        - |  8245 | `/*` |
|        - |  8246 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  8247 | ` * The xConsumer() callback which is an used defined function` |
|        - |  8248 | ` * is responsible of consuming the generated dump.` |
|        - |  8249 | ` */` |
|        2 |  8250 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  8251 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  8252 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  8253 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  8254 | `	)` |
|        1 |  8255 |  |
|        - |  8256 | `	sxi32 rc;` |
|        3 |  8257 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  8258 | `	return rc;` |
|        1 |  8259 |  |
|        - |  8260 | `/*` |
|        - |  8261 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  8262 | ` * outside a class body [i.e: global or function scope].` |
|        - |  8263 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  8264 | ` * in 'compile.c' for additional information.` |
|        - |  8265 | ` */` |
|       14 |  8266 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  8267 |  |
|       15 |  8268 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  8269 | `	/* Evaluate and expand constant value */` |
|       15 |  8270 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  8271 |  |
|        - |  8272 | `/*` |
|        - |  8273 | ` * Section:` |
|        - |  8274 | ` *  Function handling functions.` |
|        - |  8275 | ` * Status:` |
|        - |  8276 | ` *    Stable.` |
|        - |  8277 | ` */` |
|        - |  8278 | `/*` |
|        - |  8279 | ` * int func_num_args(void)` |
|        - |  8280 | ` *   Returns the number of arguments passed to the function.` |
|        - |  8281 | ` * Parameters` |
|        - |  8282 | ` *   None.` |
|        - |  8283 | ` * Return` |
|        - |  8284 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  8285 | ` *  or -1 if called from the globe scope.` |
|        - |  8286 | ` */` |
|      944 |  8287 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8288 |  |
|        - |  8289 | `	VmFrame *pFrame;` |
|        - |  8290 | `	ph7_vm *pVm;` |
|        - |  8291 | `	/* Point to the target VM */` |
|      946 |  8292 | `	pVm = pCtx->pVm;` |
|        - |  8293 | `	/* Current frame */` |
|      946 |  8294 | `	pFrame = pVm->pFrame;` |
|      946 |  8295 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  8296 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  8297 | `		SXUNUSED(nArg);` |
|      ! 0 |  8298 | `		SXUNUSED(apArg);` |
|        - |  8299 | `		/* Global frame,return -1 */` |
|      ! 0 |  8300 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  8301 | `		return SXRET_OK;` |
|        - |  8302 | `	}` |
|        - |  8303 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  8304 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  8305 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  8306 | `	return SXRET_OK;` |
|      474 |  8307 |  |
|        - |  8308 | `/*` |
|        - |  8309 | ` * value func_get_arg(int $arg_num)` |
|        - |  8310 | ` *   Return an item from the argument list.` |
|        - |  8311 | ` * Parameters` |
|        - |  8312 | ` *  Argument number(index start from zero).` |
|        - |  8313 | ` * Return` |
|        - |  8314 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  8315 | ` */` |
|       22 |  8316 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8317 |  |
|       24 |  8318 | `	ph7_value *pObj = 0;` |
|       24 |  8319 | `	VmSlot *pSlot = 0;` |
|        - |  8320 | `	VmFrame *pFrame;` |
|        - |  8321 | `	ph7_vm *pVm;` |
|        - |  8322 | `	/* Point to the target VM */` |
|       24 |  8323 | `	pVm = pCtx->pVm;` |
|        - |  8324 | `	/* Current frame */` |
|       24 |  8325 | `	pFrame = pVm->pFrame;` |
|       24 |  8326 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  8327 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  8328 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  8329 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  8330 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8331 | `		return SXRET_OK;` |
|        - |  8332 | `	}` |
|        - |  8333 | `	/* Extract the desired index */` |
|       21 |  8334 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  8335 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  8336 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  8337 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8338 | `		return SXRET_OK;` |
|        - |  8339 | `	}` |
|        - |  8340 | `	/* Extract the desired argument */` |
|       21 |  8341 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  8342 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  8343 | `			/* Return the desired argument */` |
|       21 |  8344 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  8345 | `		}else{` |
|        - |  8346 | `			/* No such argument,return false */` |
|      ! 0 |  8347 | `			ph7_result_bool(pCtx,0);` |
|        - |  8348 | `		}` |
|       11 |  8349 | `	}else{` |
|        - |  8350 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  8351 | `		ph7_result_bool(pCtx,0);` |
|        - |  8352 | `	}` |
|       21 |  8353 | `	return SXRET_OK;` |
|       13 |  8354 |  |
|        - |  8355 | `/*` |
|        - |  8356 | ` * array func_get_args_byref(void)` |
|        - |  8357 | ` *   Returns an array comprising a function's argument list.` |
|        - |  8358 | ` * Parameters` |
|        - |  8359 | ` *  None.` |
|        - |  8360 | ` * Return` |
|        - |  8361 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  8362 | ` *  member of the current user-defined function's argument list.` |
|        - |  8363 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8364 | ` * NOTE:` |
|        - |  8365 | ` *  Arguments are returned to the array by reference.` |
|        - |  8366 | ` */` |
|        2 |  8367 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8368 |  |
|        - |  8369 | `	ph7_value *pArray;` |
|        - |  8370 | `	VmFrame *pFrame;` |
|        - |  8371 | `	VmSlot *aSlot;` |
|        - |  8372 | `	sxu32 n;` |
|        - |  8373 | `	/* Point to the current frame */` |
|        3 |  8374 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  8375 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  8376 | `	if( pFrame->pParent == 0 ){` |
|        - |  8377 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8378 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8379 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8380 | `		return SXRET_OK;` |
|        - |  8381 | `	}` |
|        - |  8382 | `	/* Create a new array */` |
|        3 |  8383 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8384 | `	if( pArray == 0 ){` |
|      ! 0 |  8385 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8386 | `		SXUNUSED(apArg);` |
|      ! 0 |  8387 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8388 | `		return SXRET_OK;` |
|        - |  8389 | `	}` |
|        - |  8390 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  8391 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  8392 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  8393 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  8394 | `	}` |
|        - |  8395 | `	/* Return the freshly created array */` |
|        3 |  8396 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8397 | `	return SXRET_OK;` |
|        2 |  8398 |  |
|        - |  8399 | `/*` |
|        - |  8400 | ` * array func_get_args(void)` |
|        - |  8401 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  8402 | ` * Parameters` |
|        - |  8403 | ` *  None.` |
|        - |  8404 | ` * Return` |
|        - |  8405 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  8406 | ` *  member of the current user-defined function's argument list.` |
|        - |  8407 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  8408 | ` */` |
|       88 |  8409 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8410 |  |
|       90 |  8411 | `	ph7_value *pObj = 0;` |
|        - |  8412 | `	ph7_value *pArray;` |
|        - |  8413 | `	VmFrame *pFrame;` |
|        - |  8414 | `	VmSlot *aSlot;` |
|        - |  8415 | `	sxu32 n;` |
|        - |  8416 | `	/* Point to the current frame */` |
|       90 |  8417 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  8418 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  8419 | `	if( pFrame->pParent == 0 ){` |
|        - |  8420 | `		/* Global frame,return FALSE */` |
|      ! 0 |  8421 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  8422 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8423 | `		return SXRET_OK;` |
|        - |  8424 | `	}` |
|        - |  8425 | `	/* Create a new array */` |
|       90 |  8426 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  8427 | `	if( pArray == 0 ){` |
|      ! 0 |  8428 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8429 | `		SXUNUSED(apArg);` |
|      ! 0 |  8430 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8431 | `		return SXRET_OK;` |
|        - |  8432 | `	}` |
|        - |  8433 | `	/* Start filling the array with the given arguments */` |
|       90 |  8434 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  8435 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  8436 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  8437 | `		if( pObj ){` |
|      134 |  8438 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  8439 | `		}` |
|       68 |  8440 | `	}` |
|        - |  8441 | `	/* Return the freshly created array */` |
|       90 |  8442 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  8443 | `	return SXRET_OK;` |
|       46 |  8444 |  |
|        - |  8445 | `/*` |
|        - |  8446 | ` * bool function_exists(string $name)` |
|        - |  8447 | ` *  Return TRUE if the given function has been defined.` |
|        - |  8448 | ` * Parameters` |
|        - |  8449 | ` *  The name of the desired function.` |
|        - |  8450 | ` * Return` |
|        - |  8451 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  8452 | ` */` |
|     1684 |  8453 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8454 |  |
|        - |  8455 | `	const char *zName;` |
|        - |  8456 | `	ph7_vm *pVm;` |
|        - |  8457 | `	int nLen;` |
|        - |  8458 | `	int res;` |
|     1686 |  8459 | `	if( nArg < 1 ){` |
|        - |  8460 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  8461 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8462 | `		return SXRET_OK;` |
|        - |  8463 | `	}` |
|        - |  8464 | `	/* Point to the target VM */` |
|     1686 |  8465 | `	pVm = pCtx->pVm;` |
|        - |  8466 | `	/* Extract the function name */` |
|     1686 |  8467 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8468 | `	/* Assume the function is not defined */` |
|     1686 |  8469 | `	res = 0;` |
|        - |  8470 | `	/* Perform the lookup */` |
|     2526 |  8471 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1680 |  8472 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8473 | `			/* Function is defined */` |
|      206 |  8474 | `			res = 1;` |
|      102 |  8475 | `	}` |
|     1686 |  8476 | `	ph7_result_bool(pCtx,res);` |
|     1686 |  8477 | `	return SXRET_OK;` |
|      844 |  8478 |  |
|        - |  8479 | `/*` |
|        - |  8480 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8481 | ` * [i.e: Whether it is callable or not].` |
|        - |  8482 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  8483 | ` */` |
|    17876 |  8484 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  8485 |  |
|    17878 |  8486 | `	int res = 0;` |
|    17878 |  8487 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8488 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  8489 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  8490 | `		ph7_class_method *pMethod;` |
|      ! 0 |  8491 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  8492 | `		if( pMethod && CallInvoke ){` |
|        - |  8493 | `			ph7_value sResult;` |
|        - |  8494 | `			sxi32 rc;` |
|        - |  8495 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  8496 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  8497 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  8498 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  8499 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  8500 | `			}` |
|      ! 0 |  8501 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8502 | `		}` |
|    17878 |  8503 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  8504 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  8505 | `		if( pMap->nEntry == 2 ){` |
|        - |  8506 | `			ph7_class *pClass;` |
|        - |  8507 | `			ph7_value *pV;` |
|        - |  8508 | `			/* Extract the target class */` |
|       12 |  8509 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  8510 | `			if( pV ){` |
|       12 |  8511 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  8512 | `				if( pClass ){` |
|        - |  8513 | `					ph7_class_method *pMethod;` |
|        - |  8514 | `					/* Extract the target method */` |
|       10 |  8515 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  8516 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  8517 | `						/* Perform the lookup */` |
|       10 |  8518 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  8519 | `						if( pMethod ){` |
|        - |  8520 | `							/* Method is callable */` |
|        5 |  8521 | `							res = 1;` |
|        2 |  8522 | `						}` |
|        4 |  8523 | `					}` |
|        4 |  8524 | `				}` |
|        5 |  8525 | `			}` |
|        7 |  8526 | `		}` |
|    17865 |  8527 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  8528 | `		const char *zName;` |
|        - |  8529 | `		int nLen;` |
|        - |  8530 | `		/* Extract the name */` |
|     5054 |  8531 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  8532 | `		/* Perform the lookup */` |
|     5069 |  8533 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  8534 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8535 | `				/* Function is callable */` |
|     5036 |  8536 | `				res = 1;` |
|     2517 |  8537 | `		}` |
|     2526 |  8538 | `	}` |
|    17878 |  8539 | `	return res;` |
|        2 |  8540 |  |
|        - |  8541 | `/*` |
|        - |  8542 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  8543 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  8544 | ` * Parameters` |
|        - |  8545 | ` * $name` |
|        - |  8546 | ` *    The callback function to check` |
|        - |  8547 | ` * $syntax_only` |
|        - |  8548 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  8549 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  8550 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  8551 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  8552 | ` *    a string.` |
|        - |  8553 | ` * Return` |
|        - |  8554 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  8555 | ` */` |
|       14 |  8556 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8557 |  |
|        - |  8558 | `	ph7_vm *pVm;` |
|        - |  8559 | `	int res;` |
|       15 |  8560 | `	if( nArg < 1 ){` |
|        - |  8561 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  8562 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8563 | `		return SXRET_OK;` |
|        - |  8564 | `	}` |
|        - |  8565 | `	/* Point to the target VM */` |
|       15 |  8566 | `	pVm = pCtx->pVm;` |
|        - |  8567 | `	/* Perform the requested operation */` |
|       15 |  8568 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  8569 | `	ph7_result_bool(pCtx,res);` |
|       15 |  8570 | `	return SXRET_OK;` |
|        8 |  8571 |  |
|        - |  8572 | `/*` |
|        - |  8573 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  8574 | ` * defined below.` |
|        - |  8575 | ` */` |
|     1200 |  8576 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  8577 |  |
|     1201 |  8578 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  8579 | `	ph7_value sName;` |
|        - |  8580 | `	sxi32 rc;` |
|        - |  8581 | `	/* Prepare the function name for insertion */` |
|     1201 |  8582 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  8583 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  8584 | `	/* Perform the insertion */` |
|     1201 |  8585 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  8586 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  8587 | `	return rc;` |
|        1 |  8588 |  |
|        - |  8589 | `/*` |
|        - |  8590 | ` * array get_defined_functions(void)` |
|        - |  8591 | ` *  Returns an array of all defined functions.` |
|        - |  8592 | ` * Parameter` |
|        - |  8593 | ` *  None.` |
|        - |  8594 | ` * Return` |
|        - |  8595 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  8596 | ` *  both built-in (internal) and user-defined.` |
|        - |  8597 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  8598 | ` *  defined ones using $arr["user"].` |
|        - |  8599 | ` * Note:` |
|        - |  8600 | ` *  NULL is returned on failure.` |
|        - |  8601 | ` */` |
|        2 |  8602 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8603 |  |
|        - |  8604 | `	ph7_value *pArray,*pEntry;` |
|        - |  8605 | `	/* NOTE:` |
|        - |  8606 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  8607 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  8608 | `	 */` |
|        3 |  8609 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  8610 | ` 	if( pArray == 0 ){` |
|      ! 0 |  8611 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8612 | `		SXUNUSED(apArg);` |
|        - |  8613 | `		/* Return NULL */` |
|      ! 0 |  8614 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8615 | `		return SXRET_OK;` |
|        - |  8616 | `	}` |
|        3 |  8617 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8618 | `	if( pEntry == 0 ){` |
|        - |  8619 | `		/* Return NULL */` |
|      ! 0 |  8620 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8621 | `		return SXRET_OK;` |
|        - |  8622 | `	}` |
|        - |  8623 | `	/* Fill with the appropriate information */` |
|        3 |  8624 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  8625 | `	/* Create the 'internal' index */` |
|        3 |  8626 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  8627 | `	/* Create the user-func array */` |
|        3 |  8628 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  8629 | `	if( pEntry == 0 ){` |
|        - |  8630 | `		/* Return NULL */` |
|      ! 0 |  8631 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8632 | `		return SXRET_OK;` |
|        - |  8633 | `	}` |
|        - |  8634 | `	/* Fill with the appropriate information */` |
|        3 |  8635 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  8636 | `	/* Create the 'user' index */` |
|        3 |  8637 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  8638 | `	/* Return the multi-dimensional array */` |
|        3 |  8639 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  8640 | `	return SXRET_OK;` |
|        2 |  8641 |  |
|        - |  8642 | `/*` |
|        - |  8643 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  8644 | ` *  Register a function for execution on shutdown.` |
|        - |  8645 | ` * Note` |
|        - |  8646 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  8647 | ` *  be called in the same order as they were registered.` |
|        - |  8648 | ` * Parameters` |
|        - |  8649 | ` *  $callback` |
|        - |  8650 | ` *   The shutdown callback to register.` |
|        - |  8651 | ` * $param` |
|        - |  8652 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  8653 | ` * Return` |
|        - |  8654 | ` *  Nothing.` |
|        - |  8655 | ` */` |
|        2 |  8656 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8657 |  |
|        - |  8658 | `	VmShutdownCB sEntry;` |
|        - |  8659 | `	int i,j;` |
|        3 |  8660 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8661 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  8662 | `		return PH7_OK;` |
|        - |  8663 | `	}` |
|        - |  8664 | `	/* Zero the Entry */` |
|        3 |  8665 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  8666 | `	/* Initialize fields */` |
|        3 |  8667 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  8668 | `	/* Save the callback name for later invocation name */` |
|        3 |  8669 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  8670 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  8671 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  8672 | `	}` |
|        - |  8673 | `	/* Copy arguments */` |
|        3 |  8674 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  8675 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  8676 | `			/* Limit reached */` |
|      ! 0 |  8677 | `			break;` |
|        - |  8678 | `		}` |
|      ! 0 |  8679 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  8680 | `	}` |
|        3 |  8681 | `	sEntry.nArg = j;` |
|        - |  8682 | `	/* Install the callback */` |
|        3 |  8683 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  8684 | `	return PH7_OK;` |
|        2 |  8685 |  |
|        - |  8686 | `/*` |
|        - |  8687 | ` * Section:` |
|        - |  8688 | ` *  Class handling functions.` |
|        - |  8689 | ` * Status:` |
|        - |  8690 | ` *    Stable.` |
|        - |  8691 | ` */` |
|        - |  8692 | `/*` |
|        - |  8693 | ` * Extract the top active class. NULL is returned` |
|        - |  8694 | ` * if the class stack is empty.` |
|        - |  8695 | ` */` |
|      602 |  8696 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  8697 |  |
|      604 |  8698 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  8699 | `	ph7_class **apClass;` |
|      604 |  8700 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  8701 | `		/* Empty stack,return NULL */` |
|       15 |  8702 | `		return 0;` |
|        - |  8703 | `	}` |
|        - |  8704 | `	/* Peek the last entry */` |
|      590 |  8705 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      590 |  8706 | `	return apClass[pSet->nUsed - 1];` |
|      303 |  8707 |  |
|        - |  8708 | `/*` |
|        - |  8709 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  8710 | ` *   Get the class that declared the currently executing method.` |
|        - |  8711 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  8712 | ` *` |
|        - |  8713 | ` * Parameters` |
|        - |  8714 | ` *   pVm: Target VM` |
|        - |  8715 | ` *` |
|        - |  8716 | ` * Return` |
|        - |  8717 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  8718 | ` *   - Not executing within a class method` |
|        - |  8719 | ` *` |
|        - |  8720 | ` * Note` |
|        - |  8721 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  8722 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  8723 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  8724 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  8725 | ` *   declaring class.` |
|        - |  8726 | ` */` |
|       90 |  8727 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  8728 |  |
|       92 |  8729 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8730 | `	ph7_vm_func *pVmFunc;` |
|        - |  8731 |  |
|        - |  8732 | `	/* Skip exception frames to find the actual method frame */` |
|       92 |  8733 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  8734 |  |
|        - |  8735 | `	/* Check if we're in a method context */` |
|       92 |  8736 | `	if( pFrame->pParent ){` |
|       88 |  8737 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       88 |  8738 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  8739 | `			/* Return the declaring class */` |
|       88 |  8740 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  8741 | `		}` |
|      ! 0 |  8742 | `	}` |
|        - |  8743 |  |
|        5 |  8744 | `	return 0;` |
|       47 |  8745 |  |
|        - |  8746 |  |
|        - |  8747 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  8748 | `/*` |
|        - |  8749 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  8750 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  8751 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  8752 | ` * return value indicates failure.` |
|        - |  8753 | ` */` |
|     1530 |  8754 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  8755 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  8756 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  8757 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  8758 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  8759 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  8760 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  8761 | `	)` |
|        2 |  8762 |  |
|        - |  8763 | `	ph7_value *aStack;` |
|        - |  8764 | `	VmInstr aInstr[2];` |
|        - |  8765 | `	int iCursor;` |
|        - |  8766 | `	int i;` |
|        - |  8767 | `	/* Create a new operand stack */` |
|     1532 |  8768 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1532 |  8769 | `	if( aStack == 0 ){` |
|      ! 0 |  8770 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8771 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  8772 | `		return SXERR_MEM;` |
|        - |  8773 | `	}` |
|        - |  8774 | `	/* Fill the operand stack with the given arguments */` |
|     2168 |  8775 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      638 |  8776 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8777 | `		/*` |
|        - |  8778 | `		 * Symisc eXtension:` |
|        - |  8779 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8780 | `		 */` |
|      638 |  8781 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      320 |  8782 | `	}` |
|     1532 |  8783 | `	iCursor = nArg + 1;` |
|     1532 |  8784 | `	if( pThis ){` |
|        - |  8785 | `		/*` |
|        - |  8786 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  8787 | `		 */` |
|     1526 |  8788 | `		pThis->iRef++; /* Increment reference count */` |
|     1526 |  8789 | `		aStack[i].x.pOther = pThis;` |
|     1526 |  8790 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      762 |  8791 | `	}` |
|     1532 |  8792 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1532 |  8793 | `	i++;` |
|        - |  8794 | `	/* Push method name */` |
|     1532 |  8795 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1532 |  8796 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1532 |  8797 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1532 |  8798 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  8799 | `	/* Emit the CALL istruction */` |
|     1532 |  8800 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1532 |  8801 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1532 |  8802 | `	aInstr[0].iP2 = 0;` |
|     1532 |  8803 | `	aInstr[0].p3  = 0;` |
|        - |  8804 | `	/* Emit the DONE instruction */` |
|     1532 |  8805 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1532 |  8806 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1532 |  8807 | `	aInstr[1].iP2 = 0;` |
|     1532 |  8808 | `	aInstr[1].p3  = 0;` |
|        - |  8809 | `	/* Execute the method body (if available) */` |
|     1532 |  8810 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|        - |  8811 | `	/* Clean up the mess left behind */` |
|     1532 |  8812 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1532 |  8813 | `	return PH7_OK;` |
|      767 |  8814 |  |
|        - |  8815 | `/*` |
|        - |  8816 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  8817 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  8818 | ` * in the apArg[] array.` |
|        - |  8819 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8820 | ` * return value indicates failure.` |
|        - |  8821 | ` */` |
|      960 |  8822 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  8823 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8824 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8825 | `	int nArg,          /* Total number of given arguments */` |
|        - |  8826 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  8827 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  8828 | `	)` |
|        2 |  8829 |  |
|        - |  8830 | `	ph7_value *aStack;` |
|        - |  8831 | `	VmInstr aInstr[2];` |
|        - |  8832 | `	int i;` |
|      962 |  8833 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  8834 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 |  8835 | `		if( pResult ){` |
|        - |  8836 | `			/* Assume a null return value */` |
|      ! 0 |  8837 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8838 | `		}` |
|      479 |  8839 | `		return SXERR_INVALID;` |
|        - |  8840 | `	}` |
|      484 |  8841 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8842 | `		/* Class method */` |
|       11 |  8843 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  8844 | `		ph7_class_method *pMethod = 0;` |
|       11 |  8845 | `		ph7_class_instance *pThis = 0;` |
|       11 |  8846 | `		ph7_class *pClass = 0;` |
|        - |  8847 | `		ph7_value *pValue;` |
|        - |  8848 | `		sxi32 rc;` |
|       11 |  8849 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  8850 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  8851 | `			if( pResult ){` |
|        - |  8852 | `				/* Assume a null return value */` |
|      ! 0 |  8853 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8854 | `			}` |
|      ! 0 |  8855 | `			return SXRET_OK;` |
|        - |  8856 | `		}` |
|        - |  8857 | `		/* Extract the class name or an instance of it */` |
|       11 |  8858 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  8859 | `		if( pValue ){` |
|       11 |  8860 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  8861 | `		}` |
|       11 |  8862 | `		if( pClass == 0 ){` |
|        - |  8863 | `			/* No such class,return NULL */` |
|      ! 0 |  8864 | `			if( pResult ){` |
|      ! 0 |  8865 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8866 | `			}` |
|      ! 0 |  8867 | `			return SXRET_OK;` |
|        - |  8868 | `		}` |
|       11 |  8869 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  8870 | `			/* Point to the class instance */` |
|        5 |  8871 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  8872 | `		}` |
|        - |  8873 | `		/* Try to extract the method */` |
|       11 |  8874 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  8875 | `		if( pValue ){` |
|       11 |  8876 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  8877 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  8878 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  8879 | `			}` |
|        5 |  8880 | `		}` |
|       11 |  8881 | `		if( pMethod == 0 ){` |
|        - |  8882 | `			/* No such method,return NULL */` |
|      ! 0 |  8883 | `			if( pResult ){` |
|      ! 0 |  8884 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  8885 | `			}` |
|      ! 0 |  8886 | `			return SXRET_OK;` |
|        - |  8887 | `		}` |
|        - |  8888 | `		/* Call the class method */` |
|       11 |  8889 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  8890 | `		return rc;` |
|        - |  8891 | `	}` |
|        - |  8892 | `	/* Create a new operand stack */` |
|      474 |  8893 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      474 |  8894 | `	if( aStack == 0 ){` |
|      ! 0 |  8895 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8896 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  8897 | `		if( pResult ){` |
|        - |  8898 | `			/* Assume a null return value */` |
|      ! 0 |  8899 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  8900 | `		}` |
|      ! 0 |  8901 | `		return SXERR_MEM;` |
|        - |  8902 | `	}` |
|        - |  8903 | `	/* Fill the operand stack with the given arguments */` |
|     1522 |  8904 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1050 |  8905 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  8906 | `		/*` |
|        - |  8907 | `		 * Symisc eXtension:` |
|        - |  8908 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  8909 | `		 */` |
|     1050 |  8910 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      526 |  8911 | `	}` |
|        - |  8912 | `	/* Push the function name */` |
|      474 |  8913 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      474 |  8914 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  8915 | `	/* Emit the CALL istruction */` |
|      474 |  8916 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      474 |  8917 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      474 |  8918 | `	aInstr[0].iP2 = 0;` |
|      474 |  8919 | `	aInstr[0].p3  = 0;` |
|        - |  8920 | `	/* Emit the DONE instruction */` |
|      474 |  8921 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      474 |  8922 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      474 |  8923 | `	aInstr[1].iP2 = 0;` |
|      474 |  8924 | `	aInstr[1].p3  = 0;` |
|        - |  8925 | `	/* Execute the function body (if available) */` |
|      474 |  8926 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - |  8927 | `	/* Clean up the mess left behind */` |
|      474 |  8928 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      474 |  8929 | `	return PH7_OK;` |
|      482 |  8930 |  |
|        - |  8931 | `/*` |
|        - |  8932 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  8933 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  8934 | ` * parameter.` |
|        - |  8935 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  8936 | ` * return value indicates failure.` |
|        - |  8937 | ` */` |
|      236 |  8938 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  8939 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  8940 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  8941 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  8942 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  8943 | `	)` |
|        1 |  8944 |  |
|        - |  8945 | `	ph7_value *pArg;` |
|        - |  8946 | `	SySet aArg;` |
|        - |  8947 | `	va_list ap;` |
|        - |  8948 | `	sxi32 rc;` |
|      237 |  8949 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  8950 | `	/* Copy arguments one after one */` |
|      237 |  8951 | `	va_start(ap,pResult);` |
|      393 |  8952 | `	for(;;){` |
|      787 |  8953 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  8954 | `		if( pArg == 0 ){` |
|      237 |  8955 | `			break;` |
|        - |  8956 | `		}` |
|      551 |  8957 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  8958 | `	}` |
|        - |  8959 | `	/* Call the core routine */` |
|      237 |  8960 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  8961 | `	/* Cleanup */` |
|      237 |  8962 | `	SySetRelease(&aArg);` |
|      237 |  8963 | `	return rc;` |
|        1 |  8964 |  |
|        - |  8965 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  8966 | `/*` |
|        - |  8967 | ` * bool defined(string $name)` |
|        - |  8968 | ` *  Checks whether a given named constant exists.` |
|        - |  8969 | ` * Parameter:` |
|        - |  8970 | ` *  Name of the desired constant.` |
|        - |  8971 | ` * Return` |
|        - |  8972 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  8973 | ` */` |
|       14 |  8974 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8975 |  |
|        - |  8976 | `	const char *zName;` |
|       16 |  8977 | `	int nLen = 0;` |
|       16 |  8978 | `	int res = 0;` |
|       16 |  8979 | `	if( nArg < 1 ){` |
|        - |  8980 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  8981 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  8982 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8983 | `		return SXRET_OK;` |
|        - |  8984 | `	}` |
|        - |  8985 | `	/* Extract constant name */` |
|       16 |  8986 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  8987 | `	/* Perform the lookup */` |
|       16 |  8988 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  8989 | `		/* Already defined */` |
|       10 |  8990 | `		res = 1;` |
|        4 |  8991 | `	}` |
|       16 |  8992 | `	ph7_result_bool(pCtx,res);` |
|       16 |  8993 | `	return SXRET_OK;` |
|        9 |  8994 |  |
|        - |  8995 | `/*` |
|        - |  8996 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  8997 | ` * below.` |
|        - |  8998 | ` */` |
|       10 |  8999 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  9000 |  |
|       12 |  9001 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  9002 | `	/* Expand constant value */` |
|       12 |  9003 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 |  9004 |  |
|        - |  9005 | `/*` |
|        - |  9006 | ` * bool define(string $constant_name,expression value)` |
|        - |  9007 | ` *  Defines a named constant at runtime.` |
|        - |  9008 | ` * Parameter:` |
|        - |  9009 | ` *  $constant_name` |
|        - |  9010 | ` *   The name of the constant` |
|        - |  9011 | ` *  $value` |
|        - |  9012 | ` *   Constant value` |
|        - |  9013 | ` * Return:` |
|        - |  9014 | ` *   TRUE on success,FALSE on failure.` |
|        - |  9015 | ` */` |
|       12 |  9016 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9017 |  |
|        - |  9018 | `	const char *zName;  /* Constant name */` |
|        - |  9019 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 |  9020 | `	int nLen = 0;       /* Name length */` |
|        - |  9021 | `	sxi32 rc;` |
|       14 |  9022 | `	if( nArg < 2 ){` |
|        - |  9023 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  9024 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  9025 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9026 | `		return SXRET_OK;` |
|        - |  9027 | `	}` |
|       14 |  9028 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  9029 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  9030 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9031 | `		return SXRET_OK;` |
|        - |  9032 | `	}` |
|        - |  9033 | `	/* Extract constant name */` |
|       14 |  9034 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 |  9035 | `	if( nLen < 1 ){` |
|      ! 0 |  9036 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  9037 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9038 | `		return SXRET_OK;` |
|        - |  9039 | `	}` |
|        - |  9040 | `	/* Duplicate constant value */` |
|       14 |  9041 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 |  9042 | `	if( pValue == 0 ){` |
|      ! 0 |  9043 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9044 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9045 | `		return SXRET_OK;` |
|        - |  9046 | `	}` |
|        - |  9047 | `	/* Initialize the memory object */` |
|       14 |  9048 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  9049 | `	/* Register the constant */` |
|       14 |  9050 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 |  9051 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9052 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  9053 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  9054 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9055 | `		return SXRET_OK;` |
|        - |  9056 | `	}` |
|        - |  9057 | `	/* Duplicate constant value */` |
|       14 |  9058 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 |  9059 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  9060 | `		/* Lower case the constant name */` |
|      ! 0 |  9061 | `		char *zCur = (char *)zName;` |
|      ! 0 |  9062 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  9063 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  9064 | `				/* UTF-8 stream */` |
|      ! 0 |  9065 | `				zCur++;` |
|      ! 0 |  9066 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  9067 | `					zCur++;` |
|      ! 0 |  9068 | `				}` |
|      ! 0 |  9069 | `				continue;` |
|        - |  9070 | `			}` |
|      ! 0 |  9071 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  9072 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  9073 | `				zCur[0] = (char)c;` |
|      ! 0 |  9074 | `			}` |
|      ! 0 |  9075 | `			zCur++;` |
|      ! 0 |  9076 | `		}` |
|        - |  9077 | `		/* Finally,register the constant */` |
|      ! 0 |  9078 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  9079 | `	}` |
|        - |  9080 | `	/* All done,return TRUE */` |
|       14 |  9081 | `	ph7_result_bool(pCtx,1);` |
|       14 |  9082 | `	return SXRET_OK;` |
|        8 |  9083 |  |
|        - |  9084 | `/*` |
|        - |  9085 | ` * value constant(string $name)` |
|        - |  9086 | ` *  Returns the value of a constant` |
|        - |  9087 | ` * Parameter` |
|        - |  9088 | ` *  $name` |
|        - |  9089 | ` *    Name of the constant.` |
|        - |  9090 | ` * Return` |
|        - |  9091 | ` *  Constant value or NULL if not defined.` |
|        - |  9092 | ` */` |
|        8 |  9093 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9094 |  |
|        - |  9095 | `	SyHashEntry *pEntry;` |
|        - |  9096 | `	ph7_constant *pCons;` |
|        - |  9097 | `	const char *zName; /* Constant name */` |
|        - |  9098 | `	ph7_value sVal;    /* Constant value */` |
|        - |  9099 | `	int nLen;` |
|       10 |  9100 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9101 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  9102 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  9103 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9104 | `		return SXRET_OK;` |
|        - |  9105 | `	}` |
|        - |  9106 | `	/* Extract the constant name */` |
|       10 |  9107 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9108 | `	/* Perform the query */` |
|       10 |  9109 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  9110 | `	if( pEntry == 0 ){` |
|        3 |  9111 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  9112 | `		ph7_result_null(pCtx);` |
|        3 |  9113 | `		return SXRET_OK;` |
|        - |  9114 | `	}` |
|        8 |  9115 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  9116 | `	/* Point to the structure that describe the constant */` |
|        8 |  9117 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  9118 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  9119 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  9120 | `	/* Return that value */` |
|        8 |  9121 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  9122 | `	/* Cleanup */` |
|        8 |  9123 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  9124 | `	return SXRET_OK;` |
|        6 |  9125 |  |
|        - |  9126 | `/*` |
|        - |  9127 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  9128 | ` * defined below.` |
|        - |  9129 | ` */` |
|      452 |  9130 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9131 |  |
|      453 |  9132 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9133 | `	ph7_value sName;` |
|        - |  9134 | `	sxi32 rc;` |
|        - |  9135 | `	/* Prepare the constant name for insertion */` |
|      453 |  9136 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 |  9137 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9138 | `	/* Perform the insertion */` |
|      453 |  9139 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 |  9140 | `	PH7_MemObjRelease(&sName);` |
|      453 |  9141 | `	return rc;` |
|        1 |  9142 |  |
|        - |  9143 | `/*` |
|        - |  9144 | ` * array get_defined_constants(void)` |
|        - |  9145 | ` *  Returns an associative array with the names of all defined` |
|        - |  9146 | ` *  constants.` |
|        - |  9147 | ` * Parameters` |
|        - |  9148 | ` *  NONE.` |
|        - |  9149 | ` * Returns` |
|        - |  9150 | ` *  Returns the names of all the constants currently defined.` |
|        - |  9151 | ` */` |
|        2 |  9152 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9153 |  |
|        - |  9154 | `	ph7_value *pArray;` |
|        - |  9155 | `	/* Create the array first*/` |
|        3 |  9156 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9157 | `	if( pArray == 0 ){` |
|      ! 0 |  9158 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9159 | `		SXUNUSED(apArg);` |
|        - |  9160 | `		/* Return NULL */` |
|      ! 0 |  9161 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9162 | `		return SXRET_OK;` |
|        - |  9163 | `	}` |
|        - |  9164 | `	/* Fill the array with the defined constants */` |
|        3 |  9165 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  9166 | `	/* Return the created array */` |
|        3 |  9167 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9168 | `	return SXRET_OK;` |
|        2 |  9169 |  |
|        - |  9170 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  9171 | `/*` |
|        - |  9172 | ` * Section:` |
|        - |  9173 | ` *  Random numbers/string generators.` |
|        - |  9174 | ` * Status:` |
|        - |  9175 | ` *    Stable.` |
|        - |  9176 | ` */` |
|        - |  9177 | `/*` |
|        - |  9178 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  9179 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  9180 | ` * used by te SQLite3 library.` |
|        - |  9181 | ` */` |
|     2426 |  9182 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  9183 |  |
|        - |  9184 | `	sxu32 iNum;` |
|     2428 |  9185 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2428 |  9186 | `	return iNum;` |
|        2 |  9187 |  |
|        - |  9188 | `/*` |
|        - |  9189 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  9190 | ` * Note that the generated string is NOT null terminated.` |
|        - |  9191 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  9192 | ` * by te SQLite3 library.` |
|        - |  9193 | ` */` |
|   126476 |  9194 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  9195 |  |
|        - |  9196 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  9197 | `	int i;` |
|        - |  9198 | `	/* Generate a binary string first */` |
|   126478 |  9199 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  9200 | `	/* Turn the binary string into english based alphabet */` |
|  1391406 |  9201 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1264930 |  9202 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   632466 |  9203 | `	 }` |
|   126478 |  9204 |  |
|        - |  9205 | `/*` |
|        - |  9206 | ` * int rand()` |
|        - |  9207 | ` * int mt_rand()` |
|        - |  9208 | ` * int rand(int $min,int $max)` |
|        - |  9209 | ` * int mt_rand(int $min,int $max)` |
|        - |  9210 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  9211 | ` * Parameter` |
|        - |  9212 | ` *  $min` |
|        - |  9213 | ` *    The lowest value to return (default: 0)` |
|        - |  9214 | ` *  $max` |
|        - |  9215 | ` *   The highest value to return (default: getrandmax())` |
|        - |  9216 | ` * Return` |
|        - |  9217 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  9218 | ` * Note:` |
|        - |  9219 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9220 | ` *  by te SQLite3 library.` |
|        - |  9221 | ` */` |
|       20 |  9222 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9223 |  |
|        - |  9224 | `	sxu32 iNum;` |
|        - |  9225 | `	/* Generate the random number */` |
|       21 |  9226 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  9227 | `	if( nArg > 1 ){` |
|        - |  9228 | `		sxu32 iMin,iMax;` |
|        3 |  9229 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  9230 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  9231 | `		if( iMin < iMax ){` |
|        3 |  9232 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  9233 | `			if( iDiv > 0 ){` |
|        3 |  9234 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  9235 | `			}` |
|        1 |  9236 | `		}else if(iMax > 0 ){` |
|      ! 0 |  9237 | `			iNum %= iMax;` |
|      ! 0 |  9238 | `		}` |
|        1 |  9239 | `	}` |
|        - |  9240 | `	/* Return the number */` |
|       21 |  9241 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  9242 | `	return SXRET_OK;` |
|        1 |  9243 |  |
|        - |  9244 | `/*` |
|        - |  9245 | ` * int getrandmax(void)` |
|        - |  9246 | ` * int mt_getrandmax(void)` |
|        - |  9247 | ` * int rc4_getrandmax(void)` |
|        - |  9248 | ` *   Show largest possible random value` |
|        - |  9249 | ` * Return` |
|        - |  9250 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  9251 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  9252 | ` * Note:` |
|        - |  9253 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9254 | ` *  by te SQLite3 library.` |
|        - |  9255 | ` */` |
|        4 |  9256 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9257 |  |
|        2 |  9258 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  9259 | `	SXUNUSED(apArg);` |
|        5 |  9260 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  9261 | `	return SXRET_OK;` |
|        1 |  9262 |  |
|        - |  9263 | `/*` |
|        - |  9264 | ` * string rand_str()` |
|        - |  9265 | ` * string rand_str(int $len)` |
|        - |  9266 | ` *  Generate a random string (English alphabet).` |
|        - |  9267 | ` * Parameter` |
|        - |  9268 | ` *  $len` |
|        - |  9269 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  9270 | ` * Return` |
|        - |  9271 | ` *   A pseudo random string.` |
|        - |  9272 | ` * Note:` |
|        - |  9273 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  9274 | ` *  by te SQLite3 library.` |
|        - |  9275 | ` *  This function is a symisc extension.` |
|        - |  9276 | ` */` |
|      120 |  9277 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9278 |  |
|        - |  9279 | `	char zString[1024];` |
|      122 |  9280 | `	int iLen = 0x10;` |
|      122 |  9281 | `	if( nArg > 0 ){` |
|        - |  9282 | `		/* Get the desired length */` |
|      122 |  9283 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  9284 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  9285 | `			/* Default length */` |
|        3 |  9286 | `			iLen = 0x10;` |
|        1 |  9287 | `		}` |
|       60 |  9288 | `	}` |
|        - |  9289 | `	/* Generate the random string */` |
|      122 |  9290 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  9291 | `	/* Return the generated string */` |
|      122 |  9292 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  9293 | `	return SXRET_OK;` |
|        2 |  9294 |  |
|        - |  9295 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9296 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9297 | `/* Unique ID private data */` |
|        - |  9298 | `struct unique_id_data` |
|        - |  9299 |  |
|        - |  9300 | `	ph7_context *pCtx; /* Call context */` |
|        - |  9301 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  9302 | `};` |
|        - |  9303 | `/*` |
|        - |  9304 | ` * Binary to hex consumer callback.` |
|        - |  9305 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  9306 | ` * defined below.` |
|        - |  9307 | ` */` |
|      192 |  9308 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  9309 |  |
|      193 |  9310 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  9311 | `	sxu32 nBuflen;` |
|        - |  9312 | `	/* Extract result buffer length */` |
|      193 |  9313 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  9314 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  9315 | `			/*` |
|        - |  9316 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  9317 | `			 * string will be 13 characters long` |
|        - |  9318 | `			 */` |
|       25 |  9319 | `		return SXERR_ABORT;` |
|        - |  9320 | `	}` |
|      169 |  9321 | `	if( nBuflen > 22 ){` |
|      ! 0 |  9322 | `		return SXERR_ABORT;` |
|        - |  9323 | `	}` |
|        - |  9324 | `	/* Safely Consume the hex stream */` |
|      169 |  9325 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  9326 | `	return SXRET_OK;` |
|       97 |  9327 |  |
|        - |  9328 | `/*` |
|        - |  9329 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  9330 | ` *  Generate a unique ID` |
|        - |  9331 | ` * Parameter` |
|        - |  9332 | ` * $prefix` |
|        - |  9333 | ` *  Append this prefix to the generated unique ID.` |
|        - |  9334 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  9335 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  9336 | ` * $more_entropy` |
|        - |  9337 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  9338 | ` *  that the result will be unique.` |
|        - |  9339 | ` * Return` |
|        - |  9340 | ` *  Returns the unique identifier, as a string.` |
|        - |  9341 | ` */` |
|       24 |  9342 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9343 |  |
|        - |  9344 | `	struct unique_id_data sUniq;` |
|        - |  9345 | `	unsigned char zDigest[20];` |
|       25 |  9346 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9347 | `	const char *zPrefix;` |
|        - |  9348 | `	SHA1Context sCtx;` |
|        - |  9349 | `	char zRandom[7];` |
|        - |  9350 | `	int nPrefix;` |
|        - |  9351 | `	int entropy;` |
|        - |  9352 | `	/* Generate a random string first */` |
|       25 |  9353 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  9354 | `	/* Initialize fields */` |
|       25 |  9355 | `	zPrefix = 0;` |
|       25 |  9356 | `	nPrefix = 0;` |
|       25 |  9357 | `	entropy = 0;` |
|       25 |  9358 | `	if( nArg > 0 ){` |
|        - |  9359 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  9360 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  9361 | `		if( nArg > 1 ){` |
|      ! 0 |  9362 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  9363 | `		}` |
|      ! 0 |  9364 | `	}` |
|       25 |  9365 | `	SHA1Init(&sCtx);` |
|        - |  9366 | `	/* Generate the random ID */` |
|       25 |  9367 | `	if( nPrefix > 0 ){` |
|      ! 0 |  9368 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  9369 | `	}` |
|        - |  9370 | `	/* Append the random ID */` |
|       25 |  9371 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  9372 | `	/* Append the random string */` |
|       25 |  9373 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  9374 | `	/* Increment the number */` |
|       25 |  9375 | `	pVm->unique_id++;` |
|       25 |  9376 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  9377 | `	/* Hexify the digest */` |
|       25 |  9378 | `	sUniq.pCtx = pCtx;` |
|       25 |  9379 | `	sUniq.entropy = entropy;` |
|       25 |  9380 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  9381 | `	/* All done */` |
|       25 |  9382 | `	return PH7_OK;` |
|        1 |  9383 |  |
|        - |  9384 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9385 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  9386 | `/*` |
|        - |  9387 | ` * Section:` |
|        - |  9388 | ` *  Language construct implementation as foreign functions.` |
|        - |  9389 | ` * Status:` |
|        - |  9390 | ` *    Stable.` |
|        - |  9391 | ` */` |
|        - |  9392 | `/*` |
|        - |  9393 | ` * void echo($string...)` |
|        - |  9394 | ` *  Output one or more messages.` |
|        - |  9395 | ` * Parameters` |
|        - |  9396 | ` *  $string` |
|        - |  9397 | ` *   Message to output.` |
|        - |  9398 | ` * Return` |
|        - |  9399 | ` *  NULL.` |
|        - |  9400 | ` */` |
|      ! 0 |  9401 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9402 |  |
|        - |  9403 | `	const char *zData;` |
|      ! 0 |  9404 | `	int nDataLen = 0;` |
|        - |  9405 | `	ph7_vm *pVm;` |
|        - |  9406 | `	int i,rc;` |
|        - |  9407 | `	/* Point to the target VM */` |
|      ! 0 |  9408 | `	pVm = pCtx->pVm;` |
|        - |  9409 | `	/* Output */` |
|      ! 0 |  9410 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  9411 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  9412 | `		if( nDataLen > 0 ){` |
|      ! 0 |  9413 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  9414 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 |  9415 | `			if( rc == SXERR_ABORT ){` |
|        - |  9416 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9417 | `				return PH7_ABORT;` |
|        - |  9418 | `			}` |
|      ! 0 |  9419 | `		}` |
|      ! 0 |  9420 | `	}` |
|      ! 0 |  9421 | `	return SXRET_OK;` |
|      ! 0 |  9422 |  |
|        - |  9423 | `/*` |
|        - |  9424 | ` * int print($string...)` |
|        - |  9425 | ` *  Output one or more messages.` |
|        - |  9426 | ` * Parameters` |
|        - |  9427 | ` *  $string` |
|        - |  9428 | ` *   Message to output.` |
|        - |  9429 | ` * Return` |
|        - |  9430 | ` *  1 always.` |
|        - |  9431 | ` */` |
|        2 |  9432 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9433 |  |
|        - |  9434 | `	const char *zData;` |
|        3 |  9435 | `	int nDataLen = 0;` |
|        - |  9436 | `	ph7_vm *pVm;` |
|        - |  9437 | `	int i,rc;` |
|        - |  9438 | `	/* Point to the target VM */` |
|        3 |  9439 | `	pVm = pCtx->pVm;` |
|        - |  9440 | `	/* Output */` |
|        5 |  9441 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  9442 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  9443 | `		if( nDataLen > 0 ){` |
|        3 |  9444 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  9445 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 |  9446 | `			if( rc == SXERR_ABORT ){` |
|        - |  9447 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  9448 | `				return PH7_ABORT;` |
|        - |  9449 | `			}` |
|        1 |  9450 | `		}` |
|        2 |  9451 | `	}` |
|        - |  9452 | `	/* Return 1 */` |
|        3 |  9453 | `	ph7_result_int(pCtx,1);` |
|        3 |  9454 | `	return SXRET_OK;` |
|        2 |  9455 |  |
|        - |  9456 | `/*` |
|        - |  9457 | ` * void exit(string $msg)` |
|        - |  9458 | ` * void exit(int $status)` |
|        - |  9459 | ` * void die(string $ms)` |
|        - |  9460 | ` * void die(int $status)` |
|        - |  9461 | ` *   Output a message and terminate program execution.` |
|        - |  9462 | ` * Parameter` |
|        - |  9463 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  9464 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  9465 | ` *  and not printed` |
|        - |  9466 | ` * Return` |
|        - |  9467 | ` *  NULL` |
|        - |  9468 | ` */` |
|      ! 0 |  9469 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  9470 |  |
|      ! 0 |  9471 | `	if( nArg > 0 ){` |
|      ! 0 |  9472 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  9473 | `			const char *zData;` |
|      ! 0 |  9474 | `			int iLen = 0;` |
|        - |  9475 | `			/* Print exit message */` |
|      ! 0 |  9476 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  9477 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  9478 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  9479 | `			sxi32 iExitStatus;` |
|        - |  9480 | `			/* Record exit status code */` |
|      ! 0 |  9481 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  9482 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  9483 | `		}` |
|      ! 0 |  9484 | `	}` |
|        - |  9485 | `	/* Check if we are in an included file */` |
|      ! 0 |  9486 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  9487 | `		/* Exit the entire process */` |
|      ! 0 |  9488 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  9489 | `	}` |
|        - |  9490 | `	/* Abort processing immediately */` |
|      ! 0 |  9491 | `	return PH7_ABORT;` |
|      ! 0 |  9492 |  |
|        - |  9493 | `/*` |
|        - |  9494 | ` * bool isset($var,...)` |
|        - |  9495 | ` *  Finds out whether a variable is set.` |
|        - |  9496 | ` * Parameters` |
|        - |  9497 | ` *  One or more variable to check.` |
|        - |  9498 | ` * Return` |
|        - |  9499 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  9500 | ` */` |
|    76716 |  9501 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9502 |  |
|        - |  9503 | `	ph7_value *pObj;` |
|    76718 |  9504 | `	int res = 0;` |
|        - |  9505 | `	int i;` |
|    76718 |  9506 | `	if( nArg < 1 ){` |
|        - |  9507 | `		/* Missing arguments,return false */` |
|      ! 0 |  9508 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  9509 | `		return SXRET_OK;` |
|        - |  9510 | `	}` |
|        - |  9511 | `	/* Iterate over available arguments */` |
|   101014 |  9512 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    76718 |  9513 | `		pObj = apArg[i];` |
|    76718 |  9514 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    51872 |  9515 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9516 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  9517 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  9518 | `			}` |
|    25935 |  9519 | `		}` |
|    76718 |  9520 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    76718 |  9521 | `		if( !res ){` |
|        - |  9522 | `			/* Variable not set,return FALSE */` |
|    52422 |  9523 | `			ph7_result_bool(pCtx,0);` |
|    52422 |  9524 | `			return SXRET_OK;` |
|        - |  9525 | `		}` |
|    12150 |  9526 | `	}` |
|        - |  9527 | `	/* All given variable are set,return TRUE */` |
|    24298 |  9528 | `	ph7_result_bool(pCtx,1);` |
|    24298 |  9529 | `	return SXRET_OK;` |
|    38360 |  9530 |  |
|        - |  9531 | `/*` |
|        - |  9532 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  9533 | ` * frame,the reference table and discard it's contents.` |
|        - |  9534 | ` * This function never fail and always return SXRET_OK.` |
|        - |  9535 | ` */` |
|  3037850 |  9536 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  9537 |  |
|        - |  9538 | `	ph7_value *pObj;` |
|        - |  9539 | `	VmRefObj *pRef;` |
|  3037852 |  9540 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3037852 |  9541 | `	if( pObj ){` |
|        - |  9542 | `		/* Release the object */` |
|  3037852 |  9543 | `		PH7_MemObjRelease(pObj);` |
|  1518925 |  9544 | `	}` |
|        - |  9545 | `	/* Remove old reference links */` |
|  3037852 |  9546 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3037852 |  9547 | `	if( pRef ){` |
|  3037846 |  9548 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  9549 | `		/* Unlink from the reference table */` |
|  3037846 |  9550 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3037846 |  9551 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  9552 | `			VmSlot sFree;` |
|        - |  9553 | `			/* Restore to the free list */` |
|  3037840 |  9554 | `			sFree.nIdx = nObjIdx;` |
|  3037840 |  9555 | `			sFree.pUserData = 0;` |
|  3037840 |  9556 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1518919 |  9557 | `		}` |
|  1518922 |  9558 | `	}` |
|  3037852 |  9559 | `	return SXRET_OK;` |
|        2 |  9560 |  |
|        - |  9561 | `/*` |
|        - |  9562 | ` * void unset($var,...)` |
|        - |  9563 | ` *   Unset one or more given variable.` |
|        - |  9564 | ` * Parameters` |
|        - |  9565 | ` *  One or more variable to unset.` |
|        - |  9566 | ` * Return` |
|        - |  9567 | ` *  Nothing.` |
|        - |  9568 | ` */` |
|     6844 |  9569 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9570 |  |
|        - |  9571 | `	ph7_value *pObj;` |
|        - |  9572 | `	ph7_vm *pVm;` |
|        - |  9573 | `	int i;` |
|        - |  9574 | `	/* Point to the target VM */` |
|     6846 |  9575 | `	pVm = pCtx->pVm;` |
|        - |  9576 | `	/* Iterate and unset */` |
|    13690 |  9577 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6846 |  9578 | `		pObj = apArg[i];` |
|     6846 |  9579 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 |  9580 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  9581 | `				/* Throw an error */` |
|      ! 0 |  9582 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  9583 | `			}` |
|      ! 0 |  9584 | `		}else{` |
|     6846 |  9585 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  9586 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6846 |  9587 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6840 |  9588 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3419 |  9589 | `			}` |
|        - |  9590 | `		}` |
|     3424 |  9591 | `	}` |
|     6846 |  9592 | `	return SXRET_OK;` |
|        2 |  9593 |  |
|        - |  9594 | `/*` |
|        - |  9595 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  9596 | ` */` |
|      110 |  9597 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9598 |  |
|      111 |  9599 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  9600 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9601 | `	ph7_value *pObj;` |
|        - |  9602 | `	sxu32 nIdx;` |
|        - |  9603 | `	/* Extract the memory object */` |
|      111 |  9604 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  9605 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  9606 | `	if( pObj ){` |
|      111 |  9607 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  9608 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  9609 | `				SyString sName;` |
|        - |  9610 | `				ph7_value sKey;` |
|        - |  9611 | `				/* Perform the insertion */` |
|      109 |  9612 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  9613 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  9614 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  9615 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  9616 | `			}` |
|       54 |  9617 | `		}` |
|       55 |  9618 | `	}` |
|      111 |  9619 | `	return SXRET_OK;` |
|        1 |  9620 |  |
|        - |  9621 | `/*` |
|        - |  9622 | ` * array get_defined_vars(void)` |
|        - |  9623 | ` *  Returns an array of all defined variables.` |
|        - |  9624 | ` * Parameter` |
|        - |  9625 | ` *  None` |
|        - |  9626 | ` * Return` |
|        - |  9627 | ` *  An array with all the variables defined in the current scope.` |
|        - |  9628 | ` */` |
|        2 |  9629 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9630 |  |
|        3 |  9631 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9632 | `	ph7_value *pArray;` |
|        - |  9633 | `	/* Create a new array */` |
|        3 |  9634 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9635 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9636 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9637 | `		SXUNUSED(apArg);` |
|        - |  9638 | `		/* Return NULL */` |
|      ! 0 |  9639 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9640 | `		return SXRET_OK;` |
|        - |  9641 | `	}` |
|        - |  9642 | `	/* Superglobals first */` |
|        3 |  9643 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  9644 | `	/* Then variable defined in the current frame */` |
|        3 |  9645 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  9646 | `	/* Finally,return the created array */` |
|        3 |  9647 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9648 | `	return SXRET_OK;` |
|        2 |  9649 |  |
|        - |  9650 | `/*` |
|        - |  9651 | ` * bool gettype($var)` |
|        - |  9652 | ` *  Get the type of a variable` |
|        - |  9653 | ` * Parameters` |
|        - |  9654 | ` *   $var` |
|        - |  9655 | ` *    The variable being type checked.` |
|        - |  9656 | ` * Return` |
|        - |  9657 | ` *   String representation of the given variable type.` |
|        - |  9658 | ` */` |
|       32 |  9659 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9660 |  |
|       34 |  9661 | `	const char *zType = "Empty";` |
|       34 |  9662 | `	if( nArg > 0 ){` |
|       34 |  9663 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  9664 | `	}` |
|        - |  9665 | `	/* Return the variable type */` |
|       34 |  9666 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  9667 | `	return SXRET_OK;` |
|        2 |  9668 |  |
|        - |  9669 | `/*` |
|        - |  9670 | ` * string get_resource_type(resource $handle)` |
|        - |  9671 | ` *  This function gets the type of the given resource.` |
|        - |  9672 | ` * Parameters` |
|        - |  9673 | ` *  $handle` |
|        - |  9674 | ` *  The evaluated resource handle.` |
|        - |  9675 | ` * Return` |
|        - |  9676 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  9677 | ` *  representing its type. If the type is not identified by this function` |
|        - |  9678 | ` *  the return value will be the string Unknown.` |
|        - |  9679 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  9680 | ` *  is not a resource.` |
|        - |  9681 | ` */` |
|        2 |  9682 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9683 |  |
|        3 |  9684 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  9685 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  9686 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9687 | `		return PH7_OK;` |
|        - |  9688 | `	}` |
|        3 |  9689 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  9690 | `	return SXRET_OK;` |
|        2 |  9691 |  |
|        - |  9692 | `/*` |
|        - |  9693 | ` * void var_dump(expression,....)` |
|        - |  9694 | ` *   var_dump � Dumps information about a variable` |
|        - |  9695 | ` * Parameters` |
|        - |  9696 | ` *   One or more expression to dump.` |
|        - |  9697 | ` * Returns` |
|        - |  9698 | ` *  Nothing.` |
|        - |  9699 | ` */` |
|      218 |  9700 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9701 |  |
|        - |  9702 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  9703 | `	int i;` |
|      220 |  9704 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  9705 | `	/* Dump one or more expressions */` |
|      444 |  9706 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  9707 | `		ph7_value *pObj = apArg[i];` |
|        - |  9708 | `		/* Reset the working buffer */` |
|      226 |  9709 | `		SyBlobReset(&sDump);` |
|        - |  9710 | `		/* Dump the given expression */` |
|      226 |  9711 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  9712 | `		/* Output */` |
|      226 |  9713 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  9714 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  9715 | `		}` |
|      114 |  9716 | `	}` |
|        - |  9717 | `	/* Release the working buffer */` |
|      220 |  9718 | `	SyBlobRelease(&sDump);` |
|      220 |  9719 | `	return SXRET_OK;` |
|        2 |  9720 |  |
|        - |  9721 | `/*` |
|        - |  9722 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  9723 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  9724 | ` * Parameters` |
|        - |  9725 | ` *   expression: Expression to dump` |
|        - |  9726 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  9727 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  9728 | ` *            print_r() will return the information rather than print it.` |
|        - |  9729 | ` * Return` |
|        - |  9730 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  9731 | ` *  Otherwise, the return value is TRUE.` |
|        - |  9732 | ` */` |
|       16 |  9733 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9734 |  |
|       17 |  9735 | `	int ret_string = 0;` |
|        - |  9736 | `	SyBlob sDump;` |
|       17 |  9737 | `	if( nArg < 1 ){` |
|        - |  9738 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9739 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9740 | `		return SXRET_OK;` |
|        - |  9741 | `	}` |
|       17 |  9742 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  9743 | `	if ( nArg > 1 ){` |
|        - |  9744 | `		/* Where to redirect output */` |
|       11 |  9745 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  9746 | `	}` |
|        - |  9747 | `	/* Generate dump */` |
|       17 |  9748 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  9749 | `	if( !ret_string ){` |
|        - |  9750 | `		/* Output dump */` |
|        7 |  9751 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9752 | `		/* Return true */` |
|        7 |  9753 | `		ph7_result_bool(pCtx,1);` |
|        4 |  9754 | `	}else{` |
|        - |  9755 | `		/* Generated dump as return value */` |
|       11 |  9756 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9757 | `	}` |
|        - |  9758 | `	/* Release the working buffer */` |
|       17 |  9759 | `	SyBlobRelease(&sDump);` |
|       17 |  9760 | `	return SXRET_OK;` |
|        9 |  9761 |  |
|        - |  9762 | `/*` |
|        - |  9763 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  9764 | ` * Same job as print_r. (see coment above)` |
|        - |  9765 | ` */` |
|        2 |  9766 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9767 |  |
|        3 |  9768 | `	int ret_string = 0;` |
|        - |  9769 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  9770 | `	if( nArg < 1 ){` |
|        - |  9771 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  9772 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9773 | `		return SXRET_OK;` |
|        - |  9774 | `	}` |
|        3 |  9775 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  9776 | `	if ( nArg > 1 ){` |
|        - |  9777 | `		/* Where to redirect output */` |
|        3 |  9778 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  9779 | `	}` |
|        - |  9780 | `	/* Generate dump */` |
|        3 |  9781 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  9782 | `	if( !ret_string ){` |
|        - |  9783 | `		/* Output dump */` |
|      ! 0 |  9784 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9785 | `		/* Return NULL */` |
|      ! 0 |  9786 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9787 | `	}else{` |
|        - |  9788 | `		/* Generated dump as return value */` |
|        3 |  9789 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  9790 | `	}` |
|        - |  9791 | `	/* Release the working buffer */` |
|        3 |  9792 | `	SyBlobRelease(&sDump);` |
|        3 |  9793 | `	return SXRET_OK;` |
|        2 |  9794 |  |
|        - |  9795 | `/*` |
|        - |  9796 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  9797 | ` *  Set/get the various assert flags.` |
|        - |  9798 | ` * Parameter` |
|        - |  9799 | ` * $what` |
|        - |  9800 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  9801 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  9802 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  9803 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  9804 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  9805 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  9806 | ` * $value` |
|        - |  9807 | ` *   An optional new value for the option.` |
|        - |  9808 | ` * Return` |
|        - |  9809 | ` *  Old setting on success or FALSE on failure.` |
|        - |  9810 | ` */` |
|       28 |  9811 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9812 |  |
|       30 |  9813 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9814 | `	int iOption;` |
|        - |  9815 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 |  9816 | `	if( nArg < 1 ){` |
|        3 |  9817 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9818 | `			"ArgumentCountError",` |
|        - |  9819 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  9820 | `			);` |
|        - |  9821 | `	}` |
|        - |  9822 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 |  9823 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 |  9824 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  9825 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9826 | `			"TypeError",` |
|        - |  9827 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  9828 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  9829 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  9830 | `			);` |
|        - |  9831 | `	}` |
|       28 |  9832 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  9833 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  9834 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  9835 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 |  9836 | `	switch( iOption ){` |
|        5 |  9837 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  9838 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 |  9839 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 |  9840 | `		if( nArg > 1 ){` |
|        5 |  9841 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9842 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  9843 | `			}else{` |
|        3 |  9844 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  9845 | `			}` |
|        2 |  9846 | `		}` |
|       12 |  9847 | `		break;` |
|        1 |  9848 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  9849 | `		/* Return old callback or null */` |
|        3 |  9850 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  9851 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  9852 | `		}else{` |
|        3 |  9853 | `			ph7_result_null(pCtx);` |
|        - |  9854 | `		}` |
|        3 |  9855 | `		if( nArg > 1 ){` |
|      ! 0 |  9856 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  9857 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  9858 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  9859 | `			}else{` |
|      ! 0 |  9860 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  9861 | `			}` |
|      ! 0 |  9862 | `		}` |
|        3 |  9863 | `		break;` |
|        5 |  9864 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  9865 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  9866 | `		if( nArg > 1 ){` |
|        5 |  9867 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  9868 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  9869 | `			}else{` |
|        3 |  9870 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  9871 | `			}` |
|        2 |  9872 | `		}` |
|       11 |  9873 | `		break;` |
|      ! 0 |  9874 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  9875 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9876 | `		break;` |
|        1 |  9877 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  9878 | `		ph7_result_int(pCtx, 1);` |
|        3 |  9879 | `		break;` |
|      ! 0 |  9880 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  9881 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  9882 | `		break;` |
|        1 |  9883 | `	default:` |
|        - |  9884 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  9885 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9886 | `			"ValueError",` |
|        - |  9887 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  9888 | `			);` |
|        - |  9889 | `	}` |
|       26 |  9890 | `	return PH7_OK;` |
|       16 |  9891 |  |
|        - |  9892 | `/*` |
|        - |  9893 | ` * bool assert(mixed $assertion)` |
|        - |  9894 | ` *  Checks if assertion is FALSE.` |
|        - |  9895 | ` * Parameter` |
|        - |  9896 | ` *  $assertion` |
|        - |  9897 | ` *    The assertion to test.` |
|        - |  9898 | ` * Return` |
|        - |  9899 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  9900 | ` */` |
|       24 |  9901 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9902 |  |
|       26 |  9903 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9904 | `	int iFlags,iResult;` |
|        - |  9905 | `	const char *zDesc;` |
|        - |  9906 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 |  9907 | `	if( nArg < 1 ){` |
|        3 |  9908 | `		return PH7_VmThrowException(pCtx,` |
|        - |  9909 | `			"ArgumentCountError",` |
|        - |  9910 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  9911 | `			);` |
|        - |  9912 | `	}` |
|       24 |  9913 | `	iFlags = pVm->iAssertFlags;` |
|       24 |  9914 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  9915 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  9916 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  9917 | `		return PH7_OK;` |
|        - |  9918 | `	}` |
|        - |  9919 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 |  9920 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 |  9921 | `	if( !iResult ){` |
|        - |  9922 | `		/* Assertion failed */` |
|        - |  9923 | `		/* Extract optional description */` |
|       13 |  9924 | `		zDesc = 0;` |
|       13 |  9925 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9926 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  9927 | `		}` |
|       13 |  9928 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  9929 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  9930 | `			ph7_value sFile,sLine;` |
|        - |  9931 | `			ph7_value *apCbArg[3];` |
|        - |  9932 | `			SyString *pFile;` |
|        - |  9933 | `			/* Extract the processed script */` |
|      ! 0 |  9934 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  9935 | `			if( pFile == 0 ){` |
|      ! 0 |  9936 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  9937 | `			}` |
|        - |  9938 | `			/* Invoke the callback */` |
|      ! 0 |  9939 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  9940 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  9941 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  9942 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  9943 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  9944 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  9945 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  9946 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  9947 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  9948 | `		}` |
|       13 |  9949 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  9950 | `			/* Abort VM execution immediately */` |
|      ! 0 |  9951 | `			return PH7_ABORT;` |
|        - |  9952 | `		}` |
|        - |  9953 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  9954 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  9955 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9956 | `				"AssertionError",` |
|        - |  9957 | `				"%s",` |
|        1 |  9958 | `				zDesc` |
|        - |  9959 | `				);` |
|      ! 0 |  9960 | `		}else{` |
|       11 |  9961 | `			return PH7_VmThrowException(pCtx,` |
|        - |  9962 | `				"AssertionError",` |
|        - |  9963 | `				"assert(false)"` |
|        - |  9964 | `				);` |
|        - |  9965 | `		}` |
|        - |  9966 | `	}` |
|        - |  9967 | `	/* Assertion passed */` |
|       11 |  9968 | `	ph7_result_bool(pCtx,1);` |
|       11 |  9969 | `	return PH7_OK;` |
|       14 |  9970 |  |
|        - |  9971 | `/*` |
|        - |  9972 | ` * Section:` |
|        - |  9973 | ` *  Error reporting functions.` |
|        - |  9974 | ` * Status:` |
|        - |  9975 | ` *    Stable.` |
|        - |  9976 | ` */` |
|        - |  9977 | `/*` |
|        - |  9978 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  9979 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  9980 | ` * Parameters` |
|        - |  9981 | ` *  $error_msg` |
|        - |  9982 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  9983 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  9984 | ` * $error_type` |
|        - |  9985 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  9986 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  9987 | ` * Return` |
|        - |  9988 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  9989 | ` */` |
|       12 |  9990 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9991 |  |
|       14 |  9992 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  9993 | `	int rc = PH7_OK;` |
|       14 |  9994 | `	if( nArg > 0 ){` |
|        - |  9995 | `		const char *zErr;` |
|        - |  9996 | `		int nLen;` |
|        - |  9997 | `		/* Extract the error message */` |
|       12 |  9998 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  9999 | `		if( nArg > 1 ){` |
|        - | 10000 | `			/* Extract the error type */` |
|       12 | 10001 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 10002 | `			switch( nErr ){` |
|        1 | 10003 | `			case 1:   /* E_ERROR */` |
|        - | 10004 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 10005 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 10006 | `			case 256: /* E_USER_ERROR */` |
|        3 | 10007 | `				nErr = PH7_CTX_ERR;` |
|        3 | 10008 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 10009 | `				break;` |
|        1 | 10010 | `			case 2:   /* E_WARNING */` |
|        - | 10011 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 10012 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 10013 | `			case 512: /* E_USER_WARNING */` |
|        3 | 10014 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 10015 | `				break;` |
|        3 | 10016 | `			default:` |
|        8 | 10017 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 10018 | `				break;` |
|        - | 10019 | `			}` |
|        5 | 10020 | `		}` |
|        - | 10021 | `		/* Report error */` |
|       12 | 10022 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 10023 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 10024 | `			return rc;` |
|        - | 10025 | `		}` |
|        - | 10026 | `		/* Return true */` |
|       12 | 10027 | `		ph7_result_bool(pCtx,1);` |
|        7 | 10028 | `	}else{` |
|        - | 10029 | `		/* Missing arguments,return FALSE */` |
|        3 | 10030 | `		ph7_result_bool(pCtx,0);` |
|        - | 10031 | `	}` |
|       14 | 10032 | `	return rc;` |
|        8 | 10033 |  |
|        - | 10034 | `/*` |
|        - | 10035 | ` * int error_reporting([int $level])` |
|        - | 10036 | ` *  Sets which PHP errors are reported.` |
|        - | 10037 | ` * Parameters` |
|        - | 10038 | ` *  $level` |
|        - | 10039 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 10040 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 10041 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 10042 | ` *   levels will not always behave as expected.` |
|        - | 10043 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 10044 | ` *   in the predefined constants.` |
|        - | 10045 | ` * Return` |
|        - | 10046 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 10047 | ` *   parameter is given.` |
|        - | 10048 | ` */` |
|       38 | 10049 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10050 |  |
|       40 | 10051 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10052 | `	int nOld;` |
|        - | 10053 | `	/* Extract the old reporting level */` |
|       40 | 10054 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 10055 | `	if( nArg > 0 ){` |
|        - | 10056 | `		int nNew;` |
|        - | 10057 | `		/* Extract the desired error reporting level */` |
|       32 | 10058 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 10059 | `		if( !nNew ){` |
|        - | 10060 | `			/* Do not report errors at all */` |
|        5 | 10061 | `			pVm->bErrReport = 0;` |
|        3 | 10062 | `		}else{` |
|        - | 10063 | `			/* Report all errors */` |
|       28 | 10064 | `			pVm->bErrReport = 1;` |
|        - | 10065 | `		}` |
|       15 | 10066 | `	}` |
|        - | 10067 | `	/* Return the old level */` |
|       40 | 10068 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 10069 | `	return PH7_OK;` |
|        2 | 10070 |  |
|        - | 10071 | `/*` |
|        - | 10072 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 10073 | ` *  Send an error message somewhere.` |
|        - | 10074 | ` * Parameter` |
|        - | 10075 | ` *  $message` |
|        - | 10076 | ` *   The error message that should be logged.` |
|        - | 10077 | ` *  $message_type` |
|        - | 10078 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 10079 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 10080 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 10081 | ` *       This is the default option.` |
|        - | 10082 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 10083 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 10084 | ` *    2  No longer an option.` |
|        - | 10085 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 10086 | ` *       to the end of the message string.` |
|        - | 10087 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 10088 | ` *  $destination` |
|        - | 10089 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 10090 | ` *  $extra_headers` |
|        - | 10091 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 10092 | ` * Return` |
|        - | 10093 | ` *  TRUE on success or FALSE on failure.` |
|        - | 10094 | ` * NOTE:` |
|        - | 10095 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 10096 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 10097 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 10098 | ` *  Otherwise this function is no-op.` |
|        - | 10099 | ` */` |
|        4 | 10100 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10101 |  |
|        - | 10102 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 10103 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 10104 | `	int iType = 0;` |
|        5 | 10105 | `	if( nArg < 1 ){` |
|        - | 10106 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 10107 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10108 | `		return PH7_OK;` |
|        - | 10109 | `	}` |
|        5 | 10110 | `	if( pVm->xErrLog  ){` |
|        - | 10111 | `		/* Invoke the user callback */` |
|      ! 0 | 10112 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 10113 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 10114 | `		if( nArg > 1 ){` |
|      ! 0 | 10115 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 10116 | `			if( nArg > 2 ){` |
|      ! 0 | 10117 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 10118 | `				if( nArg > 3 ){` |
|      ! 0 | 10119 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 10120 | `				}` |
|      ! 0 | 10121 | `			}` |
|      ! 0 | 10122 | `		}` |
|      ! 0 | 10123 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 10124 | `	}` |
|        - | 10125 | `	/* Retun TRUE */` |
|        5 | 10126 | `	ph7_result_bool(pCtx,1);` |
|        5 | 10127 | `	return PH7_OK;` |
|        3 | 10128 |  |
|        - | 10129 | `/*` |
|        - | 10130 | ` * bool restore_exception_handler(void)` |
|        - | 10131 | ` *  Restores the previously defined exception handler function.` |
|        - | 10132 | ` * Parameter` |
|        - | 10133 | ` *  None` |
|        - | 10134 | ` * Return` |
|        - | 10135 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 10136 | ` */` |
|        4 | 10137 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10138 |  |
|        5 | 10139 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10140 | `	ph7_value *pOld,*pNew;` |
|        - | 10141 | `	/* Point to the old and the new handler */` |
|        5 | 10142 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 10143 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 10144 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10145 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10146 | `		SXUNUSED(apArg);` |
|        - | 10147 | `		/* No installed handler,return FALSE */` |
|        5 | 10148 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10149 | `		return PH7_OK;` |
|        - | 10150 | `	}` |
|        - | 10151 | `	/* Copy the old handler */` |
|      ! 0 | 10152 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10153 | `	PH7_MemObjRelease(pOld);` |
|        - | 10154 | `	/* Return TRUE */` |
|      ! 0 | 10155 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10156 | `	return PH7_OK;` |
|        3 | 10157 |  |
|        - | 10158 | `/*` |
|        - | 10159 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 10160 | ` *  Sets a user-defined exception handler function.` |
|        - | 10161 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 10162 | ` * NOTE` |
|        - | 10163 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 10164 | ` *  the satndard PHP engine.` |
|        - | 10165 | ` * Parameters` |
|        - | 10166 | ` *  $exception_handler` |
|        - | 10167 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 10168 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 10169 | ` *   that was thrown.` |
|        - | 10170 | ` *  Note:` |
|        - | 10171 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10172 | ` * Return` |
|        - | 10173 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 10174 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10175 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10176 | ` */` |
|        4 | 10177 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10178 |  |
|        6 | 10179 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10180 | `	ph7_value *pOld,*pNew;` |
|        - | 10181 | `	/* Point to the old and the new handler */` |
|        6 | 10182 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 10183 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 10184 | `	/* Return the old handler */` |
|        6 | 10185 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 10186 | `	if( nArg > 0 ){` |
|        6 | 10187 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10188 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 10189 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 10190 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 10191 | `		}else{` |
|        6 | 10192 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10193 | `			/* Install the new handler */` |
|        6 | 10194 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10195 | `		}` |
|        2 | 10196 | `	}` |
|        6 | 10197 | `	return PH7_OK;` |
|        2 | 10198 |  |
|        - | 10199 | `/*` |
|        - | 10200 | ` * bool restore_error_handler(void)` |
|        - | 10201 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10202 | ` * Parameters:` |
|        - | 10203 | ` *  None.` |
|        - | 10204 | ` * Return` |
|        - | 10205 | ` *  Always TRUE.` |
|        - | 10206 | ` */` |
|        4 | 10207 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10208 |  |
|        5 | 10209 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10210 | `	ph7_value *pOld,*pNew;` |
|        - | 10211 | `	/* Point to the old and the new handler */` |
|        5 | 10212 | `	pOld = &pVm->aErrCB[0];` |
|        5 | 10213 | `	pNew = &pVm->aErrCB[1];` |
|        5 | 10214 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 10215 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 10216 | `		SXUNUSED(apArg);` |
|        - | 10217 | `		/* No installed callback,return FALSE */` |
|        5 | 10218 | `		ph7_result_bool(pCtx,0);` |
|        5 | 10219 | `		return PH7_OK;` |
|        - | 10220 | `	}` |
|        - | 10221 | `	/* Copy the old callback */` |
|      ! 0 | 10222 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 10223 | `	PH7_MemObjRelease(pOld);` |
|        - | 10224 | `	/* Return TRUE */` |
|      ! 0 | 10225 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 10226 | `	return PH7_OK;` |
|        3 | 10227 |  |
|        - | 10228 | `/*` |
|        - | 10229 | ` * value set_error_handler(callable $error_handler)` |
|        - | 10230 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10231 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 10232 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 10233 | ` *  Sets a user-defined error handler function.` |
|        - | 10234 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 10235 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 10236 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 10237 | ` *  conditions (using trigger_error()).` |
|        - | 10238 | ` * Parameters` |
|        - | 10239 | ` *  $error_handler` |
|        - | 10240 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 10241 | ` *   describing the error.` |
|        - | 10242 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 10243 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 10244 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 10245 | ` *   The function can be shown as:` |
|        - | 10246 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 10247 | ` *     errno` |
|        - | 10248 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 10249 | ` *   errstr` |
|        - | 10250 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 10251 | ` *   errfile` |
|        - | 10252 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 10253 | ` *     was raised in, as a string.` |
|        - | 10254 | ` *  Note:` |
|        - | 10255 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 10256 | ` * Return` |
|        - | 10257 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 10258 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 10259 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 10260 | ` */` |
|     9374 | 10261 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10262 |  |
|     9376 | 10263 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10264 | `	ph7_value *pOld,*pNew;` |
|        - | 10265 | `	/* Point to the old and the new handler */` |
|     9376 | 10266 | `	pOld = &pVm->aErrCB[0];` |
|     9376 | 10267 | `	pNew = &pVm->aErrCB[1];` |
|        - | 10268 | `	/* Return the old handler */` |
|     9376 | 10269 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9376 | 10270 | `	if( nArg > 0 ){` |
|     9376 | 10271 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 10272 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4687 | 10273 | `			PH7_MemObjRelease(pNew);` |
|     4687 | 10274 | `			ph7_result_bool(pCtx,1);` |
|     2344 | 10275 | `		}else{` |
|     4690 | 10276 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 10277 | `			/* Install the new handler */` |
|     4690 | 10278 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 10279 | `		}` |
|     4687 | 10280 | `	}` |
|     9376 | 10281 | `	return PH7_OK;` |
|        2 | 10282 |  |
|        - | 10283 | `/*` |
|        - | 10284 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 10285 | ` *  Generates a backtrace.` |
|        - | 10286 | ` * Paramaeter` |
|        - | 10287 | ` *  $options` |
|        - | 10288 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 10289 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 10290 | ` *   all the function/method arguments, to save memory.` |
|        - | 10291 | ` * $limit` |
|        - | 10292 | ` *   (Not Used)` |
|        - | 10293 | ` * Return` |
|        - | 10294 | ` *  An array.The possible returned elements are as follows:` |
|        - | 10295 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 10296 | ` *          Name        Type      Description` |
|        - | 10297 | ` *          ------      ------     -----------` |
|        - | 10298 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 10299 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 10300 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 10301 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 10302 | ` *          object      object    The current object.` |
|        - | 10303 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 10304 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 10305 | ` */` |
|      544 | 10306 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10307 |  |
|      546 | 10308 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10309 | `	ph7_value *pArray;` |
|        - | 10310 | `	ph7_class *pClass;` |
|        - | 10311 | `	ph7_value *pValue;` |
|        - | 10312 | `	SyString *pFile;` |
|        - | 10313 | `	/* Create a new array */` |
|      546 | 10314 | `	pArray = ph7_context_new_array(pCtx);` |
|      546 | 10315 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      546 | 10316 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 10317 | `		/* Out of memory,return NULL */` |
|      ! 0 | 10318 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 10319 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10320 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10321 | `		SXUNUSED(apArg);` |
|      ! 0 | 10322 | `		return PH7_OK;` |
|        - | 10323 | `	}` |
|        - | 10324 | `	/* Dump running function name and it's arguments  */` |
|      546 | 10325 | `	if( pVm->pFrame->pParent ){` |
|      546 | 10326 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 10327 | `		ph7_vm_func *pFunc;` |
|        - | 10328 | `		ph7_value *pArg;` |
|      546 | 10329 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      546 | 10330 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      546 | 10331 | `		if( pFrame->pParent && pFunc ){` |
|      546 | 10332 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      546 | 10333 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      546 | 10334 | `			ph7_value_reset_string_cursor(pValue);` |
|      272 | 10335 | `		}` |
|        - | 10336 | `		/* Function arguments */` |
|      546 | 10337 | `		pArg = ph7_context_new_array(pCtx);` |
|      546 | 10338 | `		if( pArg  ){` |
|        - | 10339 | `			ph7_value *pObj;` |
|        - | 10340 | `			VmSlot *aSlot;` |
|        - | 10341 | `			sxu32 n;` |
|        - | 10342 | `			/* Start filling the array with the given arguments */` |
|      546 | 10343 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2170 | 10344 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1626 | 10345 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1626 | 10346 | `				if( pObj ){` |
|     1626 | 10347 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      812 | 10348 | `				}` |
|      814 | 10349 | `			}` |
|        - | 10350 | `			/* Save the array */` |
|      546 | 10351 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      272 | 10352 | `		}` |
|      272 | 10353 | `	}` |
|      546 | 10354 | `	ph7_value_int(pValue,1);` |
|        - | 10355 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 10356 | `	 * line numbers at run-time. )` |
|        - | 10357 | `	 */` |
|      546 | 10358 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 10359 | `	/* Current processed script */` |
|      546 | 10360 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      546 | 10361 | `	if( pFile ){` |
|      546 | 10362 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      546 | 10363 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      546 | 10364 | `		ph7_value_reset_string_cursor(pValue);` |
|      272 | 10365 | `	}` |
|        - | 10366 | `	/* Top class */` |
|      546 | 10367 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      546 | 10368 | `	if( pClass ){` |
|      542 | 10369 | `		ph7_value_reset_string_cursor(pValue);` |
|      542 | 10370 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      542 | 10371 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      270 | 10372 | `	}` |
|        - | 10373 | `	/* Return the freshly created array */` |
|      546 | 10374 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10375 | `	/*` |
|        - | 10376 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 10377 | `	 * as soon we return from this function.` |
|        - | 10378 | `	 */` |
|      546 | 10379 | `	return PH7_OK;` |
|      274 | 10380 |  |
|        - | 10381 | `/*` |
|        - | 10382 | ` * Generate a small backtrace.` |
|        - | 10383 | ` * Store the generated dump in the given BLOB` |
|        - | 10384 | ` */` |
|        4 | 10385 | `static int VmMiniBacktrace(` |
|        - | 10386 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10387 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 10388 | `	)` |
|        1 | 10389 |  |
|        5 | 10390 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10391 | `	ph7_vm_func *pFunc;` |
|        - | 10392 | `	ph7_class *pClass;` |
|        - | 10393 | `	SyString *pFile;` |
|        - | 10394 | `	/* Called function */` |
|        5 | 10395 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 10396 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 10397 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10398 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 10399 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 10400 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 10401 | `	}else{` |
|      ! 0 | 10402 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 10403 | `	}` |
|        5 | 10404 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 10405 | `	/* Current processed script */` |
|        5 | 10406 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 10407 | `	if( pFile ){` |
|        5 | 10408 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 10409 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 10410 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 10411 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 10412 | `	}` |
|        - | 10413 | `	/* Top class */` |
|        5 | 10414 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 10415 | `	if( pClass ){` |
|      ! 0 | 10416 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 10417 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 10418 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 10419 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 10420 | `	}` |
|        5 | 10421 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 10422 | `	/* All done */` |
|        5 | 10423 | `	return SXRET_OK;` |
|        1 | 10424 |  |
|        - | 10425 | `/*` |
|        - | 10426 | ` * void debug_print_backtrace()` |
|        - | 10427 | ` *  Prints a backtrace` |
|        - | 10428 | ` * Parameters` |
|        - | 10429 | ` * None` |
|        - | 10430 | ` * Return` |
|        - | 10431 | ` * NULL` |
|        - | 10432 | ` */` |
|        2 | 10433 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10434 |  |
|        3 | 10435 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10436 | `	SyBlob sDump;` |
|        3 | 10437 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10438 | `	/* Generate the backtrace */` |
|        3 | 10439 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10440 | `	/* Output backtrace */` |
|        3 | 10441 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 10442 | `	/* All done,cleanup */` |
|        3 | 10443 | `	SyBlobRelease(&sDump);` |
|        1 | 10444 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10445 | `	SXUNUSED(apArg);` |
|        3 | 10446 | `	return PH7_OK;` |
|        1 | 10447 |  |
|        - | 10448 | `/*` |
|        - | 10449 | ` * string debug_string_backtrace()` |
|        - | 10450 | ` *  Generate a backtrace` |
|        - | 10451 | ` * Parameters` |
|        - | 10452 | ` * None` |
|        - | 10453 | ` * Return` |
|        - | 10454 | ` *  A mini backtrace().` |
|        - | 10455 | ` * Note that this is a symisc extension.` |
|        - | 10456 | ` */` |
|        2 | 10457 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10458 |  |
|        3 | 10459 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10460 | `	SyBlob sDump;` |
|        3 | 10461 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 10462 | `	/* Generate the backtrace */` |
|        3 | 10463 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 10464 | `	/* Return the backtrace */` |
|        3 | 10465 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 10466 | `	/* All done,cleanup */` |
|        3 | 10467 | `	SyBlobRelease(&sDump);` |
|        1 | 10468 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10469 | `	SXUNUSED(apArg);` |
|        3 | 10470 | `	return PH7_OK;` |
|        1 | 10471 |  |
|        - | 10472 | `/*` |
|        - | 10473 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 10474 | ` * exception is triggered.` |
|        - | 10475 | ` */` |
|      480 | 10476 | `static sxi32 VmUncaughtException(` |
|        - | 10477 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 10478 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10479 | `	)` |
|        1 | 10480 |  |
|        - | 10481 | `	ph7_value *apArg[2],sArg;` |
|      481 | 10482 | `	int nArg = 1;` |
|        - | 10483 | `	sxi32 rc;` |
|      481 | 10484 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 10485 | `		/* Nesting limit reached */` |
|      ! 0 | 10486 | `		return SXRET_OK;` |
|        - | 10487 | `	}` |
|        - | 10488 | `	/* Call any exception handler if available */` |
|      481 | 10489 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 10490 | `	if( pThis ){` |
|        - | 10491 | `		/* Load the exception instance */` |
|      481 | 10492 | `		sArg.x.pOther = pThis;` |
|      481 | 10493 | `		pThis->iRef++;` |
|      481 | 10494 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 10495 | `	}else{` |
|      ! 0 | 10496 | `		nArg = 0;` |
|        - | 10497 | `	}` |
|      481 | 10498 | `	apArg[0] = &sArg;` |
|        - | 10499 | `	/* Call the exception handler if available */` |
|      481 | 10500 | `	pVm->nExceptDepth++;` |
|      481 | 10501 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 10502 | `	pVm->nExceptDepth--;` |
|      481 | 10503 | `	if( rc != SXRET_OK ){` |
|        - | 10504 | `		SyBlob sMsgBuf;` |
|      479 | 10505 | `		const char *zClass = "Exception";` |
|      479 | 10506 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 10507 | `		const char *zMsg;` |
|        - | 10508 | `		sxu32 nMsg;` |
|        - | 10509 | `		const char *zFuncName;` |
|        - | 10510 | `		int nFuncLen;` |
|      479 | 10511 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 10512 | `		if( pThis ){` |
|        - | 10513 | `			ph7_class_method *pGetMessage;` |
|        - | 10514 | `			ph7_value sMsg;` |
|        - | 10515 | `			const char *zTmp;` |
|        - | 10516 | `			int nTmp;` |
|      479 | 10517 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 10518 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 10519 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 10520 | `			if( pGetMessage ){` |
|      479 | 10521 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 10522 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 10523 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 10524 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 10525 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 10526 | `					}` |
|      239 | 10527 | `				}` |
|      479 | 10528 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 10529 | `			}` |
|      239 | 10530 | `		}` |
|      479 | 10531 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 10532 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 10533 | `		}` |
|      479 | 10534 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 10535 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 10536 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 10537 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 10538 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 10539 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 10540 | `		rc = SXERR_ABORT;` |
|      239 | 10541 | `	}` |
|      481 | 10542 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 10543 | `	return rc;` |
|      241 | 10544 |  |
|        - | 10545 | `/*` |
|        - | 10546 | ` * Throw a user exception.` |
|        - | 10547 | ` *` |
|        - | 10548 | ` * Exception dispatch follows this sequence:` |
|        - | 10549 | ` *` |
|        - | 10550 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 10551 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 10552 | ` *` |
|        - | 10553 | ` * 2. If NO catch matches:` |
|        - | 10554 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 10555 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 10556 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 10557 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 10558 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 10559 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 10560 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 10561 | ` *` |
|        - | 10562 | ` * 3. If a catch DOES match:` |
|        - | 10563 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 10564 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 10565 | ` *       inside the catch body from immediately propagating past our` |
|        - | 10566 | ` *       finally block.` |
|        - | 10567 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 10568 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 10569 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 10570 | ` *       in pPendingException (step 2c).` |
|        - | 10571 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 10572 | ` *    d. Run finally (if present).` |
|        - | 10573 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 10574 | ` *       that handlers are restored and finally has run.` |
|        - | 10575 | ` */` |
|      548 | 10576 | `static sxi32 VmThrowException(` |
|        - | 10577 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 10578 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 10579 | `	)` |
|        2 | 10580 |  |
|        - | 10581 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 10582 | `	ph7_exception **apException;` |
|        - | 10583 | `	ph7_exception *pException;` |
|        - | 10584 | `	/* Point to the stack of loaded exceptions */` |
|      550 | 10585 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      550 | 10586 | `	pException = 0;` |
|      550 | 10587 | `	pCatch = 0;` |
|      550 | 10588 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10589 | `		ph7_exception_block *aCatch;` |
|        - | 10590 | `		ph7_class *pClass;` |
|        - | 10591 | `		SyString *aNames;` |
|        - | 10592 | `		sxu32 nNames;` |
|        - | 10593 | `		int matched;` |
|        - | 10594 | `		sxu32 j,k;` |
|        - | 10595 | `		/* Locate the appropriate block to execute */` |
|       64 | 10596 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       64 | 10597 | `		(void)SySetPop(&pVm->aException);` |
|       64 | 10598 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       66 | 10599 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 10600 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|       64 | 10601 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|       64 | 10602 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|       64 | 10603 | `			matched = 0;` |
|       78 | 10604 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 10605 | `				/* Extract the target class */` |
|       76 | 10606 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,TRUE,0);` |
|       76 | 10607 | `				if( pClass == 0 ){` |
|        - | 10608 | `					/* No such class */` |
|      ! 0 | 10609 | `					continue;` |
|        - | 10610 | `				}` |
|       76 | 10611 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|       62 | 10612 | `					matched = 1;` |
|       62 | 10613 | `					break;` |
|        - | 10614 | `				}` |
|        8 | 10615 | `			}` |
|       64 | 10616 | `			if( matched ){` |
|        - | 10617 | `				/* Catch block found,break immediately */` |
|       62 | 10618 | `				pCatch = &aCatch[j];` |
|       62 | 10619 | `				break;` |
|        - | 10620 | `			}` |
|        2 | 10621 | `		}` |
|       31 | 10622 | `	}` |
|        - | 10623 | `	/* Execute the cached block if available */` |
|      550 | 10624 | `	if( pCatch == 0 ){` |
|        - | 10625 | `		sxi32 rc;` |
|        - | 10626 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      490 | 10627 | `		if( pException && pException->iHasFinally ){` |
|        3 | 10628 | `			pException->iFinallyDone = 1;` |
|        3 | 10629 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 10630 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 10631 | `				return SXERR_ABORT;` |
|        - | 10632 | `			}` |
|        1 | 10633 | `		}` |
|        - | 10634 | `		/* Check if there is an outer exception handler on the stack */` |
|      490 | 10635 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 10636 | `			/* Re-throw to the outer handler */` |
|        3 | 10637 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 10638 | `		}` |
|        - | 10639 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 10640 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 10641 | `		 * exception instead of reporting it uncaught.` |
|        - | 10642 | `		 */` |
|      488 | 10643 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 10644 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 10645 | `			 * by looking for a catch frame on the stack.` |
|        - | 10646 | `			 */` |
|      488 | 10647 | `			VmFrame *pF = pVm->pFrame;` |
|      488 | 10648 | `			int inCatch = 0;` |
|      974 | 10649 | `			while( pF ){` |
|      494 | 10650 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        7 | 10651 | `					inCatch = 1;` |
|        7 | 10652 | `					break;` |
|        - | 10653 | `				}` |
|      487 | 10654 | `				pF = pF->pParent;` |
|        1 | 10655 | `			}` |
|      488 | 10656 | `			if( inCatch ){` |
|        - | 10657 | `				/* Defer — will be re-thrown after finally runs */` |
|        7 | 10658 | `				pThis->iRef++;` |
|        7 | 10659 | `				pVm->pPendingException = pThis;` |
|        7 | 10660 | `				return SXRET_OK;` |
|        - | 10661 | `			}` |
|      240 | 10662 | `		}` |
|        - | 10663 | `		/* Truly uncaught */` |
|      481 | 10664 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 10665 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 10666 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 10667 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 10668 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 10669 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 10670 | `			}` |
|      ! 0 | 10671 | `		}` |
|      481 | 10672 | `		return rc;` |
|      ! 0 | 10673 | `	}else{` |
|       62 | 10674 | `		VmFrame *pFrame = pVm->pFrame;` |
|       62 | 10675 | `		ph7_exception **apSaved = 0;` |
|        - | 10676 | `		sxu32 nSavedCount;` |
|        - | 10677 | `		sxi32 rc;` |
|       62 | 10678 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       62 | 10679 | `		if( pException->pFrame == pFrame ){` |
|       48 | 10680 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       23 | 10681 | `		}` |
|        - | 10682 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 10683 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 10684 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 10685 | `		 */` |
|       62 | 10686 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       62 | 10687 | `		if( nSavedCount > 0 ){` |
|       13 | 10688 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 | 10689 | `				nSavedCount * sizeof(ph7_exception *));` |
|        9 | 10690 | `			if( apSaved ){` |
|       13 | 10691 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        4 | 10692 | `					nSavedCount * sizeof(ph7_exception *));` |
|        9 | 10693 | `				SySetReset(&pVm->aException);` |
|        4 | 10694 | `			}` |
|        4 | 10695 | `		}` |
|        - | 10696 | `		/* Create a private frame first */` |
|       62 | 10697 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       62 | 10698 | `		if( rc == SXRET_OK ){` |
|       62 | 10699 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       62 | 10700 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       62 | 10701 | `			if( pObj ){` |
|       62 | 10702 | `				pThis->iRef++;` |
|       62 | 10703 | `				pObj->x.pOther = pThis;` |
|       62 | 10704 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       30 | 10705 | `			}` |
|        - | 10706 | `			/* Execute the catch block */` |
|       62 | 10707 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 10708 | `			/* Leave the frame */` |
|       62 | 10709 | `			VmLeaveFrame(&(*pVm));` |
|       30 | 10710 | `		}` |
|        - | 10711 | `		/* Restore the outer exception handlers */` |
|       62 | 10712 | `		if( apSaved ){` |
|        - | 10713 | `			sxu32 k;` |
|        - | 10714 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 10715 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 10716 | `			 * Restore the original outer entries.` |
|        - | 10717 | `			 */` |
|        9 | 10718 | `			SySetReset(&pVm->aException);` |
|       17 | 10719 | `			for(k = 0; k < nSavedCount; k++){` |
|        9 | 10720 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 10721 | `			}` |
|        9 | 10722 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        4 | 10723 | `		}` |
|        - | 10724 | `		/* Execute the finally block after catch */` |
|       62 | 10725 | `		if( pException->iHasFinally ){` |
|       16 | 10726 | `			pException->iFinallyDone = 1;` |
|        - | 10727 | `			{` |
|       16 | 10728 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 10729 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 10730 | `					return SXERR_ABORT;` |
|        - | 10731 | `				}` |
|        - | 10732 | `			}` |
|        7 | 10733 | `		}` |
|       62 | 10734 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 10735 | `			return SXERR_ABORT;` |
|        - | 10736 | `		}` |
|        - | 10737 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 10738 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 10739 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 10740 | `		 */` |
|       62 | 10741 | `		if( pVm->pPendingException ){` |
|        7 | 10742 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        7 | 10743 | `			pVm->pPendingException = 0;` |
|        7 | 10744 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 10745 | `		}` |
|        - | 10746 | `	}` |
|        - | 10747 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 10748 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 10749 | `	 */` |
|       56 | 10750 | `	return SXRET_OK;` |
|      276 | 10751 |  |
|        - | 10752 | `/*` |
|        - | 10753 | ` * Section:` |
|        - | 10754 | ` *  Version,Credits and Copyright related functions.` |
|        - | 10755 | ` * Status:` |
|        - | 10756 | ` *    Stable.` |
|        - | 10757 | ` */` |
|        - | 10758 | `/*` |
|        - | 10759 | ` * string ph7version(void)` |
|        - | 10760 | ` *  Returns the running version of the PH7 version.` |
|        - | 10761 | ` * Parameters` |
|        - | 10762 | ` *  None` |
|        - | 10763 | ` * Return` |
|        - | 10764 | ` * Current PH7 version.` |
|        - | 10765 | ` */` |
|        2 | 10766 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10767 |  |
|        1 | 10768 | `	SXUNUSED(nArg);` |
|        1 | 10769 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 10770 | `	/* Current engine version */` |
|        3 | 10771 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 10772 | `	return PH7_OK;` |
|        1 | 10773 |  |
|        - | 10774 | `/*` |
|        - | 10775 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 10776 | ` */` |
|        - | 10777 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 10778 | ` "<html><head>"\` |
|        - | 10779 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 10780 | ` "<style type=\"text/css\">"\` |
|        - | 10781 | ` "div {"\` |
|        - | 10782 | `     "border: 1px solid #cccccc;"\` |
|        - | 10783 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 10784 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 10785 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 10786 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 10787 | `     "-webkit-border-radius: 10px;"\` |
|        - | 10788 | `     "-o-border-radius: 10px;"\` |
|        - | 10789 | `     "border-radius: 10px;"\` |
|        - | 10790 | `     "padding-left: 2em;"\` |
|        - | 10791 | `     "background-color: white;"\` |
|        - | 10792 | `     "margin-left: auto;"\` |
|        - | 10793 | `     "font-family: verdana;"\` |
|        - | 10794 | `     "padding-right: 2em;"\` |
|        - | 10795 | `     "margin-right: auto;"\` |
|        - | 10796 | `     "}"\` |
|        - | 10797 | `     "body {"\` |
|        - | 10798 | `     "padding: 0.2em;"\` |
|        - | 10799 | `     "font-style: normal;"\` |
|        - | 10800 | `     "font-size: medium;"\` |
|        - | 10801 | `     "background-color: #f2f2f2;"\` |
|        - | 10802 | `     "}"\` |
|        - | 10803 | `     "hr {"\` |
|        - | 10804 | `     "border-style: solid none none;"\` |
|        - | 10805 | `     "border-width: 1px medium medium;"\` |
|        - | 10806 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 10807 | `     "height: 1px;"\` |
|        - | 10808 | `     "}"\` |
|        - | 10809 | `     "a {"\` |
|        - | 10810 | `     "color: #3366cc;"\` |
|        - | 10811 | `     "text-decoration: none;"\` |
|        - | 10812 | `     "}"\` |
|        - | 10813 | `     "a:hover {"\` |
|        - | 10814 | `     "color: #999999;"\` |
|        - | 10815 | `     "}"\` |
|        - | 10816 | `     "a:active {"\` |
|        - | 10817 | `     "color: #663399;"\` |
|        - | 10818 | `     "}"\` |
|        - | 10819 | `     "h1 {"\` |
|        - | 10820 | `     "margin: 0;"\` |
|        - | 10821 | `     "padding: 0;"\` |
|        - | 10822 | `     "font-family: Verdana;"\` |
|        - | 10823 | `     "font-weight: bold;"\` |
|        - | 10824 | `     "font-style: normal;"\` |
|        - | 10825 | `     "font-size: medium;"\` |
|        - | 10826 | `     "text-transform: capitalize;"\` |
|        - | 10827 | `     "color: #0a328c;"\` |
|        - | 10828 | `     "}"\` |
|        - | 10829 | `     "p {"\` |
|        - | 10830 | `     "margin: 0 auto;"\` |
|        - | 10831 | `     "font-size: medium;"\` |
|        - | 10832 | `     "font-style: normal;"\` |
|        - | 10833 | `     "font-family: verdana;"\` |
|        - | 10834 | `     "}"\` |
|        - | 10835 | `"</style></head><body>"\` |
|        - | 10836 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 10837 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 10838 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 10839 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 10840 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 10841 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 10842 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 10843 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 10844 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 10845 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 10846 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 10847 |  |
|        - | 10848 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10849 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 10850 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 10851 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 10852 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10853 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 10854 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10855 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 10856 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 10857 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 10858 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 10859 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 10860 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 10861 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 10862 |  |
|        - | 10863 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 10864 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 10865 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 10866 | `"&nbsp;*<br>"\` |
|        - | 10867 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 10868 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 10869 | `"&nbsp;* are met:<br>"\` |
|        - | 10870 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 10871 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 10872 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 10873 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 10874 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 10875 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 10876 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 10877 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 10878 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 10879 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 10880 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 10881 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 10882 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 10883 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 10884 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 10885 | `"&nbsp;*<br>"\` |
|        - | 10886 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 10887 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 10888 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 10889 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 10890 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 10891 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 10892 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 10893 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 10894 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 10895 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 10896 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 10897 | `"&nbsp;*/<br>"\` |
|        - | 10898 | `"</span></small></small></p>"\` |
|        - | 10899 | `"</div></body></html>"` |
|        - | 10900 | `/*` |
|        - | 10901 | ` * bool ph7credits(void)` |
|        - | 10902 | ` * bool ph7info(void)` |
|        - | 10903 | ` * bool ph7copyright(void)` |
|        - | 10904 | ` *  Prints out the credits for PH7 engine` |
|        - | 10905 | ` * Parameters` |
|        - | 10906 | ` *  None` |
|        - | 10907 | ` * Return` |
|        - | 10908 | ` *  Always TRUE` |
|        - | 10909 | ` */` |
|        2 | 10910 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10911 |  |
|        3 | 10912 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 10913 | `	/* Expand the HTML page above*/` |
|        3 | 10914 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 10915 | `	ph7_context_output_format(` |
|        1 | 10916 | `		pCtx,` |
|        - | 10917 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 10918 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 10919 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 10920 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 10921 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 10922 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 10923 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 10924 | `#ifdef __WINNT__` |
|        - | 10925 | `		"Windows NT"` |
|        - | 10926 | `#elif defined(__UNIXES__)` |
|        - | 10927 | `		"UNIX-Like"` |
|        - | 10928 | `#else` |
|        - | 10929 | `		"Other OS"` |
|        - | 10930 | `#endif` |
|        - | 10931 | `		);` |
|        3 | 10932 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 10933 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10934 | `	SXUNUSED(apArg);` |
|        - | 10935 | `	/* Return TRUE */` |
|        - | 10936 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 10937 | `	return PH7_OK;` |
|        1 | 10938 |  |
|        - | 10939 | `/*` |
|        - | 10940 | ` * Section:` |
|        - | 10941 | ` *    URL related routines.` |
|        - | 10942 | ` * Status:` |
|        - | 10943 | ` *    Stable.` |
|        - | 10944 | ` */` |
|        - | 10945 | `/*` |
|        - | 10946 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 10947 | ` *  Parse a URL and return its fields.` |
|        - | 10948 | ` * Parameters` |
|        - | 10949 | ` *  $url` |
|        - | 10950 | ` *   The URL to parse.` |
|        - | 10951 | ` * $component` |
|        - | 10952 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 10953 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 10954 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 10955 | ` *  in which case the return value will be an integer).` |
|        - | 10956 | ` * Return` |
|        - | 10957 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 10958 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 10959 | ` *  this array are:` |
|        - | 10960 | ` *   scheme - e.g. http` |
|        - | 10961 | ` *   host` |
|        - | 10962 | ` *   port` |
|        - | 10963 | ` *   user` |
|        - | 10964 | ` *   pass` |
|        - | 10965 | ` *   path` |
|        - | 10966 | ` *   query - after the question mark ?` |
|        - | 10967 | ` *   fragment - after the hashmark #` |
|        - | 10968 | ` * Note:` |
|        - | 10969 | ` *  FALSE is returned on failure.` |
|        - | 10970 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 10971 | ` *  with the standard PHP engine.` |
|        - | 10972 | ` */` |
|       28 | 10973 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10974 |  |
|        - | 10975 | `	const char *zStr; /* Input string */` |
|        - | 10976 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 10977 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 10978 | `	int nLen;` |
|        - | 10979 | `	sxi32 rc;` |
|       29 | 10980 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10981 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 10982 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10983 | `		return PH7_OK;` |
|        - | 10984 | `	}` |
|        - | 10985 | `	/* Extract the given URI */` |
|       29 | 10986 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 10987 | `	if( nLen < 1 ){` |
|        - | 10988 | `		/* Nothing to process,return FALSE */` |
|        3 | 10989 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10990 | `		return PH7_OK;` |
|        - | 10991 | `	}` |
|        - | 10992 | `	/* Get a parse */` |
|       27 | 10993 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 10994 | `	if( rc != SXRET_OK ){` |
|        - | 10995 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 10996 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10997 | `		return PH7_OK;` |
|        - | 10998 | `	}` |
|       27 | 10999 | `	if( nArg > 1 ){` |
|      ! 0 | 11000 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 11001 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 11002 | `		switch(nComponent){` |
|      ! 0 | 11003 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 11004 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 11005 | `			if( pComp->nByte < 1 ){` |
|        - | 11006 | `				/* No available value,return NULL */` |
|      ! 0 | 11007 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11008 | `			}else{` |
|      ! 0 | 11009 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11010 | `			}` |
|      ! 0 | 11011 | `			break;` |
|      ! 0 | 11012 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 11013 | `			pComp = &sURI.sHost;` |
|      ! 0 | 11014 | `			if( pComp->nByte < 1 ){` |
|        - | 11015 | `				/* No available value,return NULL */` |
|      ! 0 | 11016 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11017 | `			}else{` |
|      ! 0 | 11018 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11019 | `			}` |
|      ! 0 | 11020 | `			break;` |
|      ! 0 | 11021 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 11022 | `			pComp = &sURI.sPort;` |
|      ! 0 | 11023 | `			if( pComp->nByte < 1 ){` |
|        - | 11024 | `				/* No available value,return NULL */` |
|      ! 0 | 11025 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11026 | `			}else{` |
|      ! 0 | 11027 | `				int iPort = 0;` |
|        - | 11028 | `				/* Cast the value to integer */` |
|      ! 0 | 11029 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 11030 | `				ph7_result_int(pCtx,iPort);` |
|        - | 11031 | `			}` |
|      ! 0 | 11032 | `			break;` |
|      ! 0 | 11033 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 11034 | `			pComp = &sURI.sUser;` |
|      ! 0 | 11035 | `			if( pComp->nByte < 1 ){` |
|        - | 11036 | `				/* No available value,return NULL */` |
|      ! 0 | 11037 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11038 | `			}else{` |
|      ! 0 | 11039 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11040 | `			}` |
|      ! 0 | 11041 | `			break;` |
|      ! 0 | 11042 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 11043 | `			pComp = &sURI.sPass;` |
|      ! 0 | 11044 | `			if( pComp->nByte < 1 ){` |
|        - | 11045 | `				/* No available value,return NULL */` |
|      ! 0 | 11046 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11047 | `			}else{` |
|      ! 0 | 11048 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11049 | `			}` |
|      ! 0 | 11050 | `			break;` |
|      ! 0 | 11051 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 11052 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 11053 | `			if( pComp->nByte < 1 ){` |
|        - | 11054 | `				/* No available value,return NULL */` |
|      ! 0 | 11055 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11056 | `			}else{` |
|      ! 0 | 11057 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11058 | `			}` |
|      ! 0 | 11059 | `			break;` |
|      ! 0 | 11060 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 11061 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 11062 | `			if( pComp->nByte < 1 ){` |
|        - | 11063 | `				/* No available value,return NULL */` |
|      ! 0 | 11064 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11065 | `			}else{` |
|      ! 0 | 11066 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11067 | `			}` |
|      ! 0 | 11068 | `			break;` |
|      ! 0 | 11069 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 11070 | `			pComp = &sURI.sPath;` |
|      ! 0 | 11071 | `			if( pComp->nByte < 1 ){` |
|        - | 11072 | `				/* No available value,return NULL */` |
|      ! 0 | 11073 | `				ph7_result_null(pCtx);` |
|      ! 0 | 11074 | `			}else{` |
|      ! 0 | 11075 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 11076 | `			}` |
|      ! 0 | 11077 | `			break;` |
|      ! 0 | 11078 | `		default:` |
|        - | 11079 | `			/* No such entry,return NULL */` |
|      ! 0 | 11080 | `			ph7_result_null(pCtx);` |
|      ! 0 | 11081 | `			break;` |
|        - | 11082 | `		}` |
|      ! 0 | 11083 | `	}else{` |
|        - | 11084 | `		ph7_value *pArray,*pValue;` |
|        - | 11085 | `		/* Return an associative array */` |
|       27 | 11086 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 11087 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 11088 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11089 | `			/* Out of memory */` |
|      ! 0 | 11090 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11091 | `			/* Return false */` |
|      ! 0 | 11092 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 11093 | `			return PH7_OK;` |
|        - | 11094 | `		}` |
|        - | 11095 | `		/* Fill the array */` |
|       27 | 11096 | `		pComp = &sURI.sScheme;` |
|       27 | 11097 | `		if( pComp->nByte > 0 ){` |
|       19 | 11098 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 11099 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 11100 | `		}` |
|        - | 11101 | `		/* Reset the string cursor */` |
|       27 | 11102 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11103 | `		pComp = &sURI.sHost;` |
|       27 | 11104 | `		if( pComp->nByte > 0 ){` |
|       25 | 11105 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 11106 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 11107 | `		}` |
|        - | 11108 | `		/* Reset the string cursor */` |
|       27 | 11109 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11110 | `		pComp = &sURI.sPort;` |
|       27 | 11111 | `		if( pComp->nByte > 0 ){` |
|       11 | 11112 | `			int iPort = 0;/* cc warning */` |
|        - | 11113 | `			/* Convert to integer */` |
|       11 | 11114 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 11115 | `			ph7_value_int(pValue,iPort);` |
|       11 | 11116 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 11117 | `		}` |
|        - | 11118 | `		/* Reset the string cursor */` |
|       27 | 11119 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11120 | `		pComp = &sURI.sUser;` |
|       27 | 11121 | `		if( pComp->nByte > 0 ){` |
|        7 | 11122 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11123 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 11124 | `		}` |
|        - | 11125 | `		/* Reset the string cursor */` |
|       27 | 11126 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11127 | `		pComp = &sURI.sPass;` |
|       27 | 11128 | `		if( pComp->nByte > 0 ){` |
|        7 | 11129 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 11130 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 11131 | `		}` |
|        - | 11132 | `		/* Reset the string cursor */` |
|       27 | 11133 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11134 | `		pComp = &sURI.sPath;` |
|       27 | 11135 | `		if( pComp->nByte > 0 ){` |
|       17 | 11136 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 11137 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 11138 | `		}` |
|        - | 11139 | `		/* Reset the string cursor */` |
|       27 | 11140 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11141 | `		pComp = &sURI.sQuery;` |
|       27 | 11142 | `		if( pComp->nByte > 0 ){` |
|        5 | 11143 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11144 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 11145 | `		}` |
|        - | 11146 | `		/* Reset the string cursor */` |
|       27 | 11147 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 11148 | `		pComp = &sURI.sFragment;` |
|       27 | 11149 | `		if( pComp->nByte > 0 ){` |
|        5 | 11150 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 11151 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 11152 | `		}` |
|        - | 11153 | `		/* Return the created array */` |
|       27 | 11154 | `		ph7_result_value(pCtx,pArray);` |
|        - | 11155 | `		/* NOTE:` |
|        - | 11156 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 11157 | `		 * automatically as soon we return from this function.` |
|        - | 11158 | `		 */` |
|        - | 11159 | `	}` |
|        - | 11160 | `	/* All done */` |
|       27 | 11161 | `	return PH7_OK;` |
|       15 | 11162 |  |
|        - | 11163 | `/*` |
|        - | 11164 | ` * Section:` |
|        - | 11165 | ` *   Array related routines.` |
|        - | 11166 | ` * Status:` |
|        - | 11167 | ` *    Stable.` |
|        - | 11168 | ` * Note 2012-5-21 01:04:15:` |
|        - | 11169 | ` *  Array related functions that need access to the underlying` |
|        - | 11170 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 11171 | ` */` |
|        - | 11172 | `/*` |
|        - | 11173 | ` * The [compact()] function store it's state information in an instance` |
|        - | 11174 | ` * of the following structure.` |
|        - | 11175 | ` */` |
|        - | 11176 | `struct compact_data` |
|        - | 11177 |  |
|        - | 11178 | `	ph7_value *pArray;  /* Target array */` |
|        - | 11179 | `	int nRecCount;      /* Recursion count */` |
|        - | 11180 | `};` |
|        - | 11181 | `/*` |
|        - | 11182 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 11183 | ` */` |
|      ! 0 | 11184 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 11185 |  |
|      ! 0 | 11186 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 11187 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 11188 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11189 | `	/* Act according to the hashmap value */` |
|      ! 0 | 11190 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 11191 | `		SyString sVar;` |
|      ! 0 | 11192 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 11193 | `		if( sVar.nByte > 0 ){` |
|        - | 11194 | `			/* Query the current frame */` |
|      ! 0 | 11195 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 11196 | `			/* ^` |
|        - | 11197 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 11198 | `			 */` |
|      ! 0 | 11199 | `			if( pKey ){` |
|        - | 11200 | `				/* Perform the insertion */` |
|      ! 0 | 11201 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 11202 | `			}` |
|      ! 0 | 11203 | `		}` |
|      ! 0 | 11204 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 11205 | `		int rc;` |
|        - | 11206 | `		/* Recursively traverse this array */` |
|      ! 0 | 11207 | `		pData->nRecCount++;` |
|      ! 0 | 11208 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 11209 | `		pData->nRecCount--;` |
|      ! 0 | 11210 | `		return rc;` |
|        - | 11211 | `	}` |
|      ! 0 | 11212 | `	return SXRET_OK;` |
|      ! 0 | 11213 |  |
|        - | 11214 | `/*` |
|        - | 11215 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 11216 | ` *  Create array containing variables and their values.` |
|        - | 11217 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 11218 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 11219 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 11220 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 11221 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 11222 | ` * Parameters` |
|        - | 11223 | ` *  $varname` |
|        - | 11224 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 11225 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 11226 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 11227 | ` *   it recursively.` |
|        - | 11228 | ` * Return` |
|        - | 11229 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 11230 | ` */` |
|        2 | 11231 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11232 |  |
|        - | 11233 | `	ph7_value *pArray,*pObj;` |
|        3 | 11234 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11235 | `	const char *zName;` |
|        - | 11236 | `	SyString sVar;` |
|        - | 11237 | `	int i,nLen;` |
|        3 | 11238 | `	if( nArg < 1 ){` |
|        - | 11239 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 11240 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11241 | `		return PH7_OK;` |
|        - | 11242 | `	}` |
|        - | 11243 | `	/* Create the array */` |
|        3 | 11244 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11245 | `	if( pArray == 0 ){` |
|        - | 11246 | `		/* Out of memory */` |
|      ! 0 | 11247 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 11248 | `		/* Return NULL */` |
|      ! 0 | 11249 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11250 | `		return PH7_OK;` |
|        - | 11251 | `	}` |
|        - | 11252 | `	/* Perform the requested operation */` |
|        7 | 11253 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 11254 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 11255 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 11256 | `				struct compact_data sData;` |
|      ! 0 | 11257 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 11258 | `				/* Recursively walk the array */` |
|      ! 0 | 11259 | `				sData.nRecCount = 0;` |
|      ! 0 | 11260 | `				sData.pArray = pArray;` |
|      ! 0 | 11261 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 11262 | `			}` |
|      ! 0 | 11263 | `		}else{` |
|        - | 11264 | `			/* Extract variable name */` |
|        5 | 11265 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 11266 | `			if( nLen > 0 ){` |
|        5 | 11267 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 11268 | `				/* Check if the variable is available in the current frame */` |
|        5 | 11269 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 11270 | `				if( pObj ){` |
|        5 | 11271 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 11272 | `				}` |
|        2 | 11273 | `			}` |
|        - | 11274 | `		}` |
|        3 | 11275 | `	}` |
|        - | 11276 | `	/* Return the array */` |
|        3 | 11277 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11278 | `	return PH7_OK;` |
|        2 | 11279 |  |
|        - | 11280 | `/*` |
|        - | 11281 | ` * The [extract()] function store it's state information in an instance` |
|        - | 11282 | ` * of the following structure.` |
|        - | 11283 | ` */` |
|        - | 11284 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 11285 | `struct extract_aux_data` |
|        - | 11286 |  |
|        - | 11287 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 11288 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 11289 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 11290 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 11291 | `	int iFlags;           /* Control flags */` |
|        - | 11292 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 11293 | `};` |
|        - | 11294 | `/* Forward declaration */` |
|        - | 11295 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 11296 | `/*` |
|        - | 11297 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 11298 | ` *   Import variables into the current symbol table from an array.` |
|        - | 11299 | ` * Parameters` |
|        - | 11300 | ` * $var_array` |
|        - | 11301 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 11302 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 11303 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 11304 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 11305 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 11306 | ` * $extract_type` |
|        - | 11307 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 11308 | ` *  It can be one of the following values:` |
|        - | 11309 | ` *   EXTR_OVERWRITE` |
|        - | 11310 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 11311 | ` *   EXTR_SKIP` |
|        - | 11312 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 11313 | ` *   EXTR_PREFIX_SAME` |
|        - | 11314 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 11315 | ` *   EXTR_PREFIX_ALL` |
|        - | 11316 | ` *       Prefix all variable names with prefix.` |
|        - | 11317 | ` *   EXTR_PREFIX_INVALID` |
|        - | 11318 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 11319 | ` *   EXTR_IF_EXISTS` |
|        - | 11320 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 11321 | ` *       otherwise do nothing.` |
|        - | 11322 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 11323 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 11324 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 11325 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 11326 | ` *      the current symbol table.` |
|        - | 11327 | ` * $prefix` |
|        - | 11328 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 11329 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 11330 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 11331 | ` *  underscore character.` |
|        - | 11332 | ` * Return` |
|        - | 11333 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 11334 | ` */` |
|        4 | 11335 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11336 |  |
|        - | 11337 | `	extract_aux_data sAux;` |
|        - | 11338 | `	ph7_hashmap *pMap;` |
|        5 | 11339 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 11340 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 11341 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11342 | `		return PH7_OK;` |
|        - | 11343 | `	}` |
|        - | 11344 | `	/* Point to the target hashmap */` |
|        5 | 11345 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 11346 | `	if( pMap->nEntry < 1 ){` |
|        - | 11347 | `		/* Empty map,return  0 */` |
|      ! 0 | 11348 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 11349 | `		return PH7_OK;` |
|        - | 11350 | `	}` |
|        - | 11351 | `	/* Prepare the aux data */` |
|        5 | 11352 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 11353 | `	if( nArg > 1 ){` |
|        3 | 11354 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 11355 | `		if( nArg > 2 ){` |
|      ! 0 | 11356 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 11357 | `		}` |
|        1 | 11358 | `	}` |
|        5 | 11359 | `	sAux.pVm = pCtx->pVm;` |
|        - | 11360 | `	/* Invoke the worker callback */` |
|        5 | 11361 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 11362 | `	/* Number of variables successfully imported */` |
|        5 | 11363 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 11364 | `	return PH7_OK;` |
|        3 | 11365 |  |
|        - | 11366 | `/*` |
|        - | 11367 | ` * Worker callback for the [extract()] function defined` |
|        - | 11368 | ` * below.` |
|        - | 11369 | ` */` |
|        8 | 11370 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11371 |  |
|        9 | 11372 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 11373 | `	int iFlags = pAux->iFlags;` |
|        9 | 11374 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11375 | `	ph7_value *pObj;` |
|        - | 11376 | `	SyString sVar;` |
|        9 | 11377 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 11378 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 11379 | `	}` |
|        - | 11380 | `	/* Perform a string cast */` |
|        9 | 11381 | `	PH7_MemObjToString(pKey);` |
|        9 | 11382 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11383 | `		/* Unavailable variable name */` |
|      ! 0 | 11384 | `		return SXRET_OK;` |
|        - | 11385 | `	}` |
|        9 | 11386 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 11387 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 11388 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11389 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11390 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11391 | `			);` |
|      ! 0 | 11392 | `	}else{` |
|       13 | 11393 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 11394 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11395 | `	}` |
|        9 | 11396 | `	sVar.zString = pAux->zWorker;` |
|        - | 11397 | `	/* Try to extract the variable */` |
|        9 | 11398 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 11399 | `	if( pObj ){` |
|        - | 11400 | `		/* Collision */` |
|        5 | 11401 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 11402 | `			return SXRET_OK;` |
|        - | 11403 | `		}` |
|        5 | 11404 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 11405 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 11406 | `				/* Already prefixed */` |
|      ! 0 | 11407 | `				return SXRET_OK;` |
|        - | 11408 | `			}` |
|      ! 0 | 11409 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 11410 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 11411 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11412 | `				);` |
|      ! 0 | 11413 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 11414 | `		}` |
|        3 | 11415 | `	}else{` |
|        - | 11416 | `		/* Create the variable */` |
|        5 | 11417 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 11418 | `	}` |
|        9 | 11419 | `	if( pObj ){` |
|        - | 11420 | `		/* Overwrite the old value */` |
|        9 | 11421 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 11422 | `		/* Increment counter */` |
|        9 | 11423 | `		pAux->iCount++;` |
|        4 | 11424 | `	}` |
|        9 | 11425 | `	return SXRET_OK;` |
|        5 | 11426 |  |
|        - | 11427 | `/*` |
|        - | 11428 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 11429 | ` * defined below.` |
|        - | 11430 | ` */` |
|        2 | 11431 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 11432 |  |
|        3 | 11433 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 11434 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 11435 | `	ph7_value *pObj;` |
|        - | 11436 | `	SyString sVar;` |
|        - | 11437 | `	/* Perform a string cast */` |
|        3 | 11438 | `	PH7_MemObjToString(pKey);` |
|        3 | 11439 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 11440 | `		/* Unavailable variable name */` |
|      ! 0 | 11441 | `		return SXRET_OK;` |
|        - | 11442 | `	}` |
|        3 | 11443 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 11444 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 11445 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 11446 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 11447 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 11448 | `			);` |
|        2 | 11449 | `	}else{` |
|      ! 0 | 11450 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 11451 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 11452 | `	}` |
|        3 | 11453 | `	sVar.zString = pAux->zWorker;` |
|        - | 11454 | `	/* Extract the variable */` |
|        3 | 11455 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 11456 | `	if( pObj ){` |
|        3 | 11457 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 11458 | `	}` |
|        3 | 11459 | `	return SXRET_OK;` |
|        2 | 11460 |  |
|        - | 11461 | `/*` |
|        - | 11462 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 11463 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 11464 | ` * Parameters` |
|        - | 11465 | ` * $types` |
|        - | 11466 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 11467 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 11468 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 11469 | ` *  POST includes the POST uploaded file information.` |
|        - | 11470 | ` *  Note:` |
|        - | 11471 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 11472 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 11473 | ` * $prefix` |
|        - | 11474 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 11475 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 11476 | ` *  variable named $pref_userid.` |
|        - | 11477 | ` * Return` |
|        - | 11478 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11479 | ` */` |
|        2 | 11480 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11481 |  |
|        - | 11482 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 11483 | `	extract_aux_data sAux;` |
|        - | 11484 | `	int nLen,nPrefixLen;` |
|        - | 11485 | `	ph7_value *pSuper;` |
|        - | 11486 | `	ph7_vm *pVm;` |
|        - | 11487 | `	/* By default import only $_GET variables  */` |
|        3 | 11488 | `	zImport = "G";` |
|        3 | 11489 | `	nLen = (int)sizeof(char);` |
|        3 | 11490 | `	zPrefix = 0;` |
|        3 | 11491 | `	nPrefixLen = 0;` |
|        3 | 11492 | `	if( nArg > 0 ){` |
|        3 | 11493 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 11494 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 11495 | `		}` |
|        3 | 11496 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11497 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 11498 | `		}` |
|        1 | 11499 | `	}` |
|        - | 11500 | `	/* Point to the underlying VM */` |
|        3 | 11501 | `	pVm = pCtx->pVm;` |
|        - | 11502 | `	/* Initialize the aux data */` |
|        3 | 11503 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 11504 | `	sAux.zPrefix = zPrefix;` |
|        3 | 11505 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 11506 | `	sAux.pVm = pVm;` |
|        - | 11507 | `	/* Extract */` |
|        3 | 11508 | `	zEnd = &zImport[nLen];` |
|        5 | 11509 | `	while( zImport < zEnd ){` |
|        3 | 11510 | `		int c = zImport[0];` |
|        3 | 11511 | `		pSuper = 0;` |
|        3 | 11512 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 11513 | `			/* Import $_GET variables */` |
|        3 | 11514 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 11515 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 11516 | `			/* Import $_POST variables */` |
|      ! 0 | 11517 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 11518 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 11519 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 11520 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 11521 | `		}` |
|        3 | 11522 | `		if( pSuper ){` |
|        - | 11523 | `			/* Iterate throw array entries */` |
|        3 | 11524 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 11525 | `		}` |
|        - | 11526 | `		/* Advance the cursor */` |
|        3 | 11527 | `		zImport++;` |
|        1 | 11528 | `	}` |
|        - | 11529 | `	/* All done,return TRUE*/` |
|        3 | 11530 | `	ph7_result_bool(pCtx,0);` |
|        3 | 11531 | `	return PH7_OK;` |
|        1 | 11532 |  |
|        - | 11533 | `/*` |
|        - | 11534 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 11535 | ` * Refer to the eval() language construct implementation for more` |
|        - | 11536 | ` * information.` |
|        - | 11537 | ` */` |
|    10822 | 11538 | `static sxi32 VmEvalChunk(` |
|        - | 11539 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 11540 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 11541 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 11542 | `	int iFlags,         /* Compile flag */` |
|        - | 11543 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 11544 | `	)` |
|        2 | 11545 |  |
|        - | 11546 | `	SySet *pByteCode,aByteCode;` |
|        - | 11547 | `	SyBlob sSavedNs;` |
|    10824 | 11548 | `	ProcConsumer xErr = 0;` |
|    10824 | 11549 | `	void *pErrData = 0;` |
|        - | 11550 | `	/* Initialize bytecode container */` |
|    10824 | 11551 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10824 | 11552 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 11553 | `	/* Reset the code generator */` |
|    10824 | 11554 | `	if( bTrueReturn ){` |
|        - | 11555 | `		/* Included file,log compile-time errors */` |
|     8182 | 11556 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8182 | 11557 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4090 | 11558 | `	}` |
|    10824 | 11559 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 11560 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 11561 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 11562 | `	 * the caller's namespace is restored. */` |
|    10824 | 11563 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10824 | 11564 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10824 | 11565 | `	if( bTrueReturn ){` |
|        - | 11566 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8182 | 11567 | `		SyBlobReset(&pVm->sNamespace);` |
|     4090 | 11568 | `	}` |
|        - | 11569 | `	/* Swap bytecode container */` |
|    10824 | 11570 | `	pByteCode = pVm->pByteContainer;` |
|    10824 | 11571 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 11572 | `	/* Compile the chunk */` |
|    10824 | 11573 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    16235 | 11574 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 11575 | `		/* Compilation error,return false */` |
|        3 | 11576 | `		if( pCtx ){` |
|        3 | 11577 | `			ph7_result_bool(pCtx,0);` |
|        1 | 11578 | `		}` |
|        2 | 11579 | `	}else{` |
|        - | 11580 | `		/* Mount any newly defined classes */` |
|        - | 11581 | `		SyHashEntry *pEntry;` |
|        - | 11582 | `		ph7_class *pClass;` |
|        - | 11583 | `		ph7_value sResult; /* Return value */` |
|        - | 11584 | `		sxi32 rc;` |
|    10822 | 11585 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   399062 | 11586 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   382832 | 11587 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 11588 | `			/* Only mount classes that haven't been mounted yet */` |
|   382832 | 11589 | `			if( !pClass->bMounted ){` |
|    83276 | 11590 | `				rc = VmMountUserClass(pVm,pClass);` |
|    83276 | 11591 | `				if( rc != SXRET_OK ){` |
|        - | 11592 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 11593 | `					if( pCtx ){` |
|      ! 0 | 11594 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 11595 | `					}` |
|      ! 0 | 11596 | `					goto Cleanup;` |
|        - | 11597 | `				}` |
|    41637 | 11598 | `			}` |
|        2 | 11599 | `		}` |
|    10822 | 11600 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 11601 | `			/* Out of memory */` |
|      ! 0 | 11602 | `			if( pCtx ){` |
|      ! 0 | 11603 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 11604 | `			}` |
|      ! 0 | 11605 | `			goto Cleanup;` |
|        - | 11606 | `		}` |
|    10822 | 11607 | `		if( bTrueReturn ){` |
|        - | 11608 | `			/* Assume a boolean true return value */` |
|     8182 | 11609 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4092 | 11610 | `		}else{` |
|        - | 11611 | `			/* Assume a null return value */` |
|     2642 | 11612 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 11613 | `		}` |
|        - | 11614 | `		/* Execute the compiled chunk */` |
|    10822 | 11615 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10822 | 11616 | `		if( pCtx ){` |
|        - | 11617 | `			/* Set the execution result */` |
|     8200 | 11618 | `			ph7_result_value(pCtx,&sResult);` |
|     4099 | 11619 | `		}` |
|    10822 | 11620 | `		PH7_MemObjRelease(&sResult);` |
|        - | 11621 | `	}` |
|     5411 | 11622 | `Cleanup:` |
|        - | 11623 | `	/* Cleanup the mess left behind */` |
|    10824 | 11624 | `	pVm->pByteContainer = pByteCode;` |
|    10824 | 11625 | `	SySetRelease(&aByteCode);` |
|        - | 11626 | `	/* Restore caller's namespace state */` |
|    10824 | 11627 | `	SyBlobReset(&pVm->sNamespace);` |
|    10824 | 11628 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10824 | 11629 | `	SyBlobRelease(&sSavedNs);` |
|    10824 | 11630 | `	return SXRET_OK;` |
|        2 | 11631 |  |
|        - | 11632 | `/*` |
|        - | 11633 | ` * value eval(string $code)` |
|        - | 11634 | ` *   Evaluate a string as PHP code.` |
|        - | 11635 | ` * Parameter` |
|        - | 11636 | ` *  code: PHP code to evaluate.` |
|        - | 11637 | ` * Return` |
|        - | 11638 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 11639 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 11640 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 11641 | ` */` |
|       22 | 11642 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11643 |  |
|        - | 11644 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 11645 | `	if( nArg < 1 ){` |
|        - | 11646 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11647 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11648 | `		return SXRET_OK;` |
|        - | 11649 | `	}` |
|        - | 11650 | `	/* Chunk to evaluate */` |
|       24 | 11651 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 11652 | `	if( sChunk.nByte < 1 ){` |
|        - | 11653 | `		/* Empty string,return NULL */` |
|        3 | 11654 | `		ph7_result_null(pCtx);` |
|        3 | 11655 | `		return SXRET_OK;` |
|        - | 11656 | `	}` |
|        - | 11657 | `	/* Eval the chunk */` |
|       22 | 11658 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 11659 | `	return SXRET_OK;` |
|       13 | 11660 |  |
|        - | 11661 | `/*` |
|        - | 11662 | ` * Check if a file path is already included.` |
|        - | 11663 | ` */` |
|    16356 | 11664 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 11665 |  |
|        - | 11666 | `	SyString *aEntries;` |
|        - | 11667 | `	sxu32 n;` |
|    16358 | 11668 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 11669 | `	/* Perform a linear search */` |
| 66834534 | 11670 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 66818184 | 11671 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 11672 | `			/* Already included */` |
|        7 | 11673 | `			return TRUE;` |
|        - | 11674 | `		}` |
| 33409090 | 11675 | `	}` |
|    16352 | 11676 | `	return FALSE;` |
|     8180 | 11677 |  |
|        - | 11678 | `/*` |
|        - | 11679 | ` * Push a file path in the appropriate VM container.` |
|        - | 11680 | ` */` |
|    18970 | 11681 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 11682 |  |
|        - | 11683 | `	SyString sPath;` |
|        - | 11684 | `	char *zDup;` |
|        - | 11685 | `#ifdef __WINNT__` |
|        - | 11686 | `	char *zCur;` |
|        - | 11687 | `#endif` |
|        - | 11688 | `	sxi32 rc;` |
|    18972 | 11689 | `	if( nLen < 0 ){` |
|     2616 | 11690 | `		nLen = SyStrlen(zPath);` |
|     1307 | 11691 | `	}` |
|        - | 11692 | `	/* Duplicate the file path first */` |
|    18972 | 11693 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    18972 | 11694 | `	if( zDup == 0 ){` |
|      ! 0 | 11695 | `		return SXERR_MEM;` |
|        - | 11696 | `	}` |
|        - | 11697 | `#ifdef __WINNT__` |
|        - | 11698 | `	/* Normalize path on windows` |
|        - | 11699 | `	 * Example:` |
|        - | 11700 | `	 *    Path/To/File.php` |
|        - | 11701 | `	 * becomes` |
|        - | 11702 | `	 *   path\to\file.php` |
|        - | 11703 | `	 */` |
|        2 | 11704 | `	zCur = zDup;` |
|        2 | 11705 | `	while( zCur[0] != 0 ){` |
|        2 | 11706 | `		if( zCur[0] == '/' ){` |
|        2 | 11707 | `			zCur[0] = '\\';` |
|        2 | 11708 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 11709 | `			int c = SyToLower(zCur[0]);` |
|        1 | 11710 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 11711 | `		}` |
|        2 | 11712 | `		zCur++;` |
|        2 | 11713 | `	}` |
|        - | 11714 | `#endif` |
|        - | 11715 | `	/* Install the file path */` |
|    18972 | 11716 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    18972 | 11717 | `	if( !bMain ){` |
|    16358 | 11718 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 11719 | `			/* Already included */` |
|        7 | 11720 | `			*pNew = 0;` |
|        4 | 11721 | `		}else{` |
|        - | 11722 | `			/* Insert in the corresponding container */` |
|    16352 | 11723 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    16352 | 11724 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 11725 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 11726 | `				return rc;` |
|        - | 11727 | `			}` |
|    16352 | 11728 | `			*pNew = 1;` |
|        - | 11729 | `		}` |
|     8178 | 11730 | `	}` |
|    18972 | 11731 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    18972 | 11732 | `	return SXRET_OK;` |
|     9487 | 11733 |  |
|        - | 11734 | `/*` |
|        - | 11735 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 11736 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 11737 | ` * indicates failure.` |
|        - | 11738 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 11739 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 11740 | ` * operations.` |
|        - | 11741 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 11742 | ` * this function is a no-op.` |
|        - | 11743 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 11744 | ` * constructs for more information.` |
|        - | 11745 | ` */` |
|     8190 | 11746 | `static sxi32 VmExecIncludedFile(` |
|        - | 11747 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 11748 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 11749 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 11750 | `	 )` |
|        2 | 11751 |  |
|        - | 11752 | `	sxi32 rc;` |
|        - | 11753 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11754 | `	const ph7_io_stream *pStream;` |
|        - | 11755 | `	SyBlob sContents;` |
|        - | 11756 | `	void *pHandle;` |
|        - | 11757 | `	ph7_vm *pVm;` |
|        - | 11758 | `	int isNew;` |
|        - | 11759 | `	/* Initialize fields */` |
|     8192 | 11760 | `	pVm = pCtx->pVm;` |
|     8192 | 11761 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8192 | 11762 | `	isNew = 0;` |
|        - | 11763 | `	/* Extract the associated stream */` |
|     8192 | 11764 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 11765 | `	/*` |
|        - | 11766 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 11767 | `	 * in a read-only mode.` |
|        - | 11768 | `	 */` |
|     8192 | 11769 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8192 | 11770 | `	if( pHandle == 0 ){` |
|        8 | 11771 | `		return SXERR_IO;` |
|        - | 11772 | `	}` |
|     8186 | 11773 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8186 | 11774 | `	if( IncludeOnce && !isNew ){` |
|        - | 11775 | `		/* Already included */` |
|        5 | 11776 | `		rc = SXERR_EXISTS;` |
|        3 | 11777 | `	}else{` |
|        - | 11778 | `		/* Read the whole file contents */` |
|     8182 | 11779 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8182 | 11780 | `		if( rc == SXRET_OK ){` |
|        - | 11781 | `			SyString sScript;` |
|        - | 11782 | `			/* Compile and execute the script */` |
|     8182 | 11783 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8182 | 11784 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4090 | 11785 | `		}` |
|        - | 11786 | `	}` |
|        - | 11787 | `	/* Pop from the set of included file */` |
|     8186 | 11788 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 11789 | `	/* Close the handle */` |
|     8186 | 11790 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 11791 | `	/* Release the working buffer */` |
|     8186 | 11792 | `	SyBlobRelease(&sContents);` |
|        - | 11793 | `#else` |
|        - | 11794 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 11795 | `	SXUNUSED(pPath);` |
|        - | 11796 | `	SXUNUSED(IncludeOnce);` |
|        - | 11797 | `	rc = SXERR_IO;` |
|        - | 11798 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8186 | 11799 | `	return rc;` |
|     4097 | 11800 |  |
|        - | 11801 | `/*` |
|        - | 11802 | ` * string get_include_path(void)` |
|        - | 11803 | ` *  Gets the current include_path configuration option.` |
|        - | 11804 | ` * Parameter` |
|        - | 11805 | ` *  None` |
|        - | 11806 | ` * Return` |
|        - | 11807 | ` *  Included paths as a string` |
|        - | 11808 | ` */` |
|        2 | 11809 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11810 |  |
|        3 | 11811 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11812 | `	SyString *aEntry;` |
|        - | 11813 | `	int dir_sep;` |
|        - | 11814 | `	sxu32 n;` |
|        - | 11815 | `#ifdef __WINNT__` |
|        1 | 11816 | `	dir_sep = ';';` |
|        - | 11817 | `#else` |
|        - | 11818 | `	/* Assume UNIX path separator */` |
|        2 | 11819 | `	dir_sep = ':';` |
|        - | 11820 | `#endif` |
|        1 | 11821 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11822 | `	SXUNUSED(apArg);` |
|        - | 11823 | `	/* Point to the list of import paths */` |
|        3 | 11824 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 11825 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 11826 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 11827 | `		if( n > 0 ){` |
|        - | 11828 | `			/* Append dir seprator */` |
|      ! 0 | 11829 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 11830 | `		}` |
|        - | 11831 | `		/* Append path */` |
|        3 | 11832 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 11833 | `	}` |
|        3 | 11834 | `	return PH7_OK;` |
|        1 | 11835 |  |
|        - | 11836 | `/*` |
|        - | 11837 | ` * string get_get_included_files(void)` |
|        - | 11838 | ` *  Gets the current include_path configuration option.` |
|        - | 11839 | ` * Parameter` |
|        - | 11840 | ` *  None` |
|        - | 11841 | ` * Return` |
|        - | 11842 | ` *  Included paths as a string` |
|        - | 11843 | ` */` |
|        2 | 11844 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11845 |  |
|        3 | 11846 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 11847 | `	ph7_value *pArray,*pWorker;` |
|        - | 11848 | `	SyString *pEntry;` |
|        - | 11849 | `	int c,d;` |
|        - | 11850 | `	/* Create an array and a working value */` |
|        3 | 11851 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 11852 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 11853 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 11854 | `		/* Out of memory,return null */` |
|      ! 0 | 11855 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11856 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11857 | `		SXUNUSED(apArg);` |
|      ! 0 | 11858 | `		return PH7_OK;` |
|        - | 11859 | `	}` |
|        3 | 11860 | `	c = d = '/';` |
|        - | 11861 | `#ifdef __WINNT__` |
|        1 | 11862 | `	d = '\\';` |
|        - | 11863 | `#endif` |
|        - | 11864 | `	/* Iterate throw entries */` |
|        3 | 11865 | `	SySetResetCursor(pFiles);` |
|     3839 | 11866 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 11867 | `		const char *zBase,*zEnd;` |
|        - | 11868 | `		int iLen;` |
|        - | 11869 | `		/* reset the string cursor */` |
|     3837 | 11870 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 11871 | `		/* Extract base name */` |
|     3837 | 11872 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 11873 | `		/* Ignore trailing '/' */` |
|     5755 | 11874 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 11875 | `			zEnd--;` |
|      ! 0 | 11876 | `		}` |
|     3837 | 11877 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 11878 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 11879 | `			zEnd--;` |
|        1 | 11880 | `		}` |
|     3837 | 11881 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 11882 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 11883 | `		/* Copy entry name */` |
|     3837 | 11884 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 11885 | `		/* Perform the insertion */` |
|     3837 | 11886 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 11887 | `	}` |
|        - | 11888 | `	/* All done,return the created array */` |
|        3 | 11889 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11890 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 11891 | `	 * by the engine as soon we return from this foreign` |
|        - | 11892 | `	 * function.` |
|        - | 11893 | `	 */` |
|        3 | 11894 | `	return PH7_OK;` |
|        2 | 11895 |  |
|        - | 11896 | `/*` |
|        - | 11897 | ` * include:` |
|        - | 11898 | ` * According to the PHP reference manual.` |
|        - | 11899 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 11900 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 11901 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 11902 | ` *  include() will finally check in the calling script's own directory` |
|        - | 11903 | ` *  and the current working directory before failing. The include()` |
|        - | 11904 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 11905 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 11906 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 11907 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 11908 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 11909 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 11910 | ` *  directory to find the requested file.` |
|        - | 11911 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 11912 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 11913 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 11914 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 11915 | ` */` |
|     8172 | 11916 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11917 |  |
|        - | 11918 | `	SyString sFile;` |
|        - | 11919 | `	sxi32 rc;` |
|     8174 | 11920 | `	if( nArg < 1 ){` |
|        - | 11921 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11922 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11923 | `		return SXRET_OK;` |
|        - | 11924 | `	}` |
|        - | 11925 | `	/* File to include */` |
|     8174 | 11926 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8174 | 11927 | `	if( sFile.nByte < 1 ){` |
|        - | 11928 | `		/* Empty string,return NULL */` |
|      ! 0 | 11929 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11930 | `		return SXRET_OK;` |
|        - | 11931 | `	}` |
|        - | 11932 | `	/* Open,compile and execute the desired script */` |
|     8174 | 11933 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8174 | 11934 | `	if( rc != SXRET_OK ){` |
|        - | 11935 | `		/* Emit a warning and return false */` |
|        3 | 11936 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 11937 | `		ph7_result_bool(pCtx,0);` |
|        1 | 11938 | `	}` |
|     8174 | 11939 | `	return SXRET_OK;` |
|     4088 | 11940 |  |
|        - | 11941 | `/*` |
|        - | 11942 | ` * include_once:` |
|        - | 11943 | ` *  According to the PHP reference manual.` |
|        - | 11944 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 11945 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 11946 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 11947 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 11948 | ` *   just once.` |
|        - | 11949 | ` */` |
|        4 | 11950 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11951 |  |
|        - | 11952 | `	SyString sFile;` |
|        - | 11953 | `	sxi32 rc;` |
|        5 | 11954 | `	if( nArg < 1 ){` |
|        - | 11955 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11956 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11957 | `		return SXRET_OK;` |
|        - | 11958 | `	}` |
|        - | 11959 | `	/* File to include */` |
|        5 | 11960 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 11961 | `	if( sFile.nByte < 1 ){` |
|        - | 11962 | `		/* Empty string,return NULL */` |
|      ! 0 | 11963 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11964 | `		return SXRET_OK;` |
|        - | 11965 | `	}` |
|        - | 11966 | `	/* Open,compile and execute the desired script */` |
|        5 | 11967 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 11968 | `	if( rc == SXERR_EXISTS ){` |
|        - | 11969 | `		/* File already included,return TRUE */` |
|        3 | 11970 | `		ph7_result_bool(pCtx,1);` |
|        3 | 11971 | `		return SXRET_OK;` |
|        - | 11972 | `	}` |
|        3 | 11973 | `	if( rc != SXRET_OK ){` |
|        - | 11974 | `		/* Emit a warning and return false */` |
|      ! 0 | 11975 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 11976 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11977 | ` 	}` |
|        3 | 11978 | `	return SXRET_OK;` |
|        3 | 11979 |  |
|        - | 11980 | `/*` |
|        - | 11981 | ` * require.` |
|        - | 11982 | ` *  According to the PHP reference manual.` |
|        - | 11983 | ` *   require() is identical to include() except upon failure it will` |
|        - | 11984 | ` *   also produce a fatal level error.` |
|        - | 11985 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 11986 | ` *   emits a warning  which allows the script to continue.` |
|        - | 11987 | ` */` |
|        6 | 11988 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11989 |  |
|        - | 11990 | `	SyString sFile;` |
|        - | 11991 | `	sxi32 rc;` |
|        8 | 11992 | `	if( nArg < 1 ){` |
|        - | 11993 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 11994 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11995 | `		return SXRET_OK;` |
|        - | 11996 | `	}` |
|        - | 11997 | `	/* File to include */` |
|        8 | 11998 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 11999 | `	if( sFile.nByte < 1 ){` |
|        - | 12000 | `		/* Empty string,return NULL */` |
|      ! 0 | 12001 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12002 | `		return SXRET_OK;` |
|        - | 12003 | `	}` |
|        - | 12004 | `	/* Open,compile and execute the desired script */` |
|        8 | 12005 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 12006 | `	if( rc != SXRET_OK ){` |
|        - | 12007 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12008 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12009 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12010 | `		return PH7_ABORT;` |
|        - | 12011 | `	}` |
|        8 | 12012 | `	return SXRET_OK;` |
|        5 | 12013 |  |
|        - | 12014 | `/*` |
|        - | 12015 | ` * require_once:` |
|        - | 12016 | ` *  According to the PHP reference manual.` |
|        - | 12017 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 12018 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 12019 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 12020 | ` *   and how it differs from its non _once siblings.` |
|        - | 12021 | ` */` |
|        4 | 12022 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12023 |  |
|        - | 12024 | `	SyString sFile;` |
|        - | 12025 | `	sxi32 rc;` |
|        5 | 12026 | `	if( nArg < 1 ){` |
|        - | 12027 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12028 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12029 | `		return SXRET_OK;` |
|        - | 12030 | `	}` |
|        - | 12031 | `	/* File to include */` |
|        5 | 12032 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 12033 | `	if( sFile.nByte < 1 ){` |
|        - | 12034 | `		/* Empty string,return NULL */` |
|      ! 0 | 12035 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12036 | `		return SXRET_OK;` |
|        - | 12037 | `	}` |
|        - | 12038 | `	/* Open,compile and execute the desired script */` |
|        5 | 12039 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 12040 | `	if( rc == SXERR_EXISTS ){` |
|        - | 12041 | `		/* File already included,return TRUE */` |
|        3 | 12042 | `		ph7_result_bool(pCtx,1);` |
|        3 | 12043 | `		return SXRET_OK;` |
|        - | 12044 | `	}` |
|        3 | 12045 | `	if( rc != SXRET_OK ){` |
|        - | 12046 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 12047 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 12048 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12049 | `		return PH7_ABORT;` |
|        - | 12050 | `	}` |
|        3 | 12051 | `	return SXRET_OK;` |
|        3 | 12052 |  |
|        - | 12053 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 12054 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 12055 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 12056 | `/*` |
|        - | 12057 | ` * Section:` |
|        - | 12058 | ` *  SPL Autoloading functions.` |
|        - | 12059 | ` * Status:` |
|        - | 12060 | ` *  Stable.` |
|        - | 12061 | ` */` |
|        - | 12062 | `/*` |
|        - | 12063 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 12064 | ` *  Register given function as __autoload() implementation.` |
|        - | 12065 | ` * Parameters` |
|        - | 12066 | ` *  callback` |
|        - | 12067 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 12068 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 12069 | ` *  throw` |
|        - | 12070 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 12071 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 12072 | ` *  prepend` |
|        - | 12073 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 12074 | ` *   autoload stack instead of appending it.` |
|        - | 12075 | ` * Return` |
|        - | 12076 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12077 | ` */` |
|       34 | 12078 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12079 |  |
|        - | 12080 | `	VmAutoloadCB sEntry;` |
|       36 | 12081 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 12082 | `	int iPrepend = 0;` |
|        - | 12083 | `	sxu32 n;` |
|       36 | 12084 | `	if( nArg < 1 ){` |
|        - | 12085 | `		/* No callback provided — register default spl_autoload.` |
|        - | 12086 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 12087 | `		/* Check for duplicates first */` |
|        9 | 12088 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 12089 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 12090 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 12091 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 12092 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 12093 | `				ph7_result_bool(pCtx,1);` |
|        5 | 12094 | `				return SXRET_OK;` |
|        - | 12095 | `			}` |
|      ! 0 | 12096 | `		}` |
|        5 | 12097 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 12098 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 12099 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 12100 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 12101 | `		ph7_result_bool(pCtx,1);` |
|        5 | 12102 | `		return SXRET_OK;` |
|        - | 12103 | `	}` |
|        - | 12104 | `	/* Validate that the callback is callable */` |
|       28 | 12105 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 12106 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 12107 | `		if( nArg >= 2 ){` |
|      ! 0 | 12108 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12109 | `		}` |
|      ! 0 | 12110 | `		if( iThrow ){` |
|      ! 0 | 12111 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 12112 | `				"Argument is not callable");` |
|      ! 0 | 12113 | `		}` |
|      ! 0 | 12114 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12115 | `		return SXRET_OK;` |
|        - | 12116 | `	}` |
|        - | 12117 | `	/* Check for duplicates */` |
|       46 | 12118 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 12119 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 12120 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12121 | `			/* Already registered */` |
|      ! 0 | 12122 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12123 | `			return SXRET_OK;` |
|        - | 12124 | `		}` |
|       11 | 12125 | `	}` |
|        - | 12126 | `	/* Check prepend flag */` |
|       28 | 12127 | `	if( nArg >= 3 ){` |
|        3 | 12128 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 12129 | `	}` |
|        - | 12130 | `	/* Store the callback */` |
|       28 | 12131 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 12132 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 12133 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 12134 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 12135 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 12136 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 12137 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 12138 | `		VmAutoloadCB *aBase;` |
|        3 | 12139 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12140 | `		/* Rotate: move last entry to front */` |
|        3 | 12141 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 12142 | `		if( aBase ){` |
|        - | 12143 | `			VmAutoloadCB sTemp;` |
|        - | 12144 | `			sxu32 i;` |
|        3 | 12145 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 12146 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 12147 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 12148 | `			}` |
|        3 | 12149 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 12150 | `		}` |
|        2 | 12151 | `	}else{` |
|       26 | 12152 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 12153 | `	}` |
|       28 | 12154 | `	ph7_result_bool(pCtx,1);` |
|       28 | 12155 | `	return SXRET_OK;` |
|       19 | 12156 |  |
|        - | 12157 | `/*` |
|        - | 12158 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 12159 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 12160 | ` * Parameters` |
|        - | 12161 | ` *  callback` |
|        - | 12162 | ` *   The autoload function being unregistered.` |
|        - | 12163 | ` * Return` |
|        - | 12164 | ` *  TRUE on success, FALSE on failure.` |
|        - | 12165 | ` */` |
|       32 | 12166 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12167 |  |
|       34 | 12168 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12169 | `	sxu32 n,nEntry;` |
|       34 | 12170 | `	if( nArg < 1 ){` |
|      ! 0 | 12171 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12172 | `		return SXRET_OK;` |
|        - | 12173 | `	}` |
|       34 | 12174 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12175 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 12176 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 12177 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 12178 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 12179 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 12180 | `			sxu32 i;` |
|       32 | 12181 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 12182 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 12183 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 12184 | `			}` |
|        - | 12185 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 12186 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 12187 | `			ph7_result_bool(pCtx,1);` |
|       32 | 12188 | `			return SXRET_OK;` |
|        - | 12189 | `		}` |
|        3 | 12190 | `	}` |
|        3 | 12191 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12192 | `	return SXRET_OK;` |
|       18 | 12193 |  |
|        - | 12194 | `/*` |
|        - | 12195 | ` * array spl_autoload_functions(void)` |
|        - | 12196 | ` *  Return all registered __autoload() functions.` |
|        - | 12197 | ` * Return` |
|        - | 12198 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 12199 | ` *  an empty array is returned.` |
|        - | 12200 | ` */` |
|       20 | 12201 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12202 |  |
|       21 | 12203 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12204 | `	ph7_value *pArray;` |
|        - | 12205 | `	sxu32 n,nEntry;` |
|       10 | 12206 | `	SXUNUSED(nArg);` |
|       10 | 12207 | `	SXUNUSED(apArg);` |
|       21 | 12208 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 12209 | `	if( pArray == 0 ){` |
|      ! 0 | 12210 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12211 | `		return SXRET_OK;` |
|        - | 12212 | `	}` |
|       21 | 12213 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 12214 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 12215 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 12216 | `		if( pEntry ){` |
|       15 | 12217 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 12218 | `		}` |
|        8 | 12219 | `	}` |
|       21 | 12220 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 12221 | `	return SXRET_OK;` |
|       11 | 12222 |  |
|        - | 12223 | `/*` |
|        - | 12224 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 12225 | ` *  Default implementation of __autoload().` |
|        - | 12226 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 12227 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 12228 | ` * Parameters` |
|        - | 12229 | ` *  class` |
|        - | 12230 | ` *   The class name being searched.` |
|        - | 12231 | ` *  file_extensions` |
|        - | 12232 | ` *   Comma-separated list of file extensions to try.` |
|        - | 12233 | ` */` |
|        2 | 12234 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12235 |  |
|        - | 12236 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 12237 | `	SyBlob sPath;` |
|        - | 12238 | `	int nClass;` |
|        - | 12239 | `	sxi32 rc;` |
|        3 | 12240 | `	if( nArg < 1 ){` |
|      ! 0 | 12241 | `		return SXRET_OK;` |
|        - | 12242 | `	}` |
|        3 | 12243 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 12244 | `	if( nClass < 1 ){` |
|      ! 0 | 12245 | `		return SXRET_OK;` |
|        - | 12246 | `	}` |
|        - | 12247 | `	/* Default extensions */` |
|        3 | 12248 | `	zExt = ".php,.inc";` |
|        3 | 12249 | `	if( nArg >= 2 ){` |
|        - | 12250 | `		int nExt;` |
|      ! 0 | 12251 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 12252 | `		if( nExt < 1 ){` |
|      ! 0 | 12253 | `			zExt = ".php,.inc";` |
|      ! 0 | 12254 | `		}` |
|      ! 0 | 12255 | `	}` |
|        3 | 12256 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 12257 | `	/* Iterate over comma-separated extensions */` |
|        3 | 12258 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 12259 | `	zCur = zExt;` |
|        7 | 12260 | `	while( zCur < zEnd ){` |
|        - | 12261 | `		const char *zComma;` |
|        - | 12262 | `		SyString sFile;` |
|        - | 12263 | `		int i;` |
|        - | 12264 | `		/* Find next comma or end */` |
|        5 | 12265 | `		zComma = zCur;` |
|       21 | 12266 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 12267 | `			zComma++;` |
|        1 | 12268 | `		}` |
|        - | 12269 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 12270 | `		SyBlobReset(&sPath);` |
|       69 | 12271 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 12272 | `			char c = zClass[i];` |
|       65 | 12273 | `			if( c == '\\' ){` |
|      ! 0 | 12274 | `				c = '/';` |
|       65 | 12275 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 12276 | `				c = c + ('a' - 'A');` |
|        6 | 12277 | `			}` |
|       65 | 12278 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 12279 | `		}` |
|        - | 12280 | `		/* Append extension */` |
|        5 | 12281 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 12282 | `		/* Try to include the file */` |
|        5 | 12283 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 12284 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 12285 | `		if( rc == SXRET_OK ){` |
|        - | 12286 | `			/* File included successfully */` |
|      ! 0 | 12287 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 12288 | `			return SXRET_OK;` |
|        - | 12289 | `		}` |
|        - | 12290 | `		/* Move past the comma */` |
|        5 | 12291 | `		zCur = zComma;` |
|        5 | 12292 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 12293 | `			zCur++;` |
|        1 | 12294 | `		}` |
|        1 | 12295 | `	}` |
|        3 | 12296 | `	SyBlobRelease(&sPath);` |
|        3 | 12297 | `	return SXRET_OK;` |
|        2 | 12298 |  |
|        - | 12299 | `/* Table of built-in VM functions. */` |
|        - | 12300 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 12301 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 12302 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 12303 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 12304 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 12305 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 12306 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 12307 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 12308 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 12309 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 12310 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 12311 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 12312 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 12313 | `	    /* Constants management */` |
|        - | 12314 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 12315 | `	{ "define",   vm_builtin_define               },` |
|        - | 12316 | `	{ "constant", vm_builtin_constant             },` |
|        - | 12317 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 12318 | `	   /* Class/Object functions */` |
|        - | 12319 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 12320 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 12321 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 12322 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 12323 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 12324 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 12325 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 12326 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 12327 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 12328 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 12329 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 12330 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 12331 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 12332 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 12333 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 12334 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 12335 | `	   /* SPL Autoloading */` |
|        - | 12336 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 12337 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 12338 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 12339 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 12340 | `	   /* Random numbers/strings generators */` |
|        - | 12341 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 12342 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 12343 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 12344 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 12345 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 12346 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12347 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12348 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 12349 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12350 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12351 | `	   /* Language constructs functions */` |
|        - | 12352 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 12353 | `	{ "print", vm_builtin_print                   },` |
|        - | 12354 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 12355 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 12356 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 12357 | `	  /* Variable handling functions */` |
|        - | 12358 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 12359 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 12360 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 12361 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 12362 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 12363 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 12364 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 12365 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 12366 | `	  /* Ouput control functions */` |
|        - | 12367 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 12368 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 12369 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 12370 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 12371 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 12372 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 12373 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 12374 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 12375 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 12376 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 12377 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 12378 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 12379 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 12380 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 12381 | `	  /* Assertion functions */` |
|        - | 12382 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 12383 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 12384 | `	  /* Error reporting functions */` |
|        - | 12385 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 12386 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 12387 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 12388 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 12389 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 12390 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 12391 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 12392 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 12393 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 12394 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 12395 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 12396 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 12397 | `	  /* Release info */` |
|        - | 12398 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 12399 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 12400 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 12401 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 12402 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 12403 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 12404 | `	  /* hashmap */` |
|        - | 12405 | `	{"compact",          vm_builtin_compact       },` |
|        - | 12406 | `	{"extract",          vm_builtin_extract       },` |
|        - | 12407 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 12408 | `	  /* URL related function */` |
|        - | 12409 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 12410 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 12411 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12412 | `	   /* XML processing functions */` |
|        - | 12413 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 12414 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 12415 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 12416 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 12417 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 12418 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 12419 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 12420 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 12421 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 12422 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 12423 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 12424 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 12425 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 12426 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 12427 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 12428 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 12429 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 12430 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 12431 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 12432 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 12433 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 12434 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12435 | `	   /* UTF-8 encoding/decoding */` |
|        - | 12436 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 12437 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 12438 | `	   /* Command line processing */` |
|        - | 12439 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 12440 | `	   /* JSON encoding/decoding */` |
|        - | 12441 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 12442 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 12443 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 12444 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 12445 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 12446 | `	   /* Files/URI inclusion facility */` |
|        - | 12447 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 12448 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 12449 | `	{ "include",      vm_builtin_include          },` |
|        - | 12450 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 12451 | `	{ "require",      vm_builtin_require          },` |
|        - | 12452 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 12453 | `};` |
|        - | 12454 | `/*` |
|        - | 12455 | ` * Register the built-in VM functions defined above.` |
|        - | 12456 | ` */` |
|     2354 | 12457 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 12458 |  |
|        - | 12459 | `	sxi32 rc;` |
|        - | 12460 | `	sxu32 n;` |
|   303668 | 12461 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 12462 | `		/* Note that these special functions have access` |
|        - | 12463 | `		 * to the underlying virtual machine as their` |
|        - | 12464 | `		 * private data.` |
|        - | 12465 | `		 */` |
|   301314 | 12466 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   301314 | 12467 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 12468 | `			return rc;` |
|        - | 12469 | `		}` |
|   150658 | 12470 | `	}` |
|     2356 | 12471 | `	return SXRET_OK;` |
|     1179 | 12472 |  |
|        - | 12473 | `/*` |
|        - | 12474 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 12475 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 12476 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 12477 | ` */` |
|    33210 | 12478 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 12479 |  |
|    33212 | 12480 | `	if( !iLoadable ){` |
|    31898 | 12481 | `		return pClass;` |
|        - | 12482 | `	}` |
|     1316 | 12483 | `	while(pClass){` |
|     1316 | 12484 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1316 | 12485 | `			return pClass;` |
|        - | 12486 | `		}` |
|      ! 0 | 12487 | `		pClass = pClass->pNextName;` |
|      ! 0 | 12488 | `	}` |
|      ! 0 | 12489 | `	return 0;` |
|    16607 | 12490 |  |
|        - | 12491 | `/*` |
|        - | 12492 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 12493 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 12494 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 12495 | ` * registered in the VM's class table.` |
|        - | 12496 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 12497 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 12498 | ` */` |
|       36 | 12499 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12500 |  |
|        - | 12501 | `	VmAutoloadCB *pEntry;` |
|        - | 12502 | `	ph7_value sArg,sResult;` |
|        - | 12503 | `	SyHashEntry *pHashEntry;` |
|        - | 12504 | `	ph7_class *pClass;` |
|        - | 12505 | `	sxu32 n,nEntry;` |
|       38 | 12506 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 12507 | `	if( nEntry < 1 ){` |
|       24 | 12508 | `		return 0;` |
|        - | 12509 | `	}` |
|        - | 12510 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 12511 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 12512 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 12513 | `	}` |
|        - | 12514 | `	/* Mark this class as being autoloaded */` |
|       14 | 12515 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 12516 | `	/* Prepare the class name argument */` |
|       14 | 12517 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 12518 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 12519 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 12520 | `	pClass = 0;` |
|       28 | 12521 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 12522 | `		ph7_value *apArg[1];` |
|       24 | 12523 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 12524 | `		if( pEntry == 0 ){` |
|      ! 0 | 12525 | `			continue;` |
|        - | 12526 | `		}` |
|       24 | 12527 | `		apArg[0] = &sArg;` |
|       24 | 12528 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 12529 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 12530 | `			continue;` |
|        - | 12531 | `		}` |
|        - | 12532 | `		/* Check if the class is now available */` |
|       24 | 12533 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 12534 | `		if( pHashEntry ){` |
|       10 | 12535 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 12536 | `			if( pClass ){` |
|       10 | 12537 | `				break;` |
|        - | 12538 | `			}` |
|      ! 0 | 12539 | `		}` |
|        9 | 12540 | `	}` |
|       14 | 12541 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 12542 | `	PH7_MemObjRelease(&sResult);` |
|        - | 12543 | `	/* Remove reentrancy guard */` |
|       14 | 12544 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 12545 | `	return pClass;` |
|       20 | 12546 |  |
|        - | 12547 | `/*` |
|        - | 12548 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 12549 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 12550 | ` */` |
|       18 | 12551 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 12552 |  |
|       20 | 12553 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 12554 |  |
|        - | 12555 | `/*` |
|        - | 12556 | ` * Check if the given name refer to an installed class.` |
|        - | 12557 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 12558 | ` */` |
|    33220 | 12559 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 12560 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 12561 | `	const char *zName,  /* Name of the target class */` |
|        - | 12562 | `	sxu32 nByte,        /* zName length */` |
|        - | 12563 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 12564 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 12565 | `						 */` |
|        - | 12566 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 12567 | `	)` |
|        2 | 12568 |  |
|        - | 12569 | `	SyHashEntry *pEntry;` |
|        - | 12570 | `	ph7_class *pClass;` |
|    16610 | 12571 | `	SXUNUSED(iNest);` |
|        - | 12572 | `	/* Exact class lookup.` |
|        - | 12573 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 12574 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    33222 | 12575 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    33222 | 12576 | `	if( pEntry == 0 ){` |
|        - | 12577 | `		/* Class not found in hash table — try autoload before giving up */` |
|       20 | 12578 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 12579 | `	}` |
|    33204 | 12580 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    33204 | 12581 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    16612 | 12582 |  |
|        - | 12583 | `/*` |
|        - | 12584 | ` * Reference Table Implementation` |
|        - | 12585 | ` * Status: stable <chm@symisc.net>` |
|        - | 12586 | ` * Intro` |
|        - | 12587 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 12588 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 12589 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 12590 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 12591 | ` *  Refer to the official for more information on this powerful` |
|        - | 12592 | ` *  extension.` |
|        - | 12593 | ` */` |
|        - | 12594 | `/*` |
|        - | 12595 | ` * Allocate a new reference entry.` |
|        - | 12596 | ` */` |
|  3071772 | 12597 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 12598 |  |
|        - | 12599 | `	VmRefObj *pRef;` |
|        - | 12600 | `	/* Allocate a new instance */` |
|  3071774 | 12601 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3071774 | 12602 | `	if( pRef == 0 ){` |
|      ! 0 | 12603 | `		return 0;` |
|        - | 12604 | `	}` |
|        - | 12605 | `	/* Zero the structure */` |
|  3071774 | 12606 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 12607 | `	/* Initialize fields */` |
|  3071774 | 12608 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3071774 | 12609 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3071774 | 12610 | `	pRef->nIdx = nIdx;` |
|  3071774 | 12611 | `	return pRef;` |
|  1535888 | 12612 |  |
|        - | 12613 | `/*` |
|        - | 12614 | ` * Default hash function used by the reference table` |
|        - | 12615 | ` * for lookup/insertion operations.` |
|        - | 12616 | ` */` |
| 16948654 | 12617 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 12618 |  |
|        - | 12619 | `	/* Calculate the hash based on the memory object index */` |
| 16948656 | 12620 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 12621 |  |
|        - | 12622 | `/*` |
|        - | 12623 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 12624 | ` * in the reference table.` |
|        - | 12625 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 12626 | ` * otherwise.` |
|        - | 12627 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12628 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12629 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12630 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12631 | ` * Refer to the official for more information on this powerful` |
|        - | 12632 | ` * extension.` |
|        - | 12633 | ` */` |
|  9167856 | 12634 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 12635 |  |
|        - | 12636 | `	VmRefObj *pRef;` |
|        - | 12637 | `	sxu32 nBucket;` |
|        - | 12638 | `	/* Point to the appropriate bucket */` |
|  9167858 | 12639 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 12640 | `	/* Perform the lookup */` |
|  9167858 | 12641 | `	pRef = pVm->apRefObj[nBucket];` |
| 19994428 | 12642 | `	for(;;){` |
| 39974332 | 12643 | `		if( pRef == 0 ){` |
|  3152634 | 12644 | `			break;` |
|        - | 12645 | `		}` |
| 36821700 | 12646 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 12647 | `			/* Entry found */` |
|  6015226 | 12648 | `			return pRef;` |
|        - | 12649 | `		}` |
|        - | 12650 | `		/* Point to the next entry */` |
| 30806476 | 12651 | `		pRef = pRef->pNextCollide;` |
|        2 | 12652 | `	}` |
|        - | 12653 | `	/* No such entry,return NULL */` |
|  3152634 | 12654 | `	return 0;` |
|  4583930 | 12655 |  |
|        - | 12656 | `/*` |
|        - | 12657 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12658 | ` *` |
|        - | 12659 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12660 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12661 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12662 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12663 | ` * Refer to the official for more information on this powerful` |
|        - | 12664 | ` * extension.` |
|        - | 12665 | ` */` |
|  3071772 | 12666 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12667 |  |
|        - | 12668 | `	sxu32 nBucket;` |
|  3071774 | 12669 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 12670 | `		VmRefObj **apNew;` |
|        - | 12671 | `		sxu32 nNew;` |
|        - | 12672 | `		/* Allocate a larger table */` |
|     4022 | 12673 | `		nNew = pVm->nRefSize << 1;` |
|     4022 | 12674 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4022 | 12675 | `		if( apNew ){` |
|     4022 | 12676 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 12677 | `			sxu32 n;` |
|        - | 12678 | `			/* Zero the structure */` |
|     4022 | 12679 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 12680 | `			/* Rehash all referenced entries */` |
|  2841050 | 12681 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 12682 | `				/* Remove old collision links */` |
|  2837030 | 12683 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 12684 | `				/* Point to the appropriate bucket */` |
|  2837030 | 12685 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 12686 | `				/* Insert the entry  */` |
|  2837030 | 12687 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2837030 | 12688 | `				if( apNew[nBucket] ){` |
|  2298896 | 12689 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 12690 | `				}` |
|  2837030 | 12691 | `				apNew[nBucket] = pEntry;` |
|        - | 12692 | `				/* Point to the next entry */` |
|  2837030 | 12693 | `				pEntry = pEntry->pNext;` |
|  1418516 | 12694 | `			}` |
|        - | 12695 | `			/* Release the old table */` |
|     4022 | 12696 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 12697 | `			/* Install the new one */` |
|     4022 | 12698 | `			pVm->apRefObj = apNew;` |
|     4022 | 12699 | `			pVm->nRefSize = nNew;` |
|     2010 | 12700 | `		}` |
|     2010 | 12701 | `	}` |
|        - | 12702 | `	/* Point to the appropriate bucket */` |
|  3071774 | 12703 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 12704 | `	/* Insert the entry */` |
|  3071774 | 12705 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3071774 | 12706 | `	if( pVm->apRefObj[nBucket] ){` |
|  2534637 | 12707 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1267261 | 12708 | `	}` |
|  3071774 | 12709 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3071774 | 12710 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3071774 | 12711 | `	pVm->nRefUsed++;` |
|  3071774 | 12712 | `	return SXRET_OK;` |
|        2 | 12713 |  |
|        - | 12714 | `/*` |
|        - | 12715 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 12716 | ` * the reference table.` |
|        - | 12717 | ` * This function is invoked when the user perform an unset` |
|        - | 12718 | ` * call [i.e: unset($var); ].` |
|        - | 12719 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12720 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12721 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12722 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12723 | ` * Refer to the official for more information on this powerful` |
|        - | 12724 | ` * extension.` |
|        - | 12725 | ` */` |
|  3037844 | 12726 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 12727 |  |
|        - | 12728 | `	ph7_hashmap_node **apNode;` |
|        - | 12729 | `	SyHashEntry **apEntry;` |
|        - | 12730 | `	sxu32 n;` |
|        - | 12731 | `	/* Point to the reference table */` |
|  3037846 | 12732 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3037846 | 12733 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 12734 | `	/* Unlink the entry from the reference table */` |
|  3124738 | 12735 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    86894 | 12736 | `		if( apEntry[n] ){` |
|    86844 | 12737 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    43421 | 12738 | `		}` |
|    43448 | 12739 | `	}` |
|  5991454 | 12740 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2953610 | 12741 | `		if( apNode[n] ){` |
|     6960 | 12742 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3479 | 12743 | `		}` |
|  1476806 | 12744 | `	}` |
|  3037846 | 12745 | `	if( pRef->pPrevCollide ){` |
|  1165848 | 12746 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   583056 | 12747 | `	}else{` |
|  1872000 | 12748 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 12749 | `	}` |
|  3037846 | 12750 | `	if( pRef->pNextCollide ){` |
|  1723364 | 12751 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   861606 | 12752 | `	}` |
|  3037846 | 12753 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 12754 | `	/* Release the node */` |
|  3037846 | 12755 | `	SySetRelease(&pRef->aReference);` |
|  3037846 | 12756 | `	SySetRelease(&pRef->aArrEntries);` |
|  3037846 | 12757 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3037846 | 12758 | `	pVm->nRefUsed--;` |
|  3037846 | 12759 | `	return SXRET_OK;` |
|        2 | 12760 |  |
|        - | 12761 | `/*` |
|        - | 12762 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 12763 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12764 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12765 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12766 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12767 | ` * Refer to the official for more information on this powerful` |
|        - | 12768 | ` * extension.` |
|        - | 12769 | ` */` |
|  3102440 | 12770 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 12771 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12772 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12773 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12774 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 12775 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 12776 | `	)` |
|        2 | 12777 |  |
|  3102442 | 12778 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12779 | `	VmRefObj *pRef;` |
|        - | 12780 | `	/* Check if the referenced object already exists */` |
|  3102442 | 12781 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3102442 | 12782 | `	if( pRef == 0 ){` |
|        - | 12783 | `		/* Create a new entry */` |
|  3071774 | 12784 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3071774 | 12785 | `		if( pRef == 0 ){` |
|      ! 0 | 12786 | `			return SXERR_MEM;` |
|        - | 12787 | `		}` |
|  3071774 | 12788 | `		pRef->iFlags = iFlags;` |
|        - | 12789 | `		/* Install the entry */` |
|  3071774 | 12790 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1535886 | 12791 | `	}` |
|  3102442 | 12792 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3102442 | 12793 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 12794 | `		VmSlot sRef;` |
|        - | 12795 | `		/* Local frame,record referenced entry so that it can` |
|        - | 12796 | `		 * be deleted when we leave this frame.` |
|        - | 12797 | `		 */` |
|    80940 | 12798 | `		sRef.nIdx = nIdx;` |
|    80940 | 12799 | `		sRef.pUserData = pEntry;` |
|    80940 | 12800 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 12801 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 12802 | `		}` |
|    40469 | 12803 | `	}` |
|  3102442 | 12804 | `	if( pEntry ){` |
|        - | 12805 | `		/* Address of the hash-entry */` |
|   111416 | 12806 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    55707 | 12807 | `	}` |
|  3102442 | 12808 | `	if( pMapEntry ){` |
|        - | 12809 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2985814 | 12810 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1492906 | 12811 | `	}` |
|  3102442 | 12812 | `	return SXRET_OK;` |
|  1551222 | 12813 |  |
|        - | 12814 | `/*` |
|        - | 12815 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 12816 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 12817 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 12818 | ` * the reference implementation is consistent,solid and it's` |
|        - | 12819 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 12820 | ` * Refer to the official for more information on this powerful` |
|        - | 12821 | ` * extension.` |
|        - | 12822 | ` */` |
|  3027566 | 12823 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 12824 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 12825 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 12826 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 12827 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 12828 | `	)` |
|        2 | 12829 |  |
|        - | 12830 | `	VmRefObj *pRef;` |
|        - | 12831 | `	sxu32 n;` |
|        - | 12832 | `	/* Check if the referenced object already exists */` |
|  3027568 | 12833 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3027568 | 12834 | `	if( pRef == 0 ){` |
|        - | 12835 | `		/* Not such entry */` |
|    80856 | 12836 | `		return SXERR_NOTFOUND;` |
|        - | 12837 | `	}` |
|        - | 12838 | `	/* Remove the desired entry */` |
|  2946714 | 12839 | `	if( pEntry ){` |
|        - | 12840 | `		SyHashEntry **apEntry;` |
|       56 | 12841 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 12842 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 12843 | `			if( apEntry[n] == pEntry ){` |
|        - | 12844 | `				/* Nullify the entry */` |
|       56 | 12845 | `				apEntry[n] = 0;` |
|        - | 12846 | `				/*` |
|        - | 12847 | `				 * NOTE:` |
|        - | 12848 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 12849 | `				 * we avoid wasting spaces.` |
|        - | 12850 | `				 */` |
|       27 | 12851 | `			}` |
|       79 | 12852 | `		}` |
|       27 | 12853 | `	}` |
|  2946714 | 12854 | `	if( pMapEntry ){` |
|        - | 12855 | `		ph7_hashmap_node **apNode;` |
|  2946660 | 12856 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5893412 | 12857 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2946754 | 12858 | `			if( apNode[n] == pMapEntry ){` |
|        - | 12859 | `				/* nullify the entry */` |
|  2946660 | 12860 | `				apNode[n] = 0;` |
|  1473329 | 12861 | `			}` |
|  1473378 | 12862 | `		}` |
|  1473329 | 12863 | `	}` |
|  2946714 | 12864 | `	return SXRET_OK;` |
|  1513785 | 12865 |  |
|        - | 12866 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 12867 | `/*` |
|        - | 12868 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 12869 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 12870 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 12871 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 12872 | ` * For more information on how to register IO stream devices,please` |
|        - | 12873 | ` * refer to the official documentation.` |
|        - | 12874 | ` */` |
|    24822 | 12875 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 12876 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 12877 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 12878 | `	int nByte              /* *pzDevice length*/` |
|        - | 12879 | `	)` |
|        2 | 12880 |  |
|        - | 12881 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 12882 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 12883 | `	SyString sDev,sCur;` |
|        - | 12884 | `	sxu32 n,nEntry;` |
|        - | 12885 | `	int rc;` |
|        - | 12886 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    24824 | 12887 | `	zNext = zCur = zIn = *pzDevice;` |
|    24824 | 12888 | `	zEnd = &zIn[nByte];` |
|  1580304 | 12889 | `	while( zIn < zEnd ){` |
|  1555484 | 12890 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 12891 | `			/* Got one */` |
|        3 | 12892 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 12893 | `			break;` |
|        - | 12894 | `		}` |
|        - | 12895 | `		/* Advance the cursor */` |
|  1555482 | 12896 | `		zIn++;` |
|        2 | 12897 | `	}` |
|    24824 | 12898 | `	if( zIn >= zEnd ){` |
|        - | 12899 | `		/* No such scheme,return the default stream */` |
|    24822 | 12900 | `		return pVm->pDefStream;` |
|        - | 12901 | `	}` |
|        3 | 12902 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 12903 | `	/* Remove leading and trailing white spaces */` |
|        3 | 12904 | `	SyStringFullTrim(&sDev);` |
|        - | 12905 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 12906 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 12907 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 12908 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 12909 | `		pStream = apStream[n];` |
|        3 | 12910 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 12911 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 12912 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 12913 | `		if( rc == 0 ){` |
|        - | 12914 | `			/* Stream device found */` |
|        3 | 12915 | `			*pzDevice = zNext;` |
|        3 | 12916 | `			return pStream;` |
|        - | 12917 | `		}` |
|      ! 0 | 12918 | `	}` |
|        - | 12919 | `	/* No such stream,return NULL */` |
|      ! 0 | 12920 | `	return 0;` |
|    12413 | 12921 |  |
|        - | 12922 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 12923 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 12924 |  |
