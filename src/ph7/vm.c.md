# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5789/7529 lines (76.89%)

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
|   854958 |    96 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    97 |  |
|   854960 |    98 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |    99 | `		return TRUE;` |
|        - |   100 | `	}` |
|   854926 |   101 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   102 | `		return TRUE;` |
|        - |   103 | `	}` |
|   854916 |   104 | `	return FALSE;` |
|   427503 |   105 |  |
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
|   560320 |   120 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   560322 |   131 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   560322 |   132 | `	if( pEntry ){` |
|        - |   133 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   134 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   135 | `		pCons->xExpand = xExpand;` |
|        6 |   136 | `		pCons->pUserData = pUserData;` |
|        6 |   137 | `		return SXRET_OK;` |
|        - |   138 | `	}` |
|        - |   139 | `	/* Allocate a new constant instance */` |
|   560318 |   140 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   560318 |   141 | `	if( pCons == 0 ){` |
|      ! 0 |   142 | `		return 0;` |
|        - |   143 | `	}` |
|        - |   144 | `	/* Duplicate constant name */` |
|   560318 |   145 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   560318 |   146 | `	if( zDupName == 0 ){` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return 0;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* Install the constant */` |
|   560318 |   151 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   560318 |   152 | `	pCons->xExpand = xExpand;` |
|   560318 |   153 | `	pCons->pUserData = pUserData;` |
|   560318 |   154 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   560318 |   155 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   156 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   157 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   158 | `		return rc;` |
|        - |   159 | `	}` |
|        - |   160 | `	/* All done,constant can be invoked from PHP code */` |
|   560318 |   161 | `	return SXRET_OK;` |
|   280162 |   162 |  |
|        - |   163 | `/*` |
|        - |   164 | ` * Allocate a new foreign function instance.` |
|        - |   165 | ` * This function return SXRET_OK on success. Any other` |
|        - |   166 | ` * return value indicates failure.` |
|        - |   167 | ` * Please refer to the official documentation for an introduction to` |
|        - |   168 | ` * the foreign function mechanism.` |
|        - |   169 | ` */` |
|  1231846 |   170 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1231848 |   181 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1231848 |   182 | `	if( pFunc == 0 ){` |
|      ! 0 |   183 | `		return SXERR_MEM;` |
|        - |   184 | `	}` |
|        - |   185 | `	/* Duplicate function name */` |
|  1231848 |   186 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1231848 |   187 | `	if( zDup == 0 ){` |
|      ! 0 |   188 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   189 | `		return SXERR_MEM;` |
|        - |   190 | `	}` |
|        - |   191 | `	/* Zero the structure */` |
|  1231848 |   192 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   193 | `	/* Initialize structure fields */` |
|  1231848 |   194 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1231848 |   195 | `	pFunc->pVm   = pVm;` |
|  1231848 |   196 | `	pFunc->xFunc = xFunc;` |
|  1231848 |   197 | `	pFunc->pUserData = pUserData;` |
|  1231848 |   198 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   199 | `	/* Write a pointer to the new function */` |
|  1231848 |   200 | `	*ppOut = pFunc;` |
|  1231848 |   201 | `	return SXRET_OK;` |
|   615925 |   202 |  |
|        - |   203 | `/*` |
|        - |   204 | ` * Install a foreign function and it's associated callback so that` |
|        - |   205 | ` * it can be invoked from the target PHP code.` |
|        - |   206 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   207 | ` * return value indicates failure.` |
|        - |   208 | ` * Please refer to the official documentation for an introduction to` |
|        - |   209 | ` * the foreign function mechanism.` |
|        - |   210 | ` */` |
|  1234428 |   211 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1234430 |   222 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1234430 |   223 | `	if( pEntry ){` |
|     2584 |   224 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2584 |   225 | `		pFunc->pUserData = pUserData;` |
|     2584 |   226 | `		pFunc->xFunc = xFunc;` |
|     2584 |   227 | `		SySetReset(&pFunc->aAux);` |
|     2584 |   228 | `		return SXRET_OK;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Create a new user function */` |
|  1231848 |   231 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1231848 |   232 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   233 | `		return rc;` |
|        - |   234 | `	}` |
|        - |   235 | `	/* Install the function in the corresponding hashtable */` |
|  1231848 |   236 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1231848 |   237 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   238 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   239 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   240 | `		return rc;` |
|        - |   241 | `	}` |
|        - |   242 | `	/* User function successfully installed */` |
|  1231848 |   243 | `	return SXRET_OK;` |
|   617216 |   244 |  |
|        - |   245 | `/*` |
|        - |   246 | ` * Initialize a VM function.` |
|        - |   247 | ` */` |
|   176266 |   248 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   249 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   250 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   251 | `	const char *zName,  /* Function name */` |
|        - |   252 | `	sxu32 nByte,        /* zName length */` |
|        - |   253 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   254 | `	void *pUserData     /* Function private data */` |
|        - |   255 | `	)` |
|        2 |   256 |  |
|        - |   257 | `	/* Zero the structure */` |
|   176268 |   258 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   259 | `	/* Initialize structure fields */` |
|        - |   260 | `	/* Arguments container */` |
|   176268 |   261 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   262 | `	/* Static variable container */` |
|   176268 |   263 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   264 | `	/* Bytecode container */` |
|   176268 |   265 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   266 | `    /* Preallocate some instruction slots */` |
|   176268 |   267 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   268 | `	/* Closure environment */` |
|   176268 |   269 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   270 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   176268 |   271 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   176268 |   272 | `	pFunc->iFlags = iFlags;` |
|   176268 |   273 | `	pFunc->pUserData = pUserData;` |
|   176268 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   176268 |   275 | `	return SXRET_OK;` |
|        2 |   276 |  |
|        - |   277 | `/*` |
|        - |   278 | ` * Namespace-aware function lookup.` |
|        - |   279 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   280 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   281 | ` */` |
|        - |   282 | `/*` |
|        - |   283 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   284 | ` */` |
|   692776 |   285 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   286 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   287 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   288 | `	SyString *pName     /* Function name */` |
|        - |   289 | `	)` |
|        2 |   290 |  |
|        - |   291 | `	SyHashEntry *pEntry;` |
|        - |   292 | `	sxi32 rc;` |
|   692778 |   293 | `	if( pName == 0 ){` |
|        - |   294 | `		/* Use the built-in name */` |
|    38158 |   295 | `		pName = &pFunc->sName;` |
|    19078 |   296 | `	}` |
|        - |   297 | `	/* Check for duplicates (functions with the same name) first */` |
|   692778 |   298 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   692778 |   299 | `	if( pEntry ){` |
|   539714 |   300 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   539714 |   301 | `		if( pLink != pFunc ){` |
|        - |   302 | `			/* Link */` |
|      188 |   303 | `			pFunc->pNextName = pLink;` |
|      188 |   304 | `			pEntry->pUserData = pFunc;` |
|       93 |   305 | `		}` |
|   539714 |   306 | `		return SXRET_OK;` |
|        - |   307 | `	}` |
|        - |   308 | `	/* First time seen */` |
|   153066 |   309 | `	pFunc->pNextName = 0;` |
|   153066 |   310 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   153066 |   311 | `	return rc;` |
|   346390 |   312 |  |
|        - |   313 | `/*` |
|        - |   314 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   315 | ` */` |
|    49496 |   316 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   317 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   318 | `	ph7_class *pClass /* Target Class */` |
|        - |   319 | `	)` |
|        2 |   320 |  |
|    49498 |   321 | `	SyString *pName = &pClass->sName;` |
|        - |   322 | `	SyHashEntry *pEntry;` |
|        - |   323 | `	sxi32 rc;` |
|        - |   324 | `	/* Check for duplicates */` |
|    49498 |   325 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    49498 |   326 | `	if( pEntry ){` |
|       31 |   327 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   328 | `		/* Link entry with the same name */` |
|       31 |   329 | `		pClass->pNextName = pLink;` |
|       31 |   330 | `		pEntry->pUserData = pClass;` |
|       31 |   331 | `		return SXRET_OK;` |
|        - |   332 | `	}` |
|    49468 |   333 | `	pClass->pNextName = 0;` |
|        - |   334 | `	/* Perform a simple hashtable insertion */` |
|    49468 |   335 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    49468 |   336 | `	return rc;` |
|    24750 |   337 |  |
|        - |   338 | `/*` |
|        - |   339 | ` * Instruction builder interface.` |
|        - |   340 | ` */` |
|  3554616 |   341 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  3554618 |   353 | `	sInstr.iOp = (sxu8)iOp;` |
|  3554618 |   354 | `	sInstr.iP1 = iP1;` |
|  3554618 |   355 | `	sInstr.iP2 = iP2;` |
|  3554618 |   356 | `	sInstr.p3  = p3;` |
|  3554618 |   357 | `	if( pIndex ){` |
|        - |   358 | `		/* Instruction index in the bytecode array */` |
|   204740 |   359 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   102369 |   360 | `	}` |
|        - |   361 | `	/* Finally,record the instruction */` |
|  3554618 |   362 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  3554618 |   363 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   364 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   365 | `		/* Fall throw */` |
|      ! 0 |   366 | `	}` |
|  3554618 |   367 | `	return rc;` |
|        2 |   368 |  |
|        - |   369 | `/*` |
|        - |   370 | ` * Swap the current bytecode container with the given one.` |
|        - |   371 | ` */` |
|   423212 |   372 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   373 |  |
|   423214 |   374 | `	if( pContainer == 0 ){` |
|        - |   375 | `		/* Point to the default container */` |
|      ! 0 |   376 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   377 | `	}else{` |
|        - |   378 | `		/* Change container */` |
|   423214 |   379 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   380 | `	}` |
|   423214 |   381 | `	return SXRET_OK;` |
|        2 |   382 |  |
|        - |   383 | `/*` |
|        - |   384 | ` * Return the current bytecode container.` |
|        - |   385 | ` */` |
|   211606 |   386 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   387 |  |
|   211608 |   388 | `	return pVm->pByteContainer;` |
|        2 |   389 |  |
|        - |   390 | `/*` |
|        - |   391 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   392 | ` */` |
|   201796 |   393 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   394 |  |
|        - |   395 | `	VmInstr *pInstr;` |
|   201798 |   396 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   201798 |   397 | `	return pInstr;` |
|        2 |   398 |  |
|        - |   399 | `/*` |
|        - |   400 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   401 | ` */` |
|  1064820 |   402 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   403 |  |
|  1064822 |   404 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   405 |  |
|        - |   406 | `/*` |
|        - |   407 | ` * Pop the last VM instruction.` |
|        - |   408 | ` */` |
|   192162 |   409 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   410 |  |
|   192164 |   411 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   412 |  |
|        - |   413 | `/*` |
|        - |   414 | ` * Peek the last VM instruction.` |
|        - |   415 | ` */` |
|   689218 |   416 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   417 |  |
|   689220 |   418 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   419 |  |
|    29936 |   420 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   421 |  |
|        - |   422 | `	VmInstr *aInstr;` |
|        - |   423 | `	sxu32 n;` |
|    29938 |   424 | `	n = SySetUsed(pVm->pByteContainer);` |
|    29938 |   425 | `	if( n < 2 ){` |
|      ! 0 |   426 | `		return 0;` |
|        - |   427 | `	}` |
|    29938 |   428 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    29938 |   429 | `	return &aInstr[n - 2];` |
|    14970 |   430 |  |
|        - |   431 | `/*` |
|        - |   432 | ` * Allocate a new virtual machine frame.` |
|        - |   433 | ` */` |
|    18798 |   434 | `static VmFrame * VmNewFrame(` |
|        - |   435 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   436 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   437 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   438 | `	)` |
|        2 |   439 |  |
|        - |   440 | `	VmFrame *pFrame;` |
|        - |   441 | `	/* Allocate a new vm frame */` |
|    18800 |   442 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    18800 |   443 | `	if( pFrame == 0 ){` |
|      ! 0 |   444 | `		return 0;` |
|        - |   445 | `	}` |
|        - |   446 | `	/* Zero the structure */` |
|    18800 |   447 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   448 | `	/* Initialize frame fields */` |
|    18800 |   449 | `	pFrame->pUserData = pUserData;` |
|    18800 |   450 | `	pFrame->pThis = pThis;` |
|    18800 |   451 | `	pFrame->pVm = pVm;` |
|    18800 |   452 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    18800 |   453 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    18800 |   454 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    18800 |   455 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    18800 |   456 | `	return pFrame;` |
|     9401 |   457 |  |
|        - |   458 | `/* Forward declaration */` |
|        - |   459 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   460 | `/*` |
|        - |   461 | ` * Enter a VM frame.` |
|        - |   462 | ` */` |
|    18752 |   463 | `static sxi32 VmEnterFrame(` |
|        - |   464 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   465 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   466 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   467 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   468 | `	)` |
|        2 |   469 |  |
|        - |   470 | `	VmFrame *pFrame;` |
|        - |   471 | `	/* Allocate a new frame */` |
|    18754 |   472 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    18754 |   473 | `	if( pFrame == 0 ){` |
|      ! 0 |   474 | `		return SXERR_MEM;` |
|        - |   475 | `	}` |
|        - |   476 | `	/* Link to the list of active VM frame */` |
|    18754 |   477 | `	pFrame->pParent = pVm->pFrame;` |
|    18754 |   478 | `	pVm->pFrame = pFrame;` |
|    18754 |   479 | `	if( ppFrame ){` |
|        - |   480 | `		/* Write a pointer to the new VM frame */` |
|    15888 |   481 | `		*ppFrame = pFrame;` |
|     7943 |   482 | `	}` |
|    18754 |   483 | `	return SXRET_OK;` |
|     9378 |   484 |  |
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
|    15880 |   528 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   529 |  |
|    15882 |   530 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    15882 |   531 | `	if( pCurFrame ){` |
|        - |   532 | `		/* Unlink from the list of active VM frame */` |
|    15882 |   533 | `		pVm->pFrame = pCurFrame->pParent;` |
|    15882 |   534 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   535 | `			VmSlot  *aSlot;` |
|        - |   536 | `			sxu32 n;` |
|        - |   537 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    15722 |   538 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   107224 |   539 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   540 | `				/* Unset the local variable */` |
|    91504 |   541 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    45753 |   542 | `			}` |
|        - |   543 | `			/* Remove local reference */` |
|    15722 |   544 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   107286 |   545 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    91566 |   546 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    45784 |   547 | `			}` |
|     7860 |   548 | `		}` |
|        - |   549 | `		/* Release internal containers */` |
|    15882 |   550 | `		SyHashRelease(&pCurFrame->hVar);` |
|    15882 |   551 | `		SySetRelease(&pCurFrame->sArg);` |
|    15882 |   552 | `		SySetRelease(&pCurFrame->sLocal);` |
|    15882 |   553 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   554 | `		/* Release the whole structure */` |
|    15882 |   555 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     7940 |   556 | `	}` |
|    15882 |   557 |  |
|        - |   558 | `/*` |
|        - |   559 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   560 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   561 | ` * should be skipped when looking for the real execution context.` |
|        - |   562 | ` */` |
|  6724102 |   563 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   564 |  |
|  6724968 |   565 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      866 |   566 | `		pFrame = pFrame->pParent;` |
|        2 |   567 | `	}` |
|  6724104 |   568 | `	return pFrame;` |
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
|   136736 |   688 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   689 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   690 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   691 | `	)` |
|        2 |   692 |  |
|        - |   693 | `	ph7_class_method *pMeth;` |
|        - |   694 | `	ph7_class_attr *pAttr;` |
|        - |   695 | `	SyHashEntry *pEntry;` |
|        - |   696 | `	sxi32 rc;` |
|        - |   697 | `	/* Reset the loop cursor */` |
|   136738 |   698 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   699 | `	/* Process only static and constant attribute */` |
|   573440 |   700 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   701 | `		/* Extract the current attribute */` |
|   368336 |   702 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   368336 |   703 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   704 | `			ph7_value *pMemObj;` |
|        - |   705 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1686 |   706 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1686 |   707 | `			if( pMemObj == 0 ){` |
|      ! 0 |   708 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   709 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   710 | `					&pClass->sName,&pAttr->sName` |
|        - |   711 | `					);` |
|      ! 0 |   712 | `				return SXERR_MEM;` |
|        - |   713 | `			}` |
|     1686 |   714 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   715 | `				/* Initialize attribute default value (any complex expression) */` |
|     1684 |   716 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      841 |   717 | `			}` |
|        - |   718 | `			/* Record attribute index */` |
|     1686 |   719 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   720 | `			/* Install static attribute in the reference table */` |
|     1686 |   721 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   722 | `			/* If this is a typed static property, register the slot so the` |
|        - |   723 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   724 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   725 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1686 |   726 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|      842 |   745 | `		}` |
|        2 |   746 | `	}` |
|        - |   747 | `	/* Install class methods */` |
|   136738 |   748 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   749 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   750 | `		 */` |
|    59834 |   751 | `		return SXRET_OK;` |
|        - |   752 | `	}` |
|        - |   753 | `	/* Create constructor alias if not yet done */` |
|    76906 |   754 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   755 | `		/* User constructor with the same base class name */` |
|     5984 |   756 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     5984 |   757 | `		if( pEntry ){` |
|      ! 0 |   758 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   759 | `			/* Create the alias */` |
|      ! 0 |   760 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   761 | `		}` |
|     2991 |   762 | `	}` |
|        - |   763 | `	/* Install the methods now */` |
|    76906 |   764 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   769986 |   765 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   654630 |   766 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   654630 |   767 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   654622 |   768 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   654622 |   769 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   770 | `				return rc;` |
|        - |   771 | `			}` |
|   327310 |   772 | `		}` |
|        2 |   773 | `	}` |
|        - |   774 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    76906 |   775 | `	pClass->bMounted = TRUE;` |
|    76906 |   776 | `	return SXRET_OK;` |
|    68370 |   777 |  |
|        - |   778 | `/*` |
|        - |   779 | ` * Allocate a private frame for attributes of the given` |
|        - |   780 | ` * class instance (Object in the PHP jargon).` |
|        - |   781 | ` */` |
|     1556 |   782 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   783 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   784 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   785 | `	)` |
|        2 |   786 |  |
|     1558 |   787 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   788 | `	ph7_class_attr *pAttr;` |
|        - |   789 | `	SyHashEntry *pEntry;` |
|        - |   790 | `	sxi32 rc;` |
|        - |   791 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1558 |   792 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     6158 |   793 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   794 | `		VmClassAttr *pVmAttr;` |
|        - |   795 | `		/* Extract the current attribute */` |
|     4602 |   796 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     4602 |   797 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     4602 |   798 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   799 | `			return SXERR_MEM;` |
|        - |   800 | `		}` |
|     4602 |   801 | `		pVmAttr->pAttr = pAttr;` |
|     4602 |   802 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   803 | `			ph7_value *pMemObj;` |
|        - |   804 | `			/* Reserve a memory object for this attribute */` |
|     4578 |   805 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     4578 |   806 | `			if( pMemObj == 0 ){` |
|      ! 0 |   807 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   808 | `				return SXERR_MEM;` |
|        - |   809 | `			}` |
|     4578 |   810 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     4578 |   811 | `			pVmAttr->iState = 0;` |
|     4578 |   812 | `			pVmAttr->pOwner = pClass;` |
|     4578 |   813 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   814 | `				/* Initialize attribute default value (any complex expression) */` |
|     1584 |   815 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     3787 |   816 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   817 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   818 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       28 |   819 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       13 |   820 | `			}` |
|     4578 |   821 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     4578 |   822 | `			if( rc != SXRET_OK ){` |
|        - |   823 | `				VmSlot sSlot;` |
|        - |   824 | `				/* Restore memory object */` |
|      ! 0 |   825 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   826 | `				sSlot.pUserData = 0;` |
|      ! 0 |   827 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   828 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   829 | `				return SXERR_MEM;` |
|        - |   830 | `			}` |
|        - |   831 | `			/* Install attribute in the reference table */` |
|     4578 |   832 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   833 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   834 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   835 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     4578 |   836 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|     2290 |   848 | `		}else{` |
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
|     1558 |   860 | `	return SXRET_OK;` |
|      780 |   861 |  |
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
|   408180 |   873 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   874 |  |
|        - |   875 | `	ph7_value *pObj;` |
|        - |   876 | `	sxi32 rc;` |
|   408182 |   877 | `	if( pIndex ){` |
|        - |   878 | `		/* Object index in the object table */` |
|   399584 |   879 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   199791 |   880 | `	}` |
|        - |   881 | `	/* Reserve a slot for the new object */` |
|   408182 |   882 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   408182 |   883 | `	if( rc != SXRET_OK ){` |
|        - |   884 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   885 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   886 | `		 */` |
|      ! 0 |   887 | `		return 0;` |
|        - |   888 | `	}` |
|   408182 |   889 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   408182 |   890 | `	return pObj;` |
|   204092 |   891 |  |
|        - |   892 | `/*` |
|        - |   893 | ` * Reserve a memory object.` |
|        - |   894 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   895 | ` */` |
|  2147066 |   896 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   897 |  |
|        - |   898 | `	ph7_value *pObj;` |
|        - |   899 | `	sxi32 rc;` |
|  2147068 |   900 | `	if( pIndex ){` |
|        - |   901 | `		/* Object index in the object table */` |
|  2147068 |   902 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1073533 |   903 | `	}` |
|        - |   904 | `	/* Reserve a slot for the new object */` |
|  2147068 |   905 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2147068 |   906 | `	if( rc != SXRET_OK ){` |
|        - |   907 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   908 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   909 | `		 */` |
|      ! 0 |   910 | `		return 0;` |
|        - |   911 | `	}` |
|  2147068 |   912 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2147068 |   913 | `	return pObj;` |
|  1073535 |   914 |  |
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
|        - |   953 | `	"class Exception { "\` |
|        - |   954 | `    "protected $message = 'Unknown exception';"\` |
|        - |   955 | `    "protected $code = 0;"\` |
|        - |   956 | `    "protected $file;"\` |
|        - |   957 | `    "protected $line;"\` |
|        - |   958 | `    "protected $trace;"\` |
|        - |   959 | `    "protected $previous;"\` |
|        - |   960 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   961 | `	"   if( isset($message) ){"\` |
|        - |   962 | `	"	  $this->message = $message;"\` |
|        - |   963 | `	"   }"\` |
|        - |   964 | `	"   $this->code = $code;"\` |
|        - |   965 | `	"   $this->file = __FILE__;"\` |
|        - |   966 | `	"   $this->line = __LINE__;"\` |
|        - |   967 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   968 | `	"   if( isset($previous) ){"\` |
|        - |   969 | `	"     $this->previous = $previous;"\` |
|        - |   970 | `	"   }"\` |
|        - |   971 | `	"}"\` |
|        - |   972 | `	"public function getMessage(){"\` |
|        - |   973 | `	"   return $this->message;"\` |
|        - |   974 | `	"}"\` |
|        - |   975 | `	" public function getCode(){"\` |
|        - |   976 | `	"  return $this->code;"\` |
|        - |   977 | `	"}"\` |
|        - |   978 | `	"public function getFile(){"\` |
|        - |   979 | `	"  return $this->file;"\` |
|        - |   980 | `	"}"\` |
|        - |   981 | `	"public function getLine(){"\` |
|        - |   982 | `	"  return $this->line;"\` |
|        - |   983 | `	"}"\` |
|        - |   984 | `	"public function getTrace(){"\` |
|        - |   985 | `	"   return $this->trace;"\` |
|        - |   986 | `	"}"\` |
|        - |   987 | `	"public function getTraceAsString(){"\` |
|        - |   988 | `	"  return debug_string_backtrace();"\` |
|        - |   989 | `	"}"\` |
|        - |   990 | `	"public function getPrevious(){"\` |
|        - |   991 | `	"    return $this->previous;"\` |
|        - |   992 | `	"}"\` |
|        - |   993 | `	"public function __toString(){"\` |
|        - |   994 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   995 | `    "}"\` |
|        - |   996 | `	"}"\` |
|        - |   997 | `	"class Error extends Exception { }"\` |
|        - |   998 | `	"class TypeError extends Error { }"\` |
|        - |   999 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1000 | `	"class ValueError extends Error { }"\` |
|        - |  1001 | `	"class FiberError extends Error { }"\` |
|        - |  1002 | `	"class AssertionError extends Error { }"\` |
|        - |  1003 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1004 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1005 | `	"class ErrorException extends Exception { "\` |
|        - |  1006 | `	"protected $severity;"\` |
|        - |  1007 | `	"public function __construct(string $message = null,"\` |
|        - |  1008 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |  1009 | `	"   if( isset($message) ){"\` |
|        - |  1010 | `	"	  $this->message = $message;"\` |
|        - |  1011 | `	"   }"\` |
|        - |  1012 | `	"   $this->severity = $severity;"\` |
|        - |  1013 | `	"   $this->code = $code;"\` |
|        - |  1014 | `	"   $this->file = $filename;"\` |
|        - |  1015 | `	"   $this->line = $lineno;"\` |
|        - |  1016 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1017 | `	"   if( isset($previous) ){"\` |
|        - |  1018 | `	"     $this->previous = $previous;"\` |
|        - |  1019 | `	"   }"\` |
|        - |  1020 | `	"}"\` |
|        - |  1021 | `	"public function getSeverity(){"\` |
|        - |  1022 | `	"   return $this->severity;"\` |
|        - |  1023 | `    "}"\` |
|        - |  1024 | `	"}"\` |
|        - |  1025 | `	"interface Iterator {"\` |
|        - |  1026 | `	"public function current();"\` |
|        - |  1027 | `	"public function key();"\` |
|        - |  1028 | `	"public function next();"\` |
|        - |  1029 | `	"public function rewind();"\` |
|        - |  1030 | `	"public function valid();"\` |
|        - |  1031 | `	"}"\` |
|        - |  1032 | `	"interface IteratorAggregate {"\` |
|        - |  1033 | `	"public function getIterator();"\` |
|        - |  1034 | `	"}"\` |
|        - |  1035 | `	"interface Serializable {"\` |
|        - |  1036 | `	"public function serialize();"\` |
|        - |  1037 | `	"public function unserialize(string $serialized);"\` |
|        - |  1038 | `	"}"\` |
|        - |  1039 | `	"/* Directory releated IO */"\` |
|        - |  1040 | `	"class Directory {"\` |
|        - |  1041 | `	"public $handle = null;"\` |
|        - |  1042 | `	"public $path  = null;"\` |
|        - |  1043 | `	"public function __construct(string $path)"\` |
|        - |  1044 | `	"{"\` |
|        - |  1045 | `	"   $this->handle = opendir($path);"\` |
|        - |  1046 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1047 | `	"      $this->path = $path;"\` |
|        - |  1048 | `	"   }"\` |
|        - |  1049 | `	"}"\` |
|        - |  1050 | `	"public function __destruct()"\` |
|        - |  1051 | `	"{"\` |
|        - |  1052 | `	"  if( $this->handle != null ){"\` |
|        - |  1053 | `	"       closedir($this->handle);"\` |
|        - |  1054 | `	"  }"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"public function read()"\` |
|        - |  1057 | `	"{"\` |
|        - |  1058 | `	"    return readdir($this->handle);"\` |
|        - |  1059 | `	"}"\` |
|        - |  1060 | `	"public function rewind()"\` |
|        - |  1061 | `	"{"\` |
|        - |  1062 | `	"    rewinddir($this->handle);"\` |
|        - |  1063 | `	"}"\` |
|        - |  1064 | `	"public function close()"\` |
|        - |  1065 | `	"{"\` |
|        - |  1066 | `	"    closedir($this->handle);"\` |
|        - |  1067 | `	"    $this->handle = null;"\` |
|        - |  1068 | `	"}"\` |
|        - |  1069 | `	"}"\` |
|        - |  1070 | `	"class Fiber {"\` |
|        - |  1071 | `	"  private $__ctx;"\` |
|        - |  1072 | `	"  private $__callable;"\` |
|        - |  1073 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1074 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1075 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1076 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1077 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1078 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1079 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1080 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1081 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1082 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1083 | `	"}"\` |
|        - |  1084 | `	"class Generator implements Iterator {"\` |
|        - |  1085 | `	"  private $__ctx;"\` |
|        - |  1086 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1087 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1088 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1089 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1090 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1091 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1092 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1093 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1094 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1095 | `	"}"\` |
|        - |  1096 | `	"class stdClass{"\` |
|        - |  1097 | `	"  public $value;"\` |
|        - |  1098 | `	" /* Magic methods */"\` |
|        - |  1099 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1100 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1101 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1102 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1103 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1104 | `	"}"\` |
|        - |  1105 | `	"function dir(string $path){"\` |
|        - |  1106 | `	"   return new Directory($path);"\` |
|        - |  1107 | `	"}"\` |
|        - |  1108 | `	"function Dir(string $path){"\` |
|        - |  1109 | `	"   return new Directory($path);"\` |
|        - |  1110 | `	"}"\` |
|        - |  1111 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1112 | `    "{"\` |
|        - |  1113 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1114 | `	"  $aDir = array();"\` |
|        - |  1115 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1116 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1117 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1118 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1119 | `	"   }"\` |
|        - |  1120 | `	"  closedir($pHandle);"\` |
|        - |  1121 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1122 | `	"      rsort($aDir);"\` |
|        - |  1123 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1124 | `	"      sort($aDir);"\` |
|        - |  1125 | `	"  }"\` |
|        - |  1126 | `	"  return $aDir;"\` |
|        - |  1127 | `	"}"\` |
|        - |  1128 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1129 | `	"/* Open the target directory */"\` |
|        - |  1130 | `	"$zDir = dirname($pattern);"\` |
|        - |  1131 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1132 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1133 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1134 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1135 | `	"	return FALSE;"\` |
|        - |  1136 | `	"}"\` |
|        - |  1137 | `	"$pattern = basename($pattern);"\` |
|        - |  1138 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1139 | `	"/* Loop throw available entries */"\` |
|        - |  1140 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1141 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1142 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1143 | `	"	if( $rc ){"\` |
|        - |  1144 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1145 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1146 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1147 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1148 | `	"		  }"\` |
|        - |  1149 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1150 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1151 | `	"		 continue;"\` |
|        - |  1152 | `	"	   }"\` |
|        - |  1153 | `	"	   /* Add the entry */"\` |
|        - |  1154 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1155 | `	"	}"\` |
|        - |  1156 | `	" }"\` |
|        - |  1157 | `	"/* Close the handle */"\` |
|        - |  1158 | `	"closedir($pHandle);"\` |
|        - |  1159 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1160 | `	"  /* Sort the array */"\` |
|        - |  1161 | `	"  sort($pArray);"\` |
|        - |  1162 | `	"}"\` |
|        - |  1163 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1164 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1165 | `	"  $pArray[] = $pattern;"\` |
|        - |  1166 | `	"}"\` |
|        - |  1167 | `	"/* Return the created array */"\` |
|        - |  1168 | `	"return $pArray;"\` |
|        - |  1169 | `   "}"\` |
|        - |  1170 | `   "/* Creates a temporary file */"\` |
|        - |  1171 | `   "function tmpfile(){"\` |
|        - |  1172 | `   "  /* Extract the temp directory */"\` |
|        - |  1173 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1174 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1175 | `   "    /* Use the current dir */"\` |
|        - |  1176 | `   "    $zTempDir = '.';"\` |
|        - |  1177 | `   "  }"\` |
|        - |  1178 | `   "  /* Create the file */"\` |
|        - |  1179 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1180 | `   "  return $pHandle;"\` |
|        - |  1181 | `   "}"\` |
|        - |  1182 | `   "/* Creates a temporary filename */"\` |
|        - |  1183 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1184 | `   "{"\` |
|        - |  1185 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1186 | `   "}"\` |
|        - |  1187 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1188 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1189 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1190 | `   "/* Copy arguments */"\` |
|        - |  1191 | `   "$nArgs = func_num_args();"\` |
|        - |  1192 | `   "$pNew = array();"\` |
|        - |  1193 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1194 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1195 | `    "}"\` |
|        - |  1196 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1197 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1198 | `	"/* Erase */"\` |
|        - |  1199 | `	"array_erase($pArray);"\` |
|        - |  1200 | `	"/* Unshift */"\` |
|        - |  1201 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1202 | `	"return sizeof($pArray);"\` |
|        - |  1203 | `    "}"\` |
|        - |  1204 | `	"function array_merge_recursive(){"\` |
|        - |  1205 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1206 | `    "$arrays = func_get_args();"\` |
|        - |  1207 | `    "$narrays = count($arrays);"\` |
|        - |  1208 | `    "$ret = array();"\` |
|        - |  1209 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1210 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1211 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1212 | `	 " }"\` |
|        - |  1213 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1214 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1215 | `     "  if( $keyIsInt ) {"\` |
|        - |  1216 | `     "   $ret[] = $value;"\` |
|        - |  1217 | `     "  } else {"\` |
|        - |  1218 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1219 | `     "    $cur = $ret[$key];"\` |
|        - |  1220 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1221 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1222 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1223 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1224 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1225 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1226 | `     "    } else {"\` |
|        - |  1227 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1228 | `     "    }"\` |
|        - |  1229 | `     "   } else {"\` |
|        - |  1230 | `     "    $ret[$key] = $value;"\` |
|        - |  1231 | `     "   }"\` |
|        - |  1232 | `     "  }"\` |
|        - |  1233 | `     " }"\` |
|        - |  1234 | `	 " }"\` |
|        - |  1235 | `	 " return $ret;"\` |
|        - |  1236 | `    "}"\` |
|        - |  1237 | `	"function max(){"\` |
|        - |  1238 | `    "  $pArgs = func_get_args();"\` |
|        - |  1239 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1240 | `	"  return null;"\` |
|        - |  1241 | `    " }"\` |
|        - |  1242 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1243 | `    " $pArg = $pArgs[0];"\` |
|        - |  1244 | `	" if( !is_array($pArg) ){"\` |
|        - |  1245 | `	"   return $pArg; "\` |
|        - |  1246 | `	" }"\` |
|        - |  1247 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1248 | `	"   return null;"\` |
|        - |  1249 | `	" }"\` |
|        - |  1250 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1251 | `	" reset($pArg);"\` |
|        - |  1252 | `	" $max = current($pArg);"\` |
|        - |  1253 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1254 | `	"   if( $val > $max ){"\` |
|        - |  1255 | `	"     $max = $val;"\` |
|        - |  1256 | `    " }"\` |
|        - |  1257 | `	" }"\` |
|        - |  1258 | `	" return $max;"\` |
|        - |  1259 | `    " }"\` |
|        - |  1260 | `    " $max = $pArgs[0];"\` |
|        - |  1261 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1262 | `    " $val = $pArgs[$i];"\` |
|        - |  1263 | `	"if( $val > $max ){"\` |
|        - |  1264 | `	" $max = $val;"\` |
|        - |  1265 | `	"}"\` |
|        - |  1266 | `    " }"\` |
|        - |  1267 | `	" return $max;"\` |
|        - |  1268 | `    "}"\` |
|        - |  1269 | `	"function min(){"\` |
|        - |  1270 | `    "  $pArgs = func_get_args();"\` |
|        - |  1271 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1272 | `	"  return null;"\` |
|        - |  1273 | `    " }"\` |
|        - |  1274 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1275 | `    " $pArg = $pArgs[0];"\` |
|        - |  1276 | `	" if( !is_array($pArg) ){"\` |
|        - |  1277 | `	"   return $pArg; "\` |
|        - |  1278 | `	" }"\` |
|        - |  1279 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1280 | `	"   return null;"\` |
|        - |  1281 | `	" }"\` |
|        - |  1282 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1283 | `	" reset($pArg);"\` |
|        - |  1284 | `	" $min = current($pArg);"\` |
|        - |  1285 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1286 | `	"   if( $val < $min ){"\` |
|        - |  1287 | `	"     $min = $val;"\` |
|        - |  1288 | `    " }"\` |
|        - |  1289 | `	" }"\` |
|        - |  1290 | `	" return $min;"\` |
|        - |  1291 | `    " }"\` |
|        - |  1292 | `    " $min = $pArgs[0];"\` |
|        - |  1293 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1294 | `    " $val = $pArgs[$i];"\` |
|        - |  1295 | `	"if( $val < $min ){"\` |
|        - |  1296 | `	" $min = $val;"\` |
|        - |  1297 | `	" }"\` |
|        - |  1298 | `    " }"\` |
|        - |  1299 | `	" return $min;"\` |
|        - |  1300 | `	"}"\` |
|        - |  1301 | `	"function fileowner(string $file){"\` |
|        - |  1302 | `    " $a = stat($file);"\` |
|        - |  1303 | `	" if( !is_array($a) ){"\` |
|        - |  1304 | `	"	return false;"\` |
|        - |  1305 | `	" }"\` |
|        - |  1306 | `	" return $a['uid'];"\` |
|        - |  1307 | `    "}"\` |
|        - |  1308 | `    "function filegroup(string $file){"\` |
|        - |  1309 | `	" $a = stat($file);"\` |
|        - |  1310 | `	" if( !is_array($a) ){"\` |
|        - |  1311 | `	"	return false;"\` |
|        - |  1312 | `	" }"\` |
|        - |  1313 | `	" return $a['gid'];"\` |
|        - |  1314 | `    "}"\` |
|        - |  1315 | `	 "function fileinode(string $file){"\` |
|        - |  1316 | `	" $a = stat($file);"\` |
|        - |  1317 | `	" if( !is_array($a) ){"\` |
|        - |  1318 | `	"	return false;"\` |
|        - |  1319 | `	" }"\` |
|        - |  1320 | `	" return $a['ino'];"\` |
|        - |  1321 | `    "}"` |
|        - |  1322 |  |
|        - |  1323 | `/*` |
|        - |  1324 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1325 | ` * start compiling the target PHP program.` |
|        - |  1326 | ` */` |
|     2866 |  1327 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1328 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1329 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1330 | `	 )` |
|        2 |  1331 |  |
|        - |  1332 | `	SyString sBuiltin;` |
|        - |  1333 | `	ph7_value *pObj;` |
|        - |  1334 | `	sxi32 rc;` |
|        - |  1335 | `	/* Zero the structure */` |
|     2868 |  1336 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1337 | `	/* Initialize VM fields */` |
|     2868 |  1338 | `	pVm->pEngine = &(*pEngine);` |
|     2868 |  1339 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1340 | `	/* Instructions containers */` |
|     2868 |  1341 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2868 |  1342 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2868 |  1343 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1344 | `	/* Object containers */` |
|     2868 |  1345 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2868 |  1346 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1347 | `	/* Virtual machine internal containers */` |
|     2868 |  1348 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2868 |  1349 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2868 |  1350 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2868 |  1351 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2868 |  1352 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2868 |  1353 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2868 |  1354 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2868 |  1355 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2868 |  1356 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2868 |  1357 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2868 |  1358 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2868 |  1359 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2868 |  1360 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2868 |  1361 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2868 |  1362 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2868 |  1363 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2868 |  1364 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2868 |  1365 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2868 |  1366 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2868 |  1367 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2868 |  1368 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2868 |  1369 | `	pVm->pPendingException = 0;` |
|        - |  1370 | `	/* Configuration containers */` |
|     2868 |  1371 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2868 |  1372 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2868 |  1373 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2868 |  1374 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2868 |  1375 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2868 |  1376 | `	pVm->iResponseStatus = 200;` |
|     2868 |  1377 | `	pVm->bHeadersSent = 0;` |
|     2868 |  1378 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1379 | `	/* Error callbacks containers */` |
|     2868 |  1380 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2868 |  1381 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2868 |  1382 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2868 |  1383 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2868 |  1384 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1385 | `	/* Set a default recursion limit */` |
|        - |  1386 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2868 |  1387 | `	pVm->nMaxDepth = 32;` |
|        - |  1388 | `#else` |
|        - |  1389 | `	pVm->nMaxDepth = 16;` |
|        - |  1390 | `#endif` |
|        - |  1391 | `	/* Default assertion flags */` |
|     2868 |  1392 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1393 | `	/* JSON return status */` |
|     2868 |  1394 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1395 | `	/* PRNG context */` |
|     2868 |  1396 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1397 | `	/* Install the null constant */` |
|     2868 |  1398 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2868 |  1399 | `	if( pObj == 0 ){` |
|      ! 0 |  1400 | `		rc = SXERR_MEM;` |
|      ! 0 |  1401 | `		goto Err;` |
|        - |  1402 | `	}` |
|     2868 |  1403 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1404 | `	/* Install the boolean TRUE constant */` |
|     2868 |  1405 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2868 |  1406 | `	if( pObj == 0 ){` |
|      ! 0 |  1407 | `		rc = SXERR_MEM;` |
|      ! 0 |  1408 | `		goto Err;` |
|        - |  1409 | `	}` |
|     2868 |  1410 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1411 | `	/* Install the boolean FALSE constant */` |
|     2868 |  1412 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2868 |  1413 | `	if( pObj == 0 ){` |
|      ! 0 |  1414 | `		rc = SXERR_MEM;` |
|      ! 0 |  1415 | `		goto Err;` |
|        - |  1416 | `	}` |
|     2868 |  1417 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1418 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1419 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1420 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2868 |  1421 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2868 |  1422 | `	if( pObj == 0 ){` |
|      ! 0 |  1423 | `		rc = SXERR_MEM;` |
|      ! 0 |  1424 | `		goto Err;` |
|        - |  1425 | `	}` |
|     2868 |  1426 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1427 | `	/* Create the global frame */` |
|     2868 |  1428 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2868 |  1429 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1430 | `		goto Err;` |
|        - |  1431 | `	}` |
|        - |  1432 | `	/* Initialize the code generator */` |
|     2868 |  1433 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2868 |  1434 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1435 | `		goto Err;` |
|        - |  1436 | `	}` |
|        - |  1437 | `	/* VM correctly initialized,set the magic number */` |
|     2868 |  1438 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2868 |  1439 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1440 | `	/* Compile the built-in library */` |
|     2868 |  1441 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1442 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2868 |  1443 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1444 | `	/* Register Fiber internal C functions */` |
|     2868 |  1445 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2868 |  1446 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2868 |  1447 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2868 |  1448 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2868 |  1449 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2868 |  1450 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2868 |  1451 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2868 |  1452 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2868 |  1453 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2868 |  1454 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1455 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2868 |  1456 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2868 |  1457 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2868 |  1458 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2868 |  1459 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2868 |  1460 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2868 |  1461 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2868 |  1462 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2868 |  1463 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2868 |  1464 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2868 |  1465 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1466 | `	/* Reset the code generator */` |
|     2868 |  1467 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2868 |  1468 | `	return SXRET_OK;` |
|      ! 0 |  1469 | `Err:` |
|      ! 0 |  1470 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1471 | `	return rc;` |
|     1435 |  1472 |  |
|        - |  1473 | `/*` |
|        - |  1474 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1475 | ` * routine which store the output in an internal blob.` |
|        - |  1476 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1477 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1478 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1479 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1480 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1481 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1482 | ` * to finish executing and extracting the output.` |
|        - |  1483 | ` */` |
|       38 |  1484 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1485 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1486 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1487 | `	void *pUserData     /* User private data */` |
|        - |  1488 | `	)` |
|      ! 0 |  1489 |  |
|        - |  1490 | `	 sxi32 rc;` |
|        - |  1491 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1492 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1493 | `	 return rc;` |
|      ! 0 |  1494 |  |
|        - |  1495 | `/*` |
|        - |  1496 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1497 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1498 | ` */` |
|    16428 |  1499 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1500 |  |
|    16430 |  1501 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    16430 |  1502 | `	if( xCons != VmObConsumer ){` |
|     7048 |  1503 | `		pVm->nOutputLen += nLen;` |
|     7048 |  1504 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      908 |  1505 | `			pVm->bHeadersSent = 1;` |
|      453 |  1506 | `		}` |
|     3523 |  1507 | `	}` |
|    16430 |  1508 |  |
|        - |  1509 | `#define VM_STACK_GUARD 16` |
|        - |  1510 | `/*` |
|        - |  1511 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1512 | ` * our compiled PHP program.` |
|        - |  1513 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1514 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1515 | ` */` |
|    38126 |  1516 | `static ph7_value * VmNewOperandStack(` |
|        - |  1517 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1518 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1519 | `	)` |
|        2 |  1520 |  |
|        - |  1521 | `	ph7_value *pStack;` |
|        - |  1522 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1523 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1524 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1525 | `  ** on the maximum stack depth required.` |
|        - |  1526 | `  **` |
|        - |  1527 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1528 | `  */` |
|    38128 |  1529 | `	nInstr += VM_STACK_GUARD;` |
|    38128 |  1530 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    38128 |  1531 | `	if( pStack == 0 ){` |
|      ! 0 |  1532 | `		return 0;` |
|        - |  1533 | `	}` |
|        - |  1534 | `	/* Initialize the operand stack */` |
|  2558936 |  1535 | `	while( nInstr > 0 ){` |
|  2520810 |  1536 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2520810 |  1537 | `		--nInstr;` |
|        2 |  1538 | `	}` |
|        - |  1539 | `	/* Ready for bytecode execution */` |
|    38128 |  1540 | `	return pStack;` |
|    19065 |  1541 |  |
|        - |  1542 | `/* Forward declaration */` |
|        - |  1543 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1544 | `/*` |
|        - |  1545 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1546 | ` * This routine gets called by the PH7 engine after` |
|        - |  1547 | ` * successful compilation of the target PHP program.` |
|        - |  1548 | ` */` |
|     2582 |  1549 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1550 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1551 | `	)` |
|        2 |  1552 |  |
|        - |  1553 | `	SyHashEntry *pEntry;` |
|        - |  1554 | `	sxi32 rc;` |
|     2584 |  1555 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1556 | `		/* Initialize your VM first */` |
|      ! 0 |  1557 | `		return SXERR_CORRUPT;` |
|        - |  1558 | `	}` |
|        - |  1559 | `	/* Mark the VM ready for byte-code execution */` |
|     2584 |  1560 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1561 | `	/* Release the code generator now we have compiled our program */` |
|     2584 |  1562 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1563 | `	/* Emit the DONE instruction */` |
|     2584 |  1564 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2584 |  1565 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1566 | `		return SXERR_MEM;` |
|        - |  1567 | `	}` |
|        - |  1568 | `	/* Script return value */` |
|     2584 |  1569 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1570 | `	/* Allocate a new operand stack */` |
|     2584 |  1571 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2584 |  1572 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1573 | `		return SXERR_MEM;` |
|        - |  1574 | `	}` |
|        - |  1575 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1576 | `	 * private data. */` |
|     2584 |  1577 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2584 |  1578 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1579 | `	/* Allocate the reference table */` |
|     2584 |  1580 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2584 |  1581 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2584 |  1582 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1583 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1584 | `		return SXERR_MEM;` |
|        - |  1585 | `	}` |
|        - |  1586 | `	/* Zero the reference table */` |
|     2584 |  1587 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1588 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2584 |  1589 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2584 |  1590 | `	if( rc != SXRET_OK ){` |
|        - |  1591 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1592 | `		return rc;` |
|        - |  1593 | `	}` |
|        - |  1594 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2584 |  1595 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2584 |  1596 | `	if( rc != SXRET_OK ){` |
|        - |  1597 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1598 | `		return rc;` |
|        - |  1599 | `	}` |
|        - |  1600 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2584 |  1601 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1602 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2584 |  1603 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1604 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2584 |  1605 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1606 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1607 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2584 |  1608 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2584 |  1609 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1610 | `#endif` |
|        - |  1611 | `	/* Initialize and install static and constants class attributes */` |
|     2584 |  1612 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    46736 |  1613 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    44154 |  1614 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    44154 |  1615 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1616 | `			return rc;` |
|        - |  1617 | `		}` |
|        2 |  1618 | `	}` |
|        - |  1619 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2584 |  1620 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1621 | `	/* VM is ready for bytecode execution */` |
|     2584 |  1622 | `	return SXRET_OK;` |
|     1293 |  1623 |  |
|        - |  1624 | `/*` |
|        - |  1625 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1626 | ` */` |
|      ! 0 |  1627 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1628 |  |
|      ! 0 |  1629 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1630 | `		return SXERR_CORRUPT;` |
|        - |  1631 | `	}` |
|        - |  1632 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1633 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1634 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1635 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1636 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1637 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1638 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1639 | `	pVm->bHttpContext = 0;` |
|        - |  1640 | `	/* Set the ready flag */` |
|      ! 0 |  1641 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1642 | `	return SXRET_OK;` |
|      ! 0 |  1643 |  |
|        - |  1644 | `/*` |
|        - |  1645 | ` * Release a Virtual Machine.` |
|        - |  1646 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1647 | ` */` |
|     2574 |  1648 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1649 |  |
|        - |  1650 | `	/* Set the stale magic number */` |
|     2576 |  1651 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1652 | `	/* Release the private memory subsystem */` |
|     2576 |  1653 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2576 |  1654 | `	return SXRET_OK;` |
|        2 |  1655 |  |
|        - |  1656 | `/*` |
|        - |  1657 | ` * Initialize a foreign function call context.` |
|        - |  1658 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1659 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1660 | ` * functions.` |
|        - |  1661 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1662 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1663 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1664 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1665 | ` */` |
|   636112 |  1666 | `static sxi32 VmInitCallContext(` |
|        - |  1667 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1668 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1669 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1670 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1671 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1672 | `	)` |
|        2 |  1673 |  |
|   636114 |  1674 | `	pOut->pFunc = pFunc;` |
|   636114 |  1675 | `	pOut->pVm   = pVm;` |
|   636114 |  1676 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   636114 |  1677 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1678 | `	/* Assume a null return value */` |
|   636114 |  1679 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   636114 |  1680 | `	pOut->pRet = pRet;` |
|   636114 |  1681 | `	pOut->iFlags = iFlags;` |
|   636114 |  1682 | `	return SXRET_OK;` |
|        2 |  1683 |  |
|        - |  1684 | `/*` |
|        - |  1685 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1686 | ` * left behind.` |
|        - |  1687 | ` */` |
|   636112 |  1688 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1689 |  |
|        - |  1690 | `	sxu32 n;` |
|   636114 |  1691 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     7676 |  1692 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    22086 |  1693 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    14412 |  1694 | `			if( apObj[n] == 0 ){` |
|        - |  1695 | `				/* Already released */` |
|      298 |  1696 | `				continue;` |
|        - |  1697 | `			}` |
|    14116 |  1698 | `			PH7_MemObjRelease(apObj[n]);` |
|    14116 |  1699 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7059 |  1700 | `		}` |
|     7676 |  1701 | `		SySetRelease(&pCtx->sVar);` |
|     3837 |  1702 | `	}` |
|   636114 |  1703 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1704 | `		ph7_aux_data *aAux;` |
|        - |  1705 | `		void *pChunk;` |
|        - |  1706 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1707 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1708 | `		 */` |
|        9 |  1709 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1710 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1711 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1712 | `			/* Release the chunk */` |
|       25 |  1713 | `			if( pChunk ){` |
|       25 |  1714 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1715 | `			}` |
|       13 |  1716 | `		}` |
|        9 |  1717 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1718 | `	}` |
|   636114 |  1719 |  |
|        - |  1720 | `/*` |
|        - |  1721 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1722 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1723 | ` */` |
|      296 |  1724 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1725 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1726 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1727 | `	)` |
|        2 |  1728 |  |
|      298 |  1729 | `	if( pValue == 0 ){` |
|        - |  1730 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1731 | `		return;` |
|        - |  1732 | `	}` |
|      298 |  1733 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      298 |  1734 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1735 | `		sxu32 n;` |
|     1054 |  1736 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1054 |  1737 | `			if( apObj[n] == pValue ){` |
|      298 |  1738 | `				PH7_MemObjRelease(pValue);` |
|      298 |  1739 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1740 | `				/* Mark as released */` |
|      298 |  1741 | `				apObj[n] = 0;` |
|      298 |  1742 | `				break;` |
|        - |  1743 | `			}` |
|      380 |  1744 | `		}` |
|      148 |  1745 | `	}` |
|      150 |  1746 |  |
|        - |  1747 | `/*` |
|        - |  1748 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1749 | ` */` |
|  3646938 |  1750 | `static void VmPopOperand(` |
|        - |  1751 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1752 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|  3646940 |  1755 | `	ph7_value *pTos = *ppTos;` |
|  7760652 |  1756 | `	while( nPop > 0 ){` |
|  4113714 |  1757 | `		PH7_MemObjRelease(pTos);` |
|  4113714 |  1758 | `		pTos--;` |
|  4113714 |  1759 | `		nPop--;` |
|        2 |  1760 | `	}` |
|        - |  1761 | `	/* Top of the stack */` |
|  3646940 |  1762 | `	*ppTos = pTos;` |
|  3646940 |  1763 |  |
|        - |  1764 | `/*` |
|        - |  1765 | ` * Reserve a memory object.` |
|        - |  1766 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1767 | ` */` |
|  3112028 |  1768 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1769 |  |
|  3112030 |  1770 | `	ph7_value *pObj = 0;` |
|        - |  1771 | `	VmSlot *pSlot;` |
|        - |  1772 | `	sxu32 nIdx;` |
|        - |  1773 | `	/* Check for a free slot */` |
|  3112030 |  1774 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3112030 |  1775 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3112030 |  1776 | `	if( pSlot ){` |
|   964964 |  1777 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   964964 |  1778 | `		nIdx = pSlot->nIdx;` |
|   482481 |  1779 | `	}` |
|  3112030 |  1780 | `	if( pObj == 0 ){` |
|        - |  1781 | `		/* Reserve a new memory object */` |
|  2147068 |  1782 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2147068 |  1783 | `		if( pObj == 0 ){` |
|      ! 0 |  1784 | `			return 0;` |
|        - |  1785 | `		}` |
|  1073533 |  1786 | `	}` |
|        - |  1787 | `	/* Set a null default value */` |
|  3112030 |  1788 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3112030 |  1789 | `	pObj->nIdx = nIdx;` |
|  3112030 |  1790 | `	return pObj;` |
|  1556016 |  1791 |  |
|        - |  1792 | `/*` |
|        - |  1793 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1794 | ` */` |
|    33246 |  1795 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1796 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1797 | `	const char *zKey,  /* Entry key */` |
|        - |  1798 | `	sxu32 nByte,       /* Key length */` |
|        - |  1799 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1800 | `	)` |
|        2 |  1801 |  |
|        - |  1802 | `	ph7_value sKey;` |
|        - |  1803 | `	sxi32 rc;` |
|    33248 |  1804 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    33248 |  1805 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1806 | `	/* Perform the insertion */` |
|    33248 |  1807 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    33248 |  1808 | `	PH7_MemObjRelease(&sKey);` |
|    33248 |  1809 | `	return rc;` |
|        2 |  1810 |  |
|        - |  1811 | `/*` |
|        - |  1812 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1813 | ` * Return a pointer to the variable value on success.` |
|        - |  1814 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1815 | ` */` |
|  3394432 |  1816 | `static ph7_value * VmExtractMemObj(` |
|        - |  1817 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1818 | `	const SyString *pName, /* Variable name */` |
|        - |  1819 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1820 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1821 | `	)` |
|        2 |  1822 |  |
|  3394434 |  1823 | `	int bNullify = FALSE;` |
|        - |  1824 | `	SyHashEntry *pEntry;` |
|        - |  1825 | `	VmFrame *pFrame;` |
|        - |  1826 | `	ph7_value *pObj;` |
|        - |  1827 | `	sxu32 nIdx;` |
|        - |  1828 | `	sxi32 rc;` |
|        - |  1829 | `	/* Point to the top active frame */` |
|  3394434 |  1830 | `	pFrame = pVm->pFrame;` |
|  3394434 |  1831 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1832 | `	/* Perform the lookup */` |
|  3394434 |  1833 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1834 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1835 | `		pName = &sAnnon;` |
|        - |  1836 | `		/* Always nullify the object */` |
|      ! 0 |  1837 | `		bNullify = TRUE;` |
|      ! 0 |  1838 | `		bDup = FALSE;` |
|      ! 0 |  1839 | `	}` |
|        - |  1840 | `	/* Check the superglobals table first */` |
|  3394434 |  1841 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3394434 |  1842 | `	if( pEntry == 0 ){` |
|        - |  1843 | `		/* Query the top active frame */` |
|  3394394 |  1844 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3394394 |  1845 | `		if( pEntry == 0 ){` |
|    99064 |  1846 | `			char *zName = (char *)pName->zString;` |
|        - |  1847 | `			VmSlot sLocal;` |
|    99064 |  1848 | `			if( !bCreate ){` |
|        - |  1849 | `				/* Do not create the variable,return NULL instead */` |
|      116 |  1850 | `				return 0;` |
|        - |  1851 | `			}` |
|        - |  1852 | `			/* No such variable,automatically create a new one and install` |
|        - |  1853 | `			 * it in the current frame.` |
|        - |  1854 | `			 */` |
|    98950 |  1855 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    98950 |  1856 | `			if( pObj == 0 ){` |
|      ! 0 |  1857 | `				return 0;` |
|        - |  1858 | `			}` |
|    98950 |  1859 | `			nIdx = pObj->nIdx;` |
|    98950 |  1860 | `			if( bDup ){` |
|        - |  1861 | `				/* Duplicate name */` |
|      172 |  1862 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      172 |  1863 | `				if( zName == 0 ){` |
|      ! 0 |  1864 | `					return 0;` |
|        - |  1865 | `				}` |
|       85 |  1866 | `			}` |
|        - |  1867 | `			/* Link to the top active VM frame */` |
|    98950 |  1868 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    98950 |  1869 | `			if( rc != SXRET_OK ){` |
|        - |  1870 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1871 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1872 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1873 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1874 | `				return 0;` |
|        - |  1875 | `			}` |
|    98950 |  1876 | `			if( pFrame->pParent != 0 ){` |
|        - |  1877 | `				/* Local variable */` |
|    91552 |  1878 | `				sLocal.nIdx = nIdx;` |
|    91552 |  1879 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    45777 |  1880 | `			}else{` |
|        - |  1881 | `				/* Register in the $GLOBALS array */` |
|     7400 |  1882 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1883 | `			}` |
|        - |  1884 | `			/* Install in the reference table */` |
|    98950 |  1885 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1886 | `			/* Save object index */` |
|    98950 |  1887 | `			pObj->nIdx = nIdx;` |
|    49476 |  1888 | `		}else{` |
|        - |  1889 | `			/* Extract variable contents */` |
|  3295332 |  1890 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3295332 |  1891 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3295332 |  1892 | `			if( bNullify && pObj ){` |
|      ! 0 |  1893 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1894 | `			}` |
|        - |  1895 | `		}` |
|  1697251 |  1896 | `	}else{` |
|        - |  1897 | `		/* Superglobal */` |
|       42 |  1898 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  1899 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1900 | `	}` |
|  3394320 |  1901 | `	return pObj;` |
|  1697328 |  1902 |  |
|        - |  1903 | `/*` |
|        - |  1904 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1905 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1906 | ` */` |
|     2886 |  1907 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1908 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1909 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1910 | `	sxu32 nByte        /* zName length */` |
|        - |  1911 | `	)` |
|        2 |  1912 |  |
|        - |  1913 | `	SyHashEntry *pEntry;` |
|        - |  1914 | `	ph7_value *pValue;` |
|        - |  1915 | `	sxu32 nIdx;` |
|        - |  1916 | `	/* Query the superglobal table */` |
|     2888 |  1917 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2888 |  1918 | `	if( pEntry == 0 ){` |
|        - |  1919 | `		/* No such entry */` |
|      ! 0 |  1920 | `		return 0;` |
|        - |  1921 | `	}` |
|        - |  1922 | `	/* Extract the superglobal index in the global object pool */` |
|     2888 |  1923 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1924 | `	/* Extract the variable value  */` |
|     2888 |  1925 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2888 |  1926 | `	return pValue;` |
|     1445 |  1927 |  |
|        - |  1928 | `/*` |
|        - |  1929 | ` * Perform a raw hashmap insertion.` |
|        - |  1930 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1931 | ` */` |
|     2916 |  1932 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1933 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1934 | `	const char *zKey,   /* Entry key */` |
|        - |  1935 | `	int nKeylen,        /* zKey length*/` |
|        - |  1936 | `	const char *zData,  /* Entry data */` |
|        - |  1937 | `	int nLen            /* zData length */` |
|        - |  1938 | `	)` |
|        2 |  1939 |  |
|        - |  1940 | `	ph7_value sKey,sValue;` |
|        - |  1941 | `	sxi32 rc;` |
|     2918 |  1942 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2918 |  1943 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2918 |  1944 | `	if( zKey ){` |
|     2896 |  1945 | `		if( nKeylen < 0 ){` |
|     2844 |  1946 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1421 |  1947 | `		}` |
|     2896 |  1948 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1447 |  1949 | `	}` |
|     2918 |  1950 | `	if( zData ){` |
|     2918 |  1951 | `		if( nLen < 0 ){` |
|        - |  1952 | `			/* Compute length automatically */` |
|      144 |  1953 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  1954 | `		}` |
|     2918 |  1955 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1458 |  1956 | `	}` |
|        - |  1957 | `	/* Perform the insertion */` |
|     2918 |  1958 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2918 |  1959 | `	PH7_MemObjRelease(&sKey);` |
|     2918 |  1960 | `	PH7_MemObjRelease(&sValue);` |
|     2918 |  1961 | `	return rc;` |
|        2 |  1962 |  |
|        - |  1963 | `/*` |
|        - |  1964 | ` * Configure a working virtual machine instance.` |
|        - |  1965 | ` *` |
|        - |  1966 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1967 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1968 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1969 | ` * The second argument to this function is an integer configuration option` |
|        - |  1970 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1971 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1972 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1973 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1974 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1975 | ` */` |
|    41642 |  1976 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1977 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1978 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1979 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1980 | `	)` |
|        2 |  1981 |  |
|    41644 |  1982 | `	sxi32 rc = SXRET_OK;` |
|    41644 |  1983 | `	switch(nOp){` |
|     1283 |  1984 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2568 |  1985 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2568 |  1986 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1987 | `		/* VM output consumer callback */` |
|        - |  1988 | `#ifdef UNTRUST` |
|        - |  1989 | `		if( xConsumer == 0 ){` |
|        - |  1990 | `			rc = SXERR_CORRUPT;` |
|        - |  1991 | `			break;` |
|        - |  1992 | `		}` |
|        - |  1993 | `#endif` |
|        - |  1994 | `		/* Install the output consumer */` |
|     2568 |  1995 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2568 |  1996 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2568 |  1997 | `		break;` |
|        - |  1998 | `							   }` |
|     1291 |  1999 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2000 | `		/* Import path */` |
|        - |  2001 | `		  const char *zPath;` |
|        - |  2002 | `		  SyString sPath;` |
|     2584 |  2003 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2004 | `#if defined(UNTRUST)` |
|        - |  2005 | `		  if( zPath == 0 ){` |
|        - |  2006 | `			  rc = SXERR_EMPTY;` |
|        - |  2007 | `			  break;` |
|        - |  2008 | `		  }` |
|        - |  2009 | `#endif` |
|     2584 |  2010 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2011 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2012 | `#ifdef __WINNT__` |
|        2 |  2013 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2014 | `#endif` |
|     5166 |  2015 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2016 | `		  /* Remove leading and trailing white spaces */` |
|     2584 |  2017 | `		  SyStringFullTrim(&sPath);` |
|     2584 |  2018 | `		  if( sPath.nByte > 0 ){` |
|        - |  2019 | `			  /* Store the path in the corresponding conatiner */` |
|     2584 |  2020 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1291 |  2021 | `		  }` |
|     2584 |  2022 | `		  break;` |
|        - |  2023 | `									 }` |
|     1291 |  2024 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2025 | `		/* Run-Time Error report */` |
|     2584 |  2026 | `		pVm->bErrReport = 1;` |
|     2584 |  2027 | `		break;` |
|      ! 0 |  2028 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2029 | `		/* Recursion depth */` |
|      ! 0 |  2030 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2031 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2032 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2033 | `		}` |
|      ! 0 |  2034 | `		break;` |
|        - |  2035 | `									   }` |
|      ! 0 |  2036 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2037 | `		/* VM output length in bytes */` |
|      ! 0 |  2038 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2039 | `#ifdef UNTRUST` |
|        - |  2040 | `		if( pOut == 0 ){` |
|        - |  2041 | `			rc = SXERR_CORRUPT;` |
|        - |  2042 | `			break;` |
|        - |  2043 | `		}` |
|        - |  2044 | `#endif` |
|      ! 0 |  2045 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2046 | `		break;` |
|        - |  2047 | `							   }` |
|        - |  2048 |  |
|    12910 |  2049 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2050 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2051 | `		/* Create a new superglobal/global variable */` |
|    25822 |  2052 | `		const char *zName = va_arg(ap,const char *);` |
|    25822 |  2053 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2054 | `		SyHashEntry *pEntry;` |
|        - |  2055 | `		ph7_value *pObj;` |
|        - |  2056 | `		sxu32 nByte;` |
|        - |  2057 | `		sxu32 nIdx;` |
|        - |  2058 | `#ifdef UNTRUST` |
|        - |  2059 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2060 | `			rc = SXERR_CORRUPT;` |
|        - |  2061 | `			break;` |
|        - |  2062 | `		}` |
|        - |  2063 | `#endif` |
|    25822 |  2064 | `		nByte = SyStrlen(zName);` |
|    25822 |  2065 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2066 | `			/* Check if the superglobal is already installed */` |
|    25822 |  2067 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    12912 |  2068 | `		}else{` |
|        - |  2069 | `			/* Query the top active VM frame */` |
|      ! 0 |  2070 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2071 | `		}` |
|    25822 |  2072 | `		if( pEntry ){` |
|        - |  2073 | `			/* Variable already installed */` |
|      ! 0 |  2074 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2075 | `			/* Extract contents */` |
|      ! 0 |  2076 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2077 | `			if( pObj ){` |
|        - |  2078 | `				/* Overwrite old contents */` |
|      ! 0 |  2079 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2080 | `			}` |
|      ! 0 |  2081 | `		}else{` |
|        - |  2082 | `			/* Install a new variable */` |
|    25822 |  2083 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    25822 |  2084 | `			if( pObj == 0 ){` |
|      ! 0 |  2085 | `				rc = SXERR_MEM;` |
|      ! 0 |  2086 | `				break;` |
|        - |  2087 | `			}` |
|    25822 |  2088 | `			nIdx = pObj->nIdx;` |
|        - |  2089 | `			/* Copy value */` |
|    25822 |  2090 | `			PH7_MemObjStore(pValue,pObj);` |
|    25822 |  2091 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2092 | `				/* Install the superglobal */` |
|    25822 |  2093 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    12912 |  2094 | `			}else{` |
|        - |  2095 | `				/* Install in the current frame */` |
|      ! 0 |  2096 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2097 | `			}` |
|    25822 |  2098 | `			if( rc == SXRET_OK ){` |
|        - |  2099 | `				SyHashEntry *pRef;` |
|    25822 |  2100 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    25822 |  2101 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    12912 |  2102 | `				}else{` |
|      ! 0 |  2103 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2104 | `				}` |
|        - |  2105 | `				/* Install in the reference table */` |
|    25822 |  2106 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    25822 |  2107 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2108 | `					/* Register in the $GLOBALS array */` |
|    25822 |  2109 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    12910 |  2110 | `				}` |
|    12910 |  2111 | `			}` |
|        - |  2112 | `		}` |
|    25822 |  2113 | `		break;` |
|        - |  2114 | `									}` |
|     1421 |  2115 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2116 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2117 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2118 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2119 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2120 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2121 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2844 |  2122 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2844 |  2123 | `		const char *zValue = va_arg(ap,const char *);` |
|     2844 |  2124 | `		int nLen = va_arg(ap,int);` |
|        - |  2125 | `		ph7_hashmap *pMap;` |
|        - |  2126 | `		ph7_value *pValue;` |
|     2844 |  2127 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2128 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2129 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2843 |  2130 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2131 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2132 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2842 |  2133 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2134 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2135 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2842 |  2136 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2137 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2138 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2842 |  2139 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2140 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2141 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2842 |  2142 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2143 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2144 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2145 | `		}else{` |
|        - |  2146 | `			/* Extract the $_SERVER superglobal */` |
|     2842 |  2147 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2148 | `		}` |
|     2844 |  2149 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2150 | `			/* No such entry */` |
|      ! 0 |  2151 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2152 | `			break;` |
|        - |  2153 | `		}` |
|        - |  2154 | `		/* Point to the hashmap */` |
|     2844 |  2155 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2156 | `		/* Perform the insertion */` |
|     2844 |  2157 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2844 |  2158 | `		break;` |
|        - |  2159 | `								   }` |
|       11 |  2160 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2161 | `		/* Script arguments */` |
|       24 |  2162 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2163 | `		ph7_hashmap *pMap;` |
|        - |  2164 | `		ph7_value *pValue;` |
|        - |  2165 | `		sxu32 n;` |
|       24 |  2166 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2167 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2168 | `			break;` |
|        - |  2169 | `		}` |
|        - |  2170 | `		/* Extract the $argv array */` |
|       24 |  2171 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2172 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2173 | `			/* No such entry */` |
|      ! 0 |  2174 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2175 | `			break;` |
|        - |  2176 | `		}` |
|        - |  2177 | `		/* Point to the hashmap */` |
|       24 |  2178 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2179 | `		/* Perform the insertion */` |
|       24 |  2180 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2181 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2182 | `		if( rc == SXRET_OK ){` |
|       24 |  2183 | `			if( pMap->nEntry > 1 ){` |
|        - |  2184 | `				/* Append space separator first */` |
|       18 |  2185 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2186 | `			}` |
|       24 |  2187 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2188 | `		}` |
|       24 |  2189 | `		break;` |
|        - |  2190 | `								  }` |
|      ! 0 |  2191 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2192 | `		/* error_log() consumer */` |
|      ! 0 |  2193 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2194 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2195 | `		break;` |
|        - |  2196 | `										}` |
|      ! 0 |  2197 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2198 | `		/* Script return value */` |
|      ! 0 |  2199 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2200 | `#ifdef UNTRUST` |
|        - |  2201 | `		if( ppValue == 0 ){` |
|        - |  2202 | `			rc = SXERR_CORRUPT;` |
|        - |  2203 | `			break;` |
|        - |  2204 | `		}` |
|        - |  2205 | `#endif` |
|      ! 0 |  2206 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2207 | `		break;` |
|        - |  2208 | `								   }` |
|     2582 |  2209 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2210 | `		/* Register an IO stream device */` |
|     5166 |  2211 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2212 | `		/* Make sure we are dealing with a valid IO stream */` |
|     7746 |  2213 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5166 |  2214 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2215 | `				/* Invalid stream */` |
|      ! 0 |  2216 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2217 | `				break;` |
|        - |  2218 | `		}` |
|     5166 |  2219 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2220 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2584 |  2221 | `			pVm->pDefStream = pStream;` |
|     1291 |  2222 | `		}` |
|        - |  2223 | `		/* Insert in the appropriate container */` |
|     5166 |  2224 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5166 |  2225 | `		break;` |
|        - |  2226 | `								  }` |
|        8 |  2227 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2228 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2229 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2230 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2231 | `#ifdef UNTRUST` |
|        - |  2232 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2233 | `			rc = SXERR_CORRUPT;` |
|        - |  2234 | `			break;` |
|        - |  2235 | `		}` |
|        - |  2236 | `#endif` |
|       16 |  2237 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2238 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2239 | `		break;` |
|        - |  2240 | `									   }` |
|        8 |  2241 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2242 | `		/* Raw HTTP request*/` |
|       16 |  2243 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2244 | `		int nByte = va_arg(ap,int);` |
|       16 |  2245 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2246 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2247 | `			break;` |
|        - |  2248 | `		}` |
|       16 |  2249 | `		if( nByte < 0 ){` |
|        - |  2250 | `			/* Compute length automatically */` |
|      ! 0 |  2251 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2252 | `		}` |
|        - |  2253 | `		/* Process the request */` |
|       16 |  2254 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2255 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2256 | `		if( rc == SXRET_OK ){` |
|       16 |  2257 | `			pVm->bHttpContext = 1;` |
|        8 |  2258 | `		}` |
|       16 |  2259 | `		break;` |
|        - |  2260 | `									}` |
|        8 |  2261 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2262 | `		/* Extract HTTP response status code */` |
|       16 |  2263 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2264 | `		if( pStatus ){` |
|       16 |  2265 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2266 | `		}` |
|       16 |  2267 | `		break;` |
|        - |  2268 | `										}` |
|        8 |  2269 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2270 | `		/* Iterate response headers via callback */` |
|        - |  2271 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2272 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2273 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2274 | `		if( xCallback ){` |
|       16 |  2275 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2276 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2277 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2278 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2279 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2280 | `							   pUserData);` |
|       12 |  2281 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2282 | `					break;` |
|        - |  2283 | `				}` |
|        6 |  2284 | `			}` |
|        8 |  2285 | `		}` |
|       16 |  2286 | `		break;` |
|        - |  2287 | `										 }` |
|      ! 0 |  2288 | `	default:` |
|        - |  2289 | `		/* Unknown configuration option */` |
|      ! 0 |  2290 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2291 | `		break;` |
|        - |  2292 | `	}` |
|    41644 |  2293 | `	return rc;` |
|        2 |  2294 |  |
|        - |  2295 | `/* Forward declaration */` |
|        - |  2296 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2297 | `/*` |
|        - |  2298 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2299 | ` * format.` |
|        - |  2300 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2301 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2302 | ` * (STDOUT).` |
|        - |  2303 | ` */` |
|        2 |  2304 | `static sxi32 VmByteCodeDump(` |
|        - |  2305 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2306 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2307 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2308 | `	)` |
|        1 |  2309 |  |
|        - |  2310 | `	static const char zDump[] = {` |
|        - |  2311 | `		"====================================================\n"` |
|        - |  2312 | `		"PH7 VM Dump\n"` |
|        - |  2313 | `		"====================================================\n"` |
|        - |  2314 | `	};` |
|        - |  2315 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2316 | `	sxi32 rc = SXRET_OK;` |
|        - |  2317 | `	sxu32 n;` |
|        - |  2318 | `	/* Point to the PH7 instructions */` |
|        3 |  2319 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2320 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2321 | `	n = 0;` |
|        3 |  2322 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2323 | `	/* Dump instructions */` |
|        7 |  2324 | `	for(;;){` |
|       15 |  2325 | `		if( pInstr >= pEnd ){` |
|        - |  2326 | `			/* No more instructions */` |
|        3 |  2327 | `			break;` |
|        - |  2328 | `		}` |
|        - |  2329 | `		/* Format and call the consumer callback */` |
|       19 |  2330 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2331 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2332 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2333 | `		if( rc != SXRET_OK ){` |
|        - |  2334 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2335 | `			return rc;` |
|        - |  2336 | `		}` |
|       13 |  2337 | `		++n;` |
|       13 |  2338 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2339 | `	}` |
|        3 |  2340 | `	return rc;` |
|        2 |  2341 |  |
|        - |  2342 | `/* Forward declaration */` |
|        - |  2343 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2344 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2345 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2346 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2347 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2348 | `/*` |
|        - |  2349 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2350 | ` * consumer callback.` |
|        - |  2351 | ` */` |
|      568 |  2352 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2353 |  |
|      569 |  2354 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      569 |  2355 | `	sxi32 rc = SXRET_OK;` |
|        - |  2356 | `	/* Append a new line */` |
|        - |  2357 | `#ifdef __WINNT__` |
|        1 |  2358 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2359 | `#else` |
|      568 |  2360 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2361 | `#endif` |
|        - |  2362 | `	/* Invoke the output consumer callback */` |
|      569 |  2363 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      569 |  2364 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      569 |  2365 | `	return rc;` |
|        1 |  2366 |  |
|        - |  2367 | `/*` |
|        - |  2368 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2369 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2370 | ` * information.` |
|        - |  2371 | ` */` |
|      136 |  2372 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2373 |  |
|      138 |  2374 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2375 | `		ph7_value apArg[4];` |
|        - |  2376 | `		ph7_value *apArgPtr[4];` |
|        - |  2377 | `		ph7_value sResult;` |
|        - |  2378 | `		SyString sErr;` |
|        - |  2379 | `		/* Prepare arguments */` |
|       64 |  2380 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2381 | `			/* use explicit message length to avoid reading past buffer */` |
|       64 |  2382 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       64 |  2383 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       64 |  2384 | `		if( pFile ){` |
|       64 |  2385 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       64 |  2386 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       33 |  2387 | `		}else{` |
|      ! 0 |  2388 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2389 | `		}` |
|       64 |  2390 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       64 |  2391 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2392 | `		/* Set up pointer array */` |
|       64 |  2393 | `		apArgPtr[0] = &apArg[0];` |
|       64 |  2394 | `		apArgPtr[1] = &apArg[1];` |
|       64 |  2395 | `		apArgPtr[2] = &apArg[2];` |
|       64 |  2396 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2397 | `		/* Call the handler */` |
|       64 |  2398 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2399 | `		/* Check return value */` |
|       64 |  2400 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2401 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2402 | `		}` |
|        - |  2403 | `		/* Release */` |
|       64 |  2404 | `		PH7_MemObjRelease(&apArg[0]);` |
|       64 |  2405 | `		PH7_MemObjRelease(&apArg[1]);` |
|       64 |  2406 | `		PH7_MemObjRelease(&apArg[2]);` |
|       64 |  2407 | `		PH7_MemObjRelease(&apArg[3]);` |
|       64 |  2408 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2409 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2410 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       64 |  2411 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2412 | `	}` |
|        - |  2413 | `	/* No handler, always call error handler */` |
|       75 |  2414 | `	return TRUE;` |
|       70 |  2415 |  |
|       98 |  2416 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2417 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2418 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2419 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2420 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2421 | `	)` |
|        2 |  2422 |  |
|      100 |  2423 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2424 | `	SyString *pFile;` |
|        - |  2425 | `	char *zErr;` |
|      100 |  2426 | `	sxi32 rc = SXRET_OK;` |
|      100 |  2427 | `	if( !pVm->bErrReport ){` |
|        - |  2428 | `		/* Don't bother reporting errors */` |
|        3 |  2429 | `		return SXRET_OK;` |
|        - |  2430 | `	}` |
|        - |  2431 | `	/* Reset the working buffer */` |
|       98 |  2432 | `	SyBlobReset(pWorker);` |
|        - |  2433 | `	/* Peek the processed file if available */` |
|       98 |  2434 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       98 |  2435 | `	if( pFile ){` |
|        - |  2436 | `		/* Append file name */` |
|       98 |  2437 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       98 |  2438 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       48 |  2439 | `	}` |
|        - |  2440 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2441 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2442 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2443 | `	 * E_DEPRECATED). */` |
|       98 |  2444 | `	zErr = "Error:  ";` |
|       98 |  2445 | `	switch(iErr){` |
|       19 |  2446 | `	case PH7_CTX_WARNING:` |
|       40 |  2447 | `		zErr = "Warning:  ";` |
|       40 |  2448 | `		break;` |
|        6 |  2449 | `	case PH7_CTX_NOTICE:` |
|       14 |  2450 | `		zErr = "Notice:  ";` |
|       12 |  2451 | `		break;` |
|       23 |  2452 | `	default:` |
|        - |  2453 | `		/* keep iErr unchanged */` |
|       46 |  2454 | `		break;` |
|        - |  2455 | `	}` |
|       98 |  2456 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       98 |  2457 | `	if( pFuncName ){` |
|        - |  2458 | `		/* Append function name first */` |
|       23 |  2459 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2460 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2461 | `	}` |
|       98 |  2462 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2463 | `	/* Check for user error handler.  compute length of C string */` |
|       98 |  2464 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2465 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2466 | `	}` |
|       98 |  2467 | `	return rc;` |
|       51 |  2468 |  |
|        - |  2469 | `/*` |
|        - |  2470 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2471 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2472 | ` * information.` |
|        - |  2473 | ` */` |
|       40 |  2474 | `static sxi32 VmThrowErrorAp(` |
|        - |  2475 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2476 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2477 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2478 | `	const char *zFormat, /* Format message */` |
|        - |  2479 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2480 | `	)` |
|        2 |  2481 |  |
|       42 |  2482 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2483 | `	SyBlob sMsg;` |
|        - |  2484 | `	SyString *pFile;` |
|        - |  2485 | `	char *zErr;` |
|       42 |  2486 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2487 | `	if( !pVm->bErrReport ){` |
|        - |  2488 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2489 | `		return SXRET_OK;` |
|        - |  2490 | `	}` |
|        - |  2491 | `	/* Reset the working buffer */` |
|       42 |  2492 | `	SyBlobReset(pWorker);` |
|        - |  2493 | `	/* Peek the processed file if available */` |
|       42 |  2494 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2495 | `	if( pFile ){` |
|        - |  2496 | `		/* Append file name */` |
|       42 |  2497 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2498 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2499 | `	}` |
|        - |  2500 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2501 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2502 | `	 * the correct errno value. */` |
|       42 |  2503 | `	zErr = "Error:  ";` |
|       42 |  2504 | `	switch(iErr){` |
|        4 |  2505 | `	case PH7_CTX_WARNING:` |
|        9 |  2506 | `		zErr = "Warning:  ";` |
|        9 |  2507 | `		break;` |
|        3 |  2508 | `	case PH7_CTX_NOTICE:` |
|        7 |  2509 | `		zErr = "Notice:  ";` |
|        6 |  2510 | `		break;` |
|       13 |  2511 | `	default:` |
|        - |  2512 | `		/* do not change iErr */` |
|       26 |  2513 | `		break;` |
|        - |  2514 | `	}` |
|       42 |  2515 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2516 | `	if( pFuncName ){` |
|        - |  2517 | `		/* Append function name first */` |
|       26 |  2518 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2519 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2520 | `	}` |
|        - |  2521 | `	/* Format the raw message */` |
|       42 |  2522 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2523 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2524 | `	/* Check if a user error handler is installed */` |
|       42 |  2525 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2526 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2527 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2528 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2529 | `	}` |
|       42 |  2530 | `	SyBlobRelease(&sMsg);` |
|       42 |  2531 | `	return rc;` |
|       22 |  2532 |  |
|        - |  2533 | `/*` |
|        - |  2534 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2535 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2536 | ` * possible.` |
|        - |  2537 | ` */` |
|       36 |  2538 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2539 |  |
|        - |  2540 | `	ph7_class *pClass;` |
|       37 |  2541 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2542 | `	ph7_class_instance *pThis;` |
|        - |  2543 | `	ph7_class_method *pCons;` |
|        - |  2544 | `	ph7_value sArg;` |
|        - |  2545 | `	ph7_value *apArg[1];` |
|        - |  2546 | `	SyBlob sMsg;` |
|        - |  2547 | `	SyString sMsgStr;` |
|        - |  2548 | `	VmFrame *pFrame;` |
|        - |  2549 | `	sxi32 rc;` |
|       37 |  2550 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       37 |  2551 | `	if( pClass == 0 ){` |
|      ! 0 |  2552 | `		return PH7_ABORT;` |
|        - |  2553 | `	}` |
|       37 |  2554 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       37 |  2555 | `	if( pThis == 0 ){` |
|      ! 0 |  2556 | `		return PH7_ABORT;` |
|        - |  2557 | `	}` |
|       37 |  2558 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2559 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2560 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2561 | `	{` |
|       37 |  2562 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       37 |  2563 | `		if( pOwner ){` |
|       37 |  2564 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       18 |  2565 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       19 |  2566 | `		}else{` |
|      ! 0 |  2567 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2568 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2569 | `		}` |
|        - |  2570 | `	}` |
|       37 |  2571 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       37 |  2572 | `	if( pCons ){` |
|       37 |  2573 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       37 |  2574 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       37 |  2575 | `		apArg[0] = &sArg;` |
|       37 |  2576 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       37 |  2577 | `		PH7_MemObjRelease(&sArg);` |
|       18 |  2578 | `	}` |
|       37 |  2579 | `	SyBlobRelease(&sMsg);` |
|       37 |  2580 | `	pFrame = pVm->pFrame;` |
|       37 |  2581 | `	if( pFrame ){` |
|       37 |  2582 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       37 |  2583 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       18 |  2584 | `	}` |
|       37 |  2585 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       37 |  2586 | `	PH7_ClassInstanceUnref(pThis);` |
|       37 |  2587 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2588 | `		return PH7_ABORT;` |
|        - |  2589 | `	}` |
|       37 |  2590 | `	return PH7_EXCEPTION;` |
|       19 |  2591 |  |
|        - |  2592 |  |
|        - |  2593 | `/*` |
|        - |  2594 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2595 | ` */` |
|        4 |  2596 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2597 |  |
|        - |  2598 | `	ph7_class *pErrClass;` |
|        - |  2599 | `	ph7_class_instance *pThis;` |
|        - |  2600 | `	ph7_class_method *pCons;` |
|        - |  2601 | `	ph7_value sArg;` |
|        - |  2602 | `	ph7_value *apArg[1];` |
|        - |  2603 | `	SyBlob sMsg;` |
|        - |  2604 | `	SyString sMsgStr;` |
|        - |  2605 | `	VmFrame *pFrame;` |
|        - |  2606 | `	sxi32 rc;` |
|        5 |  2607 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2608 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2609 | `		return PH7_ABORT;` |
|        - |  2610 | `	}` |
|        5 |  2611 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2612 | `	if( pThis == 0 ){` |
|      ! 0 |  2613 | `		return PH7_ABORT;` |
|        - |  2614 | `	}` |
|        5 |  2615 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2616 | `	{` |
|        5 |  2617 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2618 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2619 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2620 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2621 | `	}` |
|        5 |  2622 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2623 | `	if( pCons ){` |
|        5 |  2624 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2625 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2626 | `		apArg[0] = &sArg;` |
|        5 |  2627 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2628 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2629 | `	}` |
|        5 |  2630 | `	SyBlobRelease(&sMsg);` |
|        5 |  2631 | `	pFrame = pVm->pFrame;` |
|        5 |  2632 | `	if( pFrame ){` |
|        5 |  2633 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2634 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2635 | `	}` |
|        5 |  2636 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2637 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2638 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2639 | `		return PH7_ABORT;` |
|        - |  2640 | `	}` |
|        5 |  2641 | `	return PH7_EXCEPTION;` |
|        3 |  2642 |  |
|        - |  2643 |  |
|        - |  2644 | `/*` |
|        - |  2645 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2646 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2647 | ` * For class types, instanceof is verified.` |
|        - |  2648 | ` *` |
|        - |  2649 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2650 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2651 | ` */` |
|        - |  2652 | `/*` |
|        - |  2653 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2654 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2655 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2656 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2657 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2658 | ` */` |
|       16 |  2659 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2660 |  |
|        - |  2661 | `	const char *z, *zEnd, *zTail;` |
|        - |  2662 | `	sxu32 n;` |
|        - |  2663 | `	sxu8 bReal;` |
|        - |  2664 | `	sxi32 rc;` |
|       18 |  2665 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2666 | `		return 0;` |
|        - |  2667 | `	}` |
|       18 |  2668 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2669 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2670 | `	zEnd = z + n;` |
|       18 |  2671 | `	if( n == 0 ){` |
|      ! 0 |  2672 | `		return 0;` |
|        - |  2673 | `	}` |
|       18 |  2674 | `	zTail = 0;` |
|       18 |  2675 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2676 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        5 |  2677 | `		return 0;` |
|        - |  2678 | `	}` |
|        - |  2679 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       14 |  2680 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2681 | `		zTail++;` |
|      ! 0 |  2682 | `	}` |
|       14 |  2683 | `	return zTail == zEnd ? 1 : 0;` |
|       10 |  2684 |  |
|        - |  2685 |  |
|        - |  2686 | `/*` |
|        - |  2687 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2688 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2689 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2690 | ` *   0 if it's not strictly numeric.` |
|        - |  2691 | ` */` |
|       16 |  2692 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2693 |  |
|        - |  2694 | `	const char *z, *zEnd, *zTail;` |
|        - |  2695 | `	sxu32 n;` |
|       18 |  2696 | `	sxu8 bReal = 0;` |
|        - |  2697 | `	sxi32 rc;` |
|       18 |  2698 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2699 | `		return 0;` |
|        - |  2700 | `	}` |
|       18 |  2701 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2702 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2703 | `	zEnd = z + n;` |
|       18 |  2704 | `	if( n == 0 ) return 0;` |
|       18 |  2705 | `	zTail = 0;` |
|       18 |  2706 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2707 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2708 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2709 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2710 | `	return bReal ? 2 : 1;` |
|       10 |  2711 |  |
|        - |  2712 |  |
|        - |  2713 | `/*` |
|        - |  2714 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts* using` |
|        - |  2715 | ` * PHP 8 weak-mode union semantics. Returns SXRET_OK on accept (pValue may` |
|        - |  2716 | ` * have been mutated by the cast), SXERR_INVALID on reject. Caller is` |
|        - |  2717 | ` * responsible for the actual TypeError throw.` |
|        - |  2718 | ` *` |
|        - |  2719 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2720 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2721 | ` */` |
|       90 |  2722 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable)` |
|        2 |  2723 |  |
|        - |  2724 | `	sxu32 i;` |
|        - |  2725 | `	ph7_type_alt *aAlts;` |
|        - |  2726 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2727 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|       92 |  2728 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2729 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2730 | `	}` |
|       80 |  2731 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       80 |  2732 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       80 |  2733 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      236 |  2734 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      158 |  2735 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      134 |  2736 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      134 |  2737 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      134 |  2738 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       68 |  2739 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       40 |  2740 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2741 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       80 |  2742 | `	}` |
|        - |  2743 | `	/* Object handling */` |
|       80 |  2744 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2745 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2746 | `		if( bHasClassAlt ){` |
|       14 |  2747 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2748 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2749 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2750 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2751 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2752 | `			}` |
|       26 |  2753 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2754 | `				ph7_class *pExpected;` |
|        - |  2755 | `				SyString *pCN;` |
|       22 |  2756 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2757 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2758 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2759 | `					pExpected = pSelfNow;` |
|       22 |  2760 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2761 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2762 | `				}else{` |
|       22 |  2763 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2764 | `				}` |
|       22 |  2765 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2766 | `					return SXRET_OK;` |
|        - |  2767 | `				}` |
|        8 |  2768 | `			}` |
|        2 |  2769 | `		}` |
|        9 |  2770 | `		return SXERR_INVALID;` |
|        - |  2771 | `	}` |
|        - |  2772 | `	/* Array handling */` |
|       64 |  2773 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2774 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2775 | `	}` |
|        - |  2776 | `	/* Scalar handling — exact match first */` |
|       58 |  2777 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       22 |  2778 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2779 | `	}` |
|       38 |  2780 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2781 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2782 | `	}` |
|       34 |  2783 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       34 |  2784 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2785 | `	}` |
|       18 |  2786 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2787 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2788 | `	}` |
|        - |  2789 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2790 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2791 | `	 * to match PHP's union RFC. */` |
|        - |  2792 | `	{` |
|       18 |  2793 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2794 | `		if( bHasInt ){` |
|        - |  2795 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2796 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2797 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2798 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2799 | `				return SXRET_OK;` |
|        - |  2800 | `			}` |
|       18 |  2801 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2802 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2803 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2804 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2805 | `					return SXRET_OK;` |
|        - |  2806 | `				}` |
|      ! 0 |  2807 | `			}` |
|       18 |  2808 | `			if( kind == 1 ){` |
|        9 |  2809 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2810 | `				return SXRET_OK;` |
|        - |  2811 | `			}` |
|        4 |  2812 | `		}` |
|       10 |  2813 | `		if( bHasFloat ){` |
|       10 |  2814 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2815 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2816 | `				return SXRET_OK;` |
|        - |  2817 | `			}` |
|       10 |  2818 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2819 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2820 | `				return SXRET_OK;` |
|        - |  2821 | `			}` |
|        1 |  2822 | `		}` |
|        3 |  2823 | `		if( bHasString ){` |
|      ! 0 |  2824 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2825 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2826 | `				return SXRET_OK;` |
|        - |  2827 | `			}` |
|      ! 0 |  2828 | `		}` |
|        3 |  2829 | `		if( bHasBool ){` |
|      ! 0 |  2830 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2831 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2832 | `				return SXRET_OK;` |
|        - |  2833 | `			}` |
|      ! 0 |  2834 | `		}` |
|        - |  2835 | `	}` |
|        3 |  2836 | `	return SXERR_INVALID;` |
|       47 |  2837 |  |
|        - |  2838 |  |
|        - |  2839 | `/*` |
|        - |  2840 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  2841 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  2842 | ` */` |
|       16 |  2843 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  2844 |  |
|       17 |  2845 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       25 |  2846 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       16 |  2847 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       17 |  2848 | `	return zBuf;` |
|        1 |  2849 |  |
|        - |  2850 |  |
|    12206 |  2851 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  2852 |  |
|        - |  2853 | `	SyHashEntry *pSlot;` |
|        - |  2854 | `	VmClassAttr *pVmAttr;` |
|        - |  2855 | `	ph7_class_attr *pAttr;` |
|    12208 |  2856 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    12208 |  2857 | `	if( pSlot == 0 ){` |
|    12062 |  2858 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  2859 | `	}` |
|      148 |  2860 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      148 |  2861 | `	pAttr = pVmAttr->pAttr;` |
|      148 |  2862 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  2863 | `		return SXRET_OK;` |
|        - |  2864 | `	}` |
|        - |  2865 | `	/* Union type: dispatch to the shared coercion helper. */` |
|      148 |  2866 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  2867 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  2868 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0);` |
|       16 |  2869 | `		if( rc == SXRET_OK ){` |
|        9 |  2870 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  2871 | `			return SXRET_OK;` |
|        - |  2872 | `		}` |
|        7 |  2873 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2874 | `			char zBuf[128];` |
|        4 |  2875 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  2876 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2877 | `		}` |
|        5 |  2878 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2879 | `	}` |
|        - |  2880 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      134 |  2881 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       10 |  2882 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|        8 |  2883 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        8 |  2884 | `			return SXRET_OK;` |
|        - |  2885 | `		}` |
|        3 |  2886 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  2887 | `	}` |
|        - |  2888 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  2889 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  2890 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      126 |  2891 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  2892 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  2893 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  2894 | `			return SXRET_OK;` |
|        - |  2895 | `		}` |
|        7 |  2896 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2897 | `	}` |
|      116 |  2898 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  2899 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  2900 | `		 * currently active on the self-stack. */` |
|       20 |  2901 | `		ph7_class *pExpected = 0;` |
|       20 |  2902 | `		SyString *pClassName = &pAttr->sClass;` |
|       20 |  2903 | `		ph7_class *pSelfNow = 0;` |
|       20 |  2904 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2905 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2906 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2907 | `		}` |
|       20 |  2908 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  2909 | `			pExpected = pSelfNow;` |
|       18 |  2910 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  2911 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2912 | `		}else{` |
|       16 |  2913 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  2914 | `		}` |
|       20 |  2915 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  2916 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2917 | `		}` |
|       20 |  2918 | `		if( pExpected ){` |
|       16 |  2919 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       16 |  2920 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  2921 | `				char zBuf[128];` |
|        7 |  2922 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2923 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2924 | `			}` |
|        5 |  2925 | `		}` |
|       16 |  2926 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       16 |  2927 | `		return SXRET_OK;` |
|        - |  2928 | `	}` |
|        - |  2929 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  2930 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|       98 |  2931 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  2932 | `		char zBuf[128];` |
|        7 |  2933 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  2934 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  2935 | `	}` |
|       94 |  2936 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  2937 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  2938 | `		if( xCast ){` |
|        - |  2939 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  2940 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  2941 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2942 | `			}` |
|       24 |  2943 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  2944 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  2945 | `			}` |
|        - |  2946 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  2947 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  2948 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  2949 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  2950 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  2951 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  2952 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  2953 | `			}` |
|       12 |  2954 | `			xCast(pValue);` |
|        5 |  2955 | `		}` |
|        5 |  2956 | `	}` |
|       80 |  2957 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       80 |  2958 | `	return SXRET_OK;` |
|     6105 |  2959 |  |
|        - |  2960 |  |
|        - |  2961 | `/*` |
|        - |  2962 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2963 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2964 | ` * information.` |
|        - |  2965 | ` * ------------------------------------` |
|        - |  2966 | ` * Simple boring wrapper function.` |
|        - |  2967 | ` * ------------------------------------` |
|        - |  2968 | ` */` |
|       16 |  2969 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2970 |  |
|        - |  2971 | `	va_list ap;` |
|        - |  2972 | `	sxi32 rc;` |
|       17 |  2973 | `	va_start(ap,zFormat);` |
|       17 |  2974 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  2975 | `	va_end(ap);` |
|       17 |  2976 | `	return rc;` |
|        1 |  2977 |  |
|        - |  2978 | `/*` |
|        - |  2979 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  2980 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  2981 | ` */` |
|       30 |  2982 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  2983 |  |
|        - |  2984 | `	ph7_class *pClass;` |
|        - |  2985 | `	ph7_class_instance *pThis;` |
|        - |  2986 | `	ph7_class_method *pCons;` |
|        - |  2987 | `	ph7_value sArg;` |
|        - |  2988 | `	ph7_value *apArg[1];` |
|        - |  2989 | `	SyBlob sMsg;` |
|        - |  2990 | `	SyString sMsgStr;` |
|        - |  2991 | `	VmFrame *pFrame;` |
|        - |  2992 | `	sxi32 rc;` |
|       31 |  2993 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       31 |  2994 | `	if( pClass == 0 ){` |
|      ! 0 |  2995 | `		return PH7_ABORT;` |
|        - |  2996 | `	}` |
|       31 |  2997 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       31 |  2998 | `	if( pThis == 0 ){` |
|      ! 0 |  2999 | `		return PH7_ABORT;` |
|        - |  3000 | `	}` |
|       31 |  3001 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       31 |  3002 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       15 |  3003 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       31 |  3004 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       31 |  3005 | `	if( pCons ){` |
|       31 |  3006 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       31 |  3007 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       31 |  3008 | `		apArg[0] = &sArg;` |
|       31 |  3009 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       31 |  3010 | `		PH7_MemObjRelease(&sArg);` |
|       15 |  3011 | `	}` |
|       31 |  3012 | `	SyBlobRelease(&sMsg);` |
|       31 |  3013 | `	pFrame = pVm->pFrame;` |
|       31 |  3014 | `	if( pFrame ){` |
|       31 |  3015 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       31 |  3016 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       15 |  3017 | `	}` |
|       31 |  3018 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       31 |  3019 | `	PH7_ClassInstanceUnref(pThis);` |
|       31 |  3020 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3021 | `		return PH7_ABORT;` |
|        - |  3022 | `	}` |
|       31 |  3023 | `	return PH7_EXCEPTION;` |
|       16 |  3024 |  |
|        - |  3025 | `/*` |
|        - |  3026 | ` * Report a fatal named-argument error.` |
|        - |  3027 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3028 | ` */` |
|        6 |  3029 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3030 |  |
|        7 |  3031 | `	const char *zFunc = 0;` |
|        7 |  3032 | `	int nFunc = 0;` |
|        7 |  3033 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3034 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3035 |  |
|        - |  3036 | `/*` |
|        - |  3037 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3038 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3039 | ` * information.` |
|        - |  3040 | ` * ------------------------------------` |
|        - |  3041 | ` * Simple boring wrapper function.` |
|        - |  3042 | ` * ------------------------------------` |
|        - |  3043 | ` */` |
|       24 |  3044 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3045 |  |
|        - |  3046 | `	sxi32 rc;` |
|       26 |  3047 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3048 | `	return rc;` |
|        2 |  3049 |  |
|        - |  3050 | `/*` |
|        - |  3051 | ` * Resolve function context from the current frame.` |
|        - |  3052 | ` */` |
|      964 |  3053 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3054 |  |
|        - |  3055 | `	VmFrame *pFrame;` |
|        - |  3056 | `	ph7_vm_func *pFunc;` |
|      965 |  3057 | `	*pzFuncName = 0;` |
|      965 |  3058 | `	*pnFuncLen = 0;` |
|      965 |  3059 | `	pFrame = pVm->pFrame;` |
|      965 |  3060 | `	if( pFrame == 0 ){` |
|      ! 0 |  3061 | `		return;` |
|        - |  3062 | `	}` |
|      965 |  3063 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      965 |  3064 | `	if( pFrame->pParent == 0 ){` |
|      951 |  3065 | `		return;` |
|        - |  3066 | `	}` |
|       15 |  3067 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  3068 | `	if( pFunc == 0 ){` |
|      ! 0 |  3069 | `		return;` |
|        - |  3070 | `	}` |
|       15 |  3071 | `	*pzFuncName = pFunc->sName.zString;` |
|       15 |  3072 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      483 |  3073 |  |
|        - |  3074 | `/*` |
|        - |  3075 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3076 | ` */` |
|      492 |  3077 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3078 |  |
|        - |  3079 | `	SyBlob sOut;` |
|        - |  3080 | `	SyString *pFile;` |
|      493 |  3081 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3082 | `		return PH7_OK;` |
|        - |  3083 | `	}` |
|      493 |  3084 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3085 | `		zClass = "Exception";` |
|      ! 0 |  3086 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3087 | `	}` |
|      493 |  3088 | `	if( zMsg == 0 ){` |
|      ! 0 |  3089 | `		zMsg = "Unknown exception";` |
|      ! 0 |  3090 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  3091 | `	}` |
|      493 |  3092 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      481 |  3093 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      240 |  3094 | `	}` |
|      493 |  3095 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      493 |  3096 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      493 |  3097 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      493 |  3098 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      493 |  3099 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      493 |  3100 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      493 |  3101 | `	if( pFile ){` |
|      493 |  3102 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      493 |  3103 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      493 |  3104 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      246 |  3105 | `	}` |
|      493 |  3106 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      493 |  3107 | `	if( pFile ){` |
|      493 |  3108 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      493 |  3109 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      493 |  3110 | `		if( zFuncName && nFuncLen > 0 ){` |
|       15 |  3111 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        8 |  3112 | `		}else{` |
|      479 |  3113 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3114 | `		}` |
|      246 |  3115 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3116 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3117 | `	}else{` |
|      ! 0 |  3118 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3119 | `	}` |
|      493 |  3120 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      493 |  3121 | `	if( pFile ){` |
|      493 |  3122 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      493 |  3123 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      493 |  3124 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      493 |  3125 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      246 |  3126 | `	}` |
|      493 |  3127 | `	VmCallErrorHandler(pVm,&sOut);` |
|      493 |  3128 | `	SyBlobRelease(&sOut);` |
|      493 |  3129 | `	return PH7_ABORT;` |
|      247 |  3130 |  |
|        - |  3131 | `/*` |
|        - |  3132 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3133 | ` */` |
|      480 |  3134 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3135 |  |
|        - |  3136 | `	ph7_vm *pVm;` |
|        - |  3137 | `	ph7_class *pClass;` |
|        - |  3138 | `	ph7_class_instance *pThis;` |
|        - |  3139 | `	ph7_class_method *pCons;` |
|        - |  3140 | `	ph7_value sArg;` |
|        - |  3141 | `	ph7_value *apArg[1];` |
|        - |  3142 | `	SyBlob sMsg;` |
|        - |  3143 | `	SyString sMsgStr;` |
|        - |  3144 | `	VmFrame *pFrame;` |
|        - |  3145 | `	va_list ap;` |
|        - |  3146 | `	sxi32 rc;` |
|        - |  3147 |  |
|      482 |  3148 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3149 | `		return PH7_ABORT;` |
|        - |  3150 | `	}` |
|      482 |  3151 | `	pVm = pCtx->pVm;` |
|      482 |  3152 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3153 | `		zClass = "Error";` |
|      ! 0 |  3154 | `	}` |
|      482 |  3155 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      482 |  3156 | `	if( pClass == 0 ){` |
|      ! 0 |  3157 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3158 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3159 | `			zClass` |
|        - |  3160 | `			);` |
|        - |  3161 | `	}` |
|      482 |  3162 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      482 |  3163 | `	if( pThis == 0 ){` |
|      ! 0 |  3164 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3165 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3166 | `			);` |
|        - |  3167 | `	}` |
|        - |  3168 |  |
|      482 |  3169 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      482 |  3170 | `	va_start(ap,zFormat);` |
|      482 |  3171 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      482 |  3172 | `	va_end(ap);` |
|        - |  3173 |  |
|      482 |  3174 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      482 |  3175 | `	if( pCons ){` |
|      482 |  3176 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      482 |  3177 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      482 |  3178 | `		apArg[0] = &sArg;` |
|      482 |  3179 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      482 |  3180 | `		PH7_MemObjRelease(&sArg);` |
|      240 |  3181 | `	}` |
|      482 |  3182 | `	SyBlobRelease(&sMsg);` |
|        - |  3183 |  |
|      482 |  3184 | `	pFrame = pVm->pFrame;` |
|      482 |  3185 | `	if( pFrame ){` |
|      482 |  3186 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      482 |  3187 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      240 |  3188 | `	}` |
|      482 |  3189 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      482 |  3190 | `	PH7_ClassInstanceUnref(pThis);` |
|      482 |  3191 | `	if( rc == SXERR_ABORT ){` |
|      471 |  3192 | `		return PH7_ABORT;` |
|        - |  3193 | `	}` |
|       12 |  3194 | `	return PH7_EXCEPTION;` |
|      242 |  3195 |  |
|        - |  3196 | `/*` |
|        - |  3197 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3198 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3199 | ` */` |
|      ! 0 |  3200 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3201 |  |
|        - |  3202 | `	ph7_vm *pVm;` |
|        - |  3203 | `	SyBlob sMsg;` |
|      ! 0 |  3204 | `	const char *zFuncName = 0;` |
|      ! 0 |  3205 | `	int nFuncLen = 0;` |
|        - |  3206 | `	va_list ap;` |
|        - |  3207 | `	sxi32 rc;` |
|        - |  3208 |  |
|      ! 0 |  3209 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3210 | `		return PH7_OK;` |
|        - |  3211 | `	}` |
|      ! 0 |  3212 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3213 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3214 | `		zClass = "Error";` |
|      ! 0 |  3215 | `	}` |
|        - |  3216 |  |
|      ! 0 |  3217 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3218 |  |
|      ! 0 |  3219 | `	va_start(ap,zFormat);` |
|      ! 0 |  3220 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3221 | `	va_end(ap);` |
|        - |  3222 |  |
|      ! 0 |  3223 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3224 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3225 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3226 | `	}` |
|      ! 0 |  3227 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3228 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3229 | `	}` |
|      ! 0 |  3230 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3231 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3232 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3233 | `	return rc;` |
|      ! 0 |  3234 |  |
|        - |  3235 | `/*` |
|        - |  3236 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3237 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3238 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3239 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3240 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3241 | ` * when VmByteCodeExec returns.` |
|        - |  3242 | ` */` |
|      144 |  3243 | `static sxi32 VmSuspendCtx(` |
|        - |  3244 | `	ph7_vm *pVm,` |
|        - |  3245 | `	ph7_exec_ctx *pCtx,` |
|        - |  3246 | `	sxi32 pc,` |
|        - |  3247 | `	sxi32 nTos` |
|        - |  3248 | `	)` |
|        2 |  3249 |  |
|       72 |  3250 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3251 | `	pCtx->pc = pc;` |
|      146 |  3252 | `	pCtx->nTos = nTos;` |
|      146 |  3253 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3254 | `	return PH7_SUSPEND;` |
|        2 |  3255 |  |
|        - |  3256 | `/*` |
|        - |  3257 | ` * Resolve named-argument mapping.` |
|        - |  3258 | ` *` |
|        - |  3259 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3260 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3261 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3262 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3263 | ` * every formal parameter that received a value.` |
|        - |  3264 | ` *` |
|        - |  3265 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3266 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3267 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3268 | ` */` |
|       92 |  3269 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3270 | `	ph7_vm *pVm,` |
|        - |  3271 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3272 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3273 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3274 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3275 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3276 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3277 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3278 |  |
|        2 |  3279 |  |
|       94 |  3280 | `	sxi32 posIdx = 0;` |
|        - |  3281 | `	sxu32 i;` |
|        - |  3282 | `	char zErrMsg[256];` |
|       94 |  3283 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      278 |  3284 | `	for( i = 0; i < nActual; i++ ){` |
|      186 |  3285 | `		aSlot[i] = -2;` |
|       94 |  3286 | `	}` |
|      272 |  3287 | `	for( i = 0; i < nActual; i++ ){` |
|      269 |  3288 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3289 | `			/* Named argument — find formal by name */` |
|      174 |  3290 | `			int found = 0;` |
|        - |  3291 | `			sxu32 k;` |
|      288 |  3292 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      274 |  3293 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      265 |  3294 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      252 |  3295 | `						pMap->aNames[i].zString,` |
|      378 |  3296 | `						pMap->aNames[i].nByte) == 0 ){` |
|      162 |  3297 | `					if( aUsed[k] ){` |
|        7 |  3298 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3299 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3300 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3301 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3302 | `						return PH7_ABORT;` |
|        - |  3303 | `					}` |
|      158 |  3304 | `					aSlot[i] = (sxi32)k;` |
|      158 |  3305 | `					aUsed[k] = 1;` |
|      158 |  3306 | `					found = 1;` |
|      158 |  3307 | `					break;` |
|        - |  3308 | `				}` |
|       59 |  3309 | `			}` |
|      170 |  3310 | `			if( !found ){` |
|       14 |  3311 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3312 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3313 | `				}else{` |
|        4 |  3314 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3315 | `						"Unknown named parameter $%.*s",` |
|        2 |  3316 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3317 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3318 | `					return PH7_ABORT;` |
|        - |  3319 | `				}` |
|        5 |  3320 | `			}` |
|       85 |  3321 | `		}else{` |
|        - |  3322 | `			/* Positional argument */` |
|       14 |  3323 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       14 |  3324 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3325 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3326 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3327 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3328 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3329 | `					return PH7_ABORT;` |
|        - |  3330 | `				}` |
|       14 |  3331 | `				aSlot[i] = posIdx;` |
|       14 |  3332 | `				aUsed[posIdx] = 1;` |
|        6 |  3333 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3334 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3335 | `			}` |
|       14 |  3336 | `			posIdx++;` |
|        - |  3337 | `		}` |
|       91 |  3338 | `	}` |
|       87 |  3339 | `	return SXRET_OK;` |
|       48 |  3340 |  |
|        - |  3341 | `/*` |
|        - |  3342 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3343 | ` *` |
|        - |  3344 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3345 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3346 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3347 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3348 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3349 | ` * then the program execution is halted.` |
|        - |  3350 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3351 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3352 | ` * or to reset the VM to it's initial state.` |
|        - |  3353 | ` */` |
|    38224 |  3354 | `static sxi32 VmByteCodeExec(` |
|        - |  3355 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3356 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3357 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3358 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3359 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3360 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3361 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3362 | `	sxi32 nPc            /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3363 | `	)` |
|        2 |  3364 |  |
|        - |  3365 | `	VmInstr *pInstr;` |
|        - |  3366 | `	ph7_value *pTos;` |
|        - |  3367 | `	SySet aArg;` |
|        - |  3368 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3369 | `	sxi32 pc;` |
|        - |  3370 | `	sxi32 rc;` |
|        - |  3371 | `	/* Argument container */` |
|    38226 |  3372 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    38226 |  3373 | `	if( nTos < 0 ){` |
|    35964 |  3374 | `		pTos = &pStack[-1];` |
|    17983 |  3375 | `	}else{` |
|     2264 |  3376 | `		pTos = &pStack[nTos];` |
|        - |  3377 | `	}` |
|    38226 |  3378 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    38226 |  3379 | `	pc = nPc;` |
|        - |  3380 | `/*` |
|        - |  3381 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3382 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3383 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3384 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3385 | ` */` |
|        - |  3386 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3387 | `	{ \` |
|        - |  3388 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3389 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3390 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3391 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3392 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3393 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3394 | `				break; \` |
|        - |  3395 | `			} \` |
|        - |  3396 | `			goto Exception; \` |
|        - |  3397 | `		} \` |
|        - |  3398 | `	}` |
|        - |  3399 | `	/* Execute as much as we can */` |
|  5456875 |  3400 | `	for(;;){` |
|        - |  3401 | `		/* Fetch the instruction to execute */` |
| 10913048 |  3402 | `		pInstr = &aInstr[pc];` |
| 10913048 |  3403 | `		rc = SXRET_OK;` |
|        - |  3404 | `/*` |
|        - |  3405 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3406 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3407 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3408 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3409 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3410 | ` */` |
| 10913048 |  3411 | `		switch(pInstr->iOp){` |
|        - |  3412 | `/*` |
|        - |  3413 | ` * DONE: P1 * *` |
|        - |  3414 | ` *` |
|        - |  3415 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3416 | ` * and return immediately.` |
|        - |  3417 | ` */` |
|    18783 |  3418 | `case PH7_OP_DONE:` |
|    37568 |  3419 | `	if( pInstr->iP1 ){` |
|        - |  3420 | `#ifdef UNTRUST` |
|        - |  3421 | `		if( pTos < pStack ){` |
|        - |  3422 | `			goto Abort;` |
|        - |  3423 | `		}` |
|        - |  3424 | `#endif` |
|    22296 |  3425 | `		if( pLastRef ){` |
|    14168 |  3426 | `			*pLastRef = pTos->nIdx;` |
|     7083 |  3427 | `		}` |
|    22296 |  3428 | `		if( pResult ){` |
|        - |  3429 | `			/* Execution result */` |
|    21196 |  3430 | `			PH7_MemObjStore(pTos,pResult);` |
|    10597 |  3431 | `		}` |
|    22296 |  3432 | `		VmPopOperand(&pTos,1);` |
|    26421 |  3433 | `	}else if( pLastRef ){` |
|        - |  3434 | `		/* Nothing referenced */` |
|     1344 |  3435 | `		*pLastRef = SXU32_HIGH;` |
|      671 |  3436 | `	}` |
|        - |  3437 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3438 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3439 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3440 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3441 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3442 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3443 | `	 * block can override it.` |
|        - |  3444 | `	 */` |
|    37570 |  3445 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3446 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3447 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3448 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3449 | `		pExc->pFrame = 0;` |
|        3 |  3450 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3451 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3452 | `			pExc->iFinallyDone = 1;` |
|        - |  3453 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3454 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3455 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3456 | `				goto Abort;` |
|        - |  3457 | `			}` |
|        1 |  3458 | `		}` |
|        1 |  3459 | `	}` |
|    37568 |  3460 | `	goto Done;` |
|        - |  3461 | `/*` |
|        - |  3462 | ` * HALT: P1 * *` |
|        - |  3463 | ` *` |
|        - |  3464 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3465 | ` * and abort immediately.` |
|        - |  3466 | ` */` |
|        4 |  3467 | `case PH7_OP_HALT:` |
|        9 |  3468 | `	if( pInstr->iP1 ){` |
|        - |  3469 | `#ifdef UNTRUST` |
|        - |  3470 | `		if( pTos < pStack ){` |
|        - |  3471 | `			goto Abort;` |
|        - |  3472 | `		}` |
|        - |  3473 | `#endif` |
|        9 |  3474 | `		if( pLastRef ){` |
|      ! 0 |  3475 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3476 | `		}` |
|        9 |  3477 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3478 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3479 | `				/* Output the exit message */` |
|        7 |  3480 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3481 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3482 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3483 | `			}` |
|        7 |  3484 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3485 | `			/* Record exit status */` |
|        5 |  3486 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3487 | `		}` |
|        9 |  3488 | `		VmPopOperand(&pTos,1);` |
|        4 |  3489 | `	}else if( pLastRef ){` |
|        - |  3490 | `		/* Nothing referenced */` |
|      ! 0 |  3491 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3492 | `	}` |
|        - |  3493 | `	/* Check if we're in an included file context */` |
|        9 |  3494 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3495 | `		/* Terminate the entire process */` |
|        9 |  3496 | `		exit(pVm->iExitStatus);` |
|        - |  3497 | `	}` |
|      ! 0 |  3498 | `	goto Abort;` |
|        - |  3499 | `/*` |
|        - |  3500 | ` * JMP: * P2 *` |
|        - |  3501 | ` *` |
|        - |  3502 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3503 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3504 | ` */` |
|   233841 |  3505 | `case PH7_OP_JMP:` |
|   467728 |  3506 | `	pc = pInstr->iP2 - 1;` |
|   467728 |  3507 | `	break;` |
|        - |  3508 | `/*` |
|        - |  3509 | ` * JZ: P1 P2 *` |
|        - |  3510 | ` *` |
|        - |  3511 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3512 | ` * entry in the stack if P1 is zero.` |
|        - |  3513 | ` */` |
|   551943 |  3514 | `case PH7_OP_JZ:` |
|        - |  3515 | `#ifdef UNTRUST` |
|        - |  3516 | `	if( pTos < pStack ){` |
|        - |  3517 | `		goto Abort;` |
|        - |  3518 | `	}` |
|        - |  3519 | `#endif` |
|        - |  3520 | `	/* Get a boolean value */` |
|  1103976 |  3521 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  3522 | `		PH7_MemObjToBool(pTos);` |
|       85 |  3523 | `	}` |
|  1103976 |  3524 | `	if( !pTos->x.iVal ){` |
|        - |  3525 | `		/* Take the jump */` |
|   563910 |  3526 | `		pc = pInstr->iP2 - 1;` |
|   281954 |  3527 | `	}` |
|  1103976 |  3528 | `	if( !pInstr->iP1 ){` |
|   876646 |  3529 | `		VmPopOperand(&pTos,1);` |
|   438344 |  3530 | `	}` |
|  1103976 |  3531 | `	break;` |
|        - |  3532 | `/*` |
|        - |  3533 | ` * JNZ: P1 P2 *` |
|        - |  3534 | ` *` |
|        - |  3535 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3536 | ` * entry in the stack if P1 is zero.` |
|        - |  3537 | ` */` |
|    57686 |  3538 | `case PH7_OP_JNZ:` |
|        - |  3539 | `#ifdef UNTRUST` |
|        - |  3540 | `	if( pTos < pStack ){` |
|        - |  3541 | `		goto Abort;` |
|        - |  3542 | `	}` |
|        - |  3543 | `#endif` |
|        - |  3544 | `	/* Get a boolean value */` |
|   115374 |  3545 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3546 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3547 | `	}` |
|   115374 |  3548 | `	if( pTos->x.iVal ){` |
|        - |  3549 | `		/* Take the jump */` |
|     5068 |  3550 | `		pc = pInstr->iP2 - 1;` |
|     2533 |  3551 | `	}` |
|   115374 |  3552 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3553 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  3554 | `	}` |
|   115374 |  3555 | `	break;` |
|        - |  3556 | `/*` |
|        - |  3557 | ` * NOOP: * * *` |
|        - |  3558 | ` *` |
|        - |  3559 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  3560 | ` * destination.` |
|        - |  3561 | ` */` |
|      ! 0 |  3562 | `case PH7_OP_NOOP:` |
|      ! 0 |  3563 | `	break;` |
|        - |  3564 | `/*` |
|        - |  3565 | ` * POP: P1 * *` |
|        - |  3566 | ` *` |
|        - |  3567 | ` * Pop P1 elements from the operand stack.` |
|        - |  3568 | ` */` |
|   427278 |  3569 | `case PH7_OP_POP: {` |
|   854602 |  3570 | `	sxi32 n = pInstr->iP1;` |
|   854602 |  3571 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  3572 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  3573 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  3574 | `	}` |
|   854602 |  3575 | `	VmPopOperand(&pTos,n);` |
|   854602 |  3576 | `	break;` |
|        - |  3577 | `				 }` |
|        - |  3578 | `/*` |
|        - |  3579 | ` * DUP: * * *` |
|        - |  3580 | ` *` |
|        - |  3581 | ` * Duplicate the top of the stack.` |
|        - |  3582 | ` */` |
|       41 |  3583 | `case PH7_OP_DUP:` |
|        - |  3584 | `#ifdef UNTRUST` |
|        - |  3585 | `	if( pTos < pStack ){` |
|        - |  3586 | `		goto Abort;` |
|        - |  3587 | `	}` |
|        - |  3588 | `#endif` |
|       84 |  3589 | `	pTos++;` |
|       84 |  3590 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  3591 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  3592 | `	break;` |
|        - |  3593 | `/*` |
|        - |  3594 | ` * NSSWITCH: * * P3` |
|        - |  3595 | ` *` |
|        - |  3596 | ` * Switch the active namespace at runtime.` |
|        - |  3597 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  3598 | ` */` |
|     7171 |  3599 | `case PH7_OP_NSSWITCH:` |
|    14344 |  3600 | `	SyBlobReset(&pVm->sNamespace);` |
|    14344 |  3601 | `	if( pInstr->p3 ){` |
|       96 |  3602 | `		const char *zNs = (const char *)pInstr->p3;` |
|       96 |  3603 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       47 |  3604 | `	}` |
|        - |  3605 | `	/* Clear namespace-scoped use-const imports */` |
|    14344 |  3606 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    14344 |  3607 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    14344 |  3608 | `	break;` |
|        - |  3609 | `/* OP_USECONST P1 * P3` |
|        - |  3610 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  3611 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  3612 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  3613 | ` */` |
|        7 |  3614 | `case PH7_OP_USECONST: {` |
|       16 |  3615 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  3616 | `	if( azPair ){` |
|       16 |  3617 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  3618 | `	}` |
|       16 |  3619 | `	break;` |
|        - |  3620 | `				}` |
|        - |  3621 | `/*` |
|        - |  3622 | ` * CVT_INT: * * *` |
|        - |  3623 | ` *` |
|        - |  3624 | ` * Force the top of the stack to be an integer.` |
|        - |  3625 | ` */` |
|       77 |  3626 | `case PH7_OP_CVT_INT:` |
|        - |  3627 | `#ifdef UNTRUST` |
|        - |  3628 | `	if( pTos < pStack ){` |
|        - |  3629 | `		goto Abort;` |
|        - |  3630 | `	}` |
|        - |  3631 | `#endif` |
|      156 |  3632 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      109 |  3633 | `		PH7_MemObjToInteger(pTos);` |
|       54 |  3634 | `	}` |
|        - |  3635 | `	/* Invalidate any prior representation */` |
|      156 |  3636 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      156 |  3637 | `	break;` |
|        - |  3638 | `/*` |
|        - |  3639 | ` * CVT_REAL: * * *` |
|        - |  3640 | ` *` |
|        - |  3641 | ` * Force the top of the stack to be a real.` |
|        - |  3642 | ` */` |
|        4 |  3643 | `case PH7_OP_CVT_REAL:` |
|        - |  3644 | `#ifdef UNTRUST` |
|        - |  3645 | `	if( pTos < pStack ){` |
|        - |  3646 | `		goto Abort;` |
|        - |  3647 | `	}` |
|        - |  3648 | `#endif` |
|        9 |  3649 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3650 | `		PH7_MemObjToReal(pTos);` |
|        2 |  3651 | `	}` |
|        - |  3652 | `	/* Invalidate any prior representation */` |
|        9 |  3653 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  3654 | `	break;` |
|        - |  3655 | `/*` |
|        - |  3656 | ` * CVT_STR: * * *` |
|        - |  3657 | ` *` |
|        - |  3658 | ` * Force the top of the stack to be a string.` |
|        - |  3659 | ` */` |
|      146 |  3660 | `case PH7_OP_CVT_STR:` |
|        - |  3661 | `#ifdef UNTRUST` |
|        - |  3662 | `	if( pTos < pStack ){` |
|        - |  3663 | `		goto Abort;` |
|        - |  3664 | `	}` |
|        - |  3665 | `#endif` |
|      294 |  3666 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  3667 | `		PH7_MemObjToString(pTos);` |
|      146 |  3668 | `	}` |
|      294 |  3669 | `	break;` |
|        - |  3670 | `/*` |
|        - |  3671 | ` * CVT_BOOL: * * *` |
|        - |  3672 | ` *` |
|        - |  3673 | ` * Force the top of the stack to be a boolean.` |
|        - |  3674 | ` */` |
|        5 |  3675 | `case PH7_OP_CVT_BOOL:` |
|        - |  3676 | `#ifdef UNTRUST` |
|        - |  3677 | `	if( pTos < pStack ){` |
|        - |  3678 | `		goto Abort;` |
|        - |  3679 | `	}` |
|        - |  3680 | `#endif` |
|       11 |  3681 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  3682 | `		PH7_MemObjToBool(pTos);` |
|        3 |  3683 | `	}` |
|       11 |  3684 | `	break;` |
|        - |  3685 | `/*` |
|        - |  3686 | ` * CVT_NULL: * * *` |
|        - |  3687 | ` *` |
|        - |  3688 | ` * Nullify the top of the stack.` |
|        - |  3689 | ` */` |
|        3 |  3690 | `case PH7_OP_CVT_NULL:` |
|        - |  3691 | `#ifdef UNTRUST` |
|        - |  3692 | `	if( pTos < pStack ){` |
|        - |  3693 | `		goto Abort;` |
|        - |  3694 | `	}` |
|        - |  3695 | `#endif` |
|        7 |  3696 | `	PH7_MemObjRelease(pTos);` |
|        7 |  3697 | `	break;` |
|        - |  3698 | `/*` |
|        - |  3699 | ` * CVT_NUMC: * * *` |
|        - |  3700 | ` *` |
|        - |  3701 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  3702 | ` */` |
|      ! 0 |  3703 | `case PH7_OP_CVT_NUMC:` |
|        - |  3704 | `#ifdef UNTRUST` |
|        - |  3705 | `	if( pTos < pStack ){` |
|        - |  3706 | `		goto Abort;` |
|        - |  3707 | `	}` |
|        - |  3708 | `#endif` |
|        - |  3709 | `	/* Force a numeric cast */` |
|      ! 0 |  3710 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  3711 | `	break;` |
|        - |  3712 | `/*` |
|        - |  3713 | ` * CVT_ARRAY: * * *` |
|        - |  3714 | ` *` |
|        - |  3715 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  3716 | ` */` |
|       10 |  3717 | `case PH7_OP_CVT_ARRAY:` |
|        - |  3718 | `#ifdef UNTRUST` |
|        - |  3719 | `	if( pTos < pStack ){` |
|        - |  3720 | `		goto Abort;` |
|        - |  3721 | `	}` |
|        - |  3722 | `#endif` |
|        - |  3723 | `	/* Force a hashmap cast */` |
|       21 |  3724 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  3725 | `	if( rc != SXRET_OK ){` |
|        - |  3726 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  3727 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  3728 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  3729 | `	}` |
|       21 |  3730 | `	break;` |
|        - |  3731 | `/*` |
|        - |  3732 | ` * CVT_OBJ: * * *` |
|        - |  3733 | ` *` |
|        - |  3734 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  3735 | ` */` |
|        8 |  3736 | `case PH7_OP_CVT_OBJ:` |
|        - |  3737 | `#ifdef UNTRUST` |
|        - |  3738 | `	if( pTos < pStack ){` |
|        - |  3739 | `		goto Abort;` |
|        - |  3740 | `	}` |
|        - |  3741 | `#endif` |
|       17 |  3742 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  3743 | `		/* Force a 'stdClass()' cast */` |
|       17 |  3744 | `		PH7_MemObjToObject(pTos);` |
|        8 |  3745 | `	}` |
|       17 |  3746 | `	break;` |
|        - |  3747 | `/*` |
|        - |  3748 | ` * ERR_CTRL * * *` |
|        - |  3749 | ` *` |
|        - |  3750 | ` * Error control operator.` |
|        - |  3751 | ` */` |
|    14553 |  3752 | `case PH7_OP_ERR_CTRL:` |
|        - |  3753 | `	/*` |
|        - |  3754 | `	 * TICKET 1433-038:` |
|        - |  3755 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  3756 | `	 * use the public API,to control error output.` |
|        - |  3757 | `	 */` |
|    29106 |  3758 | `	break;` |
|        - |  3759 | `/*` |
|        - |  3760 | ` * IS_A * * *` |
|        - |  3761 | ` *` |
|        - |  3762 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  3763 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  3764 | ` * holding a class name or an object).` |
|        - |  3765 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  3766 | ` */` |
|       23 |  3767 | `case PH7_OP_IS_A:{` |
|       48 |  3768 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  3769 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  3770 | `#ifdef UNTRUST` |
|        - |  3771 | `	if( pNos < pStack ){` |
|        - |  3772 | `		goto Abort;` |
|        - |  3773 | `	}` |
|        - |  3774 | `#endif` |
|       48 |  3775 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  3776 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  3777 | `		ph7_class *pClass = 0;` |
|        - |  3778 | `		/* Extract the target class */` |
|       46 |  3779 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  3780 | `			/* Instance already loaded */` |
|      ! 0 |  3781 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  3782 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  3783 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  3784 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  3785 | `			/* Handle self/static/parent keywords */` |
|       46 |  3786 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  3787 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  3788 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  3789 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  3790 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  3791 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  3792 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  3793 | `					pClass = pSelf->pBase;` |
|        2 |  3794 | `				}` |
|        3 |  3795 | `			}else{` |
|       36 |  3796 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  3797 | `			}` |
|       22 |  3798 | `		}` |
|       46 |  3799 | `		if( pClass ){` |
|        - |  3800 | `			/* Perform the query */` |
|       46 |  3801 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  3802 | `		}` |
|       22 |  3803 | `	}` |
|        - |  3804 | `	/* Push result */` |
|       48 |  3805 | `	VmPopOperand(&pTos,1);` |
|       48 |  3806 | `	PH7_MemObjRelease(pTos);` |
|       48 |  3807 | `	pTos->x.iVal = iRes;` |
|       48 |  3808 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  3809 | `	break;` |
|        - |  3810 | `				 }` |
|        - |  3811 |  |
|        - |  3812 | `/*` |
|        - |  3813 | ` * LOADC P1 P2 *` |
|        - |  3814 | ` *` |
|        - |  3815 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  3816 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  3817 | ` */` |
|   927169 |  3818 | `case PH7_OP_LOADC: {` |
|        - |  3819 | `	ph7_value *pObj;` |
|        - |  3820 | `	/* Reserve a room */` |
|  1854384 |  3821 | `	pTos++;` |
|  2772635 |  3822 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1854384 |  3823 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  3824 | `			SyHashEntry *pEntry;` |
|        - |  3825 | `			/* Check use const imports first — imports take precedence */` |
|        - |  3826 | `			{` |
|        - |  3827 | `				SyHashEntry *pConstImport;` |
|    26891 |  3828 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    17926 |  3829 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17928 |  3830 | `				if( pConstImport ){` |
|       11 |  3831 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  3832 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  3833 | `					if( pEntry ){` |
|       11 |  3834 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  3835 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  3836 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  3837 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  3838 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  3839 | `						break;` |
|        - |  3840 | `					}` |
|        - |  3841 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  3842 | `				}` |
|        - |  3843 | `			}` |
|        - |  3844 | `			/* Candidate for expansion via user defined callbacks */` |
|    17918 |  3845 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    17918 |  3846 | `			if( pEntry ){` |
|    17914 |  3847 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3848 | `				/* Set a NULL default value */` |
|    17914 |  3849 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    17914 |  3850 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3851 | `				/* Invoke the callback and deal with the expanded value */` |
|    17914 |  3852 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3853 | `				/* Mark as constant */` |
|    17914 |  3854 | `				pTos->nIdx = SXU32_HIGH;` |
|    17914 |  3855 | `				break;` |
|        - |  3856 | `			}` |
|        - |  3857 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  3858 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  3859 | `			 * use-const imports → current NS → global → string fallback). */` |
|        - |  3860 | `			{` |
|        6 |  3861 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3862 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3863 | `				sxu32 j;` |
|        6 |  3864 | `				int isQualified = 0;` |
|       32 |  3865 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3866 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       15 |  3867 | `				}` |
|        6 |  3868 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  3869 | `					/* Try current_namespace\name */` |
|      ! 0 |  3870 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  3871 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  3872 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  3873 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  3874 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  3875 | `					if( pEntry ){` |
|      ! 0 |  3876 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  3877 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3878 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  3879 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  3880 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  3881 | `						break;` |
|        - |  3882 | `					}` |
|        - |  3883 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  3884 | `				}` |
|        6 |  3885 | `				if( isQualified ){` |
|        - |  3886 | `					/* Qualified name: must be a real constant. */` |
|        3 |  3887 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3888 | `					SyBlob sErr;` |
|        3 |  3889 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3890 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3891 | `					if( pErrFile ){` |
|        3 |  3892 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3893 | `					}` |
|        3 |  3894 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3895 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3896 | `					SyBlobRelease(&sErr);` |
|        3 |  3897 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3898 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  3899 | `					goto LoadC_Done;` |
|        - |  3900 | `				}` |
|        - |  3901 | `			}` |
|        1 |  3902 | `		}` |
|  1836460 |  3903 | `		PH7_MemObjLoad(pObj,pTos);` |
|   918253 |  3904 | `	}else{` |
|        - |  3905 | `		/* Set a NULL value */` |
|      ! 0 |  3906 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3907 | `	}` |
|   918208 |  3908 | `LoadC_Done:` |
|        - |  3909 | `	/* Mark as constant */` |
|  1836462 |  3910 | `	pTos->nIdx = SXU32_HIGH;` |
|  1836462 |  3911 | `	break;` |
|        - |  3912 | `				  }` |
|        - |  3913 | `/*` |
|        - |  3914 | ` * LOAD: P1 * P3` |
|        - |  3915 | ` *` |
|        - |  3916 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3917 | ` * from the P3 operand.` |
|        - |  3918 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3919 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3920 | ` */` |
|  1468090 |  3921 | `case PH7_OP_LOAD:{` |
|        - |  3922 | `	ph7_value *pObj;` |
|        - |  3923 | `	SyString sName;` |
|  2936402 |  3924 | `	if( pInstr->p3 == 0 ){` |
|        - |  3925 | `		/* Take the variable name from the top of the stack */` |
|        - |  3926 | `#ifdef UNTRUST` |
|        - |  3927 | `		if( pTos < pStack ){` |
|        - |  3928 | `			goto Abort;` |
|        - |  3929 | `		}` |
|        - |  3930 | `#endif` |
|        - |  3931 | `		/* Force a string cast */` |
|       19 |  3932 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3933 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3934 | `		}` |
|       19 |  3935 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3936 | `	}else{` |
|  2936384 |  3937 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3938 | `		/* Reserve a room for the target object */` |
|  2936384 |  3939 | `		pTos++;` |
|        - |  3940 | `	}` |
|        - |  3941 | `	/* Extract the requested memory object */` |
|  2936402 |  3942 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2936402 |  3943 | `	if( pObj == 0 ){` |
|       28 |  3944 | `		if( pInstr->iP1 ){` |
|        - |  3945 | `			/* Variable not found,load NULL */` |
|       28 |  3946 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3947 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3948 | `			}else{` |
|       28 |  3949 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3950 | `			}` |
|       28 |  3951 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1468105 |  3952 | `			break;` |
|      ! 0 |  3953 | `		}else{` |
|        - |  3954 | `			/* Fatal error */` |
|      ! 0 |  3955 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3956 | `			goto Abort;` |
|        - |  3957 | `		}` |
|        - |  3958 | `	}` |
|        - |  3959 | `	/* Load variable contents */` |
|  2936376 |  3960 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2936376 |  3961 | `	pTos->nIdx = pObj->nIdx;` |
|  2936376 |  3962 | `	break;` |
|        - |  3963 | `				   }` |
|        - |  3964 | `/*` |
|        - |  3965 | ` * LOAD_MAP P1 * *` |
|        - |  3966 | ` *` |
|        - |  3967 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3968 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3969 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3970 | ` */` |
|    20678 |  3971 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3972 | `	ph7_hashmap *pMap;` |
|        - |  3973 | `	/* Allocate a new hashmap instance */` |
|    41358 |  3974 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    41358 |  3975 | `	if( pMap == 0 ){` |
|      ! 0 |  3976 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3977 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3978 | `		goto Abort;` |
|        - |  3979 | `	}` |
|    41358 |  3980 | `	if( pInstr->iP1 > 0 ){` |
|     2374 |  3981 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3982 | `		/* Perform the insertion */` |
|     7284 |  3983 | `		while( pEntry < pTos ){` |
|     4912 |  3984 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3985 | `				/* Insertion by reference */` |
|      142 |  3986 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3987 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3988 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3989 | `					);` |
|       48 |  3990 | `			}else{` |
|        - |  3991 | `				/* Standard insertion */` |
|     7226 |  3992 | `				PH7_HashmapInsert(pMap,` |
|     4816 |  3993 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2408 |  3994 | `					&pEntry[1]` |
|        - |  3995 | `				);` |
|        - |  3996 | `			}` |
|        - |  3997 | `			/* Next pair on the stack */` |
|     4912 |  3998 | `			pEntry += 2;` |
|        2 |  3999 | `		}` |
|        - |  4000 | `		/* Pop P1 elements */` |
|     2374 |  4001 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1186 |  4002 | `	}` |
|        - |  4003 | `	/* Push the hashmap */` |
|    41358 |  4004 | `	pTos++;` |
|    41358 |  4005 | `	pTos->nIdx = SXU32_HIGH;` |
|    41358 |  4006 | `	pTos->x.pOther = pMap;` |
|    41358 |  4007 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    41358 |  4008 | `	break;` |
|        - |  4009 | `					  }` |
|        - |  4010 | `/*` |
|        - |  4011 | ` * LOAD_LIST: P1 * *` |
|        - |  4012 | ` *` |
|        - |  4013 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4014 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4015 | ` * Caveats:` |
|        - |  4016 | ` *  This implementation support only a single nesting level.` |
|        - |  4017 | ` */` |
|       48 |  4018 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4019 | `	ph7_value *pEntry;` |
|       98 |  4020 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4021 | `		/* Empty list,break immediately */` |
|      ! 0 |  4022 | `		break;` |
|        - |  4023 | `	}` |
|       98 |  4024 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4025 | `#ifdef UNTRUST` |
|        - |  4026 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4027 | `		goto Abort;` |
|        - |  4028 | `	}` |
|        - |  4029 | `#endif` |
|       98 |  4030 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4031 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4032 | `		ph7_hashmap_node *pNode;` |
|        - |  4033 | `		ph7_value sKey,*pObj;` |
|        - |  4034 | `		/* Start Copying */` |
|       91 |  4035 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4036 | `		while( pEntry <= pTos ){` |
|      193 |  4037 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4038 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4039 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4040 | `					if( rc == SXRET_OK ){` |
|        - |  4041 | `						/* Store node value */` |
|      165 |  4042 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4043 | `					}else{` |
|        - |  4044 | `						/* Undefined array key */` |
|        - |  4045 | `						char zMsg[128];` |
|      ! 0 |  4046 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4047 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4048 | `						PH7_MemObjRelease(pObj);` |
|        - |  4049 | `					}` |
|       82 |  4050 | `				}` |
|       82 |  4051 | `			}` |
|      193 |  4052 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4053 | `			pEntry++;` |
|        1 |  4054 | `		}` |
|       46 |  4055 | `	}else{` |
|        - |  4056 | `		/* Source is not an array */` |
|        - |  4057 | `		ph7_value *pObj;` |
|       18 |  4058 | `		while( pEntry <= pTos ){` |
|       12 |  4059 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4060 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4061 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4062 | `				}` |
|        5 |  4063 | `			}` |
|       12 |  4064 | `			pEntry++;` |
|        2 |  4065 | `		}` |
|        8 |  4066 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4067 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4068 | `			const char *zType = "unknown";` |
|        3 |  4069 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4070 | `			char zMsg[256];` |
|        3 |  4071 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4072 | `				zType = "string";` |
|        1 |  4073 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4074 | `				zType = "int";` |
|      ! 0 |  4075 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4076 | `				zType = "float";` |
|      ! 0 |  4077 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4078 | `				zType = "object";` |
|      ! 0 |  4079 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4080 | `				zType = "resource";` |
|      ! 0 |  4081 | `			}` |
|        3 |  4082 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4083 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4084 | `		}` |
|        - |  4085 | `	}` |
|       98 |  4086 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4087 | `	break;` |
|        - |  4088 | `					   }` |
|        - |  4089 | `/*` |
|        - |  4090 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4091 | ` *` |
|        - |  4092 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4093 | ` * from the stack.` |
|        - |  4094 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4095 | ` * instead.` |
|        - |  4096 | ` */` |
|   235592 |  4097 | `case PH7_OP_LOAD_IDX: {` |
|   471230 |  4098 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   471230 |  4099 | `	ph7_hashmap *pMap = 0;` |
|        - |  4100 | `	ph7_value *pIdx;` |
|   471230 |  4101 | `	pIdx = 0;` |
|   471230 |  4102 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4103 | `		if( !pInstr->iP2){` |
|        - |  4104 | `			/* No available index,load NULL */` |
|      ! 0 |  4105 | `			if( pTos >= pStack ){` |
|      ! 0 |  4106 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4107 | `			}else{` |
|        - |  4108 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4109 | `				pTos++;` |
|      ! 0 |  4110 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4111 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4112 | `			}` |
|        - |  4113 | `			/* Emit a notice */` |
|      ! 0 |  4114 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4115 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4116 | `			break;` |
|        - |  4117 | `		}` |
|      ! 0 |  4118 | `	}else{` |
|   471230 |  4119 | `		pIdx = pTos;` |
|   471230 |  4120 | `		pTos--;` |
|        - |  4121 | `	}` |
|   471230 |  4122 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4123 | `		/* String access */` |
|   367894 |  4124 | `		if( pIdx ){` |
|        - |  4125 | `			sxu32 nOfft;` |
|   367894 |  4126 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4127 | `				/* Force an int cast */` |
|      ! 0 |  4128 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4129 | `			}` |
|   367894 |  4130 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   367894 |  4131 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4132 | `				/* Invalid offset,load null */` |
|      ! 0 |  4133 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4134 | `			}else{` |
|   367894 |  4135 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   367894 |  4136 | `				int c = zData[nOfft];` |
|   367894 |  4137 | `				PH7_MemObjRelease(pTos);` |
|   367894 |  4138 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   367894 |  4139 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4140 | `			}` |
|   183970 |  4141 | `		}else{` |
|        - |  4142 | `			/* No available index,load NULL */` |
|      ! 0 |  4143 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4144 | `		}` |
|   367894 |  4145 | `		break;` |
|        - |  4146 | `	}` |
|   103338 |  4147 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4148 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4149 | `			ph7_value *pObj;` |
|        3 |  4150 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4151 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4152 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4153 | `			}` |
|        1 |  4154 | `		}` |
|        1 |  4155 | `	}` |
|   103338 |  4156 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   103338 |  4157 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   103338 |  4158 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4159 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4160 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4161 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4162 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      881 |  4163 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      440 |  4164 | `		}` |
|        - |  4165 | `		/* Point to the hashmap */` |
|   103338 |  4166 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   103338 |  4167 | `		if( pIdx ){` |
|        - |  4168 | `			/* Load the desired entry */` |
|   103338 |  4169 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    51668 |  4170 | `		}` |
|   103338 |  4171 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4172 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4173 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4174 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4175 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4176 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4177 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4178 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4179 | `			 * correct for the outermost write. */` |
|       19 |  4180 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4181 | `			if( !needWrite && pNode ){` |
|       13 |  4182 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4183 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4184 | `					needWrite = 1;` |
|        3 |  4185 | `				}` |
|        6 |  4186 | `			}` |
|       19 |  4187 | `			if( needWrite ){` |
|       13 |  4188 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4189 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4190 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4191 | `					 * into the new map's storage. */` |
|        7 |  4192 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4193 | `					if( pIdx ){` |
|        7 |  4194 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4195 | `					}` |
|        3 |  4196 | `				}` |
|        6 |  4197 | `			}` |
|        9 |  4198 | `		}` |
|   103338 |  4199 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4200 | `			/* Create a new empty entry */` |
|      273 |  4201 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4202 | `			if( rc == SXRET_OK ){` |
|        - |  4203 | `				/* Point to the last inserted entry */` |
|      273 |  4204 | `				pNode = pMap->pLast;` |
|      136 |  4205 | `			}` |
|      136 |  4206 | `		}` |
|    51668 |  4207 | `	}` |
|   103338 |  4208 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4209 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4210 | `		char zMsg[128];` |
|      ! 0 |  4211 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4212 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4213 | `		}` |
|      ! 0 |  4214 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4215 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4216 | `	}` |
|   103338 |  4217 | `	if( pIdx ){` |
|   103338 |  4218 | `		PH7_MemObjRelease(pIdx);` |
|    51668 |  4219 | `	}` |
|   103338 |  4220 | `	if( rc == SXRET_OK ){` |
|        - |  4221 | `		/* Load entry contents */` |
|    46362 |  4222 | `		if( pMap->iRef < 2 ){` |
|        - |  4223 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4224 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4225 | `			 */` |
|       24 |  4226 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4227 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4228 | `		}else{` |
|    46340 |  4229 | `			pTos->nIdx = pNode->nValIdx;` |
|    46340 |  4230 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    46340 |  4231 | `			PH7_HashmapUnref(pMap);` |
|        - |  4232 | `		}` |
|    23182 |  4233 | `	}else{` |
|        - |  4234 | `		/* No such entry,load NULL */` |
|    56978 |  4235 | `		PH7_MemObjRelease(pTos);` |
|    56978 |  4236 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4237 | `	}` |
|   103338 |  4238 | `	break;` |
|        - |  4239 | `					  }` |
|        - |  4240 | `/*` |
|        - |  4241 | ` * LOAD_CLOSURE * * P3` |
|        - |  4242 | ` *` |
|        - |  4243 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4244 | ` * name in the stack.` |
|        - |  4245 | ` */` |
|       44 |  4246 | `case PH7_OP_LOAD_CLOSURE:{` |
|       89 |  4247 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       89 |  4248 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4249 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4250 | `		ph7_vm_func *pClosure;` |
|        - |  4251 | `		char *zName;` |
|        - |  4252 | `		sxu32 mLen;` |
|        - |  4253 | `		sxu32 n;` |
|        - |  4254 | `		/* Create a new VM function */` |
|       89 |  4255 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4256 | `		/* Generate an unique closure name */` |
|       89 |  4257 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       89 |  4258 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4259 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4260 | `			goto Abort;` |
|        - |  4261 | `		}` |
|       89 |  4262 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       89 |  4263 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4264 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4265 | `		}` |
|        - |  4266 | `		/* Zero the stucture */` |
|       89 |  4267 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4268 | `		/* Perform a structure assignment on read-only items */` |
|       89 |  4269 | `		pClosure->aArgs = pFunc->aArgs;` |
|       89 |  4270 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       89 |  4271 | `		pClosure->aStatic = pFunc->aStatic;` |
|       89 |  4272 | `		pClosure->iFlags = pFunc->iFlags;` |
|       89 |  4273 | `		pClosure->pUserData = pFunc->pUserData;` |
|       89 |  4274 | `		pClosure->sSignature = pFunc->sSignature;` |
|       89 |  4275 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       89 |  4276 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       89 |  4277 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       89 |  4278 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       89 |  4279 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4280 | `		/* Register the closure */` |
|       89 |  4281 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4282 | `		/* Set up closure environment */` |
|       89 |  4283 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       89 |  4284 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      241 |  4285 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4286 | `			ph7_value *pValue;` |
|      153 |  4287 | `			pEnv = &aEnv[n];` |
|      153 |  4288 | `			sEnv.sName  = pEnv->sName;` |
|      153 |  4289 | `			sEnv.iFlags = pEnv->iFlags;` |
|      153 |  4290 | `			sEnv.nIdx = SXU32_HIGH;` |
|      153 |  4291 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      153 |  4292 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4293 | `				/* Pass by reference */` |
|      ! 0 |  4294 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4295 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4296 | `					);` |
|      ! 0 |  4297 | `			}` |
|        - |  4298 | `			/* Standard pass by value */` |
|      153 |  4299 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      153 |  4300 | `			if( pValue ){` |
|        - |  4301 | `				/* Copy imported value */` |
|       69 |  4302 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       34 |  4303 | `			}` |
|        - |  4304 | `			/* Insert the imported variable */` |
|      153 |  4305 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       77 |  4306 | `		}` |
|        - |  4307 | `		/* Finally,load the closure name on the stack */` |
|       89 |  4308 | `		pTos++;` |
|       89 |  4309 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       44 |  4310 | `	}` |
|       89 |  4311 | `	break;` |
|        - |  4312 | `						 }` |
|        - |  4313 | `/*` |
|        - |  4314 | ` * STORE * P2 P3` |
|        - |  4315 | ` *` |
|        - |  4316 | ` * Perform a store (Assignment) operation.` |
|        - |  4317 | ` */` |
|   129127 |  4318 | `case PH7_OP_STORE: {` |
|        - |  4319 | `	ph7_value *pObj;` |
|        - |  4320 | `	SyString sName;` |
|        - |  4321 | `#ifdef UNTRUST` |
|        - |  4322 | `	if( pTos < pStack ){` |
|        - |  4323 | `		goto Abort;` |
|        - |  4324 | `	}` |
|        - |  4325 | `#endif` |
|   258256 |  4326 | `	if( pInstr->iP2 ){` |
|        - |  4327 | `		sxu32 nIdx;` |
|        - |  4328 | `		sxi32 rcT;` |
|        - |  4329 | `		/* Member store operation */` |
|     3676 |  4330 | `		nIdx = pTos->nIdx;` |
|     3676 |  4331 | `		VmPopOperand(&pTos,1);` |
|     3676 |  4332 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4333 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4334 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4335 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4336 | `		}else{` |
|        - |  4337 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4338 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     3672 |  4339 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     3672 |  4340 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4341 | `				goto Abort;` |
|        - |  4342 | `			}` |
|     3672 |  4343 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4344 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4345 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4346 | `				 * propagate out of the VM loop. */` |
|       35 |  4347 | `				VmPopOperand(&pTos,1);` |
|        - |  4348 | `				{` |
|       35 |  4349 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       35 |  4350 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       35 |  4351 | `						pc = pFrm2->iExceptionJump - 1;` |
|   129145 |  4352 | `						break;` |
|        - |  4353 | `					}` |
|        - |  4354 | `				}` |
|      ! 0 |  4355 | `				goto Exception;` |
|        - |  4356 | `			}` |
|        - |  4357 | `			/* Point to the desired memory object */` |
|     3638 |  4358 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3638 |  4359 | `			if( pObj ){` |
|        - |  4360 | `				/* Perform the store operation */` |
|     3638 |  4361 | `				PH7_MemObjStore(pTos,pObj);` |
|     1818 |  4362 | `			}` |
|        - |  4363 | `		}` |
|     3642 |  4364 | `		break;` |
|   254582 |  4365 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4366 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4367 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4368 | `			/* Force a string cast */` |
|      ! 0 |  4369 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4370 | `		}` |
|        7 |  4371 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4372 | `		pTos--;` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `		if( pTos < pStack  ){` |
|        - |  4375 | `			goto Abort;` |
|        - |  4376 | `		}` |
|        - |  4377 | `#endif` |
|        4 |  4378 | `	}else{` |
|   254576 |  4379 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4380 | `	}` |
|        - |  4381 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   254582 |  4382 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   254582 |  4383 | `	if( pObj == 0 ){` |
|      ! 0 |  4384 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4385 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4386 | `		goto Abort;` |
|        - |  4387 | `	}` |
|   254582 |  4388 | `	if( !pInstr->p3 ){` |
|        7 |  4389 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4390 | `	}` |
|        - |  4391 | `	/* Perform the store operation */` |
|   254582 |  4392 | `	PH7_MemObjStore(pTos,pObj);` |
|   254582 |  4393 | `	break;` |
|        - |  4394 | `				   }` |
|        - |  4395 | `/*` |
|        - |  4396 | ` * STORE_IDX:   P1 * P3` |
|        - |  4397 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4398 | ` *` |
|        - |  4399 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4400 | ` */` |
|    90062 |  4401 | `case PH7_OP_STORE_IDX:` |
|        - |  4402 | `case PH7_OP_STORE_IDX_REF: {` |
|   180126 |  4403 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4404 | `	ph7_value *pKey;` |
|        - |  4405 | `	sxu32 nIdx;` |
|   180126 |  4406 | `	if( pInstr->iP1 ){` |
|        - |  4407 | `		/* Key is next on stack */` |
|    60614 |  4408 | `		pKey = pTos;` |
|    60614 |  4409 | `		pTos--;` |
|    30308 |  4410 | `	}else{` |
|   119514 |  4411 | `		pKey = 0;` |
|        - |  4412 | `	}` |
|   180126 |  4413 | `	nIdx = pTos->nIdx;` |
|   180126 |  4414 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4415 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4416 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4417 | `		 * checking true sharing count, then re-add after separation. */` |
|   180074 |  4418 | `		if( nIdx != SXU32_HIGH ){` |
|   180074 |  4419 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   270110 |  4420 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   180074 |  4421 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4422 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4423 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4424 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4425 | `				 * refcounts if the backing array was already separated. */` |
|   180074 |  4426 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   180074 |  4427 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   180074 |  4428 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   180074 |  4429 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   180074 |  4430 | `					pTos->x.pOther = pMap;` |
|    90038 |  4431 | `				}else{` |
|        - |  4432 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4433 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4434 | `					pMap = pCur;` |
|        - |  4435 | `				}` |
|    90038 |  4436 | `			}else{` |
|      ! 0 |  4437 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4438 | `			}` |
|    90038 |  4439 | `		}else{` |
|      ! 0 |  4440 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4441 | `		}` |
|   180074 |  4442 | `		if( pMap->iRef < 2 ){` |
|        - |  4443 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4444 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4445 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4446 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4447 | `			pMap->iRef = 2;` |
|      ! 0 |  4448 | `		}` |
|    90038 |  4449 | `	}else{` |
|        - |  4450 | `		ph7_value *pObj;` |
|       53 |  4451 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4452 | `		if( pObj == 0 ){` |
|      ! 0 |  4453 | `			if( pKey ){` |
|      ! 0 |  4454 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4455 | `			}` |
|      ! 0 |  4456 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4457 | `			break;` |
|        - |  4458 | `		}` |
|        - |  4459 | `		/* Phase#1: Load the array */` |
|       53 |  4460 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4461 | `			VmPopOperand(&pTos,1);` |
|       53 |  4462 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4463 | `				/* Force a string cast */` |
|      ! 0 |  4464 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4465 | `			}` |
|       53 |  4466 | `			if( pKey == 0 ){` |
|        - |  4467 | `				/* Append string */` |
|        3 |  4468 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4469 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4470 | `				}` |
|        2 |  4471 | `			}else{` |
|        - |  4472 | `				sxu32 nOfft;` |
|       51 |  4473 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4474 | `					/* Force an int cast */` |
|       51 |  4475 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4476 | `				}` |
|       51 |  4477 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4478 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4479 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4480 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4481 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4482 | `				}else{` |
|      ! 0 |  4483 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4484 | `						/* Perform an append operation */` |
|      ! 0 |  4485 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4486 | `					}` |
|        - |  4487 | `				}` |
|        - |  4488 | `			}` |
|       53 |  4489 | `			if( pKey ){` |
|       51 |  4490 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4491 | `			}` |
|       53 |  4492 | `			break;` |
|      ! 0 |  4493 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4494 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4495 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4496 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4497 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4498 | `				goto Abort;` |
|        - |  4499 | `			}` |
|      ! 0 |  4500 | `		}` |
|        - |  4501 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4502 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4503 | `	}` |
|   180074 |  4504 | `	VmPopOperand(&pTos,1);` |
|        - |  4505 | `	/* Phase#2: Perform the insertion */` |
|   180074 |  4506 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4507 | `		/* Insertion by reference */` |
|       15 |  4508 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4509 | `	}else{` |
|   180060 |  4510 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4511 | `	}` |
|   180074 |  4512 | `	if( pKey ){` |
|    60564 |  4513 | `		PH7_MemObjRelease(pKey);` |
|    30281 |  4514 | `	}` |
|   180074 |  4515 | `	break;` |
|        - |  4516 | `					   }` |
|        - |  4517 | `/*` |
|        - |  4518 | ` * INCR: P1 * *` |
|        - |  4519 | ` *` |
|        - |  4520 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4521 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4522 | ` * the stack and increment after that.` |
|        - |  4523 | ` */` |
|   160365 |  4524 | `case PH7_OP_INCR:` |
|        - |  4525 | `#ifdef UNTRUST` |
|        - |  4526 | `	if( pTos < pStack ){` |
|        - |  4527 | `		goto Abort;` |
|        - |  4528 | `	}` |
|        - |  4529 | `#endif` |
|   320776 |  4530 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   320776 |  4531 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4532 | `			ph7_value *pObj;` |
|   320776 |  4533 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4534 | `				/* Force a numeric cast */` |
|   320776 |  4535 | `				PH7_MemObjToNumeric(pObj);` |
|   320776 |  4536 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4537 | `					pObj->rVal++;` |
|        - |  4538 | `					/* Try to get an integer representation */` |
|      ! 0 |  4539 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4540 | `				}else{` |
|   320776 |  4541 | `					pObj->x.iVal++;` |
|   320776 |  4542 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4543 | `				}` |
|   320776 |  4544 | `				if( pInstr->iP1 ){` |
|        - |  4545 | `					/* Pre-icrement */` |
|       77 |  4546 | `					PH7_MemObjStore(pObj,pTos);` |
|       38 |  4547 | `				}` |
|   160409 |  4548 | `			}` |
|   160411 |  4549 | `		}else{` |
|      ! 0 |  4550 | `			if( pInstr->iP1 ){` |
|        - |  4551 | `				/* Force a numeric cast */` |
|      ! 0 |  4552 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  4553 | `				/* Pre-increment */` |
|      ! 0 |  4554 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4555 | `					pTos->rVal++;` |
|        - |  4556 | `					/* Try to get an integer representation */` |
|      ! 0 |  4557 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4558 | `				}else{` |
|      ! 0 |  4559 | `					pTos->x.iVal++;` |
|      ! 0 |  4560 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4561 | `				}` |
|      ! 0 |  4562 | `			}` |
|        - |  4563 | `		}` |
|   160409 |  4564 | `	}` |
|   320776 |  4565 | `	break;` |
|        - |  4566 | `/*` |
|        - |  4567 | ` * DECR: P1 * *` |
|        - |  4568 | ` *` |
|        - |  4569 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  4570 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  4571 | ` * and decrement after that.` |
|        - |  4572 | ` */` |
|        2 |  4573 | `case PH7_OP_DECR:` |
|        - |  4574 | `#ifdef UNTRUST` |
|        - |  4575 | `	if( pTos < pStack ){` |
|        - |  4576 | `		goto Abort;` |
|        - |  4577 | `	}` |
|        - |  4578 | `#endif` |
|        5 |  4579 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  4580 | `		/* Force a numeric cast */` |
|        5 |  4581 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  4582 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4583 | `			ph7_value *pObj;` |
|        5 |  4584 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  4585 | `				/* Force a numeric cast */` |
|        5 |  4586 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  4587 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4588 | `					pObj->rVal--;` |
|        - |  4589 | `					/* Try to get an integer representation */` |
|      ! 0 |  4590 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4591 | `				}else{` |
|        5 |  4592 | `					pObj->x.iVal--;` |
|        5 |  4593 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4594 | `				}` |
|        5 |  4595 | `				if( pInstr->iP1 ){` |
|        - |  4596 | `					/* Pre-icrement */` |
|      ! 0 |  4597 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  4598 | `				}` |
|        2 |  4599 | `			}` |
|        3 |  4600 | `		}else{` |
|      ! 0 |  4601 | `			if( pInstr->iP1 ){` |
|        - |  4602 | `				/* Pre-increment */` |
|      ! 0 |  4603 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4604 | `					pTos->rVal--;` |
|        - |  4605 | `					/* Try to get an integer representation */` |
|      ! 0 |  4606 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  4607 | `				}else{` |
|      ! 0 |  4608 | `					pTos->x.iVal--;` |
|      ! 0 |  4609 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  4610 | `				}` |
|      ! 0 |  4611 | `			}` |
|        - |  4612 | `		}` |
|        2 |  4613 | `	}` |
|        5 |  4614 | `	break;` |
|        - |  4615 | `/*` |
|        - |  4616 | ` * UMINUS: * * *` |
|        - |  4617 | ` *` |
|        - |  4618 | ` * Perform a unary minus operation.` |
|        - |  4619 | ` */` |
|    26923 |  4620 | `case PH7_OP_UMINUS:` |
|        - |  4621 | `#ifdef UNTRUST` |
|        - |  4622 | `	if( pTos < pStack ){` |
|        - |  4623 | `		goto Abort;` |
|        - |  4624 | `	}` |
|        - |  4625 | `#endif` |
|        - |  4626 | `	/* Force a numeric (integer,real or both) cast */` |
|    53848 |  4627 | `	PH7_MemObjToNumeric(pTos);` |
|    53848 |  4628 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  4629 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  4630 | `	}` |
|    53848 |  4631 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    53818 |  4632 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    26908 |  4633 | `	}` |
|    53848 |  4634 | `	break;` |
|        - |  4635 | `/*` |
|        - |  4636 | ` * UPLUS: * * *` |
|        - |  4637 | ` *` |
|        - |  4638 | ` * Perform a unary plus operation.` |
|        - |  4639 | ` */` |
|       18 |  4640 | `case PH7_OP_UPLUS:` |
|        - |  4641 | `#ifdef UNTRUST` |
|        - |  4642 | `	if( pTos < pStack ){` |
|        - |  4643 | `		goto Abort;` |
|        - |  4644 | `	}` |
|        - |  4645 | `#endif` |
|        - |  4646 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  4647 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  4648 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4649 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  4650 | `	}` |
|       37 |  4651 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  4652 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  4653 | `	}` |
|       37 |  4654 | `	break;` |
|        - |  4655 | `/*` |
|        - |  4656 | ` * OP_LNOT: * * *` |
|        - |  4657 | ` *` |
|        - |  4658 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  4659 | ` * with its complement.` |
|        - |  4660 | ` */` |
|    42433 |  4661 | `case PH7_OP_LNOT:` |
|        - |  4662 | `#ifdef UNTRUST` |
|        - |  4663 | `	if( pTos < pStack ){` |
|        - |  4664 | `		goto Abort;` |
|        - |  4665 | `	}` |
|        - |  4666 | `#endif` |
|        - |  4667 | `	/* Force a boolean cast */` |
|    84912 |  4668 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  4669 | `		PH7_MemObjToBool(pTos);` |
|       10 |  4670 | `	}` |
|    84912 |  4671 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    84912 |  4672 | `	break;` |
|        - |  4673 | `/*` |
|        - |  4674 | ` * OP_BITNOT: * * *` |
|        - |  4675 | ` *` |
|        - |  4676 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  4677 | ` * with its ones-complement.` |
|        - |  4678 | ` */` |
|       13 |  4679 | `case PH7_OP_BITNOT:` |
|        - |  4680 | `#ifdef UNTRUST` |
|        - |  4681 | `	if( pTos < pStack ){` |
|        - |  4682 | `		goto Abort;` |
|        - |  4683 | `	}` |
|        - |  4684 | `#endif` |
|        - |  4685 | `	/* Force an integer cast */` |
|       28 |  4686 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4687 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4688 | `	}` |
|       28 |  4689 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       28 |  4690 | `	break;` |
|        - |  4691 | `/* OP_MUL * * *` |
|        - |  4692 | ` * OP_MUL_STORE * * *` |
|        - |  4693 | ` *` |
|        - |  4694 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  4695 | ` * and push the result back onto the stack.` |
|        - |  4696 | ` */` |
|     1278 |  4697 | `case PH7_OP_MUL:` |
|        - |  4698 | `case PH7_OP_MUL_STORE: {` |
|     2558 |  4699 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4700 | `	/* Force the operand to be numeric */` |
|        - |  4701 | `#ifdef UNTRUST` |
|        - |  4702 | `	if( pNos < pStack ){` |
|        - |  4703 | `		goto Abort;` |
|        - |  4704 | `	}` |
|        - |  4705 | `#endif` |
|     2558 |  4706 | `	PH7_MemObjToNumeric(pTos);` |
|     2558 |  4707 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  4708 | `	/* Perform the requested operation */` |
|     2558 |  4709 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4710 | `		/* Floating point arithemic */` |
|        - |  4711 | `		ph7_real a,b,r;` |
|       19 |  4712 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  4713 | `			PH7_MemObjToReal(pTos);` |
|        4 |  4714 | `		}` |
|       19 |  4715 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4716 | `			PH7_MemObjToReal(pNos);` |
|        3 |  4717 | `		}` |
|       19 |  4718 | `		a = pNos->rVal;` |
|       19 |  4719 | `		b = pTos->rVal;` |
|       19 |  4720 | `		r = a * b;` |
|        - |  4721 | `		/* Push the result */` |
|       19 |  4722 | `		pNos->rVal = r;` |
|       19 |  4723 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4724 | `		/* Try to get an integer representation */` |
|       19 |  4725 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  4726 | `	}else{` |
|        - |  4727 | `		/* Integer arithmetic */` |
|        - |  4728 | `		sxi64 a,b,r;` |
|     2540 |  4729 | `		a = pNos->x.iVal;` |
|     2540 |  4730 | `		b = pTos->x.iVal;` |
|     2540 |  4731 | `		r = a * b;` |
|        - |  4732 | `		/* Push the result */` |
|     2540 |  4733 | `		pNos->x.iVal = r;` |
|     2540 |  4734 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4735 | `	}` |
|     2558 |  4736 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  4737 | `		ph7_value *pObj;` |
|       32 |  4738 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4739 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  4740 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  4741 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  4742 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  4743 | `		}` |
|       15 |  4744 | `	}` |
|     2558 |  4745 | `	VmPopOperand(&pTos,1);` |
|     2558 |  4746 | `	break;` |
|        - |  4747 | `				 }` |
|        - |  4748 | `/* OP_ADD * * *` |
|        - |  4749 | ` *` |
|        - |  4750 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4751 | ` * and push the result back onto the stack.` |
|        - |  4752 | ` */` |
|      491 |  4753 | `case PH7_OP_ADD:{` |
|      984 |  4754 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4755 | `#ifdef UNTRUST` |
|        - |  4756 | `	if( pNos < pStack ){` |
|        - |  4757 | `		goto Abort;` |
|        - |  4758 | `	}` |
|        - |  4759 | `#endif` |
|        - |  4760 | `	/* Perform the addition */` |
|      984 |  4761 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      984 |  4762 | `	VmPopOperand(&pTos,1);` |
|      984 |  4763 | `	break;` |
|        - |  4764 | `				}` |
|        - |  4765 | `/*` |
|        - |  4766 | ` * OP_ADD_STORE * * *` |
|        - |  4767 | ` *` |
|        - |  4768 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  4769 | ` * and push the result back onto the stack.` |
|        - |  4770 | ` */` |
|      502 |  4771 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  4772 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4773 | `	ph7_value *pObj;` |
|        - |  4774 | `	sxu32 nIdx;` |
|        - |  4775 | `#ifdef UNTRUST` |
|        - |  4776 | `	if( pNos < pStack ){` |
|        - |  4777 | `		goto Abort;` |
|        - |  4778 | `	}` |
|        - |  4779 | `#endif` |
|        - |  4780 | `	/* Perform the addition */` |
|     1006 |  4781 | `	nIdx = pTos->nIdx;` |
|     1006 |  4782 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  4783 | `	/* Peform the store operation */` |
|     1006 |  4784 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  4785 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  4786 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  4787 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  4788 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  4789 | `	}` |
|        - |  4790 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  4791 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  4792 | `	VmPopOperand(&pTos,1);` |
|     1006 |  4793 | `	break;` |
|        - |  4794 | `				}` |
|        - |  4795 | `/* OP_SUB * * *` |
|        - |  4796 | ` *` |
|        - |  4797 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4798 | ` * first (what was next on the stack) from the second (the` |
|        - |  4799 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4800 | ` */` |
|      302 |  4801 | `case PH7_OP_SUB: {` |
|      606 |  4802 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4803 | `#ifdef UNTRUST` |
|        - |  4804 | `	if( pNos < pStack ){` |
|        - |  4805 | `		goto Abort;` |
|        - |  4806 | `	}` |
|        - |  4807 | `#endif` |
|      606 |  4808 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4809 | `		/* Floating point arithemic */` |
|        - |  4810 | `		ph7_real a,b,r;` |
|       95 |  4811 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4812 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4813 | `		}` |
|       95 |  4814 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  4815 | `			PH7_MemObjToReal(pNos);` |
|        2 |  4816 | `		}` |
|       95 |  4817 | `		a = pNos->rVal;` |
|       95 |  4818 | `		b = pTos->rVal;` |
|       95 |  4819 | `		r = a - b;` |
|        - |  4820 | `		/* Push the result */` |
|       95 |  4821 | `		pNos->rVal = r;` |
|       95 |  4822 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4823 | `		/* Try to get an integer representation */` |
|       95 |  4824 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  4825 | `	}else{` |
|        - |  4826 | `		/* Integer arithmetic */` |
|        - |  4827 | `		sxi64 a,b,r;` |
|      512 |  4828 | `		a = pNos->x.iVal;` |
|      512 |  4829 | `		b = pTos->x.iVal;` |
|      512 |  4830 | `		r = a - b;` |
|        - |  4831 | `		/* Push the result */` |
|      512 |  4832 | `		pNos->x.iVal = r;` |
|      512 |  4833 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4834 | `	}` |
|      606 |  4835 | `	VmPopOperand(&pTos,1);` |
|      606 |  4836 | `	break;` |
|        - |  4837 | `				 }` |
|        - |  4838 | `/* OP_SUB_STORE * * *` |
|        - |  4839 | ` *` |
|        - |  4840 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  4841 | ` * first (what was next on the stack) from the second (the` |
|        - |  4842 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  4843 | ` */` |
|        4 |  4844 | `case PH7_OP_SUB_STORE: {` |
|       10 |  4845 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4846 | `	ph7_value *pObj;` |
|        - |  4847 | `#ifdef UNTRUST` |
|        - |  4848 | `	if( pNos < pStack ){` |
|        - |  4849 | `		goto Abort;` |
|        - |  4850 | `	}` |
|        - |  4851 | `#endif` |
|       10 |  4852 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  4853 | `		/* Floating point arithemic */` |
|        - |  4854 | `		ph7_real a,b,r;` |
|      ! 0 |  4855 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4856 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  4857 | `		}` |
|      ! 0 |  4858 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  4859 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  4860 | `		}` |
|      ! 0 |  4861 | `		a = pTos->rVal;` |
|      ! 0 |  4862 | `		b = pNos->rVal;` |
|      ! 0 |  4863 | `		r = a - b;` |
|        - |  4864 | `		/* Push the result */` |
|      ! 0 |  4865 | `		pNos->rVal = r;` |
|      ! 0 |  4866 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4867 | `		/* Try to get an integer representation */` |
|      ! 0 |  4868 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  4869 | `	}else{` |
|        - |  4870 | `		/* Integer arithmetic */` |
|        - |  4871 | `		sxi64 a,b,r;` |
|       10 |  4872 | `		a = pTos->x.iVal;` |
|       10 |  4873 | `		b = pNos->x.iVal;` |
|       10 |  4874 | `		r = a - b;` |
|        - |  4875 | `		/* Push the result */` |
|       10 |  4876 | `		pNos->x.iVal = r;` |
|       10 |  4877 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  4878 | `	}` |
|       10 |  4879 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4880 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  4881 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  4882 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  4883 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  4884 | `	}` |
|       10 |  4885 | `	VmPopOperand(&pTos,1);` |
|       10 |  4886 | `	break;` |
|        - |  4887 | `				 }` |
|        - |  4888 |  |
|        - |  4889 | `/*` |
|        - |  4890 | ` * OP_MOD * * *` |
|        - |  4891 | ` *` |
|        - |  4892 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4893 | ` * first (what was next on the stack) from the second (the` |
|        - |  4894 | ` * top of the stack) and push the remainder after division` |
|        - |  4895 | ` * onto the stack.` |
|        - |  4896 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4897 | ` */` |
|      307 |  4898 | `case PH7_OP_MOD:{` |
|      616 |  4899 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4900 | `	sxi64 a,b,r;` |
|        - |  4901 | `#ifdef UNTRUST` |
|        - |  4902 | `	if( pNos < pStack ){` |
|        - |  4903 | `		goto Abort;` |
|        - |  4904 | `	}` |
|        - |  4905 | `#endif` |
|        - |  4906 | `	/* Force the operands to be integer */` |
|      616 |  4907 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4908 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4909 | `	}` |
|      616 |  4910 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  4911 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  4912 | `	}` |
|        - |  4913 | `	/* Perform the requested operation */` |
|      616 |  4914 | `	a = pNos->x.iVal;` |
|      616 |  4915 | `	b = pTos->x.iVal;` |
|      616 |  4916 | `	if( b == 0 ){` |
|        3 |  4917 | `		r = 0;` |
|        3 |  4918 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4919 | `		/* goto Abort; */` |
|        2 |  4920 | `	}else{` |
|      613 |  4921 | `		r = a%b;` |
|        - |  4922 | `	}` |
|        - |  4923 | `	/* Push the result */` |
|      616 |  4924 | `	pNos->x.iVal = r;` |
|      616 |  4925 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      616 |  4926 | `	VmPopOperand(&pTos,1);` |
|      616 |  4927 | `	break;` |
|        - |  4928 | `				}` |
|        - |  4929 | `/*` |
|        - |  4930 | ` * OP_MOD_STORE * * *` |
|        - |  4931 | ` *` |
|        - |  4932 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4933 | ` * first (what was next on the stack) from the second (the` |
|        - |  4934 | ` * top of the stack) and push the remainder after division` |
|        - |  4935 | ` * onto the stack.` |
|        - |  4936 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  4937 | ` */` |
|        1 |  4938 | `case PH7_OP_MOD_STORE: {` |
|        3 |  4939 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4940 | `	ph7_value *pObj;` |
|        - |  4941 | `	sxi64 a,b,r;` |
|        - |  4942 | `#ifdef UNTRUST` |
|        - |  4943 | `	if( pNos < pStack ){` |
|        - |  4944 | `		goto Abort;` |
|        - |  4945 | `	}` |
|        - |  4946 | `#endif` |
|        - |  4947 | `	/* Force the operands to be integer */` |
|        3 |  4948 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4949 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4950 | `	}` |
|        3 |  4951 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4952 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4953 | `	}` |
|        - |  4954 | `	/* Perform the requested operation */` |
|        3 |  4955 | `	a = pTos->x.iVal;` |
|        3 |  4956 | `	b = pNos->x.iVal;` |
|        3 |  4957 | `	if( b == 0 ){` |
|      ! 0 |  4958 | `		r = 0;` |
|      ! 0 |  4959 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  4960 | `		/* goto Abort; */` |
|      ! 0 |  4961 | `	}else{` |
|        3 |  4962 | `		r = a%b;` |
|        - |  4963 | `	}` |
|        - |  4964 | `	/* Push the result */` |
|        3 |  4965 | `	pNos->x.iVal = r;` |
|        3 |  4966 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  4967 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4968 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4969 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4970 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  4971 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4972 | `	}` |
|        3 |  4973 | `	VmPopOperand(&pTos,1);` |
|        3 |  4974 | `	break;` |
|        - |  4975 | `				}` |
|        - |  4976 | `/*` |
|        - |  4977 | ` * OP_DIV * * *` |
|        - |  4978 | ` *` |
|        - |  4979 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4980 | ` * first (what was next on the stack) from the second (the` |
|        - |  4981 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4982 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4983 | ` */` |
|       30 |  4984 | `case PH7_OP_DIV:{` |
|       62 |  4985 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4986 | `	ph7_real a,b,r;` |
|        - |  4987 | `#ifdef UNTRUST` |
|        - |  4988 | `	if( pNos < pStack ){` |
|        - |  4989 | `		goto Abort;` |
|        - |  4990 | `	}` |
|        - |  4991 | `#endif` |
|        - |  4992 | `	/* Force the operands to be real */` |
|       62 |  4993 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       58 |  4994 | `		PH7_MemObjToReal(pTos);` |
|       28 |  4995 | `	}` |
|       62 |  4996 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       24 |  4997 | `		PH7_MemObjToReal(pNos);` |
|       11 |  4998 | `	}` |
|        - |  4999 | `	/* Perform the requested operation */` |
|       62 |  5000 | `	a = pNos->rVal;` |
|       62 |  5001 | `	b = pTos->rVal;` |
|       62 |  5002 | `	if( b == 0 ){` |
|        - |  5003 | `		/* Division by zero */` |
|        3 |  5004 | `		pNos->rVal = 0;` |
|        3 |  5005 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  5006 | `		/* goto Abort; */` |
|        2 |  5007 | `	}else{` |
|       59 |  5008 | `		r = a/b;` |
|        - |  5009 | `		/* Push the result */` |
|       59 |  5010 | `		pNos->rVal = r;` |
|       59 |  5011 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5012 | `		/* Try to get an integer representation */` |
|       59 |  5013 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5014 | `	}` |
|       62 |  5015 | `	VmPopOperand(&pTos,1);` |
|       62 |  5016 | `	break;` |
|        - |  5017 | `				}` |
|        - |  5018 | `/*` |
|        - |  5019 | ` * OP_DIV_STORE * * *` |
|        - |  5020 | ` *` |
|        - |  5021 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5022 | ` * first (what was next on the stack) from the second (the` |
|        - |  5023 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5024 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5025 | ` */` |
|        2 |  5026 | `case PH7_OP_DIV_STORE:{` |
|        5 |  5027 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5028 | `	ph7_value *pObj;` |
|        - |  5029 | `	ph7_real a,b,r;` |
|        - |  5030 | `#ifdef UNTRUST` |
|        - |  5031 | `	if( pNos < pStack ){` |
|        - |  5032 | `		goto Abort;` |
|        - |  5033 | `	}` |
|        - |  5034 | `#endif` |
|        - |  5035 | `	/* Force the operands to be real */` |
|        5 |  5036 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5037 | `		PH7_MemObjToReal(pTos);` |
|        2 |  5038 | `	}` |
|        5 |  5039 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5040 | `		PH7_MemObjToReal(pNos);` |
|        2 |  5041 | `	}` |
|        - |  5042 | `	/* Perform the requested operation */` |
|        5 |  5043 | `	a = pTos->rVal;` |
|        5 |  5044 | `	b = pNos->rVal;` |
|        5 |  5045 | `	if( b == 0 ){` |
|        - |  5046 | `		/* Division by zero */` |
|      ! 0 |  5047 | `		r = 0;` |
|      ! 0 |  5048 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  5049 | `		/* goto Abort; */` |
|      ! 0 |  5050 | `	}else{` |
|        5 |  5051 | `		r = a/b;` |
|        - |  5052 | `		/* Push the result */` |
|        5 |  5053 | `		pNos->rVal = r;` |
|        5 |  5054 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5055 | `		/* Try to get an integer representation */` |
|        5 |  5056 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5057 | `	}` |
|        5 |  5058 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5059 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  5060 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  5061 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  5062 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  5063 | `	}` |
|        5 |  5064 | `	VmPopOperand(&pTos,1);` |
|        5 |  5065 | `	break;` |
|        - |  5066 | `				}` |
|        - |  5067 | `/* OP_BAND * * *` |
|        - |  5068 | ` *` |
|        - |  5069 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5070 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5071 | ` * two elements.` |
|        - |  5072 | `*/` |
|        - |  5073 | `/* OP_BOR * * *` |
|        - |  5074 | ` *` |
|        - |  5075 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5076 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5077 | ` * two elements.` |
|        - |  5078 | ` */` |
|        - |  5079 | `/* OP_BXOR * * *` |
|        - |  5080 | ` *` |
|        - |  5081 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5082 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5083 | ` * two elements.` |
|        - |  5084 | ` */` |
|       44 |  5085 | `case PH7_OP_BAND:` |
|        - |  5086 | `case PH7_OP_BOR:` |
|        - |  5087 | `case PH7_OP_BXOR:{` |
|       90 |  5088 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5089 | `	sxi64 a,b,r;` |
|        - |  5090 | `#ifdef UNTRUST` |
|        - |  5091 | `	if( pNos < pStack ){` |
|        - |  5092 | `		goto Abort;` |
|        - |  5093 | `	}` |
|        - |  5094 | `#endif` |
|        - |  5095 | `	/* Force the operands to be integer */` |
|       90 |  5096 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5097 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5098 | `	}` |
|       90 |  5099 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5100 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5101 | `	}` |
|        - |  5102 | `	/* Perform the requested operation */` |
|       90 |  5103 | `	a = pNos->x.iVal;` |
|       90 |  5104 | `	b = pTos->x.iVal;` |
|       90 |  5105 | `	switch(pInstr->iOp){` |
|        7 |  5106 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5107 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5108 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5109 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5110 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5111 | `	case PH7_OP_BAND:` |
|       62 |  5112 | `	default:          r = a&b; break;` |
|        - |  5113 | `	}` |
|        - |  5114 | `	/* Push the result */` |
|       90 |  5115 | `	pNos->x.iVal = r;` |
|       90 |  5116 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5117 | `	VmPopOperand(&pTos,1);` |
|       90 |  5118 | `	break;` |
|        - |  5119 | `				 }` |
|        - |  5120 | `/* OP_BAND_STORE * * *` |
|        - |  5121 | ` *` |
|        - |  5122 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5123 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5124 | ` * two elements.` |
|        - |  5125 | `*/` |
|        - |  5126 | `/* OP_BOR_STORE * * *` |
|        - |  5127 | ` *` |
|        - |  5128 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5129 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5130 | ` * two elements.` |
|        - |  5131 | ` */` |
|        - |  5132 | `/* OP_BXOR_STORE * * *` |
|        - |  5133 | ` *` |
|        - |  5134 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5135 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5136 | ` * two elements.` |
|        - |  5137 | ` */` |
|       10 |  5138 | `case PH7_OP_BAND_STORE:` |
|        - |  5139 | `case PH7_OP_BOR_STORE:` |
|        - |  5140 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5141 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5142 | `	ph7_value *pObj;` |
|        - |  5143 | `	sxi64 a,b,r;` |
|        - |  5144 | `#ifdef UNTRUST` |
|        - |  5145 | `	if( pNos < pStack ){` |
|        - |  5146 | `		goto Abort;` |
|        - |  5147 | `	}` |
|        - |  5148 | `#endif` |
|        - |  5149 | `	/* Force the operands to be integer */` |
|       21 |  5150 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5151 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5152 | `	}` |
|       21 |  5153 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5154 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5155 | `	}` |
|        - |  5156 | `	/* Perform the requested operation */` |
|       21 |  5157 | `	a = pTos->x.iVal;` |
|       21 |  5158 | `	b = pNos->x.iVal;` |
|       21 |  5159 | `	switch(pInstr->iOp){` |
|        3 |  5160 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5161 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5162 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5163 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5164 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5165 | `	case PH7_OP_BAND:` |
|        7 |  5166 | `	default:          r = a&b; break;` |
|        - |  5167 | `	}` |
|        - |  5168 | `	/* Push the result */` |
|       21 |  5169 | `	pNos->x.iVal = r;` |
|       21 |  5170 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5171 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5172 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5173 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5174 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5175 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5176 | `	}` |
|       21 |  5177 | `	VmPopOperand(&pTos,1);` |
|       21 |  5178 | `	break;` |
|        - |  5179 | `				 }` |
|        - |  5180 | `/* OP_SHL * * *` |
|        - |  5181 | ` *` |
|        - |  5182 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5183 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5184 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5185 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5186 | ` */` |
|        - |  5187 | `/* OP_SHR * * *` |
|        - |  5188 | ` *` |
|        - |  5189 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5190 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5191 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5192 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5193 | ` */` |
|       12 |  5194 | `case PH7_OP_SHL:` |
|        - |  5195 | `case PH7_OP_SHR: {` |
|       25 |  5196 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5197 | `	sxi64 a,r;` |
|        - |  5198 | `	sxi32 b;` |
|        - |  5199 | `#ifdef UNTRUST` |
|        - |  5200 | `	if( pNos < pStack ){` |
|        - |  5201 | `		goto Abort;` |
|        - |  5202 | `	}` |
|        - |  5203 | `#endif` |
|        - |  5204 | `	/* Force the operands to be integer */` |
|       25 |  5205 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5206 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5207 | `	}` |
|       25 |  5208 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5209 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5210 | `	}` |
|        - |  5211 | `	/* Perform the requested operation */` |
|       25 |  5212 | `	a = pNos->x.iVal;` |
|       25 |  5213 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5214 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5215 | `		r = a << b;` |
|        8 |  5216 | `	}else{` |
|       11 |  5217 | `		r = a >> b;` |
|        - |  5218 | `	}` |
|        - |  5219 | `	/* Push the result */` |
|       25 |  5220 | `	pNos->x.iVal = r;` |
|       25 |  5221 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5222 | `	VmPopOperand(&pTos,1);` |
|       25 |  5223 | `	break;` |
|        - |  5224 | `				 }` |
|        - |  5225 | `/*  OP_SHL_STORE * * *` |
|        - |  5226 | ` *` |
|        - |  5227 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5228 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5229 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5230 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5231 | ` */` |
|        - |  5232 | `/* OP_SHR_STORE * * *` |
|        - |  5233 | ` *` |
|        - |  5234 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5235 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5236 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5237 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5238 | ` */` |
|        9 |  5239 | `case PH7_OP_SHL_STORE:` |
|        - |  5240 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5241 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5242 | `	ph7_value *pObj;` |
|        - |  5243 | `	sxi64 a,r;` |
|        - |  5244 | `	sxi32 b;` |
|        - |  5245 | `#ifdef UNTRUST` |
|        - |  5246 | `	if( pNos < pStack ){` |
|        - |  5247 | `		goto Abort;` |
|        - |  5248 | `	}` |
|        - |  5249 | `#endif` |
|        - |  5250 | `	/* Force the operands to be integer */` |
|       19 |  5251 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5252 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5253 | `	}` |
|       19 |  5254 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5255 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5256 | `	}` |
|        - |  5257 | `	/* Perform the requested operation */` |
|       19 |  5258 | `	a = pTos->x.iVal;` |
|       19 |  5259 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5260 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5261 | `		r = a << b;` |
|        5 |  5262 | `	}else{` |
|       11 |  5263 | `		r = a >> b;` |
|        - |  5264 | `	}` |
|        - |  5265 | `	/* Push the result */` |
|       19 |  5266 | `	pNos->x.iVal = r;` |
|       19 |  5267 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5268 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5269 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5270 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5271 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5272 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5273 | `	}` |
|       19 |  5274 | `	VmPopOperand(&pTos,1);` |
|       19 |  5275 | `	break;` |
|        - |  5276 | `				 }` |
|        - |  5277 | `/* CAT:  P1 * *` |
|        - |  5278 | ` *` |
|        - |  5279 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5280 | ` * back.` |
|        - |  5281 | ` */` |
|    67633 |  5282 | `case PH7_OP_CAT:{` |
|        - |  5283 | `	ph7_value *pNos,*pCur;` |
|   135268 |  5284 | `	if( pInstr->iP1 < 1 ){` |
|   108002 |  5285 | `		pNos = &pTos[-1];` |
|    54002 |  5286 | `	}else{` |
|    27268 |  5287 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5288 | `	}` |
|        - |  5289 | `#ifdef UNTRUST` |
|        - |  5290 | `	if( pNos < pStack ){` |
|        - |  5291 | `		goto Abort;` |
|        - |  5292 | `	}` |
|        - |  5293 | `#endif` |
|        - |  5294 | `	/* Force a string cast */` |
|   135268 |  5295 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1636 |  5296 | `		PH7_MemObjToString(pNos);` |
|      817 |  5297 | `	}` |
|   135268 |  5298 | `	pCur = &pNos[1];` |
|   273072 |  5299 | `	while( pCur <= pTos ){` |
|   137806 |  5300 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50870 |  5301 | `			PH7_MemObjToString(pCur);` |
|    25434 |  5302 | `		}` |
|        - |  5303 | `		/* Perform the concatenation */` |
|   137806 |  5304 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   137764 |  5305 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    68881 |  5306 | `		}` |
|   137806 |  5307 | `		SyBlobRelease(&pCur->sBlob);` |
|   137806 |  5308 | `		pCur++;` |
|        2 |  5309 | `	}` |
|   135268 |  5310 | `	pTos = pNos;` |
|   135268 |  5311 | `	break;` |
|        - |  5312 | `				}` |
|        - |  5313 | `/*  CAT_STORE: * * *` |
|        - |  5314 | ` *` |
|        - |  5315 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5316 | ` * back.` |
|        - |  5317 | ` */` |
|     3711 |  5318 | `case PH7_OP_CAT_STORE:{` |
|     7424 |  5319 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5320 | `	ph7_value *pObj;` |
|        - |  5321 | `#ifdef UNTRUST` |
|        - |  5322 | `	if( pNos < pStack ){` |
|        - |  5323 | `		goto Abort;` |
|        - |  5324 | `	}` |
|        - |  5325 | `#endif` |
|     7424 |  5326 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5327 | `		/* Force a string cast */` |
|        3 |  5328 | `		PH7_MemObjToString(pTos);` |
|        1 |  5329 | `	}` |
|     7424 |  5330 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5331 | `		/* Force a string cast */` |
|      ! 0 |  5332 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5333 | `	}` |
|        - |  5334 | `	/* Perform the concatenation (Reverse order) */` |
|     7424 |  5335 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7424 |  5336 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3711 |  5337 | `	}` |
|        - |  5338 | `	/* Perform the store operation */` |
|     7424 |  5339 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5340 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7424 |  5341 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7424 |  5342 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7422 |  5343 | `		PH7_MemObjStore(pTos,pObj);` |
|     3710 |  5344 | `	}` |
|     7422 |  5345 | `	PH7_MemObjStore(pTos,pNos);` |
|     7422 |  5346 | `	VmPopOperand(&pTos,1);` |
|     7422 |  5347 | `	break;` |
|        - |  5348 | `				}` |
|        - |  5349 | `/* OP_AND: * * *` |
|        - |  5350 | ` *` |
|        - |  5351 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5352 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5353 | ` * stack.` |
|        - |  5354 | ` */` |
|        - |  5355 | `/* OP_OR: * * *` |
|        - |  5356 | ` *` |
|        - |  5357 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5358 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5359 | ` * stack.` |
|        - |  5360 | ` */` |
|   101961 |  5361 | `case PH7_OP_LAND:` |
|        - |  5362 | `case PH7_OP_LOR: {` |
|   203968 |  5363 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5364 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5365 | `#ifdef UNTRUST` |
|        - |  5366 | `	if( pNos < pStack ){` |
|        - |  5367 | `		goto Abort;` |
|        - |  5368 | `	}` |
|        - |  5369 | `#endif` |
|        - |  5370 | `	/* Force a boolean cast */` |
|   203968 |  5371 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5372 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5373 | `	}` |
|   203968 |  5374 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5375 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5376 | `	}` |
|   203968 |  5377 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   203968 |  5378 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   203968 |  5379 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5380 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    93662 |  5381 | `		v1 = and_logic[v1*3+v2];` |
|    46854 |  5382 | `	}else{` |
|        - |  5383 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   110308 |  5384 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5385 | `	}` |
|   203968 |  5386 | `	if( v1 == 2 ){` |
|      ! 0 |  5387 | `		v1 = 1;` |
|      ! 0 |  5388 | `	}` |
|   203968 |  5389 | `	VmPopOperand(&pTos,1);` |
|   203968 |  5390 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   203968 |  5391 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   203968 |  5392 | `	break;` |
|        - |  5393 | `				 }` |
|        - |  5394 | `/*` |
|        - |  5395 | ` * OP_NULLC: * * *` |
|        - |  5396 | ` * Null coalescing operator '??'.` |
|        - |  5397 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5398 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5399 | ` */` |
|        - |  5400 | `/*` |
|        - |  5401 | ` * OP_NULLC: * P2 *` |
|        - |  5402 | ` * Short-circuit null coalescing '??'.` |
|        - |  5403 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  5404 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  5405 | ` */` |
|       34 |  5406 | `case PH7_OP_NULLC: {` |
|        - |  5407 | `#ifdef UNTRUST` |
|        - |  5408 | `	if( pTos < pStack ){` |
|        - |  5409 | `		goto Abort;` |
|        - |  5410 | `	}` |
|        - |  5411 | `#endif` |
|       70 |  5412 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  5413 | `		/* Left is not null — keep it and skip the RHS */` |
|       32 |  5414 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       17 |  5415 | `	}else{` |
|        - |  5416 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       40 |  5417 | `		VmPopOperand(&pTos, 1);` |
|        - |  5418 | `	}` |
|       70 |  5419 | `	break;` |
|        - |  5420 |  |
|        - |  5421 | `/*` |
|        - |  5422 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  5423 | ` * Null coalescing assignment short-circuit.` |
|        - |  5424 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  5425 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  5426 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  5427 | ` */` |
|       23 |  5428 | `case PH7_OP_NULLC_JMP: {` |
|        - |  5429 | `#ifdef UNTRUST` |
|        - |  5430 | `	if( pTos < pStack ){` |
|        - |  5431 | `		goto Abort;` |
|        - |  5432 | `	}` |
|        - |  5433 | `#endif` |
|       47 |  5434 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  5435 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  5436 | `	}` |
|       47 |  5437 | `	break;` |
|        - |  5438 |  |
|        - |  5439 | `/*` |
|        - |  5440 | ` * OP_NULLC_STORE: * * *` |
|        - |  5441 | ` * Null coalescing assignment store.` |
|        - |  5442 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  5443 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  5444 | ` * expression result.` |
|        - |  5445 | ` */` |
|        - |  5446 | `/*` |
|        - |  5447 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  5448 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  5449 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  5450 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  5451 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  5452 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  5453 | ` */` |
|       51 |  5454 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  5455 | `#ifdef UNTRUST` |
|        - |  5456 | `	if( pTos < pStack ){` |
|        - |  5457 | `		goto Abort;` |
|        - |  5458 | `	}` |
|        - |  5459 | `#endif` |
|      104 |  5460 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  5461 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  5462 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  5463 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  5464 | `	}` |
|      104 |  5465 | `	break;` |
|        - |  5466 |  |
|       14 |  5467 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  5468 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5469 | `	ph7_value *pObj;` |
|        - |  5470 | `	sxu32 nIdx;` |
|        - |  5471 | `#ifdef UNTRUST` |
|        - |  5472 | `	if( pNos < pStack ){` |
|        - |  5473 | `		goto Abort;` |
|        - |  5474 | `	}` |
|        - |  5475 | `#endif` |
|       29 |  5476 | `	nIdx = pNos->nIdx;` |
|       29 |  5477 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5478 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5479 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  5480 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  5481 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  5482 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  5483 | `	}` |
|       29 |  5484 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  5485 | `	VmPopOperand(&pTos,1);` |
|       29 |  5486 | `	break;` |
|        - |  5487 |  |
|        - |  5488 | `/*` |
|        - |  5489 | ` * OP_SPREAD: * * *` |
|        - |  5490 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  5491 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  5492 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  5493 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  5494 | ` */` |
|        9 |  5495 | `case PH7_OP_SPREAD: {` |
|        - |  5496 | `#ifdef UNTRUST` |
|        - |  5497 | `	if( pTos < pStack ){` |
|        - |  5498 | `		goto Abort;` |
|        - |  5499 | `	}` |
|        - |  5500 | `#endif` |
|       20 |  5501 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  5502 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  5503 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  5504 | `		if( nEntry == 0 ){` |
|        - |  5505 | `			/* Empty array — remove from stack */` |
|        3 |  5506 | `			VmPopOperand(&pTos, 1);` |
|        3 |  5507 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  5508 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  5509 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  5510 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  5511 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  5512 | `				VM_STACK_GUARD);` |
|      ! 0 |  5513 | `		}else{` |
|        - |  5514 | `			ph7_hashmap_node *pNode2;` |
|        - |  5515 | `			ph7_value *pElem;` |
|        - |  5516 | `			sxu32 i;` |
|        - |  5517 | `			/* Overwrite TOS with first element */` |
|       18 |  5518 | `			pNode2 = pMap->pFirst;` |
|       18 |  5519 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  5520 | `			PH7_MemObjRelease(pTos);` |
|       18 |  5521 | `			if( pElem ){` |
|       18 |  5522 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  5523 | `			}` |
|       18 |  5524 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5525 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  5526 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  5527 | `			pNode2 = pNode2->pPrev;` |
|        - |  5528 | `			/* Push remaining elements */` |
|       44 |  5529 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  5530 | `				pTos++;` |
|       28 |  5531 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  5532 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  5533 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  5534 | `				if( pElem ){` |
|       28 |  5535 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  5536 | `				}` |
|       28 |  5537 | `				pNode2 = pNode2->pPrev;` |
|       15 |  5538 | `			}` |
|       18 |  5539 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  5540 | `		}` |
|        9 |  5541 | `	}` |
|        - |  5542 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  5543 | `	break;` |
|        - |  5544 |  |
|        - |  5545 | `/* OP_LXOR: * * *` |
|        - |  5546 | ` *` |
|        - |  5547 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  5548 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5549 | ` * stack.` |
|        - |  5550 | ` * According to the PHP language reference manual:` |
|        - |  5551 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  5552 | ` *  TRUE,but not both.` |
|        - |  5553 | ` */` |
|        5 |  5554 | `case PH7_OP_LXOR:{` |
|       11 |  5555 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  5556 | `	sxi32 v = 0;` |
|        - |  5557 | `#ifdef UNTRUST` |
|        - |  5558 | `	if( pNos < pStack ){` |
|        - |  5559 | `		goto Abort;` |
|        - |  5560 | `	}` |
|        - |  5561 | `#endif` |
|        - |  5562 | `	/* Force a boolean cast */` |
|       11 |  5563 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5564 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  5565 | `	}` |
|       11 |  5566 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5567 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5568 | `	}` |
|       11 |  5569 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  5570 | `		v = 1;` |
|        3 |  5571 | `	}` |
|       11 |  5572 | `	VmPopOperand(&pTos,1);` |
|       11 |  5573 | `	pTos->x.iVal = v;` |
|       11 |  5574 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  5575 | `	break;` |
|        - |  5576 | `				 }` |
|        - |  5577 | `/* OP_EQ P1 P2 P3` |
|        - |  5578 | ` *` |
|        - |  5579 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  5580 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5581 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5582 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5583 | ` */` |
|        - |  5584 | `/* OP_NEQ P1 P2 P3` |
|        - |  5585 | ` *` |
|        - |  5586 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  5587 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5588 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5589 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5590 | ` */` |
|     4264 |  5591 | `case PH7_OP_EQ:` |
|        - |  5592 | `case PH7_OP_NEQ: {` |
|     8530 |  5593 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5594 | `	/* Perform the comparison and act accordingly */` |
|        - |  5595 | `#ifdef UNTRUST` |
|        - |  5596 | `	if( pNos < pStack ){` |
|        - |  5597 | `		goto Abort;` |
|        - |  5598 | `	}` |
|        - |  5599 | `#endif` |
|     8530 |  5600 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8530 |  5601 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  5602 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8521 |  5603 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8486 |  5604 | `		rc = rc == 0;` |
|     4244 |  5605 | `	}else{` |
|       28 |  5606 | `		rc = rc != 0;` |
|        - |  5607 | `	}` |
|     8530 |  5608 | `	VmPopOperand(&pTos,1);` |
|     8530 |  5609 | `	if( !pInstr->iP2 ){` |
|        - |  5610 | `		/* Push comparison result without taking the jump */` |
|     8530 |  5611 | `		PH7_MemObjRelease(pTos);` |
|     8530 |  5612 | `		pTos->x.iVal = rc;` |
|        - |  5613 | `		/* Invalidate any prior representation */` |
|     8530 |  5614 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4266 |  5615 | `	}else{` |
|      ! 0 |  5616 | `		if( rc ){` |
|        - |  5617 | `			/* Jump to the desired location */` |
|      ! 0 |  5618 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5619 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5620 | `		}` |
|        - |  5621 | `	}` |
|     8530 |  5622 | `	break;` |
|        - |  5623 | `				 }` |
|        - |  5624 | `/* OP_TEQ P1 P2 *` |
|        - |  5625 | ` *` |
|        - |  5626 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  5627 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  5628 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5629 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5630 | ` */` |
|   148318 |  5631 | `case PH7_OP_TEQ: {` |
|   296638 |  5632 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5633 | `	/* Perform the comparison and act accordingly */` |
|        - |  5634 | `#ifdef UNTRUST` |
|        - |  5635 | `	if( pNos < pStack ){` |
|        - |  5636 | `		goto Abort;` |
|        - |  5637 | `	}` |
|        - |  5638 | `#endif` |
|   296638 |  5639 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   296638 |  5640 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5641 | `		rc = 0;` |
|        2 |  5642 | `	}else{` |
|   296636 |  5643 | `		rc = rc == 0;` |
|        - |  5644 | `	}` |
|   296638 |  5645 | `	VmPopOperand(&pTos,1);` |
|   296638 |  5646 | `	if( !pInstr->iP2 ){` |
|        - |  5647 | `		/* Push comparison result without taking the jump */` |
|   296638 |  5648 | `		PH7_MemObjRelease(pTos);` |
|   296638 |  5649 | `		pTos->x.iVal = rc;` |
|        - |  5650 | `		/* Invalidate any prior representation */` |
|   296638 |  5651 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   148320 |  5652 | `	}else{` |
|      ! 0 |  5653 | `		if( rc ){` |
|        - |  5654 | `			/* Jump to the desired location */` |
|      ! 0 |  5655 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5656 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5657 | `		}` |
|        - |  5658 | `	}` |
|   296638 |  5659 | `	break;` |
|        - |  5660 | `				 }` |
|        - |  5661 | `/* OP_TNE P1 P2 *` |
|        - |  5662 | ` *` |
|        - |  5663 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  5664 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  5665 | ` * instruction.` |
|        - |  5666 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5667 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5668 | ` *` |
|        - |  5669 | ` */` |
|   114432 |  5670 | `case PH7_OP_TNE: {` |
|   228866 |  5671 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5672 | `	/* Perform the comparison and act accordingly */` |
|        - |  5673 | `#ifdef UNTRUST` |
|        - |  5674 | `	if( pNos < pStack ){` |
|        - |  5675 | `		goto Abort;` |
|        - |  5676 | `	}` |
|        - |  5677 | `#endif` |
|   228866 |  5678 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   228866 |  5679 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  5680 | `		rc = 1;` |
|        2 |  5681 | `	}else{` |
|   228864 |  5682 | `		rc = rc != 0;` |
|        - |  5683 | `	}` |
|   228866 |  5684 | `	VmPopOperand(&pTos,1);` |
|   228866 |  5685 | `	if( !pInstr->iP2 ){` |
|        - |  5686 | `		/* Push comparison result without taking the jump */` |
|   228866 |  5687 | `		PH7_MemObjRelease(pTos);` |
|   228866 |  5688 | `		pTos->x.iVal = rc;` |
|        - |  5689 | `		/* Invalidate any prior representation */` |
|   228866 |  5690 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   114434 |  5691 | `	}else{` |
|      ! 0 |  5692 | `		if( rc ){` |
|        - |  5693 | `			/* Jump to the desired location */` |
|      ! 0 |  5694 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5695 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5696 | `		}` |
|        - |  5697 | `	}` |
|   228866 |  5698 | `	break;` |
|        - |  5699 | `				 }` |
|        - |  5700 | `/* OP_LT P1 P2 P3` |
|        - |  5701 | ` *` |
|        - |  5702 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5703 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5704 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5705 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5706 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5707 | ` *` |
|        - |  5708 | ` */` |
|        - |  5709 | `/* OP_LE P1 P2 P3` |
|        - |  5710 | ` *` |
|        - |  5711 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5712 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5713 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5714 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5715 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5716 | ` *` |
|        - |  5717 | ` */` |
|   107887 |  5718 | `case PH7_OP_LT:` |
|        - |  5719 | `case PH7_OP_LE: {` |
|   215820 |  5720 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5721 | `	/* Perform the comparison and act accordingly */` |
|        - |  5722 | `#ifdef UNTRUST` |
|        - |  5723 | `	if( pNos < pStack ){` |
|        - |  5724 | `		goto Abort;` |
|        - |  5725 | `	}` |
|        - |  5726 | `#endif` |
|   215820 |  5727 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   215820 |  5728 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5729 | `		rc = 0;` |
|   215816 |  5730 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1044 |  5731 | `		rc = rc < 1;` |
|      523 |  5732 | `	}else{` |
|   214770 |  5733 | `		rc = rc < 0;` |
|        - |  5734 | `	}` |
|   215820 |  5735 | `	VmPopOperand(&pTos,1);` |
|   215820 |  5736 | `	if( !pInstr->iP2 ){` |
|        - |  5737 | `		/* Push comparison result without taking the jump */` |
|   215820 |  5738 | `		PH7_MemObjRelease(pTos);` |
|   215820 |  5739 | `		pTos->x.iVal = rc;` |
|        - |  5740 | `		/* Invalidate any prior representation */` |
|   215820 |  5741 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   107933 |  5742 | `	}else{` |
|      ! 0 |  5743 | `		if( rc ){` |
|        - |  5744 | `			/* Jump to the desired location */` |
|      ! 0 |  5745 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5746 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5747 | `		}` |
|        - |  5748 | `	}` |
|   215820 |  5749 | `	break;` |
|        - |  5750 | `				}` |
|        - |  5751 | `/* OP_GT P1 P2 P3` |
|        - |  5752 | ` *` |
|        - |  5753 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5754 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  5755 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5756 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5757 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5758 | ` *` |
|        - |  5759 | ` */` |
|        - |  5760 | `/* OP_GE P1 P2 P3` |
|        - |  5761 | ` *` |
|        - |  5762 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  5763 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  5764 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  5765 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5766 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5767 | ` *` |
|        - |  5768 | ` */` |
|    52531 |  5769 | `case PH7_OP_GT:` |
|        - |  5770 | `case PH7_OP_GE: {` |
|   105064 |  5771 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5772 | `	/* Perform the comparison and act accordingly */` |
|        - |  5773 | `#ifdef UNTRUST` |
|        - |  5774 | `	if( pNos < pStack ){` |
|        - |  5775 | `		goto Abort;` |
|        - |  5776 | `	}` |
|        - |  5777 | `#endif` |
|   105064 |  5778 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   105064 |  5779 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  5780 | `		rc = 0;` |
|   105060 |  5781 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   104896 |  5782 | `		rc = rc >= 0;` |
|    52449 |  5783 | `	}else{` |
|      162 |  5784 | `		rc = rc > 0;` |
|        - |  5785 | `	}` |
|   105064 |  5786 | `	VmPopOperand(&pTos,1);` |
|   105064 |  5787 | `	if( !pInstr->iP2 ){` |
|        - |  5788 | `		/* Push comparison result without taking the jump */` |
|   105064 |  5789 | `		PH7_MemObjRelease(pTos);` |
|   105064 |  5790 | `		pTos->x.iVal = rc;` |
|        - |  5791 | `		/* Invalidate any prior representation */` |
|   105064 |  5792 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    52533 |  5793 | `	}else{` |
|      ! 0 |  5794 | `		if( rc ){` |
|        - |  5795 | `			/* Jump to the desired location */` |
|      ! 0 |  5796 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5797 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5798 | `		}` |
|        - |  5799 | `	}` |
|   105064 |  5800 | `	break;` |
|        - |  5801 | `				}` |
|        - |  5802 | `/* OP_SPACESHIP * * *` |
|        - |  5803 | ` *` |
|        - |  5804 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  5805 | ` *   -1 if left < right` |
|        - |  5806 | ` *    0 if left == right` |
|        - |  5807 | ` *    1 if left > right` |
|        - |  5808 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  5809 | ` */` |
|       25 |  5810 | `case PH7_OP_SPACESHIP: {` |
|       51 |  5811 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5812 | `#ifdef UNTRUST` |
|        - |  5813 | `	if( pNos < pStack ){` |
|        - |  5814 | `		goto Abort;` |
|        - |  5815 | `	}` |
|        - |  5816 | `#endif` |
|       51 |  5817 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  5818 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  5819 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  5820 | `		rc = 1;` |
|        4 |  5821 | `	}else{` |
|        - |  5822 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  5823 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  5824 | `	}` |
|       51 |  5825 | `	VmPopOperand(&pTos,1);` |
|       51 |  5826 | `	PH7_MemObjRelease(pTos);` |
|       51 |  5827 | `	pTos->x.iVal = rc;` |
|       51 |  5828 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  5829 | `	break;` |
|        - |  5830 | `				}` |
|        - |  5831 | `/* OP_SEQ P1 P2 *` |
|        - |  5832 | ` * Strict string comparison.` |
|        - |  5833 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  5834 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5835 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5836 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5837 | ` * use PH7_OP_EQ.` |
|        - |  5838 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5839 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5840 | ` */` |
|        - |  5841 | `/* OP_SNE P1 P2 *` |
|        - |  5842 | ` * Strict string comparison.` |
|        - |  5843 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  5844 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  5845 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  5846 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  5847 | ` * use PH7_OP_EQ.` |
|        - |  5848 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  5849 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  5850 | ` */` |
|       18 |  5851 | `case PH7_OP_SEQ:` |
|        - |  5852 | `case PH7_OP_SNE: {` |
|       38 |  5853 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5854 | `	SyString s1,s2;` |
|        - |  5855 | `	/* Perform the comparison and act accordingly */` |
|        - |  5856 | `#ifdef UNTRUST` |
|        - |  5857 | `	if( pNos < pStack ){` |
|        - |  5858 | `		goto Abort;` |
|        - |  5859 | `	}` |
|        - |  5860 | `#endif` |
|        - |  5861 | `	/* Force a string cast */` |
|       38 |  5862 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  5863 | `		PH7_MemObjToString(pTos);` |
|        2 |  5864 | `	}` |
|       38 |  5865 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  5866 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5867 | `	}` |
|       38 |  5868 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  5869 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  5870 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  5871 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  5872 | `		rc = rc != 0;` |
|      ! 0 |  5873 | `	}else{` |
|       38 |  5874 | `		rc = rc == 0;` |
|        - |  5875 | `	}` |
|       38 |  5876 | `	VmPopOperand(&pTos,1);` |
|       38 |  5877 | `	if( !pInstr->iP2 ){` |
|        - |  5878 | `		/* Push comparison result without taking the jump */` |
|       38 |  5879 | `		PH7_MemObjRelease(pTos);` |
|       38 |  5880 | `		pTos->x.iVal = rc;` |
|        - |  5881 | `		/* Invalidate any prior representation */` |
|       38 |  5882 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  5883 | `	}else{` |
|      ! 0 |  5884 | `		if( rc ){` |
|        - |  5885 | `			/* Jump to the desired location */` |
|      ! 0 |  5886 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5887 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5888 | `		}` |
|        - |  5889 | `	}` |
|       38 |  5890 | `	break;` |
|        - |  5891 | `				 }` |
|        - |  5892 | `/*` |
|        - |  5893 | ` * OP_LOAD_REF * * *` |
|        - |  5894 | ` * Push the index of a referenced object on the stack.` |
|        - |  5895 | ` */` |
|       57 |  5896 | `case PH7_OP_LOAD_REF: {` |
|        - |  5897 | `	sxu32 nIdx;` |
|        - |  5898 | `#ifdef UNTRUST` |
|        - |  5899 | `	if( pTos < pStack ){` |
|        - |  5900 | `		goto Abort;` |
|        - |  5901 | `	}` |
|        - |  5902 | `#endif` |
|        - |  5903 | `	/* Extract memory object index */` |
|      115 |  5904 | `	nIdx = pTos->nIdx;` |
|      115 |  5905 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  5906 | `		/* Nullify the object */` |
|       95 |  5907 | `		PH7_MemObjRelease(pTos);` |
|        - |  5908 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  5909 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  5910 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  5911 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  5912 | `	}` |
|      115 |  5913 | `	break;` |
|        - |  5914 | `					  }` |
|        - |  5915 | `/*` |
|        - |  5916 | ` * OP_STORE_REF * * P3` |
|        - |  5917 | ` * Perform an assignment operation by reference.` |
|        - |  5918 | ` */` |
|       16 |  5919 | ` case PH7_OP_STORE_REF: {` |
|       34 |  5920 | `	 SyString sName = { 0 , 0 };` |
|        - |  5921 | `	 VmFrame *pFrameLocal;` |
|        - |  5922 | `	SyHashEntry *pEntry;` |
|        - |  5923 | `	sxu32 nIdx;` |
|        - |  5924 | `#ifdef UNTRUST` |
|        - |  5925 | `	if( pTos < pStack ){` |
|        - |  5926 | `		goto Abort;` |
|        - |  5927 | `	}` |
|        - |  5928 | `#endif` |
|       34 |  5929 | `	if( pInstr->p3 == 0 ){` |
|        - |  5930 | `		char *zName;` |
|        - |  5931 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  5932 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5933 | `			/* Force a string cast */` |
|      ! 0 |  5934 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5935 | `		}` |
|      ! 0 |  5936 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  5937 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  5938 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5939 | `			if( zName ){` |
|      ! 0 |  5940 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  5941 | `			}` |
|      ! 0 |  5942 | `		}` |
|      ! 0 |  5943 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5944 | `		pTos--;` |
|      ! 0 |  5945 | `	}else{` |
|       34 |  5946 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5947 | `	}` |
|       34 |  5948 | `	nIdx = pTos->nIdx;` |
|       34 |  5949 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  5950 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5951 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5952 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  5953 | `		}else{` |
|        - |  5954 | `			ph7_value *pObj;` |
|        - |  5955 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  5956 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  5957 | `			if( pObj == 0 ){` |
|      ! 0 |  5958 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5959 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5960 | `				goto Abort;` |
|        - |  5961 | `			}` |
|        - |  5962 | `			/* Perform the store operation */` |
|      ! 0 |  5963 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  5964 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  5965 | `		}` |
|       34 |  5966 | `	}else if( sName.nByte > 0){` |
|       34 |  5967 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  5968 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  5969 | `		}else{` |
|       34 |  5970 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  5971 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  5972 | `			/* Query the local frame */` |
|       34 |  5973 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  5974 | `			if( pEntry ){` |
|      ! 0 |  5975 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  5976 | `			}else{` |
|       34 |  5977 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  5978 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  5979 | `					/* Insert in the $GLOBALS array */` |
|       30 |  5980 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  5981 | `				}` |
|       34 |  5982 | `				if( rc == SXRET_OK ){` |
|       34 |  5983 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  5984 | `				}` |
|        - |  5985 | `			}` |
|        - |  5986 | `		}` |
|       16 |  5987 | `	}` |
|       34 |  5988 | `	break;` |
|        - |  5989 | `				 }` |
|        - |  5990 | `/*` |
|        - |  5991 | ` * OP_UPLINK P1 * *` |
|        - |  5992 | ` * Link a variable to the top active VM frame.` |
|        - |  5993 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  5994 | ` */` |
|       28 |  5995 | `case PH7_OP_UPLINK: {` |
|       58 |  5996 | `	if( pVm->pFrame->pParent ){` |
|       58 |  5997 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  5998 | `		SyString sName;` |
|        - |  5999 | `		/* Perform the link */` |
|      116 |  6000 | `		while( pLink <= pTos ){` |
|       60 |  6001 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6002 | `				/* Force a string cast */` |
|      ! 0 |  6003 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  6004 | `			}` |
|       60 |  6005 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  6006 | `			if( sName.nByte > 0 ){` |
|       60 |  6007 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  6008 | `			}` |
|       60 |  6009 | `			pLink++;` |
|        2 |  6010 | `		}` |
|       28 |  6011 | `	}` |
|       58 |  6012 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  6013 | `	break;` |
|        - |  6014 | `					}` |
|        - |  6015 | `/*` |
|        - |  6016 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  6017 | ` * Push an exception in the corresponding container so that` |
|        - |  6018 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  6019 | ` */` |
|       80 |  6020 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      162 |  6021 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6022 | `	VmFrame *pFrameLocal;` |
|        - |  6023 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      162 |  6024 | `	pException->iFinallyDone = 0;` |
|      162 |  6025 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6026 | `	/* Create the exception frame */` |
|      162 |  6027 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      162 |  6028 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6029 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6030 | `		goto Abort;` |
|        - |  6031 | `	}` |
|        - |  6032 | `	/* Mark the special frame */` |
|      162 |  6033 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      162 |  6034 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6035 | `	/* Point to the frame that trigger the exception */` |
|      162 |  6036 | `	pFrameLocal = pFrameLocal->pParent;` |
|      162 |  6037 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      162 |  6038 | `	pException->pFrame = pFrameLocal;` |
|      162 |  6039 | `	break;` |
|        - |  6040 | `							}` |
|        - |  6041 | `/*` |
|        - |  6042 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6043 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6044 | ` */` |
|       79 |  6045 | `case PH7_OP_POP_EXCEPTION: {` |
|      160 |  6046 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      160 |  6047 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6048 | `		ph7_exception **apException;` |
|        - |  6049 | `		/* Pop the loaded exception */` |
|       28 |  6050 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  6051 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  6052 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  6053 | `		}` |
|       13 |  6054 | `	}` |
|      160 |  6055 | `	pException->pFrame = 0;` |
|        - |  6056 | `	/* Leave the exception frame */` |
|      160 |  6057 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6058 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      160 |  6059 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6060 | `		sxi32 rcFinally;` |
|       20 |  6061 | `		pException->iFinallyDone = 1;` |
|       20 |  6062 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6063 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6064 | `			goto Abort;` |
|        - |  6065 | `		}` |
|        9 |  6066 | `	}` |
|      160 |  6067 | `	break;` |
|        - |  6068 | `							}` |
|        - |  6069 |  |
|        - |  6070 | `/*` |
|        - |  6071 | ` * OP_THROW * P2 *` |
|        - |  6072 | ` * Throw an user exception.` |
|        - |  6073 | ` */` |
|       31 |  6074 | `case PH7_OP_THROW: {` |
|       64 |  6075 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       64 |  6076 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6077 | `#ifdef UNTRUST` |
|        - |  6078 | `	if( pTos < pStack ){` |
|        - |  6079 | `		goto Abort;` |
|        - |  6080 | `	}` |
|        - |  6081 | `#endif` |
|       64 |  6082 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6083 | `	/* Tell the upper layer that an exception was thrown */` |
|       64 |  6084 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       64 |  6085 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       64 |  6086 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6087 | `		ph7_class *pException;` |
|        - |  6088 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  6089 | `		 */` |
|       64 |  6090 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       64 |  6091 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  6092 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  6093 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6094 | `			if( rc == SXERR_ABORT ){` |
|        - |  6095 | `				/* Abort processing immediately */` |
|      ! 0 |  6096 | `				goto Abort;` |
|        - |  6097 | `			}` |
|      ! 0 |  6098 | `		}else{` |
|        - |  6099 | `			/* Throw the exception */` |
|       64 |  6100 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       64 |  6101 | `			if( rc == SXERR_ABORT ){` |
|        - |  6102 | `				/* Abort processing immediately */` |
|        9 |  6103 | `				goto Abort;` |
|        - |  6104 | `			}` |
|        - |  6105 | `		}` |
|       29 |  6106 | `	}else{` |
|        - |  6107 | `		/* Expecting a class instance */` |
|      ! 0 |  6108 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6109 | `		if( rc == SXERR_ABORT ){` |
|        - |  6110 | `			/* Abort processing immediately */` |
|      ! 0 |  6111 | `			goto Abort;` |
|        - |  6112 | `		}` |
|        - |  6113 | `	}` |
|        - |  6114 | `	/* Pop the top entry */` |
|       56 |  6115 | `	VmPopOperand(&pTos,1);` |
|        - |  6116 | `	/* Perform an unconditional jump */` |
|       56 |  6117 | `	pc = nJump - 1;` |
|       56 |  6118 | `	break;` |
|        - |  6119 | `				   }` |
|        - |  6120 | `/*` |
|        - |  6121 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6122 | ` * Prepare a foreach step.` |
|        - |  6123 | ` */` |
|     5607 |  6124 | `case PH7_OP_FOREACH_INIT: {` |
|    11216 |  6125 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6126 | `	void *pName;` |
|        - |  6127 | `#ifdef UNTRUST` |
|        - |  6128 | `	if( pTos < pStack ){` |
|        - |  6129 | `		goto Abort;` |
|        - |  6130 | `	}` |
|        - |  6131 | `#endif` |
|    11216 |  6132 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6133 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6134 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6135 | `			/* Force a string cast */` |
|      ! 0 |  6136 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6137 | `		}` |
|        - |  6138 | `		/* Duplicate name */` |
|      ! 0 |  6139 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6140 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6141 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6142 | `		}` |
|      ! 0 |  6143 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6144 | `	}` |
|    11216 |  6145 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6146 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6147 | `			/* Force a string cast */` |
|      ! 0 |  6148 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6149 | `		}` |
|        - |  6150 | `		/* Duplicate name */` |
|      ! 0 |  6151 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6152 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6153 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6154 | `		}` |
|      ! 0 |  6155 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6156 | `	}` |
|        - |  6157 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11216 |  6158 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6159 | `		/* Jump out of the loop */` |
|      ! 0 |  6160 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6161 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6162 | `		}` |
|      ! 0 |  6163 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6164 | `	}else{` |
|        - |  6165 | `		ph7_foreach_step *pStep;` |
|    11216 |  6166 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11216 |  6167 | `		if( pStep == 0 ){` |
|      ! 0 |  6168 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6169 | `			/* Jump out of the loop */` |
|      ! 0 |  6170 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6171 | `		}else{` |
|        - |  6172 | `			/* Zero the structure */` |
|    11216 |  6173 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6174 | `			/* Prepare the step */` |
|    11216 |  6175 | `			pStep->iFlags = pInfo->iFlags;` |
|    11216 |  6176 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6177 | `				ph7_hashmap *pMap;` |
|        - |  6178 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6179 | `				 * source array so mutations don't affect other sharers. */` |
|    11184 |  6180 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6181 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6182 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6183 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6184 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6185 | `						 * variable still points at the same hashmap as` |
|        - |  6186 | `						 * the stack value. */` |
|        9 |  6187 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6188 | `							pCur->iRef--;` |
|        9 |  6189 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6190 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6191 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6192 | `						}` |
|        4 |  6193 | `					}` |
|        4 |  6194 | `				}` |
|    11184 |  6195 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6196 | `				/* Reset the internal loop cursor */` |
|    11184 |  6197 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6198 | `				/* Mark the step */` |
|    11184 |  6199 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    11184 |  6200 | `				pStep->xIter.pMap = pMap;` |
|    11184 |  6201 | `				pMap->iRef++;` |
|     5593 |  6202 | `			}else{` |
|       34 |  6203 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6204 | `				ph7_class *pIteratorClass;` |
|        - |  6205 | `				/* Check if the object implements Iterator */` |
|       34 |  6206 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6207 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6208 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6209 | `					ph7_class_method *pRewind;` |
|       24 |  6210 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6211 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6212 | `					pThis->iRef++;` |
|       24 |  6213 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6214 | `					if( pRewind ){` |
|       24 |  6215 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6216 | `					}` |
|       13 |  6217 | `				}else{` |
|        - |  6218 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6219 | `					ph7_class *pIterAggClass;` |
|       12 |  6220 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6221 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6222 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6223 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6224 | `						ph7_class_method *pGetIter;` |
|        3 |  6225 | `						int iterAggOk = 0;` |
|        3 |  6226 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6227 | `						if( pGetIter ){` |
|        - |  6228 | `							ph7_value sResult;` |
|        3 |  6229 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6230 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6231 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6232 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6233 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6234 | `									ph7_class_method *pRewind;` |
|        3 |  6235 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6236 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6237 | `									pIterObj->iRef++;` |
|        - |  6238 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6239 | `									pStep->pOwner = pThis;` |
|        3 |  6240 | `									pThis->iRef++;` |
|        3 |  6241 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6242 | `									if( pRewind ){` |
|        3 |  6243 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6244 | `									}` |
|        3 |  6245 | `									iterAggOk = 1;` |
|        1 |  6246 | `								}` |
|        1 |  6247 | `							}` |
|        3 |  6248 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6249 | `						}` |
|        3 |  6250 | `						if( !iterAggOk ){` |
|        - |  6251 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6252 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6253 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6254 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6255 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6256 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6257 | `						}` |
|        2 |  6258 | `					}else{` |
|        - |  6259 | `						/* Plain object iteration via hAttr */` |
|        9 |  6260 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6261 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6262 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6263 | `						pThis->iRef++;` |
|        - |  6264 | `					}` |
|        - |  6265 | `				}` |
|        - |  6266 | `			}` |
|        - |  6267 | `		}` |
|    11216 |  6268 | `		if( pStep ){` |
|    11216 |  6269 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6270 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6271 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6272 | `				/* Jump out of the loop */` |
|      ! 0 |  6273 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6274 | `			}` |
|     5607 |  6275 | `		}` |
|        - |  6276 | `	}` |
|    11216 |  6277 | `	VmPopOperand(&pTos,1);` |
|    11216 |  6278 | `	break;` |
|        - |  6279 | `						  }` |
|        - |  6280 | `/*` |
|        - |  6281 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6282 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6283 | ` */` |
|    91475 |  6284 | `case PH7_OP_FOREACH_STEP: {` |
|   182952 |  6285 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6286 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6287 | `	ph7_value *pValue;` |
|        - |  6288 | `	VmFrame *pFrameLocal;` |
|        - |  6289 | `	/* Peek the last step */` |
|   182952 |  6290 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   182952 |  6291 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   182952 |  6292 | `	pFrameLocal = pVm->pFrame;` |
|   182952 |  6293 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   182952 |  6294 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   182824 |  6295 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6296 | `		ph7_hashmap_node *pNode;` |
|        - |  6297 | `		/* Extract the current node value */` |
|   182824 |  6298 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   182824 |  6299 | `		if( pNode == 0 ){` |
|        - |  6300 | `			/* No more entry to process */` |
|    11182 |  6301 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    11182 |  6302 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6303 | `				/* Break the reference with the last element */` |
|        7 |  6304 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6305 | `			}` |
|        - |  6306 | `			/* Automatically reset the loop cursor */` |
|    11182 |  6307 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6308 | `			/* Cleanup the mess left behind */` |
|    11182 |  6309 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    11182 |  6310 | `			SySetPop(&pInfo->aStep);` |
|    11182 |  6311 | `			PH7_HashmapUnref(pMap);` |
|     5592 |  6312 | `		}else{` |
|   171644 |  6313 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      426 |  6314 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      426 |  6315 | `				if( pKey ){` |
|      426 |  6316 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      212 |  6317 | `				}` |
|      212 |  6318 | `			}` |
|   171644 |  6319 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6320 | `				SyHashEntry *pEntry;` |
|        - |  6321 | `				/* Pass by reference */` |
|       23 |  6322 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6323 | `				if( pEntry ){` |
|       21 |  6324 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6325 | `				}else{` |
|        4 |  6326 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6327 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6328 | `				}` |
|       12 |  6329 | `			}else{` |
|        - |  6330 | `				/* Make a copy of the entry value */` |
|   171622 |  6331 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   171622 |  6332 | `				if( pValue ){` |
|   171622 |  6333 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    85810 |  6334 | `				}` |
|        - |  6335 | `			}` |
|        2 |  6336 | `		}` |
|    91541 |  6337 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6338 | `		/* Iterator-based iteration.` |
|        - |  6339 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6340 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6341 | `		 */` |
|      106 |  6342 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6343 | `		ph7_class_method *pMethod;` |
|        - |  6344 | `		ph7_value sResult;` |
|      106 |  6345 | `		int isValid = 0;` |
|        - |  6346 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  6347 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  6348 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  6349 | `		}else{` |
|       82 |  6350 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  6351 | `			if( pMethod ){` |
|       82 |  6352 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  6353 | `			}` |
|        - |  6354 | `		}` |
|        - |  6355 | `		/* Call valid() */` |
|      106 |  6356 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  6357 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  6358 | `		if( pMethod ){` |
|      106 |  6359 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  6360 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  6361 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  6362 | `		}` |
|      106 |  6363 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  6364 | `		if( !isValid ){` |
|        - |  6365 | `			/* Iterator exhausted */` |
|       24 |  6366 | `			pc = pInstr->iP2 - 1;` |
|        - |  6367 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  6368 | `			if( pStep->pOwner ){` |
|        3 |  6369 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6370 | `			}` |
|       24 |  6371 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  6372 | `			SySetPop(&pInfo->aStep);` |
|       24 |  6373 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  6374 | `		}else{` |
|        - |  6375 | `			/* Call current() to get value */` |
|       84 |  6376 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  6377 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  6378 | `			if( pMethod ){` |
|       84 |  6379 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  6380 | `			}` |
|       84 |  6381 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  6382 | `			if( pValue ){` |
|       84 |  6383 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  6384 | `			}` |
|       84 |  6385 | `			PH7_MemObjRelease(&sResult);` |
|        - |  6386 | `			/* Call key() if needed */` |
|       84 |  6387 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  6388 | `				ph7_value sKey;` |
|       35 |  6389 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  6390 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  6391 | `				if( pMethod ){` |
|       35 |  6392 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  6393 | `				}` |
|       35 |  6394 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  6395 | `				if( pValue ){` |
|       35 |  6396 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  6397 | `				}` |
|       35 |  6398 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  6399 | `			}` |
|        - |  6400 | `		}` |
|       54 |  6401 | `	}else{` |
|       25 |  6402 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  6403 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  6404 | `		SyHashEntry *pEntry;` |
|        - |  6405 | `		/* Point to the next attribute */` |
|       29 |  6406 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  6407 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  6408 | `			/* Check access permission */` |
|       31 |  6409 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  6410 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  6411 | `					break; /* Access is granted */` |
|        - |  6412 | `			}` |
|        1 |  6413 | `		}` |
|       25 |  6414 | `		if( pEntry == 0 ){` |
|        - |  6415 | `			/* Clean up the mess left behind */` |
|        9 |  6416 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  6417 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6418 | `				/* Break the reference with the last element */` |
|        3 |  6419 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  6420 | `			}` |
|        9 |  6421 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  6422 | `			SySetPop(&pInfo->aStep);` |
|        9 |  6423 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  6424 | `		}else{` |
|       17 |  6425 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  6426 | `			ph7_value *pAttrValue;` |
|       17 |  6427 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  6428 | `				/* Fill with the current attribute name */` |
|       17 |  6429 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  6430 | `				if( pKey ){` |
|       17 |  6431 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  6432 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  6433 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  6434 | `				}` |
|        8 |  6435 | `			}` |
|        - |  6436 | `			/* Extract attribute value */` |
|       17 |  6437 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  6438 | `			if( pAttrValue ){` |
|       17 |  6439 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6440 | `					/* Pass by reference */` |
|        3 |  6441 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  6442 | `					if( pEntry ){` |
|        3 |  6443 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  6444 | `					}else{` |
|      ! 0 |  6445 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  6446 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  6447 | `					}` |
|        2 |  6448 | `				}else{` |
|        - |  6449 | `					/* Make a copy of the attribute value */` |
|       15 |  6450 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  6451 | `					if( pValue ){` |
|       15 |  6452 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  6453 | `					}` |
|        - |  6454 | `				}` |
|        8 |  6455 | `			}` |
|        - |  6456 | `		}` |
|        - |  6457 | `	}` |
|   182952 |  6458 | `	break;` |
|        - |  6459 | `						  }` |
|        - |  6460 | `/*` |
|        - |  6461 | ` * OP_MEMBER P1 P2` |
|        - |  6462 | ` * Load class attribute/method on the stack.` |
|        - |  6463 | ` */` |
|     2871 |  6464 | `case PH7_OP_MEMBER: {` |
|        - |  6465 | `	ph7_class_instance *pThis;` |
|        - |  6466 | `	ph7_value *pNos;` |
|        - |  6467 | `	SyString sName;` |
|     5744 |  6468 | `	if( !pInstr->iP1 ){` |
|     5524 |  6469 | `		pNos = &pTos[-1];` |
|        - |  6470 | `#ifdef UNTRUST` |
|        - |  6471 | `		if( pNos < pStack ){` |
|        - |  6472 | `			goto Abort;` |
|        - |  6473 | `		}` |
|        - |  6474 | `#endif` |
|     5524 |  6475 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6476 | `			ph7_class *pClass;` |
|        - |  6477 | `			/* Class already instantiated */` |
|     5522 |  6478 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  6479 | `			/* Point to the instantiated class */` |
|     5522 |  6480 | `			pClass = pThis->pClass;` |
|        - |  6481 | `			/* Extract attribute name first */` |
|     5522 |  6482 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     5522 |  6483 | `			if( pInstr->iP2 ){` |
|        - |  6484 | `				/* Method call */` |
|      576 |  6485 | `				ph7_class_method *pMeth = 0;` |
|      576 |  6486 | `				if( sName.nByte > 0 ){` |
|        - |  6487 | `					/* Extract the target method */` |
|      576 |  6488 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      287 |  6489 | `				}` |
|      576 |  6490 | `				if( pMeth == 0 ){` |
|      ! 0 |  6491 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  6492 | `						&pClass->sName,&sName` |
|        - |  6493 | `						);` |
|        - |  6494 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  6495 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  6496 | `					/* Pop the method name from the stack */` |
|      ! 0 |  6497 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6498 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  6499 | `				}else{` |
|        - |  6500 | `					/* Push method name on the stack */` |
|      576 |  6501 | `					PH7_MemObjRelease(pTos);` |
|      576 |  6502 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      576 |  6503 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6504 | `				}` |
|      576 |  6505 | `				pTos->nIdx = SXU32_HIGH;` |
|      289 |  6506 | `			}else{` |
|        - |  6507 | `				/* Attribute access */` |
|     4948 |  6508 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  6509 | `				SyHashEntry *pEntry;` |
|        - |  6510 | `				/* Extract the target attribute */` |
|     4948 |  6511 | `				if( sName.nByte > 0 ){` |
|     4948 |  6512 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     4948 |  6513 | `					if( pEntry ){` |
|        - |  6514 | `						/* Point to the attribute value */` |
|     4946 |  6515 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     2472 |  6516 | `					}` |
|     2473 |  6517 | `				}` |
|     4948 |  6518 | `				if( pObjAttr == 0 ){` |
|        - |  6519 | `					/* No such attribute,load null */` |
|        4 |  6520 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  6521 | `						&pClass->sName,&sName);` |
|        - |  6522 | `					/* Call the __get magic method if available */` |
|        3 |  6523 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  6524 | `				}` |
|     4948 |  6525 | `				VmPopOperand(&pTos,1);` |
|        - |  6526 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  6527 | `				 * This is due to the following case:` |
|        - |  6528 | `				 *     (new TestClass())->foo;` |
|        - |  6529 | `				 */` |
|     4948 |  6530 | `				pThis->iRef++;` |
|     4948 |  6531 | `				PH7_MemObjRelease(pTos);` |
|     4948 |  6532 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     4948 |  6533 | `				if( pObjAttr ){` |
|     4946 |  6534 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  6535 | `					/* Check attribute access */` |
|     4946 |  6536 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  6537 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  6538 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  6539 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  6540 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  6541 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     4944 |  6542 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     2491 |  6543 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       36 |  6544 | `							VmInstr *pNext = pInstr + 1;` |
|       36 |  6545 | `							int bIsLhs = 0;` |
|       36 |  6546 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       34 |  6547 | `								bIsLhs = 1;` |
|       16 |  6548 | `							}` |
|       36 |  6549 | `							if( !bIsLhs ){` |
|        3 |  6550 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  6551 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  6552 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  6553 | `									goto Abort;` |
|        - |  6554 | `								}` |
|        - |  6555 | `								{` |
|        3 |  6556 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6557 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6558 | `										pc = pFrm2->iExceptionJump - 1;` |
|     2871 |  6559 | `										break;` |
|        - |  6560 | `									}` |
|        - |  6561 | `								}` |
|      ! 0 |  6562 | `								goto Exception;` |
|        - |  6563 | `							}` |
|       16 |  6564 | `						}` |
|        - |  6565 | `						/* Load attribute */` |
|     4944 |  6566 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     4944 |  6567 | `						if( pValue ){` |
|     4944 |  6568 | `							if( pThis->iRef < 2 ){` |
|        - |  6569 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  6570 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  6571 | `								 */` |
|        7 |  6572 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  6573 | `							}else{` |
|        - |  6574 | `								/* Simple load */` |
|     4938 |  6575 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  6576 | `							}` |
|     4944 |  6577 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     4942 |  6578 | `								if( pThis->iRef > 1 ){` |
|        - |  6579 | `									/* Load attribute index */` |
|     4936 |  6580 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     2467 |  6581 | `								}` |
|     2470 |  6582 | `							}` |
|     2471 |  6583 | `						}` |
|     2473 |  6584 | `					}else{` |
|        - |  6585 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  6586 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  6587 | `						char zMsg[256];` |
|      ! 0 |  6588 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  6589 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6590 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6591 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  6592 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6593 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  6594 | `						goto Abort;` |
|        - |  6595 | `					}` |
|     2471 |  6596 | `				}` |
|        - |  6597 | `				/* Safely unreference the object */` |
|     4946 |  6598 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  6599 | `			}` |
|     2761 |  6600 | `		}else{` |
|        3 |  6601 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  6602 | `			VmPopOperand(&pTos,1);` |
|        3 |  6603 | `			PH7_MemObjRelease(pTos);` |
|        3 |  6604 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  6605 | `		}` |
|     2762 |  6606 | `	}else{` |
|        - |  6607 | `		/* Static member access using class name */` |
|      222 |  6608 | `		pNos = pTos;` |
|      222 |  6609 | `		pThis = 0;` |
|      222 |  6610 | `		if( !pInstr->p3 ){` |
|      188 |  6611 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      188 |  6612 | `			pNos--;` |
|        - |  6613 | `#ifdef UNTRUST` |
|        - |  6614 | `			if( pNos < pStack ){` |
|        - |  6615 | `				goto Abort;` |
|        - |  6616 | `			}` |
|        - |  6617 | `#endif` |
|       95 |  6618 | `		}else{` |
|        - |  6619 | `			/* Attribute name already computed */` |
|       36 |  6620 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6621 | `		}` |
|      222 |  6622 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      222 |  6623 | `			ph7_class *pClass = 0;` |
|      222 |  6624 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6625 | `				/* Class already instantiated */` |
|        5 |  6626 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  6627 | `				pClass = pThis->pClass;` |
|        5 |  6628 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  6629 | `			}else{` |
|        - |  6630 | `				/* Try to extract the target class */` |
|      218 |  6631 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      218 |  6632 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      218 |  6633 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  6634 | `					/* Handle self/static/parent keywords */` |
|      218 |  6635 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  6636 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  6637 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  6638 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  6639 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  6640 | `						}` |
|      188 |  6641 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  6642 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      157 |  6643 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       26 |  6644 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       26 |  6645 | `						if( pSelf && pSelf->pBase ){` |
|       26 |  6646 | `							pClass = pSelf->pBase;` |
|       12 |  6647 | `						}` |
|       14 |  6648 | `					}else{` |
|      108 |  6649 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  6650 | `					}` |
|      108 |  6651 | `				}` |
|        - |  6652 | `			}` |
|      222 |  6653 | `			if( pClass == 0 ){` |
|        - |  6654 | `				/* Undefined class */` |
|      ! 0 |  6655 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  6656 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  6657 | `					);` |
|      ! 0 |  6658 | `				if( !pInstr->p3 ){` |
|      ! 0 |  6659 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  6660 | `				}` |
|      ! 0 |  6661 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  6662 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  6663 | `			}else{` |
|      222 |  6664 | `				if( pInstr->iP2 ){` |
|        - |  6665 | `					/* Method call */` |
|       84 |  6666 | `					ph7_class_method *pMeth = 0;` |
|       84 |  6667 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  6668 | `						/* Extract the target method */` |
|       84 |  6669 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       41 |  6670 | `					}` |
|       84 |  6671 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  6672 | `						if( pMeth ){` |
|      ! 0 |  6673 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  6674 | `								&pClass->sName,&sName` |
|        - |  6675 | `								);` |
|      ! 0 |  6676 | `						}else{` |
|      ! 0 |  6677 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6678 | `								&pClass->sName,&sName` |
|        - |  6679 | `								);` |
|        - |  6680 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  6681 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  6682 | `						}` |
|        - |  6683 | `						/* Pop the method name from the stack */` |
|      ! 0 |  6684 | `						if( !pInstr->p3 ){` |
|      ! 0 |  6685 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  6686 | `						}` |
|      ! 0 |  6687 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  6688 | `					}else{` |
|        - |  6689 | `						/* Push method name on the stack */` |
|       84 |  6690 | `						PH7_MemObjRelease(pTos);` |
|       84 |  6691 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       84 |  6692 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  6693 | `					}` |
|       84 |  6694 | `					pTos->nIdx = SXU32_HIGH;` |
|       43 |  6695 | `				}else{` |
|        - |  6696 | `					/* Attribute access */` |
|      140 |  6697 | `					ph7_class_attr *pAttr = 0;` |
|        - |  6698 | `					/* Check for special ::class pseudo-constant */` |
|      186 |  6699 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  6700 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  6701 | `						/* ::class returns the fully qualified class name */` |
|        - |  6702 | `						/* Pop the attribute name from the stack */` |
|       60 |  6703 | `						if( !pInstr->p3 ){` |
|       60 |  6704 | `							VmPopOperand(&pTos,1);` |
|       29 |  6705 | `						}` |
|       60 |  6706 | `						PH7_MemObjRelease(pTos);` |
|        - |  6707 | `						/* Load the class name */` |
|       60 |  6708 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  6709 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  6710 | `					}else{` |
|        - |  6711 | `						/* Extract the target attribute */` |
|       82 |  6712 | `						if( sName.nByte > 0 ){` |
|       82 |  6713 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       40 |  6714 | `						}` |
|       82 |  6715 | `						if( pAttr == 0 ){` |
|        - |  6716 | `							/* No such attribute,load null */` |
|      ! 0 |  6717 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6718 | `								&pClass->sName,&sName);` |
|        - |  6719 | `							/* Call the __get magic method if available */` |
|      ! 0 |  6720 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  6721 | `						}` |
|        - |  6722 | `						/* Pop the attribute name from the stack */` |
|       82 |  6723 | `						if( !pInstr->p3 ){` |
|       48 |  6724 | `							VmPopOperand(&pTos,1);` |
|       23 |  6725 | `						}` |
|       82 |  6726 | `						PH7_MemObjRelease(pTos);` |
|       82 |  6727 | `						pTos->nIdx = SXU32_HIGH;` |
|       82 |  6728 | `						if( pAttr ){` |
|       82 |  6729 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  6730 | `								/* Access to a non static attribute */` |
|      ! 0 |  6731 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  6732 | `									&pClass->sName,&pAttr->sName` |
|        - |  6733 | `									);` |
|      ! 0 |  6734 | `							}else{` |
|        - |  6735 | `								ph7_value *pValue;` |
|        - |  6736 | `								/* Check if the access to the attribute is allowed */` |
|       82 |  6737 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  6738 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  6739 | `									 * Same LHS-of-store peek as the instance path. */` |
|       76 |  6740 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       51 |  6741 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       35 |  6742 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       22 |  6743 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       24 |  6744 | `										if( pS ){` |
|       24 |  6745 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       24 |  6746 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        5 |  6747 | `												VmInstr *pNext = pInstr + 1;` |
|        5 |  6748 | `												int bIsLhs = 0;` |
|        5 |  6749 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        3 |  6750 | `													bIsLhs = 1;` |
|        1 |  6751 | `												}` |
|        5 |  6752 | `												if( !bIsLhs ){` |
|        3 |  6753 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  6754 | `													if( pThis ){` |
|      ! 0 |  6755 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  6756 | `													}` |
|        3 |  6757 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  6758 | `														goto Abort;` |
|        - |  6759 | `													}` |
|        - |  6760 | `													{` |
|        3 |  6761 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  6762 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  6763 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  6764 | `															break;` |
|        - |  6765 | `														}` |
|        - |  6766 | `													}` |
|      ! 0 |  6767 | `													goto Exception;` |
|        - |  6768 | `												}` |
|        1 |  6769 | `											}` |
|       10 |  6770 | `										}` |
|       10 |  6771 | `									}` |
|        - |  6772 | `									/* Load the desired attribute */` |
|       76 |  6773 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       76 |  6774 | `									if( pValue ){` |
|       76 |  6775 | `										PH7_MemObjLoad(pValue,pTos);` |
|       76 |  6776 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  6777 | `											/* Load index number */` |
|       34 |  6778 | `											pTos->nIdx = pAttr->nIdx;` |
|       16 |  6779 | `										}` |
|       37 |  6780 | `									}` |
|       39 |  6781 | `								}else{` |
|        - |  6782 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  6783 | `									char zMsg[256];` |
|        5 |  6784 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  6785 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  6786 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  6787 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  6788 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  6789 | `									}else{` |
|      ! 0 |  6790 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  6791 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  6792 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  6793 | `									}` |
|        5 |  6794 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  6795 | `									goto Abort;` |
|        - |  6796 | `								}` |
|        - |  6797 | `							}` |
|       37 |  6798 | `						}` |
|        - |  6799 | `					}` |
|        - |  6800 | `				}` |
|      216 |  6801 | `				if( pThis ){` |
|        - |  6802 | `					/* Safely unreference the object */` |
|        5 |  6803 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  6804 | `				}` |
|        - |  6805 | `			}` |
|      109 |  6806 | `		}else{` |
|        - |  6807 | `			/* Pop operands */` |
|      ! 0 |  6808 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  6809 | `			if( !pInstr->p3 ){` |
|      ! 0 |  6810 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  6811 | `			}` |
|      ! 0 |  6812 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6813 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6814 | `		}` |
|        - |  6815 | `	}` |
|     5736 |  6816 | `	break;` |
|        - |  6817 | `					}` |
|        - |  6818 | `/*` |
|        - |  6819 | ` * OP_NEW P1 * * *` |
|        - |  6820 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  6821 | ` */` |
|      462 |  6822 | `case PH7_OP_NEW: {` |
|      926 |  6823 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      926 |  6824 | `	ph7_class *pClass = 0;` |
|        - |  6825 | `	ph7_class_instance *pNew;` |
|      926 |  6826 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  6827 | `		/* Try to extract the desired class */` |
|     1388 |  6828 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      924 |  6829 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      462 |  6830 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  6831 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  6832 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  6833 | `	}` |
|      926 |  6834 | `	if( pClass == 0 ){` |
|        - |  6835 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  6836 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  6837 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  6838 | `			);` |
|        - |  6839 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  6840 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6841 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6842 | `			/* Pop given arguments */` |
|      ! 0 |  6843 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6844 | `		}` |
|      ! 0 |  6845 | `		goto Abort;` |
|      ! 0 |  6846 | `	}else{` |
|        - |  6847 | `		ph7_class_method *pCons;` |
|        - |  6848 | `		/* Create a new class instance */` |
|      926 |  6849 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      926 |  6850 | `		if( pNew == 0 ){` |
|      ! 0 |  6851 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6852 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  6853 | `				&pClass->sName` |
|        - |  6854 | `			);` |
|      ! 0 |  6855 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  6856 | `			if( pInstr->iP1 > 0 ){` |
|        - |  6857 | `				/* Pop given arguments */` |
|      ! 0 |  6858 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6859 | `			}` |
|      ! 0 |  6860 | `			break;` |
|        - |  6861 | `		}` |
|        - |  6862 | `		/* Check if a constructor is available */` |
|      926 |  6863 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      926 |  6864 | `		if( pCons == 0 ){` |
|      750 |  6865 | `			SyString *pName = &pClass->sName;` |
|        - |  6866 | `			/* Check for a constructor with the same base class name */` |
|      750 |  6867 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      374 |  6868 | `		}` |
|      926 |  6869 | `		if( pCons ){` |
|        - |  6870 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  6871 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  6872 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  6873 | `			 * (including variadic string-key packing). */` |
|      178 |  6874 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      178 |  6875 | `			SySetReset(&aArg);` |
|      358 |  6876 | `			while( pArg < pTos ){` |
|      182 |  6877 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      182 |  6878 | `				pArg++;` |
|        2 |  6879 | `			}` |
|      178 |  6880 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  6881 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  6882 | `				sxu32 n;` |
|       57 |  6883 | `				n = SySetUsed(&aArg);` |
|        - |  6884 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  6885 | `				 * for named args the missing-arg check happens downstream` |
|        - |  6886 | `				 * after resolution). */` |
|      101 |  6887 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       45 |  6888 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       45 |  6889 | `					if( pFuncArg ){` |
|       45 |  6890 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  6891 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  6892 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  6893 | `						}` |
|       22 |  6894 | `					}` |
|       45 |  6895 | `					n++;` |
|        1 |  6896 | `				}` |
|       28 |  6897 | `			}` |
|      178 |  6898 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  6899 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      178 |  6900 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  6901 | `				pNew->iRef = 1;` |
|      ! 0 |  6902 | `			}` |
|       88 |  6903 | `		}` |
|      926 |  6904 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6905 | `			/* Pop given arguments */` |
|      156 |  6906 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       77 |  6907 | `		}` |
|      926 |  6908 | `		PH7_MemObjRelease(pTos);` |
|      926 |  6909 | `		pTos->x.pOther = pNew;` |
|      926 |  6910 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6911 | `	}` |
|      926 |  6912 | `	break;` |
|        - |  6913 | `				 }` |
|        - |  6914 | `/*` |
|        - |  6915 | ` * OP_CLONE * * *` |
|        - |  6916 | ` * Perfome a clone operation.` |
|        - |  6917 | ` */` |
|       24 |  6918 | `case PH7_OP_CLONE: {` |
|        - |  6919 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  6920 | `#ifdef UNTRUST` |
|        - |  6921 | `	if( pTos < pStack ){` |
|        - |  6922 | `		goto Abort;` |
|        - |  6923 | `	}` |
|        - |  6924 | `#endif` |
|        - |  6925 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  6926 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  6927 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6928 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  6929 | `		PH7_MemObjRelease(pTos);` |
|        5 |  6930 | `		break;` |
|        - |  6931 | `	}` |
|        - |  6932 | `	/* Point to the source */` |
|       46 |  6933 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6934 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  6935 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  6936 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6937 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  6938 | `			&pSrc->pClass->sName);` |
|      ! 0 |  6939 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6940 | `		break;` |
|        - |  6941 | `	}` |
|        - |  6942 | `	/* Perform the clone operation */` |
|       46 |  6943 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  6944 | `	PH7_MemObjRelease(pTos);` |
|       46 |  6945 | `	if( pClone == 0 ){` |
|      ! 0 |  6946 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6947 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  6948 | `	}else{` |
|        - |  6949 | `		/* Load the cloned object */` |
|       46 |  6950 | `		pTos->x.pOther = pClone;` |
|       46 |  6951 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  6952 | `	}` |
|       46 |  6953 | `	break;` |
|        - |  6954 | `				   }` |
|        - |  6955 | `/*` |
|        - |  6956 | ` * OP_SWITCH * * P3` |
|        - |  6957 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  6958 | ` */` |
|       26 |  6959 | `case PH7_OP_SWITCH: {` |
|       54 |  6960 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  6961 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  6962 | `	ph7_value sValue,sCaseValue;` |
|        - |  6963 | `	sxu32 n,nEntry;` |
|        - |  6964 | `#ifdef UNTRUST` |
|        - |  6965 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  6966 | `		goto Abort;` |
|        - |  6967 | `	}` |
|        - |  6968 | `#endif` |
|        - |  6969 | `	/* Point to the case table  */` |
|       54 |  6970 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  6971 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  6972 | `	/* Select the appropriate case block to execute */` |
|       54 |  6973 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  6974 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  6975 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  6976 | `		pCase = &aCase[n];` |
|      130 |  6977 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  6978 | `		/* Execute the case expression first */` |
|      130 |  6979 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  6980 | `		/* Compare the two expression */` |
|      130 |  6981 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  6982 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  6983 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  6984 | `		if( rc == 0 ){` |
|        - |  6985 | `			/* Value match,jump to this block */` |
|       52 |  6986 | `			pc = pCase->nStart - 1;` |
|       52 |  6987 | `			break;` |
|        - |  6988 | `		}` |
|       41 |  6989 | `	}` |
|       54 |  6990 | `	VmPopOperand(&pTos,1);` |
|       54 |  6991 | `	if( n >= nEntry ){` |
|        - |  6992 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  6993 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  6994 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  6995 | `		}else{` |
|        - |  6996 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  6997 | `			pc = pSwitch->nOut - 1;` |
|        - |  6998 | `		}` |
|        1 |  6999 | `	}` |
|       54 |  7000 | `	break;` |
|        - |  7001 | `					}` |
|        - |  7002 | `/*` |
|        - |  7003 | ` * OP_MATCH * * P3` |
|        - |  7004 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  7005 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  7006 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  7007 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  7008 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  7009 | ` */` |
|       52 |  7010 | `case PH7_OP_MATCH: {` |
|      106 |  7011 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      106 |  7012 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  7013 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  7014 | `	sxu32 i,j,nArm,nCond;` |
|      106 |  7015 | `	int matched = 0;` |
|        - |  7016 | `#ifdef UNTRUST` |
|        - |  7017 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  7018 | `		goto Abort;` |
|        - |  7019 | `	}` |
|        - |  7020 | `#endif` |
|      106 |  7021 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      106 |  7022 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      106 |  7023 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      106 |  7024 | `	PH7_MemObjInit(pVm,&sCond);` |
|      106 |  7025 | `	PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7026 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      338 |  7027 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      234 |  7028 | `		pArm = &aArm[i];` |
|      234 |  7029 | `		if( pArm->bDefault ){` |
|       11 |  7030 | `			pDefault = pArm;` |
|       11 |  7031 | `			continue;` |
|        - |  7032 | `		}` |
|      224 |  7033 | `		nCond = SySetUsed(&pArm->aConds);` |
|      388 |  7034 | `		for( j = 0; j < nCond; ++j ){` |
|      256 |  7035 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      256 |  7036 | `			if( pCondBc == 0 ){` |
|      ! 0 |  7037 | `				continue;` |
|        - |  7038 | `			}` |
|      256 |  7039 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      256 |  7040 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      256 |  7041 | `			PH7_MemObjRelease(&sCond);` |
|      256 |  7042 | `			if( rc == 0 ){` |
|       91 |  7043 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       91 |  7044 | `				matched = 1;` |
|       91 |  7045 | `				break;` |
|        - |  7046 | `			}` |
|       84 |  7047 | `		}` |
|      113 |  7048 | `	}` |
|      106 |  7049 | `	if( !matched && pDefault ){` |
|       11 |  7050 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       11 |  7051 | `		matched = 1;` |
|        5 |  7052 | `	}` |
|      106 |  7053 | `	if( !matched ){` |
|        5 |  7054 | `		const char *zType = "unknown";` |
|        - |  7055 | `		char zMsg[128];` |
|        - |  7056 | `		sxu32 nMsg;` |
|        5 |  7057 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  7058 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  7059 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  7060 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  7061 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  7062 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  7063 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  7064 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  7065 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  7066 | `		default: break;` |
|        - |  7067 | `		}` |
|        7 |  7068 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  7069 | `			"Unhandled match case of type %s",zType);` |
|        7 |  7070 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  7071 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  7072 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  7073 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  7074 | `		goto Abort;` |
|        - |  7075 | `	}` |
|      101 |  7076 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  7077 | `	/* Replace subject on TOS with the arm result */` |
|      101 |  7078 | `	PH7_MemObjStore(&sResult,pTos);` |
|      101 |  7079 | `	PH7_MemObjRelease(&sResult);` |
|      101 |  7080 | `	break;` |
|        - |  7081 | `					}` |
|        - |  7082 | `/*` |
|        - |  7083 | ` * OP_YIELD P1 P2 *` |
|        - |  7084 | ` *  Yield a value from a generator function.` |
|        - |  7085 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  7086 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  7087 | ` */` |
|       34 |  7088 | `case PH7_OP_YIELD: {` |
|        - |  7089 | `	ph7_generator *pGen;` |
|       70 |  7090 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  7091 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  7092 | `		goto Abort;` |
|        - |  7093 | `	}` |
|       70 |  7094 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  7095 | `	if( pInstr->iP2 ){` |
|        - |  7096 | `		/* yield $key => $value: value on top, key below */` |
|        - |  7097 | `#ifdef UNTRUST` |
|        - |  7098 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  7099 | `#endif` |
|        7 |  7100 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7101 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7102 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7103 | `		VmPopOperand(&pTos, 1);` |
|        - |  7104 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7105 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7106 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7107 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7108 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7109 | `			}` |
|        1 |  7110 | `		}` |
|       67 |  7111 | `	}else if( pInstr->iP1 ){` |
|        - |  7112 | `		/* yield $value */` |
|        - |  7113 | `#ifdef UNTRUST` |
|        - |  7114 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7115 | `#endif` |
|       64 |  7116 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7117 | `		VmPopOperand(&pTos, 1);` |
|        - |  7118 | `		/* Auto-increment key */` |
|       64 |  7119 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7120 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7121 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7122 | `	}else{` |
|        - |  7123 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7124 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7125 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7126 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7127 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7128 | `	}` |
|        - |  7129 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7130 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7131 | `	goto Suspend;` |
|        - |  7132 |  |
|        - |  7133 | `/*` |
|        - |  7134 | ` * OP_CALL P1 * *` |
|        - |  7135 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7136 | ` *  function on the stack.` |
|        - |  7137 | ` */` |
|   325846 |  7138 | `case PH7_OP_CALL: {` |
|   651738 |  7139 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7140 | `	ph7_value *pArg;` |
|   651738 |  7141 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   651738 |  7142 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7143 | `	SyHashEntry *pEntry;` |
|        - |  7144 | `	SyString sName;` |
|        - |  7145 | `	/* Extract function name */` |
|   651738 |  7146 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  7147 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7148 | `			ph7_value sResult;` |
|      ! 0 |  7149 | `			SySetReset(&aArg);` |
|      ! 0 |  7150 | `			while( pArg < pTos ){` |
|      ! 0 |  7151 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7152 | `				pArg++;` |
|      ! 0 |  7153 | `			}` |
|      ! 0 |  7154 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7155 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7156 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7157 | `			SySetReset(&aArg);` |
|        - |  7158 | `			/* Pop given arguments */` |
|      ! 0 |  7159 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7160 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7161 | `			}` |
|        - |  7162 | `			/* Copy result */` |
|      ! 0 |  7163 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7164 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  7165 | `		}else{` |
|        3 |  7166 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  7167 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7168 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  7169 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  7170 | `			}else{` |
|        - |  7171 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  7172 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7173 | `			}` |
|        - |  7174 | `			/* Pop given arguments */` |
|        3 |  7175 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7176 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7177 | `			}` |
|        - |  7178 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7179 | `			PH7_MemObjRelease(pTos);` |
|        - |  7180 | `		}` |
|   325565 |  7181 | `		break;` |
|        - |  7182 | `	}` |
|   651736 |  7183 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7184 | `	/* Check for a compiled function first.` |
|        - |  7185 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7186 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   651736 |  7187 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7188 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7189 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7190 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7191 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7192 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7193 | `	{` |
|   651736 |  7194 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   651736 |  7195 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7196 | `		const char *zFunc;` |
|        - |  7197 | `		const char *zEnd;` |
|        - |  7198 | `		const char *z;` |
|        - |  7199 | `		SyString sGlobal;` |
|       20 |  7200 | `		zFunc = sName.zString;` |
|       20 |  7201 | `		zEnd  = zFunc + sName.nByte;` |
|       20 |  7202 | `		z = zEnd;` |
|        - |  7203 | `		/* Find last namespace separator */` |
|      174 |  7204 | `		while( z > zFunc ){` |
|      174 |  7205 | `			if( z[-1] == '\\' ){` |
|       20 |  7206 | `				break;` |
|        - |  7207 | `			}` |
|      156 |  7208 | `			z--;` |
|        2 |  7209 | `		}` |
|       20 |  7210 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7211 | `			/* Retry lookup using the unqualified/global function name */` |
|       20 |  7212 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       20 |  7213 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        9 |  7214 | `		}` |
|        9 |  7215 | `	}` |
|        - |  7216 | `	} /* end VmCallArgMap namespace scope */` |
|   651736 |  7217 | `	if( pEntry ){` |
|        - |  7218 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7219 | `		ph7_class_instance *pThis;` |
|        - |  7220 | `		ph7_value *pFrameStack;` |
|        - |  7221 | `		ph7_vm_func *pVmFunc;` |
|        - |  7222 | `		ph7_class *pSelf;` |
|        - |  7223 | `		VmFrame *pFrame;` |
|        - |  7224 | `		ph7_value *pObj;` |
|        - |  7225 | `		VmSlot sArg;` |
|        - |  7226 | `		sxu32 n;` |
|        - |  7227 | `		/* initialize fields */` |
|    15620 |  7228 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    15620 |  7229 | `		pThis = 0;` |
|    15620 |  7230 | `		pSelf = 0;` |
|    15620 |  7231 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7232 | `			ph7_class_method *pMeth;` |
|        - |  7233 | `			/* Class method call */` |
|     2342 |  7234 | `			ph7_value *pTarget = &pTos[-1];` |
|     2342 |  7235 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7236 | `				/* Extract the 'this' pointer */` |
|     2342 |  7237 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7238 | `					/* Instance already loaded */` |
|     2254 |  7239 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2254 |  7240 | `					pThis->iRef++;` |
|     2254 |  7241 | `					pSelf = pThis->pClass;` |
|     1126 |  7242 | `				}` |
|     2342 |  7243 | `				if( pSelf == 0 ){` |
|       90 |  7244 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7245 | `						/* "Late Static Binding" class name */` |
|      125 |  7246 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       41 |  7247 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       41 |  7248 | `					}` |
|       90 |  7249 | `					if( pSelf == 0 ){` |
|       19 |  7250 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        9 |  7251 | `					}` |
|       44 |  7252 | `				}` |
|     2342 |  7253 | `				if( pThis == 0  ){` |
|       90 |  7254 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       90 |  7255 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       90 |  7256 | `					if( pFrameLocal->pParent ){` |
|        - |  7257 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       64 |  7258 | `						pThis = pFrameLocal->pThis;` |
|       64 |  7259 | `						if( pThis ){` |
|       19 |  7260 | `							pThis->iRef++;` |
|        9 |  7261 | `						}` |
|       31 |  7262 | `					}` |
|       44 |  7263 | `				}` |
|     2342 |  7264 | `				VmPopOperand(&pTos,1);` |
|     2342 |  7265 | `				PH7_MemObjRelease(pTos);` |
|        - |  7266 | `				/* Synchronize pointers */` |
|     2342 |  7267 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7268 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7269 | `				 * user have already computed the random generated unique class method name` |
|        - |  7270 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7271 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7272 | `				 */` |
|     2342 |  7273 | `				while( pArg < pStack ){` |
|      ! 0 |  7274 | `					pArg++;` |
|      ! 0 |  7275 | `				}` |
|     2342 |  7276 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7277 | `					/* Check if the call is allowed */` |
|     2342 |  7278 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2342 |  7279 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7280 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7281 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7282 | `							char zMsg[256];` |
|      ! 0 |  7283 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7284 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7285 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7286 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7287 | `							/* Pop given arguments */` |
|      ! 0 |  7288 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7289 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7290 | `							}` |
|      ! 0 |  7291 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7292 | `							goto Abort;` |
|        - |  7293 | `						}` |
|        6 |  7294 | `					}` |
|     1170 |  7295 | `				}` |
|     1170 |  7296 | `			}` |
|     1170 |  7297 | `		}` |
|        - |  7298 | `		/* Check The recursion limit */` |
|    15620 |  7299 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7300 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7301 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7302 | `				&pVmFunc->sName);` |
|        - |  7303 | `			/* Pop given arguments */` |
|        3 |  7304 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7305 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7306 | `			}` |
|        - |  7307 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7308 | `			PH7_MemObjRelease(pTos);` |
|       14 |  7309 | `			break;` |
|        - |  7310 | `		}` |
|    15618 |  7311 | `		if( pVmFunc->pNextName ){` |
|        - |  7312 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7313 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7314 | `		}` |
|    15618 |  7315 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7316 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7317 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7318 | `			ph7_generator *pGenerator;` |
|        - |  7319 | `			ph7_class_instance *pGenObj;` |
|        - |  7320 | `			ph7_value *pCtxAttr;` |
|        - |  7321 | `			SyString sAttrName;` |
|        - |  7322 | `			ph7_value **apCallArgs;` |
|        - |  7323 | `			int nGenArgs, iArg;` |
|        - |  7324 | `			/* Collect arguments from the operand stack */` |
|       24 |  7325 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  7326 | `			apCallArgs = 0;` |
|       24 |  7327 | `			if( nGenArgs > 0 ){` |
|       14 |  7328 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7329 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  7330 | `				if( apCallArgs == 0 ){` |
|        - |  7331 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7332 | `					nGenArgs = 0;` |
|      ! 0 |  7333 | `				}else{` |
|       10 |  7334 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  7335 | `					int didReorder = 0;` |
|       10 |  7336 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  7337 | `						/* Named-argument reordering for generator */` |
|        5 |  7338 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  7339 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  7340 | `						sxu32 nNV = nF;` |
|        5 |  7341 | `						sxi32 iVIdx = -1;` |
|        - |  7342 | `						sxi32 *aGSlot;` |
|        - |  7343 | `						sxu8 *aGUsed;` |
|        - |  7344 | `						sxu32 gi;` |
|       13 |  7345 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  7346 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  7347 | `						}` |
|        7 |  7348 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7349 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  7350 | `						if( aGSlot ){` |
|        5 |  7351 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  7352 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  7353 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  7354 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7355 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  7356 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7357 | `								goto Abort;` |
|        - |  7358 | `							}` |
|        - |  7359 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  7360 | `							 * append overflow (variadic / positional beyond` |
|        - |  7361 | `							 * formals) so downstream sees every argument. */` |
|        - |  7362 | `							{` |
|        5 |  7363 | `								int nOut = 0;` |
|       13 |  7364 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  7365 | `									sxu32 gj;` |
|       13 |  7366 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  7367 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  7368 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  7369 | `											break;` |
|        - |  7370 | `										}` |
|        3 |  7371 | `									}` |
|        5 |  7372 | `								}` |
|       13 |  7373 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  7374 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  7375 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  7376 | `									}` |
|        5 |  7377 | `								}` |
|        5 |  7378 | `								nGenArgs = nOut;` |
|        - |  7379 | `							}` |
|        5 |  7380 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  7381 | `							didReorder = 1;` |
|        2 |  7382 | `						}` |
|        - |  7383 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  7384 | `						 * positional fill below — preserves arg order rather` |
|        - |  7385 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  7386 | `					}` |
|       10 |  7387 | `					if( !didReorder ){` |
|       12 |  7388 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  7389 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  7390 | `						}` |
|        2 |  7391 | `					}` |
|        - |  7392 | `				}` |
|        4 |  7393 | `			}` |
|        - |  7394 | `			/* Create execution context and generator wrapper */` |
|       24 |  7395 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  7396 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  7397 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7398 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7399 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7400 | `				break;` |
|        - |  7401 | `			}` |
|       24 |  7402 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  7403 | `			if( pGenerator == 0 ){` |
|      ! 0 |  7404 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  7405 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  7406 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  7407 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  7408 | `				break;` |
|        - |  7409 | `			}` |
|        - |  7410 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  7411 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  7412 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  7413 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  7414 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  7415 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  7416 | `			if( apCallArgs ){` |
|       10 |  7417 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  7418 | `			}` |
|       24 |  7419 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  7420 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7421 | `				if( pThis ){` |
|      ! 0 |  7422 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7423 | `				}` |
|      ! 0 |  7424 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7425 | `					goto Abort;` |
|        - |  7426 | `				}` |
|      ! 0 |  7427 | `				break;` |
|        - |  7428 | `			}` |
|        - |  7429 | `			/* Create Generator class instance */` |
|       24 |  7430 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  7431 | `			if( pGenObj == 0 ){` |
|      ! 0 |  7432 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  7433 | `				break;` |
|        - |  7434 | `			}` |
|        - |  7435 | `			/* Store generator in __ctx attribute */` |
|       24 |  7436 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  7437 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  7438 | `			if( pCtxAttr ){` |
|       24 |  7439 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  7440 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  7441 | `			}` |
|        - |  7442 | `			/* Pop args and function name, push Generator object */` |
|       24 |  7443 | `			PH7_MemObjRelease(pTos);` |
|       24 |  7444 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  7445 | `			pTos->x.pOther = pGenObj;` |
|       24 |  7446 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  7447 | `			pGenObj->iRef++;` |
|       24 |  7448 | `			if( pThis ){` |
|      ! 0 |  7449 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7450 | `			}` |
|       24 |  7451 | `			break;` |
|        - |  7452 | `		}` |
|        - |  7453 | `		/* Extract the formal argument set */` |
|    15596 |  7454 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  7455 | `		/* Create a new VM frame  */` |
|    15596 |  7456 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    15596 |  7457 | `		if( rc != SXRET_OK ){` |
|        - |  7458 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  7459 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7460 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  7461 | `				&pVmFunc->sName);` |
|        - |  7462 | `			/* Pop given arguments */` |
|      ! 0 |  7463 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7464 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7465 | `			}` |
|        - |  7466 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7467 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7468 | `			break;` |
|        - |  7469 | `		}` |
|    15596 |  7470 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  7471 | `			/* Install the '$this' variable */` |
|        - |  7472 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2270 |  7473 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2270 |  7474 | `			if( pObj ){` |
|        - |  7475 | `				/* Reflect the change */` |
|     2270 |  7476 | `				pObj->x.pOther = pThis;` |
|     2270 |  7477 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1134 |  7478 | `			}` |
|     1134 |  7479 | `		}` |
|    15596 |  7480 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  7481 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  7482 | `			/* Install static variables */` |
|      ! 0 |  7483 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  7484 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  7485 | `				pStatic = &aStatic[n];` |
|      ! 0 |  7486 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  7487 | `					/* Initialize the static variables */` |
|      ! 0 |  7488 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  7489 | `					if( pObj ){` |
|        - |  7490 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  7491 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  7492 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  7493 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  7494 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  7495 | `						}` |
|      ! 0 |  7496 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  7497 | `					}else{` |
|      ! 0 |  7498 | `						continue;` |
|        - |  7499 | `					}` |
|      ! 0 |  7500 | `				}` |
|        - |  7501 | `				/* Install in the current frame */` |
|      ! 0 |  7502 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  7503 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  7504 | `			}` |
|      ! 0 |  7505 | `		}` |
|        - |  7506 | `		/* Push arguments in the local frame */` |
|        - |  7507 | `		{` |
|    15596 |  7508 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|    15596 |  7509 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  7510 | `			/* ============================================================` |
|        - |  7511 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  7512 | `			 *` |
|        - |  7513 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  7514 | `			 * or position, then install them in the frame.` |
|        - |  7515 | `			 * ============================================================ */` |
|       90 |  7516 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       90 |  7517 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       90 |  7518 | `			sxi32 iVariadicIdx = -1;` |
|        - |  7519 | `			sxu32 nNonVariadic;` |
|        - |  7520 | `			sxi32 *aSlot;` |
|        - |  7521 | `			sxu8  *aUsed;` |
|        - |  7522 | `			sxu32 i;` |
|        - |  7523 | `			/* Find variadic parameter index */` |
|      274 |  7524 | `			for( i = 0; i < nFormal; i++ ){` |
|      194 |  7525 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  7526 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  7527 | `					break;` |
|        - |  7528 | `				}` |
|       94 |  7529 | `			}` |
|       90 |  7530 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  7531 | `			/* Allocate mapping arrays */` |
|      134 |  7532 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       88 |  7533 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       90 |  7534 | `			if( aSlot == 0 ){` |
|      ! 0 |  7535 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  7536 | `				goto Abort;` |
|        - |  7537 | `			}` |
|       90 |  7538 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  7539 | `			/* Resolve named arguments to formal parameters */` |
|      134 |  7540 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       44 |  7541 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       90 |  7542 | `			if( rc == PH7_ABORT ){` |
|        7 |  7543 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  7544 | `				goto Abort;` |
|        - |  7545 | `			}` |
|        - |  7546 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      257 |  7547 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  7548 | `				/* Find the stack arg mapped to formal n */` |
|      175 |  7549 | `				sxi32 iSrc = -1;` |
|      291 |  7550 | `				for( i = 0; i < nActual; i++ ){` |
|      273 |  7551 | `					if( aSlot[i] == (sxi32)n ){` |
|      157 |  7552 | `						iSrc = (sxi32)i;` |
|      157 |  7553 | `						break;` |
|        - |  7554 | `					}` |
|       59 |  7555 | `				}` |
|      175 |  7556 | `				if( iSrc >= 0 ){` |
|        - |  7557 | `					/* Argument was provided — install with type checking */` |
|      157 |  7558 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  7559 | `					/* NULL-to-default redirect (existing behavior) */` |
|      156 |  7560 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  7561 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  7562 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  7563 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7564 | `					}` |
|        - |  7565 | `					/* Type checking: union types */` |
|      157 |  7566 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  7567 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  7568 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       13 |  7569 | `						if( rcU != SXRET_OK ){` |
|        - |  7570 | `							const char *zGiven;` |
|        - |  7571 | `							char zBuf[128];` |
|      ! 0 |  7572 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7573 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  7574 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7575 | `								zGiven = "null";` |
|      ! 0 |  7576 | `							}else{` |
|      ! 0 |  7577 | `								zGiven = ph7_type_name(pVal);` |
|        - |  7578 | `							}` |
|      ! 0 |  7579 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7580 | `								&aFormalArg[n].sName,` |
|      ! 0 |  7581 | `								SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|      ! 0 |  7582 | `									? aFormalArg[n].sTypeName.zString : "union",` |
|      ! 0 |  7583 | `								zGiven);` |
|      ! 0 |  7584 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7585 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7586 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  7587 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7588 | `							pFrameStack = 0;` |
|      ! 0 |  7589 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  7590 | `							goto SkipFuncBody;` |
|        - |  7591 | `						}` |
|      159 |  7592 | `					}else if( aFormalArg[n].nType > 0` |
|       85 |  7593 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  7594 | `						/* Scalar/class type checking */` |
|       17 |  7595 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  7596 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  7597 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  7598 | `							if( pClass ){` |
|      ! 0 |  7599 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7600 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7601 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7602 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7603 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7604 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  7605 | `									}` |
|      ! 0 |  7606 | `								}else{` |
|      ! 0 |  7607 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  7608 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  7609 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7610 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7611 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7612 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  7613 | `									}` |
|        - |  7614 | `								}` |
|      ! 0 |  7615 | `							}` |
|       17 |  7616 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  7617 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  7618 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7619 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  7620 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  7621 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  7622 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  7623 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7624 | `								pFrameStack = 0;` |
|      ! 0 |  7625 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  7626 | `								goto SkipFuncBody;` |
|      ! 0 |  7627 | `							}else{` |
|        7 |  7628 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        7 |  7629 | `								if( xCast ) xCast(pVal);` |
|        - |  7630 | `							}` |
|        3 |  7631 | `						}` |
|        8 |  7632 | `					}` |
|        - |  7633 | `					/* Install: by reference or by value */` |
|      157 |  7634 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  7635 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  7636 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  7637 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7638 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7639 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7640 | `							}` |
|      ! 0 |  7641 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7642 | `						}else{` |
|        7 |  7643 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  7644 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  7645 | `							if( pRefEntry == 0 ){` |
|        7 |  7646 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  7647 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  7648 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  7649 | `								sArg.pUserData = 0;` |
|        5 |  7650 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  7651 | `							}` |
|        5 |  7652 | `							pObj = 0;` |
|        - |  7653 | `						}` |
|        3 |  7654 | `					}else{` |
|      153 |  7655 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7656 | `					}` |
|      157 |  7657 | `					if( pObj ){` |
|      153 |  7658 | `						PH7_MemObjStore(pVal,pObj);` |
|      153 |  7659 | `						sArg.nIdx = pObj->nIdx;` |
|      153 |  7660 | `						sArg.pUserData = 0;` |
|      153 |  7661 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       76 |  7662 | `					}` |
|       79 |  7663 | `				}else{` |
|        - |  7664 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  7665 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7666 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  7667 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  7668 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  7669 | `						if( pObj ){` |
|       19 |  7670 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  7671 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  7672 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  7673 | `							sArg.pUserData = 0;` |
|       19 |  7674 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  7675 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  7676 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  7677 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  7678 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  7679 | `							}` |
|        9 |  7680 | `						}` |
|        9 |  7681 | `					}` |
|        - |  7682 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  7683 | `				}` |
|       88 |  7684 | `			}` |
|        - |  7685 | `			/* Handle variadic parameter */` |
|       83 |  7686 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  7687 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  7688 | `				if( pObj ){` |
|        9 |  7689 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7690 | `					{` |
|        9 |  7691 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  7692 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  7693 | `							if( aSlot[i] == -1 ){` |
|       16 |  7694 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  7695 | `									/* Named variadic entry: insert with string key */` |
|        - |  7696 | `									ph7_value sKey;` |
|       11 |  7697 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  7698 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  7699 | `										pCallMap3->aNames[i].zString,` |
|       10 |  7700 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  7701 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  7702 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  7703 | `								}else{` |
|        - |  7704 | `									/* Positional variadic entry */` |
|      ! 0 |  7705 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  7706 | `								}` |
|        5 |  7707 | `							}` |
|       12 |  7708 | `						}` |
|        - |  7709 | `					}` |
|        9 |  7710 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  7711 | `					sArg.pUserData = 0;` |
|        9 |  7712 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  7713 | `				}` |
|        5 |  7714 | `			}else{` |
|        - |  7715 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  7716 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  7717 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  7718 | `				 * the positional-only path's behavior. */` |
|       75 |  7719 | `				sxu32 nAnon = nNonVariadic;` |
|      219 |  7720 | `				for( i = 0; i < nActual; i++ ){` |
|      145 |  7721 | `					if( aSlot[i] == -2 ){` |
|        - |  7722 | `						char zAnonBuf[32];` |
|        - |  7723 | `						SyString sAnonName;` |
|      ! 0 |  7724 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  7725 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  7726 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  7727 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  7728 | `						if( pObj ){` |
|      ! 0 |  7729 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  7730 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  7731 | `							sArg.pUserData = 0;` |
|      ! 0 |  7732 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  7733 | `						}` |
|      ! 0 |  7734 | `						nAnon++;` |
|      ! 0 |  7735 | `					}` |
|       73 |  7736 | `				}` |
|        - |  7737 | `			}` |
|        - |  7738 | `			/* Release all stack arguments */` |
|      249 |  7739 | `			for( i = 0; i < nActual; i++ ){` |
|      167 |  7740 | `				PH7_MemObjRelease(&pArg[i]);` |
|       84 |  7741 | `			}` |
|       83 |  7742 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  7743 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       83 |  7744 | `			n = nFormal;` |
|       42 |  7745 | `		}else{` |
|        - |  7746 | `		/* ============================================================` |
|        - |  7747 | `		 * Positional-only matching path (original)` |
|        - |  7748 | `		 * ============================================================ */` |
|    15508 |  7749 | `		n = 0;` |
|    41740 |  7750 | `		while( pArg < pTos ){` |
|    26296 |  7751 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  7752 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       36 |  7753 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       36 |  7754 | `				if( pObj ){` |
|        - |  7755 | `					/* Initialize as empty array */` |
|       36 |  7756 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  7757 | `					{` |
|       36 |  7758 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      136 |  7759 | `						while( pArg < pTos ){` |
|        - |  7760 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  7761 | `							 *` |
|        - |  7762 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  7763 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  7764 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  7765 | `							 * non-union variadic path below has the same limitation;` |
|        - |  7766 | `							 * fixing both wants a separate counter for elements` |
|        - |  7767 | `							 * already packed into the variadic array. */` |
|      104 |  7768 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  7769 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  7770 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       16 |  7771 | `								if( rcU != SXRET_OK ){` |
|        - |  7772 | `									const char *zGiven;` |
|        - |  7773 | `									char zBuf[128];` |
|        3 |  7774 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  7775 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  7776 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  7777 | `										zGiven = "null";` |
|      ! 0 |  7778 | `									}else{` |
|        3 |  7779 | `										zGiven = ph7_type_name(pArg);` |
|        - |  7780 | `									}` |
|        3 |  7781 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  7782 | `										&aFormalArg[n].sName,` |
|        2 |  7783 | `										SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|        2 |  7784 | `											? aFormalArg[n].sTypeName.zString : "union",` |
|        1 |  7785 | `										zGiven);` |
|        3 |  7786 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7787 | `										goto Abort;` |
|        - |  7788 | `									}` |
|        3 |  7789 | `									PH7_MemObjRelease(pTos);` |
|        3 |  7790 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  7791 | `									pFrameStack = 0;` |
|        3 |  7792 | `									rc = PH7_EXCEPTION;` |
|        3 |  7793 | `									goto SkipFuncBody;` |
|        - |  7794 | `								}` |
|       14 |  7795 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  7796 | `								pArg++;` |
|       14 |  7797 | `								continue;` |
|        - |  7798 | `							}` |
|        - |  7799 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  7800 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      104 |  7801 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  7802 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  7803 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  7804 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7805 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  7806 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  7807 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  7808 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  7809 | `										goto Abort;` |
|        - |  7810 | `									}` |
|        - |  7811 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  7812 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  7813 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  7814 | `									pFrameStack = 0;` |
|      ! 0 |  7815 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  7816 | `									goto SkipFuncBody;` |
|      ! 0 |  7817 | `								}else{` |
|       13 |  7818 | `									ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|       13 |  7819 | `									if( xCast ){` |
|       13 |  7820 | `										xCast(pArg);` |
|        6 |  7821 | `									}` |
|        - |  7822 | `								}` |
|        6 |  7823 | `							}` |
|       90 |  7824 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|       90 |  7825 | `							pArg++;` |
|        2 |  7826 | `						}` |
|        - |  7827 | `					}` |
|       34 |  7828 | `					sArg.nIdx = pObj->nIdx;` |
|       34 |  7829 | `					sArg.pUserData = 0;` |
|       34 |  7830 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       16 |  7831 | `				}` |
|       34 |  7832 | `				break; /* All remaining args consumed */` |
|        - |  7833 | `			}` |
|    26262 |  7834 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    26102 |  7835 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       28 |  7836 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  7837 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  7838 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  7839 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  7840 | `						goto Abort;` |
|        - |  7841 | `					}` |
|      ! 0 |  7842 | `				}` |
|        - |  7843 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    26104 |  7844 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       77 |  7845 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       50 |  7846 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0);` |
|       52 |  7847 | `					if( rcU != SXRET_OK ){` |
|        - |  7848 | `						const char *zGiven;` |
|        - |  7849 | `						char zBuf[128];` |
|       19 |  7850 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  7851 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  7852 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  7853 | `							zGiven = "null";` |
|        5 |  7854 | `						}else{` |
|        5 |  7855 | `							zGiven = ph7_type_name(pArg);` |
|        - |  7856 | `						}` |
|       19 |  7857 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  7858 | `							&aFormalArg[n].sName,` |
|       18 |  7859 | `							SyStringLength(&aFormalArg[n].sTypeName) > 0` |
|       18 |  7860 | `								? aFormalArg[n].sTypeName.zString : "union",` |
|        9 |  7861 | `							zGiven);` |
|       19 |  7862 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  7863 | `							goto Abort;` |
|        - |  7864 | `						}` |
|       19 |  7865 | `						PH7_MemObjRelease(pTos);` |
|       19 |  7866 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  7867 | `						pFrameStack = 0;` |
|       19 |  7868 | `						rc = PH7_EXCEPTION;` |
|       19 |  7869 | `						goto SkipFuncBody;` |
|        - |  7870 | `					}` |
|       17 |  7871 | `				}else` |
|        - |  7872 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  7873 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    26074 |  7874 | `				if( aFormalArg[n].nType > 0` |
|    13647 |  7875 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1218 |  7876 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  7877 | `						/* Argument must be a class instance [i.e: object] */` |
|       20 |  7878 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  7879 | `						ph7_class *pClass;` |
|        - |  7880 | `						/* Try to extract the desired class */` |
|       20 |  7881 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  7882 | `						if( pClass ){` |
|       20 |  7883 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  7884 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7885 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7886 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7887 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7888 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7889 | `								}` |
|      ! 0 |  7890 | `							}else{` |
|        - |  7891 | `								/* reuse pThis declared in outer scope */` |
|       20 |  7892 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  7893 | `								/* Make sure the object is an instance of the given class */` |
|       20 |  7894 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  7895 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7896 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  7897 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  7898 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  7899 | `								}` |
|        - |  7900 | `							}` |
|       11 |  7901 | `						}` |
|     1209 |  7902 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       11 |  7903 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  7904 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  7905 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  7906 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  7907 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  7908 | `								goto Abort;` |
|        - |  7909 | `							}` |
|        - |  7910 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  7911 | `							PH7_MemObjRelease(pTos);` |
|       11 |  7912 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  7913 | `							pFrameStack = 0;` |
|       11 |  7914 | `							rc = PH7_EXCEPTION;` |
|       11 |  7915 | `							goto SkipFuncBody;` |
|      ! 0 |  7916 | `						}else{` |
|      ! 0 |  7917 | `							ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  7918 | `							/* Cast to the desired type */` |
|      ! 0 |  7919 | `							xCast(pArg);` |
|        - |  7920 | `						}` |
|      ! 0 |  7921 | `					}` |
|      603 |  7922 | `				}` |
|    26076 |  7923 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  7924 | `					/* Pass by reference */` |
|       54 |  7925 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  7926 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  7927 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  7928 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  7929 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  7930 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  7931 | `						}` |
|        - |  7932 | `						/* Switch to pass by value */` |
|      ! 0 |  7933 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  7934 | `					}else{` |
|        - |  7935 | `						SyHashEntry *pRefEntry;` |
|        - |  7936 | `						/* Install the referenced variable in the private function frame */` |
|       54 |  7937 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       54 |  7938 | `						if( pRefEntry == 0 ){` |
|       80 |  7939 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       52 |  7940 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       54 |  7941 | `							sArg.nIdx = pArg->nIdx;` |
|       54 |  7942 | `							sArg.pUserData = 0;` |
|       54 |  7943 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       26 |  7944 | `						}` |
|       54 |  7945 | `						pObj = 0;` |
|        - |  7946 | `					}` |
|       28 |  7947 | `				}else{` |
|        - |  7948 | `					/* Pass by value,make a copy of the given argument */` |
|    26024 |  7949 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  7950 | `				}` |
|    13039 |  7951 | `			}else{` |
|        - |  7952 | `				char zName[32];` |
|        - |  7953 | `				SyString sArgName;` |
|        - |  7954 | `				/* Set a dummy name */` |
|      160 |  7955 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      160 |  7956 | `				sArgName.zString = zName;` |
|        - |  7957 | `				/* Annonymous argument */` |
|      160 |  7958 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  7959 | `			}` |
|    26234 |  7960 | `			if( pObj ){` |
|    26182 |  7961 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  7962 | `				/* Insert argument index  */` |
|    26182 |  7963 | `				sArg.nIdx = pObj->nIdx;` |
|    26182 |  7964 | `				sArg.pUserData = 0;` |
|    26182 |  7965 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    13090 |  7966 | `			}` |
|    26234 |  7967 | `			PH7_MemObjRelease(pArg);` |
|    26234 |  7968 | `			pArg++;` |
|    26234 |  7969 | `			++n;` |
|        2 |  7970 | `		}` |
|        - |  7971 | `		} /* end named vs positional branch */` |
|        - |  7972 | `		/* Set up closure environment */` |
|    15560 |  7973 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  7974 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  7975 | `			ph7_value *pValue;` |
|        - |  7976 | `			sxu32 iEnv;` |
|      111 |  7977 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      287 |  7978 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      177 |  7979 | `				pEnv = &aEnv[iEnv];` |
|      177 |  7980 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  7981 | `					/* Do not install null value */` |
|      105 |  7982 | `					continue;` |
|        - |  7983 | `				}` |
|       73 |  7984 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       73 |  7985 | `				if( pValue == 0 ){` |
|      ! 0 |  7986 | `					continue;` |
|        - |  7987 | `				}` |
|        - |  7988 | `				/* Invalidate any prior representation */` |
|       73 |  7989 | `				PH7_MemObjRelease(pValue);` |
|        - |  7990 | `				/* Duplicate bound variable value */` |
|       73 |  7991 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       37 |  7992 | `			}` |
|       55 |  7993 | `		}` |
|        - |  7994 | `		/* Process default values for remaining formal parameters */` |
|    17732 |  7995 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2214 |  7996 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  7997 | `				/* Variadic parameter with no extra args — create empty array */` |
|       42 |  7998 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       42 |  7999 | `				if( pObj ){` |
|       42 |  8000 | `					PH7_MemObjToHashmap(pObj);` |
|       42 |  8001 | `					sArg.nIdx = pObj->nIdx;` |
|       42 |  8002 | `					sArg.pUserData = 0;` |
|       42 |  8003 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       20 |  8004 | `				}` |
|       42 |  8005 | `				n++;` |
|       42 |  8006 | `				break; /* Variadic is always last */` |
|        - |  8007 | `			}` |
|     2174 |  8008 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2168 |  8009 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2168 |  8010 | `				if( pObj ){` |
|        - |  8011 | `					/* Evaluate the default value and extract it's result */` |
|     2168 |  8012 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2168 |  8013 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8014 | `						goto Abort;` |
|        - |  8015 | `					}` |
|        - |  8016 | `					/* Insert argument index */` |
|     2168 |  8017 | `					sArg.nIdx = pObj->nIdx;` |
|     2168 |  8018 | `					sArg.pUserData = 0;` |
|     2168 |  8019 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  8020 | `					/* Make sure the default argument is of the correct type */` |
|     2166 |  8021 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1506 |  8022 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  8023 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  8024 | `						/* Cast to the desired type */` |
|      ! 0 |  8025 | `						xCast(pObj);` |
|      ! 0 |  8026 | `					}` |
|     1083 |  8027 | `				}` |
|     1083 |  8028 | `			}` |
|     2174 |  8029 | `			++n;` |
|        2 |  8030 | `		}` |
|        - |  8031 | `		} /* end VmCallArgMap scope */` |
|        - |  8032 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  8033 | `		 * does not return anything.` |
|        - |  8034 | `		 */` |
|    15560 |  8035 | `		PH7_MemObjRelease(pTos);` |
|    15560 |  8036 | `		pTos = &pTos[-nCallArgs];` |
|        - |  8037 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    15560 |  8038 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    15560 |  8039 | `		if( pFrameStack == 0 ){` |
|        - |  8040 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8041 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8042 | `				&pVmFunc->sName);` |
|      ! 0 |  8043 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8044 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8045 | `			}` |
|      ! 0 |  8046 | `			break;` |
|        - |  8047 | `		}` |
|     7779 |  8048 | `SkipFuncBody:` |
|    15590 |  8049 | `		if( pSelf ){` |
|        - |  8050 | `			/* Push class name */` |
|     2340 |  8051 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1169 |  8052 | `		}` |
|        - |  8053 | `		/* Increment nesting level */` |
|    15590 |  8054 | `		pVm->nRecursionDepth++;` |
|    15590 |  8055 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  8056 | `			/* Execute function body */` |
|    15560 |  8057 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0);` |
|     7779 |  8058 | `		}` |
|        - |  8059 | `		/* Decrement nesting level */` |
|    15590 |  8060 | `		pVm->nRecursionDepth--;` |
|    15590 |  8061 | `		if( pSelf ){` |
|        - |  8062 | `			/* Pop class name */` |
|     2340 |  8063 | `			(void)SySetPop(&pVm->aSelf);` |
|     1169 |  8064 | `		}` |
|        - |  8065 | `		/* Cleanup the mess left behind */` |
|    15590 |  8066 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  8067 | `			/* Return by reference,reflect that */` |
|        9 |  8068 | `			if( n != SXU32_HIGH ){` |
|        9 |  8069 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  8070 | `				sxu32 i;` |
|        - |  8071 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  8072 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  8073 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  8074 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  8075 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8076 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8077 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  8078 | `								&pVmFunc->sName);` |
|      ! 0 |  8079 | `						}` |
|      ! 0 |  8080 | `						n = SXU32_HIGH;` |
|      ! 0 |  8081 | `						break;` |
|        - |  8082 | `					}` |
|        3 |  8083 | `				}` |
|        5 |  8084 | `			}else{` |
|      ! 0 |  8085 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8086 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8087 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  8088 | `						&pVmFunc->sName);` |
|      ! 0 |  8089 | `				}` |
|        - |  8090 | `			}` |
|        9 |  8091 | `			pTos->nIdx = n;` |
|        4 |  8092 | `		}` |
|        - |  8093 | `		/* Cleanup the mess left behind */` |
|    15590 |  8094 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  8095 | `			/* An exception was throw in this frame */` |
|       44 |  8096 | `			pFrame = pFrame->pParent;` |
|       44 |  8097 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  8098 | `				/* Pop the resutlt */` |
|       42 |  8099 | `				VmPopOperand(&pTos,1);` |
|        - |  8100 | `				/* Jump to this destination */` |
|       42 |  8101 | `				pc = pFrame->iExceptionJump - 1;` |
|       42 |  8102 | `				rc = PH7_OK;` |
|       22 |  8103 | `			}else{` |
|        3 |  8104 | `				if( pFrame->pParent ){` |
|        3 |  8105 | `					rc = PH7_EXCEPTION;` |
|        2 |  8106 | `				}else{` |
|        - |  8107 | `					/* Continue normal execution */` |
|      ! 0 |  8108 | `					rc = PH7_OK;` |
|        - |  8109 | `				}` |
|        - |  8110 | `			}` |
|       21 |  8111 | `		}` |
|        - |  8112 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    15590 |  8113 | `		if( pFrameStack ){` |
|    15560 |  8114 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     7779 |  8115 | `		}` |
|        - |  8116 | `		/* Leave the frame */` |
|    15590 |  8117 | `		VmLeaveFrame(&(*pVm));` |
|    15590 |  8118 | `		if( rc == PH7_ABORT ){` |
|        - |  8119 | `			/* Abort processing immeditaley */` |
|        9 |  8120 | `			goto Abort;` |
|    15582 |  8121 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8122 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8123 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8124 | `			 * overwriting the state saved by the inner level.` |
|        - |  8125 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8126 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8127 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8128 | `			goto Suspend;` |
|    15544 |  8129 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8130 | `			goto Exception;` |
|        - |  8131 | `		}` |
|     7772 |  8132 | `	}else{` |
|        - |  8133 | `		ph7_user_func *pFunc;` |
|        - |  8134 | `		ph7_context sCtx;` |
|        - |  8135 | `		ph7_value sRet;` |
|        - |  8136 | `		/* Look for an installed foreign function.` |
|        - |  8137 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8138 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8139 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8140 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   636118 |  8141 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8142 | `		{` |
|   636118 |  8143 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   636118 |  8144 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8145 | `			/* Compiler-qualified: try short name as global fallback */` |
|       20 |  8146 | `			const char *zShort = sName.zString;` |
|        - |  8147 | `			sxu32 i;` |
|      296 |  8148 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      278 |  8149 | `				if( sName.zString[i] == '\\' ){` |
|       24 |  8150 | `					zShort = &sName.zString[i + 1];` |
|       11 |  8151 | `				}` |
|      140 |  8152 | `			}` |
|       20 |  8153 | `			if( zShort != sName.zString ){` |
|       20 |  8154 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       20 |  8155 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        9 |  8156 | `			}` |
|        9 |  8157 | `		}` |
|        - |  8158 | `		} /* end VmCallArgMap namespace scope */` |
|   636118 |  8159 | `		if( pEntry == 0 ){` |
|        - |  8160 | `			/* Call to undefined function */` |
|        5 |  8161 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8162 | `			/* Pop given arguments */` |
|        5 |  8163 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8164 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8165 | `			}` |
|        - |  8166 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8167 | `			PH7_MemObjRelease(pTos);` |
|        8 |  8168 | `			break;` |
|        - |  8169 | `		}` |
|   636114 |  8170 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  8171 | `		/* Start collecting function arguments */` |
|   636114 |  8172 | `		SySetReset(&aArg);` |
|  1711608 |  8173 | `		while( pArg < pTos ){` |
|  1075496 |  8174 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1075496 |  8175 | `			pArg++;` |
|        2 |  8176 | `		}` |
|        - |  8177 | `		/* Assume a null return value */` |
|   636114 |  8178 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  8179 | `		/* Init the call context */` |
|   636114 |  8180 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  8181 | `		/* Call the foreign function */` |
|   636114 |  8182 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  8183 | `		/* Release the call context */` |
|   636114 |  8184 | `		VmReleaseCallContext(&sCtx);` |
|   636114 |  8185 | `		if( rc == PH7_ABORT ){` |
|      471 |  8186 | `			goto Abort;` |
|   635644 |  8187 | `		}else if( rc == PH7_EXCEPTION ){` |
|       12 |  8188 | `			VmFrame *pFrm = pVm->pFrame;` |
|       12 |  8189 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       12 |  8190 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  8191 | `				/* Exception was NOT caught, propagate */` |
|        5 |  8192 | `				goto Exception;` |
|        - |  8193 | `			}` |
|        - |  8194 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  8195 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  8196 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  8197 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  8198 | `			}` |
|        - |  8199 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  8200 | `			VmPopOperand(&pTos,1);` |
|        - |  8201 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  8202 | `			pFrm = pVm->pFrame;` |
|        7 |  8203 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  8204 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  8205 | `			}` |
|        7 |  8206 | `			break;` |
|        - |  8207 | `		}` |
|   635634 |  8208 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8209 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  8210 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  8211 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  8212 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  8213 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  8214 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  8215 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  8216 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  8217 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  8218 | `			}` |
|        - |  8219 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  8220 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  8221 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  8222 | `			goto Suspend;` |
|        - |  8223 | `		}` |
|   635596 |  8224 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8225 | `			/* Pop function name and arguments */` |
|   615524 |  8226 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   307783 |  8227 | `		}` |
|        - |  8228 | `		/* Save foreign function return value */` |
|   635596 |  8229 | `		PH7_MemObjStore(&sRet,pTos);` |
|   635596 |  8230 | `		PH7_MemObjRelease(&sRet);` |
|        - |  8231 | `	}` |
|   651136 |  8232 | `	break;` |
|        - |  8233 | `				  }` |
|        - |  8234 | `/*` |
|        - |  8235 | ` * OP_CONSUME: P1 * *` |
|        - |  8236 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  8237 | ` */` |
|    13428 |  8238 | `case PH7_OP_CONSUME: {` |
|    26858 |  8239 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    26858 |  8240 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  8241 |  |
|    26858 |  8242 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    26858 |  8243 | `	pCur = pOut;` |
|        - |  8244 | `	/* Start the consume process  */` |
|    53714 |  8245 | `	while( pOut <= pTos ){` |
|        - |  8246 | `		/* Force a string cast */` |
|    26858 |  8247 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      496 |  8248 | `			PH7_MemObjToString(pOut);` |
|      247 |  8249 | `		}` |
|    26858 |  8250 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  8251 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  8252 | `			/* Invoke the output consumer callback */` |
|    15484 |  8253 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    15484 |  8254 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    15484 |  8255 | `			SyBlobRelease(&pOut->sBlob);` |
|    15484 |  8256 | `			if( rc == SXERR_ABORT ){` |
|        - |  8257 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  8258 | `				goto Abort;` |
|        - |  8259 | `			}` |
|     7741 |  8260 | `		}` |
|    26858 |  8261 | `		pOut++;` |
|        2 |  8262 | `	}` |
|    26858 |  8263 | `	pTos = &pCur[-1];` |
|    26856 |  8264 | `	break;` |
|        - |  8265 | `					 }` |
|        - |  8266 |  |
|        - |  8267 | `		} /* Switch() */` |
| 10874824 |  8268 | `		pc++; /* Next instruction in the stream */` |
|        2 |  8269 | `	} /* For(;;) */` |
|    18783 |  8270 | `Done:` |
|    37568 |  8271 | `	SySetRelease(&aArg);` |
|    37568 |  8272 | `	return SXRET_OK;` |
|       72 |  8273 | `Suspend:` |
|      146 |  8274 | `	SySetRelease(&aArg);` |
|      146 |  8275 | `	return PH7_SUSPEND;` |
|      250 |  8276 | `Abort:` |
|      501 |  8277 | `	SySetRelease(&aArg);` |
|     1727 |  8278 | `	while( pTos >= pStack ){` |
|     1227 |  8279 | `		PH7_MemObjRelease(pTos);` |
|     1227 |  8280 | `		pTos--;` |
|        1 |  8281 | `	}` |
|      501 |  8282 | `	return PH7_ABORT;` |
|        3 |  8283 | `Exception:` |
|        8 |  8284 | `	SySetRelease(&aArg);` |
|       22 |  8285 | `	while( pTos >= pStack ){` |
|       16 |  8286 | `		PH7_MemObjRelease(pTos);` |
|       16 |  8287 | `		pTos--;` |
|        2 |  8288 | `	}` |
|        8 |  8289 | `	return PH7_EXCEPTION;` |
|    19110 |  8290 |  |
|        - |  8291 | `/*` |
|        - |  8292 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8293 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8294 | ` * See block-comment on that function for additional information.` |
|        - |  8295 | ` */` |
|    17776 |  8296 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  8297 |  |
|        - |  8298 | `	ph7_value *pStack;` |
|        - |  8299 | `	sxi32 rc;` |
|        - |  8300 | `	/* Allocate a new operand stack */` |
|    17778 |  8301 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    17778 |  8302 | `	if( pStack == 0 ){` |
|      ! 0 |  8303 | `		return SXERR_MEM;` |
|        - |  8304 | `	}` |
|        - |  8305 | `	/* Execute the program */` |
|    17778 |  8306 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0);` |
|        - |  8307 | `	/* Free the operand stack */` |
|    17778 |  8308 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  8309 | `	/* Execution result */` |
|    17778 |  8310 | `	return rc;` |
|     8890 |  8311 |  |
|        - |  8312 | `/*` |
|        - |  8313 | ` * Invoke any installed shutdown callbacks.` |
|        - |  8314 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  8315 | ` * or more calls to [register_shutdown_function()].` |
|        - |  8316 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  8317 | ` * execution ends.` |
|        - |  8318 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  8319 | ` * additional information.` |
|        - |  8320 | ` */` |
|     2574 |  8321 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  8322 |  |
|        - |  8323 | `	VmShutdownCB *pEntry;` |
|        - |  8324 | `	ph7_value *apArg[10];` |
|        - |  8325 | `	sxu32 n,nEntry;` |
|        - |  8326 | `	int i;` |
|        - |  8327 | `	/* Point to the stack of registered callbacks */` |
|     2576 |  8328 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    28316 |  8329 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    25742 |  8330 | `		apArg[i] = 0;` |
|    12872 |  8331 | `	}` |
|     2578 |  8332 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  8333 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8334 | `		if( pEntry ){` |
|        - |  8335 | `			/* Prepare callback arguments if any */` |
|        3 |  8336 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  8337 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  8338 | `					break;` |
|        - |  8339 | `				}` |
|      ! 0 |  8340 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  8341 | `			}` |
|        - |  8342 | `			/* Invoke the callback */` |
|        3 |  8343 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  8344 | `			/*` |
|        - |  8345 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  8346 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  8347 | `			 */` |
|        3 |  8348 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  8349 | `			if( pEntry ){` |
|        3 |  8350 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  8351 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  8352 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  8353 | `				}` |
|        1 |  8354 | `			}` |
|        1 |  8355 | `		}` |
|        2 |  8356 | `	}` |
|     2576 |  8357 | `	SySetReset(&pVm->aShutdown);` |
|     2576 |  8358 |  |
|        - |  8359 | `/*` |
|        - |  8360 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  8361 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8362 | ` * See block-comment on that function for additional information.` |
|        - |  8363 | ` */` |
|     2582 |  8364 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  8365 |  |
|        - |  8366 | `	/* Make sure we are ready to execute this program */` |
|     2584 |  8367 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  8368 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  8369 | `	}` |
|        - |  8370 | `	/* Set the execution magic number  */` |
|     2584 |  8371 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  8372 | `	/* Execute the program */` |
|     2584 |  8373 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0);` |
|        - |  8374 | `	/* Invoke any shutdown callbacks */` |
|     2580 |  8375 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  8376 | `	/*` |
|        - |  8377 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  8378 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  8379 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  8380 | `	 */` |
|     2580 |  8381 | `	return SXRET_OK;` |
|     1293 |  8382 |  |
|        - |  8383 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  8384 | `/*` |
|        - |  8385 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  8386 | ` * The context is in CREATED state and ready to be started.` |
|        - |  8387 | ` */` |
|       46 |  8388 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  8389 |  |
|        - |  8390 | `	ph7_exec_ctx *pCtx;` |
|        - |  8391 | `	ph7_value *pStack;` |
|        - |  8392 | `	VmFrame *pFrame;` |
|       48 |  8393 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  8394 | `	if( pCtx == 0 ){` |
|      ! 0 |  8395 | `		return 0;` |
|        - |  8396 | `	}` |
|       48 |  8397 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  8398 | `	pCtx->pVm = pVm;` |
|       48 |  8399 | `	pCtx->pFunc = pFunc;` |
|       48 |  8400 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  8401 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  8402 | `	pCtx->pc = 0;` |
|       48 |  8403 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  8404 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  8405 | `	/* Allocate a private operand stack */` |
|       48 |  8406 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  8407 | `	if( pStack == 0 ){` |
|      ! 0 |  8408 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8409 | `		return 0;` |
|        - |  8410 | `	}` |
|       48 |  8411 | `	pCtx->pStack = pStack;` |
|        - |  8412 | `	/* Create a detached frame for the fiber */` |
|       48 |  8413 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  8414 | `	if( pFrame == 0 ){` |
|      ! 0 |  8415 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  8416 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  8417 | `		return 0;` |
|        - |  8418 | `	}` |
|       48 |  8419 | `	pCtx->pFrame = pFrame;` |
|       48 |  8420 | `	return pCtx;` |
|       25 |  8421 |  |
|        - |  8422 | `/*` |
|        - |  8423 | ` * Start executing a fiber context for the first time.` |
|        - |  8424 | ` */` |
|       46 |  8425 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  8426 |  |
|        - |  8427 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8428 | `	sxi32 rc;` |
|       48 |  8429 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8430 | `		return SXERR_INVALID;` |
|        - |  8431 | `	}` |
|        - |  8432 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  8433 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  8434 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8435 | `	/* Save and set the active context */` |
|       48 |  8436 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  8437 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  8438 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  8439 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  8440 | `	pVm->nRecursionDepth++;` |
|        - |  8441 | `	/* Execute from the beginning */` |
|       71 |  8442 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  8443 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0);` |
|       48 |  8444 | `	pVm->nRecursionDepth--;` |
|        - |  8445 | `	/* Restore the previous context */` |
|       48 |  8446 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  8447 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8448 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  8449 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  8450 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  8451 | `		if( pResult ){` |
|       24 |  8452 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  8453 | `		}` |
|       46 |  8454 | `		return SXRET_OK;` |
|        - |  8455 | `	}` |
|        - |  8456 | `	/* Detach frame */` |
|        3 |  8457 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  8458 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  8459 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  8460 | `	}` |
|        3 |  8461 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8462 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8463 | `		return PH7_ABORT;` |
|        - |  8464 | `	}` |
|        3 |  8465 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8466 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8467 | `		return PH7_EXCEPTION;` |
|        - |  8468 | `	}` |
|        - |  8469 | `	/* Normal completion */` |
|        3 |  8470 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  8471 | `	if( pResult ){` |
|        3 |  8472 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  8473 | `	}` |
|        3 |  8474 | `	return SXRET_OK;` |
|       25 |  8475 |  |
|        - |  8476 | `/*` |
|        - |  8477 | ` * Resume a suspended fiber context.` |
|        - |  8478 | ` */` |
|       98 |  8479 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  8480 |  |
|        - |  8481 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  8482 | `	sxi32 rc;` |
|      100 |  8483 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  8484 | `		return SXERR_INVALID;` |
|        - |  8485 | `	}` |
|        - |  8486 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  8487 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  8488 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  8489 | `	if( pResumeValue ){` |
|       40 |  8490 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  8491 | `	}else{` |
|       62 |  8492 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  8493 | `	}` |
|      100 |  8494 | `	pCtx->nTos++;` |
|        - |  8495 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  8496 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  8497 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  8498 | `	/* Save and set the active context */` |
|      100 |  8499 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  8500 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  8501 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  8502 | `	pVm->nRecursionDepth++;` |
|        - |  8503 | `	/* Resume execution from saved PC */` |
|      149 |  8504 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  8505 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc);` |
|      100 |  8506 | `	pVm->nRecursionDepth--;` |
|        - |  8507 | `	/* Restore the previous context */` |
|      100 |  8508 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  8509 | `	if( rc == PH7_SUSPEND ){` |
|        - |  8510 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  8511 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  8512 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  8513 | `		if( pResult ){` |
|       18 |  8514 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  8515 | `		}` |
|       64 |  8516 | `		return SXRET_OK;` |
|        - |  8517 | `	}` |
|        - |  8518 | `	/* Detach frame */` |
|       38 |  8519 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  8520 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  8521 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  8522 | `	}` |
|       38 |  8523 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8524 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8525 | `		return PH7_ABORT;` |
|        - |  8526 | `	}` |
|       38 |  8527 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8528 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  8529 | `		return PH7_EXCEPTION;` |
|        - |  8530 | `	}` |
|        - |  8531 | `	/* Normal completion */` |
|       38 |  8532 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  8533 | `	if( pResult ){` |
|       20 |  8534 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  8535 | `	}` |
|       38 |  8536 | `	return SXRET_OK;` |
|       51 |  8537 |  |
|        - |  8538 | `/*` |
|        - |  8539 | ` * Release an execution context and all its resources.` |
|        - |  8540 | ` */` |
|        4 |  8541 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  8542 |  |
|        5 |  8543 | `	if( pCtx == 0 ){` |
|      ! 0 |  8544 | `		return;` |
|        - |  8545 | `	}` |
|        5 |  8546 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  8547 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  8548 | `		return;` |
|        - |  8549 | `	}` |
|        5 |  8550 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  8551 | `	/* Release values */` |
|        5 |  8552 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  8553 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  8554 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  8555 | `	if( pCtx->pFrame ){` |
|        - |  8556 | `		VmSlot *aSlot;` |
|        - |  8557 | `		sxu32 n;` |
|        - |  8558 | `		/* Free local variables */` |
|        5 |  8559 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  8560 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  8561 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  8562 | `		}` |
|        - |  8563 | `		/* Remove local references */` |
|        5 |  8564 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  8565 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  8566 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  8567 | `		}` |
|        5 |  8568 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  8569 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  8570 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  8571 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  8572 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  8573 | `		pCtx->pFrame = 0;` |
|        2 |  8574 | `	}` |
|        - |  8575 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  8576 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  8577 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  8578 | `	if( pCtx->pStack ){` |
|        5 |  8579 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  8580 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  8581 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  8582 | `				PH7_MemObjRelease(pTos);` |
|        5 |  8583 | `				pTos--;` |
|        1 |  8584 | `			}` |
|        2 |  8585 | `		}` |
|        5 |  8586 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  8587 | `		pCtx->pStack = 0;` |
|        2 |  8588 | `	}` |
|        - |  8589 | `	/* Free the context itself */` |
|        5 |  8590 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  8591 |  |
|        - |  8592 | `/*` |
|        - |  8593 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  8594 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  8595 | ` */` |
|       90 |  8596 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  8597 |  |
|        - |  8598 | `	ph7_class_instance *pThis;` |
|        - |  8599 | `	SyString sAttr;` |
|        - |  8600 | `	ph7_value *pAttr;` |
|       92 |  8601 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8602 | `		return 0;` |
|        - |  8603 | `	}` |
|       92 |  8604 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  8605 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  8606 | `		return 0;` |
|        - |  8607 | `	}` |
|       92 |  8608 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  8609 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  8610 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  8611 | `		return 0;` |
|        - |  8612 | `	}` |
|       62 |  8613 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  8614 |  |
|        - |  8615 | `/*` |
|        - |  8616 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  8617 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  8618 | ` */` |
|       38 |  8619 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8620 |  |
|       40 |  8621 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  8622 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  8623 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8624 | `			"Cannot suspend outside of a fiber");` |
|        - |  8625 | `	}` |
|       40 |  8626 | `	if( nArg > 0 ){` |
|       40 |  8627 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  8628 | `	}else{` |
|      ! 0 |  8629 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  8630 | `	}` |
|       40 |  8631 | `	return PH7_SUSPEND;` |
|       21 |  8632 |  |
|        - |  8633 | `/*` |
|        - |  8634 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  8635 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  8636 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  8637 | ` */` |
|       24 |  8638 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8639 |  |
|        - |  8640 | `	ph7_class_instance *pThis;` |
|        - |  8641 | `	ph7_value *pAttr;` |
|        - |  8642 | `	SyString sAttrName;` |
|       26 |  8643 | `	if( nArg < 2 ){` |
|      ! 0 |  8644 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8645 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  8646 | `	}` |
|       26 |  8647 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8648 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8649 | `			"Fiber::__construct(): invalid $this");` |
|        - |  8650 | `	}` |
|       26 |  8651 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  8652 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  8653 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8654 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  8655 | `	}` |
|        - |  8656 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  8657 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8658 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8659 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  8660 | `	}` |
|        - |  8661 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  8662 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8663 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8664 | `	if( pAttr ){` |
|       26 |  8665 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  8666 | `	}` |
|       26 |  8667 | `	return PH7_OK;` |
|       14 |  8668 |  |
|        - |  8669 | `/*` |
|        - |  8670 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  8671 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  8672 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  8673 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  8674 | ` */` |
|       24 |  8675 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  8676 | `	ph7_class_instance **ppThis)` |
|        2 |  8677 |  |
|       26 |  8678 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8679 | `	ph7_value *pCallable;` |
|        - |  8680 | `	SyString sAttrName;` |
|       26 |  8681 | `	*ppThis = 0;` |
|       26 |  8682 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  8683 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  8684 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  8685 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  8686 | `		return 0;` |
|        - |  8687 | `	}` |
|       26 |  8688 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  8689 | `		/* String callable — look up in user functions with overload support */` |
|        - |  8690 | `		SyString sName;` |
|        - |  8691 | `		SyHashEntry *pEntry;` |
|        - |  8692 | `		ph7_vm_func *pFunc;` |
|       26 |  8693 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  8694 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  8695 | `		if( pEntry == 0 ){` |
|      ! 0 |  8696 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  8697 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  8698 | `			return 0;` |
|        - |  8699 | `		}` |
|       26 |  8700 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  8701 | `		return pFunc;` |
|      ! 0 |  8702 | `	}else{` |
|        - |  8703 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  8704 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  8705 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  8706 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  8707 | `		if( pMethod == 0 ){` |
|      ! 0 |  8708 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8709 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  8710 | `			return 0;` |
|        - |  8711 | `		}` |
|      ! 0 |  8712 | `		*ppThis = pClosure;` |
|      ! 0 |  8713 | `		return &pMethod->sFunc;` |
|        - |  8714 | `	}` |
|       14 |  8715 |  |
|        - |  8716 | `/*` |
|        - |  8717 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  8718 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  8719 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  8720 | ` */` |
|       46 |  8721 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  8722 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  8723 |  |
|       48 |  8724 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  8725 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  8726 | `	sxu32 nFormal, n;` |
|        - |  8727 | `	VmSlot sSlot;` |
|        - |  8728 | `	sxi32 rc;` |
|        - |  8729 | `	/* Install $this for closure/method callables */` |
|       48 |  8730 | `	if( pClosureThis ){` |
|        - |  8731 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  8732 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  8733 | `		if( pObj ){` |
|      ! 0 |  8734 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  8735 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  8736 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  8737 | `		}` |
|      ! 0 |  8738 | `	}` |
|        - |  8739 | `	/* Install static variables */` |
|       48 |  8740 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  8741 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  8742 | `		ph7_value *pVal;` |
|      ! 0 |  8743 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  8744 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  8745 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  8746 | `			if( pVal ){` |
|      ! 0 |  8747 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8748 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  8749 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  8750 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  8751 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  8752 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  8753 | `				}` |
|      ! 0 |  8754 | `			}` |
|      ! 0 |  8755 | `		}` |
|      ! 0 |  8756 | `	}` |
|        - |  8757 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  8758 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  8759 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  8760 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  8761 | `		ph7_value *pObj;` |
|       20 |  8762 | `		if( n < (sxu32)nArg ){` |
|        - |  8763 | `			/* Argument provided — install with type casting */` |
|       20 |  8764 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  8765 | `			if( pObj ){` |
|       20 |  8766 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  8767 | `				/* Type casting */` |
|       20 |  8768 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8769 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8770 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8771 | `						if( xCast ){` |
|      ! 0 |  8772 | `							xCast(pObj);` |
|      ! 0 |  8773 | `						}` |
|      ! 0 |  8774 | `					}` |
|      ! 0 |  8775 | `				}` |
|       20 |  8776 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  8777 | `				sSlot.pUserData = 0;` |
|       20 |  8778 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  8779 | `			}` |
|        9 |  8780 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  8781 | `			/* Default value */` |
|      ! 0 |  8782 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  8783 | `			if( pObj ){` |
|      ! 0 |  8784 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  8785 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8786 | `					return rc;` |
|        - |  8787 | `				}` |
|      ! 0 |  8788 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  8789 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8790 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8791 | `						if( xCast ){` |
|      ! 0 |  8792 | `							xCast(pObj);` |
|      ! 0 |  8793 | `						}` |
|      ! 0 |  8794 | `					}` |
|      ! 0 |  8795 | `				}` |
|      ! 0 |  8796 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  8797 | `				sSlot.pUserData = 0;` |
|      ! 0 |  8798 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  8799 | `			}` |
|      ! 0 |  8800 | `		}` |
|       11 |  8801 | `	}` |
|        - |  8802 | `	/* Install closure environment (captured variables) */` |
|       48 |  8803 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8804 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  8805 | `		ph7_value *pValue;` |
|        - |  8806 | `		sxu32 iEnv;` |
|        3 |  8807 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  8808 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  8809 | `			pEnv = &aEnv[iEnv];` |
|        7 |  8810 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  8811 | `				continue;` |
|        - |  8812 | `			}` |
|        5 |  8813 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  8814 | `			if( pValue == 0 ){` |
|      ! 0 |  8815 | `				continue;` |
|        - |  8816 | `			}` |
|        5 |  8817 | `			PH7_MemObjRelease(pValue);` |
|        5 |  8818 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  8819 | `		}` |
|        1 |  8820 | `	}` |
|       48 |  8821 | `	return SXRET_OK;` |
|       25 |  8822 |  |
|        - |  8823 | `/*` |
|        - |  8824 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  8825 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  8826 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  8827 | ` */` |
|       26 |  8828 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8829 |  |
|       28 |  8830 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8831 | `	ph7_class_instance *pThis;` |
|        - |  8832 | `	ph7_class_instance *pClosureThis;` |
|        - |  8833 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8834 | `	ph7_vm_func *pFunc;` |
|        - |  8835 | `	ph7_value sResult;` |
|        - |  8836 | `	ph7_value *pCtxAttr;` |
|        - |  8837 | `	SyString sAttrName;` |
|        - |  8838 | `	sxi32 rc;` |
|       28 |  8839 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8840 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  8841 | `	}` |
|       28 |  8842 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  8843 | `	/* Check if already started (has a __ctx) */` |
|       28 |  8844 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  8845 | `	if( pExecCtx != 0 ){` |
|        3 |  8846 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8847 | `			"Cannot start a fiber that has already been started");` |
|        - |  8848 | `	}` |
|        - |  8849 | `	/* Resolve callable */` |
|       26 |  8850 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  8851 | `	if( pFunc == 0 ){` |
|      ! 0 |  8852 | `		return PH7_EXCEPTION;` |
|        - |  8853 | `	}` |
|        - |  8854 | `	/* Create execution context now that we know the function */` |
|       26 |  8855 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  8856 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8857 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8858 | `			"Fiber::start(): out of memory");` |
|        - |  8859 | `	}` |
|        - |  8860 | `	/* Store context in $this->__ctx */` |
|       26 |  8861 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  8862 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  8863 | `	if( pCtxAttr ){` |
|       26 |  8864 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  8865 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  8866 | `	}` |
|        - |  8867 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  8868 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  8869 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  8870 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  8871 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  8872 | `	/* Unpack the args array and install into the frame */` |
|        - |  8873 | `	{` |
|       26 |  8874 | `		ph7_value **apValues = 0;` |
|       26 |  8875 | `		int nActual = 0;` |
|       26 |  8876 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  8877 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  8878 | `			ph7_hashmap_node *pNode;` |
|       26 |  8879 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  8880 | `			if( nCount > 0 ){` |
|        3 |  8881 | `				sxu32 idx = 0;` |
|        4 |  8882 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  8883 | `					nCount * sizeof(ph7_value *));` |
|        3 |  8884 | `				if( apValues ){` |
|        3 |  8885 | `					pNode = pMap->pFirst;` |
|        7 |  8886 | `					while( pNode && idx < nCount ){` |
|        5 |  8887 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  8888 | `						idx++;` |
|        5 |  8889 | `						pNode = pNode->pPrev;` |
|        1 |  8890 | `					}` |
|        3 |  8891 | `					nActual = (int)idx;` |
|        1 |  8892 | `				}` |
|        1 |  8893 | `			}` |
|       12 |  8894 | `		}` |
|       26 |  8895 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  8896 | `		if( apValues ){` |
|        3 |  8897 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  8898 | `		}` |
|        - |  8899 | `	}` |
|        - |  8900 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  8901 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  8902 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  8903 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  8904 | `		return PH7_ABORT;` |
|        - |  8905 | `	}` |
|       26 |  8906 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  8907 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  8908 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8909 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8910 | `		return PH7_ABORT;` |
|        - |  8911 | `	}` |
|       26 |  8912 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8913 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8914 | `		return PH7_EXCEPTION;` |
|        - |  8915 | `	}` |
|       26 |  8916 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  8917 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  8918 | `	return PH7_OK;` |
|       15 |  8919 |  |
|        - |  8920 | `/*` |
|        - |  8921 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  8922 | ` */` |
|       36 |  8923 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8924 |  |
|       38 |  8925 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8926 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  8927 | `	ph7_value sResult;` |
|        - |  8928 | `	ph7_value *pResumeVal;` |
|        - |  8929 | `	sxi32 rc;` |
|       38 |  8930 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8931 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  8932 | `		return PH7_OK;` |
|        - |  8933 | `	}` |
|       38 |  8934 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  8935 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8936 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  8937 | `		return PH7_OK;` |
|        - |  8938 | `	}` |
|       38 |  8939 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  8940 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8941 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  8942 | `	}` |
|       36 |  8943 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  8944 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  8945 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  8946 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  8947 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8948 | `		return PH7_ABORT;` |
|        - |  8949 | `	}` |
|       36 |  8950 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  8951 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  8952 | `		return PH7_EXCEPTION;` |
|        - |  8953 | `	}` |
|       36 |  8954 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  8955 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  8956 | `	return PH7_OK;` |
|       20 |  8957 |  |
|        - |  8958 | `/*` |
|        - |  8959 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  8960 | ` */` |
|        6 |  8961 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  8962 |  |
|        8 |  8963 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8964 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  8965 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8966 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8967 | `		return PH7_OK;` |
|        - |  8968 | `	}` |
|        8 |  8969 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  8970 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  8971 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8972 | `		return PH7_OK;` |
|        - |  8973 | `	}` |
|        8 |  8974 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  8975 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  8976 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8977 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  8978 | `		}` |
|      ! 0 |  8979 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  8980 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  8981 | `	}` |
|        8 |  8982 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  8983 | `	return PH7_OK;` |
|        5 |  8984 |  |
|        - |  8985 | `/*` |
|        - |  8986 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  8987 | ` */` |
|        6 |  8988 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  8989 |  |
|        - |  8990 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  8991 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  8992 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  8993 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  8994 | `	return PH7_OK;` |
|        4 |  8995 |  |
|      ! 0 |  8996 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  8997 |  |
|        - |  8998 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  8999 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  9000 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9001 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  9002 | `	return PH7_OK;` |
|      ! 0 |  9003 |  |
|        6 |  9004 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9005 |  |
|        - |  9006 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9007 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9008 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9009 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  9010 | `	return PH7_OK;` |
|        4 |  9011 |  |
|        6 |  9012 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9013 |  |
|        - |  9014 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9015 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9016 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9017 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  9018 | `	return PH7_OK;` |
|        4 |  9019 |  |
|        - |  9020 | `/*` |
|        - |  9021 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  9022 | ` */` |
|        4 |  9023 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9024 |  |
|        5 |  9025 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9026 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  9027 | `	if( nArg < 1 ){` |
|      ! 0 |  9028 | `		return PH7_OK;` |
|        - |  9029 | `	}` |
|        5 |  9030 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  9031 | `	if( pExecCtx ){` |
|        5 |  9032 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  9033 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  9034 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  9035 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9036 | `			SyString sAttrName;` |
|        - |  9037 | `			ph7_value *pAttr;` |
|        5 |  9038 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  9039 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  9040 | `			if( pAttr ){` |
|        5 |  9041 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  9042 | `			}` |
|        2 |  9043 | `		}` |
|        2 |  9044 | `	}` |
|        5 |  9045 | `	return PH7_OK;` |
|        3 |  9046 |  |
|        - |  9047 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  9048 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  9049 |  |
|        - |  9050 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9051 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  9052 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9053 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  9054 |  |
|      ! 0 |  9055 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  9056 |  |
|        - |  9057 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9058 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  9059 | `	ph7_exec_ctx *pCtx;` |
|        - |  9060 | `	ph7_vm_func *pFunc;` |
|        - |  9061 | `	ph7_value *pCallable;` |
|        - |  9062 | `	ph7_value *pCtxAttr;` |
|        - |  9063 | `	SyString sAttrName;` |
|        - |  9064 | `	/* Must not already be started */` |
|      ! 0 |  9065 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9066 | `	if( pCtx != 0 ){` |
|      ! 0 |  9067 | `		return SXERR_INVALID;` |
|        - |  9068 | `	}` |
|      ! 0 |  9069 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9070 | `		return SXERR_INVALID;` |
|        - |  9071 | `	}` |
|      ! 0 |  9072 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  9073 | `	/* Get the callable */` |
|      ! 0 |  9074 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  9075 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9076 | `	if( pCallable == 0 ){` |
|      ! 0 |  9077 | `		return SXERR_INVALID;` |
|        - |  9078 | `	}` |
|        - |  9079 | `	/* Resolve callable */` |
|      ! 0 |  9080 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9081 | `		SyString sName;` |
|        - |  9082 | `		SyHashEntry *pEntry;` |
|      ! 0 |  9083 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  9084 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  9085 | `		if( pEntry == 0 ){` |
|      ! 0 |  9086 | `			return SXERR_NOTFOUND;` |
|        - |  9087 | `		}` |
|      ! 0 |  9088 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  9089 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9090 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9091 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9092 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9093 | `		if( pMethod == 0 ){` |
|      ! 0 |  9094 | `			return SXERR_INVALID;` |
|        - |  9095 | `		}` |
|      ! 0 |  9096 | `		pClosureThis = pClosure;` |
|      ! 0 |  9097 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  9098 | `	}else{` |
|      ! 0 |  9099 | `		return SXERR_INVALID;` |
|        - |  9100 | `	}` |
|        - |  9101 | `	/* Create context */` |
|      ! 0 |  9102 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9103 | `	if( pCtx == 0 ){` |
|      ! 0 |  9104 | `		return SXERR_MEM;` |
|        - |  9105 | `	}` |
|        - |  9106 | `	/* Store in __ctx */` |
|      ! 0 |  9107 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9108 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9109 | `	if( pCtxAttr ){` |
|      ! 0 |  9110 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9111 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9112 | `	}` |
|        - |  9113 | `	/* Set up frame with args */` |
|      ! 0 |  9114 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9115 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9116 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9117 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9118 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9119 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9120 |  |
|      ! 0 |  9121 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9122 |  |
|      ! 0 |  9123 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9124 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9125 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9126 |  |
|      ! 0 |  9127 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9128 |  |
|      ! 0 |  9129 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9130 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9131 |  |
|      ! 0 |  9132 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9133 |  |
|      ! 0 |  9134 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9135 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9136 |  |
|      ! 0 |  9137 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9138 |  |
|      ! 0 |  9139 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9140 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9141 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9142 |  |
|        - |  9143 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9144 | `/*` |
|        - |  9145 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9146 | ` */` |
|       22 |  9147 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9148 |  |
|        - |  9149 | `	ph7_generator *pGen;` |
|       24 |  9150 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9151 | `	if( pGen == 0 ){` |
|      ! 0 |  9152 | `		return 0;` |
|        - |  9153 | `	}` |
|       24 |  9154 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9155 | `	pGen->pCtx = pCtx;` |
|       24 |  9156 | `	pGen->iImplicitKey = 0;` |
|       24 |  9157 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9158 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9159 | `	/* Link the generator back to the exec context */` |
|       24 |  9160 | `	pCtx->pPrivate = pGen;` |
|       24 |  9161 | `	return pGen;` |
|       13 |  9162 |  |
|        - |  9163 | `/*` |
|        - |  9164 | ` * Release a generator and its execution context.` |
|        - |  9165 | ` */` |
|      ! 0 |  9166 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  9167 |  |
|      ! 0 |  9168 | `	if( pGen == 0 ){` |
|      ! 0 |  9169 | `		return;` |
|        - |  9170 | `	}` |
|      ! 0 |  9171 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9172 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9173 | `	if( pGen->pCtx ){` |
|      ! 0 |  9174 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  9175 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  9176 | `		pGen->pCtx = 0;` |
|      ! 0 |  9177 | `	}` |
|      ! 0 |  9178 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  9179 |  |
|        - |  9180 | `/*` |
|        - |  9181 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  9182 | ` */` |
|      236 |  9183 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  9184 |  |
|        - |  9185 | `	ph7_class_instance *pThis;` |
|        - |  9186 | `	SyString sAttr;` |
|        - |  9187 | `	ph7_value *pAttr;` |
|      238 |  9188 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9189 | `		return 0;` |
|        - |  9190 | `	}` |
|      238 |  9191 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 |  9192 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  9193 | `		return 0;` |
|        - |  9194 | `	}` |
|      238 |  9195 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 |  9196 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 |  9197 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  9198 | `		return 0;` |
|        - |  9199 | `	}` |
|      238 |  9200 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 |  9201 |  |
|        - |  9202 | `/*` |
|        - |  9203 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  9204 | ` */` |
|       22 |  9205 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9206 |  |
|        - |  9207 | `	ph7_generator *pGen;` |
|        - |  9208 | `	sxi32 rc;` |
|       24 |  9209 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 |  9210 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 |  9211 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 |  9212 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 |  9213 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 |  9214 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 |  9215 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 |  9216 | `	}` |
|       24 |  9217 | `	return PH7_OK;` |
|       13 |  9218 |  |
|        - |  9219 | `/*` |
|        - |  9220 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  9221 | ` */` |
|       68 |  9222 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9223 |  |
|        - |  9224 | `	ph7_generator *pGen;` |
|       70 |  9225 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 |  9226 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9227 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 |  9228 | `	return PH7_OK;` |
|       36 |  9229 |  |
|        - |  9230 | `/*` |
|        - |  9231 | ` * Generator::current() — return the last yielded value.` |
|        - |  9232 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9233 | ` */` |
|       68 |  9234 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9235 |  |
|        - |  9236 | `	ph7_generator *pGen;` |
|        - |  9237 | `	sxi32 rc;` |
|       70 |  9238 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9239 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9240 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9241 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9242 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9243 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9244 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9245 | `	}` |
|       70 |  9246 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 |  9247 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 |  9248 | `	}else{` |
|      ! 0 |  9249 | `		ph7_result_null(pCtx);` |
|        - |  9250 | `	}` |
|       70 |  9251 | `	return PH7_OK;` |
|       36 |  9252 |  |
|        - |  9253 | `/*` |
|        - |  9254 | ` * Generator::key() — return the last yielded key.` |
|        - |  9255 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9256 | ` */` |
|       12 |  9257 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9258 |  |
|        - |  9259 | `	ph7_generator *pGen;` |
|        - |  9260 | `	sxi32 rc;` |
|       13 |  9261 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9262 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  9263 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9264 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9265 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9266 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9267 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9268 | `	}` |
|       13 |  9269 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  9270 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  9271 | `	}else{` |
|      ! 0 |  9272 | `		ph7_result_null(pCtx);` |
|        - |  9273 | `	}` |
|       13 |  9274 | `	return PH7_OK;` |
|        7 |  9275 |  |
|        - |  9276 | `/*` |
|        - |  9277 | ` * Generator::next() — advance to the next yield point.` |
|        - |  9278 | ` */` |
|       60 |  9279 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9280 |  |
|        - |  9281 | `	ph7_generator *pGen;` |
|        - |  9282 | `	sxi32 rc;` |
|       62 |  9283 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 |  9284 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 |  9285 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 |  9286 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9287 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 |  9288 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 |  9289 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 |  9290 | `	}else{` |
|      ! 0 |  9291 | `		return PH7_OK;` |
|        - |  9292 | `	}` |
|       62 |  9293 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 |  9294 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 |  9295 | `	return PH7_OK;` |
|       32 |  9296 |  |
|        - |  9297 | `/*` |
|        - |  9298 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - |  9299 | ` */` |
|        4 |  9300 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9301 |  |
|        - |  9302 | `	ph7_generator *pGen;` |
|        - |  9303 | `	ph7_value *pSendVal;` |
|        - |  9304 | `	sxi32 rc;` |
|        5 |  9305 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 |  9306 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 |  9307 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 |  9308 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 |  9309 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - |  9310 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 |  9311 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 |  9312 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 |  9313 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 |  9314 | `	}else{` |
|      ! 0 |  9315 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9316 | `		return PH7_OK;` |
|        - |  9317 | `	}` |
|        5 |  9318 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 |  9319 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 |  9320 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9321 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 |  9322 | `	}else{` |
|        3 |  9323 | `		ph7_result_null(pCtx);` |
|        - |  9324 | `	}` |
|        5 |  9325 | `	return PH7_OK;` |
|        3 |  9326 |  |
|        - |  9327 | `/*` |
|        - |  9328 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - |  9329 | ` *` |
|        - |  9330 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - |  9331 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - |  9332 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - |  9333 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - |  9334 | ` * the exception to the caller.` |
|        - |  9335 | ` */` |
|      ! 0 |  9336 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9337 |  |
|        - |  9338 | `	ph7_generator *pGen;` |
|        - |  9339 | `	const char *zMsg;` |
|        - |  9340 | `	int nLen;` |
|      ! 0 |  9341 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 |  9342 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9343 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 |  9344 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 |  9345 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 |  9346 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9347 | `			"Cannot throw into a closed generator");` |
|        - |  9348 | `	}` |
|        - |  9349 | `	/* Close the generator. Re-throw the exception properly via` |
|        - |  9350 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - |  9351 | `	 * exception dispatch path works correctly. Extract the message` |
|        - |  9352 | `	 * from the passed exception object if possible. */` |
|      ! 0 |  9353 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9354 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 |  9355 | `	nLen = 0;` |
|      ! 0 |  9356 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - |  9357 | `		/* Try to get the exception's message */` |
|        - |  9358 | `		SyString sAttr;` |
|        - |  9359 | `		ph7_value *pMsgAttr;` |
|      ! 0 |  9360 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 |  9361 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 |  9362 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 |  9363 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 |  9364 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 |  9365 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 |  9366 | `		}` |
|      ! 0 |  9367 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 |  9368 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 |  9369 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 |  9370 | `	}` |
|      ! 0 |  9371 | `	(void)nLen;` |
|      ! 0 |  9372 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 |  9373 |  |
|        - |  9374 | `/*` |
|        - |  9375 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - |  9376 | ` */` |
|        2 |  9377 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9378 |  |
|        - |  9379 | `	ph7_generator *pGen;` |
|        3 |  9380 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9381 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 |  9382 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 |  9383 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9384 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - |  9385 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - |  9386 | `	}` |
|        3 |  9387 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 |  9388 | `	return PH7_OK;` |
|        2 |  9389 |  |
|        - |  9390 | `/*` |
|        - |  9391 | ` * Generator::__destruct() — clean up.` |
|        - |  9392 | ` */` |
|      ! 0 |  9393 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9394 |  |
|        - |  9395 | `	ph7_generator *pGen;` |
|      ! 0 |  9396 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 |  9397 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9398 | `	if( pGen ){` |
|      ! 0 |  9399 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 |  9400 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9401 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9402 | `			SyString sAttrName;` |
|        - |  9403 | `			ph7_value *pAttr;` |
|      ! 0 |  9404 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9405 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9406 | `			if( pAttr ){` |
|      ! 0 |  9407 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 |  9408 | `			}` |
|      ! 0 |  9409 | `		}` |
|      ! 0 |  9410 | `	}` |
|      ! 0 |  9411 | `	return PH7_OK;` |
|      ! 0 |  9412 |  |
|        - |  9413 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - |  9414 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - |  9415 | `/*` |
|        - |  9416 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  9417 | ` * the desired message.` |
|        - |  9418 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  9419 | ` * in 'api.c' for additional information.` |
|        - |  9420 | ` */` |
|      370 |  9421 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  9422 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  9423 | `	SyString *pString /* Message to output */` |
|        - |  9424 | `	)` |
|        2 |  9425 |  |
|      372 |  9426 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 |  9427 | `	sxi32 rc = SXRET_OK;` |
|        - |  9428 | `	/* Call the output consumer */` |
|      372 |  9429 | `	if( pString->nByte > 0 ){` |
|      372 |  9430 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 |  9431 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 |  9432 | `	}` |
|      372 |  9433 | `	return rc;` |
|        2 |  9434 |  |
|        - |  9435 | `/*` |
|        - |  9436 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  9437 | ` * callback to consume the formatted message.` |
|        - |  9438 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  9439 | ` * in 'api.c' for additional information.` |
|        - |  9440 | ` */` |
|        2 |  9441 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  9442 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  9443 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  9444 | `	va_list ap           /* Variable list of arguments */` |
|        - |  9445 | `	)` |
|        1 |  9446 |  |
|        3 |  9447 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  9448 | `	sxi32 rc = SXRET_OK;` |
|        - |  9449 | `	SyBlob sWorker;` |
|        - |  9450 | `	/* Format the message and call the output consumer */` |
|        3 |  9451 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  9452 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  9453 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  9454 | `		/* Consume the formatted message */` |
|        3 |  9455 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  9456 | `	}` |
|        3 |  9457 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - |  9458 | `	/* Release the working buffer */` |
|        3 |  9459 | `	SyBlobRelease(&sWorker);` |
|        3 |  9460 | `	return rc;` |
|        1 |  9461 |  |
|        - |  9462 | `/*` |
|        - |  9463 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  9464 | ` * This function never fail and always return a pointer` |
|        - |  9465 | ` * to a null terminated string.` |
|        - |  9466 | ` */` |
|       12 |  9467 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  9468 |  |
|       13 |  9469 | `	const char *zOp = "Unknown     ";` |
|       13 |  9470 | `	switch(nOp){` |
|        3 |  9471 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  9472 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  9473 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  9474 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  9475 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  9476 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  9477 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  9478 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  9479 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  9480 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  9481 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  9482 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  9483 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  9484 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  9485 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  9486 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  9487 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  9488 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  9489 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  9490 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  9491 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  9492 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  9493 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  9494 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  9495 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  9496 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  9497 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  9498 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  9499 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  9500 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  9501 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  9502 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  9503 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  9504 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  9505 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 |  9506 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  9507 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  9508 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  9509 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  9510 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  9511 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  9512 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  9513 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  9514 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  9515 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  9516 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  9517 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  9518 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  9519 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  9520 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  9521 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  9522 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  9523 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 |  9524 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  9525 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  9526 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 |  9527 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 |  9528 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 |  9529 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 |  9530 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 |  9531 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  9532 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  9533 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  9534 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  9535 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  9536 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  9537 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  9538 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  9539 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  9540 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  9541 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  9542 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  9543 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  9544 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  9545 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  9546 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  9547 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  9548 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  9549 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  9550 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  9551 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  9552 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  9553 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  9554 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  9555 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  9556 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  9557 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  9558 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  9559 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  9560 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  9561 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 |  9562 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  9563 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  9564 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  9565 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  9566 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  9567 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  9568 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  9569 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  9570 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  9571 | `	default:` |
|      ! 0 |  9572 | `		break;` |
|        - |  9573 | `	}` |
|       13 |  9574 | `	return zOp;` |
|        1 |  9575 |  |
|        - |  9576 | `/*` |
|        - |  9577 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  9578 | ` * The xConsumer() callback which is an used defined function` |
|        - |  9579 | ` * is responsible of consuming the generated dump.` |
|        - |  9580 | ` */` |
|        2 |  9581 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  9582 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  9583 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  9584 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  9585 | `	)` |
|        1 |  9586 |  |
|        - |  9587 | `	sxi32 rc;` |
|        3 |  9588 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  9589 | `	return rc;` |
|        1 |  9590 |  |
|        - |  9591 | `/*` |
|        - |  9592 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  9593 | ` * outside a class body [i.e: global or function scope].` |
|        - |  9594 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  9595 | ` * in 'compile.c' for additional information.` |
|        - |  9596 | ` */` |
|       14 |  9597 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  9598 |  |
|       15 |  9599 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  9600 | `	/* Evaluate and expand constant value */` |
|       15 |  9601 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 |  9602 |  |
|        - |  9603 | `/*` |
|        - |  9604 | ` * Section:` |
|        - |  9605 | ` *  Function handling functions.` |
|        - |  9606 | ` * Status:` |
|        - |  9607 | ` *    Stable.` |
|        - |  9608 | ` */` |
|        - |  9609 | `/*` |
|        - |  9610 | ` * int func_num_args(void)` |
|        - |  9611 | ` *   Returns the number of arguments passed to the function.` |
|        - |  9612 | ` * Parameters` |
|        - |  9613 | ` *   None.` |
|        - |  9614 | ` * Return` |
|        - |  9615 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  9616 | ` *  or -1 if called from the globe scope.` |
|        - |  9617 | ` */` |
|      944 |  9618 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9619 |  |
|        - |  9620 | `	VmFrame *pFrame;` |
|        - |  9621 | `	ph7_vm *pVm;` |
|        - |  9622 | `	/* Point to the target VM */` |
|      946 |  9623 | `	pVm = pCtx->pVm;` |
|        - |  9624 | `	/* Current frame */` |
|      946 |  9625 | `	pFrame = pVm->pFrame;` |
|      946 |  9626 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      946 |  9627 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  9628 | `		SXUNUSED(nArg);` |
|      ! 0 |  9629 | `		SXUNUSED(apArg);` |
|        - |  9630 | `		/* Global frame,return -1 */` |
|      ! 0 |  9631 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  9632 | `		return SXRET_OK;` |
|        - |  9633 | `	}` |
|        - |  9634 | `	/* Total number of arguments passed to the enclosing function */` |
|      946 |  9635 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      946 |  9636 | `	ph7_result_int(pCtx,nArg);` |
|      946 |  9637 | `	return SXRET_OK;` |
|      474 |  9638 |  |
|        - |  9639 | `/*` |
|        - |  9640 | ` * value func_get_arg(int $arg_num)` |
|        - |  9641 | ` *   Return an item from the argument list.` |
|        - |  9642 | ` * Parameters` |
|        - |  9643 | ` *  Argument number(index start from zero).` |
|        - |  9644 | ` * Return` |
|        - |  9645 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  9646 | ` */` |
|       22 |  9647 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9648 |  |
|       24 |  9649 | `	ph7_value *pObj = 0;` |
|       24 |  9650 | `	VmSlot *pSlot = 0;` |
|        - |  9651 | `	VmFrame *pFrame;` |
|        - |  9652 | `	ph7_vm *pVm;` |
|        - |  9653 | `	/* Point to the target VM */` |
|       24 |  9654 | `	pVm = pCtx->pVm;` |
|        - |  9655 | `	/* Current frame */` |
|       24 |  9656 | `	pFrame = pVm->pFrame;` |
|       24 |  9657 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  9658 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  9659 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  9660 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  9661 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9662 | `		return SXRET_OK;` |
|        - |  9663 | `	}` |
|        - |  9664 | `	/* Extract the desired index */` |
|       21 |  9665 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  9666 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  9667 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  9668 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9669 | `		return SXRET_OK;` |
|        - |  9670 | `	}` |
|        - |  9671 | `	/* Extract the desired argument */` |
|       21 |  9672 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  9673 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  9674 | `			/* Return the desired argument */` |
|       21 |  9675 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  9676 | `		}else{` |
|        - |  9677 | `			/* No such argument,return false */` |
|      ! 0 |  9678 | `			ph7_result_bool(pCtx,0);` |
|        - |  9679 | `		}` |
|       11 |  9680 | `	}else{` |
|        - |  9681 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  9682 | `		ph7_result_bool(pCtx,0);` |
|        - |  9683 | `	}` |
|       21 |  9684 | `	return SXRET_OK;` |
|       13 |  9685 |  |
|        - |  9686 | `/*` |
|        - |  9687 | ` * array func_get_args_byref(void)` |
|        - |  9688 | ` *   Returns an array comprising a function's argument list.` |
|        - |  9689 | ` * Parameters` |
|        - |  9690 | ` *  None.` |
|        - |  9691 | ` * Return` |
|        - |  9692 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  9693 | ` *  member of the current user-defined function's argument list.` |
|        - |  9694 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9695 | ` * NOTE:` |
|        - |  9696 | ` *  Arguments are returned to the array by reference.` |
|        - |  9697 | ` */` |
|        2 |  9698 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9699 |  |
|        - |  9700 | `	ph7_value *pArray;` |
|        - |  9701 | `	VmFrame *pFrame;` |
|        - |  9702 | `	VmSlot *aSlot;` |
|        - |  9703 | `	sxu32 n;` |
|        - |  9704 | `	/* Point to the current frame */` |
|        3 |  9705 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  9706 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  9707 | `	if( pFrame->pParent == 0 ){` |
|        - |  9708 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9709 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9710 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9711 | `		return SXRET_OK;` |
|        - |  9712 | `	}` |
|        - |  9713 | `	/* Create a new array */` |
|        3 |  9714 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9715 | `	if( pArray == 0 ){` |
|      ! 0 |  9716 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9717 | `		SXUNUSED(apArg);` |
|      ! 0 |  9718 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9719 | `		return SXRET_OK;` |
|        - |  9720 | `	}` |
|        - |  9721 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  9722 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  9723 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  9724 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  9725 | `	}` |
|        - |  9726 | `	/* Return the freshly created array */` |
|        3 |  9727 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9728 | `	return SXRET_OK;` |
|        2 |  9729 |  |
|        - |  9730 | `/*` |
|        - |  9731 | ` * array func_get_args(void)` |
|        - |  9732 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  9733 | ` * Parameters` |
|        - |  9734 | ` *  None.` |
|        - |  9735 | ` * Return` |
|        - |  9736 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  9737 | ` *  member of the current user-defined function's argument list.` |
|        - |  9738 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  9739 | ` */` |
|       88 |  9740 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9741 |  |
|       90 |  9742 | `	ph7_value *pObj = 0;` |
|        - |  9743 | `	ph7_value *pArray;` |
|        - |  9744 | `	VmFrame *pFrame;` |
|        - |  9745 | `	VmSlot *aSlot;` |
|        - |  9746 | `	sxu32 n;` |
|        - |  9747 | `	/* Point to the current frame */` |
|       90 |  9748 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 |  9749 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 |  9750 | `	if( pFrame->pParent == 0 ){` |
|        - |  9751 | `		/* Global frame,return FALSE */` |
|      ! 0 |  9752 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  9753 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9754 | `		return SXRET_OK;` |
|        - |  9755 | `	}` |
|        - |  9756 | `	/* Create a new array */` |
|       90 |  9757 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 |  9758 | `	if( pArray == 0 ){` |
|      ! 0 |  9759 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9760 | `		SXUNUSED(apArg);` |
|      ! 0 |  9761 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9762 | `		return SXRET_OK;` |
|        - |  9763 | `	}` |
|        - |  9764 | `	/* Start filling the array with the given arguments */` |
|       90 |  9765 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 |  9766 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 |  9767 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 |  9768 | `		if( pObj ){` |
|      134 |  9769 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 |  9770 | `		}` |
|       68 |  9771 | `	}` |
|        - |  9772 | `	/* Return the freshly created array */` |
|       90 |  9773 | `	ph7_result_value(pCtx,pArray);` |
|       90 |  9774 | `	return SXRET_OK;` |
|       46 |  9775 |  |
|        - |  9776 | `/*` |
|        - |  9777 | ` * bool function_exists(string $name)` |
|        - |  9778 | ` *  Return TRUE if the given function has been defined.` |
|        - |  9779 | ` * Parameters` |
|        - |  9780 | ` *  The name of the desired function.` |
|        - |  9781 | ` * Return` |
|        - |  9782 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  9783 | ` */` |
|     1680 |  9784 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9785 |  |
|        - |  9786 | `	const char *zName;` |
|        - |  9787 | `	ph7_vm *pVm;` |
|        - |  9788 | `	int nLen;` |
|        - |  9789 | `	int res;` |
|     1682 |  9790 | `	if( nArg < 1 ){` |
|        - |  9791 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  9792 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9793 | `		return SXRET_OK;` |
|        - |  9794 | `	}` |
|        - |  9795 | `	/* Point to the target VM */` |
|     1682 |  9796 | `	pVm = pCtx->pVm;` |
|        - |  9797 | `	/* Extract the function name */` |
|     1682 |  9798 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  9799 | `	/* Assume the function is not defined */` |
|     1682 |  9800 | `	res = 0;` |
|        - |  9801 | `	/* Perform the lookup */` |
|     2520 |  9802 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1676 |  9803 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9804 | `			/* Function is defined */` |
|      206 |  9805 | `			res = 1;` |
|      102 |  9806 | `	}` |
|     1682 |  9807 | `	ph7_result_bool(pCtx,res);` |
|     1682 |  9808 | `	return SXRET_OK;` |
|      842 |  9809 |  |
|        - |  9810 | `/*` |
|        - |  9811 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9812 | ` * [i.e: Whether it is callable or not].` |
|        - |  9813 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  9814 | ` */` |
|    19892 |  9815 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  9816 |  |
|    19894 |  9817 | `	int res = 0;` |
|    19894 |  9818 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  9819 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  9820 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  9821 | `		ph7_class_method *pMethod;` |
|      ! 0 |  9822 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  9823 | `		if( pMethod && CallInvoke ){` |
|        - |  9824 | `			ph7_value sResult;` |
|        - |  9825 | `			sxi32 rc;` |
|        - |  9826 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  9827 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  9828 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  9829 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  9830 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  9831 | `			}` |
|      ! 0 |  9832 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9833 | `		}` |
|    19894 |  9834 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  9835 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  9836 | `		if( pMap->nEntry == 2 ){` |
|        - |  9837 | `			ph7_class *pClass;` |
|        - |  9838 | `			ph7_value *pV;` |
|        - |  9839 | `			/* Extract the target class */` |
|       12 |  9840 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  9841 | `			if( pV ){` |
|       12 |  9842 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  9843 | `				if( pClass ){` |
|        - |  9844 | `					ph7_class_method *pMethod;` |
|        - |  9845 | `					/* Extract the target method */` |
|       10 |  9846 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  9847 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  9848 | `						/* Perform the lookup */` |
|       10 |  9849 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  9850 | `						if( pMethod ){` |
|        - |  9851 | `							/* Method is callable */` |
|        5 |  9852 | `							res = 1;` |
|        2 |  9853 | `						}` |
|        4 |  9854 | `					}` |
|        4 |  9855 | `				}` |
|        5 |  9856 | `			}` |
|        7 |  9857 | `		}` |
|    19881 |  9858 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  9859 | `		const char *zName;` |
|        - |  9860 | `		int nLen;` |
|        - |  9861 | `		/* Extract the name */` |
|     5360 |  9862 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  9863 | `		/* Perform the lookup */` |
|     5375 |  9864 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  9865 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  9866 | `				/* Function is callable */` |
|     5342 |  9867 | `				res = 1;` |
|     2670 |  9868 | `		}` |
|     2679 |  9869 | `	}` |
|    19894 |  9870 | `	return res;` |
|        2 |  9871 |  |
|        - |  9872 | `/*` |
|        - |  9873 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  9874 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  9875 | ` * Parameters` |
|        - |  9876 | ` * $name` |
|        - |  9877 | ` *    The callback function to check` |
|        - |  9878 | ` * $syntax_only` |
|        - |  9879 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  9880 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  9881 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  9882 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  9883 | ` *    a string.` |
|        - |  9884 | ` * Return` |
|        - |  9885 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  9886 | ` */` |
|       14 |  9887 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9888 |  |
|        - |  9889 | `	ph7_vm *pVm;` |
|        - |  9890 | `	int res;` |
|       15 |  9891 | `	if( nArg < 1 ){` |
|        - |  9892 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  9893 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9894 | `		return SXRET_OK;` |
|        - |  9895 | `	}` |
|        - |  9896 | `	/* Point to the target VM */` |
|       15 |  9897 | `	pVm = pCtx->pVm;` |
|        - |  9898 | `	/* Perform the requested operation */` |
|       15 |  9899 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  9900 | `	ph7_result_bool(pCtx,res);` |
|       15 |  9901 | `	return SXRET_OK;` |
|        8 |  9902 |  |
|        - |  9903 | `/*` |
|        - |  9904 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  9905 | ` * defined below.` |
|        - |  9906 | ` */` |
|     1200 |  9907 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  9908 |  |
|     1201 |  9909 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  9910 | `	ph7_value sName;` |
|        - |  9911 | `	sxi32 rc;` |
|        - |  9912 | `	/* Prepare the function name for insertion */` |
|     1201 |  9913 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1201 |  9914 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  9915 | `	/* Perform the insertion */` |
|     1201 |  9916 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1201 |  9917 | `	PH7_MemObjRelease(&sName);` |
|     1201 |  9918 | `	return rc;` |
|        1 |  9919 |  |
|        - |  9920 | `/*` |
|        - |  9921 | ` * array get_defined_functions(void)` |
|        - |  9922 | ` *  Returns an array of all defined functions.` |
|        - |  9923 | ` * Parameter` |
|        - |  9924 | ` *  None.` |
|        - |  9925 | ` * Return` |
|        - |  9926 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  9927 | ` *  both built-in (internal) and user-defined.` |
|        - |  9928 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  9929 | ` *  defined ones using $arr["user"].` |
|        - |  9930 | ` * Note:` |
|        - |  9931 | ` *  NULL is returned on failure.` |
|        - |  9932 | ` */` |
|        2 |  9933 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9934 |  |
|        - |  9935 | `	ph7_value *pArray,*pEntry;` |
|        - |  9936 | `	/* NOTE:` |
|        - |  9937 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  9938 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  9939 | `	 */` |
|        3 |  9940 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9941 | ` 	if( pArray == 0 ){` |
|      ! 0 |  9942 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9943 | `		SXUNUSED(apArg);` |
|        - |  9944 | `		/* Return NULL */` |
|      ! 0 |  9945 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9946 | `		return SXRET_OK;` |
|        - |  9947 | `	}` |
|        3 |  9948 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9949 | `	if( pEntry == 0 ){` |
|        - |  9950 | `		/* Return NULL */` |
|      ! 0 |  9951 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9952 | `		return SXRET_OK;` |
|        - |  9953 | `	}` |
|        - |  9954 | `	/* Fill with the appropriate information */` |
|        3 |  9955 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  9956 | `	/* Create the 'internal' index */` |
|        3 |  9957 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  9958 | `	/* Create the user-func array */` |
|        3 |  9959 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  9960 | `	if( pEntry == 0 ){` |
|        - |  9961 | `		/* Return NULL */` |
|      ! 0 |  9962 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9963 | `		return SXRET_OK;` |
|        - |  9964 | `	}` |
|        - |  9965 | `	/* Fill with the appropriate information */` |
|        3 |  9966 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  9967 | `	/* Create the 'user' index */` |
|        3 |  9968 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  9969 | `	/* Return the multi-dimensional array */` |
|        3 |  9970 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9971 | `	return SXRET_OK;` |
|        2 |  9972 |  |
|        - |  9973 | `/*` |
|        - |  9974 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  9975 | ` *  Register a function for execution on shutdown.` |
|        - |  9976 | ` * Note` |
|        - |  9977 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  9978 | ` *  be called in the same order as they were registered.` |
|        - |  9979 | ` * Parameters` |
|        - |  9980 | ` *  $callback` |
|        - |  9981 | ` *   The shutdown callback to register.` |
|        - |  9982 | ` * $param` |
|        - |  9983 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  9984 | ` * Return` |
|        - |  9985 | ` *  Nothing.` |
|        - |  9986 | ` */` |
|        2 |  9987 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9988 |  |
|        - |  9989 | `	VmShutdownCB sEntry;` |
|        - |  9990 | `	int i,j;` |
|        3 |  9991 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  9992 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  9993 | `		return PH7_OK;` |
|        - |  9994 | `	}` |
|        - |  9995 | `	/* Zero the Entry */` |
|        3 |  9996 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  9997 | `	/* Initialize fields */` |
|        3 |  9998 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  9999 | `	/* Save the callback name for later invocation name */` |
|        3 | 10000 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 10001 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 10002 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 10003 | `	}` |
|        - | 10004 | `	/* Copy arguments */` |
|        3 | 10005 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 10006 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 10007 | `			/* Limit reached */` |
|      ! 0 | 10008 | `			break;` |
|        - | 10009 | `		}` |
|      ! 0 | 10010 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 10011 | `	}` |
|        3 | 10012 | `	sEntry.nArg = j;` |
|        - | 10013 | `	/* Install the callback */` |
|        3 | 10014 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 10015 | `	return PH7_OK;` |
|        2 | 10016 |  |
|        - | 10017 | `/*` |
|        - | 10018 | ` * Section:` |
|        - | 10019 | ` *  Class handling functions.` |
|        - | 10020 | ` * Status:` |
|        - | 10021 | ` *    Stable.` |
|        - | 10022 | ` */` |
|        - | 10023 | `/*` |
|        - | 10024 | ` * Extract the top active class. NULL is returned` |
|        - | 10025 | ` * if the class stack is empty.` |
|        - | 10026 | ` */` |
|      674 | 10027 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 10028 |  |
|      676 | 10029 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 10030 | `	ph7_class **apClass;` |
|      676 | 10031 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 10032 | `		/* Empty stack,return NULL */` |
|       15 | 10033 | `		return 0;` |
|        - | 10034 | `	}` |
|        - | 10035 | `	/* Peek the last entry */` |
|      662 | 10036 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      662 | 10037 | `	return apClass[pSet->nUsed - 1];` |
|      339 | 10038 |  |
|        - | 10039 | `/*` |
|        - | 10040 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 10041 | ` *   Get the class that declared the currently executing method.` |
|        - | 10042 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 10043 | ` *` |
|        - | 10044 | ` * Parameters` |
|        - | 10045 | ` *   pVm: Target VM` |
|        - | 10046 | ` *` |
|        - | 10047 | ` * Return` |
|        - | 10048 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 10049 | ` *   - Not executing within a class method` |
|        - | 10050 | ` *` |
|        - | 10051 | ` * Note` |
|        - | 10052 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 10053 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 10054 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 10055 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 10056 | ` *   declaring class.` |
|        - | 10057 | ` */` |
|       96 | 10058 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 10059 |  |
|       98 | 10060 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10061 | `	ph7_vm_func *pVmFunc;` |
|        - | 10062 |  |
|        - | 10063 | `	/* Skip exception frames to find the actual method frame */` |
|       98 | 10064 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 10065 |  |
|        - | 10066 | `	/* Check if we're in a method context */` |
|       98 | 10067 | `	if( pFrame->pParent ){` |
|       94 | 10068 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       94 | 10069 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 10070 | `			/* Return the declaring class */` |
|       94 | 10071 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 10072 | `		}` |
|      ! 0 | 10073 | `	}` |
|        - | 10074 |  |
|        5 | 10075 | `	return 0;` |
|       50 | 10076 |  |
|        - | 10077 |  |
|        - | 10078 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 10079 | `/*` |
|        - | 10080 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 10081 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 10082 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 10083 | ` * return value indicates failure.` |
|        - | 10084 | ` */` |
|        - | 10085 | `/*` |
|        - | 10086 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 10087 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 10088 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 10089 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 10090 | ` */` |
|     1684 | 10091 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 10092 | `	ph7_vm *pVm,` |
|        - | 10093 | `	ph7_class_instance *pThis,` |
|        - | 10094 | `	ph7_class_method *pMethod,` |
|        - | 10095 | `	ph7_value *pResult,` |
|        - | 10096 | `	int nArg,` |
|        - | 10097 | `	ph7_value **apArg,` |
|        - | 10098 | `	VmCallArgMap *pMap` |
|        - | 10099 | `	)` |
|        2 | 10100 |  |
|        - | 10101 | `	ph7_value *aStack;` |
|        - | 10102 | `	VmInstr aInstr[2];` |
|        - | 10103 | `	int iCursor;` |
|        - | 10104 | `	int i;` |
|     1686 | 10105 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     1686 | 10106 | `	if( aStack == 0 ){` |
|      ! 0 | 10107 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10108 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10109 | `		return SXERR_MEM;` |
|        - | 10110 | `	}` |
|     2446 | 10111 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      762 | 10112 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|      762 | 10113 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      382 | 10114 | `	}` |
|     1686 | 10115 | `	iCursor = nArg + 1;` |
|     1686 | 10116 | `	if( pThis ){` |
|     1680 | 10117 | `		pThis->iRef++;` |
|     1680 | 10118 | `		aStack[i].x.pOther = pThis;` |
|     1680 | 10119 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      839 | 10120 | `	}` |
|     1686 | 10121 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1686 | 10122 | `	i++;` |
|     1686 | 10123 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1686 | 10124 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1686 | 10125 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1686 | 10126 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     1686 | 10127 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1686 | 10128 | `	aInstr[0].iP1 = nArg;` |
|     1686 | 10129 | `	aInstr[0].iP2 = 0;` |
|     1686 | 10130 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     1686 | 10131 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1686 | 10132 | `	aInstr[1].iP1 = 1;` |
|     1686 | 10133 | `	aInstr[1].iP2 = 0;` |
|     1686 | 10134 | `	aInstr[1].p3  = 0;` |
|     1686 | 10135 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0);` |
|     1686 | 10136 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1686 | 10137 | `	return PH7_OK;` |
|      844 | 10138 |  |
|     1508 | 10139 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10140 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10141 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10142 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10143 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10144 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10145 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10146 | `	)` |
|        2 | 10147 |  |
|     1510 | 10148 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10149 |  |
|        - | 10150 | `/*` |
|        - | 10151 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 10152 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 10153 | ` * in the apArg[] array.` |
|        - | 10154 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10155 | ` * return value indicates failure.` |
|        - | 10156 | ` */` |
|      968 | 10157 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 10158 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10159 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10160 | `	int nArg,          /* Total number of given arguments */` |
|        - | 10161 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 10162 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 10163 | `	)` |
|        2 | 10164 |  |
|        - | 10165 | `	ph7_value *aStack;` |
|        - | 10166 | `	VmInstr aInstr[2];` |
|        - | 10167 | `	int i;` |
|      970 | 10168 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10169 | `		/* Don't bother processing,it's invalid anyway */` |
|      479 | 10170 | `		if( pResult ){` |
|        - | 10171 | `			/* Assume a null return value */` |
|      ! 0 | 10172 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10173 | `		}` |
|      479 | 10174 | `		return SXERR_INVALID;` |
|        - | 10175 | `	}` |
|      492 | 10176 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 10177 | `		/* Class method */` |
|       11 | 10178 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 10179 | `		ph7_class_method *pMethod = 0;` |
|       11 | 10180 | `		ph7_class_instance *pThis = 0;` |
|       11 | 10181 | `		ph7_class *pClass = 0;` |
|        - | 10182 | `		ph7_value *pValue;` |
|        - | 10183 | `		sxi32 rc;` |
|       11 | 10184 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 10185 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 10186 | `			if( pResult ){` |
|        - | 10187 | `				/* Assume a null return value */` |
|      ! 0 | 10188 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10189 | `			}` |
|      ! 0 | 10190 | `			return SXRET_OK;` |
|        - | 10191 | `		}` |
|        - | 10192 | `		/* Extract the class name or an instance of it */` |
|       11 | 10193 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 10194 | `		if( pValue ){` |
|       11 | 10195 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 10196 | `		}` |
|       11 | 10197 | `		if( pClass == 0 ){` |
|        - | 10198 | `			/* No such class,return NULL */` |
|      ! 0 | 10199 | `			if( pResult ){` |
|      ! 0 | 10200 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10201 | `			}` |
|      ! 0 | 10202 | `			return SXRET_OK;` |
|        - | 10203 | `		}` |
|       11 | 10204 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10205 | `			/* Point to the class instance */` |
|        5 | 10206 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 10207 | `		}` |
|        - | 10208 | `		/* Try to extract the method */` |
|       11 | 10209 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 10210 | `		if( pValue ){` |
|       11 | 10211 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 10212 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 10213 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 10214 | `			}` |
|        5 | 10215 | `		}` |
|       11 | 10216 | `		if( pMethod == 0 ){` |
|        - | 10217 | `			/* No such method,return NULL */` |
|      ! 0 | 10218 | `			if( pResult ){` |
|      ! 0 | 10219 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10220 | `			}` |
|      ! 0 | 10221 | `			return SXRET_OK;` |
|        - | 10222 | `		}` |
|        - | 10223 | `		/* Call the class method */` |
|       11 | 10224 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 10225 | `		return rc;` |
|        - | 10226 | `	}` |
|        - | 10227 | `	/* Create a new operand stack */` |
|      482 | 10228 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      482 | 10229 | `	if( aStack == 0 ){` |
|      ! 0 | 10230 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10231 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 10232 | `		if( pResult ){` |
|        - | 10233 | `			/* Assume a null return value */` |
|      ! 0 | 10234 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10235 | `		}` |
|      ! 0 | 10236 | `		return SXERR_MEM;` |
|        - | 10237 | `	}` |
|        - | 10238 | `	/* Fill the operand stack with the given arguments */` |
|     1544 | 10239 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1064 | 10240 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 10241 | `		/*` |
|        - | 10242 | `		 * Symisc eXtension:` |
|        - | 10243 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 10244 | `		 */` |
|     1064 | 10245 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      533 | 10246 | `	}` |
|        - | 10247 | `	/* Push the function name */` |
|      482 | 10248 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      482 | 10249 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 10250 | `	/* Emit the CALL istruction */` |
|      482 | 10251 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      482 | 10252 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      482 | 10253 | `	aInstr[0].iP2 = 0;` |
|      482 | 10254 | `	aInstr[0].p3  = 0;` |
|        - | 10255 | `	/* Emit the DONE instruction */` |
|      482 | 10256 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      482 | 10257 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      482 | 10258 | `	aInstr[1].iP2 = 0;` |
|      482 | 10259 | `	aInstr[1].p3  = 0;` |
|        - | 10260 | `	/* Execute the function body (if available) */` |
|      482 | 10261 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0);` |
|        - | 10262 | `	/* Clean up the mess left behind */` |
|      482 | 10263 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      482 | 10264 | `	return PH7_OK;` |
|      486 | 10265 |  |
|        - | 10266 | `/*` |
|        - | 10267 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 10268 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 10269 | ` * parameter.` |
|        - | 10270 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10271 | ` * return value indicates failure.` |
|        - | 10272 | ` */` |
|      236 | 10273 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 10274 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10275 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10276 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 10277 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 10278 | `	)` |
|        1 | 10279 |  |
|        - | 10280 | `	ph7_value *pArg;` |
|        - | 10281 | `	SySet aArg;` |
|        - | 10282 | `	va_list ap;` |
|        - | 10283 | `	sxi32 rc;` |
|      237 | 10284 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 10285 | `	/* Copy arguments one after one */` |
|      237 | 10286 | `	va_start(ap,pResult);` |
|      393 | 10287 | `	for(;;){` |
|      787 | 10288 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 10289 | `		if( pArg == 0 ){` |
|      237 | 10290 | `			break;` |
|        - | 10291 | `		}` |
|      551 | 10292 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 10293 | `	}` |
|        - | 10294 | `	/* Call the core routine */` |
|      237 | 10295 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 10296 | `	/* Cleanup */` |
|      237 | 10297 | `	SySetRelease(&aArg);` |
|      237 | 10298 | `	return rc;` |
|        1 | 10299 |  |
|        - | 10300 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 10301 | `/*` |
|        - | 10302 | ` * bool defined(string $name)` |
|        - | 10303 | ` *  Checks whether a given named constant exists.` |
|        - | 10304 | ` * Parameter:` |
|        - | 10305 | ` *  Name of the desired constant.` |
|        - | 10306 | ` * Return` |
|        - | 10307 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 10308 | ` */` |
|       14 | 10309 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10310 |  |
|        - | 10311 | `	const char *zName;` |
|       16 | 10312 | `	int nLen = 0;` |
|       16 | 10313 | `	int res = 0;` |
|       16 | 10314 | `	if( nArg < 1 ){` |
|        - | 10315 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 10316 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 10317 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10318 | `		return SXRET_OK;` |
|        - | 10319 | `	}` |
|        - | 10320 | `	/* Extract constant name */` |
|       16 | 10321 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10322 | `	/* Perform the lookup */` |
|       16 | 10323 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10324 | `		/* Already defined */` |
|       10 | 10325 | `		res = 1;` |
|        4 | 10326 | `	}` |
|       16 | 10327 | `	ph7_result_bool(pCtx,res);` |
|       16 | 10328 | `	return SXRET_OK;` |
|        9 | 10329 |  |
|        - | 10330 | `/*` |
|        - | 10331 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 10332 | ` * below.` |
|        - | 10333 | ` */` |
|       10 | 10334 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 10335 |  |
|       12 | 10336 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 10337 | `	/* Expand constant value */` |
|       12 | 10338 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 10339 |  |
|        - | 10340 | `/*` |
|        - | 10341 | ` * bool define(string $constant_name,expression value)` |
|        - | 10342 | ` *  Defines a named constant at runtime.` |
|        - | 10343 | ` * Parameter:` |
|        - | 10344 | ` *  $constant_name` |
|        - | 10345 | ` *   The name of the constant` |
|        - | 10346 | ` *  $value` |
|        - | 10347 | ` *   Constant value` |
|        - | 10348 | ` * Return:` |
|        - | 10349 | ` *   TRUE on success,FALSE on failure.` |
|        - | 10350 | ` */` |
|       12 | 10351 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10352 |  |
|        - | 10353 | `	const char *zName;  /* Constant name */` |
|        - | 10354 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 10355 | `	int nLen = 0;       /* Name length */` |
|        - | 10356 | `	sxi32 rc;` |
|       14 | 10357 | `	if( nArg < 2 ){` |
|        - | 10358 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 10359 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 10360 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10361 | `		return SXRET_OK;` |
|        - | 10362 | `	}` |
|       14 | 10363 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 10364 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 10365 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10366 | `		return SXRET_OK;` |
|        - | 10367 | `	}` |
|        - | 10368 | `	/* Extract constant name */` |
|       14 | 10369 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 10370 | `	if( nLen < 1 ){` |
|      ! 0 | 10371 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 10372 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10373 | `		return SXRET_OK;` |
|        - | 10374 | `	}` |
|        - | 10375 | `	/* Duplicate constant value */` |
|       14 | 10376 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 10377 | `	if( pValue == 0 ){` |
|      ! 0 | 10378 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10379 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10380 | `		return SXRET_OK;` |
|        - | 10381 | `	}` |
|        - | 10382 | `	/* Initialize the memory object */` |
|       14 | 10383 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 10384 | `	/* Register the constant */` |
|       14 | 10385 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 10386 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10387 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 10388 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 10389 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10390 | `		return SXRET_OK;` |
|        - | 10391 | `	}` |
|        - | 10392 | `	/* Duplicate constant value */` |
|       14 | 10393 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 10394 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 10395 | `		/* Lower case the constant name */` |
|      ! 0 | 10396 | `		char *zCur = (char *)zName;` |
|      ! 0 | 10397 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 10398 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 10399 | `				/* UTF-8 stream */` |
|      ! 0 | 10400 | `				zCur++;` |
|      ! 0 | 10401 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 10402 | `					zCur++;` |
|      ! 0 | 10403 | `				}` |
|      ! 0 | 10404 | `				continue;` |
|        - | 10405 | `			}` |
|      ! 0 | 10406 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 10407 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 10408 | `				zCur[0] = (char)c;` |
|      ! 0 | 10409 | `			}` |
|      ! 0 | 10410 | `			zCur++;` |
|      ! 0 | 10411 | `		}` |
|        - | 10412 | `		/* Finally,register the constant */` |
|      ! 0 | 10413 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 10414 | `	}` |
|        - | 10415 | `	/* All done,return TRUE */` |
|       14 | 10416 | `	ph7_result_bool(pCtx,1);` |
|       14 | 10417 | `	return SXRET_OK;` |
|        8 | 10418 |  |
|        - | 10419 | `/*` |
|        - | 10420 | ` * value constant(string $name)` |
|        - | 10421 | ` *  Returns the value of a constant` |
|        - | 10422 | ` * Parameter` |
|        - | 10423 | ` *  $name` |
|        - | 10424 | ` *    Name of the constant.` |
|        - | 10425 | ` * Return` |
|        - | 10426 | ` *  Constant value or NULL if not defined.` |
|        - | 10427 | ` */` |
|        8 | 10428 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10429 |  |
|        - | 10430 | `	SyHashEntry *pEntry;` |
|        - | 10431 | `	ph7_constant *pCons;` |
|        - | 10432 | `	const char *zName; /* Constant name */` |
|        - | 10433 | `	ph7_value sVal;    /* Constant value */` |
|        - | 10434 | `	int nLen;` |
|       10 | 10435 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 10436 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 10437 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 10438 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10439 | `		return SXRET_OK;` |
|        - | 10440 | `	}` |
|        - | 10441 | `	/* Extract the constant name */` |
|       10 | 10442 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10443 | `	/* Perform the query */` |
|       10 | 10444 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 10445 | `	if( pEntry == 0 ){` |
|        3 | 10446 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 10447 | `		ph7_result_null(pCtx);` |
|        3 | 10448 | `		return SXRET_OK;` |
|        - | 10449 | `	}` |
|        8 | 10450 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 10451 | `	/* Point to the structure that describe the constant */` |
|        8 | 10452 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 10453 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 10454 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 10455 | `	/* Return that value */` |
|        8 | 10456 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 10457 | `	/* Cleanup */` |
|        8 | 10458 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 10459 | `	return SXRET_OK;` |
|        6 | 10460 |  |
|        - | 10461 | `/*` |
|        - | 10462 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 10463 | ` * defined below.` |
|        - | 10464 | ` */` |
|      452 | 10465 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10466 |  |
|      453 | 10467 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10468 | `	ph7_value sName;` |
|        - | 10469 | `	sxi32 rc;` |
|        - | 10470 | `	/* Prepare the constant name for insertion */` |
|      453 | 10471 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 10472 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10473 | `	/* Perform the insertion */` |
|      453 | 10474 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 10475 | `	PH7_MemObjRelease(&sName);` |
|      453 | 10476 | `	return rc;` |
|        1 | 10477 |  |
|        - | 10478 | `/*` |
|        - | 10479 | ` * array get_defined_constants(void)` |
|        - | 10480 | ` *  Returns an associative array with the names of all defined` |
|        - | 10481 | ` *  constants.` |
|        - | 10482 | ` * Parameters` |
|        - | 10483 | ` *  NONE.` |
|        - | 10484 | ` * Returns` |
|        - | 10485 | ` *  Returns the names of all the constants currently defined.` |
|        - | 10486 | ` */` |
|        2 | 10487 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10488 |  |
|        - | 10489 | `	ph7_value *pArray;` |
|        - | 10490 | `	/* Create the array first*/` |
|        3 | 10491 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10492 | `	if( pArray == 0 ){` |
|      ! 0 | 10493 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10494 | `		SXUNUSED(apArg);` |
|        - | 10495 | `		/* Return NULL */` |
|      ! 0 | 10496 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10497 | `		return SXRET_OK;` |
|        - | 10498 | `	}` |
|        - | 10499 | `	/* Fill the array with the defined constants */` |
|        3 | 10500 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 10501 | `	/* Return the created array */` |
|        3 | 10502 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10503 | `	return SXRET_OK;` |
|        2 | 10504 |  |
|        - | 10505 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 10506 | `/*` |
|        - | 10507 | ` * Section:` |
|        - | 10508 | ` *  Random numbers/string generators.` |
|        - | 10509 | ` * Status:` |
|        - | 10510 | ` *    Stable.` |
|        - | 10511 | ` */` |
|        - | 10512 | `/*` |
|        - | 10513 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 10514 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 10515 | ` * used by te SQLite3 library.` |
|        - | 10516 | ` */` |
|     2656 | 10517 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 10518 |  |
|        - | 10519 | `	sxu32 iNum;` |
|     2658 | 10520 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2658 | 10521 | `	return iNum;` |
|        2 | 10522 |  |
|        - | 10523 | `/*` |
|        - | 10524 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 10525 | ` * Note that the generated string is NOT null terminated.` |
|        - | 10526 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 10527 | ` * by te SQLite3 library.` |
|        - | 10528 | ` */` |
|   138242 | 10529 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 10530 |  |
|        - | 10531 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 10532 | `	int i;` |
|        - | 10533 | `	/* Generate a binary string first */` |
|   138244 | 10534 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 10535 | `	/* Turn the binary string into english based alphabet */` |
|  1520832 | 10536 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1382590 | 10537 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   691296 | 10538 | `	 }` |
|   138244 | 10539 |  |
|        - | 10540 | `/*` |
|        - | 10541 | ` * int rand()` |
|        - | 10542 | ` * int mt_rand()` |
|        - | 10543 | ` * int rand(int $min,int $max)` |
|        - | 10544 | ` * int mt_rand(int $min,int $max)` |
|        - | 10545 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 10546 | ` * Parameter` |
|        - | 10547 | ` *  $min` |
|        - | 10548 | ` *    The lowest value to return (default: 0)` |
|        - | 10549 | ` *  $max` |
|        - | 10550 | ` *   The highest value to return (default: getrandmax())` |
|        - | 10551 | ` * Return` |
|        - | 10552 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 10553 | ` * Note:` |
|        - | 10554 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10555 | ` *  by te SQLite3 library.` |
|        - | 10556 | ` */` |
|       20 | 10557 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10558 |  |
|        - | 10559 | `	sxu32 iNum;` |
|        - | 10560 | `	/* Generate the random number */` |
|       21 | 10561 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 10562 | `	if( nArg > 1 ){` |
|        - | 10563 | `		sxu32 iMin,iMax;` |
|        3 | 10564 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 10565 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 10566 | `		if( iMin < iMax ){` |
|        3 | 10567 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 10568 | `			if( iDiv > 0 ){` |
|        3 | 10569 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 10570 | `			}` |
|        1 | 10571 | `		}else if(iMax > 0 ){` |
|      ! 0 | 10572 | `			iNum %= iMax;` |
|      ! 0 | 10573 | `		}` |
|        1 | 10574 | `	}` |
|        - | 10575 | `	/* Return the number */` |
|       21 | 10576 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 10577 | `	return SXRET_OK;` |
|        1 | 10578 |  |
|        - | 10579 | `/*` |
|        - | 10580 | ` * int getrandmax(void)` |
|        - | 10581 | ` * int mt_getrandmax(void)` |
|        - | 10582 | ` * int rc4_getrandmax(void)` |
|        - | 10583 | ` *   Show largest possible random value` |
|        - | 10584 | ` * Return` |
|        - | 10585 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 10586 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 10587 | ` * Note:` |
|        - | 10588 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10589 | ` *  by te SQLite3 library.` |
|        - | 10590 | ` */` |
|        4 | 10591 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10592 |  |
|        2 | 10593 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 10594 | `	SXUNUSED(apArg);` |
|        5 | 10595 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 10596 | `	return SXRET_OK;` |
|        1 | 10597 |  |
|        - | 10598 | `/*` |
|        - | 10599 | ` * string rand_str()` |
|        - | 10600 | ` * string rand_str(int $len)` |
|        - | 10601 | ` *  Generate a random string (English alphabet).` |
|        - | 10602 | ` * Parameter` |
|        - | 10603 | ` *  $len` |
|        - | 10604 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 10605 | ` * Return` |
|        - | 10606 | ` *   A pseudo random string.` |
|        - | 10607 | ` * Note:` |
|        - | 10608 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 10609 | ` *  by te SQLite3 library.` |
|        - | 10610 | ` *  This function is a symisc extension.` |
|        - | 10611 | ` */` |
|      120 | 10612 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10613 |  |
|        - | 10614 | `	char zString[1024];` |
|      122 | 10615 | `	int iLen = 0x10;` |
|      122 | 10616 | `	if( nArg > 0 ){` |
|        - | 10617 | `		/* Get the desired length */` |
|      122 | 10618 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 10619 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 10620 | `			/* Default length */` |
|        3 | 10621 | `			iLen = 0x10;` |
|        1 | 10622 | `		}` |
|       60 | 10623 | `	}` |
|        - | 10624 | `	/* Generate the random string */` |
|      122 | 10625 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 10626 | `	/* Return the generated string */` |
|      122 | 10627 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 10628 | `	return SXRET_OK;` |
|        2 | 10629 |  |
|        - | 10630 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10631 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10632 | `/* Unique ID private data */` |
|        - | 10633 | `struct unique_id_data` |
|        - | 10634 |  |
|        - | 10635 | `	ph7_context *pCtx; /* Call context */` |
|        - | 10636 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 10637 | `};` |
|        - | 10638 | `/*` |
|        - | 10639 | ` * Binary to hex consumer callback.` |
|        - | 10640 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 10641 | ` * defined below.` |
|        - | 10642 | ` */` |
|      192 | 10643 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 10644 |  |
|      193 | 10645 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 10646 | `	sxu32 nBuflen;` |
|        - | 10647 | `	/* Extract result buffer length */` |
|      193 | 10648 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 10649 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 10650 | `			/*` |
|        - | 10651 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 10652 | `			 * string will be 13 characters long` |
|        - | 10653 | `			 */` |
|       25 | 10654 | `		return SXERR_ABORT;` |
|        - | 10655 | `	}` |
|      169 | 10656 | `	if( nBuflen > 22 ){` |
|      ! 0 | 10657 | `		return SXERR_ABORT;` |
|        - | 10658 | `	}` |
|        - | 10659 | `	/* Safely Consume the hex stream */` |
|      169 | 10660 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 10661 | `	return SXRET_OK;` |
|       97 | 10662 |  |
|        - | 10663 | `/*` |
|        - | 10664 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 10665 | ` *  Generate a unique ID` |
|        - | 10666 | ` * Parameter` |
|        - | 10667 | ` * $prefix` |
|        - | 10668 | ` *  Append this prefix to the generated unique ID.` |
|        - | 10669 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 10670 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 10671 | ` * $more_entropy` |
|        - | 10672 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 10673 | ` *  that the result will be unique.` |
|        - | 10674 | ` * Return` |
|        - | 10675 | ` *  Returns the unique identifier, as a string.` |
|        - | 10676 | ` */` |
|       24 | 10677 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10678 |  |
|        - | 10679 | `	struct unique_id_data sUniq;` |
|        - | 10680 | `	unsigned char zDigest[20];` |
|       25 | 10681 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10682 | `	const char *zPrefix;` |
|        - | 10683 | `	SHA1Context sCtx;` |
|        - | 10684 | `	char zRandom[7];` |
|        - | 10685 | `	int nPrefix;` |
|        - | 10686 | `	int entropy;` |
|        - | 10687 | `	/* Generate a random string first */` |
|       25 | 10688 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 10689 | `	/* Initialize fields */` |
|       25 | 10690 | `	zPrefix = 0;` |
|       25 | 10691 | `	nPrefix = 0;` |
|       25 | 10692 | `	entropy = 0;` |
|       25 | 10693 | `	if( nArg > 0 ){` |
|        - | 10694 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 10695 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 10696 | `		if( nArg > 1 ){` |
|      ! 0 | 10697 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 10698 | `		}` |
|      ! 0 | 10699 | `	}` |
|       25 | 10700 | `	SHA1Init(&sCtx);` |
|        - | 10701 | `	/* Generate the random ID */` |
|       25 | 10702 | `	if( nPrefix > 0 ){` |
|      ! 0 | 10703 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 10704 | `	}` |
|        - | 10705 | `	/* Append the random ID */` |
|       25 | 10706 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 10707 | `	/* Append the random string */` |
|       25 | 10708 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 10709 | `	/* Increment the number */` |
|       25 | 10710 | `	pVm->unique_id++;` |
|       25 | 10711 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 10712 | `	/* Hexify the digest */` |
|       25 | 10713 | `	sUniq.pCtx = pCtx;` |
|       25 | 10714 | `	sUniq.entropy = entropy;` |
|       25 | 10715 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 10716 | `	/* All done */` |
|       25 | 10717 | `	return PH7_OK;` |
|        1 | 10718 |  |
|        - | 10719 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10720 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10721 | `/*` |
|        - | 10722 | ` * Section:` |
|        - | 10723 | ` *  Language construct implementation as foreign functions.` |
|        - | 10724 | ` * Status:` |
|        - | 10725 | ` *    Stable.` |
|        - | 10726 | ` */` |
|        - | 10727 | `/*` |
|        - | 10728 | ` * void echo($string...)` |
|        - | 10729 | ` *  Output one or more messages.` |
|        - | 10730 | ` * Parameters` |
|        - | 10731 | ` *  $string` |
|        - | 10732 | ` *   Message to output.` |
|        - | 10733 | ` * Return` |
|        - | 10734 | ` *  NULL.` |
|        - | 10735 | ` */` |
|      ! 0 | 10736 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10737 |  |
|        - | 10738 | `	const char *zData;` |
|      ! 0 | 10739 | `	int nDataLen = 0;` |
|        - | 10740 | `	ph7_vm *pVm;` |
|        - | 10741 | `	int i,rc;` |
|        - | 10742 | `	/* Point to the target VM */` |
|      ! 0 | 10743 | `	pVm = pCtx->pVm;` |
|        - | 10744 | `	/* Output */` |
|      ! 0 | 10745 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 10746 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 10747 | `		if( nDataLen > 0 ){` |
|      ! 0 | 10748 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 10749 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 10750 | `			if( rc == SXERR_ABORT ){` |
|        - | 10751 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10752 | `				return PH7_ABORT;` |
|        - | 10753 | `			}` |
|      ! 0 | 10754 | `		}` |
|      ! 0 | 10755 | `	}` |
|      ! 0 | 10756 | `	return SXRET_OK;` |
|      ! 0 | 10757 |  |
|        - | 10758 | `/*` |
|        - | 10759 | ` * int print($string...)` |
|        - | 10760 | ` *  Output one or more messages.` |
|        - | 10761 | ` * Parameters` |
|        - | 10762 | ` *  $string` |
|        - | 10763 | ` *   Message to output.` |
|        - | 10764 | ` * Return` |
|        - | 10765 | ` *  1 always.` |
|        - | 10766 | ` */` |
|        2 | 10767 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10768 |  |
|        - | 10769 | `	const char *zData;` |
|        3 | 10770 | `	int nDataLen = 0;` |
|        - | 10771 | `	ph7_vm *pVm;` |
|        - | 10772 | `	int i,rc;` |
|        - | 10773 | `	/* Point to the target VM */` |
|        3 | 10774 | `	pVm = pCtx->pVm;` |
|        - | 10775 | `	/* Output */` |
|        5 | 10776 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 10777 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 10778 | `		if( nDataLen > 0 ){` |
|        3 | 10779 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 10780 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 10781 | `			if( rc == SXERR_ABORT ){` |
|        - | 10782 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 10783 | `				return PH7_ABORT;` |
|        - | 10784 | `			}` |
|        1 | 10785 | `		}` |
|        2 | 10786 | `	}` |
|        - | 10787 | `	/* Return 1 */` |
|        3 | 10788 | `	ph7_result_int(pCtx,1);` |
|        3 | 10789 | `	return SXRET_OK;` |
|        2 | 10790 |  |
|        - | 10791 | `/*` |
|        - | 10792 | ` * void exit(string $msg)` |
|        - | 10793 | ` * void exit(int $status)` |
|        - | 10794 | ` * void die(string $ms)` |
|        - | 10795 | ` * void die(int $status)` |
|        - | 10796 | ` *   Output a message and terminate program execution.` |
|        - | 10797 | ` * Parameter` |
|        - | 10798 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 10799 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 10800 | ` *  and not printed` |
|        - | 10801 | ` * Return` |
|        - | 10802 | ` *  NULL` |
|        - | 10803 | ` */` |
|      ! 0 | 10804 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 10805 |  |
|      ! 0 | 10806 | `	if( nArg > 0 ){` |
|      ! 0 | 10807 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 10808 | `			const char *zData;` |
|      ! 0 | 10809 | `			int iLen = 0;` |
|        - | 10810 | `			/* Print exit message */` |
|      ! 0 | 10811 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 10812 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 10813 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 10814 | `			sxi32 iExitStatus;` |
|        - | 10815 | `			/* Record exit status code */` |
|      ! 0 | 10816 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 10817 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 10818 | `		}` |
|      ! 0 | 10819 | `	}` |
|        - | 10820 | `	/* Check if we are in an included file */` |
|      ! 0 | 10821 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 10822 | `		/* Exit the entire process */` |
|      ! 0 | 10823 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 10824 | `	}` |
|        - | 10825 | `	/* Abort processing immediately */` |
|      ! 0 | 10826 | `	return PH7_ABORT;` |
|      ! 0 | 10827 |  |
|        - | 10828 | `/*` |
|        - | 10829 | ` * bool isset($var,...)` |
|        - | 10830 | ` *  Finds out whether a variable is set.` |
|        - | 10831 | ` * Parameters` |
|        - | 10832 | ` *  One or more variable to check.` |
|        - | 10833 | ` * Return` |
|        - | 10834 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 10835 | ` */` |
|    83650 | 10836 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10837 |  |
|        - | 10838 | `	ph7_value *pObj;` |
|    83652 | 10839 | `	int res = 0;` |
|        - | 10840 | `	int i;` |
|    83652 | 10841 | `	if( nArg < 1 ){` |
|        - | 10842 | `		/* Missing arguments,return false */` |
|      ! 0 | 10843 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 10844 | `		return SXRET_OK;` |
|        - | 10845 | `	}` |
|        - | 10846 | `	/* Iterate over available arguments */` |
|   109710 | 10847 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    83652 | 10848 | `		pObj = apArg[i];` |
|    83652 | 10849 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    56972 | 10850 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10851 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 10852 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 10853 | `			}` |
|    28485 | 10854 | `		}` |
|    83652 | 10855 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    83652 | 10856 | `		if( !res ){` |
|        - | 10857 | `			/* Variable not set,return FALSE */` |
|    57594 | 10858 | `			ph7_result_bool(pCtx,0);` |
|    57594 | 10859 | `			return SXRET_OK;` |
|        - | 10860 | `		}` |
|    13031 | 10861 | `	}` |
|        - | 10862 | `	/* All given variable are set,return TRUE */` |
|    26060 | 10863 | `	ph7_result_bool(pCtx,1);` |
|    26060 | 10864 | `	return SXRET_OK;` |
|    41827 | 10865 |  |
|        - | 10866 | `/*` |
|        - | 10867 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 10868 | ` * frame,the reference table and discard it's contents.` |
|        - | 10869 | ` * This function never fail and always return SXRET_OK.` |
|        - | 10870 | ` */` |
|  3072046 | 10871 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 10872 |  |
|        - | 10873 | `	ph7_value *pObj;` |
|        - | 10874 | `	VmRefObj *pRef;` |
|  3072048 | 10875 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3072048 | 10876 | `	if( pObj ){` |
|        - | 10877 | `		/* Release the object */` |
|  3072048 | 10878 | `		PH7_MemObjRelease(pObj);` |
|  1536023 | 10879 | `	}` |
|        - | 10880 | `	/* Remove old reference links */` |
|  3072048 | 10881 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3072048 | 10882 | `	if( pRef ){` |
|  3072042 | 10883 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 10884 | `		/* Unlink from the reference table */` |
|  3072042 | 10885 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3072042 | 10886 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 10887 | `			VmSlot sFree;` |
|        - | 10888 | `			/* Restore to the free list */` |
|  3072034 | 10889 | `			sFree.nIdx = nObjIdx;` |
|  3072034 | 10890 | `			sFree.pUserData = 0;` |
|  3072034 | 10891 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1536016 | 10892 | `		}` |
|  1536020 | 10893 | `	}` |
|  3072048 | 10894 | `	return SXRET_OK;` |
|        2 | 10895 |  |
|        - | 10896 | `/*` |
|        - | 10897 | ` * void unset($var,...)` |
|        - | 10898 | ` *   Unset one or more given variable.` |
|        - | 10899 | ` * Parameters` |
|        - | 10900 | ` *  One or more variable to unset.` |
|        - | 10901 | ` * Return` |
|        - | 10902 | ` *  Nothing.` |
|        - | 10903 | ` */` |
|     7264 | 10904 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10905 |  |
|        - | 10906 | `	ph7_value *pObj;` |
|        - | 10907 | `	ph7_vm *pVm;` |
|        - | 10908 | `	int i;` |
|        - | 10909 | `	/* Point to the target VM */` |
|     7266 | 10910 | `	pVm = pCtx->pVm;` |
|        - | 10911 | `	/* Iterate and unset */` |
|    14530 | 10912 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7266 | 10913 | `		pObj = apArg[i];` |
|     7266 | 10914 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 10915 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 10916 | `				/* Throw an error */` |
|      ! 0 | 10917 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 10918 | `			}` |
|      ! 0 | 10919 | `		}else{` |
|     7266 | 10920 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 10921 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7266 | 10922 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7260 | 10923 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3629 | 10924 | `			}` |
|        - | 10925 | `		}` |
|     3634 | 10926 | `	}` |
|     7266 | 10927 | `	return SXRET_OK;` |
|        2 | 10928 |  |
|        - | 10929 | `/*` |
|        - | 10930 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 10931 | ` */` |
|      110 | 10932 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10933 |  |
|      111 | 10934 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 10935 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 10936 | `	ph7_value *pObj;` |
|        - | 10937 | `	sxu32 nIdx;` |
|        - | 10938 | `	/* Extract the memory object */` |
|      111 | 10939 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 10940 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 10941 | `	if( pObj ){` |
|      111 | 10942 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 10943 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 10944 | `				SyString sName;` |
|        - | 10945 | `				ph7_value sKey;` |
|        - | 10946 | `				/* Perform the insertion */` |
|      109 | 10947 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 10948 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 10949 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 10950 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 10951 | `			}` |
|       54 | 10952 | `		}` |
|       55 | 10953 | `	}` |
|      111 | 10954 | `	return SXRET_OK;` |
|        1 | 10955 |  |
|        - | 10956 | `/*` |
|        - | 10957 | ` * array get_defined_vars(void)` |
|        - | 10958 | ` *  Returns an array of all defined variables.` |
|        - | 10959 | ` * Parameter` |
|        - | 10960 | ` *  None` |
|        - | 10961 | ` * Return` |
|        - | 10962 | ` *  An array with all the variables defined in the current scope.` |
|        - | 10963 | ` */` |
|        2 | 10964 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10965 |  |
|        3 | 10966 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10967 | `	ph7_value *pArray;` |
|        - | 10968 | `	/* Create a new array */` |
|        3 | 10969 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10970 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10971 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10972 | `		SXUNUSED(apArg);` |
|        - | 10973 | `		/* Return NULL */` |
|      ! 0 | 10974 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10975 | `		return SXRET_OK;` |
|        - | 10976 | `	}` |
|        - | 10977 | `	/* Superglobals first */` |
|        3 | 10978 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 10979 | `	/* Then variable defined in the current frame */` |
|        3 | 10980 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 10981 | `	/* Finally,return the created array */` |
|        3 | 10982 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10983 | `	return SXRET_OK;` |
|        2 | 10984 |  |
|        - | 10985 | `/*` |
|        - | 10986 | ` * bool gettype($var)` |
|        - | 10987 | ` *  Get the type of a variable` |
|        - | 10988 | ` * Parameters` |
|        - | 10989 | ` *   $var` |
|        - | 10990 | ` *    The variable being type checked.` |
|        - | 10991 | ` * Return` |
|        - | 10992 | ` *   String representation of the given variable type.` |
|        - | 10993 | ` */` |
|       32 | 10994 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10995 |  |
|       34 | 10996 | `	const char *zType = "Empty";` |
|       34 | 10997 | `	if( nArg > 0 ){` |
|       34 | 10998 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 10999 | `	}` |
|        - | 11000 | `	/* Return the variable type */` |
|       34 | 11001 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 11002 | `	return SXRET_OK;` |
|        2 | 11003 |  |
|        - | 11004 | `/*` |
|        - | 11005 | ` * string get_resource_type(resource $handle)` |
|        - | 11006 | ` *  This function gets the type of the given resource.` |
|        - | 11007 | ` * Parameters` |
|        - | 11008 | ` *  $handle` |
|        - | 11009 | ` *  The evaluated resource handle.` |
|        - | 11010 | ` * Return` |
|        - | 11011 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 11012 | ` *  representing its type. If the type is not identified by this function` |
|        - | 11013 | ` *  the return value will be the string Unknown.` |
|        - | 11014 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 11015 | ` *  is not a resource.` |
|        - | 11016 | ` */` |
|        2 | 11017 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11018 |  |
|        3 | 11019 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 11020 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 11021 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11022 | `		return PH7_OK;` |
|        - | 11023 | `	}` |
|        3 | 11024 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 11025 | `	return SXRET_OK;` |
|        2 | 11026 |  |
|        - | 11027 | `/*` |
|        - | 11028 | ` * void var_dump(expression,....)` |
|        - | 11029 | ` *   var_dump � Dumps information about a variable` |
|        - | 11030 | ` * Parameters` |
|        - | 11031 | ` *   One or more expression to dump.` |
|        - | 11032 | ` * Returns` |
|        - | 11033 | ` *  Nothing.` |
|        - | 11034 | ` */` |
|      218 | 11035 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11036 |  |
|        - | 11037 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 11038 | `	int i;` |
|      220 | 11039 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 11040 | `	/* Dump one or more expressions */` |
|      444 | 11041 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 11042 | `		ph7_value *pObj = apArg[i];` |
|        - | 11043 | `		/* Reset the working buffer */` |
|      226 | 11044 | `		SyBlobReset(&sDump);` |
|        - | 11045 | `		/* Dump the given expression */` |
|      226 | 11046 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 11047 | `		/* Output */` |
|      226 | 11048 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 11049 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 11050 | `		}` |
|      114 | 11051 | `	}` |
|        - | 11052 | `	/* Release the working buffer */` |
|      220 | 11053 | `	SyBlobRelease(&sDump);` |
|      220 | 11054 | `	return SXRET_OK;` |
|        2 | 11055 |  |
|        - | 11056 | `/*` |
|        - | 11057 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 11058 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 11059 | ` * Parameters` |
|        - | 11060 | ` *   expression: Expression to dump` |
|        - | 11061 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 11062 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 11063 | ` *            print_r() will return the information rather than print it.` |
|        - | 11064 | ` * Return` |
|        - | 11065 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 11066 | ` *  Otherwise, the return value is TRUE.` |
|        - | 11067 | ` */` |
|       16 | 11068 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11069 |  |
|       17 | 11070 | `	int ret_string = 0;` |
|        - | 11071 | `	SyBlob sDump;` |
|       17 | 11072 | `	if( nArg < 1 ){` |
|        - | 11073 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11074 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11075 | `		return SXRET_OK;` |
|        - | 11076 | `	}` |
|       17 | 11077 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 11078 | `	if ( nArg > 1 ){` |
|        - | 11079 | `		/* Where to redirect output */` |
|       11 | 11080 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 11081 | `	}` |
|        - | 11082 | `	/* Generate dump */` |
|       17 | 11083 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 11084 | `	if( !ret_string ){` |
|        - | 11085 | `		/* Output dump */` |
|        7 | 11086 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11087 | `		/* Return true */` |
|        7 | 11088 | `		ph7_result_bool(pCtx,1);` |
|        4 | 11089 | `	}else{` |
|        - | 11090 | `		/* Generated dump as return value */` |
|       11 | 11091 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11092 | `	}` |
|        - | 11093 | `	/* Release the working buffer */` |
|       17 | 11094 | `	SyBlobRelease(&sDump);` |
|       17 | 11095 | `	return SXRET_OK;` |
|        9 | 11096 |  |
|        - | 11097 | `/*` |
|        - | 11098 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 11099 | ` * Same job as print_r. (see coment above)` |
|        - | 11100 | ` */` |
|        2 | 11101 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11102 |  |
|        3 | 11103 | `	int ret_string = 0;` |
|        - | 11104 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 11105 | `	if( nArg < 1 ){` |
|        - | 11106 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 11107 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11108 | `		return SXRET_OK;` |
|        - | 11109 | `	}` |
|        3 | 11110 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 11111 | `	if ( nArg > 1 ){` |
|        - | 11112 | `		/* Where to redirect output */` |
|        3 | 11113 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 11114 | `	}` |
|        - | 11115 | `	/* Generate dump */` |
|        3 | 11116 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 11117 | `	if( !ret_string ){` |
|        - | 11118 | `		/* Output dump */` |
|      ! 0 | 11119 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11120 | `		/* Return NULL */` |
|      ! 0 | 11121 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11122 | `	}else{` |
|        - | 11123 | `		/* Generated dump as return value */` |
|        3 | 11124 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11125 | `	}` |
|        - | 11126 | `	/* Release the working buffer */` |
|        3 | 11127 | `	SyBlobRelease(&sDump);` |
|        3 | 11128 | `	return SXRET_OK;` |
|        2 | 11129 |  |
|        - | 11130 | `/*` |
|        - | 11131 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 11132 | ` *  Set/get the various assert flags.` |
|        - | 11133 | ` * Parameter` |
|        - | 11134 | ` * $what` |
|        - | 11135 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 11136 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 11137 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 11138 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 11139 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 11140 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 11141 | ` * $value` |
|        - | 11142 | ` *   An optional new value for the option.` |
|        - | 11143 | ` * Return` |
|        - | 11144 | ` *  Old setting on success or FALSE on failure.` |
|        - | 11145 | ` */` |
|       28 | 11146 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11147 |  |
|       30 | 11148 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11149 | `	int iOption;` |
|        - | 11150 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 11151 | `	if( nArg < 1 ){` |
|        3 | 11152 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11153 | `			"ArgumentCountError",` |
|        - | 11154 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 11155 | `			);` |
|        - | 11156 | `	}` |
|        - | 11157 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 11158 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 11159 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 11160 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11161 | `			"TypeError",` |
|        - | 11162 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 11163 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 11164 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 11165 | `			);` |
|        - | 11166 | `	}` |
|       28 | 11167 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 11168 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 11169 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 11170 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 11171 | `	switch( iOption ){` |
|        5 | 11172 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 11173 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 11174 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 11175 | `		if( nArg > 1 ){` |
|        5 | 11176 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11177 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 11178 | `			}else{` |
|        3 | 11179 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 11180 | `			}` |
|        2 | 11181 | `		}` |
|       12 | 11182 | `		break;` |
|        1 | 11183 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 11184 | `		/* Return old callback or null */` |
|        3 | 11185 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 11186 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 11187 | `		}else{` |
|        3 | 11188 | `			ph7_result_null(pCtx);` |
|        - | 11189 | `		}` |
|        3 | 11190 | `		if( nArg > 1 ){` |
|      ! 0 | 11191 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 11192 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 11193 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 11194 | `			}else{` |
|      ! 0 | 11195 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 11196 | `			}` |
|      ! 0 | 11197 | `		}` |
|        3 | 11198 | `		break;` |
|        5 | 11199 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 11200 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 11201 | `		if( nArg > 1 ){` |
|        5 | 11202 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 11203 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 11204 | `			}else{` |
|        3 | 11205 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 11206 | `			}` |
|        2 | 11207 | `		}` |
|       11 | 11208 | `		break;` |
|      ! 0 | 11209 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 11210 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11211 | `		break;` |
|        1 | 11212 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 11213 | `		ph7_result_int(pCtx, 1);` |
|        3 | 11214 | `		break;` |
|      ! 0 | 11215 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 11216 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 11217 | `		break;` |
|        1 | 11218 | `	default:` |
|        - | 11219 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 11220 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11221 | `			"ValueError",` |
|        - | 11222 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 11223 | `			);` |
|        - | 11224 | `	}` |
|       26 | 11225 | `	return PH7_OK;` |
|       16 | 11226 |  |
|        - | 11227 | `/*` |
|        - | 11228 | ` * bool assert(mixed $assertion)` |
|        - | 11229 | ` *  Checks if assertion is FALSE.` |
|        - | 11230 | ` * Parameter` |
|        - | 11231 | ` *  $assertion` |
|        - | 11232 | ` *    The assertion to test.` |
|        - | 11233 | ` * Return` |
|        - | 11234 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 11235 | ` */` |
|       24 | 11236 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11237 |  |
|       26 | 11238 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11239 | `	int iFlags,iResult;` |
|        - | 11240 | `	const char *zDesc;` |
|        - | 11241 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 11242 | `	if( nArg < 1 ){` |
|        3 | 11243 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11244 | `			"ArgumentCountError",` |
|        - | 11245 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 11246 | `			);` |
|        - | 11247 | `	}` |
|       24 | 11248 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 11249 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 11250 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 11251 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 11252 | `		return PH7_OK;` |
|        - | 11253 | `	}` |
|        - | 11254 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 11255 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 11256 | `	if( !iResult ){` |
|        - | 11257 | `		/* Assertion failed */` |
|        - | 11258 | `		/* Extract optional description */` |
|       13 | 11259 | `		zDesc = 0;` |
|       13 | 11260 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 11261 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 11262 | `		}` |
|       13 | 11263 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 11264 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 11265 | `			ph7_value sFile,sLine;` |
|        - | 11266 | `			ph7_value *apCbArg[3];` |
|        - | 11267 | `			SyString *pFile;` |
|        - | 11268 | `			/* Extract the processed script */` |
|      ! 0 | 11269 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 11270 | `			if( pFile == 0 ){` |
|      ! 0 | 11271 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 11272 | `			}` |
|        - | 11273 | `			/* Invoke the callback */` |
|      ! 0 | 11274 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 11275 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 11276 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 11277 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 11278 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 11279 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 11280 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 11281 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 11282 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 11283 | `		}` |
|       13 | 11284 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 11285 | `			/* Abort VM execution immediately */` |
|      ! 0 | 11286 | `			return PH7_ABORT;` |
|        - | 11287 | `		}` |
|        - | 11288 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 11289 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 11290 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11291 | `				"AssertionError",` |
|        - | 11292 | `				"%s",` |
|        1 | 11293 | `				zDesc` |
|        - | 11294 | `				);` |
|      ! 0 | 11295 | `		}else{` |
|       11 | 11296 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11297 | `				"AssertionError",` |
|        - | 11298 | `				"assert(false)"` |
|        - | 11299 | `				);` |
|        - | 11300 | `		}` |
|        - | 11301 | `	}` |
|        - | 11302 | `	/* Assertion passed */` |
|       11 | 11303 | `	ph7_result_bool(pCtx,1);` |
|       11 | 11304 | `	return PH7_OK;` |
|       14 | 11305 |  |
|        - | 11306 | `/*` |
|        - | 11307 | ` * Section:` |
|        - | 11308 | ` *  Error reporting functions.` |
|        - | 11309 | ` * Status:` |
|        - | 11310 | ` *    Stable.` |
|        - | 11311 | ` */` |
|        - | 11312 | `/*` |
|        - | 11313 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 11314 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 11315 | ` * Parameters` |
|        - | 11316 | ` *  $error_msg` |
|        - | 11317 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 11318 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 11319 | ` * $error_type` |
|        - | 11320 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 11321 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 11322 | ` * Return` |
|        - | 11323 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 11324 | ` */` |
|       12 | 11325 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11326 |  |
|       14 | 11327 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 11328 | `	int rc = PH7_OK;` |
|       14 | 11329 | `	if( nArg > 0 ){` |
|        - | 11330 | `		const char *zErr;` |
|        - | 11331 | `		int nLen;` |
|        - | 11332 | `		/* Extract the error message */` |
|       12 | 11333 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 11334 | `		if( nArg > 1 ){` |
|        - | 11335 | `			/* Extract the error type */` |
|       12 | 11336 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 11337 | `			switch( nErr ){` |
|        1 | 11338 | `			case 1:   /* E_ERROR */` |
|        - | 11339 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 11340 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 11341 | `			case 256: /* E_USER_ERROR */` |
|        3 | 11342 | `				nErr = PH7_CTX_ERR;` |
|        3 | 11343 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 11344 | `				break;` |
|        1 | 11345 | `			case 2:   /* E_WARNING */` |
|        - | 11346 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 11347 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 11348 | `			case 512: /* E_USER_WARNING */` |
|        3 | 11349 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 11350 | `				break;` |
|        3 | 11351 | `			default:` |
|        8 | 11352 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 11353 | `				break;` |
|        - | 11354 | `			}` |
|        5 | 11355 | `		}` |
|        - | 11356 | `		/* Report error */` |
|       12 | 11357 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 11358 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 11359 | `			return rc;` |
|        - | 11360 | `		}` |
|        - | 11361 | `		/* Return true */` |
|       12 | 11362 | `		ph7_result_bool(pCtx,1);` |
|        7 | 11363 | `	}else{` |
|        - | 11364 | `		/* Missing arguments,return FALSE */` |
|        3 | 11365 | `		ph7_result_bool(pCtx,0);` |
|        - | 11366 | `	}` |
|       14 | 11367 | `	return rc;` |
|        8 | 11368 |  |
|        - | 11369 | `/*` |
|        - | 11370 | ` * int error_reporting([int $level])` |
|        - | 11371 | ` *  Sets which PHP errors are reported.` |
|        - | 11372 | ` * Parameters` |
|        - | 11373 | ` *  $level` |
|        - | 11374 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 11375 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 11376 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 11377 | ` *   levels will not always behave as expected.` |
|        - | 11378 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 11379 | ` *   in the predefined constants.` |
|        - | 11380 | ` * Return` |
|        - | 11381 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 11382 | ` *   parameter is given.` |
|        - | 11383 | ` */` |
|       38 | 11384 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11385 |  |
|       40 | 11386 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11387 | `	int nOld;` |
|        - | 11388 | `	/* Extract the old reporting level */` |
|       40 | 11389 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 11390 | `	if( nArg > 0 ){` |
|        - | 11391 | `		int nNew;` |
|        - | 11392 | `		/* Extract the desired error reporting level */` |
|       32 | 11393 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 11394 | `		if( !nNew ){` |
|        - | 11395 | `			/* Do not report errors at all */` |
|        5 | 11396 | `			pVm->bErrReport = 0;` |
|        3 | 11397 | `		}else{` |
|        - | 11398 | `			/* Report all errors */` |
|       28 | 11399 | `			pVm->bErrReport = 1;` |
|        - | 11400 | `		}` |
|       15 | 11401 | `	}` |
|        - | 11402 | `	/* Return the old level */` |
|       40 | 11403 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 11404 | `	return PH7_OK;` |
|        2 | 11405 |  |
|        - | 11406 | `/*` |
|        - | 11407 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 11408 | ` *  Send an error message somewhere.` |
|        - | 11409 | ` * Parameter` |
|        - | 11410 | ` *  $message` |
|        - | 11411 | ` *   The error message that should be logged.` |
|        - | 11412 | ` *  $message_type` |
|        - | 11413 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 11414 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 11415 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 11416 | ` *       This is the default option.` |
|        - | 11417 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 11418 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 11419 | ` *    2  No longer an option.` |
|        - | 11420 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 11421 | ` *       to the end of the message string.` |
|        - | 11422 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 11423 | ` *  $destination` |
|        - | 11424 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 11425 | ` *  $extra_headers` |
|        - | 11426 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 11427 | ` * Return` |
|        - | 11428 | ` *  TRUE on success or FALSE on failure.` |
|        - | 11429 | ` * NOTE:` |
|        - | 11430 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 11431 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 11432 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 11433 | ` *  Otherwise this function is no-op.` |
|        - | 11434 | ` */` |
|        4 | 11435 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11436 |  |
|        - | 11437 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 11438 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 11439 | `	int iType = 0;` |
|        5 | 11440 | `	if( nArg < 1 ){` |
|        - | 11441 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 11442 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11443 | `		return PH7_OK;` |
|        - | 11444 | `	}` |
|        5 | 11445 | `	if( pVm->xErrLog  ){` |
|        - | 11446 | `		/* Invoke the user callback */` |
|      ! 0 | 11447 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 11448 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 11449 | `		if( nArg > 1 ){` |
|      ! 0 | 11450 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 11451 | `			if( nArg > 2 ){` |
|      ! 0 | 11452 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 11453 | `				if( nArg > 3 ){` |
|      ! 0 | 11454 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 11455 | `				}` |
|      ! 0 | 11456 | `			}` |
|      ! 0 | 11457 | `		}` |
|      ! 0 | 11458 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 11459 | `	}` |
|        - | 11460 | `	/* Retun TRUE */` |
|        5 | 11461 | `	ph7_result_bool(pCtx,1);` |
|        5 | 11462 | `	return PH7_OK;` |
|        3 | 11463 |  |
|        - | 11464 | `/*` |
|        - | 11465 | ` * bool restore_exception_handler(void)` |
|        - | 11466 | ` *  Restores the previously defined exception handler function.` |
|        - | 11467 | ` * Parameter` |
|        - | 11468 | ` *  None` |
|        - | 11469 | ` * Return` |
|        - | 11470 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 11471 | ` */` |
|        4 | 11472 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11473 |  |
|        5 | 11474 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11475 | `	ph7_value *pOld,*pNew;` |
|        - | 11476 | `	/* Point to the old and the new handler */` |
|        5 | 11477 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 11478 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 11479 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 11480 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 11481 | `		SXUNUSED(apArg);` |
|        - | 11482 | `		/* No installed handler,return FALSE */` |
|        5 | 11483 | `		ph7_result_bool(pCtx,0);` |
|        5 | 11484 | `		return PH7_OK;` |
|        - | 11485 | `	}` |
|        - | 11486 | `	/* Copy the old handler */` |
|      ! 0 | 11487 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11488 | `	PH7_MemObjRelease(pOld);` |
|        - | 11489 | `	/* Return TRUE */` |
|      ! 0 | 11490 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11491 | `	return PH7_OK;` |
|        3 | 11492 |  |
|        - | 11493 | `/*` |
|        - | 11494 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 11495 | ` *  Sets a user-defined exception handler function.` |
|        - | 11496 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 11497 | ` * NOTE` |
|        - | 11498 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 11499 | ` *  the satndard PHP engine.` |
|        - | 11500 | ` * Parameters` |
|        - | 11501 | ` *  $exception_handler` |
|        - | 11502 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 11503 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 11504 | ` *   that was thrown.` |
|        - | 11505 | ` *  Note:` |
|        - | 11506 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11507 | ` * Return` |
|        - | 11508 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 11509 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11510 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11511 | ` */` |
|        4 | 11512 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11513 |  |
|        6 | 11514 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11515 | `	ph7_value *pOld,*pNew;` |
|        - | 11516 | `	/* Point to the old and the new handler */` |
|        6 | 11517 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 11518 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 11519 | `	/* Return the old handler */` |
|        6 | 11520 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 11521 | `	if( nArg > 0 ){` |
|        6 | 11522 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11523 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 11524 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 11525 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 11526 | `		}else{` |
|        6 | 11527 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11528 | `			/* Install the new handler */` |
|        6 | 11529 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11530 | `		}` |
|        2 | 11531 | `	}` |
|        6 | 11532 | `	return PH7_OK;` |
|        2 | 11533 |  |
|        - | 11534 | `/*` |
|        - | 11535 | ` * bool restore_error_handler(void)` |
|        - | 11536 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11537 | ` * Parameters:` |
|        - | 11538 | ` *  None.` |
|        - | 11539 | ` * Return` |
|        - | 11540 | ` *  Always TRUE.` |
|        - | 11541 | ` */` |
|        6 | 11542 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11543 |  |
|        7 | 11544 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11545 | `	ph7_value *pOld,*pNew;` |
|        - | 11546 | `	/* Point to the old and the new handler */` |
|        7 | 11547 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 11548 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 11549 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 11550 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 11551 | `		SXUNUSED(apArg);` |
|        - | 11552 | `		/* No installed callback,return FALSE */` |
|        7 | 11553 | `		ph7_result_bool(pCtx,0);` |
|        7 | 11554 | `		return PH7_OK;` |
|        - | 11555 | `	}` |
|        - | 11556 | `	/* Copy the old callback */` |
|      ! 0 | 11557 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 11558 | `	PH7_MemObjRelease(pOld);` |
|        - | 11559 | `	/* Return TRUE */` |
|      ! 0 | 11560 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 11561 | `	return PH7_OK;` |
|        4 | 11562 |  |
|        - | 11563 | `/*` |
|        - | 11564 | ` * value set_error_handler(callable $error_handler)` |
|        - | 11565 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11566 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 11567 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 11568 | ` *  Sets a user-defined error handler function.` |
|        - | 11569 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 11570 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 11571 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 11572 | ` *  conditions (using trigger_error()).` |
|        - | 11573 | ` * Parameters` |
|        - | 11574 | ` *  $error_handler` |
|        - | 11575 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 11576 | ` *   describing the error.` |
|        - | 11577 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 11578 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 11579 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 11580 | ` *   The function can be shown as:` |
|        - | 11581 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 11582 | ` *     errno` |
|        - | 11583 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 11584 | ` *   errstr` |
|        - | 11585 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 11586 | ` *   errfile` |
|        - | 11587 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 11588 | ` *     was raised in, as a string.` |
|        - | 11589 | ` *  Note:` |
|        - | 11590 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 11591 | ` * Return` |
|        - | 11592 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 11593 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 11594 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 11595 | ` */` |
|     9976 | 11596 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11597 |  |
|     9978 | 11598 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11599 | `	ph7_value *pOld,*pNew;` |
|        - | 11600 | `	/* Point to the old and the new handler */` |
|     9978 | 11601 | `	pOld = &pVm->aErrCB[0];` |
|     9978 | 11602 | `	pNew = &pVm->aErrCB[1];` |
|        - | 11603 | `	/* Return the old handler */` |
|     9978 | 11604 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     9978 | 11605 | `	if( nArg > 0 ){` |
|     9978 | 11606 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 11607 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4987 | 11608 | `			PH7_MemObjRelease(pNew);` |
|     4987 | 11609 | `			ph7_result_bool(pCtx,1);` |
|     2494 | 11610 | `		}else{` |
|     4992 | 11611 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 11612 | `			/* Install the new handler */` |
|     4992 | 11613 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 11614 | `		}` |
|     4988 | 11615 | `	}` |
|     9978 | 11616 | `	return PH7_OK;` |
|        2 | 11617 |  |
|        - | 11618 | `/*` |
|        - | 11619 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 11620 | ` *  Generates a backtrace.` |
|        - | 11621 | ` * Paramaeter` |
|        - | 11622 | ` *  $options` |
|        - | 11623 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 11624 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 11625 | ` *   all the function/method arguments, to save memory.` |
|        - | 11626 | ` * $limit` |
|        - | 11627 | ` *   (Not Used)` |
|        - | 11628 | ` * Return` |
|        - | 11629 | ` *  An array.The possible returned elements are as follows:` |
|        - | 11630 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 11631 | ` *          Name        Type      Description` |
|        - | 11632 | ` *          ------      ------     -----------` |
|        - | 11633 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 11634 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 11635 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 11636 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 11637 | ` *          object      object    The current object.` |
|        - | 11638 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 11639 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 11640 | ` */` |
|      616 | 11641 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11642 |  |
|      618 | 11643 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11644 | `	ph7_value *pArray;` |
|        - | 11645 | `	ph7_class *pClass;` |
|        - | 11646 | `	ph7_value *pValue;` |
|        - | 11647 | `	SyString *pFile;` |
|        - | 11648 | `	/* Create a new array */` |
|      618 | 11649 | `	pArray = ph7_context_new_array(pCtx);` |
|      618 | 11650 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      618 | 11651 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 11652 | `		/* Out of memory,return NULL */` |
|      ! 0 | 11653 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 11654 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11655 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11656 | `		SXUNUSED(apArg);` |
|      ! 0 | 11657 | `		return PH7_OK;` |
|        - | 11658 | `	}` |
|        - | 11659 | `	/* Dump running function name and it's arguments  */` |
|      618 | 11660 | `	if( pVm->pFrame->pParent ){` |
|      618 | 11661 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 11662 | `		ph7_vm_func *pFunc;` |
|        - | 11663 | `		ph7_value *pArg;` |
|      618 | 11664 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      618 | 11665 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      618 | 11666 | `		if( pFrame->pParent && pFunc ){` |
|      618 | 11667 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      618 | 11668 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      618 | 11669 | `			ph7_value_reset_string_cursor(pValue);` |
|      308 | 11670 | `		}` |
|        - | 11671 | `		/* Function arguments */` |
|      618 | 11672 | `		pArg = ph7_context_new_array(pCtx);` |
|      618 | 11673 | `		if( pArg  ){` |
|        - | 11674 | `			ph7_value *pObj;` |
|        - | 11675 | `			VmSlot *aSlot;` |
|        - | 11676 | `			sxu32 n;` |
|        - | 11677 | `			/* Start filling the array with the given arguments */` |
|      618 | 11678 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2458 | 11679 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1842 | 11680 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1842 | 11681 | `				if( pObj ){` |
|     1842 | 11682 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      920 | 11683 | `				}` |
|      922 | 11684 | `			}` |
|        - | 11685 | `			/* Save the array */` |
|      618 | 11686 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      308 | 11687 | `		}` |
|      308 | 11688 | `	}` |
|      618 | 11689 | `	ph7_value_int(pValue,1);` |
|        - | 11690 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 11691 | `	 * line numbers at run-time. )` |
|        - | 11692 | `	 */` |
|      618 | 11693 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 11694 | `	/* Current processed script */` |
|      618 | 11695 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      618 | 11696 | `	if( pFile ){` |
|      618 | 11697 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      618 | 11698 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      618 | 11699 | `		ph7_value_reset_string_cursor(pValue);` |
|      308 | 11700 | `	}` |
|        - | 11701 | `	/* Top class */` |
|      618 | 11702 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      618 | 11703 | `	if( pClass ){` |
|      614 | 11704 | `		ph7_value_reset_string_cursor(pValue);` |
|      614 | 11705 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      614 | 11706 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      306 | 11707 | `	}` |
|        - | 11708 | `	/* Return the freshly created array */` |
|      618 | 11709 | `	ph7_result_value(pCtx,pArray);` |
|        - | 11710 | `	/*` |
|        - | 11711 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 11712 | `	 * as soon we return from this function.` |
|        - | 11713 | `	 */` |
|      618 | 11714 | `	return PH7_OK;` |
|      310 | 11715 |  |
|        - | 11716 | `/*` |
|        - | 11717 | ` * Generate a small backtrace.` |
|        - | 11718 | ` * Store the generated dump in the given BLOB` |
|        - | 11719 | ` */` |
|        4 | 11720 | `static int VmMiniBacktrace(` |
|        - | 11721 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11722 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 11723 | `	)` |
|        1 | 11724 |  |
|        5 | 11725 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11726 | `	ph7_vm_func *pFunc;` |
|        - | 11727 | `	ph7_class *pClass;` |
|        - | 11728 | `	SyString *pFile;` |
|        - | 11729 | `	/* Called function */` |
|        5 | 11730 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 11731 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 11732 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11733 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 11734 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 11735 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 11736 | `	}else{` |
|      ! 0 | 11737 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 11738 | `	}` |
|        5 | 11739 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 11740 | `	/* Current processed script */` |
|        5 | 11741 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 11742 | `	if( pFile ){` |
|        5 | 11743 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 11744 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 11745 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 11746 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 11747 | `	}` |
|        - | 11748 | `	/* Top class */` |
|        5 | 11749 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 11750 | `	if( pClass ){` |
|      ! 0 | 11751 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 11752 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 11753 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 11754 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 11755 | `	}` |
|        5 | 11756 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 11757 | `	/* All done */` |
|        5 | 11758 | `	return SXRET_OK;` |
|        1 | 11759 |  |
|        - | 11760 | `/*` |
|        - | 11761 | ` * void debug_print_backtrace()` |
|        - | 11762 | ` *  Prints a backtrace` |
|        - | 11763 | ` * Parameters` |
|        - | 11764 | ` * None` |
|        - | 11765 | ` * Return` |
|        - | 11766 | ` * NULL` |
|        - | 11767 | ` */` |
|        2 | 11768 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11769 |  |
|        3 | 11770 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11771 | `	SyBlob sDump;` |
|        3 | 11772 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11773 | `	/* Generate the backtrace */` |
|        3 | 11774 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11775 | `	/* Output backtrace */` |
|        3 | 11776 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 11777 | `	/* All done,cleanup */` |
|        3 | 11778 | `	SyBlobRelease(&sDump);` |
|        1 | 11779 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11780 | `	SXUNUSED(apArg);` |
|        3 | 11781 | `	return PH7_OK;` |
|        1 | 11782 |  |
|        - | 11783 | `/*` |
|        - | 11784 | ` * string debug_string_backtrace()` |
|        - | 11785 | ` *  Generate a backtrace` |
|        - | 11786 | ` * Parameters` |
|        - | 11787 | ` * None` |
|        - | 11788 | ` * Return` |
|        - | 11789 | ` *  A mini backtrace().` |
|        - | 11790 | ` * Note that this is a symisc extension.` |
|        - | 11791 | ` */` |
|        2 | 11792 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11793 |  |
|        3 | 11794 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11795 | `	SyBlob sDump;` |
|        3 | 11796 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 11797 | `	/* Generate the backtrace */` |
|        3 | 11798 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 11799 | `	/* Return the backtrace */` |
|        3 | 11800 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 11801 | `	/* All done,cleanup */` |
|        3 | 11802 | `	SyBlobRelease(&sDump);` |
|        1 | 11803 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 11804 | `	SXUNUSED(apArg);` |
|        3 | 11805 | `	return PH7_OK;` |
|        1 | 11806 |  |
|        - | 11807 | `/*` |
|        - | 11808 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 11809 | ` * exception is triggered.` |
|        - | 11810 | ` */` |
|      480 | 11811 | `static sxi32 VmUncaughtException(` |
|        - | 11812 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 11813 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11814 | `	)` |
|        1 | 11815 |  |
|        - | 11816 | `	ph7_value *apArg[2],sArg;` |
|      481 | 11817 | `	int nArg = 1;` |
|        - | 11818 | `	sxi32 rc;` |
|      481 | 11819 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 11820 | `		/* Nesting limit reached */` |
|      ! 0 | 11821 | `		return SXRET_OK;` |
|        - | 11822 | `	}` |
|        - | 11823 | `	/* Call any exception handler if available */` |
|      481 | 11824 | `	PH7_MemObjInit(pVm,&sArg);` |
|      481 | 11825 | `	if( pThis ){` |
|        - | 11826 | `		/* Load the exception instance */` |
|      481 | 11827 | `		sArg.x.pOther = pThis;` |
|      481 | 11828 | `		pThis->iRef++;` |
|      481 | 11829 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      241 | 11830 | `	}else{` |
|      ! 0 | 11831 | `		nArg = 0;` |
|        - | 11832 | `	}` |
|      481 | 11833 | `	apArg[0] = &sArg;` |
|        - | 11834 | `	/* Call the exception handler if available */` |
|      481 | 11835 | `	pVm->nExceptDepth++;` |
|      481 | 11836 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      481 | 11837 | `	pVm->nExceptDepth--;` |
|      481 | 11838 | `	if( rc != SXRET_OK ){` |
|        - | 11839 | `		SyBlob sMsgBuf;` |
|      479 | 11840 | `		const char *zClass = "Exception";` |
|      479 | 11841 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 11842 | `		const char *zMsg;` |
|        - | 11843 | `		sxu32 nMsg;` |
|        - | 11844 | `		const char *zFuncName;` |
|        - | 11845 | `		int nFuncLen;` |
|      479 | 11846 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      479 | 11847 | `		if( pThis ){` |
|        - | 11848 | `			ph7_class_method *pGetMessage;` |
|        - | 11849 | `			ph7_value sMsg;` |
|        - | 11850 | `			const char *zTmp;` |
|        - | 11851 | `			int nTmp;` |
|      479 | 11852 | `			zClass = pThis->pClass->sName.zString;` |
|      479 | 11853 | `			nClass = pThis->pClass->sName.nByte;` |
|      479 | 11854 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      479 | 11855 | `			if( pGetMessage ){` |
|      479 | 11856 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      479 | 11857 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      479 | 11858 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      479 | 11859 | `					if( zTmp && nTmp > 0 ){` |
|      479 | 11860 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      239 | 11861 | `					}` |
|      239 | 11862 | `				}` |
|      479 | 11863 | `				PH7_MemObjRelease(&sMsg);` |
|      239 | 11864 | `			}` |
|      239 | 11865 | `		}` |
|      479 | 11866 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 | 11867 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 | 11868 | `		}` |
|      479 | 11869 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      479 | 11870 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      479 | 11871 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      479 | 11872 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      479 | 11873 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 11874 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      479 | 11875 | `		rc = SXERR_ABORT;` |
|      239 | 11876 | `	}` |
|      481 | 11877 | `	PH7_MemObjRelease(&sArg);` |
|      481 | 11878 | `	return rc;` |
|      241 | 11879 |  |
|        - | 11880 | `/*` |
|        - | 11881 | ` * Throw a user exception.` |
|        - | 11882 | ` *` |
|        - | 11883 | ` * Exception dispatch follows this sequence:` |
|        - | 11884 | ` *` |
|        - | 11885 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 11886 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 11887 | ` *` |
|        - | 11888 | ` * 2. If NO catch matches:` |
|        - | 11889 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 11890 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 11891 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 11892 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 11893 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 11894 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 11895 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 11896 | ` *` |
|        - | 11897 | ` * 3. If a catch DOES match:` |
|        - | 11898 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 11899 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 11900 | ` *       inside the catch body from immediately propagating past our` |
|        - | 11901 | ` *       finally block.` |
|        - | 11902 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 11903 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 11904 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 11905 | ` *       in pPendingException (step 2c).` |
|        - | 11906 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 11907 | ` *    d. Run finally (if present).` |
|        - | 11908 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 11909 | ` *       that handlers are restored and finally has run.` |
|        - | 11910 | ` */` |
|      620 | 11911 | `static sxi32 VmThrowException(` |
|        - | 11912 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 11913 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 11914 | `	)` |
|        2 | 11915 |  |
|        - | 11916 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 11917 | `	ph7_exception **apException;` |
|        - | 11918 | `	ph7_exception *pException;` |
|        - | 11919 | `	/* Point to the stack of loaded exceptions */` |
|      622 | 11920 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      622 | 11921 | `	pException = 0;` |
|      622 | 11922 | `	pCatch = 0;` |
|      622 | 11923 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11924 | `		ph7_exception_block *aCatch;` |
|        - | 11925 | `		ph7_class *pClass;` |
|        - | 11926 | `		SyString *aNames;` |
|        - | 11927 | `		sxu32 nNames;` |
|        - | 11928 | `		int matched;` |
|        - | 11929 | `		sxu32 j,k;` |
|        - | 11930 | `		/* Locate the appropriate block to execute */` |
|      136 | 11931 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      136 | 11932 | `		(void)SySetPop(&pVm->aException);` |
|      136 | 11933 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      138 | 11934 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 11935 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      136 | 11936 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      136 | 11937 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      136 | 11938 | `			matched = 0;` |
|      150 | 11939 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 11940 | `				/* Extract the target class */` |
|      148 | 11941 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,TRUE,0);` |
|      148 | 11942 | `				if( pClass == 0 ){` |
|        - | 11943 | `					/* No such class */` |
|      ! 0 | 11944 | `					continue;` |
|        - | 11945 | `				}` |
|      148 | 11946 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      134 | 11947 | `					matched = 1;` |
|      134 | 11948 | `					break;` |
|        - | 11949 | `				}` |
|        8 | 11950 | `			}` |
|      136 | 11951 | `			if( matched ){` |
|        - | 11952 | `				/* Catch block found,break immediately */` |
|      134 | 11953 | `				pCatch = &aCatch[j];` |
|      134 | 11954 | `				break;` |
|        - | 11955 | `			}` |
|        2 | 11956 | `		}` |
|       67 | 11957 | `	}` |
|        - | 11958 | `	/* Execute the cached block if available */` |
|      622 | 11959 | `	if( pCatch == 0 ){` |
|        - | 11960 | `		sxi32 rc;` |
|        - | 11961 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      490 | 11962 | `		if( pException && pException->iHasFinally ){` |
|        3 | 11963 | `			pException->iFinallyDone = 1;` |
|        3 | 11964 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 11965 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 11966 | `				return SXERR_ABORT;` |
|        - | 11967 | `			}` |
|        1 | 11968 | `		}` |
|        - | 11969 | `		/* Check if there is an outer exception handler on the stack */` |
|      490 | 11970 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 11971 | `			/* Re-throw to the outer handler */` |
|        3 | 11972 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 11973 | `		}` |
|        - | 11974 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 11975 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 11976 | `		 * exception instead of reporting it uncaught.` |
|        - | 11977 | `		 */` |
|      488 | 11978 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 11979 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 11980 | `			 * by looking for a catch frame on the stack.` |
|        - | 11981 | `			 */` |
|      488 | 11982 | `			VmFrame *pF = pVm->pFrame;` |
|      488 | 11983 | `			int inCatch = 0;` |
|      974 | 11984 | `			while( pF ){` |
|      494 | 11985 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        7 | 11986 | `					inCatch = 1;` |
|        7 | 11987 | `					break;` |
|        - | 11988 | `				}` |
|      487 | 11989 | `				pF = pF->pParent;` |
|        1 | 11990 | `			}` |
|      488 | 11991 | `			if( inCatch ){` |
|        - | 11992 | `				/* Defer — will be re-thrown after finally runs */` |
|        7 | 11993 | `				pThis->iRef++;` |
|        7 | 11994 | `				pVm->pPendingException = pThis;` |
|        7 | 11995 | `				return SXRET_OK;` |
|        - | 11996 | `			}` |
|      240 | 11997 | `		}` |
|        - | 11998 | `		/* Truly uncaught */` |
|      481 | 11999 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      481 | 12000 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 12001 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 12002 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 12003 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 12004 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 12005 | `			}` |
|      ! 0 | 12006 | `		}` |
|      481 | 12007 | `		return rc;` |
|      ! 0 | 12008 | `	}else{` |
|      134 | 12009 | `		VmFrame *pFrame = pVm->pFrame;` |
|      134 | 12010 | `		ph7_exception **apSaved = 0;` |
|        - | 12011 | `		sxu32 nSavedCount;` |
|        - | 12012 | `		sxi32 rc;` |
|      134 | 12013 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      134 | 12014 | `		if( pException->pFrame == pFrame ){` |
|       88 | 12015 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       43 | 12016 | `		}` |
|        - | 12017 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 12018 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 12019 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 12020 | `		 */` |
|      134 | 12021 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      134 | 12022 | `		if( nSavedCount > 0 ){` |
|       13 | 12023 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 | 12024 | `				nSavedCount * sizeof(ph7_exception *));` |
|        9 | 12025 | `			if( apSaved ){` |
|       13 | 12026 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        4 | 12027 | `					nSavedCount * sizeof(ph7_exception *));` |
|        9 | 12028 | `				SySetReset(&pVm->aException);` |
|        4 | 12029 | `			}` |
|        4 | 12030 | `		}` |
|        - | 12031 | `		/* Create a private frame first */` |
|      134 | 12032 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      134 | 12033 | `		if( rc == SXRET_OK ){` |
|      134 | 12034 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      134 | 12035 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      134 | 12036 | `			if( pObj ){` |
|      134 | 12037 | `				pThis->iRef++;` |
|      134 | 12038 | `				pObj->x.pOther = pThis;` |
|      134 | 12039 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       66 | 12040 | `			}` |
|        - | 12041 | `			/* Execute the catch block */` |
|      134 | 12042 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 12043 | `			/* Leave the frame */` |
|      134 | 12044 | `			VmLeaveFrame(&(*pVm));` |
|       66 | 12045 | `		}` |
|        - | 12046 | `		/* Restore the outer exception handlers */` |
|      134 | 12047 | `		if( apSaved ){` |
|        - | 12048 | `			sxu32 k;` |
|        - | 12049 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 12050 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 12051 | `			 * Restore the original outer entries.` |
|        - | 12052 | `			 */` |
|        9 | 12053 | `			SySetReset(&pVm->aException);` |
|       17 | 12054 | `			for(k = 0; k < nSavedCount; k++){` |
|        9 | 12055 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 | 12056 | `			}` |
|        9 | 12057 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        4 | 12058 | `		}` |
|        - | 12059 | `		/* Execute the finally block after catch */` |
|      134 | 12060 | `		if( pException->iHasFinally ){` |
|       16 | 12061 | `			pException->iFinallyDone = 1;` |
|        - | 12062 | `			{` |
|       16 | 12063 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 12064 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 12065 | `					return SXERR_ABORT;` |
|        - | 12066 | `				}` |
|        - | 12067 | `			}` |
|        7 | 12068 | `		}` |
|      134 | 12069 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 12070 | `			return SXERR_ABORT;` |
|        - | 12071 | `		}` |
|        - | 12072 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 12073 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 12074 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 12075 | `		 */` |
|      134 | 12076 | `		if( pVm->pPendingException ){` |
|        7 | 12077 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        7 | 12078 | `			pVm->pPendingException = 0;` |
|        7 | 12079 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 12080 | `		}` |
|        - | 12081 | `	}` |
|        - | 12082 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 12083 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 12084 | `	 */` |
|      128 | 12085 | `	return SXRET_OK;` |
|      312 | 12086 |  |
|        - | 12087 | `/*` |
|        - | 12088 | ` * Section:` |
|        - | 12089 | ` *  Version,Credits and Copyright related functions.` |
|        - | 12090 | ` * Status:` |
|        - | 12091 | ` *    Stable.` |
|        - | 12092 | ` */` |
|        - | 12093 | `/*` |
|        - | 12094 | ` * string ph7version(void)` |
|        - | 12095 | ` *  Returns the running version of the PH7 version.` |
|        - | 12096 | ` * Parameters` |
|        - | 12097 | ` *  None` |
|        - | 12098 | ` * Return` |
|        - | 12099 | ` * Current PH7 version.` |
|        - | 12100 | ` */` |
|        2 | 12101 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12102 |  |
|        1 | 12103 | `	SXUNUSED(nArg);` |
|        1 | 12104 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 12105 | `	/* Current engine version */` |
|        3 | 12106 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 12107 | `	return PH7_OK;` |
|        1 | 12108 |  |
|        - | 12109 | `/*` |
|        - | 12110 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 12111 | ` */` |
|        - | 12112 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 12113 | ` "<html><head>"\` |
|        - | 12114 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 12115 | ` "<style type=\"text/css\">"\` |
|        - | 12116 | ` "div {"\` |
|        - | 12117 | `     "border: 1px solid #cccccc;"\` |
|        - | 12118 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 12119 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 12120 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 12121 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 12122 | `     "-webkit-border-radius: 10px;"\` |
|        - | 12123 | `     "-o-border-radius: 10px;"\` |
|        - | 12124 | `     "border-radius: 10px;"\` |
|        - | 12125 | `     "padding-left: 2em;"\` |
|        - | 12126 | `     "background-color: white;"\` |
|        - | 12127 | `     "margin-left: auto;"\` |
|        - | 12128 | `     "font-family: verdana;"\` |
|        - | 12129 | `     "padding-right: 2em;"\` |
|        - | 12130 | `     "margin-right: auto;"\` |
|        - | 12131 | `     "}"\` |
|        - | 12132 | `     "body {"\` |
|        - | 12133 | `     "padding: 0.2em;"\` |
|        - | 12134 | `     "font-style: normal;"\` |
|        - | 12135 | `     "font-size: medium;"\` |
|        - | 12136 | `     "background-color: #f2f2f2;"\` |
|        - | 12137 | `     "}"\` |
|        - | 12138 | `     "hr {"\` |
|        - | 12139 | `     "border-style: solid none none;"\` |
|        - | 12140 | `     "border-width: 1px medium medium;"\` |
|        - | 12141 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 12142 | `     "height: 1px;"\` |
|        - | 12143 | `     "}"\` |
|        - | 12144 | `     "a {"\` |
|        - | 12145 | `     "color: #3366cc;"\` |
|        - | 12146 | `     "text-decoration: none;"\` |
|        - | 12147 | `     "}"\` |
|        - | 12148 | `     "a:hover {"\` |
|        - | 12149 | `     "color: #999999;"\` |
|        - | 12150 | `     "}"\` |
|        - | 12151 | `     "a:active {"\` |
|        - | 12152 | `     "color: #663399;"\` |
|        - | 12153 | `     "}"\` |
|        - | 12154 | `     "h1 {"\` |
|        - | 12155 | `     "margin: 0;"\` |
|        - | 12156 | `     "padding: 0;"\` |
|        - | 12157 | `     "font-family: Verdana;"\` |
|        - | 12158 | `     "font-weight: bold;"\` |
|        - | 12159 | `     "font-style: normal;"\` |
|        - | 12160 | `     "font-size: medium;"\` |
|        - | 12161 | `     "text-transform: capitalize;"\` |
|        - | 12162 | `     "color: #0a328c;"\` |
|        - | 12163 | `     "}"\` |
|        - | 12164 | `     "p {"\` |
|        - | 12165 | `     "margin: 0 auto;"\` |
|        - | 12166 | `     "font-size: medium;"\` |
|        - | 12167 | `     "font-style: normal;"\` |
|        - | 12168 | `     "font-family: verdana;"\` |
|        - | 12169 | `     "}"\` |
|        - | 12170 | `"</style></head><body>"\` |
|        - | 12171 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 12172 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 12173 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 12174 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 12175 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 12176 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 12177 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 12178 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 12179 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 12180 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 12181 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 12182 |  |
|        - | 12183 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12184 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 12185 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 12186 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 12187 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12188 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 12189 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12190 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 12191 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 12192 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 12193 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 12194 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 12195 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 12196 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 12197 |  |
|        - | 12198 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 12199 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 12200 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 12201 | `"&nbsp;*<br>"\` |
|        - | 12202 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 12203 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 12204 | `"&nbsp;* are met:<br>"\` |
|        - | 12205 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 12206 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 12207 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 12208 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 12209 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 12210 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 12211 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 12212 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 12213 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 12214 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 12215 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 12216 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 12217 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 12218 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 12219 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 12220 | `"&nbsp;*<br>"\` |
|        - | 12221 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 12222 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 12223 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 12224 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 12225 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 12226 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 12227 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 12228 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 12229 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 12230 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 12231 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 12232 | `"&nbsp;*/<br>"\` |
|        - | 12233 | `"</span></small></small></p>"\` |
|        - | 12234 | `"</div></body></html>"` |
|        - | 12235 | `/*` |
|        - | 12236 | ` * bool ph7credits(void)` |
|        - | 12237 | ` * bool ph7info(void)` |
|        - | 12238 | ` * bool ph7copyright(void)` |
|        - | 12239 | ` *  Prints out the credits for PH7 engine` |
|        - | 12240 | ` * Parameters` |
|        - | 12241 | ` *  None` |
|        - | 12242 | ` * Return` |
|        - | 12243 | ` *  Always TRUE` |
|        - | 12244 | ` */` |
|        2 | 12245 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12246 |  |
|        3 | 12247 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 12248 | `	/* Expand the HTML page above*/` |
|        3 | 12249 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 12250 | `	ph7_context_output_format(` |
|        1 | 12251 | `		pCtx,` |
|        - | 12252 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 12253 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 12254 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 12255 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 12256 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 12257 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 12258 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 12259 | `#ifdef __WINNT__` |
|        - | 12260 | `		"Windows NT"` |
|        - | 12261 | `#elif defined(__UNIXES__)` |
|        - | 12262 | `		"UNIX-Like"` |
|        - | 12263 | `#else` |
|        - | 12264 | `		"Other OS"` |
|        - | 12265 | `#endif` |
|        - | 12266 | `		);` |
|        3 | 12267 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 12268 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12269 | `	SXUNUSED(apArg);` |
|        - | 12270 | `	/* Return TRUE */` |
|        - | 12271 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 12272 | `	return PH7_OK;` |
|        1 | 12273 |  |
|        - | 12274 | `/*` |
|        - | 12275 | ` * Section:` |
|        - | 12276 | ` *    URL related routines.` |
|        - | 12277 | ` * Status:` |
|        - | 12278 | ` *    Stable.` |
|        - | 12279 | ` */` |
|        - | 12280 | `/*` |
|        - | 12281 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 12282 | ` *  Parse a URL and return its fields.` |
|        - | 12283 | ` * Parameters` |
|        - | 12284 | ` *  $url` |
|        - | 12285 | ` *   The URL to parse.` |
|        - | 12286 | ` * $component` |
|        - | 12287 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 12288 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 12289 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 12290 | ` *  in which case the return value will be an integer).` |
|        - | 12291 | ` * Return` |
|        - | 12292 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 12293 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 12294 | ` *  this array are:` |
|        - | 12295 | ` *   scheme - e.g. http` |
|        - | 12296 | ` *   host` |
|        - | 12297 | ` *   port` |
|        - | 12298 | ` *   user` |
|        - | 12299 | ` *   pass` |
|        - | 12300 | ` *   path` |
|        - | 12301 | ` *   query - after the question mark ?` |
|        - | 12302 | ` *   fragment - after the hashmark #` |
|        - | 12303 | ` * Note:` |
|        - | 12304 | ` *  FALSE is returned on failure.` |
|        - | 12305 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 12306 | ` *  with the standard PHP engine.` |
|        - | 12307 | ` */` |
|       28 | 12308 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12309 |  |
|        - | 12310 | `	const char *zStr; /* Input string */` |
|        - | 12311 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 12312 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 12313 | `	int nLen;` |
|        - | 12314 | `	sxi32 rc;` |
|       29 | 12315 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 12316 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 12317 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12318 | `		return PH7_OK;` |
|        - | 12319 | `	}` |
|        - | 12320 | `	/* Extract the given URI */` |
|       29 | 12321 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 12322 | `	if( nLen < 1 ){` |
|        - | 12323 | `		/* Nothing to process,return FALSE */` |
|        3 | 12324 | `		ph7_result_bool(pCtx,0);` |
|        3 | 12325 | `		return PH7_OK;` |
|        - | 12326 | `	}` |
|        - | 12327 | `	/* Get a parse */` |
|       27 | 12328 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 12329 | `	if( rc != SXRET_OK ){` |
|        - | 12330 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 12331 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12332 | `		return PH7_OK;` |
|        - | 12333 | `	}` |
|       27 | 12334 | `	if( nArg > 1 ){` |
|      ! 0 | 12335 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 12336 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 12337 | `		switch(nComponent){` |
|      ! 0 | 12338 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 12339 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 12340 | `			if( pComp->nByte < 1 ){` |
|        - | 12341 | `				/* No available value,return NULL */` |
|      ! 0 | 12342 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12343 | `			}else{` |
|      ! 0 | 12344 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12345 | `			}` |
|      ! 0 | 12346 | `			break;` |
|      ! 0 | 12347 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 12348 | `			pComp = &sURI.sHost;` |
|      ! 0 | 12349 | `			if( pComp->nByte < 1 ){` |
|        - | 12350 | `				/* No available value,return NULL */` |
|      ! 0 | 12351 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12352 | `			}else{` |
|      ! 0 | 12353 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12354 | `			}` |
|      ! 0 | 12355 | `			break;` |
|      ! 0 | 12356 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 12357 | `			pComp = &sURI.sPort;` |
|      ! 0 | 12358 | `			if( pComp->nByte < 1 ){` |
|        - | 12359 | `				/* No available value,return NULL */` |
|      ! 0 | 12360 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12361 | `			}else{` |
|      ! 0 | 12362 | `				int iPort = 0;` |
|        - | 12363 | `				/* Cast the value to integer */` |
|      ! 0 | 12364 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 12365 | `				ph7_result_int(pCtx,iPort);` |
|        - | 12366 | `			}` |
|      ! 0 | 12367 | `			break;` |
|      ! 0 | 12368 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 12369 | `			pComp = &sURI.sUser;` |
|      ! 0 | 12370 | `			if( pComp->nByte < 1 ){` |
|        - | 12371 | `				/* No available value,return NULL */` |
|      ! 0 | 12372 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12373 | `			}else{` |
|      ! 0 | 12374 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12375 | `			}` |
|      ! 0 | 12376 | `			break;` |
|      ! 0 | 12377 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 12378 | `			pComp = &sURI.sPass;` |
|      ! 0 | 12379 | `			if( pComp->nByte < 1 ){` |
|        - | 12380 | `				/* No available value,return NULL */` |
|      ! 0 | 12381 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12382 | `			}else{` |
|      ! 0 | 12383 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12384 | `			}` |
|      ! 0 | 12385 | `			break;` |
|      ! 0 | 12386 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 12387 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 12388 | `			if( pComp->nByte < 1 ){` |
|        - | 12389 | `				/* No available value,return NULL */` |
|      ! 0 | 12390 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12391 | `			}else{` |
|      ! 0 | 12392 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12393 | `			}` |
|      ! 0 | 12394 | `			break;` |
|      ! 0 | 12395 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 12396 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 12397 | `			if( pComp->nByte < 1 ){` |
|        - | 12398 | `				/* No available value,return NULL */` |
|      ! 0 | 12399 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12400 | `			}else{` |
|      ! 0 | 12401 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12402 | `			}` |
|      ! 0 | 12403 | `			break;` |
|      ! 0 | 12404 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 12405 | `			pComp = &sURI.sPath;` |
|      ! 0 | 12406 | `			if( pComp->nByte < 1 ){` |
|        - | 12407 | `				/* No available value,return NULL */` |
|      ! 0 | 12408 | `				ph7_result_null(pCtx);` |
|      ! 0 | 12409 | `			}else{` |
|      ! 0 | 12410 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 12411 | `			}` |
|      ! 0 | 12412 | `			break;` |
|      ! 0 | 12413 | `		default:` |
|        - | 12414 | `			/* No such entry,return NULL */` |
|      ! 0 | 12415 | `			ph7_result_null(pCtx);` |
|      ! 0 | 12416 | `			break;` |
|        - | 12417 | `		}` |
|      ! 0 | 12418 | `	}else{` |
|        - | 12419 | `		ph7_value *pArray,*pValue;` |
|        - | 12420 | `		/* Return an associative array */` |
|       27 | 12421 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 12422 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 12423 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12424 | `			/* Out of memory */` |
|      ! 0 | 12425 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12426 | `			/* Return false */` |
|      ! 0 | 12427 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 12428 | `			return PH7_OK;` |
|        - | 12429 | `		}` |
|        - | 12430 | `		/* Fill the array */` |
|       27 | 12431 | `		pComp = &sURI.sScheme;` |
|       27 | 12432 | `		if( pComp->nByte > 0 ){` |
|       19 | 12433 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 12434 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 12435 | `		}` |
|        - | 12436 | `		/* Reset the string cursor */` |
|       27 | 12437 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12438 | `		pComp = &sURI.sHost;` |
|       27 | 12439 | `		if( pComp->nByte > 0 ){` |
|       25 | 12440 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 12441 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 12442 | `		}` |
|        - | 12443 | `		/* Reset the string cursor */` |
|       27 | 12444 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12445 | `		pComp = &sURI.sPort;` |
|       27 | 12446 | `		if( pComp->nByte > 0 ){` |
|       11 | 12447 | `			int iPort = 0;/* cc warning */` |
|        - | 12448 | `			/* Convert to integer */` |
|       11 | 12449 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 12450 | `			ph7_value_int(pValue,iPort);` |
|       11 | 12451 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 12452 | `		}` |
|        - | 12453 | `		/* Reset the string cursor */` |
|       27 | 12454 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12455 | `		pComp = &sURI.sUser;` |
|       27 | 12456 | `		if( pComp->nByte > 0 ){` |
|        7 | 12457 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12458 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 12459 | `		}` |
|        - | 12460 | `		/* Reset the string cursor */` |
|       27 | 12461 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12462 | `		pComp = &sURI.sPass;` |
|       27 | 12463 | `		if( pComp->nByte > 0 ){` |
|        7 | 12464 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 12465 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 12466 | `		}` |
|        - | 12467 | `		/* Reset the string cursor */` |
|       27 | 12468 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12469 | `		pComp = &sURI.sPath;` |
|       27 | 12470 | `		if( pComp->nByte > 0 ){` |
|       17 | 12471 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 12472 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 12473 | `		}` |
|        - | 12474 | `		/* Reset the string cursor */` |
|       27 | 12475 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12476 | `		pComp = &sURI.sQuery;` |
|       27 | 12477 | `		if( pComp->nByte > 0 ){` |
|        5 | 12478 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12479 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 12480 | `		}` |
|        - | 12481 | `		/* Reset the string cursor */` |
|       27 | 12482 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 12483 | `		pComp = &sURI.sFragment;` |
|       27 | 12484 | `		if( pComp->nByte > 0 ){` |
|        5 | 12485 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 12486 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 12487 | `		}` |
|        - | 12488 | `		/* Return the created array */` |
|       27 | 12489 | `		ph7_result_value(pCtx,pArray);` |
|        - | 12490 | `		/* NOTE:` |
|        - | 12491 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 12492 | `		 * automatically as soon we return from this function.` |
|        - | 12493 | `		 */` |
|        - | 12494 | `	}` |
|        - | 12495 | `	/* All done */` |
|       27 | 12496 | `	return PH7_OK;` |
|       15 | 12497 |  |
|        - | 12498 | `/*` |
|        - | 12499 | ` * Section:` |
|        - | 12500 | ` *   Array related routines.` |
|        - | 12501 | ` * Status:` |
|        - | 12502 | ` *    Stable.` |
|        - | 12503 | ` * Note 2012-5-21 01:04:15:` |
|        - | 12504 | ` *  Array related functions that need access to the underlying` |
|        - | 12505 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 12506 | ` */` |
|        - | 12507 | `/*` |
|        - | 12508 | ` * The [compact()] function store it's state information in an instance` |
|        - | 12509 | ` * of the following structure.` |
|        - | 12510 | ` */` |
|        - | 12511 | `struct compact_data` |
|        - | 12512 |  |
|        - | 12513 | `	ph7_value *pArray;  /* Target array */` |
|        - | 12514 | `	int nRecCount;      /* Recursion count */` |
|        - | 12515 | `};` |
|        - | 12516 | `/*` |
|        - | 12517 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 12518 | ` */` |
|      ! 0 | 12519 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 12520 |  |
|      ! 0 | 12521 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 12522 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 12523 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12524 | `	/* Act according to the hashmap value */` |
|      ! 0 | 12525 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 12526 | `		SyString sVar;` |
|      ! 0 | 12527 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 12528 | `		if( sVar.nByte > 0 ){` |
|        - | 12529 | `			/* Query the current frame */` |
|      ! 0 | 12530 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 12531 | `			/* ^` |
|        - | 12532 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 12533 | `			 */` |
|      ! 0 | 12534 | `			if( pKey ){` |
|        - | 12535 | `				/* Perform the insertion */` |
|      ! 0 | 12536 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 12537 | `			}` |
|      ! 0 | 12538 | `		}` |
|      ! 0 | 12539 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 12540 | `		int rc;` |
|        - | 12541 | `		/* Recursively traverse this array */` |
|      ! 0 | 12542 | `		pData->nRecCount++;` |
|      ! 0 | 12543 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 12544 | `		pData->nRecCount--;` |
|      ! 0 | 12545 | `		return rc;` |
|        - | 12546 | `	}` |
|      ! 0 | 12547 | `	return SXRET_OK;` |
|      ! 0 | 12548 |  |
|        - | 12549 | `/*` |
|        - | 12550 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 12551 | ` *  Create array containing variables and their values.` |
|        - | 12552 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 12553 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 12554 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 12555 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 12556 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 12557 | ` * Parameters` |
|        - | 12558 | ` *  $varname` |
|        - | 12559 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 12560 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 12561 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 12562 | ` *   it recursively.` |
|        - | 12563 | ` * Return` |
|        - | 12564 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 12565 | ` */` |
|        2 | 12566 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12567 |  |
|        - | 12568 | `	ph7_value *pArray,*pObj;` |
|        3 | 12569 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12570 | `	const char *zName;` |
|        - | 12571 | `	SyString sVar;` |
|        - | 12572 | `	int i,nLen;` |
|        3 | 12573 | `	if( nArg < 1 ){` |
|        - | 12574 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 12575 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12576 | `		return PH7_OK;` |
|        - | 12577 | `	}` |
|        - | 12578 | `	/* Create the array */` |
|        3 | 12579 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12580 | `	if( pArray == 0 ){` |
|        - | 12581 | `		/* Out of memory */` |
|      ! 0 | 12582 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 12583 | `		/* Return NULL */` |
|      ! 0 | 12584 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12585 | `		return PH7_OK;` |
|        - | 12586 | `	}` |
|        - | 12587 | `	/* Perform the requested operation */` |
|        7 | 12588 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 12589 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 12590 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 12591 | `				struct compact_data sData;` |
|      ! 0 | 12592 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 12593 | `				/* Recursively walk the array */` |
|      ! 0 | 12594 | `				sData.nRecCount = 0;` |
|      ! 0 | 12595 | `				sData.pArray = pArray;` |
|      ! 0 | 12596 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 12597 | `			}` |
|      ! 0 | 12598 | `		}else{` |
|        - | 12599 | `			/* Extract variable name */` |
|        5 | 12600 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 12601 | `			if( nLen > 0 ){` |
|        5 | 12602 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 12603 | `				/* Check if the variable is available in the current frame */` |
|        5 | 12604 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 12605 | `				if( pObj ){` |
|        5 | 12606 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 12607 | `				}` |
|        2 | 12608 | `			}` |
|        - | 12609 | `		}` |
|        3 | 12610 | `	}` |
|        - | 12611 | `	/* Return the array */` |
|        3 | 12612 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12613 | `	return PH7_OK;` |
|        2 | 12614 |  |
|        - | 12615 | `/*` |
|        - | 12616 | ` * The [extract()] function store it's state information in an instance` |
|        - | 12617 | ` * of the following structure.` |
|        - | 12618 | ` */` |
|        - | 12619 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 12620 | `struct extract_aux_data` |
|        - | 12621 |  |
|        - | 12622 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 12623 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 12624 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 12625 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 12626 | `	int iFlags;           /* Control flags */` |
|        - | 12627 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 12628 | `};` |
|        - | 12629 | `/* Forward declaration */` |
|        - | 12630 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 12631 | `/*` |
|        - | 12632 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 12633 | ` *   Import variables into the current symbol table from an array.` |
|        - | 12634 | ` * Parameters` |
|        - | 12635 | ` * $var_array` |
|        - | 12636 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 12637 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 12638 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 12639 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 12640 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 12641 | ` * $extract_type` |
|        - | 12642 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 12643 | ` *  It can be one of the following values:` |
|        - | 12644 | ` *   EXTR_OVERWRITE` |
|        - | 12645 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 12646 | ` *   EXTR_SKIP` |
|        - | 12647 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 12648 | ` *   EXTR_PREFIX_SAME` |
|        - | 12649 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 12650 | ` *   EXTR_PREFIX_ALL` |
|        - | 12651 | ` *       Prefix all variable names with prefix.` |
|        - | 12652 | ` *   EXTR_PREFIX_INVALID` |
|        - | 12653 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 12654 | ` *   EXTR_IF_EXISTS` |
|        - | 12655 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 12656 | ` *       otherwise do nothing.` |
|        - | 12657 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 12658 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 12659 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 12660 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 12661 | ` *      the current symbol table.` |
|        - | 12662 | ` * $prefix` |
|        - | 12663 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 12664 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 12665 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 12666 | ` *  underscore character.` |
|        - | 12667 | ` * Return` |
|        - | 12668 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 12669 | ` */` |
|        4 | 12670 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12671 |  |
|        - | 12672 | `	extract_aux_data sAux;` |
|        - | 12673 | `	ph7_hashmap *pMap;` |
|        5 | 12674 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 12675 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 12676 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12677 | `		return PH7_OK;` |
|        - | 12678 | `	}` |
|        - | 12679 | `	/* Point to the target hashmap */` |
|        5 | 12680 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 12681 | `	if( pMap->nEntry < 1 ){` |
|        - | 12682 | `		/* Empty map,return  0 */` |
|      ! 0 | 12683 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 12684 | `		return PH7_OK;` |
|        - | 12685 | `	}` |
|        - | 12686 | `	/* Prepare the aux data */` |
|        5 | 12687 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 12688 | `	if( nArg > 1 ){` |
|        3 | 12689 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 12690 | `		if( nArg > 2 ){` |
|      ! 0 | 12691 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 12692 | `		}` |
|        1 | 12693 | `	}` |
|        5 | 12694 | `	sAux.pVm = pCtx->pVm;` |
|        - | 12695 | `	/* Invoke the worker callback */` |
|        5 | 12696 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 12697 | `	/* Number of variables successfully imported */` |
|        5 | 12698 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 12699 | `	return PH7_OK;` |
|        3 | 12700 |  |
|        - | 12701 | `/*` |
|        - | 12702 | ` * Worker callback for the [extract()] function defined` |
|        - | 12703 | ` * below.` |
|        - | 12704 | ` */` |
|        8 | 12705 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12706 |  |
|        9 | 12707 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 12708 | `	int iFlags = pAux->iFlags;` |
|        9 | 12709 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12710 | `	ph7_value *pObj;` |
|        - | 12711 | `	SyString sVar;` |
|        9 | 12712 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 12713 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 12714 | `	}` |
|        - | 12715 | `	/* Perform a string cast */` |
|        9 | 12716 | `	PH7_MemObjToString(pKey);` |
|        9 | 12717 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12718 | `		/* Unavailable variable name */` |
|      ! 0 | 12719 | `		return SXRET_OK;` |
|        - | 12720 | `	}` |
|        9 | 12721 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 12722 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 12723 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12724 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12725 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12726 | `			);` |
|      ! 0 | 12727 | `	}else{` |
|       13 | 12728 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 12729 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12730 | `	}` |
|        9 | 12731 | `	sVar.zString = pAux->zWorker;` |
|        - | 12732 | `	/* Try to extract the variable */` |
|        9 | 12733 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 12734 | `	if( pObj ){` |
|        - | 12735 | `		/* Collision */` |
|        5 | 12736 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 12737 | `			return SXRET_OK;` |
|        - | 12738 | `		}` |
|        5 | 12739 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 12740 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 12741 | `				/* Already prefixed */` |
|      ! 0 | 12742 | `				return SXRET_OK;` |
|        - | 12743 | `			}` |
|      ! 0 | 12744 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 12745 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 12746 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12747 | `				);` |
|      ! 0 | 12748 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 12749 | `		}` |
|        3 | 12750 | `	}else{` |
|        - | 12751 | `		/* Create the variable */` |
|        5 | 12752 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 12753 | `	}` |
|        9 | 12754 | `	if( pObj ){` |
|        - | 12755 | `		/* Overwrite the old value */` |
|        9 | 12756 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 12757 | `		/* Increment counter */` |
|        9 | 12758 | `		pAux->iCount++;` |
|        4 | 12759 | `	}` |
|        9 | 12760 | `	return SXRET_OK;` |
|        5 | 12761 |  |
|        - | 12762 | `/*` |
|        - | 12763 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 12764 | ` * defined below.` |
|        - | 12765 | ` */` |
|        2 | 12766 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 12767 |  |
|        3 | 12768 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 12769 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 12770 | `	ph7_value *pObj;` |
|        - | 12771 | `	SyString sVar;` |
|        - | 12772 | `	/* Perform a string cast */` |
|        3 | 12773 | `	PH7_MemObjToString(pKey);` |
|        3 | 12774 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 12775 | `		/* Unavailable variable name */` |
|      ! 0 | 12776 | `		return SXRET_OK;` |
|        - | 12777 | `	}` |
|        3 | 12778 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 12779 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 12780 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 12781 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 12782 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 12783 | `			);` |
|        2 | 12784 | `	}else{` |
|      ! 0 | 12785 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 12786 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 12787 | `	}` |
|        3 | 12788 | `	sVar.zString = pAux->zWorker;` |
|        - | 12789 | `	/* Extract the variable */` |
|        3 | 12790 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 12791 | `	if( pObj ){` |
|        3 | 12792 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 12793 | `	}` |
|        3 | 12794 | `	return SXRET_OK;` |
|        2 | 12795 |  |
|        - | 12796 | `/*` |
|        - | 12797 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 12798 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 12799 | ` * Parameters` |
|        - | 12800 | ` * $types` |
|        - | 12801 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 12802 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 12803 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 12804 | ` *  POST includes the POST uploaded file information.` |
|        - | 12805 | ` *  Note:` |
|        - | 12806 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 12807 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 12808 | ` * $prefix` |
|        - | 12809 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 12810 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 12811 | ` *  variable named $pref_userid.` |
|        - | 12812 | ` * Return` |
|        - | 12813 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12814 | ` */` |
|        2 | 12815 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12816 |  |
|        - | 12817 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 12818 | `	extract_aux_data sAux;` |
|        - | 12819 | `	int nLen,nPrefixLen;` |
|        - | 12820 | `	ph7_value *pSuper;` |
|        - | 12821 | `	ph7_vm *pVm;` |
|        - | 12822 | `	/* By default import only $_GET variables  */` |
|        3 | 12823 | `	zImport = "G";` |
|        3 | 12824 | `	nLen = (int)sizeof(char);` |
|        3 | 12825 | `	zPrefix = 0;` |
|        3 | 12826 | `	nPrefixLen = 0;` |
|        3 | 12827 | `	if( nArg > 0 ){` |
|        3 | 12828 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 12829 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 12830 | `		}` |
|        3 | 12831 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12832 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 12833 | `		}` |
|        1 | 12834 | `	}` |
|        - | 12835 | `	/* Point to the underlying VM */` |
|        3 | 12836 | `	pVm = pCtx->pVm;` |
|        - | 12837 | `	/* Initialize the aux data */` |
|        3 | 12838 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 12839 | `	sAux.zPrefix = zPrefix;` |
|        3 | 12840 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 12841 | `	sAux.pVm = pVm;` |
|        - | 12842 | `	/* Extract */` |
|        3 | 12843 | `	zEnd = &zImport[nLen];` |
|        5 | 12844 | `	while( zImport < zEnd ){` |
|        3 | 12845 | `		int c = zImport[0];` |
|        3 | 12846 | `		pSuper = 0;` |
|        3 | 12847 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 12848 | `			/* Import $_GET variables */` |
|        3 | 12849 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 12850 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 12851 | `			/* Import $_POST variables */` |
|      ! 0 | 12852 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 12853 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 12854 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 12855 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 12856 | `		}` |
|        3 | 12857 | `		if( pSuper ){` |
|        - | 12858 | `			/* Iterate throw array entries */` |
|        3 | 12859 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 12860 | `		}` |
|        - | 12861 | `		/* Advance the cursor */` |
|        3 | 12862 | `		zImport++;` |
|        1 | 12863 | `	}` |
|        - | 12864 | `	/* All done,return TRUE*/` |
|        3 | 12865 | `	ph7_result_bool(pCtx,0);` |
|        3 | 12866 | `	return PH7_OK;` |
|        1 | 12867 |  |
|        - | 12868 | `/*` |
|        - | 12869 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 12870 | ` * Refer to the eval() language construct implementation for more` |
|        - | 12871 | ` * information.` |
|        - | 12872 | ` */` |
|    11666 | 12873 | `static sxi32 VmEvalChunk(` |
|        - | 12874 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 12875 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 12876 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 12877 | `	int iFlags,         /* Compile flag */` |
|        - | 12878 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 12879 | `	)` |
|        2 | 12880 |  |
|        - | 12881 | `	SySet *pByteCode,aByteCode;` |
|        - | 12882 | `	SyBlob sSavedNs;` |
|    11668 | 12883 | `	ProcConsumer xErr = 0;` |
|    11668 | 12884 | `	void *pErrData = 0;` |
|        - | 12885 | `	/* Initialize bytecode container */` |
|    11668 | 12886 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    11668 | 12887 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 12888 | `	/* Reset the code generator */` |
|    11668 | 12889 | `	if( bTrueReturn ){` |
|        - | 12890 | `		/* Included file,log compile-time errors */` |
|     8782 | 12891 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     8782 | 12892 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4390 | 12893 | `	}` |
|    11668 | 12894 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 12895 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 12896 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 12897 | `	 * the caller's namespace is restored. */` |
|    11668 | 12898 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    11668 | 12899 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    11668 | 12900 | `	if( bTrueReturn ){` |
|        - | 12901 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     8782 | 12902 | `		SyBlobReset(&pVm->sNamespace);` |
|     4390 | 12903 | `	}` |
|        - | 12904 | `	/* Swap bytecode container */` |
|    11668 | 12905 | `	pByteCode = pVm->pByteContainer;` |
|    11668 | 12906 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 12907 | `	/* Compile the chunk */` |
|    11668 | 12908 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    17501 | 12909 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 12910 | `		/* Compilation error,return false */` |
|        3 | 12911 | `		if( pCtx ){` |
|        3 | 12912 | `			ph7_result_bool(pCtx,0);` |
|        1 | 12913 | `		}` |
|        2 | 12914 | `	}else{` |
|        - | 12915 | `		/* Mount any newly defined classes */` |
|        - | 12916 | `		SyHashEntry *pEntry;` |
|        - | 12917 | `		ph7_class *pClass;` |
|        - | 12918 | `		ph7_value sResult; /* Return value */` |
|        - | 12919 | `		sxi32 rc;` |
|    11666 | 12920 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   511786 | 12921 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   494290 | 12922 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 12923 | `			/* Only mount classes that haven't been mounted yet */` |
|   494290 | 12924 | `			if( !pClass->bMounted ){` |
|    92586 | 12925 | `				rc = VmMountUserClass(pVm,pClass);` |
|    92586 | 12926 | `				if( rc != SXRET_OK ){` |
|        - | 12927 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 12928 | `					if( pCtx ){` |
|      ! 0 | 12929 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 12930 | `					}` |
|      ! 0 | 12931 | `					goto Cleanup;` |
|        - | 12932 | `				}` |
|    46292 | 12933 | `			}` |
|        2 | 12934 | `		}` |
|    11666 | 12935 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 12936 | `			/* Out of memory */` |
|      ! 0 | 12937 | `			if( pCtx ){` |
|      ! 0 | 12938 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 12939 | `			}` |
|      ! 0 | 12940 | `			goto Cleanup;` |
|        - | 12941 | `		}` |
|    11666 | 12942 | `		if( bTrueReturn ){` |
|        - | 12943 | `			/* Assume a boolean true return value */` |
|     8782 | 12944 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4392 | 12945 | `		}else{` |
|        - | 12946 | `			/* Assume a null return value */` |
|     2886 | 12947 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 12948 | `		}` |
|        - | 12949 | `		/* Execute the compiled chunk */` |
|    11666 | 12950 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    11666 | 12951 | `		if( pCtx ){` |
|        - | 12952 | `			/* Set the execution result */` |
|     8800 | 12953 | `			ph7_result_value(pCtx,&sResult);` |
|     4399 | 12954 | `		}` |
|    11666 | 12955 | `		PH7_MemObjRelease(&sResult);` |
|        - | 12956 | `	}` |
|     5833 | 12957 | `Cleanup:` |
|        - | 12958 | `	/* Cleanup the mess left behind */` |
|    11668 | 12959 | `	pVm->pByteContainer = pByteCode;` |
|    11668 | 12960 | `	SySetRelease(&aByteCode);` |
|        - | 12961 | `	/* Restore caller's namespace state */` |
|    11668 | 12962 | `	SyBlobReset(&pVm->sNamespace);` |
|    11668 | 12963 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    11668 | 12964 | `	SyBlobRelease(&sSavedNs);` |
|    11668 | 12965 | `	return SXRET_OK;` |
|        2 | 12966 |  |
|        - | 12967 | `/*` |
|        - | 12968 | ` * value eval(string $code)` |
|        - | 12969 | ` *   Evaluate a string as PHP code.` |
|        - | 12970 | ` * Parameter` |
|        - | 12971 | ` *  code: PHP code to evaluate.` |
|        - | 12972 | ` * Return` |
|        - | 12973 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 12974 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 12975 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 12976 | ` */` |
|       22 | 12977 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12978 |  |
|        - | 12979 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 12980 | `	if( nArg < 1 ){` |
|        - | 12981 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 12982 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12983 | `		return SXRET_OK;` |
|        - | 12984 | `	}` |
|        - | 12985 | `	/* Chunk to evaluate */` |
|       24 | 12986 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 12987 | `	if( sChunk.nByte < 1 ){` |
|        - | 12988 | `		/* Empty string,return NULL */` |
|        3 | 12989 | `		ph7_result_null(pCtx);` |
|        3 | 12990 | `		return SXRET_OK;` |
|        - | 12991 | `	}` |
|        - | 12992 | `	/* Eval the chunk */` |
|       22 | 12993 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 12994 | `	return SXRET_OK;` |
|       13 | 12995 |  |
|        - | 12996 | `/*` |
|        - | 12997 | ` * Check if a file path is already included.` |
|        - | 12998 | ` */` |
|    17556 | 12999 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 13000 |  |
|        - | 13001 | `	SyString *aEntries;` |
|        - | 13002 | `	sxu32 n;` |
|    17558 | 13003 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 13004 | `	/* Perform a linear search */` |
| 77002734 | 13005 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 76985184 | 13006 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 13007 | `			/* Already included */` |
|        7 | 13008 | `			return TRUE;` |
|        - | 13009 | `		}` |
| 38492590 | 13010 | `	}` |
|    17552 | 13011 | `	return FALSE;` |
|     8780 | 13012 |  |
|        - | 13013 | `/*` |
|        - | 13014 | ` * Push a file path in the appropriate VM container.` |
|        - | 13015 | ` */` |
|    20414 | 13016 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 13017 |  |
|        - | 13018 | `	SyString sPath;` |
|        - | 13019 | `	char *zDup;` |
|        - | 13020 | `#ifdef __WINNT__` |
|        - | 13021 | `	char *zCur;` |
|        - | 13022 | `#endif` |
|        - | 13023 | `	sxi32 rc;` |
|    20416 | 13024 | `	if( nLen < 0 ){` |
|     2860 | 13025 | `		nLen = SyStrlen(zPath);` |
|     1429 | 13026 | `	}` |
|        - | 13027 | `	/* Duplicate the file path first */` |
|    20416 | 13028 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    20416 | 13029 | `	if( zDup == 0 ){` |
|      ! 0 | 13030 | `		return SXERR_MEM;` |
|        - | 13031 | `	}` |
|        - | 13032 | `#ifdef __WINNT__` |
|        - | 13033 | `	/* Normalize path on windows` |
|        - | 13034 | `	 * Example:` |
|        - | 13035 | `	 *    Path/To/File.php` |
|        - | 13036 | `	 * becomes` |
|        - | 13037 | `	 *   path\to\file.php` |
|        - | 13038 | `	 */` |
|        2 | 13039 | `	zCur = zDup;` |
|        2 | 13040 | `	while( zCur[0] != 0 ){` |
|        2 | 13041 | `		if( zCur[0] == '/' ){` |
|        2 | 13042 | `			zCur[0] = '\\';` |
|        2 | 13043 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 13044 | `			int c = SyToLower(zCur[0]);` |
|        1 | 13045 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 13046 | `		}` |
|        2 | 13047 | `		zCur++;` |
|        2 | 13048 | `	}` |
|        - | 13049 | `#endif` |
|        - | 13050 | `	/* Install the file path */` |
|    20416 | 13051 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    20416 | 13052 | `	if( !bMain ){` |
|    17558 | 13053 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 13054 | `			/* Already included */` |
|        7 | 13055 | `			*pNew = 0;` |
|        4 | 13056 | `		}else{` |
|        - | 13057 | `			/* Insert in the corresponding container */` |
|    17552 | 13058 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    17552 | 13059 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 13060 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 13061 | `				return rc;` |
|        - | 13062 | `			}` |
|    17552 | 13063 | `			*pNew = 1;` |
|        - | 13064 | `		}` |
|     8778 | 13065 | `	}` |
|    20416 | 13066 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    20416 | 13067 | `	return SXRET_OK;` |
|    10209 | 13068 |  |
|        - | 13069 | `/*` |
|        - | 13070 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 13071 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 13072 | ` * indicates failure.` |
|        - | 13073 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 13074 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 13075 | ` * operations.` |
|        - | 13076 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 13077 | ` * this function is a no-op.` |
|        - | 13078 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 13079 | ` * constructs for more information.` |
|        - | 13080 | ` */` |
|     8790 | 13081 | `static sxi32 VmExecIncludedFile(` |
|        - | 13082 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 13083 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 13084 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 13085 | `	 )` |
|        2 | 13086 |  |
|        - | 13087 | `	sxi32 rc;` |
|        - | 13088 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13089 | `	const ph7_io_stream *pStream;` |
|        - | 13090 | `	SyBlob sContents;` |
|        - | 13091 | `	void *pHandle;` |
|        - | 13092 | `	ph7_vm *pVm;` |
|        - | 13093 | `	int isNew;` |
|        - | 13094 | `	/* Initialize fields */` |
|     8792 | 13095 | `	pVm = pCtx->pVm;` |
|     8792 | 13096 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     8792 | 13097 | `	isNew = 0;` |
|        - | 13098 | `	/* Extract the associated stream */` |
|     8792 | 13099 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 13100 | `	/*` |
|        - | 13101 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 13102 | `	 * in a read-only mode.` |
|        - | 13103 | `	 */` |
|     8792 | 13104 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     8792 | 13105 | `	if( pHandle == 0 ){` |
|        8 | 13106 | `		return SXERR_IO;` |
|        - | 13107 | `	}` |
|     8786 | 13108 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     8786 | 13109 | `	if( IncludeOnce && !isNew ){` |
|        - | 13110 | `		/* Already included */` |
|        5 | 13111 | `		rc = SXERR_EXISTS;` |
|        3 | 13112 | `	}else{` |
|        - | 13113 | `		/* Read the whole file contents */` |
|     8782 | 13114 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     8782 | 13115 | `		if( rc == SXRET_OK ){` |
|        - | 13116 | `			SyString sScript;` |
|        - | 13117 | `			/* Compile and execute the script */` |
|     8782 | 13118 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     8782 | 13119 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4390 | 13120 | `		}` |
|        - | 13121 | `	}` |
|        - | 13122 | `	/* Pop from the set of included file */` |
|     8786 | 13123 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 13124 | `	/* Close the handle */` |
|     8786 | 13125 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 13126 | `	/* Release the working buffer */` |
|     8786 | 13127 | `	SyBlobRelease(&sContents);` |
|        - | 13128 | `#else` |
|        - | 13129 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 13130 | `	SXUNUSED(pPath);` |
|        - | 13131 | `	SXUNUSED(IncludeOnce);` |
|        - | 13132 | `	rc = SXERR_IO;` |
|        - | 13133 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     8786 | 13134 | `	return rc;` |
|     4397 | 13135 |  |
|        - | 13136 | `/*` |
|        - | 13137 | ` * string get_include_path(void)` |
|        - | 13138 | ` *  Gets the current include_path configuration option.` |
|        - | 13139 | ` * Parameter` |
|        - | 13140 | ` *  None` |
|        - | 13141 | ` * Return` |
|        - | 13142 | ` *  Included paths as a string` |
|        - | 13143 | ` */` |
|        2 | 13144 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13145 |  |
|        3 | 13146 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13147 | `	SyString *aEntry;` |
|        - | 13148 | `	int dir_sep;` |
|        - | 13149 | `	sxu32 n;` |
|        - | 13150 | `#ifdef __WINNT__` |
|        1 | 13151 | `	dir_sep = ';';` |
|        - | 13152 | `#else` |
|        - | 13153 | `	/* Assume UNIX path separator */` |
|        2 | 13154 | `	dir_sep = ':';` |
|        - | 13155 | `#endif` |
|        1 | 13156 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13157 | `	SXUNUSED(apArg);` |
|        - | 13158 | `	/* Point to the list of import paths */` |
|        3 | 13159 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 13160 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 13161 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 13162 | `		if( n > 0 ){` |
|        - | 13163 | `			/* Append dir seprator */` |
|      ! 0 | 13164 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 13165 | `		}` |
|        - | 13166 | `		/* Append path */` |
|        3 | 13167 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 13168 | `	}` |
|        3 | 13169 | `	return PH7_OK;` |
|        1 | 13170 |  |
|        - | 13171 | `/*` |
|        - | 13172 | ` * string get_get_included_files(void)` |
|        - | 13173 | ` *  Gets the current include_path configuration option.` |
|        - | 13174 | ` * Parameter` |
|        - | 13175 | ` *  None` |
|        - | 13176 | ` * Return` |
|        - | 13177 | ` *  Included paths as a string` |
|        - | 13178 | ` */` |
|        2 | 13179 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13180 |  |
|        3 | 13181 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 13182 | `	ph7_value *pArray,*pWorker;` |
|        - | 13183 | `	SyString *pEntry;` |
|        - | 13184 | `	int c,d;` |
|        - | 13185 | `	/* Create an array and a working value */` |
|        3 | 13186 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 13187 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 13188 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 13189 | `		/* Out of memory,return null */` |
|      ! 0 | 13190 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13191 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13192 | `		SXUNUSED(apArg);` |
|      ! 0 | 13193 | `		return PH7_OK;` |
|        - | 13194 | `	}` |
|        3 | 13195 | `	c = d = '/';` |
|        - | 13196 | `#ifdef __WINNT__` |
|        1 | 13197 | `	d = '\\';` |
|        - | 13198 | `#endif` |
|        - | 13199 | `	/* Iterate throw entries */` |
|        3 | 13200 | `	SySetResetCursor(pFiles);` |
|     3839 | 13201 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 13202 | `		const char *zBase,*zEnd;` |
|        - | 13203 | `		int iLen;` |
|        - | 13204 | `		/* reset the string cursor */` |
|     3837 | 13205 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 13206 | `		/* Extract base name */` |
|     3837 | 13207 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 13208 | `		/* Ignore trailing '/' */` |
|     5755 | 13209 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 13210 | `			zEnd--;` |
|      ! 0 | 13211 | `		}` |
|     3837 | 13212 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 13213 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 13214 | `			zEnd--;` |
|        1 | 13215 | `		}` |
|     3837 | 13216 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 13217 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 13218 | `		/* Copy entry name */` |
|     3837 | 13219 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 13220 | `		/* Perform the insertion */` |
|     3837 | 13221 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 13222 | `	}` |
|        - | 13223 | `	/* All done,return the created array */` |
|        3 | 13224 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13225 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 13226 | `	 * by the engine as soon we return from this foreign` |
|        - | 13227 | `	 * function.` |
|        - | 13228 | `	 */` |
|        3 | 13229 | `	return PH7_OK;` |
|        2 | 13230 |  |
|        - | 13231 | `/*` |
|        - | 13232 | ` * include:` |
|        - | 13233 | ` * According to the PHP reference manual.` |
|        - | 13234 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 13235 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 13236 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 13237 | ` *  include() will finally check in the calling script's own directory` |
|        - | 13238 | ` *  and the current working directory before failing. The include()` |
|        - | 13239 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 13240 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 13241 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 13242 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 13243 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 13244 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 13245 | ` *  directory to find the requested file.` |
|        - | 13246 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 13247 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 13248 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 13249 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 13250 | ` */` |
|     8772 | 13251 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13252 |  |
|        - | 13253 | `	SyString sFile;` |
|        - | 13254 | `	sxi32 rc;` |
|     8774 | 13255 | `	if( nArg < 1 ){` |
|        - | 13256 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13257 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13258 | `		return SXRET_OK;` |
|        - | 13259 | `	}` |
|        - | 13260 | `	/* File to include */` |
|     8774 | 13261 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     8774 | 13262 | `	if( sFile.nByte < 1 ){` |
|        - | 13263 | `		/* Empty string,return NULL */` |
|      ! 0 | 13264 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13265 | `		return SXRET_OK;` |
|        - | 13266 | `	}` |
|        - | 13267 | `	/* Open,compile and execute the desired script */` |
|     8774 | 13268 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     8774 | 13269 | `	if( rc != SXRET_OK ){` |
|        - | 13270 | `		/* Emit a warning and return false */` |
|        3 | 13271 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 13272 | `		ph7_result_bool(pCtx,0);` |
|        1 | 13273 | `	}` |
|     8774 | 13274 | `	return SXRET_OK;` |
|     4388 | 13275 |  |
|        - | 13276 | `/*` |
|        - | 13277 | ` * include_once:` |
|        - | 13278 | ` *  According to the PHP reference manual.` |
|        - | 13279 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 13280 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 13281 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 13282 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 13283 | ` *   just once.` |
|        - | 13284 | ` */` |
|        4 | 13285 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13286 |  |
|        - | 13287 | `	SyString sFile;` |
|        - | 13288 | `	sxi32 rc;` |
|        5 | 13289 | `	if( nArg < 1 ){` |
|        - | 13290 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13291 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13292 | `		return SXRET_OK;` |
|        - | 13293 | `	}` |
|        - | 13294 | `	/* File to include */` |
|        5 | 13295 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13296 | `	if( sFile.nByte < 1 ){` |
|        - | 13297 | `		/* Empty string,return NULL */` |
|      ! 0 | 13298 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13299 | `		return SXRET_OK;` |
|        - | 13300 | `	}` |
|        - | 13301 | `	/* Open,compile and execute the desired script */` |
|        5 | 13302 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13303 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13304 | `		/* File already included,return TRUE */` |
|        3 | 13305 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13306 | `		return SXRET_OK;` |
|        - | 13307 | `	}` |
|        3 | 13308 | `	if( rc != SXRET_OK ){` |
|        - | 13309 | `		/* Emit a warning and return false */` |
|      ! 0 | 13310 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13311 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13312 | ` 	}` |
|        3 | 13313 | `	return SXRET_OK;` |
|        3 | 13314 |  |
|        - | 13315 | `/*` |
|        - | 13316 | ` * require.` |
|        - | 13317 | ` *  According to the PHP reference manual.` |
|        - | 13318 | ` *   require() is identical to include() except upon failure it will` |
|        - | 13319 | ` *   also produce a fatal level error.` |
|        - | 13320 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 13321 | ` *   emits a warning  which allows the script to continue.` |
|        - | 13322 | ` */` |
|        6 | 13323 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13324 |  |
|        - | 13325 | `	SyString sFile;` |
|        - | 13326 | `	sxi32 rc;` |
|        8 | 13327 | `	if( nArg < 1 ){` |
|        - | 13328 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13329 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13330 | `		return SXRET_OK;` |
|        - | 13331 | `	}` |
|        - | 13332 | `	/* File to include */` |
|        8 | 13333 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 13334 | `	if( sFile.nByte < 1 ){` |
|        - | 13335 | `		/* Empty string,return NULL */` |
|      ! 0 | 13336 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13337 | `		return SXRET_OK;` |
|        - | 13338 | `	}` |
|        - | 13339 | `	/* Open,compile and execute the desired script */` |
|        8 | 13340 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 13341 | `	if( rc != SXRET_OK ){` |
|        - | 13342 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13343 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13344 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13345 | `		return PH7_ABORT;` |
|        - | 13346 | `	}` |
|        8 | 13347 | `	return SXRET_OK;` |
|        5 | 13348 |  |
|        - | 13349 | `/*` |
|        - | 13350 | ` * require_once:` |
|        - | 13351 | ` *  According to the PHP reference manual.` |
|        - | 13352 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 13353 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 13354 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 13355 | ` *   and how it differs from its non _once siblings.` |
|        - | 13356 | ` */` |
|        4 | 13357 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13358 |  |
|        - | 13359 | `	SyString sFile;` |
|        - | 13360 | `	sxi32 rc;` |
|        5 | 13361 | `	if( nArg < 1 ){` |
|        - | 13362 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13363 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13364 | `		return SXRET_OK;` |
|        - | 13365 | `	}` |
|        - | 13366 | `	/* File to include */` |
|        5 | 13367 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 13368 | `	if( sFile.nByte < 1 ){` |
|        - | 13369 | `		/* Empty string,return NULL */` |
|      ! 0 | 13370 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13371 | `		return SXRET_OK;` |
|        - | 13372 | `	}` |
|        - | 13373 | `	/* Open,compile and execute the desired script */` |
|        5 | 13374 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 13375 | `	if( rc == SXERR_EXISTS ){` |
|        - | 13376 | `		/* File already included,return TRUE */` |
|        3 | 13377 | `		ph7_result_bool(pCtx,1);` |
|        3 | 13378 | `		return SXRET_OK;` |
|        - | 13379 | `	}` |
|        3 | 13380 | `	if( rc != SXRET_OK ){` |
|        - | 13381 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 13382 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 13383 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13384 | `		return PH7_ABORT;` |
|        - | 13385 | `	}` |
|        3 | 13386 | `	return SXRET_OK;` |
|        3 | 13387 |  |
|        - | 13388 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 13389 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 13390 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 13391 | `/*` |
|        - | 13392 | ` * Section:` |
|        - | 13393 | ` *  SPL Autoloading functions.` |
|        - | 13394 | ` * Status:` |
|        - | 13395 | ` *  Stable.` |
|        - | 13396 | ` */` |
|        - | 13397 | `/*` |
|        - | 13398 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 13399 | ` *  Register given function as __autoload() implementation.` |
|        - | 13400 | ` * Parameters` |
|        - | 13401 | ` *  callback` |
|        - | 13402 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 13403 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 13404 | ` *  throw` |
|        - | 13405 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 13406 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 13407 | ` *  prepend` |
|        - | 13408 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 13409 | ` *   autoload stack instead of appending it.` |
|        - | 13410 | ` * Return` |
|        - | 13411 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13412 | ` */` |
|       34 | 13413 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13414 |  |
|        - | 13415 | `	VmAutoloadCB sEntry;` |
|       36 | 13416 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 13417 | `	int iPrepend = 0;` |
|        - | 13418 | `	sxu32 n;` |
|       36 | 13419 | `	if( nArg < 1 ){` |
|        - | 13420 | `		/* No callback provided — register default spl_autoload.` |
|        - | 13421 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 13422 | `		/* Check for duplicates first */` |
|        9 | 13423 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 13424 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 13425 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 13426 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 13427 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 13428 | `				ph7_result_bool(pCtx,1);` |
|        5 | 13429 | `				return SXRET_OK;` |
|        - | 13430 | `			}` |
|      ! 0 | 13431 | `		}` |
|        5 | 13432 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 13433 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 13434 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 13435 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 13436 | `		ph7_result_bool(pCtx,1);` |
|        5 | 13437 | `		return SXRET_OK;` |
|        - | 13438 | `	}` |
|        - | 13439 | `	/* Validate that the callback is callable */` |
|       28 | 13440 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 13441 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 13442 | `		if( nArg >= 2 ){` |
|      ! 0 | 13443 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 13444 | `		}` |
|      ! 0 | 13445 | `		if( iThrow ){` |
|      ! 0 | 13446 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 13447 | `				"Argument is not callable");` |
|      ! 0 | 13448 | `		}` |
|      ! 0 | 13449 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13450 | `		return SXRET_OK;` |
|        - | 13451 | `	}` |
|        - | 13452 | `	/* Check for duplicates */` |
|       46 | 13453 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 13454 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 13455 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13456 | `			/* Already registered */` |
|      ! 0 | 13457 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13458 | `			return SXRET_OK;` |
|        - | 13459 | `		}` |
|       11 | 13460 | `	}` |
|        - | 13461 | `	/* Check prepend flag */` |
|       28 | 13462 | `	if( nArg >= 3 ){` |
|        3 | 13463 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 13464 | `	}` |
|        - | 13465 | `	/* Store the callback */` |
|       28 | 13466 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 13467 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 13468 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 13469 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 13470 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 13471 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 13472 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 13473 | `		VmAutoloadCB *aBase;` |
|        3 | 13474 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13475 | `		/* Rotate: move last entry to front */` |
|        3 | 13476 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 13477 | `		if( aBase ){` |
|        - | 13478 | `			VmAutoloadCB sTemp;` |
|        - | 13479 | `			sxu32 i;` |
|        3 | 13480 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 13481 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 13482 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 13483 | `			}` |
|        3 | 13484 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 13485 | `		}` |
|        2 | 13486 | `	}else{` |
|       26 | 13487 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 13488 | `	}` |
|       28 | 13489 | `	ph7_result_bool(pCtx,1);` |
|       28 | 13490 | `	return SXRET_OK;` |
|       19 | 13491 |  |
|        - | 13492 | `/*` |
|        - | 13493 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 13494 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 13495 | ` * Parameters` |
|        - | 13496 | ` *  callback` |
|        - | 13497 | ` *   The autoload function being unregistered.` |
|        - | 13498 | ` * Return` |
|        - | 13499 | ` *  TRUE on success, FALSE on failure.` |
|        - | 13500 | ` */` |
|       32 | 13501 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13502 |  |
|       34 | 13503 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13504 | `	sxu32 n,nEntry;` |
|       34 | 13505 | `	if( nArg < 1 ){` |
|      ! 0 | 13506 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13507 | `		return SXRET_OK;` |
|        - | 13508 | `	}` |
|       34 | 13509 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13510 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 13511 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 13512 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 13513 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 13514 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 13515 | `			sxu32 i;` |
|       32 | 13516 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 13517 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 13518 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 13519 | `			}` |
|        - | 13520 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 13521 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 13522 | `			ph7_result_bool(pCtx,1);` |
|       32 | 13523 | `			return SXRET_OK;` |
|        - | 13524 | `		}` |
|        3 | 13525 | `	}` |
|        3 | 13526 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13527 | `	return SXRET_OK;` |
|       18 | 13528 |  |
|        - | 13529 | `/*` |
|        - | 13530 | ` * array spl_autoload_functions(void)` |
|        - | 13531 | ` *  Return all registered __autoload() functions.` |
|        - | 13532 | ` * Return` |
|        - | 13533 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 13534 | ` *  an empty array is returned.` |
|        - | 13535 | ` */` |
|       20 | 13536 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13537 |  |
|       21 | 13538 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13539 | `	ph7_value *pArray;` |
|        - | 13540 | `	sxu32 n,nEntry;` |
|       10 | 13541 | `	SXUNUSED(nArg);` |
|       10 | 13542 | `	SXUNUSED(apArg);` |
|       21 | 13543 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 13544 | `	if( pArray == 0 ){` |
|      ! 0 | 13545 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13546 | `		return SXRET_OK;` |
|        - | 13547 | `	}` |
|       21 | 13548 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 13549 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 13550 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 13551 | `		if( pEntry ){` |
|       15 | 13552 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 13553 | `		}` |
|        8 | 13554 | `	}` |
|       21 | 13555 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 13556 | `	return SXRET_OK;` |
|       11 | 13557 |  |
|        - | 13558 | `/*` |
|        - | 13559 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 13560 | ` *  Default implementation of __autoload().` |
|        - | 13561 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 13562 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 13563 | ` * Parameters` |
|        - | 13564 | ` *  class` |
|        - | 13565 | ` *   The class name being searched.` |
|        - | 13566 | ` *  file_extensions` |
|        - | 13567 | ` *   Comma-separated list of file extensions to try.` |
|        - | 13568 | ` */` |
|        2 | 13569 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13570 |  |
|        - | 13571 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 13572 | `	SyBlob sPath;` |
|        - | 13573 | `	int nClass;` |
|        - | 13574 | `	sxi32 rc;` |
|        3 | 13575 | `	if( nArg < 1 ){` |
|      ! 0 | 13576 | `		return SXRET_OK;` |
|        - | 13577 | `	}` |
|        3 | 13578 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 13579 | `	if( nClass < 1 ){` |
|      ! 0 | 13580 | `		return SXRET_OK;` |
|        - | 13581 | `	}` |
|        - | 13582 | `	/* Default extensions */` |
|        3 | 13583 | `	zExt = ".php,.inc";` |
|        3 | 13584 | `	if( nArg >= 2 ){` |
|        - | 13585 | `		int nExt;` |
|      ! 0 | 13586 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 13587 | `		if( nExt < 1 ){` |
|      ! 0 | 13588 | `			zExt = ".php,.inc";` |
|      ! 0 | 13589 | `		}` |
|      ! 0 | 13590 | `	}` |
|        3 | 13591 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 13592 | `	/* Iterate over comma-separated extensions */` |
|        3 | 13593 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 13594 | `	zCur = zExt;` |
|        7 | 13595 | `	while( zCur < zEnd ){` |
|        - | 13596 | `		const char *zComma;` |
|        - | 13597 | `		SyString sFile;` |
|        - | 13598 | `		int i;` |
|        - | 13599 | `		/* Find next comma or end */` |
|        5 | 13600 | `		zComma = zCur;` |
|       21 | 13601 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 13602 | `			zComma++;` |
|        1 | 13603 | `		}` |
|        - | 13604 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 13605 | `		SyBlobReset(&sPath);` |
|       69 | 13606 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 13607 | `			char c = zClass[i];` |
|       65 | 13608 | `			if( c == '\\' ){` |
|      ! 0 | 13609 | `				c = '/';` |
|       65 | 13610 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 13611 | `				c = c + ('a' - 'A');` |
|        6 | 13612 | `			}` |
|       65 | 13613 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 13614 | `		}` |
|        - | 13615 | `		/* Append extension */` |
|        5 | 13616 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 13617 | `		/* Try to include the file */` |
|        5 | 13618 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 13619 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 13620 | `		if( rc == SXRET_OK ){` |
|        - | 13621 | `			/* File included successfully */` |
|      ! 0 | 13622 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 13623 | `			return SXRET_OK;` |
|        - | 13624 | `		}` |
|        - | 13625 | `		/* Move past the comma */` |
|        5 | 13626 | `		zCur = zComma;` |
|        5 | 13627 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 13628 | `			zCur++;` |
|        1 | 13629 | `		}` |
|        1 | 13630 | `	}` |
|        3 | 13631 | `	SyBlobRelease(&sPath);` |
|        3 | 13632 | `	return SXRET_OK;` |
|        2 | 13633 |  |
|        - | 13634 | `/* Table of built-in VM functions. */` |
|        - | 13635 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 13636 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 13637 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 13638 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 13639 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 13640 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 13641 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 13642 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 13643 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 13644 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 13645 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 13646 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 13647 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 13648 | `	    /* Constants management */` |
|        - | 13649 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 13650 | `	{ "define",   vm_builtin_define               },` |
|        - | 13651 | `	{ "constant", vm_builtin_constant             },` |
|        - | 13652 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 13653 | `	   /* Class/Object functions */` |
|        - | 13654 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 13655 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 13656 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 13657 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 13658 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 13659 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 13660 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 13661 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 13662 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 13663 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 13664 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 13665 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 13666 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 13667 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 13668 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 13669 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 13670 | `	   /* SPL Autoloading */` |
|        - | 13671 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 13672 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 13673 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 13674 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 13675 | `	   /* Random numbers/strings generators */` |
|        - | 13676 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 13677 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 13678 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 13679 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 13680 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 13681 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13682 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 13683 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 13684 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 13685 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13686 | `	   /* Language constructs functions */` |
|        - | 13687 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 13688 | `	{ "print", vm_builtin_print                   },` |
|        - | 13689 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 13690 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 13691 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 13692 | `	  /* Variable handling functions */` |
|        - | 13693 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 13694 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 13695 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 13696 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 13697 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 13698 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 13699 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 13700 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 13701 | `	  /* Ouput control functions */` |
|        - | 13702 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 13703 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 13704 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 13705 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 13706 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 13707 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 13708 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 13709 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 13710 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 13711 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 13712 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 13713 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 13714 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 13715 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 13716 | `	  /* Assertion functions */` |
|        - | 13717 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 13718 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 13719 | `	  /* Error reporting functions */` |
|        - | 13720 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 13721 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 13722 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 13723 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 13724 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 13725 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 13726 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 13727 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 13728 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 13729 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 13730 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 13731 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 13732 | `	  /* Release info */` |
|        - | 13733 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 13734 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 13735 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 13736 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 13737 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 13738 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 13739 | `	  /* hashmap */` |
|        - | 13740 | `	{"compact",          vm_builtin_compact       },` |
|        - | 13741 | `	{"extract",          vm_builtin_extract       },` |
|        - | 13742 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 13743 | `	  /* URL related function */` |
|        - | 13744 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 13745 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 13746 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 13747 | `	   /* XML processing functions */` |
|        - | 13748 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 13749 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 13750 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 13751 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 13752 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 13753 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 13754 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 13755 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 13756 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 13757 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 13758 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 13759 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 13760 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 13761 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 13762 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 13763 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 13764 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 13765 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 13766 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 13767 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 13768 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 13769 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 13770 | `	   /* UTF-8 encoding/decoding */` |
|        - | 13771 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 13772 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 13773 | `	   /* Command line processing */` |
|        - | 13774 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 13775 | `	   /* JSON encoding/decoding */` |
|        - | 13776 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 13777 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 13778 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 13779 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 13780 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 13781 | `	   /* Files/URI inclusion facility */` |
|        - | 13782 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 13783 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 13784 | `	{ "include",      vm_builtin_include          },` |
|        - | 13785 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 13786 | `	{ "require",      vm_builtin_require          },` |
|        - | 13787 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 13788 | `};` |
|        - | 13789 | `/*` |
|        - | 13790 | ` * Register the built-in VM functions defined above.` |
|        - | 13791 | ` */` |
|     2582 | 13792 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 13793 |  |
|        - | 13794 | `	sxi32 rc;` |
|        - | 13795 | `	sxu32 n;` |
|   333080 | 13796 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 13797 | `		/* Note that these special functions have access` |
|        - | 13798 | `		 * to the underlying virtual machine as their` |
|        - | 13799 | `		 * private data.` |
|        - | 13800 | `		 */` |
|   330498 | 13801 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   330498 | 13802 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 13803 | `			return rc;` |
|        - | 13804 | `		}` |
|   165250 | 13805 | `	}` |
|     2584 | 13806 | `	return SXRET_OK;` |
|     1293 | 13807 |  |
|        - | 13808 | `/*` |
|        - | 13809 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 13810 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 13811 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 13812 | ` */` |
|    36608 | 13813 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 13814 |  |
|    36610 | 13815 | `	if( !iLoadable ){` |
|    34876 | 13816 | `		return pClass;` |
|        - | 13817 | `	}` |
|     1736 | 13818 | `	while(pClass){` |
|     1736 | 13819 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1736 | 13820 | `			return pClass;` |
|        - | 13821 | `		}` |
|      ! 0 | 13822 | `		pClass = pClass->pNextName;` |
|      ! 0 | 13823 | `	}` |
|      ! 0 | 13824 | `	return 0;` |
|    18306 | 13825 |  |
|        - | 13826 | `/*` |
|        - | 13827 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 13828 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 13829 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 13830 | ` * registered in the VM's class table.` |
|        - | 13831 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 13832 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 13833 | ` */` |
|       36 | 13834 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13835 |  |
|        - | 13836 | `	VmAutoloadCB *pEntry;` |
|        - | 13837 | `	ph7_value sArg,sResult;` |
|        - | 13838 | `	SyHashEntry *pHashEntry;` |
|        - | 13839 | `	ph7_class *pClass;` |
|        - | 13840 | `	sxu32 n,nEntry;` |
|       38 | 13841 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 13842 | `	if( nEntry < 1 ){` |
|       24 | 13843 | `		return 0;` |
|        - | 13844 | `	}` |
|        - | 13845 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 13846 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 13847 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 13848 | `	}` |
|        - | 13849 | `	/* Mark this class as being autoloaded */` |
|       14 | 13850 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 13851 | `	/* Prepare the class name argument */` |
|       14 | 13852 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 13853 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 13854 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 13855 | `	pClass = 0;` |
|       28 | 13856 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 13857 | `		ph7_value *apArg[1];` |
|       24 | 13858 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 13859 | `		if( pEntry == 0 ){` |
|      ! 0 | 13860 | `			continue;` |
|        - | 13861 | `		}` |
|       24 | 13862 | `		apArg[0] = &sArg;` |
|       24 | 13863 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 13864 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 13865 | `			continue;` |
|        - | 13866 | `		}` |
|        - | 13867 | `		/* Check if the class is now available */` |
|       24 | 13868 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 13869 | `		if( pHashEntry ){` |
|       10 | 13870 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 13871 | `			if( pClass ){` |
|       10 | 13872 | `				break;` |
|        - | 13873 | `			}` |
|      ! 0 | 13874 | `		}` |
|        9 | 13875 | `	}` |
|       14 | 13876 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 13877 | `	PH7_MemObjRelease(&sResult);` |
|        - | 13878 | `	/* Remove reentrancy guard */` |
|       14 | 13879 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 13880 | `	return pClass;` |
|       20 | 13881 |  |
|        - | 13882 | `/*` |
|        - | 13883 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 13884 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 13885 | ` */` |
|       18 | 13886 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 13887 |  |
|       20 | 13888 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 13889 |  |
|        - | 13890 | `/*` |
|        - | 13891 | ` * Check if the given name refer to an installed class.` |
|        - | 13892 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 13893 | ` */` |
|    36618 | 13894 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 13895 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 13896 | `	const char *zName,  /* Name of the target class */` |
|        - | 13897 | `	sxu32 nByte,        /* zName length */` |
|        - | 13898 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 13899 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 13900 | `						 */` |
|        - | 13901 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 13902 | `	)` |
|        2 | 13903 |  |
|        - | 13904 | `	SyHashEntry *pEntry;` |
|        - | 13905 | `	ph7_class *pClass;` |
|    18309 | 13906 | `	SXUNUSED(iNest);` |
|        - | 13907 | `	/* Exact class lookup.` |
|        - | 13908 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 13909 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    36620 | 13910 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    36620 | 13911 | `	if( pEntry == 0 ){` |
|        - | 13912 | `		/* Class not found in hash table — try autoload before giving up */` |
|       20 | 13913 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 13914 | `	}` |
|    36602 | 13915 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    36602 | 13916 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    18311 | 13917 |  |
|        - | 13918 | `/*` |
|        - | 13919 | ` * Reference Table Implementation` |
|        - | 13920 | ` * Status: stable <chm@symisc.net>` |
|        - | 13921 | ` * Intro` |
|        - | 13922 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 13923 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 13924 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 13925 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 13926 | ` *  Refer to the official for more information on this powerful` |
|        - | 13927 | ` *  extension.` |
|        - | 13928 | ` */` |
|        - | 13929 | `/*` |
|        - | 13930 | ` * Allocate a new reference entry.` |
|        - | 13931 | ` */` |
|  3109446 | 13932 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 13933 |  |
|        - | 13934 | `	VmRefObj *pRef;` |
|        - | 13935 | `	/* Allocate a new instance */` |
|  3109448 | 13936 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3109448 | 13937 | `	if( pRef == 0 ){` |
|      ! 0 | 13938 | `		return 0;` |
|        - | 13939 | `	}` |
|        - | 13940 | `	/* Zero the structure */` |
|  3109448 | 13941 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 13942 | `	/* Initialize fields */` |
|  3109448 | 13943 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3109448 | 13944 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3109448 | 13945 | `	pRef->nIdx = nIdx;` |
|  3109448 | 13946 | `	return pRef;` |
|  1554725 | 13947 |  |
|        - | 13948 | `/*` |
|        - | 13949 | ` * Default hash function used by the reference table` |
|        - | 13950 | ` * for lookup/insertion operations.` |
|        - | 13951 | ` */` |
| 17127748 | 13952 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 13953 |  |
|        - | 13954 | `	/* Calculate the hash based on the memory object index */` |
| 17127750 | 13955 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 13956 |  |
|        - | 13957 | `/*` |
|        - | 13958 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 13959 | ` * in the reference table.` |
|        - | 13960 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 13961 | ` * otherwise.` |
|        - | 13962 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13963 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13964 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13965 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13966 | ` * Refer to the official for more information on this powerful` |
|        - | 13967 | ` * extension.` |
|        - | 13968 | ` */` |
|  9275672 | 13969 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 13970 |  |
|        - | 13971 | `	VmRefObj *pRef;` |
|        - | 13972 | `	sxu32 nBucket;` |
|        - | 13973 | `	/* Point to the appropriate bucket */` |
|  9275674 | 13974 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 13975 | `	/* Perform the lookup */` |
|  9275674 | 13976 | `	pRef = pVm->apRefObj[nBucket];` |
| 20164857 | 13977 | `	for(;;){` |
| 40317823 | 13978 | `		if( pRef == 0 ){` |
|  3200964 | 13979 | `			break;` |
|        - | 13980 | `		}` |
| 37116861 | 13981 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 13982 | `			/* Entry found */` |
|  6074712 | 13983 | `			return pRef;` |
|        - | 13984 | `		}` |
|        - | 13985 | `		/* Point to the next entry */` |
| 31042151 | 13986 | `		pRef = pRef->pNextCollide;` |
|        2 | 13987 | `	}` |
|        - | 13988 | `	/* No such entry,return NULL */` |
|  3200964 | 13989 | `	return 0;` |
|  4637838 | 13990 |  |
|        - | 13991 | `/*` |
|        - | 13992 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 13993 | ` *` |
|        - | 13994 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 13995 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 13996 | ` * the reference implementation is consistent,solid and it's` |
|        - | 13997 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 13998 | ` * Refer to the official for more information on this powerful` |
|        - | 13999 | ` * extension.` |
|        - | 14000 | ` */` |
|  3109446 | 14001 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14002 |  |
|        - | 14003 | `	sxu32 nBucket;` |
|  3109448 | 14004 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14005 | `		VmRefObj **apNew;` |
|        - | 14006 | `		sxu32 nNew;` |
|        - | 14007 | `		/* Allocate a larger table */` |
|     4394 | 14008 | `		nNew = pVm->nRefSize << 1;` |
|     4394 | 14009 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4394 | 14010 | `		if( apNew ){` |
|     4394 | 14011 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14012 | `			sxu32 n;` |
|        - | 14013 | `			/* Zero the structure */` |
|     4394 | 14014 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14015 | `			/* Rehash all referenced entries */` |
|  2844902 | 14016 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14017 | `				/* Remove old collision links */` |
|  2840510 | 14018 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14019 | `				/* Point to the appropriate bucket */` |
|  2840510 | 14020 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14021 | `				/* Insert the entry  */` |
|  2840510 | 14022 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2840510 | 14023 | `				if( apNew[nBucket] ){` |
|  2298896 | 14024 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 14025 | `				}` |
|  2840510 | 14026 | `				apNew[nBucket] = pEntry;` |
|        - | 14027 | `				/* Point to the next entry */` |
|  2840510 | 14028 | `				pEntry = pEntry->pNext;` |
|  1420256 | 14029 | `			}` |
|        - | 14030 | `			/* Release the old table */` |
|     4394 | 14031 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 14032 | `			/* Install the new one */` |
|     4394 | 14033 | `			pVm->apRefObj = apNew;` |
|     4394 | 14034 | `			pVm->nRefSize = nNew;` |
|     2196 | 14035 | `		}` |
|     2196 | 14036 | `	}` |
|        - | 14037 | `	/* Point to the appropriate bucket */` |
|  3109448 | 14038 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 14039 | `	/* Insert the entry */` |
|  3109448 | 14040 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3109448 | 14041 | `	if( pVm->apRefObj[nBucket] ){` |
|  2554496 | 14042 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1277274 | 14043 | `	}` |
|  3109448 | 14044 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3109448 | 14045 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3109448 | 14046 | `	pVm->nRefUsed++;` |
|  3109448 | 14047 | `	return SXRET_OK;` |
|        2 | 14048 |  |
|        - | 14049 | `/*` |
|        - | 14050 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 14051 | ` * the reference table.` |
|        - | 14052 | ` * This function is invoked when the user perform an unset` |
|        - | 14053 | ` * call [i.e: unset($var); ].` |
|        - | 14054 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14055 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14056 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14057 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14058 | ` * Refer to the official for more information on this powerful` |
|        - | 14059 | ` * extension.` |
|        - | 14060 | ` */` |
|  3072040 | 14061 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14062 |  |
|        - | 14063 | `	ph7_hashmap_node **apNode;` |
|        - | 14064 | `	SyHashEntry **apEntry;` |
|        - | 14065 | `	sxu32 n;` |
|        - | 14066 | `	/* Point to the reference table */` |
|  3072042 | 14067 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3072042 | 14068 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 14069 | `	/* Unlink the entry from the reference table */` |
|  3170010 | 14070 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    97970 | 14071 | `		if( apEntry[n] ){` |
|    97920 | 14072 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    48959 | 14073 | `		}` |
|    48986 | 14074 | `	}` |
|  6048578 | 14075 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2976538 | 14076 | `		if( apNode[n] ){` |
|     7380 | 14077 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3689 | 14078 | `		}` |
|  1488270 | 14079 | `	}` |
|  3072042 | 14080 | `	if( pRef->pPrevCollide ){` |
|  1169920 | 14081 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   585365 | 14082 | `	}else{` |
|  1902124 | 14083 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 14084 | `	}` |
|  3072042 | 14085 | `	if( pRef->pNextCollide ){` |
|  1741872 | 14086 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   870937 | 14087 | `	}` |
|  3072042 | 14088 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 14089 | `	/* Release the node */` |
|  3072042 | 14090 | `	SySetRelease(&pRef->aReference);` |
|  3072042 | 14091 | `	SySetRelease(&pRef->aArrEntries);` |
|  3072042 | 14092 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3072042 | 14093 | `	pVm->nRefUsed--;` |
|  3072042 | 14094 | `	return SXRET_OK;` |
|        2 | 14095 |  |
|        - | 14096 | `/*` |
|        - | 14097 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14098 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14099 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14100 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14101 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14102 | ` * Refer to the official for more information on this powerful` |
|        - | 14103 | ` * extension.` |
|        - | 14104 | ` */` |
|  3142892 | 14105 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 14106 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14107 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14108 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14109 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 14110 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 14111 | `	)` |
|        2 | 14112 |  |
|  3142894 | 14113 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 14114 | `	VmRefObj *pRef;` |
|        - | 14115 | `	/* Check if the referenced object already exists */` |
|  3142894 | 14116 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3142894 | 14117 | `	if( pRef == 0 ){` |
|        - | 14118 | `		/* Create a new entry */` |
|  3109448 | 14119 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3109448 | 14120 | `		if( pRef == 0 ){` |
|      ! 0 | 14121 | `			return SXERR_MEM;` |
|        - | 14122 | `		}` |
|  3109448 | 14123 | `		pRef->iFlags = iFlags;` |
|        - | 14124 | `		/* Install the entry */` |
|  3109448 | 14125 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1554723 | 14126 | `	}` |
|  3142894 | 14127 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3142894 | 14128 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 14129 | `		VmSlot sRef;` |
|        - | 14130 | `		/* Local frame,record referenced entry so that it can` |
|        - | 14131 | `		 * be deleted when we leave this frame.` |
|        - | 14132 | `		 */` |
|    91614 | 14133 | `		sRef.nIdx = nIdx;` |
|    91614 | 14134 | `		sRef.pUserData = pEntry;` |
|    91614 | 14135 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 14136 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 14137 | `		}` |
|    45806 | 14138 | `	}` |
|  3142894 | 14139 | `	if( pEntry ){` |
|        - | 14140 | `		/* Address of the hash-entry */` |
|   124860 | 14141 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    62429 | 14142 | `	}` |
|  3142894 | 14143 | `	if( pMapEntry ){` |
|        - | 14144 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3011776 | 14145 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1505887 | 14146 | `	}` |
|  3142894 | 14147 | `	return SXRET_OK;` |
|  1571448 | 14148 |  |
|        - | 14149 | `/*` |
|        - | 14150 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 14151 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14152 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14153 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14154 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14155 | ` * Refer to the official for more information on this powerful` |
|        - | 14156 | ` * extension.` |
|        - | 14157 | ` */` |
|  3060734 | 14158 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 14159 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 14160 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 14161 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 14162 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 14163 | `	)` |
|        2 | 14164 |  |
|        - | 14165 | `	VmRefObj *pRef;` |
|        - | 14166 | `	sxu32 n;` |
|        - | 14167 | `	/* Check if the referenced object already exists */` |
|  3060736 | 14168 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3060736 | 14169 | `	if( pRef == 0 ){` |
|        - | 14170 | `		/* Not such entry */` |
|    91512 | 14171 | `		return SXERR_NOTFOUND;` |
|        - | 14172 | `	}` |
|        - | 14173 | `	/* Remove the desired entry */` |
|  2969226 | 14174 | `	if( pEntry ){` |
|        - | 14175 | `		SyHashEntry **apEntry;` |
|       62 | 14176 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 14177 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 14178 | `			if( apEntry[n] == pEntry ){` |
|        - | 14179 | `				/* Nullify the entry */` |
|       62 | 14180 | `				apEntry[n] = 0;` |
|        - | 14181 | `				/*` |
|        - | 14182 | `				 * NOTE:` |
|        - | 14183 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 14184 | `				 * we avoid wasting spaces.` |
|        - | 14185 | `				 */` |
|       30 | 14186 | `			}` |
|       85 | 14187 | `		}` |
|       30 | 14188 | `	}` |
|  2969226 | 14189 | `	if( pMapEntry ){` |
|        - | 14190 | `		ph7_hashmap_node **apNode;` |
|  2969166 | 14191 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5938424 | 14192 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2969260 | 14193 | `			if( apNode[n] == pMapEntry ){` |
|        - | 14194 | `				/* nullify the entry */` |
|  2969166 | 14195 | `				apNode[n] = 0;` |
|  1484582 | 14196 | `			}` |
|  1484631 | 14197 | `		}` |
|  1484582 | 14198 | `	}` |
|  2969226 | 14199 | `	return SXRET_OK;` |
|  1530369 | 14200 |  |
|        - | 14201 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 14202 | `/*` |
|        - | 14203 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 14204 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 14205 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 14206 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 14207 | ` * For more information on how to register IO stream devices,please` |
|        - | 14208 | ` * refer to the official documentation.` |
|        - | 14209 | ` */` |
|    26690 | 14210 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 14211 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 14212 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 14213 | `	int nByte              /* *pzDevice length*/` |
|        - | 14214 | `	)` |
|        2 | 14215 |  |
|        - | 14216 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 14217 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 14218 | `	SyString sDev,sCur;` |
|        - | 14219 | `	sxu32 n,nEntry;` |
|        - | 14220 | `	int rc;` |
|        - | 14221 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    26692 | 14222 | `	zNext = zCur = zIn = *pzDevice;` |
|    26692 | 14223 | `	zEnd = &zIn[nByte];` |
|  1694092 | 14224 | `	while( zIn < zEnd ){` |
|  1667404 | 14225 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 14226 | `			/* Got one */` |
|        3 | 14227 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 14228 | `			break;` |
|        - | 14229 | `		}` |
|        - | 14230 | `		/* Advance the cursor */` |
|  1667402 | 14231 | `		zIn++;` |
|        2 | 14232 | `	}` |
|    26692 | 14233 | `	if( zIn >= zEnd ){` |
|        - | 14234 | `		/* No such scheme,return the default stream */` |
|    26690 | 14235 | `		return pVm->pDefStream;` |
|        - | 14236 | `	}` |
|        3 | 14237 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 14238 | `	/* Remove leading and trailing white spaces */` |
|        3 | 14239 | `	SyStringFullTrim(&sDev);` |
|        - | 14240 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 14241 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 14242 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 14243 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 14244 | `		pStream = apStream[n];` |
|        3 | 14245 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 14246 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 14247 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 14248 | `		if( rc == 0 ){` |
|        - | 14249 | `			/* Stream device found */` |
|        3 | 14250 | `			*pzDevice = zNext;` |
|        3 | 14251 | `			return pStream;` |
|        - | 14252 | `		}` |
|      ! 0 | 14253 | `	}` |
|        - | 14254 | `	/* No such stream,return NULL */` |
|      ! 0 | 14255 | `	return 0;` |
|    13347 | 14256 |  |
|        - | 14257 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 14258 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 14259 |  |
