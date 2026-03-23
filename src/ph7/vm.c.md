# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3909/5166 lines (75.67%)

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
|   819972 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   819974 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   819944 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   819936 |    94 | `	return FALSE;` |
|   410010 |    95 |  |
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
|   412108 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   412110 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   412110 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   412106 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   412106 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   412106 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   412106 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   412106 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   412106 |   142 | `	pCons->xExpand = xExpand;` |
|   412106 |   143 | `	pCons->pUserData = pUserData;` |
|   412106 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   412106 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   412106 |   151 | `	return SXRET_OK;` |
|   206056 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   883050 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   883052 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   883052 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   883052 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   883052 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   883052 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   883052 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   883052 |   185 | `	pFunc->pVm   = pVm;` |
|   883052 |   186 | `	pFunc->xFunc = xFunc;` |
|   883052 |   187 | `	pFunc->pUserData = pUserData;` |
|   883052 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   883052 |   190 | `	*ppOut = pFunc;` |
|   883052 |   191 | `	return SXRET_OK;` |
|   441527 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   885080 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   885082 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   885082 |   213 | `	if( pEntry ){` |
|     2032 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2032 |   215 | `		pFunc->pUserData = pUserData;` |
|     2032 |   216 | `		pFunc->xFunc = xFunc;` |
|     2032 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2032 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   883052 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   883052 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   883052 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   883052 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   883052 |   233 | `	return SXRET_OK;` |
|   442542 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|    95944 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|    95946 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|    95946 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|    95946 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|    95946 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|    95946 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|    95946 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|    95946 |   260 | `	pFunc->iFlags = iFlags;` |
|    95946 |   261 | `	pFunc->pUserData = pUserData;` |
|    95946 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|    95946 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   267 | ` */` |
|   348242 |   268 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   269 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   270 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   271 | `	SyString *pName     /* Function name */` |
|        - |   272 | `	)` |
|        2 |   273 |  |
|        - |   274 | `	SyHashEntry *pEntry;` |
|        - |   275 | `	sxi32 rc;` |
|   348244 |   276 | `	if( pName == 0 ){` |
|        - |   277 | `		/* Use the built-in name */` |
|    29958 |   278 | `		pName = &pFunc->sName;` |
|    14978 |   279 | `	}` |
|        - |   280 | `	/* Check for duplicates (functions with the same name) first */` |
|   348244 |   281 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   348244 |   282 | `	if( pEntry ){` |
|   270650 |   283 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   270650 |   284 | `		if( pLink != pFunc ){` |
|        - |   285 | `			/* Link */` |
|      179 |   286 | `			pFunc->pNextName = pLink;` |
|      179 |   287 | `			pEntry->pUserData = pFunc;` |
|       89 |   288 | `		}` |
|   270650 |   289 | `		return SXRET_OK;` |
|        - |   290 | `	}` |
|        - |   291 | `	/* First time seen */` |
|    77596 |   292 | `	pFunc->pNextName = 0;` |
|    77596 |   293 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    77596 |   294 | `	return rc;` |
|   174123 |   295 |  |
|        - |   296 | `/*` |
|        - |   297 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   298 | ` */` |
|    27482 |   299 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   300 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   301 | `	ph7_class *pClass /* Target Class */` |
|        - |   302 | `	)` |
|        2 |   303 |  |
|    27484 |   304 | `	SyString *pName = &pClass->sName;` |
|        - |   305 | `	SyHashEntry *pEntry;` |
|        - |   306 | `	sxi32 rc;` |
|        - |   307 | `	/* Check for duplicates */` |
|    27484 |   308 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    27484 |   309 | `	if( pEntry ){` |
|       31 |   310 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   311 | `		/* Link entry with the same name */` |
|       31 |   312 | `		pClass->pNextName = pLink;` |
|       31 |   313 | `		pEntry->pUserData = pClass;` |
|       31 |   314 | `		return SXRET_OK;` |
|        - |   315 | `	}` |
|    27454 |   316 | `	pClass->pNextName = 0;` |
|        - |   317 | `	/* Perform a simple hashtable insertion */` |
|    27454 |   318 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    27454 |   319 | `	return rc;` |
|    13743 |   320 |  |
|        - |   321 | `/*` |
|        - |   322 | ` * Instruction builder interface.` |
|        - |   323 | ` */` |
|  2549742 |   324 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   325 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   326 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   327 | `	sxi32 iP1,    /* First operand */` |
|        - |   328 | `	sxu32 iP2,    /* Second operand */` |
|        - |   329 | `	void *p3,     /* Third operand */` |
|        - |   330 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   331 | `	)` |
|        2 |   332 |  |
|        - |   333 | `	VmInstr sInstr;` |
|        - |   334 | `	sxi32 rc;` |
|        - |   335 | `	/* Fill the VM instruction */` |
|  2549744 |   336 | `	sInstr.iOp = (sxu8)iOp;` |
|  2549744 |   337 | `	sInstr.iP1 = iP1;` |
|  2549744 |   338 | `	sInstr.iP2 = iP2;` |
|  2549744 |   339 | `	sInstr.p3  = p3;` |
|  2549744 |   340 | `	if( pIndex ){` |
|        - |   341 | `		/* Instruction index in the bytecode array */` |
|   162562 |   342 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    81280 |   343 | `	}` |
|        - |   344 | `	/* Finally,record the instruction */` |
|  2549744 |   345 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2549744 |   346 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   347 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   348 | `		/* Fall throw */` |
|      ! 0 |   349 | `	}` |
|  2549744 |   350 | `	return rc;` |
|        2 |   351 |  |
|        - |   352 | `/*` |
|        - |   353 | ` * Swap the current bytecode container with the given one.` |
|        - |   354 | ` */` |
|   233216 |   355 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   356 |  |
|   233218 |   357 | `	if( pContainer == 0 ){` |
|        - |   358 | `		/* Point to the default container */` |
|      ! 0 |   359 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   360 | `	}else{` |
|        - |   361 | `		/* Change container */` |
|   233218 |   362 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   363 | `	}` |
|   233218 |   364 | `	return SXRET_OK;` |
|        2 |   365 |  |
|        - |   366 | `/*` |
|        - |   367 | ` * Return the current bytecode container.` |
|        - |   368 | ` */` |
|   116608 |   369 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   370 |  |
|   116610 |   371 | `	return pVm->pByteContainer;` |
|        2 |   372 |  |
|        - |   373 | `/*` |
|        - |   374 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   375 | ` */` |
|   160216 |   376 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   377 |  |
|        - |   378 | `	VmInstr *pInstr;` |
|   160218 |   379 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   160218 |   380 | `	return pInstr;` |
|        2 |   381 |  |
|        - |   382 | `/*` |
|        - |   383 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   384 | ` */` |
|   714260 |   385 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   386 |  |
|   714262 |   387 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   388 |  |
|        - |   389 | `/*` |
|        - |   390 | ` * Pop the last VM instruction.` |
|        - |   391 | ` */` |
|   152066 |   392 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   393 |  |
|   152068 |   394 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Peek the last VM instruction.` |
|        - |   398 | ` */` |
|   402788 |   399 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   400 |  |
|   402790 |   401 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   402 |  |
|    11570 |   403 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   404 |  |
|        - |   405 | `	VmInstr *aInstr;` |
|        - |   406 | `	sxu32 n;` |
|    11572 |   407 | `	n = SySetUsed(pVm->pByteContainer);` |
|    11572 |   408 | `	if( n < 2 ){` |
|      ! 0 |   409 | `		return 0;` |
|        - |   410 | `	}` |
|    11572 |   411 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    11572 |   412 | `	return &aInstr[n - 2];` |
|     5787 |   413 |  |
|        - |   414 | `/*` |
|        - |   415 | ` * Allocate a new virtual machine frame.` |
|        - |   416 | ` */` |
|    13972 |   417 | `static VmFrame * VmNewFrame(` |
|        - |   418 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   419 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   420 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   421 | `	)` |
|        2 |   422 |  |
|        - |   423 | `	VmFrame *pFrame;` |
|        - |   424 | `	/* Allocate a new vm frame */` |
|    13974 |   425 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    13974 |   426 | `	if( pFrame == 0 ){` |
|      ! 0 |   427 | `		return 0;` |
|        - |   428 | `	}` |
|        - |   429 | `	/* Zero the structure */` |
|    13974 |   430 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   431 | `	/* Initialize frame fields */` |
|    13974 |   432 | `	pFrame->pUserData = pUserData;` |
|    13974 |   433 | `	pFrame->pThis = pThis;` |
|    13974 |   434 | `	pFrame->pVm = pVm;` |
|    13974 |   435 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    13974 |   436 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    13974 |   437 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    13974 |   438 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    13974 |   439 | `	return pFrame;` |
|     6988 |   440 |  |
|        - |   441 | `/*` |
|        - |   442 | ` * Enter a VM frame.` |
|        - |   443 | ` */` |
|    13972 |   444 | `static sxi32 VmEnterFrame(` |
|        - |   445 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   446 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   447 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   448 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   449 | `	)` |
|        2 |   450 |  |
|        - |   451 | `	VmFrame *pFrame;` |
|        - |   452 | `	/* Allocate a new frame */` |
|    13974 |   453 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    13974 |   454 | `	if( pFrame == 0 ){` |
|      ! 0 |   455 | `		return SXERR_MEM;` |
|        - |   456 | `	}` |
|        - |   457 | `	/* Link to the list of active VM frame */` |
|    13974 |   458 | `	pFrame->pParent = pVm->pFrame;` |
|    13974 |   459 | `	pVm->pFrame = pFrame;` |
|    13974 |   460 | `	if( ppFrame ){` |
|        - |   461 | `		/* Write a pointer to the new VM frame */` |
|    11706 |   462 | `		*ppFrame = pFrame;` |
|     5852 |   463 | `	}` |
|    13974 |   464 | `	return SXRET_OK;` |
|     6988 |   465 |  |
|        - |   466 | `/*` |
|        - |   467 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   468 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   469 | ` * information.` |
|        - |   470 | ` */` |
|       48 |   471 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        1 |   472 |  |
|        - |   473 | `	VmFrame *pTarget,*pFrame;` |
|       49 |   474 | `	SyHashEntry *pEntry = 0;` |
|        - |   475 | `	sxi32 rc;` |
|        - |   476 | `	/* Point to the upper frame */` |
|       49 |   477 | `	pFrame = pVm->pFrame;` |
|       49 |   478 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |   479 | `		/* Safely ignore the exception frame */` |
|      ! 0 |   480 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   481 | `	}` |
|       49 |   482 | `	pTarget = pFrame;` |
|       49 |   483 | `	pFrame = pTarget->pParent;` |
|       49 |   484 | `	while( pFrame ){` |
|       49 |   485 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   486 | `			/* Query the current frame */` |
|       49 |   487 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       49 |   488 | `			if( pEntry ){` |
|        - |   489 | `				/* Variable found */` |
|       49 |   490 | `				break;` |
|        - |   491 | `			}` |
|      ! 0 |   492 | `		}` |
|        - |   493 | `		/* Point to the upper frame */` |
|      ! 0 |   494 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   495 | `	}` |
|       49 |   496 | `	if( pEntry == 0 ){` |
|        - |   497 | `		/* Inexistant variable */` |
|      ! 0 |   498 | `		return SXERR_NOTFOUND;` |
|        - |   499 | `	}` |
|        - |   500 | `	/* Link to the current frame */` |
|       49 |   501 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       49 |   502 | `	if( rc == SXRET_OK ){` |
|        - |   503 | `		sxu32 nIdx;` |
|       49 |   504 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       49 |   505 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       24 |   506 | `	}` |
|       49 |   507 | `	return rc;` |
|       25 |   508 |  |
|        - |   509 | `/*` |
|        - |   510 | ` * Leave the top-most active frame.` |
|        - |   511 | ` */` |
|    11704 |   512 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   513 |  |
|    11706 |   514 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    11706 |   515 | `	if( pCurFrame ){` |
|        - |   516 | `		/* Unlink from the list of active VM frame */` |
|    11706 |   517 | `		pVm->pFrame = pCurFrame->pParent;` |
|    11706 |   518 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   519 | `			VmSlot  *aSlot;` |
|        - |   520 | `			sxu32 n;` |
|        - |   521 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    11682 |   522 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    84382 |   523 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   524 | `				/* Unset the local variable */` |
|    72702 |   525 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    36352 |   526 | `			}` |
|        - |   527 | `			/* Remove local reference */` |
|    11682 |   528 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    84434 |   529 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    72754 |   530 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    36378 |   531 | `			}` |
|     5840 |   532 | `		}` |
|        - |   533 | `		/* Release internal containers */` |
|    11706 |   534 | `		SyHashRelease(&pCurFrame->hVar);` |
|    11706 |   535 | `		SySetRelease(&pCurFrame->sArg);` |
|    11706 |   536 | `		SySetRelease(&pCurFrame->sLocal);` |
|    11706 |   537 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   538 | `		/* Release the whole structure */` |
|    11706 |   539 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     5852 |   540 | `	}` |
|    11706 |   541 |  |
|        - |   542 | `/*` |
|        - |   543 | ` * Compare two functions signature and return the comparison result.` |
|        - |   544 | ` */` |
|      818 |   545 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   546 |  |
|      819 |   547 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      819 |   548 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      819 |   549 | `	const char *zSin = pSecond->zString;` |
|      819 |   550 | `	const char *zFin = pFirst->zString;` |
|      819 |   551 | `	const char *zPtr = zFin;` |
|      409 |   552 | `	for(;;){` |
|      819 |   553 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      410 |   554 | `			break;` |
|        - |   555 | `		}` |
|      ! 0 |   556 | `		if( zFin[0] != zSin[0] ){` |
|        - |   557 | `			/* mismatch */` |
|      ! 0 |   558 | `			break;` |
|        - |   559 | `		}` |
|      ! 0 |   560 | `		zFin++;` |
|      ! 0 |   561 | `		zSin++;` |
|      ! 0 |   562 | `	}` |
|      819 |   563 | `	return (int)(zFin-zPtr);` |
|        1 |   564 |  |
|        - |   565 | `/*` |
|        - |   566 | ` * Select the appropriate VM function for the current call context.` |
|        - |   567 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   568 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   569 | ` * Refer to the official documentation for more information.` |
|        - |   570 | ` */` |
|      122 |   571 | `static ph7_vm_func * VmOverload(` |
|        - |   572 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   573 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   574 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   575 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   576 | `	)` |
|        1 |   577 |  |
|        - |   578 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   579 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   580 | `	ph7_vm_func *pLink;` |
|        - |   581 | `	SyString sArgSig;` |
|        - |   582 | `	SyBlob sSig;` |
|        - |   583 |  |
|      123 |   584 | `	pLink = pList;` |
|      123 |   585 | `	i = 0;` |
|        - |   586 | `	/* Put functions expecting the same number of passed arguments */` |
|     1031 |   587 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      969 |   588 | `		if( pLink == 0 ){` |
|       61 |   589 | `			break;` |
|        - |   590 | `		}` |
|      909 |   591 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   592 | `			/* Candidate for overloading */` |
|      863 |   593 | `			apSet[i++] = pLink;` |
|      431 |   594 | `		}` |
|        - |   595 | `		/* Point to the next entry */` |
|      909 |   596 | `		pLink = pLink->pNextName;` |
|        1 |   597 | `	}` |
|      123 |   598 | `	if( i < 1 ){` |
|        - |   599 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   600 | `		return pList;` |
|        - |   601 | `	}` |
|      123 |   602 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   603 | `		/* Return the only candidate */` |
|       21 |   604 | `		return apSet[0];` |
|        - |   605 | `	}` |
|        - |   606 | `	/* Calculate function signature */` |
|      103 |   607 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      355 |   608 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      253 |   609 | `		int c = 'n'; /* null */` |
|      253 |   610 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   611 | `			/* Hashmap */` |
|       45 |   612 | `			c = 'h';` |
|      231 |   613 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   614 | `			/* bool */` |
|      ! 0 |   615 | `			c = 'b';` |
|      209 |   616 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   617 | `			/* int */` |
|        5 |   618 | `			c = 'i';` |
|      207 |   619 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   620 | `			/* String */` |
|      105 |   621 | `			c = 's';` |
|      153 |   622 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   623 | `			/* Float */` |
|      ! 0 |   624 | `			c = 'f';` |
|      101 |   625 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   626 | `			/* Class instance */` |
|      ! 0 |   627 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|      ! 0 |   628 | `			SyString *pName = &pClass->sName;` |
|      ! 0 |   629 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|      ! 0 |   630 | `			c = -1;` |
|      ! 0 |   631 | `		}` |
|      253 |   632 | `		if( c > 0 ){` |
|      253 |   633 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      126 |   634 | `		}` |
|      127 |   635 | `	}` |
|      103 |   636 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      103 |   637 | `	iTarget = 0;` |
|      103 |   638 | `	iMax = -1;` |
|        - |   639 | `	/* Select the appropriate function */` |
|      921 |   640 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   641 | `		/* Compare the two signatures */` |
|      819 |   642 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      819 |   643 | `		if( iCur > iMax ){` |
|      103 |   644 | `			iMax = iCur;` |
|      103 |   645 | `			iTarget = j;` |
|       51 |   646 | `		}` |
|      410 |   647 | `	}` |
|      103 |   648 | `	SyBlobRelease(&sSig);` |
|        - |   649 | `	/* Appropriate function for the current call context */` |
|      103 |   650 | `	return apSet[iTarget];` |
|       62 |   651 |  |
|        - |   652 | `/* Forward declaration */` |
|        - |   653 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   654 | `/*` |
|        - |   655 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   656 | ` * it can be instanciated from the executed PHP script.` |
|        - |   657 | ` */` |
|    83684 |   658 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   659 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   660 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   661 | `	)` |
|        2 |   662 |  |
|        - |   663 | `	ph7_class_method *pMeth;` |
|        - |   664 | `	ph7_class_attr *pAttr;` |
|        - |   665 | `	SyHashEntry *pEntry;` |
|        - |   666 | `	sxi32 rc;` |
|        - |   667 | `	/* Reset the loop cursor */` |
|    83686 |   668 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   669 | `	/* Process only static and constant attribute */` |
|   324712 |   670 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   671 | `		/* Extract the current attribute */` |
|   199186 |   672 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   199186 |   673 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   674 | `			ph7_value *pMemObj;` |
|        - |   675 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1290 |   676 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1290 |   677 | `			if( pMemObj == 0 ){` |
|      ! 0 |   678 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   679 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   680 | `					&pClass->sName,&pAttr->sName` |
|        - |   681 | `					);` |
|      ! 0 |   682 | `				return SXERR_MEM;` |
|        - |   683 | `			}` |
|     1290 |   684 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   685 | `				/* Initialize attribute default value (any complex expression) */` |
|     1290 |   686 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      644 |   687 | `			}` |
|        - |   688 | `			/* Record attribute index */` |
|     1290 |   689 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   690 | `			/* Install static attribute in the reference table */` |
|     1290 |   691 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      644 |   692 | `		}` |
|        2 |   693 | `	}` |
|        - |   694 | `	/* Install class methods */` |
|    83686 |   695 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|        - |   696 | `		/* Do not mount interface methods since they are signatures only.` |
|        - |   697 | `		 */` |
|    44762 |   698 | `		return SXRET_OK;` |
|        - |   699 | `	}` |
|        - |   700 | `	/* Create constructor alias if not yet done */` |
|    38926 |   701 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   702 | `		/* User constructor with the same base class name */` |
|      214 |   703 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      214 |   704 | `		if( pEntry ){` |
|      ! 0 |   705 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   706 | `			/* Create the alias */` |
|      ! 0 |   707 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   708 | `		}` |
|      106 |   709 | `	}` |
|        - |   710 | `	/* Install the methods now */` |
|    38926 |   711 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   376680 |   712 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   318294 |   713 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   318294 |   714 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   318288 |   715 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   318288 |   716 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   717 | `				return rc;` |
|        - |   718 | `			}` |
|   159143 |   719 | `		}` |
|        2 |   720 | `	}` |
|        - |   721 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    38926 |   722 | `	pClass->bMounted = TRUE;` |
|    38926 |   723 | `	return SXRET_OK;` |
|    41844 |   724 |  |
|        - |   725 | `/*` |
|        - |   726 | ` * Allocate a private frame for attributes of the given` |
|        - |   727 | ` * class instance (Object in the PHP jargon).` |
|        - |   728 | ` */` |
|     1040 |   729 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   730 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   731 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   732 | `	)` |
|        2 |   733 |  |
|     1042 |   734 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   735 | `	ph7_class_attr *pAttr;` |
|        - |   736 | `	SyHashEntry *pEntry;` |
|        - |   737 | `	sxi32 rc;` |
|        - |   738 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1042 |   739 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4524 |   740 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   741 | `		VmClassAttr *pVmAttr;` |
|        - |   742 | `		/* Extract the current attribute */` |
|     3484 |   743 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3484 |   744 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3484 |   745 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   746 | `			return SXERR_MEM;` |
|        - |   747 | `		}` |
|     3484 |   748 | `		pVmAttr->pAttr = pAttr;` |
|     3484 |   749 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   750 | `			ph7_value *pMemObj;` |
|        - |   751 | `			/* Reserve a memory object for this attribute */` |
|     3478 |   752 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3478 |   753 | `			if( pMemObj == 0 ){` |
|      ! 0 |   754 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   755 | `				return SXERR_MEM;` |
|        - |   756 | `			}` |
|     3478 |   757 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3478 |   758 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   759 | `				/* Initialize attribute default value (any complex expression) */` |
|     1136 |   760 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      567 |   761 | `			}` |
|     3478 |   762 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3478 |   763 | `			if( rc != SXRET_OK ){` |
|        - |   764 | `				VmSlot sSlot;` |
|        - |   765 | `				/* Restore memory object */` |
|      ! 0 |   766 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   767 | `				sSlot.pUserData = 0;` |
|      ! 0 |   768 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   769 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   770 | `				return SXERR_MEM;` |
|        - |   771 | `			}` |
|        - |   772 | `			/* Install attribute in the reference table */` |
|     3478 |   773 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1740 |   774 | `		}else{` |
|        - |   775 | `			/* Install static/constant attribute */` |
|        8 |   776 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   777 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   778 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   779 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   780 | `				return SXERR_MEM;` |
|        - |   781 | `			}` |
|        - |   782 | `		}` |
|        2 |   783 | `	}` |
|     1042 |   784 | `	return SXRET_OK;` |
|      522 |   785 |  |
|        - |   786 | `/* Forward declaration */` |
|        - |   787 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   788 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   789 | `/*` |
|        - |   790 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   791 | ` */` |
|        - |   792 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   793 | `/*` |
|        - |   794 | ` * Reserve a constant memory object.` |
|        - |   795 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   796 | ` */` |
|   278860 |   797 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   798 |  |
|        - |   799 | `	ph7_value *pObj;` |
|        - |   800 | `	sxi32 rc;` |
|   278862 |   801 | `	if( pIndex ){` |
|        - |   802 | `		/* Object index in the object table */` |
|   272058 |   803 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   136028 |   804 | `	}` |
|        - |   805 | `	/* Reserve a slot for the new object */` |
|   278862 |   806 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   278862 |   807 | `	if( rc != SXRET_OK ){` |
|        - |   808 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   809 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   810 | `		 */` |
|      ! 0 |   811 | `		return 0;` |
|        - |   812 | `	}` |
|   278862 |   813 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   278862 |   814 | `	return pObj;` |
|   139432 |   815 |  |
|        - |   816 | `/*` |
|        - |   817 | ` * Reserve a memory object.` |
|        - |   818 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   819 | ` */` |
|  2136662 |   820 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   821 |  |
|        - |   822 | `	ph7_value *pObj;` |
|        - |   823 | `	sxi32 rc;` |
|  2136664 |   824 | `	if( pIndex ){` |
|        - |   825 | `		/* Object index in the object table */` |
|  2136664 |   826 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1068331 |   827 | `	}` |
|        - |   828 | `	/* Reserve a slot for the new object */` |
|  2136664 |   829 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2136664 |   830 | `	if( rc != SXRET_OK ){` |
|        - |   831 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   832 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   833 | `		 */` |
|      ! 0 |   834 | `		return 0;` |
|        - |   835 | `	}` |
|  2136664 |   836 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2136664 |   837 | `	return pObj;` |
|  1068333 |   838 |  |
|        - |   839 | `/* Forward declaration */` |
|        - |   840 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   841 | `/*` |
|        - |   842 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   843 | ` * directly as foreign functions.` |
|        - |   844 | ` */` |
|        - |   845 | `#define PH7_BUILTIN_LIB \` |
|        - |   846 | `	"class Exception { "\` |
|        - |   847 | `    "protected $message = 'Unknown exception';"\` |
|        - |   848 | `    "protected $code = 0;"\` |
|        - |   849 | `    "protected $file;"\` |
|        - |   850 | `    "protected $line;"\` |
|        - |   851 | `    "protected $trace;"\` |
|        - |   852 | `    "protected $previous;"\` |
|        - |   853 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   854 | `	"   if( isset($message) ){"\` |
|        - |   855 | `	"	  $this->message = $message;"\` |
|        - |   856 | `	"   }"\` |
|        - |   857 | `	"   $this->code = $code;"\` |
|        - |   858 | `	"   $this->file = __FILE__;"\` |
|        - |   859 | `	"   $this->line = __LINE__;"\` |
|        - |   860 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   861 | `	"   if( isset($previous) ){"\` |
|        - |   862 | `	"     $this->previous = $previous;"\` |
|        - |   863 | `	"   }"\` |
|        - |   864 | `	"}"\` |
|        - |   865 | `	"public function getMessage(){"\` |
|        - |   866 | `	"   return $this->message;"\` |
|        - |   867 | `	"}"\` |
|        - |   868 | `	" public function getCode(){"\` |
|        - |   869 | `	"  return $this->code;"\` |
|        - |   870 | `	"}"\` |
|        - |   871 | `	"public function getFile(){"\` |
|        - |   872 | `	"  return $this->file;"\` |
|        - |   873 | `	"}"\` |
|        - |   874 | `	"public function getLine(){"\` |
|        - |   875 | `	"  return $this->line;"\` |
|        - |   876 | `	"}"\` |
|        - |   877 | `	"public function getTrace(){"\` |
|        - |   878 | `	"   return $this->trace;"\` |
|        - |   879 | `	"}"\` |
|        - |   880 | `	"public function getTraceAsString(){"\` |
|        - |   881 | `	"  return debug_string_backtrace();"\` |
|        - |   882 | `	"}"\` |
|        - |   883 | `	"public function getPrevious(){"\` |
|        - |   884 | `	"    return $this->previous;"\` |
|        - |   885 | `	"}"\` |
|        - |   886 | `	"public function __toString(){"\` |
|        - |   887 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   888 | `    "}"\` |
|        - |   889 | `	"}"\` |
|        - |   890 | `	"class Error extends Exception { }"\` |
|        - |   891 | `	"class TypeError extends Error { }"\` |
|        - |   892 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   893 | `	"class ValueError extends Error { }"\` |
|        - |   894 | `	"class AssertionError extends Error { }"\` |
|        - |   895 | `	"class ErrorException extends Exception { "\` |
|        - |   896 | `	"protected $severity;"\` |
|        - |   897 | `	"public function __construct(string $message = null,"\` |
|        - |   898 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   899 | `	"   if( isset($message) ){"\` |
|        - |   900 | `	"	  $this->message = $message;"\` |
|        - |   901 | `	"   }"\` |
|        - |   902 | `	"   $this->severity = $severity;"\` |
|        - |   903 | `	"   $this->code = $code;"\` |
|        - |   904 | `	"   $this->file = $filename;"\` |
|        - |   905 | `	"   $this->line = $lineno;"\` |
|        - |   906 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   907 | `	"   if( isset($previous) ){"\` |
|        - |   908 | `	"     $this->previous = $previous;"\` |
|        - |   909 | `	"   }"\` |
|        - |   910 | `	"}"\` |
|        - |   911 | `	"public function getSeverity(){"\` |
|        - |   912 | `	"   return $this->severity;"\` |
|        - |   913 | `    "}"\` |
|        - |   914 | `	"}"\` |
|        - |   915 | `	"interface Iterator {"\` |
|        - |   916 | `	"public function current();"\` |
|        - |   917 | `	"public function key();"\` |
|        - |   918 | `	"public function next();"\` |
|        - |   919 | `	"public function rewind();"\` |
|        - |   920 | `	"public function valid();"\` |
|        - |   921 | `	"}"\` |
|        - |   922 | `	"interface IteratorAggregate {"\` |
|        - |   923 | `	"public function getIterator();"\` |
|        - |   924 | `	"}"\` |
|        - |   925 | `	"interface Serializable {"\` |
|        - |   926 | `	"public function serialize();"\` |
|        - |   927 | `	"public function unserialize(string $serialized);"\` |
|        - |   928 | `	"}"\` |
|        - |   929 | `	"/* Directory releated IO */"\` |
|        - |   930 | `	"class Directory {"\` |
|        - |   931 | `	"public $handle = null;"\` |
|        - |   932 | `	"public $path  = null;"\` |
|        - |   933 | `	"public function __construct(string $path)"\` |
|        - |   934 | `	"{"\` |
|        - |   935 | `	"   $this->handle = opendir($path);"\` |
|        - |   936 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   937 | `	"      $this->path = $path;"\` |
|        - |   938 | `	"   }"\` |
|        - |   939 | `	"}"\` |
|        - |   940 | `	"public function __destruct()"\` |
|        - |   941 | `	"{"\` |
|        - |   942 | `	"  if( $this->handle != null ){"\` |
|        - |   943 | `	"       closedir($this->handle);"\` |
|        - |   944 | `	"  }"\` |
|        - |   945 | `	"}"\` |
|        - |   946 | `	"public function read()"\` |
|        - |   947 | `	"{"\` |
|        - |   948 | `	"    return readdir($this->handle);"\` |
|        - |   949 | `	"}"\` |
|        - |   950 | `	"public function rewind()"\` |
|        - |   951 | `	"{"\` |
|        - |   952 | `	"    rewinddir($this->handle);"\` |
|        - |   953 | `	"}"\` |
|        - |   954 | `	"public function close()"\` |
|        - |   955 | `	"{"\` |
|        - |   956 | `	"    closedir($this->handle);"\` |
|        - |   957 | `	"    $this->handle = null;"\` |
|        - |   958 | `	"}"\` |
|        - |   959 | `	"}"\` |
|        - |   960 | `	"class stdClass{"\` |
|        - |   961 | `	"  public $value;"\` |
|        - |   962 | `	" /* Magic methods */"\` |
|        - |   963 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |   964 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |   965 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |   966 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |   967 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |   968 | `	"}"\` |
|        - |   969 | `	"function dir(string $path){"\` |
|        - |   970 | `	"   return new Directory($path);"\` |
|        - |   971 | `	"}"\` |
|        - |   972 | `	"function Dir(string $path){"\` |
|        - |   973 | `	"   return new Directory($path);"\` |
|        - |   974 | `	"}"\` |
|        - |   975 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |   976 | `    "{"\` |
|        - |   977 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |   978 | `	"  $aDir = array();"\` |
|        - |   979 | `	"  $pHandle = opendir($directory);"\` |
|        - |   980 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |   981 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |   982 | `	"      $aDir[] = $pEntry;"\` |
|        - |   983 | `	"   }"\` |
|        - |   984 | `	"  closedir($pHandle);"\` |
|        - |   985 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |   986 | `	"      rsort($aDir);"\` |
|        - |   987 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |   988 | `	"      sort($aDir);"\` |
|        - |   989 | `	"  }"\` |
|        - |   990 | `	"  return $aDir;"\` |
|        - |   991 | `	"}"\` |
|        - |   992 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |   993 | `	"/* Open the target directory */"\` |
|        - |   994 | `	"$zDir = dirname($pattern);"\` |
|        - |   995 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |   996 | `	"$pHandle = opendir($zDir);"\` |
|        - |   997 | `	"if( $pHandle == FALSE ){"\` |
|        - |   998 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |   999 | `	"	return FALSE;"\` |
|        - |  1000 | `	"}"\` |
|        - |  1001 | `	"$pattern = basename($pattern);"\` |
|        - |  1002 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1003 | `	"/* Loop throw available entries */"\` |
|        - |  1004 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1005 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1006 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1007 | `	"	if( $rc ){"\` |
|        - |  1008 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1009 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1010 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1011 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1012 | `	"		  }"\` |
|        - |  1013 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1014 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1015 | `	"		 continue;"\` |
|        - |  1016 | `	"	   }"\` |
|        - |  1017 | `	"	   /* Add the entry */"\` |
|        - |  1018 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1019 | `	"	}"\` |
|        - |  1020 | `	" }"\` |
|        - |  1021 | `	"/* Close the handle */"\` |
|        - |  1022 | `	"closedir($pHandle);"\` |
|        - |  1023 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1024 | `	"  /* Sort the array */"\` |
|        - |  1025 | `	"  sort($pArray);"\` |
|        - |  1026 | `	"}"\` |
|        - |  1027 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1028 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1029 | `	"  $pArray[] = $pattern;"\` |
|        - |  1030 | `	"}"\` |
|        - |  1031 | `	"/* Return the created array */"\` |
|        - |  1032 | `	"return $pArray;"\` |
|        - |  1033 | `   "}"\` |
|        - |  1034 | `   "/* Creates a temporary file */"\` |
|        - |  1035 | `   "function tmpfile(){"\` |
|        - |  1036 | `   "  /* Extract the temp directory */"\` |
|        - |  1037 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1038 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1039 | `   "    /* Use the current dir */"\` |
|        - |  1040 | `   "    $zTempDir = '.';"\` |
|        - |  1041 | `   "  }"\` |
|        - |  1042 | `   "  /* Create the file */"\` |
|        - |  1043 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1044 | `   "  return $pHandle;"\` |
|        - |  1045 | `   "}"\` |
|        - |  1046 | `   "/* Creates a temporary filename */"\` |
|        - |  1047 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1048 | `   "{"\` |
|        - |  1049 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1050 | `   "}"\` |
|        - |  1051 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1052 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1053 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1054 | `   "/* Copy arguments */"\` |
|        - |  1055 | `   "$nArgs = func_num_args();"\` |
|        - |  1056 | `   "$pNew = array();"\` |
|        - |  1057 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1058 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1059 | `    "}"\` |
|        - |  1060 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1061 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1062 | `	"/* Erase */"\` |
|        - |  1063 | `	"array_erase($pArray);"\` |
|        - |  1064 | `	"/* Unshift */"\` |
|        - |  1065 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1066 | `	"return sizeof($pArray);"\` |
|        - |  1067 | `    "}"\` |
|        - |  1068 | `	"function array_merge_recursive(){"\` |
|        - |  1069 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1070 | `    "$arrays = func_get_args();"\` |
|        - |  1071 | `    "$narrays = count($arrays);"\` |
|        - |  1072 | `    "$ret = array();"\` |
|        - |  1073 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1074 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1075 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1076 | `	 " }"\` |
|        - |  1077 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1078 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1079 | `     "  if( $keyIsInt ) {"\` |
|        - |  1080 | `     "   $ret[] = $value;"\` |
|        - |  1081 | `     "  } else {"\` |
|        - |  1082 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1083 | `     "    $cur = $ret[$key];"\` |
|        - |  1084 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1085 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1086 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1087 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1088 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1089 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1090 | `     "    } else {"\` |
|        - |  1091 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1092 | `     "    }"\` |
|        - |  1093 | `     "   } else {"\` |
|        - |  1094 | `     "    $ret[$key] = $value;"\` |
|        - |  1095 | `     "   }"\` |
|        - |  1096 | `     "  }"\` |
|        - |  1097 | `     " }"\` |
|        - |  1098 | `	 " }"\` |
|        - |  1099 | `	 " return $ret;"\` |
|        - |  1100 | `    "}"\` |
|        - |  1101 | `	"function max(){"\` |
|        - |  1102 | `    "  $pArgs = func_get_args();"\` |
|        - |  1103 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1104 | `	"  return null;"\` |
|        - |  1105 | `    " }"\` |
|        - |  1106 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1107 | `    " $pArg = $pArgs[0];"\` |
|        - |  1108 | `	" if( !is_array($pArg) ){"\` |
|        - |  1109 | `	"   return $pArg; "\` |
|        - |  1110 | `	" }"\` |
|        - |  1111 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1112 | `	"   return null;"\` |
|        - |  1113 | `	" }"\` |
|        - |  1114 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1115 | `	" reset($pArg);"\` |
|        - |  1116 | `	" $max = current($pArg);"\` |
|        - |  1117 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1118 | `	"   if( $val > $max ){"\` |
|        - |  1119 | `	"     $max = $val;"\` |
|        - |  1120 | `    " }"\` |
|        - |  1121 | `	" }"\` |
|        - |  1122 | `	" return $max;"\` |
|        - |  1123 | `    " }"\` |
|        - |  1124 | `    " $max = $pArgs[0];"\` |
|        - |  1125 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1126 | `    " $val = $pArgs[$i];"\` |
|        - |  1127 | `	"if( $val > $max ){"\` |
|        - |  1128 | `	" $max = $val;"\` |
|        - |  1129 | `	"}"\` |
|        - |  1130 | `    " }"\` |
|        - |  1131 | `	" return $max;"\` |
|        - |  1132 | `    "}"\` |
|        - |  1133 | `	"function min(){"\` |
|        - |  1134 | `    "  $pArgs = func_get_args();"\` |
|        - |  1135 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1136 | `	"  return null;"\` |
|        - |  1137 | `    " }"\` |
|        - |  1138 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1139 | `    " $pArg = $pArgs[0];"\` |
|        - |  1140 | `	" if( !is_array($pArg) ){"\` |
|        - |  1141 | `	"   return $pArg; "\` |
|        - |  1142 | `	" }"\` |
|        - |  1143 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1144 | `	"   return null;"\` |
|        - |  1145 | `	" }"\` |
|        - |  1146 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1147 | `	" reset($pArg);"\` |
|        - |  1148 | `	" $min = current($pArg);"\` |
|        - |  1149 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1150 | `	"   if( $val < $min ){"\` |
|        - |  1151 | `	"     $min = $val;"\` |
|        - |  1152 | `    " }"\` |
|        - |  1153 | `	" }"\` |
|        - |  1154 | `	" return $min;"\` |
|        - |  1155 | `    " }"\` |
|        - |  1156 | `    " $min = $pArgs[0];"\` |
|        - |  1157 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1158 | `    " $val = $pArgs[$i];"\` |
|        - |  1159 | `	"if( $val < $min ){"\` |
|        - |  1160 | `	" $min = $val;"\` |
|        - |  1161 | `	" }"\` |
|        - |  1162 | `    " }"\` |
|        - |  1163 | `	" return $min;"\` |
|        - |  1164 | `	"}"\` |
|        - |  1165 | `	"function fileowner(string $file){"\` |
|        - |  1166 | `    " $a = stat($file);"\` |
|        - |  1167 | `	" if( !is_array($a) ){"\` |
|        - |  1168 | `	"	return false;"\` |
|        - |  1169 | `	" }"\` |
|        - |  1170 | `	" return $a['uid'];"\` |
|        - |  1171 | `    "}"\` |
|        - |  1172 | `    "function filegroup(string $file){"\` |
|        - |  1173 | `	" $a = stat($file);"\` |
|        - |  1174 | `	" if( !is_array($a) ){"\` |
|        - |  1175 | `	"	return false;"\` |
|        - |  1176 | `	" }"\` |
|        - |  1177 | `	" return $a['gid'];"\` |
|        - |  1178 | `    "}"\` |
|        - |  1179 | `	 "function fileinode(string $file){"\` |
|        - |  1180 | `	" $a = stat($file);"\` |
|        - |  1181 | `	" if( !is_array($a) ){"\` |
|        - |  1182 | `	"	return false;"\` |
|        - |  1183 | `	" }"\` |
|        - |  1184 | `	" return $a['ino'];"\` |
|        - |  1185 | `    "}"` |
|        - |  1186 |  |
|        - |  1187 | `/*` |
|        - |  1188 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1189 | ` * start compiling the target PHP program.` |
|        - |  1190 | ` */` |
|     2268 |  1191 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1192 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1193 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1194 | `	 )` |
|        2 |  1195 |  |
|        - |  1196 | `	SyString sBuiltin;` |
|        - |  1197 | `	ph7_value *pObj;` |
|        - |  1198 | `	sxi32 rc;` |
|        - |  1199 | `	/* Zero the structure */` |
|     2270 |  1200 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1201 | `	/* Initialize VM fields */` |
|     2270 |  1202 | `	pVm->pEngine = &(*pEngine);` |
|     2270 |  1203 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1204 | `	/* Instructions containers */` |
|     2270 |  1205 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2270 |  1206 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2270 |  1207 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1208 | `	/* Object containers */` |
|     2270 |  1209 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2270 |  1210 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1211 | `	/* Virtual machine internal containers */` |
|     2270 |  1212 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2270 |  1213 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2270 |  1214 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2270 |  1215 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2270 |  1216 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2270 |  1217 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2270 |  1218 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2270 |  1219 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2270 |  1220 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2270 |  1221 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2270 |  1222 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2270 |  1223 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2270 |  1224 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2270 |  1225 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2270 |  1226 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|        - |  1227 | `	/* Configuration containers */` |
|     2270 |  1228 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2270 |  1229 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2270 |  1230 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2270 |  1231 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2270 |  1232 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1233 | `	/* Error callbacks containers */` |
|     2270 |  1234 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2270 |  1235 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2270 |  1236 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2270 |  1237 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2270 |  1238 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1239 | `	/* Set a default recursion limit */` |
|        - |  1240 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2270 |  1241 | `	pVm->nMaxDepth = 32;` |
|        - |  1242 | `#else` |
|        - |  1243 | `	pVm->nMaxDepth = 16;` |
|        - |  1244 | `#endif` |
|        - |  1245 | `	/* Default assertion flags */` |
|     2270 |  1246 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1247 | `	/* JSON return status */` |
|     2270 |  1248 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1249 | `	/* PRNG context */` |
|     2270 |  1250 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1251 | `	/* Install the null constant */` |
|     2270 |  1252 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2270 |  1253 | `	if( pObj == 0 ){` |
|      ! 0 |  1254 | `		rc = SXERR_MEM;` |
|      ! 0 |  1255 | `		goto Err;` |
|        - |  1256 | `	}` |
|     2270 |  1257 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1258 | `	/* Install the boolean TRUE constant */` |
|     2270 |  1259 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2270 |  1260 | `	if( pObj == 0 ){` |
|      ! 0 |  1261 | `		rc = SXERR_MEM;` |
|      ! 0 |  1262 | `		goto Err;` |
|        - |  1263 | `	}` |
|     2270 |  1264 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1265 | `	/* Install the boolean FALSE constant */` |
|     2270 |  1266 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2270 |  1267 | `	if( pObj == 0 ){` |
|      ! 0 |  1268 | `		rc = SXERR_MEM;` |
|      ! 0 |  1269 | `		goto Err;` |
|        - |  1270 | `	}` |
|     2270 |  1271 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1272 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1273 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1274 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2270 |  1275 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2270 |  1276 | `	if( pObj == 0 ){` |
|      ! 0 |  1277 | `		rc = SXERR_MEM;` |
|      ! 0 |  1278 | `		goto Err;` |
|        - |  1279 | `	}` |
|     2270 |  1280 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1281 | `	/* Create the global frame */` |
|     2270 |  1282 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2270 |  1283 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1284 | `		goto Err;` |
|        - |  1285 | `	}` |
|        - |  1286 | `	/* Initialize the code generator */` |
|     2270 |  1287 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2270 |  1288 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1289 | `		goto Err;` |
|        - |  1290 | `	}` |
|        - |  1291 | `	/* VM correctly initialized,set the magic number */` |
|     2270 |  1292 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2270 |  1293 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1294 | `	/* Compile the built-in library */` |
|     2270 |  1295 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1296 | `	/* Reset the code generator */` |
|     2270 |  1297 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2270 |  1298 | `	return SXRET_OK;` |
|      ! 0 |  1299 | `Err:` |
|      ! 0 |  1300 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1301 | `	return rc;` |
|     1136 |  1302 |  |
|        - |  1303 | `/*` |
|        - |  1304 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1305 | ` * routine which store the output in an internal blob.` |
|        - |  1306 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1307 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1308 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1309 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1310 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1311 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1312 | ` * to finish executing and extracting the output.` |
|        - |  1313 | ` */` |
|      ! 0 |  1314 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1315 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1316 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1317 | `	void *pUserData     /* User private data */` |
|        - |  1318 | `	)` |
|      ! 0 |  1319 |  |
|        - |  1320 | `	 sxi32 rc;` |
|        - |  1321 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1322 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1323 | `	 return rc;` |
|      ! 0 |  1324 |  |
|        - |  1325 | `#define VM_STACK_GUARD 16` |
|        - |  1326 | `/*` |
|        - |  1327 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1328 | ` * our compiled PHP program.` |
|        - |  1329 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1330 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1331 | ` */` |
|    29464 |  1332 | `static ph7_value * VmNewOperandStack(` |
|        - |  1333 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1334 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1335 | `	)` |
|        2 |  1336 |  |
|        - |  1337 | `	ph7_value *pStack;` |
|        - |  1338 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1339 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1340 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1341 | `  ** on the maximum stack depth required.` |
|        - |  1342 | `  **` |
|        - |  1343 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1344 | `  */` |
|    29466 |  1345 | `	nInstr += VM_STACK_GUARD;` |
|    29466 |  1346 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    29466 |  1347 | `	if( pStack == 0 ){` |
|      ! 0 |  1348 | `		return 0;` |
|        - |  1349 | `	}` |
|        - |  1350 | `	/* Initialize the operand stack */` |
|  1871308 |  1351 | `	while( nInstr > 0 ){` |
|  1841844 |  1352 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1841844 |  1353 | `		--nInstr;` |
|        2 |  1354 | `	}` |
|        - |  1355 | `	/* Ready for bytecode execution */` |
|    29466 |  1356 | `	return pStack;` |
|    14734 |  1357 |  |
|        - |  1358 | `/* Forward declaration */` |
|        - |  1359 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1360 | `/*` |
|        - |  1361 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1362 | ` * This routine gets called by the PH7 engine after` |
|        - |  1363 | ` * successful compilation of the target PHP program.` |
|        - |  1364 | ` */` |
|     2030 |  1365 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1366 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1367 | `	)` |
|        2 |  1368 |  |
|        - |  1369 | `	SyHashEntry *pEntry;` |
|        - |  1370 | `	sxi32 rc;` |
|     2032 |  1371 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1372 | `		/* Initialize your VM first */` |
|      ! 0 |  1373 | `		return SXERR_CORRUPT;` |
|        - |  1374 | `	}` |
|        - |  1375 | `	/* Mark the VM ready for byte-code execution */` |
|     2032 |  1376 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1377 | `	/* Release the code generator now we have compiled our program */` |
|     2032 |  1378 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1379 | `	/* Emit the DONE instruction */` |
|     2032 |  1380 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2032 |  1381 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1382 | `		return SXERR_MEM;` |
|        - |  1383 | `	}` |
|        - |  1384 | `	/* Script return value */` |
|     2032 |  1385 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1386 | `	/* Allocate a new operand stack */` |
|     2032 |  1387 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2032 |  1388 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1389 | `		return SXERR_MEM;` |
|        - |  1390 | `	}` |
|        - |  1391 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1392 | `	 * private data. */` |
|     2032 |  1393 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2032 |  1394 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1395 | `	/* Allocate the reference table */` |
|     2032 |  1396 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2032 |  1397 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2032 |  1398 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1399 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1400 | `		return SXERR_MEM;` |
|        - |  1401 | `	}` |
|        - |  1402 | `	/* Zero the reference table */` |
|     2032 |  1403 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1404 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2032 |  1405 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2032 |  1406 | `	if( rc != SXRET_OK ){` |
|        - |  1407 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1408 | `		return rc;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2032 |  1411 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2032 |  1412 | `	if( rc != SXRET_OK ){` |
|        - |  1413 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1414 | `		return rc;` |
|        - |  1415 | `	}` |
|        - |  1416 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2032 |  1417 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1418 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2032 |  1419 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1420 | `	/* Initialize and install static and constants class attributes */` |
|     2032 |  1421 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    26430 |  1422 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    24400 |  1423 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    24400 |  1424 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1425 | `			return rc;` |
|        - |  1426 | `		}` |
|        2 |  1427 | `	}` |
|        - |  1428 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2032 |  1429 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1430 | `	/* VM is ready for bytecode execution */` |
|     2032 |  1431 | `	return SXRET_OK;` |
|     1017 |  1432 |  |
|        - |  1433 | `/*` |
|        - |  1434 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1435 | ` */` |
|      ! 0 |  1436 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1437 |  |
|      ! 0 |  1438 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1439 | `		return SXERR_CORRUPT;` |
|        - |  1440 | `	}` |
|        - |  1441 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1442 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1443 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1444 | `	/* Set the ready flag */` |
|      ! 0 |  1445 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1446 | `	return SXRET_OK;` |
|      ! 0 |  1447 |  |
|        - |  1448 | `/*` |
|        - |  1449 | ` * Release a Virtual Machine.` |
|        - |  1450 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1451 | ` */` |
|     2022 |  1452 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1453 |  |
|        - |  1454 | `	/* Set the stale magic number */` |
|     2024 |  1455 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1456 | `	/* Release the private memory subsystem */` |
|     2024 |  1457 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2024 |  1458 | `	return SXRET_OK;` |
|        2 |  1459 |  |
|        - |  1460 | `/*` |
|        - |  1461 | ` * Initialize a foreign function call context.` |
|        - |  1462 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1463 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1464 | ` * functions.` |
|        - |  1465 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1466 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1467 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1468 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1469 | ` */` |
|   559396 |  1470 | `static sxi32 VmInitCallContext(` |
|        - |  1471 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1472 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1473 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1474 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1475 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1476 | `	)` |
|        2 |  1477 |  |
|   559398 |  1478 | `	pOut->pFunc = pFunc;` |
|   559398 |  1479 | `	pOut->pVm   = pVm;` |
|   559398 |  1480 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   559398 |  1481 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1482 | `	/* Assume a null return value */` |
|   559398 |  1483 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   559398 |  1484 | `	pOut->pRet = pRet;` |
|   559398 |  1485 | `	pOut->iFlags = iFlags;` |
|   559398 |  1486 | `	return SXRET_OK;` |
|        2 |  1487 |  |
|        - |  1488 | `/*` |
|        - |  1489 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1490 | ` * left behind.` |
|        - |  1491 | ` */` |
|   559396 |  1492 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1493 |  |
|        - |  1494 | `	sxu32 n;` |
|   559398 |  1495 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6534 |  1496 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    18606 |  1497 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12074 |  1498 | `			if( apObj[n] == 0 ){` |
|        - |  1499 | `				/* Already released */` |
|      250 |  1500 | `				continue;` |
|        - |  1501 | `			}` |
|    11826 |  1502 | `			PH7_MemObjRelease(apObj[n]);` |
|    11826 |  1503 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     5914 |  1504 | `		}` |
|     6534 |  1505 | `		SySetRelease(&pCtx->sVar);` |
|     3266 |  1506 | `	}` |
|   559398 |  1507 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1508 | `		ph7_aux_data *aAux;` |
|        - |  1509 | `		void *pChunk;` |
|        - |  1510 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1511 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1512 | `		 */` |
|        9 |  1513 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1514 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1515 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1516 | `			/* Release the chunk */` |
|       25 |  1517 | `			if( pChunk ){` |
|       25 |  1518 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1519 | `			}` |
|       13 |  1520 | `		}` |
|        9 |  1521 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1522 | `	}` |
|   559398 |  1523 |  |
|        - |  1524 | `/*` |
|        - |  1525 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1526 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1527 | ` */` |
|      248 |  1528 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1529 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1530 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1531 | `	)` |
|        2 |  1532 |  |
|      250 |  1533 | `	if( pValue == 0 ){` |
|        - |  1534 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1535 | `		return;` |
|        - |  1536 | `	}` |
|      250 |  1537 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1538 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1539 | `		sxu32 n;` |
|      936 |  1540 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1541 | `			if( apObj[n] == pValue ){` |
|      250 |  1542 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1543 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1544 | `				/* Mark as released */` |
|      250 |  1545 | `				apObj[n] = 0;` |
|      250 |  1546 | `				break;` |
|        - |  1547 | `			}` |
|      345 |  1548 | `		}` |
|      124 |  1549 | `	}` |
|      126 |  1550 |  |
|        - |  1551 | `/*` |
|        - |  1552 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1553 | ` */` |
|  3396434 |  1554 | `static void VmPopOperand(` |
|        - |  1555 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1556 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1557 | `	)` |
|        2 |  1558 |  |
|  3396436 |  1559 | `	ph7_value *pTos = *ppTos;` |
|  7180886 |  1560 | `	while( nPop > 0 ){` |
|  3784452 |  1561 | `		PH7_MemObjRelease(pTos);` |
|  3784452 |  1562 | `		pTos--;` |
|  3784452 |  1563 | `		nPop--;` |
|        2 |  1564 | `	}` |
|        - |  1565 | `	/* Top of the stack */` |
|  3396436 |  1566 | `	*ppTos = pTos;` |
|  3396436 |  1567 |  |
|        - |  1568 | `/*` |
|        - |  1569 | ` * Reserve a memory object.` |
|        - |  1570 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1571 | ` */` |
|  2983300 |  1572 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1573 |  |
|  2983302 |  1574 | `	ph7_value *pObj = 0;` |
|        - |  1575 | `	VmSlot *pSlot;` |
|        - |  1576 | `	sxu32 nIdx;` |
|        - |  1577 | `	/* Check for a free slot */` |
|  2983302 |  1578 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2983302 |  1579 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2983302 |  1580 | `	if( pSlot ){` |
|   846640 |  1581 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   846640 |  1582 | `		nIdx = pSlot->nIdx;` |
|   423319 |  1583 | `	}` |
|  2983302 |  1584 | `	if( pObj == 0 ){` |
|        - |  1585 | `		/* Reserve a new memory object */` |
|  2136664 |  1586 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2136664 |  1587 | `		if( pObj == 0 ){` |
|      ! 0 |  1588 | `			return 0;` |
|        - |  1589 | `		}` |
|  1068331 |  1590 | `	}` |
|        - |  1591 | `	/* Set a null default value */` |
|  2983302 |  1592 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2983302 |  1593 | `	pObj->nIdx = nIdx;` |
|  2983302 |  1594 | `	return pObj;` |
|  1491652 |  1595 |  |
|        - |  1596 | `/*` |
|        - |  1597 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1598 | ` */` |
|    25858 |  1599 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1600 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1601 | `	const char *zKey,  /* Entry key */` |
|        - |  1602 | `	sxu32 nByte,       /* Key length */` |
|        - |  1603 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1604 | `	)` |
|        2 |  1605 |  |
|        - |  1606 | `	ph7_value sKey;` |
|        - |  1607 | `	sxi32 rc;` |
|    25860 |  1608 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    25860 |  1609 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1610 | `	/* Perform the insertion */` |
|    25860 |  1611 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    25860 |  1612 | `	PH7_MemObjRelease(&sKey);` |
|    25860 |  1613 | `	return rc;` |
|        2 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1617 | ` * Return a pointer to the variable value on success.` |
|        - |  1618 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1619 | ` */` |
|  3223478 |  1620 | `static ph7_value * VmExtractMemObj(` |
|        - |  1621 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1622 | `	const SyString *pName, /* Variable name */` |
|        - |  1623 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1624 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1625 | `	)` |
|        2 |  1626 |  |
|  3223480 |  1627 | `	int bNullify = FALSE;` |
|        - |  1628 | `	SyHashEntry *pEntry;` |
|        - |  1629 | `	VmFrame *pFrame;` |
|        - |  1630 | `	ph7_value *pObj;` |
|        - |  1631 | `	sxu32 nIdx;` |
|        - |  1632 | `	sxi32 rc;` |
|        - |  1633 | `	/* Point to the top active frame */` |
|  3223480 |  1634 | `	pFrame = pVm->pFrame;` |
|  3223492 |  1635 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  1636 | `		/* Safely ignore the exception frame */` |
|       13 |  1637 | `		pFrame = pFrame->pParent; /* Parent frame */` |
|        1 |  1638 | `	}` |
|        - |  1639 | `	/* Perform the lookup */` |
|  3223480 |  1640 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1641 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1642 | `		pName = &sAnnon;` |
|        - |  1643 | `		/* Always nullify the object */` |
|      ! 0 |  1644 | `		bNullify = TRUE;` |
|      ! 0 |  1645 | `		bDup = FALSE;` |
|      ! 0 |  1646 | `	}` |
|        - |  1647 | `	/* Check the superglobals table first */` |
|  3223480 |  1648 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3223480 |  1649 | `	if( pEntry == 0 ){` |
|        - |  1650 | `		/* Query the top active frame */` |
|  3223444 |  1651 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3223444 |  1652 | `		if( pEntry == 0 ){` |
|    78868 |  1653 | `			char *zName = (char *)pName->zString;` |
|        - |  1654 | `			VmSlot sLocal;` |
|    78868 |  1655 | `			if( !bCreate ){` |
|        - |  1656 | `				/* Do not create the variable,return NULL instead */` |
|      634 |  1657 | `				return 0;` |
|        - |  1658 | `			}` |
|        - |  1659 | `			/* No such variable,automatically create a new one and install` |
|        - |  1660 | `			 * it in the current frame.` |
|        - |  1661 | `			 */` |
|    78236 |  1662 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    78236 |  1663 | `			if( pObj == 0 ){` |
|      ! 0 |  1664 | `				return 0;` |
|        - |  1665 | `			}` |
|    78236 |  1666 | `			nIdx = pObj->nIdx;` |
|    78236 |  1667 | `			if( bDup ){` |
|        - |  1668 | `				/* Duplicate name */` |
|      164 |  1669 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1670 | `				if( zName == 0 ){` |
|      ! 0 |  1671 | `					return 0;` |
|        - |  1672 | `				}` |
|       81 |  1673 | `			}` |
|        - |  1674 | `			/* Link to the top active VM frame */` |
|    78236 |  1675 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    78236 |  1676 | `			if( rc != SXRET_OK ){` |
|        - |  1677 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1678 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1679 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1680 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1681 | `				return 0;` |
|        - |  1682 | `			}` |
|    78236 |  1683 | `			if( pFrame->pParent != 0 ){` |
|        - |  1684 | `				/* Local variable */` |
|    72702 |  1685 | `				sLocal.nIdx = nIdx;` |
|    72702 |  1686 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    36352 |  1687 | `			}else{` |
|        - |  1688 | `				/* Register in the $GLOBALS array */` |
|     5536 |  1689 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1690 | `			}` |
|        - |  1691 | `			/* Install in the reference table */` |
|    78236 |  1692 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1693 | `			/* Save object index */` |
|    78236 |  1694 | `			pObj->nIdx = nIdx;` |
|    39119 |  1695 | `		}else{` |
|        - |  1696 | `			/* Extract variable contents */` |
|  3144578 |  1697 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3144578 |  1698 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3144578 |  1699 | `			if( bNullify && pObj ){` |
|      ! 0 |  1700 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1701 | `			}` |
|        - |  1702 | `		}` |
|  1611517 |  1703 | `	}else{` |
|        - |  1704 | `		/* Superglobal */` |
|       38 |  1705 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1706 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1707 | `	}` |
|  3222848 |  1708 | `	return pObj;` |
|  1611851 |  1709 |  |
|        - |  1710 | `/*` |
|        - |  1711 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1712 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1713 | ` */` |
|     2056 |  1714 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1715 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1716 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1717 | `	sxu32 nByte        /* zName length */` |
|        - |  1718 | `	)` |
|        2 |  1719 |  |
|        - |  1720 | `	SyHashEntry *pEntry;` |
|        - |  1721 | `	ph7_value *pValue;` |
|        - |  1722 | `	sxu32 nIdx;` |
|        - |  1723 | `	/* Query the superglobal table */` |
|     2058 |  1724 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2058 |  1725 | `	if( pEntry == 0 ){` |
|        - |  1726 | `		/* No such entry */` |
|      ! 0 |  1727 | `		return 0;` |
|        - |  1728 | `	}` |
|        - |  1729 | `	/* Extract the superglobal index in the global object pool */` |
|     2058 |  1730 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1731 | `	/* Extract the variable value  */` |
|     2058 |  1732 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2058 |  1733 | `	return pValue;` |
|     1030 |  1734 |  |
|        - |  1735 | `/*` |
|        - |  1736 | ` * Perform a raw hashmap insertion.` |
|        - |  1737 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1738 | ` */` |
|     2054 |  1739 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1740 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1741 | `	const char *zKey,   /* Entry key */` |
|        - |  1742 | `	int nKeylen,        /* zKey length*/` |
|        - |  1743 | `	const char *zData,  /* Entry data */` |
|        - |  1744 | `	int nLen            /* zData length */` |
|        - |  1745 | `	)` |
|        2 |  1746 |  |
|        - |  1747 | `	ph7_value sKey,sValue;` |
|        - |  1748 | `	sxi32 rc;` |
|     2056 |  1749 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2056 |  1750 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2056 |  1751 | `	if( zKey ){` |
|     2034 |  1752 | `		if( nKeylen < 0 ){` |
|     2034 |  1753 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1016 |  1754 | `		}` |
|     2034 |  1755 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1016 |  1756 | `	}` |
|     2056 |  1757 | `	if( zData ){` |
|     2056 |  1758 | `		if( nLen < 0 ){` |
|        - |  1759 | `			/* Compute length automatically */` |
|      ! 0 |  1760 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1761 | `		}` |
|     2056 |  1762 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1027 |  1763 | `	}` |
|        - |  1764 | `	/* Perform the insertion */` |
|     2056 |  1765 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2056 |  1766 | `	PH7_MemObjRelease(&sKey);` |
|     2056 |  1767 | `	PH7_MemObjRelease(&sValue);` |
|     2056 |  1768 | `	return rc;` |
|        2 |  1769 |  |
|        - |  1770 | `/*` |
|        - |  1771 | ` * Configure a working virtual machine instance.` |
|        - |  1772 | ` *` |
|        - |  1773 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1774 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1775 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1776 | ` * The second argument to this function is an integer configuration option` |
|        - |  1777 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1778 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1779 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1780 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1781 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1782 | ` */` |
|    32504 |  1783 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1784 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1785 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1786 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1787 | `	)` |
|        2 |  1788 |  |
|    32506 |  1789 | `	sxi32 rc = SXRET_OK;` |
|    32506 |  1790 | `	switch(nOp){` |
|     1015 |  1791 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2032 |  1792 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2032 |  1793 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1794 | `		/* VM output consumer callback */` |
|        - |  1795 | `#ifdef UNTRUST` |
|        - |  1796 | `		if( xConsumer == 0 ){` |
|        - |  1797 | `			rc = SXERR_CORRUPT;` |
|        - |  1798 | `			break;` |
|        - |  1799 | `		}` |
|        - |  1800 | `#endif` |
|        - |  1801 | `		/* Install the output consumer */` |
|     2032 |  1802 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2032 |  1803 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2032 |  1804 | `		break;` |
|        - |  1805 | `							   }` |
|     1015 |  1806 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1807 | `		/* Import path */` |
|        - |  1808 | `		  const char *zPath;` |
|        - |  1809 | `		  SyString sPath;` |
|     2032 |  1810 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1811 | `#if defined(UNTRUST)` |
|        - |  1812 | `		  if( zPath == 0 ){` |
|        - |  1813 | `			  rc = SXERR_EMPTY;` |
|        - |  1814 | `			  break;` |
|        - |  1815 | `		  }` |
|        - |  1816 | `#endif` |
|     2032 |  1817 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1818 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1819 | `#ifdef __WINNT__` |
|        2 |  1820 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1821 | `#endif` |
|     4062 |  1822 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1823 | `		  /* Remove leading and trailing white spaces */` |
|     2032 |  1824 | `		  SyStringFullTrim(&sPath);` |
|     2032 |  1825 | `		  if( sPath.nByte > 0 ){` |
|        - |  1826 | `			  /* Store the path in the corresponding conatiner */` |
|     2032 |  1827 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1015 |  1828 | `		  }` |
|     2032 |  1829 | `		  break;` |
|        - |  1830 | `									 }` |
|     1015 |  1831 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1832 | `		/* Run-Time Error report */` |
|     2032 |  1833 | `		pVm->bErrReport = 1;` |
|     2032 |  1834 | `		break;` |
|      ! 0 |  1835 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1836 | `		/* Recursion depth */` |
|      ! 0 |  1837 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1838 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1839 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1840 | `		}` |
|      ! 0 |  1841 | `		break;` |
|        - |  1842 | `									   }` |
|      ! 0 |  1843 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1844 | `		/* VM output length in bytes */` |
|      ! 0 |  1845 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1846 | `#ifdef UNTRUST` |
|        - |  1847 | `		if( pOut == 0 ){` |
|        - |  1848 | `			rc = SXERR_CORRUPT;` |
|        - |  1849 | `			break;` |
|        - |  1850 | `		}` |
|        - |  1851 | `#endif` |
|      ! 0 |  1852 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1853 | `		break;` |
|        - |  1854 | `							   }` |
|        - |  1855 |  |
|    10150 |  1856 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1857 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1858 | `		/* Create a new superglobal/global variable */` |
|    20302 |  1859 | `		const char *zName = va_arg(ap,const char *);` |
|    20302 |  1860 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1861 | `		SyHashEntry *pEntry;` |
|        - |  1862 | `		ph7_value *pObj;` |
|        - |  1863 | `		sxu32 nByte;` |
|        - |  1864 | `		sxu32 nIdx;` |
|        - |  1865 | `#ifdef UNTRUST` |
|        - |  1866 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1867 | `			rc = SXERR_CORRUPT;` |
|        - |  1868 | `			break;` |
|        - |  1869 | `		}` |
|        - |  1870 | `#endif` |
|    20302 |  1871 | `		nByte = SyStrlen(zName);` |
|    20302 |  1872 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1873 | `			/* Check if the superglobal is already installed */` |
|    20302 |  1874 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    10152 |  1875 | `		}else{` |
|        - |  1876 | `			/* Query the top active VM frame */` |
|      ! 0 |  1877 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1878 | `		}` |
|    20302 |  1879 | `		if( pEntry ){` |
|        - |  1880 | `			/* Variable already installed */` |
|      ! 0 |  1881 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1882 | `			/* Extract contents */` |
|      ! 0 |  1883 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1884 | `			if( pObj ){` |
|        - |  1885 | `				/* Overwrite old contents */` |
|      ! 0 |  1886 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1887 | `			}` |
|      ! 0 |  1888 | `		}else{` |
|        - |  1889 | `			/* Install a new variable */` |
|    20302 |  1890 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    20302 |  1891 | `			if( pObj == 0 ){` |
|      ! 0 |  1892 | `				rc = SXERR_MEM;` |
|      ! 0 |  1893 | `				break;` |
|        - |  1894 | `			}` |
|    20302 |  1895 | `			nIdx = pObj->nIdx;` |
|        - |  1896 | `			/* Copy value */` |
|    20302 |  1897 | `			PH7_MemObjStore(pValue,pObj);` |
|    20302 |  1898 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1899 | `				/* Install the superglobal */` |
|    20302 |  1900 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    10152 |  1901 | `			}else{` |
|        - |  1902 | `				/* Install in the current frame */` |
|      ! 0 |  1903 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1904 | `			}` |
|    20302 |  1905 | `			if( rc == SXRET_OK ){` |
|        - |  1906 | `				SyHashEntry *pRef;` |
|    20302 |  1907 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    20302 |  1908 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    10152 |  1909 | `				}else{` |
|      ! 0 |  1910 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1911 | `				}` |
|        - |  1912 | `				/* Install in the reference table */` |
|    20302 |  1913 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    20302 |  1914 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1915 | `					/* Register in the $GLOBALS array */` |
|    20302 |  1916 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    10150 |  1917 | `				}` |
|    10150 |  1918 | `			}` |
|        - |  1919 | `		}` |
|    20302 |  1920 | `		break;` |
|        - |  1921 | `									}` |
|     1016 |  1922 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1923 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1924 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1925 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1926 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1927 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1928 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2034 |  1929 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2034 |  1930 | `		const char *zValue = va_arg(ap,const char *);` |
|     2034 |  1931 | `		int nLen = va_arg(ap,int);` |
|        - |  1932 | `		ph7_hashmap *pMap;` |
|        - |  1933 | `		ph7_value *pValue;` |
|     2034 |  1934 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1935 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1936 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2033 |  1937 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1938 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1939 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2032 |  1940 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1941 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1942 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2032 |  1943 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1944 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1945 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2032 |  1946 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1947 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1948 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2032 |  1949 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1950 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1951 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1952 | `		}else{` |
|        - |  1953 | `			/* Extract the $_SERVER superglobal */` |
|     2032 |  1954 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1955 | `		}` |
|     2034 |  1956 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1957 | `			/* No such entry */` |
|      ! 0 |  1958 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1959 | `			break;` |
|        - |  1960 | `		}` |
|        - |  1961 | `		/* Point to the hashmap */` |
|     2034 |  1962 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1963 | `		/* Perform the insertion */` |
|     2034 |  1964 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2034 |  1965 | `		break;` |
|        - |  1966 | `								   }` |
|       11 |  1967 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  1968 | `		/* Script arguments */` |
|       24 |  1969 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  1970 | `		ph7_hashmap *pMap;` |
|        - |  1971 | `		ph7_value *pValue;` |
|        - |  1972 | `		sxu32 n;` |
|       24 |  1973 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  1974 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  1975 | `			break;` |
|        - |  1976 | `		}` |
|        - |  1977 | `		/* Extract the $argv array */` |
|       24 |  1978 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  1979 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1980 | `			/* No such entry */` |
|      ! 0 |  1981 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1982 | `			break;` |
|        - |  1983 | `		}` |
|        - |  1984 | `		/* Point to the hashmap */` |
|       24 |  1985 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1986 | `		/* Perform the insertion */` |
|       24 |  1987 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  1988 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  1989 | `		if( rc == SXRET_OK ){` |
|       24 |  1990 | `			if( pMap->nEntry > 1 ){` |
|        - |  1991 | `				/* Append space separator first */` |
|       18 |  1992 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  1993 | `			}` |
|       24 |  1994 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  1995 | `		}` |
|       24 |  1996 | `		break;` |
|        - |  1997 | `								  }` |
|      ! 0 |  1998 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  1999 | `		/* error_log() consumer */` |
|      ! 0 |  2000 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2001 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2002 | `		break;` |
|        - |  2003 | `										}` |
|      ! 0 |  2004 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2005 | `		/* Script return value */` |
|      ! 0 |  2006 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2007 | `#ifdef UNTRUST` |
|        - |  2008 | `		if( ppValue == 0 ){` |
|        - |  2009 | `			rc = SXERR_CORRUPT;` |
|        - |  2010 | `			break;` |
|        - |  2011 | `		}` |
|        - |  2012 | `#endif` |
|      ! 0 |  2013 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2014 | `		break;` |
|        - |  2015 | `								   }` |
|     2030 |  2016 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2017 | `		/* Register an IO stream device */` |
|     4062 |  2018 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2019 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6090 |  2020 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4062 |  2021 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2022 | `				/* Invalid stream */` |
|      ! 0 |  2023 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2024 | `				break;` |
|        - |  2025 | `		}` |
|     4062 |  2026 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2027 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2032 |  2028 | `			pVm->pDefStream = pStream;` |
|     1015 |  2029 | `		}` |
|        - |  2030 | `		/* Insert in the appropriate container */` |
|     4062 |  2031 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4062 |  2032 | `		break;` |
|        - |  2033 | `								  }` |
|      ! 0 |  2034 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2035 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2036 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2037 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2038 | `#ifdef UNTRUST` |
|        - |  2039 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2040 | `			rc = SXERR_CORRUPT;` |
|        - |  2041 | `			break;` |
|        - |  2042 | `		}` |
|        - |  2043 | `#endif` |
|      ! 0 |  2044 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2045 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2046 | `		break;` |
|        - |  2047 | `									   }` |
|      ! 0 |  2048 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2049 | `		/* Raw HTTP request*/` |
|      ! 0 |  2050 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2051 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2052 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2053 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2054 | `			break;` |
|        - |  2055 | `		}` |
|      ! 0 |  2056 | `		if( nByte < 0 ){` |
|        - |  2057 | `			/* Compute length automatically */` |
|      ! 0 |  2058 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2059 | `		}` |
|        - |  2060 | `		/* Process the request */` |
|      ! 0 |  2061 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2062 | `		break;` |
|        - |  2063 | `									}` |
|      ! 0 |  2064 | `	default:` |
|        - |  2065 | `		/* Unknown configuration option */` |
|      ! 0 |  2066 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2067 | `		break;` |
|        - |  2068 | `	}` |
|    32506 |  2069 | `	return rc;` |
|        2 |  2070 |  |
|        - |  2071 | `/* Forward declaration */` |
|        - |  2072 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2073 | `/*` |
|        - |  2074 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2075 | ` * format.` |
|        - |  2076 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2077 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2078 | ` * (STDOUT).` |
|        - |  2079 | ` */` |
|        2 |  2080 | `static sxi32 VmByteCodeDump(` |
|        - |  2081 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2082 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2083 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2084 | `	)` |
|        1 |  2085 |  |
|        - |  2086 | `	static const char zDump[] = {` |
|        - |  2087 | `		"====================================================\n"` |
|        - |  2088 | `		"PH7 VM Dump\n"` |
|        - |  2089 | `		"====================================================\n"` |
|        - |  2090 | `	};` |
|        - |  2091 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2092 | `	sxi32 rc = SXRET_OK;` |
|        - |  2093 | `	sxu32 n;` |
|        - |  2094 | `	/* Point to the PH7 instructions */` |
|        3 |  2095 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2096 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2097 | `	n = 0;` |
|        3 |  2098 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2099 | `	/* Dump instructions */` |
|        6 |  2100 | `	for(;;){` |
|       13 |  2101 | `		if( pInstr >= pEnd ){` |
|        - |  2102 | `			/* No more instructions */` |
|        3 |  2103 | `			break;` |
|        - |  2104 | `		}` |
|        - |  2105 | `		/* Format and call the consumer callback */` |
|       16 |  2106 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       10 |  2107 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       10 |  2108 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       11 |  2109 | `		if( rc != SXRET_OK ){` |
|        - |  2110 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2111 | `			return rc;` |
|        - |  2112 | `		}` |
|       11 |  2113 | `		++n;` |
|       11 |  2114 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2115 | `	}` |
|        3 |  2116 | `	return rc;` |
|        2 |  2117 |  |
|        - |  2118 | `/* Forward declaration */` |
|        - |  2119 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2120 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2121 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2122 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2123 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2124 | `/*` |
|        - |  2125 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2126 | ` * consumer callback.` |
|        - |  2127 | ` */` |
|      540 |  2128 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2129 |  |
|      541 |  2130 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      541 |  2131 | `	sxi32 rc = SXRET_OK;` |
|        - |  2132 | `	/* Append a new line */` |
|        - |  2133 | `#ifdef __WINNT__` |
|        1 |  2134 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2135 | `#else` |
|      540 |  2136 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2137 | `#endif` |
|        - |  2138 | `	/* Invoke the output consumer callback */` |
|      541 |  2139 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      541 |  2140 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2141 | `		/* Increment output length */` |
|      541 |  2142 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      270 |  2143 | `	}` |
|      541 |  2144 | `	return rc;` |
|        1 |  2145 |  |
|        - |  2146 | `/*` |
|        - |  2147 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2148 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2149 | ` * information.` |
|        - |  2150 | ` */` |
|      130 |  2151 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2152 |  |
|      132 |  2153 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2154 | `		ph7_value apArg[4];` |
|        - |  2155 | `		ph7_value *apArgPtr[4];` |
|        - |  2156 | `		ph7_value sResult;` |
|        - |  2157 | `		SyString sErr;` |
|        - |  2158 | `		/* Prepare arguments */` |
|       61 |  2159 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2160 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2161 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2162 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2163 | `		if( pFile ){` |
|       61 |  2164 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2165 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2166 | `		}else{` |
|      ! 0 |  2167 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2168 | `		}` |
|       61 |  2169 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2170 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2171 | `		/* Set up pointer array */` |
|       61 |  2172 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2173 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2174 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2175 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2176 | `		/* Call the handler */` |
|       61 |  2177 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2178 | `		/* Check return value */` |
|       61 |  2179 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2180 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2181 | `		}` |
|        - |  2182 | `		/* Release */` |
|       61 |  2183 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2184 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2185 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2186 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2187 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2188 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2189 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2190 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2191 | `	}` |
|        - |  2192 | `	/* No handler, always call error handler */` |
|       71 |  2193 | `	return TRUE;` |
|       67 |  2194 |  |
|       94 |  2195 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2196 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2197 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2198 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2199 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2200 | `	)` |
|        2 |  2201 |  |
|       96 |  2202 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2203 | `	SyString *pFile;` |
|        - |  2204 | `	char *zErr;` |
|       96 |  2205 | `	sxi32 rc = SXRET_OK;` |
|       96 |  2206 | `	if( !pVm->bErrReport ){` |
|        - |  2207 | `		/* Don't bother reporting errors */` |
|        3 |  2208 | `		return SXRET_OK;` |
|        - |  2209 | `	}` |
|        - |  2210 | `	/* Reset the working buffer */` |
|       94 |  2211 | `	SyBlobReset(pWorker);` |
|        - |  2212 | `	/* Peek the processed file if available */` |
|       94 |  2213 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       94 |  2214 | `	if( pFile ){` |
|        - |  2215 | `		/* Append file name */` |
|       94 |  2216 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       94 |  2217 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       46 |  2218 | `	}` |
|        - |  2219 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2220 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2221 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2222 | `	 * E_DEPRECATED). */` |
|       94 |  2223 | `	zErr = "Error:  ";` |
|       94 |  2224 | `	switch(iErr){` |
|       17 |  2225 | `	case PH7_CTX_WARNING:` |
|       36 |  2226 | `		zErr = "Warning:  ";` |
|       36 |  2227 | `		break;` |
|        6 |  2228 | `	case PH7_CTX_NOTICE:` |
|       14 |  2229 | `		zErr = "Notice:  ";` |
|       12 |  2230 | `		break;` |
|       23 |  2231 | `	default:` |
|        - |  2232 | `		/* keep iErr unchanged */` |
|       46 |  2233 | `		break;` |
|        - |  2234 | `	}` |
|       94 |  2235 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       94 |  2236 | `	if( pFuncName ){` |
|        - |  2237 | `		/* Append function name first */` |
|       21 |  2238 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       21 |  2239 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       10 |  2240 | `	}` |
|       94 |  2241 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2242 | `	/* Check for user error handler.  compute length of C string */` |
|       94 |  2243 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       45 |  2244 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       22 |  2245 | `	}` |
|       94 |  2246 | `	return rc;` |
|       49 |  2247 |  |
|        - |  2248 | `/*` |
|        - |  2249 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2250 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2251 | ` * information.` |
|        - |  2252 | ` */` |
|       38 |  2253 | `static sxi32 VmThrowErrorAp(` |
|        - |  2254 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2255 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2256 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2257 | `	const char *zFormat, /* Format message */` |
|        - |  2258 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2259 | `	)` |
|        2 |  2260 |  |
|       40 |  2261 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2262 | `	SyBlob sMsg;` |
|        - |  2263 | `	SyString *pFile;` |
|        - |  2264 | `	char *zErr;` |
|       40 |  2265 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2266 | `	if( !pVm->bErrReport ){` |
|        - |  2267 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2268 | `		return SXRET_OK;` |
|        - |  2269 | `	}` |
|        - |  2270 | `	/* Reset the working buffer */` |
|       40 |  2271 | `	SyBlobReset(pWorker);` |
|        - |  2272 | `	/* Peek the processed file if available */` |
|       40 |  2273 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2274 | `	if( pFile ){` |
|        - |  2275 | `		/* Append file name */` |
|       40 |  2276 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2277 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2278 | `	}` |
|        - |  2279 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2280 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2281 | `	 * the correct errno value. */` |
|       40 |  2282 | `	zErr = "Error:  ";` |
|       40 |  2283 | `	switch(iErr){` |
|        4 |  2284 | `	case PH7_CTX_WARNING:` |
|        9 |  2285 | `		zErr = "Warning:  ";` |
|        9 |  2286 | `		break;` |
|        3 |  2287 | `	case PH7_CTX_NOTICE:` |
|        7 |  2288 | `		zErr = "Notice:  ";` |
|        6 |  2289 | `		break;` |
|       12 |  2290 | `	default:` |
|        - |  2291 | `		/* do not change iErr */` |
|       24 |  2292 | `		break;` |
|        - |  2293 | `	}` |
|       40 |  2294 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2295 | `	if( pFuncName ){` |
|        - |  2296 | `		/* Append function name first */` |
|       26 |  2297 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2298 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2299 | `	}` |
|        - |  2300 | `	/* Format the raw message */` |
|       40 |  2301 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2302 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2303 | `	/* Check if a user error handler is installed */` |
|       40 |  2304 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2305 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2306 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2307 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2308 | `	}` |
|       40 |  2309 | `	SyBlobRelease(&sMsg);` |
|       40 |  2310 | `	return rc;` |
|       21 |  2311 |  |
|        - |  2312 | `/*` |
|        - |  2313 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2314 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2315 | ` * information.` |
|        - |  2316 | ` * ------------------------------------` |
|        - |  2317 | ` * Simple boring wrapper function.` |
|        - |  2318 | ` * ------------------------------------` |
|        - |  2319 | ` */` |
|       14 |  2320 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2321 |  |
|        - |  2322 | `	va_list ap;` |
|        - |  2323 | `	sxi32 rc;` |
|       15 |  2324 | `	va_start(ap,zFormat);` |
|       15 |  2325 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2326 | `	va_end(ap);` |
|       15 |  2327 | `	return rc;` |
|        1 |  2328 |  |
|        - |  2329 | `/*` |
|        - |  2330 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2331 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2332 | ` * information.` |
|        - |  2333 | ` * ------------------------------------` |
|        - |  2334 | ` * Simple boring wrapper function.` |
|        - |  2335 | ` * ------------------------------------` |
|        - |  2336 | ` */` |
|       24 |  2337 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2338 |  |
|        - |  2339 | `	sxi32 rc;` |
|       26 |  2340 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2341 | `	return rc;` |
|        2 |  2342 |  |
|        - |  2343 | `/*` |
|        - |  2344 | ` * Resolve function context from the current frame.` |
|        - |  2345 | ` */` |
|      934 |  2346 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2347 |  |
|        - |  2348 | `	VmFrame *pFrame;` |
|        - |  2349 | `	ph7_vm_func *pFunc;` |
|      935 |  2350 | `	*pzFuncName = 0;` |
|      935 |  2351 | `	*pnFuncLen = 0;` |
|      935 |  2352 | `	pFrame = pVm->pFrame;` |
|      935 |  2353 | `	if( pFrame == 0 ){` |
|      ! 0 |  2354 | `		return;` |
|        - |  2355 | `	}` |
|      935 |  2356 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  2357 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  2358 | `	}` |
|      935 |  2359 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2360 | `		return;` |
|        - |  2361 | `	}` |
|        7 |  2362 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2363 | `	if( pFunc == 0 ){` |
|      ! 0 |  2364 | `		return;` |
|        - |  2365 | `	}` |
|        7 |  2366 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2367 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2368 |  |
|        - |  2369 | `/*` |
|        - |  2370 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2371 | ` */` |
|      470 |  2372 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2373 |  |
|        - |  2374 | `	SyBlob sOut;` |
|        - |  2375 | `	SyString *pFile;` |
|      471 |  2376 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2377 | `		return PH7_OK;` |
|        - |  2378 | `	}` |
|      471 |  2379 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2380 | `		zClass = "Exception";` |
|      ! 0 |  2381 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2382 | `	}` |
|      471 |  2383 | `	if( zMsg == 0 ){` |
|      ! 0 |  2384 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2385 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2386 | `	}` |
|      471 |  2387 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2388 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2389 | `	}` |
|      471 |  2390 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2391 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2392 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2393 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2394 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2395 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2396 | `	if( pFile ){` |
|      471 |  2397 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2398 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2399 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2400 | `	}` |
|      471 |  2401 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2402 | `	if( pFile ){` |
|      471 |  2403 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2404 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2405 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2406 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2407 | `		}else{` |
|      465 |  2408 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2409 | `		}` |
|      235 |  2410 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2411 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2412 | `	}else{` |
|      ! 0 |  2413 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2414 | `	}` |
|      471 |  2415 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2416 | `	if( pFile ){` |
|      471 |  2417 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2418 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2419 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2420 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2421 | `	}` |
|      471 |  2422 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2423 | `	SyBlobRelease(&sOut);` |
|      471 |  2424 | `	return PH7_ABORT;` |
|      236 |  2425 |  |
|        - |  2426 | `/*` |
|        - |  2427 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2428 | ` */` |
|      468 |  2429 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2430 |  |
|        - |  2431 | `	ph7_vm *pVm;` |
|        - |  2432 | `	ph7_class *pClass;` |
|        - |  2433 | `	ph7_class_instance *pThis;` |
|        - |  2434 | `	ph7_class_method *pCons;` |
|        - |  2435 | `	ph7_value sArg;` |
|        - |  2436 | `	ph7_value *apArg[1];` |
|        - |  2437 | `	SyBlob sMsg;` |
|        - |  2438 | `	SyString sMsgStr;` |
|        - |  2439 | `	VmFrame *pFrame;` |
|        - |  2440 | `	va_list ap;` |
|        - |  2441 | `	sxi32 rc;` |
|        - |  2442 |  |
|      470 |  2443 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2444 | `		return PH7_ABORT;` |
|        - |  2445 | `	}` |
|      470 |  2446 | `	pVm = pCtx->pVm;` |
|      470 |  2447 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2448 | `		zClass = "Error";` |
|      ! 0 |  2449 | `	}` |
|      470 |  2450 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      470 |  2451 | `	if( pClass == 0 ){` |
|      ! 0 |  2452 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2453 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2454 | `			zClass` |
|        - |  2455 | `			);` |
|        - |  2456 | `	}` |
|      470 |  2457 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      470 |  2458 | `	if( pThis == 0 ){` |
|      ! 0 |  2459 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2460 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2461 | `			);` |
|        - |  2462 | `	}` |
|        - |  2463 |  |
|      470 |  2464 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      470 |  2465 | `	va_start(ap,zFormat);` |
|      470 |  2466 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      470 |  2467 | `	va_end(ap);` |
|        - |  2468 |  |
|      470 |  2469 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      470 |  2470 | `	if( pCons ){` |
|      470 |  2471 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      470 |  2472 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      470 |  2473 | `		apArg[0] = &sArg;` |
|      470 |  2474 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      470 |  2475 | `		PH7_MemObjRelease(&sArg);` |
|      234 |  2476 | `	}` |
|      470 |  2477 | `	SyBlobRelease(&sMsg);` |
|        - |  2478 |  |
|      470 |  2479 | `	pFrame = pVm->pFrame;` |
|      470 |  2480 | `	if( pFrame ){` |
|      476 |  2481 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  2482 | `			pFrame = pFrame->pParent;` |
|        1 |  2483 | `		}` |
|      470 |  2484 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      234 |  2485 | `	}` |
|      470 |  2486 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      470 |  2487 | `	PH7_ClassInstanceUnref(pThis);` |
|      470 |  2488 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2489 | `		return PH7_ABORT;` |
|        - |  2490 | `	}` |
|        7 |  2491 | `	return PH7_EXCEPTION;` |
|      236 |  2492 |  |
|        - |  2493 | `/*` |
|        - |  2494 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2495 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2496 | ` */` |
|      ! 0 |  2497 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2498 |  |
|        - |  2499 | `	ph7_vm *pVm;` |
|        - |  2500 | `	SyBlob sMsg;` |
|      ! 0 |  2501 | `	const char *zFuncName = 0;` |
|      ! 0 |  2502 | `	int nFuncLen = 0;` |
|        - |  2503 | `	va_list ap;` |
|        - |  2504 | `	sxi32 rc;` |
|        - |  2505 |  |
|      ! 0 |  2506 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2507 | `		return PH7_OK;` |
|        - |  2508 | `	}` |
|      ! 0 |  2509 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2510 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2511 | `		zClass = "Error";` |
|      ! 0 |  2512 | `	}` |
|        - |  2513 |  |
|      ! 0 |  2514 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2515 |  |
|      ! 0 |  2516 | `	va_start(ap,zFormat);` |
|      ! 0 |  2517 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2518 | `	va_end(ap);` |
|        - |  2519 |  |
|      ! 0 |  2520 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2521 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2522 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2523 | `	}` |
|      ! 0 |  2524 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2525 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2526 | `	}` |
|      ! 0 |  2527 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2528 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2529 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2530 | `	return rc;` |
|      ! 0 |  2531 |  |
|        - |  2532 | `/*` |
|        - |  2533 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2534 | ` *` |
|        - |  2535 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2536 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2537 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2538 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2539 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2540 | ` * then the program execution is halted.` |
|        - |  2541 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2542 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2543 | ` * or to reset the VM to it's initial state.` |
|        - |  2544 | ` */` |
|    29464 |  2545 | `static sxi32 VmByteCodeExec(` |
|        - |  2546 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2547 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2548 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2549 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2550 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2551 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2552 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2553 | `	)` |
|        2 |  2554 |  |
|        - |  2555 | `	VmInstr *pInstr;` |
|        - |  2556 | `	ph7_value *pTos;` |
|        - |  2557 | `	SySet aArg;` |
|        - |  2558 | `	sxi32 pc;` |
|        - |  2559 | `	sxi32 rc;` |
|        - |  2560 | `	/* Argument container */` |
|    29466 |  2561 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    29466 |  2562 | `	if( nTos < 0 ){` |
|    27874 |  2563 | `		pTos = &pStack[-1];` |
|    13938 |  2564 | `	}else{` |
|     1594 |  2565 | `		pTos = &pStack[nTos];` |
|        - |  2566 | `	}` |
|    29466 |  2567 | `	pc = 0;` |
|        - |  2568 | `	/* Execute as much as we can */` |
|  5095910 |  2569 | `	for(;;){` |
|        - |  2570 | `		/* Fetch the instruction to execute */` |
| 10191118 |  2571 | `		pInstr = &aInstr[pc];` |
| 10191118 |  2572 | `		rc = SXRET_OK;` |
|        - |  2573 | `/*` |
|        - |  2574 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2575 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2576 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2577 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2578 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2579 | ` */` |
| 10191118 |  2580 | `		switch(pInstr->iOp){` |
|        - |  2581 | `/*` |
|        - |  2582 | ` * DONE: P1 * *` |
|        - |  2583 | ` *` |
|        - |  2584 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2585 | ` * and return immediately.` |
|        - |  2586 | ` */` |
|    14489 |  2587 | `case PH7_OP_DONE:` |
|    28980 |  2588 | `	if( pInstr->iP1 ){` |
|        - |  2589 | `#ifdef UNTRUST` |
|        - |  2590 | `		if( pTos < pStack ){` |
|        - |  2591 | `			goto Abort;` |
|        - |  2592 | `		}` |
|        - |  2593 | `#endif` |
|    16734 |  2594 | `		if( pLastRef ){` |
|    10782 |  2595 | `			*pLastRef = pTos->nIdx;` |
|     5390 |  2596 | `		}` |
|    16734 |  2597 | `		if( pResult ){` |
|        - |  2598 | `			/* Execution result */` |
|    15942 |  2599 | `			PH7_MemObjStore(pTos,pResult);` |
|     7970 |  2600 | `		}` |
|    16734 |  2601 | `		VmPopOperand(&pTos,1);` |
|    20614 |  2602 | `	}else if( pLastRef ){` |
|        - |  2603 | `		/* Nothing referenced */` |
|      878 |  2604 | `		*pLastRef = SXU32_HIGH;` |
|      438 |  2605 | `	}` |
|    28980 |  2606 | `	goto Done;` |
|        - |  2607 | `/*` |
|        - |  2608 | ` * HALT: P1 * *` |
|        - |  2609 | ` *` |
|        - |  2610 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2611 | ` * and abort immediately.` |
|        - |  2612 | ` */` |
|        4 |  2613 | `case PH7_OP_HALT:` |
|        9 |  2614 | `	if( pInstr->iP1 ){` |
|        - |  2615 | `#ifdef UNTRUST` |
|        - |  2616 | `		if( pTos < pStack ){` |
|        - |  2617 | `			goto Abort;` |
|        - |  2618 | `		}` |
|        - |  2619 | `#endif` |
|        9 |  2620 | `		if( pLastRef ){` |
|      ! 0 |  2621 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2622 | `		}` |
|        9 |  2623 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2624 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2625 | `				/* Output the exit message */` |
|        7 |  2626 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2627 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2628 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2629 | `					/* Increment output length */` |
|        5 |  2630 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2631 | `				}` |
|        3 |  2632 | `			}` |
|        7 |  2633 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2634 | `			/* Record exit status */` |
|        5 |  2635 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2636 | `		}` |
|        9 |  2637 | `		VmPopOperand(&pTos,1);` |
|        4 |  2638 | `	}else if( pLastRef ){` |
|        - |  2639 | `		/* Nothing referenced */` |
|      ! 0 |  2640 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2641 | `	}` |
|        - |  2642 | `	/* Check if we're in an included file context */` |
|        9 |  2643 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2644 | `		/* Terminate the entire process */` |
|        9 |  2645 | `		exit(pVm->iExitStatus);` |
|        - |  2646 | `	}` |
|      ! 0 |  2647 | `	goto Abort;` |
|        - |  2648 | `/*` |
|        - |  2649 | ` * JMP: * P2 *` |
|        - |  2650 | ` *` |
|        - |  2651 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2652 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2653 | ` */` |
|   223070 |  2654 | `case PH7_OP_JMP:` |
|   446186 |  2655 | `	pc = pInstr->iP2 - 1;` |
|   446186 |  2656 | `	break;` |
|        - |  2657 | `/*` |
|        - |  2658 | ` * JZ: P1 P2 *` |
|        - |  2659 | ` *` |
|        - |  2660 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2661 | ` * entry in the stack if P1 is zero.` |
|        - |  2662 | ` */` |
|   518406 |  2663 | `case PH7_OP_JZ:` |
|        - |  2664 | `#ifdef UNTRUST` |
|        - |  2665 | `	if( pTos < pStack ){` |
|        - |  2666 | `		goto Abort;` |
|        - |  2667 | `	}` |
|        - |  2668 | `#endif` |
|        - |  2669 | `	/* Get a boolean value */` |
|  1036902 |  2670 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       77 |  2671 | `		PH7_MemObjToBool(pTos);` |
|       38 |  2672 | `	}` |
|  1036902 |  2673 | `	if( !pTos->x.iVal ){` |
|        - |  2674 | `		/* Take the jump */` |
|   497930 |  2675 | `		pc = pInstr->iP2 - 1;` |
|   248964 |  2676 | `	}` |
|  1036902 |  2677 | `	if( !pInstr->iP1 ){` |
|   818258 |  2678 | `		VmPopOperand(&pTos,1);` |
|   409150 |  2679 | `	}` |
|  1036902 |  2680 | `	break;` |
|        - |  2681 | `/*` |
|        - |  2682 | ` * JNZ: P1 P2 *` |
|        - |  2683 | ` *` |
|        - |  2684 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2685 | ` * entry in the stack if P1 is zero.` |
|        - |  2686 | ` */` |
|    57520 |  2687 | `case PH7_OP_JNZ:` |
|        - |  2688 | `#ifdef UNTRUST` |
|        - |  2689 | `	if( pTos < pStack ){` |
|        - |  2690 | `		goto Abort;` |
|        - |  2691 | `	}` |
|        - |  2692 | `#endif` |
|        - |  2693 | `	/* Get a boolean value */` |
|   115042 |  2694 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2695 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2696 | `	}` |
|   115042 |  2697 | `	if( pTos->x.iVal ){` |
|        - |  2698 | `		/* Take the jump */` |
|     4164 |  2699 | `		pc = pInstr->iP2 - 1;` |
|     2081 |  2700 | `	}` |
|   115042 |  2701 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2702 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2703 | `	}` |
|   115042 |  2704 | `	break;` |
|        - |  2705 | `/*` |
|        - |  2706 | ` * NOOP: * * *` |
|        - |  2707 | ` *` |
|        - |  2708 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2709 | ` * destination.` |
|        - |  2710 | ` */` |
|      ! 0 |  2711 | `case PH7_OP_NOOP:` |
|      ! 0 |  2712 | `	break;` |
|        - |  2713 | `/*` |
|        - |  2714 | ` * POP: P1 * *` |
|        - |  2715 | ` *` |
|        - |  2716 | ` * Pop P1 elements from the operand stack.` |
|        - |  2717 | ` */` |
|   395630 |  2718 | `case PH7_OP_POP: {` |
|   791306 |  2719 | `	sxi32 n = pInstr->iP1;` |
|   791306 |  2720 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2721 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2722 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2723 | `	}` |
|   791306 |  2724 | `	VmPopOperand(&pTos,n);` |
|   791306 |  2725 | `	break;` |
|        - |  2726 | `				 }` |
|        - |  2727 | `/*` |
|        - |  2728 | ` * CVT_INT: * * *` |
|        - |  2729 | ` *` |
|        - |  2730 | ` * Force the top of the stack to be an integer.` |
|        - |  2731 | ` */` |
|       35 |  2732 | `case PH7_OP_CVT_INT:` |
|        - |  2733 | `#ifdef UNTRUST` |
|        - |  2734 | `	if( pTos < pStack ){` |
|        - |  2735 | `		goto Abort;` |
|        - |  2736 | `	}` |
|        - |  2737 | `#endif` |
|       72 |  2738 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2739 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2740 | `	}` |
|        - |  2741 | `	/* Invalidate any prior representation */` |
|       72 |  2742 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2743 | `	break;` |
|        - |  2744 | `/*` |
|        - |  2745 | ` * CVT_REAL: * * *` |
|        - |  2746 | ` *` |
|        - |  2747 | ` * Force the top of the stack to be a real.` |
|        - |  2748 | ` */` |
|        4 |  2749 | `case PH7_OP_CVT_REAL:` |
|        - |  2750 | `#ifdef UNTRUST` |
|        - |  2751 | `	if( pTos < pStack ){` |
|        - |  2752 | `		goto Abort;` |
|        - |  2753 | `	}` |
|        - |  2754 | `#endif` |
|        9 |  2755 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2756 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2757 | `	}` |
|        - |  2758 | `	/* Invalidate any prior representation */` |
|        9 |  2759 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2760 | `	break;` |
|        - |  2761 | `/*` |
|        - |  2762 | ` * CVT_STR: * * *` |
|        - |  2763 | ` *` |
|        - |  2764 | ` * Force the top of the stack to be a string.` |
|        - |  2765 | ` */` |
|      146 |  2766 | `case PH7_OP_CVT_STR:` |
|        - |  2767 | `#ifdef UNTRUST` |
|        - |  2768 | `	if( pTos < pStack ){` |
|        - |  2769 | `		goto Abort;` |
|        - |  2770 | `	}` |
|        - |  2771 | `#endif` |
|      294 |  2772 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2773 | `		PH7_MemObjToString(pTos);` |
|      146 |  2774 | `	}` |
|      294 |  2775 | `	break;` |
|        - |  2776 | `/*` |
|        - |  2777 | ` * CVT_BOOL: * * *` |
|        - |  2778 | ` *` |
|        - |  2779 | ` * Force the top of the stack to be a boolean.` |
|        - |  2780 | ` */` |
|        5 |  2781 | `case PH7_OP_CVT_BOOL:` |
|        - |  2782 | `#ifdef UNTRUST` |
|        - |  2783 | `	if( pTos < pStack ){` |
|        - |  2784 | `		goto Abort;` |
|        - |  2785 | `	}` |
|        - |  2786 | `#endif` |
|       11 |  2787 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2788 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2789 | `	}` |
|       11 |  2790 | `	break;` |
|        - |  2791 | `/*` |
|        - |  2792 | ` * CVT_NULL: * * *` |
|        - |  2793 | ` *` |
|        - |  2794 | ` * Nullify the top of the stack.` |
|        - |  2795 | ` */` |
|        3 |  2796 | `case PH7_OP_CVT_NULL:` |
|        - |  2797 | `#ifdef UNTRUST` |
|        - |  2798 | `	if( pTos < pStack ){` |
|        - |  2799 | `		goto Abort;` |
|        - |  2800 | `	}` |
|        - |  2801 | `#endif` |
|        7 |  2802 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2803 | `	break;` |
|        - |  2804 | `/*` |
|        - |  2805 | ` * CVT_NUMC: * * *` |
|        - |  2806 | ` *` |
|        - |  2807 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2808 | ` */` |
|      ! 0 |  2809 | `case PH7_OP_CVT_NUMC:` |
|        - |  2810 | `#ifdef UNTRUST` |
|        - |  2811 | `	if( pTos < pStack ){` |
|        - |  2812 | `		goto Abort;` |
|        - |  2813 | `	}` |
|        - |  2814 | `#endif` |
|        - |  2815 | `	/* Force a numeric cast */` |
|      ! 0 |  2816 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2817 | `	break;` |
|        - |  2818 | `/*` |
|        - |  2819 | ` * CVT_ARRAY: * * *` |
|        - |  2820 | ` *` |
|        - |  2821 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2822 | ` */` |
|       10 |  2823 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2824 | `#ifdef UNTRUST` |
|        - |  2825 | `	if( pTos < pStack ){` |
|        - |  2826 | `		goto Abort;` |
|        - |  2827 | `	}` |
|        - |  2828 | `#endif` |
|        - |  2829 | `	/* Force a hashmap cast */` |
|       21 |  2830 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2831 | `	if( rc != SXRET_OK ){` |
|        - |  2832 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2833 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2834 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2835 | `	}` |
|       21 |  2836 | `	break;` |
|        - |  2837 | `/*` |
|        - |  2838 | ` * CVT_OBJ: * * *` |
|        - |  2839 | ` *` |
|        - |  2840 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2841 | ` */` |
|        8 |  2842 | `case PH7_OP_CVT_OBJ:` |
|        - |  2843 | `#ifdef UNTRUST` |
|        - |  2844 | `	if( pTos < pStack ){` |
|        - |  2845 | `		goto Abort;` |
|        - |  2846 | `	}` |
|        - |  2847 | `#endif` |
|       17 |  2848 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2849 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2850 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2851 | `	}` |
|       17 |  2852 | `	break;` |
|        - |  2853 | `/*` |
|        - |  2854 | ` * ERR_CTRL * * *` |
|        - |  2855 | ` *` |
|        - |  2856 | ` * Error control operator.` |
|        - |  2857 | ` */` |
|    11982 |  2858 | `case PH7_OP_ERR_CTRL:` |
|        - |  2859 | `	/*` |
|        - |  2860 | `	 * TICKET 1433-038:` |
|        - |  2861 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2862 | `	 * use the public API,to control error output.` |
|        - |  2863 | `	 */` |
|    23964 |  2864 | `	break;` |
|        - |  2865 | `/*` |
|        - |  2866 | ` * IS_A * * *` |
|        - |  2867 | ` *` |
|        - |  2868 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2869 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2870 | ` * holding a class name or an object).` |
|        - |  2871 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2872 | ` */` |
|       11 |  2873 | `case PH7_OP_IS_A:{` |
|       23 |  2874 | `	ph7_value *pNos = &pTos[-1];` |
|       23 |  2875 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2876 | `#ifdef UNTRUST` |
|        - |  2877 | `	if( pNos < pStack ){` |
|        - |  2878 | `		goto Abort;` |
|        - |  2879 | `	}` |
|        - |  2880 | `#endif` |
|       23 |  2881 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       21 |  2882 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       21 |  2883 | `		ph7_class *pClass = 0;` |
|        - |  2884 | `		/* Extract the target class */` |
|       21 |  2885 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2886 | `			/* Instance already loaded */` |
|      ! 0 |  2887 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       21 |  2888 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2889 | `			/* Perform the query */` |
|       31 |  2890 | `			pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|       20 |  2891 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       10 |  2892 | `		}` |
|       21 |  2893 | `		if( pClass ){` |
|        - |  2894 | `			/* Perform the query */` |
|       21 |  2895 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       10 |  2896 | `		}` |
|       10 |  2897 | `	}` |
|        - |  2898 | `	/* Push result */` |
|       23 |  2899 | `	VmPopOperand(&pTos,1);` |
|       23 |  2900 | `	PH7_MemObjRelease(pTos);` |
|       23 |  2901 | `	pTos->x.iVal = iRes;` |
|       23 |  2902 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       23 |  2903 | `	break;` |
|        - |  2904 | `				 }` |
|        - |  2905 |  |
|        - |  2906 | `/*` |
|        - |  2907 | ` * LOADC P1 P2 *` |
|        - |  2908 | ` *` |
|        - |  2909 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2910 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2911 | ` */` |
|   797312 |  2912 | `case PH7_OP_LOADC: {` |
|        - |  2913 | `	ph7_value *pObj;` |
|        - |  2914 | `	/* Reserve a room */` |
|  1594670 |  2915 | `	pTos++;` |
|  1594670 |  2916 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1594670 |  2917 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2918 | `			SyHashEntry *pEntry;` |
|        - |  2919 | `			/* Candidate for expansion via user defined callbacks */` |
|    19088 |  2920 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19088 |  2921 | `			if( pEntry ){` |
|    15302 |  2922 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  2923 | `				/* Set a NULL default value */` |
|    15302 |  2924 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15302 |  2925 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  2926 | `				/* Invoke the callback and deal with the expanded value */` |
|    15302 |  2927 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  2928 | `				/* Mark as constant */` |
|    15302 |  2929 | `				pTos->nIdx = SXU32_HIGH;` |
|    15302 |  2930 | `				break;` |
|        - |  2931 | `			}` |
|     1893 |  2932 | `		}` |
|  1579370 |  2933 | `		PH7_MemObjLoad(pObj,pTos);` |
|   789708 |  2934 | `	}else{` |
|        - |  2935 | `		/* Set a NULL value */` |
|      ! 0 |  2936 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2937 | `	}` |
|        - |  2938 | `	/* Mark as constant */` |
|  1579370 |  2939 | `	pTos->nIdx = SXU32_HIGH;` |
|  1579370 |  2940 | `	break;` |
|        - |  2941 | `				  }` |
|        - |  2942 | `/*` |
|        - |  2943 | ` * LOAD: P1 * P3` |
|        - |  2944 | ` *` |
|        - |  2945 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  2946 | ` * from the P3 operand.` |
|        - |  2947 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  2948 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  2949 | ` */` |
|  1424864 |  2950 | `case PH7_OP_LOAD:{` |
|        - |  2951 | `	ph7_value *pObj;` |
|        - |  2952 | `	SyString sName;` |
|  2849950 |  2953 | `	if( pInstr->p3 == 0 ){` |
|        - |  2954 | `		/* Take the variable name from the top of the stack */` |
|        - |  2955 | `#ifdef UNTRUST` |
|        - |  2956 | `		if( pTos < pStack ){` |
|        - |  2957 | `			goto Abort;` |
|        - |  2958 | `		}` |
|        - |  2959 | `#endif` |
|        - |  2960 | `		/* Force a string cast */` |
|       19 |  2961 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2962 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  2963 | `		}` |
|       19 |  2964 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  2965 | `	}else{` |
|  2849932 |  2966 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  2967 | `		/* Reserve a room for the target object */` |
|  2849932 |  2968 | `		pTos++;` |
|        - |  2969 | `	}` |
|        - |  2970 | `	/* Extract the requested memory object */` |
|  2849950 |  2971 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2849950 |  2972 | `	if( pObj == 0 ){` |
|      626 |  2973 | `		if( pInstr->iP1 ){` |
|        - |  2974 | `			/* Variable not found,load NULL */` |
|      626 |  2975 | `			if( !pInstr->p3 ){` |
|      ! 0 |  2976 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  2977 | `			}else{` |
|      626 |  2978 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  2979 | `			}` |
|      626 |  2980 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1425178 |  2981 | `			break;` |
|      ! 0 |  2982 | `		}else{` |
|        - |  2983 | `			/* Fatal error */` |
|      ! 0 |  2984 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  2985 | `			goto Abort;` |
|        - |  2986 | `		}` |
|        - |  2987 | `	}` |
|        - |  2988 | `	/* Load variable contents */` |
|  2849326 |  2989 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2849326 |  2990 | `	pTos->nIdx = pObj->nIdx;` |
|  2849326 |  2991 | `	break;` |
|        - |  2992 | `				   }` |
|        - |  2993 | `/*` |
|        - |  2994 | ` * LOAD_MAP P1 * *` |
|        - |  2995 | ` *` |
|        - |  2996 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  2997 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  2998 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  2999 | ` */` |
|    17299 |  3000 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3001 | `	ph7_hashmap *pMap;` |
|        - |  3002 | `	/* Allocate a new hashmap instance */` |
|    34600 |  3003 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    34600 |  3004 | `	if( pMap == 0 ){` |
|      ! 0 |  3005 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3006 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3007 | `		goto Abort;` |
|        - |  3008 | `	}` |
|    34600 |  3009 | `	if( pInstr->iP1 > 0 ){` |
|     2080 |  3010 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3011 | `		/* Perform the insertion */` |
|     6302 |  3012 | `		while( pEntry < pTos ){` |
|     4224 |  3013 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3014 | `				/* Insertion by reference */` |
|      142 |  3015 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3016 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3017 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3018 | `					);` |
|       48 |  3019 | `			}else{` |
|        - |  3020 | `				/* Standard insertion */` |
|     6194 |  3021 | `				PH7_HashmapInsert(pMap,` |
|     4128 |  3022 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2064 |  3023 | `					&pEntry[1]` |
|        - |  3024 | `				);` |
|        - |  3025 | `			}` |
|        - |  3026 | `			/* Next pair on the stack */` |
|     4224 |  3027 | `			pEntry += 2;` |
|        2 |  3028 | `		}` |
|        - |  3029 | `		/* Pop P1 elements */` |
|     2080 |  3030 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1039 |  3031 | `	}` |
|        - |  3032 | `	/* Push the hashmap */` |
|    34600 |  3033 | `	pTos++;` |
|    34600 |  3034 | `	pTos->nIdx = SXU32_HIGH;` |
|    34600 |  3035 | `	pTos->x.pOther = pMap;` |
|    34600 |  3036 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    34600 |  3037 | `	break;` |
|        - |  3038 | `					  }` |
|        - |  3039 | `/*` |
|        - |  3040 | ` * LOAD_LIST: P1 * *` |
|        - |  3041 | ` *` |
|        - |  3042 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3043 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3044 | ` * Caveats:` |
|        - |  3045 | ` *  This implementation support only a single nesting level.` |
|        - |  3046 | ` */` |
|       17 |  3047 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3048 | `	ph7_value *pEntry;` |
|       35 |  3049 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3050 | `		/* Empty list,break immediately */` |
|      ! 0 |  3051 | `		break;` |
|        - |  3052 | `	}` |
|       35 |  3053 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3054 | `#ifdef UNTRUST` |
|        - |  3055 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3056 | `		goto Abort;` |
|        - |  3057 | `	}` |
|        - |  3058 | `#endif` |
|       35 |  3059 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       31 |  3060 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3061 | `		ph7_hashmap_node *pNode;` |
|        - |  3062 | `		ph7_value sKey,*pObj;` |
|        - |  3063 | `		/* Start Copying */` |
|       31 |  3064 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|       99 |  3065 | `		while( pEntry <= pTos ){` |
|       69 |  3066 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       65 |  3067 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       65 |  3068 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       65 |  3069 | `					if( rc == SXRET_OK ){` |
|        - |  3070 | `						/* Store node value */` |
|       65 |  3071 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       33 |  3072 | `					}else{` |
|        - |  3073 | `						/* Nullify the variable */` |
|      ! 0 |  3074 | `						PH7_MemObjRelease(pObj);` |
|        - |  3075 | `					}` |
|       32 |  3076 | `				}` |
|       32 |  3077 | `			}` |
|       69 |  3078 | `			sKey.x.iVal++; /* Next numeric index */` |
|       69 |  3079 | `			pEntry++;` |
|        1 |  3080 | `		}` |
|       15 |  3081 | `	}` |
|       35 |  3082 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       35 |  3083 | `	break;` |
|        - |  3084 | `					   }` |
|        - |  3085 | `/*` |
|        - |  3086 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3087 | ` *` |
|        - |  3088 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3089 | ` * from the stack.` |
|        - |  3090 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3091 | ` * instead.` |
|        - |  3092 | ` */` |
|   239377 |  3093 | `case PH7_OP_LOAD_IDX: {` |
|   478800 |  3094 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   478800 |  3095 | `	ph7_hashmap *pMap = 0;` |
|        - |  3096 | `	ph7_value *pIdx;` |
|   478800 |  3097 | `	pIdx = 0;` |
|   478800 |  3098 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3099 | `		if( !pInstr->iP2){` |
|        - |  3100 | `			/* No available index,load NULL */` |
|      ! 0 |  3101 | `			if( pTos >= pStack ){` |
|      ! 0 |  3102 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3103 | `			}else{` |
|        - |  3104 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3105 | `				pTos++;` |
|      ! 0 |  3106 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3107 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3108 | `			}` |
|        - |  3109 | `			/* Emit a notice */` |
|      ! 0 |  3110 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3111 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3112 | `			break;` |
|        - |  3113 | `		}` |
|      ! 0 |  3114 | `	}else{` |
|   478800 |  3115 | `		pIdx = pTos;` |
|   478800 |  3116 | `		pTos--;` |
|        - |  3117 | `	}` |
|   478800 |  3118 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3119 | `		/* String access */` |
|   393100 |  3120 | `		if( pIdx ){` |
|        - |  3121 | `			sxu32 nOfft;` |
|   393100 |  3122 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3123 | `				/* Force an int cast */` |
|      ! 0 |  3124 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3125 | `			}` |
|   393100 |  3126 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   393100 |  3127 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3128 | `				/* Invalid offset,load null */` |
|      ! 0 |  3129 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3130 | `			}else{` |
|   393100 |  3131 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   393100 |  3132 | `				int c = zData[nOfft];` |
|   393100 |  3133 | `				PH7_MemObjRelease(pTos);` |
|   393100 |  3134 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   393100 |  3135 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3136 | `			}` |
|   196573 |  3137 | `		}else{` |
|        - |  3138 | `			/* No available index,load NULL */` |
|      ! 0 |  3139 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3140 | `		}` |
|   393100 |  3141 | `		break;` |
|        - |  3142 | `	}` |
|    85702 |  3143 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3144 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3145 | `			ph7_value *pObj;` |
|      ! 0 |  3146 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3147 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3148 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3149 | `			}` |
|      ! 0 |  3150 | `		}` |
|      ! 0 |  3151 | `	}` |
|    85702 |  3152 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    85702 |  3153 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3154 | `		/* Point to the hashmap */` |
|    85702 |  3155 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    85702 |  3156 | `		if( pIdx ){` |
|        - |  3157 | `			/* Load the desired entry */` |
|    85702 |  3158 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    42850 |  3159 | `		}` |
|    85702 |  3160 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3161 | `			/* Create a new empty entry */` |
|      ! 0 |  3162 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3163 | `			if( rc == SXRET_OK ){` |
|        - |  3164 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3165 | `				pNode = pMap->pLast;` |
|      ! 0 |  3166 | `			}` |
|      ! 0 |  3167 | `		}` |
|    42850 |  3168 | `	}` |
|    85702 |  3169 | `	if( pIdx ){` |
|    85702 |  3170 | `		PH7_MemObjRelease(pIdx);` |
|    42850 |  3171 | `	}` |
|    85702 |  3172 | `	if( rc == SXRET_OK ){` |
|        - |  3173 | `		/* Load entry contents */` |
|    39270 |  3174 | `		if( pMap->iRef < 2 ){` |
|        - |  3175 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3176 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3177 | `			 */` |
|        7 |  3178 | `			pTos->nIdx = SXU32_HIGH;` |
|        7 |  3179 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|        4 |  3180 | `		}else{` |
|    39264 |  3181 | `			pTos->nIdx = pNode->nValIdx;` |
|    39264 |  3182 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    39264 |  3183 | `			PH7_HashmapUnref(pMap);` |
|        - |  3184 | `		}` |
|    19636 |  3185 | `	}else{` |
|        - |  3186 | `		/* No such entry,load NULL */` |
|    46434 |  3187 | `		PH7_MemObjRelease(pTos);` |
|    46434 |  3188 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3189 | `	}` |
|    85702 |  3190 | `	break;` |
|        - |  3191 | `					  }` |
|        - |  3192 | `/*` |
|        - |  3193 | ` * LOAD_CLOSURE * * P3` |
|        - |  3194 | ` *` |
|        - |  3195 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3196 | ` * name in the stack.` |
|        - |  3197 | ` */` |
|        2 |  3198 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3199 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3200 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3201 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3202 | `		ph7_vm_func *pClosure;` |
|        - |  3203 | `		char *zName;` |
|        - |  3204 | `		sxu32 mLen;` |
|        - |  3205 | `		sxu32 n;` |
|        - |  3206 | `		/* Create a new VM function */` |
|        5 |  3207 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3208 | `		/* Generate an unique closure name */` |
|        5 |  3209 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3210 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3211 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3212 | `			goto Abort;` |
|        - |  3213 | `		}` |
|        5 |  3214 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3215 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3216 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3217 | `		}` |
|        - |  3218 | `		/* Zero the stucture */` |
|        5 |  3219 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3220 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3221 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3222 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3223 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3224 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3225 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3226 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3227 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3228 | `		/* Register the closure */` |
|        5 |  3229 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3230 | `		/* Set up closure environment */` |
|        5 |  3231 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3232 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3233 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3234 | `			ph7_value *pValue;` |
|        9 |  3235 | `			pEnv = &aEnv[n];` |
|        9 |  3236 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3237 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3238 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3239 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3240 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3241 | `				/* Pass by reference */` |
|      ! 0 |  3242 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3243 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3244 | `					);` |
|      ! 0 |  3245 | `			}` |
|        - |  3246 | `			/* Standard pass by value */` |
|        9 |  3247 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3248 | `			if( pValue ){` |
|        - |  3249 | `				/* Copy imported value */` |
|        5 |  3250 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3251 | `			}` |
|        - |  3252 | `			/* Insert the imported variable */` |
|        9 |  3253 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3254 | `		}` |
|        - |  3255 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3256 | `		pTos++;` |
|        5 |  3257 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3258 | `	}` |
|        5 |  3259 | `	break;` |
|        - |  3260 | `						 }` |
|        - |  3261 | `/*` |
|        - |  3262 | ` * STORE * P2 P3` |
|        - |  3263 | ` *` |
|        - |  3264 | ` * Perform a store (Assignment) operation.` |
|        - |  3265 | ` */` |
|   106313 |  3266 | `case PH7_OP_STORE: {` |
|        - |  3267 | `	ph7_value *pObj;` |
|        - |  3268 | `	SyString sName;` |
|        - |  3269 | `#ifdef UNTRUST` |
|        - |  3270 | `	if( pTos < pStack ){` |
|        - |  3271 | `		goto Abort;` |
|        - |  3272 | `	}` |
|        - |  3273 | `#endif` |
|   212628 |  3274 | `	if( pInstr->iP2 ){` |
|        - |  3275 | `		sxu32 nIdx;` |
|        - |  3276 | `		/* Member store operation */` |
|     2838 |  3277 | `		nIdx = pTos->nIdx;` |
|     2838 |  3278 | `		VmPopOperand(&pTos,1);` |
|     2838 |  3279 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3280 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3281 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3282 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3283 | `		}else{` |
|        - |  3284 | `			/* Point to the desired memory object */` |
|     2834 |  3285 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2834 |  3286 | `			if( pObj ){` |
|        - |  3287 | `				/* Perform the store operation */` |
|     2834 |  3288 | `				PH7_MemObjStore(pTos,pObj);` |
|     1416 |  3289 | `			}` |
|        - |  3290 | `		}` |
|   107733 |  3291 | `		break;` |
|   209792 |  3292 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3293 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3294 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3295 | `			/* Force a string cast */` |
|      ! 0 |  3296 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3297 | `		}` |
|        7 |  3298 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3299 | `		pTos--;` |
|        - |  3300 | `#ifdef UNTRUST` |
|        - |  3301 | `		if( pTos < pStack  ){` |
|        - |  3302 | `			goto Abort;` |
|        - |  3303 | `		}` |
|        - |  3304 | `#endif` |
|        4 |  3305 | `	}else{` |
|   209786 |  3306 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3307 | `	}` |
|        - |  3308 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   209792 |  3309 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   209792 |  3310 | `	if( pObj == 0 ){` |
|      ! 0 |  3311 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3312 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3313 | `		goto Abort;` |
|        - |  3314 | `	}` |
|   209792 |  3315 | `	if( !pInstr->p3 ){` |
|        7 |  3316 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3317 | `	}` |
|        - |  3318 | `	/* Perform the store operation */` |
|   209792 |  3319 | `	PH7_MemObjStore(pTos,pObj);` |
|   209792 |  3320 | `	break;` |
|        - |  3321 | `				   }` |
|        - |  3322 | `/*` |
|        - |  3323 | ` * STORE_IDX:   P1 * P3` |
|        - |  3324 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3325 | ` *` |
|        - |  3326 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3327 | ` */` |
|    77946 |  3328 | `case PH7_OP_STORE_IDX:` |
|        - |  3329 | `case PH7_OP_STORE_IDX_REF: {` |
|   155894 |  3330 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3331 | `	ph7_value *pKey;` |
|        - |  3332 | `	sxu32 nIdx;` |
|   155894 |  3333 | `	if( pInstr->iP1 ){` |
|        - |  3334 | `		/* Key is next on stack */` |
|    56030 |  3335 | `		pKey = pTos;` |
|    56030 |  3336 | `		pTos--;` |
|    28016 |  3337 | `	}else{` |
|    99866 |  3338 | `		pKey = 0;` |
|        - |  3339 | `	}` |
|   155894 |  3340 | `	nIdx = pTos->nIdx;` |
|   155894 |  3341 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3342 | `		/* Hashmap already loaded */` |
|   155842 |  3343 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   155842 |  3344 | `		if( pMap->iRef < 2 ){` |
|        - |  3345 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3346 | `			pMap->iRef = 2;` |
|      ! 0 |  3347 | `		}` |
|    77922 |  3348 | `	}else{` |
|        - |  3349 | `		ph7_value *pObj;` |
|       53 |  3350 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3351 | `		if( pObj == 0 ){` |
|      ! 0 |  3352 | `			if( pKey ){` |
|      ! 0 |  3353 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3354 | `			}` |
|      ! 0 |  3355 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3356 | `			break;` |
|        - |  3357 | `		}` |
|        - |  3358 | `		/* Phase#1: Load the array */` |
|       53 |  3359 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3360 | `			VmPopOperand(&pTos,1);` |
|       53 |  3361 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3362 | `				/* Force a string cast */` |
|      ! 0 |  3363 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3364 | `			}` |
|       53 |  3365 | `			if( pKey == 0 ){` |
|        - |  3366 | `				/* Append string */` |
|        3 |  3367 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3368 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3369 | `				}` |
|        2 |  3370 | `			}else{` |
|        - |  3371 | `				sxu32 nOfft;` |
|       51 |  3372 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3373 | `					/* Force an int cast */` |
|       51 |  3374 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3375 | `				}` |
|       51 |  3376 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3377 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3378 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3379 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3380 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3381 | `				}else{` |
|      ! 0 |  3382 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3383 | `						/* Perform an append operation */` |
|      ! 0 |  3384 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3385 | `					}` |
|        - |  3386 | `				}` |
|        - |  3387 | `			}` |
|       53 |  3388 | `			if( pKey ){` |
|       51 |  3389 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3390 | `			}` |
|       53 |  3391 | `			break;` |
|      ! 0 |  3392 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3393 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3394 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3395 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3396 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3397 | `				goto Abort;` |
|        - |  3398 | `			}` |
|      ! 0 |  3399 | `		}` |
|      ! 0 |  3400 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3401 | `	}` |
|   155842 |  3402 | `	VmPopOperand(&pTos,1);` |
|        - |  3403 | `	/* Phase#2: Perform the insertion */` |
|   155842 |  3404 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3405 | `		/* Insertion by reference */` |
|       15 |  3406 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3407 | `	}else{` |
|   155828 |  3408 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3409 | `	}` |
|   155842 |  3410 | `	if( pKey ){` |
|    55980 |  3411 | `		PH7_MemObjRelease(pKey);` |
|    27989 |  3412 | `	}` |
|   155842 |  3413 | `	break;` |
|        - |  3414 | `					   }` |
|        - |  3415 | `/*` |
|        - |  3416 | ` * INCR: P1 * *` |
|        - |  3417 | ` *` |
|        - |  3418 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3419 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3420 | ` * the stack and increment after that.` |
|        - |  3421 | ` */` |
|   172945 |  3422 | `case PH7_OP_INCR:` |
|        - |  3423 | `#ifdef UNTRUST` |
|        - |  3424 | `	if( pTos < pStack ){` |
|        - |  3425 | `		goto Abort;` |
|        - |  3426 | `	}` |
|        - |  3427 | `#endif` |
|   345936 |  3428 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   345936 |  3429 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3430 | `			ph7_value *pObj;` |
|   345936 |  3431 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3432 | `				/* Force a numeric cast */` |
|   345936 |  3433 | `				PH7_MemObjToNumeric(pObj);` |
|   345936 |  3434 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3435 | `					pObj->rVal++;` |
|        - |  3436 | `					/* Try to get an integer representation */` |
|      ! 0 |  3437 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3438 | `				}else{` |
|   345936 |  3439 | `					pObj->x.iVal++;` |
|   345936 |  3440 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3441 | `				}` |
|   345936 |  3442 | `				if( pInstr->iP1 ){` |
|        - |  3443 | `					/* Pre-icrement */` |
|       71 |  3444 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3445 | `				}` |
|   172989 |  3446 | `			}` |
|   172991 |  3447 | `		}else{` |
|      ! 0 |  3448 | `			if( pInstr->iP1 ){` |
|        - |  3449 | `				/* Force a numeric cast */` |
|      ! 0 |  3450 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3451 | `				/* Pre-increment */` |
|      ! 0 |  3452 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3453 | `					pTos->rVal++;` |
|        - |  3454 | `					/* Try to get an integer representation */` |
|      ! 0 |  3455 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3456 | `				}else{` |
|      ! 0 |  3457 | `					pTos->x.iVal++;` |
|      ! 0 |  3458 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3459 | `				}` |
|      ! 0 |  3460 | `			}` |
|        - |  3461 | `		}` |
|   172989 |  3462 | `	}` |
|   345936 |  3463 | `	break;` |
|        - |  3464 | `/*` |
|        - |  3465 | ` * DECR: P1 * *` |
|        - |  3466 | ` *` |
|        - |  3467 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3468 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3469 | ` * and decrement after that.` |
|        - |  3470 | ` */` |
|        2 |  3471 | `case PH7_OP_DECR:` |
|        - |  3472 | `#ifdef UNTRUST` |
|        - |  3473 | `	if( pTos < pStack ){` |
|        - |  3474 | `		goto Abort;` |
|        - |  3475 | `	}` |
|        - |  3476 | `#endif` |
|        5 |  3477 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3478 | `		/* Force a numeric cast */` |
|        5 |  3479 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3480 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3481 | `			ph7_value *pObj;` |
|        5 |  3482 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3483 | `				/* Force a numeric cast */` |
|        5 |  3484 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3485 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3486 | `					pObj->rVal--;` |
|        - |  3487 | `					/* Try to get an integer representation */` |
|      ! 0 |  3488 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3489 | `				}else{` |
|        5 |  3490 | `					pObj->x.iVal--;` |
|        5 |  3491 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3492 | `				}` |
|        5 |  3493 | `				if( pInstr->iP1 ){` |
|        - |  3494 | `					/* Pre-icrement */` |
|      ! 0 |  3495 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3496 | `				}` |
|        2 |  3497 | `			}` |
|        3 |  3498 | `		}else{` |
|      ! 0 |  3499 | `			if( pInstr->iP1 ){` |
|        - |  3500 | `				/* Pre-increment */` |
|      ! 0 |  3501 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3502 | `					pTos->rVal--;` |
|        - |  3503 | `					/* Try to get an integer representation */` |
|      ! 0 |  3504 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3505 | `				}else{` |
|      ! 0 |  3506 | `					pTos->x.iVal--;` |
|      ! 0 |  3507 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3508 | `				}` |
|      ! 0 |  3509 | `			}` |
|        - |  3510 | `		}` |
|        2 |  3511 | `	}` |
|        5 |  3512 | `	break;` |
|        - |  3513 | `/*` |
|        - |  3514 | ` * UMINUS: * * *` |
|        - |  3515 | ` *` |
|        - |  3516 | ` * Perform a unary minus operation.` |
|        - |  3517 | ` */` |
|    22366 |  3518 | `case PH7_OP_UMINUS:` |
|        - |  3519 | `#ifdef UNTRUST` |
|        - |  3520 | `	if( pTos < pStack ){` |
|        - |  3521 | `		goto Abort;` |
|        - |  3522 | `	}` |
|        - |  3523 | `#endif` |
|        - |  3524 | `	/* Force a numeric (integer,real or both) cast */` |
|    44734 |  3525 | `	PH7_MemObjToNumeric(pTos);` |
|    44734 |  3526 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3527 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3528 | `	}` |
|    44734 |  3529 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    44704 |  3530 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    22351 |  3531 | `	}` |
|    44734 |  3532 | `	break;` |
|        - |  3533 | `/*` |
|        - |  3534 | ` * UPLUS: * * *` |
|        - |  3535 | ` *` |
|        - |  3536 | ` * Perform a unary plus operation.` |
|        - |  3537 | ` */` |
|       16 |  3538 | `case PH7_OP_UPLUS:` |
|        - |  3539 | `#ifdef UNTRUST` |
|        - |  3540 | `	if( pTos < pStack ){` |
|        - |  3541 | `		goto Abort;` |
|        - |  3542 | `	}` |
|        - |  3543 | `#endif` |
|        - |  3544 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3545 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3546 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3547 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3548 | `	}` |
|       33 |  3549 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3550 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3551 | `	}` |
|       33 |  3552 | `	break;` |
|        - |  3553 | `/*` |
|        - |  3554 | ` * OP_LNOT: * * *` |
|        - |  3555 | ` *` |
|        - |  3556 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3557 | ` * with its complement.` |
|        - |  3558 | ` */` |
|    51982 |  3559 | `case PH7_OP_LNOT:` |
|        - |  3560 | `#ifdef UNTRUST` |
|        - |  3561 | `	if( pTos < pStack ){` |
|        - |  3562 | `		goto Abort;` |
|        - |  3563 | `	}` |
|        - |  3564 | `#endif` |
|        - |  3565 | `	/* Force a boolean cast */` |
|   104010 |  3566 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3567 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3568 | `	}` |
|   104010 |  3569 | `	pTos->x.iVal = !pTos->x.iVal;` |
|   104010 |  3570 | `	break;` |
|        - |  3571 | `/*` |
|        - |  3572 | ` * OP_BITNOT: * * *` |
|        - |  3573 | ` *` |
|        - |  3574 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3575 | ` * with its ones-complement.` |
|        - |  3576 | ` */` |
|       14 |  3577 | `case PH7_OP_BITNOT:` |
|        - |  3578 | `#ifdef UNTRUST` |
|        - |  3579 | `	if( pTos < pStack ){` |
|        - |  3580 | `		goto Abort;` |
|        - |  3581 | `	}` |
|        - |  3582 | `#endif` |
|        - |  3583 | `	/* Force an integer cast */` |
|       30 |  3584 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3585 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3586 | `	}` |
|       30 |  3587 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3588 | `	break;` |
|        - |  3589 | `/* OP_MUL * * *` |
|        - |  3590 | ` * OP_MUL_STORE * * *` |
|        - |  3591 | ` *` |
|        - |  3592 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3593 | ` * and push the result back onto the stack.` |
|        - |  3594 | ` */` |
|     1234 |  3595 | `case PH7_OP_MUL:` |
|        - |  3596 | `case PH7_OP_MUL_STORE: {` |
|     2470 |  3597 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3598 | `	/* Force the operand to be numeric */` |
|        - |  3599 | `#ifdef UNTRUST` |
|        - |  3600 | `	if( pNos < pStack ){` |
|        - |  3601 | `		goto Abort;` |
|        - |  3602 | `	}` |
|        - |  3603 | `#endif` |
|     2470 |  3604 | `	PH7_MemObjToNumeric(pTos);` |
|     2470 |  3605 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3606 | `	/* Perform the requested operation */` |
|     2470 |  3607 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3608 | `		/* Floating point arithemic */` |
|        - |  3609 | `		ph7_real a,b,r;` |
|       17 |  3610 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3611 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3612 | `		}` |
|       17 |  3613 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3614 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3615 | `		}` |
|       17 |  3616 | `		a = pNos->rVal;` |
|       17 |  3617 | `		b = pTos->rVal;` |
|       17 |  3618 | `		r = a * b;` |
|        - |  3619 | `		/* Push the result */` |
|       17 |  3620 | `		pNos->rVal = r;` |
|       17 |  3621 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3622 | `		/* Try to get an integer representation */` |
|       17 |  3623 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3624 | `	}else{` |
|        - |  3625 | `		/* Integer arithmetic */` |
|        - |  3626 | `		sxi64 a,b,r;` |
|     2454 |  3627 | `		a = pNos->x.iVal;` |
|     2454 |  3628 | `		b = pTos->x.iVal;` |
|     2454 |  3629 | `		r = a * b;` |
|        - |  3630 | `		/* Push the result */` |
|     2454 |  3631 | `		pNos->x.iVal = r;` |
|     2454 |  3632 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3633 | `	}` |
|     2470 |  3634 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3635 | `		ph7_value *pObj;` |
|       19 |  3636 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3637 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3638 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3639 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3640 | `		}` |
|        9 |  3641 | `	}` |
|     2470 |  3642 | `	VmPopOperand(&pTos,1);` |
|     2470 |  3643 | `	break;` |
|        - |  3644 | `				 }` |
|        - |  3645 | `/* OP_ADD * * *` |
|        - |  3646 | ` *` |
|        - |  3647 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3648 | ` * and push the result back onto the stack.` |
|        - |  3649 | ` */` |
|      426 |  3650 | `case PH7_OP_ADD:{` |
|      854 |  3651 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3652 | `#ifdef UNTRUST` |
|        - |  3653 | `	if( pNos < pStack ){` |
|        - |  3654 | `		goto Abort;` |
|        - |  3655 | `	}` |
|        - |  3656 | `#endif` |
|        - |  3657 | `	/* Perform the addition */` |
|      854 |  3658 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      854 |  3659 | `	VmPopOperand(&pTos,1);` |
|      854 |  3660 | `	break;` |
|        - |  3661 | `				}` |
|        - |  3662 | `/*` |
|        - |  3663 | ` * OP_ADD_STORE * * *` |
|        - |  3664 | ` *` |
|        - |  3665 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3666 | ` * and push the result back onto the stack.` |
|        - |  3667 | ` */` |
|      481 |  3668 | `case PH7_OP_ADD_STORE:{` |
|      963 |  3669 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3670 | `	ph7_value *pObj;` |
|        - |  3671 | `	sxu32 nIdx;` |
|        - |  3672 | `#ifdef UNTRUST` |
|        - |  3673 | `	if( pNos < pStack ){` |
|        - |  3674 | `		goto Abort;` |
|        - |  3675 | `	}` |
|        - |  3676 | `#endif` |
|        - |  3677 | `	/* Perform the addition */` |
|      963 |  3678 | `	nIdx = pTos->nIdx;` |
|      963 |  3679 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3680 | `	/* Peform the store operation */` |
|      963 |  3681 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3682 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      963 |  3683 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      963 |  3684 | `		PH7_MemObjStore(pTos,pObj);` |
|      481 |  3685 | `	}` |
|        - |  3686 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      963 |  3687 | `	PH7_MemObjStore(pTos,pNos);` |
|      963 |  3688 | `	VmPopOperand(&pTos,1);` |
|      963 |  3689 | `	break;` |
|        - |  3690 | `				}` |
|        - |  3691 | `/* OP_SUB * * *` |
|        - |  3692 | ` *` |
|        - |  3693 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3694 | ` * first (what was next on the stack) from the second (the` |
|        - |  3695 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3696 | ` */` |
|      294 |  3697 | `case PH7_OP_SUB: {` |
|      589 |  3698 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3699 | `#ifdef UNTRUST` |
|        - |  3700 | `	if( pNos < pStack ){` |
|        - |  3701 | `		goto Abort;` |
|        - |  3702 | `	}` |
|        - |  3703 | `#endif` |
|      589 |  3704 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3705 | `		/* Floating point arithemic */` |
|        - |  3706 | `		ph7_real a,b,r;` |
|       95 |  3707 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3708 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3709 | `		}` |
|       95 |  3710 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3711 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3712 | `		}` |
|       95 |  3713 | `		a = pNos->rVal;` |
|       95 |  3714 | `		b = pTos->rVal;` |
|       95 |  3715 | `		r = a - b;` |
|        - |  3716 | `		/* Push the result */` |
|       95 |  3717 | `		pNos->rVal = r;` |
|       95 |  3718 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3719 | `		/* Try to get an integer representation */` |
|       95 |  3720 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3721 | `	}else{` |
|        - |  3722 | `		/* Integer arithmetic */` |
|        - |  3723 | `		sxi64 a,b,r;` |
|      495 |  3724 | `		a = pNos->x.iVal;` |
|      495 |  3725 | `		b = pTos->x.iVal;` |
|      495 |  3726 | `		r = a - b;` |
|        - |  3727 | `		/* Push the result */` |
|      495 |  3728 | `		pNos->x.iVal = r;` |
|      495 |  3729 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3730 | `	}` |
|      589 |  3731 | `	VmPopOperand(&pTos,1);` |
|      589 |  3732 | `	break;` |
|        - |  3733 | `				 }` |
|        - |  3734 | `/* OP_SUB_STORE * * *` |
|        - |  3735 | ` *` |
|        - |  3736 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3737 | ` * first (what was next on the stack) from the second (the` |
|        - |  3738 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3739 | ` */` |
|        1 |  3740 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3741 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3742 | `	ph7_value *pObj;` |
|        - |  3743 | `#ifdef UNTRUST` |
|        - |  3744 | `	if( pNos < pStack ){` |
|        - |  3745 | `		goto Abort;` |
|        - |  3746 | `	}` |
|        - |  3747 | `#endif` |
|        3 |  3748 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3749 | `		/* Floating point arithemic */` |
|        - |  3750 | `		ph7_real a,b,r;` |
|      ! 0 |  3751 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3752 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3753 | `		}` |
|      ! 0 |  3754 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3755 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3756 | `		}` |
|      ! 0 |  3757 | `		a = pTos->rVal;` |
|      ! 0 |  3758 | `		b = pNos->rVal;` |
|      ! 0 |  3759 | `		r = a - b;` |
|        - |  3760 | `		/* Push the result */` |
|      ! 0 |  3761 | `		pNos->rVal = r;` |
|      ! 0 |  3762 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3763 | `		/* Try to get an integer representation */` |
|      ! 0 |  3764 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3765 | `	}else{` |
|        - |  3766 | `		/* Integer arithmetic */` |
|        - |  3767 | `		sxi64 a,b,r;` |
|        3 |  3768 | `		a = pTos->x.iVal;` |
|        3 |  3769 | `		b = pNos->x.iVal;` |
|        3 |  3770 | `		r = a - b;` |
|        - |  3771 | `		/* Push the result */` |
|        3 |  3772 | `		pNos->x.iVal = r;` |
|        3 |  3773 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3774 | `	}` |
|        3 |  3775 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3776 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3777 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3778 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3779 | `	}` |
|        3 |  3780 | `	VmPopOperand(&pTos,1);` |
|        3 |  3781 | `	break;` |
|        - |  3782 | `				 }` |
|        - |  3783 |  |
|        - |  3784 | `/*` |
|        - |  3785 | ` * OP_MOD * * *` |
|        - |  3786 | ` *` |
|        - |  3787 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3788 | ` * first (what was next on the stack) from the second (the` |
|        - |  3789 | ` * top of the stack) and push the remainder after division` |
|        - |  3790 | ` * onto the stack.` |
|        - |  3791 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3792 | ` */` |
|      296 |  3793 | `case PH7_OP_MOD:{` |
|      594 |  3794 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3795 | `	sxi64 a,b,r;` |
|        - |  3796 | `#ifdef UNTRUST` |
|        - |  3797 | `	if( pNos < pStack ){` |
|        - |  3798 | `		goto Abort;` |
|        - |  3799 | `	}` |
|        - |  3800 | `#endif` |
|        - |  3801 | `	/* Force the operands to be integer */` |
|      594 |  3802 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3803 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3804 | `	}` |
|      594 |  3805 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3806 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3807 | `	}` |
|        - |  3808 | `	/* Perform the requested operation */` |
|      594 |  3809 | `	a = pNos->x.iVal;` |
|      594 |  3810 | `	b = pTos->x.iVal;` |
|      594 |  3811 | `	if( b == 0 ){` |
|        3 |  3812 | `		r = 0;` |
|        3 |  3813 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3814 | `		/* goto Abort; */` |
|        2 |  3815 | `	}else{` |
|      591 |  3816 | `		r = a%b;` |
|        - |  3817 | `	}` |
|        - |  3818 | `	/* Push the result */` |
|      594 |  3819 | `	pNos->x.iVal = r;` |
|      594 |  3820 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3821 | `	VmPopOperand(&pTos,1);` |
|      594 |  3822 | `	break;` |
|        - |  3823 | `				}` |
|        - |  3824 | `/*` |
|        - |  3825 | ` * OP_MOD_STORE * * *` |
|        - |  3826 | ` *` |
|        - |  3827 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3828 | ` * first (what was next on the stack) from the second (the` |
|        - |  3829 | ` * top of the stack) and push the remainder after division` |
|        - |  3830 | ` * onto the stack.` |
|        - |  3831 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3832 | ` */` |
|        1 |  3833 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3834 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3835 | `	ph7_value *pObj;` |
|        - |  3836 | `	sxi64 a,b,r;` |
|        - |  3837 | `#ifdef UNTRUST` |
|        - |  3838 | `	if( pNos < pStack ){` |
|        - |  3839 | `		goto Abort;` |
|        - |  3840 | `	}` |
|        - |  3841 | `#endif` |
|        - |  3842 | `	/* Force the operands to be integer */` |
|        3 |  3843 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3844 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3845 | `	}` |
|        3 |  3846 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3847 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3848 | `	}` |
|        - |  3849 | `	/* Perform the requested operation */` |
|        3 |  3850 | `	a = pTos->x.iVal;` |
|        3 |  3851 | `	b = pNos->x.iVal;` |
|        3 |  3852 | `	if( b == 0 ){` |
|      ! 0 |  3853 | `		r = 0;` |
|      ! 0 |  3854 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3855 | `		/* goto Abort; */` |
|      ! 0 |  3856 | `	}else{` |
|        3 |  3857 | `		r = a%b;` |
|        - |  3858 | `	}` |
|        - |  3859 | `	/* Push the result */` |
|        3 |  3860 | `	pNos->x.iVal = r;` |
|        3 |  3861 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3862 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3863 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3864 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3865 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3866 | `	}` |
|        3 |  3867 | `	VmPopOperand(&pTos,1);` |
|        3 |  3868 | `	break;` |
|        - |  3869 | `				}` |
|        - |  3870 | `/*` |
|        - |  3871 | ` * OP_DIV * * *` |
|        - |  3872 | ` *` |
|        - |  3873 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3874 | ` * first (what was next on the stack) from the second (the` |
|        - |  3875 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3876 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3877 | ` */` |
|       28 |  3878 | `case PH7_OP_DIV:{` |
|       58 |  3879 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3880 | `	ph7_real a,b,r;` |
|        - |  3881 | `#ifdef UNTRUST` |
|        - |  3882 | `	if( pNos < pStack ){` |
|        - |  3883 | `		goto Abort;` |
|        - |  3884 | `	}` |
|        - |  3885 | `#endif` |
|        - |  3886 | `	/* Force the operands to be real */` |
|       58 |  3887 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3888 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3889 | `	}` |
|       58 |  3890 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3891 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3892 | `	}` |
|        - |  3893 | `	/* Perform the requested operation */` |
|       58 |  3894 | `	a = pNos->rVal;` |
|       58 |  3895 | `	b = pTos->rVal;` |
|       58 |  3896 | `	if( b == 0 ){` |
|        - |  3897 | `		/* Division by zero */` |
|        3 |  3898 | `		pNos->rVal = 0;` |
|        3 |  3899 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  3900 | `		/* goto Abort; */` |
|        2 |  3901 | `	}else{` |
|       55 |  3902 | `		r = a/b;` |
|        - |  3903 | `		/* Push the result */` |
|       55 |  3904 | `		pNos->rVal = r;` |
|       55 |  3905 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3906 | `		/* Try to get an integer representation */` |
|       55 |  3907 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3908 | `	}` |
|       58 |  3909 | `	VmPopOperand(&pTos,1);` |
|       58 |  3910 | `	break;` |
|        - |  3911 | `				}` |
|        - |  3912 | `/*` |
|        - |  3913 | ` * OP_DIV_STORE * * *` |
|        - |  3914 | ` *` |
|        - |  3915 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3916 | ` * first (what was next on the stack) from the second (the` |
|        - |  3917 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3918 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3919 | ` */` |
|        1 |  3920 | `case PH7_OP_DIV_STORE:{` |
|        3 |  3921 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3922 | `	ph7_value *pObj;` |
|        - |  3923 | `	ph7_real a,b,r;` |
|        - |  3924 | `#ifdef UNTRUST` |
|        - |  3925 | `	if( pNos < pStack ){` |
|        - |  3926 | `		goto Abort;` |
|        - |  3927 | `	}` |
|        - |  3928 | `#endif` |
|        - |  3929 | `	/* Force the operands to be real */` |
|        3 |  3930 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3931 | `		PH7_MemObjToReal(pTos);` |
|        1 |  3932 | `	}` |
|        3 |  3933 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  3934 | `		PH7_MemObjToReal(pNos);` |
|        1 |  3935 | `	}` |
|        - |  3936 | `	/* Perform the requested operation */` |
|        3 |  3937 | `	a = pTos->rVal;` |
|        3 |  3938 | `	b = pNos->rVal;` |
|        3 |  3939 | `	if( b == 0 ){` |
|        - |  3940 | `		/* Division by zero */` |
|      ! 0 |  3941 | `		r = 0;` |
|      ! 0 |  3942 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  3943 | `		/* goto Abort; */` |
|      ! 0 |  3944 | `	}else{` |
|        3 |  3945 | `		r = a/b;` |
|        - |  3946 | `		/* Push the result */` |
|        3 |  3947 | `		pNos->rVal = r;` |
|        3 |  3948 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3949 | `		/* Try to get an integer representation */` |
|        3 |  3950 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  3951 | `	}` |
|        3 |  3952 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3953 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3954 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3955 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3956 | `	}` |
|        3 |  3957 | `	VmPopOperand(&pTos,1);` |
|        3 |  3958 | `	break;` |
|        - |  3959 | `				}` |
|        - |  3960 | `/* OP_BAND * * *` |
|        - |  3961 | ` *` |
|        - |  3962 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3963 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  3964 | ` * two elements.` |
|        - |  3965 | `*/` |
|        - |  3966 | `/* OP_BOR * * *` |
|        - |  3967 | ` *` |
|        - |  3968 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3969 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  3970 | ` * two elements.` |
|        - |  3971 | ` */` |
|        - |  3972 | `/* OP_BXOR * * *` |
|        - |  3973 | ` *` |
|        - |  3974 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  3975 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  3976 | ` * two elements.` |
|        - |  3977 | ` */` |
|       30 |  3978 | `case PH7_OP_BAND:` |
|        - |  3979 | `case PH7_OP_BOR:` |
|        - |  3980 | `case PH7_OP_BXOR:{` |
|       62 |  3981 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3982 | `	sxi64 a,b,r;` |
|        - |  3983 | `#ifdef UNTRUST` |
|        - |  3984 | `	if( pNos < pStack ){` |
|        - |  3985 | `		goto Abort;` |
|        - |  3986 | `	}` |
|        - |  3987 | `#endif` |
|        - |  3988 | `	/* Force the operands to be integer */` |
|       62 |  3989 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3990 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3991 | `	}` |
|       62 |  3992 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3993 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3994 | `	}` |
|        - |  3995 | `	/* Perform the requested operation */` |
|       62 |  3996 | `	a = pNos->x.iVal;` |
|       62 |  3997 | `	b = pTos->x.iVal;` |
|       62 |  3998 | `	switch(pInstr->iOp){` |
|        6 |  3999 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4000 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4001 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4002 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4003 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4004 | `	case PH7_OP_BAND:` |
|       38 |  4005 | `	default:          r = a&b; break;` |
|        - |  4006 | `	}` |
|        - |  4007 | `	/* Push the result */` |
|       62 |  4008 | `	pNos->x.iVal = r;` |
|       62 |  4009 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4010 | `	VmPopOperand(&pTos,1);` |
|       62 |  4011 | `	break;` |
|        - |  4012 | `				 }` |
|        - |  4013 | `/* OP_BAND_STORE * * *` |
|        - |  4014 | ` *` |
|        - |  4015 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4016 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4017 | ` * two elements.` |
|        - |  4018 | `*/` |
|        - |  4019 | `/* OP_BOR_STORE * * *` |
|        - |  4020 | ` *` |
|        - |  4021 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4022 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4023 | ` * two elements.` |
|        - |  4024 | ` */` |
|        - |  4025 | `/* OP_BXOR_STORE * * *` |
|        - |  4026 | ` *` |
|        - |  4027 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4028 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4029 | ` * two elements.` |
|        - |  4030 | ` */` |
|        7 |  4031 | `case PH7_OP_BAND_STORE:` |
|        - |  4032 | `case PH7_OP_BOR_STORE:` |
|        - |  4033 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4034 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4035 | `	ph7_value *pObj;` |
|        - |  4036 | `	sxi64 a,b,r;` |
|        - |  4037 | `#ifdef UNTRUST` |
|        - |  4038 | `	if( pNos < pStack ){` |
|        - |  4039 | `		goto Abort;` |
|        - |  4040 | `	}` |
|        - |  4041 | `#endif` |
|        - |  4042 | `	/* Force the operands to be integer */` |
|       15 |  4043 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4044 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4045 | `	}` |
|       15 |  4046 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4047 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4048 | `	}` |
|        - |  4049 | `	/* Perform the requested operation */` |
|       15 |  4050 | `	a = pTos->x.iVal;` |
|       15 |  4051 | `	b = pNos->x.iVal;` |
|       15 |  4052 | `	switch(pInstr->iOp){` |
|        2 |  4053 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4054 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4055 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4056 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4057 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4058 | `	case PH7_OP_BAND:` |
|        5 |  4059 | `	default:          r = a&b; break;` |
|        - |  4060 | `	}` |
|        - |  4061 | `	/* Push the result */` |
|       15 |  4062 | `	pNos->x.iVal = r;` |
|       15 |  4063 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4064 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4065 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4066 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4067 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4068 | `	}` |
|       15 |  4069 | `	VmPopOperand(&pTos,1);` |
|       15 |  4070 | `	break;` |
|        - |  4071 | `				 }` |
|        - |  4072 | `/* OP_SHL * * *` |
|        - |  4073 | ` *` |
|        - |  4074 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4075 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4076 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4077 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4078 | ` */` |
|        - |  4079 | `/* OP_SHR * * *` |
|        - |  4080 | ` *` |
|        - |  4081 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4082 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4083 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4084 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4085 | ` */` |
|        9 |  4086 | `case PH7_OP_SHL:` |
|        - |  4087 | `case PH7_OP_SHR: {` |
|       19 |  4088 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4089 | `	sxi64 a,r;` |
|        - |  4090 | `	sxi32 b;` |
|        - |  4091 | `#ifdef UNTRUST` |
|        - |  4092 | `	if( pNos < pStack ){` |
|        - |  4093 | `		goto Abort;` |
|        - |  4094 | `	}` |
|        - |  4095 | `#endif` |
|        - |  4096 | `	/* Force the operands to be integer */` |
|       19 |  4097 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4098 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4099 | `	}` |
|       19 |  4100 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4101 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4102 | `	}` |
|        - |  4103 | `	/* Perform the requested operation */` |
|       19 |  4104 | `	a = pNos->x.iVal;` |
|       19 |  4105 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4106 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4107 | `		r = a << b;` |
|        6 |  4108 | `	}else{` |
|        9 |  4109 | `		r = a >> b;` |
|        - |  4110 | `	}` |
|        - |  4111 | `	/* Push the result */` |
|       19 |  4112 | `	pNos->x.iVal = r;` |
|       19 |  4113 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4114 | `	VmPopOperand(&pTos,1);` |
|       19 |  4115 | `	break;` |
|        - |  4116 | `				 }` |
|        - |  4117 | `/*  OP_SHL_STORE * * *` |
|        - |  4118 | ` *` |
|        - |  4119 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4120 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4121 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4122 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4123 | ` */` |
|        - |  4124 | `/* OP_SHR_STORE * * *` |
|        - |  4125 | ` *` |
|        - |  4126 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4127 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4128 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4129 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4130 | ` */` |
|        7 |  4131 | `case PH7_OP_SHL_STORE:` |
|        - |  4132 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4133 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4134 | `	ph7_value *pObj;` |
|        - |  4135 | `	sxi64 a,r;` |
|        - |  4136 | `	sxi32 b;` |
|        - |  4137 | `#ifdef UNTRUST` |
|        - |  4138 | `	if( pNos < pStack ){` |
|        - |  4139 | `		goto Abort;` |
|        - |  4140 | `	}` |
|        - |  4141 | `#endif` |
|        - |  4142 | `	/* Force the operands to be integer */` |
|       15 |  4143 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4144 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4145 | `	}` |
|       15 |  4146 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4147 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4148 | `	}` |
|        - |  4149 | `	/* Perform the requested operation */` |
|       15 |  4150 | `	a = pTos->x.iVal;` |
|       15 |  4151 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4152 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4153 | `		r = a << b;` |
|        4 |  4154 | `	}else{` |
|        9 |  4155 | `		r = a >> b;` |
|        - |  4156 | `	}` |
|        - |  4157 | `	/* Push the result */` |
|       15 |  4158 | `	pNos->x.iVal = r;` |
|       15 |  4159 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4160 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4161 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4162 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4163 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4164 | `	}` |
|       15 |  4165 | `	VmPopOperand(&pTos,1);` |
|       15 |  4166 | `	break;` |
|        - |  4167 | `				 }` |
|        - |  4168 | `/* CAT:  P1 * *` |
|        - |  4169 | ` *` |
|        - |  4170 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4171 | ` * back.` |
|        - |  4172 | ` */` |
|    59827 |  4173 | `case PH7_OP_CAT:{` |
|        - |  4174 | `	ph7_value *pNos,*pCur;` |
|   119656 |  4175 | `	if( pInstr->iP1 < 1 ){` |
|    92772 |  4176 | `		pNos = &pTos[-1];` |
|    46387 |  4177 | `	}else{` |
|    26886 |  4178 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4179 | `	}` |
|        - |  4180 | `#ifdef UNTRUST` |
|        - |  4181 | `	if( pNos < pStack ){` |
|        - |  4182 | `		goto Abort;` |
|        - |  4183 | `	}` |
|        - |  4184 | `#endif` |
|        - |  4185 | `	/* Force a string cast */` |
|   119656 |  4186 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      980 |  4187 | `		PH7_MemObjToString(pNos);` |
|      489 |  4188 | `	}` |
|   119656 |  4189 | `	pCur = &pNos[1];` |
|   241150 |  4190 | `	while( pCur <= pTos ){` |
|   121496 |  4191 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50450 |  4192 | `			PH7_MemObjToString(pCur);` |
|    25224 |  4193 | `		}` |
|        - |  4194 | `		/* Perform the concatenation */` |
|   121496 |  4195 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   121458 |  4196 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    60728 |  4197 | `		}` |
|   121496 |  4198 | `		SyBlobRelease(&pCur->sBlob);` |
|   121496 |  4199 | `		pCur++;` |
|        2 |  4200 | `	}` |
|   119656 |  4201 | `	pTos = pNos;` |
|   119656 |  4202 | `	break;` |
|        - |  4203 | `				}` |
|        - |  4204 | `/*  CAT_STORE: * * *` |
|        - |  4205 | ` *` |
|        - |  4206 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4207 | ` * back.` |
|        - |  4208 | ` */` |
|     2960 |  4209 | `case PH7_OP_CAT_STORE:{` |
|     5922 |  4210 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4211 | `	ph7_value *pObj;` |
|        - |  4212 | `#ifdef UNTRUST` |
|        - |  4213 | `	if( pNos < pStack ){` |
|        - |  4214 | `		goto Abort;` |
|        - |  4215 | `	}` |
|        - |  4216 | `#endif` |
|     5922 |  4217 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4218 | `		/* Force a string cast */` |
|      ! 0 |  4219 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4220 | `	}` |
|     5922 |  4221 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4222 | `		/* Force a string cast */` |
|      ! 0 |  4223 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4224 | `	}` |
|        - |  4225 | `	/* Perform the concatenation (Reverse order) */` |
|     5922 |  4226 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     5922 |  4227 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     2960 |  4228 | `	}` |
|        - |  4229 | `	/* Perform the store operation */` |
|     5922 |  4230 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4231 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     5922 |  4232 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     5922 |  4233 | `		PH7_MemObjStore(pTos,pObj);` |
|     2960 |  4234 | `	}` |
|     5922 |  4235 | `	PH7_MemObjStore(pTos,pNos);` |
|     5922 |  4236 | `	VmPopOperand(&pTos,1);` |
|     5922 |  4237 | `	break;` |
|        - |  4238 | `				}` |
|        - |  4239 | `/* OP_AND: * * *` |
|        - |  4240 | ` *` |
|        - |  4241 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4242 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4243 | ` * stack.` |
|        - |  4244 | ` */` |
|        - |  4245 | `/* OP_OR: * * *` |
|        - |  4246 | ` *` |
|        - |  4247 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4248 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4249 | ` * stack.` |
|        - |  4250 | ` */` |
|   110876 |  4251 | `case PH7_OP_LAND:` |
|        - |  4252 | `case PH7_OP_LOR: {` |
|   221798 |  4253 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4254 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4255 | `#ifdef UNTRUST` |
|        - |  4256 | `	if( pNos < pStack ){` |
|        - |  4257 | `		goto Abort;` |
|        - |  4258 | `	}` |
|        - |  4259 | `#endif` |
|        - |  4260 | `	/* Force a boolean cast */` |
|   221798 |  4261 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4262 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4263 | `	}` |
|   221798 |  4264 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4265 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4266 | `	}` |
|   221798 |  4267 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   221798 |  4268 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   221798 |  4269 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4270 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   110920 |  4271 | `		v1 = and_logic[v1*3+v2];` |
|    55483 |  4272 | `	}else{` |
|        - |  4273 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   110880 |  4274 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4275 | `	}` |
|   221798 |  4276 | `	if( v1 == 2 ){` |
|      ! 0 |  4277 | `		v1 = 1;` |
|      ! 0 |  4278 | `	}` |
|   221798 |  4279 | `	VmPopOperand(&pTos,1);` |
|   221798 |  4280 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   221798 |  4281 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   221798 |  4282 | `	break;` |
|        - |  4283 | `				 }` |
|        - |  4284 | `/* OP_LXOR: * * *` |
|        - |  4285 | ` *` |
|        - |  4286 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4287 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4288 | ` * stack.` |
|        - |  4289 | ` * According to the PHP language reference manual:` |
|        - |  4290 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4291 | ` *  TRUE,but not both.` |
|        - |  4292 | ` */` |
|        5 |  4293 | `case PH7_OP_LXOR:{` |
|       11 |  4294 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4295 | `	sxi32 v = 0;` |
|        - |  4296 | `#ifdef UNTRUST` |
|        - |  4297 | `	if( pNos < pStack ){` |
|        - |  4298 | `		goto Abort;` |
|        - |  4299 | `	}` |
|        - |  4300 | `#endif` |
|        - |  4301 | `	/* Force a boolean cast */` |
|       11 |  4302 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4303 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4304 | `	}` |
|       11 |  4305 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4306 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4307 | `	}` |
|       11 |  4308 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4309 | `		v = 1;` |
|        3 |  4310 | `	}` |
|       11 |  4311 | `	VmPopOperand(&pTos,1);` |
|       11 |  4312 | `	pTos->x.iVal = v;` |
|       11 |  4313 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4314 | `	break;` |
|        - |  4315 | `				 }` |
|        - |  4316 | `/* OP_EQ P1 P2 P3` |
|        - |  4317 | ` *` |
|        - |  4318 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4319 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4320 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4321 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4322 | ` */` |
|        - |  4323 | `/* OP_NEQ P1 P2 P3` |
|        - |  4324 | ` *` |
|        - |  4325 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4326 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4327 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4328 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4329 | ` */` |
|     3738 |  4330 | `case PH7_OP_EQ:` |
|        - |  4331 | `case PH7_OP_NEQ: {` |
|     7478 |  4332 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4333 | `	/* Perform the comparison and act accordingly */` |
|        - |  4334 | `#ifdef UNTRUST` |
|        - |  4335 | `	if( pNos < pStack ){` |
|        - |  4336 | `		goto Abort;` |
|        - |  4337 | `	}` |
|        - |  4338 | `#endif` |
|     7478 |  4339 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7478 |  4340 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4341 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7469 |  4342 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7434 |  4343 | `		rc = rc == 0;` |
|     3718 |  4344 | `	}else{` |
|       28 |  4345 | `		rc = rc != 0;` |
|        - |  4346 | `	}` |
|     7478 |  4347 | `	VmPopOperand(&pTos,1);` |
|     7478 |  4348 | `	if( !pInstr->iP2 ){` |
|        - |  4349 | `		/* Push comparison result without taking the jump */` |
|     7478 |  4350 | `		PH7_MemObjRelease(pTos);` |
|     7478 |  4351 | `		pTos->x.iVal = rc;` |
|        - |  4352 | `		/* Invalidate any prior representation */` |
|     7478 |  4353 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3740 |  4354 | `	}else{` |
|      ! 0 |  4355 | `		if( rc ){` |
|        - |  4356 | `			/* Jump to the desired location */` |
|      ! 0 |  4357 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4358 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4359 | `		}` |
|        - |  4360 | `	}` |
|     7478 |  4361 | `	break;` |
|        - |  4362 | `				 }` |
|        - |  4363 | `/* OP_TEQ P1 P2 *` |
|        - |  4364 | ` *` |
|        - |  4365 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4366 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4367 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4368 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4369 | ` */` |
|   129995 |  4370 | `case PH7_OP_TEQ: {` |
|   259992 |  4371 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4372 | `	/* Perform the comparison and act accordingly */` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `	if( pNos < pStack ){` |
|        - |  4375 | `		goto Abort;` |
|        - |  4376 | `	}` |
|        - |  4377 | `#endif` |
|   259992 |  4378 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   259992 |  4379 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4380 | `		rc = 0;` |
|        2 |  4381 | `	}else{` |
|   259990 |  4382 | `		rc = rc == 0;` |
|        - |  4383 | `	}` |
|   259992 |  4384 | `	VmPopOperand(&pTos,1);` |
|   259992 |  4385 | `	if( !pInstr->iP2 ){` |
|        - |  4386 | `		/* Push comparison result without taking the jump */` |
|   259992 |  4387 | `		PH7_MemObjRelease(pTos);` |
|   259992 |  4388 | `		pTos->x.iVal = rc;` |
|        - |  4389 | `		/* Invalidate any prior representation */` |
|   259992 |  4390 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   129997 |  4391 | `	}else{` |
|      ! 0 |  4392 | `		if( rc ){` |
|        - |  4393 | `			/* Jump to the desired location */` |
|      ! 0 |  4394 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4395 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4396 | `		}` |
|        - |  4397 | `	}` |
|   259992 |  4398 | `	break;` |
|        - |  4399 | `				 }` |
|        - |  4400 | `/* OP_TNE P1 P2 *` |
|        - |  4401 | ` *` |
|        - |  4402 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4403 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4404 | ` * instruction.` |
|        - |  4405 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4406 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4407 | ` *` |
|        - |  4408 | ` */` |
|   103181 |  4409 | `case PH7_OP_TNE: {` |
|   206364 |  4410 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4411 | `	/* Perform the comparison and act accordingly */` |
|        - |  4412 | `#ifdef UNTRUST` |
|        - |  4413 | `	if( pNos < pStack ){` |
|        - |  4414 | `		goto Abort;` |
|        - |  4415 | `	}` |
|        - |  4416 | `#endif` |
|   206364 |  4417 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   206364 |  4418 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4419 | `		rc = 1;` |
|        2 |  4420 | `	}else{` |
|   206362 |  4421 | `		rc = rc != 0;` |
|        - |  4422 | `	}` |
|   206364 |  4423 | `	VmPopOperand(&pTos,1);` |
|   206364 |  4424 | `	if( !pInstr->iP2 ){` |
|        - |  4425 | `		/* Push comparison result without taking the jump */` |
|   206364 |  4426 | `		PH7_MemObjRelease(pTos);` |
|   206364 |  4427 | `		pTos->x.iVal = rc;` |
|        - |  4428 | `		/* Invalidate any prior representation */` |
|   206364 |  4429 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   103183 |  4430 | `	}else{` |
|      ! 0 |  4431 | `		if( rc ){` |
|        - |  4432 | `			/* Jump to the desired location */` |
|      ! 0 |  4433 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4434 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4435 | `		}` |
|        - |  4436 | `	}` |
|   206364 |  4437 | `	break;` |
|        - |  4438 | `				 }` |
|        - |  4439 | `/* OP_LT P1 P2 P3` |
|        - |  4440 | ` *` |
|        - |  4441 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4442 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4443 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4444 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4445 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4446 | ` *` |
|        - |  4447 | ` */` |
|        - |  4448 | `/* OP_LE P1 P2 P3` |
|        - |  4449 | ` *` |
|        - |  4450 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4451 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4452 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4453 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4454 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4455 | ` *` |
|        - |  4456 | ` */` |
|   119918 |  4457 | `case PH7_OP_LT:` |
|        - |  4458 | `case PH7_OP_LE: {` |
|   239882 |  4459 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4460 | `	/* Perform the comparison and act accordingly */` |
|        - |  4461 | `#ifdef UNTRUST` |
|        - |  4462 | `	if( pNos < pStack ){` |
|        - |  4463 | `		goto Abort;` |
|        - |  4464 | `	}` |
|        - |  4465 | `#endif` |
|   239882 |  4466 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   239882 |  4467 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4468 | `		rc = 0;` |
|   239878 |  4469 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      395 |  4470 | `		rc = rc < 1;` |
|      198 |  4471 | `	}else{` |
|   239480 |  4472 | `		rc = rc < 0;` |
|        - |  4473 | `	}` |
|   239882 |  4474 | `	VmPopOperand(&pTos,1);` |
|   239882 |  4475 | `	if( !pInstr->iP2 ){` |
|        - |  4476 | `		/* Push comparison result without taking the jump */` |
|   239882 |  4477 | `		PH7_MemObjRelease(pTos);` |
|   239882 |  4478 | `		pTos->x.iVal = rc;` |
|        - |  4479 | `		/* Invalidate any prior representation */` |
|   239882 |  4480 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   119964 |  4481 | `	}else{` |
|      ! 0 |  4482 | `		if( rc ){` |
|        - |  4483 | `			/* Jump to the desired location */` |
|      ! 0 |  4484 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4485 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4486 | `		}` |
|        - |  4487 | `	}` |
|   239882 |  4488 | `	break;` |
|        - |  4489 | `				}` |
|        - |  4490 | `/* OP_GT P1 P2 P3` |
|        - |  4491 | ` *` |
|        - |  4492 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4493 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4494 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4495 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4496 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4497 | ` *` |
|        - |  4498 | ` */` |
|        - |  4499 | `/* OP_GE P1 P2 P3` |
|        - |  4500 | ` *` |
|        - |  4501 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4502 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4503 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4504 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4505 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4506 | ` *` |
|        - |  4507 | ` */` |
|    53132 |  4508 | `case PH7_OP_GT:` |
|        - |  4509 | `case PH7_OP_GE: {` |
|   106266 |  4510 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4511 | `	/* Perform the comparison and act accordingly */` |
|        - |  4512 | `#ifdef UNTRUST` |
|        - |  4513 | `	if( pNos < pStack ){` |
|        - |  4514 | `		goto Abort;` |
|        - |  4515 | `	}` |
|        - |  4516 | `#endif` |
|   106266 |  4517 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   106266 |  4518 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4519 | `		rc = 0;` |
|   106262 |  4520 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   106110 |  4521 | `		rc = rc >= 0;` |
|    53056 |  4522 | `	}else{` |
|      150 |  4523 | `		rc = rc > 0;` |
|        - |  4524 | `	}` |
|   106266 |  4525 | `	VmPopOperand(&pTos,1);` |
|   106266 |  4526 | `	if( !pInstr->iP2 ){` |
|        - |  4527 | `		/* Push comparison result without taking the jump */` |
|   106266 |  4528 | `		PH7_MemObjRelease(pTos);` |
|   106266 |  4529 | `		pTos->x.iVal = rc;` |
|        - |  4530 | `		/* Invalidate any prior representation */` |
|   106266 |  4531 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    53134 |  4532 | `	}else{` |
|      ! 0 |  4533 | `		if( rc ){` |
|        - |  4534 | `			/* Jump to the desired location */` |
|      ! 0 |  4535 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4536 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4537 | `		}` |
|        - |  4538 | `	}` |
|   106266 |  4539 | `	break;` |
|        - |  4540 | `				}` |
|        - |  4541 | `/* OP_SEQ P1 P2 *` |
|        - |  4542 | ` * Strict string comparison.` |
|        - |  4543 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4544 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4545 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4546 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4547 | ` * use PH7_OP_EQ.` |
|        - |  4548 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4549 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4550 | ` */` |
|        - |  4551 | `/* OP_SNE P1 P2 *` |
|        - |  4552 | ` * Strict string comparison.` |
|        - |  4553 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4554 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4555 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4556 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4557 | ` * use PH7_OP_EQ.` |
|        - |  4558 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4559 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4560 | ` */` |
|       18 |  4561 | `case PH7_OP_SEQ:` |
|        - |  4562 | `case PH7_OP_SNE: {` |
|       38 |  4563 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4564 | `	SyString s1,s2;` |
|        - |  4565 | `	/* Perform the comparison and act accordingly */` |
|        - |  4566 | `#ifdef UNTRUST` |
|        - |  4567 | `	if( pNos < pStack ){` |
|        - |  4568 | `		goto Abort;` |
|        - |  4569 | `	}` |
|        - |  4570 | `#endif` |
|        - |  4571 | `	/* Force a string cast */` |
|       38 |  4572 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4573 | `		PH7_MemObjToString(pTos);` |
|        2 |  4574 | `	}` |
|       38 |  4575 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4576 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4577 | `	}` |
|       38 |  4578 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4579 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4580 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4581 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4582 | `		rc = rc != 0;` |
|      ! 0 |  4583 | `	}else{` |
|       38 |  4584 | `		rc = rc == 0;` |
|        - |  4585 | `	}` |
|       38 |  4586 | `	VmPopOperand(&pTos,1);` |
|       38 |  4587 | `	if( !pInstr->iP2 ){` |
|        - |  4588 | `		/* Push comparison result without taking the jump */` |
|       38 |  4589 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4590 | `		pTos->x.iVal = rc;` |
|        - |  4591 | `		/* Invalidate any prior representation */` |
|       38 |  4592 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4593 | `	}else{` |
|      ! 0 |  4594 | `		if( rc ){` |
|        - |  4595 | `			/* Jump to the desired location */` |
|      ! 0 |  4596 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4597 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4598 | `		}` |
|        - |  4599 | `	}` |
|       38 |  4600 | `	break;` |
|        - |  4601 | `				 }` |
|        - |  4602 | `/*` |
|        - |  4603 | ` * OP_LOAD_REF * * *` |
|        - |  4604 | ` * Push the index of a referenced object on the stack.` |
|        - |  4605 | ` */` |
|       57 |  4606 | `case PH7_OP_LOAD_REF: {` |
|        - |  4607 | `	sxu32 nIdx;` |
|        - |  4608 | `#ifdef UNTRUST` |
|        - |  4609 | `	if( pTos < pStack ){` |
|        - |  4610 | `		goto Abort;` |
|        - |  4611 | `	}` |
|        - |  4612 | `#endif` |
|        - |  4613 | `	/* Extract memory object index */` |
|      115 |  4614 | `	nIdx = pTos->nIdx;` |
|      115 |  4615 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4616 | `		/* Nullify the object */` |
|       95 |  4617 | `		PH7_MemObjRelease(pTos);` |
|        - |  4618 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4619 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4620 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4621 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4622 | `	}` |
|      115 |  4623 | `	break;` |
|        - |  4624 | `					  }` |
|        - |  4625 | `/*` |
|        - |  4626 | ` * OP_STORE_REF * * P3` |
|        - |  4627 | ` * Perform an assignment operation by reference.` |
|        - |  4628 | ` */` |
|       14 |  4629 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4630 | `	 SyString sName = { 0 , 0 };` |
|        - |  4631 | `	 VmFrame *pFrameLocal;` |
|        - |  4632 | `	SyHashEntry *pEntry;` |
|        - |  4633 | `	sxu32 nIdx;` |
|        - |  4634 | `#ifdef UNTRUST` |
|        - |  4635 | `	if( pTos < pStack ){` |
|        - |  4636 | `		goto Abort;` |
|        - |  4637 | `	}` |
|        - |  4638 | `#endif` |
|       30 |  4639 | `	if( pInstr->p3 == 0 ){` |
|        - |  4640 | `		char *zName;` |
|        - |  4641 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4642 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4643 | `			/* Force a string cast */` |
|      ! 0 |  4644 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4645 | `		}` |
|      ! 0 |  4646 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4647 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4648 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4649 | `			if( zName ){` |
|      ! 0 |  4650 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4651 | `			}` |
|      ! 0 |  4652 | `		}` |
|      ! 0 |  4653 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4654 | `		pTos--;` |
|      ! 0 |  4655 | `	}else{` |
|       30 |  4656 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4657 | `	}` |
|       30 |  4658 | `	nIdx = pTos->nIdx;` |
|       30 |  4659 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4660 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4661 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4662 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4663 | `		}else{` |
|        - |  4664 | `			ph7_value *pObj;` |
|        - |  4665 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4666 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4667 | `			if( pObj == 0 ){` |
|      ! 0 |  4668 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4669 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4670 | `				goto Abort;` |
|        - |  4671 | `			}` |
|        - |  4672 | `			/* Perform the store operation */` |
|      ! 0 |  4673 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4674 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4675 | `		}` |
|       30 |  4676 | `	}else if( sName.nByte > 0){` |
|       30 |  4677 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4678 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4679 | `		}else{` |
|       30 |  4680 | `			pFrameLocal = pVm->pFrame;` |
|       30 |  4681 | `			while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4682 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  4683 | `				pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4684 | `			}` |
|        - |  4685 | `			/* Query the local frame */` |
|       30 |  4686 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4687 | `			if( pEntry ){` |
|      ! 0 |  4688 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4689 | `			}else{` |
|       30 |  4690 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4691 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4692 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4693 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4694 | `				}` |
|       30 |  4695 | `				if( rc == SXRET_OK ){` |
|       30 |  4696 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4697 | `				}` |
|        - |  4698 | `			}` |
|        - |  4699 | `		}` |
|       14 |  4700 | `	}` |
|       30 |  4701 | `	break;` |
|        - |  4702 | `				 }` |
|        - |  4703 | `/*` |
|        - |  4704 | ` * OP_UPLINK P1 * *` |
|        - |  4705 | ` * Link a variable to the top active VM frame.` |
|        - |  4706 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4707 | ` */` |
|       23 |  4708 | `case PH7_OP_UPLINK: {` |
|       47 |  4709 | `	if( pVm->pFrame->pParent ){` |
|       47 |  4710 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4711 | `		SyString sName;` |
|        - |  4712 | `		/* Perform the link */` |
|       95 |  4713 | `		while( pLink <= pTos ){` |
|       49 |  4714 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4715 | `				/* Force a string cast */` |
|      ! 0 |  4716 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4717 | `			}` |
|       49 |  4718 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       49 |  4719 | `			if( sName.nByte > 0 ){` |
|       49 |  4720 | `				VmFrameLink(&(*pVm),&sName);` |
|       24 |  4721 | `			}` |
|       49 |  4722 | `			pLink++;` |
|        1 |  4723 | `		}` |
|       23 |  4724 | `	}` |
|       47 |  4725 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       47 |  4726 | `	break;` |
|        - |  4727 | `					}` |
|        - |  4728 | `/*` |
|        - |  4729 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4730 | ` * Push an exception in the corresponding container so that` |
|        - |  4731 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4732 | ` */` |
|       12 |  4733 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       26 |  4734 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4735 | `	VmFrame *pFrameLocal;` |
|       26 |  4736 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4737 | `	/* Create the exception frame */` |
|       26 |  4738 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       26 |  4739 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4740 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4741 | `		goto Abort;` |
|        - |  4742 | `	}` |
|        - |  4743 | `	/* Mark the special frame */` |
|       26 |  4744 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       26 |  4745 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4746 | `	/* Point to the frame that trigger the exception */` |
|       26 |  4747 | `	pFrameLocal = pFrameLocal->pParent;` |
|       28 |  4748 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        3 |  4749 | `		pFrameLocal = pFrameLocal->pParent;` |
|        1 |  4750 | `	}` |
|       26 |  4751 | `	pException->pFrame = pFrameLocal;` |
|       26 |  4752 | `	break;` |
|        - |  4753 | `							}` |
|        - |  4754 | `/*` |
|        - |  4755 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4756 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4757 | ` */` |
|       12 |  4758 | `case PH7_OP_POP_EXCEPTION: {` |
|       26 |  4759 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       26 |  4760 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4761 | `		ph7_exception **apException;` |
|        - |  4762 | `		/* Pop the loaded exception */` |
|        7 |  4763 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        7 |  4764 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|        7 |  4765 | `			(void)SySetPop(&pVm->aException);` |
|        3 |  4766 | `		}` |
|        3 |  4767 | `	}` |
|       26 |  4768 | `	pException->pFrame = 0;` |
|        - |  4769 | `	/* Leave the exception frame */` |
|       26 |  4770 | `	VmLeaveFrame(&(*pVm));` |
|       26 |  4771 | `	break;` |
|        - |  4772 | `							}` |
|        - |  4773 |  |
|        - |  4774 | `/*` |
|        - |  4775 | ` * OP_THROW * P2 *` |
|        - |  4776 | ` * Throw an user exception.` |
|        - |  4777 | ` */` |
|       11 |  4778 | `case PH7_OP_THROW: {` |
|       24 |  4779 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       24 |  4780 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4781 | `#ifdef UNTRUST` |
|        - |  4782 | `	if( pTos < pStack ){` |
|        - |  4783 | `		goto Abort;` |
|        - |  4784 | `	}` |
|        - |  4785 | `#endif` |
|       28 |  4786 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4787 | `		/* Safely ignore the exception frame */` |
|        6 |  4788 | `		pFrameLocal = pFrameLocal->pParent;` |
|        2 |  4789 | `	}` |
|        - |  4790 | `	/* Tell the upper layer that an exception was thrown */` |
|       24 |  4791 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       24 |  4792 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       24 |  4793 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4794 | `		ph7_class *pException;` |
|        - |  4795 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4796 | `		 */` |
|       24 |  4797 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       24 |  4798 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4799 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4800 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4801 | `			if( rc == SXERR_ABORT ){` |
|        - |  4802 | `				/* Abort processing immediately */` |
|      ! 0 |  4803 | `				goto Abort;` |
|        - |  4804 | `			}` |
|      ! 0 |  4805 | `		}else{` |
|        - |  4806 | `			/* Throw the exception */` |
|       24 |  4807 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       24 |  4808 | `			if( rc == SXERR_ABORT ){` |
|        - |  4809 | `				/* Abort processing immediately */` |
|        9 |  4810 | `				goto Abort;` |
|        - |  4811 | `			}` |
|        - |  4812 | `		}` |
|        9 |  4813 | `	}else{` |
|        - |  4814 | `		/* Expecting a class instance */` |
|      ! 0 |  4815 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4816 | `		if( rc == SXERR_ABORT ){` |
|        - |  4817 | `			/* Abort processing immediately */` |
|      ! 0 |  4818 | `			goto Abort;` |
|        - |  4819 | `		}` |
|        - |  4820 | `	}` |
|        - |  4821 | `	/* Pop the top entry */` |
|       16 |  4822 | `	VmPopOperand(&pTos,1);` |
|        - |  4823 | `	/* Perform an unconditional jump */` |
|       16 |  4824 | `	pc = nJump - 1;` |
|       16 |  4825 | `	break;` |
|        - |  4826 | `				   }` |
|        - |  4827 | `/*` |
|        - |  4828 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4829 | ` * Prepare a foreach step.` |
|        - |  4830 | ` */` |
|     4643 |  4831 | `case PH7_OP_FOREACH_INIT: {` |
|     9288 |  4832 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4833 | `	void *pName;` |
|        - |  4834 | `#ifdef UNTRUST` |
|        - |  4835 | `	if( pTos < pStack ){` |
|        - |  4836 | `		goto Abort;` |
|        - |  4837 | `	}` |
|        - |  4838 | `#endif` |
|     9288 |  4839 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4840 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4841 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4842 | `			/* Force a string cast */` |
|      ! 0 |  4843 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4844 | `		}` |
|        - |  4845 | `		/* Duplicate name */` |
|      ! 0 |  4846 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4847 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4848 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4849 | `		}` |
|      ! 0 |  4850 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4851 | `	}` |
|     9288 |  4852 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4853 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4854 | `			/* Force a string cast */` |
|      ! 0 |  4855 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4856 | `		}` |
|        - |  4857 | `		/* Duplicate name */` |
|      ! 0 |  4858 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4859 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4860 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4861 | `		}` |
|      ! 0 |  4862 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4863 | `	}` |
|        - |  4864 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9288 |  4865 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4866 | `		/* Jump out of the loop */` |
|      ! 0 |  4867 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4868 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4869 | `		}` |
|      ! 0 |  4870 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4871 | `	}else{` |
|        - |  4872 | `		ph7_foreach_step *pStep;` |
|     9288 |  4873 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9288 |  4874 | `		if( pStep == 0 ){` |
|      ! 0 |  4875 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4876 | `			/* Jump out of the loop */` |
|      ! 0 |  4877 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4878 | `		}else{` |
|        - |  4879 | `			/* Zero the structure */` |
|     9288 |  4880 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4881 | `			/* Prepare the step */` |
|     9288 |  4882 | `			pStep->iFlags = pInfo->iFlags;` |
|     9288 |  4883 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9280 |  4884 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4885 | `				/* Reset the internal loop cursor */` |
|     9280 |  4886 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4887 | `				/* Mark the step */` |
|     9280 |  4888 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9280 |  4889 | `				pStep->xIter.pMap = pMap;` |
|     9280 |  4890 | `				pMap->iRef++;` |
|     4641 |  4891 | `			}else{` |
|        9 |  4892 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4893 | `				/* Reset the loop cursor */` |
|        9 |  4894 | `				SyHashResetLoopCursor(&pThis->hAttr);` |
|        - |  4895 | `				/* Mark the step */` |
|        9 |  4896 | `				pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  4897 | `				pStep->xIter.pThis = pThis;` |
|        9 |  4898 | `				pThis->iRef++;` |
|        - |  4899 | `			}` |
|        - |  4900 | `		}` |
|     9288 |  4901 | `		if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  4902 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  4903 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  4904 | `			/* Jump out of the loop */` |
|      ! 0 |  4905 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4906 | `		}` |
|        - |  4907 | `	}` |
|     9288 |  4908 | `	VmPopOperand(&pTos,1);` |
|     9288 |  4909 | `	break;` |
|        - |  4910 | `						  }` |
|        - |  4911 | `/*` |
|        - |  4912 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  4913 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  4914 | ` */` |
|    74263 |  4915 | `case PH7_OP_FOREACH_STEP: {` |
|   148528 |  4916 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4917 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  4918 | `	ph7_value *pValue;` |
|        - |  4919 | `	VmFrame *pFrameLocal;` |
|        - |  4920 | `	/* Peek the last step */` |
|   148528 |  4921 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   148528 |  4922 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   148528 |  4923 | `	pFrameLocal = pVm->pFrame;` |
|   148528 |  4924 | `	while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  4925 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  4926 | `		pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  4927 | `	}` |
|   148528 |  4928 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   148504 |  4929 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  4930 | `		ph7_hashmap_node *pNode;` |
|        - |  4931 | `		/* Extract the current node value */` |
|   148504 |  4932 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   148504 |  4933 | `		if( pNode == 0 ){` |
|        - |  4934 | `			/* No more entry to process */` |
|     9280 |  4935 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9280 |  4936 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4937 | `				/* Break the reference with the last element */` |
|        5 |  4938 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  4939 | `			}` |
|        - |  4940 | `			/* Automatically reset the loop cursor */` |
|     9280 |  4941 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4942 | `			/* Cleanup the mess left behind */` |
|     9280 |  4943 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9280 |  4944 | `			SySetPop(&pInfo->aStep);` |
|     9280 |  4945 | `			PH7_HashmapUnref(pMap);` |
|     4641 |  4946 | `		}else{` |
|   139226 |  4947 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      408 |  4948 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      408 |  4949 | `				if( pKey ){` |
|      408 |  4950 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      203 |  4951 | `				}` |
|      203 |  4952 | `			}` |
|   139226 |  4953 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4954 | `				SyHashEntry *pEntry;` |
|        - |  4955 | `				/* Pass by reference */` |
|       13 |  4956 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       13 |  4957 | `				if( pEntry ){` |
|       13 |  4958 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        7 |  4959 | `				}else{` |
|      ! 0 |  4960 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  4961 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  4962 | `				}` |
|        7 |  4963 | `			}else{` |
|        - |  4964 | `				/* Make a copy of the entry value */` |
|   139214 |  4965 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   139214 |  4966 | `				if( pValue ){` |
|   139214 |  4967 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    69606 |  4968 | `				}` |
|        - |  4969 | `			}` |
|        - |  4970 | `		}` |
|    74253 |  4971 | `	}else{` |
|       25 |  4972 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  4973 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  4974 | `		SyHashEntry *pEntry;` |
|        - |  4975 | `		/* Point to the next attribute */` |
|       29 |  4976 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  4977 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  4978 | `			/* Check access permission */` |
|       31 |  4979 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  4980 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  4981 | `					break; /* Access is granted */` |
|        - |  4982 | `			}` |
|        1 |  4983 | `		}` |
|       25 |  4984 | `		if( pEntry == 0 ){` |
|        - |  4985 | `			/* Clean up the mess left behind */` |
|        9 |  4986 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  4987 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  4988 | `				/* Break the reference with the last element */` |
|        3 |  4989 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  4990 | `			}` |
|        9 |  4991 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  4992 | `			SySetPop(&pInfo->aStep);` |
|        9 |  4993 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  4994 | `		}else{` |
|       17 |  4995 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  4996 | `			ph7_value *pAttrValue;` |
|       17 |  4997 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  4998 | `				/* Fill with the current attribute name */` |
|       17 |  4999 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5000 | `				if( pKey ){` |
|       17 |  5001 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5002 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5003 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5004 | `				}` |
|        8 |  5005 | `			}` |
|        - |  5006 | `			/* Extract attribute value */` |
|       17 |  5007 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5008 | `			if( pAttrValue ){` |
|       17 |  5009 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5010 | `					/* Pass by reference */` |
|        3 |  5011 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5012 | `					if( pEntry ){` |
|        3 |  5013 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5014 | `					}else{` |
|      ! 0 |  5015 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5016 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5017 | `					}` |
|        2 |  5018 | `				}else{` |
|        - |  5019 | `					/* Make a copy of the attribute value */` |
|       15 |  5020 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5021 | `					if( pValue ){` |
|       15 |  5022 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5023 | `					}` |
|        - |  5024 | `				}` |
|        8 |  5025 | `			}` |
|        - |  5026 | `		}` |
|        - |  5027 | `	}` |
|   148528 |  5028 | `	break;` |
|        - |  5029 | `						  }` |
|        - |  5030 | `/*` |
|        - |  5031 | ` * OP_MEMBER P1 P2` |
|        - |  5032 | ` * Load class attribute/method on the stack.` |
|        - |  5033 | ` */` |
|     1838 |  5034 | `case PH7_OP_MEMBER: {` |
|        - |  5035 | `	ph7_class_instance *pThis;` |
|        - |  5036 | `	ph7_value *pNos;` |
|        - |  5037 | `	SyString sName;` |
|     3678 |  5038 | `	if( !pInstr->iP1 ){` |
|     3620 |  5039 | `		pNos = &pTos[-1];` |
|        - |  5040 | `#ifdef UNTRUST` |
|        - |  5041 | `		if( pNos < pStack ){` |
|        - |  5042 | `			goto Abort;` |
|        - |  5043 | `		}` |
|        - |  5044 | `#endif` |
|     3620 |  5045 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5046 | `			ph7_class *pClass;` |
|        - |  5047 | `			/* Class already instantiated */` |
|     3620 |  5048 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5049 | `			/* Point to the instantiated class */` |
|     3620 |  5050 | `			pClass = pThis->pClass;` |
|        - |  5051 | `			/* Extract attribute name first */` |
|     3620 |  5052 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     3620 |  5053 | `			if( pInstr->iP2 ){` |
|        - |  5054 | `				/* Method call */` |
|      124 |  5055 | `				ph7_class_method *pMeth = 0;` |
|      124 |  5056 | `				if( sName.nByte > 0 ){` |
|        - |  5057 | `					/* Extract the target method */` |
|      124 |  5058 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       61 |  5059 | `				}` |
|      124 |  5060 | `				if( pMeth == 0 ){` |
|      ! 0 |  5061 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5062 | `						&pClass->sName,&sName` |
|        - |  5063 | `						);` |
|        - |  5064 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5065 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5066 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5067 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5068 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5069 | `				}else{` |
|        - |  5070 | `					/* Push method name on the stack */` |
|      124 |  5071 | `					PH7_MemObjRelease(pTos);` |
|      124 |  5072 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      124 |  5073 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5074 | `				}` |
|      124 |  5075 | `				pTos->nIdx = SXU32_HIGH;` |
|       63 |  5076 | `			}else{` |
|        - |  5077 | `				/* Attribute access */` |
|     3498 |  5078 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5079 | `				SyHashEntry *pEntry;` |
|        - |  5080 | `				/* Extract the target attribute */` |
|     3498 |  5081 | `				if( sName.nByte > 0 ){` |
|     3498 |  5082 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3498 |  5083 | `					if( pEntry ){` |
|        - |  5084 | `						/* Point to the attribute value */` |
|     3496 |  5085 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1747 |  5086 | `					}` |
|     1748 |  5087 | `				}` |
|     3498 |  5088 | `				if( pObjAttr == 0 ){` |
|        - |  5089 | `					/* No such attribute,load null */` |
|        4 |  5090 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5091 | `						&pClass->sName,&sName);` |
|        - |  5092 | `					/* Call the __get magic method if available */` |
|        3 |  5093 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5094 | `				}` |
|     3498 |  5095 | `				VmPopOperand(&pTos,1);` |
|        - |  5096 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5097 | `				 * This is due to the following case:` |
|        - |  5098 | `				 *     (new TestClass())->foo;` |
|        - |  5099 | `				 */` |
|     3498 |  5100 | `				pThis->iRef++;` |
|     3498 |  5101 | `				PH7_MemObjRelease(pTos);` |
|     3498 |  5102 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3498 |  5103 | `				if( pObjAttr ){` |
|     3496 |  5104 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5105 | `					/* Check attribute access */` |
|     3496 |  5106 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5107 | `						/* Load attribute */` |
|     3496 |  5108 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3496 |  5109 | `						if( pValue ){` |
|     3496 |  5110 | `							if( pThis->iRef < 2 ){` |
|        - |  5111 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5112 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5113 | `								 */` |
|        3 |  5114 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5115 | `							}else{` |
|        - |  5116 | `								/* Simple load */` |
|     3494 |  5117 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5118 | `							}` |
|     3496 |  5119 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3494 |  5120 | `								if( pThis->iRef > 1 ){` |
|        - |  5121 | `									/* Load attribute index */` |
|     3492 |  5122 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1745 |  5123 | `								}` |
|     1746 |  5124 | `							}` |
|     1747 |  5125 | `						}` |
|     1747 |  5126 | `					}` |
|     1747 |  5127 | `				}` |
|        - |  5128 | `				/* Safely unreference the object */` |
|     3498 |  5129 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5130 | `			}` |
|     1811 |  5131 | `		}else{` |
|      ! 0 |  5132 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5133 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5134 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5135 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5136 | `		}` |
|     1811 |  5137 | `	}else{` |
|        - |  5138 | `		/* Static member access using class name */` |
|       59 |  5139 | `		pNos = pTos;` |
|       59 |  5140 | `		pThis = 0;` |
|       59 |  5141 | `		if( !pInstr->p3 ){` |
|       57 |  5142 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       57 |  5143 | `			pNos--;` |
|        - |  5144 | `#ifdef UNTRUST` |
|        - |  5145 | `			if( pNos < pStack ){` |
|        - |  5146 | `				goto Abort;` |
|        - |  5147 | `			}` |
|        - |  5148 | `#endif` |
|       29 |  5149 | `		}else{` |
|        - |  5150 | `			/* Attribute name already computed */` |
|        3 |  5151 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5152 | `		}` |
|       59 |  5153 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|       59 |  5154 | `			ph7_class *pClass = 0;` |
|       59 |  5155 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5156 | `				/* Class already instantiated */` |
|      ! 0 |  5157 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5158 | `				pClass = pThis->pClass;` |
|      ! 0 |  5159 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5160 | `			}else{` |
|        - |  5161 | `				/* Try to extract the target class */` |
|       59 |  5162 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|       88 |  5163 | `					pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pNos->sBlob),` |
|       29 |  5164 | `						SyBlobLength(&pNos->sBlob),FALSE,0);` |
|       29 |  5165 | `				}` |
|        - |  5166 | `			}` |
|       59 |  5167 | `			if( pClass == 0 ){` |
|        - |  5168 | `				/* Undefined class */` |
|      ! 0 |  5169 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5170 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5171 | `					);` |
|      ! 0 |  5172 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5173 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5174 | `				}` |
|      ! 0 |  5175 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5176 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5177 | `			}else{` |
|       59 |  5178 | `				if( pInstr->iP2 ){` |
|        - |  5179 | `					/* Method call */` |
|       25 |  5180 | `					ph7_class_method *pMeth = 0;` |
|       25 |  5181 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5182 | `						/* Extract the target method */` |
|       25 |  5183 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       12 |  5184 | `					}` |
|       25 |  5185 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5186 | `						if( pMeth ){` |
|      ! 0 |  5187 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5188 | `								&pClass->sName,&sName` |
|        - |  5189 | `								);` |
|      ! 0 |  5190 | `						}else{` |
|      ! 0 |  5191 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5192 | `								&pClass->sName,&sName` |
|        - |  5193 | `								);` |
|        - |  5194 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5195 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5196 | `						}` |
|        - |  5197 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5198 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5199 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5200 | `						}` |
|      ! 0 |  5201 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5202 | `					}else{` |
|        - |  5203 | `						/* Push method name on the stack */` |
|       25 |  5204 | `						PH7_MemObjRelease(pTos);` |
|       25 |  5205 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       25 |  5206 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5207 | `					}` |
|       25 |  5208 | `					pTos->nIdx = SXU32_HIGH;` |
|       13 |  5209 | `				}else{` |
|        - |  5210 | `					/* Attribute access */` |
|       35 |  5211 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5212 | `					/* Check for special ::class pseudo-constant */` |
|       49 |  5213 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       28 |  5214 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5215 | `						/* ::class returns the fully qualified class name */` |
|        - |  5216 | `						/* Pop the attribute name from the stack */` |
|       27 |  5217 | `						if( !pInstr->p3 ){` |
|       27 |  5218 | `							VmPopOperand(&pTos,1);` |
|       13 |  5219 | `						}` |
|       27 |  5220 | `						PH7_MemObjRelease(pTos);` |
|        - |  5221 | `						/* Load the class name */` |
|       27 |  5222 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       27 |  5223 | `						pTos->nIdx = SXU32_HIGH;` |
|       14 |  5224 | `					}else{` |
|        - |  5225 | `						/* Extract the target attribute */` |
|        9 |  5226 | `						if( sName.nByte > 0 ){` |
|        9 |  5227 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        4 |  5228 | `						}` |
|        9 |  5229 | `						if( pAttr == 0 ){` |
|        - |  5230 | `							/* No such attribute,load null */` |
|      ! 0 |  5231 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5232 | `								&pClass->sName,&sName);` |
|        - |  5233 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5234 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5235 | `						}` |
|        - |  5236 | `						/* Pop the attribute name from the stack */` |
|        9 |  5237 | `						if( !pInstr->p3 ){` |
|        7 |  5238 | `							VmPopOperand(&pTos,1);` |
|        3 |  5239 | `						}` |
|        9 |  5240 | `						PH7_MemObjRelease(pTos);` |
|        9 |  5241 | `						pTos->nIdx = SXU32_HIGH;` |
|        9 |  5242 | `						if( pAttr ){` |
|        9 |  5243 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5244 | `								/* Access to a non static attribute */` |
|      ! 0 |  5245 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5246 | `									&pClass->sName,&pAttr->sName` |
|        - |  5247 | `									);` |
|      ! 0 |  5248 | `							}else{` |
|        - |  5249 | `								ph7_value *pValue;` |
|        - |  5250 | `								/* Check if the access to the attribute is allowed */` |
|        9 |  5251 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5252 | `									/* Load the desired attribute */` |
|        9 |  5253 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|        9 |  5254 | `									if( pValue ){` |
|        9 |  5255 | `										PH7_MemObjLoad(pValue,pTos);` |
|        9 |  5256 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5257 | `											/* Load index number */` |
|        3 |  5258 | `											pTos->nIdx = pAttr->nIdx;` |
|        1 |  5259 | `										}` |
|        4 |  5260 | `									}` |
|        4 |  5261 | `								}` |
|        - |  5262 | `							}` |
|        4 |  5263 | `						}` |
|        - |  5264 | `					}` |
|        - |  5265 | `				}` |
|       59 |  5266 | `				if( pThis ){` |
|        - |  5267 | `					/* Safely unreference the object */` |
|      ! 0 |  5268 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5269 | `				}` |
|        - |  5270 | `			}` |
|       30 |  5271 | `		}else{` |
|        - |  5272 | `			/* Pop operands */` |
|      ! 0 |  5273 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5274 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5275 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5276 | `			}` |
|      ! 0 |  5277 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5278 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5279 | `		}` |
|        - |  5280 | `	}` |
|     3678 |  5281 | `	break;` |
|        - |  5282 | `					}` |
|        - |  5283 | `/*` |
|        - |  5284 | ` * OP_NEW P1 * * *` |
|        - |  5285 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5286 | ` */` |
|      257 |  5287 | `case PH7_OP_NEW: {` |
|      516 |  5288 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      516 |  5289 | `	ph7_class *pClass = 0;` |
|        - |  5290 | `	ph7_class_instance *pNew;` |
|      516 |  5291 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5292 | `		/* Try to extract the desired class */` |
|      773 |  5293 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      514 |  5294 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      257 |  5295 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5296 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5297 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5298 | `	}` |
|      516 |  5299 | `	if( pClass == 0 ){` |
|        - |  5300 | `		/* No such class */` |
|      ! 0 |  5301 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined,PH7 is loading NULL",` |
|      ! 0 |  5302 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5303 | `			);` |
|      ! 0 |  5304 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5305 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5306 | `			/* Pop given arguments */` |
|      ! 0 |  5307 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5308 | `		}` |
|      ! 0 |  5309 | `	}else{` |
|        - |  5310 | `		ph7_class_method *pCons;` |
|        - |  5311 | `		/* Create a new class instance */` |
|      516 |  5312 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      516 |  5313 | `		if( pNew == 0 ){` |
|      ! 0 |  5314 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5315 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5316 | `				&pClass->sName` |
|        - |  5317 | `			);` |
|      ! 0 |  5318 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5319 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5320 | `				/* Pop given arguments */` |
|      ! 0 |  5321 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5322 | `			}` |
|      ! 0 |  5323 | `			break;` |
|        - |  5324 | `		}` |
|        - |  5325 | `		/* Check if a constructor is available */` |
|      516 |  5326 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      516 |  5327 | `		if( pCons == 0 ){` |
|      458 |  5328 | `			SyString *pName = &pClass->sName;` |
|        - |  5329 | `			/* Check for a constructor with the same base class name */` |
|      458 |  5330 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      228 |  5331 | `		}` |
|      516 |  5332 | `		if( pCons ){` |
|        - |  5333 | `			/* Call the class constructor */` |
|       60 |  5334 | `			SySetReset(&aArg);` |
|      108 |  5335 | `			while( pArg < pTos ){` |
|       50 |  5336 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       50 |  5337 | `				pArg++;` |
|        2 |  5338 | `			}` |
|       60 |  5339 | `			if( pVm->bErrReport ){` |
|        - |  5340 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5341 | `				sxu32 n;` |
|       17 |  5342 | `				n = SySetUsed(&aArg);` |
|        - |  5343 | `				/* Emit a notice for missing arguments */` |
|       45 |  5344 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       29 |  5345 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       29 |  5346 | `					if( pFuncArg ){` |
|       29 |  5347 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5348 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5349 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5350 | `						}` |
|       14 |  5351 | `					}` |
|       29 |  5352 | `					n++;` |
|        1 |  5353 | `				}` |
|        8 |  5354 | `			}` |
|       60 |  5355 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5356 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       60 |  5357 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5358 | `				pNew->iRef = 1;` |
|      ! 0 |  5359 | `			}` |
|       29 |  5360 | `		}` |
|      516 |  5361 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5362 | `			/* Pop given arguments */` |
|       44 |  5363 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       21 |  5364 | `		}` |
|      516 |  5365 | `		PH7_MemObjRelease(pTos);` |
|      516 |  5366 | `		pTos->x.pOther = pNew;` |
|      516 |  5367 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5368 | `	}` |
|      516 |  5369 | `	break;` |
|        - |  5370 | `				 }` |
|        - |  5371 | `/*` |
|        - |  5372 | ` * OP_CLONE * * *` |
|        - |  5373 | ` * Perfome a clone operation.` |
|        - |  5374 | ` */` |
|       23 |  5375 | `case PH7_OP_CLONE: {` |
|        - |  5376 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5377 | `#ifdef UNTRUST` |
|        - |  5378 | `	if( pTos < pStack ){` |
|        - |  5379 | `		goto Abort;` |
|        - |  5380 | `	}` |
|        - |  5381 | `#endif` |
|        - |  5382 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5383 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5384 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5385 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5386 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5387 | `		break;` |
|        - |  5388 | `	}` |
|        - |  5389 | `	/* Point to the source */` |
|       44 |  5390 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5391 | `	/* Perform the clone operation */` |
|       44 |  5392 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5393 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5394 | `	if( pClone == 0 ){` |
|      ! 0 |  5395 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5396 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5397 | `	}else{` |
|        - |  5398 | `		/* Load the cloned object */` |
|       44 |  5399 | `		pTos->x.pOther = pClone;` |
|       44 |  5400 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5401 | `	}` |
|       44 |  5402 | `	break;` |
|        - |  5403 | `				   }` |
|        - |  5404 | `/*` |
|        - |  5405 | ` * OP_SWITCH * * P3` |
|        - |  5406 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5407 | ` */` |
|       18 |  5408 | `case PH7_OP_SWITCH: {` |
|       38 |  5409 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5410 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5411 | `	ph7_value sValue,sCaseValue;` |
|        - |  5412 | `	sxu32 n,nEntry;` |
|        - |  5413 | `#ifdef UNTRUST` |
|        - |  5414 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5415 | `		goto Abort;` |
|        - |  5416 | `	}` |
|        - |  5417 | `#endif` |
|        - |  5418 | `	/* Point to the case table  */` |
|       38 |  5419 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5420 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5421 | `	/* Select the appropriate case block to execute */` |
|       38 |  5422 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5423 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5424 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5425 | `		pCase = &aCase[n];` |
|       92 |  5426 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5427 | `		/* Execute the case expression first */` |
|       92 |  5428 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5429 | `		/* Compare the two expression */` |
|       92 |  5430 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5431 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5432 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5433 | `		if( rc == 0 ){` |
|        - |  5434 | `			/* Value match,jump to this block */` |
|       38 |  5435 | `			pc = pCase->nStart - 1;` |
|       38 |  5436 | `			break;` |
|        - |  5437 | `		}` |
|       29 |  5438 | `	}` |
|       38 |  5439 | `	VmPopOperand(&pTos,1);` |
|       38 |  5440 | `	if( n >= nEntry ){` |
|        - |  5441 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5442 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5443 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5444 | `		}else{` |
|        - |  5445 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5446 | `			pc = pSwitch->nOut - 1;` |
|        - |  5447 | `		}` |
|      ! 0 |  5448 | `	}` |
|       38 |  5449 | `	break;` |
|        - |  5450 | `					}` |
|        - |  5451 | `/*` |
|        - |  5452 | ` * OP_CALL P1 * *` |
|        - |  5453 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5454 | ` *  function on the stack.` |
|        - |  5455 | ` */` |
|   285511 |  5456 | `case PH7_OP_CALL: {` |
|   571068 |  5457 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5458 | `	SyHashEntry *pEntry;` |
|        - |  5459 | `	SyString sName;` |
|        - |  5460 | `	/* Extract function name */` |
|   571068 |  5461 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5462 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5463 | `			ph7_value sResult;` |
|      ! 0 |  5464 | `			SySetReset(&aArg);` |
|      ! 0 |  5465 | `			while( pArg < pTos ){` |
|      ! 0 |  5466 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5467 | `				pArg++;` |
|      ! 0 |  5468 | `			}` |
|      ! 0 |  5469 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5470 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5471 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5472 | `			SySetReset(&aArg);` |
|        - |  5473 | `			/* Pop given arguments */` |
|      ! 0 |  5474 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5475 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5476 | `			}` |
|        - |  5477 | `			/* Copy result */` |
|      ! 0 |  5478 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5479 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5480 | `		}else{` |
|        3 |  5481 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5482 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5483 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5484 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5485 | `			}else{` |
|        - |  5486 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5487 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5488 | `			}` |
|        - |  5489 | `			/* Pop given arguments */` |
|        3 |  5490 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5491 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5492 | `			}` |
|        - |  5493 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5494 | `			PH7_MemObjRelease(pTos);` |
|        - |  5495 | `		}` |
|   285278 |  5496 | `		break;` |
|        - |  5497 | `	}` |
|   571066 |  5498 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5499 | `	/* Check for a compiled function first */` |
|   571066 |  5500 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|   571066 |  5501 | `	if( pEntry ){` |
|        - |  5502 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5503 | `		ph7_class_instance *pThis;` |
|        - |  5504 | `		ph7_value *pFrameStack;` |
|        - |  5505 | `		ph7_vm_func *pVmFunc;` |
|        - |  5506 | `		ph7_class *pSelf;` |
|        - |  5507 | `		VmFrame *pFrame;` |
|        - |  5508 | `		ph7_value *pObj;` |
|        - |  5509 | `		VmSlot sArg;` |
|        - |  5510 | `		sxu32 n;` |
|        - |  5511 | `		/* initialize fields */` |
|    11666 |  5512 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    11666 |  5513 | `		pThis = 0;` |
|    11666 |  5514 | `		pSelf = 0;` |
|    11666 |  5515 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5516 | `			ph7_class_method *pMeth;` |
|        - |  5517 | `			/* Class method call */` |
|     1294 |  5518 | `			ph7_value *pTarget = &pTos[-1];` |
|     1294 |  5519 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5520 | `				/* Extract the 'this' pointer */` |
|     1294 |  5521 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5522 | `					/* Instance already loaded */` |
|     1264 |  5523 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1264 |  5524 | `					pThis->iRef++;` |
|     1264 |  5525 | `					pSelf = pThis->pClass;` |
|      631 |  5526 | `				}` |
|     1294 |  5527 | `				if( pSelf == 0 ){` |
|       31 |  5528 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5529 | `						/* "Late Static Binding" class name */` |
|       37 |  5530 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       12 |  5531 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       12 |  5532 | `					}` |
|       31 |  5533 | `					if( pSelf == 0 ){` |
|        7 |  5534 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        3 |  5535 | `					}` |
|       15 |  5536 | `				}` |
|     1294 |  5537 | `				if( pThis == 0  ){` |
|       31 |  5538 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       31 |  5539 | `					while( pFrameLocal->pParent && (pFrameLocal->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  5540 | `						/* Safely ignore the exception frame */` |
|      ! 0 |  5541 | `						pFrameLocal = pFrameLocal->pParent;` |
|      ! 0 |  5542 | `					}` |
|       31 |  5543 | `					if( pFrameLocal->pParent ){` |
|        - |  5544 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5545 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5546 | `						if( pThis ){` |
|       13 |  5547 | `							pThis->iRef++;` |
|        6 |  5548 | `						}` |
|        9 |  5549 | `					}` |
|       15 |  5550 | `				}` |
|     1294 |  5551 | `				VmPopOperand(&pTos,1);` |
|     1294 |  5552 | `				PH7_MemObjRelease(pTos);` |
|        - |  5553 | `				/* Synchronize pointers */` |
|     1294 |  5554 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5555 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5556 | `				 * user have already computed the random generated unique class method name` |
|        - |  5557 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5558 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5559 | `				 */` |
|     1294 |  5560 | `				while( pArg < pStack ){` |
|      ! 0 |  5561 | `					pArg++;` |
|      ! 0 |  5562 | `				}` |
|     1294 |  5563 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5564 | `					/* Check if the call is allowed */` |
|     1294 |  5565 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1294 |  5566 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        5 |  5567 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5568 | `							/* Pop given arguments */` |
|      ! 0 |  5569 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5570 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5571 | `							}` |
|        - |  5572 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5573 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5574 | `							break;` |
|        - |  5575 | `						}` |
|        2 |  5576 | `					}` |
|      646 |  5577 | `				}` |
|      646 |  5578 | `			}` |
|      646 |  5579 | `		}` |
|        - |  5580 | `		/* Check The recursion limit */` |
|    11666 |  5581 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5582 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5583 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5584 | `				&pVmFunc->sName);` |
|        - |  5585 | `			/* Pop given arguments */` |
|        3 |  5586 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5587 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5588 | `			}` |
|        - |  5589 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5590 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5591 | `			break;` |
|        - |  5592 | `		}` |
|    11664 |  5593 | `		if( pVmFunc->pNextName ){` |
|        - |  5594 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      123 |  5595 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       61 |  5596 | `		}` |
|        - |  5597 | `		/* Extract the formal argument set */` |
|    11664 |  5598 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5599 | `		/* Create a new VM frame  */` |
|    11664 |  5600 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    11664 |  5601 | `		if( rc != SXRET_OK ){` |
|        - |  5602 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5603 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5604 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5605 | `				&pVmFunc->sName);` |
|        - |  5606 | `			/* Pop given arguments */` |
|      ! 0 |  5607 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5608 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5609 | `			}` |
|        - |  5610 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5611 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5612 | `			break;` |
|        - |  5613 | `		}` |
|    11664 |  5614 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5615 | `			/* Install the '$this' variable */` |
|        - |  5616 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1274 |  5617 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1274 |  5618 | `			if( pObj ){` |
|        - |  5619 | `				/* Reflect the change */` |
|     1274 |  5620 | `				pObj->x.pOther = pThis;` |
|     1274 |  5621 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      636 |  5622 | `			}` |
|      636 |  5623 | `		}` |
|    11664 |  5624 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5625 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5626 | `			/* Install static variables */` |
|      ! 0 |  5627 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5628 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5629 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5630 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5631 | `					/* Initialize the static variables */` |
|      ! 0 |  5632 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5633 | `					if( pObj ){` |
|        - |  5634 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5635 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5636 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5637 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5638 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5639 | `						}` |
|      ! 0 |  5640 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5641 | `					}else{` |
|      ! 0 |  5642 | `						continue;` |
|        - |  5643 | `					}` |
|      ! 0 |  5644 | `				}` |
|        - |  5645 | `				/* Install in the current frame */` |
|      ! 0 |  5646 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5647 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5648 | `			}` |
|      ! 0 |  5649 | `		}` |
|        - |  5650 | `		/* Push arguments in the local frame */` |
|    11664 |  5651 | `		n = 0;` |
|    32646 |  5652 | `		while( pArg < pTos ){` |
|    20984 |  5653 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    20834 |  5654 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5655 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5656 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5657 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5658 | `						goto Abort;` |
|        - |  5659 | `					}` |
|      ! 0 |  5660 | `				}` |
|        - |  5661 | `				/* Make sure the given arguments are of the correct type */` |
|    20834 |  5662 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5663 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5664 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5665 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5666 | `						ph7_class *pClass;` |
|        - |  5667 | `						/* Try to extract the desired class */` |
|      ! 0 |  5668 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5669 | `						if( pClass ){` |
|      ! 0 |  5670 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5671 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5672 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5673 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5674 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5675 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5676 | `								}` |
|      ! 0 |  5677 | `							}else{` |
|        - |  5678 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5679 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5680 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5681 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5682 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5683 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5684 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5685 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5686 | `								}` |
|        - |  5687 | `							}` |
|      ! 0 |  5688 | `						}` |
|     1088 |  5689 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5690 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5691 | `						/* Cast to the desired type */` |
|      ! 0 |  5692 | `						xCast(pArg);` |
|      ! 0 |  5693 | `					}` |
|      543 |  5694 | `				}` |
|    20834 |  5695 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5696 | `					/* Pass by reference */` |
|       48 |  5697 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5698 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5699 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5700 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5701 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5702 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5703 | `						}` |
|        - |  5704 | `						/* Switch to pass by value */` |
|      ! 0 |  5705 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5706 | `					}else{` |
|        - |  5707 | `						SyHashEntry *pRefEntry;` |
|        - |  5708 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5709 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5710 | `						if( pRefEntry == 0 ){` |
|       71 |  5711 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5712 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5713 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5714 | `							sArg.pUserData = 0;` |
|       48 |  5715 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5716 | `						}` |
|       48 |  5717 | `						pObj = 0;` |
|        - |  5718 | `					}` |
|       25 |  5719 | `				}else{` |
|        - |  5720 | `					/* Pass by value,make a copy of the given argument */` |
|    20788 |  5721 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5722 | `				}` |
|    10418 |  5723 | `			}else{` |
|        - |  5724 | `				char zName[32];` |
|        - |  5725 | `				SyString sArgName;` |
|        - |  5726 | `				/* Set a dummy name */` |
|      152 |  5727 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  5728 | `				sArgName.zString = zName;` |
|        - |  5729 | `				/* Annonymous argument */` |
|      152 |  5730 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  5731 | `			}` |
|    20984 |  5732 | `			if( pObj ){` |
|    20938 |  5733 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  5734 | `				/* Insert argument index  */` |
|    20938 |  5735 | `				sArg.nIdx = pObj->nIdx;` |
|    20938 |  5736 | `				sArg.pUserData = 0;` |
|    20938 |  5737 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10468 |  5738 | `			}` |
|    20984 |  5739 | `			PH7_MemObjRelease(pArg);` |
|    20984 |  5740 | `			pArg++;` |
|    20984 |  5741 | `			++n;` |
|        2 |  5742 | `		}` |
|        - |  5743 | `		/* Set up closure environment */` |
|    11664 |  5744 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5745 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  5746 | `			ph7_value *pValue;` |
|        - |  5747 | `			sxu32 iEnv;` |
|        9 |  5748 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  5749 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  5750 | `				pEnv = &aEnv[iEnv];` |
|       17 |  5751 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  5752 | `					/* Do not install null value */` |
|        9 |  5753 | `					continue;` |
|        - |  5754 | `				}` |
|        9 |  5755 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  5756 | `				if( pValue == 0 ){` |
|      ! 0 |  5757 | `					continue;` |
|        - |  5758 | `				}` |
|        - |  5759 | `				/* Invalidate any prior representation */` |
|        9 |  5760 | `				PH7_MemObjRelease(pValue);` |
|        - |  5761 | `				/* Duplicate bound variable value */` |
|        9 |  5762 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  5763 | `			}` |
|        4 |  5764 | `		}` |
|        - |  5765 | `		/* Process default values */` |
|    13502 |  5766 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1840 |  5767 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1834 |  5768 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1834 |  5769 | `				if( pObj ){` |
|        - |  5770 | `					/* Evaluate the default value and extract it's result */` |
|     1834 |  5771 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1834 |  5772 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5773 | `						goto Abort;` |
|        - |  5774 | `					}` |
|        - |  5775 | `					/* Insert argument index */` |
|     1834 |  5776 | `					sArg.nIdx = pObj->nIdx;` |
|     1834 |  5777 | `					sArg.pUserData = 0;` |
|     1834 |  5778 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  5779 | `					/* Make sure the default argument is of the correct type */` |
|     1834 |  5780 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5781 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5782 | `						/* Cast to the desired type */` |
|      ! 0 |  5783 | `						xCast(pObj);` |
|      ! 0 |  5784 | `					}` |
|      916 |  5785 | `				}` |
|      916 |  5786 | `			}` |
|     1840 |  5787 | `			++n;` |
|        2 |  5788 | `		}` |
|        - |  5789 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  5790 | `		 * does not return anything.` |
|        - |  5791 | `		 */` |
|    11664 |  5792 | `		PH7_MemObjRelease(pTos);` |
|    11664 |  5793 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  5794 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    11664 |  5795 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    11664 |  5796 | `		if( pFrameStack == 0 ){` |
|        - |  5797 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5798 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5799 | `				&pVmFunc->sName);` |
|      ! 0 |  5800 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5801 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5802 | `			}` |
|      ! 0 |  5803 | `			break;` |
|        - |  5804 | `		}` |
|    11664 |  5805 | `		if( pSelf ){` |
|        - |  5806 | `			/* Push class name */` |
|     1292 |  5807 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      645 |  5808 | `		}` |
|        - |  5809 | `		/* Increment nesting level */` |
|    11664 |  5810 | `		pVm->nRecursionDepth++;` |
|        - |  5811 | `		/* Execute function body */` |
|    11664 |  5812 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  5813 | `		/* Decrement nesting level */` |
|    11664 |  5814 | `		pVm->nRecursionDepth--;` |
|    11664 |  5815 | `		if( pSelf ){` |
|        - |  5816 | `			/* Pop class name */` |
|     1292 |  5817 | `			(void)SySetPop(&pVm->aSelf);` |
|      645 |  5818 | `		}` |
|        - |  5819 | `		/* Cleanup the mess left behind */` |
|    11664 |  5820 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  5821 | `			/* Return by reference,reflect that */` |
|        9 |  5822 | `			if( n != SXU32_HIGH ){` |
|        9 |  5823 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  5824 | `				sxu32 i;` |
|        - |  5825 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  5826 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  5827 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  5828 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  5829 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5830 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5831 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  5832 | `								&pVmFunc->sName);` |
|      ! 0 |  5833 | `						}` |
|      ! 0 |  5834 | `						n = SXU32_HIGH;` |
|      ! 0 |  5835 | `						break;` |
|        - |  5836 | `					}` |
|        3 |  5837 | `				}` |
|        5 |  5838 | `			}else{` |
|      ! 0 |  5839 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  5840 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  5841 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  5842 | `						&pVmFunc->sName);` |
|      ! 0 |  5843 | `				}` |
|        - |  5844 | `			}` |
|        9 |  5845 | `			pTos->nIdx = n;` |
|        4 |  5846 | `		}` |
|        - |  5847 | `		/* Cleanup the mess left behind */` |
|    11664 |  5848 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  5849 | `			/* An exception was throw in this frame */` |
|        7 |  5850 | `			pFrame = pFrame->pParent;` |
|        7 |  5851 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  5852 | `				/* Pop the resutlt */` |
|        5 |  5853 | `				VmPopOperand(&pTos,1);` |
|        - |  5854 | `				/* Jump to this destination */` |
|        5 |  5855 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  5856 | `				rc = PH7_OK;` |
|        3 |  5857 | `			}else{` |
|        3 |  5858 | `				if( pFrame->pParent ){` |
|        3 |  5859 | `					rc = PH7_EXCEPTION;` |
|        2 |  5860 | `				}else{` |
|        - |  5861 | `					/* Continue normal execution */` |
|      ! 0 |  5862 | `					rc = PH7_OK;` |
|        - |  5863 | `				}` |
|        - |  5864 | `			}` |
|        3 |  5865 | `		}` |
|        - |  5866 | `		/* Free the operand stack */` |
|    11664 |  5867 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  5868 | `		/* Leave the frame */` |
|    11664 |  5869 | `		VmLeaveFrame(&(*pVm));` |
|    11664 |  5870 | `		if( rc == PH7_ABORT ){` |
|        - |  5871 | `			/* Abort processing immeditaley */` |
|        7 |  5872 | `			goto Abort;` |
|    11658 |  5873 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  5874 | `			goto Exception;` |
|        - |  5875 | `		}` |
|     5829 |  5876 | `	}else{` |
|        - |  5877 | `		ph7_user_func *pFunc;` |
|        - |  5878 | `		ph7_context sCtx;` |
|        - |  5879 | `		ph7_value sRet;` |
|        - |  5880 | `		/* Look for an installed foreign function */` |
|   559402 |  5881 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   559402 |  5882 | `		if( pEntry == 0 ){` |
|        - |  5883 | `			/* Call to undefined function */` |
|        5 |  5884 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  5885 | `			/* Pop given arguments */` |
|        5 |  5886 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5887 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5888 | `			}` |
|        - |  5889 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  5890 | `			PH7_MemObjRelease(pTos);` |
|        8 |  5891 | `			break;` |
|        - |  5892 | `		}` |
|   559398 |  5893 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  5894 | `		/* Start collecting function arguments */` |
|   559398 |  5895 | `		SySetReset(&aArg);` |
|  1483360 |  5896 | `		while( pArg < pTos ){` |
|   923964 |  5897 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   923964 |  5898 | `			pArg++;` |
|        2 |  5899 | `		}` |
|        - |  5900 | `		/* Assume a null return value */` |
|   559398 |  5901 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  5902 | `		/* Init the call context */` |
|   559398 |  5903 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  5904 | `		/* Call the foreign function */` |
|   559398 |  5905 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5906 | `		/* Release the call context */` |
|   559398 |  5907 | `		VmReleaseCallContext(&sCtx);` |
|   559398 |  5908 | `		if( rc == PH7_ABORT ){` |
|      463 |  5909 | `			goto Abort;` |
|   558936 |  5910 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  5911 | `			VmFrame *pFrm = pVm->pFrame;` |
|       13 |  5912 | `			while( pFrm->pParent && (pFrm->iFlags & VM_FRAME_EXCEPTION) ){` |
|        7 |  5913 | `				pFrm = pFrm->pParent;` |
|        1 |  5914 | `			}` |
|        7 |  5915 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  5916 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  5917 | `				goto Exception;` |
|        - |  5918 | `			}` |
|        - |  5919 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  5920 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  5921 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  5922 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  5923 | `			}` |
|        - |  5924 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  5925 | `			VmPopOperand(&pTos,1);` |
|        - |  5926 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  5927 | `			pFrm = pVm->pFrame;` |
|        7 |  5928 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  5929 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  5930 | `			}` |
|        7 |  5931 | `			break;` |
|        - |  5932 | `		}` |
|   558930 |  5933 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5934 | `			/* Pop function name and arguments */` |
|   541654 |  5935 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   270848 |  5936 | `		}` |
|        - |  5937 | `		/* Save foreign function return value */` |
|   558930 |  5938 | `		PH7_MemObjStore(&sRet,pTos);` |
|   558930 |  5939 | `		PH7_MemObjRelease(&sRet);` |
|        - |  5940 | `	}` |
|   570584 |  5941 | `	break;` |
|        - |  5942 | `				  }` |
|        - |  5943 | `/*` |
|        - |  5944 | ` * OP_CONSUME: P1 * *` |
|        - |  5945 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  5946 | ` */` |
|    10281 |  5947 | `case PH7_OP_CONSUME: {` |
|    20564 |  5948 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    20564 |  5949 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  5950 |  |
|    20564 |  5951 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    20564 |  5952 | `	pCur = pOut;` |
|        - |  5953 | `	/* Start the consume process  */` |
|    41126 |  5954 | `	while( pOut <= pTos ){` |
|        - |  5955 | `		/* Force a string cast */` |
|    20564 |  5956 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      172 |  5957 | `			PH7_MemObjToString(pOut);` |
|       85 |  5958 | `		}` |
|    20564 |  5959 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  5960 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  5961 | `			/* Invoke the output consumer callback */` |
|    10998 |  5962 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    10998 |  5963 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  5964 | `				/* Increment output length */` |
|     4580 |  5965 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2289 |  5966 | `			}` |
|    10998 |  5967 | `			SyBlobRelease(&pOut->sBlob);` |
|    10998 |  5968 | `			if( rc == SXERR_ABORT ){` |
|        - |  5969 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  5970 | `				goto Abort;` |
|        - |  5971 | `			}` |
|     5498 |  5972 | `		}` |
|    20564 |  5973 | `		pOut++;` |
|        2 |  5974 | `	}` |
|    20564 |  5975 | `	pTos = &pCur[-1];` |
|    20562 |  5976 | `	break;` |
|        - |  5977 | `					 }` |
|        - |  5978 |  |
|        - |  5979 | `		} /* Switch() */` |
| 10161654 |  5980 | `		pc++; /* Next instruction in the stream */` |
|        2 |  5981 | `	} /* For(;;) */` |
|    14489 |  5982 | `Done:` |
|    28980 |  5983 | `	SySetRelease(&aArg);` |
|    28980 |  5984 | `	return SXRET_OK;` |
|      238 |  5985 | `Abort:` |
|      477 |  5986 | `	SySetRelease(&aArg);` |
|     1661 |  5987 | `	while( pTos >= pStack ){` |
|     1185 |  5988 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  5989 | `		pTos--;` |
|        1 |  5990 | `	}` |
|      477 |  5991 | `	return PH7_ABORT;` |
|        1 |  5992 | `Exception:` |
|        3 |  5993 | `	SySetRelease(&aArg);` |
|        5 |  5994 | `	while( pTos >= pStack ){` |
|        3 |  5995 | `		PH7_MemObjRelease(pTos);` |
|        3 |  5996 | `		pTos--;` |
|        1 |  5997 | `	}` |
|        3 |  5998 | `	return PH7_EXCEPTION;` |
|    14730 |  5999 |  |
|        - |  6000 | `/*` |
|        - |  6001 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6002 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6003 | ` * See block-comment on that function for additional information.` |
|        - |  6004 | ` */` |
|    14180 |  6005 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6006 |  |
|        - |  6007 | `	ph7_value *pStack;` |
|        - |  6008 | `	sxi32 rc;` |
|        - |  6009 | `	/* Allocate a new operand stack */` |
|    14182 |  6010 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14182 |  6011 | `	if( pStack == 0 ){` |
|      ! 0 |  6012 | `		return SXERR_MEM;` |
|        - |  6013 | `	}` |
|        - |  6014 | `	/* Execute the program */` |
|    14182 |  6015 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6016 | `	/* Free the operand stack */` |
|    14182 |  6017 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6018 | `	/* Execution result */` |
|    14182 |  6019 | `	return rc;` |
|     7092 |  6020 |  |
|        - |  6021 | `/*` |
|        - |  6022 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6023 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6024 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6025 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6026 | ` * execution ends.` |
|        - |  6027 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6028 | ` * additional information.` |
|        - |  6029 | ` */` |
|     2022 |  6030 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6031 |  |
|        - |  6032 | `	VmShutdownCB *pEntry;` |
|        - |  6033 | `	ph7_value *apArg[10];` |
|        - |  6034 | `	sxu32 n,nEntry;` |
|        - |  6035 | `	int i;` |
|        - |  6036 | `	/* Point to the stack of registered callbacks */` |
|     2024 |  6037 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    22244 |  6038 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    20222 |  6039 | `		apArg[i] = 0;` |
|    10112 |  6040 | `	}` |
|     2026 |  6041 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6042 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6043 | `		if( pEntry ){` |
|        - |  6044 | `			/* Prepare callback arguments if any */` |
|        3 |  6045 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6046 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6047 | `					break;` |
|        - |  6048 | `				}` |
|      ! 0 |  6049 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6050 | `			}` |
|        - |  6051 | `			/* Invoke the callback */` |
|        3 |  6052 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6053 | `			/*` |
|        - |  6054 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6055 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6056 | `			 */` |
|        3 |  6057 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6058 | `			if( pEntry ){` |
|        3 |  6059 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6060 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6061 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6062 | `				}` |
|        1 |  6063 | `			}` |
|        1 |  6064 | `		}` |
|        2 |  6065 | `	}` |
|     2024 |  6066 | `	SySetReset(&pVm->aShutdown);` |
|     2024 |  6067 |  |
|        - |  6068 | `/*` |
|        - |  6069 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6070 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6071 | ` * See block-comment on that function for additional information.` |
|        - |  6072 | ` */` |
|     2030 |  6073 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6074 |  |
|        - |  6075 | `	/* Make sure we are ready to execute this program */` |
|     2032 |  6076 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6077 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6078 | `	}` |
|        - |  6079 | `	/* Set the execution magic number  */` |
|     2032 |  6080 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6081 | `	/* Execute the program */` |
|     2032 |  6082 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6083 | `	/* Invoke any shutdown callbacks */` |
|     2028 |  6084 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6085 | `	/*` |
|        - |  6086 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6087 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6088 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6089 | `	 */` |
|     2028 |  6090 | `	return SXRET_OK;` |
|     1017 |  6091 |  |
|        - |  6092 | `/*` |
|        - |  6093 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6094 | ` * the desired message.` |
|        - |  6095 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6096 | ` * in 'api.c' for additional information.` |
|        - |  6097 | ` */` |
|      350 |  6098 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6099 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6100 | `	SyString *pString /* Message to output */` |
|        - |  6101 | `	)` |
|        2 |  6102 |  |
|      352 |  6103 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6104 | `	sxi32 rc = SXRET_OK;` |
|        - |  6105 | `	/* Call the output consumer */` |
|      352 |  6106 | `	if( pString->nByte > 0 ){` |
|      352 |  6107 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6108 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6109 | `			/* Increment output length */` |
|       17 |  6110 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6111 | `		}` |
|      175 |  6112 | `	}` |
|      352 |  6113 | `	return rc;` |
|        2 |  6114 |  |
|        - |  6115 | `/*` |
|        - |  6116 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6117 | ` * callback to consume the formatted message.` |
|        - |  6118 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6119 | ` * in 'api.c' for additional information.` |
|        - |  6120 | ` */` |
|        2 |  6121 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6122 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6123 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6124 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6125 | `	)` |
|        1 |  6126 |  |
|        3 |  6127 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6128 | `	sxi32 rc = SXRET_OK;` |
|        - |  6129 | `	SyBlob sWorker;` |
|        - |  6130 | `	/* Format the message and call the output consumer */` |
|        3 |  6131 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6132 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6133 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6134 | `		/* Consume the formatted message */` |
|        3 |  6135 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6136 | `	}` |
|        3 |  6137 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6138 | `		/* Increment output length */` |
|      ! 0 |  6139 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6140 | `	}` |
|        - |  6141 | `	/* Release the working buffer */` |
|        3 |  6142 | `	SyBlobRelease(&sWorker);` |
|        3 |  6143 | `	return rc;` |
|        1 |  6144 |  |
|        - |  6145 | `/*` |
|        - |  6146 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6147 | ` * This function never fail and always return a pointer` |
|        - |  6148 | ` * to a null terminated string.` |
|        - |  6149 | ` */` |
|       10 |  6150 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6151 |  |
|       11 |  6152 | `	const char *zOp = "Unknown     ";` |
|       11 |  6153 | `	switch(nOp){` |
|        3 |  6154 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6155 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6156 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6157 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6158 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6159 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6160 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6161 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6162 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6163 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6164 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6165 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6166 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6167 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6168 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6169 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6170 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6171 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6172 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6173 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6174 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6175 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6176 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6177 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6178 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6179 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6180 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6181 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6182 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6183 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6184 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6185 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6186 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6187 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6188 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6189 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6190 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6191 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6192 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6193 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6194 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6195 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6196 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6197 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6198 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6199 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6200 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6201 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6202 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6203 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6204 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6205 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6206 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6207 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6208 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6209 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6210 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6211 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6212 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6213 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6214 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6215 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6216 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6217 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6218 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6219 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6220 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6221 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6222 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6223 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6224 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6225 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6226 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6227 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6228 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6229 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6230 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6231 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6232 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6233 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6234 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6235 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6236 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6237 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6238 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6239 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6240 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6241 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6242 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6243 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6244 | `	default:` |
|      ! 0 |  6245 | `		break;` |
|        - |  6246 | `	}` |
|       11 |  6247 | `	return zOp;` |
|        1 |  6248 |  |
|        - |  6249 | `/*` |
|        - |  6250 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6251 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6252 | ` * is responsible of consuming the generated dump.` |
|        - |  6253 | ` */` |
|        2 |  6254 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6255 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6256 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6257 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6258 | `	)` |
|        1 |  6259 |  |
|        - |  6260 | `	sxi32 rc;` |
|        3 |  6261 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6262 | `	return rc;` |
|        1 |  6263 |  |
|        - |  6264 | `/*` |
|        - |  6265 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6266 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6267 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6268 | ` * in 'compile.c' for additional information.` |
|        - |  6269 | ` */` |
|        8 |  6270 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6271 |  |
|        9 |  6272 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6273 | `	/* Evaluate and expand constant value */` |
|        9 |  6274 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6275 |  |
|        - |  6276 | `/*` |
|        - |  6277 | ` * Section:` |
|        - |  6278 | ` *  Function handling functions.` |
|        - |  6279 | ` * Status:` |
|        - |  6280 | ` *    Stable.` |
|        - |  6281 | ` */` |
|        - |  6282 | `/*` |
|        - |  6283 | ` * int func_num_args(void)` |
|        - |  6284 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6285 | ` * Parameters` |
|        - |  6286 | ` *   None.` |
|        - |  6287 | ` * Return` |
|        - |  6288 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6289 | ` *  or -1 if called from the globe scope.` |
|        - |  6290 | ` */` |
|      906 |  6291 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6292 |  |
|        - |  6293 | `	VmFrame *pFrame;` |
|        - |  6294 | `	ph7_vm *pVm;` |
|        - |  6295 | `	/* Point to the target VM */` |
|      908 |  6296 | `	pVm = pCtx->pVm;` |
|        - |  6297 | `	/* Current frame */` |
|      908 |  6298 | `	pFrame = pVm->pFrame;` |
|      908 |  6299 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6300 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6301 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6302 | `	}` |
|      908 |  6303 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6304 | `		SXUNUSED(nArg);` |
|      ! 0 |  6305 | `		SXUNUSED(apArg);` |
|        - |  6306 | `		/* Global frame,return -1 */` |
|      ! 0 |  6307 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6308 | `		return SXRET_OK;` |
|        - |  6309 | `	}` |
|        - |  6310 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6311 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6312 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6313 | `	return SXRET_OK;` |
|      455 |  6314 |  |
|        - |  6315 | `/*` |
|        - |  6316 | ` * value func_get_arg(int $arg_num)` |
|        - |  6317 | ` *   Return an item from the argument list.` |
|        - |  6318 | ` * Parameters` |
|        - |  6319 | ` *  Argument number(index start from zero).` |
|        - |  6320 | ` * Return` |
|        - |  6321 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6322 | ` */` |
|       22 |  6323 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6324 |  |
|       24 |  6325 | `	ph7_value *pObj = 0;` |
|       24 |  6326 | `	VmSlot *pSlot = 0;` |
|        - |  6327 | `	VmFrame *pFrame;` |
|        - |  6328 | `	ph7_vm *pVm;` |
|        - |  6329 | `	/* Point to the target VM */` |
|       24 |  6330 | `	pVm = pCtx->pVm;` |
|        - |  6331 | `	/* Current frame */` |
|       24 |  6332 | `	pFrame = pVm->pFrame;` |
|       24 |  6333 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6334 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6335 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6336 | `	}` |
|       24 |  6337 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6338 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6339 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6340 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6341 | `		return SXRET_OK;` |
|        - |  6342 | `	}` |
|        - |  6343 | `	/* Extract the desired index */` |
|       21 |  6344 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6345 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6346 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6347 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6348 | `		return SXRET_OK;` |
|        - |  6349 | `	}` |
|        - |  6350 | `	/* Extract the desired argument */` |
|       21 |  6351 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6352 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6353 | `			/* Return the desired argument */` |
|       21 |  6354 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6355 | `		}else{` |
|        - |  6356 | `			/* No such argument,return false */` |
|      ! 0 |  6357 | `			ph7_result_bool(pCtx,0);` |
|        - |  6358 | `		}` |
|       11 |  6359 | `	}else{` |
|        - |  6360 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6361 | `		ph7_result_bool(pCtx,0);` |
|        - |  6362 | `	}` |
|       21 |  6363 | `	return SXRET_OK;` |
|       13 |  6364 |  |
|        - |  6365 | `/*` |
|        - |  6366 | ` * array func_get_args_byref(void)` |
|        - |  6367 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6368 | ` * Parameters` |
|        - |  6369 | ` *  None.` |
|        - |  6370 | ` * Return` |
|        - |  6371 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6372 | ` *  member of the current user-defined function's argument list.` |
|        - |  6373 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6374 | ` * NOTE:` |
|        - |  6375 | ` *  Arguments are returned to the array by reference.` |
|        - |  6376 | ` */` |
|        2 |  6377 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6378 |  |
|        - |  6379 | `	ph7_value *pArray;` |
|        - |  6380 | `	VmFrame *pFrame;` |
|        - |  6381 | `	VmSlot *aSlot;` |
|        - |  6382 | `	sxu32 n;` |
|        - |  6383 | `	/* Point to the current frame */` |
|        3 |  6384 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6385 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6386 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6387 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6388 | `	}` |
|        3 |  6389 | `	if( pFrame->pParent == 0 ){` |
|        - |  6390 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6391 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6392 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6393 | `		return SXRET_OK;` |
|        - |  6394 | `	}` |
|        - |  6395 | `	/* Create a new array */` |
|        3 |  6396 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6397 | `	if( pArray == 0 ){` |
|      ! 0 |  6398 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6399 | `		SXUNUSED(apArg);` |
|      ! 0 |  6400 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6401 | `		return SXRET_OK;` |
|        - |  6402 | `	}` |
|        - |  6403 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6404 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6405 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6406 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6407 | `	}` |
|        - |  6408 | `	/* Return the freshly created array */` |
|        3 |  6409 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6410 | `	return SXRET_OK;` |
|        2 |  6411 |  |
|        - |  6412 | `/*` |
|        - |  6413 | ` * array func_get_args(void)` |
|        - |  6414 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6415 | ` * Parameters` |
|        - |  6416 | ` *  None.` |
|        - |  6417 | ` * Return` |
|        - |  6418 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6419 | ` *  member of the current user-defined function's argument list.` |
|        - |  6420 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6421 | ` */` |
|       62 |  6422 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6423 |  |
|       64 |  6424 | `	ph7_value *pObj = 0;` |
|        - |  6425 | `	ph7_value *pArray;` |
|        - |  6426 | `	VmFrame *pFrame;` |
|        - |  6427 | `	VmSlot *aSlot;` |
|        - |  6428 | `	sxu32 n;` |
|        - |  6429 | `	/* Point to the current frame */` |
|       64 |  6430 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6431 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  6432 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  6433 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6434 | `	}` |
|       64 |  6435 | `	if( pFrame->pParent == 0 ){` |
|        - |  6436 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6437 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6438 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6439 | `		return SXRET_OK;` |
|        - |  6440 | `	}` |
|        - |  6441 | `	/* Create a new array */` |
|       64 |  6442 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6443 | `	if( pArray == 0 ){` |
|      ! 0 |  6444 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6445 | `		SXUNUSED(apArg);` |
|      ! 0 |  6446 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6447 | `		return SXRET_OK;` |
|        - |  6448 | `	}` |
|        - |  6449 | `	/* Start filling the array with the given arguments */` |
|       64 |  6450 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6451 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6452 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6453 | `		if( pObj ){` |
|      130 |  6454 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6455 | `		}` |
|       66 |  6456 | `	}` |
|        - |  6457 | `	/* Return the freshly created array */` |
|       64 |  6458 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6459 | `	return SXRET_OK;` |
|       33 |  6460 |  |
|        - |  6461 | `/*` |
|        - |  6462 | ` * bool function_exists(string $name)` |
|        - |  6463 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6464 | ` * Parameters` |
|        - |  6465 | ` *  The name of the desired function.` |
|        - |  6466 | ` * Return` |
|        - |  6467 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6468 | ` */` |
|     1648 |  6469 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6470 |  |
|        - |  6471 | `	const char *zName;` |
|        - |  6472 | `	ph7_vm *pVm;` |
|        - |  6473 | `	int nLen;` |
|        - |  6474 | `	int res;` |
|     1650 |  6475 | `	if( nArg < 1 ){` |
|        - |  6476 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6477 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6478 | `		return SXRET_OK;` |
|        - |  6479 | `	}` |
|        - |  6480 | `	/* Point to the target VM */` |
|     1650 |  6481 | `	pVm = pCtx->pVm;` |
|        - |  6482 | `	/* Extract the function name */` |
|     1650 |  6483 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6484 | `	/* Assume the function is not defined */` |
|     1650 |  6485 | `	res = 0;` |
|        - |  6486 | `	/* Perform the lookup */` |
|     2472 |  6487 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1644 |  6488 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6489 | `			/* Function is defined */` |
|      202 |  6490 | `			res = 1;` |
|      100 |  6491 | `	}` |
|     1650 |  6492 | `	ph7_result_bool(pCtx,res);` |
|     1650 |  6493 | `	return SXRET_OK;` |
|      826 |  6494 |  |
|        - |  6495 | `/*` |
|        - |  6496 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6497 | ` * [i.e: Whether it is callable or not].` |
|        - |  6498 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6499 | ` */` |
|    15970 |  6500 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6501 |  |
|    15972 |  6502 | `	int res = 0;` |
|    15972 |  6503 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6504 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6505 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6506 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6507 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6508 | `		if( pMethod && CallInvoke ){` |
|        - |  6509 | `			ph7_value sResult;` |
|        - |  6510 | `			sxi32 rc;` |
|        - |  6511 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6512 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6513 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6514 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6515 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6516 | `			}` |
|      ! 0 |  6517 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6518 | `		}` |
|    15972 |  6519 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6520 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6521 | `		if( pMap->nEntry == 2 ){` |
|        - |  6522 | `			ph7_class *pClass;` |
|        - |  6523 | `			ph7_value *pV;` |
|        - |  6524 | `			/* Extract the target class */` |
|       12 |  6525 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6526 | `			if( pV ){` |
|       12 |  6527 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6528 | `				if( pClass ){` |
|        - |  6529 | `					ph7_class_method *pMethod;` |
|        - |  6530 | `					/* Extract the target method */` |
|       10 |  6531 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6532 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6533 | `						/* Perform the lookup */` |
|       10 |  6534 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6535 | `						if( pMethod ){` |
|        - |  6536 | `							/* Method is callable */` |
|        5 |  6537 | `							res = 1;` |
|        2 |  6538 | `						}` |
|        4 |  6539 | `					}` |
|        4 |  6540 | `				}` |
|        5 |  6541 | `			}` |
|        7 |  6542 | `		}` |
|    15959 |  6543 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6544 | `		const char *zName;` |
|        - |  6545 | `		int nLen;` |
|        - |  6546 | `		/* Extract the name */` |
|     4698 |  6547 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6548 | `		/* Perform the lookup */` |
|     4713 |  6549 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6550 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6551 | `				/* Function is callable */` |
|     4680 |  6552 | `				res = 1;` |
|     2339 |  6553 | `		}` |
|     2348 |  6554 | `	}` |
|    15972 |  6555 | `	return res;` |
|        2 |  6556 |  |
|        - |  6557 | `/*` |
|        - |  6558 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6559 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6560 | ` * Parameters` |
|        - |  6561 | ` * $name` |
|        - |  6562 | ` *    The callback function to check` |
|        - |  6563 | ` * $syntax_only` |
|        - |  6564 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6565 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6566 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6567 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6568 | ` *    a string.` |
|        - |  6569 | ` * Return` |
|        - |  6570 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6571 | ` */` |
|       14 |  6572 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6573 |  |
|        - |  6574 | `	ph7_vm *pVm;` |
|        - |  6575 | `	int res;` |
|       15 |  6576 | `	if( nArg < 1 ){` |
|        - |  6577 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6578 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6579 | `		return SXRET_OK;` |
|        - |  6580 | `	}` |
|        - |  6581 | `	/* Point to the target VM */` |
|       15 |  6582 | `	pVm = pCtx->pVm;` |
|        - |  6583 | `	/* Perform the requested operation */` |
|       15 |  6584 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6585 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6586 | `	return SXRET_OK;` |
|        8 |  6587 |  |
|        - |  6588 | `/*` |
|        - |  6589 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6590 | ` * defined below.` |
|        - |  6591 | ` */` |
|     1082 |  6592 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6593 |  |
|     1083 |  6594 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6595 | `	ph7_value sName;` |
|        - |  6596 | `	sxi32 rc;` |
|        - |  6597 | `	/* Prepare the function name for insertion */` |
|     1083 |  6598 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6599 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6600 | `	/* Perform the insertion */` |
|     1083 |  6601 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6602 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6603 | `	return rc;` |
|        1 |  6604 |  |
|        - |  6605 | `/*` |
|        - |  6606 | ` * array get_defined_functions(void)` |
|        - |  6607 | ` *  Returns an array of all defined functions.` |
|        - |  6608 | ` * Parameter` |
|        - |  6609 | ` *  None.` |
|        - |  6610 | ` * Return` |
|        - |  6611 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6612 | ` *  both built-in (internal) and user-defined.` |
|        - |  6613 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6614 | ` *  defined ones using $arr["user"].` |
|        - |  6615 | ` * Note:` |
|        - |  6616 | ` *  NULL is returned on failure.` |
|        - |  6617 | ` */` |
|        2 |  6618 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6619 |  |
|        - |  6620 | `	ph7_value *pArray,*pEntry;` |
|        - |  6621 | `	/* NOTE:` |
|        - |  6622 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6623 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6624 | `	 */` |
|        3 |  6625 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6626 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6627 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6628 | `		SXUNUSED(apArg);` |
|        - |  6629 | `		/* Return NULL */` |
|      ! 0 |  6630 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6631 | `		return SXRET_OK;` |
|        - |  6632 | `	}` |
|        3 |  6633 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6634 | `	if( pEntry == 0 ){` |
|        - |  6635 | `		/* Return NULL */` |
|      ! 0 |  6636 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6637 | `		return SXRET_OK;` |
|        - |  6638 | `	}` |
|        - |  6639 | `	/* Fill with the appropriate information */` |
|        3 |  6640 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6641 | `	/* Create the 'internal' index */` |
|        3 |  6642 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6643 | `	/* Create the user-func array */` |
|        3 |  6644 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6645 | `	if( pEntry == 0 ){` |
|        - |  6646 | `		/* Return NULL */` |
|      ! 0 |  6647 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6648 | `		return SXRET_OK;` |
|        - |  6649 | `	}` |
|        - |  6650 | `	/* Fill with the appropriate information */` |
|        3 |  6651 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6652 | `	/* Create the 'user' index */` |
|        3 |  6653 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6654 | `	/* Return the multi-dimensional array */` |
|        3 |  6655 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6656 | `	return SXRET_OK;` |
|        2 |  6657 |  |
|        - |  6658 | `/*` |
|        - |  6659 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6660 | ` *  Register a function for execution on shutdown.` |
|        - |  6661 | ` * Note` |
|        - |  6662 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6663 | ` *  be called in the same order as they were registered.` |
|        - |  6664 | ` * Parameters` |
|        - |  6665 | ` *  $callback` |
|        - |  6666 | ` *   The shutdown callback to register.` |
|        - |  6667 | ` * $param` |
|        - |  6668 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6669 | ` * Return` |
|        - |  6670 | ` *  Nothing.` |
|        - |  6671 | ` */` |
|        2 |  6672 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6673 |  |
|        - |  6674 | `	VmShutdownCB sEntry;` |
|        - |  6675 | `	int i,j;` |
|        3 |  6676 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6677 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6678 | `		return PH7_OK;` |
|        - |  6679 | `	}` |
|        - |  6680 | `	/* Zero the Entry */` |
|        3 |  6681 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6682 | `	/* Initialize fields */` |
|        3 |  6683 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6684 | `	/* Save the callback name for later invocation name */` |
|        3 |  6685 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6686 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6687 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6688 | `	}` |
|        - |  6689 | `	/* Copy arguments */` |
|        3 |  6690 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6691 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6692 | `			/* Limit reached */` |
|      ! 0 |  6693 | `			break;` |
|        - |  6694 | `		}` |
|      ! 0 |  6695 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6696 | `	}` |
|        3 |  6697 | `	sEntry.nArg = j;` |
|        - |  6698 | `	/* Install the callback */` |
|        3 |  6699 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6700 | `	return PH7_OK;` |
|        2 |  6701 |  |
|        - |  6702 | `/*` |
|        - |  6703 | ` * Section:` |
|        - |  6704 | ` *  Class handling functions.` |
|        - |  6705 | ` * Status:` |
|        - |  6706 | ` *    Stable.` |
|        - |  6707 | ` */` |
|        - |  6708 | `/*` |
|        - |  6709 | ` * Extract the top active class. NULL is returned` |
|        - |  6710 | ` * if the class stack is empty.` |
|        - |  6711 | ` */` |
|      516 |  6712 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6713 |  |
|      518 |  6714 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6715 | `	ph7_class **apClass;` |
|      518 |  6716 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6717 | `		/* Empty stack,return NULL */` |
|       15 |  6718 | `		return 0;` |
|        - |  6719 | `	}` |
|        - |  6720 | `	/* Peek the last entry */` |
|      504 |  6721 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      504 |  6722 | `	return apClass[pSet->nUsed - 1];` |
|      260 |  6723 |  |
|        - |  6724 | `/*` |
|        - |  6725 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  6726 | ` *   Get the class that declared the currently executing method.` |
|        - |  6727 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  6728 | ` *` |
|        - |  6729 | ` * Parameters` |
|        - |  6730 | ` *   pVm: Target VM` |
|        - |  6731 | ` *` |
|        - |  6732 | ` * Return` |
|        - |  6733 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  6734 | ` *   - Not executing within a class method` |
|        - |  6735 | ` *` |
|        - |  6736 | ` * Note` |
|        - |  6737 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  6738 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  6739 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  6740 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  6741 | ` *   declaring class.` |
|        - |  6742 | ` */` |
|       18 |  6743 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        1 |  6744 |  |
|       19 |  6745 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  6746 | `	ph7_vm_func *pVmFunc;` |
|        - |  6747 |  |
|        - |  6748 | `	/* Skip exception frames to find the actual method frame */` |
|       19 |  6749 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      ! 0 |  6750 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  6751 | `	}` |
|        - |  6752 |  |
|        - |  6753 | `	/* Check if we're in a method context */` |
|       19 |  6754 | `	if( pFrame->pParent ){` |
|       15 |  6755 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       15 |  6756 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  6757 | `			/* Return the declaring class */` |
|       15 |  6758 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  6759 | `		}` |
|      ! 0 |  6760 | `	}` |
|        - |  6761 |  |
|        5 |  6762 | `	return 0;` |
|       10 |  6763 |  |
|        - |  6764 |  |
|        - |  6765 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  6766 | `/*` |
|        - |  6767 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  6768 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  6769 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  6770 | ` * return value indicates failure.` |
|        - |  6771 | ` */` |
|     1146 |  6772 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  6773 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  6774 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  6775 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  6776 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  6777 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  6778 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  6779 | `	)` |
|        2 |  6780 |  |
|        - |  6781 | `	ph7_value *aStack;` |
|        - |  6782 | `	VmInstr aInstr[2];` |
|        - |  6783 | `	int iCursor;` |
|        - |  6784 | `	int i;` |
|        - |  6785 | `	/* Create a new operand stack */` |
|     1148 |  6786 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1148 |  6787 | `	if( aStack == 0 ){` |
|      ! 0 |  6788 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6789 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  6790 | `		return SXERR_MEM;` |
|        - |  6791 | `	}` |
|        - |  6792 | `	/* Fill the operand stack with the given arguments */` |
|     1694 |  6793 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      548 |  6794 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6795 | `		/*` |
|        - |  6796 | `		 * Symisc eXtension:` |
|        - |  6797 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6798 | `		 */` |
|      548 |  6799 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      275 |  6800 | `	}` |
|     1148 |  6801 | `	iCursor = nArg + 1;` |
|     1148 |  6802 | `	if( pThis ){` |
|        - |  6803 | `		/*` |
|        - |  6804 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  6805 | `		 */` |
|     1142 |  6806 | `		pThis->iRef++; /* Increment reference count */` |
|     1142 |  6807 | `		aStack[i].x.pOther = pThis;` |
|     1142 |  6808 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      570 |  6809 | `	}` |
|     1148 |  6810 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1148 |  6811 | `	i++;` |
|        - |  6812 | `	/* Push method name */` |
|     1148 |  6813 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1148 |  6814 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1148 |  6815 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1148 |  6816 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  6817 | `	/* Emit the CALL istruction */` |
|     1148 |  6818 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1148 |  6819 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1148 |  6820 | `	aInstr[0].iP2 = 0;` |
|     1148 |  6821 | `	aInstr[0].p3  = 0;` |
|        - |  6822 | `	/* Emit the DONE instruction */` |
|     1148 |  6823 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1148 |  6824 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1148 |  6825 | `	aInstr[1].iP2 = 0;` |
|     1148 |  6826 | `	aInstr[1].p3  = 0;` |
|        - |  6827 | `	/* Execute the method body (if available) */` |
|     1148 |  6828 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  6829 | `	/* Clean up the mess left behind */` |
|     1148 |  6830 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1148 |  6831 | `	return PH7_OK;` |
|      575 |  6832 |  |
|        - |  6833 | `/*` |
|        - |  6834 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  6835 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  6836 | ` * in the apArg[] array.` |
|        - |  6837 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6838 | ` * return value indicates failure.` |
|        - |  6839 | ` */` |
|      926 |  6840 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  6841 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6842 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6843 | `	int nArg,          /* Total number of given arguments */` |
|        - |  6844 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  6845 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  6846 | `	)` |
|        2 |  6847 |  |
|        - |  6848 | `	ph7_value *aStack;` |
|        - |  6849 | `	VmInstr aInstr[2];` |
|        - |  6850 | `	int i;` |
|      928 |  6851 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6852 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  6853 | `		if( pResult ){` |
|        - |  6854 | `			/* Assume a null return value */` |
|      ! 0 |  6855 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6856 | `		}` |
|      471 |  6857 | `		return SXERR_INVALID;` |
|        - |  6858 | `	}` |
|      458 |  6859 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6860 | `		/* Class method */` |
|       11 |  6861 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  6862 | `		ph7_class_method *pMethod = 0;` |
|       11 |  6863 | `		ph7_class_instance *pThis = 0;` |
|       11 |  6864 | `		ph7_class *pClass = 0;` |
|        - |  6865 | `		ph7_value *pValue;` |
|        - |  6866 | `		sxi32 rc;` |
|       11 |  6867 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  6868 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  6869 | `			if( pResult ){` |
|        - |  6870 | `				/* Assume a null return value */` |
|      ! 0 |  6871 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6872 | `			}` |
|      ! 0 |  6873 | `			return SXRET_OK;` |
|        - |  6874 | `		}` |
|        - |  6875 | `		/* Extract the class name or an instance of it */` |
|       11 |  6876 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  6877 | `		if( pValue ){` |
|       11 |  6878 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  6879 | `		}` |
|       11 |  6880 | `		if( pClass == 0 ){` |
|        - |  6881 | `			/* No such class,return NULL */` |
|      ! 0 |  6882 | `			if( pResult ){` |
|      ! 0 |  6883 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6884 | `			}` |
|      ! 0 |  6885 | `			return SXRET_OK;` |
|        - |  6886 | `		}` |
|       11 |  6887 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6888 | `			/* Point to the class instance */` |
|        5 |  6889 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  6890 | `		}` |
|        - |  6891 | `		/* Try to extract the method */` |
|       11 |  6892 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  6893 | `		if( pValue ){` |
|       11 |  6894 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  6895 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  6896 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  6897 | `			}` |
|        5 |  6898 | `		}` |
|       11 |  6899 | `		if( pMethod == 0 ){` |
|        - |  6900 | `			/* No such method,return NULL */` |
|      ! 0 |  6901 | `			if( pResult ){` |
|      ! 0 |  6902 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  6903 | `			}` |
|      ! 0 |  6904 | `			return SXRET_OK;` |
|        - |  6905 | `		}` |
|        - |  6906 | `		/* Call the class method */` |
|       11 |  6907 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  6908 | `		return rc;` |
|        - |  6909 | `	}` |
|        - |  6910 | `	/* Create a new operand stack */` |
|      448 |  6911 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  6912 | `	if( aStack == 0 ){` |
|      ! 0 |  6913 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6914 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  6915 | `		if( pResult ){` |
|        - |  6916 | `			/* Assume a null return value */` |
|      ! 0 |  6917 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  6918 | `		}` |
|      ! 0 |  6919 | `		return SXERR_MEM;` |
|        - |  6920 | `	}` |
|        - |  6921 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  6922 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  6923 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  6924 | `		/*` |
|        - |  6925 | `		 * Symisc eXtension:` |
|        - |  6926 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  6927 | `		 */` |
|     1024 |  6928 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  6929 | `	}` |
|        - |  6930 | `	/* Push the function name */` |
|      448 |  6931 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  6932 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  6933 | `	/* Emit the CALL istruction */` |
|      448 |  6934 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  6935 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  6936 | `	aInstr[0].iP2 = 0;` |
|      448 |  6937 | `	aInstr[0].p3  = 0;` |
|        - |  6938 | `	/* Emit the DONE instruction */` |
|      448 |  6939 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  6940 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  6941 | `	aInstr[1].iP2 = 0;` |
|      448 |  6942 | `	aInstr[1].p3  = 0;` |
|        - |  6943 | `	/* Execute the function body (if available) */` |
|      448 |  6944 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  6945 | `	/* Clean up the mess left behind */` |
|      448 |  6946 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  6947 | `	return PH7_OK;` |
|      465 |  6948 |  |
|        - |  6949 | `/*` |
|        - |  6950 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  6951 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  6952 | ` * parameter.` |
|        - |  6953 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  6954 | ` * return value indicates failure.` |
|        - |  6955 | ` */` |
|      236 |  6956 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  6957 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  6958 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  6959 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  6960 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  6961 | `	)` |
|        1 |  6962 |  |
|        - |  6963 | `	ph7_value *pArg;` |
|        - |  6964 | `	SySet aArg;` |
|        - |  6965 | `	va_list ap;` |
|        - |  6966 | `	sxi32 rc;` |
|      237 |  6967 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  6968 | `	/* Copy arguments one after one */` |
|      237 |  6969 | `	va_start(ap,pResult);` |
|      393 |  6970 | `	for(;;){` |
|      787 |  6971 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  6972 | `		if( pArg == 0 ){` |
|      237 |  6973 | `			break;` |
|        - |  6974 | `		}` |
|      551 |  6975 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  6976 | `	}` |
|        - |  6977 | `	/* Call the core routine */` |
|      237 |  6978 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  6979 | `	/* Cleanup */` |
|      237 |  6980 | `	SySetRelease(&aArg);` |
|      237 |  6981 | `	return rc;` |
|        1 |  6982 |  |
|        - |  6983 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  6984 | `/*` |
|        - |  6985 | ` * bool defined(string $name)` |
|        - |  6986 | ` *  Checks whether a given named constant exists.` |
|        - |  6987 | ` * Parameter:` |
|        - |  6988 | ` *  Name of the desired constant.` |
|        - |  6989 | ` * Return` |
|        - |  6990 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  6991 | ` */` |
|       14 |  6992 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6993 |  |
|        - |  6994 | `	const char *zName;` |
|       16 |  6995 | `	int nLen = 0;` |
|       16 |  6996 | `	int res = 0;` |
|       16 |  6997 | `	if( nArg < 1 ){` |
|        - |  6998 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  6999 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7000 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7001 | `		return SXRET_OK;` |
|        - |  7002 | `	}` |
|        - |  7003 | `	/* Extract constant name */` |
|       16 |  7004 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7005 | `	/* Perform the lookup */` |
|       16 |  7006 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7007 | `		/* Already defined */` |
|       10 |  7008 | `		res = 1;` |
|        4 |  7009 | `	}` |
|       16 |  7010 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7011 | `	return SXRET_OK;` |
|        9 |  7012 |  |
|        - |  7013 | `/*` |
|        - |  7014 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7015 | ` * below.` |
|        - |  7016 | ` */` |
|        8 |  7017 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7018 |  |
|       10 |  7019 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7020 | `	/* Expand constant value */` |
|       10 |  7021 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7022 |  |
|        - |  7023 | `/*` |
|        - |  7024 | ` * bool define(string $constant_name,expression value)` |
|        - |  7025 | ` *  Defines a named constant at runtime.` |
|        - |  7026 | ` * Parameter:` |
|        - |  7027 | ` *  $constant_name` |
|        - |  7028 | ` *   The name of the constant` |
|        - |  7029 | ` *  $value` |
|        - |  7030 | ` *   Constant value` |
|        - |  7031 | ` * Return:` |
|        - |  7032 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7033 | ` */` |
|       10 |  7034 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7035 |  |
|        - |  7036 | `	const char *zName;  /* Constant name */` |
|        - |  7037 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7038 | `	int nLen = 0;       /* Name length */` |
|        - |  7039 | `	sxi32 rc;` |
|       12 |  7040 | `	if( nArg < 2 ){` |
|        - |  7041 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7042 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7043 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7044 | `		return SXRET_OK;` |
|        - |  7045 | `	}` |
|       12 |  7046 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7047 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7048 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7049 | `		return SXRET_OK;` |
|        - |  7050 | `	}` |
|        - |  7051 | `	/* Extract constant name */` |
|       12 |  7052 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7053 | `	if( nLen < 1 ){` |
|      ! 0 |  7054 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7055 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7056 | `		return SXRET_OK;` |
|        - |  7057 | `	}` |
|        - |  7058 | `	/* Duplicate constant value */` |
|       12 |  7059 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7060 | `	if( pValue == 0 ){` |
|      ! 0 |  7061 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7062 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7063 | `		return SXRET_OK;` |
|        - |  7064 | `	}` |
|        - |  7065 | `	/* Initialize the memory object */` |
|       12 |  7066 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7067 | `	/* Register the constant */` |
|       12 |  7068 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7069 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7070 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7071 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7072 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7073 | `		return SXRET_OK;` |
|        - |  7074 | `	}` |
|        - |  7075 | `	/* Duplicate constant value */` |
|       12 |  7076 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7077 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7078 | `		/* Lower case the constant name */` |
|      ! 0 |  7079 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7080 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7081 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7082 | `				/* UTF-8 stream */` |
|      ! 0 |  7083 | `				zCur++;` |
|      ! 0 |  7084 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7085 | `					zCur++;` |
|      ! 0 |  7086 | `				}` |
|      ! 0 |  7087 | `				continue;` |
|        - |  7088 | `			}` |
|      ! 0 |  7089 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7090 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7091 | `				zCur[0] = (char)c;` |
|      ! 0 |  7092 | `			}` |
|      ! 0 |  7093 | `			zCur++;` |
|      ! 0 |  7094 | `		}` |
|        - |  7095 | `		/* Finally,register the constant */` |
|      ! 0 |  7096 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7097 | `	}` |
|        - |  7098 | `	/* All done,return TRUE */` |
|       12 |  7099 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7100 | `	return SXRET_OK;` |
|        7 |  7101 |  |
|        - |  7102 | `/*` |
|        - |  7103 | ` * value constant(string $name)` |
|        - |  7104 | ` *  Returns the value of a constant` |
|        - |  7105 | ` * Parameter` |
|        - |  7106 | ` *  $name` |
|        - |  7107 | ` *    Name of the constant.` |
|        - |  7108 | ` * Return` |
|        - |  7109 | ` *  Constant value or NULL if not defined.` |
|        - |  7110 | ` */` |
|        8 |  7111 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7112 |  |
|        - |  7113 | `	SyHashEntry *pEntry;` |
|        - |  7114 | `	ph7_constant *pCons;` |
|        - |  7115 | `	const char *zName; /* Constant name */` |
|        - |  7116 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7117 | `	int nLen;` |
|       10 |  7118 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7119 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7120 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7121 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7122 | `		return SXRET_OK;` |
|        - |  7123 | `	}` |
|        - |  7124 | `	/* Extract the constant name */` |
|       10 |  7125 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7126 | `	/* Perform the query */` |
|       10 |  7127 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7128 | `	if( pEntry == 0 ){` |
|        3 |  7129 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7130 | `		ph7_result_null(pCtx);` |
|        3 |  7131 | `		return SXRET_OK;` |
|        - |  7132 | `	}` |
|        8 |  7133 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7134 | `	/* Point to the structure that describe the constant */` |
|        8 |  7135 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7136 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7137 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7138 | `	/* Return that value */` |
|        8 |  7139 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7140 | `	/* Cleanup */` |
|        8 |  7141 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7142 | `	return SXRET_OK;` |
|        6 |  7143 |  |
|        - |  7144 | `/*` |
|        - |  7145 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7146 | ` * defined below.` |
|        - |  7147 | ` */` |
|      416 |  7148 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7149 |  |
|      417 |  7150 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7151 | `	ph7_value sName;` |
|        - |  7152 | `	sxi32 rc;` |
|        - |  7153 | `	/* Prepare the constant name for insertion */` |
|      417 |  7154 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7155 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7156 | `	/* Perform the insertion */` |
|      417 |  7157 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7158 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7159 | `	return rc;` |
|        1 |  7160 |  |
|        - |  7161 | `/*` |
|        - |  7162 | ` * array get_defined_constants(void)` |
|        - |  7163 | ` *  Returns an associative array with the names of all defined` |
|        - |  7164 | ` *  constants.` |
|        - |  7165 | ` * Parameters` |
|        - |  7166 | ` *  NONE.` |
|        - |  7167 | ` * Returns` |
|        - |  7168 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7169 | ` */` |
|        2 |  7170 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7171 |  |
|        - |  7172 | `	ph7_value *pArray;` |
|        - |  7173 | `	/* Create the array first*/` |
|        3 |  7174 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7175 | `	if( pArray == 0 ){` |
|      ! 0 |  7176 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7177 | `		SXUNUSED(apArg);` |
|        - |  7178 | `		/* Return NULL */` |
|      ! 0 |  7179 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7180 | `		return SXRET_OK;` |
|        - |  7181 | `	}` |
|        - |  7182 | `	/* Fill the array with the defined constants */` |
|        3 |  7183 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7184 | `	/* Return the created array */` |
|        3 |  7185 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7186 | `	return SXRET_OK;` |
|        2 |  7187 |  |
|        - |  7188 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7189 | `/*` |
|        - |  7190 | ` * Section:` |
|        - |  7191 | ` *  Random numbers/string generators.` |
|        - |  7192 | ` * Status:` |
|        - |  7193 | ` *    Stable.` |
|        - |  7194 | ` */` |
|        - |  7195 | `/*` |
|        - |  7196 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7197 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7198 | ` * used by te SQLite3 library.` |
|        - |  7199 | ` */` |
|     2101 |  7200 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7201 |  |
|        - |  7202 | `	sxu32 iNum;` |
|     2103 |  7203 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2103 |  7204 | `	return iNum;` |
|        2 |  7205 |  |
|        - |  7206 | `/*` |
|        - |  7207 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7208 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7209 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7210 | ` * by te SQLite3 library.` |
|        - |  7211 | ` */` |
|    66126 |  7212 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7213 |  |
|        - |  7214 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7215 | `	int i;` |
|        - |  7216 | `	/* Generate a binary string first */` |
|    66128 |  7217 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7218 | `	/* Turn the binary string into english based alphabet */` |
|   727556 |  7219 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   661430 |  7220 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   330716 |  7221 | `	 }` |
|    66128 |  7222 |  |
|        - |  7223 | `/*` |
|        - |  7224 | ` * int rand()` |
|        - |  7225 | ` * int mt_rand()` |
|        - |  7226 | ` * int rand(int $min,int $max)` |
|        - |  7227 | ` * int mt_rand(int $min,int $max)` |
|        - |  7228 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7229 | ` * Parameter` |
|        - |  7230 | ` *  $min` |
|        - |  7231 | ` *    The lowest value to return (default: 0)` |
|        - |  7232 | ` *  $max` |
|        - |  7233 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7234 | ` * Return` |
|        - |  7235 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7236 | ` * Note:` |
|        - |  7237 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7238 | ` *  by te SQLite3 library.` |
|        - |  7239 | ` */` |
|       20 |  7240 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7241 |  |
|        - |  7242 | `	sxu32 iNum;` |
|        - |  7243 | `	/* Generate the random number */` |
|       21 |  7244 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7245 | `	if( nArg > 1 ){` |
|        - |  7246 | `		sxu32 iMin,iMax;` |
|        3 |  7247 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7248 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7249 | `		if( iMin < iMax ){` |
|        3 |  7250 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7251 | `			if( iDiv > 0 ){` |
|        3 |  7252 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7253 | `			}` |
|        1 |  7254 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7255 | `			iNum %= iMax;` |
|      ! 0 |  7256 | `		}` |
|        1 |  7257 | `	}` |
|        - |  7258 | `	/* Return the number */` |
|       21 |  7259 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7260 | `	return SXRET_OK;` |
|        1 |  7261 |  |
|        - |  7262 | `/*` |
|        - |  7263 | ` * int getrandmax(void)` |
|        - |  7264 | ` * int mt_getrandmax(void)` |
|        - |  7265 | ` * int rc4_getrandmax(void)` |
|        - |  7266 | ` *   Show largest possible random value` |
|        - |  7267 | ` * Return` |
|        - |  7268 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7269 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7270 | ` * Note:` |
|        - |  7271 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7272 | ` *  by te SQLite3 library.` |
|        - |  7273 | ` */` |
|        4 |  7274 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7275 |  |
|        2 |  7276 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7277 | `	SXUNUSED(apArg);` |
|        5 |  7278 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7279 | `	return SXRET_OK;` |
|        1 |  7280 |  |
|        - |  7281 | `/*` |
|        - |  7282 | ` * string rand_str()` |
|        - |  7283 | ` * string rand_str(int $len)` |
|        - |  7284 | ` *  Generate a random string (English alphabet).` |
|        - |  7285 | ` * Parameter` |
|        - |  7286 | ` *  $len` |
|        - |  7287 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7288 | ` * Return` |
|        - |  7289 | ` *   A pseudo random string.` |
|        - |  7290 | ` * Note:` |
|        - |  7291 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7292 | ` *  by te SQLite3 library.` |
|        - |  7293 | ` *  This function is a symisc extension.` |
|        - |  7294 | ` */` |
|      120 |  7295 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7296 |  |
|        - |  7297 | `	char zString[1024];` |
|      122 |  7298 | `	int iLen = 0x10;` |
|      122 |  7299 | `	if( nArg > 0 ){` |
|        - |  7300 | `		/* Get the desired length */` |
|      122 |  7301 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7302 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7303 | `			/* Default length */` |
|        3 |  7304 | `			iLen = 0x10;` |
|        1 |  7305 | `		}` |
|       60 |  7306 | `	}` |
|        - |  7307 | `	/* Generate the random string */` |
|      122 |  7308 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7309 | `	/* Return the generated string */` |
|      122 |  7310 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7311 | `	return SXRET_OK;` |
|        2 |  7312 |  |
|        - |  7313 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7314 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7315 | `/* Unique ID private data */` |
|        - |  7316 | `struct unique_id_data` |
|        - |  7317 |  |
|        - |  7318 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7319 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7320 | `};` |
|        - |  7321 | `/*` |
|        - |  7322 | ` * Binary to hex consumer callback.` |
|        - |  7323 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7324 | ` * defined below.` |
|        - |  7325 | ` */` |
|      192 |  7326 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7327 |  |
|      193 |  7328 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7329 | `	sxu32 nBuflen;` |
|        - |  7330 | `	/* Extract result buffer length */` |
|      193 |  7331 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7332 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7333 | `			/*` |
|        - |  7334 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7335 | `			 * string will be 13 characters long` |
|        - |  7336 | `			 */` |
|       25 |  7337 | `		return SXERR_ABORT;` |
|        - |  7338 | `	}` |
|      169 |  7339 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7340 | `		return SXERR_ABORT;` |
|        - |  7341 | `	}` |
|        - |  7342 | `	/* Safely Consume the hex stream */` |
|      169 |  7343 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7344 | `	return SXRET_OK;` |
|       97 |  7345 |  |
|        - |  7346 | `/*` |
|        - |  7347 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7348 | ` *  Generate a unique ID` |
|        - |  7349 | ` * Parameter` |
|        - |  7350 | ` * $prefix` |
|        - |  7351 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7352 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7353 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7354 | ` * $more_entropy` |
|        - |  7355 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7356 | ` *  that the result will be unique.` |
|        - |  7357 | ` * Return` |
|        - |  7358 | ` *  Returns the unique identifier, as a string.` |
|        - |  7359 | ` */` |
|       24 |  7360 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7361 |  |
|        - |  7362 | `	struct unique_id_data sUniq;` |
|        - |  7363 | `	unsigned char zDigest[20];` |
|       25 |  7364 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7365 | `	const char *zPrefix;` |
|        - |  7366 | `	SHA1Context sCtx;` |
|        - |  7367 | `	char zRandom[7];` |
|        - |  7368 | `	int nPrefix;` |
|        - |  7369 | `	int entropy;` |
|        - |  7370 | `	/* Generate a random string first */` |
|       25 |  7371 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7372 | `	/* Initialize fields */` |
|       25 |  7373 | `	zPrefix = 0;` |
|       25 |  7374 | `	nPrefix = 0;` |
|       25 |  7375 | `	entropy = 0;` |
|       25 |  7376 | `	if( nArg > 0 ){` |
|        - |  7377 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7378 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7379 | `		if( nArg > 1 ){` |
|      ! 0 |  7380 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7381 | `		}` |
|      ! 0 |  7382 | `	}` |
|       25 |  7383 | `	SHA1Init(&sCtx);` |
|        - |  7384 | `	/* Generate the random ID */` |
|       25 |  7385 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7386 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7387 | `	}` |
|        - |  7388 | `	/* Append the random ID */` |
|       25 |  7389 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7390 | `	/* Append the random string */` |
|       25 |  7391 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7392 | `	/* Increment the number */` |
|       25 |  7393 | `	pVm->unique_id++;` |
|       25 |  7394 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7395 | `	/* Hexify the digest */` |
|       25 |  7396 | `	sUniq.pCtx = pCtx;` |
|       25 |  7397 | `	sUniq.entropy = entropy;` |
|       25 |  7398 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7399 | `	/* All done */` |
|       25 |  7400 | `	return PH7_OK;` |
|        1 |  7401 |  |
|        - |  7402 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7403 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7404 | `/*` |
|        - |  7405 | ` * Section:` |
|        - |  7406 | ` *  Language construct implementation as foreign functions.` |
|        - |  7407 | ` * Status:` |
|        - |  7408 | ` *    Stable.` |
|        - |  7409 | ` */` |
|        - |  7410 | `/*` |
|        - |  7411 | ` * void echo($string...)` |
|        - |  7412 | ` *  Output one or more messages.` |
|        - |  7413 | ` * Parameters` |
|        - |  7414 | ` *  $string` |
|        - |  7415 | ` *   Message to output.` |
|        - |  7416 | ` * Return` |
|        - |  7417 | ` *  NULL.` |
|        - |  7418 | ` */` |
|      ! 0 |  7419 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7420 |  |
|        - |  7421 | `	const char *zData;` |
|      ! 0 |  7422 | `	int nDataLen = 0;` |
|        - |  7423 | `	ph7_vm *pVm;` |
|        - |  7424 | `	int i,rc;` |
|        - |  7425 | `	/* Point to the target VM */` |
|      ! 0 |  7426 | `	pVm = pCtx->pVm;` |
|        - |  7427 | `	/* Output */` |
|      ! 0 |  7428 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7429 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7430 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7431 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7432 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7433 | `				/* Increment output length */` |
|      ! 0 |  7434 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7435 | `			}` |
|      ! 0 |  7436 | `			if( rc == SXERR_ABORT ){` |
|        - |  7437 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7438 | `				return PH7_ABORT;` |
|        - |  7439 | `			}` |
|      ! 0 |  7440 | `		}` |
|      ! 0 |  7441 | `	}` |
|      ! 0 |  7442 | `	return SXRET_OK;` |
|      ! 0 |  7443 |  |
|        - |  7444 | `/*` |
|        - |  7445 | ` * int print($string...)` |
|        - |  7446 | ` *  Output one or more messages.` |
|        - |  7447 | ` * Parameters` |
|        - |  7448 | ` *  $string` |
|        - |  7449 | ` *   Message to output.` |
|        - |  7450 | ` * Return` |
|        - |  7451 | ` *  1 always.` |
|        - |  7452 | ` */` |
|        2 |  7453 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7454 |  |
|        - |  7455 | `	const char *zData;` |
|        3 |  7456 | `	int nDataLen = 0;` |
|        - |  7457 | `	ph7_vm *pVm;` |
|        - |  7458 | `	int i,rc;` |
|        - |  7459 | `	/* Point to the target VM */` |
|        3 |  7460 | `	pVm = pCtx->pVm;` |
|        - |  7461 | `	/* Output */` |
|        5 |  7462 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7463 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7464 | `		if( nDataLen > 0 ){` |
|        3 |  7465 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7466 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7467 | `				/* Increment output length */` |
|        3 |  7468 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7469 | `			}` |
|        3 |  7470 | `			if( rc == SXERR_ABORT ){` |
|        - |  7471 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7472 | `				return PH7_ABORT;` |
|        - |  7473 | `			}` |
|        1 |  7474 | `		}` |
|        2 |  7475 | `	}` |
|        - |  7476 | `	/* Return 1 */` |
|        3 |  7477 | `	ph7_result_int(pCtx,1);` |
|        3 |  7478 | `	return SXRET_OK;` |
|        2 |  7479 |  |
|        - |  7480 | `/*` |
|        - |  7481 | ` * void exit(string $msg)` |
|        - |  7482 | ` * void exit(int $status)` |
|        - |  7483 | ` * void die(string $ms)` |
|        - |  7484 | ` * void die(int $status)` |
|        - |  7485 | ` *   Output a message and terminate program execution.` |
|        - |  7486 | ` * Parameter` |
|        - |  7487 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7488 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7489 | ` *  and not printed` |
|        - |  7490 | ` * Return` |
|        - |  7491 | ` *  NULL` |
|        - |  7492 | ` */` |
|      ! 0 |  7493 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7494 |  |
|      ! 0 |  7495 | `	if( nArg > 0 ){` |
|      ! 0 |  7496 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7497 | `			const char *zData;` |
|      ! 0 |  7498 | `			int iLen = 0;` |
|        - |  7499 | `			/* Print exit message */` |
|      ! 0 |  7500 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7501 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7502 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7503 | `			sxi32 iExitStatus;` |
|        - |  7504 | `			/* Record exit status code */` |
|      ! 0 |  7505 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7506 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7507 | `		}` |
|      ! 0 |  7508 | `	}` |
|        - |  7509 | `	/* Check if we are in an included file */` |
|      ! 0 |  7510 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7511 | `		/* Exit the entire process */` |
|      ! 0 |  7512 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7513 | `	}` |
|        - |  7514 | `	/* Abort processing immediately */` |
|      ! 0 |  7515 | `	return PH7_ABORT;` |
|      ! 0 |  7516 |  |
|        - |  7517 | `/*` |
|        - |  7518 | ` * bool isset($var,...)` |
|        - |  7519 | ` *  Finds out whether a variable is set.` |
|        - |  7520 | ` * Parameters` |
|        - |  7521 | ` *  One or more variable to check.` |
|        - |  7522 | ` * Return` |
|        - |  7523 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7524 | ` */` |
|    68922 |  7525 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7526 |  |
|        - |  7527 | `	ph7_value *pObj;` |
|    68924 |  7528 | `	int res = 0;` |
|        - |  7529 | `	int i;` |
|    68924 |  7530 | `	if( nArg < 1 ){` |
|        - |  7531 | `		/* Missing arguments,return false */` |
|      ! 0 |  7532 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7533 | `		return SXRET_OK;` |
|        - |  7534 | `	}` |
|        - |  7535 | `	/* Iterate over available arguments */` |
|    91192 |  7536 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    68924 |  7537 | `		pObj = apArg[i];` |
|    68924 |  7538 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    46160 |  7539 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7540 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7541 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7542 | `			}` |
|    23079 |  7543 | `		}` |
|    68924 |  7544 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    68924 |  7545 | `		if( !res ){` |
|        - |  7546 | `			/* Variable not set,return FALSE */` |
|    46656 |  7547 | `			ph7_result_bool(pCtx,0);` |
|    46656 |  7548 | `			return SXRET_OK;` |
|        - |  7549 | `		}` |
|    11136 |  7550 | `	}` |
|        - |  7551 | `	/* All given variable are set,return TRUE */` |
|    22270 |  7552 | `	ph7_result_bool(pCtx,1);` |
|    22270 |  7553 | `	return SXRET_OK;` |
|    34463 |  7554 |  |
|        - |  7555 | `/*` |
|        - |  7556 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7557 | ` * frame,the reference table and discard it's contents.` |
|        - |  7558 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7559 | ` */` |
|  2952254 |  7560 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7561 |  |
|        - |  7562 | `	ph7_value *pObj;` |
|        - |  7563 | `	VmRefObj *pRef;` |
|  2952256 |  7564 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2952256 |  7565 | `	if( pObj ){` |
|        - |  7566 | `		/* Release the object */` |
|  2952256 |  7567 | `		PH7_MemObjRelease(pObj);` |
|  1476127 |  7568 | `	}` |
|        - |  7569 | `	/* Remove old reference links */` |
|  2952256 |  7570 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2952256 |  7571 | `	if( pRef ){` |
|  2952236 |  7572 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7573 | `		/* Unlink from the reference table */` |
|  2952236 |  7574 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2952236 |  7575 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7576 | `			VmSlot sFree;` |
|        - |  7577 | `			/* Restore to the free list */` |
|  2952230 |  7578 | `			sFree.nIdx = nObjIdx;` |
|  2952230 |  7579 | `			sFree.pUserData = 0;` |
|  2952230 |  7580 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1476114 |  7581 | `		}` |
|  1476117 |  7582 | `	}` |
|  2952256 |  7583 | `	return SXRET_OK;` |
|        2 |  7584 |  |
|        - |  7585 | `/*` |
|        - |  7586 | ` * void unset($var,...)` |
|        - |  7587 | ` *   Unset one or more given variable.` |
|        - |  7588 | ` * Parameters` |
|        - |  7589 | ` *  One or more variable to unset.` |
|        - |  7590 | ` * Return` |
|        - |  7591 | ` *  Nothing.` |
|        - |  7592 | ` */` |
|     3260 |  7593 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7594 |  |
|        - |  7595 | `	ph7_value *pObj;` |
|        - |  7596 | `	ph7_vm *pVm;` |
|        - |  7597 | `	int i;` |
|        - |  7598 | `	/* Point to the target VM */` |
|     3262 |  7599 | `	pVm = pCtx->pVm;` |
|        - |  7600 | `	/* Iterate and unset */` |
|     9666 |  7601 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6406 |  7602 | `		pObj = apArg[i];` |
|     6406 |  7603 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      870 |  7604 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7605 | `				/* Throw an error */` |
|      ! 0 |  7606 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7607 | `			}` |
|      436 |  7608 | `		}else{` |
|     5537 |  7609 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7610 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5537 |  7611 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5531 |  7612 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2765 |  7613 | `			}` |
|        - |  7614 | `		}` |
|     3204 |  7615 | `	}` |
|     3262 |  7616 | `	return SXRET_OK;` |
|        2 |  7617 |  |
|        - |  7618 | `/*` |
|        - |  7619 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7620 | ` */` |
|      110 |  7621 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7622 |  |
|      111 |  7623 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7624 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7625 | `	ph7_value *pObj;` |
|        - |  7626 | `	sxu32 nIdx;` |
|        - |  7627 | `	/* Extract the memory object */` |
|      111 |  7628 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7629 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7630 | `	if( pObj ){` |
|      111 |  7631 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7632 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7633 | `				SyString sName;` |
|        - |  7634 | `				ph7_value sKey;` |
|        - |  7635 | `				/* Perform the insertion */` |
|      109 |  7636 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7637 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7638 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7639 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7640 | `			}` |
|       54 |  7641 | `		}` |
|       55 |  7642 | `	}` |
|      111 |  7643 | `	return SXRET_OK;` |
|        1 |  7644 |  |
|        - |  7645 | `/*` |
|        - |  7646 | ` * array get_defined_vars(void)` |
|        - |  7647 | ` *  Returns an array of all defined variables.` |
|        - |  7648 | ` * Parameter` |
|        - |  7649 | ` *  None` |
|        - |  7650 | ` * Return` |
|        - |  7651 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7652 | ` */` |
|        2 |  7653 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7654 |  |
|        3 |  7655 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7656 | `	ph7_value *pArray;` |
|        - |  7657 | `	/* Create a new array */` |
|        3 |  7658 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7659 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7660 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7661 | `		SXUNUSED(apArg);` |
|        - |  7662 | `		/* Return NULL */` |
|      ! 0 |  7663 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7664 | `		return SXRET_OK;` |
|        - |  7665 | `	}` |
|        - |  7666 | `	/* Superglobals first */` |
|        3 |  7667 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7668 | `	/* Then variable defined in the current frame */` |
|        3 |  7669 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7670 | `	/* Finally,return the created array */` |
|        3 |  7671 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7672 | `	return SXRET_OK;` |
|        2 |  7673 |  |
|        - |  7674 | `/*` |
|        - |  7675 | ` * bool gettype($var)` |
|        - |  7676 | ` *  Get the type of a variable` |
|        - |  7677 | ` * Parameters` |
|        - |  7678 | ` *   $var` |
|        - |  7679 | ` *    The variable being type checked.` |
|        - |  7680 | ` * Return` |
|        - |  7681 | ` *   String representation of the given variable type.` |
|        - |  7682 | ` */` |
|       32 |  7683 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7684 |  |
|       34 |  7685 | `	const char *zType = "Empty";` |
|       34 |  7686 | `	if( nArg > 0 ){` |
|       34 |  7687 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7688 | `	}` |
|        - |  7689 | `	/* Return the variable type */` |
|       34 |  7690 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7691 | `	return SXRET_OK;` |
|        2 |  7692 |  |
|        - |  7693 | `/*` |
|        - |  7694 | ` * string get_resource_type(resource $handle)` |
|        - |  7695 | ` *  This function gets the type of the given resource.` |
|        - |  7696 | ` * Parameters` |
|        - |  7697 | ` *  $handle` |
|        - |  7698 | ` *  The evaluated resource handle.` |
|        - |  7699 | ` * Return` |
|        - |  7700 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7701 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7702 | ` *  the return value will be the string Unknown.` |
|        - |  7703 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7704 | ` *  is not a resource.` |
|        - |  7705 | ` */` |
|        2 |  7706 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7707 |  |
|        3 |  7708 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7709 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7710 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7711 | `		return PH7_OK;` |
|        - |  7712 | `	}` |
|        3 |  7713 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7714 | `	return SXRET_OK;` |
|        2 |  7715 |  |
|        - |  7716 | `/*` |
|        - |  7717 | ` * void var_dump(expression,....)` |
|        - |  7718 | ` *   var_dump � Dumps information about a variable` |
|        - |  7719 | ` * Parameters` |
|        - |  7720 | ` *   One or more expression to dump.` |
|        - |  7721 | ` * Returns` |
|        - |  7722 | ` *  Nothing.` |
|        - |  7723 | ` */` |
|      218 |  7724 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7725 |  |
|        - |  7726 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  7727 | `	int i;` |
|      220 |  7728 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  7729 | `	/* Dump one or more expressions */` |
|      444 |  7730 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  7731 | `		ph7_value *pObj = apArg[i];` |
|        - |  7732 | `		/* Reset the working buffer */` |
|      226 |  7733 | `		SyBlobReset(&sDump);` |
|        - |  7734 | `		/* Dump the given expression */` |
|      226 |  7735 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  7736 | `		/* Output */` |
|      226 |  7737 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  7738 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  7739 | `		}` |
|      114 |  7740 | `	}` |
|        - |  7741 | `	/* Release the working buffer */` |
|      220 |  7742 | `	SyBlobRelease(&sDump);` |
|      220 |  7743 | `	return SXRET_OK;` |
|        2 |  7744 |  |
|        - |  7745 | `/*` |
|        - |  7746 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  7747 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  7748 | ` * Parameters` |
|        - |  7749 | ` *   expression: Expression to dump` |
|        - |  7750 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  7751 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  7752 | ` *            print_r() will return the information rather than print it.` |
|        - |  7753 | ` * Return` |
|        - |  7754 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  7755 | ` *  Otherwise, the return value is TRUE.` |
|        - |  7756 | ` */` |
|       16 |  7757 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7758 |  |
|       17 |  7759 | `	int ret_string = 0;` |
|        - |  7760 | `	SyBlob sDump;` |
|       17 |  7761 | `	if( nArg < 1 ){` |
|        - |  7762 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7763 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7764 | `		return SXRET_OK;` |
|        - |  7765 | `	}` |
|       17 |  7766 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  7767 | `	if ( nArg > 1 ){` |
|        - |  7768 | `		/* Where to redirect output */` |
|       11 |  7769 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  7770 | `	}` |
|        - |  7771 | `	/* Generate dump */` |
|       17 |  7772 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  7773 | `	if( !ret_string ){` |
|        - |  7774 | `		/* Output dump */` |
|        7 |  7775 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7776 | `		/* Return true */` |
|        7 |  7777 | `		ph7_result_bool(pCtx,1);` |
|        4 |  7778 | `	}else{` |
|        - |  7779 | `		/* Generated dump as return value */` |
|       11 |  7780 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7781 | `	}` |
|        - |  7782 | `	/* Release the working buffer */` |
|       17 |  7783 | `	SyBlobRelease(&sDump);` |
|       17 |  7784 | `	return SXRET_OK;` |
|        9 |  7785 |  |
|        - |  7786 | `/*` |
|        - |  7787 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  7788 | ` * Same job as print_r. (see coment above)` |
|        - |  7789 | ` */` |
|        2 |  7790 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7791 |  |
|        3 |  7792 | `	int ret_string = 0;` |
|        - |  7793 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  7794 | `	if( nArg < 1 ){` |
|        - |  7795 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  7796 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7797 | `		return SXRET_OK;` |
|        - |  7798 | `	}` |
|        3 |  7799 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  7800 | `	if ( nArg > 1 ){` |
|        - |  7801 | `		/* Where to redirect output */` |
|        3 |  7802 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  7803 | `	}` |
|        - |  7804 | `	/* Generate dump */` |
|        3 |  7805 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  7806 | `	if( !ret_string ){` |
|        - |  7807 | `		/* Output dump */` |
|      ! 0 |  7808 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7809 | `		/* Return NULL */` |
|      ! 0 |  7810 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7811 | `	}else{` |
|        - |  7812 | `		/* Generated dump as return value */` |
|        3 |  7813 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  7814 | `	}` |
|        - |  7815 | `	/* Release the working buffer */` |
|        3 |  7816 | `	SyBlobRelease(&sDump);` |
|        3 |  7817 | `	return SXRET_OK;` |
|        2 |  7818 |  |
|        - |  7819 | `/*` |
|        - |  7820 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  7821 | ` *  Set/get the various assert flags.` |
|        - |  7822 | ` * Parameter` |
|        - |  7823 | ` * $what` |
|        - |  7824 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  7825 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  7826 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  7827 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  7828 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  7829 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  7830 | ` * $value` |
|        - |  7831 | ` *   An optional new value for the option.` |
|        - |  7832 | ` * Return` |
|        - |  7833 | ` *  Old setting on success or FALSE on failure.` |
|        - |  7834 | ` */` |
|       30 |  7835 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7836 |  |
|       32 |  7837 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7838 | `	int iOption;` |
|        - |  7839 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  7840 | `	if( nArg < 1 ){` |
|        3 |  7841 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7842 | `			"ArgumentCountError",` |
|        - |  7843 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  7844 | `			);` |
|        - |  7845 | `	}` |
|        - |  7846 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  7847 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  7848 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  7849 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7850 | `			"TypeError",` |
|        - |  7851 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  7852 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  7853 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  7854 | `			);` |
|        - |  7855 | `	}` |
|       30 |  7856 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  7857 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  7858 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  7859 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  7860 | `	switch( iOption ){` |
|        6 |  7861 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  7862 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  7863 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  7864 | `		if( nArg > 1 ){` |
|        5 |  7865 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  7866 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  7867 | `			}else{` |
|        3 |  7868 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  7869 | `			}` |
|        2 |  7870 | `		}` |
|       14 |  7871 | `		break;` |
|        1 |  7872 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  7873 | `		/* Return old callback or null */` |
|        3 |  7874 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  7875 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  7876 | `		}else{` |
|        3 |  7877 | `			ph7_result_null(pCtx);` |
|        - |  7878 | `		}` |
|        3 |  7879 | `		if( nArg > 1 ){` |
|      ! 0 |  7880 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  7881 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  7882 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  7883 | `			}else{` |
|      ! 0 |  7884 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  7885 | `			}` |
|      ! 0 |  7886 | `		}` |
|        3 |  7887 | `		break;` |
|        5 |  7888 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  7889 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  7890 | `		if( nArg > 1 ){` |
|        5 |  7891 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  7892 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  7893 | `			}else{` |
|        3 |  7894 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  7895 | `			}` |
|        2 |  7896 | `		}` |
|       11 |  7897 | `		break;` |
|      ! 0 |  7898 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  7899 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  7900 | `		break;` |
|        1 |  7901 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  7902 | `		ph7_result_int(pCtx, 1);` |
|        3 |  7903 | `		break;` |
|      ! 0 |  7904 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  7905 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  7906 | `		break;` |
|        1 |  7907 | `	default:` |
|        - |  7908 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  7909 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7910 | `			"ValueError",` |
|        - |  7911 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  7912 | `			);` |
|        - |  7913 | `	}` |
|       28 |  7914 | `	return PH7_OK;` |
|       17 |  7915 |  |
|        - |  7916 | `/*` |
|        - |  7917 | ` * bool assert(mixed $assertion)` |
|        - |  7918 | ` *  Checks if assertion is FALSE.` |
|        - |  7919 | ` * Parameter` |
|        - |  7920 | ` *  $assertion` |
|        - |  7921 | ` *    The assertion to test.` |
|        - |  7922 | ` * Return` |
|        - |  7923 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  7924 | ` */` |
|       26 |  7925 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7926 |  |
|       28 |  7927 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7928 | `	int iFlags,iResult;` |
|        - |  7929 | `	const char *zDesc;` |
|        - |  7930 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  7931 | `	if( nArg < 1 ){` |
|        3 |  7932 | `		return PH7_VmThrowException(pCtx,` |
|        - |  7933 | `			"ArgumentCountError",` |
|        - |  7934 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  7935 | `			);` |
|        - |  7936 | `	}` |
|       26 |  7937 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  7938 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  7939 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  7940 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  7941 | `		return PH7_OK;` |
|        - |  7942 | `	}` |
|        - |  7943 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  7944 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  7945 | `	if( !iResult ){` |
|        - |  7946 | `		/* Assertion failed */` |
|        - |  7947 | `		/* Extract optional description */` |
|       13 |  7948 | `		zDesc = 0;` |
|       13 |  7949 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  7950 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  7951 | `		}` |
|       13 |  7952 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  7953 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  7954 | `			ph7_value sFile,sLine;` |
|        - |  7955 | `			ph7_value *apCbArg[3];` |
|        - |  7956 | `			SyString *pFile;` |
|        - |  7957 | `			/* Extract the processed script */` |
|      ! 0 |  7958 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  7959 | `			if( pFile == 0 ){` |
|      ! 0 |  7960 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  7961 | `			}` |
|        - |  7962 | `			/* Invoke the callback */` |
|      ! 0 |  7963 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  7964 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  7965 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  7966 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  7967 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  7968 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  7969 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  7970 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  7971 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  7972 | `		}` |
|       13 |  7973 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  7974 | `			/* Abort VM execution immediately */` |
|      ! 0 |  7975 | `			return PH7_ABORT;` |
|        - |  7976 | `		}` |
|        - |  7977 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  7978 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  7979 | `			return PH7_VmThrowException(pCtx,` |
|        - |  7980 | `				"AssertionError",` |
|        - |  7981 | `				"%s",` |
|        1 |  7982 | `				zDesc` |
|        - |  7983 | `				);` |
|      ! 0 |  7984 | `		}else{` |
|       11 |  7985 | `			return PH7_VmThrowException(pCtx,` |
|        - |  7986 | `				"AssertionError",` |
|        - |  7987 | `				"assert(false)"` |
|        - |  7988 | `				);` |
|        - |  7989 | `		}` |
|        - |  7990 | `	}` |
|        - |  7991 | `	/* Assertion passed */` |
|       14 |  7992 | `	ph7_result_bool(pCtx,1);` |
|       14 |  7993 | `	return PH7_OK;` |
|       15 |  7994 |  |
|        - |  7995 | `/*` |
|        - |  7996 | ` * Section:` |
|        - |  7997 | ` *  Error reporting functions.` |
|        - |  7998 | ` * Status:` |
|        - |  7999 | ` *    Stable.` |
|        - |  8000 | ` */` |
|        - |  8001 | `/*` |
|        - |  8002 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8003 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8004 | ` * Parameters` |
|        - |  8005 | ` *  $error_msg` |
|        - |  8006 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8007 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8008 | ` * $error_type` |
|        - |  8009 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8010 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8011 | ` * Return` |
|        - |  8012 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8013 | ` */` |
|       12 |  8014 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8015 |  |
|       14 |  8016 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8017 | `	int rc = PH7_OK;` |
|       14 |  8018 | `	if( nArg > 0 ){` |
|        - |  8019 | `		const char *zErr;` |
|        - |  8020 | `		int nLen;` |
|        - |  8021 | `		/* Extract the error message */` |
|       12 |  8022 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8023 | `		if( nArg > 1 ){` |
|        - |  8024 | `			/* Extract the error type */` |
|       12 |  8025 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8026 | `			switch( nErr ){` |
|        1 |  8027 | `			case 1:   /* E_ERROR */` |
|        - |  8028 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8029 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8030 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8031 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8032 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8033 | `				break;` |
|        1 |  8034 | `			case 2:   /* E_WARNING */` |
|        - |  8035 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8036 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8037 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8038 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8039 | `				break;` |
|        3 |  8040 | `			default:` |
|        8 |  8041 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8042 | `				break;` |
|        - |  8043 | `			}` |
|        5 |  8044 | `		}` |
|        - |  8045 | `		/* Report error */` |
|       12 |  8046 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8047 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8048 | `			return rc;` |
|        - |  8049 | `		}` |
|        - |  8050 | `		/* Return true */` |
|       12 |  8051 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8052 | `	}else{` |
|        - |  8053 | `		/* Missing arguments,return FALSE */` |
|        3 |  8054 | `		ph7_result_bool(pCtx,0);` |
|        - |  8055 | `	}` |
|       14 |  8056 | `	return rc;` |
|        8 |  8057 |  |
|        - |  8058 | `/*` |
|        - |  8059 | ` * int error_reporting([int $level])` |
|        - |  8060 | ` *  Sets which PHP errors are reported.` |
|        - |  8061 | ` * Parameters` |
|        - |  8062 | ` *  $level` |
|        - |  8063 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8064 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8065 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8066 | ` *   levels will not always behave as expected.` |
|        - |  8067 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8068 | ` *   in the predefined constants.` |
|        - |  8069 | ` * Return` |
|        - |  8070 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8071 | ` *   parameter is given.` |
|        - |  8072 | ` */` |
|       40 |  8073 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8074 |  |
|       42 |  8075 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8076 | `	int nOld;` |
|        - |  8077 | `	/* Extract the old reporting level */` |
|       42 |  8078 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8079 | `	if( nArg > 0 ){` |
|        - |  8080 | `		int nNew;` |
|        - |  8081 | `		/* Extract the desired error reporting level */` |
|       34 |  8082 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8083 | `		if( !nNew ){` |
|        - |  8084 | `			/* Do not report errors at all */` |
|        5 |  8085 | `			pVm->bErrReport = 0;` |
|        3 |  8086 | `		}else{` |
|        - |  8087 | `			/* Report all errors */` |
|       30 |  8088 | `			pVm->bErrReport = 1;` |
|        - |  8089 | `		}` |
|       16 |  8090 | `	}` |
|        - |  8091 | `	/* Return the old level */` |
|       42 |  8092 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8093 | `	return PH7_OK;` |
|        2 |  8094 |  |
|        - |  8095 | `/*` |
|        - |  8096 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8097 | ` *  Send an error message somewhere.` |
|        - |  8098 | ` * Parameter` |
|        - |  8099 | ` *  $message` |
|        - |  8100 | ` *   The error message that should be logged.` |
|        - |  8101 | ` *  $message_type` |
|        - |  8102 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8103 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8104 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8105 | ` *       This is the default option.` |
|        - |  8106 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8107 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8108 | ` *    2  No longer an option.` |
|        - |  8109 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8110 | ` *       to the end of the message string.` |
|        - |  8111 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8112 | ` *  $destination` |
|        - |  8113 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8114 | ` *  $extra_headers` |
|        - |  8115 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8116 | ` * Return` |
|        - |  8117 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8118 | ` * NOTE:` |
|        - |  8119 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8120 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8121 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8122 | ` *  Otherwise this function is no-op.` |
|        - |  8123 | ` */` |
|        4 |  8124 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8125 |  |
|        - |  8126 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8127 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8128 | `	int iType = 0;` |
|        5 |  8129 | `	if( nArg < 1 ){` |
|        - |  8130 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8131 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8132 | `		return PH7_OK;` |
|        - |  8133 | `	}` |
|        5 |  8134 | `	if( pVm->xErrLog  ){` |
|        - |  8135 | `		/* Invoke the user callback */` |
|      ! 0 |  8136 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8137 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8138 | `		if( nArg > 1 ){` |
|      ! 0 |  8139 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8140 | `			if( nArg > 2 ){` |
|      ! 0 |  8141 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8142 | `				if( nArg > 3 ){` |
|      ! 0 |  8143 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8144 | `				}` |
|      ! 0 |  8145 | `			}` |
|      ! 0 |  8146 | `		}` |
|      ! 0 |  8147 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8148 | `	}` |
|        - |  8149 | `	/* Retun TRUE */` |
|        5 |  8150 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8151 | `	return PH7_OK;` |
|        3 |  8152 |  |
|        - |  8153 | `/*` |
|        - |  8154 | ` * bool restore_exception_handler(void)` |
|        - |  8155 | ` *  Restores the previously defined exception handler function.` |
|        - |  8156 | ` * Parameter` |
|        - |  8157 | ` *  None` |
|        - |  8158 | ` * Return` |
|        - |  8159 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8160 | ` */` |
|        4 |  8161 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8162 |  |
|        5 |  8163 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8164 | `	ph7_value *pOld,*pNew;` |
|        - |  8165 | `	/* Point to the old and the new handler */` |
|        5 |  8166 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8167 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8168 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8169 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8170 | `		SXUNUSED(apArg);` |
|        - |  8171 | `		/* No installed handler,return FALSE */` |
|        5 |  8172 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8173 | `		return PH7_OK;` |
|        - |  8174 | `	}` |
|        - |  8175 | `	/* Copy the old handler */` |
|      ! 0 |  8176 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8177 | `	PH7_MemObjRelease(pOld);` |
|        - |  8178 | `	/* Return TRUE */` |
|      ! 0 |  8179 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8180 | `	return PH7_OK;` |
|        3 |  8181 |  |
|        - |  8182 | `/*` |
|        - |  8183 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8184 | ` *  Sets a user-defined exception handler function.` |
|        - |  8185 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8186 | ` * NOTE` |
|        - |  8187 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8188 | ` *  the satndard PHP engine.` |
|        - |  8189 | ` * Parameters` |
|        - |  8190 | ` *  $exception_handler` |
|        - |  8191 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8192 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8193 | ` *   that was thrown.` |
|        - |  8194 | ` *  Note:` |
|        - |  8195 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8196 | ` * Return` |
|        - |  8197 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8198 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8199 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8200 | ` */` |
|        4 |  8201 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8202 |  |
|        6 |  8203 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8204 | `	ph7_value *pOld,*pNew;` |
|        - |  8205 | `	/* Point to the old and the new handler */` |
|        6 |  8206 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8207 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8208 | `	/* Return the old handler */` |
|        6 |  8209 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8210 | `	if( nArg > 0 ){` |
|        6 |  8211 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8212 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8213 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8214 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8215 | `		}else{` |
|        6 |  8216 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8217 | `			/* Install the new handler */` |
|        6 |  8218 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8219 | `		}` |
|        2 |  8220 | `	}` |
|        6 |  8221 | `	return PH7_OK;` |
|        2 |  8222 |  |
|        - |  8223 | `/*` |
|        - |  8224 | ` * bool restore_error_handler(void)` |
|        - |  8225 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8226 | ` * Parameters:` |
|        - |  8227 | ` *  None.` |
|        - |  8228 | ` * Return` |
|        - |  8229 | ` *  Always TRUE.` |
|        - |  8230 | ` */` |
|        4 |  8231 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8232 |  |
|        5 |  8233 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8234 | `	ph7_value *pOld,*pNew;` |
|        - |  8235 | `	/* Point to the old and the new handler */` |
|        5 |  8236 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8237 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8238 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8239 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8240 | `		SXUNUSED(apArg);` |
|        - |  8241 | `		/* No installed callback,return FALSE */` |
|        5 |  8242 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8243 | `		return PH7_OK;` |
|        - |  8244 | `	}` |
|        - |  8245 | `	/* Copy the old callback */` |
|      ! 0 |  8246 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8247 | `	PH7_MemObjRelease(pOld);` |
|        - |  8248 | `	/* Return TRUE */` |
|      ! 0 |  8249 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8250 | `	return PH7_OK;` |
|        3 |  8251 |  |
|        - |  8252 | `/*` |
|        - |  8253 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8254 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8255 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8256 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8257 | ` *  Sets a user-defined error handler function.` |
|        - |  8258 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8259 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8260 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8261 | ` *  conditions (using trigger_error()).` |
|        - |  8262 | ` * Parameters` |
|        - |  8263 | ` *  $error_handler` |
|        - |  8264 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8265 | ` *   describing the error.` |
|        - |  8266 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8267 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8268 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8269 | ` *   The function can be shown as:` |
|        - |  8270 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8271 | ` *     errno` |
|        - |  8272 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8273 | ` *   errstr` |
|        - |  8274 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8275 | ` *   errfile` |
|        - |  8276 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8277 | ` *     was raised in, as a string.` |
|        - |  8278 | ` *  Note:` |
|        - |  8279 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8280 | ` * Return` |
|        - |  8281 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8282 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8283 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8284 | ` */` |
|     8718 |  8285 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8286 |  |
|     8720 |  8287 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8288 | `	ph7_value *pOld,*pNew;` |
|        - |  8289 | `	/* Point to the old and the new handler */` |
|     8720 |  8290 | `	pOld = &pVm->aErrCB[0];` |
|     8720 |  8291 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8292 | `	/* Return the old handler */` |
|     8720 |  8293 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8720 |  8294 | `	if( nArg > 0 ){` |
|     8720 |  8295 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8296 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4359 |  8297 | `			PH7_MemObjRelease(pNew);` |
|     4359 |  8298 | `			ph7_result_bool(pCtx,1);` |
|     2180 |  8299 | `		}else{` |
|     4362 |  8300 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8301 | `			/* Install the new handler */` |
|     4362 |  8302 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8303 | `		}` |
|     4359 |  8304 | `	}` |
|     8720 |  8305 | `	return PH7_OK;` |
|        2 |  8306 |  |
|        - |  8307 | `/*` |
|        - |  8308 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8309 | ` *  Generates a backtrace.` |
|        - |  8310 | ` * Paramaeter` |
|        - |  8311 | ` *  $options` |
|        - |  8312 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8313 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8314 | ` *   all the function/method arguments, to save memory.` |
|        - |  8315 | ` * $limit` |
|        - |  8316 | ` *   (Not Used)` |
|        - |  8317 | ` * Return` |
|        - |  8318 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8319 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8320 | ` *          Name        Type      Description` |
|        - |  8321 | ` *          ------      ------     -----------` |
|        - |  8322 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8323 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8324 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8325 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8326 | ` *          object      object    The current object.` |
|        - |  8327 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8328 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8329 | ` */` |
|      492 |  8330 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8331 |  |
|      494 |  8332 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8333 | `	ph7_value *pArray;` |
|        - |  8334 | `	ph7_class *pClass;` |
|        - |  8335 | `	ph7_value *pValue;` |
|        - |  8336 | `	SyString *pFile;` |
|        - |  8337 | `	/* Create a new array */` |
|      494 |  8338 | `	pArray = ph7_context_new_array(pCtx);` |
|      494 |  8339 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      494 |  8340 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8341 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8342 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8343 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8344 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8345 | `		SXUNUSED(apArg);` |
|      ! 0 |  8346 | `		return PH7_OK;` |
|        - |  8347 | `	}` |
|        - |  8348 | `	/* Dump running function name and it's arguments  */` |
|      494 |  8349 | `	if( pVm->pFrame->pParent ){` |
|      494 |  8350 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8351 | `		ph7_vm_func *pFunc;` |
|        - |  8352 | `		ph7_value *pArg;` |
|      494 |  8353 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8354 | `			/* Safely ignore the exception frame */` |
|      ! 0 |  8355 | `			pFrame = pFrame->pParent;` |
|      ! 0 |  8356 | `		}` |
|      494 |  8357 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      494 |  8358 | `		if( pFrame->pParent && pFunc ){` |
|      494 |  8359 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      494 |  8360 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      494 |  8361 | `			ph7_value_reset_string_cursor(pValue);` |
|      246 |  8362 | `		}` |
|        - |  8363 | `		/* Function arguments */` |
|      494 |  8364 | `		pArg = ph7_context_new_array(pCtx);` |
|      494 |  8365 | `		if( pArg  ){` |
|        - |  8366 | `			ph7_value *pObj;` |
|        - |  8367 | `			VmSlot *aSlot;` |
|        - |  8368 | `			sxu32 n;` |
|        - |  8369 | `			/* Start filling the array with the given arguments */` |
|      494 |  8370 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     1962 |  8371 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1470 |  8372 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1470 |  8373 | `				if( pObj ){` |
|     1470 |  8374 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      734 |  8375 | `				}` |
|      736 |  8376 | `			}` |
|        - |  8377 | `			/* Save the array */` |
|      494 |  8378 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      246 |  8379 | `		}` |
|      246 |  8380 | `	}` |
|      494 |  8381 | `	ph7_value_int(pValue,1);` |
|        - |  8382 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8383 | `	 * line numbers at run-time. )` |
|        - |  8384 | `	 */` |
|      494 |  8385 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8386 | `	/* Current processed script */` |
|      494 |  8387 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      494 |  8388 | `	if( pFile ){` |
|      494 |  8389 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      494 |  8390 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      494 |  8391 | `		ph7_value_reset_string_cursor(pValue);` |
|      246 |  8392 | `	}` |
|        - |  8393 | `	/* Top class */` |
|      494 |  8394 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      494 |  8395 | `	if( pClass ){` |
|      490 |  8396 | `		ph7_value_reset_string_cursor(pValue);` |
|      490 |  8397 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      490 |  8398 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      244 |  8399 | `	}` |
|        - |  8400 | `	/* Return the freshly created array */` |
|      494 |  8401 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8402 | `	/*` |
|        - |  8403 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8404 | `	 * as soon we return from this function.` |
|        - |  8405 | `	 */` |
|      494 |  8406 | `	return PH7_OK;` |
|      248 |  8407 |  |
|        - |  8408 | `/*` |
|        - |  8409 | ` * Generate a small backtrace.` |
|        - |  8410 | ` * Store the generated dump in the given BLOB` |
|        - |  8411 | ` */` |
|        4 |  8412 | `static int VmMiniBacktrace(` |
|        - |  8413 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8414 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8415 | `	)` |
|        1 |  8416 |  |
|        5 |  8417 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8418 | `	ph7_vm_func *pFunc;` |
|        - |  8419 | `	ph7_class *pClass;` |
|        - |  8420 | `	SyString *pFile;` |
|        - |  8421 | `	/* Called function */` |
|        5 |  8422 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8423 | `		/* Safely ignore the exception frame */` |
|      ! 0 |  8424 | `		pFrame = pFrame->pParent;` |
|      ! 0 |  8425 | `	}` |
|        5 |  8426 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8427 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8428 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8429 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8430 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8431 | `	}else{` |
|      ! 0 |  8432 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8433 | `	}` |
|        5 |  8434 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8435 | `	/* Current processed script */` |
|        5 |  8436 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8437 | `	if( pFile ){` |
|        5 |  8438 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8439 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8440 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8441 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8442 | `	}` |
|        - |  8443 | `	/* Top class */` |
|        5 |  8444 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8445 | `	if( pClass ){` |
|      ! 0 |  8446 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8447 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8448 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8449 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8450 | `	}` |
|        5 |  8451 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8452 | `	/* All done */` |
|        5 |  8453 | `	return SXRET_OK;` |
|        1 |  8454 |  |
|        - |  8455 | `/*` |
|        - |  8456 | ` * void debug_print_backtrace()` |
|        - |  8457 | ` *  Prints a backtrace` |
|        - |  8458 | ` * Parameters` |
|        - |  8459 | ` * None` |
|        - |  8460 | ` * Return` |
|        - |  8461 | ` * NULL` |
|        - |  8462 | ` */` |
|        2 |  8463 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8464 |  |
|        3 |  8465 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8466 | `	SyBlob sDump;` |
|        3 |  8467 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8468 | `	/* Generate the backtrace */` |
|        3 |  8469 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8470 | `	/* Output backtrace */` |
|        3 |  8471 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8472 | `	/* All done,cleanup */` |
|        3 |  8473 | `	SyBlobRelease(&sDump);` |
|        1 |  8474 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8475 | `	SXUNUSED(apArg);` |
|        3 |  8476 | `	return PH7_OK;` |
|        1 |  8477 |  |
|        - |  8478 | `/*` |
|        - |  8479 | ` * string debug_string_backtrace()` |
|        - |  8480 | ` *  Generate a backtrace` |
|        - |  8481 | ` * Parameters` |
|        - |  8482 | ` * None` |
|        - |  8483 | ` * Return` |
|        - |  8484 | ` *  A mini backtrace().` |
|        - |  8485 | ` * Note that this is a symisc extension.` |
|        - |  8486 | ` */` |
|        2 |  8487 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8488 |  |
|        3 |  8489 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8490 | `	SyBlob sDump;` |
|        3 |  8491 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8492 | `	/* Generate the backtrace */` |
|        3 |  8493 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8494 | `	/* Return the backtrace */` |
|        3 |  8495 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8496 | `	/* All done,cleanup */` |
|        3 |  8497 | `	SyBlobRelease(&sDump);` |
|        1 |  8498 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8499 | `	SXUNUSED(apArg);` |
|        3 |  8500 | `	return PH7_OK;` |
|        1 |  8501 |  |
|        - |  8502 | `/*` |
|        - |  8503 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8504 | ` * exception is triggered.` |
|        - |  8505 | ` */` |
|      472 |  8506 | `static sxi32 VmUncaughtException(` |
|        - |  8507 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8508 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8509 | `	)` |
|        1 |  8510 |  |
|        - |  8511 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8512 | `	int nArg = 1;` |
|        - |  8513 | `	sxi32 rc;` |
|      473 |  8514 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8515 | `		/* Nesting limit reached */` |
|      ! 0 |  8516 | `		return SXRET_OK;` |
|        - |  8517 | `	}` |
|        - |  8518 | `	/* Call any exception handler if available */` |
|      473 |  8519 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8520 | `	if( pThis ){` |
|        - |  8521 | `		/* Load the exception instance */` |
|      473 |  8522 | `		sArg.x.pOther = pThis;` |
|      473 |  8523 | `		pThis->iRef++;` |
|      473 |  8524 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8525 | `	}else{` |
|      ! 0 |  8526 | `		nArg = 0;` |
|        - |  8527 | `	}` |
|      473 |  8528 | `	apArg[0] = &sArg;` |
|        - |  8529 | `	/* Call the exception handler if available */` |
|      473 |  8530 | `	pVm->nExceptDepth++;` |
|      473 |  8531 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8532 | `	pVm->nExceptDepth--;` |
|      473 |  8533 | `	if( rc != SXRET_OK ){` |
|        - |  8534 | `		SyBlob sMsgBuf;` |
|      471 |  8535 | `		const char *zClass = "Exception";` |
|      471 |  8536 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8537 | `		const char *zMsg;` |
|        - |  8538 | `		sxu32 nMsg;` |
|        - |  8539 | `		const char *zFuncName;` |
|        - |  8540 | `		int nFuncLen;` |
|      471 |  8541 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8542 | `		if( pThis ){` |
|        - |  8543 | `			ph7_class_method *pGetMessage;` |
|        - |  8544 | `			ph7_value sMsg;` |
|        - |  8545 | `			const char *zTmp;` |
|        - |  8546 | `			int nTmp;` |
|      471 |  8547 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8548 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8549 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8550 | `			if( pGetMessage ){` |
|      471 |  8551 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8552 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8553 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8554 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8555 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8556 | `					}` |
|      235 |  8557 | `				}` |
|      471 |  8558 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8559 | `			}` |
|      235 |  8560 | `		}` |
|      471 |  8561 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8562 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8563 | `		}` |
|      471 |  8564 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8565 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8566 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8567 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8568 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8569 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8570 | `		rc = SXERR_ABORT;` |
|      235 |  8571 | `	}` |
|      473 |  8572 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8573 | `	return rc;` |
|      237 |  8574 |  |
|        - |  8575 | `/*` |
|        - |  8576 | ` * Throw an user exception.` |
|        - |  8577 | ` */` |
|      490 |  8578 | `static sxi32 VmThrowException(` |
|        - |  8579 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8580 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8581 | `	)` |
|        2 |  8582 |  |
|        - |  8583 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8584 | `	ph7_exception **apException;` |
|        - |  8585 | `	ph7_exception *pException;` |
|        - |  8586 | `	/* Point to the stack of loaded exceptions */` |
|      492 |  8587 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      492 |  8588 | `	pException = 0;` |
|      492 |  8589 | `	pCatch = 0;` |
|      492 |  8590 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8591 | `		ph7_exception_block *aCatch;` |
|        - |  8592 | `		ph7_class *pClass;` |
|        - |  8593 | `		sxu32 j;` |
|        - |  8594 | `		/* Locate the appropriate block to execute */` |
|       20 |  8595 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       20 |  8596 | `		(void)SySetPop(&pVm->aException);` |
|       20 |  8597 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       20 |  8598 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       20 |  8599 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8600 | `			/* Extract the target class */` |
|       20 |  8601 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       20 |  8602 | `			if( pClass == 0 ){` |
|        - |  8603 | `				/* No such class */` |
|      ! 0 |  8604 | `				continue;` |
|        - |  8605 | `			}` |
|       20 |  8606 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8607 | `				/* Catch block found,break immeditaley */` |
|       20 |  8608 | `				pCatch = &aCatch[j];` |
|       20 |  8609 | `				break;` |
|        - |  8610 | `			}` |
|      ! 0 |  8611 | `		}` |
|        9 |  8612 | `	}` |
|        - |  8613 | `	/* Execute the cached block if available */` |
|      492 |  8614 | `	if( pCatch == 0 ){` |
|        - |  8615 | `		sxi32 rc;` |
|      473 |  8616 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8617 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8618 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8619 | `			while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8620 | `				/* Safely ignore the exception frame */` |
|      ! 0 |  8621 | `				pFrame = pFrame->pParent;` |
|      ! 0 |  8622 | `			}` |
|      ! 0 |  8623 | `			if( pException->pFrame == pFrame ){` |
|        - |  8624 | `				/* Tell the upper layer that the exception was caught */` |
|      ! 0 |  8625 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8626 | `			}` |
|      ! 0 |  8627 | `		}` |
|      473 |  8628 | `		return rc;` |
|      ! 0 |  8629 | `	}else{` |
|       20 |  8630 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8631 | `		sxi32 rc;` |
|       30 |  8632 | `		while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - |  8633 | `			/* Safely ignore the exception frame */` |
|       12 |  8634 | `			pFrame = pFrame->pParent;` |
|        2 |  8635 | `		}` |
|       20 |  8636 | `		if( pException->pFrame == pFrame ){` |
|        - |  8637 | `			/* Tell the upper layer that the exception was caught */` |
|       12 |  8638 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        5 |  8639 | `		}` |
|        - |  8640 | `		/* Create a private frame first */` |
|       20 |  8641 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       20 |  8642 | `		if( rc == SXRET_OK ){` |
|        - |  8643 | `			/* Mark as catch frame */` |
|       20 |  8644 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       20 |  8645 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       20 |  8646 | `			if( pObj ){` |
|        - |  8647 | `				/* Install the exception instance */` |
|       20 |  8648 | `				pThis->iRef++; /* Increment reference count */` |
|       20 |  8649 | `				pObj->x.pOther = pThis;` |
|       20 |  8650 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        9 |  8651 | `			}` |
|        - |  8652 | `			/* Exceute the block */` |
|       20 |  8653 | `			VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8654 | `			/* Leave the frame */` |
|       20 |  8655 | `			VmLeaveFrame(&(*pVm));` |
|        9 |  8656 | `		}` |
|        - |  8657 | `	}` |
|        - |  8658 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  8659 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  8660 | `	 */` |
|       20 |  8661 | `	return SXRET_OK;` |
|      247 |  8662 |  |
|        - |  8663 | `/*` |
|        - |  8664 | ` * Section:` |
|        - |  8665 | ` *  Version,Credits and Copyright related functions.` |
|        - |  8666 | ` * Status:` |
|        - |  8667 | ` *    Stable.` |
|        - |  8668 | ` */` |
|        - |  8669 | `/*` |
|        - |  8670 | ` * string ph7version(void)` |
|        - |  8671 | ` *  Returns the running version of the PH7 version.` |
|        - |  8672 | ` * Parameters` |
|        - |  8673 | ` *  None` |
|        - |  8674 | ` * Return` |
|        - |  8675 | ` * Current PH7 version.` |
|        - |  8676 | ` */` |
|        2 |  8677 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8678 |  |
|        1 |  8679 | `	SXUNUSED(nArg);` |
|        1 |  8680 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  8681 | `	/* Current engine version */` |
|        3 |  8682 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  8683 | `	return PH7_OK;` |
|        1 |  8684 |  |
|        - |  8685 | `/*` |
|        - |  8686 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  8687 | ` */` |
|        - |  8688 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  8689 | ` "<html><head>"\` |
|        - |  8690 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  8691 | ` "<style type=\"text/css\">"\` |
|        - |  8692 | ` "div {"\` |
|        - |  8693 | `     "border: 1px solid #cccccc;"\` |
|        - |  8694 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  8695 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  8696 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  8697 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  8698 | `     "-webkit-border-radius: 10px;"\` |
|        - |  8699 | `     "-o-border-radius: 10px;"\` |
|        - |  8700 | `     "border-radius: 10px;"\` |
|        - |  8701 | `     "padding-left: 2em;"\` |
|        - |  8702 | `     "background-color: white;"\` |
|        - |  8703 | `     "margin-left: auto;"\` |
|        - |  8704 | `     "font-family: verdana;"\` |
|        - |  8705 | `     "padding-right: 2em;"\` |
|        - |  8706 | `     "margin-right: auto;"\` |
|        - |  8707 | `     "}"\` |
|        - |  8708 | `     "body {"\` |
|        - |  8709 | `     "padding: 0.2em;"\` |
|        - |  8710 | `     "font-style: normal;"\` |
|        - |  8711 | `     "font-size: medium;"\` |
|        - |  8712 | `     "background-color: #f2f2f2;"\` |
|        - |  8713 | `     "}"\` |
|        - |  8714 | `     "hr {"\` |
|        - |  8715 | `     "border-style: solid none none;"\` |
|        - |  8716 | `     "border-width: 1px medium medium;"\` |
|        - |  8717 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  8718 | `     "height: 1px;"\` |
|        - |  8719 | `     "}"\` |
|        - |  8720 | `     "a {"\` |
|        - |  8721 | `     "color: #3366cc;"\` |
|        - |  8722 | `     "text-decoration: none;"\` |
|        - |  8723 | `     "}"\` |
|        - |  8724 | `     "a:hover {"\` |
|        - |  8725 | `     "color: #999999;"\` |
|        - |  8726 | `     "}"\` |
|        - |  8727 | `     "a:active {"\` |
|        - |  8728 | `     "color: #663399;"\` |
|        - |  8729 | `     "}"\` |
|        - |  8730 | `     "h1 {"\` |
|        - |  8731 | `     "margin: 0;"\` |
|        - |  8732 | `     "padding: 0;"\` |
|        - |  8733 | `     "font-family: Verdana;"\` |
|        - |  8734 | `     "font-weight: bold;"\` |
|        - |  8735 | `     "font-style: normal;"\` |
|        - |  8736 | `     "font-size: medium;"\` |
|        - |  8737 | `     "text-transform: capitalize;"\` |
|        - |  8738 | `     "color: #0a328c;"\` |
|        - |  8739 | `     "}"\` |
|        - |  8740 | `     "p {"\` |
|        - |  8741 | `     "margin: 0 auto;"\` |
|        - |  8742 | `     "font-size: medium;"\` |
|        - |  8743 | `     "font-style: normal;"\` |
|        - |  8744 | `     "font-family: verdana;"\` |
|        - |  8745 | `     "}"\` |
|        - |  8746 | `"</style></head><body>"\` |
|        - |  8747 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  8748 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  8749 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  8750 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  8751 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  8752 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  8753 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  8754 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  8755 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  8756 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  8757 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  8758 |  |
|        - |  8759 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8760 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  8761 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  8762 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  8763 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8764 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  8765 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8766 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  8767 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  8768 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  8769 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  8770 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  8771 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  8772 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  8773 |  |
|        - |  8774 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  8775 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  8776 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  8777 | `"&nbsp;*<br>"\` |
|        - |  8778 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  8779 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  8780 | `"&nbsp;* are met:<br>"\` |
|        - |  8781 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  8782 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  8783 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  8784 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  8785 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  8786 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  8787 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  8788 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  8789 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  8790 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  8791 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  8792 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  8793 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  8794 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  8795 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  8796 | `"&nbsp;*<br>"\` |
|        - |  8797 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  8798 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  8799 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  8800 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  8801 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  8802 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  8803 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  8804 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  8805 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  8806 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  8807 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  8808 | `"&nbsp;*/<br>"\` |
|        - |  8809 | `"</span></small></small></p>"\` |
|        - |  8810 | `"</div></body></html>"` |
|        - |  8811 | `/*` |
|        - |  8812 | ` * bool ph7credits(void)` |
|        - |  8813 | ` * bool ph7info(void)` |
|        - |  8814 | ` * bool ph7copyright(void)` |
|        - |  8815 | ` *  Prints out the credits for PH7 engine` |
|        - |  8816 | ` * Parameters` |
|        - |  8817 | ` *  None` |
|        - |  8818 | ` * Return` |
|        - |  8819 | ` *  Always TRUE` |
|        - |  8820 | ` */` |
|        2 |  8821 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8822 |  |
|        3 |  8823 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  8824 | `	/* Expand the HTML page above*/` |
|        3 |  8825 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  8826 | `	ph7_context_output_format(` |
|        1 |  8827 | `		pCtx,` |
|        - |  8828 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  8829 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  8830 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  8831 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  8832 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  8833 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  8834 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  8835 | `#ifdef __WINNT__` |
|        - |  8836 | `		"Windows NT"` |
|        - |  8837 | `#elif defined(__UNIXES__)` |
|        - |  8838 | `		"UNIX-Like"` |
|        - |  8839 | `#else` |
|        - |  8840 | `		"Other OS"` |
|        - |  8841 | `#endif` |
|        - |  8842 | `		);` |
|        3 |  8843 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  8844 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8845 | `	SXUNUSED(apArg);` |
|        - |  8846 | `	/* Return TRUE */` |
|        - |  8847 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  8848 | `	return PH7_OK;` |
|        1 |  8849 |  |
|        - |  8850 | `/*` |
|        - |  8851 | ` * Section:` |
|        - |  8852 | ` *    URL related routines.` |
|        - |  8853 | ` * Status:` |
|        - |  8854 | ` *    Stable.` |
|        - |  8855 | ` */` |
|        - |  8856 | `/*` |
|        - |  8857 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  8858 | ` *  Parse a URL and return its fields.` |
|        - |  8859 | ` * Parameters` |
|        - |  8860 | ` *  $url` |
|        - |  8861 | ` *   The URL to parse.` |
|        - |  8862 | ` * $component` |
|        - |  8863 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  8864 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  8865 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  8866 | ` *  in which case the return value will be an integer).` |
|        - |  8867 | ` * Return` |
|        - |  8868 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  8869 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  8870 | ` *  this array are:` |
|        - |  8871 | ` *   scheme - e.g. http` |
|        - |  8872 | ` *   host` |
|        - |  8873 | ` *   port` |
|        - |  8874 | ` *   user` |
|        - |  8875 | ` *   pass` |
|        - |  8876 | ` *   path` |
|        - |  8877 | ` *   query - after the question mark ?` |
|        - |  8878 | ` *   fragment - after the hashmark #` |
|        - |  8879 | ` * Note:` |
|        - |  8880 | ` *  FALSE is returned on failure.` |
|        - |  8881 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  8882 | ` *  with the standard PHP engine.` |
|        - |  8883 | ` */` |
|       28 |  8884 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8885 |  |
|        - |  8886 | `	const char *zStr; /* Input string */` |
|        - |  8887 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  8888 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  8889 | `	int nLen;` |
|        - |  8890 | `	sxi32 rc;` |
|       29 |  8891 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  8892 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  8893 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8894 | `		return PH7_OK;` |
|        - |  8895 | `	}` |
|        - |  8896 | `	/* Extract the given URI */` |
|       29 |  8897 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  8898 | `	if( nLen < 1 ){` |
|        - |  8899 | `		/* Nothing to process,return FALSE */` |
|        3 |  8900 | `		ph7_result_bool(pCtx,0);` |
|        3 |  8901 | `		return PH7_OK;` |
|        - |  8902 | `	}` |
|        - |  8903 | `	/* Get a parse */` |
|       27 |  8904 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  8905 | `	if( rc != SXRET_OK ){` |
|        - |  8906 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  8907 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8908 | `		return PH7_OK;` |
|        - |  8909 | `	}` |
|       27 |  8910 | `	if( nArg > 1 ){` |
|      ! 0 |  8911 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  8912 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  8913 | `		switch(nComponent){` |
|      ! 0 |  8914 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  8915 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  8916 | `			if( pComp->nByte < 1 ){` |
|        - |  8917 | `				/* No available value,return NULL */` |
|      ! 0 |  8918 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8919 | `			}else{` |
|      ! 0 |  8920 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8921 | `			}` |
|      ! 0 |  8922 | `			break;` |
|      ! 0 |  8923 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  8924 | `			pComp = &sURI.sHost;` |
|      ! 0 |  8925 | `			if( pComp->nByte < 1 ){` |
|        - |  8926 | `				/* No available value,return NULL */` |
|      ! 0 |  8927 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8928 | `			}else{` |
|      ! 0 |  8929 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8930 | `			}` |
|      ! 0 |  8931 | `			break;` |
|      ! 0 |  8932 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  8933 | `			pComp = &sURI.sPort;` |
|      ! 0 |  8934 | `			if( pComp->nByte < 1 ){` |
|        - |  8935 | `				/* No available value,return NULL */` |
|      ! 0 |  8936 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8937 | `			}else{` |
|      ! 0 |  8938 | `				int iPort = 0;` |
|        - |  8939 | `				/* Cast the value to integer */` |
|      ! 0 |  8940 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  8941 | `				ph7_result_int(pCtx,iPort);` |
|        - |  8942 | `			}` |
|      ! 0 |  8943 | `			break;` |
|      ! 0 |  8944 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  8945 | `			pComp = &sURI.sUser;` |
|      ! 0 |  8946 | `			if( pComp->nByte < 1 ){` |
|        - |  8947 | `				/* No available value,return NULL */` |
|      ! 0 |  8948 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8949 | `			}else{` |
|      ! 0 |  8950 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8951 | `			}` |
|      ! 0 |  8952 | `			break;` |
|      ! 0 |  8953 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  8954 | `			pComp = &sURI.sPass;` |
|      ! 0 |  8955 | `			if( pComp->nByte < 1 ){` |
|        - |  8956 | `				/* No available value,return NULL */` |
|      ! 0 |  8957 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8958 | `			}else{` |
|      ! 0 |  8959 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8960 | `			}` |
|      ! 0 |  8961 | `			break;` |
|      ! 0 |  8962 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  8963 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  8964 | `			if( pComp->nByte < 1 ){` |
|        - |  8965 | `				/* No available value,return NULL */` |
|      ! 0 |  8966 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8967 | `			}else{` |
|      ! 0 |  8968 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8969 | `			}` |
|      ! 0 |  8970 | `			break;` |
|      ! 0 |  8971 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  8972 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  8973 | `			if( pComp->nByte < 1 ){` |
|        - |  8974 | `				/* No available value,return NULL */` |
|      ! 0 |  8975 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8976 | `			}else{` |
|      ! 0 |  8977 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8978 | `			}` |
|      ! 0 |  8979 | `			break;` |
|      ! 0 |  8980 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  8981 | `			pComp = &sURI.sPath;` |
|      ! 0 |  8982 | `			if( pComp->nByte < 1 ){` |
|        - |  8983 | `				/* No available value,return NULL */` |
|      ! 0 |  8984 | `				ph7_result_null(pCtx);` |
|      ! 0 |  8985 | `			}else{` |
|      ! 0 |  8986 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  8987 | `			}` |
|      ! 0 |  8988 | `			break;` |
|      ! 0 |  8989 | `		default:` |
|        - |  8990 | `			/* No such entry,return NULL */` |
|      ! 0 |  8991 | `			ph7_result_null(pCtx);` |
|      ! 0 |  8992 | `			break;` |
|        - |  8993 | `		}` |
|      ! 0 |  8994 | `	}else{` |
|        - |  8995 | `		ph7_value *pArray,*pValue;` |
|        - |  8996 | `		/* Return an associative array */` |
|       27 |  8997 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  8998 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  8999 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9000 | `			/* Out of memory */` |
|      ! 0 |  9001 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9002 | `			/* Return false */` |
|      ! 0 |  9003 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9004 | `			return PH7_OK;` |
|        - |  9005 | `		}` |
|        - |  9006 | `		/* Fill the array */` |
|       27 |  9007 | `		pComp = &sURI.sScheme;` |
|       27 |  9008 | `		if( pComp->nByte > 0 ){` |
|       19 |  9009 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9010 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9011 | `		}` |
|        - |  9012 | `		/* Reset the string cursor */` |
|       27 |  9013 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9014 | `		pComp = &sURI.sHost;` |
|       27 |  9015 | `		if( pComp->nByte > 0 ){` |
|       25 |  9016 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9017 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9018 | `		}` |
|        - |  9019 | `		/* Reset the string cursor */` |
|       27 |  9020 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9021 | `		pComp = &sURI.sPort;` |
|       27 |  9022 | `		if( pComp->nByte > 0 ){` |
|       11 |  9023 | `			int iPort = 0;/* cc warning */` |
|        - |  9024 | `			/* Convert to integer */` |
|       11 |  9025 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9026 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9027 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9028 | `		}` |
|        - |  9029 | `		/* Reset the string cursor */` |
|       27 |  9030 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9031 | `		pComp = &sURI.sUser;` |
|       27 |  9032 | `		if( pComp->nByte > 0 ){` |
|        7 |  9033 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9034 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9035 | `		}` |
|        - |  9036 | `		/* Reset the string cursor */` |
|       27 |  9037 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9038 | `		pComp = &sURI.sPass;` |
|       27 |  9039 | `		if( pComp->nByte > 0 ){` |
|        7 |  9040 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9041 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9042 | `		}` |
|        - |  9043 | `		/* Reset the string cursor */` |
|       27 |  9044 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9045 | `		pComp = &sURI.sPath;` |
|       27 |  9046 | `		if( pComp->nByte > 0 ){` |
|       17 |  9047 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9048 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9049 | `		}` |
|        - |  9050 | `		/* Reset the string cursor */` |
|       27 |  9051 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9052 | `		pComp = &sURI.sQuery;` |
|       27 |  9053 | `		if( pComp->nByte > 0 ){` |
|        5 |  9054 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9055 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9056 | `		}` |
|        - |  9057 | `		/* Reset the string cursor */` |
|       27 |  9058 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9059 | `		pComp = &sURI.sFragment;` |
|       27 |  9060 | `		if( pComp->nByte > 0 ){` |
|        5 |  9061 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9062 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9063 | `		}` |
|        - |  9064 | `		/* Return the created array */` |
|       27 |  9065 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9066 | `		/* NOTE:` |
|        - |  9067 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9068 | `		 * automatically as soon we return from this function.` |
|        - |  9069 | `		 */` |
|        - |  9070 | `	}` |
|        - |  9071 | `	/* All done */` |
|       27 |  9072 | `	return PH7_OK;` |
|       15 |  9073 |  |
|        - |  9074 | `/*` |
|        - |  9075 | ` * Section:` |
|        - |  9076 | ` *   Array related routines.` |
|        - |  9077 | ` * Status:` |
|        - |  9078 | ` *    Stable.` |
|        - |  9079 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9080 | ` *  Array related functions that need access to the underlying` |
|        - |  9081 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9082 | ` */` |
|        - |  9083 | `/*` |
|        - |  9084 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9085 | ` * of the following structure.` |
|        - |  9086 | ` */` |
|        - |  9087 | `struct compact_data` |
|        - |  9088 |  |
|        - |  9089 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9090 | `	int nRecCount;      /* Recursion count */` |
|        - |  9091 | `};` |
|        - |  9092 | `/*` |
|        - |  9093 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9094 | ` */` |
|      ! 0 |  9095 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9096 |  |
|      ! 0 |  9097 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9098 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9099 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9100 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9101 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9102 | `		SyString sVar;` |
|      ! 0 |  9103 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9104 | `		if( sVar.nByte > 0 ){` |
|        - |  9105 | `			/* Query the current frame */` |
|      ! 0 |  9106 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9107 | `			/* ^` |
|        - |  9108 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9109 | `			 */` |
|      ! 0 |  9110 | `			if( pKey ){` |
|        - |  9111 | `				/* Perform the insertion */` |
|      ! 0 |  9112 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9113 | `			}` |
|      ! 0 |  9114 | `		}` |
|      ! 0 |  9115 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9116 | `		int rc;` |
|        - |  9117 | `		/* Recursively traverse this array */` |
|      ! 0 |  9118 | `		pData->nRecCount++;` |
|      ! 0 |  9119 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9120 | `		pData->nRecCount--;` |
|      ! 0 |  9121 | `		return rc;` |
|        - |  9122 | `	}` |
|      ! 0 |  9123 | `	return SXRET_OK;` |
|      ! 0 |  9124 |  |
|        - |  9125 | `/*` |
|        - |  9126 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9127 | ` *  Create array containing variables and their values.` |
|        - |  9128 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9129 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9130 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9131 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9132 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9133 | ` * Parameters` |
|        - |  9134 | ` *  $varname` |
|        - |  9135 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9136 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9137 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9138 | ` *   it recursively.` |
|        - |  9139 | ` * Return` |
|        - |  9140 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9141 | ` */` |
|        2 |  9142 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9143 |  |
|        - |  9144 | `	ph7_value *pArray,*pObj;` |
|        3 |  9145 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9146 | `	const char *zName;` |
|        - |  9147 | `	SyString sVar;` |
|        - |  9148 | `	int i,nLen;` |
|        3 |  9149 | `	if( nArg < 1 ){` |
|        - |  9150 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9151 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9152 | `		return PH7_OK;` |
|        - |  9153 | `	}` |
|        - |  9154 | `	/* Create the array */` |
|        3 |  9155 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9156 | `	if( pArray == 0 ){` |
|        - |  9157 | `		/* Out of memory */` |
|      ! 0 |  9158 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9159 | `		/* Return NULL */` |
|      ! 0 |  9160 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9161 | `		return PH7_OK;` |
|        - |  9162 | `	}` |
|        - |  9163 | `	/* Perform the requested operation */` |
|        7 |  9164 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9165 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9166 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9167 | `				struct compact_data sData;` |
|      ! 0 |  9168 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9169 | `				/* Recursively walk the array */` |
|      ! 0 |  9170 | `				sData.nRecCount = 0;` |
|      ! 0 |  9171 | `				sData.pArray = pArray;` |
|      ! 0 |  9172 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9173 | `			}` |
|      ! 0 |  9174 | `		}else{` |
|        - |  9175 | `			/* Extract variable name */` |
|        5 |  9176 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9177 | `			if( nLen > 0 ){` |
|        5 |  9178 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9179 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9180 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9181 | `				if( pObj ){` |
|        5 |  9182 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9183 | `				}` |
|        2 |  9184 | `			}` |
|        - |  9185 | `		}` |
|        3 |  9186 | `	}` |
|        - |  9187 | `	/* Return the array */` |
|        3 |  9188 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9189 | `	return PH7_OK;` |
|        2 |  9190 |  |
|        - |  9191 | `/*` |
|        - |  9192 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9193 | ` * of the following structure.` |
|        - |  9194 | ` */` |
|        - |  9195 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9196 | `struct extract_aux_data` |
|        - |  9197 |  |
|        - |  9198 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9199 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9200 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9201 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9202 | `	int iFlags;           /* Control flags */` |
|        - |  9203 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9204 | `};` |
|        - |  9205 | `/* Forward declaration */` |
|        - |  9206 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9207 | `/*` |
|        - |  9208 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9209 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9210 | ` * Parameters` |
|        - |  9211 | ` * $var_array` |
|        - |  9212 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9213 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9214 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9215 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9216 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9217 | ` * $extract_type` |
|        - |  9218 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9219 | ` *  It can be one of the following values:` |
|        - |  9220 | ` *   EXTR_OVERWRITE` |
|        - |  9221 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9222 | ` *   EXTR_SKIP` |
|        - |  9223 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9224 | ` *   EXTR_PREFIX_SAME` |
|        - |  9225 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9226 | ` *   EXTR_PREFIX_ALL` |
|        - |  9227 | ` *       Prefix all variable names with prefix.` |
|        - |  9228 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9229 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9230 | ` *   EXTR_IF_EXISTS` |
|        - |  9231 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9232 | ` *       otherwise do nothing.` |
|        - |  9233 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9234 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9235 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9236 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9237 | ` *      the current symbol table.` |
|        - |  9238 | ` * $prefix` |
|        - |  9239 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9240 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9241 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9242 | ` *  underscore character.` |
|        - |  9243 | ` * Return` |
|        - |  9244 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9245 | ` */` |
|        4 |  9246 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9247 |  |
|        - |  9248 | `	extract_aux_data sAux;` |
|        - |  9249 | `	ph7_hashmap *pMap;` |
|        5 |  9250 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9251 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9252 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9253 | `		return PH7_OK;` |
|        - |  9254 | `	}` |
|        - |  9255 | `	/* Point to the target hashmap */` |
|        5 |  9256 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9257 | `	if( pMap->nEntry < 1 ){` |
|        - |  9258 | `		/* Empty map,return  0 */` |
|      ! 0 |  9259 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9260 | `		return PH7_OK;` |
|        - |  9261 | `	}` |
|        - |  9262 | `	/* Prepare the aux data */` |
|        5 |  9263 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9264 | `	if( nArg > 1 ){` |
|        3 |  9265 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9266 | `		if( nArg > 2 ){` |
|      ! 0 |  9267 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9268 | `		}` |
|        1 |  9269 | `	}` |
|        5 |  9270 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9271 | `	/* Invoke the worker callback */` |
|        5 |  9272 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9273 | `	/* Number of variables successfully imported */` |
|        5 |  9274 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9275 | `	return PH7_OK;` |
|        3 |  9276 |  |
|        - |  9277 | `/*` |
|        - |  9278 | ` * Worker callback for the [extract()] function defined` |
|        - |  9279 | ` * below.` |
|        - |  9280 | ` */` |
|        8 |  9281 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9282 |  |
|        9 |  9283 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9284 | `	int iFlags = pAux->iFlags;` |
|        9 |  9285 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9286 | `	ph7_value *pObj;` |
|        - |  9287 | `	SyString sVar;` |
|        9 |  9288 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9289 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9290 | `	}` |
|        - |  9291 | `	/* Perform a string cast */` |
|        9 |  9292 | `	PH7_MemObjToString(pKey);` |
|        9 |  9293 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9294 | `		/* Unavailable variable name */` |
|      ! 0 |  9295 | `		return SXRET_OK;` |
|        - |  9296 | `	}` |
|        9 |  9297 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9298 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9299 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9300 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9301 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9302 | `			);` |
|      ! 0 |  9303 | `	}else{` |
|       13 |  9304 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9305 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9306 | `	}` |
|        9 |  9307 | `	sVar.zString = pAux->zWorker;` |
|        - |  9308 | `	/* Try to extract the variable */` |
|        9 |  9309 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9310 | `	if( pObj ){` |
|        - |  9311 | `		/* Collision */` |
|        5 |  9312 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9313 | `			return SXRET_OK;` |
|        - |  9314 | `		}` |
|        5 |  9315 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9316 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9317 | `				/* Already prefixed */` |
|      ! 0 |  9318 | `				return SXRET_OK;` |
|        - |  9319 | `			}` |
|      ! 0 |  9320 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9321 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9322 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9323 | `				);` |
|      ! 0 |  9324 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9325 | `		}` |
|        3 |  9326 | `	}else{` |
|        - |  9327 | `		/* Create the variable */` |
|        5 |  9328 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9329 | `	}` |
|        9 |  9330 | `	if( pObj ){` |
|        - |  9331 | `		/* Overwrite the old value */` |
|        9 |  9332 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9333 | `		/* Increment counter */` |
|        9 |  9334 | `		pAux->iCount++;` |
|        4 |  9335 | `	}` |
|        9 |  9336 | `	return SXRET_OK;` |
|        5 |  9337 |  |
|        - |  9338 | `/*` |
|        - |  9339 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9340 | ` * defined below.` |
|        - |  9341 | ` */` |
|        2 |  9342 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9343 |  |
|        3 |  9344 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9345 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9346 | `	ph7_value *pObj;` |
|        - |  9347 | `	SyString sVar;` |
|        - |  9348 | `	/* Perform a string cast */` |
|        3 |  9349 | `	PH7_MemObjToString(pKey);` |
|        3 |  9350 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9351 | `		/* Unavailable variable name */` |
|      ! 0 |  9352 | `		return SXRET_OK;` |
|        - |  9353 | `	}` |
|        3 |  9354 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9355 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9356 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9357 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9358 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9359 | `			);` |
|        2 |  9360 | `	}else{` |
|      ! 0 |  9361 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9362 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9363 | `	}` |
|        3 |  9364 | `	sVar.zString = pAux->zWorker;` |
|        - |  9365 | `	/* Extract the variable */` |
|        3 |  9366 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9367 | `	if( pObj ){` |
|        3 |  9368 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9369 | `	}` |
|        3 |  9370 | `	return SXRET_OK;` |
|        2 |  9371 |  |
|        - |  9372 | `/*` |
|        - |  9373 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9374 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9375 | ` * Parameters` |
|        - |  9376 | ` * $types` |
|        - |  9377 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9378 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9379 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9380 | ` *  POST includes the POST uploaded file information.` |
|        - |  9381 | ` *  Note:` |
|        - |  9382 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9383 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9384 | ` * $prefix` |
|        - |  9385 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9386 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9387 | ` *  variable named $pref_userid.` |
|        - |  9388 | ` * Return` |
|        - |  9389 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9390 | ` */` |
|        2 |  9391 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9392 |  |
|        - |  9393 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9394 | `	extract_aux_data sAux;` |
|        - |  9395 | `	int nLen,nPrefixLen;` |
|        - |  9396 | `	ph7_value *pSuper;` |
|        - |  9397 | `	ph7_vm *pVm;` |
|        - |  9398 | `	/* By default import only $_GET variables  */` |
|        3 |  9399 | `	zImport = "G";` |
|        3 |  9400 | `	nLen = (int)sizeof(char);` |
|        3 |  9401 | `	zPrefix = 0;` |
|        3 |  9402 | `	nPrefixLen = 0;` |
|        3 |  9403 | `	if( nArg > 0 ){` |
|        3 |  9404 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9405 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9406 | `		}` |
|        3 |  9407 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9408 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9409 | `		}` |
|        1 |  9410 | `	}` |
|        - |  9411 | `	/* Point to the underlying VM */` |
|        3 |  9412 | `	pVm = pCtx->pVm;` |
|        - |  9413 | `	/* Initialize the aux data */` |
|        3 |  9414 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9415 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9416 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9417 | `	sAux.pVm = pVm;` |
|        - |  9418 | `	/* Extract */` |
|        3 |  9419 | `	zEnd = &zImport[nLen];` |
|        5 |  9420 | `	while( zImport < zEnd ){` |
|        3 |  9421 | `		int c = zImport[0];` |
|        3 |  9422 | `		pSuper = 0;` |
|        3 |  9423 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9424 | `			/* Import $_GET variables */` |
|        3 |  9425 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9426 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9427 | `			/* Import $_POST variables */` |
|      ! 0 |  9428 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9429 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9430 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9431 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9432 | `		}` |
|        3 |  9433 | `		if( pSuper ){` |
|        - |  9434 | `			/* Iterate throw array entries */` |
|        3 |  9435 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9436 | `		}` |
|        - |  9437 | `		/* Advance the cursor */` |
|        3 |  9438 | `		zImport++;` |
|        1 |  9439 | `	}` |
|        - |  9440 | `	/* All done,return TRUE*/` |
|        3 |  9441 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9442 | `	return PH7_OK;` |
|        1 |  9443 |  |
|        - |  9444 | `/*` |
|        - |  9445 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9446 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9447 | ` * information.` |
|        - |  9448 | ` */` |
|     9812 |  9449 | `static sxi32 VmEvalChunk(` |
|        - |  9450 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9451 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9452 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9453 | `	int iFlags,         /* Compile flag */` |
|        - |  9454 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9455 | `	)` |
|        2 |  9456 |  |
|        - |  9457 | `	SySet *pByteCode,aByteCode;` |
|     9814 |  9458 | `	ProcConsumer xErr = 0;` |
|     9814 |  9459 | `	void *pErrData = 0;` |
|        - |  9460 | `	/* Initialize bytecode container */` |
|     9814 |  9461 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     9814 |  9462 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9463 | `	/* Reset the code generator */` |
|     9814 |  9464 | `	if( bTrueReturn ){` |
|        - |  9465 | `		/* Included file,log compile-time errors */` |
|     7531 |  9466 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7531 |  9467 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3765 |  9468 | `	}` |
|     9814 |  9469 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9470 | `	/* Swap bytecode container */` |
|     9814 |  9471 | `	pByteCode = pVm->pByteContainer;` |
|     9814 |  9472 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9473 | `	/* Compile the chunk */` |
|     9814 |  9474 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    14720 |  9475 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9476 | `		/* Compilation error,return false */` |
|        3 |  9477 | `		if( pCtx ){` |
|        3 |  9478 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9479 | `		}` |
|        2 |  9480 | `	}else{` |
|        - |  9481 | `		/* Mount any newly defined classes */` |
|        - |  9482 | `		SyHashEntry *pEntry;` |
|        - |  9483 | `		ph7_class *pClass;` |
|        - |  9484 | `		ph7_value sResult; /* Return value */` |
|        - |  9485 | `		sxi32 rc;` |
|     9812 |  9486 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   272365 |  9487 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   257650 |  9488 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9489 | `			/* Only mount classes that haven't been mounted yet */` |
|   257650 |  9490 | `			if( !pClass->bMounted ){` |
|    59288 |  9491 | `				rc = VmMountUserClass(pVm,pClass);` |
|    59288 |  9492 | `				if( rc != SXRET_OK ){` |
|        - |  9493 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9494 | `					if( pCtx ){` |
|      ! 0 |  9495 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9496 | `					}` |
|      ! 0 |  9497 | `					goto Cleanup;` |
|        - |  9498 | `				}` |
|    29643 |  9499 | `			}` |
|        2 |  9500 | `		}` |
|     9812 |  9501 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9502 | `			/* Out of memory */` |
|      ! 0 |  9503 | `			if( pCtx ){` |
|      ! 0 |  9504 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9505 | `			}` |
|      ! 0 |  9506 | `			goto Cleanup;` |
|        - |  9507 | `		}` |
|     9812 |  9508 | `		if( bTrueReturn ){` |
|        - |  9509 | `			/* Assume a boolean true return value */` |
|     7531 |  9510 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3766 |  9511 | `		}else{` |
|        - |  9512 | `			/* Assume a null return value */` |
|     2282 |  9513 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9514 | `		}` |
|        - |  9515 | `		/* Execute the compiled chunk */` |
|     9812 |  9516 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|     9812 |  9517 | `		if( pCtx ){` |
|        - |  9518 | `			/* Set the execution result */` |
|     7544 |  9519 | `			ph7_result_value(pCtx,&sResult);` |
|     3771 |  9520 | `		}` |
|     9812 |  9521 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9522 | `	}` |
|     4906 |  9523 | `Cleanup:` |
|        - |  9524 | `	/* Cleanup the mess left behind */` |
|     9814 |  9525 | `	pVm->pByteContainer = pByteCode;` |
|     9814 |  9526 | `	SySetRelease(&aByteCode);` |
|     9814 |  9527 | `	return SXRET_OK;` |
|        2 |  9528 |  |
|        - |  9529 | `/*` |
|        - |  9530 | ` * value eval(string $code)` |
|        - |  9531 | ` *   Evaluate a string as PHP code.` |
|        - |  9532 | ` * Parameter` |
|        - |  9533 | ` *  code: PHP code to evaluate.` |
|        - |  9534 | ` * Return` |
|        - |  9535 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9536 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9537 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9538 | ` */` |
|       16 |  9539 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9540 |  |
|        - |  9541 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9542 | `	if( nArg < 1 ){` |
|        - |  9543 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9544 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9545 | `		return SXRET_OK;` |
|        - |  9546 | `	}` |
|        - |  9547 | `	/* Chunk to evaluate */` |
|       18 |  9548 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9549 | `	if( sChunk.nByte < 1 ){` |
|        - |  9550 | `		/* Empty string,return NULL */` |
|        3 |  9551 | `		ph7_result_null(pCtx);` |
|        3 |  9552 | `		return SXRET_OK;` |
|        - |  9553 | `	}` |
|        - |  9554 | `	/* Eval the chunk */` |
|       16 |  9555 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9556 | `	return SXRET_OK;` |
|       10 |  9557 |  |
|        - |  9558 | `/*` |
|        - |  9559 | ` * Check if a file path is already included.` |
|        - |  9560 | ` */` |
|    15056 |  9561 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9562 |  |
|        - |  9563 | `	SyString *aEntries;` |
|        - |  9564 | `	sxu32 n;` |
|    15057 |  9565 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9566 | `	/* Perform a linear search */` |
| 56660431 |  9567 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56645381 |  9568 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9569 | `			/* Already included */` |
|        7 |  9570 | `			return TRUE;` |
|        - |  9571 | `		}` |
| 28322688 |  9572 | `	}` |
|    15051 |  9573 | `	return FALSE;` |
|     7529 |  9574 |  |
|        - |  9575 | `/*` |
|        - |  9576 | ` * Push a file path in the appropriate VM container.` |
|        - |  9577 | ` */` |
|    17316 |  9578 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9579 |  |
|        - |  9580 | `	SyString sPath;` |
|        - |  9581 | `	char *zDup;` |
|        - |  9582 | `#ifdef __WINNT__` |
|        - |  9583 | `	char *zCur;` |
|        - |  9584 | `#endif` |
|        - |  9585 | `	sxi32 rc;` |
|    17318 |  9586 | `	if( nLen < 0 ){` |
|     2262 |  9587 | `		nLen = SyStrlen(zPath);` |
|     1130 |  9588 | `	}` |
|        - |  9589 | `	/* Duplicate the file path first */` |
|    17318 |  9590 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17318 |  9591 | `	if( zDup == 0 ){` |
|      ! 0 |  9592 | `		return SXERR_MEM;` |
|        - |  9593 | `	}` |
|        - |  9594 | `#ifdef __WINNT__` |
|        - |  9595 | `	/* Normalize path on windows` |
|        - |  9596 | `	 * Example:` |
|        - |  9597 | `	 *    Path/To/File.php` |
|        - |  9598 | `	 * becomes` |
|        - |  9599 | `	 *   path\to\file.php` |
|        - |  9600 | `	 */` |
|        2 |  9601 | `	zCur = zDup;` |
|        2 |  9602 | `	while( zCur[0] != 0 ){` |
|        2 |  9603 | `		if( zCur[0] == '/' ){` |
|        2 |  9604 | `			zCur[0] = '\\';` |
|        2 |  9605 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 |  9606 | `			int c = SyToLower(zCur[0]);` |
|        1 |  9607 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - |  9608 | `		}` |
|        2 |  9609 | `		zCur++;` |
|        2 |  9610 | `	}` |
|        - |  9611 | `#endif` |
|        - |  9612 | `	/* Install the file path */` |
|    17318 |  9613 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17318 |  9614 | `	if( !bMain ){` |
|    15057 |  9615 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - |  9616 | `			/* Already included */` |
|        7 |  9617 | `			*pNew = 0;` |
|        4 |  9618 | `		}else{` |
|        - |  9619 | `			/* Insert in the corresponding container */` |
|    15051 |  9620 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15051 |  9621 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  9622 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 |  9623 | `				return rc;` |
|        - |  9624 | `			}` |
|    15051 |  9625 | `			*pNew = 1;` |
|        - |  9626 | `		}` |
|     7528 |  9627 | `	}` |
|    17318 |  9628 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17318 |  9629 | `	return SXRET_OK;` |
|     8660 |  9630 |  |
|        - |  9631 | `/*` |
|        - |  9632 | ` * Compile and Execute a PHP script at run-time.` |
|        - |  9633 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - |  9634 | ` * indicates failure.` |
|        - |  9635 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - |  9636 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - |  9637 | ` * operations.` |
|        - |  9638 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - |  9639 | ` * this function is a no-op.` |
|        - |  9640 | ` * Refer to the implementation of the include(),include_once() language` |
|        - |  9641 | ` * constructs for more information.` |
|        - |  9642 | ` */` |
|     7536 |  9643 | `static sxi32 VmExecIncludedFile(` |
|        - |  9644 | `	 ph7_context *pCtx, /* Call Context */` |
|        - |  9645 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - |  9646 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - |  9647 | `	 )` |
|        2 |  9648 |  |
|        - |  9649 | `	sxi32 rc;` |
|        - |  9650 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9651 | `	const ph7_io_stream *pStream;` |
|        - |  9652 | `	SyBlob sContents;` |
|        - |  9653 | `	void *pHandle;` |
|        - |  9654 | `	ph7_vm *pVm;` |
|        - |  9655 | `	int isNew;` |
|        - |  9656 | `	/* Initialize fields */` |
|     7538 |  9657 | `	pVm = pCtx->pVm;` |
|     7538 |  9658 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7538 |  9659 | `	isNew = 0;` |
|        - |  9660 | `	/* Extract the associated stream */` |
|     7538 |  9661 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - |  9662 | `	/*` |
|        - |  9663 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - |  9664 | `	 * in a read-only mode.` |
|        - |  9665 | `	 */` |
|     7538 |  9666 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7538 |  9667 | `	if( pHandle == 0 ){` |
|        3 |  9668 | `		return SXERR_IO;` |
|        - |  9669 | `	}` |
|     7535 |  9670 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7535 |  9671 | `	if( IncludeOnce && !isNew ){` |
|        - |  9672 | `		/* Already included */` |
|        5 |  9673 | `		rc = SXERR_EXISTS;` |
|        3 |  9674 | `	}else{` |
|        - |  9675 | `		/* Read the whole file contents */` |
|     7531 |  9676 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7531 |  9677 | `		if( rc == SXRET_OK ){` |
|        - |  9678 | `			SyString sScript;` |
|        - |  9679 | `			/* Compile and execute the script */` |
|     7531 |  9680 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7531 |  9681 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3765 |  9682 | `		}` |
|        - |  9683 | `	}` |
|        - |  9684 | `	/* Pop from the set of included file */` |
|     7535 |  9685 | `	(void)SySetPop(&pVm->aFiles);` |
|        - |  9686 | `	/* Close the handle */` |
|     7535 |  9687 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - |  9688 | `	/* Release the working buffer */` |
|     7535 |  9689 | `	SyBlobRelease(&sContents);` |
|        - |  9690 | `#else` |
|        - |  9691 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - |  9692 | `	SXUNUSED(pPath);` |
|        - |  9693 | `	SXUNUSED(IncludeOnce);` |
|        - |  9694 | `	rc = SXERR_IO;` |
|        - |  9695 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7535 |  9696 | `	return rc;` |
|     3770 |  9697 |  |
|        - |  9698 | `/*` |
|        - |  9699 | ` * string get_include_path(void)` |
|        - |  9700 | ` *  Gets the current include_path configuration option.` |
|        - |  9701 | ` * Parameter` |
|        - |  9702 | ` *  None` |
|        - |  9703 | ` * Return` |
|        - |  9704 | ` *  Included paths as a string` |
|        - |  9705 | ` */` |
|        2 |  9706 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9707 |  |
|        3 |  9708 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9709 | `	SyString *aEntry;` |
|        - |  9710 | `	int dir_sep;` |
|        - |  9711 | `	sxu32 n;` |
|        - |  9712 | `#ifdef __WINNT__` |
|        1 |  9713 | `	dir_sep = ';';` |
|        - |  9714 | `#else` |
|        - |  9715 | `	/* Assume UNIX path separator */` |
|        2 |  9716 | `	dir_sep = ':';` |
|        - |  9717 | `#endif` |
|        1 |  9718 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9719 | `	SXUNUSED(apArg);` |
|        - |  9720 | `	/* Point to the list of import paths */` |
|        3 |  9721 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 |  9722 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 |  9723 | `		SyString *pEntry = &aEntry[n];` |
|        3 |  9724 | `		if( n > 0 ){` |
|        - |  9725 | `			/* Append dir seprator */` |
|      ! 0 |  9726 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 |  9727 | `		}` |
|        - |  9728 | `		/* Append path */` |
|        3 |  9729 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 |  9730 | `	}` |
|        3 |  9731 | `	return PH7_OK;` |
|        1 |  9732 |  |
|        - |  9733 | `/*` |
|        - |  9734 | ` * string get_get_included_files(void)` |
|        - |  9735 | ` *  Gets the current include_path configuration option.` |
|        - |  9736 | ` * Parameter` |
|        - |  9737 | ` *  None` |
|        - |  9738 | ` * Return` |
|        - |  9739 | ` *  Included paths as a string` |
|        - |  9740 | ` */` |
|        2 |  9741 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9742 |  |
|        3 |  9743 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - |  9744 | `	ph7_value *pArray,*pWorker;` |
|        - |  9745 | `	SyString *pEntry;` |
|        - |  9746 | `	int c,d;` |
|        - |  9747 | `	/* Create an array and a working value */` |
|        3 |  9748 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 |  9749 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 |  9750 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - |  9751 | `		/* Out of memory,return null */` |
|      ! 0 |  9752 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9753 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  9754 | `		SXUNUSED(apArg);` |
|      ! 0 |  9755 | `		return PH7_OK;` |
|        - |  9756 | `	}` |
|        3 |  9757 | `	c = d = '/';` |
|        - |  9758 | `#ifdef __WINNT__` |
|        1 |  9759 | `	d = '\\';` |
|        - |  9760 | `#endif` |
|        - |  9761 | `	/* Iterate throw entries */` |
|        3 |  9762 | `	SySetResetCursor(pFiles);` |
|     3691 |  9763 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - |  9764 | `		const char *zBase,*zEnd;` |
|        - |  9765 | `		int iLen;` |
|        - |  9766 | `		/* reset the string cursor */` |
|     3689 |  9767 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - |  9768 | `		/* Extract base name */` |
|     3689 |  9769 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - |  9770 | `		/* Ignore trailing '/' */` |
|     5533 |  9771 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 |  9772 | `			zEnd--;` |
|      ! 0 |  9773 | `		}` |
|     3689 |  9774 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 |  9775 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 |  9776 | `			zEnd--;` |
|        1 |  9777 | `		}` |
|     3689 |  9778 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 |  9779 | `		zEnd = &pEntry->zString[iLen];` |
|        - |  9780 | `		/* Copy entry name */` |
|     3689 |  9781 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - |  9782 | `		/* Perform the insertion */` |
|     3689 |  9783 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 |  9784 | `	}` |
|        - |  9785 | `	/* All done,return the created array */` |
|        3 |  9786 | `	ph7_result_value(pCtx,pArray);` |
|        - |  9787 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - |  9788 | `	 * by the engine as soon we return from this foreign` |
|        - |  9789 | `	 * function.` |
|        - |  9790 | `	 */` |
|        3 |  9791 | `	return PH7_OK;` |
|        2 |  9792 |  |
|        - |  9793 | `/*` |
|        - |  9794 | ` * include:` |
|        - |  9795 | ` * According to the PHP reference manual.` |
|        - |  9796 | ` *  The include() function includes and evaluates the specified file.` |
|        - |  9797 | ` *  Files are included based on the file path given or, if none is given` |
|        - |  9798 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - |  9799 | ` *  include() will finally check in the calling script's own directory` |
|        - |  9800 | ` *  and the current working directory before failing. The include()` |
|        - |  9801 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - |  9802 | ` *  behavior from require(), which will emit a fatal error.` |
|        - |  9803 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - |  9804 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - |  9805 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - |  9806 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - |  9807 | ` *  directory to find the requested file.` |
|        - |  9808 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - |  9809 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - |  9810 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - |  9811 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - |  9812 | ` */` |
|     7524 |  9813 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9814 |  |
|        - |  9815 | `	SyString sFile;` |
|        - |  9816 | `	sxi32 rc;` |
|     7526 |  9817 | `	if( nArg < 1 ){` |
|        - |  9818 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9819 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9820 | `		return SXRET_OK;` |
|        - |  9821 | `	}` |
|        - |  9822 | `	/* File to include */` |
|     7526 |  9823 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7526 |  9824 | `	if( sFile.nByte < 1 ){` |
|        - |  9825 | `		/* Empty string,return NULL */` |
|      ! 0 |  9826 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9827 | `		return SXRET_OK;` |
|        - |  9828 | `	}` |
|        - |  9829 | `	/* Open,compile and execute the desired script */` |
|     7526 |  9830 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7526 |  9831 | `	if( rc != SXRET_OK ){` |
|        - |  9832 | `		/* Emit a warning and return false */` |
|        3 |  9833 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 |  9834 | `		ph7_result_bool(pCtx,0);` |
|        1 |  9835 | `	}` |
|     7526 |  9836 | `	return SXRET_OK;` |
|     3764 |  9837 |  |
|        - |  9838 | `/*` |
|        - |  9839 | ` * include_once:` |
|        - |  9840 | ` *  According to the PHP reference manual.` |
|        - |  9841 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - |  9842 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - |  9843 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - |  9844 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - |  9845 | ` *   just once.` |
|        - |  9846 | ` */` |
|        4 |  9847 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9848 |  |
|        - |  9849 | `	SyString sFile;` |
|        - |  9850 | `	sxi32 rc;` |
|        5 |  9851 | `	if( nArg < 1 ){` |
|        - |  9852 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9853 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9854 | `		return SXRET_OK;` |
|        - |  9855 | `	}` |
|        - |  9856 | `	/* File to include */` |
|        5 |  9857 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9858 | `	if( sFile.nByte < 1 ){` |
|        - |  9859 | `		/* Empty string,return NULL */` |
|      ! 0 |  9860 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9861 | `		return SXRET_OK;` |
|        - |  9862 | `	}` |
|        - |  9863 | `	/* Open,compile and execute the desired script */` |
|        5 |  9864 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9865 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9866 | `		/* File already included,return TRUE */` |
|        3 |  9867 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9868 | `		return SXRET_OK;` |
|        - |  9869 | `	}` |
|        3 |  9870 | `	if( rc != SXRET_OK ){` |
|        - |  9871 | `		/* Emit a warning and return false */` |
|      ! 0 |  9872 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9873 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9874 | ` 	}` |
|        3 |  9875 | `	return SXRET_OK;` |
|        3 |  9876 |  |
|        - |  9877 | `/*` |
|        - |  9878 | ` * require.` |
|        - |  9879 | ` *  According to the PHP reference manual.` |
|        - |  9880 | ` *   require() is identical to include() except upon failure it will` |
|        - |  9881 | ` *   also produce a fatal level error.` |
|        - |  9882 | ` *   In other words, it will halt the script whereas include() only` |
|        - |  9883 | ` *   emits a warning  which allows the script to continue.` |
|        - |  9884 | ` */` |
|        4 |  9885 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9886 |  |
|        - |  9887 | `	SyString sFile;` |
|        - |  9888 | `	sxi32 rc;` |
|        5 |  9889 | `	if( nArg < 1 ){` |
|        - |  9890 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9891 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9892 | `		return SXRET_OK;` |
|        - |  9893 | `	}` |
|        - |  9894 | `	/* File to include */` |
|        5 |  9895 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9896 | `	if( sFile.nByte < 1 ){` |
|        - |  9897 | `		/* Empty string,return NULL */` |
|      ! 0 |  9898 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9899 | `		return SXRET_OK;` |
|        - |  9900 | `	}` |
|        - |  9901 | `	/* Open,compile and execute the desired script */` |
|        5 |  9902 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 |  9903 | `	if( rc != SXRET_OK ){` |
|        - |  9904 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9905 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9906 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9907 | `		return PH7_ABORT;` |
|        - |  9908 | `	}` |
|        5 |  9909 | `	return SXRET_OK;` |
|        3 |  9910 |  |
|        - |  9911 | `/*` |
|        - |  9912 | ` * require_once:` |
|        - |  9913 | ` *  According to the PHP reference manual.` |
|        - |  9914 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - |  9915 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - |  9916 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - |  9917 | ` *   and how it differs from its non _once siblings.` |
|        - |  9918 | ` */` |
|        4 |  9919 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9920 |  |
|        - |  9921 | `	SyString sFile;` |
|        - |  9922 | `	sxi32 rc;` |
|        5 |  9923 | `	if( nArg < 1 ){` |
|        - |  9924 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9925 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9926 | `		return SXRET_OK;` |
|        - |  9927 | `	}` |
|        - |  9928 | `	/* File to include */` |
|        5 |  9929 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 |  9930 | `	if( sFile.nByte < 1 ){` |
|        - |  9931 | `		/* Empty string,return NULL */` |
|      ! 0 |  9932 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9933 | `		return SXRET_OK;` |
|        - |  9934 | `	}` |
|        - |  9935 | `	/* Open,compile and execute the desired script */` |
|        5 |  9936 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 |  9937 | `	if( rc == SXERR_EXISTS ){` |
|        - |  9938 | `		/* File already included,return TRUE */` |
|        3 |  9939 | `		ph7_result_bool(pCtx,1);` |
|        3 |  9940 | `		return SXRET_OK;` |
|        - |  9941 | `	}` |
|        3 |  9942 | `	if( rc != SXRET_OK ){` |
|        - |  9943 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 |  9944 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 |  9945 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9946 | `		return PH7_ABORT;` |
|        - |  9947 | `	}` |
|        3 |  9948 | `	return SXRET_OK;` |
|        3 |  9949 |  |
|        - |  9950 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - |  9951 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - |  9952 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - |  9953 | `/* Table of built-in VM functions. */` |
|        - |  9954 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - |  9955 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - |  9956 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - |  9957 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - |  9958 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - |  9959 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - |  9960 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - |  9961 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - |  9962 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - |  9963 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - |  9964 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - |  9965 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - |  9966 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - |  9967 | `	    /* Constants management */` |
|        - |  9968 | `	{ "defined",  vm_builtin_defined              },` |
|        - |  9969 | `	{ "define",   vm_builtin_define               },` |
|        - |  9970 | `	{ "constant", vm_builtin_constant             },` |
|        - |  9971 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - |  9972 | `	   /* Class/Object functions */` |
|        - |  9973 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - |  9974 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - |  9975 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - |  9976 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - |  9977 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - |  9978 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - |  9979 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - |  9980 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - |  9981 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - |  9982 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - |  9983 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - |  9984 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - |  9985 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - |  9986 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - |  9987 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - |  9988 | `	{ "is_a", vm_builtin_is_a },` |
|        - |  9989 | `	   /* Random numbers/strings generators */` |
|        - |  9990 | `	{ "rand",          vm_builtin_rand            },` |
|        - |  9991 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - |  9992 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - |  9993 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - |  9994 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - |  9995 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  9996 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  9997 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - |  9998 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  9999 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10000 | `	   /* Language constructs functions */` |
|        - | 10001 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10002 | `	{ "print", vm_builtin_print                   },` |
|        - | 10003 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10004 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10005 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10006 | `	  /* Variable handling functions */` |
|        - | 10007 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10008 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10009 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10010 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10011 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10012 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10013 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10014 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10015 | `	  /* Ouput control functions */` |
|        - | 10016 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10017 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10018 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10019 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10020 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10021 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10022 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10023 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10024 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10025 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10026 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10027 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10028 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10029 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10030 | `	  /* Assertion functions */` |
|        - | 10031 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10032 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10033 | `	  /* Error reporting functions */` |
|        - | 10034 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10035 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10036 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10037 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10038 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10039 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10040 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10041 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10042 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10043 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10044 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10045 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10046 | `	  /* Release info */` |
|        - | 10047 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10048 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10049 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10050 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10051 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10052 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10053 | `	  /* hashmap */` |
|        - | 10054 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10055 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10056 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10057 | `	  /* URL related function */` |
|        - | 10058 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10059 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10060 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10061 | `	   /* XML processing functions */` |
|        - | 10062 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10063 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10064 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10065 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10066 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10067 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10068 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10069 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10070 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10071 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10072 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10073 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10074 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10075 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10076 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10077 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10078 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10079 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10080 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10081 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10082 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10083 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10084 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10085 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10086 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10087 | `	   /* Command line processing */` |
|        - | 10088 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10089 | `	   /* JSON encoding/decoding */` |
|        - | 10090 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10091 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10092 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10093 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10094 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10095 | `	   /* Files/URI inclusion facility */` |
|        - | 10096 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10097 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10098 | `	{ "include",      vm_builtin_include          },` |
|        - | 10099 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10100 | `	{ "require",      vm_builtin_require          },` |
|        - | 10101 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10102 | `};` |
|        - | 10103 | `/*` |
|        - | 10104 | ` * Register the built-in VM functions defined above.` |
|        - | 10105 | ` */` |
|     2030 | 10106 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10107 |  |
|        - | 10108 | `	sxi32 rc;` |
|        - | 10109 | `	sxu32 n;` |
|   253752 | 10110 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10111 | `		/* Note that these special functions have access` |
|        - | 10112 | `		 * to the underlying virtual machine as their` |
|        - | 10113 | `		 * private data.` |
|        - | 10114 | `		 */` |
|   251722 | 10115 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   251722 | 10116 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10117 | `			return rc;` |
|        - | 10118 | `		}` |
|   125862 | 10119 | `	}` |
|     2032 | 10120 | `	return SXRET_OK;` |
|     1017 | 10121 |  |
|        - | 10122 | `/*` |
|        - | 10123 | ` * Check if the given name refer to an installed class.` |
|        - | 10124 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10125 | ` */` |
|    14792 | 10126 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10127 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10128 | `	const char *zName,  /* Name of the target class */` |
|        - | 10129 | `	sxu32 nByte,        /* zName length */` |
|        - | 10130 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10131 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10132 | `						 */` |
|        - | 10133 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10134 | `	)` |
|        2 | 10135 |  |
|        - | 10136 | `	SyHashEntry *pEntry;` |
|        - | 10137 | `	ph7_class *pClass;` |
|     7396 | 10138 | `		SXUNUSED(iNest);` |
|        - | 10139 | `	/* Perform a hash lookup */` |
|    14794 | 10140 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|        - | 10141 |  |
|    14794 | 10142 | `	if( pEntry == 0 ){` |
|        - | 10143 | `		/* No such entry,return NULL */` |
|      ! 0 | 10144 | `		return 0;` |
|        - | 10145 | `	}` |
|    14794 | 10146 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    14794 | 10147 | `	if( !iLoadable ){` |
|        - | 10148 | `		/* Return the first class seen */` |
|    13772 | 10149 | `		return pClass;` |
|      ! 0 | 10150 | `	}else{` |
|        - | 10151 | `		/* Check the collision list */` |
|     1024 | 10152 | `		while(pClass){` |
|     1024 | 10153 | `			if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) == 0 ){` |
|        - | 10154 | `				/* Class is loadable */` |
|     1024 | 10155 | `				return pClass;` |
|        - | 10156 | `			}` |
|        - | 10157 | `			/* Point to the next entry */` |
|      ! 0 | 10158 | `			pClass = pClass->pNextName;` |
|      ! 0 | 10159 | `		}` |
|        - | 10160 | `	}` |
|        - | 10161 | `	/* No such loadable class */` |
|      ! 0 | 10162 | `	return 0;` |
|     7398 | 10163 |  |
|        - | 10164 | `/*` |
|        - | 10165 | ` * Reference Table Implementation` |
|        - | 10166 | ` * Status: stable <chm@symisc.net>` |
|        - | 10167 | ` * Intro` |
|        - | 10168 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10169 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10170 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10171 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10172 | ` *  Refer to the official for more information on this powerful` |
|        - | 10173 | ` *  extension.` |
|        - | 10174 | ` */` |
|        - | 10175 | `/*` |
|        - | 10176 | ` * Allocate a new reference entry.` |
|        - | 10177 | ` */` |
|  2981270 | 10178 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10179 |  |
|        - | 10180 | `	VmRefObj *pRef;` |
|        - | 10181 | `	/* Allocate a new instance */` |
|  2981272 | 10182 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2981272 | 10183 | `	if( pRef == 0 ){` |
|      ! 0 | 10184 | `		return 0;` |
|        - | 10185 | `	}` |
|        - | 10186 | `	/* Zero the structure */` |
|  2981272 | 10187 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10188 | `	/* Initialize fields */` |
|  2981272 | 10189 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2981272 | 10190 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2981272 | 10191 | `	pRef->nIdx = nIdx;` |
|  2981272 | 10192 | `	return pRef;` |
|  1490637 | 10193 |  |
|        - | 10194 | `/*` |
|        - | 10195 | ` * Default hash function used by the reference table` |
|        - | 10196 | ` * for lookup/insertion operations.` |
|        - | 10197 | ` */` |
| 16553006 | 10198 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10199 |  |
|        - | 10200 | `	/* Calculate the hash based on the memory object index */` |
| 16553008 | 10201 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10202 |  |
|        - | 10203 | `/*` |
|        - | 10204 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10205 | ` * in the reference table.` |
|        - | 10206 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10207 | ` * otherwise.` |
|        - | 10208 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10209 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10210 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10211 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10212 | ` * Refer to the official for more information on this powerful` |
|        - | 10213 | ` * extension.` |
|        - | 10214 | ` */` |
|  8903142 | 10215 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10216 |  |
|        - | 10217 | `	VmRefObj *pRef;` |
|        - | 10218 | `	sxu32 nBucket;` |
|        - | 10219 | `	/* Point to the appropriate bucket */` |
|  8903144 | 10220 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10221 | `	/* Perform the lookup */` |
|  8903144 | 10222 | `	pRef = pVm->apRefObj[nBucket];` |
| 18788225 | 10223 | `	for(;;){` |
| 37571306 | 10224 | `		if( pRef == 0 ){` |
|  3053994 | 10225 | `			break;` |
|        - | 10226 | `		}` |
| 34517314 | 10227 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10228 | `			/* Entry found */` |
|  5849152 | 10229 | `			return pRef;` |
|        - | 10230 | `		}` |
|        - | 10231 | `		/* Point to the next entry */` |
| 28668164 | 10232 | `		pRef = pRef->pNextCollide;` |
|        2 | 10233 | `	}` |
|        - | 10234 | `	/* No such entry,return NULL */` |
|  3053994 | 10235 | `	return 0;` |
|  4451573 | 10236 |  |
|        - | 10237 | `/*` |
|        - | 10238 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10239 | ` *` |
|        - | 10240 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10241 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10242 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10243 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10244 | ` * Refer to the official for more information on this powerful` |
|        - | 10245 | ` * extension.` |
|        - | 10246 | ` */` |
|  2981270 | 10247 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10248 |  |
|        - | 10249 | `	sxu32 nBucket;` |
|  2981272 | 10250 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10251 | `		VmRefObj **apNew;` |
|        - | 10252 | `		sxu32 nNew;` |
|        - | 10253 | `		/* Allocate a larger table */` |
|     3196 | 10254 | `		nNew = pVm->nRefSize << 1;` |
|     3196 | 10255 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3196 | 10256 | `		if( apNew ){` |
|     3196 | 10257 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10258 | `			sxu32 n;` |
|        - | 10259 | `			/* Zero the structure */` |
|     3196 | 10260 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10261 | `			/* Rehash all referenced entries */` |
|  2832274 | 10262 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10263 | `				/* Remove old collision links */` |
|  2829080 | 10264 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10265 | `				/* Point to the appropriate bucket */` |
|  2829080 | 10266 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10267 | `				/* Insert the entry  */` |
|  2829080 | 10268 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2829080 | 10269 | `				if( apNew[nBucket] ){` |
|  2298896 | 10270 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10271 | `				}` |
|  2829080 | 10272 | `				apNew[nBucket] = pEntry;` |
|        - | 10273 | `				/* Point to the next entry */` |
|  2829080 | 10274 | `				pEntry = pEntry->pNext;` |
|  1414541 | 10275 | `			}` |
|        - | 10276 | `			/* Release the old table */` |
|     3196 | 10277 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10278 | `			/* Install the new one */` |
|     3196 | 10279 | `			pVm->apRefObj = apNew;` |
|     3196 | 10280 | `			pVm->nRefSize = nNew;` |
|     1597 | 10281 | `		}` |
|     1597 | 10282 | `	}` |
|        - | 10283 | `	/* Point to the appropriate bucket */` |
|  2981272 | 10284 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10285 | `	/* Insert the entry */` |
|  2981272 | 10286 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2981272 | 10287 | `	if( pVm->apRefObj[nBucket] ){` |
|  2471371 | 10288 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1235804 | 10289 | `	}` |
|  2981272 | 10290 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2981272 | 10291 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2981272 | 10292 | `	pVm->nRefUsed++;` |
|  2981272 | 10293 | `	return SXRET_OK;` |
|        2 | 10294 |  |
|        - | 10295 | `/*` |
|        - | 10296 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10297 | ` * the reference table.` |
|        - | 10298 | ` * This function is invoked when the user perform an unset` |
|        - | 10299 | ` * call [i.e: unset($var); ].` |
|        - | 10300 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10301 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10302 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10303 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10304 | ` * Refer to the official for more information on this powerful` |
|        - | 10305 | ` * extension.` |
|        - | 10306 | ` */` |
|  2952234 | 10307 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10308 |  |
|        - | 10309 | `	ph7_hashmap_node **apNode;` |
|        - | 10310 | `	SyHashEntry **apEntry;` |
|        - | 10311 | `	sxu32 n;` |
|        - | 10312 | `	/* Point to the reference table */` |
|  2952236 | 10313 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2952236 | 10314 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10315 | `	/* Unlink the entry from the reference table */` |
|  3029920 | 10316 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    77686 | 10317 | `		if( apEntry[n] ){` |
|    77636 | 10318 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    38817 | 10319 | `		}` |
|    38844 | 10320 | `	}` |
|  5828686 | 10321 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2876452 | 10322 | `		if( apNode[n] ){` |
|     5635 | 10323 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2817 | 10324 | `		}` |
|  1438227 | 10325 | `	}` |
|  2952236 | 10326 | `	if( pRef->pPrevCollide ){` |
|  1112720 | 10327 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   556420 | 10328 | `	}else{` |
|  1839518 | 10329 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10330 | `	}` |
|  2952236 | 10331 | `	if( pRef->pNextCollide ){` |
|  1660011 | 10332 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   830067 | 10333 | `	}` |
|  2952236 | 10334 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10335 | `	/* Release the node */` |
|  2952236 | 10336 | `	SySetRelease(&pRef->aReference);` |
|  2952236 | 10337 | `	SySetRelease(&pRef->aArrEntries);` |
|  2952236 | 10338 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2952236 | 10339 | `	pVm->nRefUsed--;` |
|  2952236 | 10340 | `	return SXRET_OK;` |
|        2 | 10341 |  |
|        - | 10342 | `/*` |
|        - | 10343 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10344 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10345 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10346 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10347 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10348 | ` * Refer to the official for more information on this powerful` |
|        - | 10349 | ` * extension.` |
|        - | 10350 | ` */` |
|  3007314 | 10351 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10352 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10353 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10354 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10355 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10356 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10357 | `	)` |
|        2 | 10358 |  |
|  3007316 | 10359 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10360 | `	VmRefObj *pRef;` |
|        - | 10361 | `	/* Check if the referenced object already exists */` |
|  3007316 | 10362 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3007316 | 10363 | `	if( pRef == 0 ){` |
|        - | 10364 | `		/* Create a new entry */` |
|  2981272 | 10365 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2981272 | 10366 | `		if( pRef == 0 ){` |
|      ! 0 | 10367 | `			return SXERR_MEM;` |
|        - | 10368 | `		}` |
|  2981272 | 10369 | `		pRef->iFlags = iFlags;` |
|        - | 10370 | `		/* Install the entry */` |
|  2981272 | 10371 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1490635 | 10372 | `	}` |
|  3007392 | 10373 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|        - | 10374 | `		/* Safely ignore the exception frame */` |
|       78 | 10375 | `		pFrame = pFrame->pParent;` |
|        2 | 10376 | `	}` |
|  3007316 | 10377 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10378 | `		VmSlot sRef;` |
|        - | 10379 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10380 | `		 * be deleted when we leave this frame.` |
|        - | 10381 | `		 */` |
|    72754 | 10382 | `		sRef.nIdx = nIdx;` |
|    72754 | 10383 | `		sRef.pUserData = pEntry;` |
|    72754 | 10384 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10385 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10386 | `		}` |
|    36376 | 10387 | `	}` |
|  3007316 | 10388 | `	if( pEntry ){` |
|        - | 10389 | `		/* Address of the hash-entry */` |
|    98612 | 10390 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    49305 | 10391 | `	}` |
|  3007316 | 10392 | `	if( pMapEntry ){` |
|        - | 10393 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2903942 | 10394 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1451970 | 10395 | `	}` |
|  3007316 | 10396 | `	return SXRET_OK;` |
|  1503659 | 10397 |  |
|        - | 10398 | `/*` |
|        - | 10399 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10400 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10401 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10402 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10403 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10404 | ` * Refer to the official for more information on this powerful` |
|        - | 10405 | ` * extension.` |
|        - | 10406 | ` */` |
|  2943574 | 10407 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10408 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10409 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10410 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10411 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10412 | `	)` |
|        2 | 10413 |  |
|        - | 10414 | `	VmRefObj *pRef;` |
|        - | 10415 | `	sxu32 n;` |
|        - | 10416 | `	/* Check if the referenced object already exists */` |
|  2943576 | 10417 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2943576 | 10418 | `	if( pRef == 0 ){` |
|        - | 10419 | `		/* Not such entry */` |
|    72704 | 10420 | `		return SXERR_NOTFOUND;` |
|        - | 10421 | `	}` |
|        - | 10422 | `	/* Remove the desired entry */` |
|  2870874 | 10423 | `	if( pEntry ){` |
|        - | 10424 | `		SyHashEntry **apEntry;` |
|       51 | 10425 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      195 | 10426 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      145 | 10427 | `			if( apEntry[n] == pEntry ){` |
|        - | 10428 | `				/* Nullify the entry */` |
|       51 | 10429 | `				apEntry[n] = 0;` |
|        - | 10430 | `				/*` |
|        - | 10431 | `				 * NOTE:` |
|        - | 10432 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10433 | `				 * we avoid wasting spaces.` |
|        - | 10434 | `				 */` |
|       25 | 10435 | `			}` |
|       73 | 10436 | `		}` |
|       25 | 10437 | `	}` |
|  2870874 | 10438 | `	if( pMapEntry ){` |
|        - | 10439 | `		ph7_hashmap_node **apNode;` |
|  2870824 | 10440 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5741734 | 10441 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2870912 | 10442 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10443 | `				/* nullify the entry */` |
|  2870824 | 10444 | `				apNode[n] = 0;` |
|  1435411 | 10445 | `			}` |
|  1435457 | 10446 | `		}` |
|  1435411 | 10447 | `	}` |
|  2870874 | 10448 | `	return SXRET_OK;` |
|  1471789 | 10449 |  |
|        - | 10450 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10451 | `/*` |
|        - | 10452 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10453 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10454 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10455 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10456 | ` * For more information on how to register IO stream devices,please` |
|        - | 10457 | ` * refer to the official documentation.` |
|        - | 10458 | ` */` |
|    22648 | 10459 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10460 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10461 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10462 | `	int nByte              /* *pzDevice length*/` |
|        - | 10463 | `	)` |
|        2 | 10464 |  |
|        - | 10465 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10466 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10467 | `	SyString sDev,sCur;` |
|        - | 10468 | `	sxu32 n,nEntry;` |
|        - | 10469 | `	int rc;` |
|        - | 10470 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    22650 | 10471 | `	zNext = zCur = zIn = *pzDevice;` |
|    22650 | 10472 | `	zEnd = &zIn[nByte];` |
|  1450038 | 10473 | `	while( zIn < zEnd ){` |
|  1427392 | 10474 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10475 | `			/* Got one */` |
|        3 | 10476 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10477 | `			break;` |
|        - | 10478 | `		}` |
|        - | 10479 | `		/* Advance the cursor */` |
|  1427390 | 10480 | `		zIn++;` |
|        2 | 10481 | `	}` |
|    22650 | 10482 | `	if( zIn >= zEnd ){` |
|        - | 10483 | `		/* No such scheme,return the default stream */` |
|    22648 | 10484 | `		return pVm->pDefStream;` |
|        - | 10485 | `	}` |
|        3 | 10486 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10487 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10488 | `	SyStringFullTrim(&sDev);` |
|        - | 10489 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10490 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10491 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10492 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10493 | `		pStream = apStream[n];` |
|        3 | 10494 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10495 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10496 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10497 | `		if( rc == 0 ){` |
|        - | 10498 | `			/* Stream device found */` |
|        3 | 10499 | `			*pzDevice = zNext;` |
|        3 | 10500 | `			return pStream;` |
|        - | 10501 | `		}` |
|      ! 0 | 10502 | `	}` |
|        - | 10503 | `	/* No such stream,return NULL */` |
|      ! 0 | 10504 | `	return 0;` |
|    11326 | 10505 |  |
|        - | 10506 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10507 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10508 |  |
