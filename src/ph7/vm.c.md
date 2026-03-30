# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4112/5373 lines (76.53%)

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
|   764154 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   764156 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   764126 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   764118 |    94 | `	return FALSE;` |
|   382101 |    95 |  |
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
|   453926 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   453928 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   453928 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   453924 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   453924 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   453924 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   453924 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   453924 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   453924 |   142 | `	pCons->xExpand = xExpand;` |
|   453924 |   143 | `	pCons->pUserData = pUserData;` |
|   453924 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   453924 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   453924 |   151 | `	return SXRET_OK;` |
|   226965 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   972660 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   972662 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   972662 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   972662 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   972662 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   972662 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   972662 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   972662 |   185 | `	pFunc->pVm   = pVm;` |
|   972662 |   186 | `	pFunc->xFunc = xFunc;` |
|   972662 |   187 | `	pFunc->pUserData = pUserData;` |
|   972662 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   972662 |   190 | `	*ppOut = pFunc;` |
|   972662 |   191 | `	return SXRET_OK;` |
|   486332 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   974896 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   974898 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   974898 |   213 | `	if( pEntry ){` |
|     2238 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2238 |   215 | `		pFunc->pUserData = pUserData;` |
|     2238 |   216 | `		pFunc->xFunc = xFunc;` |
|     2238 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2238 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   972662 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   972662 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   972662 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   972662 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   972662 |   233 | `	return SXRET_OK;` |
|   487450 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   105300 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   105302 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   105302 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   105302 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   105302 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   105302 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   105302 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   105302 |   260 | `	pFunc->iFlags = iFlags;` |
|   105302 |   261 | `	pFunc->pUserData = pUserData;` |
|   105302 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   105302 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   382638 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   382640 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    32814 |   283 | `		pName = &pFunc->sName;` |
|    16406 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   382640 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   382640 |   287 | `	if( pEntry ){` |
|   297470 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   297470 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   297470 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|    85172 |   297 | `	pFunc->pNextName = 0;` |
|    85172 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    85172 |   299 | `	return rc;` |
|   191321 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    30254 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    30256 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    30256 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    30256 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    30226 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    30226 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    30226 |   324 | `	return rc;` |
|    15129 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  2797084 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2797086 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  2797086 |   342 | `	sInstr.iP1 = iP1;` |
|  2797086 |   343 | `	sInstr.iP2 = iP2;` |
|  2797086 |   344 | `	sInstr.p3  = p3;` |
|  2797086 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   177660 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    88829 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  2797086 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2797086 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  2797086 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   255912 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   255914 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   255914 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   255914 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   127956 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   127958 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   175096 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   175098 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   175098 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|   781362 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|   781364 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   166242 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   166244 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   548184 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   548186 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    25476 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    25478 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    25478 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    25478 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    25478 |   417 | `	return &aInstr[n - 2];` |
|    12740 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    14874 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    14876 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    14876 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    14876 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    14876 |   437 | `	pFrame->pUserData = pUserData;` |
|    14876 |   438 | `	pFrame->pThis = pThis;` |
|    14876 |   439 | `	pFrame->pVm = pVm;` |
|    14876 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    14876 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    14876 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    14876 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    14876 |   444 | `	return pFrame;` |
|     7439 |   445 |  |
|        - |   446 | `/*` |
|        - |   447 | ` * Enter a VM frame.` |
|        - |   448 | ` */` |
|    14874 |   449 | `static sxi32 VmEnterFrame(` |
|        - |   450 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   451 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   452 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   453 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   454 | `	)` |
|        2 |   455 |  |
|        - |   456 | `	VmFrame *pFrame;` |
|        - |   457 | `	/* Allocate a new frame */` |
|    14876 |   458 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    14876 |   459 | `	if( pFrame == 0 ){` |
|      ! 0 |   460 | `		return SXERR_MEM;` |
|        - |   461 | `	}` |
|        - |   462 | `	/* Link to the list of active VM frame */` |
|    14876 |   463 | `	pFrame->pParent = pVm->pFrame;` |
|    14876 |   464 | `	pVm->pFrame = pFrame;` |
|    14876 |   465 | `	if( ppFrame ){` |
|        - |   466 | `		/* Write a pointer to the new VM frame */` |
|    12390 |   467 | `		*ppFrame = pFrame;` |
|     6194 |   468 | `	}` |
|    14876 |   469 | `	return SXRET_OK;` |
|     7439 |   470 |  |
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
|    12388 |   517 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   518 |  |
|    12390 |   519 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12390 |   520 | `	if( pCurFrame ){` |
|        - |   521 | `		/* Unlink from the list of active VM frame */` |
|    12390 |   522 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12390 |   523 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   524 | `			VmSlot  *aSlot;` |
|        - |   525 | `			sxu32 n;` |
|        - |   526 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12342 |   527 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    87714 |   528 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   529 | `				/* Unset the local variable */` |
|    75374 |   530 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    37688 |   531 | `			}` |
|        - |   532 | `			/* Remove local reference */` |
|    12342 |   533 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    87770 |   534 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    75430 |   535 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    37716 |   536 | `			}` |
|     6170 |   537 | `		}` |
|        - |   538 | `		/* Release internal containers */` |
|    12390 |   539 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12390 |   540 | `		SySetRelease(&pCurFrame->sArg);` |
|    12390 |   541 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12390 |   542 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   543 | `		/* Release the whole structure */` |
|    12390 |   544 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6194 |   545 | `	}` |
|    12390 |   546 |  |
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
|    88900 |   663 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   664 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   665 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   666 | `	)` |
|        2 |   667 |  |
|        - |   668 | `	ph7_class_method *pMeth;` |
|        - |   669 | `	ph7_class_attr *pAttr;` |
|        - |   670 | `	SyHashEntry *pEntry;` |
|        - |   671 | `	sxi32 rc;` |
|        - |   672 | `	/* Reset the loop cursor */` |
|    88902 |   673 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   674 | `	/* Process only static and constant attribute */` |
|   352072 |   675 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   676 | `		/* Extract the current attribute */` |
|   218722 |   677 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   218722 |   678 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    88902 |   700 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   701 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   702 | `		 */` |
|    46092 |   703 | `		return SXRET_OK;` |
|        - |   704 | `	}` |
|        - |   705 | `	/* Create constructor alias if not yet done */` |
|    42812 |   706 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   707 | `		/* User constructor with the same base class name */` |
|      278 |   708 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      278 |   709 | `		if( pEntry ){` |
|      ! 0 |   710 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   711 | `			/* Create the alias */` |
|      ! 0 |   712 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   713 | `		}` |
|      138 |   714 | `	}` |
|        - |   715 | `	/* Install the methods now */` |
|    42812 |   716 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   414049 |   717 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   349834 |   718 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   349834 |   719 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   349828 |   720 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   349828 |   721 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   722 | `				return rc;` |
|        - |   723 | `			}` |
|   174913 |   724 | `		}` |
|        2 |   725 | `	}` |
|        - |   726 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    42812 |   727 | `	pClass->bMounted = TRUE;` |
|    42812 |   728 | `	return SXRET_OK;` |
|    44452 |   729 |  |
|        - |   730 | `/*` |
|        - |   731 | ` * Allocate a private frame for attributes of the given` |
|        - |   732 | ` * class instance (Object in the PHP jargon).` |
|        - |   733 | ` */` |
|     1120 |   734 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   735 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   736 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   737 | `	)` |
|        2 |   738 |  |
|     1122 |   739 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   740 | `	ph7_class_attr *pAttr;` |
|        - |   741 | `	SyHashEntry *pEntry;` |
|        - |   742 | `	sxi32 rc;` |
|        - |   743 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1122 |   744 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4688 |   745 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   746 | `		VmClassAttr *pVmAttr;` |
|        - |   747 | `		/* Extract the current attribute */` |
|     3568 |   748 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3568 |   749 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3568 |   750 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   751 | `			return SXERR_MEM;` |
|        - |   752 | `		}` |
|     3568 |   753 | `		pVmAttr->pAttr = pAttr;` |
|     3568 |   754 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   755 | `			ph7_value *pMemObj;` |
|        - |   756 | `			/* Reserve a memory object for this attribute */` |
|     3562 |   757 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3562 |   758 | `			if( pMemObj == 0 ){` |
|      ! 0 |   759 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   760 | `				return SXERR_MEM;` |
|        - |   761 | `			}` |
|     3562 |   762 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3562 |   763 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   764 | `				/* Initialize attribute default value (any complex expression) */` |
|     1168 |   765 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      583 |   766 | `			}` |
|     3562 |   767 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3562 |   768 | `			if( rc != SXRET_OK ){` |
|        - |   769 | `				VmSlot sSlot;` |
|        - |   770 | `				/* Restore memory object */` |
|      ! 0 |   771 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   772 | `				sSlot.pUserData = 0;` |
|      ! 0 |   773 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   774 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   775 | `				return SXERR_MEM;` |
|        - |   776 | `			}` |
|        - |   777 | `			/* Install attribute in the reference table */` |
|     3562 |   778 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1782 |   779 | `		}else{` |
|        - |   780 | `			/* Install static/constant attribute */` |
|        8 |   781 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   782 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   783 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   784 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   785 | `				return SXERR_MEM;` |
|        - |   786 | `			}` |
|        - |   787 | `		}` |
|        2 |   788 | `	}` |
|     1122 |   789 | `	return SXRET_OK;` |
|      562 |   790 |  |
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
|   303748 |   802 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   803 |  |
|        - |   804 | `	ph7_value *pObj;` |
|        - |   805 | `	sxi32 rc;` |
|   303750 |   806 | `	if( pIndex ){` |
|        - |   807 | `		/* Object index in the object table */` |
|   296292 |   808 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   148145 |   809 | `	}` |
|        - |   810 | `	/* Reserve a slot for the new object */` |
|   303750 |   811 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   303750 |   812 | `	if( rc != SXRET_OK ){` |
|        - |   813 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   814 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   815 | `		 */` |
|      ! 0 |   816 | `		return 0;` |
|        - |   817 | `	}` |
|   303750 |   818 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   303750 |   819 | `	return pObj;` |
|   151876 |   820 |  |
|        - |   821 | `/*` |
|        - |   822 | ` * Reserve a memory object.` |
|        - |   823 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   824 | ` */` |
|  2139950 |   825 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   826 |  |
|        - |   827 | `	ph7_value *pObj;` |
|        - |   828 | `	sxi32 rc;` |
|  2139952 |   829 | `	if( pIndex ){` |
|        - |   830 | `		/* Object index in the object table */` |
|  2139952 |   831 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1069975 |   832 | `	}` |
|        - |   833 | `	/* Reserve a slot for the new object */` |
|  2139952 |   834 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2139952 |   835 | `	if( rc != SXRET_OK ){` |
|        - |   836 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   837 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   838 | `		 */` |
|      ! 0 |   839 | `		return 0;` |
|        - |   840 | `	}` |
|  2139952 |   841 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2139952 |   842 | `	return pObj;` |
|  1069977 |   843 |  |
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
|     2486 |  1196 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1197 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1198 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1199 | `	 )` |
|        2 |  1200 |  |
|        - |  1201 | `	SyString sBuiltin;` |
|        - |  1202 | `	ph7_value *pObj;` |
|        - |  1203 | `	sxi32 rc;` |
|        - |  1204 | `	/* Zero the structure */` |
|     2488 |  1205 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1206 | `	/* Initialize VM fields */` |
|     2488 |  1207 | `	pVm->pEngine = &(*pEngine);` |
|     2488 |  1208 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1209 | `	/* Instructions containers */` |
|     2488 |  1210 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2488 |  1211 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2488 |  1212 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1213 | `	/* Object containers */` |
|     2488 |  1214 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2488 |  1215 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1216 | `	/* Virtual machine internal containers */` |
|     2488 |  1217 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2488 |  1218 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2488 |  1219 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2488 |  1220 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2488 |  1221 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2488 |  1222 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2488 |  1223 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2488 |  1224 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2488 |  1225 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2488 |  1226 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2488 |  1227 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2488 |  1228 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2488 |  1229 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2488 |  1230 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2488 |  1231 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2488 |  1232 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2488 |  1233 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2488 |  1234 | `	pVm->pPendingException = 0;` |
|        - |  1235 | `	/* Configuration containers */` |
|     2488 |  1236 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2488 |  1237 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2488 |  1238 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2488 |  1239 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2488 |  1240 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1241 | `	/* Error callbacks containers */` |
|     2488 |  1242 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2488 |  1243 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2488 |  1244 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2488 |  1245 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2488 |  1246 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1247 | `	/* Set a default recursion limit */` |
|        - |  1248 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2488 |  1249 | `	pVm->nMaxDepth = 32;` |
|        - |  1250 | `#else` |
|        - |  1251 | `	pVm->nMaxDepth = 16;` |
|        - |  1252 | `#endif` |
|        - |  1253 | `	/* Default assertion flags */` |
|     2488 |  1254 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1255 | `	/* JSON return status */` |
|     2488 |  1256 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1257 | `	/* PRNG context */` |
|     2488 |  1258 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1259 | `	/* Install the null constant */` |
|     2488 |  1260 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2488 |  1261 | `	if( pObj == 0 ){` |
|      ! 0 |  1262 | `		rc = SXERR_MEM;` |
|      ! 0 |  1263 | `		goto Err;` |
|        - |  1264 | `	}` |
|     2488 |  1265 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1266 | `	/* Install the boolean TRUE constant */` |
|     2488 |  1267 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2488 |  1268 | `	if( pObj == 0 ){` |
|      ! 0 |  1269 | `		rc = SXERR_MEM;` |
|      ! 0 |  1270 | `		goto Err;` |
|        - |  1271 | `	}` |
|     2488 |  1272 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1273 | `	/* Install the boolean FALSE constant */` |
|     2488 |  1274 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2488 |  1275 | `	if( pObj == 0 ){` |
|      ! 0 |  1276 | `		rc = SXERR_MEM;` |
|      ! 0 |  1277 | `		goto Err;` |
|        - |  1278 | `	}` |
|     2488 |  1279 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1280 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1281 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1282 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2488 |  1283 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2488 |  1284 | `	if( pObj == 0 ){` |
|      ! 0 |  1285 | `		rc = SXERR_MEM;` |
|      ! 0 |  1286 | `		goto Err;` |
|        - |  1287 | `	}` |
|     2488 |  1288 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1289 | `	/* Create the global frame */` |
|     2488 |  1290 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2488 |  1291 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1292 | `		goto Err;` |
|        - |  1293 | `	}` |
|        - |  1294 | `	/* Initialize the code generator */` |
|     2488 |  1295 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2488 |  1296 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1297 | `		goto Err;` |
|        - |  1298 | `	}` |
|        - |  1299 | `	/* VM correctly initialized,set the magic number */` |
|     2488 |  1300 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2488 |  1301 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1302 | `	/* Compile the built-in library */` |
|     2488 |  1303 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1304 | `	/* Reset the code generator */` |
|     2488 |  1305 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2488 |  1306 | `	return SXRET_OK;` |
|      ! 0 |  1307 | `Err:` |
|      ! 0 |  1308 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1309 | `	return rc;` |
|     1245 |  1310 |  |
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
|    30744 |  1340 | `static ph7_value * VmNewOperandStack(` |
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
|    30746 |  1353 | `	nInstr += VM_STACK_GUARD;` |
|    30746 |  1354 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    30746 |  1355 | `	if( pStack == 0 ){` |
|      ! 0 |  1356 | `		return 0;` |
|        - |  1357 | `	}` |
|        - |  1358 | `	/* Initialize the operand stack */` |
|  1945568 |  1359 | `	while( nInstr > 0 ){` |
|  1914824 |  1360 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1914824 |  1361 | `		--nInstr;` |
|        2 |  1362 | `	}` |
|        - |  1363 | `	/* Ready for bytecode execution */` |
|    30746 |  1364 | `	return pStack;` |
|    15374 |  1365 |  |
|        - |  1366 | `/* Forward declaration */` |
|        - |  1367 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1368 | `/*` |
|        - |  1369 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1370 | ` * This routine gets called by the PH7 engine after` |
|        - |  1371 | ` * successful compilation of the target PHP program.` |
|        - |  1372 | ` */` |
|     2236 |  1373 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1374 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1375 | `	)` |
|        2 |  1376 |  |
|        - |  1377 | `	SyHashEntry *pEntry;` |
|        - |  1378 | `	sxi32 rc;` |
|     2238 |  1379 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1380 | `		/* Initialize your VM first */` |
|      ! 0 |  1381 | `		return SXERR_CORRUPT;` |
|        - |  1382 | `	}` |
|        - |  1383 | `	/* Mark the VM ready for byte-code execution */` |
|     2238 |  1384 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1385 | `	/* Release the code generator now we have compiled our program */` |
|     2238 |  1386 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1387 | `	/* Emit the DONE instruction */` |
|     2238 |  1388 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2238 |  1389 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1390 | `		return SXERR_MEM;` |
|        - |  1391 | `	}` |
|        - |  1392 | `	/* Script return value */` |
|     2238 |  1393 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1394 | `	/* Allocate a new operand stack */` |
|     2238 |  1395 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2238 |  1396 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1397 | `		return SXERR_MEM;` |
|        - |  1398 | `	}` |
|        - |  1399 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1400 | `	 * private data. */` |
|     2238 |  1401 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2238 |  1402 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1403 | `	/* Allocate the reference table */` |
|     2238 |  1404 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2238 |  1405 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2238 |  1406 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1407 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1408 | `		return SXERR_MEM;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Zero the reference table */` |
|     2238 |  1411 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1412 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2238 |  1413 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2238 |  1414 | `	if( rc != SXRET_OK ){` |
|        - |  1415 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1416 | `		return rc;` |
|        - |  1417 | `	}` |
|        - |  1418 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2238 |  1419 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2238 |  1420 | `	if( rc != SXRET_OK ){` |
|        - |  1421 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1422 | `		return rc;` |
|        - |  1423 | `	}` |
|        - |  1424 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2238 |  1425 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1426 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2238 |  1427 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1428 | `	/* Initialize and install static and constants class attributes */` |
|     2238 |  1429 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    29216 |  1430 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    26980 |  1431 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    26980 |  1432 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1433 | `			return rc;` |
|        - |  1434 | `		}` |
|        2 |  1435 | `	}` |
|        - |  1436 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2238 |  1437 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1438 | `	/* VM is ready for bytecode execution */` |
|     2238 |  1439 | `	return SXRET_OK;` |
|     1120 |  1440 |  |
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
|     2228 |  1460 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1461 |  |
|        - |  1462 | `	/* Set the stale magic number */` |
|     2230 |  1463 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1464 | `	/* Release the private memory subsystem */` |
|     2230 |  1465 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2230 |  1466 | `	return SXRET_OK;` |
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
|   545398 |  1478 | `static sxi32 VmInitCallContext(` |
|        - |  1479 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1480 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1481 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1482 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1483 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1484 | `	)` |
|        2 |  1485 |  |
|   545400 |  1486 | `	pOut->pFunc = pFunc;` |
|   545400 |  1487 | `	pOut->pVm   = pVm;` |
|   545400 |  1488 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   545400 |  1489 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1490 | `	/* Assume a null return value */` |
|   545400 |  1491 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   545400 |  1492 | `	pOut->pRet = pRet;` |
|   545400 |  1493 | `	pOut->iFlags = iFlags;` |
|   545400 |  1494 | `	return SXRET_OK;` |
|        2 |  1495 |  |
|        - |  1496 | `/*` |
|        - |  1497 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1498 | ` * left behind.` |
|        - |  1499 | ` */` |
|   545398 |  1500 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1501 |  |
|        - |  1502 | `	sxu32 n;` |
|   545400 |  1503 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6664 |  1504 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19006 |  1505 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12344 |  1506 | `			if( apObj[n] == 0 ){` |
|        - |  1507 | `				/* Already released */` |
|      250 |  1508 | `				continue;` |
|        - |  1509 | `			}` |
|    12096 |  1510 | `			PH7_MemObjRelease(apObj[n]);` |
|    12096 |  1511 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6049 |  1512 | `		}` |
|     6664 |  1513 | `		SySetRelease(&pCtx->sVar);` |
|     3331 |  1514 | `	}` |
|   545400 |  1515 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
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
|   545400 |  1531 |  |
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
|  3220112 |  1562 | `static void VmPopOperand(` |
|        - |  1563 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1564 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1565 | `	)` |
|        2 |  1566 |  |
|  3220114 |  1567 | `	ph7_value *pTos = *ppTos;` |
|  6839232 |  1568 | `	while( nPop > 0 ){` |
|  3619120 |  1569 | `		PH7_MemObjRelease(pTos);` |
|  3619120 |  1570 | `		pTos--;` |
|  3619120 |  1571 | `		nPop--;` |
|        2 |  1572 | `	}` |
|        - |  1573 | `	/* Top of the stack */` |
|  3220114 |  1574 | `	*ppTos = pTos;` |
|  3220114 |  1575 |  |
|        - |  1576 | `/*` |
|        - |  1577 | ` * Reserve a memory object.` |
|        - |  1578 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1579 | ` */` |
|  2995646 |  1580 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1581 |  |
|  2995648 |  1582 | `	ph7_value *pObj = 0;` |
|        - |  1583 | `	VmSlot *pSlot;` |
|        - |  1584 | `	sxu32 nIdx;` |
|        - |  1585 | `	/* Check for a free slot */` |
|  2995648 |  1586 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2995648 |  1587 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2995648 |  1588 | `	if( pSlot ){` |
|   855698 |  1589 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   855698 |  1590 | `		nIdx = pSlot->nIdx;` |
|   427848 |  1591 | `	}` |
|  2995648 |  1592 | `	if( pObj == 0 ){` |
|        - |  1593 | `		/* Reserve a new memory object */` |
|  2139952 |  1594 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2139952 |  1595 | `		if( pObj == 0 ){` |
|      ! 0 |  1596 | `			return 0;` |
|        - |  1597 | `		}` |
|  1069975 |  1598 | `	}` |
|        - |  1599 | `	/* Set a null default value */` |
|  2995648 |  1600 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2995648 |  1601 | `	pObj->nIdx = nIdx;` |
|  2995648 |  1602 | `	return pObj;` |
|  1497825 |  1603 |  |
|        - |  1604 | `/*` |
|        - |  1605 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1606 | ` */` |
|    28062 |  1607 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1608 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1609 | `	const char *zKey,  /* Entry key */` |
|        - |  1610 | `	sxu32 nByte,       /* Key length */` |
|        - |  1611 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1612 | `	)` |
|        2 |  1613 |  |
|        - |  1614 | `	ph7_value sKey;` |
|        - |  1615 | `	sxi32 rc;` |
|    28064 |  1616 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    28064 |  1617 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1618 | `	/* Perform the insertion */` |
|    28064 |  1619 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    28064 |  1620 | `	PH7_MemObjRelease(&sKey);` |
|    28064 |  1621 | `	return rc;` |
|        2 |  1622 |  |
|        - |  1623 | `/*` |
|        - |  1624 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1625 | ` * Return a pointer to the variable value on success.` |
|        - |  1626 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1627 | ` */` |
|  3018754 |  1628 | `static ph7_value * VmExtractMemObj(` |
|        - |  1629 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1630 | `	const SyString *pName, /* Variable name */` |
|        - |  1631 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1632 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1633 | `	)` |
|        2 |  1634 |  |
|  3018756 |  1635 | `	int bNullify = FALSE;` |
|        - |  1636 | `	SyHashEntry *pEntry;` |
|        - |  1637 | `	VmFrame *pFrame;` |
|        - |  1638 | `	ph7_value *pObj;` |
|        - |  1639 | `	sxu32 nIdx;` |
|        - |  1640 | `	sxi32 rc;` |
|        - |  1641 | `	/* Point to the top active frame */` |
|  3018756 |  1642 | `	pFrame = pVm->pFrame;` |
|  3018774 |  1643 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1644 | `		/* Safely ignore the exception frame */` |
|       20 |  1645 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        2 |  1646 | `	}` |
|        - |  1647 | `	/* Perform the lookup */` |
|  3018756 |  1648 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1649 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1650 | `		pName = &sAnnon;` |
|        - |  1651 | `		/* Always nullify the object */` |
|      ! 0 |  1652 | `		bNullify = TRUE;` |
|      ! 0 |  1653 | `		bDup = FALSE;` |
|      ! 0 |  1654 | `	}` |
|        - |  1655 | `	/* Check the superglobals table first */` |
|  3018756 |  1656 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3018756 |  1657 | `	if( pEntry == 0 ){` |
|        - |  1658 | `		/* Query the top active frame */` |
|  3018720 |  1659 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3018720 |  1660 | `		if( pEntry == 0 ){` |
|    81682 |  1661 | `			char *zName = (char *)pName->zString;` |
|        - |  1662 | `			VmSlot sLocal;` |
|    81682 |  1663 | `			if( !bCreate ){` |
|        - |  1664 | `				/* Do not create the variable,return NULL instead */` |
|      632 |  1665 | `				return 0;` |
|        - |  1666 | `			}` |
|        - |  1667 | `			/* No such variable,automatically create a new one and install` |
|        - |  1668 | `			 * it in the current frame.` |
|        - |  1669 | `			 */` |
|    81052 |  1670 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    81052 |  1671 | `			if( pObj == 0 ){` |
|      ! 0 |  1672 | `				return 0;` |
|        - |  1673 | `			}` |
|    81052 |  1674 | `			nIdx = pObj->nIdx;` |
|    81052 |  1675 | `			if( bDup ){` |
|        - |  1676 | `				/* Duplicate name */` |
|      164 |  1677 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1678 | `				if( zName == 0 ){` |
|      ! 0 |  1679 | `					return 0;` |
|        - |  1680 | `				}` |
|       81 |  1681 | `			}` |
|        - |  1682 | `			/* Link to the top active VM frame */` |
|    81052 |  1683 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    81052 |  1684 | `			if( rc != SXRET_OK ){` |
|        - |  1685 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1686 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1687 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1688 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1689 | `				return 0;` |
|        - |  1690 | `			}` |
|    81052 |  1691 | `			if( pFrame->pParent != 0 ){` |
|        - |  1692 | `				/* Local variable */` |
|    75374 |  1693 | `				sLocal.nIdx = nIdx;` |
|    75374 |  1694 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    37688 |  1695 | `			}else{` |
|        - |  1696 | `				/* Register in the $GLOBALS array */` |
|     5680 |  1697 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1698 | `			}` |
|        - |  1699 | `			/* Install in the reference table */` |
|    81052 |  1700 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1701 | `			/* Save object index */` |
|    81052 |  1702 | `			pObj->nIdx = nIdx;` |
|    40527 |  1703 | `		}else{` |
|        - |  1704 | `			/* Extract variable contents */` |
|  2937040 |  1705 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2937040 |  1706 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2937040 |  1707 | `			if( bNullify && pObj ){` |
|      ! 0 |  1708 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1709 | `			}` |
|        - |  1710 | `		}` |
|  1509156 |  1711 | `	}else{` |
|        - |  1712 | `		/* Superglobal */` |
|       38 |  1713 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1714 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1715 | `	}` |
|  3018126 |  1716 | `	return pObj;` |
|  1509489 |  1717 |  |
|        - |  1718 | `/*` |
|        - |  1719 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1720 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1721 | ` */` |
|     2262 |  1722 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1723 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1724 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1725 | `	sxu32 nByte        /* zName length */` |
|        - |  1726 | `	)` |
|        2 |  1727 |  |
|        - |  1728 | `	SyHashEntry *pEntry;` |
|        - |  1729 | `	ph7_value *pValue;` |
|        - |  1730 | `	sxu32 nIdx;` |
|        - |  1731 | `	/* Query the superglobal table */` |
|     2264 |  1732 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2264 |  1733 | `	if( pEntry == 0 ){` |
|        - |  1734 | `		/* No such entry */` |
|      ! 0 |  1735 | `		return 0;` |
|        - |  1736 | `	}` |
|        - |  1737 | `	/* Extract the superglobal index in the global object pool */` |
|     2264 |  1738 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1739 | `	/* Extract the variable value  */` |
|     2264 |  1740 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2264 |  1741 | `	return pValue;` |
|     1133 |  1742 |  |
|        - |  1743 | `/*` |
|        - |  1744 | ` * Perform a raw hashmap insertion.` |
|        - |  1745 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1746 | ` */` |
|     2260 |  1747 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1748 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1749 | `	const char *zKey,   /* Entry key */` |
|        - |  1750 | `	int nKeylen,        /* zKey length*/` |
|        - |  1751 | `	const char *zData,  /* Entry data */` |
|        - |  1752 | `	int nLen            /* zData length */` |
|        - |  1753 | `	)` |
|        2 |  1754 |  |
|        - |  1755 | `	ph7_value sKey,sValue;` |
|        - |  1756 | `	sxi32 rc;` |
|     2262 |  1757 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2262 |  1758 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2262 |  1759 | `	if( zKey ){` |
|     2240 |  1760 | `		if( nKeylen < 0 ){` |
|     2240 |  1761 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1119 |  1762 | `		}` |
|     2240 |  1763 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1119 |  1764 | `	}` |
|     2262 |  1765 | `	if( zData ){` |
|     2262 |  1766 | `		if( nLen < 0 ){` |
|        - |  1767 | `			/* Compute length automatically */` |
|      ! 0 |  1768 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1769 | `		}` |
|     2262 |  1770 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1130 |  1771 | `	}` |
|        - |  1772 | `	/* Perform the insertion */` |
|     2262 |  1773 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2262 |  1774 | `	PH7_MemObjRelease(&sKey);` |
|     2262 |  1775 | `	PH7_MemObjRelease(&sValue);` |
|     2262 |  1776 | `	return rc;` |
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
|    35800 |  1791 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1792 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1793 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1794 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1795 | `	)` |
|        2 |  1796 |  |
|    35802 |  1797 | `	sxi32 rc = SXRET_OK;` |
|    35802 |  1798 | `	switch(nOp){` |
|     1118 |  1799 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2238 |  1800 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2238 |  1801 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1802 | `		/* VM output consumer callback */` |
|        - |  1803 | `#ifdef UNTRUST` |
|        - |  1804 | `		if( xConsumer == 0 ){` |
|        - |  1805 | `			rc = SXERR_CORRUPT;` |
|        - |  1806 | `			break;` |
|        - |  1807 | `		}` |
|        - |  1808 | `#endif` |
|        - |  1809 | `		/* Install the output consumer */` |
|     2238 |  1810 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2238 |  1811 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2238 |  1812 | `		break;` |
|        - |  1813 | `							   }` |
|     1118 |  1814 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1815 | `		/* Import path */` |
|        - |  1816 | `		  const char *zPath;` |
|        - |  1817 | `		  SyString sPath;` |
|     2238 |  1818 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1819 | `#if defined(UNTRUST)` |
|        - |  1820 | `		  if( zPath == 0 ){` |
|        - |  1821 | `			  rc = SXERR_EMPTY;` |
|        - |  1822 | `			  break;` |
|        - |  1823 | `		  }` |
|        - |  1824 | `#endif` |
|     2238 |  1825 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1826 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1827 | `#ifdef __WINNT__` |
|        2 |  1828 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1829 | `#endif` |
|     4474 |  1830 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1831 | `		  /* Remove leading and trailing white spaces */` |
|     2238 |  1832 | `		  SyStringFullTrim(&sPath);` |
|     2238 |  1833 | `		  if( sPath.nByte > 0 ){` |
|        - |  1834 | `			  /* Store the path in the corresponding conatiner */` |
|     2238 |  1835 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1118 |  1836 | `		  }` |
|     2238 |  1837 | `		  break;` |
|        - |  1838 | `									 }` |
|     1118 |  1839 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1840 | `		/* Run-Time Error report */` |
|     2238 |  1841 | `		pVm->bErrReport = 1;` |
|     2238 |  1842 | `		break;` |
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
|    11180 |  1864 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1865 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1866 | `		/* Create a new superglobal/global variable */` |
|    22362 |  1867 | `		const char *zName = va_arg(ap,const char *);` |
|    22362 |  1868 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
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
|    22362 |  1879 | `		nByte = SyStrlen(zName);` |
|    22362 |  1880 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1881 | `			/* Check if the superglobal is already installed */` |
|    22362 |  1882 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11182 |  1883 | `		}else{` |
|        - |  1884 | `			/* Query the top active VM frame */` |
|      ! 0 |  1885 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1886 | `		}` |
|    22362 |  1887 | `		if( pEntry ){` |
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
|    22362 |  1898 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    22362 |  1899 | `			if( pObj == 0 ){` |
|      ! 0 |  1900 | `				rc = SXERR_MEM;` |
|      ! 0 |  1901 | `				break;` |
|        - |  1902 | `			}` |
|    22362 |  1903 | `			nIdx = pObj->nIdx;` |
|        - |  1904 | `			/* Copy value */` |
|    22362 |  1905 | `			PH7_MemObjStore(pValue,pObj);` |
|    22362 |  1906 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1907 | `				/* Install the superglobal */` |
|    22362 |  1908 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11182 |  1909 | `			}else{` |
|        - |  1910 | `				/* Install in the current frame */` |
|      ! 0 |  1911 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1912 | `			}` |
|    22362 |  1913 | `			if( rc == SXRET_OK ){` |
|        - |  1914 | `				SyHashEntry *pRef;` |
|    22362 |  1915 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    22362 |  1916 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11182 |  1917 | `				}else{` |
|      ! 0 |  1918 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1919 | `				}` |
|        - |  1920 | `				/* Install in the reference table */` |
|    22362 |  1921 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    22362 |  1922 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1923 | `					/* Register in the $GLOBALS array */` |
|    22362 |  1924 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11180 |  1925 | `				}` |
|    11180 |  1926 | `			}` |
|        - |  1927 | `		}` |
|    22362 |  1928 | `		break;` |
|        - |  1929 | `									}` |
|     1119 |  1930 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1931 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1932 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1933 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1934 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1935 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1936 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2240 |  1937 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2240 |  1938 | `		const char *zValue = va_arg(ap,const char *);` |
|     2240 |  1939 | `		int nLen = va_arg(ap,int);` |
|        - |  1940 | `		ph7_hashmap *pMap;` |
|        - |  1941 | `		ph7_value *pValue;` |
|     2240 |  1942 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1943 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1944 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2239 |  1945 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1946 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1947 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2238 |  1948 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1949 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1950 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2238 |  1951 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1952 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1953 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2238 |  1954 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1955 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1956 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2238 |  1957 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1958 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1959 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1960 | `		}else{` |
|        - |  1961 | `			/* Extract the $_SERVER superglobal */` |
|     2238 |  1962 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1963 | `		}` |
|     2240 |  1964 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1965 | `			/* No such entry */` |
|      ! 0 |  1966 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1967 | `			break;` |
|        - |  1968 | `		}` |
|        - |  1969 | `		/* Point to the hashmap */` |
|     2240 |  1970 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1971 | `		/* Perform the insertion */` |
|     2240 |  1972 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2240 |  1973 | `		break;` |
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
|     2236 |  2024 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2025 | `		/* Register an IO stream device */` |
|     4474 |  2026 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2027 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6708 |  2028 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4474 |  2029 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2030 | `				/* Invalid stream */` |
|      ! 0 |  2031 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2032 | `				break;` |
|        - |  2033 | `		}` |
|     4474 |  2034 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2035 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2238 |  2036 | `			pVm->pDefStream = pStream;` |
|     1118 |  2037 | `		}` |
|        - |  2038 | `		/* Insert in the appropriate container */` |
|     4474 |  2039 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4474 |  2040 | `		break;` |
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
|    35802 |  2077 | `	return rc;` |
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
|    30744 |  2553 | `static sxi32 VmByteCodeExec(` |
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
|    30746 |  2569 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    30746 |  2570 | `	if( nTos < 0 ){` |
|    29040 |  2571 | `		pTos = &pStack[-1];` |
|    14521 |  2572 | `	}else{` |
|     1708 |  2573 | `		pTos = &pStack[nTos];` |
|        - |  2574 | `	}` |
|    30746 |  2575 | `	pc = 0;` |
|        - |  2576 | `	/* Execute as much as we can */` |
|  4823911 |  2577 | `	for(;;){` |
|        - |  2578 | `		/* Fetch the instruction to execute */` |
|  9647120 |  2579 | `		pInstr = &aInstr[pc];` |
|  9647120 |  2580 | `		rc = SXRET_OK;` |
|        - |  2581 | `/*` |
|        - |  2582 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2583 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2584 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2585 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2586 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2587 | ` */` |
|  9647120 |  2588 | `		switch(pInstr->iOp){` |
|        - |  2589 | `/*` |
|        - |  2590 | ` * DONE: P1 * *` |
|        - |  2591 | ` *` |
|        - |  2592 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2593 | ` * and return immediately.` |
|        - |  2594 | ` */` |
|    15129 |  2595 | `case PH7_OP_DONE:` |
|    30260 |  2596 | `	if( pInstr->iP1 ){` |
|        - |  2597 | `#ifdef UNTRUST` |
|        - |  2598 | `		if( pTos < pStack ){` |
|        - |  2599 | `			goto Abort;` |
|        - |  2600 | `		}` |
|        - |  2601 | `#endif` |
|    17492 |  2602 | `		if( pLastRef ){` |
|    11370 |  2603 | `			*pLastRef = pTos->nIdx;` |
|     5684 |  2604 | `		}` |
|    17492 |  2605 | `		if( pResult ){` |
|        - |  2606 | `			/* Execution result */` |
|    16654 |  2607 | `			PH7_MemObjStore(pTos,pResult);` |
|     8326 |  2608 | `		}` |
|    17492 |  2609 | `		VmPopOperand(&pTos,1);` |
|    21515 |  2610 | `	}else if( pLastRef ){` |
|        - |  2611 | `		/* Nothing referenced */` |
|      940 |  2612 | `		*pLastRef = SXU32_HIGH;` |
|      469 |  2613 | `	}` |
|    30260 |  2614 | `	goto Done;` |
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
|   208352 |  2662 | `case PH7_OP_JMP:` |
|   416750 |  2663 | `	pc = pInstr->iP2 - 1;` |
|   416750 |  2664 | `	break;` |
|        - |  2665 | `/*` |
|        - |  2666 | ` * JZ: P1 P2 *` |
|        - |  2667 | ` *` |
|        - |  2668 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2669 | ` * entry in the stack if P1 is zero.` |
|        - |  2670 | ` */` |
|   485812 |  2671 | `case PH7_OP_JZ:` |
|        - |  2672 | `#ifdef UNTRUST` |
|        - |  2673 | `	if( pTos < pStack ){` |
|        - |  2674 | `		goto Abort;` |
|        - |  2675 | `	}` |
|        - |  2676 | `#endif` |
|        - |  2677 | `	/* Get a boolean value */` |
|   971714 |  2678 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2679 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2680 | `	}` |
|   971714 |  2681 | `	if( !pTos->x.iVal ){` |
|        - |  2682 | `		/* Take the jump */` |
|   488534 |  2683 | `		pc = pInstr->iP2 - 1;` |
|   244266 |  2684 | `	}` |
|   971714 |  2685 | `	if( !pInstr->iP1 ){` |
|   775260 |  2686 | `		VmPopOperand(&pTos,1);` |
|   387651 |  2687 | `	}` |
|   971714 |  2688 | `	break;` |
|        - |  2689 | `/*` |
|        - |  2690 | ` * JNZ: P1 P2 *` |
|        - |  2691 | ` *` |
|        - |  2692 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2693 | ` * entry in the stack if P1 is zero.` |
|        - |  2694 | ` */` |
|    52855 |  2695 | `case PH7_OP_JNZ:` |
|        - |  2696 | `#ifdef UNTRUST` |
|        - |  2697 | `	if( pTos < pStack ){` |
|        - |  2698 | `		goto Abort;` |
|        - |  2699 | `	}` |
|        - |  2700 | `#endif` |
|        - |  2701 | `	/* Get a boolean value */` |
|   105712 |  2702 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2703 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2704 | `	}` |
|   105712 |  2705 | `	if( pTos->x.iVal ){` |
|        - |  2706 | `		/* Take the jump */` |
|     4284 |  2707 | `		pc = pInstr->iP2 - 1;` |
|     2141 |  2708 | `	}` |
|   105712 |  2709 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2710 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2711 | `	}` |
|   105712 |  2712 | `	break;` |
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
|   378617 |  2726 | `case PH7_OP_POP: {` |
|   757280 |  2727 | `	sxi32 n = pInstr->iP1;` |
|   757280 |  2728 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2729 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2730 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2731 | `	}` |
|   757280 |  2732 | `	VmPopOperand(&pTos,n);` |
|   757280 |  2733 | `	break;` |
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
|     6159 |  2756 | `case PH7_OP_NSSWITCH:` |
|    12320 |  2757 | `	SyBlobReset(&pVm->sNamespace);` |
|    12320 |  2758 | `	if( pInstr->p3 ){` |
|       49 |  2759 | `		const char *zNs = (const char *)pInstr->p3;` |
|       49 |  2760 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       24 |  2761 | `	}` |
|    12320 |  2762 | `	break;` |
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
|    12283 |  2894 | `case PH7_OP_ERR_CTRL:` |
|        - |  2895 | `	/*` |
|        - |  2896 | `	 * TICKET 1433-038:` |
|        - |  2897 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2898 | `	 * use the public API,to control error output.` |
|        - |  2899 | `	 */` |
|    24566 |  2900 | `	break;` |
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
|   798421 |  2960 | `case PH7_OP_LOADC: {` |
|        - |  2961 | `	ph7_value *pObj;` |
|        - |  2962 | `	/* Reserve a room */` |
|  1596888 |  2963 | `	pTos++;` |
|  2387478 |  2964 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1596888 |  2965 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2966 | `			SyHashEntry *pEntry;` |
|        - |  2967 | `			/* Candidate for expansion via user defined callbacks */` |
|    15754 |  2968 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15754 |  2969 | `			if( pEntry ){` |
|    15750 |  2970 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2971 | `				/* Set a NULL default value */` |
|    15750 |  2972 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15750 |  2973 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2974 | `				/* Invoke the callback and deal with the expanded value */` |
|    15750 |  2975 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2976 | `				/* Mark as constant */` |
|    15750 |  2977 | `				pTos->nIdx = SXU32_HIGH;` |
|    15750 |  2978 | `				break;` |
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
|  1581138 |  3010 | `		PH7_MemObjLoad(pObj,pTos);` |
|   790592 |  3011 | `	}else{` |
|        - |  3012 | `		/* Set a NULL value */` |
|      ! 0 |  3013 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3014 | `	}` |
|   790547 |  3015 | `LoadC_Done:` |
|        - |  3016 | `	/* Mark as constant */` |
|  1581140 |  3017 | `	pTos->nIdx = SXU32_HIGH;` |
|  1581140 |  3018 | `	break;` |
|        - |  3019 | `				  }` |
|        - |  3020 | `/*` |
|        - |  3021 | ` * LOAD: P1 * P3` |
|        - |  3022 | ` *` |
|        - |  3023 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3024 | ` * from the P3 operand.` |
|        - |  3025 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3026 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3027 | ` */` |
|  1316719 |  3028 | `case PH7_OP_LOAD:{` |
|        - |  3029 | `	ph7_value *pObj;` |
|        - |  3030 | `	SyString sName;` |
|  2633660 |  3031 | `	if( pInstr->p3 == 0 ){` |
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
|  2633642 |  3044 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3045 | `		/* Reserve a room for the target object */` |
|  2633642 |  3046 | `		pTos++;` |
|        - |  3047 | `	}` |
|        - |  3048 | `	/* Extract the requested memory object */` |
|  2633660 |  3049 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2633660 |  3050 | `	if( pObj == 0 ){` |
|      624 |  3051 | `		if( pInstr->iP1 ){` |
|        - |  3052 | `			/* Variable not found,load NULL */` |
|      624 |  3053 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3054 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3055 | `			}else{` |
|      624 |  3056 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3057 | `			}` |
|      624 |  3058 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1317032 |  3059 | `			break;` |
|      ! 0 |  3060 | `		}else{` |
|        - |  3061 | `			/* Fatal error */` |
|      ! 0 |  3062 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3063 | `			goto Abort;` |
|        - |  3064 | `		}` |
|        - |  3065 | `	}` |
|        - |  3066 | `	/* Load variable contents */` |
|  2633038 |  3067 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2633038 |  3068 | `	pTos->nIdx = pObj->nIdx;` |
|  2633038 |  3069 | `	break;` |
|        - |  3070 | `				   }` |
|        - |  3071 | `/*` |
|        - |  3072 | ` * LOAD_MAP P1 * *` |
|        - |  3073 | ` *` |
|        - |  3074 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3075 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3076 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3077 | ` */` |
|    17755 |  3078 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3079 | `	ph7_hashmap *pMap;` |
|        - |  3080 | `	/* Allocate a new hashmap instance */` |
|    35512 |  3081 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    35512 |  3082 | `	if( pMap == 0 ){` |
|      ! 0 |  3083 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3084 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3085 | `		goto Abort;` |
|        - |  3086 | `	}` |
|    35512 |  3087 | `	if( pInstr->iP1 > 0 ){` |
|     2166 |  3088 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3089 | `		/* Perform the insertion */` |
|     6592 |  3090 | `		while( pEntry < pTos ){` |
|     4428 |  3091 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3092 | `				/* Insertion by reference */` |
|      142 |  3093 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3094 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3095 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3096 | `					);` |
|       48 |  3097 | `			}else{` |
|        - |  3098 | `				/* Standard insertion */` |
|     6500 |  3099 | `				PH7_HashmapInsert(pMap,` |
|     4332 |  3100 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2166 |  3101 | `					&pEntry[1]` |
|        - |  3102 | `				);` |
|        - |  3103 | `			}` |
|        - |  3104 | `			/* Next pair on the stack */` |
|     4428 |  3105 | `			pEntry += 2;` |
|        2 |  3106 | `		}` |
|        - |  3107 | `		/* Pop P1 elements */` |
|     2166 |  3108 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1082 |  3109 | `	}` |
|        - |  3110 | `	/* Push the hashmap */` |
|    35512 |  3111 | `	pTos++;` |
|    35512 |  3112 | `	pTos->nIdx = SXU32_HIGH;` |
|    35512 |  3113 | `	pTos->x.pOther = pMap;` |
|    35512 |  3114 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    35512 |  3115 | `	break;` |
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
|   213030 |  3171 | `case PH7_OP_LOAD_IDX: {` |
|   426106 |  3172 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   426106 |  3173 | `	ph7_hashmap *pMap = 0;` |
|        - |  3174 | `	ph7_value *pIdx;` |
|   426106 |  3175 | `	pIdx = 0;` |
|   426106 |  3176 | `	if( pInstr->iP1 == 0 ){` |
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
|   426106 |  3193 | `		pIdx = pTos;` |
|   426106 |  3194 | `		pTos--;` |
|        - |  3195 | `	}` |
|   426106 |  3196 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3197 | `		/* String access */` |
|   338058 |  3198 | `		if( pIdx ){` |
|        - |  3199 | `			sxu32 nOfft;` |
|   338058 |  3200 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3201 | `				/* Force an int cast */` |
|      ! 0 |  3202 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3203 | `			}` |
|   338058 |  3204 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   338058 |  3205 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3206 | `				/* Invalid offset,load null */` |
|      ! 0 |  3207 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3208 | `			}else{` |
|   338058 |  3209 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   338058 |  3210 | `				int c = zData[nOfft];` |
|   338058 |  3211 | `				PH7_MemObjRelease(pTos);` |
|   338058 |  3212 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   338058 |  3213 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3214 | `			}` |
|   169052 |  3215 | `		}else{` |
|        - |  3216 | `			/* No available index,load NULL */` |
|      ! 0 |  3217 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3218 | `		}` |
|   338058 |  3219 | `		break;` |
|        - |  3220 | `	}` |
|    88050 |  3221 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3222 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3223 | `			ph7_value *pObj;` |
|      ! 0 |  3224 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3225 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3226 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3227 | `			}` |
|      ! 0 |  3228 | `		}` |
|      ! 0 |  3229 | `	}` |
|    88050 |  3230 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    88050 |  3231 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3232 | `		/* Point to the hashmap */` |
|    88050 |  3233 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    88050 |  3234 | `		if( pIdx ){` |
|        - |  3235 | `			/* Load the desired entry */` |
|    88050 |  3236 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    44024 |  3237 | `		}` |
|    88050 |  3238 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3239 | `			/* Create a new empty entry */` |
|      ! 0 |  3240 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3241 | `			if( rc == SXRET_OK ){` |
|        - |  3242 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3243 | `				pNode = pMap->pLast;` |
|      ! 0 |  3244 | `			}` |
|      ! 0 |  3245 | `		}` |
|    44024 |  3246 | `	}` |
|    88050 |  3247 | `	if( pIdx ){` |
|    88050 |  3248 | `		PH7_MemObjRelease(pIdx);` |
|    44024 |  3249 | `	}` |
|    88050 |  3250 | `	if( rc == SXRET_OK ){` |
|        - |  3251 | `		/* Load entry contents */` |
|    40142 |  3252 | `		if( pMap->iRef < 2 ){` |
|        - |  3253 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3254 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3255 | `			 */` |
|       24 |  3256 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3257 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3258 | `		}else{` |
|    40120 |  3259 | `			pTos->nIdx = pNode->nValIdx;` |
|    40120 |  3260 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    40120 |  3261 | `			PH7_HashmapUnref(pMap);` |
|        - |  3262 | `		}` |
|    20072 |  3263 | `	}else{` |
|        - |  3264 | `		/* No such entry,load NULL */` |
|    47910 |  3265 | `		PH7_MemObjRelease(pTos);` |
|    47910 |  3266 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3267 | `	}` |
|    88050 |  3268 | `	break;` |
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
|   109430 |  3344 | `case PH7_OP_STORE: {` |
|        - |  3345 | `	ph7_value *pObj;` |
|        - |  3346 | `	SyString sName;` |
|        - |  3347 | `#ifdef UNTRUST` |
|        - |  3348 | `	if( pTos < pStack ){` |
|        - |  3349 | `		goto Abort;` |
|        - |  3350 | `	}` |
|        - |  3351 | `#endif` |
|   218862 |  3352 | `	if( pInstr->iP2 ){` |
|        - |  3353 | `		sxu32 nIdx;` |
|        - |  3354 | `		/* Member store operation */` |
|     2906 |  3355 | `		nIdx = pTos->nIdx;` |
|     2906 |  3356 | `		VmPopOperand(&pTos,1);` |
|     2906 |  3357 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3358 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3359 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3360 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3361 | `		}else{` |
|        - |  3362 | `			/* Point to the desired memory object */` |
|     2902 |  3363 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2902 |  3364 | `			if( pObj ){` |
|        - |  3365 | `				/* Perform the store operation */` |
|     2902 |  3366 | `				PH7_MemObjStore(pTos,pObj);` |
|     1450 |  3367 | `			}` |
|        - |  3368 | `		}` |
|   110884 |  3369 | `		break;` |
|   215958 |  3370 | `	}else if( pInstr->p3 == 0 ){` |
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
|   215952 |  3384 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3385 | `	}` |
|        - |  3386 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   215958 |  3387 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   215958 |  3388 | `	if( pObj == 0 ){` |
|      ! 0 |  3389 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3390 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3391 | `		goto Abort;` |
|        - |  3392 | `	}` |
|   215958 |  3393 | `	if( !pInstr->p3 ){` |
|        7 |  3394 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3395 | `	}` |
|        - |  3396 | `	/* Perform the store operation */` |
|   215958 |  3397 | `	PH7_MemObjStore(pTos,pObj);` |
|   215958 |  3398 | `	break;` |
|        - |  3399 | `				   }` |
|        - |  3400 | `/*` |
|        - |  3401 | ` * STORE_IDX:   P1 * P3` |
|        - |  3402 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3403 | ` *` |
|        - |  3404 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3405 | ` */` |
|    79562 |  3406 | `case PH7_OP_STORE_IDX:` |
|        - |  3407 | `case PH7_OP_STORE_IDX_REF: {` |
|   159126 |  3408 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3409 | `	ph7_value *pKey;` |
|        - |  3410 | `	sxu32 nIdx;` |
|   159126 |  3411 | `	if( pInstr->iP1 ){` |
|        - |  3412 | `		/* Key is next on stack */` |
|    56612 |  3413 | `		pKey = pTos;` |
|    56612 |  3414 | `		pTos--;` |
|    28307 |  3415 | `	}else{` |
|   102516 |  3416 | `		pKey = 0;` |
|        - |  3417 | `	}` |
|   159126 |  3418 | `	nIdx = pTos->nIdx;` |
|   159126 |  3419 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3420 | `		/* Hashmap already loaded */` |
|   159074 |  3421 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   159074 |  3422 | `		if( pMap->iRef < 2 ){` |
|        - |  3423 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3424 | `			pMap->iRef = 2;` |
|      ! 0 |  3425 | `		}` |
|    79538 |  3426 | `	}else{` |
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
|   159074 |  3480 | `	VmPopOperand(&pTos,1);` |
|        - |  3481 | `	/* Phase#2: Perform the insertion */` |
|   159074 |  3482 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3483 | `		/* Insertion by reference */` |
|       15 |  3484 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3485 | `	}else{` |
|   159060 |  3486 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3487 | `	}` |
|   159074 |  3488 | `	if( pKey ){` |
|    56562 |  3489 | `		PH7_MemObjRelease(pKey);` |
|    28280 |  3490 | `	}` |
|   159074 |  3491 | `	break;` |
|        - |  3492 | `					   }` |
|        - |  3493 | `/*` |
|        - |  3494 | ` * INCR: P1 * *` |
|        - |  3495 | ` *` |
|        - |  3496 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3497 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3498 | ` * the stack and increment after that.` |
|        - |  3499 | ` */` |
|   150258 |  3500 | `case PH7_OP_INCR:` |
|        - |  3501 | `#ifdef UNTRUST` |
|        - |  3502 | `	if( pTos < pStack ){` |
|        - |  3503 | `		goto Abort;` |
|        - |  3504 | `	}` |
|        - |  3505 | `#endif` |
|   300562 |  3506 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   300562 |  3507 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3508 | `			ph7_value *pObj;` |
|   300562 |  3509 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3510 | `				/* Force a numeric cast */` |
|   300562 |  3511 | `				PH7_MemObjToNumeric(pObj);` |
|   300562 |  3512 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3513 | `					pObj->rVal++;` |
|        - |  3514 | `					/* Try to get an integer representation */` |
|      ! 0 |  3515 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3516 | `				}else{` |
|   300562 |  3517 | `					pObj->x.iVal++;` |
|   300562 |  3518 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3519 | `				}` |
|   300562 |  3520 | `				if( pInstr->iP1 ){` |
|        - |  3521 | `					/* Pre-icrement */` |
|       71 |  3522 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3523 | `				}` |
|   150302 |  3524 | `			}` |
|   150304 |  3525 | `		}else{` |
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
|   150302 |  3540 | `	}` |
|   300562 |  3541 | `	break;` |
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
|    22951 |  3596 | `case PH7_OP_UMINUS:` |
|        - |  3597 | `#ifdef UNTRUST` |
|        - |  3598 | `	if( pTos < pStack ){` |
|        - |  3599 | `		goto Abort;` |
|        - |  3600 | `	}` |
|        - |  3601 | `#endif` |
|        - |  3602 | `	/* Force a numeric (integer,real or both) cast */` |
|    45904 |  3603 | `	PH7_MemObjToNumeric(pTos);` |
|    45904 |  3604 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3605 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3606 | `	}` |
|    45904 |  3607 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    45874 |  3608 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22936 |  3609 | `	}` |
|    45904 |  3610 | `	break;` |
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
|    39322 |  3637 | `case PH7_OP_LNOT:` |
|        - |  3638 | `#ifdef UNTRUST` |
|        - |  3639 | `	if( pTos < pStack ){` |
|        - |  3640 | `		goto Abort;` |
|        - |  3641 | `	}` |
|        - |  3642 | `#endif` |
|        - |  3643 | `	/* Force a boolean cast */` |
|    78690 |  3644 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3645 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3646 | `	}` |
|    78690 |  3647 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    78690 |  3648 | `	break;` |
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
|    61164 |  4251 | `case PH7_OP_CAT:{` |
|        - |  4252 | `	ph7_value *pNos,*pCur;` |
|   122330 |  4253 | `	if( pInstr->iP1 < 1 ){` |
|    95398 |  4254 | `		pNos = &pTos[-1];` |
|    47700 |  4255 | `	}else{` |
|    26934 |  4256 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4257 | `	}` |
|        - |  4258 | `#ifdef UNTRUST` |
|        - |  4259 | `	if( pNos < pStack ){` |
|        - |  4260 | `		goto Abort;` |
|        - |  4261 | `	}` |
|        - |  4262 | `#endif` |
|        - |  4263 | `	/* Force a string cast */` |
|   122330 |  4264 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1008 |  4265 | `		PH7_MemObjToString(pNos);` |
|      503 |  4266 | `	}` |
|   122330 |  4267 | `	pCur = &pNos[1];` |
|   246578 |  4268 | `	while( pCur <= pTos ){` |
|   124250 |  4269 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50500 |  4270 | `			PH7_MemObjToString(pCur);` |
|    25249 |  4271 | `		}` |
|        - |  4272 | `		/* Perform the concatenation */` |
|   124250 |  4273 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   124212 |  4274 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    62105 |  4275 | `		}` |
|   124250 |  4276 | `		SyBlobRelease(&pCur->sBlob);` |
|   124250 |  4277 | `		pCur++;` |
|        2 |  4278 | `	}` |
|   122330 |  4279 | `	pTos = pNos;` |
|   122330 |  4280 | `	break;` |
|        - |  4281 | `				}` |
|        - |  4282 | `/*  CAT_STORE: * * *` |
|        - |  4283 | ` *` |
|        - |  4284 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4285 | ` * back.` |
|        - |  4286 | ` */` |
|     3291 |  4287 | `case PH7_OP_CAT_STORE:{` |
|     6584 |  4288 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4289 | `	ph7_value *pObj;` |
|        - |  4290 | `#ifdef UNTRUST` |
|        - |  4291 | `	if( pNos < pStack ){` |
|        - |  4292 | `		goto Abort;` |
|        - |  4293 | `	}` |
|        - |  4294 | `#endif` |
|     6584 |  4295 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4296 | `		/* Force a string cast */` |
|      ! 0 |  4297 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4298 | `	}` |
|     6584 |  4299 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4300 | `		/* Force a string cast */` |
|      ! 0 |  4301 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4302 | `	}` |
|        - |  4303 | `	/* Perform the concatenation (Reverse order) */` |
|     6584 |  4304 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6584 |  4305 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3291 |  4306 | `	}` |
|        - |  4307 | `	/* Perform the store operation */` |
|     6584 |  4308 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4309 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6584 |  4310 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6584 |  4311 | `		PH7_MemObjStore(pTos,pObj);` |
|     3291 |  4312 | `	}` |
|     6584 |  4313 | `	PH7_MemObjStore(pTos,pNos);` |
|     6584 |  4314 | `	VmPopOperand(&pTos,1);` |
|     6584 |  4315 | `	break;` |
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
|    93311 |  4329 | `case PH7_OP_LAND:` |
|        - |  4330 | `case PH7_OP_LOR: {` |
|   186668 |  4331 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4332 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4333 | `#ifdef UNTRUST` |
|        - |  4334 | `	if( pNos < pStack ){` |
|        - |  4335 | `		goto Abort;` |
|        - |  4336 | `	}` |
|        - |  4337 | `#endif` |
|        - |  4338 | `	/* Force a boolean cast */` |
|   186668 |  4339 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4340 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4341 | `	}` |
|   186668 |  4342 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4343 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4344 | `	}` |
|   186668 |  4345 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   186668 |  4346 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   186668 |  4347 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4348 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    85240 |  4349 | `		v1 = and_logic[v1*3+v2];` |
|    42643 |  4350 | `	}else{` |
|        - |  4351 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   101430 |  4352 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4353 | `	}` |
|   186668 |  4354 | `	if( v1 == 2 ){` |
|      ! 0 |  4355 | `		v1 = 1;` |
|      ! 0 |  4356 | `	}` |
|   186668 |  4357 | `	VmPopOperand(&pTos,1);` |
|   186668 |  4358 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   186668 |  4359 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   186668 |  4360 | `	break;` |
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
|     3798 |  4408 | `case PH7_OP_EQ:` |
|        - |  4409 | `case PH7_OP_NEQ: {` |
|     7598 |  4410 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4411 | `	/* Perform the comparison and act accordingly */` |
|        - |  4412 | `#ifdef UNTRUST` |
|        - |  4413 | `	if( pNos < pStack ){` |
|        - |  4414 | `		goto Abort;` |
|        - |  4415 | `	}` |
|        - |  4416 | `#endif` |
|     7598 |  4417 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7598 |  4418 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4419 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7589 |  4420 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7554 |  4421 | `		rc = rc == 0;` |
|     3778 |  4422 | `	}else{` |
|       28 |  4423 | `		rc = rc != 0;` |
|        - |  4424 | `	}` |
|     7598 |  4425 | `	VmPopOperand(&pTos,1);` |
|     7598 |  4426 | `	if( !pInstr->iP2 ){` |
|        - |  4427 | `		/* Push comparison result without taking the jump */` |
|     7598 |  4428 | `		PH7_MemObjRelease(pTos);` |
|     7598 |  4429 | `		pTos->x.iVal = rc;` |
|        - |  4430 | `		/* Invalidate any prior representation */` |
|     7598 |  4431 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3800 |  4432 | `	}else{` |
|      ! 0 |  4433 | `		if( rc ){` |
|        - |  4434 | `			/* Jump to the desired location */` |
|      ! 0 |  4435 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4436 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4437 | `		}` |
|        - |  4438 | `	}` |
|     7598 |  4439 | `	break;` |
|        - |  4440 | `				 }` |
|        - |  4441 | `/* OP_TEQ P1 P2 *` |
|        - |  4442 | ` *` |
|        - |  4443 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4444 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4445 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4446 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4447 | ` */` |
|   127989 |  4448 | `case PH7_OP_TEQ: {` |
|   255980 |  4449 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4450 | `	/* Perform the comparison and act accordingly */` |
|        - |  4451 | `#ifdef UNTRUST` |
|        - |  4452 | `	if( pNos < pStack ){` |
|        - |  4453 | `		goto Abort;` |
|        - |  4454 | `	}` |
|        - |  4455 | `#endif` |
|   255980 |  4456 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   255980 |  4457 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4458 | `		rc = 0;` |
|        2 |  4459 | `	}else{` |
|   255978 |  4460 | `		rc = rc == 0;` |
|        - |  4461 | `	}` |
|   255980 |  4462 | `	VmPopOperand(&pTos,1);` |
|   255980 |  4463 | `	if( !pInstr->iP2 ){` |
|        - |  4464 | `		/* Push comparison result without taking the jump */` |
|   255980 |  4465 | `		PH7_MemObjRelease(pTos);` |
|   255980 |  4466 | `		pTos->x.iVal = rc;` |
|        - |  4467 | `		/* Invalidate any prior representation */` |
|   255980 |  4468 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   127991 |  4469 | `	}else{` |
|      ! 0 |  4470 | `		if( rc ){` |
|        - |  4471 | `			/* Jump to the desired location */` |
|      ! 0 |  4472 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4473 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4474 | `		}` |
|        - |  4475 | `	}` |
|   255980 |  4476 | `	break;` |
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
|   100005 |  4487 | `case PH7_OP_TNE: {` |
|   200012 |  4488 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4489 | `	/* Perform the comparison and act accordingly */` |
|        - |  4490 | `#ifdef UNTRUST` |
|        - |  4491 | `	if( pNos < pStack ){` |
|        - |  4492 | `		goto Abort;` |
|        - |  4493 | `	}` |
|        - |  4494 | `#endif` |
|   200012 |  4495 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   200012 |  4496 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4497 | `		rc = 1;` |
|        2 |  4498 | `	}else{` |
|   200010 |  4499 | `		rc = rc != 0;` |
|        - |  4500 | `	}` |
|   200012 |  4501 | `	VmPopOperand(&pTos,1);` |
|   200012 |  4502 | `	if( !pInstr->iP2 ){` |
|        - |  4503 | `		/* Push comparison result without taking the jump */` |
|   200012 |  4504 | `		PH7_MemObjRelease(pTos);` |
|   200012 |  4505 | `		pTos->x.iVal = rc;` |
|        - |  4506 | `		/* Invalidate any prior representation */` |
|   200012 |  4507 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100007 |  4508 | `	}else{` |
|      ! 0 |  4509 | `		if( rc ){` |
|        - |  4510 | `			/* Jump to the desired location */` |
|      ! 0 |  4511 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4512 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4513 | `		}` |
|        - |  4514 | `	}` |
|   200012 |  4515 | `	break;` |
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
|   101905 |  4535 | `case PH7_OP_LT:` |
|        - |  4536 | `case PH7_OP_LE: {` |
|   203856 |  4537 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4538 | `	/* Perform the comparison and act accordingly */` |
|        - |  4539 | `#ifdef UNTRUST` |
|        - |  4540 | `	if( pNos < pStack ){` |
|        - |  4541 | `		goto Abort;` |
|        - |  4542 | `	}` |
|        - |  4543 | `#endif` |
|   203856 |  4544 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   203856 |  4545 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4546 | `		rc = 0;` |
|   203852 |  4547 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      408 |  4548 | `		rc = rc < 1;` |
|      205 |  4549 | `	}else{` |
|   203442 |  4550 | `		rc = rc < 0;` |
|        - |  4551 | `	}` |
|   203856 |  4552 | `	VmPopOperand(&pTos,1);` |
|   203856 |  4553 | `	if( !pInstr->iP2 ){` |
|        - |  4554 | `		/* Push comparison result without taking the jump */` |
|   203856 |  4555 | `		PH7_MemObjRelease(pTos);` |
|   203856 |  4556 | `		pTos->x.iVal = rc;` |
|        - |  4557 | `		/* Invalidate any prior representation */` |
|   203856 |  4558 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   101951 |  4559 | `	}else{` |
|      ! 0 |  4560 | `		if( rc ){` |
|        - |  4561 | `			/* Jump to the desired location */` |
|      ! 0 |  4562 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4563 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4564 | `		}` |
|        - |  4565 | `	}` |
|   203856 |  4566 | `	break;` |
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
|    48358 |  4586 | `case PH7_OP_GT:` |
|        - |  4587 | `case PH7_OP_GE: {` |
|    96718 |  4588 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4589 | `	/* Perform the comparison and act accordingly */` |
|        - |  4590 | `#ifdef UNTRUST` |
|        - |  4591 | `	if( pNos < pStack ){` |
|        - |  4592 | `		goto Abort;` |
|        - |  4593 | `	}` |
|        - |  4594 | `#endif` |
|    96718 |  4595 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    96718 |  4596 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4597 | `		rc = 0;` |
|    96714 |  4598 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    96562 |  4599 | `		rc = rc >= 0;` |
|    48282 |  4600 | `	}else{` |
|      150 |  4601 | `		rc = rc > 0;` |
|        - |  4602 | `	}` |
|    96718 |  4603 | `	VmPopOperand(&pTos,1);` |
|    96718 |  4604 | `	if( !pInstr->iP2 ){` |
|        - |  4605 | `		/* Push comparison result without taking the jump */` |
|    96718 |  4606 | `		PH7_MemObjRelease(pTos);` |
|    96718 |  4607 | `		pTos->x.iVal = rc;` |
|        - |  4608 | `		/* Invalidate any prior representation */` |
|    96718 |  4609 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48360 |  4610 | `	}else{` |
|      ! 0 |  4611 | `		if( rc ){` |
|        - |  4612 | `			/* Jump to the desired location */` |
|      ! 0 |  4613 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4614 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4615 | `		}` |
|        - |  4616 | `	}` |
|    96718 |  4617 | `	break;` |
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
|     4770 |  4920 | `case PH7_OP_FOREACH_INIT: {` |
|     9542 |  4921 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4922 | `	void *pName;` |
|        - |  4923 | `#ifdef UNTRUST` |
|        - |  4924 | `	if( pTos < pStack ){` |
|        - |  4925 | `		goto Abort;` |
|        - |  4926 | `	}` |
|        - |  4927 | `#endif` |
|     9542 |  4928 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
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
|     9542 |  4941 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
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
|     9542 |  4954 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4955 | `		/* Jump out of the loop */` |
|      ! 0 |  4956 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4957 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4958 | `		}` |
|      ! 0 |  4959 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4960 | `	}else{` |
|        - |  4961 | `		ph7_foreach_step *pStep;` |
|     9542 |  4962 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9542 |  4963 | `		if( pStep == 0 ){` |
|      ! 0 |  4964 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4965 | `			/* Jump out of the loop */` |
|      ! 0 |  4966 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4967 | `		}else{` |
|        - |  4968 | `			/* Zero the structure */` |
|     9542 |  4969 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4970 | `			/* Prepare the step */` |
|     9542 |  4971 | `			pStep->iFlags = pInfo->iFlags;` |
|     9542 |  4972 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9528 |  4973 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4974 | `				/* Reset the internal loop cursor */` |
|     9528 |  4975 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4976 | `				/* Mark the step */` |
|     9528 |  4977 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9528 |  4978 | `				pStep->xIter.pMap = pMap;` |
|     9528 |  4979 | `				pMap->iRef++;` |
|     4765 |  4980 | `			}else{` |
|       16 |  4981 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4982 | `				ph7_class *pIteratorClass;` |
|        - |  4983 | `				/* Check if the object implements Iterator */` |
|       16 |  4984 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       19 |  4985 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
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
|        - |  4996 | `					/* Plain object iteration via hAttr */` |
|        9 |  4997 | `					SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  4998 | `					pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4999 | `					pStep->xIter.pThis = pThis;` |
|        9 |  5000 | `					pThis->iRef++;` |
|        - |  5001 | `				}` |
|        - |  5002 | `			}` |
|        - |  5003 | `		}` |
|     9542 |  5004 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5005 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5006 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5007 | `			/* Jump out of the loop */` |
|      ! 0 |  5008 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  5009 | `		}` |
|        - |  5010 | `	}` |
|     9542 |  5011 | `	VmPopOperand(&pTos,1);` |
|     9542 |  5012 | `	break;` |
|        - |  5013 | `						  }` |
|        - |  5014 | `/*` |
|        - |  5015 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5016 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5017 | ` */` |
|    76567 |  5018 | `case PH7_OP_FOREACH_STEP: {` |
|   153136 |  5019 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5020 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5021 | `	ph7_value *pValue;` |
|        - |  5022 | `	VmFrame *pFrameLocal;` |
|        - |  5023 | `	/* Peek the last step */` |
|   153136 |  5024 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   153136 |  5025 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   153136 |  5026 | `	pFrameLocal = pVm->pFrame;` |
|   153136 |  5027 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5028 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  5029 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5030 | `	}` |
|   153136 |  5031 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   153084 |  5032 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5033 | `		ph7_hashmap_node *pNode;` |
|        - |  5034 | `		/* Extract the current node value */` |
|   153084 |  5035 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   153084 |  5036 | `		if( pNode == 0 ){` |
|        - |  5037 | `			/* No more entry to process */` |
|     9528 |  5038 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9528 |  5039 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5040 | `				/* Break the reference with the last element */` |
|        5 |  5041 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5042 | `			}` |
|        - |  5043 | `			/* Automatically reset the loop cursor */` |
|     9528 |  5044 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5045 | `			/* Cleanup the mess left behind */` |
|     9528 |  5046 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9528 |  5047 | `			SySetPop(&pInfo->aStep);` |
|     9528 |  5048 | `			PH7_HashmapUnref(pMap);` |
|     4765 |  5049 | `		}else{` |
|   143558 |  5050 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5051 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5052 | `				if( pKey ){` |
|      416 |  5053 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5054 | `				}` |
|      207 |  5055 | `			}` |
|   143558 |  5056 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5057 | `				SyHashEntry *pEntry;` |
|        - |  5058 | `				/* Pass by reference */` |
|       13 |  5059 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  5060 | `				if( pEntry ){` |
|       13 |  5061 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  5062 | `				}else{` |
|      ! 0 |  5063 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5064 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5065 | `				}` |
|        7 |  5066 | `			}else{` |
|        - |  5067 | `				/* Make a copy of the entry value */` |
|   143546 |  5068 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   143546 |  5069 | `				if( pValue ){` |
|   143546 |  5070 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    71772 |  5071 | `				}` |
|        - |  5072 | `			}` |
|        2 |  5073 | `		}` |
|    76595 |  5074 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5075 | `		/* Iterator-based iteration.` |
|        - |  5076 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5077 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5078 | `		 */` |
|       29 |  5079 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5080 | `		ph7_class_method *pMethod;` |
|        - |  5081 | `		ph7_value sResult;` |
|       29 |  5082 | `		int isValid = 0;` |
|        - |  5083 | `		/* Call next() to advance — but skip on the first iteration */` |
|       29 |  5084 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|        7 |  5085 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|        4 |  5086 | `		}else{` |
|       23 |  5087 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       23 |  5088 | `			if( pMethod ){` |
|       23 |  5089 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       11 |  5090 | `			}` |
|        - |  5091 | `		}` |
|        - |  5092 | `		/* Call valid() */` |
|       29 |  5093 | `		PH7_MemObjInit(pVm,&sResult);` |
|       29 |  5094 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       29 |  5095 | `		if( pMethod ){` |
|       29 |  5096 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       29 |  5097 | `			PH7_MemObjToBool(&sResult);` |
|       29 |  5098 | `			isValid = (sResult.x.iVal != 0);` |
|       14 |  5099 | `		}` |
|       29 |  5100 | `		PH7_MemObjRelease(&sResult);` |
|       29 |  5101 | `		if( !isValid ){` |
|        - |  5102 | `			/* Iterator exhausted */` |
|        5 |  5103 | `			pc = pInstr->iP2 - 1;` |
|        5 |  5104 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        5 |  5105 | `			SySetPop(&pInfo->aStep);` |
|        5 |  5106 | `			PH7_ClassInstanceUnref(pThis);` |
|        3 |  5107 | `		}else{` |
|        - |  5108 | `			/* Call current() to get value */` |
|       25 |  5109 | `			PH7_MemObjInit(pVm,&sResult);` |
|       25 |  5110 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       25 |  5111 | `			if( pMethod ){` |
|       25 |  5112 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       12 |  5113 | `			}` |
|       25 |  5114 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       25 |  5115 | `			if( pValue ){` |
|       25 |  5116 | `				PH7_MemObjStore(&sResult,pValue);` |
|       12 |  5117 | `			}` |
|       25 |  5118 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5119 | `			/* Call key() if needed */` |
|       25 |  5120 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5121 | `				ph7_value sKey;` |
|       17 |  5122 | `				PH7_MemObjInit(pVm,&sKey);` |
|       17 |  5123 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       17 |  5124 | `				if( pMethod ){` |
|       17 |  5125 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|        8 |  5126 | `				}` |
|       17 |  5127 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5128 | `				if( pValue ){` |
|       17 |  5129 | `					PH7_MemObjStore(&sKey,pValue);` |
|        8 |  5130 | `				}` |
|       17 |  5131 | `				PH7_MemObjRelease(&sKey);` |
|        8 |  5132 | `			}` |
|        - |  5133 | `		}` |
|       15 |  5134 | `	}else{` |
|       25 |  5135 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5136 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5137 | `		SyHashEntry *pEntry;` |
|        - |  5138 | `		/* Point to the next attribute */` |
|       29 |  5139 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5140 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5141 | `			/* Check access permission */` |
|       31 |  5142 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5143 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5144 | `					break; /* Access is granted */` |
|        - |  5145 | `			}` |
|        1 |  5146 | `		}` |
|       25 |  5147 | `		if( pEntry == 0 ){` |
|        - |  5148 | `			/* Clean up the mess left behind */` |
|        9 |  5149 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5150 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5151 | `				/* Break the reference with the last element */` |
|        3 |  5152 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5153 | `			}` |
|        9 |  5154 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5155 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5156 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5157 | `		}else{` |
|       17 |  5158 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5159 | `			ph7_value *pAttrValue;` |
|       17 |  5160 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5161 | `				/* Fill with the current attribute name */` |
|       17 |  5162 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5163 | `				if( pKey ){` |
|       17 |  5164 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5165 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5166 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5167 | `				}` |
|        8 |  5168 | `			}` |
|        - |  5169 | `			/* Extract attribute value */` |
|       17 |  5170 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5171 | `			if( pAttrValue ){` |
|       17 |  5172 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5173 | `					/* Pass by reference */` |
|        3 |  5174 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5175 | `					if( pEntry ){` |
|        3 |  5176 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5177 | `					}else{` |
|      ! 0 |  5178 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5179 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5180 | `					}` |
|        2 |  5181 | `				}else{` |
|        - |  5182 | `					/* Make a copy of the attribute value */` |
|       15 |  5183 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5184 | `					if( pValue ){` |
|       15 |  5185 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5186 | `					}` |
|        - |  5187 | `				}` |
|        8 |  5188 | `			}` |
|        - |  5189 | `		}` |
|        - |  5190 | `	}` |
|   153136 |  5191 | `	break;` |
|        - |  5192 | `						  }` |
|        - |  5193 | `/*` |
|        - |  5194 | ` * OP_MEMBER P1 P2` |
|        - |  5195 | ` * Load class attribute/method on the stack.` |
|        - |  5196 | ` */` |
|     2041 |  5197 | `case PH7_OP_MEMBER: {` |
|        - |  5198 | `	ph7_class_instance *pThis;` |
|        - |  5199 | `	ph7_value *pNos;` |
|        - |  5200 | `	SyString sName;` |
|     4084 |  5201 | `	if( !pInstr->iP1 ){` |
|     3990 |  5202 | `		pNos = &pTos[-1];` |
|        - |  5203 | `#ifdef UNTRUST` |
|        - |  5204 | `		if( pNos < pStack ){` |
|        - |  5205 | `			goto Abort;` |
|        - |  5206 | `		}` |
|        - |  5207 | `#endif` |
|     3990 |  5208 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5209 | `			ph7_class *pClass;` |
|        - |  5210 | `			/* Class already instantiated */` |
|     3990 |  5211 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5212 | `			/* Point to the instantiated class */` |
|     3990 |  5213 | `			pClass = pThis->pClass;` |
|        - |  5214 | `			/* Extract attribute name first */` |
|     3990 |  5215 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     3990 |  5216 | `			if( pInstr->iP2 ){` |
|        - |  5217 | `				/* Method call */` |
|      270 |  5218 | `				ph7_class_method *pMeth = 0;` |
|      270 |  5219 | `				if( sName.nByte > 0 ){` |
|        - |  5220 | `					/* Extract the target method */` |
|      270 |  5221 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      134 |  5222 | `				}` |
|      270 |  5223 | `				if( pMeth == 0 ){` |
|      ! 0 |  5224 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5225 | `						&pClass->sName,&sName` |
|        - |  5226 | `						);` |
|        - |  5227 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5228 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5229 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5230 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5231 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5232 | `				}else{` |
|        - |  5233 | `					/* Push method name on the stack */` |
|      270 |  5234 | `					PH7_MemObjRelease(pTos);` |
|      270 |  5235 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      270 |  5236 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5237 | `				}` |
|      270 |  5238 | `				pTos->nIdx = SXU32_HIGH;` |
|      136 |  5239 | `			}else{` |
|        - |  5240 | `				/* Attribute access */` |
|     3722 |  5241 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5242 | `				SyHashEntry *pEntry;` |
|        - |  5243 | `				/* Extract the target attribute */` |
|     3722 |  5244 | `				if( sName.nByte > 0 ){` |
|     3722 |  5245 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3722 |  5246 | `					if( pEntry ){` |
|        - |  5247 | `						/* Point to the attribute value */` |
|     3720 |  5248 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1859 |  5249 | `					}` |
|     1860 |  5250 | `				}` |
|     3722 |  5251 | `				if( pObjAttr == 0 ){` |
|        - |  5252 | `					/* No such attribute,load null */` |
|        4 |  5253 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5254 | `						&pClass->sName,&sName);` |
|        - |  5255 | `					/* Call the __get magic method if available */` |
|        3 |  5256 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5257 | `				}` |
|     3722 |  5258 | `				VmPopOperand(&pTos,1);` |
|        - |  5259 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5260 | `				 * This is due to the following case:` |
|        - |  5261 | `				 *     (new TestClass())->foo;` |
|        - |  5262 | `				 */` |
|     3722 |  5263 | `				pThis->iRef++;` |
|     3722 |  5264 | `				PH7_MemObjRelease(pTos);` |
|     3722 |  5265 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3722 |  5266 | `				if( pObjAttr ){` |
|     3720 |  5267 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5268 | `					/* Check attribute access */` |
|     3720 |  5269 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5270 | `						/* Load attribute */` |
|     3720 |  5271 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3720 |  5272 | `						if( pValue ){` |
|     3720 |  5273 | `							if( pThis->iRef < 2 ){` |
|        - |  5274 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5275 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5276 | `								 */` |
|        3 |  5277 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5278 | `							}else{` |
|        - |  5279 | `								/* Simple load */` |
|     3718 |  5280 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5281 | `							}` |
|     3720 |  5282 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3718 |  5283 | `								if( pThis->iRef > 1 ){` |
|        - |  5284 | `									/* Load attribute index */` |
|     3716 |  5285 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1857 |  5286 | `								}` |
|     1858 |  5287 | `							}` |
|     1859 |  5288 | `						}` |
|     1859 |  5289 | `					}` |
|     1859 |  5290 | `				}` |
|        - |  5291 | `				/* Safely unreference the object */` |
|     3722 |  5292 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5293 | `			}` |
|     1996 |  5294 | `		}else{` |
|      ! 0 |  5295 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5296 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5297 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5298 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5299 | `		}` |
|     1996 |  5300 | `	}else{` |
|        - |  5301 | `		/* Static member access using class name */` |
|       96 |  5302 | `		pNos = pTos;` |
|       96 |  5303 | `		pThis = 0;` |
|       96 |  5304 | `		if( !pInstr->p3 ){` |
|       84 |  5305 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       84 |  5306 | `			pNos--;` |
|        - |  5307 | `#ifdef UNTRUST` |
|        - |  5308 | `			if( pNos < pStack ){` |
|        - |  5309 | `				goto Abort;` |
|        - |  5310 | `			}` |
|        - |  5311 | `#endif` |
|       43 |  5312 | `		}else{` |
|        - |  5313 | `			/* Attribute name already computed */` |
|       14 |  5314 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5315 | `		}` |
|       96 |  5316 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       96 |  5317 | `			ph7_class *pClass = 0;` |
|       96 |  5318 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5319 | `				/* Class already instantiated */` |
|      ! 0 |  5320 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5321 | `				pClass = pThis->pClass;` |
|      ! 0 |  5322 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5323 | `			}else{` |
|        - |  5324 | `				/* Try to extract the target class */` |
|       96 |  5325 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       96 |  5326 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|       96 |  5327 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5328 | `					/* Handle self/static/parent keywords */` |
|       96 |  5329 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       26 |  5330 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       84 |  5331 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5332 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       71 |  5333 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5334 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5335 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5336 | `							pClass = pSelf->pBase;` |
|        6 |  5337 | `						}` |
|        8 |  5338 | `					}else{` |
|       46 |  5339 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5340 | `					}` |
|       47 |  5341 | `				}` |
|        - |  5342 | `			}` |
|       96 |  5343 | `			if( pClass == 0 ){` |
|        - |  5344 | `				/* Undefined class */` |
|      ! 0 |  5345 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5346 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5347 | `					);` |
|      ! 0 |  5348 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5349 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5350 | `				}` |
|      ! 0 |  5351 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5352 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5353 | `			}else{` |
|       96 |  5354 | `				if( pInstr->iP2 ){` |
|        - |  5355 | `					/* Method call */` |
|       30 |  5356 | `					ph7_class_method *pMeth = 0;` |
|       30 |  5357 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5358 | `						/* Extract the target method */` |
|       30 |  5359 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       14 |  5360 | `					}` |
|       30 |  5361 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5362 | `						if( pMeth ){` |
|      ! 0 |  5363 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5364 | `								&pClass->sName,&sName` |
|        - |  5365 | `								);` |
|      ! 0 |  5366 | `						}else{` |
|      ! 0 |  5367 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5368 | `								&pClass->sName,&sName` |
|        - |  5369 | `								);` |
|        - |  5370 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5371 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5372 | `						}` |
|        - |  5373 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5374 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5375 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5376 | `						}` |
|      ! 0 |  5377 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5378 | `					}else{` |
|        - |  5379 | `						/* Push method name on the stack */` |
|       30 |  5380 | `						PH7_MemObjRelease(pTos);` |
|       30 |  5381 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       30 |  5382 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5383 | `					}` |
|       30 |  5384 | `					pTos->nIdx = SXU32_HIGH;` |
|       16 |  5385 | `				}else{` |
|        - |  5386 | `					/* Attribute access */` |
|       68 |  5387 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5388 | `					/* Check for special ::class pseudo-constant */` |
|       98 |  5389 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       60 |  5390 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5391 | `						/* ::class returns the fully qualified class name */` |
|        - |  5392 | `						/* Pop the attribute name from the stack */` |
|       50 |  5393 | `						if( !pInstr->p3 ){` |
|       50 |  5394 | `							VmPopOperand(&pTos,1);` |
|       24 |  5395 | `						}` |
|       50 |  5396 | `						PH7_MemObjRelease(pTos);` |
|        - |  5397 | `						/* Load the class name */` |
|       50 |  5398 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       50 |  5399 | `						pTos->nIdx = SXU32_HIGH;` |
|       26 |  5400 | `					}else{` |
|        - |  5401 | `						/* Extract the target attribute */` |
|       20 |  5402 | `						if( sName.nByte > 0 ){` |
|       20 |  5403 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5404 | `						}` |
|       20 |  5405 | `						if( pAttr == 0 ){` |
|        - |  5406 | `							/* No such attribute,load null */` |
|      ! 0 |  5407 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5408 | `								&pClass->sName,&sName);` |
|        - |  5409 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5410 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5411 | `						}` |
|        - |  5412 | `						/* Pop the attribute name from the stack */` |
|       20 |  5413 | `						if( !pInstr->p3 ){` |
|        7 |  5414 | `							VmPopOperand(&pTos,1);` |
|        3 |  5415 | `						}` |
|       20 |  5416 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5417 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5418 | `						if( pAttr ){` |
|       20 |  5419 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5420 | `								/* Access to a non static attribute */` |
|      ! 0 |  5421 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5422 | `									&pClass->sName,&pAttr->sName` |
|        - |  5423 | `									);` |
|      ! 0 |  5424 | `							}else{` |
|        - |  5425 | `								ph7_value *pValue;` |
|        - |  5426 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5427 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5428 | `									/* Load the desired attribute */` |
|       20 |  5429 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5430 | `									if( pValue ){` |
|       20 |  5431 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5432 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5433 | `											/* Load index number */` |
|       14 |  5434 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5435 | `										}` |
|        9 |  5436 | `									}` |
|        9 |  5437 | `								}` |
|        - |  5438 | `							}` |
|        9 |  5439 | `						}` |
|        - |  5440 | `					}` |
|        - |  5441 | `				}` |
|       96 |  5442 | `				if( pThis ){` |
|        - |  5443 | `					/* Safely unreference the object */` |
|      ! 0 |  5444 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5445 | `				}` |
|        - |  5446 | `			}` |
|       49 |  5447 | `		}else{` |
|        - |  5448 | `			/* Pop operands */` |
|      ! 0 |  5449 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5450 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5451 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5452 | `			}` |
|      ! 0 |  5453 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5454 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5455 | `		}` |
|        - |  5456 | `	}` |
|     4084 |  5457 | `	break;` |
|        - |  5458 | `					}` |
|        - |  5459 | `/*` |
|        - |  5460 | ` * OP_NEW P1 * * *` |
|        - |  5461 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5462 | ` */` |
|      297 |  5463 | `case PH7_OP_NEW: {` |
|      596 |  5464 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      596 |  5465 | `	ph7_class *pClass = 0;` |
|        - |  5466 | `	ph7_class_instance *pNew;` |
|      596 |  5467 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5468 | `		/* Try to extract the desired class */` |
|      893 |  5469 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      594 |  5470 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      297 |  5471 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5472 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5473 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5474 | `	}` |
|      596 |  5475 | `	if( pClass == 0 ){` |
|        - |  5476 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5477 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5478 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5479 | `			);` |
|        - |  5480 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5481 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5482 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5483 | `			/* Pop given arguments */` |
|      ! 0 |  5484 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5485 | `		}` |
|      ! 0 |  5486 | `		goto Abort;` |
|      ! 0 |  5487 | `	}else{` |
|        - |  5488 | `		ph7_class_method *pCons;` |
|        - |  5489 | `		/* Create a new class instance */` |
|      596 |  5490 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      596 |  5491 | `		if( pNew == 0 ){` |
|      ! 0 |  5492 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5493 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5494 | `				&pClass->sName` |
|        - |  5495 | `			);` |
|      ! 0 |  5496 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5497 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5498 | `				/* Pop given arguments */` |
|      ! 0 |  5499 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5500 | `			}` |
|      ! 0 |  5501 | `			break;` |
|        - |  5502 | `		}` |
|        - |  5503 | `		/* Check if a constructor is available */` |
|      596 |  5504 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      596 |  5505 | `		if( pCons == 0 ){` |
|      520 |  5506 | `			SyString *pName = &pClass->sName;` |
|        - |  5507 | `			/* Check for a constructor with the same base class name */` |
|      520 |  5508 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      259 |  5509 | `		}` |
|      596 |  5510 | `		if( pCons ){` |
|        - |  5511 | `			/* Call the class constructor */` |
|       78 |  5512 | `			SySetReset(&aArg);` |
|      146 |  5513 | `			while( pArg < pTos ){` |
|       70 |  5514 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       70 |  5515 | `				pArg++;` |
|        2 |  5516 | `			}` |
|       78 |  5517 | `			if( pVm->bErrReport ){` |
|        - |  5518 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5519 | `				sxu32 n;` |
|       35 |  5520 | `				n = SySetUsed(&aArg);` |
|        - |  5521 | `				/* Emit a notice for missing arguments */` |
|       83 |  5522 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  5523 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  5524 | `					if( pFuncArg ){` |
|       49 |  5525 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5526 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5527 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5528 | `						}` |
|       24 |  5529 | `					}` |
|       49 |  5530 | `					n++;` |
|        1 |  5531 | `				}` |
|       17 |  5532 | `			}` |
|       78 |  5533 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5534 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       78 |  5535 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5536 | `				pNew->iRef = 1;` |
|      ! 0 |  5537 | `			}` |
|       38 |  5538 | `		}` |
|      596 |  5539 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5540 | `			/* Pop given arguments */` |
|       62 |  5541 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       30 |  5542 | `		}` |
|      596 |  5543 | `		PH7_MemObjRelease(pTos);` |
|      596 |  5544 | `		pTos->x.pOther = pNew;` |
|      596 |  5545 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5546 | `	}` |
|      596 |  5547 | `	break;` |
|        - |  5548 | `				 }` |
|        - |  5549 | `/*` |
|        - |  5550 | ` * OP_CLONE * * *` |
|        - |  5551 | ` * Perfome a clone operation.` |
|        - |  5552 | ` */` |
|       23 |  5553 | `case PH7_OP_CLONE: {` |
|        - |  5554 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5555 | `#ifdef UNTRUST` |
|        - |  5556 | `	if( pTos < pStack ){` |
|        - |  5557 | `		goto Abort;` |
|        - |  5558 | `	}` |
|        - |  5559 | `#endif` |
|        - |  5560 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5561 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5562 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5563 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5564 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5565 | `		break;` |
|        - |  5566 | `	}` |
|        - |  5567 | `	/* Point to the source */` |
|       44 |  5568 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5569 | `	/* Perform the clone operation */` |
|       44 |  5570 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5571 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5572 | `	if( pClone == 0 ){` |
|      ! 0 |  5573 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5574 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5575 | `	}else{` |
|        - |  5576 | `		/* Load the cloned object */` |
|       44 |  5577 | `		pTos->x.pOther = pClone;` |
|       44 |  5578 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5579 | `	}` |
|       44 |  5580 | `	break;` |
|        - |  5581 | `				   }` |
|        - |  5582 | `/*` |
|        - |  5583 | ` * OP_SWITCH * * P3` |
|        - |  5584 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5585 | ` */` |
|       18 |  5586 | `case PH7_OP_SWITCH: {` |
|       38 |  5587 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5588 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5589 | `	ph7_value sValue,sCaseValue;` |
|        - |  5590 | `	sxu32 n,nEntry;` |
|        - |  5591 | `#ifdef UNTRUST` |
|        - |  5592 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5593 | `		goto Abort;` |
|        - |  5594 | `	}` |
|        - |  5595 | `#endif` |
|        - |  5596 | `	/* Point to the case table  */` |
|       38 |  5597 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5598 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5599 | `	/* Select the appropriate case block to execute */` |
|       38 |  5600 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5601 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5602 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5603 | `		pCase = &aCase[n];` |
|       92 |  5604 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5605 | `		/* Execute the case expression first */` |
|       92 |  5606 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5607 | `		/* Compare the two expression */` |
|       92 |  5608 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5609 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5610 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5611 | `		if( rc == 0 ){` |
|        - |  5612 | `			/* Value match,jump to this block */` |
|       38 |  5613 | `			pc = pCase->nStart - 1;` |
|       38 |  5614 | `			break;` |
|        - |  5615 | `		}` |
|       29 |  5616 | `	}` |
|       38 |  5617 | `	VmPopOperand(&pTos,1);` |
|       38 |  5618 | `	if( n >= nEntry ){` |
|        - |  5619 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5620 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5621 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5622 | `		}else{` |
|        - |  5623 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5624 | `			pc = pSwitch->nOut - 1;` |
|        - |  5625 | `		}` |
|      ! 0 |  5626 | `	}` |
|       38 |  5627 | `	break;` |
|        - |  5628 | `					}` |
|        - |  5629 | `/*` |
|        - |  5630 | ` * OP_CALL P1 * *` |
|        - |  5631 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5632 | ` *  function on the stack.` |
|        - |  5633 | ` */` |
|   278837 |  5634 | `case PH7_OP_CALL: {` |
|   557720 |  5635 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5636 | `	SyHashEntry *pEntry;` |
|        - |  5637 | `	SyString sName;` |
|        - |  5638 | `	/* Extract function name */` |
|   557720 |  5639 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5640 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5641 | `			ph7_value sResult;` |
|      ! 0 |  5642 | `			SySetReset(&aArg);` |
|      ! 0 |  5643 | `			while( pArg < pTos ){` |
|      ! 0 |  5644 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5645 | `				pArg++;` |
|      ! 0 |  5646 | `			}` |
|      ! 0 |  5647 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5648 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5649 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5650 | `			SySetReset(&aArg);` |
|        - |  5651 | `			/* Pop given arguments */` |
|      ! 0 |  5652 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5653 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5654 | `			}` |
|        - |  5655 | `			/* Copy result */` |
|      ! 0 |  5656 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5657 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5658 | `		}else{` |
|        3 |  5659 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5660 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5661 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5662 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5663 | `			}else{` |
|        - |  5664 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5665 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5666 | `			}` |
|        - |  5667 | `			/* Pop given arguments */` |
|        3 |  5668 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5669 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5670 | `			}` |
|        - |  5671 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5672 | `			PH7_MemObjRelease(pTos);` |
|        - |  5673 | `		}` |
|   278604 |  5674 | `		break;` |
|        - |  5675 | `	}` |
|   557718 |  5676 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5677 | `	/* Check for a compiled function first.` |
|        - |  5678 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5679 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   557718 |  5680 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5681 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5682 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5683 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5684 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5685 | `	 * function calls inside namespaces. */` |
|   557718 |  5686 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5687 | `		const char *zFunc;` |
|        - |  5688 | `		const char *zEnd;` |
|        - |  5689 | `		const char *z;` |
|        - |  5690 | `		SyString sGlobal;` |
|       15 |  5691 | `		zFunc = sName.zString;` |
|       15 |  5692 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5693 | `		z = zEnd;` |
|        - |  5694 | `		/* Find last namespace separator */` |
|      133 |  5695 | `		while( z > zFunc ){` |
|      133 |  5696 | `			if( z[-1] == '\\' ){` |
|       15 |  5697 | `				break;` |
|        - |  5698 | `			}` |
|      119 |  5699 | `			z--;` |
|        1 |  5700 | `		}` |
|       15 |  5701 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5702 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5703 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5704 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5705 | `		}` |
|        7 |  5706 | `	}` |
|   557718 |  5707 | `	if( pEntry ){` |
|        - |  5708 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5709 | `		ph7_class_instance *pThis;` |
|        - |  5710 | `		ph7_value *pFrameStack;` |
|        - |  5711 | `		ph7_vm_func *pVmFunc;` |
|        - |  5712 | `		ph7_class *pSelf;` |
|        - |  5713 | `		VmFrame *pFrame;` |
|        - |  5714 | `		ph7_value *pObj;` |
|        - |  5715 | `		VmSlot sArg;` |
|        - |  5716 | `		sxu32 n;` |
|        - |  5717 | `		/* initialize fields */` |
|    12316 |  5718 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12316 |  5719 | `		pThis = 0;` |
|    12316 |  5720 | `		pSelf = 0;` |
|    12316 |  5721 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5722 | `			ph7_class_method *pMeth;` |
|        - |  5723 | `			/* Class method call */` |
|     1558 |  5724 | `			ph7_value *pTarget = &pTos[-1];` |
|     1558 |  5725 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5726 | `				/* Extract the 'this' pointer */` |
|     1558 |  5727 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5728 | `					/* Instance already loaded */` |
|     1524 |  5729 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1524 |  5730 | `					pThis->iRef++;` |
|     1524 |  5731 | `					pSelf = pThis->pClass;` |
|      761 |  5732 | `				}` |
|     1558 |  5733 | `				if( pSelf == 0 ){` |
|       36 |  5734 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5735 | `						/* "Late Static Binding" class name */` |
|       44 |  5736 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       14 |  5737 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       14 |  5738 | `					}` |
|       36 |  5739 | `					if( pSelf == 0 ){` |
|       13 |  5740 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5741 | `					}` |
|       17 |  5742 | `				}` |
|     1558 |  5743 | `				if( pThis == 0  ){` |
|       36 |  5744 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  5745 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5746 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5747 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5748 | `					}` |
|       36 |  5749 | `					if( pFrameLocal->pParent ){` |
|        - |  5750 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5751 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5752 | `						if( pThis ){` |
|       13 |  5753 | `							pThis->iRef++;` |
|        6 |  5754 | `						}` |
|        9 |  5755 | `					}` |
|       17 |  5756 | `				}` |
|     1558 |  5757 | `				VmPopOperand(&pTos,1);` |
|     1558 |  5758 | `				PH7_MemObjRelease(pTos);` |
|        - |  5759 | `				/* Synchronize pointers */` |
|     1558 |  5760 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5761 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5762 | `				 * user have already computed the random generated unique class method name` |
|        - |  5763 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5764 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5765 | `				 */` |
|     1558 |  5766 | `				while( pArg < pStack ){` |
|      ! 0 |  5767 | `					pArg++;` |
|      ! 0 |  5768 | `				}` |
|     1558 |  5769 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5770 | `					/* Check if the call is allowed */` |
|     1558 |  5771 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1558 |  5772 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  5773 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5774 | `							/* Pop given arguments */` |
|      ! 0 |  5775 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5776 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5777 | `							}` |
|        - |  5778 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5779 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5780 | `							break;` |
|        - |  5781 | `						}` |
|        3 |  5782 | `					}` |
|      778 |  5783 | `				}` |
|      778 |  5784 | `			}` |
|      778 |  5785 | `		}` |
|        - |  5786 | `		/* Check The recursion limit */` |
|    12316 |  5787 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5788 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5789 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5790 | `				&pVmFunc->sName);` |
|        - |  5791 | `			/* Pop given arguments */` |
|        3 |  5792 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5793 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5794 | `			}` |
|        - |  5795 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5796 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5797 | `			break;` |
|        - |  5798 | `		}` |
|    12314 |  5799 | `		if( pVmFunc->pNextName ){` |
|        - |  5800 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  5801 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  5802 | `		}` |
|        - |  5803 | `		/* Extract the formal argument set */` |
|    12314 |  5804 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5805 | `		/* Create a new VM frame  */` |
|    12314 |  5806 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12314 |  5807 | `		if( rc != SXRET_OK ){` |
|        - |  5808 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5809 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5810 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5811 | `				&pVmFunc->sName);` |
|        - |  5812 | `			/* Pop given arguments */` |
|      ! 0 |  5813 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5814 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5815 | `			}` |
|        - |  5816 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5817 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5818 | `			break;` |
|        - |  5819 | `		}` |
|    12314 |  5820 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5821 | `			/* Install the '$this' variable */` |
|        - |  5822 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1534 |  5823 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1534 |  5824 | `			if( pObj ){` |
|        - |  5825 | `				/* Reflect the change */` |
|     1534 |  5826 | `				pObj->x.pOther = pThis;` |
|     1534 |  5827 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      766 |  5828 | `			}` |
|      766 |  5829 | `		}` |
|    12314 |  5830 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5831 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5832 | `			/* Install static variables */` |
|      ! 0 |  5833 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5834 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5835 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5836 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5837 | `					/* Initialize the static variables */` |
|      ! 0 |  5838 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5839 | `					if( pObj ){` |
|        - |  5840 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5841 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5842 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5843 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5844 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5845 | `						}` |
|      ! 0 |  5846 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5847 | `					}else{` |
|      ! 0 |  5848 | `						continue;` |
|        - |  5849 | `					}` |
|      ! 0 |  5850 | `				}` |
|        - |  5851 | `				/* Install in the current frame */` |
|      ! 0 |  5852 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5853 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5854 | `			}` |
|      ! 0 |  5855 | `		}` |
|        - |  5856 | `		/* Push arguments in the local frame */` |
|    12314 |  5857 | `		n = 0;` |
|    34026 |  5858 | `		while( pArg < pTos ){` |
|    21714 |  5859 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    21564 |  5860 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5861 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5862 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5863 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5864 | `						goto Abort;` |
|        - |  5865 | `					}` |
|      ! 0 |  5866 | `				}` |
|        - |  5867 | `				/* Make sure the given arguments are of the correct type */` |
|    21564 |  5868 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5869 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5870 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5871 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5872 | `						ph7_class *pClass;` |
|        - |  5873 | `						/* Try to extract the desired class */` |
|      ! 0 |  5874 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5875 | `						if( pClass ){` |
|      ! 0 |  5876 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5877 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5878 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5879 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5880 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5881 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5882 | `								}` |
|      ! 0 |  5883 | `							}else{` |
|        - |  5884 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5885 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5886 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5887 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5888 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5889 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5890 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5891 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5892 | `								}` |
|        - |  5893 | `							}` |
|      ! 0 |  5894 | `						}` |
|     1088 |  5895 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5896 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5897 | `						/* Cast to the desired type */` |
|      ! 0 |  5898 | `						xCast(pArg);` |
|      ! 0 |  5899 | `					}` |
|      543 |  5900 | `				}` |
|    21564 |  5901 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5902 | `					/* Pass by reference */` |
|       48 |  5903 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5904 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5905 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5906 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5907 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5908 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5909 | `						}` |
|        - |  5910 | `						/* Switch to pass by value */` |
|      ! 0 |  5911 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5912 | `					}else{` |
|        - |  5913 | `						SyHashEntry *pRefEntry;` |
|        - |  5914 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5915 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5916 | `						if( pRefEntry == 0 ){` |
|       71 |  5917 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5918 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5919 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5920 | `							sArg.pUserData = 0;` |
|       48 |  5921 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5922 | `						}` |
|       48 |  5923 | `						pObj = 0;` |
|        - |  5924 | `					}` |
|       25 |  5925 | `				}else{` |
|        - |  5926 | `					/* Pass by value,make a copy of the given argument */` |
|    21518 |  5927 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5928 | `				}` |
|    10783 |  5929 | `			}else{` |
|        - |  5930 | `				char zName[32];` |
|        - |  5931 | `				SyString sArgName;` |
|        - |  5932 | `				/* Set a dummy name */` |
|      152 |  5933 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5934 | `				sArgName.zString = zName;` |
|        - |  5935 | `				/* Annonymous argument */` |
|      152 |  5936 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5937 | `			}` |
|    21714 |  5938 | `			if( pObj ){` |
|    21668 |  5939 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5940 | `				/* Insert argument index  */` |
|    21668 |  5941 | `				sArg.nIdx = pObj->nIdx;` |
|    21668 |  5942 | `				sArg.pUserData = 0;` |
|    21668 |  5943 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10833 |  5944 | `			}` |
|    21714 |  5945 | `			PH7_MemObjRelease(pArg);` |
|    21714 |  5946 | `			pArg++;` |
|    21714 |  5947 | `			++n;` |
|        2 |  5948 | `		}` |
|        - |  5949 | `		/* Set up closure environment */` |
|    12314 |  5950 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5951 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5952 | `			ph7_value *pValue;` |
|        - |  5953 | `			sxu32 iEnv;` |
|        9 |  5954 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5955 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5956 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5957 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5958 | `					/* Do not install null value */` |
|        9 |  5959 | `					continue;` |
|        - |  5960 | `				}` |
|        9 |  5961 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5962 | `				if( pValue == 0 ){` |
|      ! 0 |  5963 | `					continue;` |
|        - |  5964 | `				}` |
|        - |  5965 | `				/* Invalidate any prior representation */` |
|        9 |  5966 | `				PH7_MemObjRelease(pValue);` |
|        - |  5967 | `				/* Duplicate bound variable value */` |
|        9 |  5968 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5969 | `			}` |
|        4 |  5970 | `		}` |
|        - |  5971 | `		/* Process default values */` |
|    14172 |  5972 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1860 |  5973 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1854 |  5974 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1854 |  5975 | `				if( pObj ){` |
|        - |  5976 | `					/* Evaluate the default value and extract it's result */` |
|     1854 |  5977 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1854 |  5978 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5979 | `						goto Abort;` |
|        - |  5980 | `					}` |
|        - |  5981 | `					/* Insert argument index */` |
|     1854 |  5982 | `					sArg.nIdx = pObj->nIdx;` |
|     1854 |  5983 | `					sArg.pUserData = 0;` |
|     1854 |  5984 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5985 | `					/* Make sure the default argument is of the correct type */` |
|     1854 |  5986 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5987 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5988 | `						/* Cast to the desired type */` |
|      ! 0 |  5989 | `						xCast(pObj);` |
|      ! 0 |  5990 | `					}` |
|      926 |  5991 | `				}` |
|      926 |  5992 | `			}` |
|     1860 |  5993 | `			++n;` |
|        2 |  5994 | `		}` |
|        - |  5995 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5996 | `		 * does not return anything.` |
|        - |  5997 | `		 */` |
|    12314 |  5998 | `		PH7_MemObjRelease(pTos);` |
|    12314 |  5999 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6000 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12314 |  6001 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12314 |  6002 | `		if( pFrameStack == 0 ){` |
|        - |  6003 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6004 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6005 | `				&pVmFunc->sName);` |
|      ! 0 |  6006 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6007 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6008 | `			}` |
|      ! 0 |  6009 | `			break;` |
|        - |  6010 | `		}` |
|    12314 |  6011 | `		if( pSelf ){` |
|        - |  6012 | `			/* Push class name */` |
|     1556 |  6013 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      777 |  6014 | `		}` |
|        - |  6015 | `		/* Increment nesting level */` |
|    12314 |  6016 | `		pVm->nRecursionDepth++;` |
|        - |  6017 | `		/* Execute function body */` |
|    12314 |  6018 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  6019 | `		/* Decrement nesting level */` |
|    12314 |  6020 | `		pVm->nRecursionDepth--;` |
|    12314 |  6021 | `		if( pSelf ){` |
|        - |  6022 | `			/* Pop class name */` |
|     1556 |  6023 | `			(void)SySetPop(&pVm->aSelf);` |
|      777 |  6024 | `		}` |
|        - |  6025 | `		/* Cleanup the mess left behind */` |
|    12314 |  6026 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6027 | `			/* Return by reference,reflect that */` |
|        9 |  6028 | `			if( n != SXU32_HIGH ){` |
|        9 |  6029 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6030 | `				sxu32 i;` |
|        - |  6031 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6032 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6033 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6034 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6035 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6036 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6037 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6038 | `								&pVmFunc->sName);` |
|      ! 0 |  6039 | `						}` |
|      ! 0 |  6040 | `						n = SXU32_HIGH;` |
|      ! 0 |  6041 | `						break;` |
|        - |  6042 | `					}` |
|        3 |  6043 | `				}` |
|        5 |  6044 | `			}else{` |
|      ! 0 |  6045 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6046 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6047 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6048 | `						&pVmFunc->sName);` |
|      ! 0 |  6049 | `				}` |
|        - |  6050 | `			}` |
|        9 |  6051 | `			pTos->nIdx = n;` |
|        4 |  6052 | `		}` |
|        - |  6053 | `		/* Cleanup the mess left behind */` |
|    12314 |  6054 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6055 | `			/* An exception was throw in this frame */` |
|        7 |  6056 | `			pFrame = pFrame->pParent;` |
|        7 |  6057 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6058 | `				/* Pop the resutlt */` |
|        5 |  6059 | `				VmPopOperand(&pTos,1);` |
|        - |  6060 | `				/* Jump to this destination */` |
|        5 |  6061 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  6062 | `				rc = PH7_OK;` |
|        3 |  6063 | `			}else{` |
|        3 |  6064 | `				if( pFrame->pParent ){` |
|        3 |  6065 | `					rc = PH7_EXCEPTION;` |
|        2 |  6066 | `				}else{` |
|        - |  6067 | `					/* Continue normal execution */` |
|      ! 0 |  6068 | `					rc = PH7_OK;` |
|        - |  6069 | `				}` |
|        - |  6070 | `			}` |
|        3 |  6071 | `		}` |
|        - |  6072 | `		/* Free the operand stack */` |
|    12314 |  6073 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6074 | `		/* Leave the frame */` |
|    12314 |  6075 | `		VmLeaveFrame(&(*pVm));` |
|    12314 |  6076 | `		if( rc == PH7_ABORT ){` |
|        - |  6077 | `			/* Abort processing immeditaley */` |
|        7 |  6078 | `			goto Abort;` |
|    12308 |  6079 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6080 | `			goto Exception;` |
|        - |  6081 | `		}` |
|     6154 |  6082 | `	}else{` |
|        - |  6083 | `		ph7_user_func *pFunc;` |
|        - |  6084 | `		ph7_context sCtx;` |
|        - |  6085 | `		ph7_value sRet;` |
|        - |  6086 | `		/* Look for an installed foreign function.` |
|        - |  6087 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6088 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6089 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6090 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6091 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6092 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   545404 |  6093 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   545404 |  6094 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6095 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6096 | `			const char *zShort = sName.zString;` |
|        - |  6097 | `			sxu32 i;` |
|      217 |  6098 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6099 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6100 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6101 | `				}` |
|      102 |  6102 | `			}` |
|       15 |  6103 | `			if( zShort != sName.zString ){` |
|       15 |  6104 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6105 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6106 | `			}` |
|        7 |  6107 | `		}` |
|   545404 |  6108 | `		if( pEntry == 0 ){` |
|        - |  6109 | `			/* Call to undefined function */` |
|        5 |  6110 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6111 | `			/* Pop given arguments */` |
|        5 |  6112 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6113 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6114 | `			}` |
|        - |  6115 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6116 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6117 | `			break;` |
|        - |  6118 | `		}` |
|   545400 |  6119 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6120 | `		/* Start collecting function arguments */` |
|   545400 |  6121 | `		SySetReset(&aArg);` |
|  1466002 |  6122 | `		while( pArg < pTos ){` |
|   920604 |  6123 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   920604 |  6124 | `			pArg++;` |
|        2 |  6125 | `		}` |
|        - |  6126 | `		/* Assume a null return value */` |
|   545400 |  6127 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6128 | `		/* Init the call context */` |
|   545400 |  6129 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6130 | `		/* Call the foreign function */` |
|   545400 |  6131 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6132 | `		/* Release the call context */` |
|   545400 |  6133 | `		VmReleaseCallContext(&sCtx);` |
|   545400 |  6134 | `		if( rc == PH7_ABORT ){` |
|      463 |  6135 | `			goto Abort;` |
|   544938 |  6136 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6137 | `			VmFrame *pFrm = pVm->pFrame;` |
|       13 |  6138 | `			while( pFrm->pParent && (pFrm->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  6139 | `				pFrm = pFrm->pParent;` |
|        1 |  6140 | `			}` |
|        7 |  6141 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6142 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6143 | `				goto Exception;` |
|        - |  6144 | `			}` |
|        - |  6145 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6146 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6147 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6148 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6149 | `			}` |
|        - |  6150 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6151 | `			VmPopOperand(&pTos,1);` |
|        - |  6152 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6153 | `			pFrm = pVm->pFrame;` |
|        7 |  6154 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6155 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6156 | `			}` |
|        7 |  6157 | `			break;` |
|        - |  6158 | `		}` |
|   544932 |  6159 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6160 | `			/* Pop function name and arguments */` |
|   527638 |  6161 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   263840 |  6162 | `		}` |
|        - |  6163 | `		/* Save foreign function return value */` |
|   544932 |  6164 | `		PH7_MemObjStore(&sRet,pTos);` |
|   544932 |  6165 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6166 | `	}` |
|   557236 |  6167 | `	break;` |
|        - |  6168 | `				  }` |
|        - |  6169 | `/*` |
|        - |  6170 | ` * OP_CONSUME: P1 * *` |
|        - |  6171 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6172 | ` */` |
|    10838 |  6173 | `case PH7_OP_CONSUME: {` |
|    21678 |  6174 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    21678 |  6175 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6176 |  |
|    21678 |  6177 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    21678 |  6178 | `	pCur = pOut;` |
|        - |  6179 | `	/* Start the consume process  */` |
|    43354 |  6180 | `	while( pOut <= pTos ){` |
|        - |  6181 | `		/* Force a string cast */` |
|    21678 |  6182 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6183 | `			PH7_MemObjToString(pOut);` |
|      149 |  6184 | `		}` |
|    21678 |  6185 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6186 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6187 | `			/* Invoke the output consumer callback */` |
|    11902 |  6188 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    11902 |  6189 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6190 | `				/* Increment output length */` |
|     5456 |  6191 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2727 |  6192 | `			}` |
|    11902 |  6193 | `			SyBlobRelease(&pOut->sBlob);` |
|    11902 |  6194 | `			if( rc == SXERR_ABORT ){` |
|        - |  6195 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6196 | `				goto Abort;` |
|        - |  6197 | `			}` |
|     5950 |  6198 | `		}` |
|    21678 |  6199 | `		pOut++;` |
|        2 |  6200 | `	}` |
|    21678 |  6201 | `	pTos = &pCur[-1];` |
|    21676 |  6202 | `	break;` |
|        - |  6203 | `					 }` |
|        - |  6204 |  |
|        - |  6205 | `		} /* Switch() */` |
|  9616376 |  6206 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6207 | `	} /* For(;;) */` |
|    15129 |  6208 | `Done:` |
|    30260 |  6209 | `	SySetRelease(&aArg);` |
|    30260 |  6210 | `	return SXRET_OK;` |
|      238 |  6211 | `Abort:` |
|      477 |  6212 | `	SySetRelease(&aArg);` |
|     1661 |  6213 | `	while( pTos >= pStack ){` |
|     1185 |  6214 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6215 | `		pTos--;` |
|        1 |  6216 | `	}` |
|      477 |  6217 | `	return PH7_ABORT;` |
|        1 |  6218 | `Exception:` |
|        3 |  6219 | `	SySetRelease(&aArg);` |
|        5 |  6220 | `	while( pTos >= pStack ){` |
|        3 |  6221 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6222 | `		pTos--;` |
|        1 |  6223 | `	}` |
|        3 |  6224 | `	return PH7_EXCEPTION;` |
|    15370 |  6225 |  |
|        - |  6226 | `/*` |
|        - |  6227 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6228 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6229 | ` * See block-comment on that function for additional information.` |
|        - |  6230 | ` */` |
|    14490 |  6231 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6232 |  |
|        - |  6233 | `	ph7_value *pStack;` |
|        - |  6234 | `	sxi32 rc;` |
|        - |  6235 | `	/* Allocate a new operand stack */` |
|    14492 |  6236 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14492 |  6237 | `	if( pStack == 0 ){` |
|      ! 0 |  6238 | `		return SXERR_MEM;` |
|        - |  6239 | `	}` |
|        - |  6240 | `	/* Execute the program */` |
|    14492 |  6241 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6242 | `	/* Free the operand stack */` |
|    14492 |  6243 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6244 | `	/* Execution result */` |
|    14492 |  6245 | `	return rc;` |
|     7247 |  6246 |  |
|        - |  6247 | `/*` |
|        - |  6248 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6249 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6250 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6251 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6252 | ` * execution ends.` |
|        - |  6253 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6254 | ` * additional information.` |
|        - |  6255 | ` */` |
|     2228 |  6256 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6257 |  |
|        - |  6258 | `	VmShutdownCB *pEntry;` |
|        - |  6259 | `	ph7_value *apArg[10];` |
|        - |  6260 | `	sxu32 n,nEntry;` |
|        - |  6261 | `	int i;` |
|        - |  6262 | `	/* Point to the stack of registered callbacks */` |
|     2230 |  6263 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    24510 |  6264 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    22282 |  6265 | `		apArg[i] = 0;` |
|    11142 |  6266 | `	}` |
|     2232 |  6267 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6268 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6269 | `		if( pEntry ){` |
|        - |  6270 | `			/* Prepare callback arguments if any */` |
|        3 |  6271 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6272 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6273 | `					break;` |
|        - |  6274 | `				}` |
|      ! 0 |  6275 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6276 | `			}` |
|        - |  6277 | `			/* Invoke the callback */` |
|        3 |  6278 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6279 | `			/*` |
|        - |  6280 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6281 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6282 | `			 */` |
|        3 |  6283 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6284 | `			if( pEntry ){` |
|        3 |  6285 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6286 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6287 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6288 | `				}` |
|        1 |  6289 | `			}` |
|        1 |  6290 | `		}` |
|        2 |  6291 | `	}` |
|     2230 |  6292 | `	SySetReset(&pVm->aShutdown);` |
|     2230 |  6293 |  |
|        - |  6294 | `/*` |
|        - |  6295 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6296 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6297 | ` * See block-comment on that function for additional information.` |
|        - |  6298 | ` */` |
|     2236 |  6299 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6300 |  |
|        - |  6301 | `	/* Make sure we are ready to execute this program */` |
|     2238 |  6302 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6303 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6304 | `	}` |
|        - |  6305 | `	/* Set the execution magic number  */` |
|     2238 |  6306 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6307 | `	/* Execute the program */` |
|     2238 |  6308 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6309 | `	/* Invoke any shutdown callbacks */` |
|     2234 |  6310 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6311 | `	/*` |
|        - |  6312 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6313 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6314 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6315 | `	 */` |
|     2234 |  6316 | `	return SXRET_OK;` |
|     1120 |  6317 |  |
|        - |  6318 | `/*` |
|        - |  6319 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6320 | ` * the desired message.` |
|        - |  6321 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6322 | ` * in 'api.c' for additional information.` |
|        - |  6323 | ` */` |
|      350 |  6324 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6325 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6326 | `	SyString *pString /* Message to output */` |
|        - |  6327 | `	)` |
|        2 |  6328 |  |
|      352 |  6329 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6330 | `	sxi32 rc = SXRET_OK;` |
|        - |  6331 | `	/* Call the output consumer */` |
|      352 |  6332 | `	if( pString->nByte > 0 ){` |
|      352 |  6333 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6334 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6335 | `			/* Increment output length */` |
|       17 |  6336 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6337 | `		}` |
|      175 |  6338 | `	}` |
|      352 |  6339 | `	return rc;` |
|        2 |  6340 |  |
|        - |  6341 | `/*` |
|        - |  6342 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6343 | ` * callback to consume the formatted message.` |
|        - |  6344 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6345 | ` * in 'api.c' for additional information.` |
|        - |  6346 | ` */` |
|        2 |  6347 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6348 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6349 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6350 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6351 | `	)` |
|        1 |  6352 |  |
|        3 |  6353 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6354 | `	sxi32 rc = SXRET_OK;` |
|        - |  6355 | `	SyBlob sWorker;` |
|        - |  6356 | `	/* Format the message and call the output consumer */` |
|        3 |  6357 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6358 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6359 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6360 | `		/* Consume the formatted message */` |
|        3 |  6361 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6362 | `	}` |
|        3 |  6363 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6364 | `		/* Increment output length */` |
|      ! 0 |  6365 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6366 | `	}` |
|        - |  6367 | `	/* Release the working buffer */` |
|        3 |  6368 | `	SyBlobRelease(&sWorker);` |
|        3 |  6369 | `	return rc;` |
|        1 |  6370 |  |
|        - |  6371 | `/*` |
|        - |  6372 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6373 | ` * This function never fail and always return a pointer` |
|        - |  6374 | ` * to a null terminated string.` |
|        - |  6375 | ` */` |
|       12 |  6376 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6377 |  |
|       13 |  6378 | `	const char *zOp = "Unknown     ";` |
|       13 |  6379 | `	switch(nOp){` |
|        3 |  6380 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6381 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6382 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6383 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6384 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6385 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6386 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6387 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6388 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6389 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6390 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6391 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6392 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6393 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6394 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6395 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6396 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6397 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6398 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6399 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6400 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6401 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6402 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6403 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6404 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6405 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6406 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6407 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6408 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6409 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6410 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6411 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6412 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6413 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6414 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6415 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6416 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6417 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6418 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6419 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6420 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6421 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6422 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6423 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6424 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6425 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6426 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6427 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6428 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6429 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  6430 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6431 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6432 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6433 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6434 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6435 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6436 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6437 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6438 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6439 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6440 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6441 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6442 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6443 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6444 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6445 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6446 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6447 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6448 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6449 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6450 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6451 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6452 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6453 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6454 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6455 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6456 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6457 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6458 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6459 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6460 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6461 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6462 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6463 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6464 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6465 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6466 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6467 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6468 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6469 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6470 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6471 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6472 | `	default:` |
|      ! 0 |  6473 | `		break;` |
|        - |  6474 | `	}` |
|       13 |  6475 | `	return zOp;` |
|        1 |  6476 |  |
|        - |  6477 | `/*` |
|        - |  6478 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6479 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6480 | ` * is responsible of consuming the generated dump.` |
|        - |  6481 | ` */` |
|        2 |  6482 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6483 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6484 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6485 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6486 | `	)` |
|        1 |  6487 |  |
|        - |  6488 | `	sxi32 rc;` |
|        3 |  6489 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6490 | `	return rc;` |
|        1 |  6491 |  |
|        - |  6492 | `/*` |
|        - |  6493 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6494 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6495 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6496 | ` * in 'compile.c' for additional information.` |
|        - |  6497 | ` */` |
|        8 |  6498 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6499 |  |
|        9 |  6500 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6501 | `	/* Evaluate and expand constant value */` |
|        9 |  6502 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6503 |  |
|        - |  6504 | `/*` |
|        - |  6505 | ` * Section:` |
|        - |  6506 | ` *  Function handling functions.` |
|        - |  6507 | ` * Status:` |
|        - |  6508 | ` *    Stable.` |
|        - |  6509 | ` */` |
|        - |  6510 | `/*` |
|        - |  6511 | ` * int func_num_args(void)` |
|        - |  6512 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6513 | ` * Parameters` |
|        - |  6514 | ` *   None.` |
|        - |  6515 | ` * Return` |
|        - |  6516 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6517 | ` *  or -1 if called from the globe scope.` |
|        - |  6518 | ` */` |
|      906 |  6519 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6520 |  |
|        - |  6521 | `	VmFrame *pFrame;` |
|        - |  6522 | `	ph7_vm *pVm;` |
|        - |  6523 | `	/* Point to the target VM */` |
|      908 |  6524 | `	pVm = pCtx->pVm;` |
|        - |  6525 | `	/* Current frame */` |
|      908 |  6526 | `	pFrame = pVm->pFrame;` |
|      908 |  6527 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6528 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6529 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6530 | `	}` |
|      908 |  6531 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6532 | `		SXUNUSED(nArg);` |
|      ! 0 |  6533 | `		SXUNUSED(apArg);` |
|        - |  6534 | `		/* Global frame,return -1 */` |
|      ! 0 |  6535 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6536 | `		return SXRET_OK;` |
|        - |  6537 | `	}` |
|        - |  6538 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6539 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6540 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6541 | `	return SXRET_OK;` |
|      455 |  6542 |  |
|        - |  6543 | `/*` |
|        - |  6544 | ` * value func_get_arg(int $arg_num)` |
|        - |  6545 | ` *   Return an item from the argument list.` |
|        - |  6546 | ` * Parameters` |
|        - |  6547 | ` *  Argument number(index start from zero).` |
|        - |  6548 | ` * Return` |
|        - |  6549 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6550 | ` */` |
|       22 |  6551 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6552 |  |
|       24 |  6553 | `	ph7_value *pObj = 0;` |
|       24 |  6554 | `	VmSlot *pSlot = 0;` |
|        - |  6555 | `	VmFrame *pFrame;` |
|        - |  6556 | `	ph7_vm *pVm;` |
|        - |  6557 | `	/* Point to the target VM */` |
|       24 |  6558 | `	pVm = pCtx->pVm;` |
|        - |  6559 | `	/* Current frame */` |
|       24 |  6560 | `	pFrame = pVm->pFrame;` |
|       24 |  6561 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6562 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6563 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6564 | `	}` |
|       24 |  6565 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6566 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6567 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6568 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6569 | `		return SXRET_OK;` |
|        - |  6570 | `	}` |
|        - |  6571 | `	/* Extract the desired index */` |
|       21 |  6572 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6573 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6574 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6575 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6576 | `		return SXRET_OK;` |
|        - |  6577 | `	}` |
|        - |  6578 | `	/* Extract the desired argument */` |
|       21 |  6579 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6580 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6581 | `			/* Return the desired argument */` |
|       21 |  6582 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6583 | `		}else{` |
|        - |  6584 | `			/* No such argument,return false */` |
|      ! 0 |  6585 | `			ph7_result_bool(pCtx,0);` |
|        - |  6586 | `		}` |
|       11 |  6587 | `	}else{` |
|        - |  6588 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6589 | `		ph7_result_bool(pCtx,0);` |
|        - |  6590 | `	}` |
|       21 |  6591 | `	return SXRET_OK;` |
|       13 |  6592 |  |
|        - |  6593 | `/*` |
|        - |  6594 | ` * array func_get_args_byref(void)` |
|        - |  6595 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6596 | ` * Parameters` |
|        - |  6597 | ` *  None.` |
|        - |  6598 | ` * Return` |
|        - |  6599 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6600 | ` *  member of the current user-defined function's argument list.` |
|        - |  6601 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6602 | ` * NOTE:` |
|        - |  6603 | ` *  Arguments are returned to the array by reference.` |
|        - |  6604 | ` */` |
|        2 |  6605 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6606 |  |
|        - |  6607 | `	ph7_value *pArray;` |
|        - |  6608 | `	VmFrame *pFrame;` |
|        - |  6609 | `	VmSlot *aSlot;` |
|        - |  6610 | `	sxu32 n;` |
|        - |  6611 | `	/* Point to the current frame */` |
|        3 |  6612 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6613 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6614 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6615 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6616 | `	}` |
|        3 |  6617 | `	if( pFrame->pParent == 0 ){` |
|        - |  6618 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6619 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6620 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6621 | `		return SXRET_OK;` |
|        - |  6622 | `	}` |
|        - |  6623 | `	/* Create a new array */` |
|        3 |  6624 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6625 | `	if( pArray == 0 ){` |
|      ! 0 |  6626 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6627 | `		SXUNUSED(apArg);` |
|      ! 0 |  6628 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6629 | `		return SXRET_OK;` |
|        - |  6630 | `	}` |
|        - |  6631 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6632 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6633 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6634 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6635 | `	}` |
|        - |  6636 | `	/* Return the freshly created array */` |
|        3 |  6637 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6638 | `	return SXRET_OK;` |
|        2 |  6639 |  |
|        - |  6640 | `/*` |
|        - |  6641 | ` * array func_get_args(void)` |
|        - |  6642 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6643 | ` * Parameters` |
|        - |  6644 | ` *  None.` |
|        - |  6645 | ` * Return` |
|        - |  6646 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6647 | ` *  member of the current user-defined function's argument list.` |
|        - |  6648 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6649 | ` */` |
|       62 |  6650 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6651 |  |
|       64 |  6652 | `	ph7_value *pObj = 0;` |
|        - |  6653 | `	ph7_value *pArray;` |
|        - |  6654 | `	VmFrame *pFrame;` |
|        - |  6655 | `	VmSlot *aSlot;` |
|        - |  6656 | `	sxu32 n;` |
|        - |  6657 | `	/* Point to the current frame */` |
|       64 |  6658 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6659 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6660 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6661 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6662 | `	}` |
|       64 |  6663 | `	if( pFrame->pParent == 0 ){` |
|        - |  6664 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6665 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6666 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6667 | `		return SXRET_OK;` |
|        - |  6668 | `	}` |
|        - |  6669 | `	/* Create a new array */` |
|       64 |  6670 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6671 | `	if( pArray == 0 ){` |
|      ! 0 |  6672 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6673 | `		SXUNUSED(apArg);` |
|      ! 0 |  6674 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6675 | `		return SXRET_OK;` |
|        - |  6676 | `	}` |
|        - |  6677 | `	/* Start filling the array with the given arguments */` |
|       64 |  6678 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6679 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6680 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6681 | `		if( pObj ){` |
|      130 |  6682 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6683 | `		}` |
|       66 |  6684 | `	}` |
|        - |  6685 | `	/* Return the freshly created array */` |
|       64 |  6686 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6687 | `	return SXRET_OK;` |
|       33 |  6688 |  |
|        - |  6689 | `/*` |
|        - |  6690 | ` * bool function_exists(string $name)` |
|        - |  6691 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6692 | ` * Parameters` |
|        - |  6693 | ` *  The name of the desired function.` |
|        - |  6694 | ` * Return` |
|        - |  6695 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6696 | ` */` |
|     1644 |  6697 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6698 |  |
|        - |  6699 | `	const char *zName;` |
|        - |  6700 | `	ph7_vm *pVm;` |
|        - |  6701 | `	int nLen;` |
|        - |  6702 | `	int res;` |
|     1646 |  6703 | `	if( nArg < 1 ){` |
|        - |  6704 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6705 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6706 | `		return SXRET_OK;` |
|        - |  6707 | `	}` |
|        - |  6708 | `	/* Point to the target VM */` |
|     1646 |  6709 | `	pVm = pCtx->pVm;` |
|        - |  6710 | `	/* Extract the function name */` |
|     1646 |  6711 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6712 | `	/* Assume the function is not defined */` |
|     1646 |  6713 | `	res = 0;` |
|        - |  6714 | `	/* Perform the lookup */` |
|     2466 |  6715 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1640 |  6716 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6717 | `			/* Function is defined */` |
|      206 |  6718 | `			res = 1;` |
|      102 |  6719 | `	}` |
|     1646 |  6720 | `	ph7_result_bool(pCtx,res);` |
|     1646 |  6721 | `	return SXRET_OK;` |
|      824 |  6722 |  |
|        - |  6723 | `/*` |
|        - |  6724 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6725 | ` * [i.e: Whether it is callable or not].` |
|        - |  6726 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6727 | ` */` |
|    16002 |  6728 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6729 |  |
|    16004 |  6730 | `	int res = 0;` |
|    16004 |  6731 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6732 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6733 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6734 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6735 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6736 | `		if( pMethod && CallInvoke ){` |
|        - |  6737 | `			ph7_value sResult;` |
|        - |  6738 | `			sxi32 rc;` |
|        - |  6739 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6740 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6741 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6742 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6743 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6744 | `			}` |
|      ! 0 |  6745 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6746 | `		}` |
|    16004 |  6747 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6748 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6749 | `		if( pMap->nEntry == 2 ){` |
|        - |  6750 | `			ph7_class *pClass;` |
|        - |  6751 | `			ph7_value *pV;` |
|        - |  6752 | `			/* Extract the target class */` |
|       12 |  6753 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6754 | `			if( pV ){` |
|       12 |  6755 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6756 | `				if( pClass ){` |
|        - |  6757 | `					ph7_class_method *pMethod;` |
|        - |  6758 | `					/* Extract the target method */` |
|       10 |  6759 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6760 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6761 | `						/* Perform the lookup */` |
|       10 |  6762 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6763 | `						if( pMethod ){` |
|        - |  6764 | `							/* Method is callable */` |
|        5 |  6765 | `							res = 1;` |
|        2 |  6766 | `						}` |
|        4 |  6767 | `					}` |
|        4 |  6768 | `				}` |
|        5 |  6769 | `			}` |
|        7 |  6770 | `		}` |
|    15991 |  6771 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6772 | `		const char *zName;` |
|        - |  6773 | `		int nLen;` |
|        - |  6774 | `		/* Extract the name */` |
|     4700 |  6775 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6776 | `		/* Perform the lookup */` |
|     4715 |  6777 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6778 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6779 | `				/* Function is callable */` |
|     4682 |  6780 | `				res = 1;` |
|     2340 |  6781 | `		}` |
|     2349 |  6782 | `	}` |
|    16004 |  6783 | `	return res;` |
|        2 |  6784 |  |
|        - |  6785 | `/*` |
|        - |  6786 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6787 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6788 | ` * Parameters` |
|        - |  6789 | ` * $name` |
|        - |  6790 | ` *    The callback function to check` |
|        - |  6791 | ` * $syntax_only` |
|        - |  6792 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6793 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6794 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6795 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6796 | ` *    a string.` |
|        - |  6797 | ` * Return` |
|        - |  6798 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6799 | ` */` |
|       14 |  6800 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6801 |  |
|        - |  6802 | `	ph7_vm *pVm;` |
|        - |  6803 | `	int res;` |
|       15 |  6804 | `	if( nArg < 1 ){` |
|        - |  6805 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6806 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6807 | `		return SXRET_OK;` |
|        - |  6808 | `	}` |
|        - |  6809 | `	/* Point to the target VM */` |
|       15 |  6810 | `	pVm = pCtx->pVm;` |
|        - |  6811 | `	/* Perform the requested operation */` |
|       15 |  6812 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6813 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6814 | `	return SXRET_OK;` |
|        8 |  6815 |  |
|        - |  6816 | `/*` |
|        - |  6817 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6818 | ` * defined below.` |
|        - |  6819 | ` */` |
|     1082 |  6820 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6821 |  |
|     1083 |  6822 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6823 | `	ph7_value sName;` |
|        - |  6824 | `	sxi32 rc;` |
|        - |  6825 | `	/* Prepare the function name for insertion */` |
|     1083 |  6826 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6827 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6828 | `	/* Perform the insertion */` |
|     1083 |  6829 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6830 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6831 | `	return rc;` |
|        1 |  6832 |  |
|        - |  6833 | `/*` |
|        - |  6834 | ` * array get_defined_functions(void)` |
|        - |  6835 | ` *  Returns an array of all defined functions.` |
|        - |  6836 | ` * Parameter` |
|        - |  6837 | ` *  None.` |
|        - |  6838 | ` * Return` |
|        - |  6839 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6840 | ` *  both built-in (internal) and user-defined.` |
|        - |  6841 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6842 | ` *  defined ones using $arr["user"].` |
|        - |  6843 | ` * Note:` |
|        - |  6844 | ` *  NULL is returned on failure.` |
|        - |  6845 | ` */` |
|        2 |  6846 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6847 |  |
|        - |  6848 | `	ph7_value *pArray,*pEntry;` |
|        - |  6849 | `	/* NOTE:` |
|        - |  6850 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6851 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6852 | `	 */` |
|        3 |  6853 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6854 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6855 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6856 | `		SXUNUSED(apArg);` |
|        - |  6857 | `		/* Return NULL */` |
|      ! 0 |  6858 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6859 | `		return SXRET_OK;` |
|        - |  6860 | `	}` |
|        3 |  6861 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6862 | `	if( pEntry == 0 ){` |
|        - |  6863 | `		/* Return NULL */` |
|      ! 0 |  6864 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6865 | `		return SXRET_OK;` |
|        - |  6866 | `	}` |
|        - |  6867 | `	/* Fill with the appropriate information */` |
|        3 |  6868 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6869 | `	/* Create the 'internal' index */` |
|        3 |  6870 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6871 | `	/* Create the user-func array */` |
|        3 |  6872 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6873 | `	if( pEntry == 0 ){` |
|        - |  6874 | `		/* Return NULL */` |
|      ! 0 |  6875 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6876 | `		return SXRET_OK;` |
|        - |  6877 | `	}` |
|        - |  6878 | `	/* Fill with the appropriate information */` |
|        3 |  6879 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6880 | `	/* Create the 'user' index */` |
|        3 |  6881 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6882 | `	/* Return the multi-dimensional array */` |
|        3 |  6883 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6884 | `	return SXRET_OK;` |
|        2 |  6885 |  |
|        - |  6886 | `/*` |
|        - |  6887 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6888 | ` *  Register a function for execution on shutdown.` |
|        - |  6889 | ` * Note` |
|        - |  6890 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6891 | ` *  be called in the same order as they were registered.` |
|        - |  6892 | ` * Parameters` |
|        - |  6893 | ` *  $callback` |
|        - |  6894 | ` *   The shutdown callback to register.` |
|        - |  6895 | ` * $param` |
|        - |  6896 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6897 | ` * Return` |
|        - |  6898 | ` *  Nothing.` |
|        - |  6899 | ` */` |
|        2 |  6900 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6901 |  |
|        - |  6902 | `	VmShutdownCB sEntry;` |
|        - |  6903 | `	int i,j;` |
|        3 |  6904 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6905 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6906 | `		return PH7_OK;` |
|        - |  6907 | `	}` |
|        - |  6908 | `	/* Zero the Entry */` |
|        3 |  6909 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6910 | `	/* Initialize fields */` |
|        3 |  6911 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6912 | `	/* Save the callback name for later invocation name */` |
|        3 |  6913 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6914 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6915 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6916 | `	}` |
|        - |  6917 | `	/* Copy arguments */` |
|        3 |  6918 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6919 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6920 | `			/* Limit reached */` |
|      ! 0 |  6921 | `			break;` |
|        - |  6922 | `		}` |
|      ! 0 |  6923 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6924 | `	}` |
|        3 |  6925 | `	sEntry.nArg = j;` |
|        - |  6926 | `	/* Install the callback */` |
|        3 |  6927 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6928 | `	return PH7_OK;` |
|        2 |  6929 |  |
|        - |  6930 | `/*` |
|        - |  6931 | ` * Section:` |
|        - |  6932 | ` *  Class handling functions.` |
|        - |  6933 | ` * Status:` |
|        - |  6934 | ` *    Stable.` |
|        - |  6935 | ` */` |
|        - |  6936 | `/*` |
|        - |  6937 | ` * Extract the top active class. NULL is returned` |
|        - |  6938 | ` * if the class stack is empty.` |
|        - |  6939 | ` */` |
|      536 |  6940 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6941 |  |
|      538 |  6942 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6943 | `	ph7_class **apClass;` |
|      538 |  6944 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6945 | `		/* Empty stack,return NULL */` |
|       15 |  6946 | `		return 0;` |
|        - |  6947 | `	}` |
|        - |  6948 | `	/* Peek the last entry */` |
|      524 |  6949 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      524 |  6950 | `	return apClass[pSet->nUsed - 1];` |
|      270 |  6951 |  |
|        - |  6952 | `/*` |
|        - |  6953 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6954 | ` *   Get the class that declared the currently executing method.` |
|        - |  6955 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6956 | ` *` |
|        - |  6957 | ` * Parameters` |
|        - |  6958 | ` *   pVm: Target VM` |
|        - |  6959 | ` *` |
|        - |  6960 | ` * Return` |
|        - |  6961 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6962 | ` *   - Not executing within a class method` |
|        - |  6963 | ` *` |
|        - |  6964 | ` * Note` |
|        - |  6965 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6966 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6967 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6968 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6969 | ` *   declaring class.` |
|        - |  6970 | ` */` |
|       48 |  6971 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  6972 |  |
|       50 |  6973 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6974 | `	ph7_vm_func *pVmFunc;` |
|        - |  6975 |  |
|        - |  6976 | `	/* Skip exception frames to find the actual method frame */` |
|       50 |  6977 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6978 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6979 | `	}` |
|        - |  6980 |  |
|        - |  6981 | `	/* Check if we're in a method context */` |
|       50 |  6982 | `	if( pFrame->pParent ){` |
|       46 |  6983 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       46 |  6984 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6985 | `			/* Return the declaring class */` |
|       46 |  6986 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6987 | `		}` |
|      ! 0 |  6988 | `	}` |
|        - |  6989 |  |
|        5 |  6990 | `	return 0;` |
|       26 |  6991 |  |
|        - |  6992 |  |
|        - |  6993 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6994 | `/*` |
|        - |  6995 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6996 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6997 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6998 | ` * return value indicates failure.` |
|        - |  6999 | ` */` |
|     1260 |  7000 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7001 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7002 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7003 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7004 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7005 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7006 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7007 | `	)` |
|        2 |  7008 |  |
|        - |  7009 | `	ph7_value *aStack;` |
|        - |  7010 | `	VmInstr aInstr[2];` |
|        - |  7011 | `	int iCursor;` |
|        - |  7012 | `	int i;` |
|        - |  7013 | `	/* Create a new operand stack */` |
|     1262 |  7014 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1262 |  7015 | `	if( aStack == 0 ){` |
|      ! 0 |  7016 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7017 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7018 | `		return SXERR_MEM;` |
|        - |  7019 | `	}` |
|        - |  7020 | `	/* Fill the operand stack with the given arguments */` |
|     1828 |  7021 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      568 |  7022 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7023 | `		/*` |
|        - |  7024 | `		 * Symisc eXtension:` |
|        - |  7025 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7026 | `		 */` |
|      568 |  7027 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      285 |  7028 | `	}` |
|     1262 |  7029 | `	iCursor = nArg + 1;` |
|     1262 |  7030 | `	if( pThis ){` |
|        - |  7031 | `		/*` |
|        - |  7032 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7033 | `		 */` |
|     1256 |  7034 | `		pThis->iRef++; /* Increment reference count */` |
|     1256 |  7035 | `		aStack[i].x.pOther = pThis;` |
|     1256 |  7036 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      627 |  7037 | `	}` |
|     1262 |  7038 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1262 |  7039 | `	i++;` |
|        - |  7040 | `	/* Push method name */` |
|     1262 |  7041 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1262 |  7042 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1262 |  7043 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1262 |  7044 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7045 | `	/* Emit the CALL istruction */` |
|     1262 |  7046 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1262 |  7047 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1262 |  7048 | `	aInstr[0].iP2 = 0;` |
|     1262 |  7049 | `	aInstr[0].p3  = 0;` |
|        - |  7050 | `	/* Emit the DONE instruction */` |
|     1262 |  7051 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1262 |  7052 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1262 |  7053 | `	aInstr[1].iP2 = 0;` |
|     1262 |  7054 | `	aInstr[1].p3  = 0;` |
|        - |  7055 | `	/* Execute the method body (if available) */` |
|     1262 |  7056 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7057 | `	/* Clean up the mess left behind */` |
|     1262 |  7058 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1262 |  7059 | `	return PH7_OK;` |
|      632 |  7060 |  |
|        - |  7061 | `/*` |
|        - |  7062 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7063 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7064 | ` * in the apArg[] array.` |
|        - |  7065 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7066 | ` * return value indicates failure.` |
|        - |  7067 | ` */` |
|      926 |  7068 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7069 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7070 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7071 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7072 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7073 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7074 | `	)` |
|        2 |  7075 |  |
|        - |  7076 | `	ph7_value *aStack;` |
|        - |  7077 | `	VmInstr aInstr[2];` |
|        - |  7078 | `	int i;` |
|      928 |  7079 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7080 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  7081 | `		if( pResult ){` |
|        - |  7082 | `			/* Assume a null return value */` |
|      ! 0 |  7083 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7084 | `		}` |
|      471 |  7085 | `		return SXERR_INVALID;` |
|        - |  7086 | `	}` |
|      458 |  7087 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7088 | `		/* Class method */` |
|       11 |  7089 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7090 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7091 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7092 | `		ph7_class *pClass = 0;` |
|        - |  7093 | `		ph7_value *pValue;` |
|        - |  7094 | `		sxi32 rc;` |
|       11 |  7095 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7096 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7097 | `			if( pResult ){` |
|        - |  7098 | `				/* Assume a null return value */` |
|      ! 0 |  7099 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7100 | `			}` |
|      ! 0 |  7101 | `			return SXRET_OK;` |
|        - |  7102 | `		}` |
|        - |  7103 | `		/* Extract the class name or an instance of it */` |
|       11 |  7104 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7105 | `		if( pValue ){` |
|       11 |  7106 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7107 | `		}` |
|       11 |  7108 | `		if( pClass == 0 ){` |
|        - |  7109 | `			/* No such class,return NULL */` |
|      ! 0 |  7110 | `			if( pResult ){` |
|      ! 0 |  7111 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7112 | `			}` |
|      ! 0 |  7113 | `			return SXRET_OK;` |
|        - |  7114 | `		}` |
|       11 |  7115 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7116 | `			/* Point to the class instance */` |
|        5 |  7117 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7118 | `		}` |
|        - |  7119 | `		/* Try to extract the method */` |
|       11 |  7120 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7121 | `		if( pValue ){` |
|       11 |  7122 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7123 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7124 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7125 | `			}` |
|        5 |  7126 | `		}` |
|       11 |  7127 | `		if( pMethod == 0 ){` |
|        - |  7128 | `			/* No such method,return NULL */` |
|      ! 0 |  7129 | `			if( pResult ){` |
|      ! 0 |  7130 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7131 | `			}` |
|      ! 0 |  7132 | `			return SXRET_OK;` |
|        - |  7133 | `		}` |
|        - |  7134 | `		/* Call the class method */` |
|       11 |  7135 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7136 | `		return rc;` |
|        - |  7137 | `	}` |
|        - |  7138 | `	/* Create a new operand stack */` |
|      448 |  7139 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7140 | `	if( aStack == 0 ){` |
|      ! 0 |  7141 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7142 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7143 | `		if( pResult ){` |
|        - |  7144 | `			/* Assume a null return value */` |
|      ! 0 |  7145 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7146 | `		}` |
|      ! 0 |  7147 | `		return SXERR_MEM;` |
|        - |  7148 | `	}` |
|        - |  7149 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7150 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7151 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7152 | `		/*` |
|        - |  7153 | `		 * Symisc eXtension:` |
|        - |  7154 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7155 | `		 */` |
|     1024 |  7156 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7157 | `	}` |
|        - |  7158 | `	/* Push the function name */` |
|      448 |  7159 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7160 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7161 | `	/* Emit the CALL istruction */` |
|      448 |  7162 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7163 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7164 | `	aInstr[0].iP2 = 0;` |
|      448 |  7165 | `	aInstr[0].p3  = 0;` |
|        - |  7166 | `	/* Emit the DONE instruction */` |
|      448 |  7167 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7168 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7169 | `	aInstr[1].iP2 = 0;` |
|      448 |  7170 | `	aInstr[1].p3  = 0;` |
|        - |  7171 | `	/* Execute the function body (if available) */` |
|      448 |  7172 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7173 | `	/* Clean up the mess left behind */` |
|      448 |  7174 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7175 | `	return PH7_OK;` |
|      465 |  7176 |  |
|        - |  7177 | `/*` |
|        - |  7178 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7179 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7180 | ` * parameter.` |
|        - |  7181 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7182 | ` * return value indicates failure.` |
|        - |  7183 | ` */` |
|      236 |  7184 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7185 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7186 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7187 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7188 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7189 | `	)` |
|        1 |  7190 |  |
|        - |  7191 | `	ph7_value *pArg;` |
|        - |  7192 | `	SySet aArg;` |
|        - |  7193 | `	va_list ap;` |
|        - |  7194 | `	sxi32 rc;` |
|      237 |  7195 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7196 | `	/* Copy arguments one after one */` |
|      237 |  7197 | `	va_start(ap,pResult);` |
|      393 |  7198 | `	for(;;){` |
|      787 |  7199 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7200 | `		if( pArg == 0 ){` |
|      237 |  7201 | `			break;` |
|        - |  7202 | `		}` |
|      551 |  7203 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7204 | `	}` |
|        - |  7205 | `	/* Call the core routine */` |
|      237 |  7206 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7207 | `	/* Cleanup */` |
|      237 |  7208 | `	SySetRelease(&aArg);` |
|      237 |  7209 | `	return rc;` |
|        1 |  7210 |  |
|        - |  7211 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7212 | `/*` |
|        - |  7213 | ` * bool defined(string $name)` |
|        - |  7214 | ` *  Checks whether a given named constant exists.` |
|        - |  7215 | ` * Parameter:` |
|        - |  7216 | ` *  Name of the desired constant.` |
|        - |  7217 | ` * Return` |
|        - |  7218 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7219 | ` */` |
|       14 |  7220 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7221 |  |
|        - |  7222 | `	const char *zName;` |
|       16 |  7223 | `	int nLen = 0;` |
|       16 |  7224 | `	int res = 0;` |
|       16 |  7225 | `	if( nArg < 1 ){` |
|        - |  7226 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7227 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7228 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7229 | `		return SXRET_OK;` |
|        - |  7230 | `	}` |
|        - |  7231 | `	/* Extract constant name */` |
|       16 |  7232 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7233 | `	/* Perform the lookup */` |
|       16 |  7234 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7235 | `		/* Already defined */` |
|       10 |  7236 | `		res = 1;` |
|        4 |  7237 | `	}` |
|       16 |  7238 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7239 | `	return SXRET_OK;` |
|        9 |  7240 |  |
|        - |  7241 | `/*` |
|        - |  7242 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7243 | ` * below.` |
|        - |  7244 | ` */` |
|        8 |  7245 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7246 |  |
|       10 |  7247 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7248 | `	/* Expand constant value */` |
|       10 |  7249 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7250 |  |
|        - |  7251 | `/*` |
|        - |  7252 | ` * bool define(string $constant_name,expression value)` |
|        - |  7253 | ` *  Defines a named constant at runtime.` |
|        - |  7254 | ` * Parameter:` |
|        - |  7255 | ` *  $constant_name` |
|        - |  7256 | ` *   The name of the constant` |
|        - |  7257 | ` *  $value` |
|        - |  7258 | ` *   Constant value` |
|        - |  7259 | ` * Return:` |
|        - |  7260 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7261 | ` */` |
|       10 |  7262 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7263 |  |
|        - |  7264 | `	const char *zName;  /* Constant name */` |
|        - |  7265 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7266 | `	int nLen = 0;       /* Name length */` |
|        - |  7267 | `	sxi32 rc;` |
|       12 |  7268 | `	if( nArg < 2 ){` |
|        - |  7269 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7270 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7271 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7272 | `		return SXRET_OK;` |
|        - |  7273 | `	}` |
|       12 |  7274 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7275 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7276 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7277 | `		return SXRET_OK;` |
|        - |  7278 | `	}` |
|        - |  7279 | `	/* Extract constant name */` |
|       12 |  7280 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7281 | `	if( nLen < 1 ){` |
|      ! 0 |  7282 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7283 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7284 | `		return SXRET_OK;` |
|        - |  7285 | `	}` |
|        - |  7286 | `	/* Duplicate constant value */` |
|       12 |  7287 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7288 | `	if( pValue == 0 ){` |
|      ! 0 |  7289 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7290 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7291 | `		return SXRET_OK;` |
|        - |  7292 | `	}` |
|        - |  7293 | `	/* Initialize the memory object */` |
|       12 |  7294 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7295 | `	/* Register the constant */` |
|       12 |  7296 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7297 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7298 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7299 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7300 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7301 | `		return SXRET_OK;` |
|        - |  7302 | `	}` |
|        - |  7303 | `	/* Duplicate constant value */` |
|       12 |  7304 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7305 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7306 | `		/* Lower case the constant name */` |
|      ! 0 |  7307 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7308 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7309 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7310 | `				/* UTF-8 stream */` |
|      ! 0 |  7311 | `				zCur++;` |
|      ! 0 |  7312 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7313 | `					zCur++;` |
|      ! 0 |  7314 | `				}` |
|      ! 0 |  7315 | `				continue;` |
|        - |  7316 | `			}` |
|      ! 0 |  7317 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7318 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7319 | `				zCur[0] = (char)c;` |
|      ! 0 |  7320 | `			}` |
|      ! 0 |  7321 | `			zCur++;` |
|      ! 0 |  7322 | `		}` |
|        - |  7323 | `		/* Finally,register the constant */` |
|      ! 0 |  7324 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7325 | `	}` |
|        - |  7326 | `	/* All done,return TRUE */` |
|       12 |  7327 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7328 | `	return SXRET_OK;` |
|        7 |  7329 |  |
|        - |  7330 | `/*` |
|        - |  7331 | ` * value constant(string $name)` |
|        - |  7332 | ` *  Returns the value of a constant` |
|        - |  7333 | ` * Parameter` |
|        - |  7334 | ` *  $name` |
|        - |  7335 | ` *    Name of the constant.` |
|        - |  7336 | ` * Return` |
|        - |  7337 | ` *  Constant value or NULL if not defined.` |
|        - |  7338 | ` */` |
|        8 |  7339 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7340 |  |
|        - |  7341 | `	SyHashEntry *pEntry;` |
|        - |  7342 | `	ph7_constant *pCons;` |
|        - |  7343 | `	const char *zName; /* Constant name */` |
|        - |  7344 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7345 | `	int nLen;` |
|       10 |  7346 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7347 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7348 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7349 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7350 | `		return SXRET_OK;` |
|        - |  7351 | `	}` |
|        - |  7352 | `	/* Extract the constant name */` |
|       10 |  7353 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7354 | `	/* Perform the query */` |
|       10 |  7355 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7356 | `	if( pEntry == 0 ){` |
|        3 |  7357 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7358 | `		ph7_result_null(pCtx);` |
|        3 |  7359 | `		return SXRET_OK;` |
|        - |  7360 | `	}` |
|        8 |  7361 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7362 | `	/* Point to the structure that describe the constant */` |
|        8 |  7363 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7364 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7365 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7366 | `	/* Return that value */` |
|        8 |  7367 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7368 | `	/* Cleanup */` |
|        8 |  7369 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7370 | `	return SXRET_OK;` |
|        6 |  7371 |  |
|        - |  7372 | `/*` |
|        - |  7373 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7374 | ` * defined below.` |
|        - |  7375 | ` */` |
|      416 |  7376 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7377 |  |
|      417 |  7378 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7379 | `	ph7_value sName;` |
|        - |  7380 | `	sxi32 rc;` |
|        - |  7381 | `	/* Prepare the constant name for insertion */` |
|      417 |  7382 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7383 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7384 | `	/* Perform the insertion */` |
|      417 |  7385 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7386 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7387 | `	return rc;` |
|        1 |  7388 |  |
|        - |  7389 | `/*` |
|        - |  7390 | ` * array get_defined_constants(void)` |
|        - |  7391 | ` *  Returns an associative array with the names of all defined` |
|        - |  7392 | ` *  constants.` |
|        - |  7393 | ` * Parameters` |
|        - |  7394 | ` *  NONE.` |
|        - |  7395 | ` * Returns` |
|        - |  7396 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7397 | ` */` |
|        2 |  7398 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7399 |  |
|        - |  7400 | `	ph7_value *pArray;` |
|        - |  7401 | `	/* Create the array first*/` |
|        3 |  7402 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7403 | `	if( pArray == 0 ){` |
|      ! 0 |  7404 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7405 | `		SXUNUSED(apArg);` |
|        - |  7406 | `		/* Return NULL */` |
|      ! 0 |  7407 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7408 | `		return SXRET_OK;` |
|        - |  7409 | `	}` |
|        - |  7410 | `	/* Fill the array with the defined constants */` |
|        3 |  7411 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7412 | `	/* Return the created array */` |
|        3 |  7413 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7414 | `	return SXRET_OK;` |
|        2 |  7415 |  |
|        - |  7416 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7417 | `/*` |
|        - |  7418 | ` * Section:` |
|        - |  7419 | ` *  Random numbers/string generators.` |
|        - |  7420 | ` * Status:` |
|        - |  7421 | ` *    Stable.` |
|        - |  7422 | ` */` |
|        - |  7423 | `/*` |
|        - |  7424 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7425 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7426 | ` * used by te SQLite3 library.` |
|        - |  7427 | ` */` |
|     2307 |  7428 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7429 |  |
|        - |  7430 | `	sxu32 iNum;` |
|     2309 |  7431 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2309 |  7432 | `	return iNum;` |
|        2 |  7433 |  |
|        - |  7434 | `/*` |
|        - |  7435 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7436 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7437 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7438 | ` * by te SQLite3 library.` |
|        - |  7439 | ` */` |
|    72626 |  7440 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7441 |  |
|        - |  7442 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7443 | `	int i;` |
|        - |  7444 | `	/* Generate a binary string first */` |
|    72628 |  7445 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7446 | `	/* Turn the binary string into english based alphabet */` |
|   799056 |  7447 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   726430 |  7448 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   363216 |  7449 | `	 }` |
|    72628 |  7450 |  |
|        - |  7451 | `/*` |
|        - |  7452 | ` * int rand()` |
|        - |  7453 | ` * int mt_rand()` |
|        - |  7454 | ` * int rand(int $min,int $max)` |
|        - |  7455 | ` * int mt_rand(int $min,int $max)` |
|        - |  7456 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7457 | ` * Parameter` |
|        - |  7458 | ` *  $min` |
|        - |  7459 | ` *    The lowest value to return (default: 0)` |
|        - |  7460 | ` *  $max` |
|        - |  7461 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7462 | ` * Return` |
|        - |  7463 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7464 | ` * Note:` |
|        - |  7465 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7466 | ` *  by te SQLite3 library.` |
|        - |  7467 | ` */` |
|       20 |  7468 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7469 |  |
|        - |  7470 | `	sxu32 iNum;` |
|        - |  7471 | `	/* Generate the random number */` |
|       21 |  7472 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7473 | `	if( nArg > 1 ){` |
|        - |  7474 | `		sxu32 iMin,iMax;` |
|        3 |  7475 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7476 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7477 | `		if( iMin < iMax ){` |
|        3 |  7478 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7479 | `			if( iDiv > 0 ){` |
|        3 |  7480 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7481 | `			}` |
|        1 |  7482 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7483 | `			iNum %= iMax;` |
|      ! 0 |  7484 | `		}` |
|        1 |  7485 | `	}` |
|        - |  7486 | `	/* Return the number */` |
|       21 |  7487 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7488 | `	return SXRET_OK;` |
|        1 |  7489 |  |
|        - |  7490 | `/*` |
|        - |  7491 | ` * int getrandmax(void)` |
|        - |  7492 | ` * int mt_getrandmax(void)` |
|        - |  7493 | ` * int rc4_getrandmax(void)` |
|        - |  7494 | ` *   Show largest possible random value` |
|        - |  7495 | ` * Return` |
|        - |  7496 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7497 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7498 | ` * Note:` |
|        - |  7499 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7500 | ` *  by te SQLite3 library.` |
|        - |  7501 | ` */` |
|        4 |  7502 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7503 |  |
|        2 |  7504 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7505 | `	SXUNUSED(apArg);` |
|        5 |  7506 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7507 | `	return SXRET_OK;` |
|        1 |  7508 |  |
|        - |  7509 | `/*` |
|        - |  7510 | ` * string rand_str()` |
|        - |  7511 | ` * string rand_str(int $len)` |
|        - |  7512 | ` *  Generate a random string (English alphabet).` |
|        - |  7513 | ` * Parameter` |
|        - |  7514 | ` *  $len` |
|        - |  7515 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7516 | ` * Return` |
|        - |  7517 | ` *   A pseudo random string.` |
|        - |  7518 | ` * Note:` |
|        - |  7519 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7520 | ` *  by te SQLite3 library.` |
|        - |  7521 | ` *  This function is a symisc extension.` |
|        - |  7522 | ` */` |
|      120 |  7523 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7524 |  |
|        - |  7525 | `	char zString[1024];` |
|      122 |  7526 | `	int iLen = 0x10;` |
|      122 |  7527 | `	if( nArg > 0 ){` |
|        - |  7528 | `		/* Get the desired length */` |
|      122 |  7529 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7530 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7531 | `			/* Default length */` |
|        3 |  7532 | `			iLen = 0x10;` |
|        1 |  7533 | `		}` |
|       60 |  7534 | `	}` |
|        - |  7535 | `	/* Generate the random string */` |
|      122 |  7536 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7537 | `	/* Return the generated string */` |
|      122 |  7538 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7539 | `	return SXRET_OK;` |
|        2 |  7540 |  |
|        - |  7541 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7542 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7543 | `/* Unique ID private data */` |
|        - |  7544 | `struct unique_id_data` |
|        - |  7545 |  |
|        - |  7546 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7547 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7548 | `};` |
|        - |  7549 | `/*` |
|        - |  7550 | ` * Binary to hex consumer callback.` |
|        - |  7551 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7552 | ` * defined below.` |
|        - |  7553 | ` */` |
|      192 |  7554 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7555 |  |
|      193 |  7556 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7557 | `	sxu32 nBuflen;` |
|        - |  7558 | `	/* Extract result buffer length */` |
|      193 |  7559 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7560 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7561 | `			/*` |
|        - |  7562 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7563 | `			 * string will be 13 characters long` |
|        - |  7564 | `			 */` |
|       25 |  7565 | `		return SXERR_ABORT;` |
|        - |  7566 | `	}` |
|      169 |  7567 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7568 | `		return SXERR_ABORT;` |
|        - |  7569 | `	}` |
|        - |  7570 | `	/* Safely Consume the hex stream */` |
|      169 |  7571 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7572 | `	return SXRET_OK;` |
|       97 |  7573 |  |
|        - |  7574 | `/*` |
|        - |  7575 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7576 | ` *  Generate a unique ID` |
|        - |  7577 | ` * Parameter` |
|        - |  7578 | ` * $prefix` |
|        - |  7579 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7580 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7581 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7582 | ` * $more_entropy` |
|        - |  7583 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7584 | ` *  that the result will be unique.` |
|        - |  7585 | ` * Return` |
|        - |  7586 | ` *  Returns the unique identifier, as a string.` |
|        - |  7587 | ` */` |
|       24 |  7588 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7589 |  |
|        - |  7590 | `	struct unique_id_data sUniq;` |
|        - |  7591 | `	unsigned char zDigest[20];` |
|       25 |  7592 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7593 | `	const char *zPrefix;` |
|        - |  7594 | `	SHA1Context sCtx;` |
|        - |  7595 | `	char zRandom[7];` |
|        - |  7596 | `	int nPrefix;` |
|        - |  7597 | `	int entropy;` |
|        - |  7598 | `	/* Generate a random string first */` |
|       25 |  7599 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7600 | `	/* Initialize fields */` |
|       25 |  7601 | `	zPrefix = 0;` |
|       25 |  7602 | `	nPrefix = 0;` |
|       25 |  7603 | `	entropy = 0;` |
|       25 |  7604 | `	if( nArg > 0 ){` |
|        - |  7605 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7606 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7607 | `		if( nArg > 1 ){` |
|      ! 0 |  7608 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7609 | `		}` |
|      ! 0 |  7610 | `	}` |
|       25 |  7611 | `	SHA1Init(&sCtx);` |
|        - |  7612 | `	/* Generate the random ID */` |
|       25 |  7613 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7614 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7615 | `	}` |
|        - |  7616 | `	/* Append the random ID */` |
|       25 |  7617 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7618 | `	/* Append the random string */` |
|       25 |  7619 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7620 | `	/* Increment the number */` |
|       25 |  7621 | `	pVm->unique_id++;` |
|       25 |  7622 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7623 | `	/* Hexify the digest */` |
|       25 |  7624 | `	sUniq.pCtx = pCtx;` |
|       25 |  7625 | `	sUniq.entropy = entropy;` |
|       25 |  7626 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7627 | `	/* All done */` |
|       25 |  7628 | `	return PH7_OK;` |
|        1 |  7629 |  |
|        - |  7630 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7631 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7632 | `/*` |
|        - |  7633 | ` * Section:` |
|        - |  7634 | ` *  Language construct implementation as foreign functions.` |
|        - |  7635 | ` * Status:` |
|        - |  7636 | ` *    Stable.` |
|        - |  7637 | ` */` |
|        - |  7638 | `/*` |
|        - |  7639 | ` * void echo($string...)` |
|        - |  7640 | ` *  Output one or more messages.` |
|        - |  7641 | ` * Parameters` |
|        - |  7642 | ` *  $string` |
|        - |  7643 | ` *   Message to output.` |
|        - |  7644 | ` * Return` |
|        - |  7645 | ` *  NULL.` |
|        - |  7646 | ` */` |
|      ! 0 |  7647 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7648 |  |
|        - |  7649 | `	const char *zData;` |
|      ! 0 |  7650 | `	int nDataLen = 0;` |
|        - |  7651 | `	ph7_vm *pVm;` |
|        - |  7652 | `	int i,rc;` |
|        - |  7653 | `	/* Point to the target VM */` |
|      ! 0 |  7654 | `	pVm = pCtx->pVm;` |
|        - |  7655 | `	/* Output */` |
|      ! 0 |  7656 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7657 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7658 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7659 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7660 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7661 | `				/* Increment output length */` |
|      ! 0 |  7662 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7663 | `			}` |
|      ! 0 |  7664 | `			if( rc == SXERR_ABORT ){` |
|        - |  7665 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7666 | `				return PH7_ABORT;` |
|        - |  7667 | `			}` |
|      ! 0 |  7668 | `		}` |
|      ! 0 |  7669 | `	}` |
|      ! 0 |  7670 | `	return SXRET_OK;` |
|      ! 0 |  7671 |  |
|        - |  7672 | `/*` |
|        - |  7673 | ` * int print($string...)` |
|        - |  7674 | ` *  Output one or more messages.` |
|        - |  7675 | ` * Parameters` |
|        - |  7676 | ` *  $string` |
|        - |  7677 | ` *   Message to output.` |
|        - |  7678 | ` * Return` |
|        - |  7679 | ` *  1 always.` |
|        - |  7680 | ` */` |
|        2 |  7681 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7682 |  |
|        - |  7683 | `	const char *zData;` |
|        3 |  7684 | `	int nDataLen = 0;` |
|        - |  7685 | `	ph7_vm *pVm;` |
|        - |  7686 | `	int i,rc;` |
|        - |  7687 | `	/* Point to the target VM */` |
|        3 |  7688 | `	pVm = pCtx->pVm;` |
|        - |  7689 | `	/* Output */` |
|        5 |  7690 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7691 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7692 | `		if( nDataLen > 0 ){` |
|        3 |  7693 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7694 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7695 | `				/* Increment output length */` |
|        3 |  7696 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7697 | `			}` |
|        3 |  7698 | `			if( rc == SXERR_ABORT ){` |
|        - |  7699 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7700 | `				return PH7_ABORT;` |
|        - |  7701 | `			}` |
|        1 |  7702 | `		}` |
|        2 |  7703 | `	}` |
|        - |  7704 | `	/* Return 1 */` |
|        3 |  7705 | `	ph7_result_int(pCtx,1);` |
|        3 |  7706 | `	return SXRET_OK;` |
|        2 |  7707 |  |
|        - |  7708 | `/*` |
|        - |  7709 | ` * void exit(string $msg)` |
|        - |  7710 | ` * void exit(int $status)` |
|        - |  7711 | ` * void die(string $ms)` |
|        - |  7712 | ` * void die(int $status)` |
|        - |  7713 | ` *   Output a message and terminate program execution.` |
|        - |  7714 | ` * Parameter` |
|        - |  7715 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7716 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7717 | ` *  and not printed` |
|        - |  7718 | ` * Return` |
|        - |  7719 | ` *  NULL` |
|        - |  7720 | ` */` |
|      ! 0 |  7721 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7722 |  |
|      ! 0 |  7723 | `	if( nArg > 0 ){` |
|      ! 0 |  7724 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7725 | `			const char *zData;` |
|      ! 0 |  7726 | `			int iLen = 0;` |
|        - |  7727 | `			/* Print exit message */` |
|      ! 0 |  7728 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7729 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7730 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7731 | `			sxi32 iExitStatus;` |
|        - |  7732 | `			/* Record exit status code */` |
|      ! 0 |  7733 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7734 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7735 | `		}` |
|      ! 0 |  7736 | `	}` |
|        - |  7737 | `	/* Check if we are in an included file */` |
|      ! 0 |  7738 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7739 | `		/* Exit the entire process */` |
|      ! 0 |  7740 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7741 | `	}` |
|        - |  7742 | `	/* Abort processing immediately */` |
|      ! 0 |  7743 | `	return PH7_ABORT;` |
|      ! 0 |  7744 |  |
|        - |  7745 | `/*` |
|        - |  7746 | ` * bool isset($var,...)` |
|        - |  7747 | ` *  Finds out whether a variable is set.` |
|        - |  7748 | ` * Parameters` |
|        - |  7749 | ` *  One or more variable to check.` |
|        - |  7750 | ` * Return` |
|        - |  7751 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7752 | ` */` |
|    70862 |  7753 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7754 |  |
|        - |  7755 | `	ph7_value *pObj;` |
|    70864 |  7756 | `	int res = 0;` |
|        - |  7757 | `	int i;` |
|    70864 |  7758 | `	if( nArg < 1 ){` |
|        - |  7759 | `		/* Missing arguments,return false */` |
|      ! 0 |  7760 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7761 | `		return SXRET_OK;` |
|        - |  7762 | `	}` |
|        - |  7763 | `	/* Iterate over available arguments */` |
|    93586 |  7764 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    70864 |  7765 | `		pObj = apArg[i];` |
|    70864 |  7766 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    47636 |  7767 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7768 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7769 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7770 | `			}` |
|    23817 |  7771 | `		}` |
|    70864 |  7772 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    70864 |  7773 | `		if( !res ){` |
|        - |  7774 | `			/* Variable not set,return FALSE */` |
|    48142 |  7775 | `			ph7_result_bool(pCtx,0);` |
|    48142 |  7776 | `			return SXRET_OK;` |
|        - |  7777 | `		}` |
|    11363 |  7778 | `	}` |
|        - |  7779 | `	/* All given variable are set,return TRUE */` |
|    22724 |  7780 | `	ph7_result_bool(pCtx,1);` |
|    22724 |  7781 | `	return SXRET_OK;` |
|    35433 |  7782 |  |
|        - |  7783 | `/*` |
|        - |  7784 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7785 | ` * frame,the reference table and discard it's contents.` |
|        - |  7786 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7787 | ` */` |
|  2961658 |  7788 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7789 |  |
|        - |  7790 | `	ph7_value *pObj;` |
|        - |  7791 | `	VmRefObj *pRef;` |
|  2961660 |  7792 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2961660 |  7793 | `	if( pObj ){` |
|        - |  7794 | `		/* Release the object */` |
|  2961660 |  7795 | `		PH7_MemObjRelease(pObj);` |
|  1480829 |  7796 | `	}` |
|        - |  7797 | `	/* Remove old reference links */` |
|  2961660 |  7798 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2961660 |  7799 | `	if( pRef ){` |
|  2961640 |  7800 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7801 | `		/* Unlink from the reference table */` |
|  2961640 |  7802 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2961640 |  7803 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7804 | `			VmSlot sFree;` |
|        - |  7805 | `			/* Restore to the free list */` |
|  2961634 |  7806 | `			sFree.nIdx = nObjIdx;` |
|  2961634 |  7807 | `			sFree.pUserData = 0;` |
|  2961634 |  7808 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1480816 |  7809 | `		}` |
|  1480819 |  7810 | `	}` |
|  2961660 |  7811 | `	return SXRET_OK;` |
|        2 |  7812 |  |
|        - |  7813 | `/*` |
|        - |  7814 | ` * void unset($var,...)` |
|        - |  7815 | ` *   Unset one or more given variable.` |
|        - |  7816 | ` * Parameters` |
|        - |  7817 | ` *  One or more variable to unset.` |
|        - |  7818 | ` * Return` |
|        - |  7819 | ` *  Nothing.` |
|        - |  7820 | ` */` |
|     3258 |  7821 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7822 |  |
|        - |  7823 | `	ph7_value *pObj;` |
|        - |  7824 | `	ph7_vm *pVm;` |
|        - |  7825 | `	int i;` |
|        - |  7826 | `	/* Point to the target VM */` |
|     3260 |  7827 | `	pVm = pCtx->pVm;` |
|        - |  7828 | `	/* Iterate and unset */` |
|     9662 |  7829 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6404 |  7830 | `		pObj = apArg[i];` |
|     6404 |  7831 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      868 |  7832 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7833 | `				/* Throw an error */` |
|      ! 0 |  7834 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7835 | `			}` |
|      435 |  7836 | `		}else{` |
|     5537 |  7837 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7838 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5537 |  7839 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5531 |  7840 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2765 |  7841 | `			}` |
|        - |  7842 | `		}` |
|     3203 |  7843 | `	}` |
|     3260 |  7844 | `	return SXRET_OK;` |
|        2 |  7845 |  |
|        - |  7846 | `/*` |
|        - |  7847 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7848 | ` */` |
|      110 |  7849 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7850 |  |
|      111 |  7851 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7852 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7853 | `	ph7_value *pObj;` |
|        - |  7854 | `	sxu32 nIdx;` |
|        - |  7855 | `	/* Extract the memory object */` |
|      111 |  7856 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7857 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7858 | `	if( pObj ){` |
|      111 |  7859 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7860 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7861 | `				SyString sName;` |
|        - |  7862 | `				ph7_value sKey;` |
|        - |  7863 | `				/* Perform the insertion */` |
|      109 |  7864 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7865 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7866 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7867 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7868 | `			}` |
|       54 |  7869 | `		}` |
|       55 |  7870 | `	}` |
|      111 |  7871 | `	return SXRET_OK;` |
|        1 |  7872 |  |
|        - |  7873 | `/*` |
|        - |  7874 | ` * array get_defined_vars(void)` |
|        - |  7875 | ` *  Returns an array of all defined variables.` |
|        - |  7876 | ` * Parameter` |
|        - |  7877 | ` *  None` |
|        - |  7878 | ` * Return` |
|        - |  7879 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7880 | ` */` |
|        2 |  7881 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7882 |  |
|        3 |  7883 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7884 | `	ph7_value *pArray;` |
|        - |  7885 | `	/* Create a new array */` |
|        3 |  7886 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7887 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7888 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7889 | `		SXUNUSED(apArg);` |
|        - |  7890 | `		/* Return NULL */` |
|      ! 0 |  7891 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7892 | `		return SXRET_OK;` |
|        - |  7893 | `	}` |
|        - |  7894 | `	/* Superglobals first */` |
|        3 |  7895 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7896 | `	/* Then variable defined in the current frame */` |
|        3 |  7897 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7898 | `	/* Finally,return the created array */` |
|        3 |  7899 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7900 | `	return SXRET_OK;` |
|        2 |  7901 |  |
|        - |  7902 | `/*` |
|        - |  7903 | ` * bool gettype($var)` |
|        - |  7904 | ` *  Get the type of a variable` |
|        - |  7905 | ` * Parameters` |
|        - |  7906 | ` *   $var` |
|        - |  7907 | ` *    The variable being type checked.` |
|        - |  7908 | ` * Return` |
|        - |  7909 | ` *   String representation of the given variable type.` |
|        - |  7910 | ` */` |
|       32 |  7911 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7912 |  |
|       34 |  7913 | `	const char *zType = "Empty";` |
|       34 |  7914 | `	if( nArg > 0 ){` |
|       34 |  7915 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7916 | `	}` |
|        - |  7917 | `	/* Return the variable type */` |
|       34 |  7918 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7919 | `	return SXRET_OK;` |
|        2 |  7920 |  |
|        - |  7921 | `/*` |
|        - |  7922 | ` * string get_resource_type(resource $handle)` |
|        - |  7923 | ` *  This function gets the type of the given resource.` |
|        - |  7924 | ` * Parameters` |
|        - |  7925 | ` *  $handle` |
|        - |  7926 | ` *  The evaluated resource handle.` |
|        - |  7927 | ` * Return` |
|        - |  7928 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7929 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7930 | ` *  the return value will be the string Unknown.` |
|        - |  7931 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7932 | ` *  is not a resource.` |
|        - |  7933 | ` */` |
|        2 |  7934 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7935 |  |
|        3 |  7936 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7937 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7938 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7939 | `		return PH7_OK;` |
|        - |  7940 | `	}` |
|        3 |  7941 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7942 | `	return SXRET_OK;` |
|        2 |  7943 |  |
|        - |  7944 | `/*` |
|        - |  7945 | ` * void var_dump(expression,....)` |
|        - |  7946 | ` *   var_dump � Dumps information about a variable` |
|        - |  7947 | ` * Parameters` |
|        - |  7948 | ` *   One or more expression to dump.` |
|        - |  7949 | ` * Returns` |
|        - |  7950 | ` *  Nothing.` |
|        - |  7951 | ` */` |
|      218 |  7952 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7953 |  |
|        - |  7954 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7955 | `	int i;` |
|      220 |  7956 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7957 | `	/* Dump one or more expressions */` |
|      444 |  7958 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  7959 | `		ph7_value *pObj = apArg[i];` |
|        - |  7960 | `		/* Reset the working buffer */` |
|      226 |  7961 | `		SyBlobReset(&sDump);` |
|        - |  7962 | `		/* Dump the given expression */` |
|      226 |  7963 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7964 | `		/* Output */` |
|      226 |  7965 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  7966 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  7967 | `		}` |
|      114 |  7968 | `	}` |
|        - |  7969 | `	/* Release the working buffer */` |
|      220 |  7970 | `	SyBlobRelease(&sDump);` |
|      220 |  7971 | `	return SXRET_OK;` |
|        2 |  7972 |  |
|        - |  7973 | `/*` |
|        - |  7974 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7975 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7976 | ` * Parameters` |
|        - |  7977 | ` *   expression: Expression to dump` |
|        - |  7978 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7979 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7980 | ` *            print_r() will return the information rather than print it.` |
|        - |  7981 | ` * Return` |
|        - |  7982 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7983 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7984 | ` */` |
|       16 |  7985 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7986 |  |
|       17 |  7987 | `	int ret_string = 0;` |
|        - |  7988 | `	SyBlob sDump;` |
|       17 |  7989 | `	if( nArg < 1 ){` |
|        - |  7990 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7991 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7992 | `		return SXRET_OK;` |
|        - |  7993 | `	}` |
|       17 |  7994 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7995 | `	if ( nArg > 1 ){` |
|        - |  7996 | `		/* Where to redirect output */` |
|       11 |  7997 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7998 | `	}` |
|        - |  7999 | `	/* Generate dump */` |
|       17 |  8000 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8001 | `	if( !ret_string ){` |
|        - |  8002 | `		/* Output dump */` |
|        7 |  8003 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8004 | `		/* Return true */` |
|        7 |  8005 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8006 | `	}else{` |
|        - |  8007 | `		/* Generated dump as return value */` |
|       11 |  8008 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8009 | `	}` |
|        - |  8010 | `	/* Release the working buffer */` |
|       17 |  8011 | `	SyBlobRelease(&sDump);` |
|       17 |  8012 | `	return SXRET_OK;` |
|        9 |  8013 |  |
|        - |  8014 | `/*` |
|        - |  8015 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  8016 | ` * Same job as print_r. (see coment above)` |
|        - |  8017 | ` */` |
|        2 |  8018 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8019 |  |
|        3 |  8020 | `	int ret_string = 0;` |
|        - |  8021 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  8022 | `	if( nArg < 1 ){` |
|        - |  8023 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8024 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8025 | `		return SXRET_OK;` |
|        - |  8026 | `	}` |
|        3 |  8027 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  8028 | `	if ( nArg > 1 ){` |
|        - |  8029 | `		/* Where to redirect output */` |
|        3 |  8030 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  8031 | `	}` |
|        - |  8032 | `	/* Generate dump */` |
|        3 |  8033 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  8034 | `	if( !ret_string ){` |
|        - |  8035 | `		/* Output dump */` |
|      ! 0 |  8036 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8037 | `		/* Return NULL */` |
|      ! 0 |  8038 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8039 | `	}else{` |
|        - |  8040 | `		/* Generated dump as return value */` |
|        3 |  8041 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8042 | `	}` |
|        - |  8043 | `	/* Release the working buffer */` |
|        3 |  8044 | `	SyBlobRelease(&sDump);` |
|        3 |  8045 | `	return SXRET_OK;` |
|        2 |  8046 |  |
|        - |  8047 | `/*` |
|        - |  8048 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  8049 | ` *  Set/get the various assert flags.` |
|        - |  8050 | ` * Parameter` |
|        - |  8051 | ` * $what` |
|        - |  8052 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  8053 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  8054 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  8055 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  8056 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  8057 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  8058 | ` * $value` |
|        - |  8059 | ` *   An optional new value for the option.` |
|        - |  8060 | ` * Return` |
|        - |  8061 | ` *  Old setting on success or FALSE on failure.` |
|        - |  8062 | ` */` |
|       30 |  8063 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8064 |  |
|       32 |  8065 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8066 | `	int iOption;` |
|        - |  8067 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  8068 | `	if( nArg < 1 ){` |
|        3 |  8069 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8070 | `			"ArgumentCountError",` |
|        - |  8071 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  8072 | `			);` |
|        - |  8073 | `	}` |
|        - |  8074 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  8075 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  8076 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  8077 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8078 | `			"TypeError",` |
|        - |  8079 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  8080 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  8081 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  8082 | `			);` |
|        - |  8083 | `	}` |
|       30 |  8084 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  8085 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  8086 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  8087 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  8088 | `	switch( iOption ){` |
|        6 |  8089 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  8090 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  8091 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  8092 | `		if( nArg > 1 ){` |
|        5 |  8093 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8094 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  8095 | `			}else{` |
|        3 |  8096 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  8097 | `			}` |
|        2 |  8098 | `		}` |
|       14 |  8099 | `		break;` |
|        1 |  8100 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8101 | `		/* Return old callback or null */` |
|        3 |  8102 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8103 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8104 | `		}else{` |
|        3 |  8105 | `			ph7_result_null(pCtx);` |
|        - |  8106 | `		}` |
|        3 |  8107 | `		if( nArg > 1 ){` |
|      ! 0 |  8108 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8109 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8110 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8111 | `			}else{` |
|      ! 0 |  8112 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8113 | `			}` |
|      ! 0 |  8114 | `		}` |
|        3 |  8115 | `		break;` |
|        5 |  8116 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8117 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8118 | `		if( nArg > 1 ){` |
|        5 |  8119 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8120 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8121 | `			}else{` |
|        3 |  8122 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8123 | `			}` |
|        2 |  8124 | `		}` |
|       11 |  8125 | `		break;` |
|      ! 0 |  8126 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8127 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8128 | `		break;` |
|        1 |  8129 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8130 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8131 | `		break;` |
|      ! 0 |  8132 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8133 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8134 | `		break;` |
|        1 |  8135 | `	default:` |
|        - |  8136 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8137 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8138 | `			"ValueError",` |
|        - |  8139 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8140 | `			);` |
|        - |  8141 | `	}` |
|       28 |  8142 | `	return PH7_OK;` |
|       17 |  8143 |  |
|        - |  8144 | `/*` |
|        - |  8145 | ` * bool assert(mixed $assertion)` |
|        - |  8146 | ` *  Checks if assertion is FALSE.` |
|        - |  8147 | ` * Parameter` |
|        - |  8148 | ` *  $assertion` |
|        - |  8149 | ` *    The assertion to test.` |
|        - |  8150 | ` * Return` |
|        - |  8151 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8152 | ` */` |
|       26 |  8153 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8154 |  |
|       28 |  8155 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8156 | `	int iFlags,iResult;` |
|        - |  8157 | `	const char *zDesc;` |
|        - |  8158 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8159 | `	if( nArg < 1 ){` |
|        3 |  8160 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8161 | `			"ArgumentCountError",` |
|        - |  8162 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8163 | `			);` |
|        - |  8164 | `	}` |
|       26 |  8165 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8166 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8167 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8168 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8169 | `		return PH7_OK;` |
|        - |  8170 | `	}` |
|        - |  8171 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8172 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8173 | `	if( !iResult ){` |
|        - |  8174 | `		/* Assertion failed */` |
|        - |  8175 | `		/* Extract optional description */` |
|       13 |  8176 | `		zDesc = 0;` |
|       13 |  8177 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8178 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8179 | `		}` |
|       13 |  8180 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8181 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8182 | `			ph7_value sFile,sLine;` |
|        - |  8183 | `			ph7_value *apCbArg[3];` |
|        - |  8184 | `			SyString *pFile;` |
|        - |  8185 | `			/* Extract the processed script */` |
|      ! 0 |  8186 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8187 | `			if( pFile == 0 ){` |
|      ! 0 |  8188 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8189 | `			}` |
|        - |  8190 | `			/* Invoke the callback */` |
|      ! 0 |  8191 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8192 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8193 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8194 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8195 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8196 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8197 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8198 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8199 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8200 | `		}` |
|       13 |  8201 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8202 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8203 | `			return PH7_ABORT;` |
|        - |  8204 | `		}` |
|        - |  8205 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8206 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8207 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8208 | `				"AssertionError",` |
|        - |  8209 | `				"%s",` |
|        1 |  8210 | `				zDesc` |
|        - |  8211 | `				);` |
|      ! 0 |  8212 | `		}else{` |
|       11 |  8213 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8214 | `				"AssertionError",` |
|        - |  8215 | `				"assert(false)"` |
|        - |  8216 | `				);` |
|        - |  8217 | `		}` |
|        - |  8218 | `	}` |
|        - |  8219 | `	/* Assertion passed */` |
|       14 |  8220 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8221 | `	return PH7_OK;` |
|       15 |  8222 |  |
|        - |  8223 | `/*` |
|        - |  8224 | ` * Section:` |
|        - |  8225 | ` *  Error reporting functions.` |
|        - |  8226 | ` * Status:` |
|        - |  8227 | ` *    Stable.` |
|        - |  8228 | ` */` |
|        - |  8229 | `/*` |
|        - |  8230 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8231 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8232 | ` * Parameters` |
|        - |  8233 | ` *  $error_msg` |
|        - |  8234 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8235 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8236 | ` * $error_type` |
|        - |  8237 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8238 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8239 | ` * Return` |
|        - |  8240 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8241 | ` */` |
|       12 |  8242 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8243 |  |
|       14 |  8244 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8245 | `	int rc = PH7_OK;` |
|       14 |  8246 | `	if( nArg > 0 ){` |
|        - |  8247 | `		const char *zErr;` |
|        - |  8248 | `		int nLen;` |
|        - |  8249 | `		/* Extract the error message */` |
|       12 |  8250 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8251 | `		if( nArg > 1 ){` |
|        - |  8252 | `			/* Extract the error type */` |
|       12 |  8253 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8254 | `			switch( nErr ){` |
|        1 |  8255 | `			case 1:   /* E_ERROR */` |
|        - |  8256 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8257 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8258 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8259 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8260 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8261 | `				break;` |
|        1 |  8262 | `			case 2:   /* E_WARNING */` |
|        - |  8263 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8264 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8265 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8266 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8267 | `				break;` |
|        3 |  8268 | `			default:` |
|        8 |  8269 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8270 | `				break;` |
|        - |  8271 | `			}` |
|        5 |  8272 | `		}` |
|        - |  8273 | `		/* Report error */` |
|       12 |  8274 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8275 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8276 | `			return rc;` |
|        - |  8277 | `		}` |
|        - |  8278 | `		/* Return true */` |
|       12 |  8279 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8280 | `	}else{` |
|        - |  8281 | `		/* Missing arguments,return FALSE */` |
|        3 |  8282 | `		ph7_result_bool(pCtx,0);` |
|        - |  8283 | `	}` |
|       14 |  8284 | `	return rc;` |
|        8 |  8285 |  |
|        - |  8286 | `/*` |
|        - |  8287 | ` * int error_reporting([int $level])` |
|        - |  8288 | ` *  Sets which PHP errors are reported.` |
|        - |  8289 | ` * Parameters` |
|        - |  8290 | ` *  $level` |
|        - |  8291 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8292 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8293 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8294 | ` *   levels will not always behave as expected.` |
|        - |  8295 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8296 | ` *   in the predefined constants.` |
|        - |  8297 | ` * Return` |
|        - |  8298 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8299 | ` *   parameter is given.` |
|        - |  8300 | ` */` |
|       40 |  8301 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8302 |  |
|       42 |  8303 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8304 | `	int nOld;` |
|        - |  8305 | `	/* Extract the old reporting level */` |
|       42 |  8306 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8307 | `	if( nArg > 0 ){` |
|        - |  8308 | `		int nNew;` |
|        - |  8309 | `		/* Extract the desired error reporting level */` |
|       34 |  8310 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8311 | `		if( !nNew ){` |
|        - |  8312 | `			/* Do not report errors at all */` |
|        5 |  8313 | `			pVm->bErrReport = 0;` |
|        3 |  8314 | `		}else{` |
|        - |  8315 | `			/* Report all errors */` |
|       30 |  8316 | `			pVm->bErrReport = 1;` |
|        - |  8317 | `		}` |
|       16 |  8318 | `	}` |
|        - |  8319 | `	/* Return the old level */` |
|       42 |  8320 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8321 | `	return PH7_OK;` |
|        2 |  8322 |  |
|        - |  8323 | `/*` |
|        - |  8324 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8325 | ` *  Send an error message somewhere.` |
|        - |  8326 | ` * Parameter` |
|        - |  8327 | ` *  $message` |
|        - |  8328 | ` *   The error message that should be logged.` |
|        - |  8329 | ` *  $message_type` |
|        - |  8330 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8331 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8332 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8333 | ` *       This is the default option.` |
|        - |  8334 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8335 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8336 | ` *    2  No longer an option.` |
|        - |  8337 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8338 | ` *       to the end of the message string.` |
|        - |  8339 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8340 | ` *  $destination` |
|        - |  8341 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8342 | ` *  $extra_headers` |
|        - |  8343 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8344 | ` * Return` |
|        - |  8345 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8346 | ` * NOTE:` |
|        - |  8347 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8348 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8349 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8350 | ` *  Otherwise this function is no-op.` |
|        - |  8351 | ` */` |
|        4 |  8352 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8353 |  |
|        - |  8354 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8355 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8356 | `	int iType = 0;` |
|        5 |  8357 | `	if( nArg < 1 ){` |
|        - |  8358 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8359 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8360 | `		return PH7_OK;` |
|        - |  8361 | `	}` |
|        5 |  8362 | `	if( pVm->xErrLog  ){` |
|        - |  8363 | `		/* Invoke the user callback */` |
|      ! 0 |  8364 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8365 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8366 | `		if( nArg > 1 ){` |
|      ! 0 |  8367 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8368 | `			if( nArg > 2 ){` |
|      ! 0 |  8369 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8370 | `				if( nArg > 3 ){` |
|      ! 0 |  8371 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8372 | `				}` |
|      ! 0 |  8373 | `			}` |
|      ! 0 |  8374 | `		}` |
|      ! 0 |  8375 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8376 | `	}` |
|        - |  8377 | `	/* Retun TRUE */` |
|        5 |  8378 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8379 | `	return PH7_OK;` |
|        3 |  8380 |  |
|        - |  8381 | `/*` |
|        - |  8382 | ` * bool restore_exception_handler(void)` |
|        - |  8383 | ` *  Restores the previously defined exception handler function.` |
|        - |  8384 | ` * Parameter` |
|        - |  8385 | ` *  None` |
|        - |  8386 | ` * Return` |
|        - |  8387 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8388 | ` */` |
|        4 |  8389 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8390 |  |
|        5 |  8391 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8392 | `	ph7_value *pOld,*pNew;` |
|        - |  8393 | `	/* Point to the old and the new handler */` |
|        5 |  8394 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8395 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8396 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8397 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8398 | `		SXUNUSED(apArg);` |
|        - |  8399 | `		/* No installed handler,return FALSE */` |
|        5 |  8400 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8401 | `		return PH7_OK;` |
|        - |  8402 | `	}` |
|        - |  8403 | `	/* Copy the old handler */` |
|      ! 0 |  8404 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8405 | `	PH7_MemObjRelease(pOld);` |
|        - |  8406 | `	/* Return TRUE */` |
|      ! 0 |  8407 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8408 | `	return PH7_OK;` |
|        3 |  8409 |  |
|        - |  8410 | `/*` |
|        - |  8411 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8412 | ` *  Sets a user-defined exception handler function.` |
|        - |  8413 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8414 | ` * NOTE` |
|        - |  8415 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8416 | ` *  the satndard PHP engine.` |
|        - |  8417 | ` * Parameters` |
|        - |  8418 | ` *  $exception_handler` |
|        - |  8419 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8420 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8421 | ` *   that was thrown.` |
|        - |  8422 | ` *  Note:` |
|        - |  8423 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8424 | ` * Return` |
|        - |  8425 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8426 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8427 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8428 | ` */` |
|        4 |  8429 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8430 |  |
|        6 |  8431 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8432 | `	ph7_value *pOld,*pNew;` |
|        - |  8433 | `	/* Point to the old and the new handler */` |
|        6 |  8434 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8435 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8436 | `	/* Return the old handler */` |
|        6 |  8437 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8438 | `	if( nArg > 0 ){` |
|        6 |  8439 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8440 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8441 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8442 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8443 | `		}else{` |
|        6 |  8444 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8445 | `			/* Install the new handler */` |
|        6 |  8446 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8447 | `		}` |
|        2 |  8448 | `	}` |
|        6 |  8449 | `	return PH7_OK;` |
|        2 |  8450 |  |
|        - |  8451 | `/*` |
|        - |  8452 | ` * bool restore_error_handler(void)` |
|        - |  8453 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8454 | ` * Parameters:` |
|        - |  8455 | ` *  None.` |
|        - |  8456 | ` * Return` |
|        - |  8457 | ` *  Always TRUE.` |
|        - |  8458 | ` */` |
|        4 |  8459 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8460 |  |
|        5 |  8461 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8462 | `	ph7_value *pOld,*pNew;` |
|        - |  8463 | `	/* Point to the old and the new handler */` |
|        5 |  8464 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8465 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8466 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8467 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8468 | `		SXUNUSED(apArg);` |
|        - |  8469 | `		/* No installed callback,return FALSE */` |
|        5 |  8470 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8471 | `		return PH7_OK;` |
|        - |  8472 | `	}` |
|        - |  8473 | `	/* Copy the old callback */` |
|      ! 0 |  8474 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8475 | `	PH7_MemObjRelease(pOld);` |
|        - |  8476 | `	/* Return TRUE */` |
|      ! 0 |  8477 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8478 | `	return PH7_OK;` |
|        3 |  8479 |  |
|        - |  8480 | `/*` |
|        - |  8481 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8482 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8483 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8484 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8485 | ` *  Sets a user-defined error handler function.` |
|        - |  8486 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8487 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8488 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8489 | ` *  conditions (using trigger_error()).` |
|        - |  8490 | ` * Parameters` |
|        - |  8491 | ` *  $error_handler` |
|        - |  8492 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8493 | ` *   describing the error.` |
|        - |  8494 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8495 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8496 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8497 | ` *   The function can be shown as:` |
|        - |  8498 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8499 | ` *     errno` |
|        - |  8500 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8501 | ` *   errstr` |
|        - |  8502 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8503 | ` *   errfile` |
|        - |  8504 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8505 | ` *     was raised in, as a string.` |
|        - |  8506 | ` *  Note:` |
|        - |  8507 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8508 | ` * Return` |
|        - |  8509 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8510 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8511 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8512 | ` */` |
|     8722 |  8513 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8514 |  |
|     8724 |  8515 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8516 | `	ph7_value *pOld,*pNew;` |
|        - |  8517 | `	/* Point to the old and the new handler */` |
|     8724 |  8518 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8519 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8520 | `	/* Return the old handler */` |
|     8724 |  8521 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8522 | `	if( nArg > 0 ){` |
|     8724 |  8523 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8524 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8525 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8526 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8527 | `		}else{` |
|     4364 |  8528 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8529 | `			/* Install the new handler */` |
|     4364 |  8530 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8531 | `		}` |
|     4361 |  8532 | `	}` |
|     8724 |  8533 | `	return PH7_OK;` |
|        2 |  8534 |  |
|        - |  8535 | `/*` |
|        - |  8536 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8537 | ` *  Generates a backtrace.` |
|        - |  8538 | ` * Paramaeter` |
|        - |  8539 | ` *  $options` |
|        - |  8540 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8541 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8542 | ` *   all the function/method arguments, to save memory.` |
|        - |  8543 | ` * $limit` |
|        - |  8544 | ` *   (Not Used)` |
|        - |  8545 | ` * Return` |
|        - |  8546 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8547 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8548 | ` *          Name        Type      Description` |
|        - |  8549 | ` *          ------      ------     -----------` |
|        - |  8550 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8551 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8552 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8553 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8554 | ` *          object      object    The current object.` |
|        - |  8555 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8556 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8557 | ` */` |
|      502 |  8558 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8559 |  |
|      504 |  8560 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8561 | `	ph7_value *pArray;` |
|        - |  8562 | `	ph7_class *pClass;` |
|        - |  8563 | `	ph7_value *pValue;` |
|        - |  8564 | `	SyString *pFile;` |
|        - |  8565 | `	/* Create a new array */` |
|      504 |  8566 | `	pArray = ph7_context_new_array(pCtx);` |
|      504 |  8567 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      504 |  8568 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8569 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8570 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8571 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8572 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8573 | `		SXUNUSED(apArg);` |
|      ! 0 |  8574 | `		return PH7_OK;` |
|        - |  8575 | `	}` |
|        - |  8576 | `	/* Dump running function name and it's arguments  */` |
|      504 |  8577 | `	if( pVm->pFrame->pParent ){` |
|      504 |  8578 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8579 | `		ph7_vm_func *pFunc;` |
|        - |  8580 | `		ph7_value *pArg;` |
|      504 |  8581 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8582 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8583 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8584 | `		}` |
|      504 |  8585 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      504 |  8586 | `		if( pFrame->pParent && pFunc ){` |
|      504 |  8587 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      504 |  8588 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      504 |  8589 | `			ph7_value_reset_string_cursor(pValue);` |
|      251 |  8590 | `		}` |
|        - |  8591 | `		/* Function arguments */` |
|      504 |  8592 | `		pArg = ph7_context_new_array(pCtx);` |
|      504 |  8593 | `		if( pArg  ){` |
|        - |  8594 | `			ph7_value *pObj;` |
|        - |  8595 | `			VmSlot *aSlot;` |
|        - |  8596 | `			sxu32 n;` |
|        - |  8597 | `			/* Start filling the array with the given arguments */` |
|      504 |  8598 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2002 |  8599 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1500 |  8600 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1500 |  8601 | `				if( pObj ){` |
|     1500 |  8602 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      749 |  8603 | `				}` |
|      751 |  8604 | `			}` |
|        - |  8605 | `			/* Save the array */` |
|      504 |  8606 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      251 |  8607 | `		}` |
|      251 |  8608 | `	}` |
|      504 |  8609 | `	ph7_value_int(pValue,1);` |
|        - |  8610 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8611 | `	 * line numbers at run-time. )` |
|        - |  8612 | `	 */` |
|      504 |  8613 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8614 | `	/* Current processed script */` |
|      504 |  8615 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      504 |  8616 | `	if( pFile ){` |
|      504 |  8617 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      504 |  8618 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      504 |  8619 | `		ph7_value_reset_string_cursor(pValue);` |
|      251 |  8620 | `	}` |
|        - |  8621 | `	/* Top class */` |
|      504 |  8622 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      504 |  8623 | `	if( pClass ){` |
|      500 |  8624 | `		ph7_value_reset_string_cursor(pValue);` |
|      500 |  8625 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      500 |  8626 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      249 |  8627 | `	}` |
|        - |  8628 | `	/* Return the freshly created array */` |
|      504 |  8629 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8630 | `	/*` |
|        - |  8631 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8632 | `	 * as soon we return from this function.` |
|        - |  8633 | `	 */` |
|      504 |  8634 | `	return PH7_OK;` |
|      253 |  8635 |  |
|        - |  8636 | `/*` |
|        - |  8637 | ` * Generate a small backtrace.` |
|        - |  8638 | ` * Store the generated dump in the given BLOB` |
|        - |  8639 | ` */` |
|        4 |  8640 | `static int VmMiniBacktrace(` |
|        - |  8641 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8642 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8643 | `	)` |
|        1 |  8644 |  |
|        5 |  8645 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8646 | `	ph7_vm_func *pFunc;` |
|        - |  8647 | `	ph7_class *pClass;` |
|        - |  8648 | `	SyString *pFile;` |
|        - |  8649 | `	/* Called function */` |
|        5 |  8650 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8651 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8652 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8653 | `	}` |
|        5 |  8654 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8655 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8656 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8657 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8658 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8659 | `	}else{` |
|      ! 0 |  8660 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8661 | `	}` |
|        5 |  8662 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8663 | `	/* Current processed script */` |
|        5 |  8664 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8665 | `	if( pFile ){` |
|        5 |  8666 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8667 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8668 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8669 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8670 | `	}` |
|        - |  8671 | `	/* Top class */` |
|        5 |  8672 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8673 | `	if( pClass ){` |
|      ! 0 |  8674 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8675 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8676 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8677 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8678 | `	}` |
|        5 |  8679 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8680 | `	/* All done */` |
|        5 |  8681 | `	return SXRET_OK;` |
|        1 |  8682 |  |
|        - |  8683 | `/*` |
|        - |  8684 | ` * void debug_print_backtrace()` |
|        - |  8685 | ` *  Prints a backtrace` |
|        - |  8686 | ` * Parameters` |
|        - |  8687 | ` * None` |
|        - |  8688 | ` * Return` |
|        - |  8689 | ` * NULL` |
|        - |  8690 | ` */` |
|        2 |  8691 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8692 |  |
|        3 |  8693 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8694 | `	SyBlob sDump;` |
|        3 |  8695 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8696 | `	/* Generate the backtrace */` |
|        3 |  8697 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8698 | `	/* Output backtrace */` |
|        3 |  8699 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8700 | `	/* All done,cleanup */` |
|        3 |  8701 | `	SyBlobRelease(&sDump);` |
|        1 |  8702 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8703 | `	SXUNUSED(apArg);` |
|        3 |  8704 | `	return PH7_OK;` |
|        1 |  8705 |  |
|        - |  8706 | `/*` |
|        - |  8707 | ` * string debug_string_backtrace()` |
|        - |  8708 | ` *  Generate a backtrace` |
|        - |  8709 | ` * Parameters` |
|        - |  8710 | ` * None` |
|        - |  8711 | ` * Return` |
|        - |  8712 | ` *  A mini backtrace().` |
|        - |  8713 | ` * Note that this is a symisc extension.` |
|        - |  8714 | ` */` |
|        2 |  8715 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8716 |  |
|        3 |  8717 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8718 | `	SyBlob sDump;` |
|        3 |  8719 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8720 | `	/* Generate the backtrace */` |
|        3 |  8721 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8722 | `	/* Return the backtrace */` |
|        3 |  8723 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8724 | `	/* All done,cleanup */` |
|        3 |  8725 | `	SyBlobRelease(&sDump);` |
|        1 |  8726 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8727 | `	SXUNUSED(apArg);` |
|        3 |  8728 | `	return PH7_OK;` |
|        1 |  8729 |  |
|        - |  8730 | `/*` |
|        - |  8731 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8732 | ` * exception is triggered.` |
|        - |  8733 | ` */` |
|      472 |  8734 | `static sxi32 VmUncaughtException(` |
|        - |  8735 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8736 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8737 | `	)` |
|        1 |  8738 |  |
|        - |  8739 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8740 | `	int nArg = 1;` |
|        - |  8741 | `	sxi32 rc;` |
|      473 |  8742 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8743 | `		/* Nesting limit reached */` |
|      ! 0 |  8744 | `		return SXRET_OK;` |
|        - |  8745 | `	}` |
|        - |  8746 | `	/* Call any exception handler if available */` |
|      473 |  8747 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8748 | `	if( pThis ){` |
|        - |  8749 | `		/* Load the exception instance */` |
|      473 |  8750 | `		sArg.x.pOther = pThis;` |
|      473 |  8751 | `		pThis->iRef++;` |
|      473 |  8752 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8753 | `	}else{` |
|      ! 0 |  8754 | `		nArg = 0;` |
|        - |  8755 | `	}` |
|      473 |  8756 | `	apArg[0] = &sArg;` |
|        - |  8757 | `	/* Call the exception handler if available */` |
|      473 |  8758 | `	pVm->nExceptDepth++;` |
|      473 |  8759 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8760 | `	pVm->nExceptDepth--;` |
|      473 |  8761 | `	if( rc != SXRET_OK ){` |
|        - |  8762 | `		SyBlob sMsgBuf;` |
|      471 |  8763 | `		const char *zClass = "Exception";` |
|      471 |  8764 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8765 | `		const char *zMsg;` |
|        - |  8766 | `		sxu32 nMsg;` |
|        - |  8767 | `		const char *zFuncName;` |
|        - |  8768 | `		int nFuncLen;` |
|      471 |  8769 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8770 | `		if( pThis ){` |
|        - |  8771 | `			ph7_class_method *pGetMessage;` |
|        - |  8772 | `			ph7_value sMsg;` |
|        - |  8773 | `			const char *zTmp;` |
|        - |  8774 | `			int nTmp;` |
|      471 |  8775 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8776 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8777 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8778 | `			if( pGetMessage ){` |
|      471 |  8779 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8780 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8781 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8782 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8783 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8784 | `					}` |
|      235 |  8785 | `				}` |
|      471 |  8786 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8787 | `			}` |
|      235 |  8788 | `		}` |
|      471 |  8789 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8790 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8791 | `		}` |
|      471 |  8792 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8793 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8794 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8795 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8796 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8797 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8798 | `		rc = SXERR_ABORT;` |
|      235 |  8799 | `	}` |
|      473 |  8800 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8801 | `	return rc;` |
|      237 |  8802 |  |
|        - |  8803 | `/*` |
|        - |  8804 | ` * Throw an user exception.` |
|        - |  8805 | ` */` |
|      506 |  8806 | `static sxi32 VmThrowException(` |
|        - |  8807 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8808 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8809 | `	)` |
|        2 |  8810 |  |
|        - |  8811 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8812 | `	ph7_exception **apException;` |
|        - |  8813 | `	ph7_exception *pException;` |
|        - |  8814 | `	/* Point to the stack of loaded exceptions */` |
|      508 |  8815 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      508 |  8816 | `	pException = 0;` |
|      508 |  8817 | `	pCatch = 0;` |
|      508 |  8818 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8819 | `		ph7_exception_block *aCatch;` |
|        - |  8820 | `		ph7_class *pClass;` |
|        - |  8821 | `		sxu32 j;` |
|        - |  8822 | `		/* Locate the appropriate block to execute */` |
|       32 |  8823 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       32 |  8824 | `		(void)SySetPop(&pVm->aException);` |
|       32 |  8825 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       32 |  8826 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       30 |  8827 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8828 | `			/* Extract the target class */` |
|       30 |  8829 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       30 |  8830 | `			if( pClass == 0 ){` |
|        - |  8831 | `				/* No such class */` |
|      ! 0 |  8832 | `				continue;` |
|        - |  8833 | `			}` |
|       30 |  8834 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8835 | `				/* Catch block found,break immeditaley */` |
|       30 |  8836 | `				pCatch = &aCatch[j];` |
|       30 |  8837 | `				break;` |
|        - |  8838 | `			}` |
|      ! 0 |  8839 | `		}` |
|       15 |  8840 | `	}` |
|        - |  8841 | `	/* Execute the cached block if available */` |
|      508 |  8842 | `	if( pCatch == 0 ){` |
|        - |  8843 | `		sxi32 rc;` |
|        - |  8844 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 |  8845 | `		if( pException && pException->iHasFinally ){` |
|        3 |  8846 | `			pException->iFinallyDone = 1;` |
|        3 |  8847 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 |  8848 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8849 | `				return SXERR_ABORT;` |
|        - |  8850 | `			}` |
|        1 |  8851 | `		}` |
|        - |  8852 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 |  8853 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8854 | `			/* Re-throw to the outer handler */` |
|        3 |  8855 | `			return VmThrowException(&(*pVm),pThis);` |
|        - |  8856 | `		}` |
|        - |  8857 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - |  8858 | `		 * (catch body re-throw with finally pending), defer the` |
|        - |  8859 | `		 * exception instead of reporting it uncaught.` |
|        - |  8860 | `		 */` |
|      478 |  8861 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - |  8862 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - |  8863 | `			 * by looking for a catch frame on the stack.` |
|        - |  8864 | `			 */` |
|      478 |  8865 | `			VmFrame *pF = pVm->pFrame;` |
|      478 |  8866 | `			int inCatch = 0;` |
|      956 |  8867 | `			while( pF ){` |
|      484 |  8868 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 |  8869 | `					inCatch = 1;` |
|        6 |  8870 | `					break;` |
|        - |  8871 | `				}` |
|      479 |  8872 | `				pF = pF->pParent;` |
|        1 |  8873 | `			}` |
|      478 |  8874 | `			if( inCatch ){` |
|        - |  8875 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 |  8876 | `				pThis->iRef++;` |
|        6 |  8877 | `				pVm->pPendingException = pThis;` |
|        6 |  8878 | `				return SXRET_OK;` |
|        - |  8879 | `			}` |
|      236 |  8880 | `		}` |
|        - |  8881 | `		/* Truly uncaught */` |
|      473 |  8882 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8883 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8884 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8885 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  8886 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8887 | `			}` |
|      ! 0 |  8888 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 |  8889 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8890 | `			}` |
|      ! 0 |  8891 | `		}` |
|      473 |  8892 | `		return rc;` |
|      ! 0 |  8893 | `	}else{` |
|       30 |  8894 | `		VmFrame *pFrame = pVm->pFrame;` |
|       30 |  8895 | `		ph7_exception **apSaved = 0;` |
|        - |  8896 | `		sxu32 nSavedCount;` |
|        - |  8897 | `		sxi32 rc;` |
|       58 |  8898 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|       30 |  8899 | `			pFrame = pFrame->pParent;` |
|        2 |  8900 | `		}` |
|       30 |  8901 | `		if( pException->pFrame == pFrame ){` |
|       22 |  8902 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       10 |  8903 | `		}` |
|        - |  8904 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - |  8905 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - |  8906 | `		 * our finally block. We save the stack contents and restore after.` |
|        - |  8907 | `		 */` |
|       30 |  8908 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       30 |  8909 | `		if( nSavedCount > 0 ){` |
|       11 |  8910 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 |  8911 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8912 | `			if( apSaved ){` |
|       11 |  8913 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 |  8914 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8915 | `				SySetReset(&pVm->aException);` |
|        3 |  8916 | `			}` |
|        3 |  8917 | `		}` |
|        - |  8918 | `		/* Create a private frame first */` |
|       30 |  8919 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       30 |  8920 | `		if( rc == SXRET_OK ){` |
|       30 |  8921 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       30 |  8922 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       30 |  8923 | `			if( pObj ){` |
|       30 |  8924 | `				pThis->iRef++;` |
|       30 |  8925 | `				pObj->x.pOther = pThis;` |
|       30 |  8926 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       14 |  8927 | `			}` |
|        - |  8928 | `			/* Execute the catch block */` |
|       30 |  8929 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8930 | `			/* Leave the frame */` |
|       30 |  8931 | `			VmLeaveFrame(&(*pVm));` |
|       14 |  8932 | `		}` |
|        - |  8933 | `		/* Restore the outer exception handlers */` |
|       30 |  8934 | `		if( apSaved ){` |
|        - |  8935 | `			sxu32 k;` |
|        - |  8936 | `			/* Any new entries pushed during catch execution (from nested` |
|        - |  8937 | `			 * try blocks inside the catch body) are already consumed.` |
|        - |  8938 | `			 * Restore the original outer entries.` |
|        - |  8939 | `			 */` |
|        8 |  8940 | `			SySetReset(&pVm->aException);` |
|       14 |  8941 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 |  8942 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 |  8943 | `			}` |
|        8 |  8944 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 |  8945 | `		}` |
|        - |  8946 | `		/* Execute the finally block after catch */` |
|       30 |  8947 | `		if( pException->iHasFinally ){` |
|        9 |  8948 | `			pException->iFinallyDone = 1;` |
|        - |  8949 | `			{` |
|        9 |  8950 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        9 |  8951 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 |  8952 | `					return SXERR_ABORT;` |
|        - |  8953 | `				}` |
|        - |  8954 | `			}` |
|        4 |  8955 | `		}` |
|       30 |  8956 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  8957 | `			return SXERR_ABORT;` |
|        - |  8958 | `		}` |
|        - |  8959 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - |  8960 | `		 * pPendingException (because outer handlers were hidden).` |
|        - |  8961 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - |  8962 | `		 */` |
|       30 |  8963 | `		if( pVm->pPendingException ){` |
|        6 |  8964 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 |  8965 | `			pVm->pPendingException = 0;` |
|        6 |  8966 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - |  8967 | `		}` |
|        - |  8968 | `	}` |
|        - |  8969 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8970 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8971 | `	 */` |
|       26 |  8972 | `	return SXRET_OK;` |
|      255 |  8973 |  |
|        - |  8974 | `/*` |
|        - |  8975 | ` * Section:` |
|        - |  8976 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8977 | ` * Status:` |
|        - |  8978 | ` *    Stable.` |
|        - |  8979 | ` */` |
|        - |  8980 | `/*` |
|        - |  8981 | ` * string ph7version(void)` |
|        - |  8982 | ` *  Returns the running version of the PH7 version.` |
|        - |  8983 | ` * Parameters` |
|        - |  8984 | ` *  None` |
|        - |  8985 | ` * Return` |
|        - |  8986 | ` * Current PH7 version.` |
|        - |  8987 | ` */` |
|        2 |  8988 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8989 |  |
|        1 |  8990 | `	SXUNUSED(nArg);` |
|        1 |  8991 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8992 | `	/* Current engine version */` |
|        3 |  8993 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8994 | `	return PH7_OK;` |
|        1 |  8995 |  |
|        - |  8996 | `/*` |
|        - |  8997 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8998 | ` */` |
|        - |  8999 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9000 | ` "<html><head>"\` |
|        - |  9001 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9002 | ` "<style type=\"text/css\">"\` |
|        - |  9003 | ` "div {"\` |
|        - |  9004 | `     "border: 1px solid #cccccc;"\` |
|        - |  9005 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9006 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9007 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9008 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9009 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9010 | `     "-o-border-radius: 10px;"\` |
|        - |  9011 | `     "border-radius: 10px;"\` |
|        - |  9012 | `     "padding-left: 2em;"\` |
|        - |  9013 | `     "background-color: white;"\` |
|        - |  9014 | `     "margin-left: auto;"\` |
|        - |  9015 | `     "font-family: verdana;"\` |
|        - |  9016 | `     "padding-right: 2em;"\` |
|        - |  9017 | `     "margin-right: auto;"\` |
|        - |  9018 | `     "}"\` |
|        - |  9019 | `     "body {"\` |
|        - |  9020 | `     "padding: 0.2em;"\` |
|        - |  9021 | `     "font-style: normal;"\` |
|        - |  9022 | `     "font-size: medium;"\` |
|        - |  9023 | `     "background-color: #f2f2f2;"\` |
|        - |  9024 | `     "}"\` |
|        - |  9025 | `     "hr {"\` |
|        - |  9026 | `     "border-style: solid none none;"\` |
|        - |  9027 | `     "border-width: 1px medium medium;"\` |
|        - |  9028 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9029 | `     "height: 1px;"\` |
|        - |  9030 | `     "}"\` |
|        - |  9031 | `     "a {"\` |
|        - |  9032 | `     "color: #3366cc;"\` |
|        - |  9033 | `     "text-decoration: none;"\` |
|        - |  9034 | `     "}"\` |
|        - |  9035 | `     "a:hover {"\` |
|        - |  9036 | `     "color: #999999;"\` |
|        - |  9037 | `     "}"\` |
|        - |  9038 | `     "a:active {"\` |
|        - |  9039 | `     "color: #663399;"\` |
|        - |  9040 | `     "}"\` |
|        - |  9041 | `     "h1 {"\` |
|        - |  9042 | `     "margin: 0;"\` |
|        - |  9043 | `     "padding: 0;"\` |
|        - |  9044 | `     "font-family: Verdana;"\` |
|        - |  9045 | `     "font-weight: bold;"\` |
|        - |  9046 | `     "font-style: normal;"\` |
|        - |  9047 | `     "font-size: medium;"\` |
|        - |  9048 | `     "text-transform: capitalize;"\` |
|        - |  9049 | `     "color: #0a328c;"\` |
|        - |  9050 | `     "}"\` |
|        - |  9051 | `     "p {"\` |
|        - |  9052 | `     "margin: 0 auto;"\` |
|        - |  9053 | `     "font-size: medium;"\` |
|        - |  9054 | `     "font-style: normal;"\` |
|        - |  9055 | `     "font-family: verdana;"\` |
|        - |  9056 | `     "}"\` |
|        - |  9057 | `"</style></head><body>"\` |
|        - |  9058 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  9059 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  9060 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  9061 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  9062 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  9063 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  9064 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  9065 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  9066 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  9067 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  9068 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  9069 |  |
|        - |  9070 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9071 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  9072 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  9073 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9074 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9075 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9076 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9077 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9078 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9079 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9080 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9081 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9082 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9083 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9084 |  |
|        - |  9085 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9086 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9087 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9088 | `"&nbsp;*<br>"\` |
|        - |  9089 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9090 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9091 | `"&nbsp;* are met:<br>"\` |
|        - |  9092 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9093 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9094 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9095 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9096 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9097 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9098 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9099 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9100 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9101 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9102 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9103 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9104 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9105 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9106 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9107 | `"&nbsp;*<br>"\` |
|        - |  9108 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9109 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9110 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9111 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9112 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9113 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9114 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9115 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9116 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9117 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9118 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9119 | `"&nbsp;*/<br>"\` |
|        - |  9120 | `"</span></small></small></p>"\` |
|        - |  9121 | `"</div></body></html>"` |
|        - |  9122 | `/*` |
|        - |  9123 | ` * bool ph7credits(void)` |
|        - |  9124 | ` * bool ph7info(void)` |
|        - |  9125 | ` * bool ph7copyright(void)` |
|        - |  9126 | ` *  Prints out the credits for PH7 engine` |
|        - |  9127 | ` * Parameters` |
|        - |  9128 | ` *  None` |
|        - |  9129 | ` * Return` |
|        - |  9130 | ` *  Always TRUE` |
|        - |  9131 | ` */` |
|        2 |  9132 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9133 |  |
|        3 |  9134 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9135 | `	/* Expand the HTML page above*/` |
|        3 |  9136 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9137 | `	ph7_context_output_format(` |
|        1 |  9138 | `		pCtx,` |
|        - |  9139 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9140 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9141 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9142 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9143 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9144 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9145 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9146 | `#ifdef __WINNT__` |
|        - |  9147 | `		"Windows NT"` |
|        - |  9148 | `#elif defined(__UNIXES__)` |
|        - |  9149 | `		"UNIX-Like"` |
|        - |  9150 | `#else` |
|        - |  9151 | `		"Other OS"` |
|        - |  9152 | `#endif` |
|        - |  9153 | `		);` |
|        3 |  9154 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9155 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9156 | `	SXUNUSED(apArg);` |
|        - |  9157 | `	/* Return TRUE */` |
|        - |  9158 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9159 | `	return PH7_OK;` |
|        1 |  9160 |  |
|        - |  9161 | `/*` |
|        - |  9162 | ` * Section:` |
|        - |  9163 | ` *    URL related routines.` |
|        - |  9164 | ` * Status:` |
|        - |  9165 | ` *    Stable.` |
|        - |  9166 | ` */` |
|        - |  9167 | `/*` |
|        - |  9168 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9169 | ` *  Parse a URL and return its fields.` |
|        - |  9170 | ` * Parameters` |
|        - |  9171 | ` *  $url` |
|        - |  9172 | ` *   The URL to parse.` |
|        - |  9173 | ` * $component` |
|        - |  9174 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9175 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9176 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9177 | ` *  in which case the return value will be an integer).` |
|        - |  9178 | ` * Return` |
|        - |  9179 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9180 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9181 | ` *  this array are:` |
|        - |  9182 | ` *   scheme - e.g. http` |
|        - |  9183 | ` *   host` |
|        - |  9184 | ` *   port` |
|        - |  9185 | ` *   user` |
|        - |  9186 | ` *   pass` |
|        - |  9187 | ` *   path` |
|        - |  9188 | ` *   query - after the question mark ?` |
|        - |  9189 | ` *   fragment - after the hashmark #` |
|        - |  9190 | ` * Note:` |
|        - |  9191 | ` *  FALSE is returned on failure.` |
|        - |  9192 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9193 | ` *  with the standard PHP engine.` |
|        - |  9194 | ` */` |
|       28 |  9195 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9196 |  |
|        - |  9197 | `	const char *zStr; /* Input string */` |
|        - |  9198 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9199 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9200 | `	int nLen;` |
|        - |  9201 | `	sxi32 rc;` |
|       29 |  9202 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9203 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9204 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9205 | `		return PH7_OK;` |
|        - |  9206 | `	}` |
|        - |  9207 | `	/* Extract the given URI */` |
|       29 |  9208 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9209 | `	if( nLen < 1 ){` |
|        - |  9210 | `		/* Nothing to process,return FALSE */` |
|        3 |  9211 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9212 | `		return PH7_OK;` |
|        - |  9213 | `	}` |
|        - |  9214 | `	/* Get a parse */` |
|       27 |  9215 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9216 | `	if( rc != SXRET_OK ){` |
|        - |  9217 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9218 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9219 | `		return PH7_OK;` |
|        - |  9220 | `	}` |
|       27 |  9221 | `	if( nArg > 1 ){` |
|      ! 0 |  9222 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9223 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9224 | `		switch(nComponent){` |
|      ! 0 |  9225 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9226 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9227 | `			if( pComp->nByte < 1 ){` |
|        - |  9228 | `				/* No available value,return NULL */` |
|      ! 0 |  9229 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9230 | `			}else{` |
|      ! 0 |  9231 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9232 | `			}` |
|      ! 0 |  9233 | `			break;` |
|      ! 0 |  9234 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9235 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9236 | `			if( pComp->nByte < 1 ){` |
|        - |  9237 | `				/* No available value,return NULL */` |
|      ! 0 |  9238 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9239 | `			}else{` |
|      ! 0 |  9240 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9241 | `			}` |
|      ! 0 |  9242 | `			break;` |
|      ! 0 |  9243 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9244 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9245 | `			if( pComp->nByte < 1 ){` |
|        - |  9246 | `				/* No available value,return NULL */` |
|      ! 0 |  9247 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9248 | `			}else{` |
|      ! 0 |  9249 | `				int iPort = 0;` |
|        - |  9250 | `				/* Cast the value to integer */` |
|      ! 0 |  9251 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9252 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9253 | `			}` |
|      ! 0 |  9254 | `			break;` |
|      ! 0 |  9255 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9256 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9257 | `			if( pComp->nByte < 1 ){` |
|        - |  9258 | `				/* No available value,return NULL */` |
|      ! 0 |  9259 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9260 | `			}else{` |
|      ! 0 |  9261 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9262 | `			}` |
|      ! 0 |  9263 | `			break;` |
|      ! 0 |  9264 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9265 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9266 | `			if( pComp->nByte < 1 ){` |
|        - |  9267 | `				/* No available value,return NULL */` |
|      ! 0 |  9268 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9269 | `			}else{` |
|      ! 0 |  9270 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9271 | `			}` |
|      ! 0 |  9272 | `			break;` |
|      ! 0 |  9273 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9274 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9275 | `			if( pComp->nByte < 1 ){` |
|        - |  9276 | `				/* No available value,return NULL */` |
|      ! 0 |  9277 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9278 | `			}else{` |
|      ! 0 |  9279 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9280 | `			}` |
|      ! 0 |  9281 | `			break;` |
|      ! 0 |  9282 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9283 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9284 | `			if( pComp->nByte < 1 ){` |
|        - |  9285 | `				/* No available value,return NULL */` |
|      ! 0 |  9286 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9287 | `			}else{` |
|      ! 0 |  9288 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9289 | `			}` |
|      ! 0 |  9290 | `			break;` |
|      ! 0 |  9291 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9292 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9293 | `			if( pComp->nByte < 1 ){` |
|        - |  9294 | `				/* No available value,return NULL */` |
|      ! 0 |  9295 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9296 | `			}else{` |
|      ! 0 |  9297 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9298 | `			}` |
|      ! 0 |  9299 | `			break;` |
|      ! 0 |  9300 | `		default:` |
|        - |  9301 | `			/* No such entry,return NULL */` |
|      ! 0 |  9302 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9303 | `			break;` |
|        - |  9304 | `		}` |
|      ! 0 |  9305 | `	}else{` |
|        - |  9306 | `		ph7_value *pArray,*pValue;` |
|        - |  9307 | `		/* Return an associative array */` |
|       27 |  9308 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9309 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9310 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9311 | `			/* Out of memory */` |
|      ! 0 |  9312 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9313 | `			/* Return false */` |
|      ! 0 |  9314 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9315 | `			return PH7_OK;` |
|        - |  9316 | `		}` |
|        - |  9317 | `		/* Fill the array */` |
|       27 |  9318 | `		pComp = &sURI.sScheme;` |
|       27 |  9319 | `		if( pComp->nByte > 0 ){` |
|       19 |  9320 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9321 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9322 | `		}` |
|        - |  9323 | `		/* Reset the string cursor */` |
|       27 |  9324 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9325 | `		pComp = &sURI.sHost;` |
|       27 |  9326 | `		if( pComp->nByte > 0 ){` |
|       25 |  9327 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9328 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9329 | `		}` |
|        - |  9330 | `		/* Reset the string cursor */` |
|       27 |  9331 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9332 | `		pComp = &sURI.sPort;` |
|       27 |  9333 | `		if( pComp->nByte > 0 ){` |
|       11 |  9334 | `			int iPort = 0;/* cc warning */` |
|        - |  9335 | `			/* Convert to integer */` |
|       11 |  9336 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9337 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9338 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9339 | `		}` |
|        - |  9340 | `		/* Reset the string cursor */` |
|       27 |  9341 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9342 | `		pComp = &sURI.sUser;` |
|       27 |  9343 | `		if( pComp->nByte > 0 ){` |
|        7 |  9344 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9345 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9346 | `		}` |
|        - |  9347 | `		/* Reset the string cursor */` |
|       27 |  9348 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9349 | `		pComp = &sURI.sPass;` |
|       27 |  9350 | `		if( pComp->nByte > 0 ){` |
|        7 |  9351 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9352 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9353 | `		}` |
|        - |  9354 | `		/* Reset the string cursor */` |
|       27 |  9355 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9356 | `		pComp = &sURI.sPath;` |
|       27 |  9357 | `		if( pComp->nByte > 0 ){` |
|       17 |  9358 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9359 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9360 | `		}` |
|        - |  9361 | `		/* Reset the string cursor */` |
|       27 |  9362 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9363 | `		pComp = &sURI.sQuery;` |
|       27 |  9364 | `		if( pComp->nByte > 0 ){` |
|        5 |  9365 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9366 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9367 | `		}` |
|        - |  9368 | `		/* Reset the string cursor */` |
|       27 |  9369 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9370 | `		pComp = &sURI.sFragment;` |
|       27 |  9371 | `		if( pComp->nByte > 0 ){` |
|        5 |  9372 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9373 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9374 | `		}` |
|        - |  9375 | `		/* Return the created array */` |
|       27 |  9376 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9377 | `		/* NOTE:` |
|        - |  9378 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9379 | `		 * automatically as soon we return from this function.` |
|        - |  9380 | `		 */` |
|        - |  9381 | `	}` |
|        - |  9382 | `	/* All done */` |
|       27 |  9383 | `	return PH7_OK;` |
|       15 |  9384 |  |
|        - |  9385 | `/*` |
|        - |  9386 | ` * Section:` |
|        - |  9387 | ` *   Array related routines.` |
|        - |  9388 | ` * Status:` |
|        - |  9389 | ` *    Stable.` |
|        - |  9390 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9391 | ` *  Array related functions that need access to the underlying` |
|        - |  9392 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9393 | ` */` |
|        - |  9394 | `/*` |
|        - |  9395 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9396 | ` * of the following structure.` |
|        - |  9397 | ` */` |
|        - |  9398 | `struct compact_data` |
|        - |  9399 |  |
|        - |  9400 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9401 | `	int nRecCount;      /* Recursion count */` |
|        - |  9402 | `};` |
|        - |  9403 | `/*` |
|        - |  9404 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9405 | ` */` |
|      ! 0 |  9406 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9407 |  |
|      ! 0 |  9408 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9409 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9410 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9411 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9412 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9413 | `		SyString sVar;` |
|      ! 0 |  9414 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9415 | `		if( sVar.nByte > 0 ){` |
|        - |  9416 | `			/* Query the current frame */` |
|      ! 0 |  9417 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9418 | `			/* ^` |
|        - |  9419 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9420 | `			 */` |
|      ! 0 |  9421 | `			if( pKey ){` |
|        - |  9422 | `				/* Perform the insertion */` |
|      ! 0 |  9423 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9424 | `			}` |
|      ! 0 |  9425 | `		}` |
|      ! 0 |  9426 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9427 | `		int rc;` |
|        - |  9428 | `		/* Recursively traverse this array */` |
|      ! 0 |  9429 | `		pData->nRecCount++;` |
|      ! 0 |  9430 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9431 | `		pData->nRecCount--;` |
|      ! 0 |  9432 | `		return rc;` |
|        - |  9433 | `	}` |
|      ! 0 |  9434 | `	return SXRET_OK;` |
|      ! 0 |  9435 |  |
|        - |  9436 | `/*` |
|        - |  9437 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9438 | ` *  Create array containing variables and their values.` |
|        - |  9439 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9440 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9441 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9442 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9443 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9444 | ` * Parameters` |
|        - |  9445 | ` *  $varname` |
|        - |  9446 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9447 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9448 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9449 | ` *   it recursively.` |
|        - |  9450 | ` * Return` |
|        - |  9451 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9452 | ` */` |
|        2 |  9453 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9454 |  |
|        - |  9455 | `	ph7_value *pArray,*pObj;` |
|        3 |  9456 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9457 | `	const char *zName;` |
|        - |  9458 | `	SyString sVar;` |
|        - |  9459 | `	int i,nLen;` |
|        3 |  9460 | `	if( nArg < 1 ){` |
|        - |  9461 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9462 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9463 | `		return PH7_OK;` |
|        - |  9464 | `	}` |
|        - |  9465 | `	/* Create the array */` |
|        3 |  9466 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9467 | `	if( pArray == 0 ){` |
|        - |  9468 | `		/* Out of memory */` |
|      ! 0 |  9469 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9470 | `		/* Return NULL */` |
|      ! 0 |  9471 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9472 | `		return PH7_OK;` |
|        - |  9473 | `	}` |
|        - |  9474 | `	/* Perform the requested operation */` |
|        7 |  9475 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9476 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9477 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9478 | `				struct compact_data sData;` |
|      ! 0 |  9479 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9480 | `				/* Recursively walk the array */` |
|      ! 0 |  9481 | `				sData.nRecCount = 0;` |
|      ! 0 |  9482 | `				sData.pArray = pArray;` |
|      ! 0 |  9483 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9484 | `			}` |
|      ! 0 |  9485 | `		}else{` |
|        - |  9486 | `			/* Extract variable name */` |
|        5 |  9487 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9488 | `			if( nLen > 0 ){` |
|        5 |  9489 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9490 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9491 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9492 | `				if( pObj ){` |
|        5 |  9493 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9494 | `				}` |
|        2 |  9495 | `			}` |
|        - |  9496 | `		}` |
|        3 |  9497 | `	}` |
|        - |  9498 | `	/* Return the array */` |
|        3 |  9499 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9500 | `	return PH7_OK;` |
|        2 |  9501 |  |
|        - |  9502 | `/*` |
|        - |  9503 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9504 | ` * of the following structure.` |
|        - |  9505 | ` */` |
|        - |  9506 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9507 | `struct extract_aux_data` |
|        - |  9508 |  |
|        - |  9509 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9510 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9511 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9512 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9513 | `	int iFlags;           /* Control flags */` |
|        - |  9514 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9515 | `};` |
|        - |  9516 | `/* Forward declaration */` |
|        - |  9517 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9518 | `/*` |
|        - |  9519 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9520 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9521 | ` * Parameters` |
|        - |  9522 | ` * $var_array` |
|        - |  9523 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9524 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9525 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9526 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9527 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9528 | ` * $extract_type` |
|        - |  9529 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9530 | ` *  It can be one of the following values:` |
|        - |  9531 | ` *   EXTR_OVERWRITE` |
|        - |  9532 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9533 | ` *   EXTR_SKIP` |
|        - |  9534 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9535 | ` *   EXTR_PREFIX_SAME` |
|        - |  9536 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9537 | ` *   EXTR_PREFIX_ALL` |
|        - |  9538 | ` *       Prefix all variable names with prefix.` |
|        - |  9539 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9540 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9541 | ` *   EXTR_IF_EXISTS` |
|        - |  9542 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9543 | ` *       otherwise do nothing.` |
|        - |  9544 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9545 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9546 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9547 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9548 | ` *      the current symbol table.` |
|        - |  9549 | ` * $prefix` |
|        - |  9550 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9551 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9552 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9553 | ` *  underscore character.` |
|        - |  9554 | ` * Return` |
|        - |  9555 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9556 | ` */` |
|        4 |  9557 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9558 |  |
|        - |  9559 | `	extract_aux_data sAux;` |
|        - |  9560 | `	ph7_hashmap *pMap;` |
|        5 |  9561 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9562 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9563 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9564 | `		return PH7_OK;` |
|        - |  9565 | `	}` |
|        - |  9566 | `	/* Point to the target hashmap */` |
|        5 |  9567 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9568 | `	if( pMap->nEntry < 1 ){` |
|        - |  9569 | `		/* Empty map,return  0 */` |
|      ! 0 |  9570 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9571 | `		return PH7_OK;` |
|        - |  9572 | `	}` |
|        - |  9573 | `	/* Prepare the aux data */` |
|        5 |  9574 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9575 | `	if( nArg > 1 ){` |
|        3 |  9576 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9577 | `		if( nArg > 2 ){` |
|      ! 0 |  9578 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9579 | `		}` |
|        1 |  9580 | `	}` |
|        5 |  9581 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9582 | `	/* Invoke the worker callback */` |
|        5 |  9583 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9584 | `	/* Number of variables successfully imported */` |
|        5 |  9585 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9586 | `	return PH7_OK;` |
|        3 |  9587 |  |
|        - |  9588 | `/*` |
|        - |  9589 | ` * Worker callback for the [extract()] function defined` |
|        - |  9590 | ` * below.` |
|        - |  9591 | ` */` |
|        8 |  9592 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9593 |  |
|        9 |  9594 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9595 | `	int iFlags = pAux->iFlags;` |
|        9 |  9596 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9597 | `	ph7_value *pObj;` |
|        - |  9598 | `	SyString sVar;` |
|        9 |  9599 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9600 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9601 | `	}` |
|        - |  9602 | `	/* Perform a string cast */` |
|        9 |  9603 | `	PH7_MemObjToString(pKey);` |
|        9 |  9604 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9605 | `		/* Unavailable variable name */` |
|      ! 0 |  9606 | `		return SXRET_OK;` |
|        - |  9607 | `	}` |
|        9 |  9608 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9609 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9610 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9611 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9612 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9613 | `			);` |
|      ! 0 |  9614 | `	}else{` |
|       13 |  9615 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9616 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9617 | `	}` |
|        9 |  9618 | `	sVar.zString = pAux->zWorker;` |
|        - |  9619 | `	/* Try to extract the variable */` |
|        9 |  9620 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9621 | `	if( pObj ){` |
|        - |  9622 | `		/* Collision */` |
|        5 |  9623 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9624 | `			return SXRET_OK;` |
|        - |  9625 | `		}` |
|        5 |  9626 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9627 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9628 | `				/* Already prefixed */` |
|      ! 0 |  9629 | `				return SXRET_OK;` |
|        - |  9630 | `			}` |
|      ! 0 |  9631 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9632 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9633 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9634 | `				);` |
|      ! 0 |  9635 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9636 | `		}` |
|        3 |  9637 | `	}else{` |
|        - |  9638 | `		/* Create the variable */` |
|        5 |  9639 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9640 | `	}` |
|        9 |  9641 | `	if( pObj ){` |
|        - |  9642 | `		/* Overwrite the old value */` |
|        9 |  9643 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9644 | `		/* Increment counter */` |
|        9 |  9645 | `		pAux->iCount++;` |
|        4 |  9646 | `	}` |
|        9 |  9647 | `	return SXRET_OK;` |
|        5 |  9648 |  |
|        - |  9649 | `/*` |
|        - |  9650 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9651 | ` * defined below.` |
|        - |  9652 | ` */` |
|        2 |  9653 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9654 |  |
|        3 |  9655 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9656 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9657 | `	ph7_value *pObj;` |
|        - |  9658 | `	SyString sVar;` |
|        - |  9659 | `	/* Perform a string cast */` |
|        3 |  9660 | `	PH7_MemObjToString(pKey);` |
|        3 |  9661 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9662 | `		/* Unavailable variable name */` |
|      ! 0 |  9663 | `		return SXRET_OK;` |
|        - |  9664 | `	}` |
|        3 |  9665 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9666 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9667 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9668 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9669 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9670 | `			);` |
|        2 |  9671 | `	}else{` |
|      ! 0 |  9672 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9673 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9674 | `	}` |
|        3 |  9675 | `	sVar.zString = pAux->zWorker;` |
|        - |  9676 | `	/* Extract the variable */` |
|        3 |  9677 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9678 | `	if( pObj ){` |
|        3 |  9679 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9680 | `	}` |
|        3 |  9681 | `	return SXRET_OK;` |
|        2 |  9682 |  |
|        - |  9683 | `/*` |
|        - |  9684 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9685 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9686 | ` * Parameters` |
|        - |  9687 | ` * $types` |
|        - |  9688 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9689 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9690 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9691 | ` *  POST includes the POST uploaded file information.` |
|        - |  9692 | ` *  Note:` |
|        - |  9693 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9694 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9695 | ` * $prefix` |
|        - |  9696 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9697 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9698 | ` *  variable named $pref_userid.` |
|        - |  9699 | ` * Return` |
|        - |  9700 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9701 | ` */` |
|        2 |  9702 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9703 |  |
|        - |  9704 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9705 | `	extract_aux_data sAux;` |
|        - |  9706 | `	int nLen,nPrefixLen;` |
|        - |  9707 | `	ph7_value *pSuper;` |
|        - |  9708 | `	ph7_vm *pVm;` |
|        - |  9709 | `	/* By default import only $_GET variables  */` |
|        3 |  9710 | `	zImport = "G";` |
|        3 |  9711 | `	nLen = (int)sizeof(char);` |
|        3 |  9712 | `	zPrefix = 0;` |
|        3 |  9713 | `	nPrefixLen = 0;` |
|        3 |  9714 | `	if( nArg > 0 ){` |
|        3 |  9715 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9716 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9717 | `		}` |
|        3 |  9718 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9719 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9720 | `		}` |
|        1 |  9721 | `	}` |
|        - |  9722 | `	/* Point to the underlying VM */` |
|        3 |  9723 | `	pVm = pCtx->pVm;` |
|        - |  9724 | `	/* Initialize the aux data */` |
|        3 |  9725 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9726 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9727 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9728 | `	sAux.pVm = pVm;` |
|        - |  9729 | `	/* Extract */` |
|        3 |  9730 | `	zEnd = &zImport[nLen];` |
|        5 |  9731 | `	while( zImport < zEnd ){` |
|        3 |  9732 | `		int c = zImport[0];` |
|        3 |  9733 | `		pSuper = 0;` |
|        3 |  9734 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9735 | `			/* Import $_GET variables */` |
|        3 |  9736 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9737 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9738 | `			/* Import $_POST variables */` |
|      ! 0 |  9739 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9740 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9741 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9742 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9743 | `		}` |
|        3 |  9744 | `		if( pSuper ){` |
|        - |  9745 | `			/* Iterate throw array entries */` |
|        3 |  9746 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9747 | `		}` |
|        - |  9748 | `		/* Advance the cursor */` |
|        3 |  9749 | `		zImport++;` |
|        1 |  9750 | `	}` |
|        - |  9751 | `	/* All done,return TRUE*/` |
|        3 |  9752 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9753 | `	return PH7_OK;` |
|        1 |  9754 |  |
|        - |  9755 | `/*` |
|        - |  9756 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9757 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9758 | ` * information.` |
|        - |  9759 | ` */` |
|    10034 |  9760 | `static sxi32 VmEvalChunk(` |
|        - |  9761 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9762 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9763 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9764 | `	int iFlags,         /* Compile flag */` |
|        - |  9765 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9766 | `	)` |
|        2 |  9767 |  |
|        - |  9768 | `	SySet *pByteCode,aByteCode;` |
|        - |  9769 | `	SyBlob sSavedNs;` |
|    10036 |  9770 | `	ProcConsumer xErr = 0;` |
|    10036 |  9771 | `	void *pErrData = 0;` |
|        - |  9772 | `	/* Initialize bytecode container */` |
|    10036 |  9773 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10036 |  9774 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9775 | `	/* Reset the code generator */` |
|    10036 |  9776 | `	if( bTrueReturn ){` |
|        - |  9777 | `		/* Included file,log compile-time errors */` |
|     7535 |  9778 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7535 |  9779 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3767 |  9780 | `	}` |
|    10036 |  9781 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9782 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |  9783 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |  9784 | `	 * the caller's namespace is restored. */` |
|    10036 |  9785 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10036 |  9786 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10036 |  9787 | `	if( bTrueReturn ){` |
|        - |  9788 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7535 |  9789 | `		SyBlobReset(&pVm->sNamespace);` |
|     3767 |  9790 | `	}` |
|        - |  9791 | `	/* Swap bytecode container */` |
|    10036 |  9792 | `	pByteCode = pVm->pByteContainer;` |
|    10036 |  9793 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9794 | `	/* Compile the chunk */` |
|    10036 |  9795 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15053 |  9796 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9797 | `		/* Compilation error,return false */` |
|        3 |  9798 | `		if( pCtx ){` |
|        3 |  9799 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9800 | `		}` |
|        2 |  9801 | `	}else{` |
|        - |  9802 | `		/* Mount any newly defined classes */` |
|        - |  9803 | `		SyHashEntry *pEntry;` |
|        - |  9804 | `		ph7_class *pClass;` |
|        - |  9805 | `		ph7_value sResult; /* Return value */` |
|        - |  9806 | `		sxi32 rc;` |
|    10034 |  9807 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   275454 |  9808 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   260406 |  9809 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9810 | `			/* Only mount classes that haven't been mounted yet */` |
|   260406 |  9811 | `			if( !pClass->bMounted ){` |
|    61924 |  9812 | `				rc = VmMountUserClass(pVm,pClass);` |
|    61924 |  9813 | `				if( rc != SXRET_OK ){` |
|        - |  9814 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9815 | `					if( pCtx ){` |
|      ! 0 |  9816 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9817 | `					}` |
|      ! 0 |  9818 | `					goto Cleanup;` |
|        - |  9819 | `				}` |
|    30961 |  9820 | `			}` |
|        2 |  9821 | `		}` |
|    10034 |  9822 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9823 | `			/* Out of memory */` |
|      ! 0 |  9824 | `			if( pCtx ){` |
|      ! 0 |  9825 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9826 | `			}` |
|      ! 0 |  9827 | `			goto Cleanup;` |
|        - |  9828 | `		}` |
|    10034 |  9829 | `		if( bTrueReturn ){` |
|        - |  9830 | `			/* Assume a boolean true return value */` |
|     7535 |  9831 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3768 |  9832 | `		}else{` |
|        - |  9833 | `			/* Assume a null return value */` |
|     2500 |  9834 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9835 | `		}` |
|        - |  9836 | `		/* Execute the compiled chunk */` |
|    10034 |  9837 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10034 |  9838 | `		if( pCtx ){` |
|        - |  9839 | `			/* Set the execution result */` |
|     7548 |  9840 | `			ph7_result_value(pCtx,&sResult);` |
|     3773 |  9841 | `		}` |
|    10034 |  9842 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9843 | `	}` |
|     5017 |  9844 | `Cleanup:` |
|        - |  9845 | `	/* Cleanup the mess left behind */` |
|    10036 |  9846 | `	pVm->pByteContainer = pByteCode;` |
|    10036 |  9847 | `	SySetRelease(&aByteCode);` |
|        - |  9848 | `	/* Restore caller's namespace state */` |
|    10036 |  9849 | `	SyBlobReset(&pVm->sNamespace);` |
|    10036 |  9850 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10036 |  9851 | `	SyBlobRelease(&sSavedNs);` |
|    10036 |  9852 | `	return SXRET_OK;` |
|        2 |  9853 |  |
|        - |  9854 | `/*` |
|        - |  9855 | ` * value eval(string $code)` |
|        - |  9856 | ` *   Evaluate a string as PHP code.` |
|        - |  9857 | ` * Parameter` |
|        - |  9858 | ` *  code: PHP code to evaluate.` |
|        - |  9859 | ` * Return` |
|        - |  9860 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9861 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9862 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9863 | ` */` |
|       16 |  9864 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9865 |  |
|        - |  9866 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9867 | `	if( nArg < 1 ){` |
|        - |  9868 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9869 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9870 | `		return SXRET_OK;` |
|        - |  9871 | `	}` |
|        - |  9872 | `	/* Chunk to evaluate */` |
|       18 |  9873 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9874 | `	if( sChunk.nByte < 1 ){` |
|        - |  9875 | `		/* Empty string,return NULL */` |
|        3 |  9876 | `		ph7_result_null(pCtx);` |
|        3 |  9877 | `		return SXRET_OK;` |
|        - |  9878 | `	}` |
|        - |  9879 | `	/* Eval the chunk */` |
|       16 |  9880 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9881 | `	return SXRET_OK;` |
|       10 |  9882 |  |
|        - |  9883 | `/*` |
|        - |  9884 | ` * Check if a file path is already included.` |
|        - |  9885 | ` */` |
|    15064 |  9886 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9887 |  |
|        - |  9888 | `	SyString *aEntries;` |
|        - |  9889 | `	sxu32 n;` |
|    15065 |  9890 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9891 | `	/* Perform a linear search */` |
| 56720651 |  9892 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56705593 |  9893 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9894 | `			/* Already included */` |
|        7 |  9895 | `			return TRUE;` |
|        - |  9896 | `		}` |
| 28352794 |  9897 | `	}` |
|    15059 |  9898 | `	return FALSE;` |
|     7533 |  9899 |  |
|        - |  9900 | `/*` |
|        - |  9901 | ` * Push a file path in the appropriate VM container.` |
|        - |  9902 | ` */` |
|    17542 |  9903 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9904 |  |
|        - |  9905 | `	SyString sPath;` |
|        - |  9906 | `	char *zDup;` |
|        - |  9907 | `#ifdef __WINNT__` |
|        - |  9908 | `	char *zCur;` |
|        - |  9909 | `#endif` |
|        - |  9910 | `	sxi32 rc;` |
|    17544 |  9911 | `	if( nLen < 0 ){` |
|     2480 |  9912 | `		nLen = SyStrlen(zPath);` |
|     1239 |  9913 | `	}` |
|        - |  9914 | `	/* Duplicate the file path first */` |
|    17544 |  9915 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17544 |  9916 | `	if( zDup == 0 ){` |
|      ! 0 |  9917 | `		return SXERR_MEM;` |
|        - |  9918 | `	}` |
|        - |  9919 | `#ifdef __WINNT__` |
|        - |  9920 | `	/* Normalize path on windows` |
|        - |  9921 | `	 * Example:` |
|        - |  9922 | `	 *    Path/To/File.php` |
|        - |  9923 | `	 * becomes` |
|        - |  9924 | `	 *   path\to\file.php` |
|        - |  9925 | `	 */` |
|        2 |  9926 | `	zCur = zDup;` |
|        2 |  9927 | `	while( zCur[0] != 0 ){` |
|        2 |  9928 | `		if( zCur[0] == '/' ){` |
|        2 |  9929 | `			zCur[0] = '\\';` |
|        2 |  9930 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9931 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9932 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9933 | `		}` |
|        2 |  9934 | `		zCur++;` |
|        2 |  9935 | `	}` |
|        - |  9936 | `#endif` |
|        - |  9937 | `	/* Install the file path */` |
|    17544 |  9938 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17544 |  9939 | `	if( !bMain ){` |
|    15065 |  9940 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9941 | `			/* Already included */` |
|        7 |  9942 | `			*pNew = 0;` |
|        4 |  9943 | `		}else{` |
|        - |  9944 | `			/* Insert in the corresponding container */` |
|    15059 |  9945 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15059 |  9946 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9947 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9948 | `				return rc;` |
|        - |  9949 | `			}` |
|    15059 |  9950 | `			*pNew = 1;` |
|        - |  9951 | `		}` |
|     7532 |  9952 | `	}` |
|    17544 |  9953 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17544 |  9954 | `	return SXRET_OK;` |
|     8773 |  9955 |  |
|        - |  9956 | `/*` |
|        - |  9957 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9958 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9959 | ` * indicates failure.` |
|        - |  9960 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9961 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9962 | ` * operations.` |
|        - |  9963 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9964 | ` * this function is a no-op.` |
|        - |  9965 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9966 | ` * constructs for more information.` |
|        - |  9967 | ` */` |
|     7540 |  9968 | `static sxi32 VmExecIncludedFile(` |
|        - |  9969 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9970 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9971 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9972 | `	 )` |
|        2 |  9973 |  |
|        - |  9974 | `	sxi32 rc;` |
|        - |  9975 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9976 | `	const ph7_io_stream *pStream;` |
|        - |  9977 | `	SyBlob sContents;` |
|        - |  9978 | `	void *pHandle;` |
|        - |  9979 | `	ph7_vm *pVm;` |
|        - |  9980 | `	int isNew;` |
|        - |  9981 | `	/* Initialize fields */` |
|     7542 |  9982 | `	pVm = pCtx->pVm;` |
|     7542 |  9983 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7542 |  9984 | `	isNew = 0;` |
|        - |  9985 | `	/* Extract the associated stream */` |
|     7542 |  9986 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9987 | `	/*` |
|        - |  9988 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9989 | `	 * in a read-only mode.` |
|        - |  9990 | `	 */` |
|     7542 |  9991 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7542 |  9992 | `	if( pHandle == 0 ){` |
|        3 |  9993 | `		return SXERR_IO;` |
|        - |  9994 | `	}` |
|     7539 |  9995 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7539 |  9996 | `	if( IncludeOnce && !isNew ){` |
|        - |  9997 | `		/* Already included */` |
|        5 |  9998 | `		rc = SXERR_EXISTS;` |
|        3 |  9999 | `	}else{` |
|        - | 10000 | `		/* Read the whole file contents */` |
|     7535 | 10001 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7535 | 10002 | `		if( rc == SXRET_OK ){` |
|        - | 10003 | `			SyString sScript;` |
|        - | 10004 | `			/* Compile and execute the script */` |
|     7535 | 10005 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7535 | 10006 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3767 | 10007 | `		}` |
|        - | 10008 | `	}` |
|        - | 10009 | `	/* Pop from the set of included file */` |
|     7539 | 10010 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10011 | `	/* Close the handle */` |
|     7539 | 10012 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10013 | `	/* Release the working buffer */` |
|     7539 | 10014 | `	SyBlobRelease(&sContents);` |
|        - | 10015 | `#else` |
|        - | 10016 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10017 | `	SXUNUSED(pPath);` |
|        - | 10018 | `	SXUNUSED(IncludeOnce);` |
|        - | 10019 | `	rc = SXERR_IO;` |
|        - | 10020 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7539 | 10021 | `	return rc;` |
|     3772 | 10022 |  |
|        - | 10023 | `/*` |
|        - | 10024 | ` * string get_include_path(void)` |
|        - | 10025 | ` *  Gets the current include_path configuration option.` |
|        - | 10026 | ` * Parameter` |
|        - | 10027 | ` *  None` |
|        - | 10028 | ` * Return` |
|        - | 10029 | ` *  Included paths as a string` |
|        - | 10030 | ` */` |
|        2 | 10031 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10032 |  |
|        3 | 10033 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10034 | `	SyString *aEntry;` |
|        - | 10035 | `	int dir_sep;` |
|        - | 10036 | `	sxu32 n;` |
|        - | 10037 | `#ifdef __WINNT__` |
|        1 | 10038 | `	dir_sep = ';';` |
|        - | 10039 | `#else` |
|        - | 10040 | `	/* Assume UNIX path separator */` |
|        2 | 10041 | `	dir_sep = ':';` |
|        - | 10042 | `#endif` |
|        1 | 10043 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10044 | `	SXUNUSED(apArg);` |
|        - | 10045 | `	/* Point to the list of import paths */` |
|        3 | 10046 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 10047 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 10048 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 10049 | `		if( n > 0 ){` |
|        - | 10050 | `			/* Append dir seprator */` |
|      ! 0 | 10051 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 10052 | `		}` |
|        - | 10053 | `		/* Append path */` |
|        3 | 10054 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 10055 | `	}` |
|        3 | 10056 | `	return PH7_OK;` |
|        1 | 10057 |  |
|        - | 10058 | `/*` |
|        - | 10059 | ` * string get_get_included_files(void)` |
|        - | 10060 | ` *  Gets the current include_path configuration option.` |
|        - | 10061 | ` * Parameter` |
|        - | 10062 | ` *  None` |
|        - | 10063 | ` * Return` |
|        - | 10064 | ` *  Included paths as a string` |
|        - | 10065 | ` */` |
|        2 | 10066 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10067 |  |
|        3 | 10068 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 10069 | `	ph7_value *pArray,*pWorker;` |
|        - | 10070 | `	SyString *pEntry;` |
|        - | 10071 | `	int c,d;` |
|        - | 10072 | `	/* Create an array and a working value */` |
|        3 | 10073 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10074 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10075 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10076 | `		/* Out of memory,return null */` |
|      ! 0 | 10077 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10078 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10079 | `		SXUNUSED(apArg);` |
|      ! 0 | 10080 | `		return PH7_OK;` |
|        - | 10081 | `	}` |
|        3 | 10082 | `	c = d = '/';` |
|        - | 10083 | `#ifdef __WINNT__` |
|        1 | 10084 | `	d = '\\';` |
|        - | 10085 | `#endif` |
|        - | 10086 | `	/* Iterate throw entries */` |
|        3 | 10087 | `	SySetResetCursor(pFiles);` |
|     3691 | 10088 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10089 | `		const char *zBase,*zEnd;` |
|        - | 10090 | `		int iLen;` |
|        - | 10091 | `		/* reset the string cursor */` |
|     3689 | 10092 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10093 | `		/* Extract base name */` |
|     3689 | 10094 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10095 | `		/* Ignore trailing '/' */` |
|     5533 | 10096 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10097 | `			zEnd--;` |
|      ! 0 | 10098 | `		}` |
|     3689 | 10099 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 | 10100 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 | 10101 | `			zEnd--;` |
|        1 | 10102 | `		}` |
|     3689 | 10103 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 | 10104 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10105 | `		/* Copy entry name */` |
|     3689 | 10106 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10107 | `		/* Perform the insertion */` |
|     3689 | 10108 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10109 | `	}` |
|        - | 10110 | `	/* All done,return the created array */` |
|        3 | 10111 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10112 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10113 | `	 * by the engine as soon we return from this foreign` |
|        - | 10114 | `	 * function.` |
|        - | 10115 | `	 */` |
|        3 | 10116 | `	return PH7_OK;` |
|        2 | 10117 |  |
|        - | 10118 | `/*` |
|        - | 10119 | ` * include:` |
|        - | 10120 | ` * According to the PHP reference manual.` |
|        - | 10121 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10122 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10123 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10124 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10125 | ` *  and the current working directory before failing. The include()` |
|        - | 10126 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10127 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10128 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10129 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10130 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10131 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10132 | ` *  directory to find the requested file.` |
|        - | 10133 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10134 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10135 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10136 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10137 | ` */` |
|     7528 | 10138 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10139 |  |
|        - | 10140 | `	SyString sFile;` |
|        - | 10141 | `	sxi32 rc;` |
|     7530 | 10142 | `	if( nArg < 1 ){` |
|        - | 10143 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10144 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10145 | `		return SXRET_OK;` |
|        - | 10146 | `	}` |
|        - | 10147 | `	/* File to include */` |
|     7530 | 10148 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7530 | 10149 | `	if( sFile.nByte < 1 ){` |
|        - | 10150 | `		/* Empty string,return NULL */` |
|      ! 0 | 10151 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10152 | `		return SXRET_OK;` |
|        - | 10153 | `	}` |
|        - | 10154 | `	/* Open,compile and execute the desired script */` |
|     7530 | 10155 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7530 | 10156 | `	if( rc != SXRET_OK ){` |
|        - | 10157 | `		/* Emit a warning and return false */` |
|        3 | 10158 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10159 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10160 | `	}` |
|     7530 | 10161 | `	return SXRET_OK;` |
|     3766 | 10162 |  |
|        - | 10163 | `/*` |
|        - | 10164 | ` * include_once:` |
|        - | 10165 | ` *  According to the PHP reference manual.` |
|        - | 10166 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10167 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10168 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10169 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10170 | ` *   just once.` |
|        - | 10171 | ` */` |
|        4 | 10172 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10173 |  |
|        - | 10174 | `	SyString sFile;` |
|        - | 10175 | `	sxi32 rc;` |
|        5 | 10176 | `	if( nArg < 1 ){` |
|        - | 10177 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10178 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10179 | `		return SXRET_OK;` |
|        - | 10180 | `	}` |
|        - | 10181 | `	/* File to include */` |
|        5 | 10182 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10183 | `	if( sFile.nByte < 1 ){` |
|        - | 10184 | `		/* Empty string,return NULL */` |
|      ! 0 | 10185 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10186 | `		return SXRET_OK;` |
|        - | 10187 | `	}` |
|        - | 10188 | `	/* Open,compile and execute the desired script */` |
|        5 | 10189 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10190 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10191 | `		/* File already included,return TRUE */` |
|        3 | 10192 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10193 | `		return SXRET_OK;` |
|        - | 10194 | `	}` |
|        3 | 10195 | `	if( rc != SXRET_OK ){` |
|        - | 10196 | `		/* Emit a warning and return false */` |
|      ! 0 | 10197 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10198 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10199 | ` 	}` |
|        3 | 10200 | `	return SXRET_OK;` |
|        3 | 10201 |  |
|        - | 10202 | `/*` |
|        - | 10203 | ` * require.` |
|        - | 10204 | ` *  According to the PHP reference manual.` |
|        - | 10205 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10206 | ` *   also produce a fatal level error.` |
|        - | 10207 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10208 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10209 | ` */` |
|        4 | 10210 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10211 |  |
|        - | 10212 | `	SyString sFile;` |
|        - | 10213 | `	sxi32 rc;` |
|        5 | 10214 | `	if( nArg < 1 ){` |
|        - | 10215 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10216 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10217 | `		return SXRET_OK;` |
|        - | 10218 | `	}` |
|        - | 10219 | `	/* File to include */` |
|        5 | 10220 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10221 | `	if( sFile.nByte < 1 ){` |
|        - | 10222 | `		/* Empty string,return NULL */` |
|      ! 0 | 10223 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10224 | `		return SXRET_OK;` |
|        - | 10225 | `	}` |
|        - | 10226 | `	/* Open,compile and execute the desired script */` |
|        5 | 10227 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10228 | `	if( rc != SXRET_OK ){` |
|        - | 10229 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10230 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10231 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10232 | `		return PH7_ABORT;` |
|        - | 10233 | `	}` |
|        5 | 10234 | `	return SXRET_OK;` |
|        3 | 10235 |  |
|        - | 10236 | `/*` |
|        - | 10237 | ` * require_once:` |
|        - | 10238 | ` *  According to the PHP reference manual.` |
|        - | 10239 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10240 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10241 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10242 | ` *   and how it differs from its non _once siblings.` |
|        - | 10243 | ` */` |
|        4 | 10244 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10245 |  |
|        - | 10246 | `	SyString sFile;` |
|        - | 10247 | `	sxi32 rc;` |
|        5 | 10248 | `	if( nArg < 1 ){` |
|        - | 10249 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10250 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10251 | `		return SXRET_OK;` |
|        - | 10252 | `	}` |
|        - | 10253 | `	/* File to include */` |
|        5 | 10254 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10255 | `	if( sFile.nByte < 1 ){` |
|        - | 10256 | `		/* Empty string,return NULL */` |
|      ! 0 | 10257 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10258 | `		return SXRET_OK;` |
|        - | 10259 | `	}` |
|        - | 10260 | `	/* Open,compile and execute the desired script */` |
|        5 | 10261 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10262 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10263 | `		/* File already included,return TRUE */` |
|        3 | 10264 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10265 | `		return SXRET_OK;` |
|        - | 10266 | `	}` |
|        3 | 10267 | `	if( rc != SXRET_OK ){` |
|        - | 10268 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10269 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10270 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10271 | `		return PH7_ABORT;` |
|        - | 10272 | `	}` |
|        3 | 10273 | `	return SXRET_OK;` |
|        3 | 10274 |  |
|        - | 10275 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10276 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10277 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10278 | `/* Table of built-in VM functions. */` |
|        - | 10279 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10280 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10281 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10282 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10283 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10284 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10285 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10286 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10287 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10288 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10289 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10290 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10291 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10292 | `	    /* Constants management */` |
|        - | 10293 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10294 | `	{ "define",   vm_builtin_define               },` |
|        - | 10295 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10296 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10297 | `	   /* Class/Object functions */` |
|        - | 10298 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10299 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10300 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10301 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10302 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10303 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10304 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10305 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10306 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10307 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10308 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10309 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10310 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10311 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10312 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10313 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10314 | `	   /* Random numbers/strings generators */` |
|        - | 10315 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10316 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10317 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10318 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10319 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10320 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10321 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10322 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10323 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10324 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10325 | `	   /* Language constructs functions */` |
|        - | 10326 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10327 | `	{ "print", vm_builtin_print                   },` |
|        - | 10328 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10329 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10330 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10331 | `	  /* Variable handling functions */` |
|        - | 10332 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10333 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10334 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10335 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10336 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10337 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10338 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10339 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10340 | `	  /* Ouput control functions */` |
|        - | 10341 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10342 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10343 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10344 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10345 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10346 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10347 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10348 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10349 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10350 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10351 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10352 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10353 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10354 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10355 | `	  /* Assertion functions */` |
|        - | 10356 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10357 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10358 | `	  /* Error reporting functions */` |
|        - | 10359 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10360 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10361 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10362 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10363 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10364 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10365 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10366 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10367 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10368 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10369 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10370 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10371 | `	  /* Release info */` |
|        - | 10372 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10373 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10374 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10375 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10376 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10377 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10378 | `	  /* hashmap */` |
|        - | 10379 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10380 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10381 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10382 | `	  /* URL related function */` |
|        - | 10383 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10384 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10385 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10386 | `	   /* XML processing functions */` |
|        - | 10387 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10388 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10389 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10390 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10391 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10392 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10393 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10394 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10395 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10396 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10397 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10398 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10399 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10400 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10401 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10402 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10403 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10404 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10405 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10406 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10407 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10408 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10409 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10410 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10411 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10412 | `	   /* Command line processing */` |
|        - | 10413 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10414 | `	   /* JSON encoding/decoding */` |
|        - | 10415 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10416 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10417 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10418 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10419 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10420 | `	   /* Files/URI inclusion facility */` |
|        - | 10421 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10422 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10423 | `	{ "include",      vm_builtin_include          },` |
|        - | 10424 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10425 | `	{ "require",      vm_builtin_require          },` |
|        - | 10426 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10427 | `};` |
|        - | 10428 | `/*` |
|        - | 10429 | ` * Register the built-in VM functions defined above.` |
|        - | 10430 | ` */` |
|     2236 | 10431 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10432 |  |
|        - | 10433 | `	sxi32 rc;` |
|        - | 10434 | `	sxu32 n;` |
|   279502 | 10435 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10436 | `		/* Note that these special functions have access` |
|        - | 10437 | `		 * to the underlying virtual machine as their` |
|        - | 10438 | `		 * private data.` |
|        - | 10439 | `		 */` |
|   277266 | 10440 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   277266 | 10441 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10442 | `			return rc;` |
|        - | 10443 | `		}` |
|   138634 | 10444 | `	}` |
|     2238 | 10445 | `	return SXRET_OK;` |
|     1120 | 10446 |  |
|        - | 10447 | `/*` |
|        - | 10448 | ` * Check if the given name refer to an installed class.` |
|        - | 10449 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10450 | ` */` |
|    16302 | 10451 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10452 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10453 | `	const char *zName,  /* Name of the target class */` |
|        - | 10454 | `	sxu32 nByte,        /* zName length */` |
|        - | 10455 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10456 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10457 | `						 */` |
|        - | 10458 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10459 | `	)` |
|        2 | 10460 |  |
|        - | 10461 | `	SyHashEntry *pEntry;` |
|        - | 10462 | `	ph7_class *pClass;` |
|     8151 | 10463 | `	SXUNUSED(iNest);` |
|        - | 10464 | `	/* Exact class lookup.` |
|        - | 10465 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 10466 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    16304 | 10467 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    16304 | 10468 | `	if( pEntry == 0 ){` |
|        7 | 10469 | `		return 0;` |
|        - | 10470 | `	}` |
|    16298 | 10471 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    16298 | 10472 | `	if( !iLoadable ){` |
|    15176 | 10473 | `		return pClass;` |
|        - | 10474 | `	}` |
|        - | 10475 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1124 | 10476 | `	while(pClass){` |
|     1124 | 10477 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1124 | 10478 | `			return pClass;` |
|        - | 10479 | `		}` |
|      ! 0 | 10480 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10481 | `	}` |
|      ! 0 | 10482 | `	return 0;` |
|     8153 | 10483 |  |
|        - | 10484 | `/*` |
|        - | 10485 | ` * Reference Table Implementation` |
|        - | 10486 | ` * Status: stable <chm@symisc.net>` |
|        - | 10487 | ` * Intro` |
|        - | 10488 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10489 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10490 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10491 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10492 | ` *  Refer to the official for more information on this powerful` |
|        - | 10493 | ` *  extension.` |
|        - | 10494 | ` */` |
|        - | 10495 | `/*` |
|        - | 10496 | ` * Allocate a new reference entry.` |
|        - | 10497 | ` */` |
|  2993410 | 10498 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10499 |  |
|        - | 10500 | `	VmRefObj *pRef;` |
|        - | 10501 | `	/* Allocate a new instance */` |
|  2993412 | 10502 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2993412 | 10503 | `	if( pRef == 0 ){` |
|      ! 0 | 10504 | `		return 0;` |
|        - | 10505 | `	}` |
|        - | 10506 | `	/* Zero the structure */` |
|  2993412 | 10507 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10508 | `	/* Initialize fields */` |
|  2993412 | 10509 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2993412 | 10510 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2993412 | 10511 | `	pRef->nIdx = nIdx;` |
|  2993412 | 10512 | `	return pRef;` |
|  1496707 | 10513 |  |
|        - | 10514 | `/*` |
|        - | 10515 | ` * Default hash function used by the reference table` |
|        - | 10516 | ` * for lookup/insertion operations.` |
|        - | 10517 | ` */` |
| 16608059 | 10518 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10519 |  |
|        - | 10520 | `	/* Calculate the hash based on the memory object index */` |
| 16608061 | 10521 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10522 |  |
|        - | 10523 | `/*` |
|        - | 10524 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10525 | ` * in the reference table.` |
|        - | 10526 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10527 | ` * otherwise.` |
|        - | 10528 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10529 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10530 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10531 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10532 | ` * Refer to the official for more information on this powerful` |
|        - | 10533 | ` * extension.` |
|        - | 10534 | ` */` |
|  8936256 | 10535 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10536 |  |
|        - | 10537 | `	VmRefObj *pRef;` |
|        - | 10538 | `	sxu32 nBucket;` |
|        - | 10539 | `	/* Point to the appropriate bucket */` |
|  8936258 | 10540 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10541 | `	/* Perform the lookup */` |
|  8936258 | 10542 | `	pRef = pVm->apRefObj[nBucket];` |
| 18835860 | 10543 | `	for(;;){` |
| 37658919 | 10544 | `		if( pRef == 0 ){` |
|  3068806 | 10545 | `			break;` |
|        - | 10546 | `		}` |
| 34590115 | 10547 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10548 | `			/* Entry found */` |
|  5867454 | 10549 | `			return pRef;` |
|        - | 10550 | `		}` |
|        - | 10551 | `		/* Point to the next entry */` |
| 28722663 | 10552 | `		pRef = pRef->pNextCollide;` |
|        2 | 10553 | `	}` |
|        - | 10554 | `	/* No such entry,return NULL */` |
|  3068806 | 10555 | `	return 0;` |
|  4468130 | 10556 |  |
|        - | 10557 | `/*` |
|        - | 10558 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10559 | ` *` |
|        - | 10560 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10561 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10562 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10563 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10564 | ` * Refer to the official for more information on this powerful` |
|        - | 10565 | ` * extension.` |
|        - | 10566 | ` */` |
|  2993410 | 10567 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10568 |  |
|        - | 10569 | `	sxu32 nBucket;` |
|  2993412 | 10570 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10571 | `		VmRefObj **apNew;` |
|        - | 10572 | `		sxu32 nNew;` |
|        - | 10573 | `		/* Allocate a larger table */` |
|     3514 | 10574 | `		nNew = pVm->nRefSize << 1;` |
|     3514 | 10575 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3514 | 10576 | `		if( apNew ){` |
|     3514 | 10577 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10578 | `			sxu32 n;` |
|        - | 10579 | `			/* Zero the structure */` |
|     3514 | 10580 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10581 | `			/* Rehash all referenced entries */` |
|  2835300 | 10582 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10583 | `				/* Remove old collision links */` |
|  2831788 | 10584 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10585 | `				/* Point to the appropriate bucket */` |
|  2831788 | 10586 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10587 | `				/* Insert the entry  */` |
|  2831788 | 10588 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2831788 | 10589 | `				if( apNew[nBucket] ){` |
|  2298896 | 10590 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10591 | `				}` |
|  2831788 | 10592 | `				apNew[nBucket] = pEntry;` |
|        - | 10593 | `				/* Point to the next entry */` |
|  2831788 | 10594 | `				pEntry = pEntry->pNext;` |
|  1415895 | 10595 | `			}` |
|        - | 10596 | `			/* Release the old table */` |
|     3514 | 10597 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10598 | `			/* Install the new one */` |
|     3514 | 10599 | `			pVm->apRefObj = apNew;` |
|     3514 | 10600 | `			pVm->nRefSize = nNew;` |
|     1756 | 10601 | `		}` |
|     1756 | 10602 | `	}` |
|        - | 10603 | `	/* Point to the appropriate bucket */` |
|  2993412 | 10604 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10605 | `	/* Insert the entry */` |
|  2993412 | 10606 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2993412 | 10607 | `	if( pVm->apRefObj[nBucket] ){` |
|  2481644 | 10608 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1240884 | 10609 | `	}` |
|  2993412 | 10610 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2993412 | 10611 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2993412 | 10612 | `	pVm->nRefUsed++;` |
|  2993412 | 10613 | `	return SXRET_OK;` |
|        2 | 10614 |  |
|        - | 10615 | `/*` |
|        - | 10616 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10617 | ` * the reference table.` |
|        - | 10618 | ` * This function is invoked when the user perform an unset` |
|        - | 10619 | ` * call [i.e: unset($var); ].` |
|        - | 10620 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10621 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10622 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10623 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10624 | ` * Refer to the official for more information on this powerful` |
|        - | 10625 | ` * extension.` |
|        - | 10626 | ` */` |
|  2961638 | 10627 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10628 |  |
|        - | 10629 | `	ph7_hashmap_node **apNode;` |
|        - | 10630 | `	SyHashEntry **apEntry;` |
|        - | 10631 | `	sxu32 n;` |
|        - | 10632 | `	/* Point to the reference table */` |
|  2961640 | 10633 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2961640 | 10634 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10635 | `	/* Unlink the entry from the reference table */` |
|  3041996 | 10636 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    80358 | 10637 | `		if( apEntry[n] ){` |
|    80308 | 10638 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    40153 | 10639 | `		}` |
|    40180 | 10640 | `	}` |
|  5844776 | 10641 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2883138 | 10642 | `		if( apNode[n] ){` |
|     5635 | 10643 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2817 | 10644 | `		}` |
|  1441570 | 10645 | `	}` |
|  2961640 | 10646 | `	if( pRef->pPrevCollide ){` |
|  1115033 | 10647 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   557804 | 10648 | `	}else{` |
|  1846609 | 10649 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10650 | `	}` |
|  2961640 | 10651 | `	if( pRef->pNextCollide ){` |
|  1669820 | 10652 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   835018 | 10653 | `	}` |
|  2961640 | 10654 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10655 | `	/* Release the node */` |
|  2961640 | 10656 | `	SySetRelease(&pRef->aReference);` |
|  2961640 | 10657 | `	SySetRelease(&pRef->aArrEntries);` |
|  2961640 | 10658 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2961640 | 10659 | `	pVm->nRefUsed--;` |
|  2961640 | 10660 | `	return SXRET_OK;` |
|        2 | 10661 |  |
|        - | 10662 | `/*` |
|        - | 10663 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10664 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10665 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10666 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10667 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10668 | ` * Refer to the official for more information on this powerful` |
|        - | 10669 | ` * extension.` |
|        - | 10670 | ` */` |
|  3021662 | 10671 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10672 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10673 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10674 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10675 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10676 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10677 | `	)` |
|        2 | 10678 |  |
|  3021664 | 10679 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10680 | `	VmRefObj *pRef;` |
|        - | 10681 | `	/* Check if the referenced object already exists */` |
|  3021664 | 10682 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3021664 | 10683 | `	if( pRef == 0 ){` |
|        - | 10684 | `		/* Create a new entry */` |
|  2993412 | 10685 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2993412 | 10686 | `		if( pRef == 0 ){` |
|      ! 0 | 10687 | `			return SXERR_MEM;` |
|        - | 10688 | `		}` |
|  2993412 | 10689 | `		pRef->iFlags = iFlags;` |
|        - | 10690 | `		/* Install the entry */` |
|  2993412 | 10691 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1496705 | 10692 | `	}` |
|  3021824 | 10693 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10694 | `		/* Safely ignore the exception frame */` |
|      162 | 10695 | `		pFrame = pFrame->pParent;` |
|        2 | 10696 | `	}` |
|  3021664 | 10697 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10698 | `		VmSlot sRef;` |
|        - | 10699 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10700 | `		 * be deleted when we leave this frame.` |
|        - | 10701 | `		 */` |
|    75430 | 10702 | `		sRef.nIdx = nIdx;` |
|    75430 | 10703 | `		sRef.pUserData = pEntry;` |
|    75430 | 10704 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10705 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10706 | `		}` |
|    37714 | 10707 | `	}` |
|  3021664 | 10708 | `	if( pEntry ){` |
|        - | 10709 | `		/* Address of the hash-entry */` |
|   103492 | 10710 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    51745 | 10711 | `	}` |
|  3021664 | 10712 | `	if( pMapEntry ){` |
|        - | 10713 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2913322 | 10714 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1456660 | 10715 | `	}` |
|  3021664 | 10716 | `	return SXRET_OK;` |
|  1510833 | 10717 |  |
|        - | 10718 | `/*` |
|        - | 10719 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10720 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10721 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10722 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10723 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10724 | ` * Refer to the official for more information on this powerful` |
|        - | 10725 | ` * extension.` |
|        - | 10726 | ` */` |
|  2952936 | 10727 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10728 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10729 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10730 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10731 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10732 | `	)` |
|        2 | 10733 |  |
|        - | 10734 | `	VmRefObj *pRef;` |
|        - | 10735 | `	sxu32 n;` |
|        - | 10736 | `	/* Check if the referenced object already exists */` |
|  2952938 | 10737 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2952938 | 10738 | `	if( pRef == 0 ){` |
|        - | 10739 | `		/* Not such entry */` |
|    75376 | 10740 | `		return SXERR_NOTFOUND;` |
|        - | 10741 | `	}` |
|        - | 10742 | `	/* Remove the desired entry */` |
|  2877564 | 10743 | `	if( pEntry ){` |
|        - | 10744 | `		SyHashEntry **apEntry;` |
|       56 | 10745 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10746 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10747 | `			if( apEntry[n] == pEntry ){` |
|        - | 10748 | `				/* Nullify the entry */` |
|       56 | 10749 | `				apEntry[n] = 0;` |
|        - | 10750 | `				/*` |
|        - | 10751 | `				 * NOTE:` |
|        - | 10752 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10753 | `				 * we avoid wasting spaces.` |
|        - | 10754 | `				 */` |
|       27 | 10755 | `			}` |
|       79 | 10756 | `		}` |
|       27 | 10757 | `	}` |
|  2877564 | 10758 | `	if( pMapEntry ){` |
|        - | 10759 | `		ph7_hashmap_node **apNode;` |
|  2877510 | 10760 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5755106 | 10761 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2877598 | 10762 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10763 | `				/* nullify the entry */` |
|  2877510 | 10764 | `				apNode[n] = 0;` |
|  1438754 | 10765 | `			}` |
|  1438800 | 10766 | `		}` |
|  1438754 | 10767 | `	}` |
|  2877564 | 10768 | `	return SXRET_OK;` |
|  1476470 | 10769 |  |
|        - | 10770 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10771 | `/*` |
|        - | 10772 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10773 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10774 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10775 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10776 | ` * For more information on how to register IO stream devices,please` |
|        - | 10777 | ` * refer to the official documentation.` |
|        - | 10778 | ` */` |
|    22994 | 10779 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10780 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10781 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10782 | `	int nByte              /* *pzDevice length*/` |
|        - | 10783 | `	)` |
|        2 | 10784 |  |
|        - | 10785 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10786 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10787 | `	SyString sDev,sCur;` |
|        - | 10788 | `	sxu32 n,nEntry;` |
|        - | 10789 | `	int rc;` |
|        - | 10790 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    22996 | 10791 | `	zNext = zCur = zIn = *pzDevice;` |
|    22996 | 10792 | `	zEnd = &zIn[nByte];` |
|  1471471 | 10793 | `	while( zIn < zEnd ){` |
|  1448479 | 10794 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10795 | `			/* Got one */` |
|        3 | 10796 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10797 | `			break;` |
|        - | 10798 | `		}` |
|        - | 10799 | `		/* Advance the cursor */` |
|  1448477 | 10800 | `		zIn++;` |
|        2 | 10801 | `	}` |
|    22996 | 10802 | `	if( zIn >= zEnd ){` |
|        - | 10803 | `		/* No such scheme,return the default stream */` |
|    22994 | 10804 | `		return pVm->pDefStream;` |
|        - | 10805 | `	}` |
|        3 | 10806 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10807 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10808 | `	SyStringFullTrim(&sDev);` |
|        - | 10809 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10810 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10811 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10812 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10813 | `		pStream = apStream[n];` |
|        3 | 10814 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10815 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10816 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10817 | `		if( rc == 0 ){` |
|        - | 10818 | `			/* Stream device found */` |
|        3 | 10819 | `			*pzDevice = zNext;` |
|        3 | 10820 | `			return pStream;` |
|        - | 10821 | `		}` |
|      ! 0 | 10822 | `	}` |
|        - | 10823 | `	/* No such stream,return NULL */` |
|      ! 0 | 10824 | `	return 0;` |
|    11499 | 10825 |  |
|        - | 10826 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10827 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10828 |  |
