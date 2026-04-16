# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5818/7544 lines (77.12%)

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
|   867236 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   867238 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   867204 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   867194 |   104 | `	return FALSE;` |
|   433642 |   105 |  |
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
|   566830 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   566832 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   566832 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   566828 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   566828 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   566828 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   566828 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   566828 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   566828 |   152 | `	pCons->xExpand = xExpand;` |
|   566828 |   153 | `	pCons->pUserData = pUserData;` |
|   566828 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   566828 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   566828 |   161 | `	return SXRET_OK;` |
|   283417 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1246514 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1246516 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1246516 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1246516 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1246516 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1246516 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1246516 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1246516 |   195 | `	pFunc->pVm   = pVm;` |
|  1246516 |   196 | `	pFunc->xFunc = xFunc;` |
|  1246516 |   197 | `	pFunc->pUserData = pUserData;` |
|  1246516 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1246516 |   200 | `	*ppOut = pFunc;` |
|  1246516 |   201 | `	return SXRET_OK;` |
|   623259 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1249126 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1249128 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1249128 |   223 | `	if( pEntry ){` |
|     2614 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2614 |   225 | `		pFunc->pUserData = pUserData;` |
|     2614 |   226 | `		pFunc->xFunc = xFunc;` |
|     2614 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2614 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1246516 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1246516 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1246516 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1246516 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1246516 |   243 | `	return SXRET_OK;` |
|   624565 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   229096 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   229098 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   229098 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   229098 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   229098 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   229098 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   229098 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   270 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   229098 |   271 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   229098 |   272 | `	pFunc->iFlags = iFlags;` |
|   229098 |   273 | `	pFunc->pUserData = pUserData;` |
|   229098 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   229098 |   275 | `	return SXRET_OK;` |
|        2 |   276 |  |
|        - |   277 | `/*` |
|        - |   278 | ` * Namespace-aware function lookup.` |
|        - |   279 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   280 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   281 | ` */` |
|        - |   282 | `/*` |
|        - |   283 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   284 | ` */` |
|   703474 |   285 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   286 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   287 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   288 | `	SyString *pName     /* Function name */` |
|        - |   289 | `	)` |
|        2 |   290 |  |
|        - |   291 | `	SyHashEntry *pEntry;` |
|        - |   292 | `	sxi32 rc;` |
|   703476 |   293 | `	if( pName == 0 ){` |
|        - |   294 | `		/* Use the built-in name */` |
|    38848 |   295 | `		pName = &pFunc->sName;` |
|    19423 |   296 | `	}` |
|        - |   297 | `	/* Check for duplicates (functions with the same name) first */` |
|   703476 |   298 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   703476 |   299 | `	if( pEntry ){` |
|   521356 |   300 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   521356 |   301 | `		if( pLink != pFunc ){` |
|        - |   302 | `			/* Link */` |
|      188 |   303 | `			pFunc->pNextName = pLink;` |
|      188 |   304 | `			pEntry->pUserData = pFunc;` |
|       93 |   305 | `		}` |
|   521356 |   306 | `		return SXRET_OK;` |
|        - |   307 | `	}` |
|        - |   308 | `	/* First time seen */` |
|   182122 |   309 | `	pFunc->pNextName = 0;` |
|   182122 |   310 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   182122 |   311 | `	return rc;` |
|   351739 |   312 |  |
|        - |   313 | `/*` |
|        - |   314 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   315 | ` */` |
|    53360 |   316 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   317 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   318 | `	ph7_class *pClass /* Target Class */` |
|        - |   319 | `	)` |
|        2 |   320 |  |
|    53362 |   321 | `	SyString *pName = &pClass->sName;` |
|        - |   322 | `	SyHashEntry *pEntry;` |
|        - |   323 | `	sxi32 rc;` |
|        - |   324 | `	/* Check for duplicates */` |
|    53362 |   325 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    53362 |   326 | `	if( pEntry ){` |
|       31 |   327 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   328 | `		/* Link entry with the same name */` |
|       31 |   329 | `		pClass->pNextName = pLink;` |
|       31 |   330 | `		pEntry->pUserData = pClass;` |
|       31 |   331 | `		return SXRET_OK;` |
|        - |   332 | `	}` |
|    53332 |   333 | `	pClass->pNextName = 0;` |
|        - |   334 | `	/* Perform a simple hashtable insertion */` |
|    53332 |   335 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    53332 |   336 | `	return rc;` |
|    26682 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Instruction builder interface.` |
|        - |   340 | ` */` |
|  3949080 |   341 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3949082 |   353 | `	sInstr.iOp = (sxu8)iOp;` |
|  3949082 |   354 | `	sInstr.iP1 = iP1;` |
|  3949082 |   355 | `	sInstr.iP2 = iP2;` |
|  3949082 |   356 | `	sInstr.p3  = p3;` |
|  3949082 |   357 | `	if( pIndex ){` |
|        - |   358 | `		/* Instruction index in the bytecode array */` |
|   214332 |   359 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   107165 |   360 | `	}` |
|        - |   361 | `	/* Finally,record the instruction */` |
|  3949082 |   362 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3949082 |   363 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   364 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   365 | `		/* Fall throw */` |
|      ! 0 |   366 | `	}` |
|  3949082 |   367 | `	return rc;` |
|        2 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Swap the current bytecode container with the given one.` |
|        - |   371 | ` */` |
|   512736 |   372 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   373 |  |
|   512738 |   374 | `	if( pContainer == 0 ){` |
|        - |   375 | `		/* Point to the default container */` |
|      ! 0 |   376 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   377 | `	}else{` |
|        - |   378 | `		/* Change container */` |
|   512738 |   379 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   380 | `	}` |
|   512738 |   381 | `	return SXRET_OK;` |
|        2 |   382 |  |
|        - |   383 | `/*` |
|        - |   384 | ` * Return the current bytecode container.` |
|        - |   385 | ` */` |
|   256368 |   386 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   387 |  |
|   256370 |   388 | `	return pVm->pByteContainer;` |
|        2 |   389 |  |
|        - |   390 | `/*` |
|        - |   391 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   392 | ` */` |
|   211336 |   393 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   394 |  |
|        - |   395 | `	VmInstr *pInstr;` |
|   211338 |   396 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   211338 |   397 | `	return pInstr;` |
|        2 |   398 |  |
|        - |   399 | `/*` |
|        - |   400 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   401 | ` */` |
|  1187096 |   402 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   403 |  |
|  1187098 |   404 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Pop the last VM instruction.` |
|        - |   408 | ` */` |
|   195734 |   409 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   410 |  |
|   195736 |   411 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   412 |  |
|        - |   413 | `/*` |
|        - |   414 | ` * Peek the last VM instruction.` |
|        - |   415 | ` */` |
|   778168 |   416 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   417 |  |
|   778170 |   418 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   419 |  |
|    30748 |   420 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   421 |  |
|        - |   422 | `	VmInstr *aInstr;` |
|        - |   423 | `	sxu32 n;` |
|    30750 |   424 | `	n = SySetUsed(pVm->pByteContainer);` |
|    30750 |   425 | `	if( n < 2 ){` |
|      ! 0 |   426 | `		return 0;` |
|        - |   427 | `	}` |
|    30750 |   428 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    30750 |   429 | `	return &aInstr[n - 2];` |
|    15376 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Allocate a new virtual machine frame.` |
|        - |   433 | ` */` |
|    19458 |   434 | `static VmFrame * VmNewFrame(` |
|        - |   435 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   436 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   437 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   438 | `	)` |
|        2 |   439 |  |
|        - |   440 | `	VmFrame *pFrame;` |
|        - |   441 | `	/* Allocate a new vm frame */` |
|    19460 |   442 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    19460 |   443 | `	if( pFrame == 0 ){` |
|      ! 0 |   444 | `		return 0;` |
|        - |   445 | `	}` |
|        - |   446 | `	/* Zero the structure */` |
|    19460 |   447 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   448 | `	/* Initialize frame fields */` |
|    19460 |   449 | `	pFrame->pUserData = pUserData;` |
|    19460 |   450 | `	pFrame->pThis = pThis;` |
|    19460 |   451 | `	pFrame->pVm = pVm;` |
|    19460 |   452 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    19460 |   453 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    19460 |   454 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    19460 |   455 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    19460 |   456 | `	return pFrame;` |
|     9731 |   457 |  |
|        - |   458 | `/* Forward declaration */` |
|        - |   459 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   460 | `/*` |
|        - |   461 | ` * Enter a VM frame.` |
|        - |   462 | ` */` |
|    19412 |   463 | `static sxi32 VmEnterFrame(` |
|        - |   464 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   465 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   466 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   467 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   468 | `	)` |
|        2 |   469 |  |
|        - |   470 | `	VmFrame *pFrame;` |
|        - |   471 | `	/* Allocate a new frame */` |
|    19414 |   472 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    19414 |   473 | `	if( pFrame == 0 ){` |
|      ! 0 |   474 | `		return SXERR_MEM;` |
|        - |   475 | `	}` |
|        - |   476 | `	/* Link to the list of active VM frame */` |
|    19414 |   477 | `	pFrame->pParent = pVm->pFrame;` |
|    19414 |   478 | `	pVm->pFrame = pFrame;` |
|    19414 |   479 | `	if( ppFrame ){` |
|        - |   480 | `		/* Write a pointer to the new VM frame */` |
|    16496 |   481 | `		*ppFrame = pFrame;` |
|     8247 |   482 | `	}` |
|    19414 |   483 | `	return SXRET_OK;` |
|     9708 |   484 |  |
|        - |   485 | `/*` |
|        - |   486 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   487 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   488 | ` * information.` |
|        - |   489 | ` */` |
|       58 |   490 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   491 |  |
|        - |   492 | `	VmFrame *pTarget,*pFrame;` |
|       60 |   493 | `	SyHashEntry *pEntry = 0;` |
|        - |   494 | `	sxi32 rc;` |
|        - |   495 | `	/* Point to the upper frame */` |
|       60 |   496 | `	pFrame = pVm->pFrame;` |
|       60 |   497 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       60 |   498 | `	pTarget = pFrame;` |
|       60 |   499 | `	pFrame = pTarget->pParent;` |
|       60 |   500 | `	while( pFrame ){` |
|       60 |   501 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   502 | `			/* Query the current frame */` |
|       60 |   503 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       60 |   504 | `			if( pEntry ){` |
|        - |   505 | `				/* Variable found */` |
|       60 |   506 | `				break;` |
|        - |   507 | `			}` |
|      ! 0 |   508 | `		}` |
|        - |   509 | `		/* Point to the upper frame */` |
|      ! 0 |   510 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   511 | `	}` |
|       60 |   512 | `	if( pEntry == 0 ){` |
|        - |   513 | `		/* Inexistant variable */` |
|      ! 0 |   514 | `		return SXERR_NOTFOUND;` |
|        - |   515 | `	}` |
|        - |   516 | `	/* Link to the current frame */` |
|       60 |   517 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       60 |   518 | `	if( rc == SXRET_OK ){` |
|        - |   519 | `		sxu32 nIdx;` |
|       60 |   520 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       60 |   521 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       29 |   522 | `	}` |
|       60 |   523 | `	return rc;` |
|       31 |   524 |  |
|        - |   525 | `/*` |
|        - |   526 | ` * Leave the top-most active frame.` |
|        - |   527 | ` */` |
|    16488 |   528 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   529 |  |
|    16490 |   530 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    16490 |   531 | `	if( pCurFrame ){` |
|        - |   532 | `		/* Unlink from the list of active VM frame */` |
|    16490 |   533 | `		pVm->pFrame = pCurFrame->pParent;` |
|    16490 |   534 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   535 | `			VmSlot  *aSlot;` |
|        - |   536 | `			sxu32 n;` |
|        - |   537 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    16270 |   538 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   110052 |   539 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   540 | `				/* Unset the local variable */` |
|    93784 |   541 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    46893 |   542 | `			}` |
|        - |   543 | `			/* Remove local reference */` |
|    16270 |   544 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   110114 |   545 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    93846 |   546 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    46924 |   547 | `			}` |
|     8134 |   548 | `		}` |
|        - |   549 | `		/* Release internal containers */` |
|    16490 |   550 | `		SyHashRelease(&pCurFrame->hVar);` |
|    16490 |   551 | `		SySetRelease(&pCurFrame->sArg);` |
|    16490 |   552 | `		SySetRelease(&pCurFrame->sLocal);` |
|    16490 |   553 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   554 | `		/* Release the whole structure */` |
|    16490 |   555 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     8244 |   556 | `	}` |
|    16490 |   557 |  |
|        - |   558 | `/*` |
|        - |   559 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   560 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   561 | ` * should be skipped when looking for the real execution context.` |
|        - |   562 | ` */` |
|  6786532 |   563 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   564 |  |
|  6787762 |   565 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     1230 |   566 | `		pFrame = pFrame->pParent;` |
|        2 |   567 | `	}` |
|  6786534 |   568 | `	return pFrame;` |
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
|   154726 |   688 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   689 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   690 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   691 | `	)` |
|        2 |   692 |  |
|        - |   693 | `	ph7_class_method *pMeth;` |
|        - |   694 | `	ph7_class_attr *pAttr;` |
|        - |   695 | `	SyHashEntry *pEntry;` |
|        - |   696 | `	sxi32 rc;` |
|        - |   697 | `	/* Reset the loop cursor */` |
|   154728 |   698 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   699 | `	/* Process only static and constant attribute */` |
|   606053 |   700 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   701 | `		/* Extract the current attribute */` |
|   373964 |   702 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   373964 |   703 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   704 | `			ph7_value *pMemObj;` |
|        - |   705 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1688 |   706 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1688 |   707 | `			if( pMemObj == 0 ){` |
|      ! 0 |   708 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   709 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   710 | `					&pClass->sName,&pAttr->sName` |
|        - |   711 | `					);` |
|      ! 0 |   712 | `				return SXERR_MEM;` |
|        - |   713 | `			}` |
|     1688 |   714 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   715 | `				/* Initialize attribute default value (any complex expression) */` |
|     1684 |   716 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      841 |   717 | `			}` |
|        - |   718 | `			/* Record attribute index */` |
|     1688 |   719 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   720 | `			/* Install static attribute in the reference table */` |
|     1688 |   721 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   722 | `			/* If this is a typed static property, register the slot so the` |
|        - |   723 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   724 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   725 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1688 |   726 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       10 |   727 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       10 |   728 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   729 | `					return SXERR_MEM;` |
|        - |   730 | `				}` |
|       10 |   731 | `				pVmAttrS->pAttr = pAttr;` |
|       10 |   732 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       10 |   733 | `				pVmAttrS->iState = 0;` |
|       10 |   734 | `				pVmAttrS->pOwner = pClass;` |
|        - |   735 | `				/* Static typed property with no default starts uninitialized */` |
|        8 |   736 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        8 |   737 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   738 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   739 | `				}` |
|       10 |   740 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   741 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   742 | `					return SXERR_MEM;` |
|        - |   743 | `				}` |
|        4 |   744 | `			}` |
|      843 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Install class methods */` |
|   154728 |   748 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   749 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   750 | `		 */` |
|    76628 |   751 | `		return SXRET_OK;` |
|        - |   752 | `	}` |
|        - |   753 | `	/* Create constructor alias if not yet done */` |
|    78102 |   754 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   755 | `		/* User constructor with the same base class name */` |
|     6078 |   756 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6078 |   757 | `		if( pEntry ){` |
|      ! 0 |   758 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   759 | `			/* Create the alias */` |
|      ! 0 |   760 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   761 | `		}` |
|     3038 |   762 | `	}` |
|        - |   763 | `	/* Install the methods now */` |
|    78102 |   764 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   781788 |   765 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   664638 |   766 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   664638 |   767 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   664630 |   768 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   664630 |   769 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   770 | `				return rc;` |
|        - |   771 | `			}` |
|   332314 |   772 | `		}` |
|        2 |   773 | `	}` |
|        - |   774 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    78102 |   775 | `	pClass->bMounted = TRUE;` |
|    78102 |   776 | `	return SXRET_OK;` |
|    77365 |   777 |  |
|        - |   778 | `/*` |
|        - |   779 | ` * Allocate a private frame for attributes of the given` |
|        - |   780 | ` * class instance (Object in the PHP jargon).` |
|        - |   781 | ` */` |
|     1700 |   782 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   783 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   784 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   785 | `	)` |
|        2 |   786 |  |
|     1702 |   787 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   788 | `	ph7_class_attr *pAttr;` |
|        - |   789 | `	SyHashEntry *pEntry;` |
|        - |   790 | `	sxi32 rc;` |
|        - |   791 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1702 |   792 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     7000 |   793 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   794 | `		VmClassAttr *pVmAttr;` |
|        - |   795 | `		/* Extract the current attribute */` |
|     5300 |   796 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     5300 |   797 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     5300 |   798 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   799 | `			return SXERR_MEM;` |
|        - |   800 | `		}` |
|     5300 |   801 | `		pVmAttr->pAttr = pAttr;` |
|     5300 |   802 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   803 | `			ph7_value *pMemObj;` |
|        - |   804 | `			/* Reserve a memory object for this attribute */` |
|     5276 |   805 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     5276 |   806 | `			if( pMemObj == 0 ){` |
|      ! 0 |   807 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   808 | `				return SXERR_MEM;` |
|        - |   809 | `			}` |
|     5276 |   810 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     5276 |   811 | `			pVmAttr->iState = 0;` |
|     5276 |   812 | `			pVmAttr->pOwner = pClass;` |
|     5276 |   813 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   814 | `				/* Initialize attribute default value (any complex expression) */` |
|     1806 |   815 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     4374 |   816 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   817 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   818 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       64 |   819 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       31 |   820 | `			}` |
|     5276 |   821 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     5276 |   822 | `			if( rc != SXRET_OK ){` |
|        - |   823 | `				VmSlot sSlot;` |
|        - |   824 | `				/* Restore memory object */` |
|      ! 0 |   825 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   826 | `				sSlot.pUserData = 0;` |
|      ! 0 |   827 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|        - |   831 | `			/* Install attribute in the reference table */` |
|     5276 |   832 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   833 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   834 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   835 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     5276 |   836 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      156 |   837 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      156 |   838 | `				if( rc != SXRET_OK ){` |
|        - |   839 | `					VmSlot sSlot;` |
|      ! 0 |   840 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   841 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   842 | `					sSlot.pUserData = 0;` |
|      ! 0 |   843 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   844 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   845 | `					return SXERR_MEM;` |
|        - |   846 | `				}` |
|       77 |   847 | `			}` |
|     2639 |   848 | `		}else{` |
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
|     1702 |   860 | `	return SXRET_OK;` |
|      852 |   861 |  |
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
|   421248 |   873 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   874 |  |
|        - |   875 | `	ph7_value *pObj;` |
|        - |   876 | `	sxi32 rc;` |
|   421250 |   877 | `	if( pIndex ){` |
|        - |   878 | `		/* Object index in the object table */` |
|   412496 |   879 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   206247 |   880 | `	}` |
|        - |   881 | `	/* Reserve a slot for the new object */` |
|   421250 |   882 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   421250 |   883 | `	if( rc != SXRET_OK ){` |
|        - |   884 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   885 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   886 | `		 */` |
|      ! 0 |   887 | `		return 0;` |
|        - |   888 | `	}` |
|   421250 |   889 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   421250 |   890 | `	return pObj;` |
|   210626 |   891 |  |
|        - |   892 | `/*` |
|        - |   893 | ` * Reserve a memory object.` |
|        - |   894 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   895 | ` */` |
|  2147620 |   896 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   897 |  |
|        - |   898 | `	ph7_value *pObj;` |
|        - |   899 | `	sxi32 rc;` |
|  2147622 |   900 | `	if( pIndex ){` |
|        - |   901 | `		/* Object index in the object table */` |
|  2147622 |   902 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1073810 |   903 | `	}` |
|        - |   904 | `	/* Reserve a slot for the new object */` |
|  2147622 |   905 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2147622 |   906 | `	if( rc != SXRET_OK ){` |
|        - |   907 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   908 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   909 | `		 */` |
|      ! 0 |   910 | `		return 0;` |
|        - |   911 | `	}` |
|  2147622 |   912 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2147622 |   913 | `	return pObj;` |
|  1073812 |   914 |  |
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
|        - |   933 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |   934 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |   935 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |   936 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |   937 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |   938 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |   939 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   940 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   941 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   942 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   943 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   944 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   945 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   946 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   947 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |   948 | `/*` |
|        - |   949 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   950 | ` * directly as foreign functions.` |
|        - |   951 | ` */` |
|        - |   952 | `#define PH7_BUILTIN_LIB \` |
|        - |   953 | `	"interface Throwable {"\` |
|        - |   954 | `	"public function getMessage();"\` |
|        - |   955 | `	"public function getCode();"\` |
|        - |   956 | `	"public function getFile();"\` |
|        - |   957 | `	"public function getLine();"\` |
|        - |   958 | `	"public function getTrace();"\` |
|        - |   959 | `	"public function getTraceAsString();"\` |
|        - |   960 | `	"public function getPrevious();"\` |
|        - |   961 | `	"public function __toString();"\` |
|        - |   962 | `	"}"\` |
|        - |   963 | `	"class Exception implements Throwable { "\` |
|        - |   964 | `    "protected $message = '';"\` |
|        - |   965 | `    "protected $code = 0;"\` |
|        - |   966 | `    "protected $file;"\` |
|        - |   967 | `    "protected $line;"\` |
|        - |   968 | `    "protected $trace;"\` |
|        - |   969 | `    "protected $previous;"\` |
|        - |   970 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |   971 | `	"   if( isset($message) ){"\` |
|        - |   972 | `	"	  $this->message = $message;"\` |
|        - |   973 | `	"   }"\` |
|        - |   974 | `	"   $this->code = $code;"\` |
|        - |   975 | `	"   $this->file = __FILE__;"\` |
|        - |   976 | `	"   $this->line = __LINE__;"\` |
|        - |   977 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   978 | `	"   if( isset($previous) ){"\` |
|        - |   979 | `	"     $this->previous = $previous;"\` |
|        - |   980 | `	"   }"\` |
|        - |   981 | `	"}"\` |
|        - |   982 | `	"public function getMessage(){"\` |
|        - |   983 | `	"   return $this->message;"\` |
|        - |   984 | `	"}"\` |
|        - |   985 | `	" public function getCode(){"\` |
|        - |   986 | `	"  return $this->code;"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"public function getFile(){"\` |
|        - |   989 | `	"  return $this->file;"\` |
|        - |   990 | `	"}"\` |
|        - |   991 | `	"public function getLine(){"\` |
|        - |   992 | `	"  return $this->line;"\` |
|        - |   993 | `	"}"\` |
|        - |   994 | `	"public function getTrace(){"\` |
|        - |   995 | `	"   return $this->trace;"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"public function getTraceAsString(){"\` |
|        - |   998 | `	"  return debug_string_backtrace();"\` |
|        - |   999 | `	"}"\` |
|        - |  1000 | `	"public function getPrevious(){"\` |
|        - |  1001 | `	"    return $this->previous;"\` |
|        - |  1002 | `	"}"\` |
|        - |  1003 | `	"public function __toString(){"\` |
|        - |  1004 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1005 | `    "}"\` |
|        - |  1006 | `	"}"\` |
|        - |  1007 | `	"class Error implements Throwable { "\` |
|        - |  1008 | `    "protected $message = '';"\` |
|        - |  1009 | `    "protected $code = 0;"\` |
|        - |  1010 | `    "protected $file;"\` |
|        - |  1011 | `    "protected $line;"\` |
|        - |  1012 | `    "protected $trace;"\` |
|        - |  1013 | `    "protected $previous;"\` |
|        - |  1014 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1015 | `	"   if( isset($message) ){"\` |
|        - |  1016 | `	"	  $this->message = $message;"\` |
|        - |  1017 | `	"   }"\` |
|        - |  1018 | `	"   $this->code = $code;"\` |
|        - |  1019 | `	"   $this->file = __FILE__;"\` |
|        - |  1020 | `	"   $this->line = __LINE__;"\` |
|        - |  1021 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1022 | `	"   if( isset($previous) ){"\` |
|        - |  1023 | `	"     $this->previous = $previous;"\` |
|        - |  1024 | `	"   }"\` |
|        - |  1025 | `	"}"\` |
|        - |  1026 | `	"public function getMessage(){"\` |
|        - |  1027 | `	"   return $this->message;"\` |
|        - |  1028 | `	"}"\` |
|        - |  1029 | `	"public function getCode(){"\` |
|        - |  1030 | `	"  return $this->code;"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"public function getFile(){"\` |
|        - |  1033 | `	"  return $this->file;"\` |
|        - |  1034 | `	"}"\` |
|        - |  1035 | `	"public function getLine(){"\` |
|        - |  1036 | `	"  return $this->line;"\` |
|        - |  1037 | `	"}"\` |
|        - |  1038 | `	"public function getTrace(){"\` |
|        - |  1039 | `	"   return $this->trace;"\` |
|        - |  1040 | `	"}"\` |
|        - |  1041 | `	"public function getTraceAsString(){"\` |
|        - |  1042 | `	"  return debug_string_backtrace();"\` |
|        - |  1043 | `	"}"\` |
|        - |  1044 | `	"public function getPrevious(){"\` |
|        - |  1045 | `	"    return $this->previous;"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"public function __toString(){"\` |
|        - |  1048 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1049 | `	"}"\` |
|        - |  1050 | `	"}"\` |
|        - |  1051 | `	"class TypeError extends Error { }"\` |
|        - |  1052 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1053 | `	"class ValueError extends Error { }"\` |
|        - |  1054 | `	"class FiberError extends Error { }"\` |
|        - |  1055 | `	"class AssertionError extends Error { }"\` |
|        - |  1056 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1057 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1058 | `	"class ErrorException extends Exception { "\` |
|        - |  1059 | `	"protected $severity;"\` |
|        - |  1060 | `	"public function __construct(string $message = null,"\` |
|        - |  1061 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1062 | `	"   if( isset($message) ){"\` |
|        - |  1063 | `	"	  $this->message = $message;"\` |
|        - |  1064 | `	"   }"\` |
|        - |  1065 | `	"   $this->severity = $severity;"\` |
|        - |  1066 | `	"   $this->code = $code;"\` |
|        - |  1067 | `	"   $this->file = $filename;"\` |
|        - |  1068 | `	"   $this->line = $lineno;"\` |
|        - |  1069 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1070 | `	"   if( isset($previous) ){"\` |
|        - |  1071 | `	"     $this->previous = $previous;"\` |
|        - |  1072 | `	"   }"\` |
|        - |  1073 | `	"}"\` |
|        - |  1074 | `	"public function getSeverity(){"\` |
|        - |  1075 | `	"   return $this->severity;"\` |
|        - |  1076 | `    "}"\` |
|        - |  1077 | `	"}"\` |
|        - |  1078 | `	"interface Iterator {"\` |
|        - |  1079 | `	"public function current();"\` |
|        - |  1080 | `	"public function key();"\` |
|        - |  1081 | `	"public function next();"\` |
|        - |  1082 | `	"public function rewind();"\` |
|        - |  1083 | `	"public function valid();"\` |
|        - |  1084 | `	"}"\` |
|        - |  1085 | `	"interface IteratorAggregate {"\` |
|        - |  1086 | `	"public function getIterator();"\` |
|        - |  1087 | `	"}"\` |
|        - |  1088 | `	"interface Serializable {"\` |
|        - |  1089 | `	"public function serialize();"\` |
|        - |  1090 | `	"public function unserialize(string $serialized);"\` |
|        - |  1091 | `	"}"\` |
|        - |  1092 | `	"/* Directory releated IO */"\` |
|        - |  1093 | `	"class Directory {"\` |
|        - |  1094 | `	"public $handle = null;"\` |
|        - |  1095 | `	"public $path  = null;"\` |
|        - |  1096 | `	"public function __construct(string $path)"\` |
|        - |  1097 | `	"{"\` |
|        - |  1098 | `	"   $this->handle = opendir($path);"\` |
|        - |  1099 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1100 | `	"      $this->path = $path;"\` |
|        - |  1101 | `	"   }"\` |
|        - |  1102 | `	"}"\` |
|        - |  1103 | `	"public function __destruct()"\` |
|        - |  1104 | `	"{"\` |
|        - |  1105 | `	"  if( $this->handle != null ){"\` |
|        - |  1106 | `	"       closedir($this->handle);"\` |
|        - |  1107 | `	"  }"\` |
|        - |  1108 | `	"}"\` |
|        - |  1109 | `	"public function read()"\` |
|        - |  1110 | `	"{"\` |
|        - |  1111 | `	"    return readdir($this->handle);"\` |
|        - |  1112 | `	"}"\` |
|        - |  1113 | `	"public function rewind()"\` |
|        - |  1114 | `	"{"\` |
|        - |  1115 | `	"    rewinddir($this->handle);"\` |
|        - |  1116 | `	"}"\` |
|        - |  1117 | `	"public function close()"\` |
|        - |  1118 | `	"{"\` |
|        - |  1119 | `	"    closedir($this->handle);"\` |
|        - |  1120 | `	"    $this->handle = null;"\` |
|        - |  1121 | `	"}"\` |
|        - |  1122 | `	"}"\` |
|        - |  1123 | `	"class Fiber {"\` |
|        - |  1124 | `	"  private $__ctx;"\` |
|        - |  1125 | `	"  private $__callable;"\` |
|        - |  1126 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1127 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1128 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1129 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1130 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1131 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1132 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1133 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1134 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1135 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1136 | `	"}"\` |
|        - |  1137 | `	"class Generator implements Iterator {"\` |
|        - |  1138 | `	"  private $__ctx;"\` |
|        - |  1139 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1140 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1141 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1142 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1143 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1144 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1145 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1146 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1147 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1148 | `	"}"\` |
|        - |  1149 | `	"class stdClass{"\` |
|        - |  1150 | `	"  public $value;"\` |
|        - |  1151 | `	" /* Magic methods */"\` |
|        - |  1152 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1153 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1154 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1155 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1156 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1157 | `	"}"\` |
|        - |  1158 | `	"function dir(string $path){"\` |
|        - |  1159 | `	"   return new Directory($path);"\` |
|        - |  1160 | `	"}"\` |
|        - |  1161 | `	"function Dir(string $path){"\` |
|        - |  1162 | `	"   return new Directory($path);"\` |
|        - |  1163 | `	"}"\` |
|        - |  1164 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1165 | `    "{"\` |
|        - |  1166 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1167 | `	"  $aDir = array();"\` |
|        - |  1168 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1169 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1170 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1171 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1172 | `	"   }"\` |
|        - |  1173 | `	"  closedir($pHandle);"\` |
|        - |  1174 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1175 | `	"      rsort($aDir);"\` |
|        - |  1176 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1177 | `	"      sort($aDir);"\` |
|        - |  1178 | `	"  }"\` |
|        - |  1179 | `	"  return $aDir;"\` |
|        - |  1180 | `	"}"\` |
|        - |  1181 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1182 | `	"/* Open the target directory */"\` |
|        - |  1183 | `	"$zDir = dirname($pattern);"\` |
|        - |  1184 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1185 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1186 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1187 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1188 | `	"	return FALSE;"\` |
|        - |  1189 | `	"}"\` |
|        - |  1190 | `	"$pattern = basename($pattern);"\` |
|        - |  1191 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1192 | `	"/* Loop throw available entries */"\` |
|        - |  1193 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1194 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1195 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1196 | `	"	if( $rc ){"\` |
|        - |  1197 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1198 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1199 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1200 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1201 | `	"		  }"\` |
|        - |  1202 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1203 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1204 | `	"		 continue;"\` |
|        - |  1205 | `	"	   }"\` |
|        - |  1206 | `	"	   /* Add the entry */"\` |
|        - |  1207 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1208 | `	"	}"\` |
|        - |  1209 | `	" }"\` |
|        - |  1210 | `	"/* Close the handle */"\` |
|        - |  1211 | `	"closedir($pHandle);"\` |
|        - |  1212 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1213 | `	"  /* Sort the array */"\` |
|        - |  1214 | `	"  sort($pArray);"\` |
|        - |  1215 | `	"}"\` |
|        - |  1216 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1217 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1218 | `	"  $pArray[] = $pattern;"\` |
|        - |  1219 | `	"}"\` |
|        - |  1220 | `	"/* Return the created array */"\` |
|        - |  1221 | `	"return $pArray;"\` |
|        - |  1222 | `   "}"\` |
|        - |  1223 | `   "/* Creates a temporary file */"\` |
|        - |  1224 | `   "function tmpfile(){"\` |
|        - |  1225 | `   "  /* Extract the temp directory */"\` |
|        - |  1226 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1227 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1228 | `   "    /* Use the current dir */"\` |
|        - |  1229 | `   "    $zTempDir = '.';"\` |
|        - |  1230 | `   "  }"\` |
|        - |  1231 | `   "  /* Create the file */"\` |
|        - |  1232 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1233 | `   "  return $pHandle;"\` |
|        - |  1234 | `   "}"\` |
|        - |  1235 | `   "/* Creates a temporary filename */"\` |
|        - |  1236 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1237 | `   "{"\` |
|        - |  1238 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1239 | `   "}"\` |
|        - |  1240 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1241 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1242 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1243 | `   "/* Copy arguments */"\` |
|        - |  1244 | `   "$nArgs = func_num_args();"\` |
|        - |  1245 | `   "$pNew = array();"\` |
|        - |  1246 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1247 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1248 | `    "}"\` |
|        - |  1249 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1250 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1251 | `	"/* Erase */"\` |
|        - |  1252 | `	"array_erase($pArray);"\` |
|        - |  1253 | `	"/* Unshift */"\` |
|        - |  1254 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1255 | `	"return sizeof($pArray);"\` |
|        - |  1256 | `    "}"\` |
|        - |  1257 | `	"function array_merge_recursive(){"\` |
|        - |  1258 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1259 | `    "$arrays = func_get_args();"\` |
|        - |  1260 | `    "$narrays = count($arrays);"\` |
|        - |  1261 | `    "$ret = array();"\` |
|        - |  1262 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1263 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1264 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1265 | `	 " }"\` |
|        - |  1266 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1267 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1268 | `     "  if( $keyIsInt ) {"\` |
|        - |  1269 | `     "   $ret[] = $value;"\` |
|        - |  1270 | `     "  } else {"\` |
|        - |  1271 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1272 | `     "    $cur = $ret[$key];"\` |
|        - |  1273 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1274 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1275 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1276 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1277 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1278 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1279 | `     "    } else {"\` |
|        - |  1280 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1281 | `     "    }"\` |
|        - |  1282 | `     "   } else {"\` |
|        - |  1283 | `     "    $ret[$key] = $value;"\` |
|        - |  1284 | `     "   }"\` |
|        - |  1285 | `     "  }"\` |
|        - |  1286 | `     " }"\` |
|        - |  1287 | `	 " }"\` |
|        - |  1288 | `	 " return $ret;"\` |
|        - |  1289 | `    "}"\` |
|        - |  1290 | `	"function max(){"\` |
|        - |  1291 | `    "  $pArgs = func_get_args();"\` |
|        - |  1292 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1293 | `	"  return null;"\` |
|        - |  1294 | `    " }"\` |
|        - |  1295 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1296 | `    " $pArg = $pArgs[0];"\` |
|        - |  1297 | `	" if( !is_array($pArg) ){"\` |
|        - |  1298 | `	"   return $pArg; "\` |
|        - |  1299 | `	" }"\` |
|        - |  1300 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1301 | `	"   return null;"\` |
|        - |  1302 | `	" }"\` |
|        - |  1303 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1304 | `	" reset($pArg);"\` |
|        - |  1305 | `	" $max = current($pArg);"\` |
|        - |  1306 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1307 | `	"   if( $val > $max ){"\` |
|        - |  1308 | `	"     $max = $val;"\` |
|        - |  1309 | `    " }"\` |
|        - |  1310 | `	" }"\` |
|        - |  1311 | `	" return $max;"\` |
|        - |  1312 | `    " }"\` |
|        - |  1313 | `    " $max = $pArgs[0];"\` |
|        - |  1314 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1315 | `    " $val = $pArgs[$i];"\` |
|        - |  1316 | `	"if( $val > $max ){"\` |
|        - |  1317 | `	" $max = $val;"\` |
|        - |  1318 | `	"}"\` |
|        - |  1319 | `    " }"\` |
|        - |  1320 | `	" return $max;"\` |
|        - |  1321 | `    "}"\` |
|        - |  1322 | `	"function min(){"\` |
|        - |  1323 | `    "  $pArgs = func_get_args();"\` |
|        - |  1324 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1325 | `	"  return null;"\` |
|        - |  1326 | `    " }"\` |
|        - |  1327 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1328 | `    " $pArg = $pArgs[0];"\` |
|        - |  1329 | `	" if( !is_array($pArg) ){"\` |
|        - |  1330 | `	"   return $pArg; "\` |
|        - |  1331 | `	" }"\` |
|        - |  1332 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1333 | `	"   return null;"\` |
|        - |  1334 | `	" }"\` |
|        - |  1335 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1336 | `	" reset($pArg);"\` |
|        - |  1337 | `	" $min = current($pArg);"\` |
|        - |  1338 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1339 | `	"   if( $val < $min ){"\` |
|        - |  1340 | `	"     $min = $val;"\` |
|        - |  1341 | `    " }"\` |
|        - |  1342 | `	" }"\` |
|        - |  1343 | `	" return $min;"\` |
|        - |  1344 | `    " }"\` |
|        - |  1345 | `    " $min = $pArgs[0];"\` |
|        - |  1346 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1347 | `    " $val = $pArgs[$i];"\` |
|        - |  1348 | `	"if( $val < $min ){"\` |
|        - |  1349 | `	" $min = $val;"\` |
|        - |  1350 | `	" }"\` |
|        - |  1351 | `    " }"\` |
|        - |  1352 | `	" return $min;"\` |
|        - |  1353 | `	"}"\` |
|        - |  1354 | `	"function fileowner(string $file){"\` |
|        - |  1355 | `    " $a = stat($file);"\` |
|        - |  1356 | `	" if( !is_array($a) ){"\` |
|        - |  1357 | `	"	return false;"\` |
|        - |  1358 | `	" }"\` |
|        - |  1359 | `	" return $a['uid'];"\` |
|        - |  1360 | `    "}"\` |
|        - |  1361 | `    "function filegroup(string $file){"\` |
|        - |  1362 | `	" $a = stat($file);"\` |
|        - |  1363 | `	" if( !is_array($a) ){"\` |
|        - |  1364 | `	"	return false;"\` |
|        - |  1365 | `	" }"\` |
|        - |  1366 | `	" return $a['gid'];"\` |
|        - |  1367 | `    "}"\` |
|        - |  1368 | `	 "function fileinode(string $file){"\` |
|        - |  1369 | `	" $a = stat($file);"\` |
|        - |  1370 | `	" if( !is_array($a) ){"\` |
|        - |  1371 | `	"	return false;"\` |
|        - |  1372 | `	" }"\` |
|        - |  1373 | `	" return $a['ino'];"\` |
|        - |  1374 | `    "}"` |
|        - |  1375 |  |
|        - |  1376 | `/*` |
|        - |  1377 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1378 | ` * start compiling the target PHP program.` |
|        - |  1379 | ` */` |
|     2918 |  1380 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1381 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1382 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1383 | `	 )` |
|        2 |  1384 |  |
|        - |  1385 | `	SyString sBuiltin;` |
|        - |  1386 | `	ph7_value *pObj;` |
|        - |  1387 | `	sxi32 rc;` |
|        - |  1388 | `	/* Zero the structure */` |
|     2920 |  1389 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1390 | `	/* Initialize VM fields */` |
|     2920 |  1391 | `	pVm->pEngine = &(*pEngine);` |
|     2920 |  1392 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1393 | `	/* Instructions containers */` |
|     2920 |  1394 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2920 |  1395 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2920 |  1396 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1397 | `	/* Object containers */` |
|     2920 |  1398 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2920 |  1399 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1400 | `	/* Virtual machine internal containers */` |
|     2920 |  1401 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2920 |  1402 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2920 |  1403 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2920 |  1404 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2920 |  1405 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2920 |  1406 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2920 |  1407 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2920 |  1408 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2920 |  1409 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2920 |  1410 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2920 |  1411 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2920 |  1412 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2920 |  1413 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2920 |  1414 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2920 |  1415 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2920 |  1416 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2920 |  1417 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2920 |  1418 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2920 |  1419 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2920 |  1420 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2920 |  1421 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2920 |  1422 | `	pVm->pPendingException = 0;` |
|        - |  1423 | `	/* Configuration containers */` |
|     2920 |  1424 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2920 |  1425 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2920 |  1426 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2920 |  1427 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2920 |  1428 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2920 |  1429 | `	pVm->iResponseStatus = 200;` |
|     2920 |  1430 | `	pVm->bHeadersSent = 0;` |
|     2920 |  1431 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1432 | `	/* Error callbacks containers */` |
|     2920 |  1433 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2920 |  1434 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2920 |  1435 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2920 |  1436 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2920 |  1437 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1438 | `	/* Set a default recursion limit */` |
|        - |  1439 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2920 |  1440 | `	pVm->nMaxDepth = 32;` |
|        - |  1441 | `#else` |
|        - |  1442 | `	pVm->nMaxDepth = 16;` |
|        - |  1443 | `#endif` |
|        - |  1444 | `	/* Default assertion flags */` |
|     2920 |  1445 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1446 | `	/* JSON return status */` |
|     2920 |  1447 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1448 | `	/* PRNG context */` |
|     2920 |  1449 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1450 | `	/* Install the null constant */` |
|     2920 |  1451 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2920 |  1452 | `	if( pObj == 0 ){` |
|      ! 0 |  1453 | `		rc = SXERR_MEM;` |
|      ! 0 |  1454 | `		goto Err;` |
|        - |  1455 | `	}` |
|     2920 |  1456 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1457 | `	/* Install the boolean TRUE constant */` |
|     2920 |  1458 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2920 |  1459 | `	if( pObj == 0 ){` |
|      ! 0 |  1460 | `		rc = SXERR_MEM;` |
|      ! 0 |  1461 | `		goto Err;` |
|        - |  1462 | `	}` |
|     2920 |  1463 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1464 | `	/* Install the boolean FALSE constant */` |
|     2920 |  1465 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2920 |  1466 | `	if( pObj == 0 ){` |
|      ! 0 |  1467 | `		rc = SXERR_MEM;` |
|      ! 0 |  1468 | `		goto Err;` |
|        - |  1469 | `	}` |
|     2920 |  1470 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1471 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1472 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1473 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2920 |  1474 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2920 |  1475 | `	if( pObj == 0 ){` |
|      ! 0 |  1476 | `		rc = SXERR_MEM;` |
|      ! 0 |  1477 | `		goto Err;` |
|        - |  1478 | `	}` |
|     2920 |  1479 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1480 | `	/* Create the global frame */` |
|     2920 |  1481 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2920 |  1482 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1483 | `		goto Err;` |
|        - |  1484 | `	}` |
|        - |  1485 | `	/* Initialize the code generator */` |
|     2920 |  1486 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2920 |  1487 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1488 | `		goto Err;` |
|        - |  1489 | `	}` |
|        - |  1490 | `	/* VM correctly initialized,set the magic number */` |
|     2920 |  1491 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2920 |  1492 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1493 | `	/* Compile the built-in library */` |
|     2920 |  1494 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1495 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2920 |  1496 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1497 | `	/* Register Fiber internal C functions */` |
|     2920 |  1498 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2920 |  1499 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2920 |  1500 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2920 |  1501 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2920 |  1502 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2920 |  1503 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2920 |  1504 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2920 |  1505 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2920 |  1506 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2920 |  1507 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1508 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2920 |  1509 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2920 |  1510 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2920 |  1511 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2920 |  1512 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2920 |  1513 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2920 |  1514 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2920 |  1515 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2920 |  1516 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2920 |  1517 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2920 |  1518 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1519 | `	/* Reset the code generator */` |
|     2920 |  1520 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2920 |  1521 | `	return SXRET_OK;` |
|      ! 0 |  1522 | `Err:` |
|      ! 0 |  1523 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1524 | `	return rc;` |
|     1461 |  1525 |  |
|        - |  1526 | `/*` |
|        - |  1527 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1528 | ` * routine which store the output in an internal blob.` |
|        - |  1529 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1530 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1531 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1532 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1533 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1534 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1535 | ` * to finish executing and extracting the output.` |
|        - |  1536 | ` */` |
|       38 |  1537 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1538 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1539 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1540 | `	void *pUserData     /* User private data */` |
|        - |  1541 | `	)` |
|      ! 0 |  1542 |  |
|        - |  1543 | `	 sxi32 rc;` |
|        - |  1544 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1545 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1546 | `	 return rc;` |
|      ! 0 |  1547 |  |
|        - |  1548 | `/*` |
|        - |  1549 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1550 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1551 | ` */` |
|    16926 |  1552 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1553 |  |
|    16928 |  1554 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    16928 |  1555 | `	if( xCons != VmObConsumer ){` |
|     7150 |  1556 | `		pVm->nOutputLen += nLen;` |
|     7150 |  1557 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      912 |  1558 | `			pVm->bHeadersSent = 1;` |
|      455 |  1559 | `		}` |
|     3574 |  1560 | `	}` |
|    16928 |  1561 |  |
|        - |  1562 | `#define VM_STACK_GUARD 16` |
|        - |  1563 | `/*` |
|        - |  1564 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1565 | ` * our compiled PHP program.` |
|        - |  1566 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1567 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1568 | ` */` |
|    39506 |  1569 | `static ph7_value * VmNewOperandStack(` |
|        - |  1570 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1571 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1572 | `	)` |
|        2 |  1573 |  |
|        - |  1574 | `	ph7_value *pStack;` |
|        - |  1575 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1576 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1577 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1578 | `  ** on the maximum stack depth required.` |
|        - |  1579 | `  **` |
|        - |  1580 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1581 | `  */` |
|    39508 |  1582 | `	nInstr += VM_STACK_GUARD;` |
|    39508 |  1583 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    39508 |  1584 | `	if( pStack == 0 ){` |
|      ! 0 |  1585 | `		return 0;` |
|        - |  1586 | `	}` |
|        - |  1587 | `	/* Initialize the operand stack */` |
|  2634436 |  1588 | `	while( nInstr > 0 ){` |
|  2594930 |  1589 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2594930 |  1590 | `		--nInstr;` |
|        2 |  1591 | `	}` |
|        - |  1592 | `	/* Ready for bytecode execution */` |
|    39508 |  1593 | `	return pStack;` |
|    19755 |  1594 |  |
|        - |  1595 | `/* Forward declaration */` |
|        - |  1596 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1597 | `/*` |
|        - |  1598 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1599 | ` * This routine gets called by the PH7 engine after` |
|        - |  1600 | ` * successful compilation of the target PHP program.` |
|        - |  1601 | ` */` |
|     2612 |  1602 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1603 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1604 | `	)` |
|        2 |  1605 |  |
|        - |  1606 | `	SyHashEntry *pEntry;` |
|        - |  1607 | `	sxi32 rc;` |
|     2614 |  1608 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1609 | `		/* Initialize your VM first */` |
|      ! 0 |  1610 | `		return SXERR_CORRUPT;` |
|        - |  1611 | `	}` |
|        - |  1612 | `	/* Mark the VM ready for byte-code execution */` |
|     2614 |  1613 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1614 | `	/* Release the code generator now we have compiled our program */` |
|     2614 |  1615 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1616 | `	/* Emit the DONE instruction */` |
|     2614 |  1617 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2614 |  1618 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1619 | `		return SXERR_MEM;` |
|        - |  1620 | `	}` |
|        - |  1621 | `	/* Script return value */` |
|     2614 |  1622 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1623 | `	/* Allocate a new operand stack */` |
|     2614 |  1624 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2614 |  1625 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1626 | `		return SXERR_MEM;` |
|        - |  1627 | `	}` |
|        - |  1628 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1629 | `	 * private data. */` |
|     2614 |  1630 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2614 |  1631 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1632 | `	/* Allocate the reference table */` |
|     2614 |  1633 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2614 |  1634 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2614 |  1635 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1636 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1637 | `		return SXERR_MEM;` |
|        - |  1638 | `	}` |
|        - |  1639 | `	/* Zero the reference table */` |
|     2614 |  1640 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1641 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2614 |  1642 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2614 |  1643 | `	if( rc != SXRET_OK ){` |
|        - |  1644 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1645 | `		return rc;` |
|        - |  1646 | `	}` |
|        - |  1647 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2614 |  1648 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2614 |  1649 | `	if( rc != SXRET_OK ){` |
|        - |  1650 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1651 | `		return rc;` |
|        - |  1652 | `	}` |
|        - |  1653 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2614 |  1654 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1655 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2614 |  1656 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1657 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2614 |  1658 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1659 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1660 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2614 |  1661 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2614 |  1662 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1663 | `#endif` |
|        - |  1664 | `	/* Initialize and install static and constants class attributes */` |
|     2614 |  1665 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    49892 |  1666 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    47280 |  1667 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    47280 |  1668 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1669 | `			return rc;` |
|        - |  1670 | `		}` |
|        2 |  1671 | `	}` |
|        - |  1672 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2614 |  1673 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1674 | `	/* VM is ready for bytecode execution */` |
|     2614 |  1675 | `	return SXRET_OK;` |
|     1308 |  1676 |  |
|        - |  1677 | `/*` |
|        - |  1678 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1679 | ` */` |
|      ! 0 |  1680 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1681 |  |
|      ! 0 |  1682 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1683 | `		return SXERR_CORRUPT;` |
|        - |  1684 | `	}` |
|        - |  1685 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1686 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1687 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1688 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1689 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1690 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1691 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1692 | `	pVm->bHttpContext = 0;` |
|        - |  1693 | `	/* Set the ready flag */` |
|      ! 0 |  1694 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1695 | `	return SXRET_OK;` |
|      ! 0 |  1696 |  |
|        - |  1697 | `/*` |
|        - |  1698 | ` * Release a Virtual Machine.` |
|        - |  1699 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1700 | ` */` |
|     2604 |  1701 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1702 |  |
|        - |  1703 | `	/* Set the stale magic number */` |
|     2606 |  1704 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1705 | `	/* Release the private memory subsystem */` |
|     2606 |  1706 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2606 |  1707 | `	return SXRET_OK;` |
|        2 |  1708 |  |
|        - |  1709 | `/*` |
|        - |  1710 | ` * Initialize a foreign function call context.` |
|        - |  1711 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1712 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1713 | ` * functions.` |
|        - |  1714 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1715 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1716 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1717 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1718 | ` */` |
|   645688 |  1719 | `static sxi32 VmInitCallContext(` |
|        - |  1720 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1721 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1722 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1723 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1724 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1725 | `	)` |
|        2 |  1726 |  |
|   645690 |  1727 | `	pOut->pFunc = pFunc;` |
|   645690 |  1728 | `	pOut->pVm   = pVm;` |
|   645690 |  1729 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   645690 |  1730 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1731 | `	/* Assume a null return value */` |
|   645690 |  1732 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   645690 |  1733 | `	pOut->pRet = pRet;` |
|   645690 |  1734 | `	pOut->iFlags = iFlags;` |
|   645690 |  1735 | `	return SXRET_OK;` |
|        2 |  1736 |  |
|        - |  1737 | `/*` |
|        - |  1738 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1739 | ` * left behind.` |
|        - |  1740 | ` */` |
|   645688 |  1741 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1742 |  |
|        - |  1743 | `	sxu32 n;` |
|   645690 |  1744 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7874 |  1745 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    22788 |  1746 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    14916 |  1747 | `			if( apObj[n] == 0 ){` |
|        - |  1748 | `				/* Already released */` |
|      298 |  1749 | `				continue;` |
|        - |  1750 | `			}` |
|    14620 |  1751 | `			PH7_MemObjRelease(apObj[n]);` |
|    14620 |  1752 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7311 |  1753 | `		}` |
|     7874 |  1754 | `		SySetRelease(&pCtx->sVar);` |
|     3936 |  1755 | `	}` |
|   645690 |  1756 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1757 | `		ph7_aux_data *aAux;` |
|        - |  1758 | `		void *pChunk;` |
|        - |  1759 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1760 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1761 | `		 */` |
|        9 |  1762 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1763 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1764 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1765 | `			/* Release the chunk */` |
|       25 |  1766 | `			if( pChunk ){` |
|       25 |  1767 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1768 | `			}` |
|       13 |  1769 | `		}` |
|        9 |  1770 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1771 | `	}` |
|   645690 |  1772 |  |
|        - |  1773 | `/*` |
|        - |  1774 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1775 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1776 | ` */` |
|      296 |  1777 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1778 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1779 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1780 | `	)` |
|        2 |  1781 |  |
|      298 |  1782 | `	if( pValue == 0 ){` |
|        - |  1783 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1784 | `		return;` |
|        - |  1785 | `	}` |
|      298 |  1786 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1787 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1788 | `		sxu32 n;` |
|     1054 |  1789 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1790 | `			if( apObj[n] == pValue ){` |
|      298 |  1791 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1792 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1793 | `				/* Mark as released */` |
|      298 |  1794 | `				apObj[n] = 0;` |
|      298 |  1795 | `				break;` |
|        - |  1796 | `			}` |
|      380 |  1797 | `		}` |
|      148 |  1798 | `	}` |
|      150 |  1799 |  |
|        - |  1800 | `/*` |
|        - |  1801 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1802 | ` */` |
|  3701000 |  1803 | `static void VmPopOperand(` |
|        - |  1804 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1805 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1806 | `	)` |
|        2 |  1807 |  |
|  3701002 |  1808 | `	ph7_value *pTos = *ppTos;` |
|  7876310 |  1809 | `	while( nPop > 0 ){` |
|  4175310 |  1810 | `		PH7_MemObjRelease(pTos);` |
|  4175310 |  1811 | `		pTos--;` |
|  4175310 |  1812 | `		nPop--;` |
|        2 |  1813 | `	}` |
|        - |  1814 | `	/* Top of the stack */` |
|  3701002 |  1815 | `	*ppTos = pTos;` |
|  3701002 |  1816 |  |
|        - |  1817 | `/*` |
|        - |  1818 | ` * Reserve a memory object.` |
|        - |  1819 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1820 | ` */` |
|  3120898 |  1821 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1822 |  |
|  3120900 |  1823 | `	ph7_value *pObj = 0;` |
|        - |  1824 | `	VmSlot *pSlot;` |
|        - |  1825 | `	sxu32 nIdx;` |
|        - |  1826 | `	/* Check for a free slot */` |
|  3120900 |  1827 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3120900 |  1828 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3120900 |  1829 | `	if( pSlot ){` |
|   973280 |  1830 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   973280 |  1831 | `		nIdx = pSlot->nIdx;` |
|   486639 |  1832 | `	}` |
|  3120900 |  1833 | `	if( pObj == 0 ){` |
|        - |  1834 | `		/* Reserve a new memory object */` |
|  2147622 |  1835 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2147622 |  1836 | `		if( pObj == 0 ){` |
|      ! 0 |  1837 | `			return 0;` |
|        - |  1838 | `		}` |
|  1073810 |  1839 | `	}` |
|        - |  1840 | `	/* Set a null default value */` |
|  3120900 |  1841 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3120900 |  1842 | `	pObj->nIdx = nIdx;` |
|  3120900 |  1843 | `	return pObj;` |
|  1560451 |  1844 |  |
|        - |  1845 | `/*` |
|        - |  1846 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1847 | ` */` |
|    33588 |  1848 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1849 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1850 | `	const char *zKey,  /* Entry key */` |
|        - |  1851 | `	sxu32 nByte,       /* Key length */` |
|        - |  1852 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1853 | `	)` |
|        2 |  1854 |  |
|        - |  1855 | `	ph7_value sKey;` |
|        - |  1856 | `	sxi32 rc;` |
|    33590 |  1857 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    33590 |  1858 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1859 | `	/* Perform the insertion */` |
|    33590 |  1860 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    33590 |  1861 | `	PH7_MemObjRelease(&sKey);` |
|    33590 |  1862 | `	return rc;` |
|        2 |  1863 |  |
|        - |  1864 | `/*` |
|        - |  1865 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1866 | ` * Return a pointer to the variable value on success.` |
|        - |  1867 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1868 | ` */` |
|  3444234 |  1869 | `static ph7_value * VmExtractMemObj(` |
|        - |  1870 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1871 | `	const SyString *pName, /* Variable name */` |
|        - |  1872 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1873 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1874 | `	)` |
|        2 |  1875 |  |
|  3444236 |  1876 | `	int bNullify = FALSE;` |
|        - |  1877 | `	SyHashEntry *pEntry;` |
|        - |  1878 | `	VmFrame *pFrame;` |
|        - |  1879 | `	ph7_value *pObj;` |
|        - |  1880 | `	sxu32 nIdx;` |
|        - |  1881 | `	sxi32 rc;` |
|        - |  1882 | `	/* Point to the top active frame */` |
|  3444236 |  1883 | `	pFrame = pVm->pFrame;` |
|  3444236 |  1884 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1885 | `	/* Perform the lookup */` |
|  3444236 |  1886 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1887 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1888 | `		pName = &sAnnon;` |
|        - |  1889 | `		/* Always nullify the object */` |
|      ! 0 |  1890 | `		bNullify = TRUE;` |
|      ! 0 |  1891 | `		bDup = FALSE;` |
|      ! 0 |  1892 | `	}` |
|        - |  1893 | `	/* Check the superglobals table first */` |
|  3444236 |  1894 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3444236 |  1895 | `	if( pEntry == 0 ){` |
|        - |  1896 | `		/* Query the top active frame */` |
|  3444196 |  1897 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3444196 |  1898 | `		if( pEntry == 0 ){` |
|   101388 |  1899 | `			char *zName = (char *)pName->zString;` |
|        - |  1900 | `			VmSlot sLocal;` |
|   101388 |  1901 | `			if( !bCreate ){` |
|        - |  1902 | `				/* Do not create the variable,return NULL instead */` |
|      118 |  1903 | `				return 0;` |
|        - |  1904 | `			}` |
|        - |  1905 | `			/* No such variable,automatically create a new one and install` |
|        - |  1906 | `			 * it in the current frame.` |
|        - |  1907 | `			 */` |
|   101272 |  1908 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   101272 |  1909 | `			if( pObj == 0 ){` |
|      ! 0 |  1910 | `				return 0;` |
|        - |  1911 | `			}` |
|   101272 |  1912 | `			nIdx = pObj->nIdx;` |
|   101272 |  1913 | `			if( bDup ){` |
|        - |  1914 | `				/* Duplicate name */` |
|      172 |  1915 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      172 |  1916 | `				if( zName == 0 ){` |
|      ! 0 |  1917 | `					return 0;` |
|        - |  1918 | `				}` |
|       85 |  1919 | `			}` |
|        - |  1920 | `			/* Link to the top active VM frame */` |
|   101272 |  1921 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   101272 |  1922 | `			if( rc != SXRET_OK ){` |
|        - |  1923 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1924 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1925 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1926 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1927 | `				return 0;` |
|        - |  1928 | `			}` |
|   101272 |  1929 | `			if( pFrame->pParent != 0 ){` |
|        - |  1930 | `				/* Local variable */` |
|    93832 |  1931 | `				sLocal.nIdx = nIdx;` |
|    93832 |  1932 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    46917 |  1933 | `			}else{` |
|        - |  1934 | `				/* Register in the $GLOBALS array */` |
|     7442 |  1935 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1936 | `			}` |
|        - |  1937 | `			/* Install in the reference table */` |
|   101272 |  1938 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1939 | `			/* Save object index */` |
|   101272 |  1940 | `			pObj->nIdx = nIdx;` |
|    50637 |  1941 | `		}else{` |
|        - |  1942 | `			/* Extract variable contents */` |
|  3342810 |  1943 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3342810 |  1944 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3342810 |  1945 | `			if( bNullify && pObj ){` |
|      ! 0 |  1946 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1947 | `			}` |
|        - |  1948 | `		}` |
|  1722151 |  1949 | `	}else{` |
|        - |  1950 | `		/* Superglobal */` |
|       42 |  1951 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1952 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1953 | `	}` |
|  3444120 |  1954 | `	return pObj;` |
|  1722229 |  1955 |  |
|        - |  1956 | `/*` |
|        - |  1957 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1958 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1959 | ` */` |
|     2916 |  1960 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1961 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1962 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1963 | `	sxu32 nByte        /* zName length */` |
|        - |  1964 | `	)` |
|        2 |  1965 |  |
|        - |  1966 | `	SyHashEntry *pEntry;` |
|        - |  1967 | `	ph7_value *pValue;` |
|        - |  1968 | `	sxu32 nIdx;` |
|        - |  1969 | `	/* Query the superglobal table */` |
|     2918 |  1970 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2918 |  1971 | `	if( pEntry == 0 ){` |
|        - |  1972 | `		/* No such entry */` |
|      ! 0 |  1973 | `		return 0;` |
|        - |  1974 | `	}` |
|        - |  1975 | `	/* Extract the superglobal index in the global object pool */` |
|     2918 |  1976 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1977 | `	/* Extract the variable value  */` |
|     2918 |  1978 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2918 |  1979 | `	return pValue;` |
|     1460 |  1980 |  |
|        - |  1981 | `/*` |
|        - |  1982 | ` * Perform a raw hashmap insertion.` |
|        - |  1983 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1984 | ` */` |
|     2946 |  1985 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1986 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1987 | `	const char *zKey,   /* Entry key */` |
|        - |  1988 | `	int nKeylen,        /* zKey length*/` |
|        - |  1989 | `	const char *zData,  /* Entry data */` |
|        - |  1990 | `	int nLen            /* zData length */` |
|        - |  1991 | `	)` |
|        2 |  1992 |  |
|        - |  1993 | `	ph7_value sKey,sValue;` |
|        - |  1994 | `	sxi32 rc;` |
|     2948 |  1995 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2948 |  1996 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2948 |  1997 | `	if( zKey ){` |
|     2926 |  1998 | `		if( nKeylen < 0 ){` |
|     2874 |  1999 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1436 |  2000 | `		}` |
|     2926 |  2001 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1462 |  2002 | `	}` |
|     2948 |  2003 | `	if( zData ){` |
|     2948 |  2004 | `		if( nLen < 0 ){` |
|        - |  2005 | `			/* Compute length automatically */` |
|      144 |  2006 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2007 | `		}` |
|     2948 |  2008 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1473 |  2009 | `	}` |
|        - |  2010 | `	/* Perform the insertion */` |
|     2948 |  2011 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2948 |  2012 | `	PH7_MemObjRelease(&sKey);` |
|     2948 |  2013 | `	PH7_MemObjRelease(&sValue);` |
|     2948 |  2014 | `	return rc;` |
|        2 |  2015 |  |
|        - |  2016 | `/*` |
|        - |  2017 | ` * Configure a working virtual machine instance.` |
|        - |  2018 | ` *` |
|        - |  2019 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2020 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2021 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2022 | ` * The second argument to this function is an integer configuration option` |
|        - |  2023 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2024 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2025 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2026 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2027 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2028 | ` */` |
|    42122 |  2029 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2030 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2031 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2032 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2033 | `	)` |
|        2 |  2034 |  |
|    42124 |  2035 | `	sxi32 rc = SXRET_OK;` |
|    42124 |  2036 | `	switch(nOp){` |
|     1298 |  2037 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2598 |  2038 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2598 |  2039 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2040 | `		/* VM output consumer callback */` |
|        - |  2041 | `#ifdef UNTRUST` |
|        - |  2042 | `		if( xConsumer == 0 ){` |
|        - |  2043 | `			rc = SXERR_CORRUPT;` |
|        - |  2044 | `			break;` |
|        - |  2045 | `		}` |
|        - |  2046 | `#endif` |
|        - |  2047 | `		/* Install the output consumer */` |
|     2598 |  2048 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2598 |  2049 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2598 |  2050 | `		break;` |
|        - |  2051 | `							   }` |
|     1306 |  2052 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2053 | `		/* Import path */` |
|        - |  2054 | `		  const char *zPath;` |
|        - |  2055 | `		  SyString sPath;` |
|     2614 |  2056 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2057 | `#if defined(UNTRUST)` |
|        - |  2058 | `		  if( zPath == 0 ){` |
|        - |  2059 | `			  rc = SXERR_EMPTY;` |
|        - |  2060 | `			  break;` |
|        - |  2061 | `		  }` |
|        - |  2062 | `#endif` |
|     2614 |  2063 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2064 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2065 | `#ifdef __WINNT__` |
|        2 |  2066 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2067 | `#endif` |
|     5226 |  2068 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2069 | `		  /* Remove leading and trailing white spaces */` |
|     2614 |  2070 | `		  SyStringFullTrim(&sPath);` |
|     2614 |  2071 | `		  if( sPath.nByte > 0 ){` |
|        - |  2072 | `			  /* Store the path in the corresponding conatiner */` |
|     2614 |  2073 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1306 |  2074 | `		  }` |
|     2614 |  2075 | `		  break;` |
|        - |  2076 | `									 }` |
|     1306 |  2077 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2078 | `		/* Run-Time Error report */` |
|     2614 |  2079 | `		pVm->bErrReport = 1;` |
|     2614 |  2080 | `		break;` |
|      ! 0 |  2081 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2082 | `		/* Recursion depth */` |
|      ! 0 |  2083 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2084 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2085 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2086 | `		}` |
|      ! 0 |  2087 | `		break;` |
|        - |  2088 | `									   }` |
|      ! 0 |  2089 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2090 | `		/* VM output length in bytes */` |
|      ! 0 |  2091 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2092 | `#ifdef UNTRUST` |
|        - |  2093 | `		if( pOut == 0 ){` |
|        - |  2094 | `			rc = SXERR_CORRUPT;` |
|        - |  2095 | `			break;` |
|        - |  2096 | `		}` |
|        - |  2097 | `#endif` |
|      ! 0 |  2098 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2099 | `		break;` |
|        - |  2100 | `							   }` |
|        - |  2101 |  |
|    13060 |  2102 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2103 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2104 | `		/* Create a new superglobal/global variable */` |
|    26122 |  2105 | `		const char *zName = va_arg(ap,const char *);` |
|    26122 |  2106 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2107 | `		SyHashEntry *pEntry;` |
|        - |  2108 | `		ph7_value *pObj;` |
|        - |  2109 | `		sxu32 nByte;` |
|        - |  2110 | `		sxu32 nIdx;` |
|        - |  2111 | `#ifdef UNTRUST` |
|        - |  2112 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2113 | `			rc = SXERR_CORRUPT;` |
|        - |  2114 | `			break;` |
|        - |  2115 | `		}` |
|        - |  2116 | `#endif` |
|    26122 |  2117 | `		nByte = SyStrlen(zName);` |
|    26122 |  2118 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2119 | `			/* Check if the superglobal is already installed */` |
|    26122 |  2120 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    13062 |  2121 | `		}else{` |
|        - |  2122 | `			/* Query the top active VM frame */` |
|      ! 0 |  2123 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2124 | `		}` |
|    26122 |  2125 | `		if( pEntry ){` |
|        - |  2126 | `			/* Variable already installed */` |
|      ! 0 |  2127 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2128 | `			/* Extract contents */` |
|      ! 0 |  2129 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2130 | `			if( pObj ){` |
|        - |  2131 | `				/* Overwrite old contents */` |
|      ! 0 |  2132 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2133 | `			}` |
|      ! 0 |  2134 | `		}else{` |
|        - |  2135 | `			/* Install a new variable */` |
|    26122 |  2136 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    26122 |  2137 | `			if( pObj == 0 ){` |
|      ! 0 |  2138 | `				rc = SXERR_MEM;` |
|      ! 0 |  2139 | `				break;` |
|        - |  2140 | `			}` |
|    26122 |  2141 | `			nIdx = pObj->nIdx;` |
|        - |  2142 | `			/* Copy value */` |
|    26122 |  2143 | `			PH7_MemObjStore(pValue,pObj);` |
|    26122 |  2144 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2145 | `				/* Install the superglobal */` |
|    26122 |  2146 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    13062 |  2147 | `			}else{` |
|        - |  2148 | `				/* Install in the current frame */` |
|      ! 0 |  2149 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2150 | `			}` |
|    26122 |  2151 | `			if( rc == SXRET_OK ){` |
|        - |  2152 | `				SyHashEntry *pRef;` |
|    26122 |  2153 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    26122 |  2154 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    13062 |  2155 | `				}else{` |
|      ! 0 |  2156 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2157 | `				}` |
|        - |  2158 | `				/* Install in the reference table */` |
|    26122 |  2159 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    26122 |  2160 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2161 | `					/* Register in the $GLOBALS array */` |
|    26122 |  2162 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    13060 |  2163 | `				}` |
|    13060 |  2164 | `			}` |
|        - |  2165 | `		}` |
|    26122 |  2166 | `		break;` |
|        - |  2167 | `									}` |
|     1436 |  2168 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2169 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2170 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2171 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2172 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2173 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2174 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2874 |  2175 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2874 |  2176 | `		const char *zValue = va_arg(ap,const char *);` |
|     2874 |  2177 | `		int nLen = va_arg(ap,int);` |
|        - |  2178 | `		ph7_hashmap *pMap;` |
|        - |  2179 | `		ph7_value *pValue;` |
|     2874 |  2180 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2181 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2182 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2873 |  2183 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2184 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2185 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2872 |  2186 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2187 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2188 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2872 |  2189 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2190 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2191 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2872 |  2192 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2193 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2194 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2872 |  2195 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2196 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2197 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2198 | `		}else{` |
|        - |  2199 | `			/* Extract the $_SERVER superglobal */` |
|     2872 |  2200 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2201 | `		}` |
|     2874 |  2202 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2203 | `			/* No such entry */` |
|      ! 0 |  2204 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2205 | `			break;` |
|        - |  2206 | `		}` |
|        - |  2207 | `		/* Point to the hashmap */` |
|     2874 |  2208 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2209 | `		/* Perform the insertion */` |
|     2874 |  2210 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2874 |  2211 | `		break;` |
|        - |  2212 | `								   }` |
|       11 |  2213 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2214 | `		/* Script arguments */` |
|       24 |  2215 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2216 | `		ph7_hashmap *pMap;` |
|        - |  2217 | `		ph7_value *pValue;` |
|        - |  2218 | `		sxu32 n;` |
|       24 |  2219 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2220 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2221 | `			break;` |
|        - |  2222 | `		}` |
|        - |  2223 | `		/* Extract the $argv array */` |
|       24 |  2224 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2225 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2226 | `			/* No such entry */` |
|      ! 0 |  2227 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2228 | `			break;` |
|        - |  2229 | `		}` |
|        - |  2230 | `		/* Point to the hashmap */` |
|       24 |  2231 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2232 | `		/* Perform the insertion */` |
|       24 |  2233 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2234 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2235 | `		if( rc == SXRET_OK ){` |
|       24 |  2236 | `			if( pMap->nEntry > 1 ){` |
|        - |  2237 | `				/* Append space separator first */` |
|       18 |  2238 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2239 | `			}` |
|       24 |  2240 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2241 | `		}` |
|       24 |  2242 | `		break;` |
|        - |  2243 | `								  }` |
|      ! 0 |  2244 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2245 | `		/* error_log() consumer */` |
|      ! 0 |  2246 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2247 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2248 | `		break;` |
|        - |  2249 | `										}` |
|      ! 0 |  2250 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2251 | `		/* Script return value */` |
|      ! 0 |  2252 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2253 | `#ifdef UNTRUST` |
|        - |  2254 | `		if( ppValue == 0 ){` |
|        - |  2255 | `			rc = SXERR_CORRUPT;` |
|        - |  2256 | `			break;` |
|        - |  2257 | `		}` |
|        - |  2258 | `#endif` |
|      ! 0 |  2259 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2260 | `		break;` |
|        - |  2261 | `								   }` |
|     2612 |  2262 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2263 | `		/* Register an IO stream device */` |
|     5226 |  2264 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2265 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7836 |  2266 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5226 |  2267 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2268 | `				/* Invalid stream */` |
|      ! 0 |  2269 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2270 | `				break;` |
|        - |  2271 | `		}` |
|     5226 |  2272 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2273 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2614 |  2274 | `			pVm->pDefStream = pStream;` |
|     1306 |  2275 | `		}` |
|        - |  2276 | `		/* Insert in the appropriate container */` |
|     5226 |  2277 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5226 |  2278 | `		break;` |
|        - |  2279 | `								  }` |
|        8 |  2280 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2281 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2282 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2283 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2284 | `#ifdef UNTRUST` |
|        - |  2285 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2286 | `			rc = SXERR_CORRUPT;` |
|        - |  2287 | `			break;` |
|        - |  2288 | `		}` |
|        - |  2289 | `#endif` |
|       16 |  2290 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2291 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2292 | `		break;` |
|        - |  2293 | `									   }` |
|        8 |  2294 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2295 | `		/* Raw HTTP request*/` |
|       16 |  2296 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2297 | `		int nByte = va_arg(ap,int);` |
|       16 |  2298 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2299 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2300 | `			break;` |
|        - |  2301 | `		}` |
|       16 |  2302 | `		if( nByte < 0 ){` |
|        - |  2303 | `			/* Compute length automatically */` |
|      ! 0 |  2304 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2305 | `		}` |
|        - |  2306 | `		/* Process the request */` |
|       16 |  2307 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2308 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2309 | `		if( rc == SXRET_OK ){` |
|       16 |  2310 | `			pVm->bHttpContext = 1;` |
|        8 |  2311 | `		}` |
|       16 |  2312 | `		break;` |
|        - |  2313 | `									}` |
|        8 |  2314 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2315 | `		/* Extract HTTP response status code */` |
|       16 |  2316 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2317 | `		if( pStatus ){` |
|       16 |  2318 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2319 | `		}` |
|       16 |  2320 | `		break;` |
|        - |  2321 | `										}` |
|        8 |  2322 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2323 | `		/* Iterate response headers via callback */` |
|        - |  2324 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2325 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2326 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2327 | `		if( xCallback ){` |
|       16 |  2328 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2329 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2330 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2331 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2332 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2333 | `							   pUserData);` |
|       12 |  2334 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2335 | `					break;` |
|        - |  2336 | `				}` |
|        6 |  2337 | `			}` |
|        8 |  2338 | `		}` |
|       16 |  2339 | `		break;` |
|        - |  2340 | `										 }` |
|      ! 0 |  2341 | `	default:` |
|        - |  2342 | `		/* Unknown configuration option */` |
|      ! 0 |  2343 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2344 | `		break;` |
|        - |  2345 | `	}` |
|    42124 |  2346 | `	return rc;` |
|        2 |  2347 |  |
|        - |  2348 | `/* Forward declaration */` |
|        - |  2349 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2350 | `/*` |
|        - |  2351 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2352 | ` * format.` |
|        - |  2353 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2354 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2355 | ` * (STDOUT).` |
|        - |  2356 | ` */` |
|        2 |  2357 | `static sxi32 VmByteCodeDump(` |
|        - |  2358 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2359 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2360 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2361 | `	)` |
|        1 |  2362 |  |
|        - |  2363 | `	static const char zDump[] = {` |
|        - |  2364 | `		"====================================================\n"` |
|        - |  2365 | `		"PH7 VM Dump\n"` |
|        - |  2366 | `		"====================================================\n"` |
|        - |  2367 | `	};` |
|        - |  2368 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2369 | `	sxi32 rc = SXRET_OK;` |
|        - |  2370 | `	sxu32 n;` |
|        - |  2371 | `	/* Point to the PH7 instructions */` |
|        3 |  2372 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2373 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2374 | `	n = 0;` |
|        3 |  2375 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2376 | `	/* Dump instructions */` |
|        7 |  2377 | `	for(;;){` |
|       15 |  2378 | `		if( pInstr >= pEnd ){` |
|        - |  2379 | `			/* No more instructions */` |
|        3 |  2380 | `			break;` |
|        - |  2381 | `		}` |
|        - |  2382 | `		/* Format and call the consumer callback */` |
|       19 |  2383 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2384 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2385 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2386 | `		if( rc != SXRET_OK ){` |
|        - |  2387 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2388 | `			return rc;` |
|        - |  2389 | `		}` |
|       13 |  2390 | `		++n;` |
|       13 |  2391 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2392 | `	}` |
|        3 |  2393 | `	return rc;` |
|        2 |  2394 |  |
|        - |  2395 | `/* Forward declaration */` |
|        - |  2396 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2397 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2398 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2399 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2400 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2401 | `/*` |
|        - |  2402 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2403 | ` * consumer callback.` |
|        - |  2404 | ` */` |
|      570 |  2405 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2406 |  |
|      571 |  2407 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      571 |  2408 | `	sxi32 rc = SXRET_OK;` |
|        - |  2409 | `	/* Append a new line */` |
|        - |  2410 | `#ifdef __WINNT__` |
|        1 |  2411 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2412 | `#else` |
|      570 |  2413 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2414 | `#endif` |
|        - |  2415 | `	/* Invoke the output consumer callback */` |
|      571 |  2416 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      571 |  2417 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      571 |  2418 | `	return rc;` |
|        1 |  2419 |  |
|        - |  2420 | `/*` |
|        - |  2421 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2422 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2423 | ` * information.` |
|        - |  2424 | ` */` |
|      136 |  2425 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2426 |  |
|      138 |  2427 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2428 | `		ph7_value apArg[4];` |
|        - |  2429 | `		ph7_value *apArgPtr[4];` |
|        - |  2430 | `		ph7_value sResult;` |
|        - |  2431 | `		SyString sErr;` |
|        - |  2432 | `		/* Prepare arguments */` |
|       64 |  2433 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2434 | `			/* use explicit message length to avoid reading past buffer */` |
|       64 |  2435 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       64 |  2436 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       64 |  2437 | `		if( pFile ){` |
|       64 |  2438 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       64 |  2439 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       33 |  2440 | `		}else{` |
|      ! 0 |  2441 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2442 | `		}` |
|       64 |  2443 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       64 |  2444 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2445 | `		/* Set up pointer array */` |
|       64 |  2446 | `		apArgPtr[0] = &apArg[0];` |
|       64 |  2447 | `		apArgPtr[1] = &apArg[1];` |
|       64 |  2448 | `		apArgPtr[2] = &apArg[2];` |
|       64 |  2449 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2450 | `		/* Call the handler */` |
|       64 |  2451 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2452 | `		/* Check return value */` |
|       64 |  2453 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2454 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2455 | `		}` |
|        - |  2456 | `		/* Release */` |
|       64 |  2457 | `		PH7_MemObjRelease(&apArg[0]);` |
|       64 |  2458 | `		PH7_MemObjRelease(&apArg[1]);` |
|       64 |  2459 | `		PH7_MemObjRelease(&apArg[2]);` |
|       64 |  2460 | `		PH7_MemObjRelease(&apArg[3]);` |
|       64 |  2461 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2462 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2463 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       64 |  2464 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2465 | `	}` |
|        - |  2466 | `	/* No handler, always call error handler */` |
|       75 |  2467 | `	return TRUE;` |
|       70 |  2468 |  |
|       98 |  2469 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2470 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2471 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2472 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2473 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2474 | `	)` |
|        2 |  2475 |  |
|      100 |  2476 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2477 | `	SyString *pFile;` |
|        - |  2478 | `	char *zErr;` |
|      100 |  2479 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2480 | `	if( !pVm->bErrReport ){` |
|        - |  2481 | `		/* Don't bother reporting errors */` |
|        3 |  2482 | `		return SXRET_OK;` |
|        - |  2483 | `	}` |
|        - |  2484 | `	/* Reset the working buffer */` |
|       98 |  2485 | `	SyBlobReset(pWorker);` |
|        - |  2486 | `	/* Peek the processed file if available */` |
|       98 |  2487 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2488 | `	if( pFile ){` |
|        - |  2489 | `		/* Append file name */` |
|       98 |  2490 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2491 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2492 | `	}` |
|        - |  2493 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2494 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2495 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2496 | `	 * E_DEPRECATED). */` |
|       98 |  2497 | `	zErr = "Error:  ";` |
|       98 |  2498 | `	switch(iErr){` |
|       19 |  2499 | `	case PH7_CTX_WARNING:` |
|       40 |  2500 | `		zErr = "Warning:  ";` |
|       40 |  2501 | `		break;` |
|        6 |  2502 | `	case PH7_CTX_NOTICE:` |
|       14 |  2503 | `		zErr = "Notice:  ";` |
|       12 |  2504 | `		break;` |
|       23 |  2505 | `	default:` |
|        - |  2506 | `		/* keep iErr unchanged */` |
|       46 |  2507 | `		break;` |
|        - |  2508 | `	}` |
|       98 |  2509 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2510 | `	if( pFuncName ){` |
|        - |  2511 | `		/* Append function name first */` |
|       23 |  2512 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2513 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2514 | `	}` |
|       98 |  2515 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2516 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2517 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2518 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2519 | `	}` |
|       98 |  2520 | `	return rc;` |
|       51 |  2521 |  |
|        - |  2522 | `/*` |
|        - |  2523 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2524 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2525 | ` * information.` |
|        - |  2526 | ` */` |
|       40 |  2527 | `static sxi32 VmThrowErrorAp(` |
|        - |  2528 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2529 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2530 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2531 | `	const char *zFormat, /* Format message */` |
|        - |  2532 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2533 | `	)` |
|        2 |  2534 |  |
|       42 |  2535 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2536 | `	SyBlob sMsg;` |
|        - |  2537 | `	SyString *pFile;` |
|        - |  2538 | `	char *zErr;` |
|       42 |  2539 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2540 | `	if( !pVm->bErrReport ){` |
|        - |  2541 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2542 | `		return SXRET_OK;` |
|        - |  2543 | `	}` |
|        - |  2544 | `	/* Reset the working buffer */` |
|       42 |  2545 | `	SyBlobReset(pWorker);` |
|        - |  2546 | `	/* Peek the processed file if available */` |
|       42 |  2547 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2548 | `	if( pFile ){` |
|        - |  2549 | `		/* Append file name */` |
|       42 |  2550 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2551 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2552 | `	}` |
|        - |  2553 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2554 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2555 | `	 * the correct errno value. */` |
|       42 |  2556 | `	zErr = "Error:  ";` |
|       42 |  2557 | `	switch(iErr){` |
|        4 |  2558 | `	case PH7_CTX_WARNING:` |
|        9 |  2559 | `		zErr = "Warning:  ";` |
|        9 |  2560 | `		break;` |
|        3 |  2561 | `	case PH7_CTX_NOTICE:` |
|        7 |  2562 | `		zErr = "Notice:  ";` |
|        6 |  2563 | `		break;` |
|       13 |  2564 | `	default:` |
|        - |  2565 | `		/* do not change iErr */` |
|       26 |  2566 | `		break;` |
|        - |  2567 | `	}` |
|       42 |  2568 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2569 | `	if( pFuncName ){` |
|        - |  2570 | `		/* Append function name first */` |
|       26 |  2571 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2572 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2573 | `	}` |
|        - |  2574 | `	/* Format the raw message */` |
|       42 |  2575 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2576 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2577 | `	/* Check if a user error handler is installed */` |
|       42 |  2578 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2579 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2580 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2581 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2582 | `	}` |
|       42 |  2583 | `	SyBlobRelease(&sMsg);` |
|       42 |  2584 | `	return rc;` |
|       22 |  2585 |  |
|        - |  2586 | `/*` |
|        - |  2587 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2588 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2589 | ` * possible.` |
|        - |  2590 | ` */` |
|       38 |  2591 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2592 |  |
|        - |  2593 | `	ph7_class *pClass;` |
|       39 |  2594 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2595 | `	ph7_class_instance *pThis;` |
|        - |  2596 | `	ph7_class_method *pCons;` |
|        - |  2597 | `	ph7_value sArg;` |
|        - |  2598 | `	ph7_value *apArg[1];` |
|        - |  2599 | `	SyBlob sMsg;` |
|        - |  2600 | `	SyString sMsgStr;` |
|        - |  2601 | `	VmFrame *pFrame;` |
|        - |  2602 | `	sxi32 rc;` |
|       39 |  2603 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2604 | `	if( pClass == 0 ){` |
|      ! 0 |  2605 | `		return PH7_ABORT;` |
|        - |  2606 | `	}` |
|       39 |  2607 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2608 | `	if( pThis == 0 ){` |
|      ! 0 |  2609 | `		return PH7_ABORT;` |
|        - |  2610 | `	}` |
|       39 |  2611 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2612 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2613 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2614 | `	{` |
|       39 |  2615 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2616 | `		if( pOwner ){` |
|       39 |  2617 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2618 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2619 | `		}else{` |
|      ! 0 |  2620 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2621 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2622 | `		}` |
|        - |  2623 | `	}` |
|       39 |  2624 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2625 | `	if( pCons ){` |
|       39 |  2626 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2627 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2628 | `		apArg[0] = &sArg;` |
|       39 |  2629 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2630 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2631 | `	}` |
|       39 |  2632 | `	SyBlobRelease(&sMsg);` |
|       39 |  2633 | `	pFrame = pVm->pFrame;` |
|       39 |  2634 | `	if( pFrame ){` |
|       39 |  2635 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2636 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2637 | `	}` |
|       39 |  2638 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2639 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2640 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2641 | `		return PH7_ABORT;` |
|        - |  2642 | `	}` |
|       39 |  2643 | `	return PH7_EXCEPTION;` |
|       20 |  2644 |  |
|        - |  2645 |  |
|        - |  2646 | `/*` |
|        - |  2647 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2648 | ` */` |
|        4 |  2649 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2650 |  |
|        - |  2651 | `	ph7_class *pErrClass;` |
|        - |  2652 | `	ph7_class_instance *pThis;` |
|        - |  2653 | `	ph7_class_method *pCons;` |
|        - |  2654 | `	ph7_value sArg;` |
|        - |  2655 | `	ph7_value *apArg[1];` |
|        - |  2656 | `	SyBlob sMsg;` |
|        - |  2657 | `	SyString sMsgStr;` |
|        - |  2658 | `	VmFrame *pFrame;` |
|        - |  2659 | `	sxi32 rc;` |
|        5 |  2660 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2661 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2662 | `		return PH7_ABORT;` |
|        - |  2663 | `	}` |
|        5 |  2664 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2665 | `	if( pThis == 0 ){` |
|      ! 0 |  2666 | `		return PH7_ABORT;` |
|        - |  2667 | `	}` |
|        5 |  2668 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2669 | `	{` |
|        5 |  2670 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2671 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2672 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2673 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2674 | `	}` |
|        5 |  2675 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2676 | `	if( pCons ){` |
|        5 |  2677 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2678 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2679 | `		apArg[0] = &sArg;` |
|        5 |  2680 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2681 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2682 | `	}` |
|        5 |  2683 | `	SyBlobRelease(&sMsg);` |
|        5 |  2684 | `	pFrame = pVm->pFrame;` |
|        5 |  2685 | `	if( pFrame ){` |
|        5 |  2686 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2687 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2688 | `	}` |
|        5 |  2689 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2690 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2691 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2692 | `		return PH7_ABORT;` |
|        - |  2693 | `	}` |
|        5 |  2694 | `	return PH7_EXCEPTION;` |
|        3 |  2695 |  |
|        - |  2696 |  |
|        - |  2697 | `/*` |
|        - |  2698 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2699 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2700 | ` * For class types, instanceof is verified.` |
|        - |  2701 | ` *` |
|        - |  2702 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2703 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2704 | ` */` |
|        - |  2705 | `/*` |
|        - |  2706 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2707 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2708 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2709 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2710 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2711 | ` */` |
|       16 |  2712 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2713 |  |
|        - |  2714 | `	const char *z, *zEnd, *zTail;` |
|        - |  2715 | `	sxu32 n;` |
|        - |  2716 | `	sxu8 bReal;` |
|        - |  2717 | `	sxi32 rc;` |
|       18 |  2718 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2719 | `		return 0;` |
|        - |  2720 | `	}` |
|       18 |  2721 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2722 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2723 | `	zEnd = z + n;` |
|       18 |  2724 | `	if( n == 0 ){` |
|      ! 0 |  2725 | `		return 0;` |
|        - |  2726 | `	}` |
|       18 |  2727 | `	zTail = 0;` |
|       18 |  2728 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2729 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        5 |  2730 | `		return 0;` |
|        - |  2731 | `	}` |
|        - |  2732 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       14 |  2733 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2734 | `		zTail++;` |
|      ! 0 |  2735 | `	}` |
|       14 |  2736 | `	return zTail == zEnd ? 1 : 0;` |
|       10 |  2737 |  |
|        - |  2738 |  |
|        - |  2739 | `/*` |
|        - |  2740 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2741 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2742 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2743 | ` *   0 if it's not strictly numeric.` |
|        - |  2744 | ` */` |
|       16 |  2745 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2746 |  |
|        - |  2747 | `	const char *z, *zEnd, *zTail;` |
|        - |  2748 | `	sxu32 n;` |
|       18 |  2749 | `	sxu8 bReal = 0;` |
|        - |  2750 | `	sxi32 rc;` |
|       18 |  2751 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2752 | `		return 0;` |
|        - |  2753 | `	}` |
|       18 |  2754 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2755 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2756 | `	zEnd = z + n;` |
|       18 |  2757 | `	if( n == 0 ) return 0;` |
|       18 |  2758 | `	zTail = 0;` |
|       18 |  2759 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2760 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2761 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2762 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2763 | `	return bReal ? 2 : 1;` |
|       10 |  2764 |  |
|        - |  2765 |  |
|        - |  2766 | `/*` |
|        - |  2767 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts* using` |
|        - |  2768 | ` * PHP 8 weak-mode union semantics. Returns SXRET_OK on accept (pValue may` |
|        - |  2769 | ` * have been mutated by the cast), SXERR_INVALID on reject. Caller is` |
|        - |  2770 | ` * responsible for the actual TypeError throw.` |
|        - |  2771 | ` *` |
|        - |  2772 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2773 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2774 | ` */` |
|       94 |  2775 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable)` |
|        2 |  2776 |  |
|        - |  2777 | `	sxu32 i;` |
|        - |  2778 | `	ph7_type_alt *aAlts;` |
|        - |  2779 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2780 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|       96 |  2781 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2782 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2783 | `	}` |
|       84 |  2784 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       84 |  2785 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       84 |  2786 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      248 |  2787 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      166 |  2788 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      142 |  2789 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      142 |  2790 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      142 |  2791 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       72 |  2792 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       44 |  2793 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2794 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       84 |  2795 | `	}` |
|        - |  2796 | `	/* Object handling */` |
|       84 |  2797 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2798 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2799 | `		if( bHasClassAlt ){` |
|       14 |  2800 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2801 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2802 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2803 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2804 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2805 | `			}` |
|       26 |  2806 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2807 | `				ph7_class *pExpected;` |
|        - |  2808 | `				SyString *pCN;` |
|       22 |  2809 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2810 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2811 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2812 | `					pExpected = pSelfNow;` |
|       22 |  2813 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2814 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2815 | `				}else{` |
|       22 |  2816 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2817 | `				}` |
|       22 |  2818 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2819 | `					return SXRET_OK;` |
|        - |  2820 | `				}` |
|        8 |  2821 | `			}` |
|        2 |  2822 | `		}` |
|        9 |  2823 | `		return SXERR_INVALID;` |
|        - |  2824 | `	}` |
|        - |  2825 | `	/* Array handling */` |
|       68 |  2826 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2827 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2828 | `	}` |
|        - |  2829 | `	/* Scalar handling — exact match first */` |
|       62 |  2830 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       24 |  2831 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2832 | `	}` |
|       40 |  2833 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2834 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2835 | `	}` |
|       36 |  2836 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       36 |  2837 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2838 | `	}` |
|       18 |  2839 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2840 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2841 | `	}` |
|        - |  2842 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2843 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2844 | `	 * to match PHP's union RFC. */` |
|        - |  2845 | `	{` |
|       18 |  2846 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2847 | `		if( bHasInt ){` |
|        - |  2848 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2849 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2850 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2851 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2852 | `				return SXRET_OK;` |
|        - |  2853 | `			}` |
|       18 |  2854 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2855 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2856 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2857 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2858 | `					return SXRET_OK;` |
|        - |  2859 | `				}` |
|      ! 0 |  2860 | `			}` |
|       18 |  2861 | `			if( kind == 1 ){` |
|        9 |  2862 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2863 | `				return SXRET_OK;` |
|        - |  2864 | `			}` |
|        4 |  2865 | `		}` |
|       10 |  2866 | `		if( bHasFloat ){` |
|       10 |  2867 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2868 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2869 | `				return SXRET_OK;` |
|        - |  2870 | `			}` |
|       10 |  2871 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2872 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2873 | `				return SXRET_OK;` |
|        - |  2874 | `			}` |
|        1 |  2875 | `		}` |
|        3 |  2876 | `		if( bHasString ){` |
|      ! 0 |  2877 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2878 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2879 | `				return SXRET_OK;` |
|        - |  2880 | `			}` |
|      ! 0 |  2881 | `		}` |
|        3 |  2882 | `		if( bHasBool ){` |
|      ! 0 |  2883 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2884 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2885 | `				return SXRET_OK;` |
|        - |  2886 | `			}` |
|      ! 0 |  2887 | `		}` |
|        - |  2888 | `	}` |
|        3 |  2889 | `	return SXERR_INVALID;` |
|       49 |  2890 |  |
|        - |  2891 |  |
|        - |  2892 | `/*` |
|        - |  2893 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  2894 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  2895 | ` */` |
|       18 |  2896 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  2897 |  |
|       19 |  2898 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  2899 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  2900 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  2901 | `	return zBuf;` |
|        1 |  2902 |  |
|        - |  2903 |  |
|    12882 |  2904 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  2905 |  |
|        - |  2906 | `	SyHashEntry *pSlot;` |
|        - |  2907 | `	VmClassAttr *pVmAttr;` |
|        - |  2908 | `	ph7_class_attr *pAttr;` |
|    12884 |  2909 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    12884 |  2910 | `	if( pSlot == 0 ){` |
|    12688 |  2911 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  2912 | `	}` |
|      198 |  2913 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      198 |  2914 | `	pAttr = pVmAttr->pAttr;` |
|      198 |  2915 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  2916 | `		return SXRET_OK;` |
|        - |  2917 | `	}` |
|        - |  2918 | `	/* Union type: dispatch to the shared coercion helper. */` |
|      198 |  2919 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  2920 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  2921 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0);` |
|       16 |  2922 | `		if( rc == SXRET_OK ){` |
|        9 |  2923 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  2924 | `			return SXRET_OK;` |
|        - |  2925 | `		}` |
|        7 |  2926 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2927 | `			char zBuf[128];` |
|        4 |  2928 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  2929 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2930 | `		}` |
|        5 |  2931 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2932 | `	}` |
|        - |  2933 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      184 |  2934 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2935 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  2936 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  2937 | `			return SXRET_OK;` |
|        - |  2938 | `		}` |
|        3 |  2939 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  2940 | `	}` |
|        - |  2941 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  2942 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  2943 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      172 |  2944 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  2945 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  2946 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  2947 | `			return SXRET_OK;` |
|        - |  2948 | `		}` |
|        7 |  2949 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2950 | `	}` |
|      162 |  2951 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  2952 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  2953 | `		 * currently active on the self-stack. */` |
|       26 |  2954 | `		ph7_class *pExpected = 0;` |
|       26 |  2955 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  2956 | `		ph7_class *pSelfNow = 0;` |
|       26 |  2957 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  2958 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  2959 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  2960 | `		}` |
|       26 |  2961 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  2962 | `			pExpected = pSelfNow;` |
|       24 |  2963 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  2964 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2965 | `		}else{` |
|       22 |  2966 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  2967 | `		}` |
|       26 |  2968 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  2969 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2970 | `		}` |
|       26 |  2971 | `		if( pExpected ){` |
|       22 |  2972 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  2973 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  2974 | `				char zBuf[128];` |
|        7 |  2975 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2976 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2977 | `			}` |
|        8 |  2978 | `		}` |
|       22 |  2979 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  2980 | `		return SXRET_OK;` |
|        - |  2981 | `	}` |
|        - |  2982 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  2983 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      138 |  2984 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2985 | `		char zBuf[128];` |
|       10 |  2986 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  2987 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2988 | `	}` |
|      132 |  2989 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  2990 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  2991 | `		if( xCast ){` |
|        - |  2992 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  2993 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  2994 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2995 | `			}` |
|       24 |  2996 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  2997 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2998 | `			}` |
|        - |  2999 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3000 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3001 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3002 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3003 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3004 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3005 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3006 | `			}` |
|       12 |  3007 | `			xCast(pValue);` |
|        5 |  3008 | `		}` |
|        5 |  3009 | `	}` |
|      118 |  3010 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      118 |  3011 | `	return SXRET_OK;` |
|     6443 |  3012 |  |
|        - |  3013 |  |
|        - |  3014 | `/*` |
|        - |  3015 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3016 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3017 | ` * information.` |
|        - |  3018 | ` * ------------------------------------` |
|        - |  3019 | ` * Simple boring wrapper function.` |
|        - |  3020 | ` * ------------------------------------` |
|        - |  3021 | ` */` |
|       16 |  3022 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3023 |  |
|        - |  3024 | `	va_list ap;` |
|        - |  3025 | `	sxi32 rc;` |
|       17 |  3026 | `	va_start(ap,zFormat);` |
|       17 |  3027 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3028 | `	va_end(ap);` |
|       17 |  3029 | `	return rc;` |
|        1 |  3030 |  |
|        - |  3031 | `/*` |
|        - |  3032 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3033 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3034 | ` */` |
|       30 |  3035 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  3036 |  |
|        - |  3037 | `	ph7_class *pClass;` |
|        - |  3038 | `	ph7_class_instance *pThis;` |
|        - |  3039 | `	ph7_class_method *pCons;` |
|        - |  3040 | `	ph7_value sArg;` |
|        - |  3041 | `	ph7_value *apArg[1];` |
|        - |  3042 | `	SyBlob sMsg;` |
|        - |  3043 | `	SyString sMsgStr;` |
|        - |  3044 | `	VmFrame *pFrame;` |
|        - |  3045 | `	sxi32 rc;` |
|       31 |  3046 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       31 |  3047 | `	if( pClass == 0 ){` |
|      ! 0 |  3048 | `		return PH7_ABORT;` |
|        - |  3049 | `	}` |
|       31 |  3050 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       31 |  3051 | `	if( pThis == 0 ){` |
|      ! 0 |  3052 | `		return PH7_ABORT;` |
|        - |  3053 | `	}` |
|       31 |  3054 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       31 |  3055 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       15 |  3056 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       31 |  3057 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       31 |  3058 | `	if( pCons ){` |
|       31 |  3059 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       31 |  3060 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       31 |  3061 | `		apArg[0] = &sArg;` |
|       31 |  3062 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       31 |  3063 | `		PH7_MemObjRelease(&sArg);` |
|       15 |  3064 | `	}` |
|       31 |  3065 | `	SyBlobRelease(&sMsg);` |
|       31 |  3066 | `	pFrame = pVm->pFrame;` |
|       31 |  3067 | `	if( pFrame ){` |
|       31 |  3068 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       31 |  3069 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       15 |  3070 | `	}` |
|       31 |  3071 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       31 |  3072 | `	PH7_ClassInstanceUnref(pThis);` |
|       31 |  3073 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3074 | `		return PH7_ABORT;` |
|        - |  3075 | `	}` |
|       31 |  3076 | `	return PH7_EXCEPTION;` |
|       16 |  3077 |  |
|        - |  3078 | `/*` |
|        - |  3079 | ` * Report a fatal named-argument error.` |
|        - |  3080 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3081 | ` */` |
|        6 |  3082 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3083 |  |
|        7 |  3084 | `	const char *zFunc = 0;` |
|        7 |  3085 | `	int nFunc = 0;` |
|        7 |  3086 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3087 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3088 |  |
|        - |  3089 | `/*` |
|        - |  3090 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3091 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3092 | ` * information.` |
|        - |  3093 | ` * ------------------------------------` |
|        - |  3094 | ` * Simple boring wrapper function.` |
|        - |  3095 | ` * ------------------------------------` |
|        - |  3096 | ` */` |
|       24 |  3097 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3098 |  |
|        - |  3099 | `	sxi32 rc;` |
|       26 |  3100 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3101 | `	return rc;` |
|        2 |  3102 |  |
|        - |  3103 | `/*` |
|        - |  3104 | ` * Resolve function context from the current frame.` |
|        - |  3105 | ` */` |
|      968 |  3106 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3107 |  |
|        - |  3108 | `	VmFrame *pFrame;` |
|        - |  3109 | `	ph7_vm_func *pFunc;` |
|      969 |  3110 | `	*pzFuncName = 0;` |
|      969 |  3111 | `	*pnFuncLen = 0;` |
|      969 |  3112 | `	pFrame = pVm->pFrame;` |
|      969 |  3113 | `	if( pFrame == 0 ){` |
|      ! 0 |  3114 | `		return;` |
|        - |  3115 | `	}` |
|      969 |  3116 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      969 |  3117 | `	if( pFrame->pParent == 0 ){` |
|      955 |  3118 | `		return;` |
|        - |  3119 | `	}` |
|       15 |  3120 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  3121 | `	if( pFunc == 0 ){` |
|      ! 0 |  3122 | `		return;` |
|        - |  3123 | `	}` |
|       15 |  3124 | `	*pzFuncName = pFunc->sName.zString;` |
|       15 |  3125 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      485 |  3126 |  |
|        - |  3127 | `/*` |
|        - |  3128 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3129 | ` */` |
|      494 |  3130 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3131 |  |
|        - |  3132 | `	SyBlob sOut;` |
|        - |  3133 | `	SyString *pFile;` |
|      495 |  3134 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3135 | `		return PH7_OK;` |
|        - |  3136 | `	}` |
|      495 |  3137 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3138 | `		zClass = "Exception";` |
|      ! 0 |  3139 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3140 | `	}` |
|      495 |  3141 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      483 |  3142 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      241 |  3143 | `	}` |
|      495 |  3144 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      495 |  3145 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      495 |  3146 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      495 |  3147 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      495 |  3148 | `	if( zMsg && nMsg > 0 ){` |
|      495 |  3149 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      495 |  3150 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      247 |  3151 | `	}` |
|      495 |  3152 | `	if( pFile ){` |
|      495 |  3153 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      495 |  3154 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      495 |  3155 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      247 |  3156 | `	}` |
|      495 |  3157 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      495 |  3158 | `	if( pFile ){` |
|      495 |  3159 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      495 |  3160 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      495 |  3161 | `		if( zFuncName && nFuncLen > 0 ){` |
|       15 |  3162 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        8 |  3163 | `		}else{` |
|      481 |  3164 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3165 | `		}` |
|      247 |  3166 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3167 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3168 | `	}else{` |
|      ! 0 |  3169 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3170 | `	}` |
|      495 |  3171 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      495 |  3172 | `	if( pFile ){` |
|      495 |  3173 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      495 |  3174 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      495 |  3175 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      495 |  3176 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      247 |  3177 | `	}` |
|      495 |  3178 | `	VmCallErrorHandler(pVm,&sOut);` |
|      495 |  3179 | `	SyBlobRelease(&sOut);` |
|      495 |  3180 | `	return PH7_ABORT;` |
|      248 |  3181 |  |
|        - |  3182 | `/*` |
|        - |  3183 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3184 | ` */` |
|      482 |  3185 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3186 |  |
|        - |  3187 | `	ph7_vm *pVm;` |
|        - |  3188 | `	ph7_class *pClass;` |
|        - |  3189 | `	ph7_class_instance *pThis;` |
|        - |  3190 | `	ph7_class_method *pCons;` |
|        - |  3191 | `	ph7_value sArg;` |
|        - |  3192 | `	ph7_value *apArg[1];` |
|        - |  3193 | `	SyBlob sMsg;` |
|        - |  3194 | `	SyString sMsgStr;` |
|        - |  3195 | `	VmFrame *pFrame;` |
|        - |  3196 | `	va_list ap;` |
|        - |  3197 | `	sxi32 rc;` |
|        - |  3198 |  |
|      484 |  3199 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3200 | `		return PH7_ABORT;` |
|        - |  3201 | `	}` |
|      484 |  3202 | `	pVm = pCtx->pVm;` |
|      484 |  3203 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3204 | `		zClass = "Error";` |
|      ! 0 |  3205 | `	}` |
|      484 |  3206 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      484 |  3207 | `	if( pClass == 0 ){` |
|      ! 0 |  3208 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3209 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3210 | `			zClass` |
|        - |  3211 | `			);` |
|        - |  3212 | `	}` |
|      484 |  3213 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      484 |  3214 | `	if( pThis == 0 ){` |
|      ! 0 |  3215 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3216 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3217 | `			);` |
|        - |  3218 | `	}` |
|        - |  3219 |  |
|      484 |  3220 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      484 |  3221 | `	va_start(ap,zFormat);` |
|      484 |  3222 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      484 |  3223 | `	va_end(ap);` |
|        - |  3224 |  |
|      484 |  3225 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      484 |  3226 | `	if( pCons ){` |
|      484 |  3227 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      484 |  3228 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      484 |  3229 | `		apArg[0] = &sArg;` |
|      484 |  3230 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      484 |  3231 | `		PH7_MemObjRelease(&sArg);` |
|      241 |  3232 | `	}` |
|      484 |  3233 | `	SyBlobRelease(&sMsg);` |
|        - |  3234 |  |
|      484 |  3235 | `	pFrame = pVm->pFrame;` |
|      484 |  3236 | `	if( pFrame ){` |
|      484 |  3237 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      484 |  3238 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      241 |  3239 | `	}` |
|      484 |  3240 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      484 |  3241 | `	PH7_ClassInstanceUnref(pThis);` |
|      484 |  3242 | `	if( rc == SXERR_ABORT ){` |
|      471 |  3243 | `		return PH7_ABORT;` |
|        - |  3244 | `	}` |
|       14 |  3245 | `	return PH7_EXCEPTION;` |
|      243 |  3246 |  |
|        - |  3247 | `/*` |
|        - |  3248 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3249 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3250 | ` */` |
|      ! 0 |  3251 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3252 |  |
|        - |  3253 | `	ph7_vm *pVm;` |
|        - |  3254 | `	SyBlob sMsg;` |
|      ! 0 |  3255 | `	const char *zFuncName = 0;` |
|      ! 0 |  3256 | `	int nFuncLen = 0;` |
|        - |  3257 | `	va_list ap;` |
|        - |  3258 | `	sxi32 rc;` |
|        - |  3259 |  |
|      ! 0 |  3260 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3261 | `		return PH7_OK;` |
|        - |  3262 | `	}` |
|      ! 0 |  3263 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3264 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3265 | `		zClass = "Error";` |
|      ! 0 |  3266 | `	}` |
|        - |  3267 |  |
|      ! 0 |  3268 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3269 |  |
|      ! 0 |  3270 | `	va_start(ap,zFormat);` |
|      ! 0 |  3271 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3272 | `	va_end(ap);` |
|        - |  3273 |  |
|      ! 0 |  3274 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3275 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3276 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3277 | `	}` |
|      ! 0 |  3278 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3279 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3280 | `	}` |
|      ! 0 |  3281 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3282 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3283 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3284 | `	return rc;` |
|      ! 0 |  3285 |  |
|        - |  3286 | `/*` |
|        - |  3287 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3288 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3289 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3290 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3291 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3292 | ` * when VmByteCodeExec returns.` |
|        - |  3293 | ` */` |
|      144 |  3294 | `static sxi32 VmSuspendCtx(` |
|        - |  3295 | `	ph7_vm *pVm,` |
|        - |  3296 | `	ph7_exec_ctx *pCtx,` |
|        - |  3297 | `	sxi32 pc,` |
|        - |  3298 | `	sxi32 nTos` |
|        - |  3299 | `	)` |
|        2 |  3300 |  |
|       72 |  3301 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3302 | `	pCtx->pc = pc;` |
|      146 |  3303 | `	pCtx->nTos = nTos;` |
|      146 |  3304 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3305 | `	return PH7_SUSPEND;` |
|        2 |  3306 |  |
|        - |  3307 | `/*` |
|        - |  3308 | ` * Resolve named-argument mapping.` |
|        - |  3309 | ` *` |
|        - |  3310 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3311 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3312 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3313 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3314 | ` * every formal parameter that received a value.` |
|        - |  3315 | ` *` |
|        - |  3316 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3317 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3318 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3319 | ` */` |
|       92 |  3320 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3321 | `	ph7_vm *pVm,` |
|        - |  3322 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3323 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3324 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3325 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3326 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3327 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3328 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3329 |  |
|        2 |  3330 |  |
|       94 |  3331 | `	sxi32 posIdx = 0;` |
|        - |  3332 | `	sxu32 i;` |
|        - |  3333 | `	char zErrMsg[256];` |
|       94 |  3334 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      278 |  3335 | `	for( i = 0; i < nActual; i++ ){` |
|      186 |  3336 | `		aSlot[i] = -2;` |
|       94 |  3337 | `	}` |
|      272 |  3338 | `	for( i = 0; i < nActual; i++ ){` |
|      269 |  3339 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3340 | `			/* Named argument — find formal by name */` |
|      174 |  3341 | `			int found = 0;` |
|        - |  3342 | `			sxu32 k;` |
|      288 |  3343 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      274 |  3344 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      265 |  3345 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      252 |  3346 | `						pMap->aNames[i].zString,` |
|      378 |  3347 | `						pMap->aNames[i].nByte) == 0 ){` |
|      162 |  3348 | `					if( aUsed[k] ){` |
|        7 |  3349 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3350 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3351 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3352 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3353 | `						return PH7_ABORT;` |
|        - |  3354 | `					}` |
|      158 |  3355 | `					aSlot[i] = (sxi32)k;` |
|      158 |  3356 | `					aUsed[k] = 1;` |
|      158 |  3357 | `					found = 1;` |
|      158 |  3358 | `					break;` |
|        - |  3359 | `				}` |
|       59 |  3360 | `			}` |
|      170 |  3361 | `			if( !found ){` |
|       14 |  3362 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3363 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3364 | `				}else{` |
|        4 |  3365 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3366 | `						"Unknown named parameter $%.*s",` |
|        2 |  3367 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3368 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3369 | `					return PH7_ABORT;` |
|        - |  3370 | `				}` |
|        5 |  3371 | `			}` |
|       85 |  3372 | `		}else{` |
|        - |  3373 | `			/* Positional argument */` |
|       14 |  3374 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       14 |  3375 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3376 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3377 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3378 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3379 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3380 | `					return PH7_ABORT;` |
|        - |  3381 | `				}` |
|       14 |  3382 | `				aSlot[i] = posIdx;` |
|       14 |  3383 | `				aUsed[posIdx] = 1;` |
|        6 |  3384 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3385 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3386 | `			}` |
|       14 |  3387 | `			posIdx++;` |
|        - |  3388 | `		}` |
|       91 |  3389 | `	}` |
|       87 |  3390 | `	return SXRET_OK;` |
|       48 |  3391 |  |
|        - |  3392 | `/*` |
|        - |  3393 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3394 | ` *` |
|        - |  3395 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3396 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3397 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3398 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3399 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3400 | ` * then the program execution is halted.` |
|        - |  3401 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3402 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3403 | ` * or to reset the VM to it's initial state.` |
|        - |  3404 | ` */` |
|    39604 |  3405 | `static sxi32 VmByteCodeExec(` |
|        - |  3406 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3407 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3408 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3409 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3410 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3411 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3412 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3413 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3414 | `	)` |
|        2 |  3415 |  |
|        - |  3416 | `	VmInstr *pInstr;` |
|        - |  3417 | `	ph7_value *pTos;` |
|        - |  3418 | `	SySet aArg;` |
|        - |  3419 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3420 | `	sxi32 pc;` |
|        - |  3421 | `	sxi32 rc;` |
|        - |  3422 | `	/* Argument container */` |
|    39606 |  3423 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    39606 |  3424 | `	if( nTos < 0 ){` |
|    37208 |  3425 | `		pTos = &pStack[-1];` |
|    18605 |  3426 | `	}else{` |
|     2400 |  3427 | `		pTos = &pStack[nTos];` |
|        - |  3428 | `	}` |
|    39606 |  3429 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    39606 |  3430 | `	pc = nPc;` |
|        - |  3431 | `/*` |
|        - |  3432 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3433 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3434 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3435 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3436 | ` */` |
|        - |  3437 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3438 | `	{ \` |
|        - |  3439 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3440 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3441 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3442 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3443 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3444 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3445 | `				break; \` |
|        - |  3446 | `			} \` |
|        - |  3447 | `			goto Exception; \` |
|        - |  3448 | `		} \` |
|        - |  3449 | `	}` |
|        - |  3450 | `	/* Execute as much as we can */` |
|  5536575 |  3451 | `	for(;;){` |
|        - |  3452 | `		/* Fetch the instruction to execute */` |
| 11072448 |  3453 | `		pInstr = &aInstr[pc];` |
| 11072448 |  3454 | `		rc = SXRET_OK;` |
|        - |  3455 | `/*` |
|        - |  3456 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3457 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3458 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3459 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3460 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3461 | ` */` |
| 11072448 |  3462 | `		switch(pInstr->iOp){` |
|        - |  3463 | `/*` |
|        - |  3464 | ` * DONE: P1 * *` |
|        - |  3465 | ` *` |
|        - |  3466 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3467 | ` * and return immediately.` |
|        - |  3468 | ` */` |
|    19472 |  3469 | `case PH7_OP_DONE:` |
|    38946 |  3470 | `	if( pInstr->iP1 ){` |
|        - |  3471 | `#ifdef UNTRUST` |
|        - |  3472 | `		if( pTos < pStack ){` |
|        - |  3473 | `			goto Abort;` |
|        - |  3474 | `		}` |
|        - |  3475 | `#endif` |
|    23228 |  3476 | `		if( pLastRef ){` |
|    14480 |  3477 | `			*pLastRef = pTos->nIdx;` |
|     7239 |  3478 | `		}` |
|    23228 |  3479 | `		if( pResult ){` |
|        - |  3480 | `			/* Execution result */` |
|    21994 |  3481 | `			PH7_MemObjStore(pTos,pResult);` |
|    10996 |  3482 | `		}` |
|    23228 |  3483 | `		VmPopOperand(&pTos,1);` |
|    27333 |  3484 | `	}else if( pLastRef ){` |
|        - |  3485 | `		/* Nothing referenced */` |
|     1524 |  3486 | `		*pLastRef = SXU32_HIGH;` |
|      761 |  3487 | `	}` |
|        - |  3488 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3489 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3490 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3491 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3492 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3493 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3494 | `	 * block can override it.` |
|        - |  3495 | `	 */` |
|    38948 |  3496 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3497 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3498 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3499 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3500 | `		pExc->pFrame = 0;` |
|        3 |  3501 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3502 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3503 | `			pExc->iFinallyDone = 1;` |
|        - |  3504 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3505 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3506 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3507 | `				goto Abort;` |
|        - |  3508 | `			}` |
|        1 |  3509 | `		}` |
|        1 |  3510 | `	}` |
|    38946 |  3511 | `	goto Done;` |
|        - |  3512 | `/*` |
|        - |  3513 | ` * HALT: P1 * *` |
|        - |  3514 | ` *` |
|        - |  3515 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3516 | ` * and abort immediately.` |
|        - |  3517 | ` */` |
|        4 |  3518 | `case PH7_OP_HALT:` |
|        9 |  3519 | `	if( pInstr->iP1 ){` |
|        - |  3520 | `#ifdef UNTRUST` |
|        - |  3521 | `		if( pTos < pStack ){` |
|        - |  3522 | `			goto Abort;` |
|        - |  3523 | `		}` |
|        - |  3524 | `#endif` |
|        9 |  3525 | `		if( pLastRef ){` |
|      ! 0 |  3526 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3527 | `		}` |
|        9 |  3528 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3529 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3530 | `				/* Output the exit message */` |
|        7 |  3531 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3532 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3533 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3534 | `			}` |
|        7 |  3535 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3536 | `			/* Record exit status */` |
|        5 |  3537 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3538 | `		}` |
|        9 |  3539 | `		VmPopOperand(&pTos,1);` |
|        4 |  3540 | `	}else if( pLastRef ){` |
|        - |  3541 | `		/* Nothing referenced */` |
|      ! 0 |  3542 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3543 | `	}` |
|        - |  3544 | `	/* Check if we're in an included file context */` |
|        9 |  3545 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3546 | `		/* Terminate the entire process */` |
|        9 |  3547 | `		exit(pVm->iExitStatus);` |
|        - |  3548 | `	}` |
|      ! 0 |  3549 | `	goto Abort;` |
|        - |  3550 | `/*` |
|        - |  3551 | ` * JMP: * P2 *` |
|        - |  3552 | ` *` |
|        - |  3553 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3554 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3555 | ` */` |
|   236901 |  3556 | `case PH7_OP_JMP:` |
|   473848 |  3557 | `	pc = pInstr->iP2 - 1;` |
|   473848 |  3558 | `	break;` |
|        - |  3559 | `/*` |
|        - |  3560 | ` * JZ: P1 P2 *` |
|        - |  3561 | ` *` |
|        - |  3562 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3563 | ` * entry in the stack if P1 is zero.` |
|        - |  3564 | ` */` |
|   560003 |  3565 | `case PH7_OP_JZ:` |
|        - |  3566 | `#ifdef UNTRUST` |
|        - |  3567 | `	if( pTos < pStack ){` |
|        - |  3568 | `		goto Abort;` |
|        - |  3569 | `	}` |
|        - |  3570 | `#endif` |
|        - |  3571 | `	/* Get a boolean value */` |
|  1120096 |  3572 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  3573 | `		PH7_MemObjToBool(pTos);` |
|       85 |  3574 | `	}` |
|  1120096 |  3575 | `	if( !pTos->x.iVal ){` |
|        - |  3576 | `		/* Take the jump */` |
|   573404 |  3577 | `		pc = pInstr->iP2 - 1;` |
|   286701 |  3578 | `	}` |
|  1120096 |  3579 | `	if( !pInstr->iP1 ){` |
|   889658 |  3580 | `		VmPopOperand(&pTos,1);` |
|   444850 |  3581 | `	}` |
|  1120096 |  3582 | `	break;` |
|        - |  3583 | `/*` |
|        - |  3584 | ` * JNZ: P1 P2 *` |
|        - |  3585 | ` *` |
|        - |  3586 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3587 | ` * entry in the stack if P1 is zero.` |
|        - |  3588 | ` */` |
|    58588 |  3589 | `case PH7_OP_JNZ:` |
|        - |  3590 | `#ifdef UNTRUST` |
|        - |  3591 | `	if( pTos < pStack ){` |
|        - |  3592 | `		goto Abort;` |
|        - |  3593 | `	}` |
|        - |  3594 | `#endif` |
|        - |  3595 | `	/* Get a boolean value */` |
|   117178 |  3596 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3597 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3598 | `	}` |
|   117178 |  3599 | `	if( pTos->x.iVal ){` |
|        - |  3600 | `		/* Take the jump */` |
|     5160 |  3601 | `		pc = pInstr->iP2 - 1;` |
|     2579 |  3602 | `	}` |
|   117178 |  3603 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3604 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3605 | `	}` |
|   117178 |  3606 | `	break;` |
|        - |  3607 | `/*` |
|        - |  3608 | ` * NOOP: * * *` |
|        - |  3609 | ` *` |
|        - |  3610 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  3611 | ` * destination.` |
|        - |  3612 | ` */` |
|      ! 0 |  3613 | `case PH7_OP_NOOP:` |
|      ! 0 |  3614 | `	break;` |
|        - |  3615 | `/*` |
|        - |  3616 | ` * POP: P1 * *` |
|        - |  3617 | ` *` |
|        - |  3618 | ` * Pop P1 elements from the operand stack.` |
|        - |  3619 | ` */` |
|   433267 |  3620 | `case PH7_OP_POP: {` |
|   866580 |  3621 | `	sxi32 n = pInstr->iP1;` |
|   866580 |  3622 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3623 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3624 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3625 | `	}` |
|   866580 |  3626 | `	VmPopOperand(&pTos,n);` |
|   866580 |  3627 | `	break;` |
|        - |  3628 | `				 }` |
|        - |  3629 | `/*` |
|        - |  3630 | ` * DUP: * * *` |
|        - |  3631 | ` *` |
|        - |  3632 | ` * Duplicate the top of the stack.` |
|        - |  3633 | ` */` |
|       41 |  3634 | `case PH7_OP_DUP:` |
|        - |  3635 | `#ifdef UNTRUST` |
|        - |  3636 | `	if( pTos < pStack ){` |
|        - |  3637 | `		goto Abort;` |
|        - |  3638 | `	}` |
|        - |  3639 | `#endif` |
|       84 |  3640 | `	pTos++;` |
|       84 |  3641 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  3642 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  3643 | `	break;` |
|        - |  3644 | `/*` |
|        - |  3645 | ` * NSSWITCH: * * P3` |
|        - |  3646 | ` *` |
|        - |  3647 | ` * Switch the active namespace at runtime.` |
|        - |  3648 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  3649 | ` */` |
|     7277 |  3650 | `case PH7_OP_NSSWITCH:` |
|    14556 |  3651 | `	SyBlobReset(&pVm->sNamespace);` |
|    14556 |  3652 | `	if( pInstr->p3 ){` |
|       98 |  3653 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  3654 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  3655 | `	}` |
|        - |  3656 | `	/* Clear namespace-scoped use-const imports */` |
|    14556 |  3657 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    14556 |  3658 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    14556 |  3659 | `	break;` |
|        - |  3660 | `/* OP_USECONST P1 * P3` |
|        - |  3661 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  3662 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  3663 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  3664 | ` */` |
|        7 |  3665 | `case PH7_OP_USECONST: {` |
|       16 |  3666 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  3667 | `	if( azPair ){` |
|       16 |  3668 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  3669 | `	}` |
|       16 |  3670 | `	break;` |
|        - |  3671 | `				}` |
|        - |  3672 | `/*` |
|        - |  3673 | ` * CVT_INT: * * *` |
|        - |  3674 | ` *` |
|        - |  3675 | ` * Force the top of the stack to be an integer.` |
|        - |  3676 | ` */` |
|       77 |  3677 | `case PH7_OP_CVT_INT:` |
|        - |  3678 | `#ifdef UNTRUST` |
|        - |  3679 | `	if( pTos < pStack ){` |
|        - |  3680 | `		goto Abort;` |
|        - |  3681 | `	}` |
|        - |  3682 | `#endif` |
|      156 |  3683 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      109 |  3684 | `		PH7_MemObjToInteger(pTos);` |
|       54 |  3685 | `	}` |
|        - |  3686 | `	/* Invalidate any prior representation */` |
|      156 |  3687 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      156 |  3688 | `	break;` |
|        - |  3689 | `/*` |
|        - |  3690 | ` * CVT_REAL: * * *` |
|        - |  3691 | ` *` |
|        - |  3692 | ` * Force the top of the stack to be a real.` |
|        - |  3693 | ` */` |
|        4 |  3694 | `case PH7_OP_CVT_REAL:` |
|        - |  3695 | `#ifdef UNTRUST` |
|        - |  3696 | `	if( pTos < pStack ){` |
|        - |  3697 | `		goto Abort;` |
|        - |  3698 | `	}` |
|        - |  3699 | `#endif` |
|        9 |  3700 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3701 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3702 | `	}` |
|        - |  3703 | `	/* Invalidate any prior representation */` |
|        9 |  3704 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3705 | `	break;` |
|        - |  3706 | `/*` |
|        - |  3707 | ` * CVT_STR: * * *` |
|        - |  3708 | ` *` |
|        - |  3709 | ` * Force the top of the stack to be a string.` |
|        - |  3710 | ` */` |
|      146 |  3711 | `case PH7_OP_CVT_STR:` |
|        - |  3712 | `#ifdef UNTRUST` |
|        - |  3713 | `	if( pTos < pStack ){` |
|        - |  3714 | `		goto Abort;` |
|        - |  3715 | `	}` |
|        - |  3716 | `#endif` |
|      294 |  3717 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3718 | `		PH7_MemObjToString(pTos);` |
|      146 |  3719 | `	}` |
|      294 |  3720 | `	break;` |
|        - |  3721 | `/*` |
|        - |  3722 | ` * CVT_BOOL: * * *` |
|        - |  3723 | ` *` |
|        - |  3724 | ` * Force the top of the stack to be a boolean.` |
|        - |  3725 | ` */` |
|        5 |  3726 | `case PH7_OP_CVT_BOOL:` |
|        - |  3727 | `#ifdef UNTRUST` |
|        - |  3728 | `	if( pTos < pStack ){` |
|        - |  3729 | `		goto Abort;` |
|        - |  3730 | `	}` |
|        - |  3731 | `#endif` |
|       11 |  3732 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3733 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3734 | `	}` |
|       11 |  3735 | `	break;` |
|        - |  3736 | `/*` |
|        - |  3737 | ` * CVT_NULL: * * *` |
|        - |  3738 | ` *` |
|        - |  3739 | ` * Nullify the top of the stack.` |
|        - |  3740 | ` */` |
|        3 |  3741 | `case PH7_OP_CVT_NULL:` |
|        - |  3742 | `#ifdef UNTRUST` |
|        - |  3743 | `	if( pTos < pStack ){` |
|        - |  3744 | `		goto Abort;` |
|        - |  3745 | `	}` |
|        - |  3746 | `#endif` |
|        7 |  3747 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3748 | `	break;` |
|        - |  3749 | `/*` |
|        - |  3750 | ` * CVT_NUMC: * * *` |
|        - |  3751 | ` *` |
|        - |  3752 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3753 | ` */` |
|      ! 0 |  3754 | `case PH7_OP_CVT_NUMC:` |
|        - |  3755 | `#ifdef UNTRUST` |
|        - |  3756 | `	if( pTos < pStack ){` |
|        - |  3757 | `		goto Abort;` |
|        - |  3758 | `	}` |
|        - |  3759 | `#endif` |
|        - |  3760 | `	/* Force a numeric cast */` |
|      ! 0 |  3761 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3762 | `	break;` |
|        - |  3763 | `/*` |
|        - |  3764 | ` * CVT_ARRAY: * * *` |
|        - |  3765 | ` *` |
|        - |  3766 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3767 | ` */` |
|       10 |  3768 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3769 | `#ifdef UNTRUST` |
|        - |  3770 | `	if( pTos < pStack ){` |
|        - |  3771 | `		goto Abort;` |
|        - |  3772 | `	}` |
|        - |  3773 | `#endif` |
|        - |  3774 | `	/* Force a hashmap cast */` |
|       21 |  3775 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3776 | `	if( rc != SXRET_OK ){` |
|        - |  3777 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3778 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3779 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3780 | `	}` |
|       21 |  3781 | `	break;` |
|        - |  3782 | `/*` |
|        - |  3783 | ` * CVT_OBJ: * * *` |
|        - |  3784 | ` *` |
|        - |  3785 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3786 | ` */` |
|        8 |  3787 | `case PH7_OP_CVT_OBJ:` |
|        - |  3788 | `#ifdef UNTRUST` |
|        - |  3789 | `	if( pTos < pStack ){` |
|        - |  3790 | `		goto Abort;` |
|        - |  3791 | `	}` |
|        - |  3792 | `#endif` |
|       17 |  3793 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3794 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3795 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3796 | `	}` |
|       17 |  3797 | `	break;` |
|        - |  3798 | `/*` |
|        - |  3799 | ` * ERR_CTRL * * *` |
|        - |  3800 | ` *` |
|        - |  3801 | ` * Error control operator.` |
|        - |  3802 | ` */` |
|    14810 |  3803 | `case PH7_OP_ERR_CTRL:` |
|        - |  3804 | `	/*` |
|        - |  3805 | `	 * TICKET 1433-038:` |
|        - |  3806 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3807 | `	 * use the public API,to control error output.` |
|        - |  3808 | `	 */` |
|    29620 |  3809 | `	break;` |
|        - |  3810 | `/*` |
|        - |  3811 | ` * IS_A * * *` |
|        - |  3812 | ` *` |
|        - |  3813 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3814 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3815 | ` * holding a class name or an object).` |
|        - |  3816 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3817 | ` */` |
|       42 |  3818 | `case PH7_OP_IS_A:{` |
|       86 |  3819 | `	ph7_value *pNos = &pTos[-1];` |
|       86 |  3820 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3821 | `#ifdef UNTRUST` |
|        - |  3822 | `	if( pNos < pStack ){` |
|        - |  3823 | `		goto Abort;` |
|        - |  3824 | `	}` |
|        - |  3825 | `#endif` |
|       86 |  3826 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       84 |  3827 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       84 |  3828 | `		ph7_class *pClass = 0;` |
|        - |  3829 | `		/* Extract the target class */` |
|       84 |  3830 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3831 | `			/* Instance already loaded */` |
|      ! 0 |  3832 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       84 |  3833 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       84 |  3834 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       84 |  3835 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3836 | `			/* Handle self/static/parent keywords */` |
|       84 |  3837 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3838 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       82 |  3839 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3840 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       81 |  3841 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3842 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3843 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3844 | `					pClass = pSelf->pBase;` |
|        2 |  3845 | `				}` |
|        3 |  3846 | `			}else{` |
|       74 |  3847 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3848 | `			}` |
|       41 |  3849 | `		}` |
|       84 |  3850 | `		if( pClass ){` |
|        - |  3851 | `			/* Perform the query */` |
|       84 |  3852 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       41 |  3853 | `		}` |
|       41 |  3854 | `	}` |
|        - |  3855 | `	/* Push result */` |
|       86 |  3856 | `	VmPopOperand(&pTos,1);` |
|       86 |  3857 | `	PH7_MemObjRelease(pTos);` |
|       86 |  3858 | `	pTos->x.iVal = iRes;` |
|       86 |  3859 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       86 |  3860 | `	break;` |
|        - |  3861 | `				 }` |
|        - |  3862 |  |
|        - |  3863 | `/*` |
|        - |  3864 | ` * LOADC P1 P2 *` |
|        - |  3865 | ` *` |
|        - |  3866 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3867 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3868 | ` */` |
|   941697 |  3869 | `case PH7_OP_LOADC: {` |
|        - |  3870 | `	ph7_value *pObj;` |
|        - |  3871 | `	/* Reserve a room */` |
|  1883440 |  3872 | `	pTos++;` |
|  2816049 |  3873 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1883440 |  3874 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3875 | `			SyHashEntry *pEntry;` |
|        - |  3876 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3877 | `			{` |
|        - |  3878 | `				SyHashEntry *pConstImport;` |
|    27401 |  3879 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    18266 |  3880 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18268 |  3881 | `				if( pConstImport ){` |
|       11 |  3882 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3883 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3884 | `					if( pEntry ){` |
|       11 |  3885 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3886 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3887 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3888 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3889 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3890 | `						break;` |
|        - |  3891 | `					}` |
|        - |  3892 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3893 | `				}` |
|        - |  3894 | `			}` |
|        - |  3895 | `			/* Candidate for expansion via user defined callbacks */` |
|    18258 |  3896 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18258 |  3897 | `			if( pEntry ){` |
|    18254 |  3898 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3899 | `				/* Set a NULL default value */` |
|    18254 |  3900 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    18254 |  3901 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3902 | `				/* Invoke the callback and deal with the expanded value */` |
|    18254 |  3903 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3904 | `				/* Mark as constant */` |
|    18254 |  3905 | `				pTos->nIdx = SXU32_HIGH;` |
|    18254 |  3906 | `				break;` |
|        - |  3907 | `			}` |
|        - |  3908 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3909 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3910 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  3911 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  3912 | `			{` |
|        6 |  3913 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3914 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3915 | `				sxu32 j;` |
|        6 |  3916 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       14 |  3917 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|        9 |  3918 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|        5 |  3919 | `				}` |
|        6 |  3920 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3921 | `					/* Try current_namespace\name */` |
|      ! 0 |  3922 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3923 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3924 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3925 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3926 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3927 | `					if( pEntry ){` |
|      ! 0 |  3928 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3929 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3930 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3931 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3932 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3933 | `						break;` |
|        - |  3934 | `					}` |
|        - |  3935 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3936 | `				}` |
|        6 |  3937 | `				if( isQualified ){` |
|        - |  3938 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3939 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3940 | `					SyBlob sErr;` |
|        3 |  3941 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3942 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3943 | `					if( pErrFile ){` |
|        3 |  3944 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3945 | `					}` |
|        3 |  3946 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3947 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3948 | `					SyBlobRelease(&sErr);` |
|        3 |  3949 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3950 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3951 | `					goto LoadC_Done;` |
|        - |  3952 | `				}` |
|        - |  3953 | `			}` |
|        1 |  3954 | `		}` |
|  1865176 |  3955 | `		PH7_MemObjLoad(pObj,pTos);` |
|   932611 |  3956 | `	}else{` |
|        - |  3957 | `		/* Set a NULL value */` |
|      ! 0 |  3958 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3959 | `	}` |
|   932566 |  3960 | `LoadC_Done:` |
|        - |  3961 | `	/* Mark as constant */` |
|  1865178 |  3962 | `	pTos->nIdx = SXU32_HIGH;` |
|  1865178 |  3963 | `	break;` |
|        - |  3964 | `				  }` |
|        - |  3965 | `/*` |
|        - |  3966 | ` * LOAD: P1 * P3` |
|        - |  3967 | ` *` |
|        - |  3968 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3969 | ` * from the P3 operand.` |
|        - |  3970 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3971 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3972 | ` */` |
|  1488763 |  3973 | `case PH7_OP_LOAD:{` |
|        - |  3974 | `	ph7_value *pObj;` |
|        - |  3975 | `	SyString sName;` |
|  2977748 |  3976 | `	if( pInstr->p3 == 0 ){` |
|        - |  3977 | `		/* Take the variable name from the top of the stack */` |
|        - |  3978 | `#ifdef UNTRUST` |
|        - |  3979 | `		if( pTos < pStack ){` |
|        - |  3980 | `			goto Abort;` |
|        - |  3981 | `		}` |
|        - |  3982 | `#endif` |
|        - |  3983 | `		/* Force a string cast */` |
|       19 |  3984 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3985 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3986 | `		}` |
|       19 |  3987 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3988 | `	}else{` |
|  2977730 |  3989 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3990 | `		/* Reserve a room for the target object */` |
|  2977730 |  3991 | `		pTos++;` |
|        - |  3992 | `	}` |
|        - |  3993 | `	/* Extract the requested memory object */` |
|  2977748 |  3994 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2977748 |  3995 | `	if( pObj == 0 ){` |
|       28 |  3996 | `		if( pInstr->iP1 ){` |
|        - |  3997 | `			/* Variable not found,load NULL */` |
|       28 |  3998 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3999 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4000 | `			}else{` |
|       28 |  4001 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4002 | `			}` |
|       28 |  4003 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1488778 |  4004 | `			break;` |
|      ! 0 |  4005 | `		}else{` |
|        - |  4006 | `			/* Fatal error */` |
|      ! 0 |  4007 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4008 | `			goto Abort;` |
|        - |  4009 | `		}` |
|        - |  4010 | `	}` |
|        - |  4011 | `	/* Load variable contents */` |
|  2977722 |  4012 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2977722 |  4013 | `	pTos->nIdx = pObj->nIdx;` |
|  2977722 |  4014 | `	break;` |
|        - |  4015 | `				   }` |
|        - |  4016 | `/*` |
|        - |  4017 | ` * LOAD_MAP P1 * *` |
|        - |  4018 | ` *` |
|        - |  4019 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4020 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4021 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4022 | ` */` |
|    20994 |  4023 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4024 | `	ph7_hashmap *pMap;` |
|        - |  4025 | `	/* Allocate a new hashmap instance */` |
|    41990 |  4026 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    41990 |  4027 | `	if( pMap == 0 ){` |
|      ! 0 |  4028 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4029 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4030 | `		goto Abort;` |
|        - |  4031 | `	}` |
|    41990 |  4032 | `	if( pInstr->iP1 > 0 ){` |
|     2376 |  4033 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  4034 | `		/* Perform the insertion */` |
|     7288 |  4035 | `		while( pEntry < pTos ){` |
|     4914 |  4036 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4037 | `				/* Insertion by reference */` |
|      142 |  4038 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  4039 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  4040 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4041 | `					);` |
|       48 |  4042 | `			}else{` |
|        - |  4043 | `				/* Standard insertion */` |
|     7229 |  4044 | `				PH7_HashmapInsert(pMap,` |
|     4818 |  4045 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2409 |  4046 | `					&pEntry[1]` |
|        - |  4047 | `				);` |
|        - |  4048 | `			}` |
|        - |  4049 | `			/* Next pair on the stack */` |
|     4914 |  4050 | `			pEntry += 2;` |
|        2 |  4051 | `		}` |
|        - |  4052 | `		/* Pop P1 elements */` |
|     2376 |  4053 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1187 |  4054 | `	}` |
|        - |  4055 | `	/* Push the hashmap */` |
|    41990 |  4056 | `	pTos++;` |
|    41990 |  4057 | `	pTos->nIdx = SXU32_HIGH;` |
|    41990 |  4058 | `	pTos->x.pOther = pMap;` |
|    41990 |  4059 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    41990 |  4060 | `	break;` |
|        - |  4061 | `					  }` |
|        - |  4062 | `/*` |
|        - |  4063 | ` * LOAD_LIST: P1 * *` |
|        - |  4064 | ` *` |
|        - |  4065 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4066 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4067 | ` * Caveats:` |
|        - |  4068 | ` *  This implementation support only a single nesting level.` |
|        - |  4069 | ` */` |
|       48 |  4070 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4071 | `	ph7_value *pEntry;` |
|       98 |  4072 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4073 | `		/* Empty list,break immediately */` |
|      ! 0 |  4074 | `		break;` |
|        - |  4075 | `	}` |
|       98 |  4076 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4077 | `#ifdef UNTRUST` |
|        - |  4078 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4079 | `		goto Abort;` |
|        - |  4080 | `	}` |
|        - |  4081 | `#endif` |
|       98 |  4082 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4083 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4084 | `		ph7_hashmap_node *pNode;` |
|        - |  4085 | `		ph7_value sKey,*pObj;` |
|        - |  4086 | `		/* Start Copying */` |
|       91 |  4087 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4088 | `		while( pEntry <= pTos ){` |
|      193 |  4089 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4090 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4091 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4092 | `					if( rc == SXRET_OK ){` |
|        - |  4093 | `						/* Store node value */` |
|      165 |  4094 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4095 | `					}else{` |
|        - |  4096 | `						/* Undefined array key */` |
|        - |  4097 | `						char zMsg[128];` |
|      ! 0 |  4098 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4099 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4100 | `						PH7_MemObjRelease(pObj);` |
|        - |  4101 | `					}` |
|       82 |  4102 | `				}` |
|       82 |  4103 | `			}` |
|      193 |  4104 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4105 | `			pEntry++;` |
|        1 |  4106 | `		}` |
|       46 |  4107 | `	}else{` |
|        - |  4108 | `		/* Source is not an array */` |
|        - |  4109 | `		ph7_value *pObj;` |
|       18 |  4110 | `		while( pEntry <= pTos ){` |
|       12 |  4111 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4112 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4113 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4114 | `				}` |
|        5 |  4115 | `			}` |
|       12 |  4116 | `			pEntry++;` |
|        2 |  4117 | `		}` |
|        8 |  4118 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4119 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4120 | `			const char *zType = "unknown";` |
|        3 |  4121 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4122 | `			char zMsg[256];` |
|        3 |  4123 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4124 | `				zType = "string";` |
|        1 |  4125 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4126 | `				zType = "int";` |
|      ! 0 |  4127 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4128 | `				zType = "float";` |
|      ! 0 |  4129 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4130 | `				zType = "object";` |
|      ! 0 |  4131 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4132 | `				zType = "resource";` |
|      ! 0 |  4133 | `			}` |
|        3 |  4134 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4135 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4136 | `		}` |
|        - |  4137 | `	}` |
|       98 |  4138 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4139 | `	break;` |
|        - |  4140 | `					   }` |
|        - |  4141 | `/*` |
|        - |  4142 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4143 | ` *` |
|        - |  4144 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4145 | ` * from the stack.` |
|        - |  4146 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4147 | ` * instead.` |
|        - |  4148 | ` */` |
|   238995 |  4149 | `case PH7_OP_LOAD_IDX: {` |
|   478036 |  4150 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   478036 |  4151 | `	ph7_hashmap *pMap = 0;` |
|        - |  4152 | `	ph7_value *pIdx;` |
|   478036 |  4153 | `	pIdx = 0;` |
|   478036 |  4154 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4155 | `		if( !pInstr->iP2){` |
|        - |  4156 | `			/* No available index,load NULL */` |
|      ! 0 |  4157 | `			if( pTos >= pStack ){` |
|      ! 0 |  4158 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4159 | `			}else{` |
|        - |  4160 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4161 | `				pTos++;` |
|      ! 0 |  4162 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4163 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4164 | `			}` |
|        - |  4165 | `			/* Emit a notice */` |
|      ! 0 |  4166 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4167 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4168 | `			break;` |
|        - |  4169 | `		}` |
|      ! 0 |  4170 | `	}else{` |
|   478036 |  4171 | `		pIdx = pTos;` |
|   478036 |  4172 | `		pTos--;` |
|        - |  4173 | `	}` |
|   478036 |  4174 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4175 | `		/* String access */` |
|   372988 |  4176 | `		if( pIdx ){` |
|        - |  4177 | `			sxu32 nOfft;` |
|   372988 |  4178 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4179 | `				/* Force an int cast */` |
|      ! 0 |  4180 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4181 | `			}` |
|   372988 |  4182 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   372988 |  4183 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4184 | `				/* Invalid offset,load null */` |
|      ! 0 |  4185 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4186 | `			}else{` |
|   372988 |  4187 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   372988 |  4188 | `				int c = zData[nOfft];` |
|   372988 |  4189 | `				PH7_MemObjRelease(pTos);` |
|   372988 |  4190 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   372988 |  4191 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4192 | `			}` |
|   186517 |  4193 | `		}else{` |
|        - |  4194 | `			/* No available index,load NULL */` |
|      ! 0 |  4195 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4196 | `		}` |
|   372988 |  4197 | `		break;` |
|        - |  4198 | `	}` |
|   105050 |  4199 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4200 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4201 | `			ph7_value *pObj;` |
|        3 |  4202 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4203 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4204 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4205 | `			}` |
|        1 |  4206 | `		}` |
|        1 |  4207 | `	}` |
|   105050 |  4208 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   105050 |  4209 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   105050 |  4210 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4211 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4212 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4213 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4214 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      881 |  4215 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      440 |  4216 | `		}` |
|        - |  4217 | `		/* Point to the hashmap */` |
|   105050 |  4218 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   105050 |  4219 | `		if( pIdx ){` |
|        - |  4220 | `			/* Load the desired entry */` |
|   105050 |  4221 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    52524 |  4222 | `		}` |
|   105050 |  4223 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4224 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4225 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4226 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4227 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4228 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4229 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4230 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4231 | `			 * correct for the outermost write. */` |
|       19 |  4232 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4233 | `			if( !needWrite && pNode ){` |
|       13 |  4234 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4235 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4236 | `					needWrite = 1;` |
|        3 |  4237 | `				}` |
|        6 |  4238 | `			}` |
|       19 |  4239 | `			if( needWrite ){` |
|       13 |  4240 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4241 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4242 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4243 | `					 * into the new map's storage. */` |
|        7 |  4244 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4245 | `					if( pIdx ){` |
|        7 |  4246 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4247 | `					}` |
|        3 |  4248 | `				}` |
|        6 |  4249 | `			}` |
|        9 |  4250 | `		}` |
|   105050 |  4251 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4252 | `			/* Create a new empty entry */` |
|      273 |  4253 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4254 | `			if( rc == SXRET_OK ){` |
|        - |  4255 | `				/* Point to the last inserted entry */` |
|      273 |  4256 | `				pNode = pMap->pLast;` |
|      136 |  4257 | `			}` |
|      136 |  4258 | `		}` |
|    52524 |  4259 | `	}` |
|   105050 |  4260 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4261 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4262 | `		char zMsg[128];` |
|      ! 0 |  4263 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4264 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4265 | `		}` |
|      ! 0 |  4266 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4267 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4268 | `	}` |
|   105050 |  4269 | `	if( pIdx ){` |
|   105050 |  4270 | `		PH7_MemObjRelease(pIdx);` |
|    52524 |  4271 | `	}` |
|   105050 |  4272 | `	if( rc == SXRET_OK ){` |
|        - |  4273 | `		/* Load entry contents */` |
|    46994 |  4274 | `		if( pMap->iRef < 2 ){` |
|        - |  4275 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4276 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4277 | `			 */` |
|       24 |  4278 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4279 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4280 | `		}else{` |
|    46972 |  4281 | `			pTos->nIdx = pNode->nValIdx;` |
|    46972 |  4282 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    46972 |  4283 | `			PH7_HashmapUnref(pMap);` |
|        - |  4284 | `		}` |
|    23498 |  4285 | `	}else{` |
|        - |  4286 | `		/* No such entry,load NULL */` |
|    58058 |  4287 | `		PH7_MemObjRelease(pTos);` |
|    58058 |  4288 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4289 | `	}` |
|   105050 |  4290 | `	break;` |
|        - |  4291 | `					  }` |
|        - |  4292 | `/*` |
|        - |  4293 | ` * LOAD_CLOSURE * * P3` |
|        - |  4294 | ` *` |
|        - |  4295 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4296 | ` * name in the stack.` |
|        - |  4297 | ` */` |
|       45 |  4298 | `case PH7_OP_LOAD_CLOSURE:{` |
|       91 |  4299 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       91 |  4300 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4301 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4302 | `		ph7_vm_func *pClosure;` |
|        - |  4303 | `		char *zName;` |
|        - |  4304 | `		sxu32 mLen;` |
|        - |  4305 | `		sxu32 n;` |
|        - |  4306 | `		/* Create a new VM function */` |
|       91 |  4307 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4308 | `		/* Generate an unique closure name */` |
|       91 |  4309 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       91 |  4310 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4311 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4312 | `			goto Abort;` |
|        - |  4313 | `		}` |
|       91 |  4314 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       91 |  4315 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4316 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4317 | `		}` |
|        - |  4318 | `		/* Zero the stucture */` |
|       91 |  4319 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4320 | `		/* Perform a structure assignment on read-only items */` |
|       91 |  4321 | `		pClosure->aArgs = pFunc->aArgs;` |
|       91 |  4322 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       91 |  4323 | `		pClosure->aStatic = pFunc->aStatic;` |
|       91 |  4324 | `		pClosure->iFlags = pFunc->iFlags;` |
|       91 |  4325 | `		pClosure->pUserData = pFunc->pUserData;` |
|       91 |  4326 | `		pClosure->sSignature = pFunc->sSignature;` |
|       91 |  4327 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       91 |  4328 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       91 |  4329 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       91 |  4330 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       91 |  4331 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4332 | `		/* Register the closure */` |
|       91 |  4333 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4334 | `		/* Set up closure environment */` |
|       91 |  4335 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       91 |  4336 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      245 |  4337 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4338 | `			ph7_value *pValue;` |
|      155 |  4339 | `			pEnv = &aEnv[n];` |
|      155 |  4340 | `			sEnv.sName  = pEnv->sName;` |
|      155 |  4341 | `			sEnv.iFlags = pEnv->iFlags;` |
|      155 |  4342 | `			sEnv.nIdx = SXU32_HIGH;` |
|      155 |  4343 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      155 |  4344 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4345 | `				/* Pass by reference */` |
|      ! 0 |  4346 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4347 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4348 | `					);` |
|      ! 0 |  4349 | `			}` |
|        - |  4350 | `			/* Standard pass by value */` |
|      155 |  4351 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      155 |  4352 | `			if( pValue ){` |
|        - |  4353 | `				/* Copy imported value */` |
|       69 |  4354 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       34 |  4355 | `			}` |
|        - |  4356 | `			/* Insert the imported variable */` |
|      155 |  4357 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       78 |  4358 | `		}` |
|        - |  4359 | `		/* Finally,load the closure name on the stack */` |
|       91 |  4360 | `		pTos++;` |
|       91 |  4361 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       45 |  4362 | `	}` |
|       91 |  4363 | `	break;` |
|        - |  4364 | `						 }` |
|        - |  4365 | `/*` |
|        - |  4366 | ` * STORE * P2 P3` |
|        - |  4367 | ` *` |
|        - |  4368 | ` * Perform a store (Assignment) operation.` |
|        - |  4369 | ` */` |
|   131580 |  4370 | `case PH7_OP_STORE: {` |
|        - |  4371 | `	ph7_value *pObj;` |
|        - |  4372 | `	SyString sName;` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `	if( pTos < pStack ){` |
|        - |  4375 | `		goto Abort;` |
|        - |  4376 | `	}` |
|        - |  4377 | `#endif` |
|   263162 |  4378 | `	if( pInstr->iP2 ){` |
|        - |  4379 | `		sxu32 nIdx;` |
|        - |  4380 | `		sxi32 rcT;` |
|        - |  4381 | `		/* Member store operation */` |
|     4242 |  4382 | `		nIdx = pTos->nIdx;` |
|     4242 |  4383 | `		VmPopOperand(&pTos,1);` |
|     4242 |  4384 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4385 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4386 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4387 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4388 | `		}else{` |
|        - |  4389 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4390 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     4238 |  4391 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     4238 |  4392 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4393 | `				goto Abort;` |
|        - |  4394 | `			}` |
|     4238 |  4395 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4396 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4397 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4398 | `				 * propagate out of the VM loop. */` |
|       37 |  4399 | `				VmPopOperand(&pTos,1);` |
|        - |  4400 | `				{` |
|       37 |  4401 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  4402 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  4403 | `						pc = pFrm2->iExceptionJump - 1;` |
|   131599 |  4404 | `						break;` |
|        - |  4405 | `					}` |
|        - |  4406 | `				}` |
|      ! 0 |  4407 | `				goto Exception;` |
|        - |  4408 | `			}` |
|        - |  4409 | `			/* Point to the desired memory object */` |
|     4202 |  4410 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     4202 |  4411 | `			if( pObj ){` |
|        - |  4412 | `				/* Perform the store operation */` |
|     4202 |  4413 | `				PH7_MemObjStore(pTos,pObj);` |
|     2100 |  4414 | `			}` |
|        - |  4415 | `		}` |
|     4206 |  4416 | `		break;` |
|   258922 |  4417 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4418 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4419 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4420 | `			/* Force a string cast */` |
|      ! 0 |  4421 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4422 | `		}` |
|        7 |  4423 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4424 | `		pTos--;` |
|        - |  4425 | `#ifdef UNTRUST` |
|        - |  4426 | `		if( pTos < pStack  ){` |
|        - |  4427 | `			goto Abort;` |
|        - |  4428 | `		}` |
|        - |  4429 | `#endif` |
|        4 |  4430 | `	}else{` |
|   258916 |  4431 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4432 | `	}` |
|        - |  4433 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   258922 |  4434 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   258922 |  4435 | `	if( pObj == 0 ){` |
|      ! 0 |  4436 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4437 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4438 | `		goto Abort;` |
|        - |  4439 | `	}` |
|   258922 |  4440 | `	if( !pInstr->p3 ){` |
|        7 |  4441 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4442 | `	}` |
|        - |  4443 | `	/* Perform the store operation */` |
|   258922 |  4444 | `	PH7_MemObjStore(pTos,pObj);` |
|   258922 |  4445 | `	break;` |
|        - |  4446 | `				   }` |
|        - |  4447 | `/*` |
|        - |  4448 | ` * STORE_IDX:   P1 * P3` |
|        - |  4449 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4450 | ` *` |
|        - |  4451 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4452 | ` */` |
|    91144 |  4453 | `case PH7_OP_STORE_IDX:` |
|        - |  4454 | `case PH7_OP_STORE_IDX_REF: {` |
|   182290 |  4455 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4456 | `	ph7_value *pKey;` |
|        - |  4457 | `	sxu32 nIdx;` |
|   182290 |  4458 | `	if( pInstr->iP1 ){` |
|        - |  4459 | `		/* Key is next on stack */` |
|    61064 |  4460 | `		pKey = pTos;` |
|    61064 |  4461 | `		pTos--;` |
|    30533 |  4462 | `	}else{` |
|   121228 |  4463 | `		pKey = 0;` |
|        - |  4464 | `	}` |
|   182290 |  4465 | `	nIdx = pTos->nIdx;` |
|   182290 |  4466 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4467 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4468 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4469 | `		 * checking true sharing count, then re-add after separation. */` |
|   182238 |  4470 | `		if( nIdx != SXU32_HIGH ){` |
|   182238 |  4471 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   273356 |  4472 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   182238 |  4473 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4474 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4475 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4476 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4477 | `				 * refcounts if the backing array was already separated. */` |
|   182238 |  4478 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   182238 |  4479 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   182238 |  4480 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   182238 |  4481 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   182238 |  4482 | `					pTos->x.pOther = pMap;` |
|    91120 |  4483 | `				}else{` |
|        - |  4484 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4485 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4486 | `					pMap = pCur;` |
|        - |  4487 | `				}` |
|    91120 |  4488 | `			}else{` |
|      ! 0 |  4489 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4490 | `			}` |
|    91120 |  4491 | `		}else{` |
|      ! 0 |  4492 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4493 | `		}` |
|   182238 |  4494 | `		if( pMap->iRef < 2 ){` |
|        - |  4495 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4496 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4497 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4498 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4499 | `			pMap->iRef = 2;` |
|      ! 0 |  4500 | `		}` |
|    91120 |  4501 | `	}else{` |
|        - |  4502 | `		ph7_value *pObj;` |
|       53 |  4503 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4504 | `		if( pObj == 0 ){` |
|      ! 0 |  4505 | `			if( pKey ){` |
|      ! 0 |  4506 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4507 | `			}` |
|      ! 0 |  4508 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4509 | `			break;` |
|        - |  4510 | `		}` |
|        - |  4511 | `		/* Phase#1: Load the array */` |
|       53 |  4512 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4513 | `			VmPopOperand(&pTos,1);` |
|       53 |  4514 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4515 | `				/* Force a string cast */` |
|      ! 0 |  4516 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4517 | `			}` |
|       53 |  4518 | `			if( pKey == 0 ){` |
|        - |  4519 | `				/* Append string */` |
|        3 |  4520 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4521 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4522 | `				}` |
|        2 |  4523 | `			}else{` |
|        - |  4524 | `				sxu32 nOfft;` |
|       51 |  4525 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4526 | `					/* Force an int cast */` |
|       51 |  4527 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4528 | `				}` |
|       51 |  4529 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4530 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4531 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4532 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4533 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4534 | `				}else{` |
|      ! 0 |  4535 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4536 | `						/* Perform an append operation */` |
|      ! 0 |  4537 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4538 | `					}` |
|        - |  4539 | `				}` |
|        - |  4540 | `			}` |
|       53 |  4541 | `			if( pKey ){` |
|       51 |  4542 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4543 | `			}` |
|       53 |  4544 | `			break;` |
|      ! 0 |  4545 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4546 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4547 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4548 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4549 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4550 | `				goto Abort;` |
|        - |  4551 | `			}` |
|      ! 0 |  4552 | `		}` |
|        - |  4553 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4554 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4555 | `	}` |
|   182238 |  4556 | `	VmPopOperand(&pTos,1);` |
|        - |  4557 | `	/* Phase#2: Perform the insertion */` |
|   182238 |  4558 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4559 | `		/* Insertion by reference */` |
|       15 |  4560 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4561 | `	}else{` |
|   182224 |  4562 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4563 | `	}` |
|   182238 |  4564 | `	if( pKey ){` |
|    61014 |  4565 | `		PH7_MemObjRelease(pKey);` |
|    30506 |  4566 | `	}` |
|   182238 |  4567 | `	break;` |
|        - |  4568 | `					   }` |
|        - |  4569 | `/*` |
|        - |  4570 | ` * INCR: P1 * *` |
|        - |  4571 | ` *` |
|        - |  4572 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4573 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4574 | ` * the stack and increment after that.` |
|        - |  4575 | ` */` |
|   162120 |  4576 | `case PH7_OP_INCR:` |
|        - |  4577 | `#ifdef UNTRUST` |
|        - |  4578 | `	if( pTos < pStack ){` |
|        - |  4579 | `		goto Abort;` |
|        - |  4580 | `	}` |
|        - |  4581 | `#endif` |
|   324286 |  4582 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   324286 |  4583 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4584 | `			ph7_value *pObj;` |
|   324286 |  4585 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4586 | `				/* Force a numeric cast */` |
|   324286 |  4587 | `				PH7_MemObjToNumeric(pObj);` |
|   324286 |  4588 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4589 | `					pObj->rVal++;` |
|        - |  4590 | `					/* Try to get an integer representation */` |
|      ! 0 |  4591 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4592 | `				}else{` |
|   324286 |  4593 | `					pObj->x.iVal++;` |
|   324286 |  4594 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4595 | `				}` |
|   324286 |  4596 | `				if( pInstr->iP1 ){` |
|        - |  4597 | `					/* Pre-icrement */` |
|       77 |  4598 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4599 | `				}` |
|   162164 |  4600 | `			}` |
|   162166 |  4601 | `		}else{` |
|      ! 0 |  4602 | `			if( pInstr->iP1 ){` |
|        - |  4603 | `				/* Force a numeric cast */` |
|      ! 0 |  4604 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  4605 | `				/* Pre-increment */` |
|      ! 0 |  4606 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4607 | `					pTos->rVal++;` |
|        - |  4608 | `					/* Try to get an integer representation */` |
|      ! 0 |  4609 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4610 | `				}else{` |
|      ! 0 |  4611 | `					pTos->x.iVal++;` |
|      ! 0 |  4612 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4613 | `				}` |
|      ! 0 |  4614 | `			}` |
|        - |  4615 | `		}` |
|   162164 |  4616 | `	}` |
|   324286 |  4617 | `	break;` |
|        - |  4618 | `/*` |
|        - |  4619 | ` * DECR: P1 * *` |
|        - |  4620 | ` *` |
|        - |  4621 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  4622 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  4623 | ` * and decrement after that.` |
|        - |  4624 | ` */` |
|        2 |  4625 | `case PH7_OP_DECR:` |
|        - |  4626 | `#ifdef UNTRUST` |
|        - |  4627 | `	if( pTos < pStack ){` |
|        - |  4628 | `		goto Abort;` |
|        - |  4629 | `	}` |
|        - |  4630 | `#endif` |
|        5 |  4631 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  4632 | `		/* Force a numeric cast */` |
|        5 |  4633 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  4634 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4635 | `			ph7_value *pObj;` |
|        5 |  4636 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4637 | `				/* Force a numeric cast */` |
|        5 |  4638 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  4639 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4640 | `					pObj->rVal--;` |
|        - |  4641 | `					/* Try to get an integer representation */` |
|      ! 0 |  4642 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4643 | `				}else{` |
|        5 |  4644 | `					pObj->x.iVal--;` |
|        5 |  4645 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4646 | `				}` |
|        5 |  4647 | `				if( pInstr->iP1 ){` |
|        - |  4648 | `					/* Pre-icrement */` |
|      ! 0 |  4649 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  4650 | `				}` |
|        2 |  4651 | `			}` |
|        3 |  4652 | `		}else{` |
|      ! 0 |  4653 | `			if( pInstr->iP1 ){` |
|        - |  4654 | `				/* Pre-increment */` |
|      ! 0 |  4655 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4656 | `					pTos->rVal--;` |
|        - |  4657 | `					/* Try to get an integer representation */` |
|      ! 0 |  4658 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4659 | `				}else{` |
|      ! 0 |  4660 | `					pTos->x.iVal--;` |
|      ! 0 |  4661 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4662 | `				}` |
|      ! 0 |  4663 | `			}` |
|        - |  4664 | `		}` |
|        2 |  4665 | `	}` |
|        5 |  4666 | `	break;` |
|        - |  4667 | `/*` |
|        - |  4668 | ` * UMINUS: * * *` |
|        - |  4669 | ` *` |
|        - |  4670 | ` * Perform a unary minus operation.` |
|        - |  4671 | ` */` |
|    27374 |  4672 | `case PH7_OP_UMINUS:` |
|        - |  4673 | `#ifdef UNTRUST` |
|        - |  4674 | `	if( pTos < pStack ){` |
|        - |  4675 | `		goto Abort;` |
|        - |  4676 | `	}` |
|        - |  4677 | `#endif` |
|        - |  4678 | `	/* Force a numeric (integer,real or both) cast */` |
|    54750 |  4679 | `	PH7_MemObjToNumeric(pTos);` |
|    54750 |  4680 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  4681 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  4682 | `	}` |
|    54750 |  4683 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    54720 |  4684 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    27359 |  4685 | `	}` |
|    54750 |  4686 | `	break;` |
|        - |  4687 | `/*` |
|        - |  4688 | ` * UPLUS: * * *` |
|        - |  4689 | ` *` |
|        - |  4690 | ` * Perform a unary plus operation.` |
|        - |  4691 | ` */` |
|       18 |  4692 | `case PH7_OP_UPLUS:` |
|        - |  4693 | `#ifdef UNTRUST` |
|        - |  4694 | `	if( pTos < pStack ){` |
|        - |  4695 | `		goto Abort;` |
|        - |  4696 | `	}` |
|        - |  4697 | `#endif` |
|        - |  4698 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  4699 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  4700 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4701 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  4702 | `	}` |
|       37 |  4703 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  4704 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  4705 | `	}` |
|       37 |  4706 | `	break;` |
|        - |  4707 | `/*` |
|        - |  4708 | ` * OP_LNOT: * * *` |
|        - |  4709 | ` *` |
|        - |  4710 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  4711 | ` * with its complement.` |
|        - |  4712 | ` */` |
|    42690 |  4713 | `case PH7_OP_LNOT:` |
|        - |  4714 | `#ifdef UNTRUST` |
|        - |  4715 | `	if( pTos < pStack ){` |
|        - |  4716 | `		goto Abort;` |
|        - |  4717 | `	}` |
|        - |  4718 | `#endif` |
|        - |  4719 | `	/* Force a boolean cast */` |
|    85426 |  4720 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  4721 | `		PH7_MemObjToBool(pTos);` |
|       10 |  4722 | `	}` |
|    85426 |  4723 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    85426 |  4724 | `	break;` |
|        - |  4725 | `/*` |
|        - |  4726 | ` * OP_BITNOT: * * *` |
|        - |  4727 | ` *` |
|        - |  4728 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  4729 | ` * with its ones-complement.` |
|        - |  4730 | ` */` |
|       13 |  4731 | `case PH7_OP_BITNOT:` |
|        - |  4732 | `#ifdef UNTRUST` |
|        - |  4733 | `	if( pTos < pStack ){` |
|        - |  4734 | `		goto Abort;` |
|        - |  4735 | `	}` |
|        - |  4736 | `#endif` |
|        - |  4737 | `	/* Force an integer cast */` |
|       28 |  4738 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4739 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4740 | `	}` |
|       28 |  4741 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  4742 | `	break;` |
|        - |  4743 | `/* OP_MUL * * *` |
|        - |  4744 | ` * OP_MUL_STORE * * *` |
|        - |  4745 | ` *` |
|        - |  4746 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  4747 | ` * and push the result back onto the stack.` |
|        - |  4748 | ` */` |
|     1278 |  4749 | `case PH7_OP_MUL:` |
|        - |  4750 | `case PH7_OP_MUL_STORE: {` |
|     2558 |  4751 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4752 | `	/* Force the operand to be numeric */` |
|        - |  4753 | `#ifdef UNTRUST` |
|        - |  4754 | `	if( pNos < pStack ){` |
|        - |  4755 | `		goto Abort;` |
|        - |  4756 | `	}` |
|        - |  4757 | `#endif` |
|     2558 |  4758 | `	PH7_MemObjToNumeric(pTos);` |
|     2558 |  4759 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  4760 | `	/* Perform the requested operation */` |
|     2558 |  4761 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4762 | `		/* Floating point arithemic */` |
|        - |  4763 | `		ph7_real a,b,r;` |
|       19 |  4764 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  4765 | `			PH7_MemObjToReal(pTos);` |
|        4 |  4766 | `		}` |
|       19 |  4767 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4768 | `			PH7_MemObjToReal(pNos);` |
|        3 |  4769 | `		}` |
|       19 |  4770 | `		a = pNos->rVal;` |
|       19 |  4771 | `		b = pTos->rVal;` |
|       19 |  4772 | `		r = a * b;` |
|        - |  4773 | `		/* Push the result */` |
|       19 |  4774 | `		pNos->rVal = r;` |
|       19 |  4775 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4776 | `		/* Try to get an integer representation */` |
|       19 |  4777 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  4778 | `	}else{` |
|        - |  4779 | `		/* Integer arithmetic */` |
|        - |  4780 | `		sxi64 a,b,r;` |
|     2540 |  4781 | `		a = pNos->x.iVal;` |
|     2540 |  4782 | `		b = pTos->x.iVal;` |
|     2540 |  4783 | `		r = a * b;` |
|        - |  4784 | `		/* Push the result */` |
|     2540 |  4785 | `		pNos->x.iVal = r;` |
|     2540 |  4786 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4787 | `	}` |
|     2558 |  4788 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4789 | `		ph7_value *pObj;` |
|       32 |  4790 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4791 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  4792 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  4793 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  4794 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  4795 | `		}` |
|       15 |  4796 | `	}` |
|     2558 |  4797 | `	VmPopOperand(&pTos,1);` |
|     2558 |  4798 | `	break;` |
|        - |  4799 | `				 }` |
|        - |  4800 | `/* OP_ADD * * *` |
|        - |  4801 | ` *` |
|        - |  4802 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4803 | ` * and push the result back onto the stack.` |
|        - |  4804 | ` */` |
|      492 |  4805 | `case PH7_OP_ADD:{` |
|      986 |  4806 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4807 | `#ifdef UNTRUST` |
|        - |  4808 | `	if( pNos < pStack ){` |
|        - |  4809 | `		goto Abort;` |
|        - |  4810 | `	}` |
|        - |  4811 | `#endif` |
|        - |  4812 | `	/* Perform the addition */` |
|      986 |  4813 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      986 |  4814 | `	VmPopOperand(&pTos,1);` |
|      986 |  4815 | `	break;` |
|        - |  4816 | `				}` |
|        - |  4817 | `/*` |
|        - |  4818 | ` * OP_ADD_STORE * * *` |
|        - |  4819 | ` *` |
|        - |  4820 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4821 | ` * and push the result back onto the stack.` |
|        - |  4822 | ` */` |
|      502 |  4823 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  4824 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4825 | `	ph7_value *pObj;` |
|        - |  4826 | `	sxu32 nIdx;` |
|        - |  4827 | `#ifdef UNTRUST` |
|        - |  4828 | `	if( pNos < pStack ){` |
|        - |  4829 | `		goto Abort;` |
|        - |  4830 | `	}` |
|        - |  4831 | `#endif` |
|        - |  4832 | `	/* Perform the addition */` |
|     1006 |  4833 | `	nIdx = pTos->nIdx;` |
|     1006 |  4834 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4835 | `	/* Peform the store operation */` |
|     1006 |  4836 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4837 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  4838 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  4839 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  4840 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  4841 | `	}` |
|        - |  4842 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  4843 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  4844 | `	VmPopOperand(&pTos,1);` |
|     1006 |  4845 | `	break;` |
|        - |  4846 | `				}` |
|        - |  4847 | `/* OP_SUB * * *` |
|        - |  4848 | ` *` |
|        - |  4849 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4850 | ` * first (what was next on the stack) from the second (the` |
|        - |  4851 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4852 | ` */` |
|      302 |  4853 | `case PH7_OP_SUB: {` |
|      606 |  4854 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4855 | `#ifdef UNTRUST` |
|        - |  4856 | `	if( pNos < pStack ){` |
|        - |  4857 | `		goto Abort;` |
|        - |  4858 | `	}` |
|        - |  4859 | `#endif` |
|      606 |  4860 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4861 | `		/* Floating point arithemic */` |
|        - |  4862 | `		ph7_real a,b,r;` |
|       95 |  4863 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4864 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4865 | `		}` |
|       95 |  4866 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4867 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4868 | `		}` |
|       95 |  4869 | `		a = pNos->rVal;` |
|       95 |  4870 | `		b = pTos->rVal;` |
|       95 |  4871 | `		r = a - b;` |
|        - |  4872 | `		/* Push the result */` |
|       95 |  4873 | `		pNos->rVal = r;` |
|       95 |  4874 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4875 | `		/* Try to get an integer representation */` |
|       95 |  4876 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4877 | `	}else{` |
|        - |  4878 | `		/* Integer arithmetic */` |
|        - |  4879 | `		sxi64 a,b,r;` |
|      512 |  4880 | `		a = pNos->x.iVal;` |
|      512 |  4881 | `		b = pTos->x.iVal;` |
|      512 |  4882 | `		r = a - b;` |
|        - |  4883 | `		/* Push the result */` |
|      512 |  4884 | `		pNos->x.iVal = r;` |
|      512 |  4885 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4886 | `	}` |
|      606 |  4887 | `	VmPopOperand(&pTos,1);` |
|      606 |  4888 | `	break;` |
|        - |  4889 | `				 }` |
|        - |  4890 | `/* OP_SUB_STORE * * *` |
|        - |  4891 | ` *` |
|        - |  4892 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4893 | ` * first (what was next on the stack) from the second (the` |
|        - |  4894 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4895 | ` */` |
|        4 |  4896 | `case PH7_OP_SUB_STORE: {` |
|       10 |  4897 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4898 | `	ph7_value *pObj;` |
|        - |  4899 | `#ifdef UNTRUST` |
|        - |  4900 | `	if( pNos < pStack ){` |
|        - |  4901 | `		goto Abort;` |
|        - |  4902 | `	}` |
|        - |  4903 | `#endif` |
|       10 |  4904 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4905 | `		/* Floating point arithemic */` |
|        - |  4906 | `		ph7_real a,b,r;` |
|      ! 0 |  4907 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4908 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4909 | `		}` |
|      ! 0 |  4910 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4911 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4912 | `		}` |
|      ! 0 |  4913 | `		a = pTos->rVal;` |
|      ! 0 |  4914 | `		b = pNos->rVal;` |
|      ! 0 |  4915 | `		r = a - b;` |
|        - |  4916 | `		/* Push the result */` |
|      ! 0 |  4917 | `		pNos->rVal = r;` |
|      ! 0 |  4918 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4919 | `		/* Try to get an integer representation */` |
|      ! 0 |  4920 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4921 | `	}else{` |
|        - |  4922 | `		/* Integer arithmetic */` |
|        - |  4923 | `		sxi64 a,b,r;` |
|       10 |  4924 | `		a = pTos->x.iVal;` |
|       10 |  4925 | `		b = pNos->x.iVal;` |
|       10 |  4926 | `		r = a - b;` |
|        - |  4927 | `		/* Push the result */` |
|       10 |  4928 | `		pNos->x.iVal = r;` |
|       10 |  4929 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4930 | `	}` |
|       10 |  4931 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4932 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  4933 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  4934 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  4935 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  4936 | `	}` |
|       10 |  4937 | `	VmPopOperand(&pTos,1);` |
|       10 |  4938 | `	break;` |
|        - |  4939 | `				 }` |
|        - |  4940 |  |
|        - |  4941 | `/*` |
|        - |  4942 | ` * OP_MOD * * *` |
|        - |  4943 | ` *` |
|        - |  4944 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4945 | ` * first (what was next on the stack) from the second (the` |
|        - |  4946 | ` * top of the stack) and push the remainder after division` |
|        - |  4947 | ` * onto the stack.` |
|        - |  4948 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4949 | ` */` |
|      307 |  4950 | `case PH7_OP_MOD:{` |
|      616 |  4951 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4952 | `	sxi64 a,b,r;` |
|        - |  4953 | `#ifdef UNTRUST` |
|        - |  4954 | `	if( pNos < pStack ){` |
|        - |  4955 | `		goto Abort;` |
|        - |  4956 | `	}` |
|        - |  4957 | `#endif` |
|        - |  4958 | `	/* Force the operands to be integer */` |
|      616 |  4959 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4960 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4961 | `	}` |
|      616 |  4962 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4963 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4964 | `	}` |
|        - |  4965 | `	/* Perform the requested operation */` |
|      616 |  4966 | `	a = pNos->x.iVal;` |
|      616 |  4967 | `	b = pTos->x.iVal;` |
|      616 |  4968 | `	if( b == 0 ){` |
|        3 |  4969 | `		r = 0;` |
|        3 |  4970 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4971 | `		/* goto Abort; */` |
|        2 |  4972 | `	}else{` |
|      613 |  4973 | `		r = a%b;` |
|        - |  4974 | `	}` |
|        - |  4975 | `	/* Push the result */` |
|      616 |  4976 | `	pNos->x.iVal = r;` |
|      616 |  4977 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      616 |  4978 | `	VmPopOperand(&pTos,1);` |
|      616 |  4979 | `	break;` |
|        - |  4980 | `				}` |
|        - |  4981 | `/*` |
|        - |  4982 | ` * OP_MOD_STORE * * *` |
|        - |  4983 | ` *` |
|        - |  4984 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4985 | ` * first (what was next on the stack) from the second (the` |
|        - |  4986 | ` * top of the stack) and push the remainder after division` |
|        - |  4987 | ` * onto the stack.` |
|        - |  4988 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4989 | ` */` |
|        1 |  4990 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4991 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4992 | `	ph7_value *pObj;` |
|        - |  4993 | `	sxi64 a,b,r;` |
|        - |  4994 | `#ifdef UNTRUST` |
|        - |  4995 | `	if( pNos < pStack ){` |
|        - |  4996 | `		goto Abort;` |
|        - |  4997 | `	}` |
|        - |  4998 | `#endif` |
|        - |  4999 | `	/* Force the operands to be integer */` |
|        3 |  5000 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5001 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5002 | `	}` |
|        3 |  5003 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5004 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5005 | `	}` |
|        - |  5006 | `	/* Perform the requested operation */` |
|        3 |  5007 | `	a = pTos->x.iVal;` |
|        3 |  5008 | `	b = pNos->x.iVal;` |
|        3 |  5009 | `	if( b == 0 ){` |
|      ! 0 |  5010 | `		r = 0;` |
|      ! 0 |  5011 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5012 | `		/* goto Abort; */` |
|      ! 0 |  5013 | `	}else{` |
|        3 |  5014 | `		r = a%b;` |
|        - |  5015 | `	}` |
|        - |  5016 | `	/* Push the result */` |
|        3 |  5017 | `	pNos->x.iVal = r;` |
|        3 |  5018 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  5019 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5020 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  5021 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5022 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  5023 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  5024 | `	}` |
|        3 |  5025 | `	VmPopOperand(&pTos,1);` |
|        3 |  5026 | `	break;` |
|        - |  5027 | `				}` |
|        - |  5028 | `/*` |
|        - |  5029 | ` * OP_DIV * * *` |
|        - |  5030 | ` *` |
|        - |  5031 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5032 | ` * first (what was next on the stack) from the second (the` |
|        - |  5033 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5034 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5035 | ` */` |
|       30 |  5036 | `case PH7_OP_DIV:{` |
|       62 |  5037 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5038 | `	ph7_real a,b,r;` |
|        - |  5039 | `#ifdef UNTRUST` |
|        - |  5040 | `	if( pNos < pStack ){` |
|        - |  5041 | `		goto Abort;` |
|        - |  5042 | `	}` |
|        - |  5043 | `#endif` |
|        - |  5044 | `	/* Force the operands to be real */` |
|       62 |  5045 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       58 |  5046 | `		PH7_MemObjToReal(pTos);` |
|       28 |  5047 | `	}` |
|       62 |  5048 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       24 |  5049 | `		PH7_MemObjToReal(pNos);` |
|       11 |  5050 | `	}` |
|        - |  5051 | `	/* Perform the requested operation */` |
|       62 |  5052 | `	a = pNos->rVal;` |
|       62 |  5053 | `	b = pTos->rVal;` |
|       62 |  5054 | `	if( b == 0 ){` |
|        - |  5055 | `		/* Division by zero */` |
|        3 |  5056 | `		pNos->rVal = 0;` |
|        3 |  5057 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  5058 | `		/* goto Abort; */` |
|        2 |  5059 | `	}else{` |
|       59 |  5060 | `		r = a/b;` |
|        - |  5061 | `		/* Push the result */` |
|       59 |  5062 | `		pNos->rVal = r;` |
|       59 |  5063 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5064 | `		/* Try to get an integer representation */` |
|       59 |  5065 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5066 | `	}` |
|       62 |  5067 | `	VmPopOperand(&pTos,1);` |
|       62 |  5068 | `	break;` |
|        - |  5069 | `				}` |
|        - |  5070 | `/*` |
|        - |  5071 | ` * OP_DIV_STORE * * *` |
|        - |  5072 | ` *` |
|        - |  5073 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5074 | ` * first (what was next on the stack) from the second (the` |
|        - |  5075 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5076 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5077 | ` */` |
|        2 |  5078 | `case PH7_OP_DIV_STORE:{` |
|        5 |  5079 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5080 | `	ph7_value *pObj;` |
|        - |  5081 | `	ph7_real a,b,r;` |
|        - |  5082 | `#ifdef UNTRUST` |
|        - |  5083 | `	if( pNos < pStack ){` |
|        - |  5084 | `		goto Abort;` |
|        - |  5085 | `	}` |
|        - |  5086 | `#endif` |
|        - |  5087 | `	/* Force the operands to be real */` |
|        5 |  5088 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5089 | `		PH7_MemObjToReal(pTos);` |
|        2 |  5090 | `	}` |
|        5 |  5091 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5092 | `		PH7_MemObjToReal(pNos);` |
|        2 |  5093 | `	}` |
|        - |  5094 | `	/* Perform the requested operation */` |
|        5 |  5095 | `	a = pTos->rVal;` |
|        5 |  5096 | `	b = pNos->rVal;` |
|        5 |  5097 | `	if( b == 0 ){` |
|        - |  5098 | `		/* Division by zero */` |
|      ! 0 |  5099 | `		r = 0;` |
|      ! 0 |  5100 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  5101 | `		/* goto Abort; */` |
|      ! 0 |  5102 | `	}else{` |
|        5 |  5103 | `		r = a/b;` |
|        - |  5104 | `		/* Push the result */` |
|        5 |  5105 | `		pNos->rVal = r;` |
|        5 |  5106 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5107 | `		/* Try to get an integer representation */` |
|        5 |  5108 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5109 | `	}` |
|        5 |  5110 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5111 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  5112 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  5113 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  5114 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  5115 | `	}` |
|        5 |  5116 | `	VmPopOperand(&pTos,1);` |
|        5 |  5117 | `	break;` |
|        - |  5118 | `				}` |
|        - |  5119 | `/* OP_BAND * * *` |
|        - |  5120 | ` *` |
|        - |  5121 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5122 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5123 | ` * two elements.` |
|        - |  5124 | `*/` |
|        - |  5125 | `/* OP_BOR * * *` |
|        - |  5126 | ` *` |
|        - |  5127 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5128 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5129 | ` * two elements.` |
|        - |  5130 | ` */` |
|        - |  5131 | `/* OP_BXOR * * *` |
|        - |  5132 | ` *` |
|        - |  5133 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5134 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5135 | ` * two elements.` |
|        - |  5136 | ` */` |
|       44 |  5137 | `case PH7_OP_BAND:` |
|        - |  5138 | `case PH7_OP_BOR:` |
|        - |  5139 | `case PH7_OP_BXOR:{` |
|       90 |  5140 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5141 | `	sxi64 a,b,r;` |
|        - |  5142 | `#ifdef UNTRUST` |
|        - |  5143 | `	if( pNos < pStack ){` |
|        - |  5144 | `		goto Abort;` |
|        - |  5145 | `	}` |
|        - |  5146 | `#endif` |
|        - |  5147 | `	/* Force the operands to be integer */` |
|       90 |  5148 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5149 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5150 | `	}` |
|       90 |  5151 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5152 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5153 | `	}` |
|        - |  5154 | `	/* Perform the requested operation */` |
|       90 |  5155 | `	a = pNos->x.iVal;` |
|       90 |  5156 | `	b = pTos->x.iVal;` |
|       90 |  5157 | `	switch(pInstr->iOp){` |
|        7 |  5158 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5159 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5160 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5161 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5162 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5163 | `	case PH7_OP_BAND:` |
|       62 |  5164 | `	default:          r = a&b; break;` |
|        - |  5165 | `	}` |
|        - |  5166 | `	/* Push the result */` |
|       90 |  5167 | `	pNos->x.iVal = r;` |
|       90 |  5168 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5169 | `	VmPopOperand(&pTos,1);` |
|       90 |  5170 | `	break;` |
|        - |  5171 | `				 }` |
|        - |  5172 | `/* OP_BAND_STORE * * *` |
|        - |  5173 | ` *` |
|        - |  5174 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5175 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5176 | ` * two elements.` |
|        - |  5177 | `*/` |
|        - |  5178 | `/* OP_BOR_STORE * * *` |
|        - |  5179 | ` *` |
|        - |  5180 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5181 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5182 | ` * two elements.` |
|        - |  5183 | ` */` |
|        - |  5184 | `/* OP_BXOR_STORE * * *` |
|        - |  5185 | ` *` |
|        - |  5186 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5187 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5188 | ` * two elements.` |
|        - |  5189 | ` */` |
|       10 |  5190 | `case PH7_OP_BAND_STORE:` |
|        - |  5191 | `case PH7_OP_BOR_STORE:` |
|        - |  5192 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5193 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5194 | `	ph7_value *pObj;` |
|        - |  5195 | `	sxi64 a,b,r;` |
|        - |  5196 | `#ifdef UNTRUST` |
|        - |  5197 | `	if( pNos < pStack ){` |
|        - |  5198 | `		goto Abort;` |
|        - |  5199 | `	}` |
|        - |  5200 | `#endif` |
|        - |  5201 | `	/* Force the operands to be integer */` |
|       21 |  5202 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5203 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5204 | `	}` |
|       21 |  5205 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5206 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5207 | `	}` |
|        - |  5208 | `	/* Perform the requested operation */` |
|       21 |  5209 | `	a = pTos->x.iVal;` |
|       21 |  5210 | `	b = pNos->x.iVal;` |
|       21 |  5211 | `	switch(pInstr->iOp){` |
|        3 |  5212 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5213 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5214 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5215 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5216 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5217 | `	case PH7_OP_BAND:` |
|        7 |  5218 | `	default:          r = a&b; break;` |
|        - |  5219 | `	}` |
|        - |  5220 | `	/* Push the result */` |
|       21 |  5221 | `	pNos->x.iVal = r;` |
|       21 |  5222 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5223 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5224 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5225 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5226 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5227 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5228 | `	}` |
|       21 |  5229 | `	VmPopOperand(&pTos,1);` |
|       21 |  5230 | `	break;` |
|        - |  5231 | `				 }` |
|        - |  5232 | `/* OP_SHL * * *` |
|        - |  5233 | ` *` |
|        - |  5234 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5235 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5236 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5237 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5238 | ` */` |
|        - |  5239 | `/* OP_SHR * * *` |
|        - |  5240 | ` *` |
|        - |  5241 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5242 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5243 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5244 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5245 | ` */` |
|       12 |  5246 | `case PH7_OP_SHL:` |
|        - |  5247 | `case PH7_OP_SHR: {` |
|       25 |  5248 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5249 | `	sxi64 a,r;` |
|        - |  5250 | `	sxi32 b;` |
|        - |  5251 | `#ifdef UNTRUST` |
|        - |  5252 | `	if( pNos < pStack ){` |
|        - |  5253 | `		goto Abort;` |
|        - |  5254 | `	}` |
|        - |  5255 | `#endif` |
|        - |  5256 | `	/* Force the operands to be integer */` |
|       25 |  5257 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5258 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5259 | `	}` |
|       25 |  5260 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5261 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5262 | `	}` |
|        - |  5263 | `	/* Perform the requested operation */` |
|       25 |  5264 | `	a = pNos->x.iVal;` |
|       25 |  5265 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5266 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5267 | `		r = a << b;` |
|        8 |  5268 | `	}else{` |
|       11 |  5269 | `		r = a >> b;` |
|        - |  5270 | `	}` |
|        - |  5271 | `	/* Push the result */` |
|       25 |  5272 | `	pNos->x.iVal = r;` |
|       25 |  5273 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5274 | `	VmPopOperand(&pTos,1);` |
|       25 |  5275 | `	break;` |
|        - |  5276 | `				 }` |
|        - |  5277 | `/*  OP_SHL_STORE * * *` |
|        - |  5278 | ` *` |
|        - |  5279 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5280 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5281 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5282 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5283 | ` */` |
|        - |  5284 | `/* OP_SHR_STORE * * *` |
|        - |  5285 | ` *` |
|        - |  5286 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5287 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5288 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5289 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5290 | ` */` |
|        9 |  5291 | `case PH7_OP_SHL_STORE:` |
|        - |  5292 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5293 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5294 | `	ph7_value *pObj;` |
|        - |  5295 | `	sxi64 a,r;` |
|        - |  5296 | `	sxi32 b;` |
|        - |  5297 | `#ifdef UNTRUST` |
|        - |  5298 | `	if( pNos < pStack ){` |
|        - |  5299 | `		goto Abort;` |
|        - |  5300 | `	}` |
|        - |  5301 | `#endif` |
|        - |  5302 | `	/* Force the operands to be integer */` |
|       19 |  5303 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5304 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5305 | `	}` |
|       19 |  5306 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5307 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5308 | `	}` |
|        - |  5309 | `	/* Perform the requested operation */` |
|       19 |  5310 | `	a = pTos->x.iVal;` |
|       19 |  5311 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5312 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5313 | `		r = a << b;` |
|        5 |  5314 | `	}else{` |
|       11 |  5315 | `		r = a >> b;` |
|        - |  5316 | `	}` |
|        - |  5317 | `	/* Push the result */` |
|       19 |  5318 | `	pNos->x.iVal = r;` |
|       19 |  5319 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5320 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5321 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5322 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5323 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5324 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5325 | `	}` |
|       19 |  5326 | `	VmPopOperand(&pTos,1);` |
|       19 |  5327 | `	break;` |
|        - |  5328 | `				 }` |
|        - |  5329 | `/* CAT:  P1 * *` |
|        - |  5330 | ` *` |
|        - |  5331 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5332 | ` * back.` |
|        - |  5333 | ` */` |
|    68287 |  5334 | `case PH7_OP_CAT:{` |
|        - |  5335 | `	ph7_value *pNos,*pCur;` |
|   136576 |  5336 | `	if( pInstr->iP1 < 1 ){` |
|   109304 |  5337 | `		pNos = &pTos[-1];` |
|    54653 |  5338 | `	}else{` |
|    27274 |  5339 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5340 | `	}` |
|        - |  5341 | `#ifdef UNTRUST` |
|        - |  5342 | `	if( pNos < pStack ){` |
|        - |  5343 | `		goto Abort;` |
|        - |  5344 | `	}` |
|        - |  5345 | `#endif` |
|        - |  5346 | `	/* Force a string cast */` |
|   136576 |  5347 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1636 |  5348 | `		PH7_MemObjToString(pNos);` |
|      817 |  5349 | `	}` |
|   136576 |  5350 | `	pCur = &pNos[1];` |
|   275692 |  5351 | `	while( pCur <= pTos ){` |
|   139118 |  5352 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50874 |  5353 | `			PH7_MemObjToString(pCur);` |
|    25436 |  5354 | `		}` |
|        - |  5355 | `		/* Perform the concatenation */` |
|   139118 |  5356 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   139076 |  5357 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    69537 |  5358 | `		}` |
|   139118 |  5359 | `		SyBlobRelease(&pCur->sBlob);` |
|   139118 |  5360 | `		pCur++;` |
|        2 |  5361 | `	}` |
|   136576 |  5362 | `	pTos = pNos;` |
|   136576 |  5363 | `	break;` |
|        - |  5364 | `				}` |
|        - |  5365 | `/*  CAT_STORE: * * *` |
|        - |  5366 | ` *` |
|        - |  5367 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5368 | ` * back.` |
|        - |  5369 | ` */` |
|     3766 |  5370 | `case PH7_OP_CAT_STORE:{` |
|     7534 |  5371 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5372 | `	ph7_value *pObj;` |
|        - |  5373 | `#ifdef UNTRUST` |
|        - |  5374 | `	if( pNos < pStack ){` |
|        - |  5375 | `		goto Abort;` |
|        - |  5376 | `	}` |
|        - |  5377 | `#endif` |
|     7534 |  5378 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5379 | `		/* Force a string cast */` |
|        3 |  5380 | `		PH7_MemObjToString(pTos);` |
|        1 |  5381 | `	}` |
|     7534 |  5382 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5383 | `		/* Force a string cast */` |
|      ! 0 |  5384 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5385 | `	}` |
|        - |  5386 | `	/* Perform the concatenation (Reverse order) */` |
|     7534 |  5387 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7534 |  5388 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3766 |  5389 | `	}` |
|        - |  5390 | `	/* Perform the store operation */` |
|     7534 |  5391 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5392 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7534 |  5393 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7534 |  5394 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7532 |  5395 | `		PH7_MemObjStore(pTos,pObj);` |
|     3765 |  5396 | `	}` |
|     7532 |  5397 | `	PH7_MemObjStore(pTos,pNos);` |
|     7532 |  5398 | `	VmPopOperand(&pTos,1);` |
|     7532 |  5399 | `	break;` |
|        - |  5400 | `				}` |
|        - |  5401 | `/* OP_AND: * * *` |
|        - |  5402 | ` *` |
|        - |  5403 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5404 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5405 | ` * stack.` |
|        - |  5406 | ` */` |
|        - |  5407 | `/* OP_OR: * * *` |
|        - |  5408 | ` *` |
|        - |  5409 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5410 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5411 | ` * stack.` |
|        - |  5412 | ` */` |
|   103198 |  5413 | `case PH7_OP_LAND:` |
|        - |  5414 | `case PH7_OP_LOR: {` |
|   206442 |  5415 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5416 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5417 | `#ifdef UNTRUST` |
|        - |  5418 | `	if( pNos < pStack ){` |
|        - |  5419 | `		goto Abort;` |
|        - |  5420 | `	}` |
|        - |  5421 | `#endif` |
|        - |  5422 | `	/* Force a boolean cast */` |
|   206442 |  5423 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5424 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5425 | `	}` |
|   206442 |  5426 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5427 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5428 | `	}` |
|   206442 |  5429 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   206442 |  5430 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   206442 |  5431 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5432 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    94426 |  5433 | `		v1 = and_logic[v1*3+v2];` |
|    47236 |  5434 | `	}else{` |
|        - |  5435 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   112018 |  5436 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5437 | `	}` |
|   206442 |  5438 | `	if( v1 == 2 ){` |
|      ! 0 |  5439 | `		v1 = 1;` |
|      ! 0 |  5440 | `	}` |
|   206442 |  5441 | `	VmPopOperand(&pTos,1);` |
|   206442 |  5442 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   206442 |  5443 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   206442 |  5444 | `	break;` |
|        - |  5445 | `				 }` |
|        - |  5446 | `/*` |
|        - |  5447 | ` * OP_NULLC: * * *` |
|        - |  5448 | ` * Null coalescing operator '??'.` |
|        - |  5449 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5450 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5451 | ` */` |
|        - |  5452 | `/*` |
|        - |  5453 | ` * OP_NULLC: * P2 *` |
|        - |  5454 | ` * Short-circuit null coalescing '??'.` |
|        - |  5455 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  5456 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  5457 | ` */` |
|       52 |  5458 | `case PH7_OP_NULLC: {` |
|        - |  5459 | `#ifdef UNTRUST` |
|        - |  5460 | `	if( pTos < pStack ){` |
|        - |  5461 | `		goto Abort;` |
|        - |  5462 | `	}` |
|        - |  5463 | `#endif` |
|      106 |  5464 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5465 | `		/* Left is not null — keep it and skip the RHS */` |
|       42 |  5466 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       22 |  5467 | `	}else{` |
|        - |  5468 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       66 |  5469 | `		VmPopOperand(&pTos, 1);` |
|        - |  5470 | `	}` |
|      106 |  5471 | `	break;` |
|        - |  5472 |  |
|        - |  5473 | `/*` |
|        - |  5474 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  5475 | ` * Null coalescing assignment short-circuit.` |
|        - |  5476 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  5477 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  5478 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  5479 | ` */` |
|       23 |  5480 | `case PH7_OP_NULLC_JMP: {` |
|        - |  5481 | `#ifdef UNTRUST` |
|        - |  5482 | `	if( pTos < pStack ){` |
|        - |  5483 | `		goto Abort;` |
|        - |  5484 | `	}` |
|        - |  5485 | `#endif` |
|       47 |  5486 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  5487 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  5488 | `	}` |
|       47 |  5489 | `	break;` |
|        - |  5490 |  |
|        - |  5491 | `/*` |
|        - |  5492 | ` * OP_NULLC_STORE: * * *` |
|        - |  5493 | ` * Null coalescing assignment store.` |
|        - |  5494 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  5495 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  5496 | ` * expression result.` |
|        - |  5497 | ` */` |
|        - |  5498 | `/*` |
|        - |  5499 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  5500 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  5501 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  5502 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  5503 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  5504 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  5505 | ` */` |
|       51 |  5506 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  5507 | `#ifdef UNTRUST` |
|        - |  5508 | `	if( pTos < pStack ){` |
|        - |  5509 | `		goto Abort;` |
|        - |  5510 | `	}` |
|        - |  5511 | `#endif` |
|      104 |  5512 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  5513 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  5514 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  5515 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  5516 | `	}` |
|      104 |  5517 | `	break;` |
|        - |  5518 |  |
|       14 |  5519 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  5520 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5521 | `	ph7_value *pObj;` |
|        - |  5522 | `	sxu32 nIdx;` |
|        - |  5523 | `#ifdef UNTRUST` |
|        - |  5524 | `	if( pNos < pStack ){` |
|        - |  5525 | `		goto Abort;` |
|        - |  5526 | `	}` |
|        - |  5527 | `#endif` |
|       29 |  5528 | `	nIdx = pNos->nIdx;` |
|       29 |  5529 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5530 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5531 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  5532 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  5533 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  5534 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  5535 | `	}` |
|       29 |  5536 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  5537 | `	VmPopOperand(&pTos,1);` |
|       29 |  5538 | `	break;` |
|        - |  5539 |  |
|        - |  5540 | `/*` |
|        - |  5541 | ` * OP_SPREAD: * * *` |
|        - |  5542 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  5543 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  5544 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  5545 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  5546 | ` */` |
|        9 |  5547 | `case PH7_OP_SPREAD: {` |
|        - |  5548 | `#ifdef UNTRUST` |
|        - |  5549 | `	if( pTos < pStack ){` |
|        - |  5550 | `		goto Abort;` |
|        - |  5551 | `	}` |
|        - |  5552 | `#endif` |
|       20 |  5553 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  5554 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  5555 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  5556 | `		if( nEntry == 0 ){` |
|        - |  5557 | `			/* Empty array — remove from stack */` |
|        3 |  5558 | `			VmPopOperand(&pTos, 1);` |
|        3 |  5559 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  5560 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  5561 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  5562 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  5563 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  5564 | `				VM_STACK_GUARD);` |
|      ! 0 |  5565 | `		}else{` |
|        - |  5566 | `			ph7_hashmap_node *pNode2;` |
|        - |  5567 | `			ph7_value *pElem;` |
|        - |  5568 | `			sxu32 i;` |
|        - |  5569 | `			/* Overwrite TOS with first element */` |
|       18 |  5570 | `			pNode2 = pMap->pFirst;` |
|       18 |  5571 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  5572 | `			PH7_MemObjRelease(pTos);` |
|       18 |  5573 | `			if( pElem ){` |
|       18 |  5574 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  5575 | `			}` |
|       18 |  5576 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5577 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  5578 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  5579 | `			pNode2 = pNode2->pPrev;` |
|        - |  5580 | `			/* Push remaining elements */` |
|       44 |  5581 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  5582 | `				pTos++;` |
|       28 |  5583 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  5584 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  5585 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  5586 | `				if( pElem ){` |
|       28 |  5587 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  5588 | `				}` |
|       28 |  5589 | `				pNode2 = pNode2->pPrev;` |
|       15 |  5590 | `			}` |
|       18 |  5591 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  5592 | `		}` |
|        9 |  5593 | `	}` |
|        - |  5594 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  5595 | `	break;` |
|        - |  5596 |  |
|        - |  5597 | `/* OP_LXOR: * * *` |
|        - |  5598 | ` *` |
|        - |  5599 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  5600 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5601 | ` * stack.` |
|        - |  5602 | ` * According to the PHP language reference manual:` |
|        - |  5603 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  5604 | ` *  TRUE,but not both.` |
|        - |  5605 | ` */` |
|        5 |  5606 | `case PH7_OP_LXOR:{` |
|       11 |  5607 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  5608 | `	sxi32 v = 0;` |
|        - |  5609 | `#ifdef UNTRUST` |
|        - |  5610 | `	if( pNos < pStack ){` |
|        - |  5611 | `		goto Abort;` |
|        - |  5612 | `	}` |
|        - |  5613 | `#endif` |
|        - |  5614 | `	/* Force a boolean cast */` |
|       11 |  5615 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5616 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  5617 | `	}` |
|       11 |  5618 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5619 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5620 | `	}` |
|       11 |  5621 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  5622 | `		v = 1;` |
|        3 |  5623 | `	}` |
|       11 |  5624 | `	VmPopOperand(&pTos,1);` |
|       11 |  5625 | `	pTos->x.iVal = v;` |
|       11 |  5626 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  5627 | `	break;` |
|        - |  5628 | `				 }` |
|        - |  5629 | `/* OP_EQ P1 P2 P3` |
|        - |  5630 | ` *` |
|        - |  5631 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  5632 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5633 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5634 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5635 | ` */` |
|        - |  5636 | `/* OP_NEQ P1 P2 P3` |
|        - |  5637 | ` *` |
|        - |  5638 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  5639 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5640 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5641 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5642 | ` */` |
|     4309 |  5643 | `case PH7_OP_EQ:` |
|        - |  5644 | `case PH7_OP_NEQ: {` |
|     8620 |  5645 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5646 | `	/* Perform the comparison and act accordingly */` |
|        - |  5647 | `#ifdef UNTRUST` |
|        - |  5648 | `	if( pNos < pStack ){` |
|        - |  5649 | `		goto Abort;` |
|        - |  5650 | `	}` |
|        - |  5651 | `#endif` |
|     8620 |  5652 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8620 |  5653 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  5654 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8611 |  5655 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8576 |  5656 | `		rc = rc == 0;` |
|     4289 |  5657 | `	}else{` |
|       28 |  5658 | `		rc = rc != 0;` |
|        - |  5659 | `	}` |
|     8620 |  5660 | `	VmPopOperand(&pTos,1);` |
|     8620 |  5661 | `	if( !pInstr->iP2 ){` |
|        - |  5662 | `		/* Push comparison result without taking the jump */` |
|     8620 |  5663 | `		PH7_MemObjRelease(pTos);` |
|     8620 |  5664 | `		pTos->x.iVal = rc;` |
|        - |  5665 | `		/* Invalidate any prior representation */` |
|     8620 |  5666 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4311 |  5667 | `	}else{` |
|      ! 0 |  5668 | `		if( rc ){` |
|        - |  5669 | `			/* Jump to the desired location */` |
|      ! 0 |  5670 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5671 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5672 | `		}` |
|        - |  5673 | `	}` |
|     8620 |  5674 | `	break;` |
|        - |  5675 | `				 }` |
|        - |  5676 | `/* OP_TEQ P1 P2 *` |
|        - |  5677 | ` *` |
|        - |  5678 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  5679 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5680 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5681 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5682 | ` */` |
|   150780 |  5683 | `case PH7_OP_TEQ: {` |
|   301562 |  5684 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5685 | `	/* Perform the comparison and act accordingly */` |
|        - |  5686 | `#ifdef UNTRUST` |
|        - |  5687 | `	if( pNos < pStack ){` |
|        - |  5688 | `		goto Abort;` |
|        - |  5689 | `	}` |
|        - |  5690 | `#endif` |
|   301562 |  5691 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   301562 |  5692 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5693 | `		rc = 0;` |
|        2 |  5694 | `	}else{` |
|   301560 |  5695 | `		rc = rc == 0;` |
|        - |  5696 | `	}` |
|   301562 |  5697 | `	VmPopOperand(&pTos,1);` |
|   301562 |  5698 | `	if( !pInstr->iP2 ){` |
|        - |  5699 | `		/* Push comparison result without taking the jump */` |
|   301562 |  5700 | `		PH7_MemObjRelease(pTos);` |
|   301562 |  5701 | `		pTos->x.iVal = rc;` |
|        - |  5702 | `		/* Invalidate any prior representation */` |
|   301562 |  5703 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   150782 |  5704 | `	}else{` |
|      ! 0 |  5705 | `		if( rc ){` |
|        - |  5706 | `			/* Jump to the desired location */` |
|      ! 0 |  5707 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5708 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5709 | `		}` |
|        - |  5710 | `	}` |
|   301562 |  5711 | `	break;` |
|        - |  5712 | `				 }` |
|        - |  5713 | `/* OP_TNE P1 P2 *` |
|        - |  5714 | ` *` |
|        - |  5715 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  5716 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  5717 | ` * instruction.` |
|        - |  5718 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5719 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5720 | ` *` |
|        - |  5721 | ` */` |
|   116306 |  5722 | `case PH7_OP_TNE: {` |
|   232614 |  5723 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5724 | `	/* Perform the comparison and act accordingly */` |
|        - |  5725 | `#ifdef UNTRUST` |
|        - |  5726 | `	if( pNos < pStack ){` |
|        - |  5727 | `		goto Abort;` |
|        - |  5728 | `	}` |
|        - |  5729 | `#endif` |
|   232614 |  5730 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   232614 |  5731 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5732 | `		rc = 1;` |
|        2 |  5733 | `	}else{` |
|   232612 |  5734 | `		rc = rc != 0;` |
|        - |  5735 | `	}` |
|   232614 |  5736 | `	VmPopOperand(&pTos,1);` |
|   232614 |  5737 | `	if( !pInstr->iP2 ){` |
|        - |  5738 | `		/* Push comparison result without taking the jump */` |
|   232614 |  5739 | `		PH7_MemObjRelease(pTos);` |
|   232614 |  5740 | `		pTos->x.iVal = rc;` |
|        - |  5741 | `		/* Invalidate any prior representation */` |
|   232614 |  5742 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   116308 |  5743 | `	}else{` |
|      ! 0 |  5744 | `		if( rc ){` |
|        - |  5745 | `			/* Jump to the desired location */` |
|      ! 0 |  5746 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5747 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5748 | `		}` |
|        - |  5749 | `	}` |
|   232614 |  5750 | `	break;` |
|        - |  5751 | `				 }` |
|        - |  5752 | `/* OP_LT P1 P2 P3` |
|        - |  5753 | ` *` |
|        - |  5754 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5755 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5756 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5757 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5758 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5759 | ` *` |
|        - |  5760 | ` */` |
|        - |  5761 | `/* OP_LE P1 P2 P3` |
|        - |  5762 | ` *` |
|        - |  5763 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5764 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5765 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5766 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5767 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5768 | ` *` |
|        - |  5769 | ` */` |
|   108808 |  5770 | `case PH7_OP_LT:` |
|        - |  5771 | `case PH7_OP_LE: {` |
|   217662 |  5772 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5773 | `	/* Perform the comparison and act accordingly */` |
|        - |  5774 | `#ifdef UNTRUST` |
|        - |  5775 | `	if( pNos < pStack ){` |
|        - |  5776 | `		goto Abort;` |
|        - |  5777 | `	}` |
|        - |  5778 | `#endif` |
|   217662 |  5779 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   217662 |  5780 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5781 | `		rc = 0;` |
|   217658 |  5782 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1094 |  5783 | `		rc = rc < 1;` |
|      548 |  5784 | `	}else{` |
|   216562 |  5785 | `		rc = rc < 0;` |
|        - |  5786 | `	}` |
|   217662 |  5787 | `	VmPopOperand(&pTos,1);` |
|   217662 |  5788 | `	if( !pInstr->iP2 ){` |
|        - |  5789 | `		/* Push comparison result without taking the jump */` |
|   217662 |  5790 | `		PH7_MemObjRelease(pTos);` |
|   217662 |  5791 | `		pTos->x.iVal = rc;` |
|        - |  5792 | `		/* Invalidate any prior representation */` |
|   217662 |  5793 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   108854 |  5794 | `	}else{` |
|      ! 0 |  5795 | `		if( rc ){` |
|        - |  5796 | `			/* Jump to the desired location */` |
|      ! 0 |  5797 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5798 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5799 | `		}` |
|        - |  5800 | `	}` |
|   217662 |  5801 | `	break;` |
|        - |  5802 | `				}` |
|        - |  5803 | `/* OP_GT P1 P2 P3` |
|        - |  5804 | ` *` |
|        - |  5805 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5806 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5807 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5808 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5809 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5810 | ` *` |
|        - |  5811 | ` */` |
|        - |  5812 | `/* OP_GE P1 P2 P3` |
|        - |  5813 | ` *` |
|        - |  5814 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5815 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5816 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5817 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5818 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5819 | ` *` |
|        - |  5820 | ` */` |
|    53368 |  5821 | `case PH7_OP_GT:` |
|        - |  5822 | `case PH7_OP_GE: {` |
|   106738 |  5823 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5824 | `	/* Perform the comparison and act accordingly */` |
|        - |  5825 | `#ifdef UNTRUST` |
|        - |  5826 | `	if( pNos < pStack ){` |
|        - |  5827 | `		goto Abort;` |
|        - |  5828 | `	}` |
|        - |  5829 | `#endif` |
|   106738 |  5830 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   106738 |  5831 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5832 | `		rc = 0;` |
|   106734 |  5833 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   106566 |  5834 | `		rc = rc >= 0;` |
|    53284 |  5835 | `	}else{` |
|      166 |  5836 | `		rc = rc > 0;` |
|        - |  5837 | `	}` |
|   106738 |  5838 | `	VmPopOperand(&pTos,1);` |
|   106738 |  5839 | `	if( !pInstr->iP2 ){` |
|        - |  5840 | `		/* Push comparison result without taking the jump */` |
|   106738 |  5841 | `		PH7_MemObjRelease(pTos);` |
|   106738 |  5842 | `		pTos->x.iVal = rc;` |
|        - |  5843 | `		/* Invalidate any prior representation */` |
|   106738 |  5844 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    53370 |  5845 | `	}else{` |
|      ! 0 |  5846 | `		if( rc ){` |
|        - |  5847 | `			/* Jump to the desired location */` |
|      ! 0 |  5848 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5849 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5850 | `		}` |
|        - |  5851 | `	}` |
|   106738 |  5852 | `	break;` |
|        - |  5853 | `				}` |
|        - |  5854 | `/* OP_SPACESHIP * * *` |
|        - |  5855 | ` *` |
|        - |  5856 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5857 | ` *   -1 if left < right` |
|        - |  5858 | ` *    0 if left == right` |
|        - |  5859 | ` *    1 if left > right` |
|        - |  5860 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5861 | ` */` |
|       25 |  5862 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5863 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5864 | `#ifdef UNTRUST` |
|        - |  5865 | `	if( pNos < pStack ){` |
|        - |  5866 | `		goto Abort;` |
|        - |  5867 | `	}` |
|        - |  5868 | `#endif` |
|       51 |  5869 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5870 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5871 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5872 | `		rc = 1;` |
|        4 |  5873 | `	}else{` |
|        - |  5874 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5875 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5876 | `	}` |
|       51 |  5877 | `	VmPopOperand(&pTos,1);` |
|       51 |  5878 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5879 | `	pTos->x.iVal = rc;` |
|       51 |  5880 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5881 | `	break;` |
|        - |  5882 | `				}` |
|        - |  5883 | `/* OP_SEQ P1 P2 *` |
|        - |  5884 | ` * Strict string comparison.` |
|        - |  5885 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5886 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5887 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5888 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5889 | ` * use PH7_OP_EQ.` |
|        - |  5890 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5891 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5892 | ` */` |
|        - |  5893 | `/* OP_SNE P1 P2 *` |
|        - |  5894 | ` * Strict string comparison.` |
|        - |  5895 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5896 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5897 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5898 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5899 | ` * use PH7_OP_EQ.` |
|        - |  5900 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5901 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5902 | ` */` |
|       18 |  5903 | `case PH7_OP_SEQ:` |
|        - |  5904 | `case PH7_OP_SNE: {` |
|       38 |  5905 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5906 | `	SyString s1,s2;` |
|        - |  5907 | `	/* Perform the comparison and act accordingly */` |
|        - |  5908 | `#ifdef UNTRUST` |
|        - |  5909 | `	if( pNos < pStack ){` |
|        - |  5910 | `		goto Abort;` |
|        - |  5911 | `	}` |
|        - |  5912 | `#endif` |
|        - |  5913 | `	/* Force a string cast */` |
|       38 |  5914 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5915 | `		PH7_MemObjToString(pTos);` |
|        2 |  5916 | `	}` |
|       38 |  5917 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5918 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5919 | `	}` |
|       38 |  5920 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5921 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5922 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5923 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5924 | `		rc = rc != 0;` |
|      ! 0 |  5925 | `	}else{` |
|       38 |  5926 | `		rc = rc == 0;` |
|        - |  5927 | `	}` |
|       38 |  5928 | `	VmPopOperand(&pTos,1);` |
|       38 |  5929 | `	if( !pInstr->iP2 ){` |
|        - |  5930 | `		/* Push comparison result without taking the jump */` |
|       38 |  5931 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5932 | `		pTos->x.iVal = rc;` |
|        - |  5933 | `		/* Invalidate any prior representation */` |
|       38 |  5934 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5935 | `	}else{` |
|      ! 0 |  5936 | `		if( rc ){` |
|        - |  5937 | `			/* Jump to the desired location */` |
|      ! 0 |  5938 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5939 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5940 | `		}` |
|        - |  5941 | `	}` |
|       38 |  5942 | `	break;` |
|        - |  5943 | `				 }` |
|        - |  5944 | `/*` |
|        - |  5945 | ` * OP_LOAD_REF * * *` |
|        - |  5946 | ` * Push the index of a referenced object on the stack.` |
|        - |  5947 | ` */` |
|       57 |  5948 | `case PH7_OP_LOAD_REF: {` |
|        - |  5949 | `	sxu32 nIdx;` |
|        - |  5950 | `#ifdef UNTRUST` |
|        - |  5951 | `	if( pTos < pStack ){` |
|        - |  5952 | `		goto Abort;` |
|        - |  5953 | `	}` |
|        - |  5954 | `#endif` |
|        - |  5955 | `	/* Extract memory object index */` |
|      115 |  5956 | `	nIdx = pTos->nIdx;` |
|      115 |  5957 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5958 | `		/* Nullify the object */` |
|       95 |  5959 | `		PH7_MemObjRelease(pTos);` |
|        - |  5960 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5961 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5962 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5963 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5964 | `	}` |
|      115 |  5965 | `	break;` |
|        - |  5966 | `					  }` |
|        - |  5967 | `/*` |
|        - |  5968 | ` * OP_STORE_REF * * P3` |
|        - |  5969 | ` * Perform an assignment operation by reference.` |
|        - |  5970 | ` */` |
|       16 |  5971 | ` case PH7_OP_STORE_REF: {` |
|       34 |  5972 | `	 SyString sName = { 0 , 0 };` |
|        - |  5973 | `	 VmFrame *pFrameLocal;` |
|        - |  5974 | `	SyHashEntry *pEntry;` |
|        - |  5975 | `	sxu32 nIdx;` |
|        - |  5976 | `#ifdef UNTRUST` |
|        - |  5977 | `	if( pTos < pStack ){` |
|        - |  5978 | `		goto Abort;` |
|        - |  5979 | `	}` |
|        - |  5980 | `#endif` |
|       34 |  5981 | `	if( pInstr->p3 == 0 ){` |
|        - |  5982 | `		char *zName;` |
|        - |  5983 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5984 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5985 | `			/* Force a string cast */` |
|      ! 0 |  5986 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5987 | `		}` |
|      ! 0 |  5988 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5989 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5990 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5991 | `			if( zName ){` |
|      ! 0 |  5992 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5993 | `			}` |
|      ! 0 |  5994 | `		}` |
|      ! 0 |  5995 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5996 | `		pTos--;` |
|      ! 0 |  5997 | `	}else{` |
|       34 |  5998 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5999 | `	}` |
|       34 |  6000 | `	nIdx = pTos->nIdx;` |
|       34 |  6001 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  6002 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6003 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6004 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  6005 | `		}else{` |
|        - |  6006 | `			ph7_value *pObj;` |
|        - |  6007 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  6008 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  6009 | `			if( pObj == 0 ){` |
|      ! 0 |  6010 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6011 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  6012 | `				goto Abort;` |
|        - |  6013 | `			}` |
|        - |  6014 | `			/* Perform the store operation */` |
|      ! 0 |  6015 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  6016 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  6017 | `		}` |
|       34 |  6018 | `	}else if( sName.nByte > 0){` |
|       34 |  6019 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  6020 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  6021 | `		}else{` |
|       34 |  6022 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  6023 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6024 | `			/* Query the local frame */` |
|       34 |  6025 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  6026 | `			if( pEntry ){` |
|      ! 0 |  6027 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  6028 | `			}else{` |
|       34 |  6029 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  6030 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  6031 | `					/* Insert in the $GLOBALS array */` |
|       30 |  6032 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  6033 | `				}` |
|       34 |  6034 | `				if( rc == SXRET_OK ){` |
|       34 |  6035 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  6036 | `				}` |
|        - |  6037 | `			}` |
|        - |  6038 | `		}` |
|       16 |  6039 | `	}` |
|       34 |  6040 | `	break;` |
|        - |  6041 | `				 }` |
|        - |  6042 | `/*` |
|        - |  6043 | ` * OP_UPLINK P1 * *` |
|        - |  6044 | ` * Link a variable to the top active VM frame.` |
|        - |  6045 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  6046 | ` */` |
|       28 |  6047 | `case PH7_OP_UPLINK: {` |
|       58 |  6048 | `	if( pVm->pFrame->pParent ){` |
|       58 |  6049 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  6050 | `		SyString sName;` |
|        - |  6051 | `		/* Perform the link */` |
|      116 |  6052 | `		while( pLink <= pTos ){` |
|       60 |  6053 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6054 | `				/* Force a string cast */` |
|      ! 0 |  6055 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  6056 | `			}` |
|       60 |  6057 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  6058 | `			if( sName.nByte > 0 ){` |
|       60 |  6059 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  6060 | `			}` |
|       60 |  6061 | `			pLink++;` |
|        2 |  6062 | `		}` |
|       28 |  6063 | `	}` |
|       58 |  6064 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  6065 | `	break;` |
|        - |  6066 | `					}` |
|        - |  6067 | `/*` |
|        - |  6068 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  6069 | ` * Push an exception in the corresponding container so that` |
|        - |  6070 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  6071 | ` */` |
|      110 |  6072 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      222 |  6073 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6074 | `	VmFrame *pFrameLocal;` |
|        - |  6075 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      222 |  6076 | `	pException->iFinallyDone = 0;` |
|      222 |  6077 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6078 | `	/* Create the exception frame */` |
|      222 |  6079 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      222 |  6080 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6081 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6082 | `		goto Abort;` |
|        - |  6083 | `	}` |
|        - |  6084 | `	/* Mark the special frame */` |
|      222 |  6085 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      222 |  6086 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6087 | `	/* Point to the frame that trigger the exception */` |
|      222 |  6088 | `	pFrameLocal = pFrameLocal->pParent;` |
|      222 |  6089 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      222 |  6090 | `	pException->pFrame = pFrameLocal;` |
|      222 |  6091 | `	break;` |
|        - |  6092 | `							}` |
|        - |  6093 | `/*` |
|        - |  6094 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6095 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6096 | ` */` |
|      109 |  6097 | `case PH7_OP_POP_EXCEPTION: {` |
|      220 |  6098 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      220 |  6099 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6100 | `		ph7_exception **apException;` |
|        - |  6101 | `		/* Pop the loaded exception */` |
|       32 |  6102 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  6103 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  6104 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  6105 | `		}` |
|       15 |  6106 | `	}` |
|      220 |  6107 | `	pException->pFrame = 0;` |
|        - |  6108 | `	/* Leave the exception frame */` |
|      220 |  6109 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6110 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      220 |  6111 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6112 | `		sxi32 rcFinally;` |
|       20 |  6113 | `		pException->iFinallyDone = 1;` |
|       20 |  6114 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6115 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6116 | `			goto Abort;` |
|        - |  6117 | `		}` |
|        9 |  6118 | `	}` |
|      220 |  6119 | `	break;` |
|        - |  6120 | `							}` |
|        - |  6121 |  |
|        - |  6122 | `/*` |
|        - |  6123 | ` * OP_THROW * P2 *` |
|        - |  6124 | ` * Throw an user exception.` |
|        - |  6125 | ` */` |
|       58 |  6126 | `case PH7_OP_THROW: {` |
|      118 |  6127 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      118 |  6128 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6129 | `#ifdef UNTRUST` |
|        - |  6130 | `	if( pTos < pStack ){` |
|        - |  6131 | `		goto Abort;` |
|        - |  6132 | `	}` |
|        - |  6133 | `#endif` |
|      118 |  6134 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6135 | `	/* Tell the upper layer that an exception was thrown */` |
|      118 |  6136 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      118 |  6137 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      118 |  6138 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6139 | `		ph7_class *pThrowable;` |
|        - |  6140 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      118 |  6141 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      119 |  6142 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  6143 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  6144 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  6145 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  6146 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  6147 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  6148 | `			if( pErrorClass ){` |
|        3 |  6149 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  6150 | `			}` |
|        3 |  6151 | `			if( pErrInst ){` |
|        - |  6152 | `				ph7_class_method *pCons;` |
|        3 |  6153 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  6154 | `				if( pCons ){` |
|        - |  6155 | `					ph7_value sArg;` |
|        - |  6156 | `					ph7_value *apArg[1];` |
|        - |  6157 | `					SyString sMsgStr;` |
|        - |  6158 | `					static const char zErrMsg[] =` |
|        - |  6159 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  6160 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  6161 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  6162 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  6163 | `					apArg[0] = &sArg;` |
|        3 |  6164 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  6165 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  6166 | `				}` |
|        3 |  6167 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  6168 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  6169 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6170 | `					goto Abort;` |
|        - |  6171 | `				}` |
|        2 |  6172 | `			}else{` |
|        - |  6173 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  6174 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6175 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6176 | `					goto Abort;` |
|        - |  6177 | `				}` |
|        - |  6178 | `			}` |
|        2 |  6179 | `		}else{` |
|        - |  6180 | `			/* Throw the exception */` |
|      116 |  6181 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      116 |  6182 | `			if( rc == SXERR_ABORT ){` |
|        - |  6183 | `				/* Abort processing immediately */` |
|       11 |  6184 | `				goto Abort;` |
|        - |  6185 | `			}` |
|        - |  6186 | `		}` |
|       55 |  6187 | `	}else{` |
|        - |  6188 | `		/* Expecting a class instance */` |
|      ! 0 |  6189 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6190 | `		if( rc == SXERR_ABORT ){` |
|        - |  6191 | `			/* Abort processing immediately */` |
|      ! 0 |  6192 | `			goto Abort;` |
|        - |  6193 | `		}` |
|        - |  6194 | `	}` |
|        - |  6195 | `	/* Pop the top entry */` |
|      108 |  6196 | `	VmPopOperand(&pTos,1);` |
|        - |  6197 | `	/* Perform an unconditional jump */` |
|      108 |  6198 | `	pc = nJump - 1;` |
|      108 |  6199 | `	break;` |
|        - |  6200 | `				   }` |
|        - |  6201 | `/*` |
|        - |  6202 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6203 | ` * Prepare a foreach step.` |
|        - |  6204 | ` */` |
|     5697 |  6205 | `case PH7_OP_FOREACH_INIT: {` |
|    11396 |  6206 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6207 | `	void *pName;` |
|        - |  6208 | `#ifdef UNTRUST` |
|        - |  6209 | `	if( pTos < pStack ){` |
|        - |  6210 | `		goto Abort;` |
|        - |  6211 | `	}` |
|        - |  6212 | `#endif` |
|    11396 |  6213 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6214 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6215 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6216 | `			/* Force a string cast */` |
|      ! 0 |  6217 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6218 | `		}` |
|        - |  6219 | `		/* Duplicate name */` |
|      ! 0 |  6220 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6221 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6222 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6223 | `		}` |
|      ! 0 |  6224 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6225 | `	}` |
|    11396 |  6226 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6227 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6228 | `			/* Force a string cast */` |
|      ! 0 |  6229 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6230 | `		}` |
|        - |  6231 | `		/* Duplicate name */` |
|      ! 0 |  6232 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6233 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6234 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6235 | `		}` |
|      ! 0 |  6236 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6237 | `	}` |
|        - |  6238 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11396 |  6239 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6240 | `		/* Jump out of the loop */` |
|      ! 0 |  6241 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6242 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6243 | `		}` |
|      ! 0 |  6244 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6245 | `	}else{` |
|        - |  6246 | `		ph7_foreach_step *pStep;` |
|    11396 |  6247 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11396 |  6248 | `		if( pStep == 0 ){` |
|      ! 0 |  6249 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6250 | `			/* Jump out of the loop */` |
|      ! 0 |  6251 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6252 | `		}else{` |
|        - |  6253 | `			/* Zero the structure */` |
|    11396 |  6254 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6255 | `			/* Prepare the step */` |
|    11396 |  6256 | `			pStep->iFlags = pInfo->iFlags;` |
|    11396 |  6257 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6258 | `				ph7_hashmap *pMap;` |
|        - |  6259 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6260 | `				 * source array so mutations don't affect other sharers. */` |
|    11364 |  6261 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6262 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6263 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6264 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6265 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6266 | `						 * variable still points at the same hashmap as` |
|        - |  6267 | `						 * the stack value. */` |
|        9 |  6268 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6269 | `							pCur->iRef--;` |
|        9 |  6270 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6271 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6272 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6273 | `						}` |
|        4 |  6274 | `					}` |
|        4 |  6275 | `				}` |
|    11364 |  6276 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6277 | `				/* Reset the internal loop cursor */` |
|    11364 |  6278 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6279 | `				/* Mark the step */` |
|    11364 |  6280 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    11364 |  6281 | `				pStep->xIter.pMap = pMap;` |
|    11364 |  6282 | `				pMap->iRef++;` |
|     5683 |  6283 | `			}else{` |
|       34 |  6284 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6285 | `				ph7_class *pIteratorClass;` |
|        - |  6286 | `				/* Check if the object implements Iterator */` |
|       34 |  6287 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6288 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6289 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6290 | `					ph7_class_method *pRewind;` |
|       24 |  6291 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6292 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6293 | `					pThis->iRef++;` |
|       24 |  6294 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6295 | `					if( pRewind ){` |
|       24 |  6296 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6297 | `					}` |
|       13 |  6298 | `				}else{` |
|        - |  6299 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6300 | `					ph7_class *pIterAggClass;` |
|       12 |  6301 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6302 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6303 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6304 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6305 | `						ph7_class_method *pGetIter;` |
|        3 |  6306 | `						int iterAggOk = 0;` |
|        3 |  6307 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6308 | `						if( pGetIter ){` |
|        - |  6309 | `							ph7_value sResult;` |
|        3 |  6310 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6311 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6312 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6313 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6314 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6315 | `									ph7_class_method *pRewind;` |
|        3 |  6316 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6317 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6318 | `									pIterObj->iRef++;` |
|        - |  6319 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6320 | `									pStep->pOwner = pThis;` |
|        3 |  6321 | `									pThis->iRef++;` |
|        3 |  6322 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6323 | `									if( pRewind ){` |
|        3 |  6324 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6325 | `									}` |
|        3 |  6326 | `									iterAggOk = 1;` |
|        1 |  6327 | `								}` |
|        1 |  6328 | `							}` |
|        3 |  6329 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6330 | `						}` |
|        3 |  6331 | `						if( !iterAggOk ){` |
|        - |  6332 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6333 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6334 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6335 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6336 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6337 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6338 | `						}` |
|        2 |  6339 | `					}else{` |
|        - |  6340 | `						/* Plain object iteration via hAttr */` |
|        9 |  6341 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6342 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6343 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6344 | `						pThis->iRef++;` |
|        - |  6345 | `					}` |
|        - |  6346 | `				}` |
|        - |  6347 | `			}` |
|        - |  6348 | `		}` |
|    11396 |  6349 | `		if( pStep ){` |
|    11396 |  6350 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6351 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6352 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6353 | `				/* Jump out of the loop */` |
|      ! 0 |  6354 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6355 | `			}` |
|     5697 |  6356 | `		}` |
|        - |  6357 | `	}` |
|    11396 |  6358 | `	VmPopOperand(&pTos,1);` |
|    11396 |  6359 | `	break;` |
|        - |  6360 | `						  }` |
|        - |  6361 | `/*` |
|        - |  6362 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6363 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6364 | ` */` |
|    93052 |  6365 | `case PH7_OP_FOREACH_STEP: {` |
|   186106 |  6366 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6367 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6368 | `	ph7_value *pValue;` |
|        - |  6369 | `	VmFrame *pFrameLocal;` |
|        - |  6370 | `	/* Peek the last step */` |
|   186106 |  6371 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   186106 |  6372 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   186106 |  6373 | `	pFrameLocal = pVm->pFrame;` |
|   186106 |  6374 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   186106 |  6375 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   185978 |  6376 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6377 | `		ph7_hashmap_node *pNode;` |
|        - |  6378 | `		/* Extract the current node value */` |
|   185978 |  6379 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   185978 |  6380 | `		if( pNode == 0 ){` |
|        - |  6381 | `			/* No more entry to process */` |
|    11362 |  6382 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    11362 |  6383 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6384 | `				/* Break the reference with the last element */` |
|        7 |  6385 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6386 | `			}` |
|        - |  6387 | `			/* Automatically reset the loop cursor */` |
|    11362 |  6388 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6389 | `			/* Cleanup the mess left behind */` |
|    11362 |  6390 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    11362 |  6391 | `			SySetPop(&pInfo->aStep);` |
|    11362 |  6392 | `			PH7_HashmapUnref(pMap);` |
|     5682 |  6393 | `		}else{` |
|   174618 |  6394 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      426 |  6395 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      426 |  6396 | `				if( pKey ){` |
|      426 |  6397 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      212 |  6398 | `				}` |
|      212 |  6399 | `			}` |
|   174618 |  6400 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6401 | `				SyHashEntry *pEntry;` |
|        - |  6402 | `				/* Pass by reference */` |
|       23 |  6403 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6404 | `				if( pEntry ){` |
|       21 |  6405 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6406 | `				}else{` |
|        4 |  6407 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6408 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6409 | `				}` |
|       12 |  6410 | `			}else{` |
|        - |  6411 | `				/* Make a copy of the entry value */` |
|   174596 |  6412 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   174596 |  6413 | `				if( pValue ){` |
|   174596 |  6414 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    87297 |  6415 | `				}` |
|        - |  6416 | `			}` |
|        2 |  6417 | `		}` |
|    93118 |  6418 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6419 | `		/* Iterator-based iteration.` |
|        - |  6420 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6421 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6422 | `		 */` |
|      106 |  6423 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6424 | `		ph7_class_method *pMethod;` |
|        - |  6425 | `		ph7_value sResult;` |
|      106 |  6426 | `		int isValid = 0;` |
|        - |  6427 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  6428 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  6429 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  6430 | `		}else{` |
|       82 |  6431 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  6432 | `			if( pMethod ){` |
|       82 |  6433 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  6434 | `			}` |
|        - |  6435 | `		}` |
|        - |  6436 | `		/* Call valid() */` |
|      106 |  6437 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  6438 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  6439 | `		if( pMethod ){` |
|      106 |  6440 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  6441 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  6442 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  6443 | `		}` |
|      106 |  6444 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  6445 | `		if( !isValid ){` |
|        - |  6446 | `			/* Iterator exhausted */` |
|       24 |  6447 | `			pc = pInstr->iP2 - 1;` |
|        - |  6448 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  6449 | `			if( pStep->pOwner ){` |
|        3 |  6450 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6451 | `			}` |
|       24 |  6452 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  6453 | `			SySetPop(&pInfo->aStep);` |
|       24 |  6454 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  6455 | `		}else{` |
|        - |  6456 | `			/* Call current() to get value */` |
|       84 |  6457 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  6458 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  6459 | `			if( pMethod ){` |
|       84 |  6460 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  6461 | `			}` |
|       84 |  6462 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  6463 | `			if( pValue ){` |
|       84 |  6464 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  6465 | `			}` |
|       84 |  6466 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6467 | `			/* Call key() if needed */` |
|       84 |  6468 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6469 | `				ph7_value sKey;` |
|       35 |  6470 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6471 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6472 | `				if( pMethod ){` |
|       35 |  6473 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6474 | `				}` |
|       35 |  6475 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6476 | `				if( pValue ){` |
|       35 |  6477 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6478 | `				}` |
|       35 |  6479 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6480 | `			}` |
|        - |  6481 | `		}` |
|       54 |  6482 | `	}else{` |
|       25 |  6483 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6484 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6485 | `		SyHashEntry *pEntry;` |
|        - |  6486 | `		/* Point to the next attribute */` |
|       29 |  6487 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6488 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6489 | `			/* Check access permission */` |
|       31 |  6490 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6491 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6492 | `					break; /* Access is granted */` |
|        - |  6493 | `			}` |
|        1 |  6494 | `		}` |
|       25 |  6495 | `		if( pEntry == 0 ){` |
|        - |  6496 | `			/* Clean up the mess left behind */` |
|        9 |  6497 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6498 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6499 | `				/* Break the reference with the last element */` |
|        3 |  6500 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6501 | `			}` |
|        9 |  6502 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6503 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6504 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6505 | `		}else{` |
|       17 |  6506 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6507 | `			ph7_value *pAttrValue;` |
|       17 |  6508 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6509 | `				/* Fill with the current attribute name */` |
|       17 |  6510 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6511 | `				if( pKey ){` |
|       17 |  6512 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6513 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6514 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6515 | `				}` |
|        8 |  6516 | `			}` |
|        - |  6517 | `			/* Extract attribute value */` |
|       17 |  6518 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  6519 | `			if( pAttrValue ){` |
|       17 |  6520 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6521 | `					/* Pass by reference */` |
|        3 |  6522 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  6523 | `					if( pEntry ){` |
|        3 |  6524 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  6525 | `					}else{` |
|      ! 0 |  6526 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  6527 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  6528 | `					}` |
|        2 |  6529 | `				}else{` |
|        - |  6530 | `					/* Make a copy of the attribute value */` |
|       15 |  6531 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  6532 | `					if( pValue ){` |
|       15 |  6533 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  6534 | `					}` |
|        - |  6535 | `				}` |
|        8 |  6536 | `			}` |
|        - |  6537 | `		}` |
|        - |  6538 | `	}` |
|   186106 |  6539 | `	break;` |
|        - |  6540 | `						  }` |
|        - |  6541 | `/*` |
|        - |  6542 | ` * OP_MEMBER P1 P2` |
|        - |  6543 | ` * Load class attribute/method on the stack.` |
|        - |  6544 | ` */` |
|     3277 |  6545 | `case PH7_OP_MEMBER: {` |
|        - |  6546 | `	ph7_class_instance *pThis;` |
|        - |  6547 | `	ph7_value *pNos;` |
|        - |  6548 | `	SyString sName;` |
|     6556 |  6549 | `	if( !pInstr->iP1 ){` |
|     6330 |  6550 | `		pNos = &pTos[-1];` |
|        - |  6551 | `#ifdef UNTRUST` |
|        - |  6552 | `		if( pNos < pStack ){` |
|        - |  6553 | `			goto Abort;` |
|        - |  6554 | `		}` |
|        - |  6555 | `#endif` |
|     6330 |  6556 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6557 | `			ph7_class *pClass;` |
|        - |  6558 | `			/* Class already instantiated */` |
|     6328 |  6559 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  6560 | `			/* Point to the instantiated class */` |
|     6328 |  6561 | `			pClass = pThis->pClass;` |
|        - |  6562 | `			/* Extract attribute name first */` |
|     6328 |  6563 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     6328 |  6564 | `			if( pInstr->iP2 ){` |
|        - |  6565 | `				/* Method call */` |
|      666 |  6566 | `				ph7_class_method *pMeth = 0;` |
|      666 |  6567 | `				if( sName.nByte > 0 ){` |
|        - |  6568 | `					/* Extract the target method */` |
|      666 |  6569 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      332 |  6570 | `				}` |
|      666 |  6571 | `				if( pMeth == 0 ){` |
|      ! 0 |  6572 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  6573 | `						&pClass->sName,&sName` |
|        - |  6574 | `						);` |
|        - |  6575 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  6576 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  6577 | `					/* Pop the method name from the stack */` |
|      ! 0 |  6578 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6579 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  6580 | `				}else{` |
|        - |  6581 | `					/* Push method name on the stack */` |
|      666 |  6582 | `					PH7_MemObjRelease(pTos);` |
|      666 |  6583 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      666 |  6584 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6585 | `				}` |
|      666 |  6586 | `				pTos->nIdx = SXU32_HIGH;` |
|      334 |  6587 | `			}else{` |
|        - |  6588 | `				/* Attribute access */` |
|     5664 |  6589 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  6590 | `				SyHashEntry *pEntry;` |
|        - |  6591 | `				/* Extract the target attribute */` |
|     5664 |  6592 | `				if( sName.nByte > 0 ){` |
|     5664 |  6593 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     5664 |  6594 | `					if( pEntry ){` |
|        - |  6595 | `						/* Point to the attribute value */` |
|     5662 |  6596 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2830 |  6597 | `					}` |
|     2831 |  6598 | `				}` |
|     5664 |  6599 | `				if( pObjAttr == 0 ){` |
|        - |  6600 | `					/* No such attribute,load null */` |
|        4 |  6601 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  6602 | `						&pClass->sName,&sName);` |
|        - |  6603 | `					/* Call the __get magic method if available */` |
|        3 |  6604 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  6605 | `				}` |
|     5664 |  6606 | `				VmPopOperand(&pTos,1);` |
|        - |  6607 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  6608 | `				 * This is due to the following case:` |
|        - |  6609 | `				 *     (new TestClass())->foo;` |
|        - |  6610 | `				 */` |
|     5664 |  6611 | `				pThis->iRef++;` |
|     5664 |  6612 | `				PH7_MemObjRelease(pTos);` |
|     5664 |  6613 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     5664 |  6614 | `				if( pObjAttr ){` |
|     5662 |  6615 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  6616 | `					/* Check attribute access */` |
|     5662 |  6617 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  6618 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  6619 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  6620 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  6621 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  6622 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     5660 |  6623 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     2867 |  6624 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       72 |  6625 | `							VmInstr *pNext = pInstr + 1;` |
|       72 |  6626 | `							int bIsLhs = 0;` |
|       72 |  6627 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       70 |  6628 | `								bIsLhs = 1;` |
|       34 |  6629 | `							}` |
|       72 |  6630 | `							if( !bIsLhs ){` |
|        3 |  6631 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  6632 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  6633 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  6634 | `									goto Abort;` |
|        - |  6635 | `								}` |
|        - |  6636 | `								{` |
|        3 |  6637 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6638 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6639 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3277 |  6640 | `										break;` |
|        - |  6641 | `									}` |
|        - |  6642 | `								}` |
|      ! 0 |  6643 | `								goto Exception;` |
|        - |  6644 | `							}` |
|       34 |  6645 | `						}` |
|        - |  6646 | `						/* Load attribute */` |
|     5660 |  6647 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     5660 |  6648 | `						if( pValue ){` |
|     5660 |  6649 | `							if( pThis->iRef < 2 ){` |
|        - |  6650 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  6651 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  6652 | `								 */` |
|        7 |  6653 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  6654 | `							}else{` |
|        - |  6655 | `								/* Simple load */` |
|     5654 |  6656 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  6657 | `							}` |
|     5660 |  6658 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     5658 |  6659 | `								if( pThis->iRef > 1 ){` |
|        - |  6660 | `									/* Load attribute index */` |
|     5652 |  6661 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2825 |  6662 | `								}` |
|     2828 |  6663 | `							}` |
|     2829 |  6664 | `						}` |
|     2831 |  6665 | `					}else{` |
|        - |  6666 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  6667 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  6668 | `						char zMsg[256];` |
|      ! 0 |  6669 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6670 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6671 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6672 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  6673 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6674 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6675 | `						goto Abort;` |
|        - |  6676 | `					}` |
|     2829 |  6677 | `				}` |
|        - |  6678 | `				/* Safely unreference the object */` |
|     5662 |  6679 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  6680 | `			}` |
|     3164 |  6681 | `		}else{` |
|        3 |  6682 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  6683 | `			VmPopOperand(&pTos,1);` |
|        3 |  6684 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6685 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  6686 | `		}` |
|     3165 |  6687 | `	}else{` |
|        - |  6688 | `		/* Static member access using class name */` |
|      228 |  6689 | `		pNos = pTos;` |
|      228 |  6690 | `		pThis = 0;` |
|      228 |  6691 | `		if( !pInstr->p3 ){` |
|      190 |  6692 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      190 |  6693 | `			pNos--;` |
|        - |  6694 | `#ifdef UNTRUST` |
|        - |  6695 | `			if( pNos < pStack ){` |
|        - |  6696 | `				goto Abort;` |
|        - |  6697 | `			}` |
|        - |  6698 | `#endif` |
|       96 |  6699 | `		}else{` |
|        - |  6700 | `			/* Attribute name already computed */` |
|       40 |  6701 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6702 | `		}` |
|      228 |  6703 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      228 |  6704 | `			ph7_class *pClass = 0;` |
|      228 |  6705 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6706 | `				/* Class already instantiated */` |
|        5 |  6707 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  6708 | `				pClass = pThis->pClass;` |
|        5 |  6709 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  6710 | `			}else{` |
|        - |  6711 | `				/* Try to extract the target class */` |
|      224 |  6712 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      224 |  6713 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      224 |  6714 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  6715 | `					/* Handle self/static/parent keywords */` |
|      224 |  6716 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  6717 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  6718 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  6719 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  6720 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  6721 | `						}` |
|      194 |  6722 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  6723 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      164 |  6724 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  6725 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  6726 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  6727 | `							pClass = pSelf->pBase;` |
|       13 |  6728 | `						}` |
|       15 |  6729 | `					}else{` |
|      112 |  6730 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  6731 | `					}` |
|      111 |  6732 | `				}` |
|        - |  6733 | `			}` |
|      228 |  6734 | `			if( pClass == 0 ){` |
|        - |  6735 | `				/* Undefined class */` |
|      ! 0 |  6736 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  6737 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  6738 | `					);` |
|      ! 0 |  6739 | `				if( !pInstr->p3 ){` |
|      ! 0 |  6740 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6741 | `				}` |
|      ! 0 |  6742 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6743 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  6744 | `			}else{` |
|      228 |  6745 | `				if( pInstr->iP2 ){` |
|        - |  6746 | `					/* Method call */` |
|       86 |  6747 | `					ph7_class_method *pMeth = 0;` |
|       86 |  6748 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  6749 | `						/* Extract the target method */` |
|       86 |  6750 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  6751 | `					}` |
|       86 |  6752 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  6753 | `						if( pMeth ){` |
|      ! 0 |  6754 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  6755 | `								&pClass->sName,&sName` |
|        - |  6756 | `								);` |
|      ! 0 |  6757 | `						}else{` |
|      ! 0 |  6758 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6759 | `								&pClass->sName,&sName` |
|        - |  6760 | `								);` |
|        - |  6761 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  6762 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  6763 | `						}` |
|        - |  6764 | `						/* Pop the method name from the stack */` |
|      ! 0 |  6765 | `						if( !pInstr->p3 ){` |
|      ! 0 |  6766 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  6767 | `						}` |
|      ! 0 |  6768 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  6769 | `					}else{` |
|        - |  6770 | `						/* Push method name on the stack */` |
|       86 |  6771 | `						PH7_MemObjRelease(pTos);` |
|       86 |  6772 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  6773 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6774 | `					}` |
|       86 |  6775 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  6776 | `				}else{` |
|        - |  6777 | `					/* Attribute access */` |
|      144 |  6778 | `					ph7_class_attr *pAttr = 0;` |
|        - |  6779 | `					/* Check for special ::class pseudo-constant */` |
|      190 |  6780 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  6781 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  6782 | `						/* ::class returns the fully qualified class name */` |
|        - |  6783 | `						/* Pop the attribute name from the stack */` |
|       60 |  6784 | `						if( !pInstr->p3 ){` |
|       60 |  6785 | `							VmPopOperand(&pTos,1);` |
|       29 |  6786 | `						}` |
|       60 |  6787 | `						PH7_MemObjRelease(pTos);` |
|        - |  6788 | `						/* Load the class name */` |
|       60 |  6789 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  6790 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  6791 | `					}else{` |
|        - |  6792 | `						/* Extract the target attribute */` |
|       86 |  6793 | `						if( sName.nByte > 0 ){` |
|       86 |  6794 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       42 |  6795 | `						}` |
|       86 |  6796 | `						if( pAttr == 0 ){` |
|        - |  6797 | `							/* No such attribute,load null */` |
|      ! 0 |  6798 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6799 | `								&pClass->sName,&sName);` |
|        - |  6800 | `							/* Call the __get magic method if available */` |
|      ! 0 |  6801 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  6802 | `						}` |
|        - |  6803 | `						/* Pop the attribute name from the stack */` |
|       86 |  6804 | `						if( !pInstr->p3 ){` |
|       48 |  6805 | `							VmPopOperand(&pTos,1);` |
|       23 |  6806 | `						}` |
|       86 |  6807 | `						PH7_MemObjRelease(pTos);` |
|       86 |  6808 | `						pTos->nIdx = SXU32_HIGH;` |
|       86 |  6809 | `						if( pAttr ){` |
|       86 |  6810 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  6811 | `								/* Access to a non static attribute */` |
|      ! 0 |  6812 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6813 | `									&pClass->sName,&pAttr->sName` |
|        - |  6814 | `									);` |
|      ! 0 |  6815 | `							}else{` |
|        - |  6816 | `								ph7_value *pValue;` |
|        - |  6817 | `								/* Check if the access to the attribute is allowed */` |
|       86 |  6818 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  6819 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  6820 | `									 * Same LHS-of-store peek as the instance path. */` |
|       80 |  6821 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       55 |  6822 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  6823 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  6824 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  6825 | `										if( pS ){` |
|       28 |  6826 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  6827 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  6828 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  6829 | `												int bIsLhs = 0;` |
|        8 |  6830 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  6831 | `													bIsLhs = 1;` |
|        2 |  6832 | `												}` |
|        8 |  6833 | `												if( !bIsLhs ){` |
|        3 |  6834 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  6835 | `													if( pThis ){` |
|      ! 0 |  6836 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6837 | `													}` |
|        3 |  6838 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  6839 | `														goto Abort;` |
|        - |  6840 | `													}` |
|        - |  6841 | `													{` |
|        3 |  6842 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6843 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6844 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  6845 | `															break;` |
|        - |  6846 | `														}` |
|        - |  6847 | `													}` |
|      ! 0 |  6848 | `													goto Exception;` |
|        - |  6849 | `												}` |
|        2 |  6850 | `											}` |
|       12 |  6851 | `										}` |
|       12 |  6852 | `									}` |
|        - |  6853 | `									/* Load the desired attribute */` |
|       80 |  6854 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       80 |  6855 | `									if( pValue ){` |
|       80 |  6856 | `										PH7_MemObjLoad(pValue,pTos);` |
|       80 |  6857 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  6858 | `											/* Load index number */` |
|       38 |  6859 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  6860 | `										}` |
|       39 |  6861 | `									}` |
|       41 |  6862 | `								}else{` |
|        - |  6863 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  6864 | `									char zMsg[256];` |
|        5 |  6865 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  6866 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  6867 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  6868 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  6869 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  6870 | `									}else{` |
|      ! 0 |  6871 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6872 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6873 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  6874 | `									}` |
|        5 |  6875 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  6876 | `									goto Abort;` |
|        - |  6877 | `								}` |
|        - |  6878 | `							}` |
|       39 |  6879 | `						}` |
|        - |  6880 | `					}` |
|        - |  6881 | `				}` |
|      222 |  6882 | `				if( pThis ){` |
|        - |  6883 | `					/* Safely unreference the object */` |
|        5 |  6884 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  6885 | `				}` |
|        - |  6886 | `			}` |
|      112 |  6887 | `		}else{` |
|        - |  6888 | `			/* Pop operands */` |
|      ! 0 |  6889 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  6890 | `			if( !pInstr->p3 ){` |
|      ! 0 |  6891 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  6892 | `			}` |
|      ! 0 |  6893 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6894 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6895 | `		}` |
|        - |  6896 | `	}` |
|     6548 |  6897 | `	break;` |
|        - |  6898 | `					}` |
|        - |  6899 | `/*` |
|        - |  6900 | ` * OP_NEW P1 * * *` |
|        - |  6901 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  6902 | ` */` |
|      531 |  6903 | `case PH7_OP_NEW: {` |
|     1064 |  6904 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1064 |  6905 | `	ph7_class *pClass = 0;` |
|        - |  6906 | `	ph7_class_instance *pNew;` |
|     1064 |  6907 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  6908 | `		/* Try to extract the desired class */` |
|     1595 |  6909 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1062 |  6910 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      531 |  6911 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6912 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6913 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6914 | `	}` |
|     1064 |  6915 | `	if( pClass == 0 ){` |
|        - |  6916 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6917 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6918 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6919 | `			);` |
|        - |  6920 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6921 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6922 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6923 | `			/* Pop given arguments */` |
|      ! 0 |  6924 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6925 | `		}` |
|      ! 0 |  6926 | `		goto Abort;` |
|      ! 0 |  6927 | `	}else{` |
|        - |  6928 | `		ph7_class_method *pCons;` |
|        - |  6929 | `		/* Create a new class instance */` |
|     1064 |  6930 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1064 |  6931 | `		if( pNew == 0 ){` |
|      ! 0 |  6932 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6933 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6934 | `				&pClass->sName` |
|        - |  6935 | `			);` |
|      ! 0 |  6936 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6937 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6938 | `				/* Pop given arguments */` |
|      ! 0 |  6939 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6940 | `			}` |
|      ! 0 |  6941 | `			break;` |
|        - |  6942 | `		}` |
|        - |  6943 | `		/* Check if a constructor is available */` |
|     1064 |  6944 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1064 |  6945 | `		if( pCons == 0 ){` |
|      760 |  6946 | `			SyString *pName = &pClass->sName;` |
|        - |  6947 | `			/* Check for a constructor with the same base class name */` |
|      760 |  6948 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      379 |  6949 | `		}` |
|     1064 |  6950 | `		if( pCons ){` |
|        - |  6951 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  6952 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  6953 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  6954 | `			 * (including variadic string-key packing). */` |
|      306 |  6955 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      306 |  6956 | `			SySetReset(&aArg);` |
|      600 |  6957 | `			while( pArg < pTos ){` |
|      296 |  6958 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      296 |  6959 | `				pArg++;` |
|        2 |  6960 | `			}` |
|      306 |  6961 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  6962 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6963 | `				sxu32 n;` |
|       61 |  6964 | `				n = SySetUsed(&aArg);` |
|        - |  6965 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  6966 | `				 * for named args the missing-arg check happens downstream` |
|        - |  6967 | `				 * after resolution). */` |
|      109 |  6968 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  6969 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  6970 | `					if( pFuncArg ){` |
|       49 |  6971 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6972 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6973 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6974 | `						}` |
|       24 |  6975 | `					}` |
|       49 |  6976 | `					n++;` |
|        1 |  6977 | `				}` |
|       30 |  6978 | `			}` |
|      306 |  6979 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  6980 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      306 |  6981 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6982 | `				pNew->iRef = 1;` |
|      ! 0 |  6983 | `			}` |
|      152 |  6984 | `		}` |
|     1064 |  6985 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6986 | `			/* Pop given arguments */` |
|      242 |  6987 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      120 |  6988 | `		}` |
|     1064 |  6989 | `		PH7_MemObjRelease(pTos);` |
|     1064 |  6990 | `		pTos->x.pOther = pNew;` |
|     1064 |  6991 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6992 | `	}` |
|     1064 |  6993 | `	break;` |
|        - |  6994 | `				 }` |
|        - |  6995 | `/*` |
|        - |  6996 | ` * OP_CLONE * * *` |
|        - |  6997 | ` * Perfome a clone operation.` |
|        - |  6998 | ` */` |
|       24 |  6999 | `case PH7_OP_CLONE: {` |
|        - |  7000 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  7001 | `#ifdef UNTRUST` |
|        - |  7002 | `	if( pTos < pStack ){` |
|        - |  7003 | `		goto Abort;` |
|        - |  7004 | `	}` |
|        - |  7005 | `#endif` |
|        - |  7006 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  7007 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  7008 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7009 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  7010 | `		PH7_MemObjRelease(pTos);` |
|        5 |  7011 | `		break;` |
|        - |  7012 | `	}` |
|        - |  7013 | `	/* Point to the source */` |
|       46 |  7014 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7015 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  7016 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  7017 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7018 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  7019 | `			&pSrc->pClass->sName);` |
|      ! 0 |  7020 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7021 | `		break;` |
|        - |  7022 | `	}` |
|        - |  7023 | `	/* Perform the clone operation */` |
|       46 |  7024 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  7025 | `	PH7_MemObjRelease(pTos);` |
|       46 |  7026 | `	if( pClone == 0 ){` |
|      ! 0 |  7027 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7028 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  7029 | `	}else{` |
|        - |  7030 | `		/* Load the cloned object */` |
|       46 |  7031 | `		pTos->x.pOther = pClone;` |
|       46 |  7032 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7033 | `	}` |
|       46 |  7034 | `	break;` |
|        - |  7035 | `				   }` |
|        - |  7036 | `/*` |
|        - |  7037 | ` * OP_SWITCH * * P3` |
|        - |  7038 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  7039 | ` */` |
|       26 |  7040 | `case PH7_OP_SWITCH: {` |
|       54 |  7041 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  7042 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  7043 | `	ph7_value sValue,sCaseValue;` |
|        - |  7044 | `	sxu32 n,nEntry;` |
|        - |  7045 | `#ifdef UNTRUST` |
|        - |  7046 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  7047 | `		goto Abort;` |
|        - |  7048 | `	}` |
|        - |  7049 | `#endif` |
|        - |  7050 | `	/* Point to the case table  */` |
|       54 |  7051 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  7052 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  7053 | `	/* Select the appropriate case block to execute */` |
|       54 |  7054 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  7055 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  7056 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  7057 | `		pCase = &aCase[n];` |
|      130 |  7058 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  7059 | `		/* Execute the case expression first */` |
|      130 |  7060 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  7061 | `		/* Compare the two expression */` |
|      130 |  7062 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  7063 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  7064 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  7065 | `		if( rc == 0 ){` |
|        - |  7066 | `			/* Value match,jump to this block */` |
|       52 |  7067 | `			pc = pCase->nStart - 1;` |
|       52 |  7068 | `			break;` |
|        - |  7069 | `		}` |
|       41 |  7070 | `	}` |
|       54 |  7071 | `	VmPopOperand(&pTos,1);` |
|       54 |  7072 | `	if( n >= nEntry ){` |
|        - |  7073 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  7074 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  7075 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  7076 | `		}else{` |
|        - |  7077 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  7078 | `			pc = pSwitch->nOut - 1;` |
|        - |  7079 | `		}` |
|        1 |  7080 | `	}` |
|       54 |  7081 | `	break;` |
|        - |  7082 | `					}` |
|        - |  7083 | `/*` |
|        - |  7084 | ` * OP_MATCH * * P3` |
|        - |  7085 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  7086 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  7087 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  7088 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  7089 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  7090 | ` */` |
|       54 |  7091 | `case PH7_OP_MATCH: {` |
|      110 |  7092 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  7093 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  7094 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  7095 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  7096 | `	int matched = 0;` |
|        - |  7097 | `#ifdef UNTRUST` |
|        - |  7098 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  7099 | `		goto Abort;` |
|        - |  7100 | `	}` |
|        - |  7101 | `#endif` |
|      110 |  7102 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  7103 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  7104 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  7105 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  7106 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  7107 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  7108 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  7109 | `		pArm = &aArm[i];` |
|      240 |  7110 | `		if( pArm->bDefault ){` |
|       13 |  7111 | `			pDefault = pArm;` |
|       13 |  7112 | `			continue;` |
|        - |  7113 | `		}` |
|      228 |  7114 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  7115 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  7116 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  7117 | `			if( pCondBc == 0 ){` |
|      ! 0 |  7118 | `				continue;` |
|        - |  7119 | `			}` |
|      260 |  7120 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  7121 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  7122 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  7123 | `			if( rc == 0 ){` |
|       93 |  7124 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  7125 | `				matched = 1;` |
|       93 |  7126 | `				break;` |
|        - |  7127 | `			}` |
|       85 |  7128 | `		}` |
|      115 |  7129 | `	}` |
|      110 |  7130 | `	if( !matched && pDefault ){` |
|       13 |  7131 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  7132 | `		matched = 1;` |
|        6 |  7133 | `	}` |
|      110 |  7134 | `	if( !matched ){` |
|        5 |  7135 | `		const char *zType = "unknown";` |
|        - |  7136 | `		char zMsg[128];` |
|        - |  7137 | `		sxu32 nMsg;` |
|        5 |  7138 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  7139 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  7140 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  7141 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  7142 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  7143 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  7144 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  7145 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  7146 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  7147 | `		default: break;` |
|        - |  7148 | `		}` |
|        7 |  7149 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  7150 | `			"Unhandled match case of type %s",zType);` |
|        7 |  7151 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  7152 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  7153 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  7154 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  7155 | `		goto Abort;` |
|        - |  7156 | `	}` |
|      105 |  7157 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  7158 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  7159 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  7160 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  7161 | `	break;` |
|        - |  7162 | `					}` |
|        - |  7163 | `/*` |
|        - |  7164 | ` * OP_YIELD P1 P2 *` |
|        - |  7165 | ` *  Yield a value from a generator function.` |
|        - |  7166 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  7167 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  7168 | ` */` |
|       34 |  7169 | `case PH7_OP_YIELD: {` |
|        - |  7170 | `	ph7_generator *pGen;` |
|       70 |  7171 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  7172 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  7173 | `		goto Abort;` |
|        - |  7174 | `	}` |
|       70 |  7175 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  7176 | `	if( pInstr->iP2 ){` |
|        - |  7177 | `		/* yield $key => $value: value on top, key below */` |
|        - |  7178 | `#ifdef UNTRUST` |
|        - |  7179 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  7180 | `#endif` |
|        7 |  7181 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7182 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7183 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7184 | `		VmPopOperand(&pTos, 1);` |
|        - |  7185 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7186 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7187 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7188 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7189 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7190 | `			}` |
|        1 |  7191 | `		}` |
|       67 |  7192 | `	}else if( pInstr->iP1 ){` |
|        - |  7193 | `		/* yield $value */` |
|        - |  7194 | `#ifdef UNTRUST` |
|        - |  7195 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7196 | `#endif` |
|       64 |  7197 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7198 | `		VmPopOperand(&pTos, 1);` |
|        - |  7199 | `		/* Auto-increment key */` |
|       64 |  7200 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7201 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7202 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7203 | `	}else{` |
|        - |  7204 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7205 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7206 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7207 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7208 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7209 | `	}` |
|        - |  7210 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7211 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7212 | `	goto Suspend;` |
|        - |  7213 |  |
|        - |  7214 | `/*` |
|        - |  7215 | ` * OP_CALL P1 * *` |
|        - |  7216 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7217 | ` *  function on the stack.` |
|        - |  7218 | ` */` |
|   330880 |  7219 | `case PH7_OP_CALL: {` |
|   661806 |  7220 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7221 | `	ph7_value *pArg;` |
|   661806 |  7222 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   661806 |  7223 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7224 | `	SyHashEntry *pEntry;` |
|        - |  7225 | `	SyString sName;` |
|        - |  7226 | `	/* Extract function name */` |
|   661806 |  7227 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  7228 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7229 | `			ph7_value sResult;` |
|      ! 0 |  7230 | `			SySetReset(&aArg);` |
|      ! 0 |  7231 | `			while( pArg < pTos ){` |
|      ! 0 |  7232 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7233 | `				pArg++;` |
|      ! 0 |  7234 | `			}` |
|      ! 0 |  7235 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7236 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7237 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7238 | `			SySetReset(&aArg);` |
|        - |  7239 | `			/* Pop given arguments */` |
|      ! 0 |  7240 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7241 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7242 | `			}` |
|        - |  7243 | `			/* Copy result */` |
|      ! 0 |  7244 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7245 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7246 | `		}else{` |
|        3 |  7247 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  7248 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7249 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  7250 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  7251 | `			}else{` |
|        - |  7252 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  7253 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7254 | `			}` |
|        - |  7255 | `			/* Pop given arguments */` |
|        3 |  7256 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7257 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7258 | `			}` |
|        - |  7259 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7260 | `			PH7_MemObjRelease(pTos);` |
|        - |  7261 | `		}` |
|   330599 |  7262 | `		break;` |
|        - |  7263 | `	}` |
|   661804 |  7264 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7265 | `	/* Check for a compiled function first.` |
|        - |  7266 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7267 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   661804 |  7268 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7269 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7270 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7271 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7272 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7273 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7274 | `	{` |
|   661804 |  7275 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   661804 |  7276 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7277 | `		const char *zFunc;` |
|        - |  7278 | `		const char *zEnd;` |
|        - |  7279 | `		const char *z;` |
|        - |  7280 | `		SyString sGlobal;` |
|       22 |  7281 | `		zFunc = sName.zString;` |
|       22 |  7282 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  7283 | `		z = zEnd;` |
|        - |  7284 | `		/* Find last namespace separator */` |
|      194 |  7285 | `		while( z > zFunc ){` |
|      194 |  7286 | `			if( z[-1] == '\\' ){` |
|       22 |  7287 | `				break;` |
|        - |  7288 | `			}` |
|      174 |  7289 | `			z--;` |
|        2 |  7290 | `		}` |
|       22 |  7291 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7292 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  7293 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  7294 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  7295 | `		}` |
|       10 |  7296 | `	}` |
|        - |  7297 | `	} /* end VmCallArgMap namespace scope */` |
|   661804 |  7298 | `	if( pEntry ){` |
|        - |  7299 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7300 | `		ph7_class_instance *pThis;` |
|        - |  7301 | `		ph7_value *pFrameStack;` |
|        - |  7302 | `		ph7_vm_func *pVmFunc;` |
|        - |  7303 | `		ph7_class *pSelf;` |
|        - |  7304 | `		VmFrame *pFrame;` |
|        - |  7305 | `		ph7_value *pObj;` |
|        - |  7306 | `		VmSlot sArg;` |
|        - |  7307 | `		sxu32 n;` |
|        - |  7308 | `		/* initialize fields */` |
|    16112 |  7309 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    16112 |  7310 | `		pThis = 0;` |
|    16112 |  7311 | `		pSelf = 0;` |
|    16112 |  7312 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7313 | `			ph7_class_method *pMeth;` |
|        - |  7314 | `			/* Class method call */` |
|     2570 |  7315 | `			ph7_value *pTarget = &pTos[-1];` |
|     2570 |  7316 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7317 | `				/* Extract the 'this' pointer */` |
|     2570 |  7318 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7319 | `					/* Instance already loaded */` |
|     2480 |  7320 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2480 |  7321 | `					pThis->iRef++;` |
|     2480 |  7322 | `					pSelf = pThis->pClass;` |
|     1239 |  7323 | `				}` |
|     2570 |  7324 | `				if( pSelf == 0 ){` |
|       92 |  7325 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7326 | `						/* "Late Static Binding" class name */` |
|      128 |  7327 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  7328 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  7329 | `					}` |
|       92 |  7330 | `					if( pSelf == 0 ){` |
|       21 |  7331 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  7332 | `					}` |
|       45 |  7333 | `				}` |
|     2570 |  7334 | `				if( pThis == 0  ){` |
|       92 |  7335 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  7336 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  7337 | `					if( pFrameLocal->pParent ){` |
|        - |  7338 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  7339 | `						pThis = pFrameLocal->pThis;` |
|       66 |  7340 | `						if( pThis ){` |
|       21 |  7341 | `							pThis->iRef++;` |
|       10 |  7342 | `						}` |
|       32 |  7343 | `					}` |
|       45 |  7344 | `				}` |
|     2570 |  7345 | `				VmPopOperand(&pTos,1);` |
|     2570 |  7346 | `				PH7_MemObjRelease(pTos);` |
|        - |  7347 | `				/* Synchronize pointers */` |
|     2570 |  7348 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7349 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7350 | `				 * user have already computed the random generated unique class method name` |
|        - |  7351 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7352 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7353 | `				 */` |
|     2570 |  7354 | `				while( pArg < pStack ){` |
|      ! 0 |  7355 | `					pArg++;` |
|      ! 0 |  7356 | `				}` |
|     2570 |  7357 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7358 | `					/* Check if the call is allowed */` |
|     2570 |  7359 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2570 |  7360 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7361 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7362 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7363 | `							char zMsg[256];` |
|      ! 0 |  7364 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7365 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7366 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7367 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7368 | `							/* Pop given arguments */` |
|      ! 0 |  7369 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7370 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7371 | `							}` |
|      ! 0 |  7372 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7373 | `							goto Abort;` |
|        - |  7374 | `						}` |
|        6 |  7375 | `					}` |
|     1284 |  7376 | `				}` |
|     1284 |  7377 | `			}` |
|     1284 |  7378 | `		}` |
|        - |  7379 | `		/* Check The recursion limit */` |
|    16112 |  7380 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7381 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7382 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7383 | `				&pVmFunc->sName);` |
|        - |  7384 | `			/* Pop given arguments */` |
|        3 |  7385 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7386 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7387 | `			}` |
|        - |  7388 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7389 | `			PH7_MemObjRelease(pTos);` |
|       14 |  7390 | `			break;` |
|        - |  7391 | `		}` |
|    16110 |  7392 | `		if( pVmFunc->pNextName ){` |
|        - |  7393 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7394 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7395 | `		}` |
|    16110 |  7396 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7397 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7398 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7399 | `			ph7_generator *pGenerator;` |
|        - |  7400 | `			ph7_class_instance *pGenObj;` |
|        - |  7401 | `			ph7_value *pCtxAttr;` |
|        - |  7402 | `			SyString sAttrName;` |
|        - |  7403 | `			ph7_value **apCallArgs;` |
|        - |  7404 | `			int nGenArgs, iArg;` |
|        - |  7405 | `			/* Collect arguments from the operand stack */` |
|       24 |  7406 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  7407 | `			apCallArgs = 0;` |
|       24 |  7408 | `			if( nGenArgs > 0 ){` |
|       14 |  7409 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7410 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  7411 | `				if( apCallArgs == 0 ){` |
|        - |  7412 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7413 | `					nGenArgs = 0;` |
|      ! 0 |  7414 | `				}else{` |
|       10 |  7415 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  7416 | `					int didReorder = 0;` |
|       10 |  7417 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  7418 | `						/* Named-argument reordering for generator */` |
|        5 |  7419 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  7420 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  7421 | `						sxu32 nNV = nF;` |
|        5 |  7422 | `						sxi32 iVIdx = -1;` |
|        - |  7423 | `						sxi32 *aGSlot;` |
|        - |  7424 | `						sxu8 *aGUsed;` |
|        - |  7425 | `						sxu32 gi;` |
|       13 |  7426 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  7427 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  7428 | `						}` |
|        7 |  7429 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7430 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  7431 | `						if( aGSlot ){` |
|        5 |  7432 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  7433 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  7434 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  7435 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7436 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  7437 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7438 | `								goto Abort;` |
|        - |  7439 | `							}` |
|        - |  7440 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  7441 | `							 * append overflow (variadic / positional beyond` |
|        - |  7442 | `							 * formals) so downstream sees every argument. */` |
|        - |  7443 | `							{` |
|        5 |  7444 | `								int nOut = 0;` |
|       13 |  7445 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  7446 | `									sxu32 gj;` |
|       13 |  7447 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  7448 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  7449 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  7450 | `											break;` |
|        - |  7451 | `										}` |
|        3 |  7452 | `									}` |
|        5 |  7453 | `								}` |
|       13 |  7454 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  7455 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  7456 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  7457 | `									}` |
|        5 |  7458 | `								}` |
|        5 |  7459 | `								nGenArgs = nOut;` |
|        - |  7460 | `							}` |
|        5 |  7461 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  7462 | `							didReorder = 1;` |
|        2 |  7463 | `						}` |
|        - |  7464 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  7465 | `						 * positional fill below — preserves arg order rather` |
|        - |  7466 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  7467 | `					}` |
|       10 |  7468 | `					if( !didReorder ){` |
|       12 |  7469 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  7470 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  7471 | `						}` |
|        2 |  7472 | `					}` |
|        - |  7473 | `				}` |
|        4 |  7474 | `			}` |
|        - |  7475 | `			/* Create execution context and generator wrapper */` |
|       24 |  7476 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  7477 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  7478 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7479 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7480 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7481 | `				break;` |
|        - |  7482 | `			}` |
|       24 |  7483 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  7484 | `			if( pGenerator == 0 ){` |
|      ! 0 |  7485 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  7486 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7487 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7488 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7489 | `				break;` |
|        - |  7490 | `			}` |
|        - |  7491 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  7492 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  7493 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  7494 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  7495 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  7496 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  7497 | `			if( apCallArgs ){` |
|       10 |  7498 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  7499 | `			}` |
|       24 |  7500 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7501 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7502 | `				if( pThis ){` |
|      ! 0 |  7503 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7504 | `				}` |
|      ! 0 |  7505 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7506 | `					goto Abort;` |
|        - |  7507 | `				}` |
|      ! 0 |  7508 | `				break;` |
|        - |  7509 | `			}` |
|        - |  7510 | `			/* Create Generator class instance */` |
|       24 |  7511 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  7512 | `			if( pGenObj == 0 ){` |
|      ! 0 |  7513 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7514 | `				break;` |
|        - |  7515 | `			}` |
|        - |  7516 | `			/* Store generator in __ctx attribute */` |
|       24 |  7517 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  7518 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  7519 | `			if( pCtxAttr ){` |
|       24 |  7520 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  7521 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  7522 | `			}` |
|        - |  7523 | `			/* Pop args and function name, push Generator object */` |
|       24 |  7524 | `			PH7_MemObjRelease(pTos);` |
|       24 |  7525 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  7526 | `			pTos->x.pOther = pGenObj;` |
|       24 |  7527 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  7528 | `			pGenObj->iRef++;` |
|       24 |  7529 | `			if( pThis ){` |
|      ! 0 |  7530 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7531 | `			}` |
|       24 |  7532 | `			break;` |
|        - |  7533 | `		}` |
|        - |  7534 | `		/* Extract the formal argument set */` |
|    16088 |  7535 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  7536 | `		/* Create a new VM frame  */` |
|    16088 |  7537 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    16088 |  7538 | `		if( rc != SXRET_OK ){` |
|        - |  7539 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7540 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7541 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7542 | `				&pVmFunc->sName);` |
|        - |  7543 | `			/* Pop given arguments */` |
|      ! 0 |  7544 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7545 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7546 | `			}` |
|        - |  7547 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7548 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7549 | `			break;` |
|        - |  7550 | `		}` |
|    16088 |  7551 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  7552 | `			/* Install the '$this' variable */` |
|        - |  7553 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2498 |  7554 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2498 |  7555 | `			if( pObj ){` |
|        - |  7556 | `				/* Reflect the change */` |
|     2498 |  7557 | `				pObj->x.pOther = pThis;` |
|     2498 |  7558 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1248 |  7559 | `			}` |
|     1248 |  7560 | `		}` |
|    16088 |  7561 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  7562 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  7563 | `			/* Install static variables */` |
|      ! 0 |  7564 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  7565 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  7566 | `				pStatic = &aStatic[n];` |
|      ! 0 |  7567 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  7568 | `					/* Initialize the static variables */` |
|      ! 0 |  7569 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  7570 | `					if( pObj ){` |
|        - |  7571 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  7572 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  7573 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  7574 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  7575 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  7576 | `						}` |
|      ! 0 |  7577 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  7578 | `					}else{` |
|      ! 0 |  7579 | `						continue;` |
|        - |  7580 | `					}` |
|      ! 0 |  7581 | `				}` |
|        - |  7582 | `				/* Install in the current frame */` |
|      ! 0 |  7583 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  7584 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  7585 | `			}` |
|      ! 0 |  7586 | `		}` |
|        - |  7587 | `		/* Push arguments in the local frame */` |
|        - |  7588 | `		{` |
|    16088 |  7589 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|    16088 |  7590 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  7591 | `			/* ============================================================` |
|        - |  7592 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  7593 | `			 *` |
|        - |  7594 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  7595 | `			 * or position, then install them in the frame.` |
|        - |  7596 | `			 * ============================================================ */` |
|       90 |  7597 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       90 |  7598 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       90 |  7599 | `			sxi32 iVariadicIdx = -1;` |
|        - |  7600 | `			sxu32 nNonVariadic;` |
|        - |  7601 | `			sxi32 *aSlot;` |
|        - |  7602 | `			sxu8  *aUsed;` |
|        - |  7603 | `			sxu32 i;` |
|        - |  7604 | `			/* Find variadic parameter index */` |
|      274 |  7605 | `			for( i = 0; i < nFormal; i++ ){` |
|      194 |  7606 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  7607 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  7608 | `					break;` |
|        - |  7609 | `				}` |
|       94 |  7610 | `			}` |
|       90 |  7611 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  7612 | `			/* Allocate mapping arrays */` |
|      134 |  7613 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       88 |  7614 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       90 |  7615 | `			if( aSlot == 0 ){` |
|      ! 0 |  7616 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  7617 | `				goto Abort;` |
|        - |  7618 | `			}` |
|       90 |  7619 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  7620 | `			/* Resolve named arguments to formal parameters */` |
|      134 |  7621 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       44 |  7622 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       90 |  7623 | `			if( rc == PH7_ABORT ){` |
|        7 |  7624 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  7625 | `				goto Abort;` |
|        - |  7626 | `			}` |
|        - |  7627 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      257 |  7628 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  7629 | `				/* Find the stack arg mapped to formal n */` |
|      175 |  7630 | `				sxi32 iSrc = -1;` |
|      291 |  7631 | `				for( i = 0; i < nActual; i++ ){` |
|      273 |  7632 | `					if( aSlot[i] == (sxi32)n ){` |
|      157 |  7633 | `						iSrc = (sxi32)i;` |
|      157 |  7634 | `						break;` |
|        - |  7635 | `					}` |
|       59 |  7636 | `				}` |
|      175 |  7637 | `				if( iSrc >= 0 ){` |
|        - |  7638 | `					/* Argument was provided — install with type checking */` |
|      157 |  7639 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  7640 | `					/* NULL-to-default redirect (existing behavior) */` |
|      156 |  7641 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  7642 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  7643 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  7644 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7645 | `					}` |
|        - |  7646 | `					/* Type checking: union types */` |
|      157 |  7647 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  7648 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  7649 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       13 |  7650 | `						if( rcU != SXRET_OK ){` |
|        - |  7651 | `							const char *zGiven;` |
|        - |  7652 | `							char zBuf[128];` |
|      ! 0 |  7653 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7654 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  7655 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7656 | `								zGiven = "null";` |
|      ! 0 |  7657 | `							}else{` |
|      ! 0 |  7658 | `								zGiven = ph7_type_name(pVal);` |
|        - |  7659 | `							}` |
|      ! 0 |  7660 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7661 | `								&aFormalArg[n].sName,` |
|      ! 0 |  7662 | `								SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|      ! 0 |  7663 | `									? aFormalArg[n].sTypeName.zString : "union",` |
|      ! 0 |  7664 | `								zGiven);` |
|      ! 0 |  7665 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7666 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7667 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  7668 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7669 | `							pFrameStack = 0;` |
|      ! 0 |  7670 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  7671 | `							goto SkipFuncBody;` |
|        - |  7672 | `						}` |
|      159 |  7673 | `					}else if( aFormalArg[n].nType > 0` |
|       85 |  7674 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  7675 | `						/* Scalar/class type checking */` |
|       17 |  7676 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  7677 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  7678 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  7679 | `							if( pClass ){` |
|      ! 0 |  7680 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7681 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7682 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7683 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7684 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7685 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  7686 | `									}` |
|      ! 0 |  7687 | `								}else{` |
|      ! 0 |  7688 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7689 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  7690 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7691 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7692 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7693 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  7694 | `									}` |
|        - |  7695 | `								}` |
|      ! 0 |  7696 | `							}` |
|       17 |  7697 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  7698 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  7699 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7700 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  7701 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7702 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7703 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  7704 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7705 | `								pFrameStack = 0;` |
|      ! 0 |  7706 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  7707 | `								goto SkipFuncBody;` |
|      ! 0 |  7708 | `							}else{` |
|        7 |  7709 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        7 |  7710 | `								if( xCast ) xCast(pVal);` |
|        - |  7711 | `							}` |
|        3 |  7712 | `						}` |
|        8 |  7713 | `					}` |
|        - |  7714 | `					/* Install: by reference or by value */` |
|      157 |  7715 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  7716 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7717 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  7718 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7719 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7720 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7721 | `							}` |
|      ! 0 |  7722 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7723 | `						}else{` |
|        7 |  7724 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  7725 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  7726 | `							if( pRefEntry == 0 ){` |
|        7 |  7727 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  7728 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  7729 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  7730 | `								sArg.pUserData = 0;` |
|        5 |  7731 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  7732 | `							}` |
|        5 |  7733 | `							pObj = 0;` |
|        - |  7734 | `						}` |
|        3 |  7735 | `					}else{` |
|      153 |  7736 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7737 | `					}` |
|      157 |  7738 | `					if( pObj ){` |
|      153 |  7739 | `						PH7_MemObjStore(pVal,pObj);` |
|      153 |  7740 | `						sArg.nIdx = pObj->nIdx;` |
|      153 |  7741 | `						sArg.pUserData = 0;` |
|      153 |  7742 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       76 |  7743 | `					}` |
|       79 |  7744 | `				}else{` |
|        - |  7745 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  7746 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7747 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  7748 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  7749 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  7750 | `						if( pObj ){` |
|       19 |  7751 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  7752 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  7753 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  7754 | `							sArg.pUserData = 0;` |
|       19 |  7755 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  7756 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  7757 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7758 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7759 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  7760 | `							}` |
|        9 |  7761 | `						}` |
|        9 |  7762 | `					}` |
|        - |  7763 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  7764 | `				}` |
|       88 |  7765 | `			}` |
|        - |  7766 | `			/* Handle variadic parameter */` |
|       83 |  7767 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  7768 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  7769 | `				if( pObj ){` |
|        9 |  7770 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7771 | `					{` |
|        9 |  7772 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  7773 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  7774 | `							if( aSlot[i] == -1 ){` |
|       16 |  7775 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  7776 | `									/* Named variadic entry: insert with string key */` |
|        - |  7777 | `									ph7_value sKey;` |
|       11 |  7778 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  7779 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  7780 | `										pCallMap3->aNames[i].zString,` |
|       10 |  7781 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  7782 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  7783 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  7784 | `								}else{` |
|        - |  7785 | `									/* Positional variadic entry */` |
|      ! 0 |  7786 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  7787 | `								}` |
|        5 |  7788 | `							}` |
|       12 |  7789 | `						}` |
|        - |  7790 | `					}` |
|        9 |  7791 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  7792 | `					sArg.pUserData = 0;` |
|        9 |  7793 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  7794 | `				}` |
|        5 |  7795 | `			}else{` |
|        - |  7796 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  7797 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  7798 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  7799 | `				 * the positional-only path's behavior. */` |
|       75 |  7800 | `				sxu32 nAnon = nNonVariadic;` |
|      219 |  7801 | `				for( i = 0; i < nActual; i++ ){` |
|      145 |  7802 | `					if( aSlot[i] == -2 ){` |
|        - |  7803 | `						char zAnonBuf[32];` |
|        - |  7804 | `						SyString sAnonName;` |
|      ! 0 |  7805 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  7806 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  7807 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  7808 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  7809 | `						if( pObj ){` |
|      ! 0 |  7810 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  7811 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  7812 | `							sArg.pUserData = 0;` |
|      ! 0 |  7813 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  7814 | `						}` |
|      ! 0 |  7815 | `						nAnon++;` |
|      ! 0 |  7816 | `					}` |
|       73 |  7817 | `				}` |
|        - |  7818 | `			}` |
|        - |  7819 | `			/* Release all stack arguments */` |
|      249 |  7820 | `			for( i = 0; i < nActual; i++ ){` |
|      167 |  7821 | `				PH7_MemObjRelease(&pArg[i]);` |
|       84 |  7822 | `			}` |
|       83 |  7823 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  7824 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       83 |  7825 | `			n = nFormal;` |
|       42 |  7826 | `		}else{` |
|        - |  7827 | `		/* ============================================================` |
|        - |  7828 | `		 * Positional-only matching path (original)` |
|        - |  7829 | `		 * ============================================================ */` |
|    16000 |  7830 | `		n = 0;` |
|    42832 |  7831 | `		while( pArg < pTos ){` |
|    26896 |  7832 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  7833 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       36 |  7834 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       36 |  7835 | `				if( pObj ){` |
|        - |  7836 | `					/* Initialize as empty array */` |
|       36 |  7837 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7838 | `					{` |
|       36 |  7839 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      136 |  7840 | `						while( pArg < pTos ){` |
|        - |  7841 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  7842 | `							 *` |
|        - |  7843 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  7844 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  7845 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  7846 | `							 * non-union variadic path below has the same limitation;` |
|        - |  7847 | `							 * fixing both wants a separate counter for elements` |
|        - |  7848 | `							 * already packed into the variadic array. */` |
|      104 |  7849 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  7850 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  7851 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       16 |  7852 | `								if( rcU != SXRET_OK ){` |
|        - |  7853 | `									const char *zGiven;` |
|        - |  7854 | `									char zBuf[128];` |
|        3 |  7855 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7856 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  7857 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7858 | `										zGiven = "null";` |
|      ! 0 |  7859 | `									}else{` |
|        3 |  7860 | `										zGiven = ph7_type_name(pArg);` |
|        - |  7861 | `									}` |
|        3 |  7862 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  7863 | `										&aFormalArg[n].sName,` |
|        2 |  7864 | `										SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|        2 |  7865 | `											? aFormalArg[n].sTypeName.zString : "union",` |
|        1 |  7866 | `										zGiven);` |
|        3 |  7867 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7868 | `										goto Abort;` |
|        - |  7869 | `									}` |
|        3 |  7870 | `									PH7_MemObjRelease(pTos);` |
|        3 |  7871 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  7872 | `									pFrameStack = 0;` |
|        3 |  7873 | `									rc = PH7_EXCEPTION;` |
|        3 |  7874 | `									goto SkipFuncBody;` |
|        - |  7875 | `								}` |
|       14 |  7876 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  7877 | `								pArg++;` |
|       14 |  7878 | `								continue;` |
|        - |  7879 | `							}` |
|        - |  7880 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  7881 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      104 |  7882 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  7883 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  7884 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  7885 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7886 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  7887 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7888 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  7889 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7890 | `										goto Abort;` |
|        - |  7891 | `									}` |
|        - |  7892 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  7893 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  7894 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7895 | `									pFrameStack = 0;` |
|      ! 0 |  7896 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  7897 | `									goto SkipFuncBody;` |
|      ! 0 |  7898 | `								}else{` |
|       13 |  7899 | `									ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  7900 | `									if( xCast ){` |
|       13 |  7901 | `										xCast(pArg);` |
|        6 |  7902 | `									}` |
|        - |  7903 | `								}` |
|        6 |  7904 | `							}` |
|       90 |  7905 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       90 |  7906 | `							pArg++;` |
|        2 |  7907 | `						}` |
|        - |  7908 | `					}` |
|       34 |  7909 | `					sArg.nIdx = pObj->nIdx;` |
|       34 |  7910 | `					sArg.pUserData = 0;` |
|       34 |  7911 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       16 |  7912 | `				}` |
|       34 |  7913 | `				break; /* All remaining args consumed */` |
|        - |  7914 | `			}` |
|    26862 |  7915 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    26702 |  7916 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       32 |  7917 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  7918 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  7919 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  7920 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7921 | `						goto Abort;` |
|        - |  7922 | `					}` |
|      ! 0 |  7923 | `				}` |
|        - |  7924 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    26704 |  7925 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       83 |  7926 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       54 |  7927 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       56 |  7928 | `					if( rcU != SXRET_OK ){` |
|        - |  7929 | `						const char *zGiven;` |
|        - |  7930 | `						char zBuf[128];` |
|       19 |  7931 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  7932 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  7933 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  7934 | `							zGiven = "null";` |
|        5 |  7935 | `						}else{` |
|        5 |  7936 | `							zGiven = ph7_type_name(pArg);` |
|        - |  7937 | `						}` |
|       19 |  7938 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  7939 | `							&aFormalArg[n].sName,` |
|       18 |  7940 | `							SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|       18 |  7941 | `								? aFormalArg[n].sTypeName.zString : "union",` |
|        9 |  7942 | `							zGiven);` |
|       19 |  7943 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  7944 | `							goto Abort;` |
|        - |  7945 | `						}` |
|       19 |  7946 | `						PH7_MemObjRelease(pTos);` |
|       19 |  7947 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  7948 | `						pFrameStack = 0;` |
|       19 |  7949 | `						rc = PH7_EXCEPTION;` |
|       19 |  7950 | `						goto SkipFuncBody;` |
|        - |  7951 | `					}` |
|       19 |  7952 | `				}else` |
|        - |  7953 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  7954 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    26672 |  7955 | `				if( aFormalArg[n].nType > 0` |
|    13968 |  7956 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1262 |  7957 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  7958 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  7959 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  7960 | `						ph7_class *pClass;` |
|        - |  7961 | `						/* Try to extract the desired class */` |
|       26 |  7962 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  7963 | `						if( pClass ){` |
|       22 |  7964 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7965 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7966 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7967 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7968 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7969 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7970 | `								}` |
|      ! 0 |  7971 | `							}else{` |
|        - |  7972 | `								/* reuse pThis declared in outer scope */` |
|       22 |  7973 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  7974 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  7975 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  7976 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7977 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7978 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7979 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7980 | `								}` |
|        - |  7981 | `							}` |
|       12 |  7982 | `						}` |
|     1250 |  7983 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       11 |  7984 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7985 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  7986 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  7987 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  7988 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7989 | `								goto Abort;` |
|        - |  7990 | `							}` |
|        - |  7991 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  7992 | `							PH7_MemObjRelease(pTos);` |
|       11 |  7993 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  7994 | `							pFrameStack = 0;` |
|       11 |  7995 | `							rc = PH7_EXCEPTION;` |
|       11 |  7996 | `							goto SkipFuncBody;` |
|      ! 0 |  7997 | `						}else{` |
|      ! 0 |  7998 | `							ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7999 | `							/* Cast to the desired type */` |
|      ! 0 |  8000 | `							xCast(pArg);` |
|        - |  8001 | `						}` |
|      ! 0 |  8002 | `					}` |
|      625 |  8003 | `				}` |
|    26676 |  8004 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  8005 | `					/* Pass by reference */` |
|       54 |  8006 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  8007 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  8008 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  8009 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8010 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8011 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8012 | `						}` |
|        - |  8013 | `						/* Switch to pass by value */` |
|      ! 0 |  8014 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8015 | `					}else{` |
|        - |  8016 | `						SyHashEntry *pRefEntry;` |
|        - |  8017 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  8018 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  8019 | `						if( pRefEntry == 0 ){` |
|       80 |  8020 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  8021 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  8022 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  8023 | `							sArg.pUserData = 0;` |
|       54 |  8024 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  8025 | `						}` |
|       54 |  8026 | `						pObj = 0;` |
|        - |  8027 | `					}` |
|       28 |  8028 | `				}else{` |
|        - |  8029 | `					/* Pass by value,make a copy of the given argument */` |
|    26624 |  8030 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8031 | `				}` |
|    13339 |  8032 | `			}else{` |
|        - |  8033 | `				char zName[32];` |
|        - |  8034 | `				SyString sArgName;` |
|        - |  8035 | `				/* Set a dummy name */` |
|      160 |  8036 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      160 |  8037 | `				sArgName.zString = zName;` |
|        - |  8038 | `				/* Annonymous argument */` |
|      160 |  8039 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  8040 | `			}` |
|    26834 |  8041 | `			if( pObj ){` |
|    26782 |  8042 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  8043 | `				/* Insert argument index  */` |
|    26782 |  8044 | `				sArg.nIdx = pObj->nIdx;` |
|    26782 |  8045 | `				sArg.pUserData = 0;` |
|    26782 |  8046 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    13390 |  8047 | `			}` |
|    26834 |  8048 | `			PH7_MemObjRelease(pArg);` |
|    26834 |  8049 | `			pArg++;` |
|    26834 |  8050 | `			++n;` |
|        2 |  8051 | `		}` |
|        - |  8052 | `		} /* end named vs positional branch */` |
|        - |  8053 | `		/* Set up closure environment */` |
|    16052 |  8054 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8055 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  8056 | `			ph7_value *pValue;` |
|        - |  8057 | `			sxu32 iEnv;` |
|      115 |  8058 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      295 |  8059 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      181 |  8060 | `				pEnv = &aEnv[iEnv];` |
|      181 |  8061 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  8062 | `					/* Do not install null value */` |
|      109 |  8063 | `					continue;` |
|        - |  8064 | `				}` |
|       73 |  8065 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       73 |  8066 | `				if( pValue == 0 ){` |
|      ! 0 |  8067 | `					continue;` |
|        - |  8068 | `				}` |
|        - |  8069 | `				/* Invalidate any prior representation */` |
|       73 |  8070 | `				PH7_MemObjRelease(pValue);` |
|        - |  8071 | `				/* Duplicate bound variable value */` |
|       73 |  8072 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       37 |  8073 | `			}` |
|       57 |  8074 | `		}` |
|        - |  8075 | `		/* Process default values for remaining formal parameters */` |
|    18480 |  8076 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2470 |  8077 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8078 | `				/* Variadic parameter with no extra args — create empty array */` |
|       42 |  8079 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       42 |  8080 | `				if( pObj ){` |
|       42 |  8081 | `					PH7_MemObjToHashmap(pObj);` |
|       42 |  8082 | `					sArg.nIdx = pObj->nIdx;` |
|       42 |  8083 | `					sArg.pUserData = 0;` |
|       42 |  8084 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       20 |  8085 | `				}` |
|       42 |  8086 | `				n++;` |
|       42 |  8087 | `				break; /* Variadic is always last */` |
|        - |  8088 | `			}` |
|     2430 |  8089 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2424 |  8090 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2424 |  8091 | `				if( pObj ){` |
|        - |  8092 | `					/* Evaluate the default value and extract it's result */` |
|     2424 |  8093 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2424 |  8094 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8095 | `						goto Abort;` |
|        - |  8096 | `					}` |
|        - |  8097 | `					/* Insert argument index */` |
|     2424 |  8098 | `					sArg.nIdx = pObj->nIdx;` |
|     2424 |  8099 | `					sArg.pUserData = 0;` |
|     2424 |  8100 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  8101 | `					/* Make sure the default argument is of the correct type */` |
|     2422 |  8102 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1626 |  8103 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  8104 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  8105 | `						/* Cast to the desired type */` |
|        3 |  8106 | `						xCast(pObj);` |
|        1 |  8107 | `					}` |
|     1211 |  8108 | `				}` |
|     1211 |  8109 | `			}` |
|     2430 |  8110 | `			++n;` |
|        2 |  8111 | `		}` |
|        - |  8112 | `		} /* end VmCallArgMap scope */` |
|        - |  8113 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  8114 | `		 * does not return anything.` |
|        - |  8115 | `		 */` |
|    16052 |  8116 | `		PH7_MemObjRelease(pTos);` |
|    16052 |  8117 | `		pTos = &pTos[-nCallArgs];` |
|        - |  8118 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    16052 |  8119 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    16052 |  8120 | `		if( pFrameStack == 0 ){` |
|        - |  8121 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8122 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8123 | `				&pVmFunc->sName);` |
|      ! 0 |  8124 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8125 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8126 | `			}` |
|      ! 0 |  8127 | `			break;` |
|        - |  8128 | `		}` |
|     8025 |  8129 | `SkipFuncBody:` |
|    16082 |  8130 | `		if( pSelf ){` |
|        - |  8131 | `			/* Push class name */` |
|     2568 |  8132 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1283 |  8133 | `		}` |
|        - |  8134 | `		/* Increment nesting level */` |
|    16082 |  8135 | `		pVm->nRecursionDepth++;` |
|    16082 |  8136 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  8137 | `			/* Execute function body */` |
|    16052 |  8138 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|     8025 |  8139 | `		}` |
|        - |  8140 | `		/* Decrement nesting level */` |
|    16082 |  8141 | `		pVm->nRecursionDepth--;` |
|    16082 |  8142 | `		if( pSelf ){` |
|        - |  8143 | `			/* Pop class name */` |
|     2568 |  8144 | `			(void)SySetPop(&pVm->aSelf);` |
|     1283 |  8145 | `		}` |
|        - |  8146 | `		/* Cleanup the mess left behind */` |
|    16082 |  8147 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  8148 | `			/* Return by reference,reflect that */` |
|        9 |  8149 | `			if( n != SXU32_HIGH ){` |
|        9 |  8150 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  8151 | `				sxu32 i;` |
|        - |  8152 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  8153 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  8154 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  8155 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  8156 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8157 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8158 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  8159 | `								&pVmFunc->sName);` |
|      ! 0 |  8160 | `						}` |
|      ! 0 |  8161 | `						n = SXU32_HIGH;` |
|      ! 0 |  8162 | `						break;` |
|        - |  8163 | `					}` |
|        3 |  8164 | `				}` |
|        5 |  8165 | `			}else{` |
|      ! 0 |  8166 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8167 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8168 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  8169 | `						&pVmFunc->sName);` |
|      ! 0 |  8170 | `				}` |
|        - |  8171 | `			}` |
|        9 |  8172 | `			pTos->nIdx = n;` |
|        4 |  8173 | `		}` |
|        - |  8174 | `		/* Cleanup the mess left behind */` |
|    16082 |  8175 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  8176 | `			/* An exception was throw in this frame */` |
|       48 |  8177 | `			pFrame = pFrame->pParent;` |
|       48 |  8178 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  8179 | `				/* Pop the resutlt */` |
|       46 |  8180 | `				VmPopOperand(&pTos,1);` |
|        - |  8181 | `				/* Jump to this destination */` |
|       46 |  8182 | `				pc = pFrame->iExceptionJump - 1;` |
|       46 |  8183 | `				rc = PH7_OK;` |
|       24 |  8184 | `			}else{` |
|        3 |  8185 | `				if( pFrame->pParent ){` |
|        3 |  8186 | `					rc = PH7_EXCEPTION;` |
|        2 |  8187 | `				}else{` |
|        - |  8188 | `					/* Continue normal execution */` |
|      ! 0 |  8189 | `					rc = PH7_OK;` |
|        - |  8190 | `				}` |
|        - |  8191 | `			}` |
|       23 |  8192 | `		}` |
|        - |  8193 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    16082 |  8194 | `		if( pFrameStack ){` |
|    16052 |  8195 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     8025 |  8196 | `		}` |
|        - |  8197 | `		/* Leave the frame */` |
|    16082 |  8198 | `		VmLeaveFrame(&(*pVm));` |
|    16082 |  8199 | `		if( rc == PH7_ABORT ){` |
|        - |  8200 | `			/* Abort processing immeditaley */` |
|        9 |  8201 | `			goto Abort;` |
|    16074 |  8202 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8203 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8204 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8205 | `			 * overwriting the state saved by the inner level.` |
|        - |  8206 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8207 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8208 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8209 | `			goto Suspend;` |
|    16036 |  8210 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8211 | `			goto Exception;` |
|        - |  8212 | `		}` |
|     8018 |  8213 | `	}else{` |
|        - |  8214 | `		ph7_user_func *pFunc;` |
|        - |  8215 | `		ph7_context sCtx;` |
|        - |  8216 | `		ph7_value sRet;` |
|        - |  8217 | `		/* Look for an installed foreign function.` |
|        - |  8218 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8219 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8220 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8221 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   645694 |  8222 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8223 | `		{` |
|   645694 |  8224 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   645694 |  8225 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8226 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  8227 | `			const char *zShort = sName.zString;` |
|        - |  8228 | `			sxu32 i;` |
|      334 |  8229 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  8230 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  8231 | `					zShort = &sName.zString[i + 1];` |
|       13 |  8232 | `				}` |
|      158 |  8233 | `			}` |
|       22 |  8234 | `			if( zShort != sName.zString ){` |
|       22 |  8235 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  8236 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  8237 | `			}` |
|       10 |  8238 | `		}` |
|        - |  8239 | `		} /* end VmCallArgMap namespace scope */` |
|   645694 |  8240 | `		if( pEntry == 0 ){` |
|        - |  8241 | `			/* Call to undefined function */` |
|        5 |  8242 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8243 | `			/* Pop given arguments */` |
|        5 |  8244 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8245 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8246 | `			}` |
|        - |  8247 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8248 | `			PH7_MemObjRelease(pTos);` |
|        9 |  8249 | `			break;` |
|        - |  8250 | `		}` |
|   645690 |  8251 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  8252 | `		/* Start collecting function arguments */` |
|   645690 |  8253 | `		SySetReset(&aArg);` |
|  1737898 |  8254 | `		while( pArg < pTos ){` |
|  1092210 |  8255 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1092210 |  8256 | `			pArg++;` |
|        2 |  8257 | `		}` |
|        - |  8258 | `		/* Assume a null return value */` |
|   645690 |  8259 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  8260 | `		/* Init the call context */` |
|   645690 |  8261 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  8262 | `		/* Call the foreign function */` |
|   645690 |  8263 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  8264 | `		/* Release the call context */` |
|   645690 |  8265 | `		VmReleaseCallContext(&sCtx);` |
|   645690 |  8266 | `		if( rc == PH7_ABORT ){` |
|      471 |  8267 | `			goto Abort;` |
|   645220 |  8268 | `		}else if( rc == PH7_EXCEPTION ){` |
|       14 |  8269 | `			VmFrame *pFrm = pVm->pFrame;` |
|       14 |  8270 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       14 |  8271 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  8272 | `				/* Exception was NOT caught, propagate */` |
|        5 |  8273 | `				goto Exception;` |
|        - |  8274 | `			}` |
|        - |  8275 | `			/* Exception was caught: pop args and the result slot */` |
|        9 |  8276 | `			PH7_MemObjRelease(&sRet);` |
|        9 |  8277 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  8278 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8279 | `			}` |
|        - |  8280 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        9 |  8281 | `			VmPopOperand(&pTos,1);` |
|        - |  8282 | `			/* Jump past the try/catch block via the exception frame */` |
|        9 |  8283 | `			pFrm = pVm->pFrame;` |
|        9 |  8284 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        9 |  8285 | `				pc = pFrm->iExceptionJump - 1;` |
|        4 |  8286 | `			}` |
|        9 |  8287 | `			break;` |
|        - |  8288 | `		}` |
|   645208 |  8289 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8290 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  8291 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  8292 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  8293 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  8294 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  8295 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  8296 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  8297 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  8298 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  8299 | `			}` |
|        - |  8300 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  8301 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  8302 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  8303 | `			goto Suspend;` |
|        - |  8304 | `		}` |
|   645170 |  8305 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8306 | `			/* Pop function name and arguments */` |
|   624734 |  8307 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   312388 |  8308 | `		}` |
|        - |  8309 | `		/* Save foreign function return value */` |
|   645170 |  8310 | `		PH7_MemObjStore(&sRet,pTos);` |
|   645170 |  8311 | `		PH7_MemObjRelease(&sRet);` |
|        - |  8312 | `	}` |
|   661202 |  8313 | `	break;` |
|        - |  8314 | `				  }` |
|        - |  8315 | `/*` |
|        - |  8316 | ` * OP_CONSUME: P1 * *` |
|        - |  8317 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  8318 | ` */` |
|    13758 |  8319 | `case PH7_OP_CONSUME: {` |
|    27518 |  8320 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    27518 |  8321 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  8322 |  |
|    27518 |  8323 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    27518 |  8324 | `	pCur = pOut;` |
|        - |  8325 | `	/* Start the consume process  */` |
|    55034 |  8326 | `	while( pOut <= pTos ){` |
|        - |  8327 | `		/* Force a string cast */` |
|    27518 |  8328 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      542 |  8329 | `			PH7_MemObjToString(pOut);` |
|      270 |  8330 | `		}` |
|    27518 |  8331 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  8332 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  8333 | `			/* Invoke the output consumer callback */` |
|    15980 |  8334 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    15980 |  8335 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    15980 |  8336 | `			SyBlobRelease(&pOut->sBlob);` |
|    15980 |  8337 | `			if( rc == SXERR_ABORT ){` |
|        - |  8338 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  8339 | `				goto Abort;` |
|        - |  8340 | `			}` |
|     7989 |  8341 | `		}` |
|    27518 |  8342 | `		pOut++;` |
|        2 |  8343 | `	}` |
|    27518 |  8344 | `	pTos = &pCur[-1];` |
|    27516 |  8345 | `	break;` |
|        - |  8346 | `					 }` |
|        - |  8347 |  |
|        - |  8348 | `		} /* Switch() */` |
| 11032844 |  8349 | `		pc++; /* Next instruction in the stream */` |
|        2 |  8350 | `	} /* For(;;) */` |
|    19472 |  8351 | `Done:` |
|    38946 |  8352 | `	SySetRelease(&aArg);` |
|    38946 |  8353 | `	return SXRET_OK;` |
|       72 |  8354 | `Suspend:` |
|      146 |  8355 | `	SySetRelease(&aArg);` |
|      146 |  8356 | `	return PH7_SUSPEND;` |
|      251 |  8357 | `Abort:` |
|      503 |  8358 | `	SySetRelease(&aArg);` |
|     1731 |  8359 | `	while( pTos >= pStack ){` |
|     1229 |  8360 | `		PH7_MemObjRelease(pTos);` |
|     1229 |  8361 | `		pTos--;` |
|        1 |  8362 | `	}` |
|      503 |  8363 | `	return PH7_ABORT;` |
|        3 |  8364 | `Exception:` |
|        8 |  8365 | `	SySetRelease(&aArg);` |
|       22 |  8366 | `	while( pTos >= pStack ){` |
|       16 |  8367 | `		PH7_MemObjRelease(pTos);` |
|       16 |  8368 | `		pTos--;` |
|        2 |  8369 | `	}` |
|        8 |  8370 | `	return PH7_EXCEPTION;` |
|    19800 |  8371 |  |
|        - |  8372 | `/*` |
|        - |  8373 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8374 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8375 | ` * See block-comment on that function for additional information.` |
|        - |  8376 | ` */` |
|    18498 |  8377 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  8378 |  |
|        - |  8379 | `	ph7_value *pStack;` |
|        - |  8380 | `	sxi32 rc;` |
|        - |  8381 | `	/* Allocate a new operand stack */` |
|    18500 |  8382 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    18500 |  8383 | `	if( pStack == 0 ){` |
|      ! 0 |  8384 | `		return SXERR_MEM;` |
|        - |  8385 | `	}` |
|        - |  8386 | `	/* Execute the program */` |
|    18500 |  8387 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  8388 | `	/* Free the operand stack */` |
|    18500 |  8389 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  8390 | `	/* Execution result */` |
|    18500 |  8391 | `	return rc;` |
|     9251 |  8392 |  |
|        - |  8393 | `/*` |
|        - |  8394 | ` * Invoke any installed shutdown callbacks.` |
|        - |  8395 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  8396 | ` * or more calls to [register_shutdown_function()].` |
|        - |  8397 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  8398 | ` * execution ends.` |
|        - |  8399 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  8400 | ` * additional information.` |
|        - |  8401 | ` */` |
|     2604 |  8402 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  8403 |  |
|        - |  8404 | `	VmShutdownCB *pEntry;` |
|        - |  8405 | `	ph7_value *apArg[10];` |
|        - |  8406 | `	sxu32 n,nEntry;` |
|        - |  8407 | `	int i;` |
|        - |  8408 | `	/* Point to the stack of registered callbacks */` |
|     2606 |  8409 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    28646 |  8410 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    26042 |  8411 | `		apArg[i] = 0;` |
|    13022 |  8412 | `	}` |
|     2608 |  8413 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  8414 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8415 | `		if( pEntry ){` |
|        - |  8416 | `			/* Prepare callback arguments if any */` |
|        3 |  8417 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  8418 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  8419 | `					break;` |
|        - |  8420 | `				}` |
|      ! 0 |  8421 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  8422 | `			}` |
|        - |  8423 | `			/* Invoke the callback */` |
|        3 |  8424 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  8425 | `			/*` |
|        - |  8426 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  8427 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  8428 | `			 */` |
|        3 |  8429 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8430 | `			if( pEntry ){` |
|        3 |  8431 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  8432 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  8433 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  8434 | `				}` |
|        1 |  8435 | `			}` |
|        1 |  8436 | `		}` |
|        2 |  8437 | `	}` |
|     2606 |  8438 | `	SySetReset(&pVm->aShutdown);` |
|     2606 |  8439 |  |
|        - |  8440 | `/*` |
|        - |  8441 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  8442 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8443 | ` * See block-comment on that function for additional information.` |
|        - |  8444 | ` */` |
|     2612 |  8445 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  8446 |  |
|        - |  8447 | `	/* Make sure we are ready to execute this program */` |
|     2614 |  8448 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  8449 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  8450 | `	}` |
|        - |  8451 | `	/* Set the execution magic number  */` |
|     2614 |  8452 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  8453 | `	/* Execute the program */` |
|     2614 |  8454 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  8455 | `	/* Invoke any shutdown callbacks */` |
|     2610 |  8456 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  8457 | `	/*` |
|        - |  8458 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  8459 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  8460 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  8461 | `	 */` |
|     2610 |  8462 | `	return SXRET_OK;` |
|     1308 |  8463 |  |
|        - |  8464 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  8465 | `/*` |
|        - |  8466 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  8467 | ` * The context is in CREATED state and ready to be started.` |
|        - |  8468 | ` */` |
|       46 |  8469 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  8470 |  |
|        - |  8471 | `	ph7_exec_ctx *pCtx;` |
|        - |  8472 | `	ph7_value *pStack;` |
|        - |  8473 | `	VmFrame *pFrame;` |
|       48 |  8474 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  8475 | `	if( pCtx == 0 ){` |
|      ! 0 |  8476 | `		return 0;` |
|        - |  8477 | `	}` |
|       48 |  8478 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  8479 | `	pCtx->pVm = pVm;` |
|       48 |  8480 | `	pCtx->pFunc = pFunc;` |
|       48 |  8481 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  8482 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  8483 | `	pCtx->pc = 0;` |
|       48 |  8484 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  8485 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  8486 | `	/* Allocate a private operand stack */` |
|       48 |  8487 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  8488 | `	if( pStack == 0 ){` |
|      ! 0 |  8489 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8490 | `		return 0;` |
|        - |  8491 | `	}` |
|       48 |  8492 | `	pCtx->pStack = pStack;` |
|        - |  8493 | `	/* Create a detached frame for the fiber */` |
|       48 |  8494 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  8495 | `	if( pFrame == 0 ){` |
|      ! 0 |  8496 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  8497 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8498 | `		return 0;` |
|        - |  8499 | `	}` |
|       48 |  8500 | `	pCtx->pFrame = pFrame;` |
|       48 |  8501 | `	return pCtx;` |
|       25 |  8502 |  |
|        - |  8503 | `/*` |
|        - |  8504 | ` * Start executing a fiber context for the first time.` |
|        - |  8505 | ` */` |
|       46 |  8506 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  8507 |  |
|        - |  8508 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8509 | `	sxi32 rc;` |
|       48 |  8510 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8511 | `		return SXERR_INVALID;` |
|        - |  8512 | `	}` |
|        - |  8513 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  8514 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  8515 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8516 | `	/* Save and set the active context */` |
|       48 |  8517 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  8518 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  8519 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  8520 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  8521 | `	pVm->nRecursionDepth++;` |
|        - |  8522 | `	/* Execute from the beginning */` |
|       71 |  8523 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  8524 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       48 |  8525 | `	pVm->nRecursionDepth--;` |
|        - |  8526 | `	/* Restore the previous context */` |
|       48 |  8527 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  8528 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8529 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  8530 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  8531 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  8532 | `		if( pResult ){` |
|       24 |  8533 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  8534 | `		}` |
|       46 |  8535 | `		return SXRET_OK;` |
|        - |  8536 | `	}` |
|        - |  8537 | `	/* Detach frame */` |
|        3 |  8538 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  8539 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  8540 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  8541 | `	}` |
|        3 |  8542 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8543 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8544 | `		return PH7_ABORT;` |
|        - |  8545 | `	}` |
|        3 |  8546 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8547 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8548 | `		return PH7_EXCEPTION;` |
|        - |  8549 | `	}` |
|        - |  8550 | `	/* Normal completion */` |
|        3 |  8551 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  8552 | `	if( pResult ){` |
|        3 |  8553 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  8554 | `	}` |
|        3 |  8555 | `	return SXRET_OK;` |
|       25 |  8556 |  |
|        - |  8557 | `/*` |
|        - |  8558 | ` * Resume a suspended fiber context.` |
|        - |  8559 | ` */` |
|       98 |  8560 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  8561 |  |
|        - |  8562 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8563 | `	sxi32 rc;` |
|      100 |  8564 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  8565 | `		return SXERR_INVALID;` |
|        - |  8566 | `	}` |
|        - |  8567 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  8568 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  8569 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  8570 | `	if( pResumeValue ){` |
|       40 |  8571 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  8572 | `	}else{` |
|       62 |  8573 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  8574 | `	}` |
|      100 |  8575 | `	pCtx->nTos++;` |
|        - |  8576 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  8577 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  8578 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8579 | `	/* Save and set the active context */` |
|      100 |  8580 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  8581 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  8582 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  8583 | `	pVm->nRecursionDepth++;` |
|        - |  8584 | `	/* Resume execution from saved PC */` |
|      149 |  8585 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  8586 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|      100 |  8587 | `	pVm->nRecursionDepth--;` |
|        - |  8588 | `	/* Restore the previous context */` |
|      100 |  8589 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  8590 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8591 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  8592 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  8593 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  8594 | `		if( pResult ){` |
|       18 |  8595 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  8596 | `		}` |
|       64 |  8597 | `		return SXRET_OK;` |
|        - |  8598 | `	}` |
|        - |  8599 | `	/* Detach frame */` |
|       38 |  8600 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  8601 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  8602 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  8603 | `	}` |
|       38 |  8604 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8605 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8606 | `		return PH7_ABORT;` |
|        - |  8607 | `	}` |
|       38 |  8608 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8609 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8610 | `		return PH7_EXCEPTION;` |
|        - |  8611 | `	}` |
|        - |  8612 | `	/* Normal completion */` |
|       38 |  8613 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  8614 | `	if( pResult ){` |
|       20 |  8615 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  8616 | `	}` |
|       38 |  8617 | `	return SXRET_OK;` |
|       51 |  8618 |  |
|        - |  8619 | `/*` |
|        - |  8620 | ` * Release an execution context and all its resources.` |
|        - |  8621 | ` */` |
|        4 |  8622 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  8623 |  |
|        5 |  8624 | `	if( pCtx == 0 ){` |
|      ! 0 |  8625 | `		return;` |
|        - |  8626 | `	}` |
|        5 |  8627 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  8628 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  8629 | `		return;` |
|        - |  8630 | `	}` |
|        5 |  8631 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  8632 | `	/* Release values */` |
|        5 |  8633 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  8634 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  8635 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  8636 | `	if( pCtx->pFrame ){` |
|        - |  8637 | `		VmSlot *aSlot;` |
|        - |  8638 | `		sxu32 n;` |
|        - |  8639 | `		/* Free local variables */` |
|        5 |  8640 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  8641 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  8642 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  8643 | `		}` |
|        - |  8644 | `		/* Remove local references */` |
|        5 |  8645 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  8646 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  8647 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  8648 | `		}` |
|        5 |  8649 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  8650 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  8651 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  8652 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  8653 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  8654 | `		pCtx->pFrame = 0;` |
|        2 |  8655 | `	}` |
|        - |  8656 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  8657 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  8658 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  8659 | `	if( pCtx->pStack ){` |
|        5 |  8660 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  8661 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  8662 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  8663 | `				PH7_MemObjRelease(pTos);` |
|        5 |  8664 | `				pTos--;` |
|        1 |  8665 | `			}` |
|        2 |  8666 | `		}` |
|        5 |  8667 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  8668 | `		pCtx->pStack = 0;` |
|        2 |  8669 | `	}` |
|        - |  8670 | `	/* Free the context itself */` |
|        5 |  8671 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  8672 |  |
|        - |  8673 | `/*` |
|        - |  8674 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  8675 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  8676 | ` */` |
|       90 |  8677 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  8678 |  |
|        - |  8679 | `	ph7_class_instance *pThis;` |
|        - |  8680 | `	SyString sAttr;` |
|        - |  8681 | `	ph7_value *pAttr;` |
|       92 |  8682 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8683 | `		return 0;` |
|        - |  8684 | `	}` |
|       92 |  8685 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  8686 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  8687 | `		return 0;` |
|        - |  8688 | `	}` |
|       92 |  8689 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  8690 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  8691 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  8692 | `		return 0;` |
|        - |  8693 | `	}` |
|       62 |  8694 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  8695 |  |
|        - |  8696 | `/*` |
|        - |  8697 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  8698 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  8699 | ` */` |
|       38 |  8700 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8701 |  |
|       40 |  8702 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  8703 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  8704 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8705 | `			"Cannot suspend outside of a fiber");` |
|        - |  8706 | `	}` |
|       40 |  8707 | `	if( nArg > 0 ){` |
|       40 |  8708 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  8709 | `	}else{` |
|      ! 0 |  8710 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  8711 | `	}` |
|       40 |  8712 | `	return PH7_SUSPEND;` |
|       21 |  8713 |  |
|        - |  8714 | `/*` |
|        - |  8715 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  8716 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  8717 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  8718 | ` */` |
|       24 |  8719 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8720 |  |
|        - |  8721 | `	ph7_class_instance *pThis;` |
|        - |  8722 | `	ph7_value *pAttr;` |
|        - |  8723 | `	SyString sAttrName;` |
|       26 |  8724 | `	if( nArg < 2 ){` |
|      ! 0 |  8725 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8726 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  8727 | `	}` |
|       26 |  8728 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8729 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8730 | `			"Fiber::__construct(): invalid $this");` |
|        - |  8731 | `	}` |
|       26 |  8732 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  8733 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  8734 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8735 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  8736 | `	}` |
|        - |  8737 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  8738 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8739 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8740 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  8741 | `	}` |
|        - |  8742 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  8743 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8744 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8745 | `	if( pAttr ){` |
|       26 |  8746 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  8747 | `	}` |
|       26 |  8748 | `	return PH7_OK;` |
|       14 |  8749 |  |
|        - |  8750 | `/*` |
|        - |  8751 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  8752 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  8753 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  8754 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  8755 | ` */` |
|       24 |  8756 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  8757 | `	ph7_class_instance **ppThis)` |
|        2 |  8758 |  |
|       26 |  8759 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8760 | `	ph7_value *pCallable;` |
|        - |  8761 | `	SyString sAttrName;` |
|       26 |  8762 | `	*ppThis = 0;` |
|       26 |  8763 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8764 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  8765 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8766 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  8767 | `		return 0;` |
|        - |  8768 | `	}` |
|       26 |  8769 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  8770 | `		/* String callable — look up in user functions with overload support */` |
|        - |  8771 | `		SyString sName;` |
|        - |  8772 | `		SyHashEntry *pEntry;` |
|        - |  8773 | `		ph7_vm_func *pFunc;` |
|       26 |  8774 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  8775 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  8776 | `		if( pEntry == 0 ){` |
|      ! 0 |  8777 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  8778 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  8779 | `			return 0;` |
|        - |  8780 | `		}` |
|       26 |  8781 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  8782 | `		return pFunc;` |
|      ! 0 |  8783 | `	}else{` |
|        - |  8784 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  8785 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  8786 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  8787 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  8788 | `		if( pMethod == 0 ){` |
|      ! 0 |  8789 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8790 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  8791 | `			return 0;` |
|        - |  8792 | `		}` |
|      ! 0 |  8793 | `		*ppThis = pClosure;` |
|      ! 0 |  8794 | `		return &pMethod->sFunc;` |
|        - |  8795 | `	}` |
|       14 |  8796 |  |
|        - |  8797 | `/*` |
|        - |  8798 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  8799 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  8800 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  8801 | ` */` |
|       46 |  8802 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  8803 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  8804 |  |
|       48 |  8805 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  8806 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  8807 | `	sxu32 nFormal, n;` |
|        - |  8808 | `	VmSlot sSlot;` |
|        - |  8809 | `	sxi32 rc;` |
|        - |  8810 | `	/* Install $this for closure/method callables */` |
|       48 |  8811 | `	if( pClosureThis ){` |
|        - |  8812 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  8813 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  8814 | `		if( pObj ){` |
|      ! 0 |  8815 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  8816 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  8817 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  8818 | `		}` |
|      ! 0 |  8819 | `	}` |
|        - |  8820 | `	/* Install static variables */` |
|       48 |  8821 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  8822 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  8823 | `		ph7_value *pVal;` |
|      ! 0 |  8824 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  8825 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  8826 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  8827 | `			if( pVal ){` |
|      ! 0 |  8828 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8829 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  8830 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  8831 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  8832 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  8833 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  8834 | `				}` |
|      ! 0 |  8835 | `			}` |
|      ! 0 |  8836 | `		}` |
|      ! 0 |  8837 | `	}` |
|        - |  8838 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  8839 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  8840 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  8841 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  8842 | `		ph7_value *pObj;` |
|       20 |  8843 | `		if( n < (sxu32)nArg ){` |
|        - |  8844 | `			/* Argument provided — install with type casting */` |
|       20 |  8845 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  8846 | `			if( pObj ){` |
|       20 |  8847 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  8848 | `				/* Type casting */` |
|       20 |  8849 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8850 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8851 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8852 | `						if( xCast ){` |
|      ! 0 |  8853 | `							xCast(pObj);` |
|      ! 0 |  8854 | `						}` |
|      ! 0 |  8855 | `					}` |
|      ! 0 |  8856 | `				}` |
|       20 |  8857 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  8858 | `				sSlot.pUserData = 0;` |
|       20 |  8859 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  8860 | `			}` |
|        9 |  8861 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  8862 | `			/* Default value */` |
|      ! 0 |  8863 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  8864 | `			if( pObj ){` |
|      ! 0 |  8865 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  8866 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8867 | `					return rc;` |
|        - |  8868 | `				}` |
|      ! 0 |  8869 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8870 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8871 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8872 | `						if( xCast ){` |
|      ! 0 |  8873 | `							xCast(pObj);` |
|      ! 0 |  8874 | `						}` |
|      ! 0 |  8875 | `					}` |
|      ! 0 |  8876 | `				}` |
|      ! 0 |  8877 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  8878 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8879 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  8880 | `			}` |
|      ! 0 |  8881 | `		}` |
|       11 |  8882 | `	}` |
|        - |  8883 | `	/* Install closure environment (captured variables) */` |
|       48 |  8884 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8885 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  8886 | `		ph7_value *pValue;` |
|        - |  8887 | `		sxu32 iEnv;` |
|        3 |  8888 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  8889 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  8890 | `			pEnv = &aEnv[iEnv];` |
|        7 |  8891 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  8892 | `				continue;` |
|        - |  8893 | `			}` |
|        5 |  8894 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  8895 | `			if( pValue == 0 ){` |
|      ! 0 |  8896 | `				continue;` |
|        - |  8897 | `			}` |
|        5 |  8898 | `			PH7_MemObjRelease(pValue);` |
|        5 |  8899 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  8900 | `		}` |
|        1 |  8901 | `	}` |
|       48 |  8902 | `	return SXRET_OK;` |
|       25 |  8903 |  |
|        - |  8904 | `/*` |
|        - |  8905 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  8906 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  8907 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  8908 | ` */` |
|       26 |  8909 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8910 |  |
|       28 |  8911 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8912 | `	ph7_class_instance *pThis;` |
|        - |  8913 | `	ph7_class_instance *pClosureThis;` |
|        - |  8914 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8915 | `	ph7_vm_func *pFunc;` |
|        - |  8916 | `	ph7_value sResult;` |
|        - |  8917 | `	ph7_value *pCtxAttr;` |
|        - |  8918 | `	SyString sAttrName;` |
|        - |  8919 | `	sxi32 rc;` |
|       28 |  8920 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8921 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  8922 | `	}` |
|       28 |  8923 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8924 | `	/* Check if already started (has a __ctx) */` |
|       28 |  8925 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  8926 | `	if( pExecCtx != 0 ){` |
|        3 |  8927 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8928 | `			"Cannot start a fiber that has already been started");` |
|        - |  8929 | `	}` |
|        - |  8930 | `	/* Resolve callable */` |
|       26 |  8931 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  8932 | `	if( pFunc == 0 ){` |
|      ! 0 |  8933 | `		return PH7_EXCEPTION;` |
|        - |  8934 | `	}` |
|        - |  8935 | `	/* Create execution context now that we know the function */` |
|       26 |  8936 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  8937 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8938 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8939 | `			"Fiber::start(): out of memory");` |
|        - |  8940 | `	}` |
|        - |  8941 | `	/* Store context in $this->__ctx */` |
|       26 |  8942 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  8943 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8944 | `	if( pCtxAttr ){` |
|       26 |  8945 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  8946 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  8947 | `	}` |
|        - |  8948 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  8949 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  8950 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  8951 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  8952 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  8953 | `	/* Unpack the args array and install into the frame */` |
|        - |  8954 | `	{` |
|       26 |  8955 | `		ph7_value **apValues = 0;` |
|       26 |  8956 | `		int nActual = 0;` |
|       26 |  8957 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  8958 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  8959 | `			ph7_hashmap_node *pNode;` |
|       26 |  8960 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  8961 | `			if( nCount > 0 ){` |
|        3 |  8962 | `				sxu32 idx = 0;` |
|        4 |  8963 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  8964 | `					nCount * sizeof(ph7_value *));` |
|        3 |  8965 | `				if( apValues ){` |
|        3 |  8966 | `					pNode = pMap->pFirst;` |
|        7 |  8967 | `					while( pNode && idx < nCount ){` |
|        5 |  8968 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  8969 | `						idx++;` |
|        5 |  8970 | `						pNode = pNode->pPrev;` |
|        1 |  8971 | `					}` |
|        3 |  8972 | `					nActual = (int)idx;` |
|        1 |  8973 | `				}` |
|        1 |  8974 | `			}` |
|       12 |  8975 | `		}` |
|       26 |  8976 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  8977 | `		if( apValues ){` |
|        3 |  8978 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  8979 | `		}` |
|        - |  8980 | `	}` |
|        - |  8981 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  8982 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  8983 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  8984 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8985 | `		return PH7_ABORT;` |
|        - |  8986 | `	}` |
|       26 |  8987 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  8988 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  8989 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8990 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8991 | `		return PH7_ABORT;` |
|        - |  8992 | `	}` |
|       26 |  8993 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8994 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8995 | `		return PH7_EXCEPTION;` |
|        - |  8996 | `	}` |
|       26 |  8997 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  8998 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  8999 | `	return PH7_OK;` |
|       15 |  9000 |  |
|        - |  9001 | `/*` |
|        - |  9002 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  9003 | ` */` |
|       36 |  9004 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9005 |  |
|       38 |  9006 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9007 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9008 | `	ph7_value sResult;` |
|        - |  9009 | `	ph7_value *pResumeVal;` |
|        - |  9010 | `	sxi32 rc;` |
|       38 |  9011 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9012 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  9013 | `		return PH7_OK;` |
|        - |  9014 | `	}` |
|       38 |  9015 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  9016 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9017 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  9018 | `		return PH7_OK;` |
|        - |  9019 | `	}` |
|       38 |  9020 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9021 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9022 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  9023 | `	}` |
|       36 |  9024 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  9025 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  9026 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  9027 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9028 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9029 | `		return PH7_ABORT;` |
|        - |  9030 | `	}` |
|       36 |  9031 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9032 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9033 | `		return PH7_EXCEPTION;` |
|        - |  9034 | `	}` |
|       36 |  9035 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  9036 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  9037 | `	return PH7_OK;` |
|       20 |  9038 |  |
|        - |  9039 | `/*` |
|        - |  9040 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  9041 | ` */` |
|        6 |  9042 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9043 |  |
|        8 |  9044 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9045 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  9046 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9047 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9048 | `		return PH7_OK;` |
|        - |  9049 | `	}` |
|        8 |  9050 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  9051 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9052 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9053 | `		return PH7_OK;` |
|        - |  9054 | `	}` |
|        8 |  9055 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9056 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9057 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9058 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  9059 | `		}` |
|      ! 0 |  9060 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9061 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  9062 | `	}` |
|        8 |  9063 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  9064 | `	return PH7_OK;` |
|        5 |  9065 |  |
|        - |  9066 | `/*` |
|        - |  9067 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  9068 | ` */` |
|        6 |  9069 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9070 |  |
|        - |  9071 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9072 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9073 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9074 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  9075 | `	return PH7_OK;` |
|        4 |  9076 |  |
|      ! 0 |  9077 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9078 |  |
|        - |  9079 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  9080 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  9081 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9082 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  9083 | `	return PH7_OK;` |
|      ! 0 |  9084 |  |
|        6 |  9085 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9086 |  |
|        - |  9087 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9088 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9089 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9090 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  9091 | `	return PH7_OK;` |
|        4 |  9092 |  |
|        6 |  9093 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9094 |  |
|        - |  9095 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9096 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9097 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9098 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  9099 | `	return PH7_OK;` |
|        4 |  9100 |  |
|        - |  9101 | `/*` |
|        - |  9102 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  9103 | ` */` |
|        4 |  9104 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9105 |  |
|        5 |  9106 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9107 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  9108 | `	if( nArg < 1 ){` |
|      ! 0 |  9109 | `		return PH7_OK;` |
|        - |  9110 | `	}` |
|        5 |  9111 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  9112 | `	if( pExecCtx ){` |
|        5 |  9113 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  9114 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  9115 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  9116 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9117 | `			SyString sAttrName;` |
|        - |  9118 | `			ph7_value *pAttr;` |
|        5 |  9119 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  9120 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  9121 | `			if( pAttr ){` |
|        5 |  9122 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  9123 | `			}` |
|        2 |  9124 | `		}` |
|        2 |  9125 | `	}` |
|        5 |  9126 | `	return PH7_OK;` |
|        3 |  9127 |  |
|        - |  9128 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  9129 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  9130 |  |
|        - |  9131 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9132 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  9133 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9134 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  9135 |  |
|      ! 0 |  9136 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  9137 |  |
|        - |  9138 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9139 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  9140 | `	ph7_exec_ctx *pCtx;` |
|        - |  9141 | `	ph7_vm_func *pFunc;` |
|        - |  9142 | `	ph7_value *pCallable;` |
|        - |  9143 | `	ph7_value *pCtxAttr;` |
|        - |  9144 | `	SyString sAttrName;` |
|        - |  9145 | `	/* Must not already be started */` |
|      ! 0 |  9146 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9147 | `	if( pCtx != 0 ){` |
|      ! 0 |  9148 | `		return SXERR_INVALID;` |
|        - |  9149 | `	}` |
|      ! 0 |  9150 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9151 | `		return SXERR_INVALID;` |
|        - |  9152 | `	}` |
|      ! 0 |  9153 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  9154 | `	/* Get the callable */` |
|      ! 0 |  9155 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  9156 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9157 | `	if( pCallable == 0 ){` |
|      ! 0 |  9158 | `		return SXERR_INVALID;` |
|        - |  9159 | `	}` |
|        - |  9160 | `	/* Resolve callable */` |
|      ! 0 |  9161 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9162 | `		SyString sName;` |
|        - |  9163 | `		SyHashEntry *pEntry;` |
|      ! 0 |  9164 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  9165 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  9166 | `		if( pEntry == 0 ){` |
|      ! 0 |  9167 | `			return SXERR_NOTFOUND;` |
|        - |  9168 | `		}` |
|      ! 0 |  9169 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  9170 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9171 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9172 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9173 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9174 | `		if( pMethod == 0 ){` |
|      ! 0 |  9175 | `			return SXERR_INVALID;` |
|        - |  9176 | `		}` |
|      ! 0 |  9177 | `		pClosureThis = pClosure;` |
|      ! 0 |  9178 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  9179 | `	}else{` |
|      ! 0 |  9180 | `		return SXERR_INVALID;` |
|        - |  9181 | `	}` |
|        - |  9182 | `	/* Create context */` |
|      ! 0 |  9183 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9184 | `	if( pCtx == 0 ){` |
|      ! 0 |  9185 | `		return SXERR_MEM;` |
|        - |  9186 | `	}` |
|        - |  9187 | `	/* Store in __ctx */` |
|      ! 0 |  9188 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9189 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9190 | `	if( pCtxAttr ){` |
|      ! 0 |  9191 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9192 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9193 | `	}` |
|        - |  9194 | `	/* Set up frame with args */` |
|      ! 0 |  9195 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9196 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9197 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9198 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9199 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9200 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9201 |  |
|      ! 0 |  9202 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9203 |  |
|      ! 0 |  9204 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9205 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9206 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9207 |  |
|      ! 0 |  9208 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9209 |  |
|      ! 0 |  9210 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9211 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9212 |  |
|      ! 0 |  9213 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9214 |  |
|      ! 0 |  9215 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9216 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9217 |  |
|      ! 0 |  9218 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9219 |  |
|      ! 0 |  9220 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9221 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9222 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9223 |  |
|        - |  9224 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9225 | `/*` |
|        - |  9226 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9227 | ` */` |
|       22 |  9228 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9229 |  |
|        - |  9230 | `	ph7_generator *pGen;` |
|       24 |  9231 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9232 | `	if( pGen == 0 ){` |
|      ! 0 |  9233 | `		return 0;` |
|        - |  9234 | `	}` |
|       24 |  9235 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9236 | `	pGen->pCtx = pCtx;` |
|       24 |  9237 | `	pGen->iImplicitKey = 0;` |
|       24 |  9238 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9239 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9240 | `	/* Link the generator back to the exec context */` |
|       24 |  9241 | `	pCtx->pPrivate = pGen;` |
|       24 |  9242 | `	return pGen;` |
|       13 |  9243 |  |
|        - |  9244 | `/*` |
|        - |  9245 | ` * Release a generator and its execution context.` |
|        - |  9246 | ` */` |
|      ! 0 |  9247 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  9248 |  |
|      ! 0 |  9249 | `	if( pGen == 0 ){` |
|      ! 0 |  9250 | `		return;` |
|        - |  9251 | `	}` |
|      ! 0 |  9252 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9253 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9254 | `	if( pGen->pCtx ){` |
|      ! 0 |  9255 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  9256 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  9257 | `		pGen->pCtx = 0;` |
|      ! 0 |  9258 | `	}` |
|      ! 0 |  9259 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  9260 |  |
|        - |  9261 | `/*` |
|        - |  9262 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  9263 | ` */` |
|      236 |  9264 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  9265 |  |
|        - |  9266 | `	ph7_class_instance *pThis;` |
|        - |  9267 | `	SyString sAttr;` |
|        - |  9268 | `	ph7_value *pAttr;` |
|      238 |  9269 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9270 | `		return 0;` |
|        - |  9271 | `	}` |
|      238 |  9272 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 |  9273 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  9274 | `		return 0;` |
|        - |  9275 | `	}` |
|      238 |  9276 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 |  9277 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 |  9278 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  9279 | `		return 0;` |
|        - |  9280 | `	}` |
|      238 |  9281 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 |  9282 |  |
|        - |  9283 | `/*` |
|        - |  9284 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  9285 | ` */` |
|       22 |  9286 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9287 |  |
|        - |  9288 | `	ph7_generator *pGen;` |
|        - |  9289 | `	sxi32 rc;` |
|       24 |  9290 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 |  9291 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 |  9292 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 |  9293 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 |  9294 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 |  9295 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 |  9296 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 |  9297 | `	}` |
|       24 |  9298 | `	return PH7_OK;` |
|       13 |  9299 |  |
|        - |  9300 | `/*` |
|        - |  9301 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  9302 | ` */` |
|       68 |  9303 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9304 |  |
|        - |  9305 | `	ph7_generator *pGen;` |
|       70 |  9306 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 |  9307 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9308 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 |  9309 | `	return PH7_OK;` |
|       36 |  9310 |  |
|        - |  9311 | `/*` |
|        - |  9312 | ` * Generator::current() — return the last yielded value.` |
|        - |  9313 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9314 | ` */` |
|       68 |  9315 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9316 |  |
|        - |  9317 | `	ph7_generator *pGen;` |
|        - |  9318 | `	sxi32 rc;` |
|       70 |  9319 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9320 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9321 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9322 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9323 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9324 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9325 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9326 | `	}` |
|       70 |  9327 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 |  9328 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 |  9329 | `	}else{` |
|      ! 0 |  9330 | `		ph7_result_null(pCtx);` |
|        - |  9331 | `	}` |
|       70 |  9332 | `	return PH7_OK;` |
|       36 |  9333 |  |
|        - |  9334 | `/*` |
|        - |  9335 | ` * Generator::key() — return the last yielded key.` |
|        - |  9336 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9337 | ` */` |
|       12 |  9338 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9339 |  |
|        - |  9340 | `	ph7_generator *pGen;` |
|        - |  9341 | `	sxi32 rc;` |
|       13 |  9342 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9343 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  9344 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9345 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9346 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9347 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9348 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9349 | `	}` |
|       13 |  9350 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  9351 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  9352 | `	}else{` |
|      ! 0 |  9353 | `		ph7_result_null(pCtx);` |
|        - |  9354 | `	}` |
|       13 |  9355 | `	return PH7_OK;` |
|        7 |  9356 |  |
|        - |  9357 | `/*` |
|        - |  9358 | ` * Generator::next() — advance to the next yield point.` |
|        - |  9359 | ` */` |
|       60 |  9360 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9361 |  |
|        - |  9362 | `	ph7_generator *pGen;` |
|        - |  9363 | `	sxi32 rc;` |
|       62 |  9364 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 |  9365 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 |  9366 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 |  9367 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9368 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 |  9369 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 |  9370 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 |  9371 | `	}else{` |
|      ! 0 |  9372 | `		return PH7_OK;` |
|        - |  9373 | `	}` |
|       62 |  9374 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 |  9375 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 |  9376 | `	return PH7_OK;` |
|       32 |  9377 |  |
|        - |  9378 | `/*` |
|        - |  9379 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  9380 | ` */` |
|        4 |  9381 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9382 |  |
|        - |  9383 | `	ph7_generator *pGen;` |
|        - |  9384 | `	ph7_value *pSendVal;` |
|        - |  9385 | `	sxi32 rc;` |
|        5 |  9386 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  9387 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  9388 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  9389 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  9390 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  9391 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  9392 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  9393 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  9394 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  9395 | `	}else{` |
|      ! 0 |  9396 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9397 | `		return PH7_OK;` |
|        - |  9398 | `	}` |
|        5 |  9399 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  9400 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  9401 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9402 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  9403 | `	}else{` |
|        3 |  9404 | `		ph7_result_null(pCtx);` |
|        - |  9405 | `	}` |
|        5 |  9406 | `	return PH7_OK;` |
|        3 |  9407 |  |
|        - |  9408 | `/*` |
|        - |  9409 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  9410 | ` *` |
|        - |  9411 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  9412 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  9413 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  9414 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  9415 | ` * the exception to the caller.` |
|        - |  9416 | ` */` |
|      ! 0 |  9417 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9418 |  |
|        - |  9419 | `	ph7_generator *pGen;` |
|        - |  9420 | `	const char *zMsg;` |
|        - |  9421 | `	int nLen;` |
|      ! 0 |  9422 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  9423 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9424 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  9425 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  9426 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  9427 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9428 | `			"Cannot throw into a closed generator");` |
|        - |  9429 | `	}` |
|        - |  9430 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  9431 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  9432 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  9433 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  9434 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9435 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  9436 | `	nLen = 0;` |
|      ! 0 |  9437 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  9438 | `		/* Try to get the exception's message */` |
|        - |  9439 | `		SyString sAttr;` |
|        - |  9440 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  9441 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  9442 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  9443 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  9444 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  9445 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  9446 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  9447 | `		}` |
|      ! 0 |  9448 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  9449 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  9450 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  9451 | `	}` |
|      ! 0 |  9452 | `	(void)nLen;` |
|      ! 0 |  9453 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  9454 |  |
|        - |  9455 | `/*` |
|        - |  9456 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  9457 | ` */` |
|        2 |  9458 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9459 |  |
|        - |  9460 | `	ph7_generator *pGen;` |
|        3 |  9461 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9462 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  9463 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9464 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9465 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9466 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  9467 | `	}` |
|        3 |  9468 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  9469 | `	return PH7_OK;` |
|        2 |  9470 |  |
|        - |  9471 | `/*` |
|        - |  9472 | ` * Generator::__destruct() — clean up.` |
|        - |  9473 | ` */` |
|      ! 0 |  9474 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9475 |  |
|        - |  9476 | `	ph7_generator *pGen;` |
|      ! 0 |  9477 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  9478 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9479 | `	if( pGen ){` |
|      ! 0 |  9480 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  9481 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9482 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9483 | `			SyString sAttrName;` |
|        - |  9484 | `			ph7_value *pAttr;` |
|      ! 0 |  9485 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9486 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9487 | `			if( pAttr ){` |
|      ! 0 |  9488 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  9489 | `			}` |
|      ! 0 |  9490 | `		}` |
|      ! 0 |  9491 | `	}` |
|      ! 0 |  9492 | `	return PH7_OK;` |
|      ! 0 |  9493 |  |
|        - |  9494 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  9495 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  9496 | `/*` |
|        - |  9497 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  9498 | ` * the desired message.` |
|        - |  9499 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  9500 | ` * in 'api.c' for additional information.` |
|        - |  9501 | ` */` |
|      370 |  9502 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  9503 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  9504 | `	SyString *pString /* Message to output */` |
|        - |  9505 | `	)` |
|        2 |  9506 |  |
|      372 |  9507 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  9508 | `	sxi32 rc = SXRET_OK;` |
|        - |  9509 | `	/* Call the output consumer */` |
|      372 |  9510 | `	if( pString->nByte > 0 ){` |
|      372 |  9511 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  9512 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  9513 | `	}` |
|      372 |  9514 | `	return rc;` |
|        2 |  9515 |  |
|        - |  9516 | `/*` |
|        - |  9517 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  9518 | ` * callback to consume the formatted message.` |
|        - |  9519 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  9520 | ` * in 'api.c' for additional information.` |
|        - |  9521 | ` */` |
|        2 |  9522 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  9523 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  9524 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  9525 | `	va_list ap           /* Variable list of arguments */` |
|        - |  9526 | `	)` |
|        1 |  9527 |  |
|        3 |  9528 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  9529 | `	sxi32 rc = SXRET_OK;` |
|        - |  9530 | `	SyBlob sWorker;` |
|        - |  9531 | `	/* Format the message and call the output consumer */` |
|        3 |  9532 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  9533 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  9534 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  9535 | `		/* Consume the formatted message */` |
|        3 |  9536 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  9537 | `	}` |
|        3 |  9538 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  9539 | `	/* Release the working buffer */` |
|        3 |  9540 | `	SyBlobRelease(&sWorker);` |
|        3 |  9541 | `	return rc;` |
|        1 |  9542 |  |
|        - |  9543 | `/*` |
|        - |  9544 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  9545 | ` * This function never fail and always return a pointer` |
|        - |  9546 | ` * to a null terminated string.` |
|        - |  9547 | ` */` |
|       12 |  9548 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  9549 |  |
|       13 |  9550 | `	const char *zOp = "Unknown     ";` |
|       13 |  9551 | `	switch(nOp){` |
|        3 |  9552 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  9553 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  9554 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  9555 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  9556 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  9557 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  9558 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  9559 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  9560 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  9561 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  9562 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  9563 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  9564 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  9565 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  9566 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  9567 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  9568 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  9569 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  9570 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  9571 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  9572 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  9573 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  9574 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  9575 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  9576 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  9577 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  9578 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  9579 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  9580 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  9581 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  9582 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  9583 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  9584 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  9585 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  9586 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  9587 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  9588 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  9589 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  9590 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  9591 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  9592 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  9593 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  9594 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  9595 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  9596 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  9597 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  9598 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  9599 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  9600 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  9601 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  9602 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  9603 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  9604 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  9605 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  9606 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  9607 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  9608 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  9609 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  9610 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 |  9611 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  9612 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  9613 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  9614 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  9615 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  9616 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  9617 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  9618 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  9619 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  9620 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  9621 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  9622 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  9623 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  9624 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  9625 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  9626 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  9627 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  9628 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  9629 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  9630 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  9631 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  9632 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  9633 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  9634 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  9635 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  9636 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  9637 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  9638 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  9639 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  9640 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  9641 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  9642 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 |  9643 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  9644 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  9645 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  9646 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  9647 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  9648 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  9649 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  9650 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  9651 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  9652 | `	default:` |
|      ! 0 |  9653 | `		break;` |
|        - |  9654 | `	}` |
|       13 |  9655 | `	return zOp;` |
|        1 |  9656 |  |
|        - |  9657 | `/*` |
|        - |  9658 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  9659 | ` * The xConsumer() callback which is an used defined function` |
|        - |  9660 | ` * is responsible of consuming the generated dump.` |
|        - |  9661 | ` */` |
|        2 |  9662 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  9663 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  9664 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  9665 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  9666 | `	)` |
|        1 |  9667 |  |
|        - |  9668 | `	sxi32 rc;` |
|        3 |  9669 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  9670 | `	return rc;` |
|        1 |  9671 |  |
|        - |  9672 | `/*` |
|        - |  9673 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  9674 | ` * outside a class body [i.e: global or function scope].` |
|        - |  9675 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  9676 | ` * in 'compile.c' for additional information.` |
|        - |  9677 | ` */` |
|       14 |  9678 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  9679 |  |
|       15 |  9680 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  9681 | `	/* Evaluate and expand constant value */` |
|       15 |  9682 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  9683 |  |
|        - |  9684 | `/*` |
|        - |  9685 | ` * Section:` |
|        - |  9686 | ` *  Function handling functions.` |
|        - |  9687 | ` * Status:` |
|        - |  9688 | ` *    Stable.` |
|        - |  9689 | ` */` |
|        - |  9690 | `/*` |
|        - |  9691 | ` * int func_num_args(void)` |
|        - |  9692 | ` *   Returns the number of arguments passed to the function.` |
|        - |  9693 | ` * Parameters` |
|        - |  9694 | ` *   None.` |
|        - |  9695 | ` * Return` |
|        - |  9696 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  9697 | ` *  or -1 if called from the globe scope.` |
|        - |  9698 | ` */` |
|      944 |  9699 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9700 |  |
|        - |  9701 | `	VmFrame *pFrame;` |
|        - |  9702 | `	ph7_vm *pVm;` |
|        - |  9703 | `	/* Point to the target VM */` |
|      946 |  9704 | `	pVm = pCtx->pVm;` |
|        - |  9705 | `	/* Current frame */` |
|      946 |  9706 | `	pFrame = pVm->pFrame;` |
|      946 |  9707 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  9708 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  9709 | `		SXUNUSED(nArg);` |
|      ! 0 |  9710 | `		SXUNUSED(apArg);` |
|        - |  9711 | `		/* Global frame,return -1 */` |
|      ! 0 |  9712 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  9713 | `		return SXRET_OK;` |
|        - |  9714 | `	}` |
|        - |  9715 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  9716 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  9717 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  9718 | `	return SXRET_OK;` |
|      474 |  9719 |  |
|        - |  9720 | `/*` |
|        - |  9721 | ` * value func_get_arg(int $arg_num)` |
|        - |  9722 | ` *   Return an item from the argument list.` |
|        - |  9723 | ` * Parameters` |
|        - |  9724 | ` *  Argument number(index start from zero).` |
|        - |  9725 | ` * Return` |
|        - |  9726 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  9727 | ` */` |
|       22 |  9728 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9729 |  |
|       24 |  9730 | `	ph7_value *pObj = 0;` |
|       24 |  9731 | `	VmSlot *pSlot = 0;` |
|        - |  9732 | `	VmFrame *pFrame;` |
|        - |  9733 | `	ph7_vm *pVm;` |
|        - |  9734 | `	/* Point to the target VM */` |
|       24 |  9735 | `	pVm = pCtx->pVm;` |
|        - |  9736 | `	/* Current frame */` |
|       24 |  9737 | `	pFrame = pVm->pFrame;` |
|       24 |  9738 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  9739 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  9740 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  9741 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  9742 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9743 | `		return SXRET_OK;` |
|        - |  9744 | `	}` |
|        - |  9745 | `	/* Extract the desired index */` |
|       21 |  9746 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  9747 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  9748 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  9749 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9750 | `		return SXRET_OK;` |
|        - |  9751 | `	}` |
|        - |  9752 | `	/* Extract the desired argument */` |
|       21 |  9753 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  9754 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  9755 | `			/* Return the desired argument */` |
|       21 |  9756 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  9757 | `		}else{` |
|        - |  9758 | `			/* No such argument,return false */` |
|      ! 0 |  9759 | `			ph7_result_bool(pCtx,0);` |
|        - |  9760 | `		}` |
|       11 |  9761 | `	}else{` |
|        - |  9762 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  9763 | `		ph7_result_bool(pCtx,0);` |
|        - |  9764 | `	}` |
|       21 |  9765 | `	return SXRET_OK;` |
|       13 |  9766 |  |
|        - |  9767 | `/*` |
|        - |  9768 | ` * array func_get_args_byref(void)` |
|        - |  9769 | ` *   Returns an array comprising a function's argument list.` |
|        - |  9770 | ` * Parameters` |
|        - |  9771 | ` *  None.` |
|        - |  9772 | ` * Return` |
|        - |  9773 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  9774 | ` *  member of the current user-defined function's argument list.` |
|        - |  9775 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9776 | ` * NOTE:` |
|        - |  9777 | ` *  Arguments are returned to the array by reference.` |
|        - |  9778 | ` */` |
|        2 |  9779 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9780 |  |
|        - |  9781 | `	ph7_value *pArray;` |
|        - |  9782 | `	VmFrame *pFrame;` |
|        - |  9783 | `	VmSlot *aSlot;` |
|        - |  9784 | `	sxu32 n;` |
|        - |  9785 | `	/* Point to the current frame */` |
|        3 |  9786 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  9787 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  9788 | `	if( pFrame->pParent == 0 ){` |
|        - |  9789 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9790 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9791 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9792 | `		return SXRET_OK;` |
|        - |  9793 | `	}` |
|        - |  9794 | `	/* Create a new array */` |
|        3 |  9795 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9796 | `	if( pArray == 0 ){` |
|      ! 0 |  9797 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9798 | `		SXUNUSED(apArg);` |
|      ! 0 |  9799 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9800 | `		return SXRET_OK;` |
|        - |  9801 | `	}` |
|        - |  9802 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  9803 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  9804 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  9805 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  9806 | `	}` |
|        - |  9807 | `	/* Return the freshly created array */` |
|        3 |  9808 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9809 | `	return SXRET_OK;` |
|        2 |  9810 |  |
|        - |  9811 | `/*` |
|        - |  9812 | ` * array func_get_args(void)` |
|        - |  9813 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  9814 | ` * Parameters` |
|        - |  9815 | ` *  None.` |
|        - |  9816 | ` * Return` |
|        - |  9817 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  9818 | ` *  member of the current user-defined function's argument list.` |
|        - |  9819 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9820 | ` */` |
|       88 |  9821 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9822 |  |
|       90 |  9823 | `	ph7_value *pObj = 0;` |
|        - |  9824 | `	ph7_value *pArray;` |
|        - |  9825 | `	VmFrame *pFrame;` |
|        - |  9826 | `	VmSlot *aSlot;` |
|        - |  9827 | `	sxu32 n;` |
|        - |  9828 | `	/* Point to the current frame */` |
|       90 |  9829 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  9830 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  9831 | `	if( pFrame->pParent == 0 ){` |
|        - |  9832 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9833 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9834 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9835 | `		return SXRET_OK;` |
|        - |  9836 | `	}` |
|        - |  9837 | `	/* Create a new array */` |
|       90 |  9838 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  9839 | `	if( pArray == 0 ){` |
|      ! 0 |  9840 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9841 | `		SXUNUSED(apArg);` |
|      ! 0 |  9842 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9843 | `		return SXRET_OK;` |
|        - |  9844 | `	}` |
|        - |  9845 | `	/* Start filling the array with the given arguments */` |
|       90 |  9846 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  9847 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  9848 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  9849 | `		if( pObj ){` |
|      134 |  9850 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  9851 | `		}` |
|       68 |  9852 | `	}` |
|        - |  9853 | `	/* Return the freshly created array */` |
|       90 |  9854 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  9855 | `	return SXRET_OK;` |
|       46 |  9856 |  |
|        - |  9857 | `/*` |
|        - |  9858 | ` * bool function_exists(string $name)` |
|        - |  9859 | ` *  Return TRUE if the given function has been defined.` |
|        - |  9860 | ` * Parameters` |
|        - |  9861 | ` *  The name of the desired function.` |
|        - |  9862 | ` * Return` |
|        - |  9863 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  9864 | ` */` |
|     1680 |  9865 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9866 |  |
|        - |  9867 | `	const char *zName;` |
|        - |  9868 | `	ph7_vm *pVm;` |
|        - |  9869 | `	int nLen;` |
|        - |  9870 | `	int res;` |
|     1682 |  9871 | `	if( nArg < 1 ){` |
|        - |  9872 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  9873 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9874 | `		return SXRET_OK;` |
|        - |  9875 | `	}` |
|        - |  9876 | `	/* Point to the target VM */` |
|     1682 |  9877 | `	pVm = pCtx->pVm;` |
|        - |  9878 | `	/* Extract the function name */` |
|     1682 |  9879 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9880 | `	/* Assume the function is not defined */` |
|     1682 |  9881 | `	res = 0;` |
|        - |  9882 | `	/* Perform the lookup */` |
|     2520 |  9883 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1676 |  9884 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9885 | `			/* Function is defined */` |
|      206 |  9886 | `			res = 1;` |
|      102 |  9887 | `	}` |
|     1682 |  9888 | `	ph7_result_bool(pCtx,res);` |
|     1682 |  9889 | `	return SXRET_OK;` |
|      842 |  9890 |  |
|        - |  9891 | `/*` |
|        - |  9892 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9893 | ` * [i.e: Whether it is callable or not].` |
|        - |  9894 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  9895 | ` */` |
|    20416 |  9896 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  9897 |  |
|    20418 |  9898 | `	int res = 0;` |
|    20418 |  9899 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9900 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  9901 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  9902 | `		ph7_class_method *pMethod;` |
|      ! 0 |  9903 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  9904 | `		if( pMethod && CallInvoke ){` |
|        - |  9905 | `			ph7_value sResult;` |
|        - |  9906 | `			sxi32 rc;` |
|        - |  9907 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  9908 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  9909 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  9910 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  9911 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  9912 | `			}` |
|      ! 0 |  9913 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9914 | `		}` |
|    20418 |  9915 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  9916 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  9917 | `		if( pMap->nEntry == 2 ){` |
|        - |  9918 | `			ph7_class *pClass;` |
|        - |  9919 | `			ph7_value *pV;` |
|        - |  9920 | `			/* Extract the target class */` |
|       12 |  9921 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  9922 | `			if( pV ){` |
|       12 |  9923 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  9924 | `				if( pClass ){` |
|        - |  9925 | `					ph7_class_method *pMethod;` |
|        - |  9926 | `					/* Extract the target method */` |
|       10 |  9927 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  9928 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  9929 | `						/* Perform the lookup */` |
|       10 |  9930 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  9931 | `						if( pMethod ){` |
|        - |  9932 | `							/* Method is callable */` |
|        5 |  9933 | `							res = 1;` |
|        2 |  9934 | `						}` |
|        4 |  9935 | `					}` |
|        4 |  9936 | `				}` |
|        5 |  9937 | `			}` |
|        7 |  9938 | `		}` |
|    20405 |  9939 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  9940 | `		const char *zName;` |
|        - |  9941 | `		int nLen;` |
|        - |  9942 | `		/* Extract the name */` |
|     5424 |  9943 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  9944 | `		/* Perform the lookup */` |
|     5439 |  9945 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  9946 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9947 | `				/* Function is callable */` |
|     5406 |  9948 | `				res = 1;` |
|     2702 |  9949 | `		}` |
|     2711 |  9950 | `	}` |
|    20418 |  9951 | `	return res;` |
|        2 |  9952 |  |
|        - |  9953 | `/*` |
|        - |  9954 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  9955 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9956 | ` * Parameters` |
|        - |  9957 | ` * $name` |
|        - |  9958 | ` *    The callback function to check` |
|        - |  9959 | ` * $syntax_only` |
|        - |  9960 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  9961 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  9962 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  9963 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  9964 | ` *    a string.` |
|        - |  9965 | ` * Return` |
|        - |  9966 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  9967 | ` */` |
|       14 |  9968 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9969 |  |
|        - |  9970 | `	ph7_vm *pVm;` |
|        - |  9971 | `	int res;` |
|       15 |  9972 | `	if( nArg < 1 ){` |
|        - |  9973 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9974 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9975 | `		return SXRET_OK;` |
|        - |  9976 | `	}` |
|        - |  9977 | `	/* Point to the target VM */` |
|       15 |  9978 | `	pVm = pCtx->pVm;` |
|        - |  9979 | `	/* Perform the requested operation */` |
|       15 |  9980 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  9981 | `	ph7_result_bool(pCtx,res);` |
|       15 |  9982 | `	return SXRET_OK;` |
|        8 |  9983 |  |
|        - |  9984 | `/*` |
|        - |  9985 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  9986 | ` * defined below.` |
|        - |  9987 | ` */` |
|     1218 |  9988 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9989 |  |
|     1219 |  9990 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9991 | `	ph7_value sName;` |
|        - |  9992 | `	sxi32 rc;` |
|        - |  9993 | `	/* Prepare the function name for insertion */` |
|     1219 |  9994 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1219 |  9995 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9996 | `	/* Perform the insertion */` |
|     1219 |  9997 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1219 |  9998 | `	PH7_MemObjRelease(&sName);` |
|     1219 |  9999 | `	return rc;` |
|        1 | 10000 |  |
|        - | 10001 | `/*` |
|        - | 10002 | ` * array get_defined_functions(void)` |
|        - | 10003 | ` *  Returns an array of all defined functions.` |
|        - | 10004 | ` * Parameter` |
|        - | 10005 | ` *  None.` |
|        - | 10006 | ` * Return` |
|        - | 10007 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 10008 | ` *  both built-in (internal) and user-defined.` |
|        - | 10009 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 10010 | ` *  defined ones using $arr["user"].` |
|        - | 10011 | ` * Note:` |
|        - | 10012 | ` *  NULL is returned on failure.` |
|        - | 10013 | ` */` |
|        2 | 10014 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10015 |  |
|        - | 10016 | `	ph7_value *pArray,*pEntry;` |
|        - | 10017 | `	/* NOTE:` |
|        - | 10018 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 10019 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 10020 | `	 */` |
|        3 | 10021 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10022 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10023 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10024 | `		SXUNUSED(apArg);` |
|        - | 10025 | `		/* Return NULL */` |
|      ! 0 | 10026 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10027 | `		return SXRET_OK;` |
|        - | 10028 | `	}` |
|        3 | 10029 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10030 | `	if( pEntry == 0 ){` |
|        - | 10031 | `		/* Return NULL */` |
|      ! 0 | 10032 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10033 | `		return SXRET_OK;` |
|        - | 10034 | `	}` |
|        - | 10035 | `	/* Fill with the appropriate information */` |
|        3 | 10036 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 10037 | `	/* Create the 'internal' index */` |
|        3 | 10038 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 10039 | `	/* Create the user-func array */` |
|        3 | 10040 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10041 | `	if( pEntry == 0 ){` |
|        - | 10042 | `		/* Return NULL */` |
|      ! 0 | 10043 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10044 | `		return SXRET_OK;` |
|        - | 10045 | `	}` |
|        - | 10046 | `	/* Fill with the appropriate information */` |
|        3 | 10047 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 10048 | `	/* Create the 'user' index */` |
|        3 | 10049 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 10050 | `	/* Return the multi-dimensional array */` |
|        3 | 10051 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10052 | `	return SXRET_OK;` |
|        2 | 10053 |  |
|        - | 10054 | `/*` |
|        - | 10055 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 10056 | ` *  Register a function for execution on shutdown.` |
|        - | 10057 | ` * Note` |
|        - | 10058 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 10059 | ` *  be called in the same order as they were registered.` |
|        - | 10060 | ` * Parameters` |
|        - | 10061 | ` *  $callback` |
|        - | 10062 | ` *   The shutdown callback to register.` |
|        - | 10063 | ` * $param` |
|        - | 10064 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 10065 | ` * Return` |
|        - | 10066 | ` *  Nothing.` |
|        - | 10067 | ` */` |
|        2 | 10068 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10069 |  |
|        - | 10070 | `	VmShutdownCB sEntry;` |
|        - | 10071 | `	int i,j;` |
|        3 | 10072 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10073 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 10074 | `		return PH7_OK;` |
|        - | 10075 | `	}` |
|        - | 10076 | `	/* Zero the Entry */` |
|        3 | 10077 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 10078 | `	/* Initialize fields */` |
|        3 | 10079 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 10080 | `	/* Save the callback name for later invocation name */` |
|        3 | 10081 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 10082 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 10083 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 10084 | `	}` |
|        - | 10085 | `	/* Copy arguments */` |
|        3 | 10086 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 10087 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 10088 | `			/* Limit reached */` |
|      ! 0 | 10089 | `			break;` |
|        - | 10090 | `		}` |
|      ! 0 | 10091 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 10092 | `	}` |
|        3 | 10093 | `	sEntry.nArg = j;` |
|        - | 10094 | `	/* Install the callback */` |
|        3 | 10095 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 10096 | `	return PH7_OK;` |
|        2 | 10097 |  |
|        - | 10098 | `/*` |
|        - | 10099 | ` * Section:` |
|        - | 10100 | ` *  Class handling functions.` |
|        - | 10101 | ` * Status:` |
|        - | 10102 | ` *    Stable.` |
|        - | 10103 | ` */` |
|        - | 10104 | `/*` |
|        - | 10105 | ` * Extract the top active class. NULL is returned` |
|        - | 10106 | ` * if the class stack is empty.` |
|        - | 10107 | ` */` |
|      782 | 10108 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 10109 |  |
|      784 | 10110 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 10111 | `	ph7_class **apClass;` |
|      784 | 10112 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 10113 | `		/* Empty stack,return NULL */` |
|       15 | 10114 | `		return 0;` |
|        - | 10115 | `	}` |
|        - | 10116 | `	/* Peek the last entry */` |
|      770 | 10117 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      770 | 10118 | `	return apClass[pSet->nUsed - 1];` |
|      393 | 10119 |  |
|        - | 10120 | `/*` |
|        - | 10121 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 10122 | ` *   Get the class that declared the currently executing method.` |
|        - | 10123 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 10124 | ` *` |
|        - | 10125 | ` * Parameters` |
|        - | 10126 | ` *   pVm: Target VM` |
|        - | 10127 | ` *` |
|        - | 10128 | ` * Return` |
|        - | 10129 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 10130 | ` *   - Not executing within a class method` |
|        - | 10131 | ` *` |
|        - | 10132 | ` * Note` |
|        - | 10133 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 10134 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 10135 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 10136 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 10137 | ` *   declaring class.` |
|        - | 10138 | ` */` |
|       98 | 10139 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 10140 |  |
|      100 | 10141 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10142 | `	ph7_vm_func *pVmFunc;` |
|        - | 10143 |  |
|        - | 10144 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 10145 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 10146 |  |
|        - | 10147 | `	/* Check if we're in a method context */` |
|      100 | 10148 | `	if( pFrame->pParent ){` |
|       96 | 10149 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 10150 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 10151 | `			/* Return the declaring class */` |
|       96 | 10152 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 10153 | `		}` |
|      ! 0 | 10154 | `	}` |
|        - | 10155 |  |
|        5 | 10156 | `	return 0;` |
|       51 | 10157 |  |
|        - | 10158 |  |
|        - | 10159 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 10160 | `/*` |
|        - | 10161 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 10162 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 10163 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 10164 | ` * return value indicates failure.` |
|        - | 10165 | ` */` |
|        - | 10166 | `/*` |
|        - | 10167 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 10168 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 10169 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 10170 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 10171 | ` */` |
|     1820 | 10172 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 10173 | `	ph7_vm *pVm,` |
|        - | 10174 | `	ph7_class_instance *pThis,` |
|        - | 10175 | `	ph7_class_method *pMethod,` |
|        - | 10176 | `	ph7_value *pResult,` |
|        - | 10177 | `	int nArg,` |
|        - | 10178 | `	ph7_value **apArg,` |
|        - | 10179 | `	VmCallArgMap *pMap` |
|        - | 10180 | `	)` |
|        2 | 10181 |  |
|        - | 10182 | `	ph7_value *aStack;` |
|        - | 10183 | `	VmInstr aInstr[2];` |
|        - | 10184 | `	int iCursor;` |
|        - | 10185 | `	int i;` |
|     1822 | 10186 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     1822 | 10187 | `	if( aStack == 0 ){` |
|      ! 0 | 10188 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10189 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10190 | `		return SXERR_MEM;` |
|        - | 10191 | `	}` |
|     2702 | 10192 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      882 | 10193 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|      882 | 10194 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      442 | 10195 | `	}` |
|     1822 | 10196 | `	iCursor = nArg + 1;` |
|     1822 | 10197 | `	if( pThis ){` |
|     1816 | 10198 | `		pThis->iRef++;` |
|     1816 | 10199 | `		aStack[i].x.pOther = pThis;` |
|     1816 | 10200 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      907 | 10201 | `	}` |
|     1822 | 10202 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1822 | 10203 | `	i++;` |
|     1822 | 10204 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1822 | 10205 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1822 | 10206 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1822 | 10207 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1822 | 10208 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1822 | 10209 | `	aInstr[0].iP1 = nArg;` |
|     1822 | 10210 | `	aInstr[0].iP2 = 0;` |
|     1822 | 10211 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     1822 | 10212 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1822 | 10213 | `	aInstr[1].iP1 = 1;` |
|     1822 | 10214 | `	aInstr[1].iP2 = 0;` |
|     1822 | 10215 | `	aInstr[1].p3  = 0;` |
|     1822 | 10216 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|     1822 | 10217 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1822 | 10218 | `	return PH7_OK;` |
|      912 | 10219 |  |
|     1516 | 10220 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10221 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10222 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10223 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10224 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10225 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10226 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10227 | `	)` |
|        2 | 10228 |  |
|     1518 | 10229 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10230 |  |
|        - | 10231 | `/*` |
|        - | 10232 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 10233 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 10234 | ` * in the apArg[] array.` |
|        - | 10235 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10236 | ` * return value indicates failure.` |
|        - | 10237 | ` */` |
|      970 | 10238 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 10239 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10240 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10241 | `	int nArg,          /* Total number of given arguments */` |
|        - | 10242 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 10243 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 10244 | `	)` |
|        2 | 10245 |  |
|        - | 10246 | `	ph7_value *aStack;` |
|        - | 10247 | `	VmInstr aInstr[2];` |
|        - | 10248 | `	int i;` |
|      972 | 10249 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10250 | `		/* Don't bother processing,it's invalid anyway */` |
|      481 | 10251 | `		if( pResult ){` |
|        - | 10252 | `			/* Assume a null return value */` |
|      ! 0 | 10253 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10254 | `		}` |
|      481 | 10255 | `		return SXERR_INVALID;` |
|        - | 10256 | `	}` |
|      492 | 10257 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 10258 | `		/* Class method */` |
|       11 | 10259 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 10260 | `		ph7_class_method *pMethod = 0;` |
|       11 | 10261 | `		ph7_class_instance *pThis = 0;` |
|       11 | 10262 | `		ph7_class *pClass = 0;` |
|        - | 10263 | `		ph7_value *pValue;` |
|        - | 10264 | `		sxi32 rc;` |
|       11 | 10265 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 10266 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 10267 | `			if( pResult ){` |
|        - | 10268 | `				/* Assume a null return value */` |
|      ! 0 | 10269 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10270 | `			}` |
|      ! 0 | 10271 | `			return SXRET_OK;` |
|        - | 10272 | `		}` |
|        - | 10273 | `		/* Extract the class name or an instance of it */` |
|       11 | 10274 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 10275 | `		if( pValue ){` |
|       11 | 10276 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 10277 | `		}` |
|       11 | 10278 | `		if( pClass == 0 ){` |
|        - | 10279 | `			/* No such class,return NULL */` |
|      ! 0 | 10280 | `			if( pResult ){` |
|      ! 0 | 10281 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10282 | `			}` |
|      ! 0 | 10283 | `			return SXRET_OK;` |
|        - | 10284 | `		}` |
|       11 | 10285 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10286 | `			/* Point to the class instance */` |
|        5 | 10287 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 10288 | `		}` |
|        - | 10289 | `		/* Try to extract the method */` |
|       11 | 10290 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 10291 | `		if( pValue ){` |
|       11 | 10292 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 10293 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 10294 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 10295 | `			}` |
|        5 | 10296 | `		}` |
|       11 | 10297 | `		if( pMethod == 0 ){` |
|        - | 10298 | `			/* No such method,return NULL */` |
|      ! 0 | 10299 | `			if( pResult ){` |
|      ! 0 | 10300 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10301 | `			}` |
|      ! 0 | 10302 | `			return SXRET_OK;` |
|        - | 10303 | `		}` |
|        - | 10304 | `		/* Call the class method */` |
|       11 | 10305 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 10306 | `		return rc;` |
|        - | 10307 | `	}` |
|        - | 10308 | `	/* Create a new operand stack */` |
|      482 | 10309 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      482 | 10310 | `	if( aStack == 0 ){` |
|      ! 0 | 10311 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10312 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 10313 | `		if( pResult ){` |
|        - | 10314 | `			/* Assume a null return value */` |
|      ! 0 | 10315 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10316 | `		}` |
|      ! 0 | 10317 | `		return SXERR_MEM;` |
|        - | 10318 | `	}` |
|        - | 10319 | `	/* Fill the operand stack with the given arguments */` |
|     1544 | 10320 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1064 | 10321 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 10322 | `		/*` |
|        - | 10323 | `		 * Symisc eXtension:` |
|        - | 10324 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 10325 | `		 */` |
|     1064 | 10326 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      533 | 10327 | `	}` |
|        - | 10328 | `	/* Push the function name */` |
|      482 | 10329 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      482 | 10330 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 10331 | `	/* Emit the CALL istruction */` |
|      482 | 10332 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      482 | 10333 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      482 | 10334 | `	aInstr[0].iP2 = 0;` |
|      482 | 10335 | `	aInstr[0].p3  = 0;` |
|        - | 10336 | `	/* Emit the DONE instruction */` |
|      482 | 10337 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      482 | 10338 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      482 | 10339 | `	aInstr[1].iP2 = 0;` |
|      482 | 10340 | `	aInstr[1].p3  = 0;` |
|        - | 10341 | `	/* Execute the function body (if available) */` |
|      482 | 10342 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - | 10343 | `	/* Clean up the mess left behind */` |
|      482 | 10344 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      482 | 10345 | `	return PH7_OK;` |
|      487 | 10346 |  |
|        - | 10347 | `/*` |
|        - | 10348 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 10349 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 10350 | ` * parameter.` |
|        - | 10351 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10352 | ` * return value indicates failure.` |
|        - | 10353 | ` */` |
|      236 | 10354 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 10355 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10356 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10357 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 10358 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 10359 | `	)` |
|        1 | 10360 |  |
|        - | 10361 | `	ph7_value *pArg;` |
|        - | 10362 | `	SySet aArg;` |
|        - | 10363 | `	va_list ap;` |
|        - | 10364 | `	sxi32 rc;` |
|      237 | 10365 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 10366 | `	/* Copy arguments one after one */` |
|      237 | 10367 | `	va_start(ap,pResult);` |
|      393 | 10368 | `	for(;;){` |
|      787 | 10369 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 10370 | `		if( pArg == 0 ){` |
|      237 | 10371 | `			break;` |
|        - | 10372 | `		}` |
|      551 | 10373 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 10374 | `	}` |
|        - | 10375 | `	/* Call the core routine */` |
|      237 | 10376 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 10377 | `	/* Cleanup */` |
|      237 | 10378 | `	SySetRelease(&aArg);` |
|      237 | 10379 | `	return rc;` |
|        1 | 10380 |  |
|        - | 10381 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 10382 | `/*` |
|        - | 10383 | ` * bool defined(string $name)` |
|        - | 10384 | ` *  Checks whether a given named constant exists.` |
|        - | 10385 | ` * Parameter:` |
|        - | 10386 | ` *  Name of the desired constant.` |
|        - | 10387 | ` * Return` |
|        - | 10388 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 10389 | ` */` |
|       14 | 10390 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10391 |  |
|        - | 10392 | `	const char *zName;` |
|       16 | 10393 | `	int nLen = 0;` |
|       16 | 10394 | `	int res = 0;` |
|       16 | 10395 | `	if( nArg < 1 ){` |
|        - | 10396 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 10397 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 10398 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10399 | `		return SXRET_OK;` |
|        - | 10400 | `	}` |
|        - | 10401 | `	/* Extract constant name */` |
|       16 | 10402 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10403 | `	/* Perform the lookup */` |
|       16 | 10404 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10405 | `		/* Already defined */` |
|       10 | 10406 | `		res = 1;` |
|        4 | 10407 | `	}` |
|       16 | 10408 | `	ph7_result_bool(pCtx,res);` |
|       16 | 10409 | `	return SXRET_OK;` |
|        9 | 10410 |  |
|        - | 10411 | `/*` |
|        - | 10412 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 10413 | ` * below.` |
|        - | 10414 | ` */` |
|       10 | 10415 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 10416 |  |
|       12 | 10417 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 10418 | `	/* Expand constant value */` |
|       12 | 10419 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 10420 |  |
|        - | 10421 | `/*` |
|        - | 10422 | ` * bool define(string $constant_name,expression value)` |
|        - | 10423 | ` *  Defines a named constant at runtime.` |
|        - | 10424 | ` * Parameter:` |
|        - | 10425 | ` *  $constant_name` |
|        - | 10426 | ` *   The name of the constant` |
|        - | 10427 | ` *  $value` |
|        - | 10428 | ` *   Constant value` |
|        - | 10429 | ` * Return:` |
|        - | 10430 | ` *   TRUE on success,FALSE on failure.` |
|        - | 10431 | ` */` |
|       12 | 10432 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10433 |  |
|        - | 10434 | `	const char *zName;  /* Constant name */` |
|        - | 10435 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 10436 | `	int nLen = 0;       /* Name length */` |
|        - | 10437 | `	sxi32 rc;` |
|       14 | 10438 | `	if( nArg < 2 ){` |
|        - | 10439 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 10440 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 10441 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10442 | `		return SXRET_OK;` |
|        - | 10443 | `	}` |
|       14 | 10444 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 10445 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 10446 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10447 | `		return SXRET_OK;` |
|        - | 10448 | `	}` |
|        - | 10449 | `	/* Extract constant name */` |
|       14 | 10450 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 10451 | `	if( nLen < 1 ){` |
|      ! 0 | 10452 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 10453 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10454 | `		return SXRET_OK;` |
|        - | 10455 | `	}` |
|        - | 10456 | `	/* Duplicate constant value */` |
|       14 | 10457 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 10458 | `	if( pValue == 0 ){` |
|      ! 0 | 10459 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10460 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10461 | `		return SXRET_OK;` |
|        - | 10462 | `	}` |
|        - | 10463 | `	/* Initialize the memory object */` |
|       14 | 10464 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 10465 | `	/* Register the constant */` |
|       14 | 10466 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 10467 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10468 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 10469 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10470 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10471 | `		return SXRET_OK;` |
|        - | 10472 | `	}` |
|        - | 10473 | `	/* Duplicate constant value */` |
|       14 | 10474 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 10475 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 10476 | `		/* Lower case the constant name */` |
|      ! 0 | 10477 | `		char *zCur = (char *)zName;` |
|      ! 0 | 10478 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 10479 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 10480 | `				/* UTF-8 stream */` |
|      ! 0 | 10481 | `				zCur++;` |
|      ! 0 | 10482 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 10483 | `					zCur++;` |
|      ! 0 | 10484 | `				}` |
|      ! 0 | 10485 | `				continue;` |
|        - | 10486 | `			}` |
|      ! 0 | 10487 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 10488 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 10489 | `				zCur[0] = (char)c;` |
|      ! 0 | 10490 | `			}` |
|      ! 0 | 10491 | `			zCur++;` |
|      ! 0 | 10492 | `		}` |
|        - | 10493 | `		/* Finally,register the constant */` |
|      ! 0 | 10494 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 10495 | `	}` |
|        - | 10496 | `	/* All done,return TRUE */` |
|       14 | 10497 | `	ph7_result_bool(pCtx,1);` |
|       14 | 10498 | `	return SXRET_OK;` |
|        8 | 10499 |  |
|        - | 10500 | `/*` |
|        - | 10501 | ` * value constant(string $name)` |
|        - | 10502 | ` *  Returns the value of a constant` |
|        - | 10503 | ` * Parameter` |
|        - | 10504 | ` *  $name` |
|        - | 10505 | ` *    Name of the constant.` |
|        - | 10506 | ` * Return` |
|        - | 10507 | ` *  Constant value or NULL if not defined.` |
|        - | 10508 | ` */` |
|        8 | 10509 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10510 |  |
|        - | 10511 | `	SyHashEntry *pEntry;` |
|        - | 10512 | `	ph7_constant *pCons;` |
|        - | 10513 | `	const char *zName; /* Constant name */` |
|        - | 10514 | `	ph7_value sVal;    /* Constant value */` |
|        - | 10515 | `	int nLen;` |
|       10 | 10516 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10517 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 10518 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 10519 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10520 | `		return SXRET_OK;` |
|        - | 10521 | `	}` |
|        - | 10522 | `	/* Extract the constant name */` |
|       10 | 10523 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10524 | `	/* Perform the query */` |
|       10 | 10525 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 10526 | `	if( pEntry == 0 ){` |
|        3 | 10527 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 10528 | `		ph7_result_null(pCtx);` |
|        3 | 10529 | `		return SXRET_OK;` |
|        - | 10530 | `	}` |
|        8 | 10531 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 10532 | `	/* Point to the structure that describe the constant */` |
|        8 | 10533 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 10534 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 10535 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 10536 | `	/* Return that value */` |
|        8 | 10537 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 10538 | `	/* Cleanup */` |
|        8 | 10539 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 10540 | `	return SXRET_OK;` |
|        6 | 10541 |  |
|        - | 10542 | `/*` |
|        - | 10543 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 10544 | ` * defined below.` |
|        - | 10545 | ` */` |
|      452 | 10546 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10547 |  |
|      453 | 10548 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10549 | `	ph7_value sName;` |
|        - | 10550 | `	sxi32 rc;` |
|        - | 10551 | `	/* Prepare the constant name for insertion */` |
|      453 | 10552 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 10553 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10554 | `	/* Perform the insertion */` |
|      453 | 10555 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 10556 | `	PH7_MemObjRelease(&sName);` |
|      453 | 10557 | `	return rc;` |
|        1 | 10558 |  |
|        - | 10559 | `/*` |
|        - | 10560 | ` * array get_defined_constants(void)` |
|        - | 10561 | ` *  Returns an associative array with the names of all defined` |
|        - | 10562 | ` *  constants.` |
|        - | 10563 | ` * Parameters` |
|        - | 10564 | ` *  NONE.` |
|        - | 10565 | ` * Returns` |
|        - | 10566 | ` *  Returns the names of all the constants currently defined.` |
|        - | 10567 | ` */` |
|        2 | 10568 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10569 |  |
|        - | 10570 | `	ph7_value *pArray;` |
|        - | 10571 | `	/* Create the array first*/` |
|        3 | 10572 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10573 | `	if( pArray == 0 ){` |
|      ! 0 | 10574 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10575 | `		SXUNUSED(apArg);` |
|        - | 10576 | `		/* Return NULL */` |
|      ! 0 | 10577 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10578 | `		return SXRET_OK;` |
|        - | 10579 | `	}` |
|        - | 10580 | `	/* Fill the array with the defined constants */` |
|        3 | 10581 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 10582 | `	/* Return the created array */` |
|        3 | 10583 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10584 | `	return SXRET_OK;` |
|        2 | 10585 |  |
|        - | 10586 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 10587 | `/*` |
|        - | 10588 | ` * Section:` |
|        - | 10589 | ` *  Random numbers/string generators.` |
|        - | 10590 | ` * Status:` |
|        - | 10591 | ` *    Stable.` |
|        - | 10592 | ` */` |
|        - | 10593 | `/*` |
|        - | 10594 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 10595 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 10596 | ` * used by te SQLite3 library.` |
|        - | 10597 | ` */` |
|     2684 | 10598 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 10599 |  |
|        - | 10600 | `	sxu32 iNum;` |
|     2686 | 10601 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2686 | 10602 | `	return iNum;` |
|        2 | 10603 |  |
|        - | 10604 | `/*` |
|        - | 10605 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 10606 | ` * Note that the generated string is NOT null terminated.` |
|        - | 10607 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 10608 | ` * by te SQLite3 library.` |
|        - | 10609 | ` */` |
|   190382 | 10610 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 10611 |  |
|        - | 10612 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 10613 | `	int i;` |
|        - | 10614 | `	/* Generate a binary string first */` |
|   190384 | 10615 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 10616 | `	/* Turn the binary string into english based alphabet */` |
|  2094372 | 10617 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1903990 | 10618 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   951996 | 10619 | `	 }` |
|   190384 | 10620 |  |
|        - | 10621 | `/*` |
|        - | 10622 | ` * int rand()` |
|        - | 10623 | ` * int mt_rand()` |
|        - | 10624 | ` * int rand(int $min,int $max)` |
|        - | 10625 | ` * int mt_rand(int $min,int $max)` |
|        - | 10626 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 10627 | ` * Parameter` |
|        - | 10628 | ` *  $min` |
|        - | 10629 | ` *    The lowest value to return (default: 0)` |
|        - | 10630 | ` *  $max` |
|        - | 10631 | ` *   The highest value to return (default: getrandmax())` |
|        - | 10632 | ` * Return` |
|        - | 10633 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 10634 | ` * Note:` |
|        - | 10635 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10636 | ` *  by te SQLite3 library.` |
|        - | 10637 | ` */` |
|       20 | 10638 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10639 |  |
|        - | 10640 | `	sxu32 iNum;` |
|        - | 10641 | `	/* Generate the random number */` |
|       21 | 10642 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 10643 | `	if( nArg > 1 ){` |
|        - | 10644 | `		sxu32 iMin,iMax;` |
|        3 | 10645 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 10646 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 10647 | `		if( iMin < iMax ){` |
|        3 | 10648 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 10649 | `			if( iDiv > 0 ){` |
|        3 | 10650 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 10651 | `			}` |
|        1 | 10652 | `		}else if(iMax > 0 ){` |
|      ! 0 | 10653 | `			iNum %= iMax;` |
|      ! 0 | 10654 | `		}` |
|        1 | 10655 | `	}` |
|        - | 10656 | `	/* Return the number */` |
|       21 | 10657 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 10658 | `	return SXRET_OK;` |
|        1 | 10659 |  |
|        - | 10660 | `/*` |
|        - | 10661 | ` * int getrandmax(void)` |
|        - | 10662 | ` * int mt_getrandmax(void)` |
|        - | 10663 | ` * int rc4_getrandmax(void)` |
|        - | 10664 | ` *   Show largest possible random value` |
|        - | 10665 | ` * Return` |
|        - | 10666 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 10667 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 10668 | ` * Note:` |
|        - | 10669 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10670 | ` *  by te SQLite3 library.` |
|        - | 10671 | ` */` |
|        4 | 10672 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10673 |  |
|        2 | 10674 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 10675 | `	SXUNUSED(apArg);` |
|        5 | 10676 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 10677 | `	return SXRET_OK;` |
|        1 | 10678 |  |
|        - | 10679 | `/*` |
|        - | 10680 | ` * string rand_str()` |
|        - | 10681 | ` * string rand_str(int $len)` |
|        - | 10682 | ` *  Generate a random string (English alphabet).` |
|        - | 10683 | ` * Parameter` |
|        - | 10684 | ` *  $len` |
|        - | 10685 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 10686 | ` * Return` |
|        - | 10687 | ` *   A pseudo random string.` |
|        - | 10688 | ` * Note:` |
|        - | 10689 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10690 | ` *  by te SQLite3 library.` |
|        - | 10691 | ` *  This function is a symisc extension.` |
|        - | 10692 | ` */` |
|      120 | 10693 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10694 |  |
|        - | 10695 | `	char zString[1024];` |
|      122 | 10696 | `	int iLen = 0x10;` |
|      122 | 10697 | `	if( nArg > 0 ){` |
|        - | 10698 | `		/* Get the desired length */` |
|      122 | 10699 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 10700 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 10701 | `			/* Default length */` |
|        3 | 10702 | `			iLen = 0x10;` |
|        1 | 10703 | `		}` |
|       60 | 10704 | `	}` |
|        - | 10705 | `	/* Generate the random string */` |
|      122 | 10706 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 10707 | `	/* Return the generated string */` |
|      122 | 10708 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 10709 | `	return SXRET_OK;` |
|        2 | 10710 |  |
|        - | 10711 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10712 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10713 | `/* Unique ID private data */` |
|        - | 10714 | `struct unique_id_data` |
|        - | 10715 |  |
|        - | 10716 | `	ph7_context *pCtx; /* Call context */` |
|        - | 10717 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 10718 | `};` |
|        - | 10719 | `/*` |
|        - | 10720 | ` * Binary to hex consumer callback.` |
|        - | 10721 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 10722 | ` * defined below.` |
|        - | 10723 | ` */` |
|      192 | 10724 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 10725 |  |
|      193 | 10726 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 10727 | `	sxu32 nBuflen;` |
|        - | 10728 | `	/* Extract result buffer length */` |
|      193 | 10729 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 10730 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 10731 | `			/*` |
|        - | 10732 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 10733 | `			 * string will be 13 characters long` |
|        - | 10734 | `			 */` |
|       25 | 10735 | `		return SXERR_ABORT;` |
|        - | 10736 | `	}` |
|      169 | 10737 | `	if( nBuflen > 22 ){` |
|      ! 0 | 10738 | `		return SXERR_ABORT;` |
|        - | 10739 | `	}` |
|        - | 10740 | `	/* Safely Consume the hex stream */` |
|      169 | 10741 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 10742 | `	return SXRET_OK;` |
|       97 | 10743 |  |
|        - | 10744 | `/*` |
|        - | 10745 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 10746 | ` *  Generate a unique ID` |
|        - | 10747 | ` * Parameter` |
|        - | 10748 | ` * $prefix` |
|        - | 10749 | ` *  Append this prefix to the generated unique ID.` |
|        - | 10750 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 10751 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 10752 | ` * $more_entropy` |
|        - | 10753 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 10754 | ` *  that the result will be unique.` |
|        - | 10755 | ` * Return` |
|        - | 10756 | ` *  Returns the unique identifier, as a string.` |
|        - | 10757 | ` */` |
|       24 | 10758 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10759 |  |
|        - | 10760 | `	struct unique_id_data sUniq;` |
|        - | 10761 | `	unsigned char zDigest[20];` |
|       25 | 10762 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10763 | `	const char *zPrefix;` |
|        - | 10764 | `	SHA1Context sCtx;` |
|        - | 10765 | `	char zRandom[7];` |
|        - | 10766 | `	int nPrefix;` |
|        - | 10767 | `	int entropy;` |
|        - | 10768 | `	/* Generate a random string first */` |
|       25 | 10769 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 10770 | `	/* Initialize fields */` |
|       25 | 10771 | `	zPrefix = 0;` |
|       25 | 10772 | `	nPrefix = 0;` |
|       25 | 10773 | `	entropy = 0;` |
|       25 | 10774 | `	if( nArg > 0 ){` |
|        - | 10775 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 10776 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 10777 | `		if( nArg > 1 ){` |
|      ! 0 | 10778 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 10779 | `		}` |
|      ! 0 | 10780 | `	}` |
|       25 | 10781 | `	SHA1Init(&sCtx);` |
|        - | 10782 | `	/* Generate the random ID */` |
|       25 | 10783 | `	if( nPrefix > 0 ){` |
|      ! 0 | 10784 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 10785 | `	}` |
|        - | 10786 | `	/* Append the random ID */` |
|       25 | 10787 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 10788 | `	/* Append the random string */` |
|       25 | 10789 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 10790 | `	/* Increment the number */` |
|       25 | 10791 | `	pVm->unique_id++;` |
|       25 | 10792 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 10793 | `	/* Hexify the digest */` |
|       25 | 10794 | `	sUniq.pCtx = pCtx;` |
|       25 | 10795 | `	sUniq.entropy = entropy;` |
|       25 | 10796 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 10797 | `	/* All done */` |
|       25 | 10798 | `	return PH7_OK;` |
|        1 | 10799 |  |
|        - | 10800 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10801 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10802 | `/*` |
|        - | 10803 | ` * Section:` |
|        - | 10804 | ` *  Language construct implementation as foreign functions.` |
|        - | 10805 | ` * Status:` |
|        - | 10806 | ` *    Stable.` |
|        - | 10807 | ` */` |
|        - | 10808 | `/*` |
|        - | 10809 | ` * void echo($string...)` |
|        - | 10810 | ` *  Output one or more messages.` |
|        - | 10811 | ` * Parameters` |
|        - | 10812 | ` *  $string` |
|        - | 10813 | ` *   Message to output.` |
|        - | 10814 | ` * Return` |
|        - | 10815 | ` *  NULL.` |
|        - | 10816 | ` */` |
|      ! 0 | 10817 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10818 |  |
|        - | 10819 | `	const char *zData;` |
|      ! 0 | 10820 | `	int nDataLen = 0;` |
|        - | 10821 | `	ph7_vm *pVm;` |
|        - | 10822 | `	int i,rc;` |
|        - | 10823 | `	/* Point to the target VM */` |
|      ! 0 | 10824 | `	pVm = pCtx->pVm;` |
|        - | 10825 | `	/* Output */` |
|      ! 0 | 10826 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 10827 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 10828 | `		if( nDataLen > 0 ){` |
|      ! 0 | 10829 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 10830 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 10831 | `			if( rc == SXERR_ABORT ){` |
|        - | 10832 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10833 | `				return PH7_ABORT;` |
|        - | 10834 | `			}` |
|      ! 0 | 10835 | `		}` |
|      ! 0 | 10836 | `	}` |
|      ! 0 | 10837 | `	return SXRET_OK;` |
|      ! 0 | 10838 |  |
|        - | 10839 | `/*` |
|        - | 10840 | ` * int print($string...)` |
|        - | 10841 | ` *  Output one or more messages.` |
|        - | 10842 | ` * Parameters` |
|        - | 10843 | ` *  $string` |
|        - | 10844 | ` *   Message to output.` |
|        - | 10845 | ` * Return` |
|        - | 10846 | ` *  1 always.` |
|        - | 10847 | ` */` |
|        2 | 10848 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10849 |  |
|        - | 10850 | `	const char *zData;` |
|        3 | 10851 | `	int nDataLen = 0;` |
|        - | 10852 | `	ph7_vm *pVm;` |
|        - | 10853 | `	int i,rc;` |
|        - | 10854 | `	/* Point to the target VM */` |
|        3 | 10855 | `	pVm = pCtx->pVm;` |
|        - | 10856 | `	/* Output */` |
|        5 | 10857 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 10858 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 10859 | `		if( nDataLen > 0 ){` |
|        3 | 10860 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 10861 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 10862 | `			if( rc == SXERR_ABORT ){` |
|        - | 10863 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10864 | `				return PH7_ABORT;` |
|        - | 10865 | `			}` |
|        1 | 10866 | `		}` |
|        2 | 10867 | `	}` |
|        - | 10868 | `	/* Return 1 */` |
|        3 | 10869 | `	ph7_result_int(pCtx,1);` |
|        3 | 10870 | `	return SXRET_OK;` |
|        2 | 10871 |  |
|        - | 10872 | `/*` |
|        - | 10873 | ` * void exit(string $msg)` |
|        - | 10874 | ` * void exit(int $status)` |
|        - | 10875 | ` * void die(string $ms)` |
|        - | 10876 | ` * void die(int $status)` |
|        - | 10877 | ` *   Output a message and terminate program execution.` |
|        - | 10878 | ` * Parameter` |
|        - | 10879 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 10880 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 10881 | ` *  and not printed` |
|        - | 10882 | ` * Return` |
|        - | 10883 | ` *  NULL` |
|        - | 10884 | ` */` |
|      ! 0 | 10885 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10886 |  |
|      ! 0 | 10887 | `	if( nArg > 0 ){` |
|      ! 0 | 10888 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 10889 | `			const char *zData;` |
|      ! 0 | 10890 | `			int iLen = 0;` |
|        - | 10891 | `			/* Print exit message */` |
|      ! 0 | 10892 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 10893 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 10894 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 10895 | `			sxi32 iExitStatus;` |
|        - | 10896 | `			/* Record exit status code */` |
|      ! 0 | 10897 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 10898 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 10899 | `		}` |
|      ! 0 | 10900 | `	}` |
|        - | 10901 | `	/* Check if we are in an included file */` |
|      ! 0 | 10902 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 10903 | `		/* Exit the entire process */` |
|      ! 0 | 10904 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 10905 | `	}` |
|        - | 10906 | `	/* Abort processing immediately */` |
|      ! 0 | 10907 | `	return PH7_ABORT;` |
|      ! 0 | 10908 |  |
|        - | 10909 | `/*` |
|        - | 10910 | ` * bool isset($var,...)` |
|        - | 10911 | ` *  Finds out whether a variable is set.` |
|        - | 10912 | ` * Parameters` |
|        - | 10913 | ` *  One or more variable to check.` |
|        - | 10914 | ` * Return` |
|        - | 10915 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 10916 | ` */` |
|    85306 | 10917 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10918 |  |
|        - | 10919 | `	ph7_value *pObj;` |
|    85308 | 10920 | `	int res = 0;` |
|        - | 10921 | `	int i;` |
|    85308 | 10922 | `	if( nArg < 1 ){` |
|        - | 10923 | `		/* Missing arguments,return false */` |
|      ! 0 | 10924 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 10925 | `		return SXRET_OK;` |
|        - | 10926 | `	}` |
|        - | 10927 | `	/* Iterate over available arguments */` |
|   111800 | 10928 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    85308 | 10929 | `		pObj = apArg[i];` |
|    85308 | 10930 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    58052 | 10931 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10932 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 10933 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 10934 | `			}` |
|    29025 | 10935 | `		}` |
|    85308 | 10936 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    85308 | 10937 | `		if( !res ){` |
|        - | 10938 | `			/* Variable not set,return FALSE */` |
|    58816 | 10939 | `			ph7_result_bool(pCtx,0);` |
|    58816 | 10940 | `			return SXRET_OK;` |
|        - | 10941 | `		}` |
|    13248 | 10942 | `	}` |
|        - | 10943 | `	/* All given variable are set,return TRUE */` |
|    26494 | 10944 | `	ph7_result_bool(pCtx,1);` |
|    26494 | 10945 | `	return SXRET_OK;` |
|    42655 | 10946 |  |
|        - | 10947 | `/*` |
|        - | 10948 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 10949 | ` * frame,the reference table and discard it's contents.` |
|        - | 10950 | ` * This function never fail and always return SXRET_OK.` |
|        - | 10951 | ` */` |
|  3080304 | 10952 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 10953 |  |
|        - | 10954 | `	ph7_value *pObj;` |
|        - | 10955 | `	VmRefObj *pRef;` |
|  3080306 | 10956 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3080306 | 10957 | `	if( pObj ){` |
|        - | 10958 | `		/* Release the object */` |
|  3080306 | 10959 | `		PH7_MemObjRelease(pObj);` |
|  1540152 | 10960 | `	}` |
|        - | 10961 | `	/* Remove old reference links */` |
|  3080306 | 10962 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3080306 | 10963 | `	if( pRef ){` |
|  3080300 | 10964 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 10965 | `		/* Unlink from the reference table */` |
|  3080300 | 10966 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3080300 | 10967 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 10968 | `			VmSlot sFree;` |
|        - | 10969 | `			/* Restore to the free list */` |
|  3080292 | 10970 | `			sFree.nIdx = nObjIdx;` |
|  3080292 | 10971 | `			sFree.pUserData = 0;` |
|  3080292 | 10972 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1540145 | 10973 | `		}` |
|  1540149 | 10974 | `	}` |
|  3080306 | 10975 | `	return SXRET_OK;` |
|        2 | 10976 |  |
|        - | 10977 | `/*` |
|        - | 10978 | ` * void unset($var,...)` |
|        - | 10979 | ` *   Unset one or more given variable.` |
|        - | 10980 | ` * Parameters` |
|        - | 10981 | ` *  One or more variable to unset.` |
|        - | 10982 | ` * Return` |
|        - | 10983 | ` *  Nothing.` |
|        - | 10984 | ` */` |
|     7290 | 10985 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10986 |  |
|        - | 10987 | `	ph7_value *pObj;` |
|        - | 10988 | `	ph7_vm *pVm;` |
|        - | 10989 | `	int i;` |
|        - | 10990 | `	/* Point to the target VM */` |
|     7292 | 10991 | `	pVm = pCtx->pVm;` |
|        - | 10992 | `	/* Iterate and unset */` |
|    14582 | 10993 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7292 | 10994 | `		pObj = apArg[i];` |
|     7292 | 10995 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 10996 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10997 | `				/* Throw an error */` |
|      ! 0 | 10998 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 10999 | `			}` |
|      ! 0 | 11000 | `		}else{` |
|     7292 | 11001 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 11002 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7292 | 11003 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7286 | 11004 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3642 | 11005 | `			}` |
|        - | 11006 | `		}` |
|     3647 | 11007 | `	}` |
|     7292 | 11008 | `	return SXRET_OK;` |
|        2 | 11009 |  |
|        - | 11010 | `/*` |
|        - | 11011 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 11012 | ` */` |
|      110 | 11013 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11014 |  |
|      111 | 11015 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 11016 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11017 | `	ph7_value *pObj;` |
|        - | 11018 | `	sxu32 nIdx;` |
|        - | 11019 | `	/* Extract the memory object */` |
|      111 | 11020 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 11021 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 11022 | `	if( pObj ){` |
|      111 | 11023 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 11024 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 11025 | `				SyString sName;` |
|        - | 11026 | `				ph7_value sKey;` |
|        - | 11027 | `				/* Perform the insertion */` |
|      109 | 11028 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 11029 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 11030 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 11031 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 11032 | `			}` |
|       54 | 11033 | `		}` |
|       55 | 11034 | `	}` |
|      111 | 11035 | `	return SXRET_OK;` |
|        1 | 11036 |  |
|        - | 11037 | `/*` |
|        - | 11038 | ` * array get_defined_vars(void)` |
|        - | 11039 | ` *  Returns an array of all defined variables.` |
|        - | 11040 | ` * Parameter` |
|        - | 11041 | ` *  None` |
|        - | 11042 | ` * Return` |
|        - | 11043 | ` *  An array with all the variables defined in the current scope.` |
|        - | 11044 | ` */` |
|        2 | 11045 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11046 |  |
|        3 | 11047 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11048 | `	ph7_value *pArray;` |
|        - | 11049 | `	/* Create a new array */` |
|        3 | 11050 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11051 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11052 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11053 | `		SXUNUSED(apArg);` |
|        - | 11054 | `		/* Return NULL */` |
|      ! 0 | 11055 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11056 | `		return SXRET_OK;` |
|        - | 11057 | `	}` |
|        - | 11058 | `	/* Superglobals first */` |
|        3 | 11059 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 11060 | `	/* Then variable defined in the current frame */` |
|        3 | 11061 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 11062 | `	/* Finally,return the created array */` |
|        3 | 11063 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11064 | `	return SXRET_OK;` |
|        2 | 11065 |  |
|        - | 11066 | `/*` |
|        - | 11067 | ` * bool gettype($var)` |
|        - | 11068 | ` *  Get the type of a variable` |
|        - | 11069 | ` * Parameters` |
|        - | 11070 | ` *   $var` |
|        - | 11071 | ` *    The variable being type checked.` |
|        - | 11072 | ` * Return` |
|        - | 11073 | ` *   String representation of the given variable type.` |
|        - | 11074 | ` */` |
|       32 | 11075 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11076 |  |
|       34 | 11077 | `	const char *zType = "Empty";` |
|       34 | 11078 | `	if( nArg > 0 ){` |
|       34 | 11079 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 11080 | `	}` |
|        - | 11081 | `	/* Return the variable type */` |
|       34 | 11082 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 11083 | `	return SXRET_OK;` |
|        2 | 11084 |  |
|        - | 11085 | `/*` |
|        - | 11086 | ` * string get_resource_type(resource $handle)` |
|        - | 11087 | ` *  This function gets the type of the given resource.` |
|        - | 11088 | ` * Parameters` |
|        - | 11089 | ` *  $handle` |
|        - | 11090 | ` *  The evaluated resource handle.` |
|        - | 11091 | ` * Return` |
|        - | 11092 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 11093 | ` *  representing its type. If the type is not identified by this function` |
|        - | 11094 | ` *  the return value will be the string Unknown.` |
|        - | 11095 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 11096 | ` *  is not a resource.` |
|        - | 11097 | ` */` |
|        2 | 11098 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11099 |  |
|        3 | 11100 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 11101 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 11102 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11103 | `		return PH7_OK;` |
|        - | 11104 | `	}` |
|        3 | 11105 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 11106 | `	return SXRET_OK;` |
|        2 | 11107 |  |
|        - | 11108 | `/*` |
|        - | 11109 | ` * void var_dump(expression,....)` |
|        - | 11110 | ` *   var_dump � Dumps information about a variable` |
|        - | 11111 | ` * Parameters` |
|        - | 11112 | ` *   One or more expression to dump.` |
|        - | 11113 | ` * Returns` |
|        - | 11114 | ` *  Nothing.` |
|        - | 11115 | ` */` |
|      218 | 11116 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11117 |  |
|        - | 11118 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 11119 | `	int i;` |
|      220 | 11120 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 11121 | `	/* Dump one or more expressions */` |
|      444 | 11122 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 11123 | `		ph7_value *pObj = apArg[i];` |
|        - | 11124 | `		/* Reset the working buffer */` |
|      226 | 11125 | `		SyBlobReset(&sDump);` |
|        - | 11126 | `		/* Dump the given expression */` |
|      226 | 11127 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 11128 | `		/* Output */` |
|      226 | 11129 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 11130 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 11131 | `		}` |
|      114 | 11132 | `	}` |
|        - | 11133 | `	/* Release the working buffer */` |
|      220 | 11134 | `	SyBlobRelease(&sDump);` |
|      220 | 11135 | `	return SXRET_OK;` |
|        2 | 11136 |  |
|        - | 11137 | `/*` |
|        - | 11138 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 11139 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 11140 | ` * Parameters` |
|        - | 11141 | ` *   expression: Expression to dump` |
|        - | 11142 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 11143 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 11144 | ` *            print_r() will return the information rather than print it.` |
|        - | 11145 | ` * Return` |
|        - | 11146 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 11147 | ` *  Otherwise, the return value is TRUE.` |
|        - | 11148 | ` */` |
|       16 | 11149 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11150 |  |
|       17 | 11151 | `	int ret_string = 0;` |
|        - | 11152 | `	SyBlob sDump;` |
|       17 | 11153 | `	if( nArg < 1 ){` |
|        - | 11154 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11155 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11156 | `		return SXRET_OK;` |
|        - | 11157 | `	}` |
|       17 | 11158 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 11159 | `	if ( nArg > 1 ){` |
|        - | 11160 | `		/* Where to redirect output */` |
|       11 | 11161 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 11162 | `	}` |
|        - | 11163 | `	/* Generate dump */` |
|       17 | 11164 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 11165 | `	if( !ret_string ){` |
|        - | 11166 | `		/* Output dump */` |
|        7 | 11167 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11168 | `		/* Return true */` |
|        7 | 11169 | `		ph7_result_bool(pCtx,1);` |
|        4 | 11170 | `	}else{` |
|        - | 11171 | `		/* Generated dump as return value */` |
|       11 | 11172 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11173 | `	}` |
|        - | 11174 | `	/* Release the working buffer */` |
|       17 | 11175 | `	SyBlobRelease(&sDump);` |
|       17 | 11176 | `	return SXRET_OK;` |
|        9 | 11177 |  |
|        - | 11178 | `/*` |
|        - | 11179 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 11180 | ` * Same job as print_r. (see coment above)` |
|        - | 11181 | ` */` |
|        2 | 11182 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11183 |  |
|        3 | 11184 | `	int ret_string = 0;` |
|        - | 11185 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 11186 | `	if( nArg < 1 ){` |
|        - | 11187 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11188 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11189 | `		return SXRET_OK;` |
|        - | 11190 | `	}` |
|        3 | 11191 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 11192 | `	if ( nArg > 1 ){` |
|        - | 11193 | `		/* Where to redirect output */` |
|        3 | 11194 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 11195 | `	}` |
|        - | 11196 | `	/* Generate dump */` |
|        3 | 11197 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 11198 | `	if( !ret_string ){` |
|        - | 11199 | `		/* Output dump */` |
|      ! 0 | 11200 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11201 | `		/* Return NULL */` |
|      ! 0 | 11202 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11203 | `	}else{` |
|        - | 11204 | `		/* Generated dump as return value */` |
|        3 | 11205 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11206 | `	}` |
|        - | 11207 | `	/* Release the working buffer */` |
|        3 | 11208 | `	SyBlobRelease(&sDump);` |
|        3 | 11209 | `	return SXRET_OK;` |
|        2 | 11210 |  |
|        - | 11211 | `/*` |
|        - | 11212 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 11213 | ` *  Set/get the various assert flags.` |
|        - | 11214 | ` * Parameter` |
|        - | 11215 | ` * $what` |
|        - | 11216 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 11217 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 11218 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 11219 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 11220 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 11221 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 11222 | ` * $value` |
|        - | 11223 | ` *   An optional new value for the option.` |
|        - | 11224 | ` * Return` |
|        - | 11225 | ` *  Old setting on success or FALSE on failure.` |
|        - | 11226 | ` */` |
|       28 | 11227 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11228 |  |
|       30 | 11229 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11230 | `	int iOption;` |
|        - | 11231 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 11232 | `	if( nArg < 1 ){` |
|        3 | 11233 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11234 | `			"ArgumentCountError",` |
|        - | 11235 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 11236 | `			);` |
|        - | 11237 | `	}` |
|        - | 11238 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 11239 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 11240 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 11241 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11242 | `			"TypeError",` |
|        - | 11243 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 11244 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 11245 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 11246 | `			);` |
|        - | 11247 | `	}` |
|       28 | 11248 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 11249 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 11250 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 11251 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 11252 | `	switch( iOption ){` |
|        5 | 11253 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 11254 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 11255 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 11256 | `		if( nArg > 1 ){` |
|        5 | 11257 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11258 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 11259 | `			}else{` |
|        3 | 11260 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 11261 | `			}` |
|        2 | 11262 | `		}` |
|       12 | 11263 | `		break;` |
|        1 | 11264 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 11265 | `		/* Return old callback or null */` |
|        3 | 11266 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 11267 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 11268 | `		}else{` |
|        3 | 11269 | `			ph7_result_null(pCtx);` |
|        - | 11270 | `		}` |
|        3 | 11271 | `		if( nArg > 1 ){` |
|      ! 0 | 11272 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 11273 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 11274 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 11275 | `			}else{` |
|      ! 0 | 11276 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 11277 | `			}` |
|      ! 0 | 11278 | `		}` |
|        3 | 11279 | `		break;` |
|        5 | 11280 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 11281 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 11282 | `		if( nArg > 1 ){` |
|        5 | 11283 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11284 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 11285 | `			}else{` |
|        3 | 11286 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 11287 | `			}` |
|        2 | 11288 | `		}` |
|       11 | 11289 | `		break;` |
|      ! 0 | 11290 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 11291 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11292 | `		break;` |
|        1 | 11293 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 11294 | `		ph7_result_int(pCtx, 1);` |
|        3 | 11295 | `		break;` |
|      ! 0 | 11296 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 11297 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11298 | `		break;` |
|        1 | 11299 | `	default:` |
|        - | 11300 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 11301 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11302 | `			"ValueError",` |
|        - | 11303 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 11304 | `			);` |
|        - | 11305 | `	}` |
|       26 | 11306 | `	return PH7_OK;` |
|       16 | 11307 |  |
|        - | 11308 | `/*` |
|        - | 11309 | ` * bool assert(mixed $assertion)` |
|        - | 11310 | ` *  Checks if assertion is FALSE.` |
|        - | 11311 | ` * Parameter` |
|        - | 11312 | ` *  $assertion` |
|        - | 11313 | ` *    The assertion to test.` |
|        - | 11314 | ` * Return` |
|        - | 11315 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 11316 | ` */` |
|       24 | 11317 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11318 |  |
|       26 | 11319 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11320 | `	int iFlags,iResult;` |
|        - | 11321 | `	const char *zDesc;` |
|        - | 11322 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 11323 | `	if( nArg < 1 ){` |
|        3 | 11324 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11325 | `			"ArgumentCountError",` |
|        - | 11326 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 11327 | `			);` |
|        - | 11328 | `	}` |
|       24 | 11329 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 11330 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 11331 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 11332 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 11333 | `		return PH7_OK;` |
|        - | 11334 | `	}` |
|        - | 11335 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 11336 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 11337 | `	if( !iResult ){` |
|        - | 11338 | `		/* Assertion failed */` |
|        - | 11339 | `		/* Extract optional description */` |
|       13 | 11340 | `		zDesc = 0;` |
|       13 | 11341 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11342 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 11343 | `		}` |
|       13 | 11344 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 11345 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 11346 | `			ph7_value sFile,sLine;` |
|        - | 11347 | `			ph7_value *apCbArg[3];` |
|        - | 11348 | `			SyString *pFile;` |
|        - | 11349 | `			/* Extract the processed script */` |
|      ! 0 | 11350 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 11351 | `			if( pFile == 0 ){` |
|      ! 0 | 11352 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 11353 | `			}` |
|        - | 11354 | `			/* Invoke the callback */` |
|      ! 0 | 11355 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 11356 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 11357 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 11358 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 11359 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 11360 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 11361 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 11362 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 11363 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 11364 | `		}` |
|       13 | 11365 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 11366 | `			/* Abort VM execution immediately */` |
|      ! 0 | 11367 | `			return PH7_ABORT;` |
|        - | 11368 | `		}` |
|        - | 11369 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 11370 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 11371 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11372 | `				"AssertionError",` |
|        - | 11373 | `				"%s",` |
|        1 | 11374 | `				zDesc` |
|        - | 11375 | `				);` |
|      ! 0 | 11376 | `		}else{` |
|       11 | 11377 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11378 | `				"AssertionError",` |
|        - | 11379 | `				"assert(false)"` |
|        - | 11380 | `				);` |
|        - | 11381 | `		}` |
|        - | 11382 | `	}` |
|        - | 11383 | `	/* Assertion passed */` |
|       11 | 11384 | `	ph7_result_bool(pCtx,1);` |
|       11 | 11385 | `	return PH7_OK;` |
|       14 | 11386 |  |
|        - | 11387 | `/*` |
|        - | 11388 | ` * Section:` |
|        - | 11389 | ` *  Error reporting functions.` |
|        - | 11390 | ` * Status:` |
|        - | 11391 | ` *    Stable.` |
|        - | 11392 | ` */` |
|        - | 11393 | `/*` |
|        - | 11394 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 11395 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 11396 | ` * Parameters` |
|        - | 11397 | ` *  $error_msg` |
|        - | 11398 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 11399 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 11400 | ` * $error_type` |
|        - | 11401 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 11402 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 11403 | ` * Return` |
|        - | 11404 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 11405 | ` */` |
|       12 | 11406 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11407 |  |
|       14 | 11408 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 11409 | `	int rc = PH7_OK;` |
|       14 | 11410 | `	if( nArg > 0 ){` |
|        - | 11411 | `		const char *zErr;` |
|        - | 11412 | `		int nLen;` |
|        - | 11413 | `		/* Extract the error message */` |
|       12 | 11414 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 11415 | `		if( nArg > 1 ){` |
|        - | 11416 | `			/* Extract the error type */` |
|       12 | 11417 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 11418 | `			switch( nErr ){` |
|        1 | 11419 | `			case 1:   /* E_ERROR */` |
|        - | 11420 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 11421 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 11422 | `			case 256: /* E_USER_ERROR */` |
|        3 | 11423 | `				nErr = PH7_CTX_ERR;` |
|        3 | 11424 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 11425 | `				break;` |
|        1 | 11426 | `			case 2:   /* E_WARNING */` |
|        - | 11427 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 11428 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 11429 | `			case 512: /* E_USER_WARNING */` |
|        3 | 11430 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 11431 | `				break;` |
|        3 | 11432 | `			default:` |
|        8 | 11433 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 11434 | `				break;` |
|        - | 11435 | `			}` |
|        5 | 11436 | `		}` |
|        - | 11437 | `		/* Report error */` |
|       12 | 11438 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 11439 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 11440 | `			return rc;` |
|        - | 11441 | `		}` |
|        - | 11442 | `		/* Return true */` |
|       12 | 11443 | `		ph7_result_bool(pCtx,1);` |
|        7 | 11444 | `	}else{` |
|        - | 11445 | `		/* Missing arguments,return FALSE */` |
|        3 | 11446 | `		ph7_result_bool(pCtx,0);` |
|        - | 11447 | `	}` |
|       14 | 11448 | `	return rc;` |
|        8 | 11449 |  |
|        - | 11450 | `/*` |
|        - | 11451 | ` * int error_reporting([int $level])` |
|        - | 11452 | ` *  Sets which PHP errors are reported.` |
|        - | 11453 | ` * Parameters` |
|        - | 11454 | ` *  $level` |
|        - | 11455 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 11456 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 11457 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 11458 | ` *   levels will not always behave as expected.` |
|        - | 11459 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 11460 | ` *   in the predefined constants.` |
|        - | 11461 | ` * Return` |
|        - | 11462 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 11463 | ` *   parameter is given.` |
|        - | 11464 | ` */` |
|       38 | 11465 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11466 |  |
|       40 | 11467 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11468 | `	int nOld;` |
|        - | 11469 | `	/* Extract the old reporting level */` |
|       40 | 11470 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 11471 | `	if( nArg > 0 ){` |
|        - | 11472 | `		int nNew;` |
|        - | 11473 | `		/* Extract the desired error reporting level */` |
|       32 | 11474 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 11475 | `		if( !nNew ){` |
|        - | 11476 | `			/* Do not report errors at all */` |
|        5 | 11477 | `			pVm->bErrReport = 0;` |
|        3 | 11478 | `		}else{` |
|        - | 11479 | `			/* Report all errors */` |
|       28 | 11480 | `			pVm->bErrReport = 1;` |
|        - | 11481 | `		}` |
|       15 | 11482 | `	}` |
|        - | 11483 | `	/* Return the old level */` |
|       40 | 11484 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 11485 | `	return PH7_OK;` |
|        2 | 11486 |  |
|        - | 11487 | `/*` |
|        - | 11488 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 11489 | ` *  Send an error message somewhere.` |
|        - | 11490 | ` * Parameter` |
|        - | 11491 | ` *  $message` |
|        - | 11492 | ` *   The error message that should be logged.` |
|        - | 11493 | ` *  $message_type` |
|        - | 11494 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 11495 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 11496 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 11497 | ` *       This is the default option.` |
|        - | 11498 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 11499 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 11500 | ` *    2  No longer an option.` |
|        - | 11501 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 11502 | ` *       to the end of the message string.` |
|        - | 11503 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 11504 | ` *  $destination` |
|        - | 11505 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 11506 | ` *  $extra_headers` |
|        - | 11507 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 11508 | ` * Return` |
|        - | 11509 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11510 | ` * NOTE:` |
|        - | 11511 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 11512 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 11513 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 11514 | ` *  Otherwise this function is no-op.` |
|        - | 11515 | ` */` |
|        4 | 11516 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11517 |  |
|        - | 11518 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 11519 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 11520 | `	int iType = 0;` |
|        5 | 11521 | `	if( nArg < 1 ){` |
|        - | 11522 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 11523 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11524 | `		return PH7_OK;` |
|        - | 11525 | `	}` |
|        5 | 11526 | `	if( pVm->xErrLog  ){` |
|        - | 11527 | `		/* Invoke the user callback */` |
|      ! 0 | 11528 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 11529 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 11530 | `		if( nArg > 1 ){` |
|      ! 0 | 11531 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11532 | `			if( nArg > 2 ){` |
|      ! 0 | 11533 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 11534 | `				if( nArg > 3 ){` |
|      ! 0 | 11535 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 11536 | `				}` |
|      ! 0 | 11537 | `			}` |
|      ! 0 | 11538 | `		}` |
|      ! 0 | 11539 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 11540 | `	}` |
|        - | 11541 | `	/* Retun TRUE */` |
|        5 | 11542 | `	ph7_result_bool(pCtx,1);` |
|        5 | 11543 | `	return PH7_OK;` |
|        3 | 11544 |  |
|        - | 11545 | `/*` |
|        - | 11546 | ` * bool restore_exception_handler(void)` |
|        - | 11547 | ` *  Restores the previously defined exception handler function.` |
|        - | 11548 | ` * Parameter` |
|        - | 11549 | ` *  None` |
|        - | 11550 | ` * Return` |
|        - | 11551 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 11552 | ` */` |
|        4 | 11553 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11554 |  |
|        5 | 11555 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11556 | `	ph7_value *pOld,*pNew;` |
|        - | 11557 | `	/* Point to the old and the new handler */` |
|        5 | 11558 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 11559 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 11560 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 11561 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 11562 | `		SXUNUSED(apArg);` |
|        - | 11563 | `		/* No installed handler,return FALSE */` |
|        5 | 11564 | `		ph7_result_bool(pCtx,0);` |
|        5 | 11565 | `		return PH7_OK;` |
|        - | 11566 | `	}` |
|        - | 11567 | `	/* Copy the old handler */` |
|      ! 0 | 11568 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11569 | `	PH7_MemObjRelease(pOld);` |
|        - | 11570 | `	/* Return TRUE */` |
|      ! 0 | 11571 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11572 | `	return PH7_OK;` |
|        3 | 11573 |  |
|        - | 11574 | `/*` |
|        - | 11575 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 11576 | ` *  Sets a user-defined exception handler function.` |
|        - | 11577 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 11578 | ` * NOTE` |
|        - | 11579 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 11580 | ` *  the satndard PHP engine.` |
|        - | 11581 | ` * Parameters` |
|        - | 11582 | ` *  $exception_handler` |
|        - | 11583 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 11584 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 11585 | ` *   that was thrown.` |
|        - | 11586 | ` *  Note:` |
|        - | 11587 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11588 | ` * Return` |
|        - | 11589 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 11590 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11591 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11592 | ` */` |
|        4 | 11593 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11594 |  |
|        6 | 11595 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11596 | `	ph7_value *pOld,*pNew;` |
|        - | 11597 | `	/* Point to the old and the new handler */` |
|        6 | 11598 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 11599 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 11600 | `	/* Return the old handler */` |
|        6 | 11601 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 11602 | `	if( nArg > 0 ){` |
|        6 | 11603 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11604 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 11605 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 11606 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 11607 | `		}else{` |
|        6 | 11608 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11609 | `			/* Install the new handler */` |
|        6 | 11610 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11611 | `		}` |
|        2 | 11612 | `	}` |
|        6 | 11613 | `	return PH7_OK;` |
|        2 | 11614 |  |
|        - | 11615 | `/*` |
|        - | 11616 | ` * bool restore_error_handler(void)` |
|        - | 11617 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11618 | ` * Parameters:` |
|        - | 11619 | ` *  None.` |
|        - | 11620 | ` * Return` |
|        - | 11621 | ` *  Always TRUE.` |
|        - | 11622 | ` */` |
|        6 | 11623 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11624 |  |
|        7 | 11625 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11626 | `	ph7_value *pOld,*pNew;` |
|        - | 11627 | `	/* Point to the old and the new handler */` |
|        7 | 11628 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 11629 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 11630 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 11631 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 11632 | `		SXUNUSED(apArg);` |
|        - | 11633 | `		/* No installed callback,return FALSE */` |
|        7 | 11634 | `		ph7_result_bool(pCtx,0);` |
|        7 | 11635 | `		return PH7_OK;` |
|        - | 11636 | `	}` |
|        - | 11637 | `	/* Copy the old callback */` |
|      ! 0 | 11638 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11639 | `	PH7_MemObjRelease(pOld);` |
|        - | 11640 | `	/* Return TRUE */` |
|      ! 0 | 11641 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11642 | `	return PH7_OK;` |
|        4 | 11643 |  |
|        - | 11644 | `/*` |
|        - | 11645 | ` * value set_error_handler(callable $error_handler)` |
|        - | 11646 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11647 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11648 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11649 | ` *  Sets a user-defined error handler function.` |
|        - | 11650 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 11651 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 11652 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 11653 | ` *  conditions (using trigger_error()).` |
|        - | 11654 | ` * Parameters` |
|        - | 11655 | ` *  $error_handler` |
|        - | 11656 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 11657 | ` *   describing the error.` |
|        - | 11658 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 11659 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 11660 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 11661 | ` *   The function can be shown as:` |
|        - | 11662 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 11663 | ` *     errno` |
|        - | 11664 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 11665 | ` *   errstr` |
|        - | 11666 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 11667 | ` *   errfile` |
|        - | 11668 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 11669 | ` *     was raised in, as a string.` |
|        - | 11670 | ` *  Note:` |
|        - | 11671 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11672 | ` * Return` |
|        - | 11673 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 11674 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11675 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11676 | ` */` |
|    10104 | 11677 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11678 |  |
|    10106 | 11679 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11680 | `	ph7_value *pOld,*pNew;` |
|        - | 11681 | `	/* Point to the old and the new handler */` |
|    10106 | 11682 | `	pOld = &pVm->aErrCB[0];` |
|    10106 | 11683 | `	pNew = &pVm->aErrCB[1];` |
|        - | 11684 | `	/* Return the old handler */` |
|    10106 | 11685 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10106 | 11686 | `	if( nArg > 0 ){` |
|    10106 | 11687 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11688 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5051 | 11689 | `			PH7_MemObjRelease(pNew);` |
|     5051 | 11690 | `			ph7_result_bool(pCtx,1);` |
|     2526 | 11691 | `		}else{` |
|     5056 | 11692 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11693 | `			/* Install the new handler */` |
|     5056 | 11694 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11695 | `		}` |
|     5052 | 11696 | `	}` |
|    10106 | 11697 | `	return PH7_OK;` |
|        2 | 11698 |  |
|        - | 11699 | `/*` |
|        - | 11700 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 11701 | ` *  Generates a backtrace.` |
|        - | 11702 | ` * Paramaeter` |
|        - | 11703 | ` *  $options` |
|        - | 11704 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 11705 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 11706 | ` *   all the function/method arguments, to save memory.` |
|        - | 11707 | ` * $limit` |
|        - | 11708 | ` *   (Not Used)` |
|        - | 11709 | ` * Return` |
|        - | 11710 | ` *  An array.The possible returned elements are as follows:` |
|        - | 11711 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 11712 | ` *          Name        Type      Description` |
|        - | 11713 | ` *          ------      ------     -----------` |
|        - | 11714 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 11715 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 11716 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 11717 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 11718 | ` *          object      object    The current object.` |
|        - | 11719 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 11720 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 11721 | ` */` |
|      724 | 11722 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11723 |  |
|      726 | 11724 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11725 | `	ph7_value *pArray;` |
|        - | 11726 | `	ph7_class *pClass;` |
|        - | 11727 | `	ph7_value *pValue;` |
|        - | 11728 | `	SyString *pFile;` |
|        - | 11729 | `	/* Create a new array */` |
|      726 | 11730 | `	pArray = ph7_context_new_array(pCtx);` |
|      726 | 11731 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      726 | 11732 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11733 | `		/* Out of memory,return NULL */` |
|      ! 0 | 11734 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11735 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11736 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11737 | `		SXUNUSED(apArg);` |
|      ! 0 | 11738 | `		return PH7_OK;` |
|        - | 11739 | `	}` |
|        - | 11740 | `	/* Dump running function name and it's arguments  */` |
|      726 | 11741 | `	if( pVm->pFrame->pParent ){` |
|      726 | 11742 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 11743 | `		ph7_vm_func *pFunc;` |
|        - | 11744 | `		ph7_value *pArg;` |
|      726 | 11745 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      726 | 11746 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      726 | 11747 | `		if( pFrame->pParent && pFunc ){` |
|      726 | 11748 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      726 | 11749 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      726 | 11750 | `			ph7_value_reset_string_cursor(pValue);` |
|      362 | 11751 | `		}` |
|        - | 11752 | `		/* Function arguments */` |
|      726 | 11753 | `		pArg = ph7_context_new_array(pCtx);` |
|      726 | 11754 | `		if( pArg  ){` |
|        - | 11755 | `			ph7_value *pObj;` |
|        - | 11756 | `			VmSlot *aSlot;` |
|        - | 11757 | `			sxu32 n;` |
|        - | 11758 | `			/* Start filling the array with the given arguments */` |
|      726 | 11759 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2902 | 11760 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2178 | 11761 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2178 | 11762 | `				if( pObj ){` |
|     2178 | 11763 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1088 | 11764 | `				}` |
|     1090 | 11765 | `			}` |
|        - | 11766 | `			/* Save the array */` |
|      726 | 11767 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      362 | 11768 | `		}` |
|      362 | 11769 | `	}` |
|      726 | 11770 | `	ph7_value_int(pValue,1);` |
|        - | 11771 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 11772 | `	 * line numbers at run-time. )` |
|        - | 11773 | `	 */` |
|      726 | 11774 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 11775 | `	/* Current processed script */` |
|      726 | 11776 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      726 | 11777 | `	if( pFile ){` |
|      726 | 11778 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      726 | 11779 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      726 | 11780 | `		ph7_value_reset_string_cursor(pValue);` |
|      362 | 11781 | `	}` |
|        - | 11782 | `	/* Top class */` |
|      726 | 11783 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      726 | 11784 | `	if( pClass ){` |
|      722 | 11785 | `		ph7_value_reset_string_cursor(pValue);` |
|      722 | 11786 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      722 | 11787 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      360 | 11788 | `	}` |
|        - | 11789 | `	/* Return the freshly created array */` |
|      726 | 11790 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11791 | `	/*` |
|        - | 11792 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 11793 | `	 * as soon we return from this function.` |
|        - | 11794 | `	 */` |
|      726 | 11795 | `	return PH7_OK;` |
|      364 | 11796 |  |
|        - | 11797 | `/*` |
|        - | 11798 | ` * Generate a small backtrace.` |
|        - | 11799 | ` * Store the generated dump in the given BLOB` |
|        - | 11800 | ` */` |
|        4 | 11801 | `static int VmMiniBacktrace(` |
|        - | 11802 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11803 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 11804 | `	)` |
|        1 | 11805 |  |
|        5 | 11806 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11807 | `	ph7_vm_func *pFunc;` |
|        - | 11808 | `	ph7_class *pClass;` |
|        - | 11809 | `	SyString *pFile;` |
|        - | 11810 | `	/* Called function */` |
|        5 | 11811 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 11812 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 11813 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11814 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 11815 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 11816 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 11817 | `	}else{` |
|      ! 0 | 11818 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 11819 | `	}` |
|        5 | 11820 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 11821 | `	/* Current processed script */` |
|        5 | 11822 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 11823 | `	if( pFile ){` |
|        5 | 11824 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11825 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 11826 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 11827 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 11828 | `	}` |
|        - | 11829 | `	/* Top class */` |
|        5 | 11830 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 11831 | `	if( pClass ){` |
|      ! 0 | 11832 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 11833 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 11834 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 11835 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 11836 | `	}` |
|        5 | 11837 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 11838 | `	/* All done */` |
|        5 | 11839 | `	return SXRET_OK;` |
|        1 | 11840 |  |
|        - | 11841 | `/*` |
|        - | 11842 | ` * void debug_print_backtrace()` |
|        - | 11843 | ` *  Prints a backtrace` |
|        - | 11844 | ` * Parameters` |
|        - | 11845 | ` * None` |
|        - | 11846 | ` * Return` |
|        - | 11847 | ` * NULL` |
|        - | 11848 | ` */` |
|        2 | 11849 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11850 |  |
|        3 | 11851 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11852 | `	SyBlob sDump;` |
|        3 | 11853 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11854 | `	/* Generate the backtrace */` |
|        3 | 11855 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11856 | `	/* Output backtrace */` |
|        3 | 11857 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11858 | `	/* All done,cleanup */` |
|        3 | 11859 | `	SyBlobRelease(&sDump);` |
|        1 | 11860 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11861 | `	SXUNUSED(apArg);` |
|        3 | 11862 | `	return PH7_OK;` |
|        1 | 11863 |  |
|        - | 11864 | `/*` |
|        - | 11865 | ` * string debug_string_backtrace()` |
|        - | 11866 | ` *  Generate a backtrace` |
|        - | 11867 | ` * Parameters` |
|        - | 11868 | ` * None` |
|        - | 11869 | ` * Return` |
|        - | 11870 | ` *  A mini backtrace().` |
|        - | 11871 | ` * Note that this is a symisc extension.` |
|        - | 11872 | ` */` |
|        2 | 11873 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11874 |  |
|        3 | 11875 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11876 | `	SyBlob sDump;` |
|        3 | 11877 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11878 | `	/* Generate the backtrace */` |
|        3 | 11879 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11880 | `	/* Return the backtrace */` |
|        3 | 11881 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 11882 | `	/* All done,cleanup */` |
|        3 | 11883 | `	SyBlobRelease(&sDump);` |
|        1 | 11884 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11885 | `	SXUNUSED(apArg);` |
|        3 | 11886 | `	return PH7_OK;` |
|        1 | 11887 |  |
|        - | 11888 | `/*` |
|        - | 11889 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 11890 | ` * exception is triggered.` |
|        - | 11891 | ` */` |
|      482 | 11892 | `static sxi32 VmUncaughtException(` |
|        - | 11893 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11894 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11895 | `	)` |
|        1 | 11896 |  |
|        - | 11897 | `	ph7_value *apArg[2],sArg;` |
|      483 | 11898 | `	int nArg = 1;` |
|        - | 11899 | `	sxi32 rc;` |
|      483 | 11900 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 11901 | `		/* Nesting limit reached */` |
|      ! 0 | 11902 | `		return SXRET_OK;` |
|        - | 11903 | `	}` |
|        - | 11904 | `	/* Call any exception handler if available */` |
|      483 | 11905 | `	PH7_MemObjInit(pVm,&sArg);` |
|      483 | 11906 | `	if( pThis ){` |
|        - | 11907 | `		/* Load the exception instance */` |
|      483 | 11908 | `		sArg.x.pOther = pThis;` |
|      483 | 11909 | `		pThis->iRef++;` |
|      483 | 11910 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      242 | 11911 | `	}else{` |
|      ! 0 | 11912 | `		nArg = 0;` |
|        - | 11913 | `	}` |
|      483 | 11914 | `	apArg[0] = &sArg;` |
|        - | 11915 | `	/* Call the exception handler if available */` |
|      483 | 11916 | `	pVm->nExceptDepth++;` |
|      483 | 11917 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      483 | 11918 | `	pVm->nExceptDepth--;` |
|      483 | 11919 | `	if( rc != SXRET_OK ){` |
|        - | 11920 | `		SyBlob sMsgBuf;` |
|      481 | 11921 | `		const char *zClass = "Exception";` |
|      481 | 11922 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 11923 | `		const char *zMsg;` |
|        - | 11924 | `		sxu32 nMsg;` |
|        - | 11925 | `		const char *zFuncName;` |
|        - | 11926 | `		int nFuncLen;` |
|      481 | 11927 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      481 | 11928 | `		if( pThis ){` |
|        - | 11929 | `			ph7_class_method *pGetMessage;` |
|        - | 11930 | `			ph7_value sMsg;` |
|        - | 11931 | `			const char *zTmp;` |
|        - | 11932 | `			int nTmp;` |
|      481 | 11933 | `			zClass = pThis->pClass->sName.zString;` |
|      481 | 11934 | `			nClass = pThis->pClass->sName.nByte;` |
|      481 | 11935 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      481 | 11936 | `			if( pGetMessage ){` |
|      481 | 11937 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      481 | 11938 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      481 | 11939 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      481 | 11940 | `					if( zTmp && nTmp > 0 ){` |
|      481 | 11941 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      240 | 11942 | `					}` |
|      240 | 11943 | `				}` |
|      481 | 11944 | `				PH7_MemObjRelease(&sMsg);` |
|      240 | 11945 | `			}` |
|      240 | 11946 | `		}` |
|      481 | 11947 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      481 | 11948 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      481 | 11949 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      481 | 11950 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      481 | 11951 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 11952 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      481 | 11953 | `		rc = SXERR_ABORT;` |
|      240 | 11954 | `	}` |
|      483 | 11955 | `	PH7_MemObjRelease(&sArg);` |
|      483 | 11956 | `	return rc;` |
|      242 | 11957 |  |
|        - | 11958 | `/*` |
|        - | 11959 | ` * Throw a user exception.` |
|        - | 11960 | ` *` |
|        - | 11961 | ` * Exception dispatch follows this sequence:` |
|        - | 11962 | ` *` |
|        - | 11963 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 11964 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 11965 | ` *` |
|        - | 11966 | ` * 2. If NO catch matches:` |
|        - | 11967 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 11968 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 11969 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 11970 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 11971 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 11972 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 11973 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 11974 | ` *` |
|        - | 11975 | ` * 3. If a catch DOES match:` |
|        - | 11976 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 11977 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 11978 | ` *       inside the catch body from immediately propagating past our` |
|        - | 11979 | ` *       finally block.` |
|        - | 11980 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 11981 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 11982 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 11983 | ` *       in pPendingException (step 2c).` |
|        - | 11984 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 11985 | ` *    d. Run finally (if present).` |
|        - | 11986 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 11987 | ` *       that handlers are restored and finally has run.` |
|        - | 11988 | ` */` |
|      680 | 11989 | `static sxi32 VmThrowException(` |
|        - | 11990 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 11991 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11992 | `	)` |
|        2 | 11993 |  |
|        - | 11994 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 11995 | `	ph7_exception **apException;` |
|        - | 11996 | `	ph7_exception *pException;` |
|        - | 11997 | `	/* Point to the stack of loaded exceptions */` |
|      682 | 11998 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      682 | 11999 | `	pException = 0;` |
|      682 | 12000 | `	pCatch = 0;` |
|      682 | 12001 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12002 | `		ph7_exception_block *aCatch;` |
|        - | 12003 | `		ph7_class *pClass;` |
|        - | 12004 | `		SyString *aNames;` |
|        - | 12005 | `		sxu32 nNames;` |
|        - | 12006 | `		int matched;` |
|        - | 12007 | `		sxu32 j,k;` |
|        - | 12008 | `		/* Locate the appropriate block to execute */` |
|      192 | 12009 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      192 | 12010 | `		(void)SySetPop(&pVm->aException);` |
|      192 | 12011 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      200 | 12012 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 12013 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      198 | 12014 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      198 | 12015 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      198 | 12016 | `			matched = 0;` |
|      224 | 12017 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 12018 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 12019 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 12020 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      216 | 12021 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      216 | 12022 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 12023 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 12024 | `					continue;` |
|        - | 12025 | `				}` |
|      216 | 12026 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      190 | 12027 | `					matched = 1;` |
|      190 | 12028 | `					break;` |
|        - | 12029 | `				}` |
|       14 | 12030 | `			}` |
|      198 | 12031 | `			if( matched ){` |
|        - | 12032 | `				/* Catch block found,break immediately */` |
|      190 | 12033 | `				pCatch = &aCatch[j];` |
|      190 | 12034 | `				break;` |
|        - | 12035 | `			}` |
|        5 | 12036 | `		}` |
|       95 | 12037 | `	}` |
|        - | 12038 | `	/* Execute the cached block if available */` |
|      682 | 12039 | `	if( pCatch == 0 ){` |
|        - | 12040 | `		sxi32 rc;` |
|        - | 12041 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      494 | 12042 | `		if( pException && pException->iHasFinally ){` |
|        3 | 12043 | `			pException->iFinallyDone = 1;` |
|        3 | 12044 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 12045 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12046 | `				return SXERR_ABORT;` |
|        - | 12047 | `			}` |
|        1 | 12048 | `		}` |
|        - | 12049 | `		/* Check if there is an outer exception handler on the stack */` |
|      494 | 12050 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12051 | `			/* Re-throw to the outer handler */` |
|        3 | 12052 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 12053 | `		}` |
|        - | 12054 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 12055 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 12056 | `		 * exception instead of reporting it uncaught.` |
|        - | 12057 | `		 */` |
|      492 | 12058 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 12059 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 12060 | `			 * by looking for a catch frame on the stack.` |
|        - | 12061 | `			 */` |
|      492 | 12062 | `			VmFrame *pF = pVm->pFrame;` |
|      492 | 12063 | `			int inCatch = 0;` |
|      980 | 12064 | `			while( pF ){` |
|      498 | 12065 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 12066 | `					inCatch = 1;` |
|        9 | 12067 | `					break;` |
|        - | 12068 | `				}` |
|      489 | 12069 | `				pF = pF->pParent;` |
|        1 | 12070 | `			}` |
|      492 | 12071 | `			if( inCatch ){` |
|        - | 12072 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 12073 | `				pThis->iRef++;` |
|        9 | 12074 | `				pVm->pPendingException = pThis;` |
|        9 | 12075 | `				return SXRET_OK;` |
|        - | 12076 | `			}` |
|      241 | 12077 | `		}` |
|        - | 12078 | `		/* Truly uncaught */` |
|      483 | 12079 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      483 | 12080 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 12081 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 12082 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 12083 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 12084 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 12085 | `			}` |
|      ! 0 | 12086 | `		}` |
|      483 | 12087 | `		return rc;` |
|      ! 0 | 12088 | `	}else{` |
|      190 | 12089 | `		VmFrame *pFrame = pVm->pFrame;` |
|      190 | 12090 | `		ph7_exception **apSaved = 0;` |
|        - | 12091 | `		sxu32 nSavedCount;` |
|        - | 12092 | `		sxi32 rc;` |
|      190 | 12093 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      190 | 12094 | `		if( pException->pFrame == pFrame ){` |
|      140 | 12095 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       69 | 12096 | `		}` |
|        - | 12097 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 12098 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 12099 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 12100 | `		 */` |
|      190 | 12101 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      190 | 12102 | `		if( nSavedCount > 0 ){` |
|       16 | 12103 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 12104 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12105 | `			if( apSaved ){` |
|       16 | 12106 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 12107 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12108 | `				SySetReset(&pVm->aException);` |
|        5 | 12109 | `			}` |
|        5 | 12110 | `		}` |
|        - | 12111 | `		/* Create a private frame first */` |
|      190 | 12112 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      190 | 12113 | `		if( rc == SXRET_OK ){` |
|      190 | 12114 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      190 | 12115 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      190 | 12116 | `			if( pObj ){` |
|      190 | 12117 | `				pThis->iRef++;` |
|      190 | 12118 | `				pObj->x.pOther = pThis;` |
|      190 | 12119 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       94 | 12120 | `			}` |
|        - | 12121 | `			/* Execute the catch block */` |
|      190 | 12122 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 12123 | `			/* Leave the frame */` |
|      190 | 12124 | `			VmLeaveFrame(&(*pVm));` |
|       94 | 12125 | `		}` |
|        - | 12126 | `		/* Restore the outer exception handlers */` |
|      190 | 12127 | `		if( apSaved ){` |
|        - | 12128 | `			sxu32 k;` |
|        - | 12129 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 12130 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 12131 | `			 * Restore the original outer entries.` |
|        - | 12132 | `			 */` |
|       11 | 12133 | `			SySetReset(&pVm->aException);` |
|       21 | 12134 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 12135 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 12136 | `			}` |
|       11 | 12137 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 12138 | `		}` |
|        - | 12139 | `		/* Execute the finally block after catch */` |
|      190 | 12140 | `		if( pException->iHasFinally ){` |
|       16 | 12141 | `			pException->iFinallyDone = 1;` |
|        - | 12142 | `			{` |
|       16 | 12143 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 12144 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 12145 | `					return SXERR_ABORT;` |
|        - | 12146 | `				}` |
|        - | 12147 | `			}` |
|        7 | 12148 | `		}` |
|      190 | 12149 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12150 | `			return SXERR_ABORT;` |
|        - | 12151 | `		}` |
|        - | 12152 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 12153 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 12154 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 12155 | `		 */` |
|      190 | 12156 | `		if( pVm->pPendingException ){` |
|        9 | 12157 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 12158 | `			pVm->pPendingException = 0;` |
|        9 | 12159 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 12160 | `		}` |
|        - | 12161 | `	}` |
|        - | 12162 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 12163 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 12164 | `	 */` |
|      182 | 12165 | `	return SXRET_OK;` |
|      342 | 12166 |  |
|        - | 12167 | `/*` |
|        - | 12168 | ` * Section:` |
|        - | 12169 | ` *  Version,Credits and Copyright related functions.` |
|        - | 12170 | ` * Status:` |
|        - | 12171 | ` *    Stable.` |
|        - | 12172 | ` */` |
|        - | 12173 | `/*` |
|        - | 12174 | ` * string ph7version(void)` |
|        - | 12175 | ` *  Returns the running version of the PH7 version.` |
|        - | 12176 | ` * Parameters` |
|        - | 12177 | ` *  None` |
|        - | 12178 | ` * Return` |
|        - | 12179 | ` * Current PH7 version.` |
|        - | 12180 | ` */` |
|        2 | 12181 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12182 |  |
|        1 | 12183 | `	SXUNUSED(nArg);` |
|        1 | 12184 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 12185 | `	/* Current engine version */` |
|        3 | 12186 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 12187 | `	return PH7_OK;` |
|        1 | 12188 |  |
|        - | 12189 | `/*` |
|        - | 12190 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 12191 | ` */` |
|        - | 12192 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 12193 | ` "<html><head>"\` |
|        - | 12194 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 12195 | ` "<style type=\"text/css\">"\` |
|        - | 12196 | ` "div {"\` |
|        - | 12197 | `     "border: 1px solid #cccccc;"\` |
|        - | 12198 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 12199 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 12200 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 12201 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 12202 | `     "-webkit-border-radius: 10px;"\` |
|        - | 12203 | `     "-o-border-radius: 10px;"\` |
|        - | 12204 | `     "border-radius: 10px;"\` |
|        - | 12205 | `     "padding-left: 2em;"\` |
|        - | 12206 | `     "background-color: white;"\` |
|        - | 12207 | `     "margin-left: auto;"\` |
|        - | 12208 | `     "font-family: verdana;"\` |
|        - | 12209 | `     "padding-right: 2em;"\` |
|        - | 12210 | `     "margin-right: auto;"\` |
|        - | 12211 | `     "}"\` |
|        - | 12212 | `     "body {"\` |
|        - | 12213 | `     "padding: 0.2em;"\` |
|        - | 12214 | `     "font-style: normal;"\` |
|        - | 12215 | `     "font-size: medium;"\` |
|        - | 12216 | `     "background-color: #f2f2f2;"\` |
|        - | 12217 | `     "}"\` |
|        - | 12218 | `     "hr {"\` |
|        - | 12219 | `     "border-style: solid none none;"\` |
|        - | 12220 | `     "border-width: 1px medium medium;"\` |
|        - | 12221 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 12222 | `     "height: 1px;"\` |
|        - | 12223 | `     "}"\` |
|        - | 12224 | `     "a {"\` |
|        - | 12225 | `     "color: #3366cc;"\` |
|        - | 12226 | `     "text-decoration: none;"\` |
|        - | 12227 | `     "}"\` |
|        - | 12228 | `     "a:hover {"\` |
|        - | 12229 | `     "color: #999999;"\` |
|        - | 12230 | `     "}"\` |
|        - | 12231 | `     "a:active {"\` |
|        - | 12232 | `     "color: #663399;"\` |
|        - | 12233 | `     "}"\` |
|        - | 12234 | `     "h1 {"\` |
|        - | 12235 | `     "margin: 0;"\` |
|        - | 12236 | `     "padding: 0;"\` |
|        - | 12237 | `     "font-family: Verdana;"\` |
|        - | 12238 | `     "font-weight: bold;"\` |
|        - | 12239 | `     "font-style: normal;"\` |
|        - | 12240 | `     "font-size: medium;"\` |
|        - | 12241 | `     "text-transform: capitalize;"\` |
|        - | 12242 | `     "color: #0a328c;"\` |
|        - | 12243 | `     "}"\` |
|        - | 12244 | `     "p {"\` |
|        - | 12245 | `     "margin: 0 auto;"\` |
|        - | 12246 | `     "font-size: medium;"\` |
|        - | 12247 | `     "font-style: normal;"\` |
|        - | 12248 | `     "font-family: verdana;"\` |
|        - | 12249 | `     "}"\` |
|        - | 12250 | `"</style></head><body>"\` |
|        - | 12251 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 12252 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 12253 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 12254 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 12255 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 12256 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 12257 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 12258 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 12259 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 12260 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 12261 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 12262 |  |
|        - | 12263 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12264 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 12265 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 12266 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 12267 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12268 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 12269 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12270 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 12271 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12272 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 12273 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12274 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 12275 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 12276 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 12277 |  |
|        - | 12278 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 12279 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 12280 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 12281 | `"&nbsp;*<br>"\` |
|        - | 12282 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 12283 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 12284 | `"&nbsp;* are met:<br>"\` |
|        - | 12285 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 12286 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 12287 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 12288 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 12289 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 12290 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 12291 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 12292 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 12293 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 12294 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 12295 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 12296 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 12297 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 12298 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 12299 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 12300 | `"&nbsp;*<br>"\` |
|        - | 12301 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 12302 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 12303 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 12304 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 12305 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 12306 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 12307 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 12308 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 12309 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 12310 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 12311 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 12312 | `"&nbsp;*/<br>"\` |
|        - | 12313 | `"</span></small></small></p>"\` |
|        - | 12314 | `"</div></body></html>"` |
|        - | 12315 | `/*` |
|        - | 12316 | ` * bool ph7credits(void)` |
|        - | 12317 | ` * bool ph7info(void)` |
|        - | 12318 | ` * bool ph7copyright(void)` |
|        - | 12319 | ` *  Prints out the credits for PH7 engine` |
|        - | 12320 | ` * Parameters` |
|        - | 12321 | ` *  None` |
|        - | 12322 | ` * Return` |
|        - | 12323 | ` *  Always TRUE` |
|        - | 12324 | ` */` |
|        2 | 12325 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12326 |  |
|        3 | 12327 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 12328 | `	/* Expand the HTML page above*/` |
|        3 | 12329 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 12330 | `	ph7_context_output_format(` |
|        1 | 12331 | `		pCtx,` |
|        - | 12332 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 12333 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 12334 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 12335 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 12336 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 12337 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 12338 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 12339 | `#ifdef __WINNT__` |
|        - | 12340 | `		"Windows NT"` |
|        - | 12341 | `#elif defined(__UNIXES__)` |
|        - | 12342 | `		"UNIX-Like"` |
|        - | 12343 | `#else` |
|        - | 12344 | `		"Other OS"` |
|        - | 12345 | `#endif` |
|        - | 12346 | `		);` |
|        3 | 12347 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 12348 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12349 | `	SXUNUSED(apArg);` |
|        - | 12350 | `	/* Return TRUE */` |
|        - | 12351 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 12352 | `	return PH7_OK;` |
|        1 | 12353 |  |
|        - | 12354 | `/*` |
|        - | 12355 | ` * Section:` |
|        - | 12356 | ` *    URL related routines.` |
|        - | 12357 | ` * Status:` |
|        - | 12358 | ` *    Stable.` |
|        - | 12359 | ` */` |
|        - | 12360 | `/*` |
|        - | 12361 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 12362 | ` *  Parse a URL and return its fields.` |
|        - | 12363 | ` * Parameters` |
|        - | 12364 | ` *  $url` |
|        - | 12365 | ` *   The URL to parse.` |
|        - | 12366 | ` * $component` |
|        - | 12367 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 12368 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 12369 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 12370 | ` *  in which case the return value will be an integer).` |
|        - | 12371 | ` * Return` |
|        - | 12372 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 12373 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 12374 | ` *  this array are:` |
|        - | 12375 | ` *   scheme - e.g. http` |
|        - | 12376 | ` *   host` |
|        - | 12377 | ` *   port` |
|        - | 12378 | ` *   user` |
|        - | 12379 | ` *   pass` |
|        - | 12380 | ` *   path` |
|        - | 12381 | ` *   query - after the question mark ?` |
|        - | 12382 | ` *   fragment - after the hashmark #` |
|        - | 12383 | ` * Note:` |
|        - | 12384 | ` *  FALSE is returned on failure.` |
|        - | 12385 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 12386 | ` *  with the standard PHP engine.` |
|        - | 12387 | ` */` |
|       28 | 12388 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12389 |  |
|        - | 12390 | `	const char *zStr; /* Input string */` |
|        - | 12391 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 12392 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 12393 | `	int nLen;` |
|        - | 12394 | `	sxi32 rc;` |
|       29 | 12395 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12396 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 12397 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12398 | `		return PH7_OK;` |
|        - | 12399 | `	}` |
|        - | 12400 | `	/* Extract the given URI */` |
|       29 | 12401 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 12402 | `	if( nLen < 1 ){` |
|        - | 12403 | `		/* Nothing to process,return FALSE */` |
|        3 | 12404 | `		ph7_result_bool(pCtx,0);` |
|        3 | 12405 | `		return PH7_OK;` |
|        - | 12406 | `	}` |
|        - | 12407 | `	/* Get a parse */` |
|       27 | 12408 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 12409 | `	if( rc != SXRET_OK ){` |
|        - | 12410 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 12411 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12412 | `		return PH7_OK;` |
|        - | 12413 | `	}` |
|       27 | 12414 | `	if( nArg > 1 ){` |
|      ! 0 | 12415 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 12416 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 12417 | `		switch(nComponent){` |
|      ! 0 | 12418 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 12419 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 12420 | `			if( pComp->nByte < 1 ){` |
|        - | 12421 | `				/* No available value,return NULL */` |
|      ! 0 | 12422 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12423 | `			}else{` |
|      ! 0 | 12424 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12425 | `			}` |
|      ! 0 | 12426 | `			break;` |
|      ! 0 | 12427 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 12428 | `			pComp = &sURI.sHost;` |
|      ! 0 | 12429 | `			if( pComp->nByte < 1 ){` |
|        - | 12430 | `				/* No available value,return NULL */` |
|      ! 0 | 12431 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12432 | `			}else{` |
|      ! 0 | 12433 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12434 | `			}` |
|      ! 0 | 12435 | `			break;` |
|      ! 0 | 12436 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 12437 | `			pComp = &sURI.sPort;` |
|      ! 0 | 12438 | `			if( pComp->nByte < 1 ){` |
|        - | 12439 | `				/* No available value,return NULL */` |
|      ! 0 | 12440 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12441 | `			}else{` |
|      ! 0 | 12442 | `				int iPort = 0;` |
|        - | 12443 | `				/* Cast the value to integer */` |
|      ! 0 | 12444 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 12445 | `				ph7_result_int(pCtx,iPort);` |
|        - | 12446 | `			}` |
|      ! 0 | 12447 | `			break;` |
|      ! 0 | 12448 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 12449 | `			pComp = &sURI.sUser;` |
|      ! 0 | 12450 | `			if( pComp->nByte < 1 ){` |
|        - | 12451 | `				/* No available value,return NULL */` |
|      ! 0 | 12452 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12453 | `			}else{` |
|      ! 0 | 12454 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12455 | `			}` |
|      ! 0 | 12456 | `			break;` |
|      ! 0 | 12457 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 12458 | `			pComp = &sURI.sPass;` |
|      ! 0 | 12459 | `			if( pComp->nByte < 1 ){` |
|        - | 12460 | `				/* No available value,return NULL */` |
|      ! 0 | 12461 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12462 | `			}else{` |
|      ! 0 | 12463 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12464 | `			}` |
|      ! 0 | 12465 | `			break;` |
|      ! 0 | 12466 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 12467 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 12468 | `			if( pComp->nByte < 1 ){` |
|        - | 12469 | `				/* No available value,return NULL */` |
|      ! 0 | 12470 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12471 | `			}else{` |
|      ! 0 | 12472 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12473 | `			}` |
|      ! 0 | 12474 | `			break;` |
|      ! 0 | 12475 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 12476 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 12477 | `			if( pComp->nByte < 1 ){` |
|        - | 12478 | `				/* No available value,return NULL */` |
|      ! 0 | 12479 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12480 | `			}else{` |
|      ! 0 | 12481 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12482 | `			}` |
|      ! 0 | 12483 | `			break;` |
|      ! 0 | 12484 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 12485 | `			pComp = &sURI.sPath;` |
|      ! 0 | 12486 | `			if( pComp->nByte < 1 ){` |
|        - | 12487 | `				/* No available value,return NULL */` |
|      ! 0 | 12488 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12489 | `			}else{` |
|      ! 0 | 12490 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12491 | `			}` |
|      ! 0 | 12492 | `			break;` |
|      ! 0 | 12493 | `		default:` |
|        - | 12494 | `			/* No such entry,return NULL */` |
|      ! 0 | 12495 | `			ph7_result_null(pCtx);` |
|      ! 0 | 12496 | `			break;` |
|        - | 12497 | `		}` |
|      ! 0 | 12498 | `	}else{` |
|        - | 12499 | `		ph7_value *pArray,*pValue;` |
|        - | 12500 | `		/* Return an associative array */` |
|       27 | 12501 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 12502 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 12503 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12504 | `			/* Out of memory */` |
|      ! 0 | 12505 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12506 | `			/* Return false */` |
|      ! 0 | 12507 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 12508 | `			return PH7_OK;` |
|        - | 12509 | `		}` |
|        - | 12510 | `		/* Fill the array */` |
|       27 | 12511 | `		pComp = &sURI.sScheme;` |
|       27 | 12512 | `		if( pComp->nByte > 0 ){` |
|       19 | 12513 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 12514 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 12515 | `		}` |
|        - | 12516 | `		/* Reset the string cursor */` |
|       27 | 12517 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12518 | `		pComp = &sURI.sHost;` |
|       27 | 12519 | `		if( pComp->nByte > 0 ){` |
|       25 | 12520 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 12521 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 12522 | `		}` |
|        - | 12523 | `		/* Reset the string cursor */` |
|       27 | 12524 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12525 | `		pComp = &sURI.sPort;` |
|       27 | 12526 | `		if( pComp->nByte > 0 ){` |
|       11 | 12527 | `			int iPort = 0;/* cc warning */` |
|        - | 12528 | `			/* Convert to integer */` |
|       11 | 12529 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 12530 | `			ph7_value_int(pValue,iPort);` |
|       11 | 12531 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 12532 | `		}` |
|        - | 12533 | `		/* Reset the string cursor */` |
|       27 | 12534 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12535 | `		pComp = &sURI.sUser;` |
|       27 | 12536 | `		if( pComp->nByte > 0 ){` |
|        7 | 12537 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12538 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 12539 | `		}` |
|        - | 12540 | `		/* Reset the string cursor */` |
|       27 | 12541 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12542 | `		pComp = &sURI.sPass;` |
|       27 | 12543 | `		if( pComp->nByte > 0 ){` |
|        7 | 12544 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12545 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 12546 | `		}` |
|        - | 12547 | `		/* Reset the string cursor */` |
|       27 | 12548 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12549 | `		pComp = &sURI.sPath;` |
|       27 | 12550 | `		if( pComp->nByte > 0 ){` |
|       17 | 12551 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 12552 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 12553 | `		}` |
|        - | 12554 | `		/* Reset the string cursor */` |
|       27 | 12555 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12556 | `		pComp = &sURI.sQuery;` |
|       27 | 12557 | `		if( pComp->nByte > 0 ){` |
|        5 | 12558 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12559 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 12560 | `		}` |
|        - | 12561 | `		/* Reset the string cursor */` |
|       27 | 12562 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12563 | `		pComp = &sURI.sFragment;` |
|       27 | 12564 | `		if( pComp->nByte > 0 ){` |
|        5 | 12565 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12566 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 12567 | `		}` |
|        - | 12568 | `		/* Return the created array */` |
|       27 | 12569 | `		ph7_result_value(pCtx,pArray);` |
|        - | 12570 | `		/* NOTE:` |
|        - | 12571 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 12572 | `		 * automatically as soon we return from this function.` |
|        - | 12573 | `		 */` |
|        - | 12574 | `	}` |
|        - | 12575 | `	/* All done */` |
|       27 | 12576 | `	return PH7_OK;` |
|       15 | 12577 |  |
|        - | 12578 | `/*` |
|        - | 12579 | ` * Section:` |
|        - | 12580 | ` *   Array related routines.` |
|        - | 12581 | ` * Status:` |
|        - | 12582 | ` *    Stable.` |
|        - | 12583 | ` * Note 2012-5-21 01:04:15:` |
|        - | 12584 | ` *  Array related functions that need access to the underlying` |
|        - | 12585 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 12586 | ` */` |
|        - | 12587 | `/*` |
|        - | 12588 | ` * The [compact()] function store it's state information in an instance` |
|        - | 12589 | ` * of the following structure.` |
|        - | 12590 | ` */` |
|        - | 12591 | `struct compact_data` |
|        - | 12592 |  |
|        - | 12593 | `	ph7_value *pArray;  /* Target array */` |
|        - | 12594 | `	int nRecCount;      /* Recursion count */` |
|        - | 12595 | `};` |
|        - | 12596 | `/*` |
|        - | 12597 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 12598 | ` */` |
|      ! 0 | 12599 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 12600 |  |
|      ! 0 | 12601 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 12602 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 12603 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12604 | `	/* Act according to the hashmap value */` |
|      ! 0 | 12605 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 12606 | `		SyString sVar;` |
|      ! 0 | 12607 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 12608 | `		if( sVar.nByte > 0 ){` |
|        - | 12609 | `			/* Query the current frame */` |
|      ! 0 | 12610 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 12611 | `			/* ^` |
|        - | 12612 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 12613 | `			 */` |
|      ! 0 | 12614 | `			if( pKey ){` |
|        - | 12615 | `				/* Perform the insertion */` |
|      ! 0 | 12616 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 12617 | `			}` |
|      ! 0 | 12618 | `		}` |
|      ! 0 | 12619 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 12620 | `		int rc;` |
|        - | 12621 | `		/* Recursively traverse this array */` |
|      ! 0 | 12622 | `		pData->nRecCount++;` |
|      ! 0 | 12623 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 12624 | `		pData->nRecCount--;` |
|      ! 0 | 12625 | `		return rc;` |
|        - | 12626 | `	}` |
|      ! 0 | 12627 | `	return SXRET_OK;` |
|      ! 0 | 12628 |  |
|        - | 12629 | `/*` |
|        - | 12630 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 12631 | ` *  Create array containing variables and their values.` |
|        - | 12632 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 12633 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 12634 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 12635 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 12636 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 12637 | ` * Parameters` |
|        - | 12638 | ` *  $varname` |
|        - | 12639 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 12640 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 12641 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 12642 | ` *   it recursively.` |
|        - | 12643 | ` * Return` |
|        - | 12644 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 12645 | ` */` |
|        2 | 12646 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12647 |  |
|        - | 12648 | `	ph7_value *pArray,*pObj;` |
|        3 | 12649 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12650 | `	const char *zName;` |
|        - | 12651 | `	SyString sVar;` |
|        - | 12652 | `	int i,nLen;` |
|        3 | 12653 | `	if( nArg < 1 ){` |
|        - | 12654 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 12655 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12656 | `		return PH7_OK;` |
|        - | 12657 | `	}` |
|        - | 12658 | `	/* Create the array */` |
|        3 | 12659 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12660 | `	if( pArray == 0 ){` |
|        - | 12661 | `		/* Out of memory */` |
|      ! 0 | 12662 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12663 | `		/* Return NULL */` |
|      ! 0 | 12664 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12665 | `		return PH7_OK;` |
|        - | 12666 | `	}` |
|        - | 12667 | `	/* Perform the requested operation */` |
|        7 | 12668 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 12669 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 12670 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 12671 | `				struct compact_data sData;` |
|      ! 0 | 12672 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 12673 | `				/* Recursively walk the array */` |
|      ! 0 | 12674 | `				sData.nRecCount = 0;` |
|      ! 0 | 12675 | `				sData.pArray = pArray;` |
|      ! 0 | 12676 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 12677 | `			}` |
|      ! 0 | 12678 | `		}else{` |
|        - | 12679 | `			/* Extract variable name */` |
|        5 | 12680 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 12681 | `			if( nLen > 0 ){` |
|        5 | 12682 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 12683 | `				/* Check if the variable is available in the current frame */` |
|        5 | 12684 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 12685 | `				if( pObj ){` |
|        5 | 12686 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 12687 | `				}` |
|        2 | 12688 | `			}` |
|        - | 12689 | `		}` |
|        3 | 12690 | `	}` |
|        - | 12691 | `	/* Return the array */` |
|        3 | 12692 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12693 | `	return PH7_OK;` |
|        2 | 12694 |  |
|        - | 12695 | `/*` |
|        - | 12696 | ` * The [extract()] function store it's state information in an instance` |
|        - | 12697 | ` * of the following structure.` |
|        - | 12698 | ` */` |
|        - | 12699 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 12700 | `struct extract_aux_data` |
|        - | 12701 |  |
|        - | 12702 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 12703 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 12704 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 12705 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 12706 | `	int iFlags;           /* Control flags */` |
|        - | 12707 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 12708 | `};` |
|        - | 12709 | `/* Forward declaration */` |
|        - | 12710 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 12711 | `/*` |
|        - | 12712 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 12713 | ` *   Import variables into the current symbol table from an array.` |
|        - | 12714 | ` * Parameters` |
|        - | 12715 | ` * $var_array` |
|        - | 12716 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 12717 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 12718 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 12719 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 12720 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 12721 | ` * $extract_type` |
|        - | 12722 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 12723 | ` *  It can be one of the following values:` |
|        - | 12724 | ` *   EXTR_OVERWRITE` |
|        - | 12725 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 12726 | ` *   EXTR_SKIP` |
|        - | 12727 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 12728 | ` *   EXTR_PREFIX_SAME` |
|        - | 12729 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 12730 | ` *   EXTR_PREFIX_ALL` |
|        - | 12731 | ` *       Prefix all variable names with prefix.` |
|        - | 12732 | ` *   EXTR_PREFIX_INVALID` |
|        - | 12733 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 12734 | ` *   EXTR_IF_EXISTS` |
|        - | 12735 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 12736 | ` *       otherwise do nothing.` |
|        - | 12737 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 12738 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 12739 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 12740 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 12741 | ` *      the current symbol table.` |
|        - | 12742 | ` * $prefix` |
|        - | 12743 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 12744 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 12745 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 12746 | ` *  underscore character.` |
|        - | 12747 | ` * Return` |
|        - | 12748 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 12749 | ` */` |
|        4 | 12750 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12751 |  |
|        - | 12752 | `	extract_aux_data sAux;` |
|        - | 12753 | `	ph7_hashmap *pMap;` |
|        5 | 12754 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 12755 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 12756 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12757 | `		return PH7_OK;` |
|        - | 12758 | `	}` |
|        - | 12759 | `	/* Point to the target hashmap */` |
|        5 | 12760 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 12761 | `	if( pMap->nEntry < 1 ){` |
|        - | 12762 | `		/* Empty map,return  0 */` |
|      ! 0 | 12763 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12764 | `		return PH7_OK;` |
|        - | 12765 | `	}` |
|        - | 12766 | `	/* Prepare the aux data */` |
|        5 | 12767 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 12768 | `	if( nArg > 1 ){` |
|        3 | 12769 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 12770 | `		if( nArg > 2 ){` |
|      ! 0 | 12771 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 12772 | `		}` |
|        1 | 12773 | `	}` |
|        5 | 12774 | `	sAux.pVm = pCtx->pVm;` |
|        - | 12775 | `	/* Invoke the worker callback */` |
|        5 | 12776 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 12777 | `	/* Number of variables successfully imported */` |
|        5 | 12778 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 12779 | `	return PH7_OK;` |
|        3 | 12780 |  |
|        - | 12781 | `/*` |
|        - | 12782 | ` * Worker callback for the [extract()] function defined` |
|        - | 12783 | ` * below.` |
|        - | 12784 | ` */` |
|        8 | 12785 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12786 |  |
|        9 | 12787 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 12788 | `	int iFlags = pAux->iFlags;` |
|        9 | 12789 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12790 | `	ph7_value *pObj;` |
|        - | 12791 | `	SyString sVar;` |
|        9 | 12792 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 12793 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 12794 | `	}` |
|        - | 12795 | `	/* Perform a string cast */` |
|        9 | 12796 | `	PH7_MemObjToString(pKey);` |
|        9 | 12797 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12798 | `		/* Unavailable variable name */` |
|      ! 0 | 12799 | `		return SXRET_OK;` |
|        - | 12800 | `	}` |
|        9 | 12801 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 12802 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 12803 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12804 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12805 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12806 | `			);` |
|      ! 0 | 12807 | `	}else{` |
|       13 | 12808 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 12809 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12810 | `	}` |
|        9 | 12811 | `	sVar.zString = pAux->zWorker;` |
|        - | 12812 | `	/* Try to extract the variable */` |
|        9 | 12813 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 12814 | `	if( pObj ){` |
|        - | 12815 | `		/* Collision */` |
|        5 | 12816 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 12817 | `			return SXRET_OK;` |
|        - | 12818 | `		}` |
|        5 | 12819 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 12820 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 12821 | `				/* Already prefixed */` |
|      ! 0 | 12822 | `				return SXRET_OK;` |
|        - | 12823 | `			}` |
|      ! 0 | 12824 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12825 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12826 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12827 | `				);` |
|      ! 0 | 12828 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 12829 | `		}` |
|        3 | 12830 | `	}else{` |
|        - | 12831 | `		/* Create the variable */` |
|        5 | 12832 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 12833 | `	}` |
|        9 | 12834 | `	if( pObj ){` |
|        - | 12835 | `		/* Overwrite the old value */` |
|        9 | 12836 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 12837 | `		/* Increment counter */` |
|        9 | 12838 | `		pAux->iCount++;` |
|        4 | 12839 | `	}` |
|        9 | 12840 | `	return SXRET_OK;` |
|        5 | 12841 |  |
|        - | 12842 | `/*` |
|        - | 12843 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 12844 | ` * defined below.` |
|        - | 12845 | ` */` |
|        2 | 12846 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12847 |  |
|        3 | 12848 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 12849 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12850 | `	ph7_value *pObj;` |
|        - | 12851 | `	SyString sVar;` |
|        - | 12852 | `	/* Perform a string cast */` |
|        3 | 12853 | `	PH7_MemObjToString(pKey);` |
|        3 | 12854 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12855 | `		/* Unavailable variable name */` |
|      ! 0 | 12856 | `		return SXRET_OK;` |
|        - | 12857 | `	}` |
|        3 | 12858 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 12859 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 12860 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 12861 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 12862 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12863 | `			);` |
|        2 | 12864 | `	}else{` |
|      ! 0 | 12865 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 12866 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12867 | `	}` |
|        3 | 12868 | `	sVar.zString = pAux->zWorker;` |
|        - | 12869 | `	/* Extract the variable */` |
|        3 | 12870 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 12871 | `	if( pObj ){` |
|        3 | 12872 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 12873 | `	}` |
|        3 | 12874 | `	return SXRET_OK;` |
|        2 | 12875 |  |
|        - | 12876 | `/*` |
|        - | 12877 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 12878 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 12879 | ` * Parameters` |
|        - | 12880 | ` * $types` |
|        - | 12881 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 12882 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 12883 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 12884 | ` *  POST includes the POST uploaded file information.` |
|        - | 12885 | ` *  Note:` |
|        - | 12886 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 12887 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 12888 | ` * $prefix` |
|        - | 12889 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 12890 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 12891 | ` *  variable named $pref_userid.` |
|        - | 12892 | ` * Return` |
|        - | 12893 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12894 | ` */` |
|        2 | 12895 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12896 |  |
|        - | 12897 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 12898 | `	extract_aux_data sAux;` |
|        - | 12899 | `	int nLen,nPrefixLen;` |
|        - | 12900 | `	ph7_value *pSuper;` |
|        - | 12901 | `	ph7_vm *pVm;` |
|        - | 12902 | `	/* By default import only $_GET variables  */` |
|        3 | 12903 | `	zImport = "G";` |
|        3 | 12904 | `	nLen = (int)sizeof(char);` |
|        3 | 12905 | `	zPrefix = 0;` |
|        3 | 12906 | `	nPrefixLen = 0;` |
|        3 | 12907 | `	if( nArg > 0 ){` |
|        3 | 12908 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 12909 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 12910 | `		}` |
|        3 | 12911 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12912 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 12913 | `		}` |
|        1 | 12914 | `	}` |
|        - | 12915 | `	/* Point to the underlying VM */` |
|        3 | 12916 | `	pVm = pCtx->pVm;` |
|        - | 12917 | `	/* Initialize the aux data */` |
|        3 | 12918 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 12919 | `	sAux.zPrefix = zPrefix;` |
|        3 | 12920 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 12921 | `	sAux.pVm = pVm;` |
|        - | 12922 | `	/* Extract */` |
|        3 | 12923 | `	zEnd = &zImport[nLen];` |
|        5 | 12924 | `	while( zImport < zEnd ){` |
|        3 | 12925 | `		int c = zImport[0];` |
|        3 | 12926 | `		pSuper = 0;` |
|        3 | 12927 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 12928 | `			/* Import $_GET variables */` |
|        3 | 12929 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 12930 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 12931 | `			/* Import $_POST variables */` |
|      ! 0 | 12932 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 12933 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 12934 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 12935 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 12936 | `		}` |
|        3 | 12937 | `		if( pSuper ){` |
|        - | 12938 | `			/* Iterate throw array entries */` |
|        3 | 12939 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 12940 | `		}` |
|        - | 12941 | `		/* Advance the cursor */` |
|        3 | 12942 | `		zImport++;` |
|        1 | 12943 | `	}` |
|        - | 12944 | `	/* All done,return TRUE*/` |
|        3 | 12945 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12946 | `	return PH7_OK;` |
|        1 | 12947 |  |
|        - | 12948 | `/*` |
|        - | 12949 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 12950 | ` * Refer to the eval() language construct implementation for more` |
|        - | 12951 | ` * information.` |
|        - | 12952 | ` */` |
|    11846 | 12953 | `static sxi32 VmEvalChunk(` |
|        - | 12954 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 12955 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 12956 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 12957 | `	int iFlags,         /* Compile flag */` |
|        - | 12958 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 12959 | `	)` |
|        2 | 12960 |  |
|        - | 12961 | `	SySet *pByteCode,aByteCode;` |
|        - | 12962 | `	SyBlob sSavedNs;` |
|    11848 | 12963 | `	ProcConsumer xErr = 0;` |
|    11848 | 12964 | `	void *pErrData = 0;` |
|        - | 12965 | `	/* Initialize bytecode container */` |
|    11848 | 12966 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    11848 | 12967 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 12968 | `	/* Reset the code generator */` |
|    11848 | 12969 | `	if( bTrueReturn ){` |
|        - | 12970 | `		/* Included file,log compile-time errors */` |
|     8910 | 12971 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8910 | 12972 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4454 | 12973 | `	}` |
|    11848 | 12974 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 12975 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 12976 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 12977 | `	 * the caller's namespace is restored. */` |
|    11848 | 12978 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    11848 | 12979 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    11848 | 12980 | `	if( bTrueReturn ){` |
|        - | 12981 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8910 | 12982 | `		SyBlobReset(&pVm->sNamespace);` |
|     4454 | 12983 | `	}` |
|        - | 12984 | `	/* Swap bytecode container */` |
|    11848 | 12985 | `	pByteCode = pVm->pByteContainer;` |
|    11848 | 12986 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 12987 | `	/* Compile the chunk */` |
|    11848 | 12988 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    17771 | 12989 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 12990 | `		/* Compilation error,return false */` |
|        3 | 12991 | `		if( pCtx ){` |
|        3 | 12992 | `			ph7_result_bool(pCtx,0);` |
|        1 | 12993 | `		}` |
|        2 | 12994 | `	}else{` |
|        - | 12995 | `		/* Mount any newly defined classes */` |
|        - | 12996 | `		SyHashEntry *pEntry;` |
|        - | 12997 | `		ph7_class *pClass;` |
|        - | 12998 | `		ph7_value sResult; /* Return value */` |
|        - | 12999 | `		sxi32 rc;` |
|    11846 | 13000 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   560636 | 13001 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   542870 | 13002 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 13003 | `			/* Only mount classes that haven't been mounted yet */` |
|   542870 | 13004 | `			if( !pClass->bMounted ){` |
|   107450 | 13005 | `				rc = VmMountUserClass(pVm,pClass);` |
|   107450 | 13006 | `				if( rc != SXRET_OK ){` |
|        - | 13007 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 13008 | `					if( pCtx ){` |
|      ! 0 | 13009 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 13010 | `					}` |
|      ! 0 | 13011 | `					goto Cleanup;` |
|        - | 13012 | `				}` |
|    53724 | 13013 | `			}` |
|        2 | 13014 | `		}` |
|    11846 | 13015 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 13016 | `			/* Out of memory */` |
|      ! 0 | 13017 | `			if( pCtx ){` |
|      ! 0 | 13018 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 13019 | `			}` |
|      ! 0 | 13020 | `			goto Cleanup;` |
|        - | 13021 | `		}` |
|    11846 | 13022 | `		if( bTrueReturn ){` |
|        - | 13023 | `			/* Assume a boolean true return value */` |
|     8910 | 13024 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4456 | 13025 | `		}else{` |
|        - | 13026 | `			/* Assume a null return value */` |
|     2938 | 13027 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 13028 | `		}` |
|        - | 13029 | `		/* Execute the compiled chunk */` |
|    11846 | 13030 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    11846 | 13031 | `		if( pCtx ){` |
|        - | 13032 | `			/* Set the execution result */` |
|     8928 | 13033 | `			ph7_result_value(pCtx,&sResult);` |
|     4463 | 13034 | `		}` |
|    11846 | 13035 | `		PH7_MemObjRelease(&sResult);` |
|        - | 13036 | `	}` |
|     5923 | 13037 | `Cleanup:` |
|        - | 13038 | `	/* Cleanup the mess left behind */` |
|    11848 | 13039 | `	pVm->pByteContainer = pByteCode;` |
|    11848 | 13040 | `	SySetRelease(&aByteCode);` |
|        - | 13041 | `	/* Restore caller's namespace state */` |
|    11848 | 13042 | `	SyBlobReset(&pVm->sNamespace);` |
|    11848 | 13043 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    11848 | 13044 | `	SyBlobRelease(&sSavedNs);` |
|    11848 | 13045 | `	return SXRET_OK;` |
|        2 | 13046 |  |
|        - | 13047 | `/*` |
|        - | 13048 | ` * value eval(string $code)` |
|        - | 13049 | ` *   Evaluate a string as PHP code.` |
|        - | 13050 | ` * Parameter` |
|        - | 13051 | ` *  code: PHP code to evaluate.` |
|        - | 13052 | ` * Return` |
|        - | 13053 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 13054 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 13055 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 13056 | ` */` |
|       22 | 13057 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13058 |  |
|        - | 13059 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 13060 | `	if( nArg < 1 ){` |
|        - | 13061 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13062 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13063 | `		return SXRET_OK;` |
|        - | 13064 | `	}` |
|        - | 13065 | `	/* Chunk to evaluate */` |
|       24 | 13066 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 13067 | `	if( sChunk.nByte < 1 ){` |
|        - | 13068 | `		/* Empty string,return NULL */` |
|        3 | 13069 | `		ph7_result_null(pCtx);` |
|        3 | 13070 | `		return SXRET_OK;` |
|        - | 13071 | `	}` |
|        - | 13072 | `	/* Eval the chunk */` |
|       22 | 13073 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 13074 | `	return SXRET_OK;` |
|       13 | 13075 |  |
|        - | 13076 | `/*` |
|        - | 13077 | ` * Check if a file path is already included.` |
|        - | 13078 | ` */` |
|    17812 | 13079 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 13080 |  |
|        - | 13081 | `	SyString *aEntries;` |
|        - | 13082 | `	sxu32 n;` |
|    17814 | 13083 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 13084 | `	/* Perform a linear search */` |
| 79265134 | 13085 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 79247328 | 13086 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 13087 | `			/* Already included */` |
|        7 | 13088 | `			return TRUE;` |
|        - | 13089 | `		}` |
| 39623662 | 13090 | `	}` |
|    17808 | 13091 | `	return FALSE;` |
|     8908 | 13092 |  |
|        - | 13093 | `/*` |
|        - | 13094 | ` * Push a file path in the appropriate VM container.` |
|        - | 13095 | ` */` |
|    20722 | 13096 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 13097 |  |
|        - | 13098 | `	SyString sPath;` |
|        - | 13099 | `	char *zDup;` |
|        - | 13100 | `#ifdef __WINNT__` |
|        - | 13101 | `	char *zCur;` |
|        - | 13102 | `#endif` |
|        - | 13103 | `	sxi32 rc;` |
|    20724 | 13104 | `	if( nLen < 0 ){` |
|     2912 | 13105 | `		nLen = SyStrlen(zPath);` |
|     1455 | 13106 | `	}` |
|        - | 13107 | `	/* Duplicate the file path first */` |
|    20724 | 13108 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    20724 | 13109 | `	if( zDup == 0 ){` |
|      ! 0 | 13110 | `		return SXERR_MEM;` |
|        - | 13111 | `	}` |
|        - | 13112 | `#ifdef __WINNT__` |
|        - | 13113 | `	/* Normalize path on windows` |
|        - | 13114 | `	 * Example:` |
|        - | 13115 | `	 *    Path/To/File.php` |
|        - | 13116 | `	 * becomes` |
|        - | 13117 | `	 *   path\to\file.php` |
|        - | 13118 | `	 */` |
|        2 | 13119 | `	zCur = zDup;` |
|        2 | 13120 | `	while( zCur[0] != 0 ){` |
|        2 | 13121 | `		if( zCur[0] == '/' ){` |
|        2 | 13122 | `			zCur[0] = '\\';` |
|        2 | 13123 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 13124 | `			int c = SyToLower(zCur[0]);` |
|        1 | 13125 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 13126 | `		}` |
|        2 | 13127 | `		zCur++;` |
|        2 | 13128 | `	}` |
|        - | 13129 | `#endif` |
|        - | 13130 | `	/* Install the file path */` |
|    20724 | 13131 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    20724 | 13132 | `	if( !bMain ){` |
|    17814 | 13133 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 13134 | `			/* Already included */` |
|        7 | 13135 | `			*pNew = 0;` |
|        4 | 13136 | `		}else{` |
|        - | 13137 | `			/* Insert in the corresponding container */` |
|    17808 | 13138 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    17808 | 13139 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13140 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 13141 | `				return rc;` |
|        - | 13142 | `			}` |
|    17808 | 13143 | `			*pNew = 1;` |
|        - | 13144 | `		}` |
|     8906 | 13145 | `	}` |
|    20724 | 13146 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    20724 | 13147 | `	return SXRET_OK;` |
|    10363 | 13148 |  |
|        - | 13149 | `/*` |
|        - | 13150 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 13151 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 13152 | ` * indicates failure.` |
|        - | 13153 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 13154 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 13155 | ` * operations.` |
|        - | 13156 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 13157 | ` * this function is a no-op.` |
|        - | 13158 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 13159 | ` * constructs for more information.` |
|        - | 13160 | ` */` |
|     8918 | 13161 | `static sxi32 VmExecIncludedFile(` |
|        - | 13162 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 13163 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 13164 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 13165 | `	 )` |
|        2 | 13166 |  |
|        - | 13167 | `	sxi32 rc;` |
|        - | 13168 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13169 | `	const ph7_io_stream *pStream;` |
|        - | 13170 | `	SyBlob sContents;` |
|        - | 13171 | `	void *pHandle;` |
|        - | 13172 | `	ph7_vm *pVm;` |
|        - | 13173 | `	int isNew;` |
|        - | 13174 | `	/* Initialize fields */` |
|     8920 | 13175 | `	pVm = pCtx->pVm;` |
|     8920 | 13176 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8920 | 13177 | `	isNew = 0;` |
|        - | 13178 | `	/* Extract the associated stream */` |
|     8920 | 13179 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 13180 | `	/*` |
|        - | 13181 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 13182 | `	 * in a read-only mode.` |
|        - | 13183 | `	 */` |
|     8920 | 13184 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8920 | 13185 | `	if( pHandle == 0 ){` |
|        8 | 13186 | `		return SXERR_IO;` |
|        - | 13187 | `	}` |
|     8914 | 13188 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8914 | 13189 | `	if( IncludeOnce && !isNew ){` |
|        - | 13190 | `		/* Already included */` |
|        5 | 13191 | `		rc = SXERR_EXISTS;` |
|        3 | 13192 | `	}else{` |
|        - | 13193 | `		/* Read the whole file contents */` |
|     8910 | 13194 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8910 | 13195 | `		if( rc == SXRET_OK ){` |
|        - | 13196 | `			SyString sScript;` |
|        - | 13197 | `			/* Compile and execute the script */` |
|     8910 | 13198 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8910 | 13199 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4454 | 13200 | `		}` |
|        - | 13201 | `	}` |
|        - | 13202 | `	/* Pop from the set of included file */` |
|     8914 | 13203 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 13204 | `	/* Close the handle */` |
|     8914 | 13205 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 13206 | `	/* Release the working buffer */` |
|     8914 | 13207 | `	SyBlobRelease(&sContents);` |
|        - | 13208 | `#else` |
|        - | 13209 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 13210 | `	SXUNUSED(pPath);` |
|        - | 13211 | `	SXUNUSED(IncludeOnce);` |
|        - | 13212 | `	rc = SXERR_IO;` |
|        - | 13213 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8914 | 13214 | `	return rc;` |
|     4461 | 13215 |  |
|        - | 13216 | `/*` |
|        - | 13217 | ` * string get_include_path(void)` |
|        - | 13218 | ` *  Gets the current include_path configuration option.` |
|        - | 13219 | ` * Parameter` |
|        - | 13220 | ` *  None` |
|        - | 13221 | ` * Return` |
|        - | 13222 | ` *  Included paths as a string` |
|        - | 13223 | ` */` |
|        2 | 13224 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13225 |  |
|        3 | 13226 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13227 | `	SyString *aEntry;` |
|        - | 13228 | `	int dir_sep;` |
|        - | 13229 | `	sxu32 n;` |
|        - | 13230 | `#ifdef __WINNT__` |
|        1 | 13231 | `	dir_sep = ';';` |
|        - | 13232 | `#else` |
|        - | 13233 | `	/* Assume UNIX path separator */` |
|        2 | 13234 | `	dir_sep = ':';` |
|        - | 13235 | `#endif` |
|        1 | 13236 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13237 | `	SXUNUSED(apArg);` |
|        - | 13238 | `	/* Point to the list of import paths */` |
|        3 | 13239 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 13240 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 13241 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 13242 | `		if( n > 0 ){` |
|        - | 13243 | `			/* Append dir seprator */` |
|      ! 0 | 13244 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 13245 | `		}` |
|        - | 13246 | `		/* Append path */` |
|        3 | 13247 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 13248 | `	}` |
|        3 | 13249 | `	return PH7_OK;` |
|        1 | 13250 |  |
|        - | 13251 | `/*` |
|        - | 13252 | ` * string get_get_included_files(void)` |
|        - | 13253 | ` *  Gets the current include_path configuration option.` |
|        - | 13254 | ` * Parameter` |
|        - | 13255 | ` *  None` |
|        - | 13256 | ` * Return` |
|        - | 13257 | ` *  Included paths as a string` |
|        - | 13258 | ` */` |
|        2 | 13259 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13260 |  |
|        3 | 13261 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 13262 | `	ph7_value *pArray,*pWorker;` |
|        - | 13263 | `	SyString *pEntry;` |
|        - | 13264 | `	int c,d;` |
|        - | 13265 | `	/* Create an array and a working value */` |
|        3 | 13266 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 13267 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 13268 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 13269 | `		/* Out of memory,return null */` |
|      ! 0 | 13270 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13271 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13272 | `		SXUNUSED(apArg);` |
|      ! 0 | 13273 | `		return PH7_OK;` |
|        - | 13274 | `	}` |
|        3 | 13275 | `	c = d = '/';` |
|        - | 13276 | `#ifdef __WINNT__` |
|        1 | 13277 | `	d = '\\';` |
|        - | 13278 | `#endif` |
|        - | 13279 | `	/* Iterate throw entries */` |
|        3 | 13280 | `	SySetResetCursor(pFiles);` |
|     3839 | 13281 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 13282 | `		const char *zBase,*zEnd;` |
|        - | 13283 | `		int iLen;` |
|        - | 13284 | `		/* reset the string cursor */` |
|     3837 | 13285 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 13286 | `		/* Extract base name */` |
|     3837 | 13287 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 13288 | `		/* Ignore trailing '/' */` |
|     5755 | 13289 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 13290 | `			zEnd--;` |
|      ! 0 | 13291 | `		}` |
|     3837 | 13292 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 13293 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 13294 | `			zEnd--;` |
|        1 | 13295 | `		}` |
|     3837 | 13296 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 13297 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 13298 | `		/* Copy entry name */` |
|     3837 | 13299 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 13300 | `		/* Perform the insertion */` |
|     3837 | 13301 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 13302 | `	}` |
|        - | 13303 | `	/* All done,return the created array */` |
|        3 | 13304 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13305 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 13306 | `	 * by the engine as soon we return from this foreign` |
|        - | 13307 | `	 * function.` |
|        - | 13308 | `	 */` |
|        3 | 13309 | `	return PH7_OK;` |
|        2 | 13310 |  |
|        - | 13311 | `/*` |
|        - | 13312 | ` * include:` |
|        - | 13313 | ` * According to the PHP reference manual.` |
|        - | 13314 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 13315 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 13316 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 13317 | ` *  include() will finally check in the calling script's own directory` |
|        - | 13318 | ` *  and the current working directory before failing. The include()` |
|        - | 13319 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 13320 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 13321 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 13322 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 13323 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 13324 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 13325 | ` *  directory to find the requested file.` |
|        - | 13326 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 13327 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 13328 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 13329 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 13330 | ` */` |
|     8900 | 13331 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13332 |  |
|        - | 13333 | `	SyString sFile;` |
|        - | 13334 | `	sxi32 rc;` |
|     8902 | 13335 | `	if( nArg < 1 ){` |
|        - | 13336 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13337 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13338 | `		return SXRET_OK;` |
|        - | 13339 | `	}` |
|        - | 13340 | `	/* File to include */` |
|     8902 | 13341 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8902 | 13342 | `	if( sFile.nByte < 1 ){` |
|        - | 13343 | `		/* Empty string,return NULL */` |
|      ! 0 | 13344 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13345 | `		return SXRET_OK;` |
|        - | 13346 | `	}` |
|        - | 13347 | `	/* Open,compile and execute the desired script */` |
|     8902 | 13348 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8902 | 13349 | `	if( rc != SXRET_OK ){` |
|        - | 13350 | `		/* Emit a warning and return false */` |
|        3 | 13351 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 13352 | `		ph7_result_bool(pCtx,0);` |
|        1 | 13353 | `	}` |
|     8902 | 13354 | `	return SXRET_OK;` |
|     4452 | 13355 |  |
|        - | 13356 | `/*` |
|        - | 13357 | ` * include_once:` |
|        - | 13358 | ` *  According to the PHP reference manual.` |
|        - | 13359 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 13360 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 13361 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 13362 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 13363 | ` *   just once.` |
|        - | 13364 | ` */` |
|        4 | 13365 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13366 |  |
|        - | 13367 | `	SyString sFile;` |
|        - | 13368 | `	sxi32 rc;` |
|        5 | 13369 | `	if( nArg < 1 ){` |
|        - | 13370 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13371 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13372 | `		return SXRET_OK;` |
|        - | 13373 | `	}` |
|        - | 13374 | `	/* File to include */` |
|        5 | 13375 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13376 | `	if( sFile.nByte < 1 ){` |
|        - | 13377 | `		/* Empty string,return NULL */` |
|      ! 0 | 13378 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13379 | `		return SXRET_OK;` |
|        - | 13380 | `	}` |
|        - | 13381 | `	/* Open,compile and execute the desired script */` |
|        5 | 13382 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13383 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13384 | `		/* File already included,return TRUE */` |
|        3 | 13385 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13386 | `		return SXRET_OK;` |
|        - | 13387 | `	}` |
|        3 | 13388 | `	if( rc != SXRET_OK ){` |
|        - | 13389 | `		/* Emit a warning and return false */` |
|      ! 0 | 13390 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13391 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13392 | ` 	}` |
|        3 | 13393 | `	return SXRET_OK;` |
|        3 | 13394 |  |
|        - | 13395 | `/*` |
|        - | 13396 | ` * require.` |
|        - | 13397 | ` *  According to the PHP reference manual.` |
|        - | 13398 | ` *   require() is identical to include() except upon failure it will` |
|        - | 13399 | ` *   also produce a fatal level error.` |
|        - | 13400 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 13401 | ` *   emits a warning  which allows the script to continue.` |
|        - | 13402 | ` */` |
|        6 | 13403 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13404 |  |
|        - | 13405 | `	SyString sFile;` |
|        - | 13406 | `	sxi32 rc;` |
|        8 | 13407 | `	if( nArg < 1 ){` |
|        - | 13408 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13409 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13410 | `		return SXRET_OK;` |
|        - | 13411 | `	}` |
|        - | 13412 | `	/* File to include */` |
|        8 | 13413 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 13414 | `	if( sFile.nByte < 1 ){` |
|        - | 13415 | `		/* Empty string,return NULL */` |
|      ! 0 | 13416 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13417 | `		return SXRET_OK;` |
|        - | 13418 | `	}` |
|        - | 13419 | `	/* Open,compile and execute the desired script */` |
|        8 | 13420 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 13421 | `	if( rc != SXRET_OK ){` |
|        - | 13422 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13423 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13424 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13425 | `		return PH7_ABORT;` |
|        - | 13426 | `	}` |
|        8 | 13427 | `	return SXRET_OK;` |
|        5 | 13428 |  |
|        - | 13429 | `/*` |
|        - | 13430 | ` * require_once:` |
|        - | 13431 | ` *  According to the PHP reference manual.` |
|        - | 13432 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 13433 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 13434 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 13435 | ` *   and how it differs from its non _once siblings.` |
|        - | 13436 | ` */` |
|        4 | 13437 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13438 |  |
|        - | 13439 | `	SyString sFile;` |
|        - | 13440 | `	sxi32 rc;` |
|        5 | 13441 | `	if( nArg < 1 ){` |
|        - | 13442 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13443 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13444 | `		return SXRET_OK;` |
|        - | 13445 | `	}` |
|        - | 13446 | `	/* File to include */` |
|        5 | 13447 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13448 | `	if( sFile.nByte < 1 ){` |
|        - | 13449 | `		/* Empty string,return NULL */` |
|      ! 0 | 13450 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13451 | `		return SXRET_OK;` |
|        - | 13452 | `	}` |
|        - | 13453 | `	/* Open,compile and execute the desired script */` |
|        5 | 13454 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13455 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13456 | `		/* File already included,return TRUE */` |
|        3 | 13457 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13458 | `		return SXRET_OK;` |
|        - | 13459 | `	}` |
|        3 | 13460 | `	if( rc != SXRET_OK ){` |
|        - | 13461 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13462 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13463 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13464 | `		return PH7_ABORT;` |
|        - | 13465 | `	}` |
|        3 | 13466 | `	return SXRET_OK;` |
|        3 | 13467 |  |
|        - | 13468 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 13469 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 13470 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 13471 | `/*` |
|        - | 13472 | ` * Section:` |
|        - | 13473 | ` *  SPL Autoloading functions.` |
|        - | 13474 | ` * Status:` |
|        - | 13475 | ` *  Stable.` |
|        - | 13476 | ` */` |
|        - | 13477 | `/*` |
|        - | 13478 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 13479 | ` *  Register given function as __autoload() implementation.` |
|        - | 13480 | ` * Parameters` |
|        - | 13481 | ` *  callback` |
|        - | 13482 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 13483 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 13484 | ` *  throw` |
|        - | 13485 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 13486 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 13487 | ` *  prepend` |
|        - | 13488 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 13489 | ` *   autoload stack instead of appending it.` |
|        - | 13490 | ` * Return` |
|        - | 13491 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13492 | ` */` |
|       34 | 13493 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13494 |  |
|        - | 13495 | `	VmAutoloadCB sEntry;` |
|       36 | 13496 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 13497 | `	int iPrepend = 0;` |
|        - | 13498 | `	sxu32 n;` |
|       36 | 13499 | `	if( nArg < 1 ){` |
|        - | 13500 | `		/* No callback provided — register default spl_autoload.` |
|        - | 13501 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 13502 | `		/* Check for duplicates first */` |
|        9 | 13503 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 13504 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 13505 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 13506 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 13507 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 13508 | `				ph7_result_bool(pCtx,1);` |
|        5 | 13509 | `				return SXRET_OK;` |
|        - | 13510 | `			}` |
|      ! 0 | 13511 | `		}` |
|        5 | 13512 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 13513 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 13514 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 13515 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 13516 | `		ph7_result_bool(pCtx,1);` |
|        5 | 13517 | `		return SXRET_OK;` |
|        - | 13518 | `	}` |
|        - | 13519 | `	/* Validate that the callback is callable */` |
|       28 | 13520 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 13521 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 13522 | `		if( nArg >= 2 ){` |
|      ! 0 | 13523 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13524 | `		}` |
|      ! 0 | 13525 | `		if( iThrow ){` |
|      ! 0 | 13526 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 13527 | `				"Argument is not callable");` |
|      ! 0 | 13528 | `		}` |
|      ! 0 | 13529 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13530 | `		return SXRET_OK;` |
|        - | 13531 | `	}` |
|        - | 13532 | `	/* Check for duplicates */` |
|       46 | 13533 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 13534 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 13535 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13536 | `			/* Already registered */` |
|      ! 0 | 13537 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13538 | `			return SXRET_OK;` |
|        - | 13539 | `		}` |
|       11 | 13540 | `	}` |
|        - | 13541 | `	/* Check prepend flag */` |
|       28 | 13542 | `	if( nArg >= 3 ){` |
|        3 | 13543 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 13544 | `	}` |
|        - | 13545 | `	/* Store the callback */` |
|       28 | 13546 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 13547 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 13548 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 13549 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 13550 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 13551 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 13552 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 13553 | `		VmAutoloadCB *aBase;` |
|        3 | 13554 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13555 | `		/* Rotate: move last entry to front */` |
|        3 | 13556 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 13557 | `		if( aBase ){` |
|        - | 13558 | `			VmAutoloadCB sTemp;` |
|        - | 13559 | `			sxu32 i;` |
|        3 | 13560 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 13561 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 13562 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 13563 | `			}` |
|        3 | 13564 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 13565 | `		}` |
|        2 | 13566 | `	}else{` |
|       26 | 13567 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13568 | `	}` |
|       28 | 13569 | `	ph7_result_bool(pCtx,1);` |
|       28 | 13570 | `	return SXRET_OK;` |
|       19 | 13571 |  |
|        - | 13572 | `/*` |
|        - | 13573 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 13574 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 13575 | ` * Parameters` |
|        - | 13576 | ` *  callback` |
|        - | 13577 | ` *   The autoload function being unregistered.` |
|        - | 13578 | ` * Return` |
|        - | 13579 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13580 | ` */` |
|       32 | 13581 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13582 |  |
|       34 | 13583 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13584 | `	sxu32 n,nEntry;` |
|       34 | 13585 | `	if( nArg < 1 ){` |
|      ! 0 | 13586 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13587 | `		return SXRET_OK;` |
|        - | 13588 | `	}` |
|       34 | 13589 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13590 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 13591 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 13592 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13593 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 13594 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 13595 | `			sxu32 i;` |
|       32 | 13596 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 13597 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 13598 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 13599 | `			}` |
|        - | 13600 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 13601 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 13602 | `			ph7_result_bool(pCtx,1);` |
|       32 | 13603 | `			return SXRET_OK;` |
|        - | 13604 | `		}` |
|        3 | 13605 | `	}` |
|        3 | 13606 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13607 | `	return SXRET_OK;` |
|       18 | 13608 |  |
|        - | 13609 | `/*` |
|        - | 13610 | ` * array spl_autoload_functions(void)` |
|        - | 13611 | ` *  Return all registered __autoload() functions.` |
|        - | 13612 | ` * Return` |
|        - | 13613 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 13614 | ` *  an empty array is returned.` |
|        - | 13615 | ` */` |
|       20 | 13616 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13617 |  |
|       21 | 13618 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13619 | `	ph7_value *pArray;` |
|        - | 13620 | `	sxu32 n,nEntry;` |
|       10 | 13621 | `	SXUNUSED(nArg);` |
|       10 | 13622 | `	SXUNUSED(apArg);` |
|       21 | 13623 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 13624 | `	if( pArray == 0 ){` |
|      ! 0 | 13625 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13626 | `		return SXRET_OK;` |
|        - | 13627 | `	}` |
|       21 | 13628 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 13629 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 13630 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 13631 | `		if( pEntry ){` |
|       15 | 13632 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 13633 | `		}` |
|        8 | 13634 | `	}` |
|       21 | 13635 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 13636 | `	return SXRET_OK;` |
|       11 | 13637 |  |
|        - | 13638 | `/*` |
|        - | 13639 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 13640 | ` *  Default implementation of __autoload().` |
|        - | 13641 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 13642 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 13643 | ` * Parameters` |
|        - | 13644 | ` *  class` |
|        - | 13645 | ` *   The class name being searched.` |
|        - | 13646 | ` *  file_extensions` |
|        - | 13647 | ` *   Comma-separated list of file extensions to try.` |
|        - | 13648 | ` */` |
|        2 | 13649 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13650 |  |
|        - | 13651 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 13652 | `	SyBlob sPath;` |
|        - | 13653 | `	int nClass;` |
|        - | 13654 | `	sxi32 rc;` |
|        3 | 13655 | `	if( nArg < 1 ){` |
|      ! 0 | 13656 | `		return SXRET_OK;` |
|        - | 13657 | `	}` |
|        3 | 13658 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 13659 | `	if( nClass < 1 ){` |
|      ! 0 | 13660 | `		return SXRET_OK;` |
|        - | 13661 | `	}` |
|        - | 13662 | `	/* Default extensions */` |
|        3 | 13663 | `	zExt = ".php,.inc";` |
|        3 | 13664 | `	if( nArg >= 2 ){` |
|        - | 13665 | `		int nExt;` |
|      ! 0 | 13666 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 13667 | `		if( nExt < 1 ){` |
|      ! 0 | 13668 | `			zExt = ".php,.inc";` |
|      ! 0 | 13669 | `		}` |
|      ! 0 | 13670 | `	}` |
|        3 | 13671 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 13672 | `	/* Iterate over comma-separated extensions */` |
|        3 | 13673 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 13674 | `	zCur = zExt;` |
|        7 | 13675 | `	while( zCur < zEnd ){` |
|        - | 13676 | `		const char *zComma;` |
|        - | 13677 | `		SyString sFile;` |
|        - | 13678 | `		int i;` |
|        - | 13679 | `		/* Find next comma or end */` |
|        5 | 13680 | `		zComma = zCur;` |
|       21 | 13681 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 13682 | `			zComma++;` |
|        1 | 13683 | `		}` |
|        - | 13684 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 13685 | `		SyBlobReset(&sPath);` |
|       69 | 13686 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 13687 | `			char c = zClass[i];` |
|       65 | 13688 | `			if( c == '\\' ){` |
|      ! 0 | 13689 | `				c = '/';` |
|       65 | 13690 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 13691 | `				c = c + ('a' - 'A');` |
|        6 | 13692 | `			}` |
|       65 | 13693 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 13694 | `		}` |
|        - | 13695 | `		/* Append extension */` |
|        5 | 13696 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 13697 | `		/* Try to include the file */` |
|        5 | 13698 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 13699 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 13700 | `		if( rc == SXRET_OK ){` |
|        - | 13701 | `			/* File included successfully */` |
|      ! 0 | 13702 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 13703 | `			return SXRET_OK;` |
|        - | 13704 | `		}` |
|        - | 13705 | `		/* Move past the comma */` |
|        5 | 13706 | `		zCur = zComma;` |
|        5 | 13707 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 13708 | `			zCur++;` |
|        1 | 13709 | `		}` |
|        1 | 13710 | `	}` |
|        3 | 13711 | `	SyBlobRelease(&sPath);` |
|        3 | 13712 | `	return SXRET_OK;` |
|        2 | 13713 |  |
|        - | 13714 | `/* Table of built-in VM functions. */` |
|        - | 13715 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13716 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13717 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13718 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13719 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13720 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13721 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13722 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13723 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13724 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13725 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13726 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13727 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13728 | `	    /* Constants management */` |
|        - | 13729 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13730 | `	{ "define",   vm_builtin_define               },` |
|        - | 13731 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13732 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13733 | `	   /* Class/Object functions */` |
|        - | 13734 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13735 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13736 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13737 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13738 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13739 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13740 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13741 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13742 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13743 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13744 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13745 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13746 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13747 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13748 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13749 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13750 | `	   /* SPL Autoloading */` |
|        - | 13751 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 13752 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 13753 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 13754 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 13755 | `	   /* Random numbers/strings generators */` |
|        - | 13756 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13757 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13758 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13759 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13760 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13761 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13762 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13763 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13764 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13765 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13766 | `	   /* Language constructs functions */` |
|        - | 13767 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13768 | `	{ "print", vm_builtin_print                   },` |
|        - | 13769 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13770 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13771 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13772 | `	  /* Variable handling functions */` |
|        - | 13773 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13774 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13775 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13776 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13777 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13778 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13779 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13780 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13781 | `	  /* Ouput control functions */` |
|        - | 13782 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13783 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13784 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13785 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13786 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13787 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13788 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13789 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13790 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13791 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13792 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13793 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13794 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13795 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13796 | `	  /* Assertion functions */` |
|        - | 13797 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13798 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13799 | `	  /* Error reporting functions */` |
|        - | 13800 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13801 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13802 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13803 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13804 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13805 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13806 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13807 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13808 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13809 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13810 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13811 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13812 | `	  /* Release info */` |
|        - | 13813 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13814 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13815 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13816 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13817 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13818 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13819 | `	  /* hashmap */` |
|        - | 13820 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13821 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13822 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13823 | `	  /* URL related function */` |
|        - | 13824 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13825 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13826 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13827 | `	   /* XML processing functions */` |
|        - | 13828 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13829 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13830 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 13831 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 13832 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 13833 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 13834 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13835 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13836 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13837 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13838 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13839 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 13840 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 13841 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 13842 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 13843 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 13844 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 13845 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 13846 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 13847 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 13848 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 13849 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13850 | `	   /* UTF-8 encoding/decoding */` |
|        - | 13851 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 13852 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 13853 | `	   /* Command line processing */` |
|        - | 13854 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 13855 | `	   /* JSON encoding/decoding */` |
|        - | 13856 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 13857 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 13858 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 13859 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 13860 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 13861 | `	   /* Files/URI inclusion facility */` |
|        - | 13862 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 13863 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 13864 | `	{ "include",      vm_builtin_include          },` |
|        - | 13865 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 13866 | `	{ "require",      vm_builtin_require          },` |
|        - | 13867 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 13868 | `};` |
|        - | 13869 | `/*` |
|        - | 13870 | ` * Register the built-in VM functions defined above.` |
|        - | 13871 | ` */` |
|     2612 | 13872 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 13873 |  |
|        - | 13874 | `	sxi32 rc;` |
|        - | 13875 | `	sxu32 n;` |
|   336950 | 13876 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 13877 | `		/* Note that these special functions have access` |
|        - | 13878 | `		 * to the underlying virtual machine as their` |
|        - | 13879 | `		 * private data.` |
|        - | 13880 | `		 */` |
|   334338 | 13881 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   334338 | 13882 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13883 | `			return rc;` |
|        - | 13884 | `		}` |
|   167170 | 13885 | `	}` |
|     2614 | 13886 | `	return SXRET_OK;` |
|     1308 | 13887 |  |
|        - | 13888 | `/*` |
|        - | 13889 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 13890 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 13891 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 13892 | ` */` |
|    40502 | 13893 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 13894 |  |
|    40504 | 13895 | `	if( !iLoadable ){` |
|    38822 | 13896 | `		return pClass;` |
|        - | 13897 | `	}` |
|     1688 | 13898 | `	while(pClass){` |
|     1684 | 13899 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1680 | 13900 | `			return pClass;` |
|        - | 13901 | `		}` |
|        5 | 13902 | `		pClass = pClass->pNextName;` |
|        1 | 13903 | `	}` |
|        5 | 13904 | `	return 0;` |
|    20253 | 13905 |  |
|        - | 13906 | `/*` |
|        - | 13907 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 13908 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 13909 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 13910 | ` * registered in the VM's class table.` |
|        - | 13911 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 13912 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 13913 | ` */` |
|       38 | 13914 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13915 |  |
|        - | 13916 | `	VmAutoloadCB *pEntry;` |
|        - | 13917 | `	ph7_value sArg,sResult;` |
|        - | 13918 | `	SyHashEntry *pHashEntry;` |
|        - | 13919 | `	ph7_class *pClass;` |
|        - | 13920 | `	sxu32 n,nEntry;` |
|       40 | 13921 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 13922 | `	if( nEntry < 1 ){` |
|       26 | 13923 | `		return 0;` |
|        - | 13924 | `	}` |
|        - | 13925 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 13926 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 13927 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 13928 | `	}` |
|        - | 13929 | `	/* Mark this class as being autoloaded */` |
|       14 | 13930 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 13931 | `	/* Prepare the class name argument */` |
|       14 | 13932 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 13933 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 13934 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 13935 | `	pClass = 0;` |
|       28 | 13936 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 13937 | `		ph7_value *apArg[1];` |
|       24 | 13938 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 13939 | `		if( pEntry == 0 ){` |
|      ! 0 | 13940 | `			continue;` |
|        - | 13941 | `		}` |
|       24 | 13942 | `		apArg[0] = &sArg;` |
|       24 | 13943 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 13944 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 13945 | `			continue;` |
|        - | 13946 | `		}` |
|        - | 13947 | `		/* Check if the class is now available */` |
|       24 | 13948 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 13949 | `		if( pHashEntry ){` |
|       10 | 13950 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 13951 | `			if( pClass ){` |
|       10 | 13952 | `				break;` |
|        - | 13953 | `			}` |
|      ! 0 | 13954 | `		}` |
|        9 | 13955 | `	}` |
|       14 | 13956 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 13957 | `	PH7_MemObjRelease(&sResult);` |
|        - | 13958 | `	/* Remove reentrancy guard */` |
|       14 | 13959 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 13960 | `	return pClass;` |
|       21 | 13961 |  |
|        - | 13962 | `/*` |
|        - | 13963 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 13964 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 13965 | ` */` |
|       18 | 13966 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13967 |  |
|       20 | 13968 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 13969 |  |
|        - | 13970 | `/*` |
|        - | 13971 | ` * Check if the given name refer to an installed class.` |
|        - | 13972 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 13973 | ` */` |
|    40514 | 13974 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 13975 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 13976 | `	const char *zName,  /* Name of the target class */` |
|        - | 13977 | `	sxu32 nByte,        /* zName length */` |
|        - | 13978 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 13979 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 13980 | `						 */` |
|        - | 13981 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 13982 | `	)` |
|        2 | 13983 |  |
|        - | 13984 | `	SyHashEntry *pEntry;` |
|        - | 13985 | `	ph7_class *pClass;` |
|    20257 | 13986 | `	SXUNUSED(iNest);` |
|        - | 13987 | `	/* Exact class lookup.` |
|        - | 13988 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 13989 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    40516 | 13990 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    40516 | 13991 | `	if( pEntry == 0 ){` |
|        - | 13992 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 13993 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 13994 | `	}` |
|    40496 | 13995 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    40496 | 13996 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    20259 | 13997 |  |
|        - | 13998 | `/*` |
|        - | 13999 | ` * Reference Table Implementation` |
|        - | 14000 | ` * Status: stable <chm@symisc.net>` |
|        - | 14001 | ` * Intro` |
|        - | 14002 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14003 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14004 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14005 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14006 | ` *  Refer to the official for more information on this powerful` |
|        - | 14007 | ` *  extension.` |
|        - | 14008 | ` */` |
|        - | 14009 | `/*` |
|        - | 14010 | ` * Allocate a new reference entry.` |
|        - | 14011 | ` */` |
|  3118286 | 14012 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14013 |  |
|        - | 14014 | `	VmRefObj *pRef;` |
|        - | 14015 | `	/* Allocate a new instance */` |
|  3118288 | 14016 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3118288 | 14017 | `	if( pRef == 0 ){` |
|      ! 0 | 14018 | `		return 0;` |
|        - | 14019 | `	}` |
|        - | 14020 | `	/* Zero the structure */` |
|  3118288 | 14021 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14022 | `	/* Initialize fields */` |
|  3118288 | 14023 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3118288 | 14024 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3118288 | 14025 | `	pRef->nIdx = nIdx;` |
|  3118288 | 14026 | `	return pRef;` |
|  1559145 | 14027 |  |
|        - | 14028 | `/*` |
|        - | 14029 | ` * Default hash function used by the reference table` |
|        - | 14030 | ` * for lookup/insertion operations.` |
|        - | 14031 | ` */` |
| 17169606 | 14032 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14033 |  |
|        - | 14034 | `	/* Calculate the hash based on the memory object index */` |
| 17169608 | 14035 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14036 |  |
|        - | 14037 | `/*` |
|        - | 14038 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14039 | ` * in the reference table.` |
|        - | 14040 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14041 | ` * otherwise.` |
|        - | 14042 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14043 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14044 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14045 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14046 | ` * Refer to the official for more information on this powerful` |
|        - | 14047 | ` * extension.` |
|        - | 14048 | ` */` |
|  9300708 | 14049 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14050 |  |
|        - | 14051 | `	VmRefObj *pRef;` |
|        - | 14052 | `	sxu32 nBucket;` |
|        - | 14053 | `	/* Point to the appropriate bucket */` |
|  9300710 | 14054 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14055 | `	/* Perform the lookup */` |
|  9300710 | 14056 | `	pRef = pVm->apRefObj[nBucket];` |
| 20212033 | 14057 | `	for(;;){` |
| 40410356 | 14058 | `		if( pRef == 0 ){` |
|  3212084 | 14059 | `			break;` |
|        - | 14060 | `		}` |
| 37198274 | 14061 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14062 | `			/* Entry found */` |
|  6088628 | 14063 | `			return pRef;` |
|        - | 14064 | `		}` |
|        - | 14065 | `		/* Point to the next entry */` |
| 31109648 | 14066 | `		pRef = pRef->pNextCollide;` |
|        2 | 14067 | `	}` |
|        - | 14068 | `	/* No such entry,return NULL */` |
|  3212084 | 14069 | `	return 0;` |
|  4650356 | 14070 |  |
|        - | 14071 | `/*` |
|        - | 14072 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14073 | ` *` |
|        - | 14074 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14075 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14076 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14077 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14078 | ` * Refer to the official for more information on this powerful` |
|        - | 14079 | ` * extension.` |
|        - | 14080 | ` */` |
|  3118286 | 14081 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14082 |  |
|        - | 14083 | `	sxu32 nBucket;` |
|  3118288 | 14084 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14085 | `		VmRefObj **apNew;` |
|        - | 14086 | `		sxu32 nNew;` |
|        - | 14087 | `		/* Allocate a larger table */` |
|     4432 | 14088 | `		nNew = pVm->nRefSize << 1;` |
|     4432 | 14089 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4432 | 14090 | `		if( apNew ){` |
|     4432 | 14091 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14092 | `			sxu32 n;` |
|        - | 14093 | `			/* Zero the structure */` |
|     4432 | 14094 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14095 | `			/* Rehash all referenced entries */` |
|  2845252 | 14096 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14097 | `				/* Remove old collision links */` |
|  2840822 | 14098 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14099 | `				/* Point to the appropriate bucket */` |
|  2840822 | 14100 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14101 | `				/* Insert the entry  */` |
|  2840822 | 14102 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2840822 | 14103 | `				if( apNew[nBucket] ){` |
|  2298896 | 14104 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 14105 | `				}` |
|  2840822 | 14106 | `				apNew[nBucket] = pEntry;` |
|        - | 14107 | `				/* Point to the next entry */` |
|  2840822 | 14108 | `				pEntry = pEntry->pNext;` |
|  1420412 | 14109 | `			}` |
|        - | 14110 | `			/* Release the old table */` |
|     4432 | 14111 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14112 | `			/* Install the new one */` |
|     4432 | 14113 | `			pVm->apRefObj = apNew;` |
|     4432 | 14114 | `			pVm->nRefSize = nNew;` |
|     2215 | 14115 | `		}` |
|     2215 | 14116 | `	}` |
|        - | 14117 | `	/* Point to the appropriate bucket */` |
|  3118288 | 14118 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14119 | `	/* Insert the entry */` |
|  3118288 | 14120 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3118288 | 14121 | `	if( pVm->apRefObj[nBucket] ){` |
|  2556913 | 14122 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1278483 | 14123 | `	}` |
|  3118288 | 14124 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3118288 | 14125 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3118288 | 14126 | `	pVm->nRefUsed++;` |
|  3118288 | 14127 | `	return SXRET_OK;` |
|        2 | 14128 |  |
|        - | 14129 | `/*` |
|        - | 14130 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14131 | ` * the reference table.` |
|        - | 14132 | ` * This function is invoked when the user perform an unset` |
|        - | 14133 | ` * call [i.e: unset($var); ].` |
|        - | 14134 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14135 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14136 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14137 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14138 | ` * Refer to the official for more information on this powerful` |
|        - | 14139 | ` * extension.` |
|        - | 14140 | ` */` |
|  3080298 | 14141 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14142 |  |
|        - | 14143 | `	ph7_hashmap_node **apNode;` |
|        - | 14144 | `	SyHashEntry **apEntry;` |
|        - | 14145 | `	sxu32 n;` |
|        - | 14146 | `	/* Point to the reference table */` |
|  3080300 | 14147 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3080300 | 14148 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14149 | `	/* Unlink the entry from the reference table */` |
|  3180574 | 14150 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   100276 | 14151 | `		if( apEntry[n] ){` |
|   100226 | 14152 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    50112 | 14153 | `		}` |
|    50139 | 14154 | `	}` |
|  6062178 | 14155 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2981880 | 14156 | `		if( apNode[n] ){` |
|     7406 | 14157 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3702 | 14158 | `		}` |
|  1490941 | 14159 | `	}` |
|  3080300 | 14160 | `	if( pRef->pPrevCollide ){` |
|  1170508 | 14161 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   585706 | 14162 | `	}else{` |
|  1909794 | 14163 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14164 | `	}` |
|  3080300 | 14165 | `	if( pRef->pNextCollide ){` |
|  1744060 | 14166 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   872030 | 14167 | `	}` |
|  3080300 | 14168 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14169 | `	/* Release the node */` |
|  3080300 | 14170 | `	SySetRelease(&pRef->aReference);` |
|  3080300 | 14171 | `	SySetRelease(&pRef->aArrEntries);` |
|  3080300 | 14172 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3080300 | 14173 | `	pVm->nRefUsed--;` |
|  3080300 | 14174 | `	return SXRET_OK;` |
|        2 | 14175 |  |
|        - | 14176 | `/*` |
|        - | 14177 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14178 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14179 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14180 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14181 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14182 | ` * Refer to the official for more information on this powerful` |
|        - | 14183 | ` * extension.` |
|        - | 14184 | ` */` |
|  3152074 | 14185 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14186 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14187 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14188 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14189 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14190 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14191 | `	)` |
|        2 | 14192 |  |
|  3152076 | 14193 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14194 | `	VmRefObj *pRef;` |
|        - | 14195 | `	/* Check if the referenced object already exists */` |
|  3152076 | 14196 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3152076 | 14197 | `	if( pRef == 0 ){` |
|        - | 14198 | `		/* Create a new entry */` |
|  3118288 | 14199 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3118288 | 14200 | `		if( pRef == 0 ){` |
|      ! 0 | 14201 | `			return SXERR_MEM;` |
|        - | 14202 | `		}` |
|  3118288 | 14203 | `		pRef->iFlags = iFlags;` |
|        - | 14204 | `		/* Install the entry */` |
|  3118288 | 14205 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1559143 | 14206 | `	}` |
|  3152076 | 14207 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3152076 | 14208 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14209 | `		VmSlot sRef;` |
|        - | 14210 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14211 | `		 * be deleted when we leave this frame.` |
|        - | 14212 | `		 */` |
|    93894 | 14213 | `		sRef.nIdx = nIdx;` |
|    93894 | 14214 | `		sRef.pUserData = pEntry;` |
|    93894 | 14215 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14216 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14217 | `		}` |
|    46946 | 14218 | `	}` |
|  3152076 | 14219 | `	if( pEntry ){` |
|        - | 14220 | `		/* Address of the hash-entry */` |
|   127482 | 14221 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    63740 | 14222 | `	}` |
|  3152076 | 14223 | `	if( pMapEntry ){` |
|        - | 14224 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3017636 | 14225 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1508817 | 14226 | `	}` |
|  3152076 | 14227 | `	return SXRET_OK;` |
|  1576039 | 14228 |  |
|        - | 14229 | `/*` |
|        - | 14230 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14231 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14232 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14233 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14234 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14235 | ` * Refer to the official for more information on this powerful` |
|        - | 14236 | ` * extension.` |
|        - | 14237 | ` */` |
|  3068330 | 14238 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14239 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14240 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14241 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14242 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14243 | `	)` |
|        2 | 14244 |  |
|        - | 14245 | `	VmRefObj *pRef;` |
|        - | 14246 | `	sxu32 n;` |
|        - | 14247 | `	/* Check if the referenced object already exists */` |
|  3068332 | 14248 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3068332 | 14249 | `	if( pRef == 0 ){` |
|        - | 14250 | `		/* Not such entry */` |
|    93792 | 14251 | `		return SXERR_NOTFOUND;` |
|        - | 14252 | `	}` |
|        - | 14253 | `	/* Remove the desired entry */` |
|  2974542 | 14254 | `	if( pEntry ){` |
|        - | 14255 | `		SyHashEntry **apEntry;` |
|       62 | 14256 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 14257 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 14258 | `			if( apEntry[n] == pEntry ){` |
|        - | 14259 | `				/* Nullify the entry */` |
|       62 | 14260 | `				apEntry[n] = 0;` |
|        - | 14261 | `				/*` |
|        - | 14262 | `				 * NOTE:` |
|        - | 14263 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14264 | `				 * we avoid wasting spaces.` |
|        - | 14265 | `				 */` |
|       30 | 14266 | `			}` |
|       85 | 14267 | `		}` |
|       30 | 14268 | `	}` |
|  2974542 | 14269 | `	if( pMapEntry ){` |
|        - | 14270 | `		ph7_hashmap_node **apNode;` |
|  2974482 | 14271 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5949056 | 14272 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2974576 | 14273 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14274 | `				/* nullify the entry */` |
|  2974482 | 14275 | `				apNode[n] = 0;` |
|  1487240 | 14276 | `			}` |
|  1487289 | 14277 | `		}` |
|  1487240 | 14278 | `	}` |
|  2974542 | 14279 | `	return SXRET_OK;` |
|  1534167 | 14280 |  |
|        - | 14281 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14282 | `/*` |
|        - | 14283 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14284 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14285 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14286 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14287 | ` * For more information on how to register IO stream devices,please` |
|        - | 14288 | ` * refer to the official documentation.` |
|        - | 14289 | ` */` |
|    27088 | 14290 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14291 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14292 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14293 | `	int nByte              /* *pzDevice length*/` |
|        - | 14294 | `	)` |
|        2 | 14295 |  |
|        - | 14296 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14297 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14298 | `	SyString sDev,sCur;` |
|        - | 14299 | `	sxu32 n,nEntry;` |
|        - | 14300 | `	int rc;` |
|        - | 14301 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    27090 | 14302 | `	zNext = zCur = zIn = *pzDevice;` |
|    27090 | 14303 | `	zEnd = &zIn[nByte];` |
|  1719352 | 14304 | `	while( zIn < zEnd ){` |
|  1692266 | 14305 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14306 | `			/* Got one */` |
|        3 | 14307 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14308 | `			break;` |
|        - | 14309 | `		}` |
|        - | 14310 | `		/* Advance the cursor */` |
|  1692264 | 14311 | `		zIn++;` |
|        2 | 14312 | `	}` |
|    27090 | 14313 | `	if( zIn >= zEnd ){` |
|        - | 14314 | `		/* No such scheme,return the default stream */` |
|    27088 | 14315 | `		return pVm->pDefStream;` |
|        - | 14316 | `	}` |
|        3 | 14317 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14318 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14319 | `	SyStringFullTrim(&sDev);` |
|        - | 14320 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14321 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14322 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14323 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14324 | `		pStream = apStream[n];` |
|        3 | 14325 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14326 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14327 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14328 | `		if( rc == 0 ){` |
|        - | 14329 | `			/* Stream device found */` |
|        3 | 14330 | `			*pzDevice = zNext;` |
|        3 | 14331 | `			return pStream;` |
|        - | 14332 | `		}` |
|      ! 0 | 14333 | `	}` |
|        - | 14334 | `	/* No such stream,return NULL */` |
|      ! 0 | 14335 | `	return 0;` |
|    13546 | 14336 |  |
|        - | 14337 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14338 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 14339 |  |
