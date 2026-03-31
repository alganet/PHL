# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4154/5393 lines (77.03%)

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
|   770042 |    86 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |    87 |  |
|   770044 |    88 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       32 |    89 | `		return TRUE;` |
|        - |    90 | `	}` |
|   770014 |    91 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|        9 |    92 | `		return TRUE;` |
|        - |    93 | `	}` |
|   770006 |    94 | `	return FALSE;` |
|   385045 |    95 |  |
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
|   464482 |   110 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   464484 |   121 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   464484 |   122 | `	if( pEntry ){` |
|        - |   123 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   124 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   125 | `		pCons->xExpand = xExpand;` |
|        6 |   126 | `		pCons->pUserData = pUserData;` |
|        6 |   127 | `		return SXRET_OK;` |
|        - |   128 | `	}` |
|        - |   129 | `	/* Allocate a new constant instance */` |
|   464480 |   130 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   464480 |   131 | `	if( pCons == 0 ){` |
|      ! 0 |   132 | `		return 0;` |
|        - |   133 | `	}` |
|        - |   134 | `	/* Duplicate constant name */` |
|   464480 |   135 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   464480 |   136 | `	if( zDupName == 0 ){` |
|      ! 0 |   137 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   138 | `		return 0;` |
|        - |   139 | `	}` |
|        - |   140 | `	/* Install the constant */` |
|   464480 |   141 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   464480 |   142 | `	pCons->xExpand = xExpand;` |
|   464480 |   143 | `	pCons->pUserData = pUserData;` |
|   464480 |   144 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   464480 |   145 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   146 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   147 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   148 | `		return rc;` |
|        - |   149 | `	}` |
|        - |   150 | `	/* All done,constant can be invoked from PHP code */` |
|   464480 |   151 | `	return SXRET_OK;` |
|   232243 |   152 |  |
|        - |   153 | `/*` |
|        - |   154 | ` * Allocate a new foreign function instance.` |
|        - |   155 | ` * This function return SXRET_OK on success. Any other` |
|        - |   156 | ` * return value indicates failure.` |
|        - |   157 | ` * Please refer to the official documentation for an introduction to` |
|        - |   158 | ` * the foreign function mechanism.` |
|        - |   159 | ` */` |
|   995280 |   160 | `static sxi32 PH7_NewForeignFunction(` |
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
|   995282 |   171 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   995282 |   172 | `	if( pFunc == 0 ){` |
|      ! 0 |   173 | `		return SXERR_MEM;` |
|        - |   174 | `	}` |
|        - |   175 | `	/* Duplicate function name */` |
|   995282 |   176 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   995282 |   177 | `	if( zDup == 0 ){` |
|      ! 0 |   178 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   179 | `		return SXERR_MEM;` |
|        - |   180 | `	}` |
|        - |   181 | `	/* Zero the structure */` |
|   995282 |   182 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   183 | `	/* Initialize structure fields */` |
|   995282 |   184 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   995282 |   185 | `	pFunc->pVm   = pVm;` |
|   995282 |   186 | `	pFunc->xFunc = xFunc;` |
|   995282 |   187 | `	pFunc->pUserData = pUserData;` |
|   995282 |   188 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   189 | `	/* Write a pointer to the new function */` |
|   995282 |   190 | `	*ppOut = pFunc;` |
|   995282 |   191 | `	return SXRET_OK;` |
|   497642 |   192 |  |
|        - |   193 | `/*` |
|        - |   194 | ` * Install a foreign function and it's associated callback so that` |
|        - |   195 | ` * it can be invoked from the target PHP code.` |
|        - |   196 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   197 | ` * return value indicates failure.` |
|        - |   198 | ` * Please refer to the official documentation for an introduction to` |
|        - |   199 | ` * the foreign function mechanism.` |
|        - |   200 | ` */` |
|   997568 |   201 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|   997570 |   212 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   997570 |   213 | `	if( pEntry ){` |
|     2290 |   214 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2290 |   215 | `		pFunc->pUserData = pUserData;` |
|     2290 |   216 | `		pFunc->xFunc = xFunc;` |
|     2290 |   217 | `		SySetReset(&pFunc->aAux);` |
|     2290 |   218 | `		return SXRET_OK;` |
|        - |   219 | `	}` |
|        - |   220 | `	/* Create a new user function */` |
|   995282 |   221 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   995282 |   222 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   223 | `		return rc;` |
|        - |   224 | `	}` |
|        - |   225 | `	/* Install the function in the corresponding hashtable */` |
|   995282 |   226 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   995282 |   227 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   228 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   229 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   230 | `		return rc;` |
|        - |   231 | `	}` |
|        - |   232 | `	/* User function successfully installed */` |
|   995282 |   233 | `	return SXRET_OK;` |
|   498786 |   234 |  |
|        - |   235 | `/*` |
|        - |   236 | ` * Initialize a VM function.` |
|        - |   237 | ` */` |
|   108034 |   238 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   239 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   240 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   241 | `	const char *zName,  /* Function name */` |
|        - |   242 | `	sxu32 nByte,        /* zName length */` |
|        - |   243 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   244 | `	void *pUserData     /* Function private data */` |
|        - |   245 | `	)` |
|        2 |   246 |  |
|        - |   247 | `	/* Zero the structure */` |
|   108036 |   248 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   249 | `	/* Initialize structure fields */` |
|        - |   250 | `	/* Arguments container */` |
|   108036 |   251 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   252 | `	/* Static variable container */` |
|   108036 |   253 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   254 | `	/* Bytecode container */` |
|   108036 |   255 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   256 | `    /* Preallocate some instruction slots */` |
|   108036 |   257 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   258 | `	/* Closure environment */` |
|   108036 |   259 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   108036 |   260 | `	pFunc->iFlags = iFlags;` |
|   108036 |   261 | `	pFunc->pUserData = pUserData;` |
|   108036 |   262 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   108036 |   263 | `	return SXRET_OK;` |
|        2 |   264 |  |
|        - |   265 | `/*` |
|        - |   266 | ` * Namespace-aware function lookup.` |
|        - |   267 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   268 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   269 | ` */` |
|        - |   270 | `/*` |
|        - |   271 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   272 | ` */` |
|   392084 |   273 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   274 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   275 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   276 | `	SyString *pName     /* Function name */` |
|        - |   277 | `	)` |
|        2 |   278 |  |
|        - |   279 | `	SyHashEntry *pEntry;` |
|        - |   280 | `	sxi32 rc;` |
|   392086 |   281 | `	if( pName == 0 ){` |
|        - |   282 | `		/* Use the built-in name */` |
|    33652 |   283 | `		pName = &pFunc->sName;` |
|    16825 |   284 | `	}` |
|        - |   285 | `	/* Check for duplicates (functions with the same name) first */` |
|   392086 |   286 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   392086 |   287 | `	if( pEntry ){` |
|   304712 |   288 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   304712 |   289 | `		if( pLink != pFunc ){` |
|        - |   290 | `			/* Link */` |
|      184 |   291 | `			pFunc->pNextName = pLink;` |
|      184 |   292 | `			pEntry->pUserData = pFunc;` |
|       91 |   293 | `		}` |
|   304712 |   294 | `		return SXRET_OK;` |
|        - |   295 | `	}` |
|        - |   296 | `	/* First time seen */` |
|    87376 |   297 | `	pFunc->pNextName = 0;` |
|    87376 |   298 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|    87376 |   299 | `	return rc;` |
|   196044 |   300 |  |
|        - |   301 | `/*` |
|        - |   302 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   303 | ` */` |
|    31056 |   304 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   305 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   306 | `	ph7_class *pClass /* Target Class */` |
|        - |   307 | `	)` |
|        2 |   308 |  |
|    31058 |   309 | `	SyString *pName = &pClass->sName;` |
|        - |   310 | `	SyHashEntry *pEntry;` |
|        - |   311 | `	sxi32 rc;` |
|        - |   312 | `	/* Check for duplicates */` |
|    31058 |   313 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    31058 |   314 | `	if( pEntry ){` |
|       31 |   315 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   316 | `		/* Link entry with the same name */` |
|       31 |   317 | `		pClass->pNextName = pLink;` |
|       31 |   318 | `		pEntry->pUserData = pClass;` |
|       31 |   319 | `		return SXRET_OK;` |
|        - |   320 | `	}` |
|    31028 |   321 | `	pClass->pNextName = 0;` |
|        - |   322 | `	/* Perform a simple hashtable insertion */` |
|    31028 |   323 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    31028 |   324 | `	return rc;` |
|    15530 |   325 |  |
|        - |   326 | `/*` |
|        - |   327 | ` * Instruction builder interface.` |
|        - |   328 | ` */` |
|  2865348 |   329 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  2865350 |   341 | `	sInstr.iOp = (sxu8)iOp;` |
|  2865350 |   342 | `	sInstr.iP1 = iP1;` |
|  2865350 |   343 | `	sInstr.iP2 = iP2;` |
|  2865350 |   344 | `	sInstr.p3  = p3;` |
|  2865350 |   345 | `	if( pIndex ){` |
|        - |   346 | `		/* Instruction index in the bytecode array */` |
|   182044 |   347 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    91021 |   348 | `	}` |
|        - |   349 | `	/* Finally,record the instruction */` |
|  2865350 |   350 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  2865350 |   351 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   352 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   353 | `		/* Fall throw */` |
|      ! 0 |   354 | `	}` |
|  2865350 |   355 | `	return rc;` |
|        2 |   356 |  |
|        - |   357 | `/*` |
|        - |   358 | ` * Swap the current bytecode container with the given one.` |
|        - |   359 | ` */` |
|   262540 |   360 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   361 |  |
|   262542 |   362 | `	if( pContainer == 0 ){` |
|        - |   363 | `		/* Point to the default container */` |
|      ! 0 |   364 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   365 | `	}else{` |
|        - |   366 | `		/* Change container */` |
|   262542 |   367 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   368 | `	}` |
|   262542 |   369 | `	return SXRET_OK;` |
|        2 |   370 |  |
|        - |   371 | `/*` |
|        - |   372 | ` * Return the current bytecode container.` |
|        - |   373 | ` */` |
|   131270 |   374 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   375 |  |
|   131272 |   376 | `	return pVm->pByteContainer;` |
|        2 |   377 |  |
|        - |   378 | `/*` |
|        - |   379 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   380 | ` */` |
|   179416 |   381 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   382 |  |
|        - |   383 | `	VmInstr *pInstr;` |
|   179418 |   384 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   179418 |   385 | `	return pInstr;` |
|        2 |   386 |  |
|        - |   387 | `/*` |
|        - |   388 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   389 | ` */` |
|   800992 |   390 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   391 |  |
|   800994 |   392 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   393 |  |
|        - |   394 | `/*` |
|        - |   395 | ` * Pop the last VM instruction.` |
|        - |   396 | ` */` |
|   170376 |   397 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   398 |  |
|   170378 |   399 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   400 |  |
|        - |   401 | `/*` |
|        - |   402 | ` * Peek the last VM instruction.` |
|        - |   403 | ` */` |
|   561874 |   404 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   405 |  |
|   561876 |   406 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   407 |  |
|    26144 |   408 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   409 |  |
|        - |   410 | `	VmInstr *aInstr;` |
|        - |   411 | `	sxu32 n;` |
|    26146 |   412 | `	n = SySetUsed(pVm->pByteContainer);` |
|    26146 |   413 | `	if( n < 2 ){` |
|      ! 0 |   414 | `		return 0;` |
|        - |   415 | `	}` |
|    26146 |   416 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    26146 |   417 | `	return &aInstr[n - 2];` |
|    13074 |   418 |  |
|        - |   419 | `/*` |
|        - |   420 | ` * Allocate a new virtual machine frame.` |
|        - |   421 | ` */` |
|    15110 |   422 | `static VmFrame * VmNewFrame(` |
|        - |   423 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   424 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   425 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   426 | `	)` |
|        2 |   427 |  |
|        - |   428 | `	VmFrame *pFrame;` |
|        - |   429 | `	/* Allocate a new vm frame */` |
|    15112 |   430 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    15112 |   431 | `	if( pFrame == 0 ){` |
|      ! 0 |   432 | `		return 0;` |
|        - |   433 | `	}` |
|        - |   434 | `	/* Zero the structure */` |
|    15112 |   435 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   436 | `	/* Initialize frame fields */` |
|    15112 |   437 | `	pFrame->pUserData = pUserData;` |
|    15112 |   438 | `	pFrame->pThis = pThis;` |
|    15112 |   439 | `	pFrame->pVm = pVm;` |
|    15112 |   440 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    15112 |   441 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    15112 |   442 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    15112 |   443 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    15112 |   444 | `	return pFrame;` |
|     7557 |   445 |  |
|        - |   446 | `/* Forward declaration */` |
|        - |   447 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   448 | `/*` |
|        - |   449 | ` * Enter a VM frame.` |
|        - |   450 | ` */` |
|    15110 |   451 | `static sxi32 VmEnterFrame(` |
|        - |   452 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   453 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   454 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   455 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   456 | `	)` |
|        2 |   457 |  |
|        - |   458 | `	VmFrame *pFrame;` |
|        - |   459 | `	/* Allocate a new frame */` |
|    15112 |   460 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    15112 |   461 | `	if( pFrame == 0 ){` |
|      ! 0 |   462 | `		return SXERR_MEM;` |
|        - |   463 | `	}` |
|        - |   464 | `	/* Link to the list of active VM frame */` |
|    15112 |   465 | `	pFrame->pParent = pVm->pFrame;` |
|    15112 |   466 | `	pVm->pFrame = pFrame;` |
|    15112 |   467 | `	if( ppFrame ){` |
|        - |   468 | `		/* Write a pointer to the new VM frame */` |
|    12562 |   469 | `		*ppFrame = pFrame;` |
|     6280 |   470 | `	}` |
|    15112 |   471 | `	return SXRET_OK;` |
|     7557 |   472 |  |
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
|    12560 |   516 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   517 |  |
|    12562 |   518 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    12562 |   519 | `	if( pCurFrame ){` |
|        - |   520 | `		/* Unlink from the list of active VM frame */` |
|    12562 |   521 | `		pVm->pFrame = pCurFrame->pParent;` |
|    12562 |   522 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   523 | `			VmSlot  *aSlot;` |
|        - |   524 | `			sxu32 n;` |
|        - |   525 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    12504 |   526 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|    88654 |   527 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   528 | `				/* Unset the local variable */` |
|    76152 |   529 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    38077 |   530 | `			}` |
|        - |   531 | `			/* Remove local reference */` |
|    12504 |   532 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|    88710 |   533 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|    76208 |   534 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    38105 |   535 | `			}` |
|     6251 |   536 | `		}` |
|        - |   537 | `		/* Release internal containers */` |
|    12562 |   538 | `		SyHashRelease(&pCurFrame->hVar);` |
|    12562 |   539 | `		SySetRelease(&pCurFrame->sArg);` |
|    12562 |   540 | `		SySetRelease(&pCurFrame->sLocal);` |
|    12562 |   541 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   542 | `		/* Release the whole structure */` |
|    12562 |   543 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     6280 |   544 | `	}` |
|    12562 |   545 |  |
|        - |   546 | `/*` |
|        - |   547 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   548 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   549 | ` * should be skipped when looking for the real execution context.` |
|        - |   550 | ` */` |
|  6223780 |   551 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   552 |  |
|  6224052 |   553 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      272 |   554 | `		pFrame = pFrame->pParent;` |
|        2 |   555 | `	}` |
|  6223782 |   556 | `	return pFrame;` |
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
|    90310 |   674 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   675 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   676 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   677 | `	)` |
|        2 |   678 |  |
|        - |   679 | `	ph7_class_method *pMeth;` |
|        - |   680 | `	ph7_class_attr *pAttr;` |
|        - |   681 | `	SyHashEntry *pEntry;` |
|        - |   682 | `	sxi32 rc;` |
|        - |   683 | `	/* Reset the loop cursor */` |
|    90312 |   684 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   685 | `	/* Process only static and constant attribute */` |
|   359529 |   686 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   687 | `		/* Extract the current attribute */` |
|   224064 |   688 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   224064 |   689 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
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
|    90312 |   711 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   712 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   713 | `		 */` |
|    46446 |   714 | `		return SXRET_OK;` |
|        - |   715 | `	}` |
|        - |   716 | `	/* Create constructor alias if not yet done */` |
|    43868 |   717 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   718 | `		/* User constructor with the same base class name */` |
|      284 |   719 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|      284 |   720 | `		if( pEntry ){` |
|      ! 0 |   721 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   722 | `			/* Create the alias */` |
|      ! 0 |   723 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   724 | `		}` |
|      141 |   725 | `	}` |
|        - |   726 | `	/* Install the methods now */` |
|    43868 |   727 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   424241 |   728 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   358442 |   729 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   358442 |   730 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   358436 |   731 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   358436 |   732 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   733 | `				return rc;` |
|        - |   734 | `			}` |
|   179217 |   735 | `		}` |
|        2 |   736 | `	}` |
|        - |   737 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    43868 |   738 | `	pClass->bMounted = TRUE;` |
|    43868 |   739 | `	return SXRET_OK;` |
|    45157 |   740 |  |
|        - |   741 | `/*` |
|        - |   742 | ` * Allocate a private frame for attributes of the given` |
|        - |   743 | ` * class instance (Object in the PHP jargon).` |
|        - |   744 | ` */` |
|     1134 |   745 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   746 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   747 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   748 | `	)` |
|        2 |   749 |  |
|     1136 |   750 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   751 | `	ph7_class_attr *pAttr;` |
|        - |   752 | `	SyHashEntry *pEntry;` |
|        - |   753 | `	sxi32 rc;` |
|        - |   754 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1136 |   755 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     4720 |   756 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   757 | `		VmClassAttr *pVmAttr;` |
|        - |   758 | `		/* Extract the current attribute */` |
|     3586 |   759 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     3586 |   760 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     3586 |   761 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   762 | `			return SXERR_MEM;` |
|        - |   763 | `		}` |
|     3586 |   764 | `		pVmAttr->pAttr = pAttr;` |
|     3586 |   765 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   766 | `			ph7_value *pMemObj;` |
|        - |   767 | `			/* Reserve a memory object for this attribute */` |
|     3580 |   768 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     3580 |   769 | `			if( pMemObj == 0 ){` |
|      ! 0 |   770 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   771 | `				return SXERR_MEM;` |
|        - |   772 | `			}` |
|     3580 |   773 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     3580 |   774 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   775 | `				/* Initialize attribute default value (any complex expression) */` |
|     1174 |   776 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      586 |   777 | `			}` |
|     3580 |   778 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     3580 |   779 | `			if( rc != SXRET_OK ){` |
|        - |   780 | `				VmSlot sSlot;` |
|        - |   781 | `				/* Restore memory object */` |
|      ! 0 |   782 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   783 | `				sSlot.pUserData = 0;` |
|      ! 0 |   784 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   785 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   786 | `				return SXERR_MEM;` |
|        - |   787 | `			}` |
|        - |   788 | `			/* Install attribute in the reference table */` |
|     3580 |   789 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|     1791 |   790 | `		}else{` |
|        - |   791 | `			/* Install static/constant attribute */` |
|        8 |   792 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|        8 |   793 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|        8 |   794 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   795 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   796 | `				return SXERR_MEM;` |
|        - |   797 | `			}` |
|        - |   798 | `		}` |
|        2 |   799 | `	}` |
|     1136 |   800 | `	return SXRET_OK;` |
|      569 |   801 |  |
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
|   310816 |   813 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   814 |  |
|        - |   815 | `	ph7_value *pObj;` |
|        - |   816 | `	sxi32 rc;` |
|   310818 |   817 | `	if( pIndex ){` |
|        - |   818 | `		/* Object index in the object table */` |
|   303168 |   819 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   151583 |   820 | `	}` |
|        - |   821 | `	/* Reserve a slot for the new object */` |
|   310818 |   822 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   310818 |   823 | `	if( rc != SXRET_OK ){` |
|        - |   824 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   825 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   826 | `		 */` |
|      ! 0 |   827 | `		return 0;` |
|        - |   828 | `	}` |
|   310818 |   829 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   310818 |   830 | `	return pObj;` |
|   155410 |   831 |  |
|        - |   832 | `/*` |
|        - |   833 | ` * Reserve a memory object.` |
|        - |   834 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   835 | ` */` |
|  2140752 |   836 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   837 |  |
|        - |   838 | `	ph7_value *pObj;` |
|        - |   839 | `	sxi32 rc;` |
|  2140754 |   840 | `	if( pIndex ){` |
|        - |   841 | `		/* Object index in the object table */` |
|  2140754 |   842 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1070376 |   843 | `	}` |
|        - |   844 | `	/* Reserve a slot for the new object */` |
|  2140754 |   845 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2140754 |   846 | `	if( rc != SXRET_OK ){` |
|        - |   847 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   848 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   849 | `		 */` |
|      ! 0 |   850 | `		return 0;` |
|        - |   851 | `	}` |
|  2140754 |   852 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2140754 |   853 | `	return pObj;` |
|  1070378 |   854 |  |
|        - |   855 | `/* Forward declaration */` |
|        - |   856 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |   857 | `/*` |
|        - |   858 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |   859 | ` * directly as foreign functions.` |
|        - |   860 | ` */` |
|        - |   861 | `#define PH7_BUILTIN_LIB \` |
|        - |   862 | `	"class Exception { "\` |
|        - |   863 | `    "protected $message = 'Unknown exception';"\` |
|        - |   864 | `    "protected $code = 0;"\` |
|        - |   865 | `    "protected $file;"\` |
|        - |   866 | `    "protected $line;"\` |
|        - |   867 | `    "protected $trace;"\` |
|        - |   868 | `    "protected $previous;"\` |
|        - |   869 | `	"public function __construct($message = null, $code = 0, Exception $previous = null){"\` |
|        - |   870 | `	"   if( isset($message) ){"\` |
|        - |   871 | `	"	  $this->message = $message;"\` |
|        - |   872 | `	"   }"\` |
|        - |   873 | `	"   $this->code = $code;"\` |
|        - |   874 | `	"   $this->file = __FILE__;"\` |
|        - |   875 | `	"   $this->line = __LINE__;"\` |
|        - |   876 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   877 | `	"   if( isset($previous) ){"\` |
|        - |   878 | `	"     $this->previous = $previous;"\` |
|        - |   879 | `	"   }"\` |
|        - |   880 | `	"}"\` |
|        - |   881 | `	"public function getMessage(){"\` |
|        - |   882 | `	"   return $this->message;"\` |
|        - |   883 | `	"}"\` |
|        - |   884 | `	" public function getCode(){"\` |
|        - |   885 | `	"  return $this->code;"\` |
|        - |   886 | `	"}"\` |
|        - |   887 | `	"public function getFile(){"\` |
|        - |   888 | `	"  return $this->file;"\` |
|        - |   889 | `	"}"\` |
|        - |   890 | `	"public function getLine(){"\` |
|        - |   891 | `	"  return $this->line;"\` |
|        - |   892 | `	"}"\` |
|        - |   893 | `	"public function getTrace(){"\` |
|        - |   894 | `	"   return $this->trace;"\` |
|        - |   895 | `	"}"\` |
|        - |   896 | `	"public function getTraceAsString(){"\` |
|        - |   897 | `	"  return debug_string_backtrace();"\` |
|        - |   898 | `	"}"\` |
|        - |   899 | `	"public function getPrevious(){"\` |
|        - |   900 | `	"    return $this->previous;"\` |
|        - |   901 | `	"}"\` |
|        - |   902 | `	"public function __toString(){"\` |
|        - |   903 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |   904 | `    "}"\` |
|        - |   905 | `	"}"\` |
|        - |   906 | `	"class Error extends Exception { }"\` |
|        - |   907 | `	"class TypeError extends Error { }"\` |
|        - |   908 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |   909 | `	"class ValueError extends Error { }"\` |
|        - |   910 | `	"class AssertionError extends Error { }"\` |
|        - |   911 | `	"class ErrorException extends Exception { "\` |
|        - |   912 | `	"protected $severity;"\` |
|        - |   913 | `	"public function __construct(string $message = null,"\` |
|        - |   914 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Exception $previous = null){"\` |
|        - |   915 | `	"   if( isset($message) ){"\` |
|        - |   916 | `	"	  $this->message = $message;"\` |
|        - |   917 | `	"   }"\` |
|        - |   918 | `	"   $this->severity = $severity;"\` |
|        - |   919 | `	"   $this->code = $code;"\` |
|        - |   920 | `	"   $this->file = $filename;"\` |
|        - |   921 | `	"   $this->line = $lineno;"\` |
|        - |   922 | `	"   $this->trace = debug_backtrace();"\` |
|        - |   923 | `	"   if( isset($previous) ){"\` |
|        - |   924 | `	"     $this->previous = $previous;"\` |
|        - |   925 | `	"   }"\` |
|        - |   926 | `	"}"\` |
|        - |   927 | `	"public function getSeverity(){"\` |
|        - |   928 | `	"   return $this->severity;"\` |
|        - |   929 | `    "}"\` |
|        - |   930 | `	"}"\` |
|        - |   931 | `	"interface Iterator {"\` |
|        - |   932 | `	"public function current();"\` |
|        - |   933 | `	"public function key();"\` |
|        - |   934 | `	"public function next();"\` |
|        - |   935 | `	"public function rewind();"\` |
|        - |   936 | `	"public function valid();"\` |
|        - |   937 | `	"}"\` |
|        - |   938 | `	"interface IteratorAggregate {"\` |
|        - |   939 | `	"public function getIterator();"\` |
|        - |   940 | `	"}"\` |
|        - |   941 | `	"interface Serializable {"\` |
|        - |   942 | `	"public function serialize();"\` |
|        - |   943 | `	"public function unserialize(string $serialized);"\` |
|        - |   944 | `	"}"\` |
|        - |   945 | `	"/* Directory releated IO */"\` |
|        - |   946 | `	"class Directory {"\` |
|        - |   947 | `	"public $handle = null;"\` |
|        - |   948 | `	"public $path  = null;"\` |
|        - |   949 | `	"public function __construct(string $path)"\` |
|        - |   950 | `	"{"\` |
|        - |   951 | `	"   $this->handle = opendir($path);"\` |
|        - |   952 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |   953 | `	"      $this->path = $path;"\` |
|        - |   954 | `	"   }"\` |
|        - |   955 | `	"}"\` |
|        - |   956 | `	"public function __destruct()"\` |
|        - |   957 | `	"{"\` |
|        - |   958 | `	"  if( $this->handle != null ){"\` |
|        - |   959 | `	"       closedir($this->handle);"\` |
|        - |   960 | `	"  }"\` |
|        - |   961 | `	"}"\` |
|        - |   962 | `	"public function read()"\` |
|        - |   963 | `	"{"\` |
|        - |   964 | `	"    return readdir($this->handle);"\` |
|        - |   965 | `	"}"\` |
|        - |   966 | `	"public function rewind()"\` |
|        - |   967 | `	"{"\` |
|        - |   968 | `	"    rewinddir($this->handle);"\` |
|        - |   969 | `	"}"\` |
|        - |   970 | `	"public function close()"\` |
|        - |   971 | `	"{"\` |
|        - |   972 | `	"    closedir($this->handle);"\` |
|        - |   973 | `	"    $this->handle = null;"\` |
|        - |   974 | `	"}"\` |
|        - |   975 | `	"}"\` |
|        - |   976 | `	"class stdClass{"\` |
|        - |   977 | `	"  public $value;"\` |
|        - |   978 | `	" /* Magic methods */"\` |
|        - |   979 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |   980 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |   981 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |   982 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |   983 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |   984 | `	"}"\` |
|        - |   985 | `	"function dir(string $path){"\` |
|        - |   986 | `	"   return new Directory($path);"\` |
|        - |   987 | `	"}"\` |
|        - |   988 | `	"function Dir(string $path){"\` |
|        - |   989 | `	"   return new Directory($path);"\` |
|        - |   990 | `	"}"\` |
|        - |   991 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |   992 | `    "{"\` |
|        - |   993 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |   994 | `	"  $aDir = array();"\` |
|        - |   995 | `	"  $pHandle = opendir($directory);"\` |
|        - |   996 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |   997 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |   998 | `	"      $aDir[] = $pEntry;"\` |
|        - |   999 | `	"   }"\` |
|        - |  1000 | `	"  closedir($pHandle);"\` |
|        - |  1001 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1002 | `	"      rsort($aDir);"\` |
|        - |  1003 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1004 | `	"      sort($aDir);"\` |
|        - |  1005 | `	"  }"\` |
|        - |  1006 | `	"  return $aDir;"\` |
|        - |  1007 | `	"}"\` |
|        - |  1008 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1009 | `	"/* Open the target directory */"\` |
|        - |  1010 | `	"$zDir = dirname($pattern);"\` |
|        - |  1011 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1012 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1013 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1014 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1015 | `	"	return FALSE;"\` |
|        - |  1016 | `	"}"\` |
|        - |  1017 | `	"$pattern = basename($pattern);"\` |
|        - |  1018 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1019 | `	"/* Loop throw available entries */"\` |
|        - |  1020 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1021 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1022 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1023 | `	"	if( $rc ){"\` |
|        - |  1024 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1025 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1026 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1027 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1028 | `	"		  }"\` |
|        - |  1029 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1030 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1031 | `	"		 continue;"\` |
|        - |  1032 | `	"	   }"\` |
|        - |  1033 | `	"	   /* Add the entry */"\` |
|        - |  1034 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1035 | `	"	}"\` |
|        - |  1036 | `	" }"\` |
|        - |  1037 | `	"/* Close the handle */"\` |
|        - |  1038 | `	"closedir($pHandle);"\` |
|        - |  1039 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1040 | `	"  /* Sort the array */"\` |
|        - |  1041 | `	"  sort($pArray);"\` |
|        - |  1042 | `	"}"\` |
|        - |  1043 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1044 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1045 | `	"  $pArray[] = $pattern;"\` |
|        - |  1046 | `	"}"\` |
|        - |  1047 | `	"/* Return the created array */"\` |
|        - |  1048 | `	"return $pArray;"\` |
|        - |  1049 | `   "}"\` |
|        - |  1050 | `   "/* Creates a temporary file */"\` |
|        - |  1051 | `   "function tmpfile(){"\` |
|        - |  1052 | `   "  /* Extract the temp directory */"\` |
|        - |  1053 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1054 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1055 | `   "    /* Use the current dir */"\` |
|        - |  1056 | `   "    $zTempDir = '.';"\` |
|        - |  1057 | `   "  }"\` |
|        - |  1058 | `   "  /* Create the file */"\` |
|        - |  1059 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1060 | `   "  return $pHandle;"\` |
|        - |  1061 | `   "}"\` |
|        - |  1062 | `   "/* Creates a temporary filename */"\` |
|        - |  1063 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1064 | `   "{"\` |
|        - |  1065 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1066 | `   "}"\` |
|        - |  1067 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1068 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1069 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1070 | `   "/* Copy arguments */"\` |
|        - |  1071 | `   "$nArgs = func_num_args();"\` |
|        - |  1072 | `   "$pNew = array();"\` |
|        - |  1073 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1074 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1075 | `    "}"\` |
|        - |  1076 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1077 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1078 | `	"/* Erase */"\` |
|        - |  1079 | `	"array_erase($pArray);"\` |
|        - |  1080 | `	"/* Unshift */"\` |
|        - |  1081 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1082 | `	"return sizeof($pArray);"\` |
|        - |  1083 | `    "}"\` |
|        - |  1084 | `	"function array_merge_recursive(){"\` |
|        - |  1085 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1086 | `    "$arrays = func_get_args();"\` |
|        - |  1087 | `    "$narrays = count($arrays);"\` |
|        - |  1088 | `    "$ret = array();"\` |
|        - |  1089 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1090 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1091 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1092 | `	 " }"\` |
|        - |  1093 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1094 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1095 | `     "  if( $keyIsInt ) {"\` |
|        - |  1096 | `     "   $ret[] = $value;"\` |
|        - |  1097 | `     "  } else {"\` |
|        - |  1098 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1099 | `     "    $cur = $ret[$key];"\` |
|        - |  1100 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1101 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1102 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1103 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1104 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1105 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1106 | `     "    } else {"\` |
|        - |  1107 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1108 | `     "    }"\` |
|        - |  1109 | `     "   } else {"\` |
|        - |  1110 | `     "    $ret[$key] = $value;"\` |
|        - |  1111 | `     "   }"\` |
|        - |  1112 | `     "  }"\` |
|        - |  1113 | `     " }"\` |
|        - |  1114 | `	 " }"\` |
|        - |  1115 | `	 " return $ret;"\` |
|        - |  1116 | `    "}"\` |
|        - |  1117 | `	"function max(){"\` |
|        - |  1118 | `    "  $pArgs = func_get_args();"\` |
|        - |  1119 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1120 | `	"  return null;"\` |
|        - |  1121 | `    " }"\` |
|        - |  1122 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1123 | `    " $pArg = $pArgs[0];"\` |
|        - |  1124 | `	" if( !is_array($pArg) ){"\` |
|        - |  1125 | `	"   return $pArg; "\` |
|        - |  1126 | `	" }"\` |
|        - |  1127 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1128 | `	"   return null;"\` |
|        - |  1129 | `	" }"\` |
|        - |  1130 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1131 | `	" reset($pArg);"\` |
|        - |  1132 | `	" $max = current($pArg);"\` |
|        - |  1133 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1134 | `	"   if( $val > $max ){"\` |
|        - |  1135 | `	"     $max = $val;"\` |
|        - |  1136 | `    " }"\` |
|        - |  1137 | `	" }"\` |
|        - |  1138 | `	" return $max;"\` |
|        - |  1139 | `    " }"\` |
|        - |  1140 | `    " $max = $pArgs[0];"\` |
|        - |  1141 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1142 | `    " $val = $pArgs[$i];"\` |
|        - |  1143 | `	"if( $val > $max ){"\` |
|        - |  1144 | `	" $max = $val;"\` |
|        - |  1145 | `	"}"\` |
|        - |  1146 | `    " }"\` |
|        - |  1147 | `	" return $max;"\` |
|        - |  1148 | `    "}"\` |
|        - |  1149 | `	"function min(){"\` |
|        - |  1150 | `    "  $pArgs = func_get_args();"\` |
|        - |  1151 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1152 | `	"  return null;"\` |
|        - |  1153 | `    " }"\` |
|        - |  1154 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1155 | `    " $pArg = $pArgs[0];"\` |
|        - |  1156 | `	" if( !is_array($pArg) ){"\` |
|        - |  1157 | `	"   return $pArg; "\` |
|        - |  1158 | `	" }"\` |
|        - |  1159 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1160 | `	"   return null;"\` |
|        - |  1161 | `	" }"\` |
|        - |  1162 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1163 | `	" reset($pArg);"\` |
|        - |  1164 | `	" $min = current($pArg);"\` |
|        - |  1165 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1166 | `	"   if( $val < $min ){"\` |
|        - |  1167 | `	"     $min = $val;"\` |
|        - |  1168 | `    " }"\` |
|        - |  1169 | `	" }"\` |
|        - |  1170 | `	" return $min;"\` |
|        - |  1171 | `    " }"\` |
|        - |  1172 | `    " $min = $pArgs[0];"\` |
|        - |  1173 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1174 | `    " $val = $pArgs[$i];"\` |
|        - |  1175 | `	"if( $val < $min ){"\` |
|        - |  1176 | `	" $min = $val;"\` |
|        - |  1177 | `	" }"\` |
|        - |  1178 | `    " }"\` |
|        - |  1179 | `	" return $min;"\` |
|        - |  1180 | `	"}"\` |
|        - |  1181 | `	"function fileowner(string $file){"\` |
|        - |  1182 | `    " $a = stat($file);"\` |
|        - |  1183 | `	" if( !is_array($a) ){"\` |
|        - |  1184 | `	"	return false;"\` |
|        - |  1185 | `	" }"\` |
|        - |  1186 | `	" return $a['uid'];"\` |
|        - |  1187 | `    "}"\` |
|        - |  1188 | `    "function filegroup(string $file){"\` |
|        - |  1189 | `	" $a = stat($file);"\` |
|        - |  1190 | `	" if( !is_array($a) ){"\` |
|        - |  1191 | `	"	return false;"\` |
|        - |  1192 | `	" }"\` |
|        - |  1193 | `	" return $a['gid'];"\` |
|        - |  1194 | `    "}"\` |
|        - |  1195 | `	 "function fileinode(string $file){"\` |
|        - |  1196 | `	" $a = stat($file);"\` |
|        - |  1197 | `	" if( !is_array($a) ){"\` |
|        - |  1198 | `	"	return false;"\` |
|        - |  1199 | `	" }"\` |
|        - |  1200 | `	" return $a['ino'];"\` |
|        - |  1201 | `    "}"` |
|        - |  1202 |  |
|        - |  1203 | `/*` |
|        - |  1204 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1205 | ` * start compiling the target PHP program.` |
|        - |  1206 | ` */` |
|     2550 |  1207 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1208 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1209 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1210 | `	 )` |
|        2 |  1211 |  |
|        - |  1212 | `	SyString sBuiltin;` |
|        - |  1213 | `	ph7_value *pObj;` |
|        - |  1214 | `	sxi32 rc;` |
|        - |  1215 | `	/* Zero the structure */` |
|     2552 |  1216 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1217 | `	/* Initialize VM fields */` |
|     2552 |  1218 | `	pVm->pEngine = &(*pEngine);` |
|     2552 |  1219 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1220 | `	/* Instructions containers */` |
|     2552 |  1221 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2552 |  1222 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2552 |  1223 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1224 | `	/* Object containers */` |
|     2552 |  1225 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2552 |  1226 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1227 | `	/* Virtual machine internal containers */` |
|     2552 |  1228 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2552 |  1229 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2552 |  1230 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2552 |  1231 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2552 |  1232 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2552 |  1233 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2552 |  1234 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2552 |  1235 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2552 |  1236 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2552 |  1237 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2552 |  1238 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2552 |  1239 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2552 |  1240 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2552 |  1241 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2552 |  1242 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2552 |  1243 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2552 |  1244 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2552 |  1245 | `	pVm->pPendingException = 0;` |
|        - |  1246 | `	/* Configuration containers */` |
|     2552 |  1247 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2552 |  1248 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2552 |  1249 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2552 |  1250 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2552 |  1251 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1252 | `	/* Error callbacks containers */` |
|     2552 |  1253 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2552 |  1254 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2552 |  1255 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2552 |  1256 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2552 |  1257 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1258 | `	/* Set a default recursion limit */` |
|        - |  1259 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2552 |  1260 | `	pVm->nMaxDepth = 32;` |
|        - |  1261 | `#else` |
|        - |  1262 | `	pVm->nMaxDepth = 16;` |
|        - |  1263 | `#endif` |
|        - |  1264 | `	/* Default assertion flags */` |
|     2552 |  1265 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1266 | `	/* JSON return status */` |
|     2552 |  1267 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1268 | `	/* PRNG context */` |
|     2552 |  1269 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1270 | `	/* Install the null constant */` |
|     2552 |  1271 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2552 |  1272 | `	if( pObj == 0 ){` |
|      ! 0 |  1273 | `		rc = SXERR_MEM;` |
|      ! 0 |  1274 | `		goto Err;` |
|        - |  1275 | `	}` |
|     2552 |  1276 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1277 | `	/* Install the boolean TRUE constant */` |
|     2552 |  1278 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2552 |  1279 | `	if( pObj == 0 ){` |
|      ! 0 |  1280 | `		rc = SXERR_MEM;` |
|      ! 0 |  1281 | `		goto Err;` |
|        - |  1282 | `	}` |
|     2552 |  1283 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1284 | `	/* Install the boolean FALSE constant */` |
|     2552 |  1285 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2552 |  1286 | `	if( pObj == 0 ){` |
|      ! 0 |  1287 | `		rc = SXERR_MEM;` |
|      ! 0 |  1288 | `		goto Err;` |
|        - |  1289 | `	}` |
|     2552 |  1290 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1291 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1292 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1293 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2552 |  1294 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2552 |  1295 | `	if( pObj == 0 ){` |
|      ! 0 |  1296 | `		rc = SXERR_MEM;` |
|      ! 0 |  1297 | `		goto Err;` |
|        - |  1298 | `	}` |
|     2552 |  1299 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1300 | `	/* Create the global frame */` |
|     2552 |  1301 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2552 |  1302 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1303 | `		goto Err;` |
|        - |  1304 | `	}` |
|        - |  1305 | `	/* Initialize the code generator */` |
|     2552 |  1306 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2552 |  1307 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1308 | `		goto Err;` |
|        - |  1309 | `	}` |
|        - |  1310 | `	/* VM correctly initialized,set the magic number */` |
|     2552 |  1311 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2552 |  1312 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1313 | `	/* Compile the built-in library */` |
|     2552 |  1314 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1315 | `	/* Reset the code generator */` |
|     2552 |  1316 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2552 |  1317 | `	return SXRET_OK;` |
|      ! 0 |  1318 | `Err:` |
|      ! 0 |  1319 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1320 | `	return rc;` |
|     1277 |  1321 |  |
|        - |  1322 | `/*` |
|        - |  1323 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1324 | ` * routine which store the output in an internal blob.` |
|        - |  1325 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1326 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1327 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1328 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1329 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1330 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1331 | ` * to finish executing and extracting the output.` |
|        - |  1332 | ` */` |
|      ! 0 |  1333 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1334 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1335 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1336 | `	void *pUserData     /* User private data */` |
|        - |  1337 | `	)` |
|      ! 0 |  1338 |  |
|        - |  1339 | `	 sxi32 rc;` |
|        - |  1340 | `	 /* Store the output in an internal BLOB */` |
|      ! 0 |  1341 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|      ! 0 |  1342 | `	 return rc;` |
|      ! 0 |  1343 |  |
|        - |  1344 | `#define VM_STACK_GUARD 16` |
|        - |  1345 | `/*` |
|        - |  1346 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1347 | ` * our compiled PHP program.` |
|        - |  1348 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1349 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1350 | ` */` |
|    31082 |  1351 | `static ph7_value * VmNewOperandStack(` |
|        - |  1352 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1353 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1354 | `	)` |
|        2 |  1355 |  |
|        - |  1356 | `	ph7_value *pStack;` |
|        - |  1357 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1358 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1359 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1360 | `  ** on the maximum stack depth required.` |
|        - |  1361 | `  **` |
|        - |  1362 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1363 | `  */` |
|    31084 |  1364 | `	nInstr += VM_STACK_GUARD;` |
|    31084 |  1365 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    31084 |  1366 | `	if( pStack == 0 ){` |
|      ! 0 |  1367 | `		return 0;` |
|        - |  1368 | `	}` |
|        - |  1369 | `	/* Initialize the operand stack */` |
|  1964858 |  1370 | `	while( nInstr > 0 ){` |
|  1933776 |  1371 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  1933776 |  1372 | `		--nInstr;` |
|        2 |  1373 | `	}` |
|        - |  1374 | `	/* Ready for bytecode execution */` |
|    31084 |  1375 | `	return pStack;` |
|    15543 |  1376 |  |
|        - |  1377 | `/* Forward declaration */` |
|        - |  1378 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1379 | `/*` |
|        - |  1380 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1381 | ` * This routine gets called by the PH7 engine after` |
|        - |  1382 | ` * successful compilation of the target PHP program.` |
|        - |  1383 | ` */` |
|     2288 |  1384 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1385 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1386 | `	)` |
|        2 |  1387 |  |
|        - |  1388 | `	SyHashEntry *pEntry;` |
|        - |  1389 | `	sxi32 rc;` |
|     2290 |  1390 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1391 | `		/* Initialize your VM first */` |
|      ! 0 |  1392 | `		return SXERR_CORRUPT;` |
|        - |  1393 | `	}` |
|        - |  1394 | `	/* Mark the VM ready for byte-code execution */` |
|     2290 |  1395 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1396 | `	/* Release the code generator now we have compiled our program */` |
|     2290 |  1397 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1398 | `	/* Emit the DONE instruction */` |
|     2290 |  1399 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2290 |  1400 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1401 | `		return SXERR_MEM;` |
|        - |  1402 | `	}` |
|        - |  1403 | `	/* Script return value */` |
|     2290 |  1404 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1405 | `	/* Allocate a new operand stack */` |
|     2290 |  1406 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2290 |  1407 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1408 | `		return SXERR_MEM;` |
|        - |  1409 | `	}` |
|        - |  1410 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1411 | `	 * private data. */` |
|     2290 |  1412 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2290 |  1413 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1414 | `	/* Allocate the reference table */` |
|     2290 |  1415 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2290 |  1416 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2290 |  1417 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1418 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1419 | `		return SXERR_MEM;` |
|        - |  1420 | `	}` |
|        - |  1421 | `	/* Zero the reference table */` |
|     2290 |  1422 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1423 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2290 |  1424 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2290 |  1425 | `	if( rc != SXRET_OK ){` |
|        - |  1426 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1427 | `		return rc;` |
|        - |  1428 | `	}` |
|        - |  1429 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2290 |  1430 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2290 |  1431 | `	if( rc != SXRET_OK ){` |
|        - |  1432 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1433 | `		return rc;` |
|        - |  1434 | `	}` |
|        - |  1435 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2290 |  1436 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1437 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2290 |  1438 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1439 | `	/* Initialize and install static and constants class attributes */` |
|     2290 |  1440 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    29910 |  1441 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    27622 |  1442 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    27622 |  1443 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1444 | `			return rc;` |
|        - |  1445 | `		}` |
|        2 |  1446 | `	}` |
|        - |  1447 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2290 |  1448 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1449 | `	/* VM is ready for bytecode execution */` |
|     2290 |  1450 | `	return SXRET_OK;` |
|     1146 |  1451 |  |
|        - |  1452 | `/*` |
|        - |  1453 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1454 | ` */` |
|      ! 0 |  1455 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1456 |  |
|      ! 0 |  1457 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1458 | `		return SXERR_CORRUPT;` |
|        - |  1459 | `	}` |
|        - |  1460 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1461 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1462 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1463 | `	/* Set the ready flag */` |
|      ! 0 |  1464 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1465 | `	return SXRET_OK;` |
|      ! 0 |  1466 |  |
|        - |  1467 | `/*` |
|        - |  1468 | ` * Release a Virtual Machine.` |
|        - |  1469 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1470 | ` */` |
|     2280 |  1471 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1472 |  |
|        - |  1473 | `	/* Set the stale magic number */` |
|     2282 |  1474 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1475 | `	/* Release the private memory subsystem */` |
|     2282 |  1476 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2282 |  1477 | `	return SXRET_OK;` |
|        2 |  1478 |  |
|        - |  1479 | `/*` |
|        - |  1480 | ` * Initialize a foreign function call context.` |
|        - |  1481 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1482 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1483 | ` * functions.` |
|        - |  1484 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1485 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1486 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1487 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1488 | ` */` |
|   548894 |  1489 | `static sxi32 VmInitCallContext(` |
|        - |  1490 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1491 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1492 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1493 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1494 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1495 | `	)` |
|        2 |  1496 |  |
|   548896 |  1497 | `	pOut->pFunc = pFunc;` |
|   548896 |  1498 | `	pOut->pVm   = pVm;` |
|   548896 |  1499 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   548896 |  1500 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1501 | `	/* Assume a null return value */` |
|   548896 |  1502 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   548896 |  1503 | `	pOut->pRet = pRet;` |
|   548896 |  1504 | `	pOut->iFlags = iFlags;` |
|   548896 |  1505 | `	return SXRET_OK;` |
|        2 |  1506 |  |
|        - |  1507 | `/*` |
|        - |  1508 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1509 | ` * left behind.` |
|        - |  1510 | ` */` |
|   548894 |  1511 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1512 |  |
|        - |  1513 | `	sxu32 n;` |
|   548896 |  1514 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     6698 |  1515 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    19110 |  1516 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    12414 |  1517 | `			if( apObj[n] == 0 ){` |
|        - |  1518 | `				/* Already released */` |
|      250 |  1519 | `				continue;` |
|        - |  1520 | `			}` |
|    12166 |  1521 | `			PH7_MemObjRelease(apObj[n]);` |
|    12166 |  1522 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     6084 |  1523 | `		}` |
|     6698 |  1524 | `		SySetRelease(&pCtx->sVar);` |
|     3348 |  1525 | `	}` |
|   548896 |  1526 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1527 | `		ph7_aux_data *aAux;` |
|        - |  1528 | `		void *pChunk;` |
|        - |  1529 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1530 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1531 | `		 */` |
|        9 |  1532 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1533 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1534 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1535 | `			/* Release the chunk */` |
|       25 |  1536 | `			if( pChunk ){` |
|       25 |  1537 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1538 | `			}` |
|       13 |  1539 | `		}` |
|        9 |  1540 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1541 | `	}` |
|   548896 |  1542 |  |
|        - |  1543 | `/*` |
|        - |  1544 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1545 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1546 | ` */` |
|      248 |  1547 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1548 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1549 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1550 | `	)` |
|        2 |  1551 |  |
|      250 |  1552 | `	if( pValue == 0 ){` |
|        - |  1553 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1554 | `		return;` |
|        - |  1555 | `	}` |
|      250 |  1556 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      250 |  1557 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1558 | `		sxu32 n;` |
|      936 |  1559 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|      936 |  1560 | `			if( apObj[n] == pValue ){` |
|      250 |  1561 | `				PH7_MemObjRelease(pValue);` |
|      250 |  1562 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1563 | `				/* Mark as released */` |
|      250 |  1564 | `				apObj[n] = 0;` |
|      250 |  1565 | `				break;` |
|        - |  1566 | `			}` |
|      345 |  1567 | `		}` |
|      124 |  1568 | `	}` |
|      126 |  1569 |  |
|        - |  1570 | `/*` |
|        - |  1571 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1572 | ` */` |
|  3242928 |  1573 | `static void VmPopOperand(` |
|        - |  1574 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1575 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1576 | `	)` |
|        2 |  1577 |  |
|  3242930 |  1578 | `	ph7_value *pTos = *ppTos;` |
|  6887586 |  1579 | `	while( nPop > 0 ){` |
|  3644658 |  1580 | `		PH7_MemObjRelease(pTos);` |
|  3644658 |  1581 | `		pTos--;` |
|  3644658 |  1582 | `		nPop--;` |
|        2 |  1583 | `	}` |
|        - |  1584 | `	/* Top of the stack */` |
|  3242930 |  1585 | `	*ppTos = pTos;` |
|  3242930 |  1586 |  |
|        - |  1587 | `/*` |
|        - |  1588 | ` * Reserve a memory object.` |
|        - |  1589 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1590 | ` */` |
|  2998714 |  1591 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1592 |  |
|  2998716 |  1593 | `	ph7_value *pObj = 0;` |
|        - |  1594 | `	VmSlot *pSlot;` |
|        - |  1595 | `	sxu32 nIdx;` |
|        - |  1596 | `	/* Check for a free slot */` |
|  2998716 |  1597 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  2998716 |  1598 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  2998716 |  1599 | `	if( pSlot ){` |
|   857964 |  1600 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|   857964 |  1601 | `		nIdx = pSlot->nIdx;` |
|   428981 |  1602 | `	}` |
|  2998716 |  1603 | `	if( pObj == 0 ){` |
|        - |  1604 | `		/* Reserve a new memory object */` |
|  2140754 |  1605 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2140754 |  1606 | `		if( pObj == 0 ){` |
|      ! 0 |  1607 | `			return 0;` |
|        - |  1608 | `		}` |
|  1070376 |  1609 | `	}` |
|        - |  1610 | `	/* Set a null default value */` |
|  2998716 |  1611 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  2998716 |  1612 | `	pObj->nIdx = nIdx;` |
|  2998716 |  1613 | `	return pObj;` |
|  1499359 |  1614 |  |
|        - |  1615 | `/*` |
|        - |  1616 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1617 | ` */` |
|    28612 |  1618 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1619 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1620 | `	const char *zKey,  /* Entry key */` |
|        - |  1621 | `	sxu32 nByte,       /* Key length */` |
|        - |  1622 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1623 | `	)` |
|        2 |  1624 |  |
|        - |  1625 | `	ph7_value sKey;` |
|        - |  1626 | `	sxi32 rc;` |
|    28614 |  1627 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    28614 |  1628 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1629 | `	/* Perform the insertion */` |
|    28614 |  1630 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    28614 |  1631 | `	PH7_MemObjRelease(&sKey);` |
|    28614 |  1632 | `	return rc;` |
|        2 |  1633 |  |
|        - |  1634 | `/*` |
|        - |  1635 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1636 | ` * Return a pointer to the variable value on success.` |
|        - |  1637 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1638 | ` */` |
|  3041096 |  1639 | `static ph7_value * VmExtractMemObj(` |
|        - |  1640 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1641 | `	const SyString *pName, /* Variable name */` |
|        - |  1642 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1643 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1644 | `	)` |
|        2 |  1645 |  |
|  3041098 |  1646 | `	int bNullify = FALSE;` |
|        - |  1647 | `	SyHashEntry *pEntry;` |
|        - |  1648 | `	VmFrame *pFrame;` |
|        - |  1649 | `	ph7_value *pObj;` |
|        - |  1650 | `	sxu32 nIdx;` |
|        - |  1651 | `	sxi32 rc;` |
|        - |  1652 | `	/* Point to the top active frame */` |
|  3041098 |  1653 | `	pFrame = pVm->pFrame;` |
|  3041098 |  1654 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1655 | `	/* Perform the lookup */` |
|  3041098 |  1656 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1657 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1658 | `		pName = &sAnnon;` |
|        - |  1659 | `		/* Always nullify the object */` |
|      ! 0 |  1660 | `		bNullify = TRUE;` |
|      ! 0 |  1661 | `		bDup = FALSE;` |
|      ! 0 |  1662 | `	}` |
|        - |  1663 | `	/* Check the superglobals table first */` |
|  3041098 |  1664 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3041098 |  1665 | `	if( pEntry == 0 ){` |
|        - |  1666 | `		/* Query the top active frame */` |
|  3041062 |  1667 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3041062 |  1668 | `		if( pEntry == 0 ){` |
|    82490 |  1669 | `			char *zName = (char *)pName->zString;` |
|        - |  1670 | `			VmSlot sLocal;` |
|    82490 |  1671 | `			if( !bCreate ){` |
|        - |  1672 | `				/* Do not create the variable,return NULL instead */` |
|      632 |  1673 | `				return 0;` |
|        - |  1674 | `			}` |
|        - |  1675 | `			/* No such variable,automatically create a new one and install` |
|        - |  1676 | `			 * it in the current frame.` |
|        - |  1677 | `			 */` |
|    81860 |  1678 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    81860 |  1679 | `			if( pObj == 0 ){` |
|      ! 0 |  1680 | `				return 0;` |
|        - |  1681 | `			}` |
|    81860 |  1682 | `			nIdx = pObj->nIdx;` |
|    81860 |  1683 | `			if( bDup ){` |
|        - |  1684 | `				/* Duplicate name */` |
|      164 |  1685 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      164 |  1686 | `				if( zName == 0 ){` |
|      ! 0 |  1687 | `					return 0;` |
|        - |  1688 | `				}` |
|       81 |  1689 | `			}` |
|        - |  1690 | `			/* Link to the top active VM frame */` |
|    81860 |  1691 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|    81860 |  1692 | `			if( rc != SXRET_OK ){` |
|        - |  1693 | `				/* Return the slot to the free pool */` |
|      ! 0 |  1694 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  1695 | `				sLocal.pUserData = 0;` |
|      ! 0 |  1696 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  1697 | `				return 0;` |
|        - |  1698 | `			}` |
|    81860 |  1699 | `			if( pFrame->pParent != 0 ){` |
|        - |  1700 | `				/* Local variable */` |
|    76152 |  1701 | `				sLocal.nIdx = nIdx;` |
|    76152 |  1702 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    38077 |  1703 | `			}else{` |
|        - |  1704 | `				/* Register in the $GLOBALS array */` |
|     5710 |  1705 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  1706 | `			}` |
|        - |  1707 | `			/* Install in the reference table */` |
|    81860 |  1708 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  1709 | `			/* Save object index */` |
|    81860 |  1710 | `			pObj->nIdx = nIdx;` |
|    40931 |  1711 | `		}else{` |
|        - |  1712 | `			/* Extract variable contents */` |
|  2958574 |  1713 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  2958574 |  1714 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  2958574 |  1715 | `			if( bNullify && pObj ){` |
|      ! 0 |  1716 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  1717 | `			}` |
|        - |  1718 | `		}` |
|  1520327 |  1719 | `	}else{` |
|        - |  1720 | `		/* Superglobal */` |
|       38 |  1721 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       38 |  1722 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  1723 | `	}` |
|  3040468 |  1724 | `	return pObj;` |
|  1520660 |  1725 |  |
|        - |  1726 | `/*` |
|        - |  1727 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  1728 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  1729 | ` */` |
|     2314 |  1730 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  1731 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  1732 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  1733 | `	sxu32 nByte        /* zName length */` |
|        - |  1734 | `	)` |
|        2 |  1735 |  |
|        - |  1736 | `	SyHashEntry *pEntry;` |
|        - |  1737 | `	ph7_value *pValue;` |
|        - |  1738 | `	sxu32 nIdx;` |
|        - |  1739 | `	/* Query the superglobal table */` |
|     2316 |  1740 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2316 |  1741 | `	if( pEntry == 0 ){` |
|        - |  1742 | `		/* No such entry */` |
|      ! 0 |  1743 | `		return 0;` |
|        - |  1744 | `	}` |
|        - |  1745 | `	/* Extract the superglobal index in the global object pool */` |
|     2316 |  1746 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1747 | `	/* Extract the variable value  */` |
|     2316 |  1748 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2316 |  1749 | `	return pValue;` |
|     1159 |  1750 |  |
|        - |  1751 | `/*` |
|        - |  1752 | ` * Perform a raw hashmap insertion.` |
|        - |  1753 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  1754 | ` */` |
|     2312 |  1755 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  1756 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  1757 | `	const char *zKey,   /* Entry key */` |
|        - |  1758 | `	int nKeylen,        /* zKey length*/` |
|        - |  1759 | `	const char *zData,  /* Entry data */` |
|        - |  1760 | `	int nLen            /* zData length */` |
|        - |  1761 | `	)` |
|        2 |  1762 |  |
|        - |  1763 | `	ph7_value sKey,sValue;` |
|        - |  1764 | `	sxi32 rc;` |
|     2314 |  1765 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     2314 |  1766 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     2314 |  1767 | `	if( zKey ){` |
|     2292 |  1768 | `		if( nKeylen < 0 ){` |
|     2292 |  1769 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1145 |  1770 | `		}` |
|     2292 |  1771 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1145 |  1772 | `	}` |
|     2314 |  1773 | `	if( zData ){` |
|     2314 |  1774 | `		if( nLen < 0 ){` |
|        - |  1775 | `			/* Compute length automatically */` |
|      ! 0 |  1776 | `			nLen = (int)SyStrlen(zData);` |
|      ! 0 |  1777 | `		}` |
|     2314 |  1778 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1156 |  1779 | `	}` |
|        - |  1780 | `	/* Perform the insertion */` |
|     2314 |  1781 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     2314 |  1782 | `	PH7_MemObjRelease(&sKey);` |
|     2314 |  1783 | `	PH7_MemObjRelease(&sValue);` |
|     2314 |  1784 | `	return rc;` |
|        2 |  1785 |  |
|        - |  1786 | `/*` |
|        - |  1787 | ` * Configure a working virtual machine instance.` |
|        - |  1788 | ` *` |
|        - |  1789 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  1790 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  1791 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  1792 | ` * The second argument to this function is an integer configuration option` |
|        - |  1793 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  1794 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  1795 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  1796 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  1797 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  1798 | ` */` |
|    36632 |  1799 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  1800 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1801 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  1802 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  1803 | `	)` |
|        2 |  1804 |  |
|    36634 |  1805 | `	sxi32 rc = SXRET_OK;` |
|    36634 |  1806 | `	switch(nOp){` |
|     1144 |  1807 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2290 |  1808 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2290 |  1809 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  1810 | `		/* VM output consumer callback */` |
|        - |  1811 | `#ifdef UNTRUST` |
|        - |  1812 | `		if( xConsumer == 0 ){` |
|        - |  1813 | `			rc = SXERR_CORRUPT;` |
|        - |  1814 | `			break;` |
|        - |  1815 | `		}` |
|        - |  1816 | `#endif` |
|        - |  1817 | `		/* Install the output consumer */` |
|     2290 |  1818 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2290 |  1819 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2290 |  1820 | `		break;` |
|        - |  1821 | `							   }` |
|     1144 |  1822 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  1823 | `		/* Import path */` |
|        - |  1824 | `		  const char *zPath;` |
|        - |  1825 | `		  SyString sPath;` |
|     2290 |  1826 | `		  zPath = va_arg(ap,const char *);` |
|        - |  1827 | `#if defined(UNTRUST)` |
|        - |  1828 | `		  if( zPath == 0 ){` |
|        - |  1829 | `			  rc = SXERR_EMPTY;` |
|        - |  1830 | `			  break;` |
|        - |  1831 | `		  }` |
|        - |  1832 | `#endif` |
|     2290 |  1833 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  1834 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  1835 | `#ifdef __WINNT__` |
|        2 |  1836 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  1837 | `#endif` |
|     4578 |  1838 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  1839 | `		  /* Remove leading and trailing white spaces */` |
|     2290 |  1840 | `		  SyStringFullTrim(&sPath);` |
|     2290 |  1841 | `		  if( sPath.nByte > 0 ){` |
|        - |  1842 | `			  /* Store the path in the corresponding conatiner */` |
|     2290 |  1843 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1144 |  1844 | `		  }` |
|     2290 |  1845 | `		  break;` |
|        - |  1846 | `									 }` |
|     1144 |  1847 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  1848 | `		/* Run-Time Error report */` |
|     2290 |  1849 | `		pVm->bErrReport = 1;` |
|     2290 |  1850 | `		break;` |
|      ! 0 |  1851 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  1852 | `		/* Recursion depth */` |
|      ! 0 |  1853 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  1854 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  1855 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  1856 | `		}` |
|      ! 0 |  1857 | `		break;` |
|        - |  1858 | `									   }` |
|      ! 0 |  1859 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  1860 | `		/* VM output length in bytes */` |
|      ! 0 |  1861 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  1862 | `#ifdef UNTRUST` |
|        - |  1863 | `		if( pOut == 0 ){` |
|        - |  1864 | `			rc = SXERR_CORRUPT;` |
|        - |  1865 | `			break;` |
|        - |  1866 | `		}` |
|        - |  1867 | `#endif` |
|      ! 0 |  1868 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  1869 | `		break;` |
|        - |  1870 | `							   }` |
|        - |  1871 |  |
|    11440 |  1872 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  1873 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  1874 | `		/* Create a new superglobal/global variable */` |
|    22882 |  1875 | `		const char *zName = va_arg(ap,const char *);` |
|    22882 |  1876 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  1877 | `		SyHashEntry *pEntry;` |
|        - |  1878 | `		ph7_value *pObj;` |
|        - |  1879 | `		sxu32 nByte;` |
|        - |  1880 | `		sxu32 nIdx;` |
|        - |  1881 | `#ifdef UNTRUST` |
|        - |  1882 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  1883 | `			rc = SXERR_CORRUPT;` |
|        - |  1884 | `			break;` |
|        - |  1885 | `		}` |
|        - |  1886 | `#endif` |
|    22882 |  1887 | `		nByte = SyStrlen(zName);` |
|    22882 |  1888 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1889 | `			/* Check if the superglobal is already installed */` |
|    22882 |  1890 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    11442 |  1891 | `		}else{` |
|        - |  1892 | `			/* Query the top active VM frame */` |
|      ! 0 |  1893 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  1894 | `		}` |
|    22882 |  1895 | `		if( pEntry ){` |
|        - |  1896 | `			/* Variable already installed */` |
|      ! 0 |  1897 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  1898 | `			/* Extract contents */` |
|      ! 0 |  1899 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  1900 | `			if( pObj ){` |
|        - |  1901 | `				/* Overwrite old contents */` |
|      ! 0 |  1902 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  1903 | `			}` |
|      ! 0 |  1904 | `		}else{` |
|        - |  1905 | `			/* Install a new variable */` |
|    22882 |  1906 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    22882 |  1907 | `			if( pObj == 0 ){` |
|      ! 0 |  1908 | `				rc = SXERR_MEM;` |
|      ! 0 |  1909 | `				break;` |
|        - |  1910 | `			}` |
|    22882 |  1911 | `			nIdx = pObj->nIdx;` |
|        - |  1912 | `			/* Copy value */` |
|    22882 |  1913 | `			PH7_MemObjStore(pValue,pObj);` |
|    22882 |  1914 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  1915 | `				/* Install the superglobal */` |
|    22882 |  1916 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    11442 |  1917 | `			}else{` |
|        - |  1918 | `				/* Install in the current frame */` |
|      ! 0 |  1919 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  1920 | `			}` |
|    22882 |  1921 | `			if( rc == SXRET_OK ){` |
|        - |  1922 | `				SyHashEntry *pRef;` |
|    22882 |  1923 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    22882 |  1924 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    11442 |  1925 | `				}else{` |
|      ! 0 |  1926 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  1927 | `				}` |
|        - |  1928 | `				/* Install in the reference table */` |
|    22882 |  1929 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    22882 |  1930 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  1931 | `					/* Register in the $GLOBALS array */` |
|    22882 |  1932 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    11440 |  1933 | `				}` |
|    11440 |  1934 | `			}` |
|        - |  1935 | `		}` |
|    22882 |  1936 | `		break;` |
|        - |  1937 | `									}` |
|     1145 |  1938 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  1939 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  1940 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  1941 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  1942 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  1943 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  1944 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2292 |  1945 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2292 |  1946 | `		const char *zValue = va_arg(ap,const char *);` |
|     2292 |  1947 | `		int nLen = va_arg(ap,int);` |
|        - |  1948 | `		ph7_hashmap *pMap;` |
|        - |  1949 | `		ph7_value *pValue;` |
|     2292 |  1950 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  1951 | `			/* Extract the $_ENV superglobal */` |
|        3 |  1952 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2291 |  1953 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  1954 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  1955 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2290 |  1956 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  1957 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  1958 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2290 |  1959 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  1960 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  1961 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2290 |  1962 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  1963 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  1964 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2290 |  1965 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  1966 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  1967 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  1968 | `		}else{` |
|        - |  1969 | `			/* Extract the $_SERVER superglobal */` |
|     2290 |  1970 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  1971 | `		}` |
|     2292 |  1972 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1973 | `			/* No such entry */` |
|      ! 0 |  1974 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1975 | `			break;` |
|        - |  1976 | `		}` |
|        - |  1977 | `		/* Point to the hashmap */` |
|     2292 |  1978 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  1979 | `		/* Perform the insertion */` |
|     2292 |  1980 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2292 |  1981 | `		break;` |
|        - |  1982 | `								   }` |
|       11 |  1983 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  1984 | `		/* Script arguments */` |
|       24 |  1985 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  1986 | `		ph7_hashmap *pMap;` |
|        - |  1987 | `		ph7_value *pValue;` |
|        - |  1988 | `		sxu32 n;` |
|       24 |  1989 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  1990 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  1991 | `			break;` |
|        - |  1992 | `		}` |
|        - |  1993 | `		/* Extract the $argv array */` |
|       24 |  1994 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  1995 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  1996 | `			/* No such entry */` |
|      ! 0 |  1997 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  1998 | `			break;` |
|        - |  1999 | `		}` |
|        - |  2000 | `		/* Point to the hashmap */` |
|       24 |  2001 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2002 | `		/* Perform the insertion */` |
|       24 |  2003 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2004 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2005 | `		if( rc == SXRET_OK ){` |
|       24 |  2006 | `			if( pMap->nEntry > 1 ){` |
|        - |  2007 | `				/* Append space separator first */` |
|       18 |  2008 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2009 | `			}` |
|       24 |  2010 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2011 | `		}` |
|       24 |  2012 | `		break;` |
|        - |  2013 | `								  }` |
|      ! 0 |  2014 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2015 | `		/* error_log() consumer */` |
|      ! 0 |  2016 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2017 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2018 | `		break;` |
|        - |  2019 | `										}` |
|      ! 0 |  2020 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2021 | `		/* Script return value */` |
|      ! 0 |  2022 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2023 | `#ifdef UNTRUST` |
|        - |  2024 | `		if( ppValue == 0 ){` |
|        - |  2025 | `			rc = SXERR_CORRUPT;` |
|        - |  2026 | `			break;` |
|        - |  2027 | `		}` |
|        - |  2028 | `#endif` |
|      ! 0 |  2029 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2030 | `		break;` |
|        - |  2031 | `								   }` |
|     2288 |  2032 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2033 | `		/* Register an IO stream device */` |
|     4578 |  2034 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2035 | `		/* Make sure we are dealing with a valid IO stream */` |
|     6864 |  2036 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     4578 |  2037 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2038 | `				/* Invalid stream */` |
|      ! 0 |  2039 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2040 | `				break;` |
|        - |  2041 | `		}` |
|     4578 |  2042 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2043 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2290 |  2044 | `			pVm->pDefStream = pStream;` |
|     1144 |  2045 | `		}` |
|        - |  2046 | `		/* Insert in the appropriate container */` |
|     4578 |  2047 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     4578 |  2048 | `		break;` |
|        - |  2049 | `								  }` |
|      ! 0 |  2050 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2051 | `		/* Point to the VM internal output consumer buffer */` |
|      ! 0 |  2052 | `		const void **ppOut = va_arg(ap,const void **);` |
|      ! 0 |  2053 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2054 | `#ifdef UNTRUST` |
|        - |  2055 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2056 | `			rc = SXERR_CORRUPT;` |
|        - |  2057 | `			break;` |
|        - |  2058 | `		}` |
|        - |  2059 | `#endif` |
|      ! 0 |  2060 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|      ! 0 |  2061 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|      ! 0 |  2062 | `		break;` |
|        - |  2063 | `									   }` |
|      ! 0 |  2064 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2065 | `		/* Raw HTTP request*/` |
|      ! 0 |  2066 | `		const char *zRequest = va_arg(ap,const char *);` |
|      ! 0 |  2067 | `		int nByte = va_arg(ap,int);` |
|      ! 0 |  2068 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2069 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2070 | `			break;` |
|        - |  2071 | `		}` |
|      ! 0 |  2072 | `		if( nByte < 0 ){` |
|        - |  2073 | `			/* Compute length automatically */` |
|      ! 0 |  2074 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2075 | `		}` |
|        - |  2076 | `		/* Process the request */` |
|      ! 0 |  2077 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|      ! 0 |  2078 | `		break;` |
|        - |  2079 | `									}` |
|      ! 0 |  2080 | `	default:` |
|        - |  2081 | `		/* Unknown configuration option */` |
|      ! 0 |  2082 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2083 | `		break;` |
|        - |  2084 | `	}` |
|    36634 |  2085 | `	return rc;` |
|        2 |  2086 |  |
|        - |  2087 | `/* Forward declaration */` |
|        - |  2088 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2089 | `/*` |
|        - |  2090 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2091 | ` * format.` |
|        - |  2092 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2093 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2094 | ` * (STDOUT).` |
|        - |  2095 | ` */` |
|        2 |  2096 | `static sxi32 VmByteCodeDump(` |
|        - |  2097 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2098 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2099 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2100 | `	)` |
|        1 |  2101 |  |
|        - |  2102 | `	static const char zDump[] = {` |
|        - |  2103 | `		"====================================================\n"` |
|        - |  2104 | `		"PH7 VM Dump\n"` |
|        - |  2105 | `		"====================================================\n"` |
|        - |  2106 | `	};` |
|        - |  2107 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2108 | `	sxi32 rc = SXRET_OK;` |
|        - |  2109 | `	sxu32 n;` |
|        - |  2110 | `	/* Point to the PH7 instructions */` |
|        3 |  2111 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2112 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2113 | `	n = 0;` |
|        3 |  2114 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2115 | `	/* Dump instructions */` |
|        7 |  2116 | `	for(;;){` |
|       15 |  2117 | `		if( pInstr >= pEnd ){` |
|        - |  2118 | `			/* No more instructions */` |
|        3 |  2119 | `			break;` |
|        - |  2120 | `		}` |
|        - |  2121 | `		/* Format and call the consumer callback */` |
|       19 |  2122 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2123 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2124 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2125 | `		if( rc != SXRET_OK ){` |
|        - |  2126 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2127 | `			return rc;` |
|        - |  2128 | `		}` |
|       13 |  2129 | `		++n;` |
|       13 |  2130 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2131 | `	}` |
|        3 |  2132 | `	return rc;` |
|        2 |  2133 |  |
|        - |  2134 | `/* Forward declaration */` |
|        - |  2135 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2136 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2137 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2138 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2139 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2140 | `/*` |
|        - |  2141 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2142 | ` * consumer callback.` |
|        - |  2143 | ` */` |
|      542 |  2144 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2145 |  |
|      543 |  2146 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      543 |  2147 | `	sxi32 rc = SXRET_OK;` |
|        - |  2148 | `	/* Append a new line */` |
|        - |  2149 | `#ifdef __WINNT__` |
|        1 |  2150 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2151 | `#else` |
|      542 |  2152 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2153 | `#endif` |
|        - |  2154 | `	/* Invoke the output consumer callback */` |
|      543 |  2155 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      543 |  2156 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  2157 | `		/* Increment output length */` |
|      543 |  2158 | `		pVm->nOutputLen += SyBlobLength(pMsg);` |
|      271 |  2159 | `	}` |
|      543 |  2160 | `	return rc;` |
|        1 |  2161 |  |
|        - |  2162 | `/*` |
|        - |  2163 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2164 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2165 | ` * information.` |
|        - |  2166 | ` */` |
|      130 |  2167 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2168 |  |
|      132 |  2169 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2170 | `		ph7_value apArg[4];` |
|        - |  2171 | `		ph7_value *apArgPtr[4];` |
|        - |  2172 | `		ph7_value sResult;` |
|        - |  2173 | `		SyString sErr;` |
|        - |  2174 | `		/* Prepare arguments */` |
|       61 |  2175 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2176 | `			/* use explicit message length to avoid reading past buffer */` |
|       61 |  2177 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       61 |  2178 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       61 |  2179 | `		if( pFile ){` |
|       61 |  2180 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       61 |  2181 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       31 |  2182 | `		}else{` |
|      ! 0 |  2183 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2184 | `		}` |
|       61 |  2185 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       61 |  2186 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2187 | `		/* Set up pointer array */` |
|       61 |  2188 | `		apArgPtr[0] = &apArg[0];` |
|       61 |  2189 | `		apArgPtr[1] = &apArg[1];` |
|       61 |  2190 | `		apArgPtr[2] = &apArg[2];` |
|       61 |  2191 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2192 | `		/* Call the handler */` |
|       61 |  2193 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2194 | `		/* Check return value */` |
|       61 |  2195 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2196 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2197 | `		}` |
|        - |  2198 | `		/* Release */` |
|       61 |  2199 | `		PH7_MemObjRelease(&apArg[0]);` |
|       61 |  2200 | `		PH7_MemObjRelease(&apArg[1]);` |
|       61 |  2201 | `		PH7_MemObjRelease(&apArg[2]);` |
|       61 |  2202 | `		PH7_MemObjRelease(&apArg[3]);` |
|       61 |  2203 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2204 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2205 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       61 |  2206 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2207 | `	}` |
|        - |  2208 | `	/* No handler, always call error handler */` |
|       71 |  2209 | `	return TRUE;` |
|       67 |  2210 |  |
|       94 |  2211 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2212 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2213 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2214 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2215 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2216 | `	)` |
|        2 |  2217 |  |
|       96 |  2218 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2219 | `	SyString *pFile;` |
|        - |  2220 | `	char *zErr;` |
|       96 |  2221 | `	sxi32 rc = SXRET_OK;` |
|       96 |  2222 | `	if( !pVm->bErrReport ){` |
|        - |  2223 | `		/* Don't bother reporting errors */` |
|        3 |  2224 | `		return SXRET_OK;` |
|        - |  2225 | `	}` |
|        - |  2226 | `	/* Reset the working buffer */` |
|       94 |  2227 | `	SyBlobReset(pWorker);` |
|        - |  2228 | `	/* Peek the processed file if available */` |
|       94 |  2229 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       94 |  2230 | `	if( pFile ){` |
|        - |  2231 | `		/* Append file name */` |
|       94 |  2232 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       94 |  2233 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       46 |  2234 | `	}` |
|        - |  2235 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2236 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2237 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2238 | `	 * E_DEPRECATED). */` |
|       94 |  2239 | `	zErr = "Error:  ";` |
|       94 |  2240 | `	switch(iErr){` |
|       17 |  2241 | `	case PH7_CTX_WARNING:` |
|       36 |  2242 | `		zErr = "Warning:  ";` |
|       36 |  2243 | `		break;` |
|        6 |  2244 | `	case PH7_CTX_NOTICE:` |
|       14 |  2245 | `		zErr = "Notice:  ";` |
|       12 |  2246 | `		break;` |
|       23 |  2247 | `	default:` |
|        - |  2248 | `		/* keep iErr unchanged */` |
|       46 |  2249 | `		break;` |
|        - |  2250 | `	}` |
|       94 |  2251 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       94 |  2252 | `	if( pFuncName ){` |
|        - |  2253 | `		/* Append function name first */` |
|       21 |  2254 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       21 |  2255 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       10 |  2256 | `	}` |
|       94 |  2257 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2258 | `	/* Check for user error handler.  compute length of C string */` |
|       94 |  2259 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       45 |  2260 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       22 |  2261 | `	}` |
|       94 |  2262 | `	return rc;` |
|       49 |  2263 |  |
|        - |  2264 | `/*` |
|        - |  2265 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2266 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2267 | ` * information.` |
|        - |  2268 | ` */` |
|       38 |  2269 | `static sxi32 VmThrowErrorAp(` |
|        - |  2270 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2271 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2272 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2273 | `	const char *zFormat, /* Format message */` |
|        - |  2274 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2275 | `	)` |
|        2 |  2276 |  |
|       40 |  2277 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2278 | `	SyBlob sMsg;` |
|        - |  2279 | `	SyString *pFile;` |
|        - |  2280 | `	char *zErr;` |
|       40 |  2281 | `	sxi32 rc = SXRET_OK;` |
|       40 |  2282 | `	if( !pVm->bErrReport ){` |
|        - |  2283 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2284 | `		return SXRET_OK;` |
|        - |  2285 | `	}` |
|        - |  2286 | `	/* Reset the working buffer */` |
|       40 |  2287 | `	SyBlobReset(pWorker);` |
|        - |  2288 | `	/* Peek the processed file if available */` |
|       40 |  2289 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       40 |  2290 | `	if( pFile ){` |
|        - |  2291 | `		/* Append file name */` |
|       40 |  2292 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       40 |  2293 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       19 |  2294 | `	}` |
|        - |  2295 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2296 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2297 | `	 * the correct errno value. */` |
|       40 |  2298 | `	zErr = "Error:  ";` |
|       40 |  2299 | `	switch(iErr){` |
|        4 |  2300 | `	case PH7_CTX_WARNING:` |
|        9 |  2301 | `		zErr = "Warning:  ";` |
|        9 |  2302 | `		break;` |
|        3 |  2303 | `	case PH7_CTX_NOTICE:` |
|        7 |  2304 | `		zErr = "Notice:  ";` |
|        6 |  2305 | `		break;` |
|       12 |  2306 | `	default:` |
|        - |  2307 | `		/* do not change iErr */` |
|       24 |  2308 | `		break;` |
|        - |  2309 | `	}` |
|       40 |  2310 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       40 |  2311 | `	if( pFuncName ){` |
|        - |  2312 | `		/* Append function name first */` |
|       26 |  2313 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2314 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2315 | `	}` |
|        - |  2316 | `	/* Format the raw message */` |
|       40 |  2317 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       40 |  2318 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2319 | `	/* Check if a user error handler is installed */` |
|       40 |  2320 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2321 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2322 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2323 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2324 | `	}` |
|       40 |  2325 | `	SyBlobRelease(&sMsg);` |
|       40 |  2326 | `	return rc;` |
|       21 |  2327 |  |
|        - |  2328 | `/*` |
|        - |  2329 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2330 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2331 | ` * information.` |
|        - |  2332 | ` * ------------------------------------` |
|        - |  2333 | ` * Simple boring wrapper function.` |
|        - |  2334 | ` * ------------------------------------` |
|        - |  2335 | ` */` |
|       14 |  2336 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  2337 |  |
|        - |  2338 | `	va_list ap;` |
|        - |  2339 | `	sxi32 rc;` |
|       15 |  2340 | `	va_start(ap,zFormat);` |
|       15 |  2341 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       15 |  2342 | `	va_end(ap);` |
|       15 |  2343 | `	return rc;` |
|        1 |  2344 |  |
|        - |  2345 | `/*` |
|        - |  2346 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2347 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2348 | ` * information.` |
|        - |  2349 | ` * ------------------------------------` |
|        - |  2350 | ` * Simple boring wrapper function.` |
|        - |  2351 | ` * ------------------------------------` |
|        - |  2352 | ` */` |
|       24 |  2353 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  2354 |  |
|        - |  2355 | `	sxi32 rc;` |
|       26 |  2356 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  2357 | `	return rc;` |
|        2 |  2358 |  |
|        - |  2359 | `/*` |
|        - |  2360 | ` * Resolve function context from the current frame.` |
|        - |  2361 | ` */` |
|      934 |  2362 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  2363 |  |
|        - |  2364 | `	VmFrame *pFrame;` |
|        - |  2365 | `	ph7_vm_func *pFunc;` |
|      935 |  2366 | `	*pzFuncName = 0;` |
|      935 |  2367 | `	*pnFuncLen = 0;` |
|      935 |  2368 | `	pFrame = pVm->pFrame;` |
|      935 |  2369 | `	if( pFrame == 0 ){` |
|      ! 0 |  2370 | `		return;` |
|        - |  2371 | `	}` |
|      935 |  2372 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      935 |  2373 | `	if( pFrame->pParent == 0 ){` |
|      929 |  2374 | `		return;` |
|        - |  2375 | `	}` |
|        7 |  2376 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        7 |  2377 | `	if( pFunc == 0 ){` |
|      ! 0 |  2378 | `		return;` |
|        - |  2379 | `	}` |
|        7 |  2380 | `	*pzFuncName = pFunc->sName.zString;` |
|        7 |  2381 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      468 |  2382 |  |
|        - |  2383 | `/*` |
|        - |  2384 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  2385 | ` */` |
|      470 |  2386 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  2387 |  |
|        - |  2388 | `	SyBlob sOut;` |
|        - |  2389 | `	SyString *pFile;` |
|      471 |  2390 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  2391 | `		return PH7_OK;` |
|        - |  2392 | `	}` |
|      471 |  2393 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  2394 | `		zClass = "Exception";` |
|      ! 0 |  2395 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  2396 | `	}` |
|      471 |  2397 | `	if( zMsg == 0 ){` |
|      ! 0 |  2398 | `		zMsg = "Unknown exception";` |
|      ! 0 |  2399 | `		nMsg = (sxu32)sizeof("Unknown exception") - 1;` |
|      ! 0 |  2400 | `	}` |
|      471 |  2401 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      465 |  2402 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      232 |  2403 | `	}` |
|      471 |  2404 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      471 |  2405 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      471 |  2406 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      471 |  2407 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      471 |  2408 | `	SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      471 |  2409 | `	SyBlobAppend(&sOut,zMsg,nMsg);` |
|      471 |  2410 | `	if( pFile ){` |
|      471 |  2411 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      471 |  2412 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2413 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      235 |  2414 | `	}` |
|      471 |  2415 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      471 |  2416 | `	if( pFile ){` |
|      471 |  2417 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      471 |  2418 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2419 | `		if( zFuncName && nFuncLen > 0 ){` |
|        7 |  2420 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|        4 |  2421 | `		}else{` |
|      465 |  2422 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  2423 | `		}` |
|      235 |  2424 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  2425 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  2426 | `	}else{` |
|      ! 0 |  2427 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  2428 | `	}` |
|      471 |  2429 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      471 |  2430 | `	if( pFile ){` |
|      471 |  2431 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      471 |  2432 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      471 |  2433 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      471 |  2434 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      235 |  2435 | `	}` |
|      471 |  2436 | `	VmCallErrorHandler(pVm,&sOut);` |
|      471 |  2437 | `	SyBlobRelease(&sOut);` |
|      471 |  2438 | `	return PH7_ABORT;` |
|      236 |  2439 |  |
|        - |  2440 | `/*` |
|        - |  2441 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  2442 | ` */` |
|      468 |  2443 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  2444 |  |
|        - |  2445 | `	ph7_vm *pVm;` |
|        - |  2446 | `	ph7_class *pClass;` |
|        - |  2447 | `	ph7_class_instance *pThis;` |
|        - |  2448 | `	ph7_class_method *pCons;` |
|        - |  2449 | `	ph7_value sArg;` |
|        - |  2450 | `	ph7_value *apArg[1];` |
|        - |  2451 | `	SyBlob sMsg;` |
|        - |  2452 | `	SyString sMsgStr;` |
|        - |  2453 | `	VmFrame *pFrame;` |
|        - |  2454 | `	va_list ap;` |
|        - |  2455 | `	sxi32 rc;` |
|        - |  2456 |  |
|      470 |  2457 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2458 | `		return PH7_ABORT;` |
|        - |  2459 | `	}` |
|      470 |  2460 | `	pVm = pCtx->pVm;` |
|      470 |  2461 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2462 | `		zClass = "Error";` |
|      ! 0 |  2463 | `	}` |
|      470 |  2464 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      470 |  2465 | `	if( pClass == 0 ){` |
|      ! 0 |  2466 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2467 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  2468 | `			zClass` |
|        - |  2469 | `			);` |
|        - |  2470 | `	}` |
|      470 |  2471 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      470 |  2472 | `	if( pThis == 0 ){` |
|      ! 0 |  2473 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  2474 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  2475 | `			);` |
|        - |  2476 | `	}` |
|        - |  2477 |  |
|      470 |  2478 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      470 |  2479 | `	va_start(ap,zFormat);` |
|      470 |  2480 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      470 |  2481 | `	va_end(ap);` |
|        - |  2482 |  |
|      470 |  2483 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      470 |  2484 | `	if( pCons ){` |
|      470 |  2485 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      470 |  2486 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      470 |  2487 | `		apArg[0] = &sArg;` |
|      470 |  2488 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      470 |  2489 | `		PH7_MemObjRelease(&sArg);` |
|      234 |  2490 | `	}` |
|      470 |  2491 | `	SyBlobRelease(&sMsg);` |
|        - |  2492 |  |
|      470 |  2493 | `	pFrame = pVm->pFrame;` |
|      470 |  2494 | `	if( pFrame ){` |
|      470 |  2495 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      470 |  2496 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      234 |  2497 | `	}` |
|      470 |  2498 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      470 |  2499 | `	PH7_ClassInstanceUnref(pThis);` |
|      470 |  2500 | `	if( rc == SXERR_ABORT ){` |
|      463 |  2501 | `		return PH7_ABORT;` |
|        - |  2502 | `	}` |
|        7 |  2503 | `	return PH7_EXCEPTION;` |
|      236 |  2504 |  |
|        - |  2505 | `/*` |
|        - |  2506 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  2507 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  2508 | ` */` |
|      ! 0 |  2509 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  2510 |  |
|        - |  2511 | `	ph7_vm *pVm;` |
|        - |  2512 | `	SyBlob sMsg;` |
|      ! 0 |  2513 | `	const char *zFuncName = 0;` |
|      ! 0 |  2514 | `	int nFuncLen = 0;` |
|        - |  2515 | `	va_list ap;` |
|        - |  2516 | `	sxi32 rc;` |
|        - |  2517 |  |
|      ! 0 |  2518 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  2519 | `		return PH7_OK;` |
|        - |  2520 | `	}` |
|      ! 0 |  2521 | `	pVm = pCtx->pVm;` |
|      ! 0 |  2522 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  2523 | `		zClass = "Error";` |
|      ! 0 |  2524 | `	}` |
|        - |  2525 |  |
|      ! 0 |  2526 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2527 |  |
|      ! 0 |  2528 | `	va_start(ap,zFormat);` |
|      ! 0 |  2529 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  2530 | `	va_end(ap);` |
|        - |  2531 |  |
|      ! 0 |  2532 | `	if( pCtx->pFunc ){` |
|      ! 0 |  2533 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  2534 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  2535 | `	}` |
|      ! 0 |  2536 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  2537 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  2538 | `	}` |
|      ! 0 |  2539 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  2540 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  2541 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  2542 | `	return rc;` |
|      ! 0 |  2543 |  |
|        - |  2544 | `/*` |
|        - |  2545 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  2546 | ` *` |
|        - |  2547 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  2548 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  2549 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  2550 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  2551 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  2552 | ` * then the program execution is halted.` |
|        - |  2553 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  2554 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  2555 | ` * or to reset the VM to it's initial state.` |
|        - |  2556 | ` */` |
|    31082 |  2557 | `static sxi32 VmByteCodeExec(` |
|        - |  2558 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2559 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  2560 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  2561 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  2562 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  2563 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  2564 | `	int is_callback      /* TRUE if we are executing a callback */` |
|        - |  2565 | `	)` |
|        2 |  2566 |  |
|        - |  2567 | `	VmInstr *pInstr;` |
|        - |  2568 | `	ph7_value *pTos;` |
|        - |  2569 | `	SySet aArg;` |
|        - |  2570 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  2571 | `	sxi32 pc;` |
|        - |  2572 | `	sxi32 rc;` |
|        - |  2573 | `	/* Argument container */` |
|    31084 |  2574 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    31084 |  2575 | `	if( nTos < 0 ){` |
|    29340 |  2576 | `		pTos = &pStack[-1];` |
|    14671 |  2577 | `	}else{` |
|     1746 |  2578 | `		pTos = &pStack[nTos];` |
|        - |  2579 | `	}` |
|    31084 |  2580 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    31084 |  2581 | `	pc = 0;` |
|        - |  2582 | `	/* Execute as much as we can */` |
|  4858204 |  2583 | `	for(;;){` |
|        - |  2584 | `		/* Fetch the instruction to execute */` |
|  9715706 |  2585 | `		pInstr = &aInstr[pc];` |
|  9715706 |  2586 | `		rc = SXRET_OK;` |
|        - |  2587 | `/*` |
|        - |  2588 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  2589 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  2590 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  2591 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  2592 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  2593 | ` */` |
|  9715706 |  2594 | `		switch(pInstr->iOp){` |
|        - |  2595 | `/*` |
|        - |  2596 | ` * DONE: P1 * *` |
|        - |  2597 | ` *` |
|        - |  2598 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  2599 | ` * and return immediately.` |
|        - |  2600 | ` */` |
|    15298 |  2601 | `case PH7_OP_DONE:` |
|    30598 |  2602 | `	if( pInstr->iP1 ){` |
|        - |  2603 | `#ifdef UNTRUST` |
|        - |  2604 | `		if( pTos < pStack ){` |
|        - |  2605 | `			goto Abort;` |
|        - |  2606 | `		}` |
|        - |  2607 | `#endif` |
|    17688 |  2608 | `		if( pLastRef ){` |
|    11514 |  2609 | `			*pLastRef = pTos->nIdx;` |
|     5756 |  2610 | `		}` |
|    17688 |  2611 | `		if( pResult ){` |
|        - |  2612 | `			/* Execution result */` |
|    16834 |  2613 | `			PH7_MemObjStore(pTos,pResult);` |
|     8416 |  2614 | `		}` |
|    17688 |  2615 | `		VmPopOperand(&pTos,1);` |
|    21755 |  2616 | `	}else if( pLastRef ){` |
|        - |  2617 | `		/* Nothing referenced */` |
|      956 |  2618 | `		*pLastRef = SXU32_HIGH;` |
|      477 |  2619 | `	}` |
|        - |  2620 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  2621 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  2622 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  2623 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  2624 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  2625 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  2626 | `	 * block can override it.` |
|        - |  2627 | `	 */` |
|    30600 |  2628 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  2629 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  2630 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  2631 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  2632 | `		pExc->pFrame = 0;` |
|        3 |  2633 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  2634 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  2635 | `			pExc->iFinallyDone = 1;` |
|        - |  2636 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  2637 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  2638 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  2639 | `				goto Abort;` |
|        - |  2640 | `			}` |
|        1 |  2641 | `		}` |
|        1 |  2642 | `	}` |
|    30598 |  2643 | `	goto Done;` |
|        - |  2644 | `/*` |
|        - |  2645 | ` * HALT: P1 * *` |
|        - |  2646 | ` *` |
|        - |  2647 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  2648 | ` * and abort immediately.` |
|        - |  2649 | ` */` |
|        4 |  2650 | `case PH7_OP_HALT:` |
|        9 |  2651 | `	if( pInstr->iP1 ){` |
|        - |  2652 | `#ifdef UNTRUST` |
|        - |  2653 | `		if( pTos < pStack ){` |
|        - |  2654 | `			goto Abort;` |
|        - |  2655 | `		}` |
|        - |  2656 | `#endif` |
|        9 |  2657 | `		if( pLastRef ){` |
|      ! 0 |  2658 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  2659 | `		}` |
|        9 |  2660 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  2661 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  2662 | `				/* Output the exit message */` |
|        7 |  2663 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  2664 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  2665 | `				if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  2666 | `					/* Increment output length */` |
|        5 |  2667 | `					pVm->nOutputLen += SyBlobLength(&pTos->sBlob);` |
|        2 |  2668 | `				}` |
|        3 |  2669 | `			}` |
|        7 |  2670 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  2671 | `			/* Record exit status */` |
|        5 |  2672 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  2673 | `		}` |
|        9 |  2674 | `		VmPopOperand(&pTos,1);` |
|        4 |  2675 | `	}else if( pLastRef ){` |
|        - |  2676 | `		/* Nothing referenced */` |
|      ! 0 |  2677 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  2678 | `	}` |
|        - |  2679 | `	/* Check if we're in an included file context */` |
|        9 |  2680 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  2681 | `		/* Terminate the entire process */` |
|        9 |  2682 | `		exit(pVm->iExitStatus);` |
|        - |  2683 | `	}` |
|      ! 0 |  2684 | `	goto Abort;` |
|        - |  2685 | `/*` |
|        - |  2686 | ` * JMP: * P2 *` |
|        - |  2687 | ` *` |
|        - |  2688 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  2689 | ` * the one at index P2 from the beginning of the program.` |
|        - |  2690 | ` */` |
|   209739 |  2691 | `case PH7_OP_JMP:` |
|   419524 |  2692 | `	pc = pInstr->iP2 - 1;` |
|   419524 |  2693 | `	break;` |
|        - |  2694 | `/*` |
|        - |  2695 | ` * JZ: P1 P2 *` |
|        - |  2696 | ` *` |
|        - |  2697 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  2698 | ` * entry in the stack if P1 is zero.` |
|        - |  2699 | ` */` |
|   489395 |  2700 | `case PH7_OP_JZ:` |
|        - |  2701 | `#ifdef UNTRUST` |
|        - |  2702 | `	if( pTos < pStack ){` |
|        - |  2703 | `		goto Abort;` |
|        - |  2704 | `	}` |
|        - |  2705 | `#endif` |
|        - |  2706 | `	/* Get a boolean value */` |
|   978880 |  2707 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      138 |  2708 | `		PH7_MemObjToBool(pTos);` |
|       68 |  2709 | `	}` |
|   978880 |  2710 | `	if( !pTos->x.iVal ){` |
|        - |  2711 | `		/* Take the jump */` |
|   492642 |  2712 | `		pc = pInstr->iP2 - 1;` |
|   246320 |  2713 | `	}` |
|   978880 |  2714 | `	if( !pInstr->iP1 ){` |
|   781140 |  2715 | `		VmPopOperand(&pTos,1);` |
|   390591 |  2716 | `	}` |
|   978880 |  2717 | `	break;` |
|        - |  2718 | `/*` |
|        - |  2719 | ` * JNZ: P1 P2 *` |
|        - |  2720 | ` *` |
|        - |  2721 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  2722 | ` * entry in the stack if P1 is zero.` |
|        - |  2723 | ` */` |
|    53316 |  2724 | `case PH7_OP_JNZ:` |
|        - |  2725 | `#ifdef UNTRUST` |
|        - |  2726 | `	if( pTos < pStack ){` |
|        - |  2727 | `		goto Abort;` |
|        - |  2728 | `	}` |
|        - |  2729 | `#endif` |
|        - |  2730 | `	/* Get a boolean value */` |
|   106634 |  2731 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2732 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  2733 | `	}` |
|   106634 |  2734 | `	if( pTos->x.iVal ){` |
|        - |  2735 | `		/* Take the jump */` |
|     4316 |  2736 | `		pc = pInstr->iP2 - 1;` |
|     2157 |  2737 | `	}` |
|   106634 |  2738 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  2739 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  2740 | `	}` |
|   106634 |  2741 | `	break;` |
|        - |  2742 | `/*` |
|        - |  2743 | ` * NOOP: * * *` |
|        - |  2744 | ` *` |
|        - |  2745 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  2746 | ` * destination.` |
|        - |  2747 | ` */` |
|      ! 0 |  2748 | `case PH7_OP_NOOP:` |
|      ! 0 |  2749 | `	break;` |
|        - |  2750 | `/*` |
|        - |  2751 | ` * POP: P1 * *` |
|        - |  2752 | ` *` |
|        - |  2753 | ` * Pop P1 elements from the operand stack.` |
|        - |  2754 | ` */` |
|   381062 |  2755 | `case PH7_OP_POP: {` |
|   762170 |  2756 | `	sxi32 n = pInstr->iP1;` |
|   762170 |  2757 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  2758 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|      ! 0 |  2759 | `		n = (sxi32)(pTos - pStack);` |
|      ! 0 |  2760 | `	}` |
|   762170 |  2761 | `	VmPopOperand(&pTos,n);` |
|   762170 |  2762 | `	break;` |
|        - |  2763 | `				 }` |
|        - |  2764 | `/*` |
|        - |  2765 | ` * DUP: * * *` |
|        - |  2766 | ` *` |
|        - |  2767 | ` * Duplicate the top of the stack.` |
|        - |  2768 | ` */` |
|       35 |  2769 | `case PH7_OP_DUP:` |
|        - |  2770 | `#ifdef UNTRUST` |
|        - |  2771 | `	if( pTos < pStack ){` |
|        - |  2772 | `		goto Abort;` |
|        - |  2773 | `	}` |
|        - |  2774 | `#endif` |
|       72 |  2775 | `	pTos++;` |
|       72 |  2776 | `	PH7_MemObjInit(pVm,pTos);` |
|       72 |  2777 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       72 |  2778 | `	break;` |
|        - |  2779 | `/*` |
|        - |  2780 | ` * NSSWITCH: * * P3` |
|        - |  2781 | ` *` |
|        - |  2782 | ` * Switch the active namespace at runtime.` |
|        - |  2783 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  2784 | ` */` |
|     6218 |  2785 | `case PH7_OP_NSSWITCH:` |
|    12438 |  2786 | `	SyBlobReset(&pVm->sNamespace);` |
|    12438 |  2787 | `	if( pInstr->p3 ){` |
|       51 |  2788 | `		const char *zNs = (const char *)pInstr->p3;` |
|       51 |  2789 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       25 |  2790 | `	}` |
|    12438 |  2791 | `	break;` |
|        - |  2792 | `/*` |
|        - |  2793 | ` * CVT_INT: * * *` |
|        - |  2794 | ` *` |
|        - |  2795 | ` * Force the top of the stack to be an integer.` |
|        - |  2796 | ` */` |
|       35 |  2797 | `case PH7_OP_CVT_INT:` |
|        - |  2798 | `#ifdef UNTRUST` |
|        - |  2799 | `	if( pTos < pStack ){` |
|        - |  2800 | `		goto Abort;` |
|        - |  2801 | `	}` |
|        - |  2802 | `#endif` |
|       72 |  2803 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|       25 |  2804 | `		PH7_MemObjToInteger(pTos);` |
|       12 |  2805 | `	}` |
|        - |  2806 | `	/* Invalidate any prior representation */` |
|       72 |  2807 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       72 |  2808 | `	break;` |
|        - |  2809 | `/*` |
|        - |  2810 | ` * CVT_REAL: * * *` |
|        - |  2811 | ` *` |
|        - |  2812 | ` * Force the top of the stack to be a real.` |
|        - |  2813 | ` */` |
|        4 |  2814 | `case PH7_OP_CVT_REAL:` |
|        - |  2815 | `#ifdef UNTRUST` |
|        - |  2816 | `	if( pTos < pStack ){` |
|        - |  2817 | `		goto Abort;` |
|        - |  2818 | `	}` |
|        - |  2819 | `#endif` |
|        9 |  2820 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  2821 | `		PH7_MemObjToReal(pTos);` |
|        2 |  2822 | `	}` |
|        - |  2823 | `	/* Invalidate any prior representation */` |
|        9 |  2824 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|        9 |  2825 | `	break;` |
|        - |  2826 | `/*` |
|        - |  2827 | ` * CVT_STR: * * *` |
|        - |  2828 | ` *` |
|        - |  2829 | ` * Force the top of the stack to be a string.` |
|        - |  2830 | ` */` |
|      146 |  2831 | `case PH7_OP_CVT_STR:` |
|        - |  2832 | `#ifdef UNTRUST` |
|        - |  2833 | `	if( pTos < pStack ){` |
|        - |  2834 | `		goto Abort;` |
|        - |  2835 | `	}` |
|        - |  2836 | `#endif` |
|      294 |  2837 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  2838 | `		PH7_MemObjToString(pTos);` |
|      146 |  2839 | `	}` |
|      294 |  2840 | `	break;` |
|        - |  2841 | `/*` |
|        - |  2842 | ` * CVT_BOOL: * * *` |
|        - |  2843 | ` *` |
|        - |  2844 | ` * Force the top of the stack to be a boolean.` |
|        - |  2845 | ` */` |
|        5 |  2846 | `case PH7_OP_CVT_BOOL:` |
|        - |  2847 | `#ifdef UNTRUST` |
|        - |  2848 | `	if( pTos < pStack ){` |
|        - |  2849 | `		goto Abort;` |
|        - |  2850 | `	}` |
|        - |  2851 | `#endif` |
|       11 |  2852 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  2853 | `		PH7_MemObjToBool(pTos);` |
|        3 |  2854 | `	}` |
|       11 |  2855 | `	break;` |
|        - |  2856 | `/*` |
|        - |  2857 | ` * CVT_NULL: * * *` |
|        - |  2858 | ` *` |
|        - |  2859 | ` * Nullify the top of the stack.` |
|        - |  2860 | ` */` |
|        3 |  2861 | `case PH7_OP_CVT_NULL:` |
|        - |  2862 | `#ifdef UNTRUST` |
|        - |  2863 | `	if( pTos < pStack ){` |
|        - |  2864 | `		goto Abort;` |
|        - |  2865 | `	}` |
|        - |  2866 | `#endif` |
|        7 |  2867 | `	PH7_MemObjRelease(pTos);` |
|        7 |  2868 | `	break;` |
|        - |  2869 | `/*` |
|        - |  2870 | ` * CVT_NUMC: * * *` |
|        - |  2871 | ` *` |
|        - |  2872 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  2873 | ` */` |
|      ! 0 |  2874 | `case PH7_OP_CVT_NUMC:` |
|        - |  2875 | `#ifdef UNTRUST` |
|        - |  2876 | `	if( pTos < pStack ){` |
|        - |  2877 | `		goto Abort;` |
|        - |  2878 | `	}` |
|        - |  2879 | `#endif` |
|        - |  2880 | `	/* Force a numeric cast */` |
|      ! 0 |  2881 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  2882 | `	break;` |
|        - |  2883 | `/*` |
|        - |  2884 | ` * CVT_ARRAY: * * *` |
|        - |  2885 | ` *` |
|        - |  2886 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  2887 | ` */` |
|       10 |  2888 | `case PH7_OP_CVT_ARRAY:` |
|        - |  2889 | `#ifdef UNTRUST` |
|        - |  2890 | `	if( pTos < pStack ){` |
|        - |  2891 | `		goto Abort;` |
|        - |  2892 | `	}` |
|        - |  2893 | `#endif` |
|        - |  2894 | `	/* Force a hashmap cast */` |
|       21 |  2895 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  2896 | `	if( rc != SXRET_OK ){` |
|        - |  2897 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  2898 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  2899 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  2900 | `	}` |
|       21 |  2901 | `	break;` |
|        - |  2902 | `/*` |
|        - |  2903 | ` * CVT_OBJ: * * *` |
|        - |  2904 | ` *` |
|        - |  2905 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  2906 | ` */` |
|        8 |  2907 | `case PH7_OP_CVT_OBJ:` |
|        - |  2908 | `#ifdef UNTRUST` |
|        - |  2909 | `	if( pTos < pStack ){` |
|        - |  2910 | `		goto Abort;` |
|        - |  2911 | `	}` |
|        - |  2912 | `#endif` |
|       17 |  2913 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  2914 | `		/* Force a 'stdClass()' cast */` |
|       17 |  2915 | `		PH7_MemObjToObject(pTos);` |
|        8 |  2916 | `	}` |
|       17 |  2917 | `	break;` |
|        - |  2918 | `/*` |
|        - |  2919 | ` * ERR_CTRL * * *` |
|        - |  2920 | ` *` |
|        - |  2921 | ` * Error control operator.` |
|        - |  2922 | ` */` |
|    12363 |  2923 | `case PH7_OP_ERR_CTRL:` |
|        - |  2924 | `	/*` |
|        - |  2925 | `	 * TICKET 1433-038:` |
|        - |  2926 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  2927 | `	 * use the public API,to control error output.` |
|        - |  2928 | `	 */` |
|    24726 |  2929 | `	break;` |
|        - |  2930 | `/*` |
|        - |  2931 | ` * IS_A * * *` |
|        - |  2932 | ` *` |
|        - |  2933 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  2934 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  2935 | ` * holding a class name or an object).` |
|        - |  2936 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  2937 | ` */` |
|       23 |  2938 | `case PH7_OP_IS_A:{` |
|       48 |  2939 | `	ph7_value *pNos = &pTos[-1];` |
|       48 |  2940 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  2941 | `#ifdef UNTRUST` |
|        - |  2942 | `	if( pNos < pStack ){` |
|        - |  2943 | `		goto Abort;` |
|        - |  2944 | `	}` |
|        - |  2945 | `#endif` |
|       48 |  2946 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       46 |  2947 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       46 |  2948 | `		ph7_class *pClass = 0;` |
|        - |  2949 | `		/* Extract the target class */` |
|       46 |  2950 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  2951 | `			/* Instance already loaded */` |
|      ! 0 |  2952 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       46 |  2953 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       46 |  2954 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       46 |  2955 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  2956 | `			/* Handle self/static/parent keywords */` |
|       46 |  2957 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  2958 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       44 |  2959 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  2960 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       43 |  2961 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  2962 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  2963 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  2964 | `					pClass = pSelf->pBase;` |
|        2 |  2965 | `				}` |
|        3 |  2966 | `			}else{` |
|       36 |  2967 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  2968 | `			}` |
|       22 |  2969 | `		}` |
|       46 |  2970 | `		if( pClass ){` |
|        - |  2971 | `			/* Perform the query */` |
|       46 |  2972 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       22 |  2973 | `		}` |
|       22 |  2974 | `	}` |
|        - |  2975 | `	/* Push result */` |
|       48 |  2976 | `	VmPopOperand(&pTos,1);` |
|       48 |  2977 | `	PH7_MemObjRelease(pTos);` |
|       48 |  2978 | `	pTos->x.iVal = iRes;` |
|       48 |  2979 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       48 |  2980 | `	break;` |
|        - |  2981 | `				 }` |
|        - |  2982 |  |
|        - |  2983 | `/*` |
|        - |  2984 | ` * LOADC P1 P2 *` |
|        - |  2985 | ` *` |
|        - |  2986 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  2987 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  2988 | ` */` |
|   803772 |  2989 | `case PH7_OP_LOADC: {` |
|        - |  2990 | `	ph7_value *pObj;` |
|        - |  2991 | `	/* Reserve a room */` |
|  1607590 |  2992 | `	pTos++;` |
|  2403466 |  2993 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1607590 |  2994 | `		if( pInstr->iP1 == 1 && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  2995 | `			SyHashEntry *pEntry;` |
|        - |  2996 | `			/* Candidate for expansion via user defined callbacks */` |
|    15884 |  2997 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    15884 |  2998 | `			if( pEntry ){` |
|    15880 |  2999 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  3000 | `				/* Set a NULL default value */` |
|    15880 |  3001 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    15880 |  3002 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  3003 | `				/* Invoke the callback and deal with the expanded value */` |
|    15880 |  3004 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  3005 | `				/* Mark as constant */` |
|    15880 |  3006 | `				pTos->nIdx = SXU32_HIGH;` |
|    15880 |  3007 | `				break;` |
|        - |  3008 | `			}` |
|        - |  3009 | `			/* Constant not found.  For qualified names (containing '\')` |
|        - |  3010 | `			 * this is always an error — bare unqualified names still fall` |
|        - |  3011 | `			 * through to string value for backward compatibility. */` |
|        - |  3012 | `			{` |
|        6 |  3013 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  3014 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  3015 | `				sxu32 j;` |
|       32 |  3016 | `				for( j = 0; j < nLit; j++ ){` |
|       30 |  3017 | `					if( zLit[j] == '\\' ){` |
|        - |  3018 | `						/* Qualified name: must be a real constant.` |
|        - |  3019 | `						 * Format as PHP Fatal error to match PHP behavior. */` |
|        - |  3020 | `						{` |
|        3 |  3021 | `							SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  3022 | `							SyBlob sErr;` |
|        3 |  3023 | `							SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  3024 | `							SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  3025 | `							if( pErrFile ){` |
|        3 |  3026 | `								SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  3027 | `							}` |
|        3 |  3028 | `							SyBlobAppend(&sErr,"\n",1);` |
|        3 |  3029 | `							VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  3030 | `							SyBlobRelease(&sErr);` |
|        - |  3031 | `						}` |
|        3 |  3032 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  3033 | `						pTos->nIdx = SXU32_HIGH;` |
|        3 |  3034 | `						goto LoadC_Done;` |
|        - |  3035 | `					}` |
|       15 |  3036 | `				}` |
|        - |  3037 | `			}` |
|        1 |  3038 | `		}` |
|  1591710 |  3039 | `		PH7_MemObjLoad(pObj,pTos);` |
|   795878 |  3040 | `	}else{` |
|        - |  3041 | `		/* Set a NULL value */` |
|      ! 0 |  3042 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3043 | `	}` |
|   795833 |  3044 | `LoadC_Done:` |
|        - |  3045 | `	/* Mark as constant */` |
|  1591712 |  3046 | `	pTos->nIdx = SXU32_HIGH;` |
|  1591712 |  3047 | `	break;` |
|        - |  3048 | `				  }` |
|        - |  3049 | `/*` |
|        - |  3050 | ` * LOAD: P1 * P3` |
|        - |  3051 | ` *` |
|        - |  3052 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  3053 | ` * from the P3 operand.` |
|        - |  3054 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  3055 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  3056 | ` */` |
|  1326384 |  3057 | `case PH7_OP_LOAD:{` |
|        - |  3058 | `	ph7_value *pObj;` |
|        - |  3059 | `	SyString sName;` |
|  2652990 |  3060 | `	if( pInstr->p3 == 0 ){` |
|        - |  3061 | `		/* Take the variable name from the top of the stack */` |
|        - |  3062 | `#ifdef UNTRUST` |
|        - |  3063 | `		if( pTos < pStack ){` |
|        - |  3064 | `			goto Abort;` |
|        - |  3065 | `		}` |
|        - |  3066 | `#endif` |
|        - |  3067 | `		/* Force a string cast */` |
|       19 |  3068 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  3069 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3070 | `		}` |
|       19 |  3071 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  3072 | `	}else{` |
|  2652972 |  3073 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3074 | `		/* Reserve a room for the target object */` |
|  2652972 |  3075 | `		pTos++;` |
|        - |  3076 | `	}` |
|        - |  3077 | `	/* Extract the requested memory object */` |
|  2652990 |  3078 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  2652990 |  3079 | `	if( pObj == 0 ){` |
|      624 |  3080 | `		if( pInstr->iP1 ){` |
|        - |  3081 | `			/* Variable not found,load NULL */` |
|      624 |  3082 | `			if( !pInstr->p3 ){` |
|      ! 0 |  3083 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3084 | `			}else{` |
|      624 |  3085 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3086 | `			}` |
|      624 |  3087 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1326697 |  3088 | `			break;` |
|      ! 0 |  3089 | `		}else{` |
|        - |  3090 | `			/* Fatal error */` |
|      ! 0 |  3091 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3092 | `			goto Abort;` |
|        - |  3093 | `		}` |
|        - |  3094 | `	}` |
|        - |  3095 | `	/* Load variable contents */` |
|  2652368 |  3096 | `	PH7_MemObjLoad(pObj,pTos);` |
|  2652368 |  3097 | `	pTos->nIdx = pObj->nIdx;` |
|  2652368 |  3098 | `	break;` |
|        - |  3099 | `				   }` |
|        - |  3100 | `/*` |
|        - |  3101 | ` * LOAD_MAP P1 * *` |
|        - |  3102 | ` *` |
|        - |  3103 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  3104 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  3105 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  3106 | ` */` |
|    17874 |  3107 | `case PH7_OP_LOAD_MAP: {` |
|        - |  3108 | `	ph7_hashmap *pMap;` |
|        - |  3109 | `	/* Allocate a new hashmap instance */` |
|    35750 |  3110 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    35750 |  3111 | `	if( pMap == 0 ){` |
|      ! 0 |  3112 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  3113 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  3114 | `		goto Abort;` |
|        - |  3115 | `	}` |
|    35750 |  3116 | `	if( pInstr->iP1 > 0 ){` |
|     2180 |  3117 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  3118 | `		/* Perform the insertion */` |
|     6638 |  3119 | `		while( pEntry < pTos ){` |
|     4460 |  3120 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  3121 | `				/* Insertion by reference */` |
|      142 |  3122 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  3123 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  3124 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  3125 | `					);` |
|       48 |  3126 | `			}else{` |
|        - |  3127 | `				/* Standard insertion */` |
|     6548 |  3128 | `				PH7_HashmapInsert(pMap,` |
|     4364 |  3129 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2182 |  3130 | `					&pEntry[1]` |
|        - |  3131 | `				);` |
|        - |  3132 | `			}` |
|        - |  3133 | `			/* Next pair on the stack */` |
|     4460 |  3134 | `			pEntry += 2;` |
|        2 |  3135 | `		}` |
|        - |  3136 | `		/* Pop P1 elements */` |
|     2180 |  3137 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1089 |  3138 | `	}` |
|        - |  3139 | `	/* Push the hashmap */` |
|    35750 |  3140 | `	pTos++;` |
|    35750 |  3141 | `	pTos->nIdx = SXU32_HIGH;` |
|    35750 |  3142 | `	pTos->x.pOther = pMap;` |
|    35750 |  3143 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    35750 |  3144 | `	break;` |
|        - |  3145 | `					  }` |
|        - |  3146 | `/*` |
|        - |  3147 | ` * LOAD_LIST: P1 * *` |
|        - |  3148 | ` *` |
|        - |  3149 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  3150 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  3151 | ` * Caveats:` |
|        - |  3152 | ` *  This implementation support only a single nesting level.` |
|        - |  3153 | ` */` |
|       26 |  3154 | `case PH7_OP_LOAD_LIST: {` |
|        - |  3155 | `	ph7_value *pEntry;` |
|       54 |  3156 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  3157 | `		/* Empty list,break immediately */` |
|      ! 0 |  3158 | `		break;` |
|        - |  3159 | `	}` |
|       54 |  3160 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  3161 | `#ifdef UNTRUST` |
|        - |  3162 | `	if( &pEntry[-1] < pStack ){` |
|        - |  3163 | `		goto Abort;` |
|        - |  3164 | `	}` |
|        - |  3165 | `#endif` |
|       54 |  3166 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       50 |  3167 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  3168 | `		ph7_hashmap_node *pNode;` |
|        - |  3169 | `		ph7_value sKey,*pObj;` |
|        - |  3170 | `		/* Start Copying */` |
|       50 |  3171 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      154 |  3172 | `		while( pEntry <= pTos ){` |
|      106 |  3173 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|       98 |  3174 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|       98 |  3175 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       98 |  3176 | `					if( rc == SXRET_OK ){` |
|        - |  3177 | `						/* Store node value */` |
|       98 |  3178 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       50 |  3179 | `					}else{` |
|        - |  3180 | `						/* Nullify the variable */` |
|      ! 0 |  3181 | `						PH7_MemObjRelease(pObj);` |
|        - |  3182 | `					}` |
|       48 |  3183 | `				}` |
|       48 |  3184 | `			}` |
|      106 |  3185 | `			sKey.x.iVal++; /* Next numeric index */` |
|      106 |  3186 | `			pEntry++;` |
|        2 |  3187 | `		}` |
|       24 |  3188 | `	}` |
|       54 |  3189 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       54 |  3190 | `	break;` |
|        - |  3191 | `					   }` |
|        - |  3192 | `/*` |
|        - |  3193 | ` * LOAD_IDX: P1 P2 *` |
|        - |  3194 | ` *` |
|        - |  3195 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  3196 | ` * from the stack.` |
|        - |  3197 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  3198 | ` * instead.` |
|        - |  3199 | ` */` |
|   214771 |  3200 | `case PH7_OP_LOAD_IDX: {` |
|   429588 |  3201 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   429588 |  3202 | `	ph7_hashmap *pMap = 0;` |
|        - |  3203 | `	ph7_value *pIdx;` |
|   429588 |  3204 | `	pIdx = 0;` |
|   429588 |  3205 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  3206 | `		if( !pInstr->iP2){` |
|        - |  3207 | `			/* No available index,load NULL */` |
|      ! 0 |  3208 | `			if( pTos >= pStack ){` |
|      ! 0 |  3209 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3210 | `			}else{` |
|        - |  3211 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  3212 | `				pTos++;` |
|      ! 0 |  3213 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  3214 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  3215 | `			}` |
|        - |  3216 | `			/* Emit a notice */` |
|      ! 0 |  3217 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  3218 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  3219 | `			break;` |
|        - |  3220 | `		}` |
|      ! 0 |  3221 | `	}else{` |
|   429588 |  3222 | `		pIdx = pTos;` |
|   429588 |  3223 | `		pTos--;` |
|        - |  3224 | `	}` |
|   429588 |  3225 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  3226 | `		/* String access */` |
|   340922 |  3227 | `		if( pIdx ){` |
|        - |  3228 | `			sxu32 nOfft;` |
|   340922 |  3229 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  3230 | `				/* Force an int cast */` |
|      ! 0 |  3231 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  3232 | `			}` |
|   340922 |  3233 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   340922 |  3234 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  3235 | `				/* Invalid offset,load null */` |
|      ! 0 |  3236 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3237 | `			}else{` |
|   340922 |  3238 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   340922 |  3239 | `				int c = zData[nOfft];` |
|   340922 |  3240 | `				PH7_MemObjRelease(pTos);` |
|   340922 |  3241 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   340922 |  3242 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  3243 | `			}` |
|   170484 |  3244 | `		}else{` |
|        - |  3245 | `			/* No available index,load NULL */` |
|      ! 0 |  3246 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  3247 | `		}` |
|   340922 |  3248 | `		break;` |
|        - |  3249 | `	}` |
|    88668 |  3250 | `	if( pInstr->iP2 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      ! 0 |  3251 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3252 | `			ph7_value *pObj;` |
|      ! 0 |  3253 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      ! 0 |  3254 | `				PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3255 | `				PH7_MemObjLoad(pObj,pTos);` |
|      ! 0 |  3256 | `			}` |
|      ! 0 |  3257 | `		}` |
|      ! 0 |  3258 | `	}` |
|    88668 |  3259 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|    88668 |  3260 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3261 | `		/* Point to the hashmap */` |
|    88668 |  3262 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|    88668 |  3263 | `		if( pIdx ){` |
|        - |  3264 | `			/* Load the desired entry */` |
|    88668 |  3265 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    44333 |  3266 | `		}` |
|    88668 |  3267 | `		if( rc != SXRET_OK && pInstr->iP2 ){` |
|        - |  3268 | `			/* Create a new empty entry */` |
|      ! 0 |  3269 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      ! 0 |  3270 | `			if( rc == SXRET_OK ){` |
|        - |  3271 | `				/* Point to the last inserted entry */` |
|      ! 0 |  3272 | `				pNode = pMap->pLast;` |
|      ! 0 |  3273 | `			}` |
|      ! 0 |  3274 | `		}` |
|    44333 |  3275 | `	}` |
|    88668 |  3276 | `	if( pIdx ){` |
|    88668 |  3277 | `		PH7_MemObjRelease(pIdx);` |
|    44333 |  3278 | `	}` |
|    88668 |  3279 | `	if( rc == SXRET_OK ){` |
|        - |  3280 | `		/* Load entry contents */` |
|    40376 |  3281 | `		if( pMap->iRef < 2 ){` |
|        - |  3282 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  3283 | `			 * of the entry value,rather than pointing to it.` |
|        - |  3284 | `			 */` |
|       24 |  3285 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  3286 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  3287 | `		}else{` |
|    40354 |  3288 | `			pTos->nIdx = pNode->nValIdx;` |
|    40354 |  3289 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    40354 |  3290 | `			PH7_HashmapUnref(pMap);` |
|        - |  3291 | `		}` |
|    20189 |  3292 | `	}else{` |
|        - |  3293 | `		/* No such entry,load NULL */` |
|    48294 |  3294 | `		PH7_MemObjRelease(pTos);` |
|    48294 |  3295 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  3296 | `	}` |
|    88668 |  3297 | `	break;` |
|        - |  3298 | `					  }` |
|        - |  3299 | `/*` |
|        - |  3300 | ` * LOAD_CLOSURE * * P3` |
|        - |  3301 | ` *` |
|        - |  3302 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  3303 | ` * name in the stack.` |
|        - |  3304 | ` */` |
|        2 |  3305 | `case PH7_OP_LOAD_CLOSURE:{` |
|        5 |  3306 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|        5 |  3307 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  3308 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  3309 | `		ph7_vm_func *pClosure;` |
|        - |  3310 | `		char *zName;` |
|        - |  3311 | `		sxu32 mLen;` |
|        - |  3312 | `		sxu32 n;` |
|        - |  3313 | `		/* Create a new VM function */` |
|        5 |  3314 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  3315 | `		/* Generate an unique closure name */` |
|        5 |  3316 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|        5 |  3317 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  3318 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  3319 | `			goto Abort;` |
|        - |  3320 | `		}` |
|        5 |  3321 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|        5 |  3322 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  3323 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  3324 | `		}` |
|        - |  3325 | `		/* Zero the stucture */` |
|        5 |  3326 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  3327 | `		/* Perform a structure assignment on read-only items */` |
|        5 |  3328 | `		pClosure->aArgs = pFunc->aArgs;` |
|        5 |  3329 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|        5 |  3330 | `		pClosure->aStatic = pFunc->aStatic;` |
|        5 |  3331 | `		pClosure->iFlags = pFunc->iFlags;` |
|        5 |  3332 | `		pClosure->pUserData = pFunc->pUserData;` |
|        5 |  3333 | `		pClosure->sSignature = pFunc->sSignature;` |
|        5 |  3334 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  3335 | `		/* Register the closure */` |
|        5 |  3336 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  3337 | `		/* Set up closure environment */` |
|        5 |  3338 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        5 |  3339 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       13 |  3340 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  3341 | `			ph7_value *pValue;` |
|        9 |  3342 | `			pEnv = &aEnv[n];` |
|        9 |  3343 | `			sEnv.sName  = pEnv->sName;` |
|        9 |  3344 | `			sEnv.iFlags = pEnv->iFlags;` |
|        9 |  3345 | `			sEnv.nIdx = SXU32_HIGH;` |
|        9 |  3346 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|        9 |  3347 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  3348 | `				/* Pass by reference */` |
|      ! 0 |  3349 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  3350 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  3351 | `					);` |
|      ! 0 |  3352 | `			}` |
|        - |  3353 | `			/* Standard pass by value */` |
|        9 |  3354 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|        9 |  3355 | `			if( pValue ){` |
|        - |  3356 | `				/* Copy imported value */` |
|        5 |  3357 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|        2 |  3358 | `			}` |
|        - |  3359 | `			/* Insert the imported variable */` |
|        9 |  3360 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|        5 |  3361 | `		}` |
|        - |  3362 | `		/* Finally,load the closure name on the stack */` |
|        5 |  3363 | `		pTos++;` |
|        5 |  3364 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|        2 |  3365 | `	}` |
|        5 |  3366 | `	break;` |
|        - |  3367 | `						 }` |
|        - |  3368 | `/*` |
|        - |  3369 | ` * STORE * P2 P3` |
|        - |  3370 | ` *` |
|        - |  3371 | ` * Perform a store (Assignment) operation.` |
|        - |  3372 | ` */` |
|   110273 |  3373 | `case PH7_OP_STORE: {` |
|        - |  3374 | `	ph7_value *pObj;` |
|        - |  3375 | `	SyString sName;` |
|        - |  3376 | `#ifdef UNTRUST` |
|        - |  3377 | `	if( pTos < pStack ){` |
|        - |  3378 | `		goto Abort;` |
|        - |  3379 | `	}` |
|        - |  3380 | `#endif` |
|   220548 |  3381 | `	if( pInstr->iP2 ){` |
|        - |  3382 | `		sxu32 nIdx;` |
|        - |  3383 | `		/* Member store operation */` |
|     2922 |  3384 | `		nIdx = pTos->nIdx;` |
|     2922 |  3385 | `		VmPopOperand(&pTos,1);` |
|     2922 |  3386 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  3387 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  3388 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  3389 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  3390 | `		}else{` |
|        - |  3391 | `			/* Point to the desired memory object */` |
|     2918 |  3392 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2918 |  3393 | `			if( pObj ){` |
|        - |  3394 | `				/* Perform the store operation */` |
|     2918 |  3395 | `				PH7_MemObjStore(pTos,pObj);` |
|     1458 |  3396 | `			}` |
|        - |  3397 | `		}` |
|   111735 |  3398 | `		break;` |
|   217628 |  3399 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  3400 | `		/* Take the variable name from the next on the stack */` |
|        7 |  3401 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  3402 | `			/* Force a string cast */` |
|      ! 0 |  3403 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  3404 | `		}` |
|        7 |  3405 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  3406 | `		pTos--;` |
|        - |  3407 | `#ifdef UNTRUST` |
|        - |  3408 | `		if( pTos < pStack  ){` |
|        - |  3409 | `			goto Abort;` |
|        - |  3410 | `		}` |
|        - |  3411 | `#endif` |
|        4 |  3412 | `	}else{` |
|   217622 |  3413 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  3414 | `	}` |
|        - |  3415 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   217628 |  3416 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   217628 |  3417 | `	if( pObj == 0 ){` |
|      ! 0 |  3418 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  3419 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  3420 | `		goto Abort;` |
|        - |  3421 | `	}` |
|   217628 |  3422 | `	if( !pInstr->p3 ){` |
|        7 |  3423 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  3424 | `	}` |
|        - |  3425 | `	/* Perform the store operation */` |
|   217628 |  3426 | `	PH7_MemObjStore(pTos,pObj);` |
|   217628 |  3427 | `	break;` |
|        - |  3428 | `				   }` |
|        - |  3429 | `/*` |
|        - |  3430 | ` * STORE_IDX:   P1 * P3` |
|        - |  3431 | ` * STORE_IDX_R: P1 * P3` |
|        - |  3432 | ` *` |
|        - |  3433 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  3434 | ` */` |
|    79940 |  3435 | `case PH7_OP_STORE_IDX:` |
|        - |  3436 | `case PH7_OP_STORE_IDX_REF: {` |
|   159882 |  3437 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  3438 | `	ph7_value *pKey;` |
|        - |  3439 | `	sxu32 nIdx;` |
|   159882 |  3440 | `	if( pInstr->iP1 ){` |
|        - |  3441 | `		/* Key is next on stack */` |
|    56772 |  3442 | `		pKey = pTos;` |
|    56772 |  3443 | `		pTos--;` |
|    28387 |  3444 | `	}else{` |
|   103112 |  3445 | `		pKey = 0;` |
|        - |  3446 | `	}` |
|   159882 |  3447 | `	nIdx = pTos->nIdx;` |
|   159882 |  3448 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  3449 | `		/* Hashmap already loaded */` |
|   159830 |  3450 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   159830 |  3451 | `		if( pMap->iRef < 2 ){` |
|        - |  3452 | `			/* TICKET 1433-48: Prevent garbage collection */` |
|      ! 0 |  3453 | `			pMap->iRef = 2;` |
|      ! 0 |  3454 | `		}` |
|    79916 |  3455 | `	}else{` |
|        - |  3456 | `		ph7_value *pObj;` |
|       53 |  3457 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  3458 | `		if( pObj == 0 ){` |
|      ! 0 |  3459 | `			if( pKey ){` |
|      ! 0 |  3460 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  3461 | `			}` |
|      ! 0 |  3462 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  3463 | `			break;` |
|        - |  3464 | `		}` |
|        - |  3465 | `		/* Phase#1: Load the array */` |
|       53 |  3466 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  3467 | `			VmPopOperand(&pTos,1);` |
|       53 |  3468 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  3469 | `				/* Force a string cast */` |
|      ! 0 |  3470 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  3471 | `			}` |
|       53 |  3472 | `			if( pKey == 0 ){` |
|        - |  3473 | `				/* Append string */` |
|        3 |  3474 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  3475 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  3476 | `				}` |
|        2 |  3477 | `			}else{` |
|        - |  3478 | `				sxu32 nOfft;` |
|       51 |  3479 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  3480 | `					/* Force an int cast */` |
|       51 |  3481 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  3482 | `				}` |
|       51 |  3483 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  3484 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  3485 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  3486 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  3487 | `					zData[nOfft] = zBlob[0];` |
|       26 |  3488 | `				}else{` |
|      ! 0 |  3489 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  3490 | `						/* Perform an append operation */` |
|      ! 0 |  3491 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  3492 | `					}` |
|        - |  3493 | `				}` |
|        - |  3494 | `			}` |
|       53 |  3495 | `			if( pKey ){` |
|       51 |  3496 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  3497 | `			}` |
|       53 |  3498 | `			break;` |
|      ! 0 |  3499 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  3500 | `			/* Force a hashmap cast  */` |
|      ! 0 |  3501 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  3502 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  3503 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  3504 | `				goto Abort;` |
|        - |  3505 | `			}` |
|      ! 0 |  3506 | `		}` |
|      ! 0 |  3507 | `		pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - |  3508 | `	}` |
|   159830 |  3509 | `	VmPopOperand(&pTos,1);` |
|        - |  3510 | `	/* Phase#2: Perform the insertion */` |
|   159830 |  3511 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  3512 | `		/* Insertion by reference */` |
|       15 |  3513 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  3514 | `	}else{` |
|   159816 |  3515 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  3516 | `	}` |
|   159830 |  3517 | `	if( pKey ){` |
|    56722 |  3518 | `		PH7_MemObjRelease(pKey);` |
|    28360 |  3519 | `	}` |
|   159830 |  3520 | `	break;` |
|        - |  3521 | `					   }` |
|        - |  3522 | `/*` |
|        - |  3523 | ` * INCR: P1 * *` |
|        - |  3524 | ` *` |
|        - |  3525 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  3526 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  3527 | ` * the stack and increment after that.` |
|        - |  3528 | ` */` |
|   151251 |  3529 | `case PH7_OP_INCR:` |
|        - |  3530 | `#ifdef UNTRUST` |
|        - |  3531 | `	if( pTos < pStack ){` |
|        - |  3532 | `		goto Abort;` |
|        - |  3533 | `	}` |
|        - |  3534 | `#endif` |
|   302548 |  3535 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   302548 |  3536 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3537 | `			ph7_value *pObj;` |
|   302548 |  3538 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3539 | `				/* Force a numeric cast */` |
|   302548 |  3540 | `				PH7_MemObjToNumeric(pObj);` |
|   302548 |  3541 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3542 | `					pObj->rVal++;` |
|        - |  3543 | `					/* Try to get an integer representation */` |
|      ! 0 |  3544 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3545 | `				}else{` |
|   302548 |  3546 | `					pObj->x.iVal++;` |
|   302548 |  3547 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3548 | `				}` |
|   302548 |  3549 | `				if( pInstr->iP1 ){` |
|        - |  3550 | `					/* Pre-icrement */` |
|       71 |  3551 | `					PH7_MemObjStore(pObj,pTos);` |
|       35 |  3552 | `				}` |
|   151295 |  3553 | `			}` |
|   151297 |  3554 | `		}else{` |
|      ! 0 |  3555 | `			if( pInstr->iP1 ){` |
|        - |  3556 | `				/* Force a numeric cast */` |
|      ! 0 |  3557 | `				PH7_MemObjToNumeric(pTos);` |
|        - |  3558 | `				/* Pre-increment */` |
|      ! 0 |  3559 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3560 | `					pTos->rVal++;` |
|        - |  3561 | `					/* Try to get an integer representation */` |
|      ! 0 |  3562 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3563 | `				}else{` |
|      ! 0 |  3564 | `					pTos->x.iVal++;` |
|      ! 0 |  3565 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3566 | `				}` |
|      ! 0 |  3567 | `			}` |
|        - |  3568 | `		}` |
|   151295 |  3569 | `	}` |
|   302548 |  3570 | `	break;` |
|        - |  3571 | `/*` |
|        - |  3572 | ` * DECR: P1 * *` |
|        - |  3573 | ` *` |
|        - |  3574 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  3575 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  3576 | ` * and decrement after that.` |
|        - |  3577 | ` */` |
|        2 |  3578 | `case PH7_OP_DECR:` |
|        - |  3579 | `#ifdef UNTRUST` |
|        - |  3580 | `	if( pTos < pStack ){` |
|        - |  3581 | `		goto Abort;` |
|        - |  3582 | `	}` |
|        - |  3583 | `#endif` |
|        5 |  3584 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  3585 | `		/* Force a numeric cast */` |
|        5 |  3586 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  3587 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  3588 | `			ph7_value *pObj;` |
|        5 |  3589 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  3590 | `				/* Force a numeric cast */` |
|        5 |  3591 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  3592 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3593 | `					pObj->rVal--;` |
|        - |  3594 | `					/* Try to get an integer representation */` |
|      ! 0 |  3595 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3596 | `				}else{` |
|        5 |  3597 | `					pObj->x.iVal--;` |
|        5 |  3598 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3599 | `				}` |
|        5 |  3600 | `				if( pInstr->iP1 ){` |
|        - |  3601 | `					/* Pre-icrement */` |
|      ! 0 |  3602 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  3603 | `				}` |
|        2 |  3604 | `			}` |
|        3 |  3605 | `		}else{` |
|      ! 0 |  3606 | `			if( pInstr->iP1 ){` |
|        - |  3607 | `				/* Pre-increment */` |
|      ! 0 |  3608 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3609 | `					pTos->rVal--;` |
|        - |  3610 | `					/* Try to get an integer representation */` |
|      ! 0 |  3611 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  3612 | `				}else{` |
|      ! 0 |  3613 | `					pTos->x.iVal--;` |
|      ! 0 |  3614 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  3615 | `				}` |
|      ! 0 |  3616 | `			}` |
|        - |  3617 | `		}` |
|        2 |  3618 | `	}` |
|        5 |  3619 | `	break;` |
|        - |  3620 | `/*` |
|        - |  3621 | ` * UMINUS: * * *` |
|        - |  3622 | ` *` |
|        - |  3623 | ` * Perform a unary minus operation.` |
|        - |  3624 | ` */` |
|    23111 |  3625 | `case PH7_OP_UMINUS:` |
|        - |  3626 | `#ifdef UNTRUST` |
|        - |  3627 | `	if( pTos < pStack ){` |
|        - |  3628 | `		goto Abort;` |
|        - |  3629 | `	}` |
|        - |  3630 | `#endif` |
|        - |  3631 | `	/* Force a numeric (integer,real or both) cast */` |
|    46224 |  3632 | `	PH7_MemObjToNumeric(pTos);` |
|    46224 |  3633 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       32 |  3634 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  3635 | `	}` |
|    46224 |  3636 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    46194 |  3637 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    23096 |  3638 | `	}` |
|    46224 |  3639 | `	break;` |
|        - |  3640 | `/*` |
|        - |  3641 | ` * UPLUS: * * *` |
|        - |  3642 | ` *` |
|        - |  3643 | ` * Perform a unary plus operation.` |
|        - |  3644 | ` */` |
|       16 |  3645 | `case PH7_OP_UPLUS:` |
|        - |  3646 | `#ifdef UNTRUST` |
|        - |  3647 | `	if( pTos < pStack ){` |
|        - |  3648 | `		goto Abort;` |
|        - |  3649 | `	}` |
|        - |  3650 | `#endif` |
|        - |  3651 | `	/* Force a numeric (integer,real or both) cast */` |
|       33 |  3652 | `	PH7_MemObjToNumeric(pTos);` |
|       33 |  3653 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  3654 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  3655 | `	}` |
|       33 |  3656 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       33 |  3657 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       16 |  3658 | `	}` |
|       33 |  3659 | `	break;` |
|        - |  3660 | `/*` |
|        - |  3661 | ` * OP_LNOT: * * *` |
|        - |  3662 | ` *` |
|        - |  3663 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  3664 | ` * with its complement.` |
|        - |  3665 | ` */` |
|    39557 |  3666 | `case PH7_OP_LNOT:` |
|        - |  3667 | `#ifdef UNTRUST` |
|        - |  3668 | `	if( pTos < pStack ){` |
|        - |  3669 | `		goto Abort;` |
|        - |  3670 | `	}` |
|        - |  3671 | `#endif` |
|        - |  3672 | `	/* Force a boolean cast */` |
|    79160 |  3673 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       21 |  3674 | `		PH7_MemObjToBool(pTos);` |
|       10 |  3675 | `	}` |
|    79160 |  3676 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    79160 |  3677 | `	break;` |
|        - |  3678 | `/*` |
|        - |  3679 | ` * OP_BITNOT: * * *` |
|        - |  3680 | ` *` |
|        - |  3681 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  3682 | ` * with its ones-complement.` |
|        - |  3683 | ` */` |
|       14 |  3684 | `case PH7_OP_BITNOT:` |
|        - |  3685 | `#ifdef UNTRUST` |
|        - |  3686 | `	if( pTos < pStack ){` |
|        - |  3687 | `		goto Abort;` |
|        - |  3688 | `	}` |
|        - |  3689 | `#endif` |
|        - |  3690 | `	/* Force an integer cast */` |
|       30 |  3691 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3692 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3693 | `	}` |
|       30 |  3694 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       30 |  3695 | `	break;` |
|        - |  3696 | `/* OP_MUL * * *` |
|        - |  3697 | ` * OP_MUL_STORE * * *` |
|        - |  3698 | ` *` |
|        - |  3699 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  3700 | ` * and push the result back onto the stack.` |
|        - |  3701 | ` */` |
|     1240 |  3702 | `case PH7_OP_MUL:` |
|        - |  3703 | `case PH7_OP_MUL_STORE: {` |
|     2482 |  3704 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3705 | `	/* Force the operand to be numeric */` |
|        - |  3706 | `#ifdef UNTRUST` |
|        - |  3707 | `	if( pNos < pStack ){` |
|        - |  3708 | `		goto Abort;` |
|        - |  3709 | `	}` |
|        - |  3710 | `#endif` |
|     2482 |  3711 | `	PH7_MemObjToNumeric(pTos);` |
|     2482 |  3712 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  3713 | `	/* Perform the requested operation */` |
|     2482 |  3714 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3715 | `		/* Floating point arithemic */` |
|        - |  3716 | `		ph7_real a,b,r;` |
|       17 |  3717 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3718 | `			PH7_MemObjToReal(pTos);` |
|        3 |  3719 | `		}` |
|       17 |  3720 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  3721 | `			PH7_MemObjToReal(pNos);` |
|        3 |  3722 | `		}` |
|       17 |  3723 | `		a = pNos->rVal;` |
|       17 |  3724 | `		b = pTos->rVal;` |
|       17 |  3725 | `		r = a * b;` |
|        - |  3726 | `		/* Push the result */` |
|       17 |  3727 | `		pNos->rVal = r;` |
|       17 |  3728 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3729 | `		/* Try to get an integer representation */` |
|       17 |  3730 | `		PH7_MemObjTryInteger(pNos);` |
|        9 |  3731 | `	}else{` |
|        - |  3732 | `		/* Integer arithmetic */` |
|        - |  3733 | `		sxi64 a,b,r;` |
|     2466 |  3734 | `		a = pNos->x.iVal;` |
|     2466 |  3735 | `		b = pTos->x.iVal;` |
|     2466 |  3736 | `		r = a * b;` |
|        - |  3737 | `		/* Push the result */` |
|     2466 |  3738 | `		pNos->x.iVal = r;` |
|     2466 |  3739 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3740 | `	}` |
|     2482 |  3741 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  3742 | `		ph7_value *pObj;` |
|       19 |  3743 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3744 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  3745 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  3746 | `			PH7_MemObjStore(pNos,pObj);` |
|        9 |  3747 | `		}` |
|        9 |  3748 | `	}` |
|     2482 |  3749 | `	VmPopOperand(&pTos,1);` |
|     2482 |  3750 | `	break;` |
|        - |  3751 | `				 }` |
|        - |  3752 | `/* OP_ADD * * *` |
|        - |  3753 | ` *` |
|        - |  3754 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3755 | ` * and push the result back onto the stack.` |
|        - |  3756 | ` */` |
|      429 |  3757 | `case PH7_OP_ADD:{` |
|      860 |  3758 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3759 | `#ifdef UNTRUST` |
|        - |  3760 | `	if( pNos < pStack ){` |
|        - |  3761 | `		goto Abort;` |
|        - |  3762 | `	}` |
|        - |  3763 | `#endif` |
|        - |  3764 | `	/* Perform the addition */` |
|      860 |  3765 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|      860 |  3766 | `	VmPopOperand(&pTos,1);` |
|      860 |  3767 | `	break;` |
|        - |  3768 | `				}` |
|        - |  3769 | `/*` |
|        - |  3770 | ` * OP_ADD_STORE * * *` |
|        - |  3771 | ` *` |
|        - |  3772 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  3773 | ` * and push the result back onto the stack.` |
|        - |  3774 | ` */` |
|      482 |  3775 | `case PH7_OP_ADD_STORE:{` |
|      966 |  3776 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3777 | `	ph7_value *pObj;` |
|        - |  3778 | `	sxu32 nIdx;` |
|        - |  3779 | `#ifdef UNTRUST` |
|        - |  3780 | `	if( pNos < pStack ){` |
|        - |  3781 | `		goto Abort;` |
|        - |  3782 | `	}` |
|        - |  3783 | `#endif` |
|        - |  3784 | `	/* Perform the addition */` |
|      966 |  3785 | `	nIdx = pTos->nIdx;` |
|      966 |  3786 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  3787 | `	/* Peform the store operation */` |
|      966 |  3788 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  3789 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      966 |  3790 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      966 |  3791 | `		PH7_MemObjStore(pTos,pObj);` |
|      482 |  3792 | `	}` |
|        - |  3793 | `	/* Ticket 1433-35: Perform a stack dup */` |
|      966 |  3794 | `	PH7_MemObjStore(pTos,pNos);` |
|      966 |  3795 | `	VmPopOperand(&pTos,1);` |
|      966 |  3796 | `	break;` |
|        - |  3797 | `				}` |
|        - |  3798 | `/* OP_SUB * * *` |
|        - |  3799 | ` *` |
|        - |  3800 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3801 | ` * first (what was next on the stack) from the second (the` |
|        - |  3802 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3803 | ` */` |
|      299 |  3804 | `case PH7_OP_SUB: {` |
|      600 |  3805 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3806 | `#ifdef UNTRUST` |
|        - |  3807 | `	if( pNos < pStack ){` |
|        - |  3808 | `		goto Abort;` |
|        - |  3809 | `	}` |
|        - |  3810 | `#endif` |
|      600 |  3811 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3812 | `		/* Floating point arithemic */` |
|        - |  3813 | `		ph7_real a,b,r;` |
|       95 |  3814 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3815 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3816 | `		}` |
|       95 |  3817 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  3818 | `			PH7_MemObjToReal(pNos);` |
|        2 |  3819 | `		}` |
|       95 |  3820 | `		a = pNos->rVal;` |
|       95 |  3821 | `		b = pTos->rVal;` |
|       95 |  3822 | `		r = a - b;` |
|        - |  3823 | `		/* Push the result */` |
|       95 |  3824 | `		pNos->rVal = r;` |
|       95 |  3825 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3826 | `		/* Try to get an integer representation */` |
|       95 |  3827 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  3828 | `	}else{` |
|        - |  3829 | `		/* Integer arithmetic */` |
|        - |  3830 | `		sxi64 a,b,r;` |
|      506 |  3831 | `		a = pNos->x.iVal;` |
|      506 |  3832 | `		b = pTos->x.iVal;` |
|      506 |  3833 | `		r = a - b;` |
|        - |  3834 | `		/* Push the result */` |
|      506 |  3835 | `		pNos->x.iVal = r;` |
|      506 |  3836 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3837 | `	}` |
|      600 |  3838 | `	VmPopOperand(&pTos,1);` |
|      600 |  3839 | `	break;` |
|        - |  3840 | `				 }` |
|        - |  3841 | `/* OP_SUB_STORE * * *` |
|        - |  3842 | ` *` |
|        - |  3843 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  3844 | ` * first (what was next on the stack) from the second (the` |
|        - |  3845 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  3846 | ` */` |
|        1 |  3847 | `case PH7_OP_SUB_STORE: {` |
|        3 |  3848 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3849 | `	ph7_value *pObj;` |
|        - |  3850 | `#ifdef UNTRUST` |
|        - |  3851 | `	if( pNos < pStack ){` |
|        - |  3852 | `		goto Abort;` |
|        - |  3853 | `	}` |
|        - |  3854 | `#endif` |
|        3 |  3855 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  3856 | `		/* Floating point arithemic */` |
|        - |  3857 | `		ph7_real a,b,r;` |
|      ! 0 |  3858 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3859 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  3860 | `		}` |
|      ! 0 |  3861 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  3862 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  3863 | `		}` |
|      ! 0 |  3864 | `		a = pTos->rVal;` |
|      ! 0 |  3865 | `		b = pNos->rVal;` |
|      ! 0 |  3866 | `		r = a - b;` |
|        - |  3867 | `		/* Push the result */` |
|      ! 0 |  3868 | `		pNos->rVal = r;` |
|      ! 0 |  3869 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  3870 | `		/* Try to get an integer representation */` |
|      ! 0 |  3871 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  3872 | `	}else{` |
|        - |  3873 | `		/* Integer arithmetic */` |
|        - |  3874 | `		sxi64 a,b,r;` |
|        3 |  3875 | `		a = pTos->x.iVal;` |
|        3 |  3876 | `		b = pNos->x.iVal;` |
|        3 |  3877 | `		r = a - b;` |
|        - |  3878 | `		/* Push the result */` |
|        3 |  3879 | `		pNos->x.iVal = r;` |
|        3 |  3880 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  3881 | `	}` |
|        3 |  3882 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3883 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3884 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3885 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3886 | `	}` |
|        3 |  3887 | `	VmPopOperand(&pTos,1);` |
|        3 |  3888 | `	break;` |
|        - |  3889 | `				 }` |
|        - |  3890 |  |
|        - |  3891 | `/*` |
|        - |  3892 | ` * OP_MOD * * *` |
|        - |  3893 | ` *` |
|        - |  3894 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3895 | ` * first (what was next on the stack) from the second (the` |
|        - |  3896 | ` * top of the stack) and push the remainder after division` |
|        - |  3897 | ` * onto the stack.` |
|        - |  3898 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3899 | ` */` |
|      296 |  3900 | `case PH7_OP_MOD:{` |
|      594 |  3901 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3902 | `	sxi64 a,b,r;` |
|        - |  3903 | `#ifdef UNTRUST` |
|        - |  3904 | `	if( pNos < pStack ){` |
|        - |  3905 | `		goto Abort;` |
|        - |  3906 | `	}` |
|        - |  3907 | `#endif` |
|        - |  3908 | `	/* Force the operands to be integer */` |
|      594 |  3909 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3910 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3911 | `	}` |
|      594 |  3912 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  3913 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  3914 | `	}` |
|        - |  3915 | `	/* Perform the requested operation */` |
|      594 |  3916 | `	a = pNos->x.iVal;` |
|      594 |  3917 | `	b = pTos->x.iVal;` |
|      594 |  3918 | `	if( b == 0 ){` |
|        3 |  3919 | `		r = 0;` |
|        3 |  3920 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3921 | `		/* goto Abort; */` |
|        2 |  3922 | `	}else{` |
|      591 |  3923 | `		r = a%b;` |
|        - |  3924 | `	}` |
|        - |  3925 | `	/* Push the result */` |
|      594 |  3926 | `	pNos->x.iVal = r;` |
|      594 |  3927 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      594 |  3928 | `	VmPopOperand(&pTos,1);` |
|      594 |  3929 | `	break;` |
|        - |  3930 | `				}` |
|        - |  3931 | `/*` |
|        - |  3932 | ` * OP_MOD_STORE * * *` |
|        - |  3933 | ` *` |
|        - |  3934 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3935 | ` * first (what was next on the stack) from the second (the` |
|        - |  3936 | ` * top of the stack) and push the remainder after division` |
|        - |  3937 | ` * onto the stack.` |
|        - |  3938 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  3939 | ` */` |
|        1 |  3940 | `case PH7_OP_MOD_STORE: {` |
|        3 |  3941 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3942 | `	ph7_value *pObj;` |
|        - |  3943 | `	sxi64 a,b,r;` |
|        - |  3944 | `#ifdef UNTRUST` |
|        - |  3945 | `	if( pNos < pStack ){` |
|        - |  3946 | `		goto Abort;` |
|        - |  3947 | `	}` |
|        - |  3948 | `#endif` |
|        - |  3949 | `	/* Force the operands to be integer */` |
|        3 |  3950 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3951 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  3952 | `	}` |
|        3 |  3953 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  3954 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  3955 | `	}` |
|        - |  3956 | `	/* Perform the requested operation */` |
|        3 |  3957 | `	a = pTos->x.iVal;` |
|        3 |  3958 | `	b = pNos->x.iVal;` |
|        3 |  3959 | `	if( b == 0 ){` |
|      ! 0 |  3960 | `		r = 0;` |
|      ! 0 |  3961 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  3962 | `		/* goto Abort; */` |
|      ! 0 |  3963 | `	}else{` |
|        3 |  3964 | `		r = a%b;` |
|        - |  3965 | `	}` |
|        - |  3966 | `	/* Push the result */` |
|        3 |  3967 | `	pNos->x.iVal = r;` |
|        3 |  3968 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  3969 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  3970 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  3971 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  3972 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  3973 | `	}` |
|        3 |  3974 | `	VmPopOperand(&pTos,1);` |
|        3 |  3975 | `	break;` |
|        - |  3976 | `				}` |
|        - |  3977 | `/*` |
|        - |  3978 | ` * OP_DIV * * *` |
|        - |  3979 | ` *` |
|        - |  3980 | ` * Pop the top two elements from the stack, divide the` |
|        - |  3981 | ` * first (what was next on the stack) from the second (the` |
|        - |  3982 | ` * top of the stack) and push the result onto the stack.` |
|        - |  3983 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  3984 | ` */` |
|       28 |  3985 | `case PH7_OP_DIV:{` |
|       58 |  3986 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  3987 | `	ph7_real a,b,r;` |
|        - |  3988 | `#ifdef UNTRUST` |
|        - |  3989 | `	if( pNos < pStack ){` |
|        - |  3990 | `		goto Abort;` |
|        - |  3991 | `	}` |
|        - |  3992 | `#endif` |
|        - |  3993 | `	/* Force the operands to be real */` |
|       58 |  3994 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       54 |  3995 | `		PH7_MemObjToReal(pTos);` |
|       26 |  3996 | `	}` |
|       58 |  3997 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       20 |  3998 | `		PH7_MemObjToReal(pNos);` |
|        9 |  3999 | `	}` |
|        - |  4000 | `	/* Perform the requested operation */` |
|       58 |  4001 | `	a = pNos->rVal;` |
|       58 |  4002 | `	b = pTos->rVal;` |
|       58 |  4003 | `	if( b == 0 ){` |
|        - |  4004 | `		/* Division by zero */` |
|        3 |  4005 | `		pNos->rVal = 0;` |
|        3 |  4006 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  4007 | `		/* goto Abort; */` |
|        2 |  4008 | `	}else{` |
|       55 |  4009 | `		r = a/b;` |
|        - |  4010 | `		/* Push the result */` |
|       55 |  4011 | `		pNos->rVal = r;` |
|       55 |  4012 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4013 | `		/* Try to get an integer representation */` |
|       55 |  4014 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4015 | `	}` |
|       58 |  4016 | `	VmPopOperand(&pTos,1);` |
|       58 |  4017 | `	break;` |
|        - |  4018 | `				}` |
|        - |  4019 | `/*` |
|        - |  4020 | ` * OP_DIV_STORE * * *` |
|        - |  4021 | ` *` |
|        - |  4022 | ` * Pop the top two elements from the stack, divide the` |
|        - |  4023 | ` * first (what was next on the stack) from the second (the` |
|        - |  4024 | ` * top of the stack) and push the result onto the stack.` |
|        - |  4025 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  4026 | ` */` |
|        1 |  4027 | `case PH7_OP_DIV_STORE:{` |
|        3 |  4028 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4029 | `	ph7_value *pObj;` |
|        - |  4030 | `	ph7_real a,b,r;` |
|        - |  4031 | `#ifdef UNTRUST` |
|        - |  4032 | `	if( pNos < pStack ){` |
|        - |  4033 | `		goto Abort;` |
|        - |  4034 | `	}` |
|        - |  4035 | `#endif` |
|        - |  4036 | `	/* Force the operands to be real */` |
|        3 |  4037 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4038 | `		PH7_MemObjToReal(pTos);` |
|        1 |  4039 | `	}` |
|        3 |  4040 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        3 |  4041 | `		PH7_MemObjToReal(pNos);` |
|        1 |  4042 | `	}` |
|        - |  4043 | `	/* Perform the requested operation */` |
|        3 |  4044 | `	a = pTos->rVal;` |
|        3 |  4045 | `	b = pNos->rVal;` |
|        3 |  4046 | `	if( b == 0 ){` |
|        - |  4047 | `		/* Division by zero */` |
|      ! 0 |  4048 | `		r = 0;` |
|      ! 0 |  4049 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  4050 | `		/* goto Abort; */` |
|      ! 0 |  4051 | `	}else{` |
|        3 |  4052 | `		r = a/b;` |
|        - |  4053 | `		/* Push the result */` |
|        3 |  4054 | `		pNos->rVal = r;` |
|        3 |  4055 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  4056 | `		/* Try to get an integer representation */` |
|        3 |  4057 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  4058 | `	}` |
|        3 |  4059 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4060 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  4061 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4062 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  4063 | `	}` |
|        3 |  4064 | `	VmPopOperand(&pTos,1);` |
|        3 |  4065 | `	break;` |
|        - |  4066 | `				}` |
|        - |  4067 | `/* OP_BAND * * *` |
|        - |  4068 | ` *` |
|        - |  4069 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4070 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4071 | ` * two elements.` |
|        - |  4072 | `*/` |
|        - |  4073 | `/* OP_BOR * * *` |
|        - |  4074 | ` *` |
|        - |  4075 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4076 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4077 | ` * two elements.` |
|        - |  4078 | ` */` |
|        - |  4079 | `/* OP_BXOR * * *` |
|        - |  4080 | ` *` |
|        - |  4081 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4082 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4083 | ` * two elements.` |
|        - |  4084 | ` */` |
|       30 |  4085 | `case PH7_OP_BAND:` |
|        - |  4086 | `case PH7_OP_BOR:` |
|        - |  4087 | `case PH7_OP_BXOR:{` |
|       62 |  4088 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4089 | `	sxi64 a,b,r;` |
|        - |  4090 | `#ifdef UNTRUST` |
|        - |  4091 | `	if( pNos < pStack ){` |
|        - |  4092 | `		goto Abort;` |
|        - |  4093 | `	}` |
|        - |  4094 | `#endif` |
|        - |  4095 | `	/* Force the operands to be integer */` |
|       62 |  4096 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4097 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4098 | `	}` |
|       62 |  4099 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4100 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4101 | `	}` |
|        - |  4102 | `	/* Perform the requested operation */` |
|       62 |  4103 | `	a = pNos->x.iVal;` |
|       62 |  4104 | `	b = pTos->x.iVal;` |
|       62 |  4105 | `	switch(pInstr->iOp){` |
|        6 |  4106 | `	case PH7_OP_BOR_STORE:` |
|       13 |  4107 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        6 |  4108 | `	case PH7_OP_BXOR_STORE:` |
|       13 |  4109 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       18 |  4110 | `	case PH7_OP_BAND_STORE:` |
|       18 |  4111 | `	case PH7_OP_BAND:` |
|       38 |  4112 | `	default:          r = a&b; break;` |
|        - |  4113 | `	}` |
|        - |  4114 | `	/* Push the result */` |
|       62 |  4115 | `	pNos->x.iVal = r;` |
|       62 |  4116 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       62 |  4117 | `	VmPopOperand(&pTos,1);` |
|       62 |  4118 | `	break;` |
|        - |  4119 | `				 }` |
|        - |  4120 | `/* OP_BAND_STORE * * *` |
|        - |  4121 | ` *` |
|        - |  4122 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4123 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  4124 | ` * two elements.` |
|        - |  4125 | `*/` |
|        - |  4126 | `/* OP_BOR_STORE * * *` |
|        - |  4127 | ` *` |
|        - |  4128 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4129 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  4130 | ` * two elements.` |
|        - |  4131 | ` */` |
|        - |  4132 | `/* OP_BXOR_STORE * * *` |
|        - |  4133 | ` *` |
|        - |  4134 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4135 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  4136 | ` * two elements.` |
|        - |  4137 | ` */` |
|        7 |  4138 | `case PH7_OP_BAND_STORE:` |
|        - |  4139 | `case PH7_OP_BOR_STORE:` |
|        - |  4140 | `case PH7_OP_BXOR_STORE:{` |
|       15 |  4141 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4142 | `	ph7_value *pObj;` |
|        - |  4143 | `	sxi64 a,b,r;` |
|        - |  4144 | `#ifdef UNTRUST` |
|        - |  4145 | `	if( pNos < pStack ){` |
|        - |  4146 | `		goto Abort;` |
|        - |  4147 | `	}` |
|        - |  4148 | `#endif` |
|        - |  4149 | `	/* Force the operands to be integer */` |
|       15 |  4150 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4151 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4152 | `	}` |
|       15 |  4153 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4154 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4155 | `	}` |
|        - |  4156 | `	/* Perform the requested operation */` |
|       15 |  4157 | `	a = pTos->x.iVal;` |
|       15 |  4158 | `	b = pNos->x.iVal;` |
|       15 |  4159 | `	switch(pInstr->iOp){` |
|        2 |  4160 | `	case PH7_OP_BOR_STORE:` |
|        5 |  4161 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        3 |  4162 | `	case PH7_OP_BXOR_STORE:` |
|        7 |  4163 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        2 |  4164 | `	case PH7_OP_BAND_STORE:` |
|        2 |  4165 | `	case PH7_OP_BAND:` |
|        5 |  4166 | `	default:          r = a&b; break;` |
|        - |  4167 | `	}` |
|        - |  4168 | `	/* Push the result */` |
|       15 |  4169 | `	pNos->x.iVal = r;` |
|       15 |  4170 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4171 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4172 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4173 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4174 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4175 | `	}` |
|       15 |  4176 | `	VmPopOperand(&pTos,1);` |
|       15 |  4177 | `	break;` |
|        - |  4178 | `				 }` |
|        - |  4179 | `/* OP_SHL * * *` |
|        - |  4180 | ` *` |
|        - |  4181 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4182 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4183 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4184 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4185 | ` */` |
|        - |  4186 | `/* OP_SHR * * *` |
|        - |  4187 | ` *` |
|        - |  4188 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4189 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4190 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4191 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4192 | ` */` |
|        9 |  4193 | `case PH7_OP_SHL:` |
|        - |  4194 | `case PH7_OP_SHR: {` |
|       19 |  4195 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4196 | `	sxi64 a,r;` |
|        - |  4197 | `	sxi32 b;` |
|        - |  4198 | `#ifdef UNTRUST` |
|        - |  4199 | `	if( pNos < pStack ){` |
|        - |  4200 | `		goto Abort;` |
|        - |  4201 | `	}` |
|        - |  4202 | `#endif` |
|        - |  4203 | `	/* Force the operands to be integer */` |
|       19 |  4204 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4205 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4206 | `	}` |
|       19 |  4207 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4208 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4209 | `	}` |
|        - |  4210 | `	/* Perform the requested operation */` |
|       19 |  4211 | `	a = pNos->x.iVal;` |
|       19 |  4212 | `	b = (sxi32)pTos->x.iVal;` |
|       19 |  4213 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       11 |  4214 | `		r = a << b;` |
|        6 |  4215 | `	}else{` |
|        9 |  4216 | `		r = a >> b;` |
|        - |  4217 | `	}` |
|        - |  4218 | `	/* Push the result */` |
|       19 |  4219 | `	pNos->x.iVal = r;` |
|       19 |  4220 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  4221 | `	VmPopOperand(&pTos,1);` |
|       19 |  4222 | `	break;` |
|        - |  4223 | `				 }` |
|        - |  4224 | `/*  OP_SHL_STORE * * *` |
|        - |  4225 | ` *` |
|        - |  4226 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4227 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4228 | ` * left by N bits where N is the top element on the stack.` |
|        - |  4229 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4230 | ` */` |
|        - |  4231 | `/* OP_SHR_STORE * * *` |
|        - |  4232 | ` *` |
|        - |  4233 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  4234 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  4235 | ` * right by N bits where N is the top element on the stack.` |
|        - |  4236 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  4237 | ` */` |
|        7 |  4238 | `case PH7_OP_SHL_STORE:` |
|        - |  4239 | `case PH7_OP_SHR_STORE: {` |
|       15 |  4240 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4241 | `	ph7_value *pObj;` |
|        - |  4242 | `	sxi64 a,r;` |
|        - |  4243 | `	sxi32 b;` |
|        - |  4244 | `#ifdef UNTRUST` |
|        - |  4245 | `	if( pNos < pStack ){` |
|        - |  4246 | `		goto Abort;` |
|        - |  4247 | `	}` |
|        - |  4248 | `#endif` |
|        - |  4249 | `	/* Force the operands to be integer */` |
|       15 |  4250 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4251 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  4252 | `	}` |
|       15 |  4253 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4254 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  4255 | `	}` |
|        - |  4256 | `	/* Perform the requested operation */` |
|       15 |  4257 | `	a = pTos->x.iVal;` |
|       15 |  4258 | `	b = (sxi32)pNos->x.iVal;` |
|       15 |  4259 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        7 |  4260 | `		r = a << b;` |
|        4 |  4261 | `	}else{` |
|        9 |  4262 | `		r = a >> b;` |
|        - |  4263 | `	}` |
|        - |  4264 | `	/* Push the result */` |
|       15 |  4265 | `	pNos->x.iVal = r;` |
|       15 |  4266 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       15 |  4267 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4268 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       15 |  4269 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       15 |  4270 | `		PH7_MemObjStore(pNos,pObj);` |
|        7 |  4271 | `	}` |
|       15 |  4272 | `	VmPopOperand(&pTos,1);` |
|       15 |  4273 | `	break;` |
|        - |  4274 | `				 }` |
|        - |  4275 | `/* CAT:  P1 * *` |
|        - |  4276 | ` *` |
|        - |  4277 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  4278 | ` * back.` |
|        - |  4279 | ` */` |
|    61552 |  4280 | `case PH7_OP_CAT:{` |
|        - |  4281 | `	ph7_value *pNos,*pCur;` |
|   123106 |  4282 | `	if( pInstr->iP1 < 1 ){` |
|    96148 |  4283 | `		pNos = &pTos[-1];` |
|    48075 |  4284 | `	}else{` |
|    26960 |  4285 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  4286 | `	}` |
|        - |  4287 | `#ifdef UNTRUST` |
|        - |  4288 | `	if( pNos < pStack ){` |
|        - |  4289 | `		goto Abort;` |
|        - |  4290 | `	}` |
|        - |  4291 | `#endif` |
|        - |  4292 | `	/* Force a string cast */` |
|   123106 |  4293 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1018 |  4294 | `		PH7_MemObjToString(pNos);` |
|      508 |  4295 | `	}` |
|   123106 |  4296 | `	pCur = &pNos[1];` |
|   248174 |  4297 | `	while( pCur <= pTos ){` |
|   125070 |  4298 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50520 |  4299 | `			PH7_MemObjToString(pCur);` |
|    25259 |  4300 | `		}` |
|        - |  4301 | `		/* Perform the concatenation */` |
|   125070 |  4302 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   125032 |  4303 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    62515 |  4304 | `		}` |
|   125070 |  4305 | `		SyBlobRelease(&pCur->sBlob);` |
|   125070 |  4306 | `		pCur++;` |
|        2 |  4307 | `	}` |
|   123106 |  4308 | `	pTos = pNos;` |
|   123106 |  4309 | `	break;` |
|        - |  4310 | `				}` |
|        - |  4311 | `/*  CAT_STORE: * * *` |
|        - |  4312 | ` *` |
|        - |  4313 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  4314 | ` * back.` |
|        - |  4315 | ` */` |
|     3357 |  4316 | `case PH7_OP_CAT_STORE:{` |
|     6716 |  4317 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4318 | `	ph7_value *pObj;` |
|        - |  4319 | `#ifdef UNTRUST` |
|        - |  4320 | `	if( pNos < pStack ){` |
|        - |  4321 | `		goto Abort;` |
|        - |  4322 | `	}` |
|        - |  4323 | `#endif` |
|     6716 |  4324 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4325 | `		/* Force a string cast */` |
|      ! 0 |  4326 | `		PH7_MemObjToString(pTos);` |
|      ! 0 |  4327 | `	}` |
|     6716 |  4328 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4329 | `		/* Force a string cast */` |
|      ! 0 |  4330 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4331 | `	}` |
|        - |  4332 | `	/* Perform the concatenation (Reverse order) */` |
|     6716 |  4333 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     6716 |  4334 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3357 |  4335 | `	}` |
|        - |  4336 | `	/* Perform the store operation */` |
|     6716 |  4337 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  4338 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     6716 |  4339 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     6716 |  4340 | `		PH7_MemObjStore(pTos,pObj);` |
|     3357 |  4341 | `	}` |
|     6716 |  4342 | `	PH7_MemObjStore(pTos,pNos);` |
|     6716 |  4343 | `	VmPopOperand(&pTos,1);` |
|     6716 |  4344 | `	break;` |
|        - |  4345 | `				}` |
|        - |  4346 | `/* OP_AND: * * *` |
|        - |  4347 | ` *` |
|        - |  4348 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  4349 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4350 | ` * stack.` |
|        - |  4351 | ` */` |
|        - |  4352 | `/* OP_OR: * * *` |
|        - |  4353 | ` *` |
|        - |  4354 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  4355 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4356 | ` * stack.` |
|        - |  4357 | ` */` |
|    93989 |  4358 | `case PH7_OP_LAND:` |
|        - |  4359 | `case PH7_OP_LOR: {` |
|   188024 |  4360 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4361 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  4362 | `#ifdef UNTRUST` |
|        - |  4363 | `	if( pNos < pStack ){` |
|        - |  4364 | `		goto Abort;` |
|        - |  4365 | `	}` |
|        - |  4366 | `#endif` |
|        - |  4367 | `	/* Force a boolean cast */` |
|   188024 |  4368 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  4369 | `		PH7_MemObjToBool(pTos);` |
|        1 |  4370 | `	}` |
|   188024 |  4371 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4372 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4373 | `	}` |
|   188024 |  4374 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   188024 |  4375 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   188024 |  4376 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  4377 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    85706 |  4378 | `		v1 = and_logic[v1*3+v2];` |
|    42876 |  4379 | `	}else{` |
|        - |  4380 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   102320 |  4381 | `		v1 = or_logic[v1*3+v2];` |
|        - |  4382 | `	}` |
|   188024 |  4383 | `	if( v1 == 2 ){` |
|      ! 0 |  4384 | `		v1 = 1;` |
|      ! 0 |  4385 | `	}` |
|   188024 |  4386 | `	VmPopOperand(&pTos,1);` |
|   188024 |  4387 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   188024 |  4388 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   188024 |  4389 | `	break;` |
|        - |  4390 | `				 }` |
|        - |  4391 | `/* OP_LXOR: * * *` |
|        - |  4392 | ` *` |
|        - |  4393 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  4394 | ` * two values and push the resulting boolean value back onto the` |
|        - |  4395 | ` * stack.` |
|        - |  4396 | ` * According to the PHP language reference manual:` |
|        - |  4397 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  4398 | ` *  TRUE,but not both.` |
|        - |  4399 | ` */` |
|        5 |  4400 | `case PH7_OP_LXOR:{` |
|       11 |  4401 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  4402 | `	sxi32 v = 0;` |
|        - |  4403 | `#ifdef UNTRUST` |
|        - |  4404 | `	if( pNos < pStack ){` |
|        - |  4405 | `		goto Abort;` |
|        - |  4406 | `	}` |
|        - |  4407 | `#endif` |
|        - |  4408 | `	/* Force a boolean cast */` |
|       11 |  4409 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4410 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4411 | `	}` |
|       11 |  4412 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4413 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  4414 | `	}` |
|       11 |  4415 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  4416 | `		v = 1;` |
|        3 |  4417 | `	}` |
|       11 |  4418 | `	VmPopOperand(&pTos,1);` |
|       11 |  4419 | `	pTos->x.iVal = v;` |
|       11 |  4420 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  4421 | `	break;` |
|        - |  4422 | `				 }` |
|        - |  4423 | `/* OP_EQ P1 P2 P3` |
|        - |  4424 | ` *` |
|        - |  4425 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  4426 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4427 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4428 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4429 | ` */` |
|        - |  4430 | `/* OP_NEQ P1 P2 P3` |
|        - |  4431 | ` *` |
|        - |  4432 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  4433 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4434 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4435 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4436 | ` */` |
|     3819 |  4437 | `case PH7_OP_EQ:` |
|        - |  4438 | `case PH7_OP_NEQ: {` |
|     7640 |  4439 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4440 | `	/* Perform the comparison and act accordingly */` |
|        - |  4441 | `#ifdef UNTRUST` |
|        - |  4442 | `	if( pNos < pStack ){` |
|        - |  4443 | `		goto Abort;` |
|        - |  4444 | `	}` |
|        - |  4445 | `#endif` |
|     7640 |  4446 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     7640 |  4447 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       20 |  4448 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     7631 |  4449 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     7596 |  4450 | `		rc = rc == 0;` |
|     3799 |  4451 | `	}else{` |
|       28 |  4452 | `		rc = rc != 0;` |
|        - |  4453 | `	}` |
|     7640 |  4454 | `	VmPopOperand(&pTos,1);` |
|     7640 |  4455 | `	if( !pInstr->iP2 ){` |
|        - |  4456 | `		/* Push comparison result without taking the jump */` |
|     7640 |  4457 | `		PH7_MemObjRelease(pTos);` |
|     7640 |  4458 | `		pTos->x.iVal = rc;` |
|        - |  4459 | `		/* Invalidate any prior representation */` |
|     7640 |  4460 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     3821 |  4461 | `	}else{` |
|      ! 0 |  4462 | `		if( rc ){` |
|        - |  4463 | `			/* Jump to the desired location */` |
|      ! 0 |  4464 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4465 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4466 | `		}` |
|        - |  4467 | `	}` |
|     7640 |  4468 | `	break;` |
|        - |  4469 | `				 }` |
|        - |  4470 | `/* OP_TEQ P1 P2 *` |
|        - |  4471 | ` *` |
|        - |  4472 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  4473 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  4474 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4475 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4476 | ` */` |
|   129112 |  4477 | `case PH7_OP_TEQ: {` |
|   258226 |  4478 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4479 | `	/* Perform the comparison and act accordingly */` |
|        - |  4480 | `#ifdef UNTRUST` |
|        - |  4481 | `	if( pNos < pStack ){` |
|        - |  4482 | `		goto Abort;` |
|        - |  4483 | `	}` |
|        - |  4484 | `#endif` |
|   258226 |  4485 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   258226 |  4486 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4487 | `		rc = 0;` |
|        2 |  4488 | `	}else{` |
|   258224 |  4489 | `		rc = rc == 0;` |
|        - |  4490 | `	}` |
|   258226 |  4491 | `	VmPopOperand(&pTos,1);` |
|   258226 |  4492 | `	if( !pInstr->iP2 ){` |
|        - |  4493 | `		/* Push comparison result without taking the jump */` |
|   258226 |  4494 | `		PH7_MemObjRelease(pTos);` |
|   258226 |  4495 | `		pTos->x.iVal = rc;` |
|        - |  4496 | `		/* Invalidate any prior representation */` |
|   258226 |  4497 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   129114 |  4498 | `	}else{` |
|      ! 0 |  4499 | `		if( rc ){` |
|        - |  4500 | `			/* Jump to the desired location */` |
|      ! 0 |  4501 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4502 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4503 | `		}` |
|        - |  4504 | `	}` |
|   258226 |  4505 | `	break;` |
|        - |  4506 | `				 }` |
|        - |  4507 | `/* OP_TNE P1 P2 *` |
|        - |  4508 | ` *` |
|        - |  4509 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  4510 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  4511 | ` * instruction.` |
|        - |  4512 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4513 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4514 | ` *` |
|        - |  4515 | ` */` |
|   100788 |  4516 | `case PH7_OP_TNE: {` |
|   201578 |  4517 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4518 | `	/* Perform the comparison and act accordingly */` |
|        - |  4519 | `#ifdef UNTRUST` |
|        - |  4520 | `	if( pNos < pStack ){` |
|        - |  4521 | `		goto Abort;` |
|        - |  4522 | `	}` |
|        - |  4523 | `#endif` |
|   201578 |  4524 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   201578 |  4525 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  4526 | `		rc = 1;` |
|        2 |  4527 | `	}else{` |
|   201576 |  4528 | `		rc = rc != 0;` |
|        - |  4529 | `	}` |
|   201578 |  4530 | `	VmPopOperand(&pTos,1);` |
|   201578 |  4531 | `	if( !pInstr->iP2 ){` |
|        - |  4532 | `		/* Push comparison result without taking the jump */` |
|   201578 |  4533 | `		PH7_MemObjRelease(pTos);` |
|   201578 |  4534 | `		pTos->x.iVal = rc;` |
|        - |  4535 | `		/* Invalidate any prior representation */` |
|   201578 |  4536 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   100790 |  4537 | `	}else{` |
|      ! 0 |  4538 | `		if( rc ){` |
|        - |  4539 | `			/* Jump to the desired location */` |
|      ! 0 |  4540 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4541 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4542 | `		}` |
|        - |  4543 | `	}` |
|   201578 |  4544 | `	break;` |
|        - |  4545 | `				 }` |
|        - |  4546 | `/* OP_LT P1 P2 P3` |
|        - |  4547 | ` *` |
|        - |  4548 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4549 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4550 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4551 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4552 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4553 | ` *` |
|        - |  4554 | ` */` |
|        - |  4555 | `/* OP_LE P1 P2 P3` |
|        - |  4556 | ` *` |
|        - |  4557 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4558 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4559 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4560 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4561 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4562 | ` *` |
|        - |  4563 | ` */` |
|   102469 |  4564 | `case PH7_OP_LT:` |
|        - |  4565 | `case PH7_OP_LE: {` |
|   204984 |  4566 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4567 | `	/* Perform the comparison and act accordingly */` |
|        - |  4568 | `#ifdef UNTRUST` |
|        - |  4569 | `	if( pNos < pStack ){` |
|        - |  4570 | `		goto Abort;` |
|        - |  4571 | `	}` |
|        - |  4572 | `#endif` |
|   204984 |  4573 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   204984 |  4574 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4575 | `		rc = 0;` |
|   204980 |  4576 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|      408 |  4577 | `		rc = rc < 1;` |
|      205 |  4578 | `	}else{` |
|   204570 |  4579 | `		rc = rc < 0;` |
|        - |  4580 | `	}` |
|   204984 |  4581 | `	VmPopOperand(&pTos,1);` |
|   204984 |  4582 | `	if( !pInstr->iP2 ){` |
|        - |  4583 | `		/* Push comparison result without taking the jump */` |
|   204984 |  4584 | `		PH7_MemObjRelease(pTos);` |
|   204984 |  4585 | `		pTos->x.iVal = rc;` |
|        - |  4586 | `		/* Invalidate any prior representation */` |
|   204984 |  4587 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   102515 |  4588 | `	}else{` |
|      ! 0 |  4589 | `		if( rc ){` |
|        - |  4590 | `			/* Jump to the desired location */` |
|      ! 0 |  4591 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4592 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4593 | `		}` |
|        - |  4594 | `	}` |
|   204984 |  4595 | `	break;` |
|        - |  4596 | `				}` |
|        - |  4597 | `/* OP_GT P1 P2 P3` |
|        - |  4598 | ` *` |
|        - |  4599 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4600 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  4601 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4602 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4603 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4604 | ` *` |
|        - |  4605 | ` */` |
|        - |  4606 | `/* OP_GE P1 P2 P3` |
|        - |  4607 | ` *` |
|        - |  4608 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  4609 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  4610 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  4611 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4612 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4613 | ` *` |
|        - |  4614 | ` */` |
|    48811 |  4615 | `case PH7_OP_GT:` |
|        - |  4616 | `case PH7_OP_GE: {` |
|    97624 |  4617 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4618 | `	/* Perform the comparison and act accordingly */` |
|        - |  4619 | `#ifdef UNTRUST` |
|        - |  4620 | `	if( pNos < pStack ){` |
|        - |  4621 | `		goto Abort;` |
|        - |  4622 | `	}` |
|        - |  4623 | `#endif` |
|    97624 |  4624 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    97624 |  4625 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  4626 | `		rc = 0;` |
|    97620 |  4627 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|    97468 |  4628 | `		rc = rc >= 0;` |
|    48735 |  4629 | `	}else{` |
|      150 |  4630 | `		rc = rc > 0;` |
|        - |  4631 | `	}` |
|    97624 |  4632 | `	VmPopOperand(&pTos,1);` |
|    97624 |  4633 | `	if( !pInstr->iP2 ){` |
|        - |  4634 | `		/* Push comparison result without taking the jump */` |
|    97624 |  4635 | `		PH7_MemObjRelease(pTos);` |
|    97624 |  4636 | `		pTos->x.iVal = rc;` |
|        - |  4637 | `		/* Invalidate any prior representation */` |
|    97624 |  4638 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    48813 |  4639 | `	}else{` |
|      ! 0 |  4640 | `		if( rc ){` |
|        - |  4641 | `			/* Jump to the desired location */` |
|      ! 0 |  4642 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4643 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4644 | `		}` |
|        - |  4645 | `	}` |
|    97624 |  4646 | `	break;` |
|        - |  4647 | `				}` |
|        - |  4648 | `/* OP_SEQ P1 P2 *` |
|        - |  4649 | ` * Strict string comparison.` |
|        - |  4650 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  4651 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4652 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4653 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4654 | ` * use PH7_OP_EQ.` |
|        - |  4655 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4656 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4657 | ` */` |
|        - |  4658 | `/* OP_SNE P1 P2 *` |
|        - |  4659 | ` * Strict string comparison.` |
|        - |  4660 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  4661 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  4662 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  4663 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  4664 | ` * use PH7_OP_EQ.` |
|        - |  4665 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  4666 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  4667 | ` */` |
|       18 |  4668 | `case PH7_OP_SEQ:` |
|        - |  4669 | `case PH7_OP_SNE: {` |
|       38 |  4670 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  4671 | `	SyString s1,s2;` |
|        - |  4672 | `	/* Perform the comparison and act accordingly */` |
|        - |  4673 | `#ifdef UNTRUST` |
|        - |  4674 | `	if( pNos < pStack ){` |
|        - |  4675 | `		goto Abort;` |
|        - |  4676 | `	}` |
|        - |  4677 | `#endif` |
|        - |  4678 | `	/* Force a string cast */` |
|       38 |  4679 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  4680 | `		PH7_MemObjToString(pTos);` |
|        2 |  4681 | `	}` |
|       38 |  4682 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4683 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  4684 | `	}` |
|       38 |  4685 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  4686 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  4687 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  4688 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  4689 | `		rc = rc != 0;` |
|      ! 0 |  4690 | `	}else{` |
|       38 |  4691 | `		rc = rc == 0;` |
|        - |  4692 | `	}` |
|       38 |  4693 | `	VmPopOperand(&pTos,1);` |
|       38 |  4694 | `	if( !pInstr->iP2 ){` |
|        - |  4695 | `		/* Push comparison result without taking the jump */` |
|       38 |  4696 | `		PH7_MemObjRelease(pTos);` |
|       38 |  4697 | `		pTos->x.iVal = rc;` |
|        - |  4698 | `		/* Invalidate any prior representation */` |
|       38 |  4699 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  4700 | `	}else{` |
|      ! 0 |  4701 | `		if( rc ){` |
|        - |  4702 | `			/* Jump to the desired location */` |
|      ! 0 |  4703 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4704 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4705 | `		}` |
|        - |  4706 | `	}` |
|       38 |  4707 | `	break;` |
|        - |  4708 | `				 }` |
|        - |  4709 | `/*` |
|        - |  4710 | ` * OP_LOAD_REF * * *` |
|        - |  4711 | ` * Push the index of a referenced object on the stack.` |
|        - |  4712 | ` */` |
|       57 |  4713 | `case PH7_OP_LOAD_REF: {` |
|        - |  4714 | `	sxu32 nIdx;` |
|        - |  4715 | `#ifdef UNTRUST` |
|        - |  4716 | `	if( pTos < pStack ){` |
|        - |  4717 | `		goto Abort;` |
|        - |  4718 | `	}` |
|        - |  4719 | `#endif` |
|        - |  4720 | `	/* Extract memory object index */` |
|      115 |  4721 | `	nIdx = pTos->nIdx;` |
|      115 |  4722 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  4723 | `		/* Nullify the object */` |
|       95 |  4724 | `		PH7_MemObjRelease(pTos);` |
|        - |  4725 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  4726 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  4727 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  4728 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  4729 | `	}` |
|      115 |  4730 | `	break;` |
|        - |  4731 | `					  }` |
|        - |  4732 | `/*` |
|        - |  4733 | ` * OP_STORE_REF * * P3` |
|        - |  4734 | ` * Perform an assignment operation by reference.` |
|        - |  4735 | ` */` |
|       14 |  4736 | ` case PH7_OP_STORE_REF: {` |
|       30 |  4737 | `	 SyString sName = { 0 , 0 };` |
|        - |  4738 | `	 VmFrame *pFrameLocal;` |
|        - |  4739 | `	SyHashEntry *pEntry;` |
|        - |  4740 | `	sxu32 nIdx;` |
|        - |  4741 | `#ifdef UNTRUST` |
|        - |  4742 | `	if( pTos < pStack ){` |
|        - |  4743 | `		goto Abort;` |
|        - |  4744 | `	}` |
|        - |  4745 | `#endif` |
|       30 |  4746 | `	if( pInstr->p3 == 0 ){` |
|        - |  4747 | `		char *zName;` |
|        - |  4748 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  4749 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4750 | `			/* Force a string cast */` |
|      ! 0 |  4751 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4752 | `		}` |
|      ! 0 |  4753 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4754 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  4755 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4756 | `			if( zName ){` |
|      ! 0 |  4757 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4758 | `			}` |
|      ! 0 |  4759 | `		}` |
|      ! 0 |  4760 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  4761 | `		pTos--;` |
|      ! 0 |  4762 | `	}else{` |
|       30 |  4763 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4764 | `	}` |
|       30 |  4765 | `	nIdx = pTos->nIdx;` |
|       30 |  4766 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  4767 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  4768 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4769 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  4770 | `		}else{` |
|        - |  4771 | `			ph7_value *pObj;` |
|        - |  4772 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  4773 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  4774 | `			if( pObj == 0 ){` |
|      ! 0 |  4775 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4776 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4777 | `				goto Abort;` |
|        - |  4778 | `			}` |
|        - |  4779 | `			/* Perform the store operation */` |
|      ! 0 |  4780 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  4781 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  4782 | `		}` |
|       30 |  4783 | `	}else if( sName.nByte > 0){` |
|       30 |  4784 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  4785 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  4786 | `		}else{` |
|       30 |  4787 | `			pFrameLocal = pVm->pFrame;` |
|       30 |  4788 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4789 | `			/* Query the local frame */` |
|       30 |  4790 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       30 |  4791 | `			if( pEntry ){` |
|      ! 0 |  4792 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  4793 | `			}else{` |
|       30 |  4794 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       30 |  4795 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  4796 | `					/* Insert in the $GLOBALS array */` |
|       26 |  4797 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       12 |  4798 | `				}` |
|       30 |  4799 | `				if( rc == SXRET_OK ){` |
|       30 |  4800 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       14 |  4801 | `				}` |
|        - |  4802 | `			}` |
|        - |  4803 | `		}` |
|       14 |  4804 | `	}` |
|       30 |  4805 | `	break;` |
|        - |  4806 | `				 }` |
|        - |  4807 | `/*` |
|        - |  4808 | ` * OP_UPLINK P1 * *` |
|        - |  4809 | ` * Link a variable to the top active VM frame.` |
|        - |  4810 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  4811 | ` */` |
|       25 |  4812 | `case PH7_OP_UPLINK: {` |
|       52 |  4813 | `	if( pVm->pFrame->pParent ){` |
|       52 |  4814 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  4815 | `		SyString sName;` |
|        - |  4816 | `		/* Perform the link */` |
|      104 |  4817 | `		while( pLink <= pTos ){` |
|       54 |  4818 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4819 | `				/* Force a string cast */` |
|      ! 0 |  4820 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  4821 | `			}` |
|       54 |  4822 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       54 |  4823 | `			if( sName.nByte > 0 ){` |
|       54 |  4824 | `				VmFrameLink(&(*pVm),&sName);` |
|       26 |  4825 | `			}` |
|       54 |  4826 | `			pLink++;` |
|        2 |  4827 | `		}` |
|       25 |  4828 | `	}` |
|       52 |  4829 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       52 |  4830 | `	break;` |
|        - |  4831 | `					}` |
|        - |  4832 | `/*` |
|        - |  4833 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  4834 | ` * Push an exception in the corresponding container so that` |
|        - |  4835 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  4836 | ` */` |
|       29 |  4837 | `case PH7_OP_LOAD_EXCEPTION: {` |
|       60 |  4838 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  4839 | `	VmFrame *pFrameLocal;` |
|        - |  4840 | `	/* Reset per-entry state so finally runs on each iteration */` |
|       60 |  4841 | `	pException->iFinallyDone = 0;` |
|       60 |  4842 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  4843 | `	/* Create the exception frame */` |
|       60 |  4844 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|       60 |  4845 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  4846 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  4847 | `		goto Abort;` |
|        - |  4848 | `	}` |
|        - |  4849 | `	/* Mark the special frame */` |
|       60 |  4850 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|       60 |  4851 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  4852 | `	/* Point to the frame that trigger the exception */` |
|       60 |  4853 | `	pFrameLocal = pFrameLocal->pParent;` |
|       60 |  4854 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       60 |  4855 | `	pException->pFrame = pFrameLocal;` |
|       60 |  4856 | `	break;` |
|        - |  4857 | `							}` |
|        - |  4858 | `/*` |
|        - |  4859 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  4860 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  4861 | ` */` |
|       28 |  4862 | `case PH7_OP_POP_EXCEPTION: {` |
|       58 |  4863 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|       58 |  4864 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  4865 | `		ph7_exception **apException;` |
|        - |  4866 | `		/* Pop the loaded exception */` |
|       28 |  4867 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       28 |  4868 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       26 |  4869 | `			(void)SySetPop(&pVm->aException);` |
|       12 |  4870 | `		}` |
|       13 |  4871 | `	}` |
|       58 |  4872 | `	pException->pFrame = 0;` |
|        - |  4873 | `	/* Leave the exception frame */` |
|       58 |  4874 | `	VmLeaveFrame(&(*pVm));` |
|        - |  4875 | `	/* Execute the finally block if present and not already executed by catch path */` |
|       58 |  4876 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  4877 | `		sxi32 rcFinally;` |
|       19 |  4878 | `		pException->iFinallyDone = 1;` |
|       19 |  4879 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       19 |  4880 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  4881 | `			goto Abort;` |
|        - |  4882 | `		}` |
|        9 |  4883 | `	}` |
|       58 |  4884 | `	break;` |
|        - |  4885 | `							}` |
|        - |  4886 |  |
|        - |  4887 | `/*` |
|        - |  4888 | ` * OP_THROW * P2 *` |
|        - |  4889 | ` * Throw an user exception.` |
|        - |  4890 | ` */` |
|       17 |  4891 | `case PH7_OP_THROW: {` |
|       36 |  4892 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  4893 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  4894 | `#ifdef UNTRUST` |
|        - |  4895 | `	if( pTos < pStack ){` |
|        - |  4896 | `		goto Abort;` |
|        - |  4897 | `	}` |
|        - |  4898 | `#endif` |
|       36 |  4899 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  4900 | `	/* Tell the upper layer that an exception was thrown */` |
|       36 |  4901 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|       36 |  4902 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       36 |  4903 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  4904 | `		ph7_class *pException;` |
|        - |  4905 | `		/* Make sure the loaded object is an instance of the 'Exception' base class.` |
|        - |  4906 | `		 */` |
|       36 |  4907 | `		pException = PH7_VmExtractClass(&(*pVm),"Exception",sizeof("Exception")-1,TRUE,0);` |
|       36 |  4908 | `		if( pException == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pException) ){` |
|        - |  4909 | `			/* Exceptions must be valid objects derived from the Exception base class */` |
|      ! 0 |  4910 | `			rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  4911 | `			if( rc == SXERR_ABORT ){` |
|        - |  4912 | `				/* Abort processing immediately */` |
|      ! 0 |  4913 | `				goto Abort;` |
|        - |  4914 | `			}` |
|      ! 0 |  4915 | `		}else{` |
|        - |  4916 | `			/* Throw the exception */` |
|       36 |  4917 | `			rc = VmThrowException(&(*pVm),pThis);` |
|       36 |  4918 | `			if( rc == SXERR_ABORT ){` |
|        - |  4919 | `				/* Abort processing immediately */` |
|        9 |  4920 | `				goto Abort;` |
|        - |  4921 | `			}` |
|        - |  4922 | `		}` |
|       15 |  4923 | `	}else{` |
|        - |  4924 | `		/* Expecting a class instance */` |
|      ! 0 |  4925 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  4926 | `		if( rc == SXERR_ABORT ){` |
|        - |  4927 | `			/* Abort processing immediately */` |
|      ! 0 |  4928 | `			goto Abort;` |
|        - |  4929 | `		}` |
|        - |  4930 | `	}` |
|        - |  4931 | `	/* Pop the top entry */` |
|       28 |  4932 | `	VmPopOperand(&pTos,1);` |
|        - |  4933 | `	/* Perform an unconditional jump */` |
|       28 |  4934 | `	pc = nJump - 1;` |
|       28 |  4935 | `	break;` |
|        - |  4936 | `				   }` |
|        - |  4937 | `/*` |
|        - |  4938 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  4939 | ` * Prepare a foreach step.` |
|        - |  4940 | ` */` |
|     4805 |  4941 | `case PH7_OP_FOREACH_INIT: {` |
|     9612 |  4942 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  4943 | `	void *pName;` |
|        - |  4944 | `#ifdef UNTRUST` |
|        - |  4945 | `	if( pTos < pStack ){` |
|        - |  4946 | `		goto Abort;` |
|        - |  4947 | `	}` |
|        - |  4948 | `#endif` |
|     9612 |  4949 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4950 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  4951 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4952 | `			/* Force a string cast */` |
|      ! 0 |  4953 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4954 | `		}` |
|        - |  4955 | `		/* Duplicate name */` |
|      ! 0 |  4956 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4957 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4958 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4959 | `		}` |
|      ! 0 |  4960 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4961 | `	}` |
|     9612 |  4962 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  4963 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4964 | `			/* Force a string cast */` |
|      ! 0 |  4965 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4966 | `		}` |
|        - |  4967 | `		/* Duplicate name */` |
|      ! 0 |  4968 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  4969 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4970 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  4971 | `		}` |
|      ! 0 |  4972 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4973 | `	}` |
|        - |  4974 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|     9612 |  4975 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  4976 | `		/* Jump out of the loop */` |
|      ! 0 |  4977 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  4978 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  4979 | `		}` |
|      ! 0 |  4980 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  4981 | `	}else{` |
|        - |  4982 | `		ph7_foreach_step *pStep;` |
|     9612 |  4983 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|     9612 |  4984 | `		if( pStep == 0 ){` |
|      ! 0 |  4985 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  4986 | `			/* Jump out of the loop */` |
|      ! 0 |  4987 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  4988 | `		}else{` |
|        - |  4989 | `			/* Zero the structure */` |
|     9612 |  4990 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  4991 | `			/* Prepare the step */` |
|     9612 |  4992 | `			pStep->iFlags = pInfo->iFlags;` |
|     9612 |  4993 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|     9596 |  4994 | `				ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4995 | `				/* Reset the internal loop cursor */` |
|     9596 |  4996 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  4997 | `				/* Mark the step */` |
|     9596 |  4998 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|     9596 |  4999 | `				pStep->xIter.pMap = pMap;` |
|     9596 |  5000 | `				pMap->iRef++;` |
|     4799 |  5001 | `			}else{` |
|       18 |  5002 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5003 | `				ph7_class *pIteratorClass;` |
|        - |  5004 | `				/* Check if the object implements Iterator */` |
|       18 |  5005 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       21 |  5006 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  5007 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  5008 | `					ph7_class_method *pRewind;` |
|        7 |  5009 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        7 |  5010 | `					pStep->xIter.pThis = pThis;` |
|        7 |  5011 | `					pThis->iRef++;` |
|        7 |  5012 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|        7 |  5013 | `					if( pRewind ){` |
|        7 |  5014 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|        3 |  5015 | `					}` |
|        4 |  5016 | `				}else{` |
|        - |  5017 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  5018 | `					ph7_class *pIterAggClass;` |
|       12 |  5019 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  5020 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  5021 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  5022 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  5023 | `						ph7_class_method *pGetIter;` |
|        3 |  5024 | `						int iterAggOk = 0;` |
|        3 |  5025 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  5026 | `						if( pGetIter ){` |
|        - |  5027 | `							ph7_value sResult;` |
|        3 |  5028 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  5029 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  5030 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  5031 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  5032 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  5033 | `									ph7_class_method *pRewind;` |
|        3 |  5034 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  5035 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  5036 | `									pIterObj->iRef++;` |
|        - |  5037 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  5038 | `									pStep->pOwner = pThis;` |
|        3 |  5039 | `									pThis->iRef++;` |
|        3 |  5040 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  5041 | `									if( pRewind ){` |
|        3 |  5042 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  5043 | `									}` |
|        3 |  5044 | `									iterAggOk = 1;` |
|        1 |  5045 | `								}` |
|        1 |  5046 | `							}` |
|        3 |  5047 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  5048 | `						}` |
|        3 |  5049 | `						if( !iterAggOk ){` |
|        - |  5050 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  5051 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5052 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  5053 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  5054 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  5055 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  5056 | `						}` |
|        2 |  5057 | `					}else{` |
|        - |  5058 | `						/* Plain object iteration via hAttr */` |
|        9 |  5059 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  5060 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  5061 | `						pStep->xIter.pThis = pThis;` |
|        9 |  5062 | `						pThis->iRef++;` |
|        - |  5063 | `					}` |
|        - |  5064 | `				}` |
|        - |  5065 | `			}` |
|        - |  5066 | `		}` |
|     9612 |  5067 | `		if( pStep ){` |
|     9612 |  5068 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  5069 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  5070 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  5071 | `				/* Jump out of the loop */` |
|      ! 0 |  5072 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  5073 | `			}` |
|     4805 |  5074 | `		}` |
|        - |  5075 | `	}` |
|     9612 |  5076 | `	VmPopOperand(&pTos,1);` |
|     9612 |  5077 | `	break;` |
|        - |  5078 | `						  }` |
|        - |  5079 | `/*` |
|        - |  5080 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  5081 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  5082 | ` */` |
|    77130 |  5083 | `case PH7_OP_FOREACH_STEP: {` |
|   154262 |  5084 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  5085 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  5086 | `	ph7_value *pValue;` |
|        - |  5087 | `	VmFrame *pFrameLocal;` |
|        - |  5088 | `	/* Peek the last step */` |
|   154262 |  5089 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   154262 |  5090 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   154262 |  5091 | `	pFrameLocal = pVm->pFrame;` |
|   154262 |  5092 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   154262 |  5093 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   154202 |  5094 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  5095 | `		ph7_hashmap_node *pNode;` |
|        - |  5096 | `		/* Extract the current node value */` |
|   154202 |  5097 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   154202 |  5098 | `		if( pNode == 0 ){` |
|        - |  5099 | `			/* No more entry to process */` |
|     9594 |  5100 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|     9594 |  5101 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5102 | `				/* Break the reference with the last element */` |
|        5 |  5103 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        2 |  5104 | `			}` |
|        - |  5105 | `			/* Automatically reset the loop cursor */` |
|     9594 |  5106 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  5107 | `			/* Cleanup the mess left behind */` |
|     9594 |  5108 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     9594 |  5109 | `			SySetPop(&pInfo->aStep);` |
|     9594 |  5110 | `			PH7_HashmapUnref(pMap);` |
|     4798 |  5111 | `		}else{` |
|   144610 |  5112 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      416 |  5113 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      416 |  5114 | `				if( pKey ){` |
|      416 |  5115 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      207 |  5116 | `				}` |
|      207 |  5117 | `			}` |
|   144610 |  5118 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5119 | `				SyHashEntry *pEntry;` |
|        - |  5120 | `				/* Pass by reference */` |
|       18 |  5121 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       18 |  5122 | `				if( pEntry ){` |
|       16 |  5123 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|        9 |  5124 | `				}else{` |
|        4 |  5125 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  5126 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  5127 | `				}` |
|       10 |  5128 | `			}else{` |
|        - |  5129 | `				/* Make a copy of the entry value */` |
|   144594 |  5130 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   144594 |  5131 | `				if( pValue ){` |
|   144594 |  5132 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    72296 |  5133 | `				}` |
|        - |  5134 | `			}` |
|        2 |  5135 | `		}` |
|    77162 |  5136 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  5137 | `		/* Iterator-based iteration.` |
|        - |  5138 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  5139 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  5140 | `		 */` |
|       37 |  5141 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  5142 | `		ph7_class_method *pMethod;` |
|        - |  5143 | `		ph7_value sResult;` |
|       37 |  5144 | `		int isValid = 0;` |
|        - |  5145 | `		/* Call next() to advance — but skip on the first iteration */` |
|       37 |  5146 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|        9 |  5147 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|        5 |  5148 | `		}else{` |
|       29 |  5149 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       29 |  5150 | `			if( pMethod ){` |
|       29 |  5151 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       14 |  5152 | `			}` |
|        - |  5153 | `		}` |
|        - |  5154 | `		/* Call valid() */` |
|       37 |  5155 | `		PH7_MemObjInit(pVm,&sResult);` |
|       37 |  5156 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|       37 |  5157 | `		if( pMethod ){` |
|       37 |  5158 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       37 |  5159 | `			PH7_MemObjToBool(&sResult);` |
|       37 |  5160 | `			isValid = (sResult.x.iVal != 0);` |
|       18 |  5161 | `		}` |
|       37 |  5162 | `		PH7_MemObjRelease(&sResult);` |
|       37 |  5163 | `		if( !isValid ){` |
|        - |  5164 | `			/* Iterator exhausted */` |
|        7 |  5165 | `			pc = pInstr->iP2 - 1;` |
|        - |  5166 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|        7 |  5167 | `			if( pStep->pOwner ){` |
|        3 |  5168 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  5169 | `			}` |
|        7 |  5170 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        7 |  5171 | `			SySetPop(&pInfo->aStep);` |
|        7 |  5172 | `			PH7_ClassInstanceUnref(pThis);` |
|        4 |  5173 | `		}else{` |
|        - |  5174 | `			/* Call current() to get value */` |
|       31 |  5175 | `			PH7_MemObjInit(pVm,&sResult);` |
|       31 |  5176 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       31 |  5177 | `			if( pMethod ){` |
|       31 |  5178 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       15 |  5179 | `			}` |
|       31 |  5180 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       31 |  5181 | `			if( pValue ){` |
|       31 |  5182 | `				PH7_MemObjStore(&sResult,pValue);` |
|       15 |  5183 | `			}` |
|       31 |  5184 | `			PH7_MemObjRelease(&sResult);` |
|        - |  5185 | `			/* Call key() if needed */` |
|       31 |  5186 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  5187 | `				ph7_value sKey;` |
|       23 |  5188 | `				PH7_MemObjInit(pVm,&sKey);` |
|       23 |  5189 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       23 |  5190 | `				if( pMethod ){` |
|       23 |  5191 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       11 |  5192 | `				}` |
|       23 |  5193 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       23 |  5194 | `				if( pValue ){` |
|       23 |  5195 | `					PH7_MemObjStore(&sKey,pValue);` |
|       11 |  5196 | `				}` |
|       23 |  5197 | `				PH7_MemObjRelease(&sKey);` |
|       11 |  5198 | `			}` |
|        - |  5199 | `		}` |
|       19 |  5200 | `	}else{` |
|       25 |  5201 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  5202 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  5203 | `		SyHashEntry *pEntry;` |
|        - |  5204 | `		/* Point to the next attribute */` |
|       29 |  5205 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  5206 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  5207 | `			/* Check access permission */` |
|       31 |  5208 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  5209 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  5210 | `					break; /* Access is granted */` |
|        - |  5211 | `			}` |
|        1 |  5212 | `		}` |
|       25 |  5213 | `		if( pEntry == 0 ){` |
|        - |  5214 | `			/* Clean up the mess left behind */` |
|        9 |  5215 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  5216 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5217 | `				/* Break the reference with the last element */` |
|        3 |  5218 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  5219 | `			}` |
|        9 |  5220 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  5221 | `			SySetPop(&pInfo->aStep);` |
|        9 |  5222 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  5223 | `		}else{` |
|       17 |  5224 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  5225 | `			ph7_value *pAttrValue;` |
|       17 |  5226 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  5227 | `				/* Fill with the current attribute name */` |
|       17 |  5228 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  5229 | `				if( pKey ){` |
|       17 |  5230 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  5231 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  5232 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  5233 | `				}` |
|        8 |  5234 | `			}` |
|        - |  5235 | `			/* Extract attribute value */` |
|       17 |  5236 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  5237 | `			if( pAttrValue ){` |
|       17 |  5238 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  5239 | `					/* Pass by reference */` |
|        3 |  5240 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  5241 | `					if( pEntry ){` |
|        3 |  5242 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  5243 | `					}else{` |
|      ! 0 |  5244 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  5245 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  5246 | `					}` |
|        2 |  5247 | `				}else{` |
|        - |  5248 | `					/* Make a copy of the attribute value */` |
|       15 |  5249 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  5250 | `					if( pValue ){` |
|       15 |  5251 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  5252 | `					}` |
|        - |  5253 | `				}` |
|        8 |  5254 | `			}` |
|        - |  5255 | `		}` |
|        - |  5256 | `	}` |
|   154262 |  5257 | `	break;` |
|        - |  5258 | `						  }` |
|        - |  5259 | `/*` |
|        - |  5260 | ` * OP_MEMBER P1 P2` |
|        - |  5261 | ` * Load class attribute/method on the stack.` |
|        - |  5262 | ` */` |
|     2076 |  5263 | `case PH7_OP_MEMBER: {` |
|        - |  5264 | `	ph7_class_instance *pThis;` |
|        - |  5265 | `	ph7_value *pNos;` |
|        - |  5266 | `	SyString sName;` |
|     4154 |  5267 | `	if( !pInstr->iP1 ){` |
|     4056 |  5268 | `		pNos = &pTos[-1];` |
|        - |  5269 | `#ifdef UNTRUST` |
|        - |  5270 | `		if( pNos < pStack ){` |
|        - |  5271 | `			goto Abort;` |
|        - |  5272 | `		}` |
|        - |  5273 | `#endif` |
|     4056 |  5274 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5275 | `			ph7_class *pClass;` |
|        - |  5276 | `			/* Class already instantiated */` |
|     4056 |  5277 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  5278 | `			/* Point to the instantiated class */` |
|     4056 |  5279 | `			pClass = pThis->pClass;` |
|        - |  5280 | `			/* Extract attribute name first */` |
|     4056 |  5281 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     4056 |  5282 | `			if( pInstr->iP2 ){` |
|        - |  5283 | `				/* Method call */` |
|      278 |  5284 | `				ph7_class_method *pMeth = 0;` |
|      278 |  5285 | `				if( sName.nByte > 0 ){` |
|        - |  5286 | `					/* Extract the target method */` |
|      278 |  5287 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      138 |  5288 | `				}` |
|      278 |  5289 | `				if( pMeth == 0 ){` |
|      ! 0 |  5290 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  5291 | `						&pClass->sName,&sName` |
|        - |  5292 | `						);` |
|        - |  5293 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  5294 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  5295 | `					/* Pop the method name from the stack */` |
|      ! 0 |  5296 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5297 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  5298 | `				}else{` |
|        - |  5299 | `					/* Push method name on the stack */` |
|      278 |  5300 | `					PH7_MemObjRelease(pTos);` |
|      278 |  5301 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      278 |  5302 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5303 | `				}` |
|      278 |  5304 | `				pTos->nIdx = SXU32_HIGH;` |
|      140 |  5305 | `			}else{` |
|        - |  5306 | `				/* Attribute access */` |
|     3780 |  5307 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  5308 | `				SyHashEntry *pEntry;` |
|        - |  5309 | `				/* Extract the target attribute */` |
|     3780 |  5310 | `				if( sName.nByte > 0 ){` |
|     3780 |  5311 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     3780 |  5312 | `					if( pEntry ){` |
|        - |  5313 | `						/* Point to the attribute value */` |
|     3778 |  5314 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     1888 |  5315 | `					}` |
|     1889 |  5316 | `				}` |
|     3780 |  5317 | `				if( pObjAttr == 0 ){` |
|        - |  5318 | `					/* No such attribute,load null */` |
|        4 |  5319 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  5320 | `						&pClass->sName,&sName);` |
|        - |  5321 | `					/* Call the __get magic method if available */` |
|        3 |  5322 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  5323 | `				}` |
|     3780 |  5324 | `				VmPopOperand(&pTos,1);` |
|        - |  5325 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  5326 | `				 * This is due to the following case:` |
|        - |  5327 | `				 *     (new TestClass())->foo;` |
|        - |  5328 | `				 */` |
|     3780 |  5329 | `				pThis->iRef++;` |
|     3780 |  5330 | `				PH7_MemObjRelease(pTos);` |
|     3780 |  5331 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     3780 |  5332 | `				if( pObjAttr ){` |
|     3778 |  5333 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  5334 | `					/* Check attribute access */` |
|     3778 |  5335 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,TRUE) ){` |
|        - |  5336 | `						/* Load attribute */` |
|     3778 |  5337 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3778 |  5338 | `						if( pValue ){` |
|     3778 |  5339 | `							if( pThis->iRef < 2 ){` |
|        - |  5340 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  5341 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  5342 | `								 */` |
|        3 |  5343 | `								PH7_MemObjStore(pValue,pTos);` |
|        2 |  5344 | `							}else{` |
|        - |  5345 | `								/* Simple load */` |
|     3776 |  5346 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  5347 | `							}` |
|     3778 |  5348 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     3776 |  5349 | `								if( pThis->iRef > 1 ){` |
|        - |  5350 | `									/* Load attribute index */` |
|     3774 |  5351 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     1886 |  5352 | `								}` |
|     1887 |  5353 | `							}` |
|     1888 |  5354 | `						}` |
|     1888 |  5355 | `					}` |
|     1888 |  5356 | `				}` |
|        - |  5357 | `				/* Safely unreference the object */` |
|     3780 |  5358 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  5359 | `			}` |
|     2029 |  5360 | `		}else{` |
|      ! 0 |  5361 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|      ! 0 |  5362 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5363 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5364 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  5365 | `		}` |
|     2029 |  5366 | `	}else{` |
|        - |  5367 | `		/* Static member access using class name */` |
|      100 |  5368 | `		pNos = pTos;` |
|      100 |  5369 | `		pThis = 0;` |
|      100 |  5370 | `		if( !pInstr->p3 ){` |
|       88 |  5371 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       88 |  5372 | `			pNos--;` |
|        - |  5373 | `#ifdef UNTRUST` |
|        - |  5374 | `			if( pNos < pStack ){` |
|        - |  5375 | `				goto Abort;` |
|        - |  5376 | `			}` |
|        - |  5377 | `#endif` |
|       45 |  5378 | `		}else{` |
|        - |  5379 | `			/* Attribute name already computed */` |
|       14 |  5380 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5381 | `		}` |
|      100 |  5382 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      100 |  5383 | `			ph7_class *pClass = 0;` |
|      100 |  5384 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5385 | `				/* Class already instantiated */` |
|      ! 0 |  5386 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      ! 0 |  5387 | `				pClass = pThis->pClass;` |
|      ! 0 |  5388 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      ! 0 |  5389 | `			}else{` |
|        - |  5390 | `				/* Try to extract the target class */` |
|      100 |  5391 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      100 |  5392 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      100 |  5393 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  5394 | `					/* Handle self/static/parent keywords */` |
|      100 |  5395 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       30 |  5396 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       30 |  5397 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  5398 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  5399 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  5400 | `						}` |
|       86 |  5401 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       16 |  5402 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       71 |  5403 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       14 |  5404 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       14 |  5405 | `						if( pSelf && pSelf->pBase ){` |
|       14 |  5406 | `							pClass = pSelf->pBase;` |
|        6 |  5407 | `						}` |
|        8 |  5408 | `					}else{` |
|       46 |  5409 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  5410 | `					}` |
|       49 |  5411 | `				}` |
|        - |  5412 | `			}` |
|      100 |  5413 | `			if( pClass == 0 ){` |
|        - |  5414 | `				/* Undefined class */` |
|      ! 0 |  5415 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  5416 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  5417 | `					);` |
|      ! 0 |  5418 | `				if( !pInstr->p3 ){` |
|      ! 0 |  5419 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  5420 | `				}` |
|      ! 0 |  5421 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  5422 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  5423 | `			}else{` |
|      100 |  5424 | `				if( pInstr->iP2 ){` |
|        - |  5425 | `					/* Method call */` |
|       30 |  5426 | `					ph7_class_method *pMeth = 0;` |
|       30 |  5427 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  5428 | `						/* Extract the target method */` |
|       30 |  5429 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       14 |  5430 | `					}` |
|       30 |  5431 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  5432 | `						if( pMeth ){` |
|      ! 0 |  5433 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  5434 | `								&pClass->sName,&sName` |
|        - |  5435 | `								);` |
|      ! 0 |  5436 | `						}else{` |
|      ! 0 |  5437 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5438 | `								&pClass->sName,&sName` |
|        - |  5439 | `								);` |
|        - |  5440 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  5441 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  5442 | `						}` |
|        - |  5443 | `						/* Pop the method name from the stack */` |
|      ! 0 |  5444 | `						if( !pInstr->p3 ){` |
|      ! 0 |  5445 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  5446 | `						}` |
|      ! 0 |  5447 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  5448 | `					}else{` |
|        - |  5449 | `						/* Push method name on the stack */` |
|       30 |  5450 | `						PH7_MemObjRelease(pTos);` |
|       30 |  5451 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       30 |  5452 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  5453 | `					}` |
|       30 |  5454 | `					pTos->nIdx = SXU32_HIGH;` |
|       16 |  5455 | `				}else{` |
|        - |  5456 | `					/* Attribute access */` |
|       72 |  5457 | `					ph7_class_attr *pAttr = 0;` |
|        - |  5458 | `					/* Check for special ::class pseudo-constant */` |
|      104 |  5459 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       64 |  5460 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  5461 | `						/* ::class returns the fully qualified class name */` |
|        - |  5462 | `						/* Pop the attribute name from the stack */` |
|       54 |  5463 | `						if( !pInstr->p3 ){` |
|       54 |  5464 | `							VmPopOperand(&pTos,1);` |
|       26 |  5465 | `						}` |
|       54 |  5466 | `						PH7_MemObjRelease(pTos);` |
|        - |  5467 | `						/* Load the class name */` |
|       54 |  5468 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       54 |  5469 | `						pTos->nIdx = SXU32_HIGH;` |
|       28 |  5470 | `					}else{` |
|        - |  5471 | `						/* Extract the target attribute */` |
|       20 |  5472 | `						if( sName.nByte > 0 ){` |
|       20 |  5473 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|        9 |  5474 | `						}` |
|       20 |  5475 | `						if( pAttr == 0 ){` |
|        - |  5476 | `							/* No such attribute,load null */` |
|      ! 0 |  5477 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5478 | `								&pClass->sName,&sName);` |
|        - |  5479 | `							/* Call the __get magic method if available */` |
|      ! 0 |  5480 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  5481 | `						}` |
|        - |  5482 | `						/* Pop the attribute name from the stack */` |
|       20 |  5483 | `						if( !pInstr->p3 ){` |
|        7 |  5484 | `							VmPopOperand(&pTos,1);` |
|        3 |  5485 | `						}` |
|       20 |  5486 | `						PH7_MemObjRelease(pTos);` |
|       20 |  5487 | `						pTos->nIdx = SXU32_HIGH;` |
|       20 |  5488 | `						if( pAttr ){` |
|       20 |  5489 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  5490 | `								/* Access to a non static attribute */` |
|      ! 0 |  5491 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  5492 | `									&pClass->sName,&pAttr->sName` |
|        - |  5493 | `									);` |
|      ! 0 |  5494 | `							}else{` |
|        - |  5495 | `								ph7_value *pValue;` |
|        - |  5496 | `								/* Check if the access to the attribute is allowed */` |
|       20 |  5497 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,TRUE) ){` |
|        - |  5498 | `									/* Load the desired attribute */` |
|       20 |  5499 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       20 |  5500 | `									if( pValue ){` |
|       20 |  5501 | `										PH7_MemObjLoad(pValue,pTos);` |
|       20 |  5502 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  5503 | `											/* Load index number */` |
|       14 |  5504 | `											pTos->nIdx = pAttr->nIdx;` |
|        6 |  5505 | `										}` |
|        9 |  5506 | `									}` |
|        9 |  5507 | `								}` |
|        - |  5508 | `							}` |
|        9 |  5509 | `						}` |
|        - |  5510 | `					}` |
|        - |  5511 | `				}` |
|      100 |  5512 | `				if( pThis ){` |
|        - |  5513 | `					/* Safely unreference the object */` |
|      ! 0 |  5514 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  5515 | `				}` |
|        - |  5516 | `			}` |
|       51 |  5517 | `		}else{` |
|        - |  5518 | `			/* Pop operands */` |
|      ! 0 |  5519 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  5520 | `			if( !pInstr->p3 ){` |
|      ! 0 |  5521 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  5522 | `			}` |
|      ! 0 |  5523 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5524 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  5525 | `		}` |
|        - |  5526 | `	}` |
|     4154 |  5527 | `	break;` |
|        - |  5528 | `					}` |
|        - |  5529 | `/*` |
|        - |  5530 | ` * OP_NEW P1 * * *` |
|        - |  5531 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  5532 | ` */` |
|      304 |  5533 | `case PH7_OP_NEW: {` |
|      610 |  5534 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|      610 |  5535 | `	ph7_class *pClass = 0;` |
|        - |  5536 | `	ph7_class_instance *pNew;` |
|      610 |  5537 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  5538 | `		/* Try to extract the desired class */` |
|      914 |  5539 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      608 |  5540 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      304 |  5541 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  5542 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  5543 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  5544 | `	}` |
|      610 |  5545 | `	if( pClass == 0 ){` |
|        - |  5546 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  5547 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  5548 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  5549 | `			);` |
|        - |  5550 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  5551 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  5552 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5553 | `			/* Pop given arguments */` |
|      ! 0 |  5554 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5555 | `		}` |
|      ! 0 |  5556 | `		goto Abort;` |
|      ! 0 |  5557 | `	}else{` |
|        - |  5558 | `		ph7_class_method *pCons;` |
|        - |  5559 | `		/* Create a new class instance */` |
|      610 |  5560 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|      610 |  5561 | `		if( pNew == 0 ){` |
|      ! 0 |  5562 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5563 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  5564 | `				&pClass->sName` |
|        - |  5565 | `			);` |
|      ! 0 |  5566 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5567 | `			if( pInstr->iP1 > 0 ){` |
|        - |  5568 | `				/* Pop given arguments */` |
|      ! 0 |  5569 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5570 | `			}` |
|      ! 0 |  5571 | `			break;` |
|        - |  5572 | `		}` |
|        - |  5573 | `		/* Check if a constructor is available */` |
|      610 |  5574 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      610 |  5575 | `		if( pCons == 0 ){` |
|      526 |  5576 | `			SyString *pName = &pClass->sName;` |
|        - |  5577 | `			/* Check for a constructor with the same base class name */` |
|      526 |  5578 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      262 |  5579 | `		}` |
|      610 |  5580 | `		if( pCons ){` |
|        - |  5581 | `			/* Call the class constructor */` |
|       86 |  5582 | `			SySetReset(&aArg);` |
|      160 |  5583 | `			while( pArg < pTos ){` |
|       76 |  5584 | `				SySetPut(&aArg,(const void *)&pArg);` |
|       76 |  5585 | `				pArg++;` |
|        2 |  5586 | `			}` |
|       86 |  5587 | `			if( pVm->bErrReport ){` |
|        - |  5588 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  5589 | `				sxu32 n;` |
|       43 |  5590 | `				n = SySetUsed(&aArg);` |
|        - |  5591 | `				/* Emit a notice for missing arguments */` |
|       95 |  5592 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       53 |  5593 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       53 |  5594 | `					if( pFuncArg ){` |
|       53 |  5595 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  5596 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  5597 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  5598 | `						}` |
|       26 |  5599 | `					}` |
|       53 |  5600 | `					n++;` |
|        1 |  5601 | `				}` |
|       21 |  5602 | `			}` |
|       86 |  5603 | `			PH7_VmCallClassMethod(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  5604 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|       86 |  5605 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  5606 | `				pNew->iRef = 1;` |
|      ! 0 |  5607 | `			}` |
|       42 |  5608 | `		}` |
|      610 |  5609 | `		if( pInstr->iP1 > 0 ){` |
|        - |  5610 | `			/* Pop given arguments */` |
|       68 |  5611 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|       33 |  5612 | `		}` |
|      610 |  5613 | `		PH7_MemObjRelease(pTos);` |
|      610 |  5614 | `		pTos->x.pOther = pNew;` |
|      610 |  5615 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5616 | `	}` |
|      610 |  5617 | `	break;` |
|        - |  5618 | `				 }` |
|        - |  5619 | `/*` |
|        - |  5620 | ` * OP_CLONE * * *` |
|        - |  5621 | ` * Perfome a clone operation.` |
|        - |  5622 | ` */` |
|       23 |  5623 | `case PH7_OP_CLONE: {` |
|        - |  5624 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  5625 | `#ifdef UNTRUST` |
|        - |  5626 | `	if( pTos < pStack ){` |
|        - |  5627 | `		goto Abort;` |
|        - |  5628 | `	}` |
|        - |  5629 | `#endif` |
|        - |  5630 | `	/* Make sure we are dealing with a class instance */` |
|       48 |  5631 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  5632 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5633 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  5634 | `		PH7_MemObjRelease(pTos);` |
|        5 |  5635 | `		break;` |
|        - |  5636 | `	}` |
|        - |  5637 | `	/* Point to the source */` |
|       44 |  5638 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5639 | `	/* Perform the clone operation */` |
|       44 |  5640 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       44 |  5641 | `	PH7_MemObjRelease(pTos);` |
|       44 |  5642 | `	if( pClone == 0 ){` |
|      ! 0 |  5643 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5644 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  5645 | `	}else{` |
|        - |  5646 | `		/* Load the cloned object */` |
|       44 |  5647 | `		pTos->x.pOther = pClone;` |
|       44 |  5648 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  5649 | `	}` |
|       44 |  5650 | `	break;` |
|        - |  5651 | `				   }` |
|        - |  5652 | `/*` |
|        - |  5653 | ` * OP_SWITCH * * P3` |
|        - |  5654 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  5655 | ` */` |
|       18 |  5656 | `case PH7_OP_SWITCH: {` |
|       38 |  5657 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  5658 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  5659 | `	ph7_value sValue,sCaseValue;` |
|        - |  5660 | `	sxu32 n,nEntry;` |
|        - |  5661 | `#ifdef UNTRUST` |
|        - |  5662 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  5663 | `		goto Abort;` |
|        - |  5664 | `	}` |
|        - |  5665 | `#endif` |
|        - |  5666 | `	/* Point to the case table  */` |
|       38 |  5667 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       38 |  5668 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  5669 | `	/* Select the appropriate case block to execute */` |
|       38 |  5670 | `	PH7_MemObjInit(pVm,&sValue);` |
|       38 |  5671 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|       92 |  5672 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       92 |  5673 | `		pCase = &aCase[n];` |
|       92 |  5674 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  5675 | `		/* Execute the case expression first */` |
|       92 |  5676 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  5677 | `		/* Compare the two expression */` |
|       92 |  5678 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|       92 |  5679 | `		PH7_MemObjRelease(&sValue);` |
|       92 |  5680 | `		PH7_MemObjRelease(&sCaseValue);` |
|       92 |  5681 | `		if( rc == 0 ){` |
|        - |  5682 | `			/* Value match,jump to this block */` |
|       38 |  5683 | `			pc = pCase->nStart - 1;` |
|       38 |  5684 | `			break;` |
|        - |  5685 | `		}` |
|       29 |  5686 | `	}` |
|       38 |  5687 | `	VmPopOperand(&pTos,1);` |
|       38 |  5688 | `	if( n >= nEntry ){` |
|        - |  5689 | `		/* No approprite case to execute,jump to the default case */` |
|      ! 0 |  5690 | `		if( pSwitch->nDefault > 0 ){` |
|      ! 0 |  5691 | `			pc = pSwitch->nDefault - 1;` |
|      ! 0 |  5692 | `		}else{` |
|        - |  5693 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  5694 | `			pc = pSwitch->nOut - 1;` |
|        - |  5695 | `		}` |
|      ! 0 |  5696 | `	}` |
|       38 |  5697 | `	break;` |
|        - |  5698 | `					}` |
|        - |  5699 | `/*` |
|        - |  5700 | ` * OP_CALL P1 * *` |
|        - |  5701 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  5702 | ` *  function on the stack.` |
|        - |  5703 | ` */` |
|   280665 |  5704 | `case PH7_OP_CALL: {` |
|   561376 |  5705 | `	ph7_value *pArg = &pTos[-pInstr->iP1];` |
|        - |  5706 | `	SyHashEntry *pEntry;` |
|        - |  5707 | `	SyString sName;` |
|        - |  5708 | `	/* Extract function name */` |
|   561376 |  5709 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        3 |  5710 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5711 | `			ph7_value sResult;` |
|      ! 0 |  5712 | `			SySetReset(&aArg);` |
|      ! 0 |  5713 | `			while( pArg < pTos ){` |
|      ! 0 |  5714 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  5715 | `				pArg++;` |
|      ! 0 |  5716 | `			}` |
|      ! 0 |  5717 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  5718 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  5719 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  5720 | `			SySetReset(&aArg);` |
|        - |  5721 | `			/* Pop given arguments */` |
|      ! 0 |  5722 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5723 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5724 | `			}` |
|        - |  5725 | `			/* Copy result */` |
|      ! 0 |  5726 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  5727 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  5728 | `		}else{` |
|        3 |  5729 | `			if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        3 |  5730 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  5731 | `				/* Call the magic method '__invoke' if available */` |
|        3 |  5732 | `				PH7_ClassInstanceCallMagicMethod(&(*pVm),pThis->pClass,pThis,"__invoke",sizeof("__invoke")-1,0);` |
|        2 |  5733 | `			}else{` |
|        - |  5734 | `				/* Raise exception: Invalid function name */` |
|      ! 0 |  5735 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  5736 | `			}` |
|        - |  5737 | `			/* Pop given arguments */` |
|        3 |  5738 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5739 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5740 | `			}` |
|        - |  5741 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5742 | `			PH7_MemObjRelease(pTos);` |
|        - |  5743 | `		}` |
|   280432 |  5744 | `		break;` |
|        - |  5745 | `	}` |
|   561374 |  5746 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  5747 | `	/* Check for a compiled function first.` |
|        - |  5748 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  5749 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   561374 |  5750 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  5751 | `	/* If the compiler qualified this call with a namespace (pInstr->p3 != 0)` |
|        - |  5752 | `	 * and the namespaced function is not found, retry with the global name` |
|        - |  5753 | `	 * (strip the namespace prefix up to the last backslash) before falling` |
|        - |  5754 | `	 * back to host functions. This mirrors PHP's lookup order for unqualified` |
|        - |  5755 | `	 * function calls inside namespaces. */` |
|   561374 |  5756 | `	if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  5757 | `		const char *zFunc;` |
|        - |  5758 | `		const char *zEnd;` |
|        - |  5759 | `		const char *z;` |
|        - |  5760 | `		SyString sGlobal;` |
|       15 |  5761 | `		zFunc = sName.zString;` |
|       15 |  5762 | `		zEnd  = zFunc + sName.nByte;` |
|       15 |  5763 | `		z = zEnd;` |
|        - |  5764 | `		/* Find last namespace separator */` |
|      133 |  5765 | `		while( z > zFunc ){` |
|      133 |  5766 | `			if( z[-1] == '\\' ){` |
|       15 |  5767 | `				break;` |
|        - |  5768 | `			}` |
|      119 |  5769 | `			z--;` |
|        1 |  5770 | `		}` |
|       15 |  5771 | `		if( z > zFunc && z < zEnd ){` |
|        - |  5772 | `			/* Retry lookup using the unqualified/global function name */` |
|       15 |  5773 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       15 |  5774 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|        7 |  5775 | `		}` |
|        7 |  5776 | `	}` |
|   561374 |  5777 | `	if( pEntry ){` |
|        - |  5778 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  5779 | `		ph7_class_instance *pThis;` |
|        - |  5780 | `		ph7_value *pFrameStack;` |
|        - |  5781 | `		ph7_vm_func *pVmFunc;` |
|        - |  5782 | `		ph7_class *pSelf;` |
|        - |  5783 | `		VmFrame *pFrame;` |
|        - |  5784 | `		ph7_value *pObj;` |
|        - |  5785 | `		VmSlot sArg;` |
|        - |  5786 | `		sxu32 n;` |
|        - |  5787 | `		/* initialize fields */` |
|    12476 |  5788 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    12476 |  5789 | `		pThis = 0;` |
|    12476 |  5790 | `		pSelf = 0;` |
|    12476 |  5791 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  5792 | `			ph7_class_method *pMeth;` |
|        - |  5793 | `			/* Class method call */` |
|     1604 |  5794 | `			ph7_value *pTarget = &pTos[-1];` |
|     1604 |  5795 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  5796 | `				/* Extract the 'this' pointer */` |
|     1604 |  5797 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  5798 | `					/* Instance already loaded */` |
|     1570 |  5799 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     1570 |  5800 | `					pThis->iRef++;` |
|     1570 |  5801 | `					pSelf = pThis->pClass;` |
|      784 |  5802 | `				}` |
|     1604 |  5803 | `				if( pSelf == 0 ){` |
|       36 |  5804 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  5805 | `						/* "Late Static Binding" class name */` |
|       44 |  5806 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       14 |  5807 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       14 |  5808 | `					}` |
|       36 |  5809 | `					if( pSelf == 0 ){` |
|       13 |  5810 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|        6 |  5811 | `					}` |
|       17 |  5812 | `				}` |
|     1604 |  5813 | `				if( pThis == 0  ){` |
|       36 |  5814 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       36 |  5815 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       36 |  5816 | `					if( pFrameLocal->pParent ){` |
|        - |  5817 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       19 |  5818 | `						pThis = pFrameLocal->pThis;` |
|       19 |  5819 | `						if( pThis ){` |
|       13 |  5820 | `							pThis->iRef++;` |
|        6 |  5821 | `						}` |
|        9 |  5822 | `					}` |
|       17 |  5823 | `				}` |
|     1604 |  5824 | `				VmPopOperand(&pTos,1);` |
|     1604 |  5825 | `				PH7_MemObjRelease(pTos);` |
|        - |  5826 | `				/* Synchronize pointers */` |
|     1604 |  5827 | `				pArg = &pTos[-pInstr->iP1];` |
|        - |  5828 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  5829 | `				 * user have already computed the random generated unique class method name` |
|        - |  5830 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  5831 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  5832 | `				 */` |
|     1604 |  5833 | `				while( pArg < pStack ){` |
|      ! 0 |  5834 | `					pArg++;` |
|      ! 0 |  5835 | `				}` |
|     1604 |  5836 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  5837 | `					/* Check if the call is allowed */` |
|     1604 |  5838 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     1604 |  5839 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|        8 |  5840 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,TRUE) ){` |
|        - |  5841 | `							/* Pop given arguments */` |
|      ! 0 |  5842 | `							if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5843 | `								VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5844 | `							}` |
|        - |  5845 | `							/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5846 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  5847 | `							break;` |
|        - |  5848 | `						}` |
|        3 |  5849 | `					}` |
|      801 |  5850 | `				}` |
|      801 |  5851 | `			}` |
|      801 |  5852 | `		}` |
|        - |  5853 | `		/* Check The recursion limit */` |
|    12476 |  5854 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  5855 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5856 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  5857 | `				&pVmFunc->sName);` |
|        - |  5858 | `			/* Pop given arguments */` |
|        3 |  5859 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5860 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5861 | `			}` |
|        - |  5862 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  5863 | `			PH7_MemObjRelease(pTos);` |
|        3 |  5864 | `			break;` |
|        - |  5865 | `		}` |
|    12474 |  5866 | `		if( pVmFunc->pNextName ){` |
|        - |  5867 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      134 |  5868 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       66 |  5869 | `		}` |
|        - |  5870 | `		/* Extract the formal argument set */` |
|    12474 |  5871 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  5872 | `		/* Create a new VM frame  */` |
|    12474 |  5873 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    12474 |  5874 | `		if( rc != SXRET_OK ){` |
|        - |  5875 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  5876 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5877 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  5878 | `				&pVmFunc->sName);` |
|        - |  5879 | `			/* Pop given arguments */` |
|      ! 0 |  5880 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  5881 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  5882 | `			}` |
|        - |  5883 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  5884 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  5885 | `			break;` |
|        - |  5886 | `		}` |
|    12474 |  5887 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  5888 | `			/* Install the '$this' variable */` |
|        - |  5889 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     1580 |  5890 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     1580 |  5891 | `			if( pObj ){` |
|        - |  5892 | `				/* Reflect the change */` |
|     1580 |  5893 | `				pObj->x.pOther = pThis;` |
|     1580 |  5894 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      789 |  5895 | `			}` |
|      789 |  5896 | `		}` |
|    12474 |  5897 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  5898 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  5899 | `			/* Install static variables */` |
|      ! 0 |  5900 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  5901 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  5902 | `				pStatic = &aStatic[n];` |
|      ! 0 |  5903 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  5904 | `					/* Initialize the static variables */` |
|      ! 0 |  5905 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  5906 | `					if( pObj ){` |
|        - |  5907 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  5908 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  5909 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  5910 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  5911 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  5912 | `						}` |
|      ! 0 |  5913 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  5914 | `					}else{` |
|      ! 0 |  5915 | `						continue;` |
|        - |  5916 | `					}` |
|      ! 0 |  5917 | `				}` |
|        - |  5918 | `				/* Install in the current frame */` |
|      ! 0 |  5919 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  5920 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  5921 | `			}` |
|      ! 0 |  5922 | `		}` |
|        - |  5923 | `		/* Push arguments in the local frame */` |
|    12474 |  5924 | `		n = 0;` |
|    34414 |  5925 | `		while( pArg < pTos ){` |
|    21942 |  5926 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    21792 |  5927 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  5928 | `					/* NULL values are redirected to default arguments */` |
|      ! 0 |  5929 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  5930 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  5931 | `						goto Abort;` |
|        - |  5932 | `					}` |
|      ! 0 |  5933 | `				}` |
|        - |  5934 | `				/* Make sure the given arguments are of the correct type */` |
|    21792 |  5935 | `				if( aFormalArg[n].nType > 0 ){` |
|     1088 |  5936 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  5937 | `						/* Argument must be a class instance [i.e: object] */` |
|      ! 0 |  5938 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  5939 | `						ph7_class *pClass;` |
|        - |  5940 | `						/* Try to extract the desired class */` |
|      ! 0 |  5941 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  5942 | `						if( pClass ){` |
|      ! 0 |  5943 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  5944 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  5945 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5946 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5947 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5948 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5949 | `								}` |
|      ! 0 |  5950 | `							}else{` |
|        - |  5951 | `								/* reuse pThis declared in outer scope */` |
|      ! 0 |  5952 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  5953 | `								/* Make sure the object is an instance of the given class */` |
|      ! 0 |  5954 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  5955 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5956 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  5957 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  5958 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  5959 | `								}` |
|        - |  5960 | `							}` |
|      ! 0 |  5961 | `						}` |
|     1088 |  5962 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  5963 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  5964 | `						/* Cast to the desired type */` |
|      ! 0 |  5965 | `						xCast(pArg);` |
|      ! 0 |  5966 | `					}` |
|      543 |  5967 | `				}` |
|    21792 |  5968 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5969 | `					/* Pass by reference */` |
|       48 |  5970 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  5971 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  5972 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  5973 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  5974 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  5975 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  5976 | `						}` |
|        - |  5977 | `						/* Switch to pass by value */` |
|      ! 0 |  5978 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  5979 | `					}else{` |
|        - |  5980 | `						SyHashEntry *pRefEntry;` |
|        - |  5981 | `						/* Install the referenced variable in the private function frame */` |
|       48 |  5982 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       48 |  5983 | `						if( pRefEntry == 0 ){` |
|       71 |  5984 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       46 |  5985 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       48 |  5986 | `							sArg.nIdx = pArg->nIdx;` |
|       48 |  5987 | `							sArg.pUserData = 0;` |
|       48 |  5988 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  5989 | `						}` |
|       48 |  5990 | `						pObj = 0;` |
|        - |  5991 | `					}` |
|       25 |  5992 | `				}else{` |
|        - |  5993 | `					/* Pass by value,make a copy of the given argument */` |
|    21746 |  5994 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  5995 | `				}` |
|    10897 |  5996 | `			}else{` |
|        - |  5997 | `				char zName[32];` |
|        - |  5998 | `				SyString sArgName;` |
|        - |  5999 | `				/* Set a dummy name */` |
|      152 |  6000 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      152 |  6001 | `				sArgName.zString = zName;` |
|        - |  6002 | `				/* Annonymous argument */` |
|      152 |  6003 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  6004 | `			}` |
|    21942 |  6005 | `			if( pObj ){` |
|    21896 |  6006 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  6007 | `				/* Insert argument index  */` |
|    21896 |  6008 | `				sArg.nIdx = pObj->nIdx;` |
|    21896 |  6009 | `				sArg.pUserData = 0;` |
|    21896 |  6010 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    10947 |  6011 | `			}` |
|    21942 |  6012 | `			PH7_MemObjRelease(pArg);` |
|    21942 |  6013 | `			pArg++;` |
|    21942 |  6014 | `			++n;` |
|        2 |  6015 | `		}` |
|        - |  6016 | `		/* Set up closure environment */` |
|    12474 |  6017 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  6018 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  6019 | `			ph7_value *pValue;` |
|        - |  6020 | `			sxu32 iEnv;` |
|        9 |  6021 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|       25 |  6022 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|       17 |  6023 | `				pEnv = &aEnv[iEnv];` |
|       17 |  6024 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  6025 | `					/* Do not install null value */` |
|        9 |  6026 | `					continue;` |
|        - |  6027 | `				}` |
|        9 |  6028 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|        9 |  6029 | `				if( pValue == 0 ){` |
|      ! 0 |  6030 | `					continue;` |
|        - |  6031 | `				}` |
|        - |  6032 | `				/* Invalidate any prior representation */` |
|        9 |  6033 | `				PH7_MemObjRelease(pValue);` |
|        - |  6034 | `				/* Duplicate bound variable value */` |
|        9 |  6035 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|        5 |  6036 | `			}` |
|        4 |  6037 | `		}` |
|        - |  6038 | `		/* Process default values */` |
|    14338 |  6039 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     1866 |  6040 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     1860 |  6041 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     1860 |  6042 | `				if( pObj ){` |
|        - |  6043 | `					/* Evaluate the default value and extract it's result */` |
|     1860 |  6044 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     1860 |  6045 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  6046 | `						goto Abort;` |
|        - |  6047 | `					}` |
|        - |  6048 | `					/* Insert argument index */` |
|     1860 |  6049 | `					sArg.nIdx = pObj->nIdx;` |
|     1860 |  6050 | `					sArg.pUserData = 0;` |
|     1860 |  6051 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  6052 | `					/* Make sure the default argument is of the correct type */` |
|     1860 |  6053 | `					if( aFormalArg[n].nType > 0 && ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|      ! 0 |  6054 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  6055 | `						/* Cast to the desired type */` |
|      ! 0 |  6056 | `						xCast(pObj);` |
|      ! 0 |  6057 | `					}` |
|      929 |  6058 | `				}` |
|      929 |  6059 | `			}` |
|     1866 |  6060 | `			++n;` |
|        2 |  6061 | `		}` |
|        - |  6062 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  6063 | `		 * does not return anything.` |
|        - |  6064 | `		 */` |
|    12474 |  6065 | `		PH7_MemObjRelease(pTos);` |
|    12474 |  6066 | `		pTos = &pTos[-pInstr->iP1];` |
|        - |  6067 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    12474 |  6068 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    12474 |  6069 | `		if( pFrameStack == 0 ){` |
|        - |  6070 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  6071 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  6072 | `				&pVmFunc->sName);` |
|      ! 0 |  6073 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6074 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6075 | `			}` |
|      ! 0 |  6076 | `			break;` |
|        - |  6077 | `		}` |
|    12474 |  6078 | `		if( pSelf ){` |
|        - |  6079 | `			/* Push class name */` |
|     1602 |  6080 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|      800 |  6081 | `		}` |
|        - |  6082 | `		/* Increment nesting level */` |
|    12474 |  6083 | `		pVm->nRecursionDepth++;` |
|        - |  6084 | `		/* Execute function body */` |
|    12474 |  6085 | `		rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE);` |
|        - |  6086 | `		/* Decrement nesting level */` |
|    12474 |  6087 | `		pVm->nRecursionDepth--;` |
|    12474 |  6088 | `		if( pSelf ){` |
|        - |  6089 | `			/* Pop class name */` |
|     1602 |  6090 | `			(void)SySetPop(&pVm->aSelf);` |
|      800 |  6091 | `		}` |
|        - |  6092 | `		/* Cleanup the mess left behind */` |
|    12474 |  6093 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  6094 | `			/* Return by reference,reflect that */` |
|        9 |  6095 | `			if( n != SXU32_HIGH ){` |
|        9 |  6096 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  6097 | `				sxu32 i;` |
|        - |  6098 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  6099 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  6100 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  6101 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  6102 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6103 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6104 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  6105 | `								&pVmFunc->sName);` |
|      ! 0 |  6106 | `						}` |
|      ! 0 |  6107 | `						n = SXU32_HIGH;` |
|      ! 0 |  6108 | `						break;` |
|        - |  6109 | `					}` |
|        3 |  6110 | `				}` |
|        5 |  6111 | `			}else{` |
|      ! 0 |  6112 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6113 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  6114 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  6115 | `						&pVmFunc->sName);` |
|      ! 0 |  6116 | `				}` |
|        - |  6117 | `			}` |
|        9 |  6118 | `			pTos->nIdx = n;` |
|        4 |  6119 | `		}` |
|        - |  6120 | `		/* Cleanup the mess left behind */` |
|    12474 |  6121 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  6122 | `			/* An exception was throw in this frame */` |
|        7 |  6123 | `			pFrame = pFrame->pParent;` |
|        7 |  6124 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  6125 | `				/* Pop the resutlt */` |
|        5 |  6126 | `				VmPopOperand(&pTos,1);` |
|        - |  6127 | `				/* Jump to this destination */` |
|        5 |  6128 | `				pc = pFrame->iExceptionJump - 1;` |
|        5 |  6129 | `				rc = PH7_OK;` |
|        3 |  6130 | `			}else{` |
|        3 |  6131 | `				if( pFrame->pParent ){` |
|        3 |  6132 | `					rc = PH7_EXCEPTION;` |
|        2 |  6133 | `				}else{` |
|        - |  6134 | `					/* Continue normal execution */` |
|      ! 0 |  6135 | `					rc = PH7_OK;` |
|        - |  6136 | `				}` |
|        - |  6137 | `			}` |
|        3 |  6138 | `		}` |
|        - |  6139 | `		/* Free the operand stack */` |
|    12474 |  6140 | `		SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|        - |  6141 | `		/* Leave the frame */` |
|    12474 |  6142 | `		VmLeaveFrame(&(*pVm));` |
|    12474 |  6143 | `		if( rc == PH7_ABORT ){` |
|        - |  6144 | `			/* Abort processing immeditaley */` |
|        7 |  6145 | `			goto Abort;` |
|    12468 |  6146 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  6147 | `			goto Exception;` |
|        - |  6148 | `		}` |
|     6234 |  6149 | `	}else{` |
|        - |  6150 | `		ph7_user_func *pFunc;` |
|        - |  6151 | `		ph7_context sCtx;` |
|        - |  6152 | `		ph7_value sRet;` |
|        - |  6153 | `		/* Look for an installed foreign function.` |
|        - |  6154 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  6155 | `		 * If the CALL instruction's p3 is set (compiler-qualified name),` |
|        - |  6156 | `		 * extract the short name (last component after \) and try that.` |
|        - |  6157 | `		 * This implements PHP's global fallback for unqualified function` |
|        - |  6158 | `		 * calls in namespaces. User-written qualified names (like` |
|        - |  6159 | `		 * \Bogus\strlen) do NOT get this fallback. */` |
|   548900 |  6160 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|   548900 |  6161 | `		if( pEntry == 0 && pInstr->p3 != 0 ){` |
|        - |  6162 | `			/* Compiler-qualified: try short name as global fallback */` |
|       15 |  6163 | `			const char *zShort = sName.zString;` |
|        - |  6164 | `			sxu32 i;` |
|      217 |  6165 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      203 |  6166 | `				if( sName.zString[i] == '\\' ){` |
|       19 |  6167 | `					zShort = &sName.zString[i + 1];` |
|        9 |  6168 | `				}` |
|      102 |  6169 | `			}` |
|       15 |  6170 | `			if( zShort != sName.zString ){` |
|       15 |  6171 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       15 |  6172 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|        7 |  6173 | `			}` |
|        7 |  6174 | `		}` |
|   548900 |  6175 | `		if( pEntry == 0 ){` |
|        - |  6176 | `			/* Call to undefined function */` |
|        5 |  6177 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  6178 | `			/* Pop given arguments */` |
|        5 |  6179 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  6180 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  6181 | `			}` |
|        - |  6182 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  6183 | `			PH7_MemObjRelease(pTos);` |
|        8 |  6184 | `			break;` |
|        - |  6185 | `		}` |
|   548896 |  6186 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  6187 | `		/* Start collecting function arguments */` |
|   548896 |  6188 | `		SySetReset(&aArg);` |
|  1475656 |  6189 | `		while( pArg < pTos ){` |
|   926762 |  6190 | `			SySetPut(&aArg,(const void *)&pArg);` |
|   926762 |  6191 | `			pArg++;` |
|        2 |  6192 | `		}` |
|        - |  6193 | `		/* Assume a null return value */` |
|   548896 |  6194 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  6195 | `		/* Init the call context */` |
|   548896 |  6196 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  6197 | `		/* Call the foreign function */` |
|   548896 |  6198 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  6199 | `		/* Release the call context */` |
|   548896 |  6200 | `		VmReleaseCallContext(&sCtx);` |
|   548896 |  6201 | `		if( rc == PH7_ABORT ){` |
|      463 |  6202 | `			goto Abort;` |
|   548434 |  6203 | `		}else if( rc == PH7_EXCEPTION ){` |
|        7 |  6204 | `			VmFrame *pFrm = pVm->pFrame;` |
|        7 |  6205 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|        7 |  6206 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  6207 | `				/* Exception was NOT caught, propagate */` |
|      ! 0 |  6208 | `				goto Exception;` |
|        - |  6209 | `			}` |
|        - |  6210 | `			/* Exception was caught: pop args and the result slot */` |
|        7 |  6211 | `			PH7_MemObjRelease(&sRet);` |
|        7 |  6212 | `			if( pInstr->iP1 > 0 ){` |
|        3 |  6213 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|        1 |  6214 | `			}` |
|        - |  6215 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|        7 |  6216 | `			VmPopOperand(&pTos,1);` |
|        - |  6217 | `			/* Jump past the try/catch block via the exception frame */` |
|        7 |  6218 | `			pFrm = pVm->pFrame;` |
|        7 |  6219 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|        7 |  6220 | `				pc = pFrm->iExceptionJump - 1;` |
|        3 |  6221 | `			}` |
|        7 |  6222 | `			break;` |
|        - |  6223 | `		}` |
|   548428 |  6224 | `		if( pInstr->iP1 > 0 ){` |
|        - |  6225 | `			/* Pop function name and arguments */` |
|   531132 |  6226 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   265587 |  6227 | `		}` |
|        - |  6228 | `		/* Save foreign function return value */` |
|   548428 |  6229 | `		PH7_MemObjStore(&sRet,pTos);` |
|   548428 |  6230 | `		PH7_MemObjRelease(&sRet);` |
|        - |  6231 | `	}` |
|   560892 |  6232 | `	break;` |
|        - |  6233 | `				  }` |
|        - |  6234 | `/*` |
|        - |  6235 | ` * OP_CONSUME: P1 * *` |
|        - |  6236 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  6237 | ` */` |
|    10902 |  6238 | `case PH7_OP_CONSUME: {` |
|    21806 |  6239 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    21806 |  6240 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  6241 |  |
|    21806 |  6242 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    21806 |  6243 | `	pCur = pOut;` |
|        - |  6244 | `	/* Start the consume process  */` |
|    43610 |  6245 | `	while( pOut <= pTos ){` |
|        - |  6246 | `		/* Force a string cast */` |
|    21806 |  6247 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  6248 | `			PH7_MemObjToString(pOut);` |
|      149 |  6249 | `		}` |
|    21806 |  6250 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  6251 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  6252 | `			/* Invoke the output consumer callback */` |
|    11978 |  6253 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    11978 |  6254 | `			if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6255 | `				/* Increment output length */` |
|     5532 |  6256 | `				pVm->nOutputLen += SyBlobLength(&pOut->sBlob);` |
|     2765 |  6257 | `			}` |
|    11978 |  6258 | `			SyBlobRelease(&pOut->sBlob);` |
|    11978 |  6259 | `			if( rc == SXERR_ABORT ){` |
|        - |  6260 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  6261 | `				goto Abort;` |
|        - |  6262 | `			}` |
|     5988 |  6263 | `		}` |
|    21806 |  6264 | `		pOut++;` |
|        2 |  6265 | `	}` |
|    21806 |  6266 | `	pTos = &pCur[-1];` |
|    21804 |  6267 | `	break;` |
|        - |  6268 | `					 }` |
|        - |  6269 |  |
|        - |  6270 | `		} /* Switch() */` |
|  9684624 |  6271 | `		pc++; /* Next instruction in the stream */` |
|        2 |  6272 | `	} /* For(;;) */` |
|    15298 |  6273 | `Done:` |
|    30598 |  6274 | `	SySetRelease(&aArg);` |
|    30598 |  6275 | `	return SXRET_OK;` |
|      238 |  6276 | `Abort:` |
|      477 |  6277 | `	SySetRelease(&aArg);` |
|     1661 |  6278 | `	while( pTos >= pStack ){` |
|     1185 |  6279 | `		PH7_MemObjRelease(pTos);` |
|     1185 |  6280 | `		pTos--;` |
|        1 |  6281 | `	}` |
|      477 |  6282 | `	return PH7_ABORT;` |
|        1 |  6283 | `Exception:` |
|        3 |  6284 | `	SySetRelease(&aArg);` |
|        5 |  6285 | `	while( pTos >= pStack ){` |
|        3 |  6286 | `		PH7_MemObjRelease(pTos);` |
|        3 |  6287 | `		pTos--;` |
|        1 |  6288 | `	}` |
|        3 |  6289 | `	return PH7_EXCEPTION;` |
|    15539 |  6290 |  |
|        - |  6291 | `/*` |
|        - |  6292 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  6293 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6294 | ` * See block-comment on that function for additional information.` |
|        - |  6295 | ` */` |
|    14578 |  6296 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  6297 |  |
|        - |  6298 | `	ph7_value *pStack;` |
|        - |  6299 | `	sxi32 rc;` |
|        - |  6300 | `	/* Allocate a new operand stack */` |
|    14580 |  6301 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    14580 |  6302 | `	if( pStack == 0 ){` |
|      ! 0 |  6303 | `		return SXERR_MEM;` |
|        - |  6304 | `	}` |
|        - |  6305 | `	/* Execute the program */` |
|    14580 |  6306 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE);` |
|        - |  6307 | `	/* Free the operand stack */` |
|    14580 |  6308 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  6309 | `	/* Execution result */` |
|    14580 |  6310 | `	return rc;` |
|     7291 |  6311 |  |
|        - |  6312 | `/*` |
|        - |  6313 | ` * Invoke any installed shutdown callbacks.` |
|        - |  6314 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  6315 | ` * or more calls to [register_shutdown_function()].` |
|        - |  6316 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  6317 | ` * execution ends.` |
|        - |  6318 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  6319 | ` * additional information.` |
|        - |  6320 | ` */` |
|     2280 |  6321 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  6322 |  |
|        - |  6323 | `	VmShutdownCB *pEntry;` |
|        - |  6324 | `	ph7_value *apArg[10];` |
|        - |  6325 | `	sxu32 n,nEntry;` |
|        - |  6326 | `	int i;` |
|        - |  6327 | `	/* Point to the stack of registered callbacks */` |
|     2282 |  6328 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    25082 |  6329 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    22802 |  6330 | `		apArg[i] = 0;` |
|    11402 |  6331 | `	}` |
|     2284 |  6332 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  6333 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6334 | `		if( pEntry ){` |
|        - |  6335 | `			/* Prepare callback arguments if any */` |
|        3 |  6336 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  6337 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  6338 | `					break;` |
|        - |  6339 | `				}` |
|      ! 0 |  6340 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  6341 | `			}` |
|        - |  6342 | `			/* Invoke the callback */` |
|        3 |  6343 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  6344 | `			/*` |
|        - |  6345 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  6346 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  6347 | `			 */` |
|        3 |  6348 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  6349 | `			if( pEntry ){` |
|        3 |  6350 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  6351 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  6352 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  6353 | `				}` |
|        1 |  6354 | `			}` |
|        1 |  6355 | `		}` |
|        2 |  6356 | `	}` |
|     2282 |  6357 | `	SySetReset(&pVm->aShutdown);` |
|     2282 |  6358 |  |
|        - |  6359 | `/*` |
|        - |  6360 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  6361 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  6362 | ` * See block-comment on that function for additional information.` |
|        - |  6363 | ` */` |
|     2288 |  6364 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  6365 |  |
|        - |  6366 | `	/* Make sure we are ready to execute this program */` |
|     2290 |  6367 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  6368 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  6369 | `	}` |
|        - |  6370 | `	/* Set the execution magic number  */` |
|     2290 |  6371 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  6372 | `	/* Execute the program */` |
|     2290 |  6373 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE);` |
|        - |  6374 | `	/* Invoke any shutdown callbacks */` |
|     2286 |  6375 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  6376 | `	/*` |
|        - |  6377 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  6378 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  6379 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  6380 | `	 */` |
|     2286 |  6381 | `	return SXRET_OK;` |
|     1146 |  6382 |  |
|        - |  6383 | `/*` |
|        - |  6384 | ` * Invoke the installed VM output consumer callback to consume` |
|        - |  6385 | ` * the desired message.` |
|        - |  6386 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - |  6387 | ` * in 'api.c' for additional information.` |
|        - |  6388 | ` */` |
|      350 |  6389 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - |  6390 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |  6391 | `	SyString *pString /* Message to output */` |
|        - |  6392 | `	)` |
|        2 |  6393 |  |
|      352 |  6394 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      352 |  6395 | `	sxi32 rc = SXRET_OK;` |
|        - |  6396 | `	/* Call the output consumer */` |
|      352 |  6397 | `	if( pString->nByte > 0 ){` |
|      352 |  6398 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      352 |  6399 | `		if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6400 | `			/* Increment output length */` |
|       17 |  6401 | `			pVm->nOutputLen += pString->nByte;` |
|        8 |  6402 | `		}` |
|      175 |  6403 | `	}` |
|      352 |  6404 | `	return rc;` |
|        2 |  6405 |  |
|        - |  6406 | `/*` |
|        - |  6407 | ` * Format a message and invoke the installed VM output consumer` |
|        - |  6408 | ` * callback to consume the formatted message.` |
|        - |  6409 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - |  6410 | ` * in 'api.c' for additional information.` |
|        - |  6411 | ` */` |
|        2 |  6412 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - |  6413 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  6414 | `	const char *zFormat, /* Formatted message to output */` |
|        - |  6415 | `	va_list ap           /* Variable list of arguments */` |
|        - |  6416 | `	)` |
|        1 |  6417 |  |
|        3 |  6418 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 |  6419 | `	sxi32 rc = SXRET_OK;` |
|        - |  6420 | `	SyBlob sWorker;` |
|        - |  6421 | `	/* Format the message and call the output consumer */` |
|        3 |  6422 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 |  6423 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 |  6424 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - |  6425 | `		/* Consume the formatted message */` |
|        3 |  6426 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 |  6427 | `	}` |
|        3 |  6428 | `	if( pCons->xConsumer != VmObConsumer ){` |
|        - |  6429 | `		/* Increment output length */` |
|      ! 0 |  6430 | `		pVm->nOutputLen += SyBlobLength(&sWorker);` |
|      ! 0 |  6431 | `	}` |
|        - |  6432 | `	/* Release the working buffer */` |
|        3 |  6433 | `	SyBlobRelease(&sWorker);` |
|        3 |  6434 | `	return rc;` |
|        1 |  6435 |  |
|        - |  6436 | `/*` |
|        - |  6437 | ` * Return a string representation of the given PH7 OP code.` |
|        - |  6438 | ` * This function never fail and always return a pointer` |
|        - |  6439 | ` * to a null terminated string.` |
|        - |  6440 | ` */` |
|       12 |  6441 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 |  6442 |  |
|       13 |  6443 | `	const char *zOp = "Unknown     ";` |
|       13 |  6444 | `	switch(nOp){` |
|        3 |  6445 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 |  6446 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 |  6447 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 |  6448 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 |  6449 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 |  6450 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 |  6451 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 |  6452 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 |  6453 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 |  6454 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 |  6455 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 |  6456 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 |  6457 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 |  6458 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 |  6459 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 |  6460 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 |  6461 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 |  6462 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 |  6463 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 |  6464 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 |  6465 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 |  6466 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 |  6467 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 |  6468 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 |  6469 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 |  6470 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 |  6471 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 |  6472 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 |  6473 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 |  6474 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 |  6475 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 |  6476 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 |  6477 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 |  6478 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 |  6479 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 |  6480 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 |  6481 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 |  6482 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 |  6483 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 |  6484 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 |  6485 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 |  6486 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 |  6487 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 |  6488 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 |  6489 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 |  6490 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 |  6491 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 |  6492 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 |  6493 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 |  6494 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 |  6495 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 |  6496 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 |  6497 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 |  6498 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 |  6499 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 |  6500 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 |  6501 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 |  6502 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 |  6503 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 |  6504 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 |  6505 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 |  6506 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 |  6507 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 |  6508 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 |  6509 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 |  6510 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 |  6511 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 |  6512 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 |  6513 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 |  6514 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 |  6515 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 |  6516 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 |  6517 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 |  6518 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 |  6519 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 |  6520 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 |  6521 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 |  6522 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 |  6523 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 |  6524 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 |  6525 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 |  6526 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 |  6527 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 |  6528 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 |  6529 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 |  6530 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 |  6531 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 |  6532 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 |  6533 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 |  6534 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 |  6535 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 |  6536 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 |  6537 | `	default:` |
|      ! 0 |  6538 | `		break;` |
|        - |  6539 | `	}` |
|       13 |  6540 | `	return zOp;` |
|        1 |  6541 |  |
|        - |  6542 | `/*` |
|        - |  6543 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - |  6544 | ` * The xConsumer() callback which is an used defined function` |
|        - |  6545 | ` * is responsible of consuming the generated dump.` |
|        - |  6546 | ` */` |
|        2 |  6547 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - |  6548 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |  6549 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - |  6550 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  6551 | `	)` |
|        1 |  6552 |  |
|        - |  6553 | `	sxi32 rc;` |
|        3 |  6554 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 |  6555 | `	return rc;` |
|        1 |  6556 |  |
|        - |  6557 | `/*` |
|        - |  6558 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - |  6559 | ` * outside a class body [i.e: global or function scope].` |
|        - |  6560 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - |  6561 | ` * in 'compile.c' for additional information.` |
|        - |  6562 | ` */` |
|        8 |  6563 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 |  6564 |  |
|        9 |  6565 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - |  6566 | `	/* Evaluate and expand constant value */` |
|        9 |  6567 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|        9 |  6568 |  |
|        - |  6569 | `/*` |
|        - |  6570 | ` * Section:` |
|        - |  6571 | ` *  Function handling functions.` |
|        - |  6572 | ` * Status:` |
|        - |  6573 | ` *    Stable.` |
|        - |  6574 | ` */` |
|        - |  6575 | `/*` |
|        - |  6576 | ` * int func_num_args(void)` |
|        - |  6577 | ` *   Returns the number of arguments passed to the function.` |
|        - |  6578 | ` * Parameters` |
|        - |  6579 | ` *   None.` |
|        - |  6580 | ` * Return` |
|        - |  6581 | ` *  Total number of arguments passed into the current user-defined function` |
|        - |  6582 | ` *  or -1 if called from the globe scope.` |
|        - |  6583 | ` */` |
|      906 |  6584 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6585 |  |
|        - |  6586 | `	VmFrame *pFrame;` |
|        - |  6587 | `	ph7_vm *pVm;` |
|        - |  6588 | `	/* Point to the target VM */` |
|      908 |  6589 | `	pVm = pCtx->pVm;` |
|        - |  6590 | `	/* Current frame */` |
|      908 |  6591 | `	pFrame = pVm->pFrame;` |
|      908 |  6592 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      908 |  6593 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 |  6594 | `		SXUNUSED(nArg);` |
|      ! 0 |  6595 | `		SXUNUSED(apArg);` |
|        - |  6596 | `		/* Global frame,return -1 */` |
|      ! 0 |  6597 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 |  6598 | `		return SXRET_OK;` |
|        - |  6599 | `	}` |
|        - |  6600 | `	/* Total number of arguments passed to the enclosing function */` |
|      908 |  6601 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      908 |  6602 | `	ph7_result_int(pCtx,nArg);` |
|      908 |  6603 | `	return SXRET_OK;` |
|      455 |  6604 |  |
|        - |  6605 | `/*` |
|        - |  6606 | ` * value func_get_arg(int $arg_num)` |
|        - |  6607 | ` *   Return an item from the argument list.` |
|        - |  6608 | ` * Parameters` |
|        - |  6609 | ` *  Argument number(index start from zero).` |
|        - |  6610 | ` * Return` |
|        - |  6611 | ` *  Returns the specified argument or FALSE on error.` |
|        - |  6612 | ` */` |
|       22 |  6613 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6614 |  |
|       24 |  6615 | `	ph7_value *pObj = 0;` |
|       24 |  6616 | `	VmSlot *pSlot = 0;` |
|        - |  6617 | `	VmFrame *pFrame;` |
|        - |  6618 | `	ph7_vm *pVm;` |
|        - |  6619 | `	/* Point to the target VM */` |
|       24 |  6620 | `	pVm = pCtx->pVm;` |
|        - |  6621 | `	/* Current frame */` |
|       24 |  6622 | `	pFrame = pVm->pFrame;` |
|       24 |  6623 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 |  6624 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - |  6625 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 |  6626 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 |  6627 | `		ph7_result_bool(pCtx,0);` |
|        3 |  6628 | `		return SXRET_OK;` |
|        - |  6629 | `	}` |
|        - |  6630 | `	/* Extract the desired index */` |
|       21 |  6631 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 |  6632 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - |  6633 | `		/* Invalid index,return FALSE */` |
|      ! 0 |  6634 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6635 | `		return SXRET_OK;` |
|        - |  6636 | `	}` |
|        - |  6637 | `	/* Extract the desired argument */` |
|       21 |  6638 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 |  6639 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - |  6640 | `			/* Return the desired argument */` |
|       21 |  6641 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 |  6642 | `		}else{` |
|        - |  6643 | `			/* No such argument,return false */` |
|      ! 0 |  6644 | `			ph7_result_bool(pCtx,0);` |
|        - |  6645 | `		}` |
|       11 |  6646 | `	}else{` |
|        - |  6647 | `		/* CAN'T HAPPEN */` |
|      ! 0 |  6648 | `		ph7_result_bool(pCtx,0);` |
|        - |  6649 | `	}` |
|       21 |  6650 | `	return SXRET_OK;` |
|       13 |  6651 |  |
|        - |  6652 | `/*` |
|        - |  6653 | ` * array func_get_args_byref(void)` |
|        - |  6654 | ` *   Returns an array comprising a function's argument list.` |
|        - |  6655 | ` * Parameters` |
|        - |  6656 | ` *  None.` |
|        - |  6657 | ` * Return` |
|        - |  6658 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - |  6659 | ` *  member of the current user-defined function's argument list.` |
|        - |  6660 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6661 | ` * NOTE:` |
|        - |  6662 | ` *  Arguments are returned to the array by reference.` |
|        - |  6663 | ` */` |
|        2 |  6664 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6665 |  |
|        - |  6666 | `	ph7_value *pArray;` |
|        - |  6667 | `	VmFrame *pFrame;` |
|        - |  6668 | `	VmSlot *aSlot;` |
|        - |  6669 | `	sxu32 n;` |
|        - |  6670 | `	/* Point to the current frame */` |
|        3 |  6671 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 |  6672 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 |  6673 | `	if( pFrame->pParent == 0 ){` |
|        - |  6674 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6675 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6676 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6677 | `		return SXRET_OK;` |
|        - |  6678 | `	}` |
|        - |  6679 | `	/* Create a new array */` |
|        3 |  6680 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6681 | `	if( pArray == 0 ){` |
|      ! 0 |  6682 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6683 | `		SXUNUSED(apArg);` |
|      ! 0 |  6684 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6685 | `		return SXRET_OK;` |
|        - |  6686 | `	}` |
|        - |  6687 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 |  6688 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 |  6689 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 |  6690 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 |  6691 | `	}` |
|        - |  6692 | `	/* Return the freshly created array */` |
|        3 |  6693 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6694 | `	return SXRET_OK;` |
|        2 |  6695 |  |
|        - |  6696 | `/*` |
|        - |  6697 | ` * array func_get_args(void)` |
|        - |  6698 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - |  6699 | ` * Parameters` |
|        - |  6700 | ` *  None.` |
|        - |  6701 | ` * Return` |
|        - |  6702 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - |  6703 | ` *  member of the current user-defined function's argument list.` |
|        - |  6704 | ` *  Otherwise FALSE is returned on failure.` |
|        - |  6705 | ` */` |
|       62 |  6706 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6707 |  |
|       64 |  6708 | `	ph7_value *pObj = 0;` |
|        - |  6709 | `	ph7_value *pArray;` |
|        - |  6710 | `	VmFrame *pFrame;` |
|        - |  6711 | `	VmSlot *aSlot;` |
|        - |  6712 | `	sxu32 n;` |
|        - |  6713 | `	/* Point to the current frame */` |
|       64 |  6714 | `	pFrame = pCtx->pVm->pFrame;` |
|       64 |  6715 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       64 |  6716 | `	if( pFrame->pParent == 0 ){` |
|        - |  6717 | `		/* Global frame,return FALSE */` |
|      ! 0 |  6718 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 |  6719 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6720 | `		return SXRET_OK;` |
|        - |  6721 | `	}` |
|        - |  6722 | `	/* Create a new array */` |
|       64 |  6723 | `	pArray = ph7_context_new_array(pCtx);` |
|       64 |  6724 | `	if( pArray == 0 ){` |
|      ! 0 |  6725 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6726 | `		SXUNUSED(apArg);` |
|      ! 0 |  6727 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6728 | `		return SXRET_OK;` |
|        - |  6729 | `	}` |
|        - |  6730 | `	/* Start filling the array with the given arguments */` |
|       64 |  6731 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      192 |  6732 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      130 |  6733 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      130 |  6734 | `		if( pObj ){` |
|      130 |  6735 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       64 |  6736 | `		}` |
|       66 |  6737 | `	}` |
|        - |  6738 | `	/* Return the freshly created array */` |
|       64 |  6739 | `	ph7_result_value(pCtx,pArray);` |
|       64 |  6740 | `	return SXRET_OK;` |
|       33 |  6741 |  |
|        - |  6742 | `/*` |
|        - |  6743 | ` * bool function_exists(string $name)` |
|        - |  6744 | ` *  Return TRUE if the given function has been defined.` |
|        - |  6745 | ` * Parameters` |
|        - |  6746 | ` *  The name of the desired function.` |
|        - |  6747 | ` * Return` |
|        - |  6748 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - |  6749 | ` */` |
|     1646 |  6750 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  6751 |  |
|        - |  6752 | `	const char *zName;` |
|        - |  6753 | `	ph7_vm *pVm;` |
|        - |  6754 | `	int nLen;` |
|        - |  6755 | `	int res;` |
|     1648 |  6756 | `	if( nArg < 1 ){` |
|        - |  6757 | `		/* Missing argument,return FALSE */` |
|      ! 0 |  6758 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6759 | `		return SXRET_OK;` |
|        - |  6760 | `	}` |
|        - |  6761 | `	/* Point to the target VM */` |
|     1648 |  6762 | `	pVm = pCtx->pVm;` |
|        - |  6763 | `	/* Extract the function name */` |
|     1648 |  6764 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  6765 | `	/* Assume the function is not defined */` |
|     1648 |  6766 | `	res = 0;` |
|        - |  6767 | `	/* Perform the lookup */` |
|     2469 |  6768 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1642 |  6769 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6770 | `			/* Function is defined */` |
|      206 |  6771 | `			res = 1;` |
|      102 |  6772 | `	}` |
|     1648 |  6773 | `	ph7_result_bool(pCtx,res);` |
|     1648 |  6774 | `	return SXRET_OK;` |
|      825 |  6775 |  |
|        - |  6776 | `/*` |
|        - |  6777 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6778 | ` * [i.e: Whether it is callable or not].` |
|        - |  6779 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - |  6780 | ` */` |
|    16002 |  6781 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 |  6782 |  |
|    16004 |  6783 | `	int res = 0;` |
|    16004 |  6784 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  6785 | `		/* Call the magic method __invoke if available */` |
|      ! 0 |  6786 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        - |  6787 | `		ph7_class_method *pMethod;` |
|      ! 0 |  6788 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      ! 0 |  6789 | `		if( pMethod && CallInvoke ){` |
|        - |  6790 | `			ph7_value sResult;` |
|        - |  6791 | `			sxi32 rc;` |
|        - |  6792 | `			/* Invoke the magic method and extract the result */` |
|      ! 0 |  6793 | `			PH7_MemObjInit(pVm,&sResult);` |
|      ! 0 |  6794 | `			rc = PH7_VmCallClassMethod(pVm,pThis,pMethod,&sResult,0,0);` |
|      ! 0 |  6795 | `			if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT)) ){` |
|      ! 0 |  6796 | `				res = sResult.x.iVal != 0;` |
|      ! 0 |  6797 | `			}` |
|      ! 0 |  6798 | `			PH7_MemObjRelease(&sResult);` |
|      ! 0 |  6799 | `		}` |
|    16004 |  6800 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 |  6801 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 |  6802 | `		if( pMap->nEntry == 2 ){` |
|        - |  6803 | `			ph7_class *pClass;` |
|        - |  6804 | `			ph7_value *pV;` |
|        - |  6805 | `			/* Extract the target class */` |
|       12 |  6806 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 |  6807 | `			if( pV ){` |
|       12 |  6808 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 |  6809 | `				if( pClass ){` |
|        - |  6810 | `					ph7_class_method *pMethod;` |
|        - |  6811 | `					/* Extract the target method */` |
|       10 |  6812 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 |  6813 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - |  6814 | `						/* Perform the lookup */` |
|       10 |  6815 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 |  6816 | `						if( pMethod ){` |
|        - |  6817 | `							/* Method is callable */` |
|        5 |  6818 | `							res = 1;` |
|        2 |  6819 | `						}` |
|        4 |  6820 | `					}` |
|        4 |  6821 | `				}` |
|        5 |  6822 | `			}` |
|        7 |  6823 | `		}` |
|    15991 |  6824 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - |  6825 | `		const char *zName;` |
|        - |  6826 | `		int nLen;` |
|        - |  6827 | `		/* Extract the name */` |
|     4700 |  6828 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - |  6829 | `		/* Perform the lookup */` |
|     4715 |  6830 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 |  6831 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  6832 | `				/* Function is callable */` |
|     4682 |  6833 | `				res = 1;` |
|     2340 |  6834 | `		}` |
|     2349 |  6835 | `	}` |
|    16004 |  6836 | `	return res;` |
|        2 |  6837 |  |
|        - |  6838 | `/*` |
|        - |  6839 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - |  6840 | ` * Verify that the contents of a variable can be called as a function.` |
|        - |  6841 | ` * Parameters` |
|        - |  6842 | ` * $name` |
|        - |  6843 | ` *    The callback function to check` |
|        - |  6844 | ` * $syntax_only` |
|        - |  6845 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - |  6846 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - |  6847 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - |  6848 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - |  6849 | ` *    a string.` |
|        - |  6850 | ` * Return` |
|        - |  6851 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - |  6852 | ` */` |
|       14 |  6853 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6854 |  |
|        - |  6855 | `	ph7_vm *pVm;` |
|        - |  6856 | `	int res;` |
|       15 |  6857 | `	if( nArg < 1 ){` |
|        - |  6858 | `		/* Missing arguments,return FALSE */` |
|      ! 0 |  6859 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  6860 | `		return SXRET_OK;` |
|        - |  6861 | `	}` |
|        - |  6862 | `	/* Point to the target VM */` |
|       15 |  6863 | `	pVm = pCtx->pVm;` |
|        - |  6864 | `	/* Perform the requested operation */` |
|       15 |  6865 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       15 |  6866 | `	ph7_result_bool(pCtx,res);` |
|       15 |  6867 | `	return SXRET_OK;` |
|        8 |  6868 |  |
|        - |  6869 | `/*` |
|        - |  6870 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - |  6871 | ` * defined below.` |
|        - |  6872 | ` */` |
|     1082 |  6873 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  6874 |  |
|     1083 |  6875 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  6876 | `	ph7_value sName;` |
|        - |  6877 | `	sxi32 rc;` |
|        - |  6878 | `	/* Prepare the function name for insertion */` |
|     1083 |  6879 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1083 |  6880 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  6881 | `	/* Perform the insertion */` |
|     1083 |  6882 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1083 |  6883 | `	PH7_MemObjRelease(&sName);` |
|     1083 |  6884 | `	return rc;` |
|        1 |  6885 |  |
|        - |  6886 | `/*` |
|        - |  6887 | ` * array get_defined_functions(void)` |
|        - |  6888 | ` *  Returns an array of all defined functions.` |
|        - |  6889 | ` * Parameter` |
|        - |  6890 | ` *  None.` |
|        - |  6891 | ` * Return` |
|        - |  6892 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - |  6893 | ` *  both built-in (internal) and user-defined.` |
|        - |  6894 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - |  6895 | ` *  defined ones using $arr["user"].` |
|        - |  6896 | ` * Note:` |
|        - |  6897 | ` *  NULL is returned on failure.` |
|        - |  6898 | ` */` |
|        2 |  6899 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6900 |  |
|        - |  6901 | `	ph7_value *pArray,*pEntry;` |
|        - |  6902 | `	/* NOTE:` |
|        - |  6903 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - |  6904 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - |  6905 | `	 */` |
|        3 |  6906 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  6907 | ` 	if( pArray == 0 ){` |
|      ! 0 |  6908 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  6909 | `		SXUNUSED(apArg);` |
|        - |  6910 | `		/* Return NULL */` |
|      ! 0 |  6911 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6912 | `		return SXRET_OK;` |
|        - |  6913 | `	}` |
|        3 |  6914 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6915 | `	if( pEntry == 0 ){` |
|        - |  6916 | `		/* Return NULL */` |
|      ! 0 |  6917 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6918 | `		return SXRET_OK;` |
|        - |  6919 | `	}` |
|        - |  6920 | `	/* Fill with the appropriate information */` |
|        3 |  6921 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - |  6922 | `	/* Create the 'internal' index */` |
|        3 |  6923 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - |  6924 | `	/* Create the user-func array */` |
|        3 |  6925 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 |  6926 | `	if( pEntry == 0 ){` |
|        - |  6927 | `		/* Return NULL */` |
|      ! 0 |  6928 | `		ph7_result_null(pCtx);` |
|      ! 0 |  6929 | `		return SXRET_OK;` |
|        - |  6930 | `	}` |
|        - |  6931 | `	/* Fill with the appropriate information */` |
|        3 |  6932 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - |  6933 | `	/* Create the 'user' index */` |
|        3 |  6934 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - |  6935 | `	/* Return the multi-dimensional array */` |
|        3 |  6936 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  6937 | `	return SXRET_OK;` |
|        2 |  6938 |  |
|        - |  6939 | `/*` |
|        - |  6940 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - |  6941 | ` *  Register a function for execution on shutdown.` |
|        - |  6942 | ` * Note` |
|        - |  6943 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - |  6944 | ` *  be called in the same order as they were registered.` |
|        - |  6945 | ` * Parameters` |
|        - |  6946 | ` *  $callback` |
|        - |  6947 | ` *   The shutdown callback to register.` |
|        - |  6948 | ` * $param` |
|        - |  6949 | ` *  One or more Parameter to pass to the registered callback.` |
|        - |  6950 | ` * Return` |
|        - |  6951 | ` *  Nothing.` |
|        - |  6952 | ` */` |
|        2 |  6953 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  6954 |  |
|        - |  6955 | `	VmShutdownCB sEntry;` |
|        - |  6956 | `	int i,j;` |
|        3 |  6957 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  6958 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 |  6959 | `		return PH7_OK;` |
|        - |  6960 | `	}` |
|        - |  6961 | `	/* Zero the Entry */` |
|        3 |  6962 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - |  6963 | `	/* Initialize fields */` |
|        3 |  6964 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - |  6965 | `	/* Save the callback name for later invocation name */` |
|        3 |  6966 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 |  6967 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 |  6968 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 |  6969 | `	}` |
|        - |  6970 | `	/* Copy arguments */` |
|        3 |  6971 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 |  6972 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - |  6973 | `			/* Limit reached */` |
|      ! 0 |  6974 | `			break;` |
|        - |  6975 | `		}` |
|      ! 0 |  6976 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 |  6977 | `	}` |
|        3 |  6978 | `	sEntry.nArg = j;` |
|        - |  6979 | `	/* Install the callback */` |
|        3 |  6980 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 |  6981 | `	return PH7_OK;` |
|        2 |  6982 |  |
|        - |  6983 | `/*` |
|        - |  6984 | ` * Section:` |
|        - |  6985 | ` *  Class handling functions.` |
|        - |  6986 | ` * Status:` |
|        - |  6987 | ` *    Stable.` |
|        - |  6988 | ` */` |
|        - |  6989 | `/*` |
|        - |  6990 | ` * Extract the top active class. NULL is returned` |
|        - |  6991 | ` * if the class stack is empty.` |
|        - |  6992 | ` */` |
|      550 |  6993 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 |  6994 |  |
|      552 |  6995 | `	SySet *pSet = &pVm->aSelf;` |
|        - |  6996 | `	ph7_class **apClass;` |
|      552 |  6997 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - |  6998 | `		/* Empty stack,return NULL */` |
|       15 |  6999 | `		return 0;` |
|        - |  7000 | `	}` |
|        - |  7001 | `	/* Peek the last entry */` |
|      538 |  7002 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      538 |  7003 | `	return apClass[pSet->nUsed - 1];` |
|      277 |  7004 |  |
|        - |  7005 | `/*` |
|        - |  7006 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - |  7007 | ` *   Get the class that declared the currently executing method.` |
|        - |  7008 | ` *   This is used for resolving the 'self::' constant.` |
|        - |  7009 | ` *` |
|        - |  7010 | ` * Parameters` |
|        - |  7011 | ` *   pVm: Target VM` |
|        - |  7012 | ` *` |
|        - |  7013 | ` * Return` |
|        - |  7014 | ` *   The declaring class of the current method, or NULL if:` |
|        - |  7015 | ` *   - Not executing within a class method` |
|        - |  7016 | ` *` |
|        - |  7017 | ` * Note` |
|        - |  7018 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - |  7019 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - |  7020 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - |  7021 | ` *   This is found by walking the call frames to locate the method's` |
|        - |  7022 | ` *   declaring class.` |
|        - |  7023 | ` */` |
|       52 |  7024 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 |  7025 |  |
|       54 |  7026 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  7027 | `	ph7_vm_func *pVmFunc;` |
|        - |  7028 |  |
|        - |  7029 | `	/* Skip exception frames to find the actual method frame */` |
|       54 |  7030 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  7031 |  |
|        - |  7032 | `	/* Check if we're in a method context */` |
|       54 |  7033 | `	if( pFrame->pParent ){` |
|       50 |  7034 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       50 |  7035 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - |  7036 | `			/* Return the declaring class */` |
|       50 |  7037 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - |  7038 | `		}` |
|      ! 0 |  7039 | `	}` |
|        - |  7040 |  |
|        5 |  7041 | `	return 0;` |
|       28 |  7042 |  |
|        - |  7043 |  |
|        - |  7044 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - |  7045 | `/*` |
|        - |  7046 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - |  7047 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - |  7048 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - |  7049 | ` * return value indicates failure.` |
|        - |  7050 | ` */` |
|     1298 |  7051 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - |  7052 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |  7053 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - |  7054 | `	ph7_class_method *pMethod, /* Method name */` |
|        - |  7055 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - |  7056 | `	int nArg,                  /* Total number of given arguments */` |
|        - |  7057 | `	ph7_value **apArg          /* Method arguments */` |
|        - |  7058 | `	)` |
|        2 |  7059 |  |
|        - |  7060 | `	ph7_value *aStack;` |
|        - |  7061 | `	VmInstr aInstr[2];` |
|        - |  7062 | `	int iCursor;` |
|        - |  7063 | `	int i;` |
|        - |  7064 | `	/* Create a new operand stack */` |
|     1300 |  7065 | `	aStack = VmNewOperandStack(&(*pVm),2/* Method name + Aux data */+nArg);` |
|     1300 |  7066 | `	if( aStack == 0 ){` |
|      ! 0 |  7067 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7068 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 |  7069 | `		return SXERR_MEM;` |
|        - |  7070 | `	}` |
|        - |  7071 | `	/* Fill the operand stack with the given arguments */` |
|     1872 |  7072 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      574 |  7073 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7074 | `		/*` |
|        - |  7075 | `		 * Symisc eXtension:` |
|        - |  7076 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7077 | `		 */` |
|      574 |  7078 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      288 |  7079 | `	}` |
|     1300 |  7080 | `	iCursor = nArg + 1;` |
|     1300 |  7081 | `	if( pThis ){` |
|        - |  7082 | `		/*` |
|        - |  7083 | `		 * Push the class instance so that the '$this' variable will be available.` |
|        - |  7084 | `		 */` |
|     1294 |  7085 | `		pThis->iRef++; /* Increment reference count */` |
|     1294 |  7086 | `		aStack[i].x.pOther = pThis;` |
|     1294 |  7087 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|      646 |  7088 | `	}` |
|     1300 |  7089 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|     1300 |  7090 | `	i++;` |
|        - |  7091 | `	/* Push method name */` |
|     1300 |  7092 | `	SyBlobReset(&aStack[i].sBlob);` |
|     1300 |  7093 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     1300 |  7094 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     1300 |  7095 | `	aStack[i].nIdx = SXU32_HIGH;` |
|        - |  7096 | `	/* Emit the CALL istruction */` |
|     1300 |  7097 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     1300 |  7098 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|     1300 |  7099 | `	aInstr[0].iP2 = 0;` |
|     1300 |  7100 | `	aInstr[0].p3  = 0;` |
|        - |  7101 | `	/* Emit the DONE instruction */` |
|     1300 |  7102 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     1300 |  7103 | `	aInstr[1].iP1 = 1;   /* Extract method return value */` |
|     1300 |  7104 | `	aInstr[1].iP2 = 0;` |
|     1300 |  7105 | `	aInstr[1].p3  = 0;` |
|        - |  7106 | `	/* Execute the method body (if available) */` |
|     1300 |  7107 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE);` |
|        - |  7108 | `	/* Clean up the mess left behind */` |
|     1300 |  7109 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     1300 |  7110 | `	return PH7_OK;` |
|      651 |  7111 |  |
|        - |  7112 | `/*` |
|        - |  7113 | ` * Call a user defined or foreign function where the name of the function` |
|        - |  7114 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - |  7115 | ` * in the apArg[] array.` |
|        - |  7116 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7117 | ` * return value indicates failure.` |
|        - |  7118 | ` */` |
|      926 |  7119 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - |  7120 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7121 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7122 | `	int nArg,          /* Total number of given arguments */` |
|        - |  7123 | `	ph7_value **apArg, /* Callback arguments */` |
|        - |  7124 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - |  7125 | `	)` |
|        2 |  7126 |  |
|        - |  7127 | `	ph7_value *aStack;` |
|        - |  7128 | `	VmInstr aInstr[2];` |
|        - |  7129 | `	int i;` |
|      928 |  7130 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - |  7131 | `		/* Don't bother processing,it's invalid anyway */` |
|      471 |  7132 | `		if( pResult ){` |
|        - |  7133 | `			/* Assume a null return value */` |
|      ! 0 |  7134 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7135 | `		}` |
|      471 |  7136 | `		return SXERR_INVALID;` |
|        - |  7137 | `	}` |
|      458 |  7138 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7139 | `		/* Class method */` |
|       11 |  7140 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 |  7141 | `		ph7_class_method *pMethod = 0;` |
|       11 |  7142 | `		ph7_class_instance *pThis = 0;` |
|       11 |  7143 | `		ph7_class *pClass = 0;` |
|        - |  7144 | `		ph7_value *pValue;` |
|        - |  7145 | `		sxi32 rc;` |
|       11 |  7146 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - |  7147 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 |  7148 | `			if( pResult ){` |
|        - |  7149 | `				/* Assume a null return value */` |
|      ! 0 |  7150 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7151 | `			}` |
|      ! 0 |  7152 | `			return SXRET_OK;` |
|        - |  7153 | `		}` |
|        - |  7154 | `		/* Extract the class name or an instance of it */` |
|       11 |  7155 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 |  7156 | `		if( pValue ){` |
|       11 |  7157 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 |  7158 | `		}` |
|       11 |  7159 | `		if( pClass == 0 ){` |
|        - |  7160 | `			/* No such class,return NULL */` |
|      ! 0 |  7161 | `			if( pResult ){` |
|      ! 0 |  7162 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7163 | `			}` |
|      ! 0 |  7164 | `			return SXRET_OK;` |
|        - |  7165 | `		}` |
|       11 |  7166 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  7167 | `			/* Point to the class instance */` |
|        5 |  7168 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 |  7169 | `		}` |
|        - |  7170 | `		/* Try to extract the method */` |
|       11 |  7171 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 |  7172 | `		if( pValue ){` |
|       11 |  7173 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 |  7174 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 |  7175 | `					SyBlobLength(&pValue->sBlob));` |
|        5 |  7176 | `			}` |
|        5 |  7177 | `		}` |
|       11 |  7178 | `		if( pMethod == 0 ){` |
|        - |  7179 | `			/* No such method,return NULL */` |
|      ! 0 |  7180 | `			if( pResult ){` |
|      ! 0 |  7181 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 |  7182 | `			}` |
|      ! 0 |  7183 | `			return SXRET_OK;` |
|        - |  7184 | `		}` |
|        - |  7185 | `		/* Call the class method */` |
|       11 |  7186 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 |  7187 | `		return rc;` |
|        - |  7188 | `	}` |
|        - |  7189 | `	/* Create a new operand stack */` |
|      448 |  7190 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      448 |  7191 | `	if( aStack == 0 ){` |
|      ! 0 |  7192 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7193 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 |  7194 | `		if( pResult ){` |
|        - |  7195 | `			/* Assume a null return value */` |
|      ! 0 |  7196 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 |  7197 | `		}` |
|      ! 0 |  7198 | `		return SXERR_MEM;` |
|        - |  7199 | `	}` |
|        - |  7200 | `	/* Fill the operand stack with the given arguments */` |
|     1470 |  7201 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1024 |  7202 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - |  7203 | `		/*` |
|        - |  7204 | `		 * Symisc eXtension:` |
|        - |  7205 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - |  7206 | `		 */` |
|     1024 |  7207 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      513 |  7208 | `	}` |
|        - |  7209 | `	/* Push the function name */` |
|      448 |  7210 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      448 |  7211 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - |  7212 | `	/* Emit the CALL istruction */` |
|      448 |  7213 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      448 |  7214 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      448 |  7215 | `	aInstr[0].iP2 = 0;` |
|      448 |  7216 | `	aInstr[0].p3  = 0;` |
|        - |  7217 | `	/* Emit the DONE instruction */` |
|      448 |  7218 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      448 |  7219 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      448 |  7220 | `	aInstr[1].iP2 = 0;` |
|      448 |  7221 | `	aInstr[1].p3  = 0;` |
|        - |  7222 | `	/* Execute the function body (if available) */` |
|      448 |  7223 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE);` |
|        - |  7224 | `	/* Clean up the mess left behind */` |
|      448 |  7225 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      448 |  7226 | `	return PH7_OK;` |
|      465 |  7227 |  |
|        - |  7228 | `/*` |
|        - |  7229 | ` * Call a user defined or foreign function whith a varibale number` |
|        - |  7230 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - |  7231 | ` * parameter.` |
|        - |  7232 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - |  7233 | ` * return value indicates failure.` |
|        - |  7234 | ` */` |
|      236 |  7235 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - |  7236 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  7237 | `	ph7_value *pFunc,  /* Callback name */` |
|        - |  7238 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - |  7239 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - |  7240 | `	)` |
|        1 |  7241 |  |
|        - |  7242 | `	ph7_value *pArg;` |
|        - |  7243 | `	SySet aArg;` |
|        - |  7244 | `	va_list ap;` |
|        - |  7245 | `	sxi32 rc;` |
|      237 |  7246 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - |  7247 | `	/* Copy arguments one after one */` |
|      237 |  7248 | `	va_start(ap,pResult);` |
|      393 |  7249 | `	for(;;){` |
|      787 |  7250 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 |  7251 | `		if( pArg == 0 ){` |
|      237 |  7252 | `			break;` |
|        - |  7253 | `		}` |
|      551 |  7254 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 |  7255 | `	}` |
|        - |  7256 | `	/* Call the core routine */` |
|      237 |  7257 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - |  7258 | `	/* Cleanup */` |
|      237 |  7259 | `	SySetRelease(&aArg);` |
|      237 |  7260 | `	return rc;` |
|        1 |  7261 |  |
|        - |  7262 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - |  7263 | `/*` |
|        - |  7264 | ` * bool defined(string $name)` |
|        - |  7265 | ` *  Checks whether a given named constant exists.` |
|        - |  7266 | ` * Parameter:` |
|        - |  7267 | ` *  Name of the desired constant.` |
|        - |  7268 | ` * Return` |
|        - |  7269 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - |  7270 | ` */` |
|       14 |  7271 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7272 |  |
|        - |  7273 | `	const char *zName;` |
|       16 |  7274 | `	int nLen = 0;` |
|       16 |  7275 | `	int res = 0;` |
|       16 |  7276 | `	if( nArg < 1 ){` |
|        - |  7277 | `		/* Missing constant name,return FALSE */` |
|      ! 0 |  7278 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 |  7279 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7280 | `		return SXRET_OK;` |
|        - |  7281 | `	}` |
|        - |  7282 | `	/* Extract constant name */` |
|       16 |  7283 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7284 | `	/* Perform the lookup */` |
|       16 |  7285 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - |  7286 | `		/* Already defined */` |
|       10 |  7287 | `		res = 1;` |
|        4 |  7288 | `	}` |
|       16 |  7289 | `	ph7_result_bool(pCtx,res);` |
|       16 |  7290 | `	return SXRET_OK;` |
|        9 |  7291 |  |
|        - |  7292 | `/*` |
|        - |  7293 | ` * Constant expansion callback used by the [define()] function defined` |
|        - |  7294 | ` * below.` |
|        - |  7295 | ` */` |
|        8 |  7296 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 |  7297 |  |
|       10 |  7298 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - |  7299 | `	/* Expand constant value */` |
|       10 |  7300 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       10 |  7301 |  |
|        - |  7302 | `/*` |
|        - |  7303 | ` * bool define(string $constant_name,expression value)` |
|        - |  7304 | ` *  Defines a named constant at runtime.` |
|        - |  7305 | ` * Parameter:` |
|        - |  7306 | ` *  $constant_name` |
|        - |  7307 | ` *   The name of the constant` |
|        - |  7308 | ` *  $value` |
|        - |  7309 | ` *   Constant value` |
|        - |  7310 | ` * Return:` |
|        - |  7311 | ` *   TRUE on success,FALSE on failure.` |
|        - |  7312 | ` */` |
|       10 |  7313 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7314 |  |
|        - |  7315 | `	const char *zName;  /* Constant name */` |
|        - |  7316 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       12 |  7317 | `	int nLen = 0;       /* Name length */` |
|        - |  7318 | `	sxi32 rc;` |
|       12 |  7319 | `	if( nArg < 2 ){` |
|        - |  7320 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 |  7321 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 |  7322 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7323 | `		return SXRET_OK;` |
|        - |  7324 | `	}` |
|       12 |  7325 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 |  7326 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 |  7327 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7328 | `		return SXRET_OK;` |
|        - |  7329 | `	}` |
|        - |  7330 | `	/* Extract constant name */` |
|       12 |  7331 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  7332 | `	if( nLen < 1 ){` |
|      ! 0 |  7333 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 |  7334 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7335 | `		return SXRET_OK;` |
|        - |  7336 | `	}` |
|        - |  7337 | `	/* Duplicate constant value */` |
|       12 |  7338 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       12 |  7339 | `	if( pValue == 0 ){` |
|      ! 0 |  7340 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7341 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7342 | `		return SXRET_OK;` |
|        - |  7343 | `	}` |
|        - |  7344 | `	/* Initialize the memory object */` |
|       12 |  7345 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - |  7346 | `	/* Register the constant */` |
|       12 |  7347 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       12 |  7348 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7349 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 |  7350 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 |  7351 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7352 | `		return SXRET_OK;` |
|        - |  7353 | `	}` |
|        - |  7354 | `	/* Duplicate constant value */` |
|       12 |  7355 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       12 |  7356 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - |  7357 | `		/* Lower case the constant name */` |
|      ! 0 |  7358 | `		char *zCur = (char *)zName;` |
|      ! 0 |  7359 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 |  7360 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - |  7361 | `				/* UTF-8 stream */` |
|      ! 0 |  7362 | `				zCur++;` |
|      ! 0 |  7363 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  7364 | `					zCur++;` |
|      ! 0 |  7365 | `				}` |
|      ! 0 |  7366 | `				continue;` |
|        - |  7367 | `			}` |
|      ! 0 |  7368 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 |  7369 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 |  7370 | `				zCur[0] = (char)c;` |
|      ! 0 |  7371 | `			}` |
|      ! 0 |  7372 | `			zCur++;` |
|      ! 0 |  7373 | `		}` |
|        - |  7374 | `		/* Finally,register the constant */` |
|      ! 0 |  7375 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 |  7376 | `	}` |
|        - |  7377 | `	/* All done,return TRUE */` |
|       12 |  7378 | `	ph7_result_bool(pCtx,1);` |
|       12 |  7379 | `	return SXRET_OK;` |
|        7 |  7380 |  |
|        - |  7381 | `/*` |
|        - |  7382 | ` * value constant(string $name)` |
|        - |  7383 | ` *  Returns the value of a constant` |
|        - |  7384 | ` * Parameter` |
|        - |  7385 | ` *  $name` |
|        - |  7386 | ` *    Name of the constant.` |
|        - |  7387 | ` * Return` |
|        - |  7388 | ` *  Constant value or NULL if not defined.` |
|        - |  7389 | ` */` |
|        8 |  7390 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7391 |  |
|        - |  7392 | `	SyHashEntry *pEntry;` |
|        - |  7393 | `	ph7_constant *pCons;` |
|        - |  7394 | `	const char *zName; /* Constant name */` |
|        - |  7395 | `	ph7_value sVal;    /* Constant value */` |
|        - |  7396 | `	int nLen;` |
|       10 |  7397 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  7398 | `		/* Invallid argument,return NULL */` |
|      ! 0 |  7399 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 |  7400 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7401 | `		return SXRET_OK;` |
|        - |  7402 | `	}` |
|        - |  7403 | `	/* Extract the constant name */` |
|       10 |  7404 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - |  7405 | `	/* Perform the query */` |
|       10 |  7406 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 |  7407 | `	if( pEntry == 0 ){` |
|        3 |  7408 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 |  7409 | `		ph7_result_null(pCtx);` |
|        3 |  7410 | `		return SXRET_OK;` |
|        - |  7411 | `	}` |
|        8 |  7412 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - |  7413 | `	/* Point to the structure that describe the constant */` |
|        8 |  7414 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - |  7415 | `	/* Extract constant value by calling it's associated callback */` |
|        8 |  7416 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - |  7417 | `	/* Return that value */` |
|        8 |  7418 | `	ph7_result_value(pCtx,&sVal);` |
|        - |  7419 | `	/* Cleanup */` |
|        8 |  7420 | `	PH7_MemObjRelease(&sVal);` |
|        8 |  7421 | `	return SXRET_OK;` |
|        6 |  7422 |  |
|        - |  7423 | `/*` |
|        - |  7424 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - |  7425 | ` * defined below.` |
|        - |  7426 | ` */` |
|      416 |  7427 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7428 |  |
|      417 |  7429 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - |  7430 | `	ph7_value sName;` |
|        - |  7431 | `	sxi32 rc;` |
|        - |  7432 | `	/* Prepare the constant name for insertion */` |
|      417 |  7433 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      417 |  7434 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - |  7435 | `	/* Perform the insertion */` |
|      417 |  7436 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      417 |  7437 | `	PH7_MemObjRelease(&sName);` |
|      417 |  7438 | `	return rc;` |
|        1 |  7439 |  |
|        - |  7440 | `/*` |
|        - |  7441 | ` * array get_defined_constants(void)` |
|        - |  7442 | ` *  Returns an associative array with the names of all defined` |
|        - |  7443 | ` *  constants.` |
|        - |  7444 | ` * Parameters` |
|        - |  7445 | ` *  NONE.` |
|        - |  7446 | ` * Returns` |
|        - |  7447 | ` *  Returns the names of all the constants currently defined.` |
|        - |  7448 | ` */` |
|        2 |  7449 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7450 |  |
|        - |  7451 | `	ph7_value *pArray;` |
|        - |  7452 | `	/* Create the array first*/` |
|        3 |  7453 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7454 | `	if( pArray == 0 ){` |
|      ! 0 |  7455 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7456 | `		SXUNUSED(apArg);` |
|        - |  7457 | `		/* Return NULL */` |
|      ! 0 |  7458 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7459 | `		return SXRET_OK;` |
|        - |  7460 | `	}` |
|        - |  7461 | `	/* Fill the array with the defined constants */` |
|        3 |  7462 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - |  7463 | `	/* Return the created array */` |
|        3 |  7464 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7465 | `	return SXRET_OK;` |
|        2 |  7466 |  |
|        - |  7467 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - |  7468 | `/*` |
|        - |  7469 | ` * Section:` |
|        - |  7470 | ` *  Random numbers/string generators.` |
|        - |  7471 | ` * Status:` |
|        - |  7472 | ` *    Stable.` |
|        - |  7473 | ` */` |
|        - |  7474 | `/*` |
|        - |  7475 | ` * Generate a random 32-bit unsigned integer.` |
|        - |  7476 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - |  7477 | ` * used by te SQLite3 library.` |
|        - |  7478 | ` */` |
|     2360 |  7479 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 |  7480 |  |
|        - |  7481 | `	sxu32 iNum;` |
|     2362 |  7482 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2362 |  7483 | `	return iNum;` |
|        2 |  7484 |  |
|        - |  7485 | `/*` |
|        - |  7486 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - |  7487 | ` * Note that the generated string is NOT null terminated.` |
|        - |  7488 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - |  7489 | ` * by te SQLite3 library.` |
|        - |  7490 | ` */` |
|    74522 |  7491 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 |  7492 |  |
|        - |  7493 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - |  7494 | `	int i;` |
|        - |  7495 | `	/* Generate a binary string first */` |
|    74524 |  7496 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - |  7497 | `	/* Turn the binary string into english based alphabet */` |
|   819912 |  7498 | `	for( i = 0 ; i < nLen ; ++i ){` |
|   745390 |  7499 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   372696 |  7500 | `	 }` |
|    74524 |  7501 |  |
|        - |  7502 | `/*` |
|        - |  7503 | ` * int rand()` |
|        - |  7504 | ` * int mt_rand()` |
|        - |  7505 | ` * int rand(int $min,int $max)` |
|        - |  7506 | ` * int mt_rand(int $min,int $max)` |
|        - |  7507 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - |  7508 | ` * Parameter` |
|        - |  7509 | ` *  $min` |
|        - |  7510 | ` *    The lowest value to return (default: 0)` |
|        - |  7511 | ` *  $max` |
|        - |  7512 | ` *   The highest value to return (default: getrandmax())` |
|        - |  7513 | ` * Return` |
|        - |  7514 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - |  7515 | ` * Note:` |
|        - |  7516 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7517 | ` *  by te SQLite3 library.` |
|        - |  7518 | ` */` |
|       20 |  7519 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7520 |  |
|        - |  7521 | `	sxu32 iNum;` |
|        - |  7522 | `	/* Generate the random number */` |
|       21 |  7523 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 |  7524 | `	if( nArg > 1 ){` |
|        - |  7525 | `		sxu32 iMin,iMax;` |
|        3 |  7526 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 |  7527 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 |  7528 | `		if( iMin < iMax ){` |
|        3 |  7529 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 |  7530 | `			if( iDiv > 0 ){` |
|        3 |  7531 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 |  7532 | `			}` |
|        1 |  7533 | `		}else if(iMax > 0 ){` |
|      ! 0 |  7534 | `			iNum %= iMax;` |
|      ! 0 |  7535 | `		}` |
|        1 |  7536 | `	}` |
|        - |  7537 | `	/* Return the number */` |
|       21 |  7538 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 |  7539 | `	return SXRET_OK;` |
|        1 |  7540 |  |
|        - |  7541 | `/*` |
|        - |  7542 | ` * int getrandmax(void)` |
|        - |  7543 | ` * int mt_getrandmax(void)` |
|        - |  7544 | ` * int rc4_getrandmax(void)` |
|        - |  7545 | ` *   Show largest possible random value` |
|        - |  7546 | ` * Return` |
|        - |  7547 | ` *  The largest possible random value returned by rand() which is in` |
|        - |  7548 | ` *  this implementation 0xFFFFFFFF.` |
|        - |  7549 | ` * Note:` |
|        - |  7550 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7551 | ` *  by te SQLite3 library.` |
|        - |  7552 | ` */` |
|        4 |  7553 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7554 |  |
|        2 |  7555 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 |  7556 | `	SXUNUSED(apArg);` |
|        5 |  7557 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 |  7558 | `	return SXRET_OK;` |
|        1 |  7559 |  |
|        - |  7560 | `/*` |
|        - |  7561 | ` * string rand_str()` |
|        - |  7562 | ` * string rand_str(int $len)` |
|        - |  7563 | ` *  Generate a random string (English alphabet).` |
|        - |  7564 | ` * Parameter` |
|        - |  7565 | ` *  $len` |
|        - |  7566 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - |  7567 | ` * Return` |
|        - |  7568 | ` *   A pseudo random string.` |
|        - |  7569 | ` * Note:` |
|        - |  7570 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - |  7571 | ` *  by te SQLite3 library.` |
|        - |  7572 | ` *  This function is a symisc extension.` |
|        - |  7573 | ` */` |
|      120 |  7574 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7575 |  |
|        - |  7576 | `	char zString[1024];` |
|      122 |  7577 | `	int iLen = 0x10;` |
|      122 |  7578 | `	if( nArg > 0 ){` |
|        - |  7579 | `		/* Get the desired length */` |
|      122 |  7580 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 |  7581 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - |  7582 | `			/* Default length */` |
|        3 |  7583 | `			iLen = 0x10;` |
|        1 |  7584 | `		}` |
|       60 |  7585 | `	}` |
|        - |  7586 | `	/* Generate the random string */` |
|      122 |  7587 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - |  7588 | `	/* Return the generated string */` |
|      122 |  7589 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 |  7590 | `	return SXRET_OK;` |
|        2 |  7591 |  |
|        - |  7592 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |  7593 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - |  7594 | `/* Unique ID private data */` |
|        - |  7595 | `struct unique_id_data` |
|        - |  7596 |  |
|        - |  7597 | `	ph7_context *pCtx; /* Call context */` |
|        - |  7598 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - |  7599 | `};` |
|        - |  7600 | `/*` |
|        - |  7601 | ` * Binary to hex consumer callback.` |
|        - |  7602 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - |  7603 | ` * defined below.` |
|        - |  7604 | ` */` |
|      192 |  7605 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 |  7606 |  |
|      193 |  7607 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - |  7608 | `	sxu32 nBuflen;` |
|        - |  7609 | `	/* Extract result buffer length */` |
|      193 |  7610 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 |  7611 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - |  7612 | `			/*` |
|        - |  7613 | `			 * If the more_entropy flag is not set,then the returned` |
|        - |  7614 | `			 * string will be 13 characters long` |
|        - |  7615 | `			 */` |
|       25 |  7616 | `		return SXERR_ABORT;` |
|        - |  7617 | `	}` |
|      169 |  7618 | `	if( nBuflen > 22 ){` |
|      ! 0 |  7619 | `		return SXERR_ABORT;` |
|        - |  7620 | `	}` |
|        - |  7621 | `	/* Safely Consume the hex stream */` |
|      169 |  7622 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 |  7623 | `	return SXRET_OK;` |
|       97 |  7624 |  |
|        - |  7625 | `/*` |
|        - |  7626 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - |  7627 | ` *  Generate a unique ID` |
|        - |  7628 | ` * Parameter` |
|        - |  7629 | ` * $prefix` |
|        - |  7630 | ` *  Append this prefix to the generated unique ID.` |
|        - |  7631 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - |  7632 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - |  7633 | ` * $more_entropy` |
|        - |  7634 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - |  7635 | ` *  that the result will be unique.` |
|        - |  7636 | ` * Return` |
|        - |  7637 | ` *  Returns the unique identifier, as a string.` |
|        - |  7638 | ` */` |
|       24 |  7639 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7640 |  |
|        - |  7641 | `	struct unique_id_data sUniq;` |
|        - |  7642 | `	unsigned char zDigest[20];` |
|       25 |  7643 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7644 | `	const char *zPrefix;` |
|        - |  7645 | `	SHA1Context sCtx;` |
|        - |  7646 | `	char zRandom[7];` |
|        - |  7647 | `	int nPrefix;` |
|        - |  7648 | `	int entropy;` |
|        - |  7649 | `	/* Generate a random string first */` |
|       25 |  7650 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - |  7651 | `	/* Initialize fields */` |
|       25 |  7652 | `	zPrefix = 0;` |
|       25 |  7653 | `	nPrefix = 0;` |
|       25 |  7654 | `	entropy = 0;` |
|       25 |  7655 | `	if( nArg > 0 ){` |
|        - |  7656 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 |  7657 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 |  7658 | `		if( nArg > 1 ){` |
|      ! 0 |  7659 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 |  7660 | `		}` |
|      ! 0 |  7661 | `	}` |
|       25 |  7662 | `	SHA1Init(&sCtx);` |
|        - |  7663 | `	/* Generate the random ID */` |
|       25 |  7664 | `	if( nPrefix > 0 ){` |
|      ! 0 |  7665 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 |  7666 | `	}` |
|        - |  7667 | `	/* Append the random ID */` |
|       25 |  7668 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - |  7669 | `	/* Append the random string */` |
|       25 |  7670 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - |  7671 | `	/* Increment the number */` |
|       25 |  7672 | `	pVm->unique_id++;` |
|       25 |  7673 | `	SHA1Final(&sCtx,zDigest);` |
|        - |  7674 | `	/* Hexify the digest */` |
|       25 |  7675 | `	sUniq.pCtx = pCtx;` |
|       25 |  7676 | `	sUniq.entropy = entropy;` |
|       25 |  7677 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - |  7678 | `	/* All done */` |
|       25 |  7679 | `	return PH7_OK;` |
|        1 |  7680 |  |
|        - |  7681 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - |  7682 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |  7683 | `/*` |
|        - |  7684 | ` * Section:` |
|        - |  7685 | ` *  Language construct implementation as foreign functions.` |
|        - |  7686 | ` * Status:` |
|        - |  7687 | ` *    Stable.` |
|        - |  7688 | ` */` |
|        - |  7689 | `/*` |
|        - |  7690 | ` * void echo($string...)` |
|        - |  7691 | ` *  Output one or more messages.` |
|        - |  7692 | ` * Parameters` |
|        - |  7693 | ` *  $string` |
|        - |  7694 | ` *   Message to output.` |
|        - |  7695 | ` * Return` |
|        - |  7696 | ` *  NULL.` |
|        - |  7697 | ` */` |
|      ! 0 |  7698 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7699 |  |
|        - |  7700 | `	const char *zData;` |
|      ! 0 |  7701 | `	int nDataLen = 0;` |
|        - |  7702 | `	ph7_vm *pVm;` |
|        - |  7703 | `	int i,rc;` |
|        - |  7704 | `	/* Point to the target VM */` |
|      ! 0 |  7705 | `	pVm = pCtx->pVm;` |
|        - |  7706 | `	/* Output */` |
|      ! 0 |  7707 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 |  7708 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 |  7709 | `		if( nDataLen > 0 ){` |
|      ! 0 |  7710 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 |  7711 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7712 | `				/* Increment output length */` |
|      ! 0 |  7713 | `				pVm->nOutputLen += nDataLen;` |
|      ! 0 |  7714 | `			}` |
|      ! 0 |  7715 | `			if( rc == SXERR_ABORT ){` |
|        - |  7716 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7717 | `				return PH7_ABORT;` |
|        - |  7718 | `			}` |
|      ! 0 |  7719 | `		}` |
|      ! 0 |  7720 | `	}` |
|      ! 0 |  7721 | `	return SXRET_OK;` |
|      ! 0 |  7722 |  |
|        - |  7723 | `/*` |
|        - |  7724 | ` * int print($string...)` |
|        - |  7725 | ` *  Output one or more messages.` |
|        - |  7726 | ` * Parameters` |
|        - |  7727 | ` *  $string` |
|        - |  7728 | ` *   Message to output.` |
|        - |  7729 | ` * Return` |
|        - |  7730 | ` *  1 always.` |
|        - |  7731 | ` */` |
|        2 |  7732 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7733 |  |
|        - |  7734 | `	const char *zData;` |
|        3 |  7735 | `	int nDataLen = 0;` |
|        - |  7736 | `	ph7_vm *pVm;` |
|        - |  7737 | `	int i,rc;` |
|        - |  7738 | `	/* Point to the target VM */` |
|        3 |  7739 | `	pVm = pCtx->pVm;` |
|        - |  7740 | `	/* Output */` |
|        5 |  7741 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 |  7742 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 |  7743 | `		if( nDataLen > 0 ){` |
|        3 |  7744 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 |  7745 | `			if( pVm->sVmConsumer.xConsumer != VmObConsumer ){` |
|        - |  7746 | `				/* Increment output length */` |
|        3 |  7747 | `				pVm->nOutputLen += nDataLen;` |
|        1 |  7748 | `			}` |
|        3 |  7749 | `			if( rc == SXERR_ABORT ){` |
|        - |  7750 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 |  7751 | `				return PH7_ABORT;` |
|        - |  7752 | `			}` |
|        1 |  7753 | `		}` |
|        2 |  7754 | `	}` |
|        - |  7755 | `	/* Return 1 */` |
|        3 |  7756 | `	ph7_result_int(pCtx,1);` |
|        3 |  7757 | `	return SXRET_OK;` |
|        2 |  7758 |  |
|        - |  7759 | `/*` |
|        - |  7760 | ` * void exit(string $msg)` |
|        - |  7761 | ` * void exit(int $status)` |
|        - |  7762 | ` * void die(string $ms)` |
|        - |  7763 | ` * void die(int $status)` |
|        - |  7764 | ` *   Output a message and terminate program execution.` |
|        - |  7765 | ` * Parameter` |
|        - |  7766 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - |  7767 | ` *  If status is an integer, that value will be used as the exit status` |
|        - |  7768 | ` *  and not printed` |
|        - |  7769 | ` * Return` |
|        - |  7770 | ` *  NULL` |
|        - |  7771 | ` */` |
|      ! 0 |  7772 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 |  7773 |  |
|      ! 0 |  7774 | `	if( nArg > 0 ){` |
|      ! 0 |  7775 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - |  7776 | `			const char *zData;` |
|      ! 0 |  7777 | `			int iLen = 0;` |
|        - |  7778 | `			/* Print exit message */` |
|      ! 0 |  7779 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 |  7780 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 |  7781 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - |  7782 | `			sxi32 iExitStatus;` |
|        - |  7783 | `			/* Record exit status code */` |
|      ! 0 |  7784 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 |  7785 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 |  7786 | `		}` |
|      ! 0 |  7787 | `	}` |
|        - |  7788 | `	/* Check if we are in an included file */` |
|      ! 0 |  7789 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - |  7790 | `		/* Exit the entire process */` |
|      ! 0 |  7791 | `		exit(pCtx->pVm->iExitStatus);` |
|        - |  7792 | `	}` |
|        - |  7793 | `	/* Abort processing immediately */` |
|      ! 0 |  7794 | `	return PH7_ABORT;` |
|      ! 0 |  7795 |  |
|        - |  7796 | `/*` |
|        - |  7797 | ` * bool isset($var,...)` |
|        - |  7798 | ` *  Finds out whether a variable is set.` |
|        - |  7799 | ` * Parameters` |
|        - |  7800 | ` *  One or more variable to check.` |
|        - |  7801 | ` * Return` |
|        - |  7802 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - |  7803 | ` */` |
|    71378 |  7804 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7805 |  |
|        - |  7806 | `	ph7_value *pObj;` |
|    71380 |  7807 | `	int res = 0;` |
|        - |  7808 | `	int i;` |
|    71380 |  7809 | `	if( nArg < 1 ){` |
|        - |  7810 | `		/* Missing arguments,return false */` |
|      ! 0 |  7811 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 |  7812 | `		return SXRET_OK;` |
|        - |  7813 | `	}` |
|        - |  7814 | `	/* Iterate over available arguments */` |
|    94232 |  7815 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    71380 |  7816 | `		pObj = apArg[i];` |
|    71380 |  7817 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    48020 |  7818 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7819 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 |  7820 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 |  7821 | `			}` |
|    24009 |  7822 | `		}` |
|    71380 |  7823 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    71380 |  7824 | `		if( !res ){` |
|        - |  7825 | `			/* Variable not set,return FALSE */` |
|    48528 |  7826 | `			ph7_result_bool(pCtx,0);` |
|    48528 |  7827 | `			return SXRET_OK;` |
|        - |  7828 | `		}` |
|    11428 |  7829 | `	}` |
|        - |  7830 | `	/* All given variable are set,return TRUE */` |
|    22854 |  7831 | `	ph7_result_bool(pCtx,1);` |
|    22854 |  7832 | `	return SXRET_OK;` |
|    35691 |  7833 |  |
|        - |  7834 | `/*` |
|        - |  7835 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - |  7836 | ` * frame,the reference table and discard it's contents.` |
|        - |  7837 | ` * This function never fail and always return SXRET_OK.` |
|        - |  7838 | ` */` |
|  2964008 |  7839 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 |  7840 |  |
|        - |  7841 | `	ph7_value *pObj;` |
|        - |  7842 | `	VmRefObj *pRef;` |
|  2964010 |  7843 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  2964010 |  7844 | `	if( pObj ){` |
|        - |  7845 | `		/* Release the object */` |
|  2964010 |  7846 | `		PH7_MemObjRelease(pObj);` |
|  1482004 |  7847 | `	}` |
|        - |  7848 | `	/* Remove old reference links */` |
|  2964010 |  7849 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  2964010 |  7850 | `	if( pRef ){` |
|  2963990 |  7851 | `		sxi32 iFlags = pRef->iFlags;` |
|        - |  7852 | `		/* Unlink from the reference table */` |
|  2963990 |  7853 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  2963990 |  7854 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - |  7855 | `			VmSlot sFree;` |
|        - |  7856 | `			/* Restore to the free list */` |
|  2963984 |  7857 | `			sFree.nIdx = nObjIdx;` |
|  2963984 |  7858 | `			sFree.pUserData = 0;` |
|  2963984 |  7859 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1481991 |  7860 | `		}` |
|  1481994 |  7861 | `	}` |
|  2964010 |  7862 | `	return SXRET_OK;` |
|        2 |  7863 |  |
|        - |  7864 | `/*` |
|        - |  7865 | ` * void unset($var,...)` |
|        - |  7866 | ` *   Unset one or more given variable.` |
|        - |  7867 | ` * Parameters` |
|        - |  7868 | ` *  One or more variable to unset.` |
|        - |  7869 | ` * Return` |
|        - |  7870 | ` *  Nothing.` |
|        - |  7871 | ` */` |
|     3260 |  7872 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7873 |  |
|        - |  7874 | `	ph7_value *pObj;` |
|        - |  7875 | `	ph7_vm *pVm;` |
|        - |  7876 | `	int i;` |
|        - |  7877 | `	/* Point to the target VM */` |
|     3262 |  7878 | `	pVm = pCtx->pVm;` |
|        - |  7879 | `	/* Iterate and unset */` |
|     9666 |  7880 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     6406 |  7881 | `		pObj = apArg[i];` |
|     6406 |  7882 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      868 |  7883 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  7884 | `				/* Throw an error */` |
|      ! 0 |  7885 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 |  7886 | `			}` |
|      435 |  7887 | `		}else{` |
|     5540 |  7888 | `			sxu32 nIdx = pObj->nIdx;` |
|        - |  7889 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     5540 |  7890 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     5534 |  7891 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     2766 |  7892 | `			}` |
|        - |  7893 | `		}` |
|     3204 |  7894 | `	}` |
|     3262 |  7895 | `	return SXRET_OK;` |
|        2 |  7896 |  |
|        - |  7897 | `/*` |
|        - |  7898 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - |  7899 | ` */` |
|      110 |  7900 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 |  7901 |  |
|      111 |  7902 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 |  7903 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  7904 | `	ph7_value *pObj;` |
|        - |  7905 | `	sxu32 nIdx;` |
|        - |  7906 | `	/* Extract the memory object */` |
|      111 |  7907 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 |  7908 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 |  7909 | `	if( pObj ){` |
|      111 |  7910 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 |  7911 | `			if( pEntry->nKeyLen > 0 ){` |
|        - |  7912 | `				SyString sName;` |
|        - |  7913 | `				ph7_value sKey;` |
|        - |  7914 | `				/* Perform the insertion */` |
|      109 |  7915 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 |  7916 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 |  7917 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 |  7918 | `				PH7_MemObjRelease(&sKey);` |
|       54 |  7919 | `			}` |
|       54 |  7920 | `		}` |
|       55 |  7921 | `	}` |
|      111 |  7922 | `	return SXRET_OK;` |
|        1 |  7923 |  |
|        - |  7924 | `/*` |
|        - |  7925 | ` * array get_defined_vars(void)` |
|        - |  7926 | ` *  Returns an array of all defined variables.` |
|        - |  7927 | ` * Parameter` |
|        - |  7928 | ` *  None` |
|        - |  7929 | ` * Return` |
|        - |  7930 | ` *  An array with all the variables defined in the current scope.` |
|        - |  7931 | ` */` |
|        2 |  7932 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7933 |  |
|        3 |  7934 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  7935 | `	ph7_value *pArray;` |
|        - |  7936 | `	/* Create a new array */` |
|        3 |  7937 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  7938 | ` 	if( pArray == 0 ){` |
|      ! 0 |  7939 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  7940 | `		SXUNUSED(apArg);` |
|        - |  7941 | `		/* Return NULL */` |
|      ! 0 |  7942 | `		ph7_result_null(pCtx);` |
|      ! 0 |  7943 | `		return SXRET_OK;` |
|        - |  7944 | `	}` |
|        - |  7945 | `	/* Superglobals first */` |
|        3 |  7946 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - |  7947 | `	/* Then variable defined in the current frame */` |
|        3 |  7948 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - |  7949 | `	/* Finally,return the created array */` |
|        3 |  7950 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  7951 | `	return SXRET_OK;` |
|        2 |  7952 |  |
|        - |  7953 | `/*` |
|        - |  7954 | ` * bool gettype($var)` |
|        - |  7955 | ` *  Get the type of a variable` |
|        - |  7956 | ` * Parameters` |
|        - |  7957 | ` *   $var` |
|        - |  7958 | ` *    The variable being type checked.` |
|        - |  7959 | ` * Return` |
|        - |  7960 | ` *   String representation of the given variable type.` |
|        - |  7961 | ` */` |
|       32 |  7962 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  7963 |  |
|       34 |  7964 | `	const char *zType = "Empty";` |
|       34 |  7965 | `	if( nArg > 0 ){` |
|       34 |  7966 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 |  7967 | `	}` |
|        - |  7968 | `	/* Return the variable type */` |
|       34 |  7969 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 |  7970 | `	return SXRET_OK;` |
|        2 |  7971 |  |
|        - |  7972 | `/*` |
|        - |  7973 | ` * string get_resource_type(resource $handle)` |
|        - |  7974 | ` *  This function gets the type of the given resource.` |
|        - |  7975 | ` * Parameters` |
|        - |  7976 | ` *  $handle` |
|        - |  7977 | ` *  The evaluated resource handle.` |
|        - |  7978 | ` * Return` |
|        - |  7979 | ` *  If the given handle is a resource, this function will return a string` |
|        - |  7980 | ` *  representing its type. If the type is not identified by this function` |
|        - |  7981 | ` *  the return value will be the string Unknown.` |
|        - |  7982 | ` *  This function will return FALSE and generate an error if handle` |
|        - |  7983 | ` *  is not a resource.` |
|        - |  7984 | ` */` |
|        2 |  7985 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  7986 |  |
|        3 |  7987 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - |  7988 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 |  7989 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  7990 | `		return PH7_OK;` |
|        - |  7991 | `	}` |
|        3 |  7992 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 |  7993 | `	return SXRET_OK;` |
|        2 |  7994 |  |
|        - |  7995 | `/*` |
|        - |  7996 | ` * void var_dump(expression,....)` |
|        - |  7997 | ` *   var_dump � Dumps information about a variable` |
|        - |  7998 | ` * Parameters` |
|        - |  7999 | ` *   One or more expression to dump.` |
|        - |  8000 | ` * Returns` |
|        - |  8001 | ` *  Nothing.` |
|        - |  8002 | ` */` |
|      218 |  8003 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8004 |  |
|        - |  8005 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - |  8006 | `	int i;` |
|      220 |  8007 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - |  8008 | `	/* Dump one or more expressions */` |
|      444 |  8009 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 |  8010 | `		ph7_value *pObj = apArg[i];` |
|        - |  8011 | `		/* Reset the working buffer */` |
|      226 |  8012 | `		SyBlobReset(&sDump);` |
|        - |  8013 | `		/* Dump the given expression */` |
|      226 |  8014 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - |  8015 | `		/* Output */` |
|      226 |  8016 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 |  8017 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 |  8018 | `		}` |
|      114 |  8019 | `	}` |
|        - |  8020 | `	/* Release the working buffer */` |
|      220 |  8021 | `	SyBlobRelease(&sDump);` |
|      220 |  8022 | `	return SXRET_OK;` |
|        2 |  8023 |  |
|        - |  8024 | `/*` |
|        - |  8025 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - |  8026 | ` *   print-r - Prints human-readable information about a variable` |
|        - |  8027 | ` * Parameters` |
|        - |  8028 | ` *   expression: Expression to dump` |
|        - |  8029 | ` *   return : If you would like to capture the output of print_r() use` |
|        - |  8030 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - |  8031 | ` *            print_r() will return the information rather than print it.` |
|        - |  8032 | ` * Return` |
|        - |  8033 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - |  8034 | ` *  Otherwise, the return value is TRUE.` |
|        - |  8035 | ` */` |
|       16 |  8036 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8037 |  |
|       17 |  8038 | `	int ret_string = 0;` |
|        - |  8039 | `	SyBlob sDump;` |
|       17 |  8040 | `	if( nArg < 1 ){` |
|        - |  8041 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8042 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8043 | `		return SXRET_OK;` |
|        - |  8044 | `	}` |
|       17 |  8045 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 |  8046 | `	if ( nArg > 1 ){` |
|        - |  8047 | `		/* Where to redirect output */` |
|       11 |  8048 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 |  8049 | `	}` |
|        - |  8050 | `	/* Generate dump */` |
|       17 |  8051 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 |  8052 | `	if( !ret_string ){` |
|        - |  8053 | `		/* Output dump */` |
|        7 |  8054 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8055 | `		/* Return true */` |
|        7 |  8056 | `		ph7_result_bool(pCtx,1);` |
|        4 |  8057 | `	}else{` |
|        - |  8058 | `		/* Generated dump as return value */` |
|       11 |  8059 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8060 | `	}` |
|        - |  8061 | `	/* Release the working buffer */` |
|       17 |  8062 | `	SyBlobRelease(&sDump);` |
|       17 |  8063 | `	return SXRET_OK;` |
|        9 |  8064 |  |
|        - |  8065 | `/*` |
|        - |  8066 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - |  8067 | ` * Same job as print_r. (see coment above)` |
|        - |  8068 | ` */` |
|        2 |  8069 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8070 |  |
|        3 |  8071 | `	int ret_string = 0;` |
|        - |  8072 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 |  8073 | `	if( nArg < 1 ){` |
|        - |  8074 | `		/* Nothing to output,return FALSE */` |
|      ! 0 |  8075 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8076 | `		return SXRET_OK;` |
|        - |  8077 | `	}` |
|        3 |  8078 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 |  8079 | `	if ( nArg > 1 ){` |
|        - |  8080 | `		/* Where to redirect output */` |
|        3 |  8081 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 |  8082 | `	}` |
|        - |  8083 | `	/* Generate dump */` |
|        3 |  8084 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 |  8085 | `	if( !ret_string ){` |
|        - |  8086 | `		/* Output dump */` |
|      ! 0 |  8087 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8088 | `		/* Return NULL */` |
|      ! 0 |  8089 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8090 | `	}else{` |
|        - |  8091 | `		/* Generated dump as return value */` |
|        3 |  8092 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8093 | `	}` |
|        - |  8094 | `	/* Release the working buffer */` |
|        3 |  8095 | `	SyBlobRelease(&sDump);` |
|        3 |  8096 | `	return SXRET_OK;` |
|        2 |  8097 |  |
|        - |  8098 | `/*` |
|        - |  8099 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - |  8100 | ` *  Set/get the various assert flags.` |
|        - |  8101 | ` * Parameter` |
|        - |  8102 | ` * $what` |
|        - |  8103 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - |  8104 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - |  8105 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - |  8106 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - |  8107 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - |  8108 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - |  8109 | ` * $value` |
|        - |  8110 | ` *   An optional new value for the option.` |
|        - |  8111 | ` * Return` |
|        - |  8112 | ` *  Old setting on success or FALSE on failure.` |
|        - |  8113 | ` */` |
|       30 |  8114 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8115 |  |
|       32 |  8116 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8117 | `	int iOption;` |
|        - |  8118 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       32 |  8119 | `	if( nArg < 1 ){` |
|        3 |  8120 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8121 | `			"ArgumentCountError",` |
|        - |  8122 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - |  8123 | `			);` |
|        - |  8124 | `	}` |
|        - |  8125 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       28 |  8126 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       30 |  8127 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 |  8128 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8129 | `			"TypeError",` |
|        - |  8130 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 |  8131 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 |  8132 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - |  8133 | `			);` |
|        - |  8134 | `	}` |
|       30 |  8135 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - |  8136 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - |  8137 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - |  8138 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       30 |  8139 | `	switch( iOption ){` |
|        6 |  8140 | `	case 1: /* ASSERT_ACTIVE */` |
|        - |  8141 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       14 |  8142 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       14 |  8143 | `		if( nArg > 1 ){` |
|        5 |  8144 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8145 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 |  8146 | `			}else{` |
|        3 |  8147 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - |  8148 | `			}` |
|        2 |  8149 | `		}` |
|       14 |  8150 | `		break;` |
|        1 |  8151 | `	case 2: /* ASSERT_CALLBACK */` |
|        - |  8152 | `		/* Return old callback or null */` |
|        3 |  8153 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 |  8154 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 |  8155 | `		}else{` |
|        3 |  8156 | `			ph7_result_null(pCtx);` |
|        - |  8157 | `		}` |
|        3 |  8158 | `		if( nArg > 1 ){` |
|      ! 0 |  8159 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 |  8160 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 |  8161 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 |  8162 | `			}else{` |
|      ! 0 |  8163 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - |  8164 | `			}` |
|      ! 0 |  8165 | `		}` |
|        3 |  8166 | `		break;` |
|        5 |  8167 | `	case 3: /* ASSERT_BAIL */` |
|       11 |  8168 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 |  8169 | `		if( nArg > 1 ){` |
|        5 |  8170 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 |  8171 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 |  8172 | `			}else{` |
|        3 |  8173 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - |  8174 | `			}` |
|        2 |  8175 | `		}` |
|       11 |  8176 | `		break;` |
|      ! 0 |  8177 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 |  8178 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8179 | `		break;` |
|        1 |  8180 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 |  8181 | `		ph7_result_int(pCtx, 1);` |
|        3 |  8182 | `		break;` |
|      ! 0 |  8183 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 |  8184 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 |  8185 | `		break;` |
|        1 |  8186 | `	default:` |
|        - |  8187 | `		/* PHP 8: ValueError for invalid option */` |
|        3 |  8188 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8189 | `			"ValueError",` |
|        - |  8190 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - |  8191 | `			);` |
|        - |  8192 | `	}` |
|       28 |  8193 | `	return PH7_OK;` |
|       17 |  8194 |  |
|        - |  8195 | `/*` |
|        - |  8196 | ` * bool assert(mixed $assertion)` |
|        - |  8197 | ` *  Checks if assertion is FALSE.` |
|        - |  8198 | ` * Parameter` |
|        - |  8199 | ` *  $assertion` |
|        - |  8200 | ` *    The assertion to test.` |
|        - |  8201 | ` * Return` |
|        - |  8202 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - |  8203 | ` */` |
|       26 |  8204 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8205 |  |
|       28 |  8206 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8207 | `	int iFlags,iResult;` |
|        - |  8208 | `	const char *zDesc;` |
|        - |  8209 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       28 |  8210 | `	if( nArg < 1 ){` |
|        3 |  8211 | `		return PH7_VmThrowException(pCtx,` |
|        - |  8212 | `			"ArgumentCountError",` |
|        - |  8213 | `			"assert() expects at least 1 argument, 0 given"` |
|        - |  8214 | `			);` |
|        - |  8215 | `	}` |
|       26 |  8216 | `	iFlags = pVm->iAssertFlags;` |
|       26 |  8217 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - |  8218 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 |  8219 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 |  8220 | `		return PH7_OK;` |
|        - |  8221 | `	}` |
|        - |  8222 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       26 |  8223 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       26 |  8224 | `	if( !iResult ){` |
|        - |  8225 | `		/* Assertion failed */` |
|        - |  8226 | `		/* Extract optional description */` |
|       13 |  8227 | `		zDesc = 0;` |
|       13 |  8228 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  8229 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 |  8230 | `		}` |
|       13 |  8231 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - |  8232 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - |  8233 | `			ph7_value sFile,sLine;` |
|        - |  8234 | `			ph7_value *apCbArg[3];` |
|        - |  8235 | `			SyString *pFile;` |
|        - |  8236 | `			/* Extract the processed script */` |
|      ! 0 |  8237 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 |  8238 | `			if( pFile == 0 ){` |
|      ! 0 |  8239 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 |  8240 | `			}` |
|        - |  8241 | `			/* Invoke the callback */` |
|      ! 0 |  8242 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 |  8243 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 |  8244 | `			apCbArg[0] = &sFile;` |
|      ! 0 |  8245 | `			apCbArg[1] = &sLine;` |
|      ! 0 |  8246 | `			apCbArg[2] = apArg[0];` |
|      ! 0 |  8247 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - |  8248 | `			/* Clean-up the mess left behind */` |
|      ! 0 |  8249 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 |  8250 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 |  8251 | `		}` |
|       13 |  8252 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - |  8253 | `			/* Abort VM execution immediately */` |
|      ! 0 |  8254 | `			return PH7_ABORT;` |
|        - |  8255 | `		}` |
|        - |  8256 | `		/* PHP 8: throw AssertionError by default */` |
|       13 |  8257 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 |  8258 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8259 | `				"AssertionError",` |
|        - |  8260 | `				"%s",` |
|        1 |  8261 | `				zDesc` |
|        - |  8262 | `				);` |
|      ! 0 |  8263 | `		}else{` |
|       11 |  8264 | `			return PH7_VmThrowException(pCtx,` |
|        - |  8265 | `				"AssertionError",` |
|        - |  8266 | `				"assert(false)"` |
|        - |  8267 | `				);` |
|        - |  8268 | `		}` |
|        - |  8269 | `	}` |
|        - |  8270 | `	/* Assertion passed */` |
|       14 |  8271 | `	ph7_result_bool(pCtx,1);` |
|       14 |  8272 | `	return PH7_OK;` |
|       15 |  8273 |  |
|        - |  8274 | `/*` |
|        - |  8275 | ` * Section:` |
|        - |  8276 | ` *  Error reporting functions.` |
|        - |  8277 | ` * Status:` |
|        - |  8278 | ` *    Stable.` |
|        - |  8279 | ` */` |
|        - |  8280 | `/*` |
|        - |  8281 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - |  8282 | ` *  Generates a user-level error/warning/notice message.` |
|        - |  8283 | ` * Parameters` |
|        - |  8284 | ` *  $error_msg` |
|        - |  8285 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - |  8286 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - |  8287 | ` * $error_type` |
|        - |  8288 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - |  8289 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - |  8290 | ` * Return` |
|        - |  8291 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - |  8292 | ` */` |
|       12 |  8293 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8294 |  |
|       14 |  8295 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 |  8296 | `	int rc = PH7_OK;` |
|       14 |  8297 | `	if( nArg > 0 ){` |
|        - |  8298 | `		const char *zErr;` |
|        - |  8299 | `		int nLen;` |
|        - |  8300 | `		/* Extract the error message */` |
|       12 |  8301 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 |  8302 | `		if( nArg > 1 ){` |
|        - |  8303 | `			/* Extract the error type */` |
|       12 |  8304 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 |  8305 | `			switch( nErr ){` |
|        1 |  8306 | `			case 1:   /* E_ERROR */` |
|        - |  8307 | `			case 16:  /* E_CORE_ERROR */` |
|        - |  8308 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - |  8309 | `			case 256: /* E_USER_ERROR */` |
|        3 |  8310 | `				nErr = PH7_CTX_ERR;` |
|        3 |  8311 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 |  8312 | `				break;` |
|        1 |  8313 | `			case 2:   /* E_WARNING */` |
|        - |  8314 | `			case 32:  /* E_CORE_WARNING */` |
|        - |  8315 | `			case 123: /* E_COMPILE_WARNING */` |
|        - |  8316 | `			case 512: /* E_USER_WARNING */` |
|        3 |  8317 | `				nErr = PH7_CTX_WARNING;` |
|        3 |  8318 | `				break;` |
|        3 |  8319 | `			default:` |
|        8 |  8320 | `				nErr = PH7_CTX_NOTICE;` |
|        6 |  8321 | `				break;` |
|        - |  8322 | `			}` |
|        5 |  8323 | `		}` |
|        - |  8324 | `		/* Report error */` |
|       12 |  8325 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 |  8326 | `		if( rc == PH7_ABORT ){` |
|      ! 0 |  8327 | `			return rc;` |
|        - |  8328 | `		}` |
|        - |  8329 | `		/* Return true */` |
|       12 |  8330 | `		ph7_result_bool(pCtx,1);` |
|        7 |  8331 | `	}else{` |
|        - |  8332 | `		/* Missing arguments,return FALSE */` |
|        3 |  8333 | `		ph7_result_bool(pCtx,0);` |
|        - |  8334 | `	}` |
|       14 |  8335 | `	return rc;` |
|        8 |  8336 |  |
|        - |  8337 | `/*` |
|        - |  8338 | ` * int error_reporting([int $level])` |
|        - |  8339 | ` *  Sets which PHP errors are reported.` |
|        - |  8340 | ` * Parameters` |
|        - |  8341 | ` *  $level` |
|        - |  8342 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - |  8343 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - |  8344 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - |  8345 | ` *   levels will not always behave as expected.` |
|        - |  8346 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - |  8347 | ` *   in the predefined constants.` |
|        - |  8348 | ` * Return` |
|        - |  8349 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - |  8350 | ` *   parameter is given.` |
|        - |  8351 | ` */` |
|       40 |  8352 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8353 |  |
|       42 |  8354 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8355 | `	int nOld;` |
|        - |  8356 | `	/* Extract the old reporting level */` |
|       42 |  8357 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       42 |  8358 | `	if( nArg > 0 ){` |
|        - |  8359 | `		int nNew;` |
|        - |  8360 | `		/* Extract the desired error reporting level */` |
|       34 |  8361 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       34 |  8362 | `		if( !nNew ){` |
|        - |  8363 | `			/* Do not report errors at all */` |
|        5 |  8364 | `			pVm->bErrReport = 0;` |
|        3 |  8365 | `		}else{` |
|        - |  8366 | `			/* Report all errors */` |
|       30 |  8367 | `			pVm->bErrReport = 1;` |
|        - |  8368 | `		}` |
|       16 |  8369 | `	}` |
|        - |  8370 | `	/* Return the old level */` |
|       42 |  8371 | `	ph7_result_int(pCtx,nOld);` |
|       42 |  8372 | `	return PH7_OK;` |
|        2 |  8373 |  |
|        - |  8374 | `/*` |
|        - |  8375 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - |  8376 | ` *  Send an error message somewhere.` |
|        - |  8377 | ` * Parameter` |
|        - |  8378 | ` *  $message` |
|        - |  8379 | ` *   The error message that should be logged.` |
|        - |  8380 | ` *  $message_type` |
|        - |  8381 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - |  8382 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - |  8383 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - |  8384 | ` *       This is the default option.` |
|        - |  8385 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - |  8386 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - |  8387 | ` *    2  No longer an option.` |
|        - |  8388 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - |  8389 | ` *       to the end of the message string.` |
|        - |  8390 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - |  8391 | ` *  $destination` |
|        - |  8392 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - |  8393 | ` *  $extra_headers` |
|        - |  8394 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - |  8395 | ` * Return` |
|        - |  8396 | ` *  TRUE on success or FALSE on failure.` |
|        - |  8397 | ` * NOTE:` |
|        - |  8398 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - |  8399 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - |  8400 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - |  8401 | ` *  Otherwise this function is no-op.` |
|        - |  8402 | ` */` |
|        4 |  8403 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8404 |  |
|        - |  8405 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 |  8406 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 |  8407 | `	int iType = 0;` |
|        5 |  8408 | `	if( nArg < 1 ){` |
|        - |  8409 | `		/* Missing log message,return FALSE */` |
|      ! 0 |  8410 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  8411 | `		return PH7_OK;` |
|        - |  8412 | `	}` |
|        5 |  8413 | `	if( pVm->xErrLog  ){` |
|        - |  8414 | `		/* Invoke the user callback */` |
|      ! 0 |  8415 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 |  8416 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 |  8417 | `		if( nArg > 1 ){` |
|      ! 0 |  8418 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 |  8419 | `			if( nArg > 2 ){` |
|      ! 0 |  8420 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 |  8421 | `				if( nArg > 3 ){` |
|      ! 0 |  8422 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 |  8423 | `				}` |
|      ! 0 |  8424 | `			}` |
|      ! 0 |  8425 | `		}` |
|      ! 0 |  8426 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 |  8427 | `	}` |
|        - |  8428 | `	/* Retun TRUE */` |
|        5 |  8429 | `	ph7_result_bool(pCtx,1);` |
|        5 |  8430 | `	return PH7_OK;` |
|        3 |  8431 |  |
|        - |  8432 | `/*` |
|        - |  8433 | ` * bool restore_exception_handler(void)` |
|        - |  8434 | ` *  Restores the previously defined exception handler function.` |
|        - |  8435 | ` * Parameter` |
|        - |  8436 | ` *  None` |
|        - |  8437 | ` * Return` |
|        - |  8438 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - |  8439 | ` */` |
|        4 |  8440 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8441 |  |
|        5 |  8442 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8443 | `	ph7_value *pOld,*pNew;` |
|        - |  8444 | `	/* Point to the old and the new handler */` |
|        5 |  8445 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 |  8446 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 |  8447 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8448 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8449 | `		SXUNUSED(apArg);` |
|        - |  8450 | `		/* No installed handler,return FALSE */` |
|        5 |  8451 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8452 | `		return PH7_OK;` |
|        - |  8453 | `	}` |
|        - |  8454 | `	/* Copy the old handler */` |
|      ! 0 |  8455 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8456 | `	PH7_MemObjRelease(pOld);` |
|        - |  8457 | `	/* Return TRUE */` |
|      ! 0 |  8458 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8459 | `	return PH7_OK;` |
|        3 |  8460 |  |
|        - |  8461 | `/*` |
|        - |  8462 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - |  8463 | ` *  Sets a user-defined exception handler function.` |
|        - |  8464 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - |  8465 | ` * NOTE` |
|        - |  8466 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - |  8467 | ` *  the satndard PHP engine.` |
|        - |  8468 | ` * Parameters` |
|        - |  8469 | ` *  $exception_handler` |
|        - |  8470 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - |  8471 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - |  8472 | ` *   that was thrown.` |
|        - |  8473 | ` *  Note:` |
|        - |  8474 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8475 | ` * Return` |
|        - |  8476 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - |  8477 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8478 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8479 | ` */` |
|        4 |  8480 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8481 |  |
|        6 |  8482 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8483 | `	ph7_value *pOld,*pNew;` |
|        - |  8484 | `	/* Point to the old and the new handler */` |
|        6 |  8485 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 |  8486 | `	pNew = &pVm->aExceptionCB[1];` |
|        - |  8487 | `	/* Return the old handler */` |
|        6 |  8488 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 |  8489 | `	if( nArg > 0 ){` |
|        6 |  8490 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8491 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 |  8492 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 |  8493 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 |  8494 | `		}else{` |
|        6 |  8495 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8496 | `			/* Install the new handler */` |
|        6 |  8497 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8498 | `		}` |
|        2 |  8499 | `	}` |
|        6 |  8500 | `	return PH7_OK;` |
|        2 |  8501 |  |
|        - |  8502 | `/*` |
|        - |  8503 | ` * bool restore_error_handler(void)` |
|        - |  8504 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8505 | ` * Parameters:` |
|        - |  8506 | ` *  None.` |
|        - |  8507 | ` * Return` |
|        - |  8508 | ` *  Always TRUE.` |
|        - |  8509 | ` */` |
|        4 |  8510 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8511 |  |
|        5 |  8512 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8513 | `	ph7_value *pOld,*pNew;` |
|        - |  8514 | `	/* Point to the old and the new handler */` |
|        5 |  8515 | `	pOld = &pVm->aErrCB[0];` |
|        5 |  8516 | `	pNew = &pVm->aErrCB[1];` |
|        5 |  8517 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 |  8518 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 |  8519 | `		SXUNUSED(apArg);` |
|        - |  8520 | `		/* No installed callback,return FALSE */` |
|        5 |  8521 | `		ph7_result_bool(pCtx,0);` |
|        5 |  8522 | `		return PH7_OK;` |
|        - |  8523 | `	}` |
|        - |  8524 | `	/* Copy the old callback */` |
|      ! 0 |  8525 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 |  8526 | `	PH7_MemObjRelease(pOld);` |
|        - |  8527 | `	/* Return TRUE */` |
|      ! 0 |  8528 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 |  8529 | `	return PH7_OK;` |
|        3 |  8530 |  |
|        - |  8531 | `/*` |
|        - |  8532 | ` * value set_error_handler(callable $error_handler)` |
|        - |  8533 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8534 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - |  8535 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - |  8536 | ` *  Sets a user-defined error handler function.` |
|        - |  8537 | ` *  This function can be used for defining your own way of handling errors during` |
|        - |  8538 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - |  8539 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - |  8540 | ` *  conditions (using trigger_error()).` |
|        - |  8541 | ` * Parameters` |
|        - |  8542 | ` *  $error_handler` |
|        - |  8543 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - |  8544 | ` *   describing the error.` |
|        - |  8545 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - |  8546 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - |  8547 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - |  8548 | ` *   The function can be shown as:` |
|        - |  8549 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - |  8550 | ` *     errno` |
|        - |  8551 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - |  8552 | ` *   errstr` |
|        - |  8553 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - |  8554 | ` *   errfile` |
|        - |  8555 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - |  8556 | ` *     was raised in, as a string.` |
|        - |  8557 | ` *  Note:` |
|        - |  8558 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - |  8559 | ` * Return` |
|        - |  8560 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - |  8561 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - |  8562 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - |  8563 | ` */` |
|     8722 |  8564 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8565 |  |
|     8724 |  8566 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8567 | `	ph7_value *pOld,*pNew;` |
|        - |  8568 | `	/* Point to the old and the new handler */` |
|     8724 |  8569 | `	pOld = &pVm->aErrCB[0];` |
|     8724 |  8570 | `	pNew = &pVm->aErrCB[1];` |
|        - |  8571 | `	/* Return the old handler */` |
|     8724 |  8572 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|     8724 |  8573 | `	if( nArg > 0 ){` |
|     8724 |  8574 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - |  8575 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     4361 |  8576 | `			PH7_MemObjRelease(pNew);` |
|     4361 |  8577 | `			ph7_result_bool(pCtx,1);` |
|     2181 |  8578 | `		}else{` |
|     4364 |  8579 | `			PH7_MemObjStore(pNew,pOld);` |
|        - |  8580 | `			/* Install the new handler */` |
|     4364 |  8581 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - |  8582 | `		}` |
|     4361 |  8583 | `	}` |
|     8724 |  8584 | `	return PH7_OK;` |
|        2 |  8585 |  |
|        - |  8586 | `/*` |
|        - |  8587 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - |  8588 | ` *  Generates a backtrace.` |
|        - |  8589 | ` * Paramaeter` |
|        - |  8590 | ` *  $options` |
|        - |  8591 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - |  8592 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - |  8593 | ` *   all the function/method arguments, to save memory.` |
|        - |  8594 | ` * $limit` |
|        - |  8595 | ` *   (Not Used)` |
|        - |  8596 | ` * Return` |
|        - |  8597 | ` *  An array.The possible returned elements are as follows:` |
|        - |  8598 | ` *          Possible returned elements from debug_backtrace()` |
|        - |  8599 | ` *          Name        Type      Description` |
|        - |  8600 | ` *          ------      ------     -----------` |
|        - |  8601 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - |  8602 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - |  8603 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - |  8604 | ` *          class       string    The current class name. See also __CLASS__` |
|        - |  8605 | ` *          object      object    The current object.` |
|        - |  8606 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - |  8607 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - |  8608 | ` */` |
|      504 |  8609 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  8610 |  |
|      506 |  8611 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8612 | `	ph7_value *pArray;` |
|        - |  8613 | `	ph7_class *pClass;` |
|        - |  8614 | `	ph7_value *pValue;` |
|        - |  8615 | `	SyString *pFile;` |
|        - |  8616 | `	/* Create a new array */` |
|      506 |  8617 | `	pArray = ph7_context_new_array(pCtx);` |
|      506 |  8618 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      506 |  8619 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  8620 | `		/* Out of memory,return NULL */` |
|      ! 0 |  8621 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 |  8622 | `		ph7_result_null(pCtx);` |
|      ! 0 |  8623 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 |  8624 | `		SXUNUSED(apArg);` |
|      ! 0 |  8625 | `		return PH7_OK;` |
|        - |  8626 | `	}` |
|        - |  8627 | `	/* Dump running function name and it's arguments  */` |
|      506 |  8628 | `	if( pVm->pFrame->pParent ){` |
|      506 |  8629 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - |  8630 | `		ph7_vm_func *pFunc;` |
|        - |  8631 | `		ph7_value *pArg;` |
|      506 |  8632 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      506 |  8633 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      506 |  8634 | `		if( pFrame->pParent && pFunc ){` |
|      506 |  8635 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      506 |  8636 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      506 |  8637 | `			ph7_value_reset_string_cursor(pValue);` |
|      252 |  8638 | `		}` |
|        - |  8639 | `		/* Function arguments */` |
|      506 |  8640 | `		pArg = ph7_context_new_array(pCtx);` |
|      506 |  8641 | `		if( pArg  ){` |
|        - |  8642 | `			ph7_value *pObj;` |
|        - |  8643 | `			VmSlot *aSlot;` |
|        - |  8644 | `			sxu32 n;` |
|        - |  8645 | `			/* Start filling the array with the given arguments */` |
|      506 |  8646 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     2010 |  8647 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     1506 |  8648 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     1506 |  8649 | `				if( pObj ){` |
|     1506 |  8650 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|      752 |  8651 | `				}` |
|      754 |  8652 | `			}` |
|        - |  8653 | `			/* Save the array */` |
|      506 |  8654 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      252 |  8655 | `		}` |
|      252 |  8656 | `	}` |
|      506 |  8657 | `	ph7_value_int(pValue,1);` |
|        - |  8658 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - |  8659 | `	 * line numbers at run-time. )` |
|        - |  8660 | `	 */` |
|      506 |  8661 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - |  8662 | `	/* Current processed script */` |
|      506 |  8663 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      506 |  8664 | `	if( pFile ){` |
|      506 |  8665 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      506 |  8666 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      506 |  8667 | `		ph7_value_reset_string_cursor(pValue);` |
|      252 |  8668 | `	}` |
|        - |  8669 | `	/* Top class */` |
|      506 |  8670 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      506 |  8671 | `	if( pClass ){` |
|      502 |  8672 | `		ph7_value_reset_string_cursor(pValue);` |
|      502 |  8673 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      502 |  8674 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      250 |  8675 | `	}` |
|        - |  8676 | `	/* Return the freshly created array */` |
|      506 |  8677 | `	ph7_result_value(pCtx,pArray);` |
|        - |  8678 | `	/*` |
|        - |  8679 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - |  8680 | `	 * as soon we return from this function.` |
|        - |  8681 | `	 */` |
|      506 |  8682 | `	return PH7_OK;` |
|      254 |  8683 |  |
|        - |  8684 | `/*` |
|        - |  8685 | ` * Generate a small backtrace.` |
|        - |  8686 | ` * Store the generated dump in the given BLOB` |
|        - |  8687 | ` */` |
|        4 |  8688 | `static int VmMiniBacktrace(` |
|        - |  8689 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8690 | `	SyBlob *pOut /* Store Dump here */` |
|        - |  8691 | `	)` |
|        1 |  8692 |  |
|        5 |  8693 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - |  8694 | `	ph7_vm_func *pFunc;` |
|        - |  8695 | `	ph7_class *pClass;` |
|        - |  8696 | `	SyString *pFile;` |
|        - |  8697 | `	/* Called function */` |
|        5 |  8698 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  8699 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 |  8700 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8701 | `	if( pFrame->pParent && pFunc ){` |
|        5 |  8702 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 |  8703 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 |  8704 | `	}else{` |
|      ! 0 |  8705 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - |  8706 | `	}` |
|        5 |  8707 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - |  8708 | `	/* Current processed script */` |
|        5 |  8709 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 |  8710 | `	if( pFile ){` |
|        5 |  8711 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 |  8712 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 |  8713 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 |  8714 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 |  8715 | `	}` |
|        - |  8716 | `	/* Top class */` |
|        5 |  8717 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 |  8718 | `	if( pClass ){` |
|      ! 0 |  8719 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 |  8720 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 |  8721 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 |  8722 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 |  8723 | `	}` |
|        5 |  8724 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - |  8725 | `	/* All done */` |
|        5 |  8726 | `	return SXRET_OK;` |
|        1 |  8727 |  |
|        - |  8728 | `/*` |
|        - |  8729 | ` * void debug_print_backtrace()` |
|        - |  8730 | ` *  Prints a backtrace` |
|        - |  8731 | ` * Parameters` |
|        - |  8732 | ` * None` |
|        - |  8733 | ` * Return` |
|        - |  8734 | ` * NULL` |
|        - |  8735 | ` */` |
|        2 |  8736 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8737 |  |
|        3 |  8738 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8739 | `	SyBlob sDump;` |
|        3 |  8740 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8741 | `	/* Generate the backtrace */` |
|        3 |  8742 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8743 | `	/* Output backtrace */` |
|        3 |  8744 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - |  8745 | `	/* All done,cleanup */` |
|        3 |  8746 | `	SyBlobRelease(&sDump);` |
|        1 |  8747 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8748 | `	SXUNUSED(apArg);` |
|        3 |  8749 | `	return PH7_OK;` |
|        1 |  8750 |  |
|        - |  8751 | `/*` |
|        - |  8752 | ` * string debug_string_backtrace()` |
|        - |  8753 | ` *  Generate a backtrace` |
|        - |  8754 | ` * Parameters` |
|        - |  8755 | ` * None` |
|        - |  8756 | ` * Return` |
|        - |  8757 | ` *  A mini backtrace().` |
|        - |  8758 | ` * Note that this is a symisc extension.` |
|        - |  8759 | ` */` |
|        2 |  8760 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  8761 |  |
|        3 |  8762 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  8763 | `	SyBlob sDump;` |
|        3 |  8764 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - |  8765 | `	/* Generate the backtrace */` |
|        3 |  8766 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - |  8767 | `	/* Return the backtrace */` |
|        3 |  8768 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - |  8769 | `	/* All done,cleanup */` |
|        3 |  8770 | `	SyBlobRelease(&sDump);` |
|        1 |  8771 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  8772 | `	SXUNUSED(apArg);` |
|        3 |  8773 | `	return PH7_OK;` |
|        1 |  8774 |  |
|        - |  8775 | `/*` |
|        - |  8776 | ` * The following routine is invoked by the engine when an uncaught` |
|        - |  8777 | ` * exception is triggered.` |
|        - |  8778 | ` */` |
|      472 |  8779 | `static sxi32 VmUncaughtException(` |
|        - |  8780 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  8781 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8782 | `	)` |
|        1 |  8783 |  |
|        - |  8784 | `	ph7_value *apArg[2],sArg;` |
|      473 |  8785 | `	int nArg = 1;` |
|        - |  8786 | `	sxi32 rc;` |
|      473 |  8787 | `	if( pVm->nExceptDepth > 15 ){` |
|        - |  8788 | `		/* Nesting limit reached */` |
|      ! 0 |  8789 | `		return SXRET_OK;` |
|        - |  8790 | `	}` |
|        - |  8791 | `	/* Call any exception handler if available */` |
|      473 |  8792 | `	PH7_MemObjInit(pVm,&sArg);` |
|      473 |  8793 | `	if( pThis ){` |
|        - |  8794 | `		/* Load the exception instance */` |
|      473 |  8795 | `		sArg.x.pOther = pThis;` |
|      473 |  8796 | `		pThis->iRef++;` |
|      473 |  8797 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      237 |  8798 | `	}else{` |
|      ! 0 |  8799 | `		nArg = 0;` |
|        - |  8800 | `	}` |
|      473 |  8801 | `	apArg[0] = &sArg;` |
|        - |  8802 | `	/* Call the exception handler if available */` |
|      473 |  8803 | `	pVm->nExceptDepth++;` |
|      473 |  8804 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      473 |  8805 | `	pVm->nExceptDepth--;` |
|      473 |  8806 | `	if( rc != SXRET_OK ){` |
|        - |  8807 | `		SyBlob sMsgBuf;` |
|      471 |  8808 | `		const char *zClass = "Exception";` |
|      471 |  8809 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - |  8810 | `		const char *zMsg;` |
|        - |  8811 | `		sxu32 nMsg;` |
|        - |  8812 | `		const char *zFuncName;` |
|        - |  8813 | `		int nFuncLen;` |
|      471 |  8814 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      471 |  8815 | `		if( pThis ){` |
|        - |  8816 | `			ph7_class_method *pGetMessage;` |
|        - |  8817 | `			ph7_value sMsg;` |
|        - |  8818 | `			const char *zTmp;` |
|        - |  8819 | `			int nTmp;` |
|      471 |  8820 | `			zClass = pThis->pClass->sName.zString;` |
|      471 |  8821 | `			nClass = pThis->pClass->sName.nByte;` |
|      471 |  8822 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      471 |  8823 | `			if( pGetMessage ){` |
|      471 |  8824 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      471 |  8825 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      471 |  8826 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      471 |  8827 | `					if( zTmp && nTmp > 0 ){` |
|      471 |  8828 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      235 |  8829 | `					}` |
|      235 |  8830 | `				}` |
|      471 |  8831 | `				PH7_MemObjRelease(&sMsg);` |
|      235 |  8832 | `			}` |
|      235 |  8833 | `		}` |
|      471 |  8834 | `		if( SyBlobLength(&sMsgBuf) == 0 ){` |
|      ! 0 |  8835 | `			SyBlobAppend(&sMsgBuf,"Unknown exception",sizeof("Unknown exception")-1);` |
|      ! 0 |  8836 | `		}` |
|      471 |  8837 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      471 |  8838 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      471 |  8839 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      471 |  8840 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      471 |  8841 | `		SyBlobRelease(&sMsgBuf);` |
|        - |  8842 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      471 |  8843 | `		rc = SXERR_ABORT;` |
|      235 |  8844 | `	}` |
|      473 |  8845 | `	PH7_MemObjRelease(&sArg);` |
|      473 |  8846 | `	return rc;` |
|      237 |  8847 |  |
|        - |  8848 | `/*` |
|        - |  8849 | ` * Throw a user exception.` |
|        - |  8850 | ` *` |
|        - |  8851 | ` * Exception dispatch follows this sequence:` |
|        - |  8852 | ` *` |
|        - |  8853 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - |  8854 | ` *    try/catch whose catch block matches the exception class.` |
|        - |  8855 | ` *` |
|        - |  8856 | ` * 2. If NO catch matches:` |
|        - |  8857 | ` *    a. Run finally (if present) for the current try block.` |
|        - |  8858 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - |  8859 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - |  8860 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - |  8861 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - |  8862 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - |  8863 | ` *    d. Otherwise, report as truly uncaught.` |
|        - |  8864 | ` *` |
|        - |  8865 | ` * 3. If a catch DOES match:` |
|        - |  8866 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - |  8867 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - |  8868 | ` *       inside the catch body from immediately propagating past our` |
|        - |  8869 | ` *       finally block.` |
|        - |  8870 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - |  8871 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - |  8872 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - |  8873 | ` *       in pPendingException (step 2c).` |
|        - |  8874 | ` *    c. Restore outer handlers from the saved copy.` |
|        - |  8875 | ` *    d. Run finally (if present).` |
|        - |  8876 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - |  8877 | ` *       that handlers are restored and finally has run.` |
|        - |  8878 | ` */` |
|      508 |  8879 | `static sxi32 VmThrowException(` |
|        - |  8880 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |  8881 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - |  8882 | `	)` |
|        2 |  8883 |  |
|        - |  8884 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - |  8885 | `	ph7_exception **apException;` |
|        - |  8886 | `	ph7_exception *pException;` |
|        - |  8887 | `	/* Point to the stack of loaded exceptions */` |
|      510 |  8888 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      510 |  8889 | `	pException = 0;` |
|      510 |  8890 | `	pCatch = 0;` |
|      510 |  8891 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8892 | `		ph7_exception_block *aCatch;` |
|        - |  8893 | `		ph7_class *pClass;` |
|        - |  8894 | `		sxu32 j;` |
|        - |  8895 | `		/* Locate the appropriate block to execute */` |
|       34 |  8896 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|       34 |  8897 | `		(void)SySetPop(&pVm->aException);` |
|       34 |  8898 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|       34 |  8899 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|       32 |  8900 | `			SyString *pName = &aCatch[j].sClass;` |
|        - |  8901 | `			/* Extract the target class */` |
|       32 |  8902 | `			pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       32 |  8903 | `			if( pClass == 0 ){` |
|        - |  8904 | `				/* No such class */` |
|      ! 0 |  8905 | `				continue;` |
|        - |  8906 | `			}` |
|       32 |  8907 | `			if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|        - |  8908 | `				/* Catch block found,break immeditaley */` |
|       32 |  8909 | `				pCatch = &aCatch[j];` |
|       32 |  8910 | `				break;` |
|        - |  8911 | `			}` |
|      ! 0 |  8912 | `		}` |
|       16 |  8913 | `	}` |
|        - |  8914 | `	/* Execute the cached block if available */` |
|      510 |  8915 | `	if( pCatch == 0 ){` |
|        - |  8916 | `		sxi32 rc;` |
|        - |  8917 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      480 |  8918 | `		if( pException && pException->iHasFinally ){` |
|        3 |  8919 | `			pException->iFinallyDone = 1;` |
|        3 |  8920 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 |  8921 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  8922 | `				return SXERR_ABORT;` |
|        - |  8923 | `			}` |
|        1 |  8924 | `		}` |
|        - |  8925 | `		/* Check if there is an outer exception handler on the stack */` |
|      480 |  8926 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  8927 | `			/* Re-throw to the outer handler */` |
|        3 |  8928 | `			return VmThrowException(&(*pVm),pThis);` |
|        - |  8929 | `		}` |
|        - |  8930 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - |  8931 | `		 * (catch body re-throw with finally pending), defer the` |
|        - |  8932 | `		 * exception instead of reporting it uncaught.` |
|        - |  8933 | `		 */` |
|      478 |  8934 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - |  8935 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - |  8936 | `			 * by looking for a catch frame on the stack.` |
|        - |  8937 | `			 */` |
|      478 |  8938 | `			VmFrame *pF = pVm->pFrame;` |
|      478 |  8939 | `			int inCatch = 0;` |
|      956 |  8940 | `			while( pF ){` |
|      484 |  8941 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        6 |  8942 | `					inCatch = 1;` |
|        6 |  8943 | `					break;` |
|        - |  8944 | `				}` |
|      479 |  8945 | `				pF = pF->pParent;` |
|        1 |  8946 | `			}` |
|      478 |  8947 | `			if( inCatch ){` |
|        - |  8948 | `				/* Defer — will be re-thrown after finally runs */` |
|        6 |  8949 | `				pThis->iRef++;` |
|        6 |  8950 | `				pVm->pPendingException = pThis;` |
|        6 |  8951 | `				return SXRET_OK;` |
|        - |  8952 | `			}` |
|      236 |  8953 | `		}` |
|        - |  8954 | `		/* Truly uncaught */` |
|      473 |  8955 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      473 |  8956 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 |  8957 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 |  8958 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 |  8959 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 |  8960 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 |  8961 | `			}` |
|      ! 0 |  8962 | `		}` |
|      473 |  8963 | `		return rc;` |
|      ! 0 |  8964 | `	}else{` |
|       32 |  8965 | `		VmFrame *pFrame = pVm->pFrame;` |
|       32 |  8966 | `		ph7_exception **apSaved = 0;` |
|        - |  8967 | `		sxu32 nSavedCount;` |
|        - |  8968 | `		sxi32 rc;` |
|       32 |  8969 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       32 |  8970 | `		if( pException->pFrame == pFrame ){` |
|       24 |  8971 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|       11 |  8972 | `		}` |
|        - |  8973 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - |  8974 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - |  8975 | `		 * our finally block. We save the stack contents and restore after.` |
|        - |  8976 | `		 */` |
|       32 |  8977 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|       32 |  8978 | `		if( nSavedCount > 0 ){` |
|       11 |  8979 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        3 |  8980 | `				nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8981 | `			if( apSaved ){` |
|       11 |  8982 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        3 |  8983 | `					nSavedCount * sizeof(ph7_exception *));` |
|        8 |  8984 | `				SySetReset(&pVm->aException);` |
|        3 |  8985 | `			}` |
|        3 |  8986 | `		}` |
|        - |  8987 | `		/* Create a private frame first */` |
|       32 |  8988 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|       32 |  8989 | `		if( rc == SXRET_OK ){` |
|       32 |  8990 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|       32 |  8991 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|       32 |  8992 | `			if( pObj ){` |
|       32 |  8993 | `				pThis->iRef++;` |
|       32 |  8994 | `				pObj->x.pOther = pThis;` |
|       32 |  8995 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       15 |  8996 | `			}` |
|        - |  8997 | `			/* Execute the catch block */` |
|       32 |  8998 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - |  8999 | `			/* Leave the frame */` |
|       32 |  9000 | `			VmLeaveFrame(&(*pVm));` |
|       15 |  9001 | `		}` |
|        - |  9002 | `		/* Restore the outer exception handlers */` |
|       32 |  9003 | `		if( apSaved ){` |
|        - |  9004 | `			sxu32 k;` |
|        - |  9005 | `			/* Any new entries pushed during catch execution (from nested` |
|        - |  9006 | `			 * try blocks inside the catch body) are already consumed.` |
|        - |  9007 | `			 * Restore the original outer entries.` |
|        - |  9008 | `			 */` |
|        8 |  9009 | `			SySetReset(&pVm->aException);` |
|       14 |  9010 | `			for(k = 0; k < nSavedCount; k++){` |
|        8 |  9011 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        5 |  9012 | `			}` |
|        8 |  9013 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        3 |  9014 | `		}` |
|        - |  9015 | `		/* Execute the finally block after catch */` |
|       32 |  9016 | `		if( pException->iHasFinally ){` |
|       11 |  9017 | `			pException->iFinallyDone = 1;` |
|        - |  9018 | `			{` |
|       11 |  9019 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       11 |  9020 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 |  9021 | `					return SXERR_ABORT;` |
|        - |  9022 | `				}` |
|        - |  9023 | `			}` |
|        5 |  9024 | `		}` |
|       32 |  9025 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  9026 | `			return SXERR_ABORT;` |
|        - |  9027 | `		}` |
|        - |  9028 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - |  9029 | `		 * pPendingException (because outer handlers were hidden).` |
|        - |  9030 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - |  9031 | `		 */` |
|       32 |  9032 | `		if( pVm->pPendingException ){` |
|        6 |  9033 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        6 |  9034 | `			pVm->pPendingException = 0;` |
|        6 |  9035 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - |  9036 | `		}` |
|        - |  9037 | `	}` |
|        - |  9038 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - |  9039 | `	 * be used again if a 'goto' statement is executed.` |
|        - |  9040 | `	 */` |
|       28 |  9041 | `	return SXRET_OK;` |
|      256 |  9042 |  |
|        - |  9043 | `/*` |
|        - |  9044 | ` * Section:` |
|        - |  9045 | ` *  Version,Credits and Copyright related functions.` |
|        - |  9046 | ` * Status:` |
|        - |  9047 | ` *    Stable.` |
|        - |  9048 | ` */` |
|        - |  9049 | `/*` |
|        - |  9050 | ` * string ph7version(void)` |
|        - |  9051 | ` *  Returns the running version of the PH7 version.` |
|        - |  9052 | ` * Parameters` |
|        - |  9053 | ` *  None` |
|        - |  9054 | ` * Return` |
|        - |  9055 | ` * Current PH7 version.` |
|        - |  9056 | ` */` |
|        2 |  9057 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9058 |  |
|        1 |  9059 | `	SXUNUSED(nArg);` |
|        1 |  9060 | `	SXUNUSED(apArg); /* cc warning */` |
|        - |  9061 | `	/* Current engine version */` |
|        3 |  9062 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 |  9063 | `	return PH7_OK;` |
|        1 |  9064 |  |
|        - |  9065 | `/*` |
|        - |  9066 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - |  9067 | ` */` |
|        - |  9068 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - |  9069 | ` "<html><head>"\` |
|        - |  9070 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - |  9071 | ` "<style type=\"text/css\">"\` |
|        - |  9072 | ` "div {"\` |
|        - |  9073 | `     "border: 1px solid #cccccc;"\` |
|        - |  9074 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - |  9075 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - |  9076 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - |  9077 | `     "-moz-border-radius-topright: 10px;"\` |
|        - |  9078 | `     "-webkit-border-radius: 10px;"\` |
|        - |  9079 | `     "-o-border-radius: 10px;"\` |
|        - |  9080 | `     "border-radius: 10px;"\` |
|        - |  9081 | `     "padding-left: 2em;"\` |
|        - |  9082 | `     "background-color: white;"\` |
|        - |  9083 | `     "margin-left: auto;"\` |
|        - |  9084 | `     "font-family: verdana;"\` |
|        - |  9085 | `     "padding-right: 2em;"\` |
|        - |  9086 | `     "margin-right: auto;"\` |
|        - |  9087 | `     "}"\` |
|        - |  9088 | `     "body {"\` |
|        - |  9089 | `     "padding: 0.2em;"\` |
|        - |  9090 | `     "font-style: normal;"\` |
|        - |  9091 | `     "font-size: medium;"\` |
|        - |  9092 | `     "background-color: #f2f2f2;"\` |
|        - |  9093 | `     "}"\` |
|        - |  9094 | `     "hr {"\` |
|        - |  9095 | `     "border-style: solid none none;"\` |
|        - |  9096 | `     "border-width: 1px medium medium;"\` |
|        - |  9097 | `     "border-top: 1px solid #cccccc;"\` |
|        - |  9098 | `     "height: 1px;"\` |
|        - |  9099 | `     "}"\` |
|        - |  9100 | `     "a {"\` |
|        - |  9101 | `     "color: #3366cc;"\` |
|        - |  9102 | `     "text-decoration: none;"\` |
|        - |  9103 | `     "}"\` |
|        - |  9104 | `     "a:hover {"\` |
|        - |  9105 | `     "color: #999999;"\` |
|        - |  9106 | `     "}"\` |
|        - |  9107 | `     "a:active {"\` |
|        - |  9108 | `     "color: #663399;"\` |
|        - |  9109 | `     "}"\` |
|        - |  9110 | `     "h1 {"\` |
|        - |  9111 | `     "margin: 0;"\` |
|        - |  9112 | `     "padding: 0;"\` |
|        - |  9113 | `     "font-family: Verdana;"\` |
|        - |  9114 | `     "font-weight: bold;"\` |
|        - |  9115 | `     "font-style: normal;"\` |
|        - |  9116 | `     "font-size: medium;"\` |
|        - |  9117 | `     "text-transform: capitalize;"\` |
|        - |  9118 | `     "color: #0a328c;"\` |
|        - |  9119 | `     "}"\` |
|        - |  9120 | `     "p {"\` |
|        - |  9121 | `     "margin: 0 auto;"\` |
|        - |  9122 | `     "font-size: medium;"\` |
|        - |  9123 | `     "font-style: normal;"\` |
|        - |  9124 | `     "font-family: verdana;"\` |
|        - |  9125 | `     "}"\` |
|        - |  9126 | `"</style></head><body>"\` |
|        - |  9127 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - |  9128 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - |  9129 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - |  9130 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - |  9131 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - |  9132 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - |  9133 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - |  9134 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - |  9135 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - |  9136 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - |  9137 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - |  9138 |  |
|        - |  9139 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9140 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - |  9141 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - |  9142 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - |  9143 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9144 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - |  9145 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9146 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - |  9147 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - |  9148 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - |  9149 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - |  9150 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - |  9151 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - |  9152 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - |  9153 |  |
|        - |  9154 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - |  9155 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - |  9156 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - |  9157 | `"&nbsp;*<br>"\` |
|        - |  9158 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - |  9159 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - |  9160 | `"&nbsp;* are met:<br>"\` |
|        - |  9161 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - |  9162 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - |  9163 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - |  9164 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - |  9165 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - |  9166 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - |  9167 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - |  9168 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - |  9169 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - |  9170 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - |  9171 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - |  9172 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - |  9173 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - |  9174 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - |  9175 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - |  9176 | `"&nbsp;*<br>"\` |
|        - |  9177 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - |  9178 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - |  9179 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - |  9180 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - |  9181 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - |  9182 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - |  9183 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - |  9184 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - |  9185 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - |  9186 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - |  9187 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - |  9188 | `"&nbsp;*/<br>"\` |
|        - |  9189 | `"</span></small></small></p>"\` |
|        - |  9190 | `"</div></body></html>"` |
|        - |  9191 | `/*` |
|        - |  9192 | ` * bool ph7credits(void)` |
|        - |  9193 | ` * bool ph7info(void)` |
|        - |  9194 | ` * bool ph7copyright(void)` |
|        - |  9195 | ` *  Prints out the credits for PH7 engine` |
|        - |  9196 | ` * Parameters` |
|        - |  9197 | ` *  None` |
|        - |  9198 | ` * Return` |
|        - |  9199 | ` *  Always TRUE` |
|        - |  9200 | ` */` |
|        2 |  9201 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9202 |  |
|        3 |  9203 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - |  9204 | `	/* Expand the HTML page above*/` |
|        3 |  9205 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 |  9206 | `	ph7_context_output_format(` |
|        1 |  9207 | `		pCtx,` |
|        - |  9208 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 |  9209 | `		ph7_lib_version(),   /* Engine version */` |
|        1 |  9210 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 |  9211 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 |  9212 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 |  9213 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 |  9214 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - |  9215 | `#ifdef __WINNT__` |
|        - |  9216 | `		"Windows NT"` |
|        - |  9217 | `#elif defined(__UNIXES__)` |
|        - |  9218 | `		"UNIX-Like"` |
|        - |  9219 | `#else` |
|        - |  9220 | `		"Other OS"` |
|        - |  9221 | `#endif` |
|        - |  9222 | `		);` |
|        3 |  9223 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 |  9224 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 |  9225 | `	SXUNUSED(apArg);` |
|        - |  9226 | `	/* Return TRUE */` |
|        - |  9227 | `	//ph7_result_bool(pCtx,1);` |
|        3 |  9228 | `	return PH7_OK;` |
|        1 |  9229 |  |
|        - |  9230 | `/*` |
|        - |  9231 | ` * Section:` |
|        - |  9232 | ` *    URL related routines.` |
|        - |  9233 | ` * Status:` |
|        - |  9234 | ` *    Stable.` |
|        - |  9235 | ` */` |
|        - |  9236 | `/*` |
|        - |  9237 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - |  9238 | ` *  Parse a URL and return its fields.` |
|        - |  9239 | ` * Parameters` |
|        - |  9240 | ` *  $url` |
|        - |  9241 | ` *   The URL to parse.` |
|        - |  9242 | ` * $component` |
|        - |  9243 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - |  9244 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - |  9245 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - |  9246 | ` *  in which case the return value will be an integer).` |
|        - |  9247 | ` * Return` |
|        - |  9248 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - |  9249 | ` *  At least one element will be present within the array. Potential keys within` |
|        - |  9250 | ` *  this array are:` |
|        - |  9251 | ` *   scheme - e.g. http` |
|        - |  9252 | ` *   host` |
|        - |  9253 | ` *   port` |
|        - |  9254 | ` *   user` |
|        - |  9255 | ` *   pass` |
|        - |  9256 | ` *   path` |
|        - |  9257 | ` *   query - after the question mark ?` |
|        - |  9258 | ` *   fragment - after the hashmark #` |
|        - |  9259 | ` * Note:` |
|        - |  9260 | ` *  FALSE is returned on failure.` |
|        - |  9261 | ` *  This function work with relative URL unlike the one shipped` |
|        - |  9262 | ` *  with the standard PHP engine.` |
|        - |  9263 | ` */` |
|       28 |  9264 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9265 |  |
|        - |  9266 | `	const char *zStr; /* Input string */` |
|        - |  9267 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - |  9268 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - |  9269 | `	int nLen;` |
|        - |  9270 | `	sxi32 rc;` |
|       29 |  9271 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - |  9272 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 |  9273 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9274 | `		return PH7_OK;` |
|        - |  9275 | `	}` |
|        - |  9276 | `	/* Extract the given URI */` |
|       29 |  9277 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 |  9278 | `	if( nLen < 1 ){` |
|        - |  9279 | `		/* Nothing to process,return FALSE */` |
|        3 |  9280 | `		ph7_result_bool(pCtx,0);` |
|        3 |  9281 | `		return PH7_OK;` |
|        - |  9282 | `	}` |
|        - |  9283 | `	/* Get a parse */` |
|       27 |  9284 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 |  9285 | `	if( rc != SXRET_OK ){` |
|        - |  9286 | `		/* Malformed input,return FALSE */` |
|      ! 0 |  9287 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 |  9288 | `		return PH7_OK;` |
|        - |  9289 | `	}` |
|       27 |  9290 | `	if( nArg > 1 ){` |
|      ! 0 |  9291 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - |  9292 | `		/* Refer to constant.c for constants values */` |
|      ! 0 |  9293 | `		switch(nComponent){` |
|      ! 0 |  9294 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 |  9295 | `			pComp = &sURI.sScheme;` |
|      ! 0 |  9296 | `			if( pComp->nByte < 1 ){` |
|        - |  9297 | `				/* No available value,return NULL */` |
|      ! 0 |  9298 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9299 | `			}else{` |
|      ! 0 |  9300 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9301 | `			}` |
|      ! 0 |  9302 | `			break;` |
|      ! 0 |  9303 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 |  9304 | `			pComp = &sURI.sHost;` |
|      ! 0 |  9305 | `			if( pComp->nByte < 1 ){` |
|        - |  9306 | `				/* No available value,return NULL */` |
|      ! 0 |  9307 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9308 | `			}else{` |
|      ! 0 |  9309 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9310 | `			}` |
|      ! 0 |  9311 | `			break;` |
|      ! 0 |  9312 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 |  9313 | `			pComp = &sURI.sPort;` |
|      ! 0 |  9314 | `			if( pComp->nByte < 1 ){` |
|        - |  9315 | `				/* No available value,return NULL */` |
|      ! 0 |  9316 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9317 | `			}else{` |
|      ! 0 |  9318 | `				int iPort = 0;` |
|        - |  9319 | `				/* Cast the value to integer */` |
|      ! 0 |  9320 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 |  9321 | `				ph7_result_int(pCtx,iPort);` |
|        - |  9322 | `			}` |
|      ! 0 |  9323 | `			break;` |
|      ! 0 |  9324 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 |  9325 | `			pComp = &sURI.sUser;` |
|      ! 0 |  9326 | `			if( pComp->nByte < 1 ){` |
|        - |  9327 | `				/* No available value,return NULL */` |
|      ! 0 |  9328 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9329 | `			}else{` |
|      ! 0 |  9330 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9331 | `			}` |
|      ! 0 |  9332 | `			break;` |
|      ! 0 |  9333 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 |  9334 | `			pComp = &sURI.sPass;` |
|      ! 0 |  9335 | `			if( pComp->nByte < 1 ){` |
|        - |  9336 | `				/* No available value,return NULL */` |
|      ! 0 |  9337 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9338 | `			}else{` |
|      ! 0 |  9339 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9340 | `			}` |
|      ! 0 |  9341 | `			break;` |
|      ! 0 |  9342 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 |  9343 | `			pComp = &sURI.sQuery;` |
|      ! 0 |  9344 | `			if( pComp->nByte < 1 ){` |
|        - |  9345 | `				/* No available value,return NULL */` |
|      ! 0 |  9346 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9347 | `			}else{` |
|      ! 0 |  9348 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9349 | `			}` |
|      ! 0 |  9350 | `			break;` |
|      ! 0 |  9351 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 |  9352 | `			pComp = &sURI.sFragment;` |
|      ! 0 |  9353 | `			if( pComp->nByte < 1 ){` |
|        - |  9354 | `				/* No available value,return NULL */` |
|      ! 0 |  9355 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9356 | `			}else{` |
|      ! 0 |  9357 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9358 | `			}` |
|      ! 0 |  9359 | `			break;` |
|      ! 0 |  9360 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 |  9361 | `			pComp = &sURI.sPath;` |
|      ! 0 |  9362 | `			if( pComp->nByte < 1 ){` |
|        - |  9363 | `				/* No available value,return NULL */` |
|      ! 0 |  9364 | `				ph7_result_null(pCtx);` |
|      ! 0 |  9365 | `			}else{` |
|      ! 0 |  9366 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - |  9367 | `			}` |
|      ! 0 |  9368 | `			break;` |
|      ! 0 |  9369 | `		default:` |
|        - |  9370 | `			/* No such entry,return NULL */` |
|      ! 0 |  9371 | `			ph7_result_null(pCtx);` |
|      ! 0 |  9372 | `			break;` |
|        - |  9373 | `		}` |
|      ! 0 |  9374 | `	}else{` |
|        - |  9375 | `		ph7_value *pArray,*pValue;` |
|        - |  9376 | `		/* Return an associative array */` |
|       27 |  9377 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 |  9378 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 |  9379 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - |  9380 | `			/* Out of memory */` |
|      ! 0 |  9381 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9382 | `			/* Return false */` |
|      ! 0 |  9383 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 |  9384 | `			return PH7_OK;` |
|        - |  9385 | `		}` |
|        - |  9386 | `		/* Fill the array */` |
|       27 |  9387 | `		pComp = &sURI.sScheme;` |
|       27 |  9388 | `		if( pComp->nByte > 0 ){` |
|       19 |  9389 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 |  9390 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 |  9391 | `		}` |
|        - |  9392 | `		/* Reset the string cursor */` |
|       27 |  9393 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9394 | `		pComp = &sURI.sHost;` |
|       27 |  9395 | `		if( pComp->nByte > 0 ){` |
|       25 |  9396 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 |  9397 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 |  9398 | `		}` |
|        - |  9399 | `		/* Reset the string cursor */` |
|       27 |  9400 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9401 | `		pComp = &sURI.sPort;` |
|       27 |  9402 | `		if( pComp->nByte > 0 ){` |
|       11 |  9403 | `			int iPort = 0;/* cc warning */` |
|        - |  9404 | `			/* Convert to integer */` |
|       11 |  9405 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 |  9406 | `			ph7_value_int(pValue,iPort);` |
|       11 |  9407 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 |  9408 | `		}` |
|        - |  9409 | `		/* Reset the string cursor */` |
|       27 |  9410 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9411 | `		pComp = &sURI.sUser;` |
|       27 |  9412 | `		if( pComp->nByte > 0 ){` |
|        7 |  9413 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9414 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 |  9415 | `		}` |
|        - |  9416 | `		/* Reset the string cursor */` |
|       27 |  9417 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9418 | `		pComp = &sURI.sPass;` |
|       27 |  9419 | `		if( pComp->nByte > 0 ){` |
|        7 |  9420 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 |  9421 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 |  9422 | `		}` |
|        - |  9423 | `		/* Reset the string cursor */` |
|       27 |  9424 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9425 | `		pComp = &sURI.sPath;` |
|       27 |  9426 | `		if( pComp->nByte > 0 ){` |
|       17 |  9427 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 |  9428 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 |  9429 | `		}` |
|        - |  9430 | `		/* Reset the string cursor */` |
|       27 |  9431 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9432 | `		pComp = &sURI.sQuery;` |
|       27 |  9433 | `		if( pComp->nByte > 0 ){` |
|        5 |  9434 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9435 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 |  9436 | `		}` |
|        - |  9437 | `		/* Reset the string cursor */` |
|       27 |  9438 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 |  9439 | `		pComp = &sURI.sFragment;` |
|       27 |  9440 | `		if( pComp->nByte > 0 ){` |
|        5 |  9441 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 |  9442 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 |  9443 | `		}` |
|        - |  9444 | `		/* Return the created array */` |
|       27 |  9445 | `		ph7_result_value(pCtx,pArray);` |
|        - |  9446 | `		/* NOTE:` |
|        - |  9447 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - |  9448 | `		 * automatically as soon we return from this function.` |
|        - |  9449 | `		 */` |
|        - |  9450 | `	}` |
|        - |  9451 | `	/* All done */` |
|       27 |  9452 | `	return PH7_OK;` |
|       15 |  9453 |  |
|        - |  9454 | `/*` |
|        - |  9455 | ` * Section:` |
|        - |  9456 | ` *   Array related routines.` |
|        - |  9457 | ` * Status:` |
|        - |  9458 | ` *    Stable.` |
|        - |  9459 | ` * Note 2012-5-21 01:04:15:` |
|        - |  9460 | ` *  Array related functions that need access to the underlying` |
|        - |  9461 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - |  9462 | ` */` |
|        - |  9463 | `/*` |
|        - |  9464 | ` * The [compact()] function store it's state information in an instance` |
|        - |  9465 | ` * of the following structure.` |
|        - |  9466 | ` */` |
|        - |  9467 | `struct compact_data` |
|        - |  9468 |  |
|        - |  9469 | `	ph7_value *pArray;  /* Target array */` |
|        - |  9470 | `	int nRecCount;      /* Recursion count */` |
|        - |  9471 | `};` |
|        - |  9472 | `/*` |
|        - |  9473 | ` * Walker callback for the [compact()] function defined below.` |
|        - |  9474 | ` */` |
|      ! 0 |  9475 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 |  9476 |  |
|      ! 0 |  9477 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 |  9478 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 |  9479 | `	ph7_vm *pVm = pArray->pVm;` |
|        - |  9480 | `	/* Act according to the hashmap value */` |
|      ! 0 |  9481 | `	if( ph7_value_is_string(pValue) ){` |
|        - |  9482 | `		SyString sVar;` |
|      ! 0 |  9483 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 |  9484 | `		if( sVar.nByte > 0 ){` |
|        - |  9485 | `			/* Query the current frame */` |
|      ! 0 |  9486 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - |  9487 | `			/* ^` |
|        - |  9488 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - |  9489 | `			 */` |
|      ! 0 |  9490 | `			if( pKey ){` |
|        - |  9491 | `				/* Perform the insertion */` |
|      ! 0 |  9492 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 |  9493 | `			}` |
|      ! 0 |  9494 | `		}` |
|      ! 0 |  9495 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - |  9496 | `		int rc;` |
|        - |  9497 | `		/* Recursively traverse this array */` |
|      ! 0 |  9498 | `		pData->nRecCount++;` |
|      ! 0 |  9499 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 |  9500 | `		pData->nRecCount--;` |
|      ! 0 |  9501 | `		return rc;` |
|        - |  9502 | `	}` |
|      ! 0 |  9503 | `	return SXRET_OK;` |
|      ! 0 |  9504 |  |
|        - |  9505 | `/*` |
|        - |  9506 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - |  9507 | ` *  Create array containing variables and their values.` |
|        - |  9508 | ` *  For each of these, compact() looks for a variable with that name` |
|        - |  9509 | ` *  in the current symbol table and adds it to the output array such` |
|        - |  9510 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - |  9511 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - |  9512 | ` *  Any strings that are not set will simply be skipped.` |
|        - |  9513 | ` * Parameters` |
|        - |  9514 | ` *  $varname` |
|        - |  9515 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - |  9516 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - |  9517 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - |  9518 | ` *   it recursively.` |
|        - |  9519 | ` * Return` |
|        - |  9520 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - |  9521 | ` */` |
|        2 |  9522 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9523 |  |
|        - |  9524 | `	ph7_value *pArray,*pObj;` |
|        3 |  9525 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9526 | `	const char *zName;` |
|        - |  9527 | `	SyString sVar;` |
|        - |  9528 | `	int i,nLen;` |
|        3 |  9529 | `	if( nArg < 1 ){` |
|        - |  9530 | `		/* Missing arguments,return NULL */` |
|      ! 0 |  9531 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9532 | `		return PH7_OK;` |
|        - |  9533 | `	}` |
|        - |  9534 | `	/* Create the array */` |
|        3 |  9535 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 |  9536 | `	if( pArray == 0 ){` |
|        - |  9537 | `		/* Out of memory */` |
|      ! 0 |  9538 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - |  9539 | `		/* Return NULL */` |
|      ! 0 |  9540 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9541 | `		return PH7_OK;` |
|        - |  9542 | `	}` |
|        - |  9543 | `	/* Perform the requested operation */` |
|        7 |  9544 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 |  9545 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 |  9546 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - |  9547 | `				struct compact_data sData;` |
|      ! 0 |  9548 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - |  9549 | `				/* Recursively walk the array */` |
|      ! 0 |  9550 | `				sData.nRecCount = 0;` |
|      ! 0 |  9551 | `				sData.pArray = pArray;` |
|      ! 0 |  9552 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 |  9553 | `			}` |
|      ! 0 |  9554 | `		}else{` |
|        - |  9555 | `			/* Extract variable name */` |
|        5 |  9556 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 |  9557 | `			if( nLen > 0 ){` |
|        5 |  9558 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - |  9559 | `				/* Check if the variable is available in the current frame */` |
|        5 |  9560 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 |  9561 | `				if( pObj ){` |
|        5 |  9562 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 |  9563 | `				}` |
|        2 |  9564 | `			}` |
|        - |  9565 | `		}` |
|        3 |  9566 | `	}` |
|        - |  9567 | `	/* Return the array */` |
|        3 |  9568 | `	ph7_result_value(pCtx,pArray);` |
|        3 |  9569 | `	return PH7_OK;` |
|        2 |  9570 |  |
|        - |  9571 | `/*` |
|        - |  9572 | ` * The [extract()] function store it's state information in an instance` |
|        - |  9573 | ` * of the following structure.` |
|        - |  9574 | ` */` |
|        - |  9575 | `typedef struct extract_aux_data extract_aux_data;` |
|        - |  9576 | `struct extract_aux_data` |
|        - |  9577 |  |
|        - |  9578 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - |  9579 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - |  9580 | `	const char *zPrefix;  /* Prefix name */` |
|        - |  9581 | `	int Prefixlen;        /* Prefix  length */` |
|        - |  9582 | `	int iFlags;           /* Control flags */` |
|        - |  9583 | `	char zWorker[1024];   /* Working buffer */` |
|        - |  9584 | `};` |
|        - |  9585 | `/* Forward declaration */` |
|        - |  9586 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - |  9587 | `/*` |
|        - |  9588 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - |  9589 | ` *   Import variables into the current symbol table from an array.` |
|        - |  9590 | ` * Parameters` |
|        - |  9591 | ` * $var_array` |
|        - |  9592 | ` *  An associative array. This function treats keys as variable names and values` |
|        - |  9593 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - |  9594 | ` *  table, subject to extract_type and prefix parameters.` |
|        - |  9595 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - |  9596 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - |  9597 | ` * $extract_type` |
|        - |  9598 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - |  9599 | ` *  It can be one of the following values:` |
|        - |  9600 | ` *   EXTR_OVERWRITE` |
|        - |  9601 | ` *       If there is a collision, overwrite the existing variable.` |
|        - |  9602 | ` *   EXTR_SKIP` |
|        - |  9603 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - |  9604 | ` *   EXTR_PREFIX_SAME` |
|        - |  9605 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - |  9606 | ` *   EXTR_PREFIX_ALL` |
|        - |  9607 | ` *       Prefix all variable names with prefix.` |
|        - |  9608 | ` *   EXTR_PREFIX_INVALID` |
|        - |  9609 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - |  9610 | ` *   EXTR_IF_EXISTS` |
|        - |  9611 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - |  9612 | ` *       otherwise do nothing.` |
|        - |  9613 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - |  9614 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - |  9615 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - |  9616 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - |  9617 | ` *      the current symbol table.` |
|        - |  9618 | ` * $prefix` |
|        - |  9619 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - |  9620 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - |  9621 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - |  9622 | ` *  underscore character.` |
|        - |  9623 | ` * Return` |
|        - |  9624 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - |  9625 | ` */` |
|        4 |  9626 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9627 |  |
|        - |  9628 | `	extract_aux_data sAux;` |
|        - |  9629 | `	ph7_hashmap *pMap;` |
|        5 |  9630 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - |  9631 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 |  9632 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9633 | `		return PH7_OK;` |
|        - |  9634 | `	}` |
|        - |  9635 | `	/* Point to the target hashmap */` |
|        5 |  9636 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 |  9637 | `	if( pMap->nEntry < 1 ){` |
|        - |  9638 | `		/* Empty map,return  0 */` |
|      ! 0 |  9639 | `		ph7_result_int(pCtx,0);` |
|      ! 0 |  9640 | `		return PH7_OK;` |
|        - |  9641 | `	}` |
|        - |  9642 | `	/* Prepare the aux data */` |
|        5 |  9643 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 |  9644 | `	if( nArg > 1 ){` |
|        3 |  9645 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 |  9646 | `		if( nArg > 2 ){` |
|      ! 0 |  9647 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 |  9648 | `		}` |
|        1 |  9649 | `	}` |
|        5 |  9650 | `	sAux.pVm = pCtx->pVm;` |
|        - |  9651 | `	/* Invoke the worker callback */` |
|        5 |  9652 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - |  9653 | `	/* Number of variables successfully imported */` |
|        5 |  9654 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 |  9655 | `	return PH7_OK;` |
|        3 |  9656 |  |
|        - |  9657 | `/*` |
|        - |  9658 | ` * Worker callback for the [extract()] function defined` |
|        - |  9659 | ` * below.` |
|        - |  9660 | ` */` |
|        8 |  9661 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9662 |  |
|        9 |  9663 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 |  9664 | `	int iFlags = pAux->iFlags;` |
|        9 |  9665 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9666 | `	ph7_value *pObj;` |
|        - |  9667 | `	SyString sVar;` |
|        9 |  9668 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 |  9669 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 |  9670 | `	}` |
|        - |  9671 | `	/* Perform a string cast */` |
|        9 |  9672 | `	PH7_MemObjToString(pKey);` |
|        9 |  9673 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9674 | `		/* Unavailable variable name */` |
|      ! 0 |  9675 | `		return SXRET_OK;` |
|        - |  9676 | `	}` |
|        9 |  9677 | `	sVar.nByte = 0; /* cc warning */` |
|        9 |  9678 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 |  9679 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9680 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9681 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9682 | `			);` |
|      ! 0 |  9683 | `	}else{` |
|       13 |  9684 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 |  9685 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9686 | `	}` |
|        9 |  9687 | `	sVar.zString = pAux->zWorker;` |
|        - |  9688 | `	/* Try to extract the variable */` |
|        9 |  9689 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 |  9690 | `	if( pObj ){` |
|        - |  9691 | `		/* Collision */` |
|        5 |  9692 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 |  9693 | `			return SXRET_OK;` |
|        - |  9694 | `		}` |
|        5 |  9695 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 |  9696 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - |  9697 | `				/* Already prefixed */` |
|      ! 0 |  9698 | `				return SXRET_OK;` |
|        - |  9699 | `			}` |
|      ! 0 |  9700 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 |  9701 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 |  9702 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9703 | `				);` |
|      ! 0 |  9704 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 |  9705 | `		}` |
|        3 |  9706 | `	}else{` |
|        - |  9707 | `		/* Create the variable */` |
|        5 |  9708 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - |  9709 | `	}` |
|        9 |  9710 | `	if( pObj ){` |
|        - |  9711 | `		/* Overwrite the old value */` |
|        9 |  9712 | `		PH7_MemObjStore(pValue,pObj);` |
|        - |  9713 | `		/* Increment counter */` |
|        9 |  9714 | `		pAux->iCount++;` |
|        4 |  9715 | `	}` |
|        9 |  9716 | `	return SXRET_OK;` |
|        5 |  9717 |  |
|        - |  9718 | `/*` |
|        - |  9719 | ` * Worker callback for the [import_request_variables()] function` |
|        - |  9720 | ` * defined below.` |
|        - |  9721 | ` */` |
|        2 |  9722 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  9723 |  |
|        3 |  9724 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 |  9725 | `	ph7_vm *pVm = pAux->pVm;` |
|        - |  9726 | `	ph7_value *pObj;` |
|        - |  9727 | `	SyString sVar;` |
|        - |  9728 | `	/* Perform a string cast */` |
|        3 |  9729 | `	PH7_MemObjToString(pKey);` |
|        3 |  9730 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - |  9731 | `		/* Unavailable variable name */` |
|      ! 0 |  9732 | `		return SXRET_OK;` |
|        - |  9733 | `	}` |
|        3 |  9734 | `	sVar.nByte = 0; /* cc warning */` |
|        3 |  9735 | `	if( pAux->Prefixlen > 0 ){` |
|        4 |  9736 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 |  9737 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 |  9738 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - |  9739 | `			);` |
|        2 |  9740 | `	}else{` |
|      ! 0 |  9741 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 |  9742 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - |  9743 | `	}` |
|        3 |  9744 | `	sVar.zString = pAux->zWorker;` |
|        - |  9745 | `	/* Extract the variable */` |
|        3 |  9746 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 |  9747 | `	if( pObj ){` |
|        3 |  9748 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 |  9749 | `	}` |
|        3 |  9750 | `	return SXRET_OK;` |
|        2 |  9751 |  |
|        - |  9752 | `/*` |
|        - |  9753 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - |  9754 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - |  9755 | ` * Parameters` |
|        - |  9756 | ` * $types` |
|        - |  9757 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - |  9758 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - |  9759 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - |  9760 | ` *  POST includes the POST uploaded file information.` |
|        - |  9761 | ` *  Note:` |
|        - |  9762 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - |  9763 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - |  9764 | ` * $prefix` |
|        - |  9765 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - |  9766 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - |  9767 | ` *  variable named $pref_userid.` |
|        - |  9768 | ` * Return` |
|        - |  9769 | ` *  TRUE on success or FALSE on failure.` |
|        - |  9770 | ` */` |
|        2 |  9771 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 |  9772 |  |
|        - |  9773 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - |  9774 | `	extract_aux_data sAux;` |
|        - |  9775 | `	int nLen,nPrefixLen;` |
|        - |  9776 | `	ph7_value *pSuper;` |
|        - |  9777 | `	ph7_vm *pVm;` |
|        - |  9778 | `	/* By default import only $_GET variables  */` |
|        3 |  9779 | `	zImport = "G";` |
|        3 |  9780 | `	nLen = (int)sizeof(char);` |
|        3 |  9781 | `	zPrefix = 0;` |
|        3 |  9782 | `	nPrefixLen = 0;` |
|        3 |  9783 | `	if( nArg > 0 ){` |
|        3 |  9784 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 |  9785 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 |  9786 | `		}` |
|        3 |  9787 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 |  9788 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 |  9789 | `		}` |
|        1 |  9790 | `	}` |
|        - |  9791 | `	/* Point to the underlying VM */` |
|        3 |  9792 | `	pVm = pCtx->pVm;` |
|        - |  9793 | `	/* Initialize the aux data */` |
|        3 |  9794 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 |  9795 | `	sAux.zPrefix = zPrefix;` |
|        3 |  9796 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 |  9797 | `	sAux.pVm = pVm;` |
|        - |  9798 | `	/* Extract */` |
|        3 |  9799 | `	zEnd = &zImport[nLen];` |
|        5 |  9800 | `	while( zImport < zEnd ){` |
|        3 |  9801 | `		int c = zImport[0];` |
|        3 |  9802 | `		pSuper = 0;` |
|        3 |  9803 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - |  9804 | `			/* Import $_GET variables */` |
|        3 |  9805 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 |  9806 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - |  9807 | `			/* Import $_POST variables */` |
|      ! 0 |  9808 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 |  9809 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - |  9810 | `			/* Import $_COOKIE variables */` |
|      ! 0 |  9811 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 |  9812 | `		}` |
|        3 |  9813 | `		if( pSuper ){` |
|        - |  9814 | `			/* Iterate throw array entries */` |
|        3 |  9815 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 |  9816 | `		}` |
|        - |  9817 | `		/* Advance the cursor */` |
|        3 |  9818 | `		zImport++;` |
|        1 |  9819 | `	}` |
|        - |  9820 | `	/* All done,return TRUE*/` |
|        3 |  9821 | `	ph7_result_bool(pCtx,0);` |
|        3 |  9822 | `	return PH7_OK;` |
|        1 |  9823 |  |
|        - |  9824 | `/*` |
|        - |  9825 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - |  9826 | ` * Refer to the eval() language construct implementation for more` |
|        - |  9827 | ` * information.` |
|        - |  9828 | ` */` |
|    10098 |  9829 | `static sxi32 VmEvalChunk(` |
|        - |  9830 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - |  9831 | `	ph7_context *pCtx,  /* Call Context */` |
|        - |  9832 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - |  9833 | `	int iFlags,         /* Compile flag */` |
|        - |  9834 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - |  9835 | `	)` |
|        2 |  9836 |  |
|        - |  9837 | `	SySet *pByteCode,aByteCode;` |
|        - |  9838 | `	SyBlob sSavedNs;` |
|    10100 |  9839 | `	ProcConsumer xErr = 0;` |
|    10100 |  9840 | `	void *pErrData = 0;` |
|        - |  9841 | `	/* Initialize bytecode container */` |
|    10100 |  9842 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    10100 |  9843 | `	SySetAlloc(&aByteCode,0x20);` |
|        - |  9844 | `	/* Reset the code generator */` |
|    10100 |  9845 | `	if( bTrueReturn ){` |
|        - |  9846 | `		/* Included file,log compile-time errors */` |
|     7535 |  9847 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     7535 |  9848 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     3767 |  9849 | `	}` |
|    10100 |  9850 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - |  9851 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - |  9852 | `	 * Each included file has its own namespace scope; after execution,` |
|        - |  9853 | `	 * the caller's namespace is restored. */` |
|    10100 |  9854 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    10100 |  9855 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    10100 |  9856 | `	if( bTrueReturn ){` |
|        - |  9857 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     7535 |  9858 | `		SyBlobReset(&pVm->sNamespace);` |
|     3767 |  9859 | `	}` |
|        - |  9860 | `	/* Swap bytecode container */` |
|    10100 |  9861 | `	pByteCode = pVm->pByteContainer;` |
|    10100 |  9862 | `	pVm->pByteContainer = &aByteCode;` |
|        - |  9863 | `	/* Compile the chunk */` |
|    10100 |  9864 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    15149 |  9865 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - |  9866 | `		/* Compilation error,return false */` |
|        3 |  9867 | `		if( pCtx ){` |
|        3 |  9868 | `			ph7_result_bool(pCtx,0);` |
|        1 |  9869 | `		}` |
|        2 |  9870 | `	}else{` |
|        - |  9871 | `		/* Mount any newly defined classes */` |
|        - |  9872 | `		SyHashEntry *pEntry;` |
|        - |  9873 | `		ph7_class *pClass;` |
|        - |  9874 | `		ph7_value sResult; /* Return value */` |
|        - |  9875 | `		sxi32 rc;` |
|    10098 |  9876 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   276318 |  9877 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   261174 |  9878 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - |  9879 | `			/* Only mount classes that haven't been mounted yet */` |
|   261174 |  9880 | `			if( !pClass->bMounted ){` |
|    62692 |  9881 | `				rc = VmMountUserClass(pVm,pClass);` |
|    62692 |  9882 | `				if( rc != SXRET_OK ){` |
|        - |  9883 | `					/* Mount failure (likely memory error) */` |
|      ! 0 |  9884 | `					if( pCtx ){` |
|      ! 0 |  9885 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 |  9886 | `					}` |
|      ! 0 |  9887 | `					goto Cleanup;` |
|        - |  9888 | `				}` |
|    31345 |  9889 | `			}` |
|        2 |  9890 | `		}` |
|    10098 |  9891 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - |  9892 | `			/* Out of memory */` |
|      ! 0 |  9893 | `			if( pCtx ){` |
|      ! 0 |  9894 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 |  9895 | `			}` |
|      ! 0 |  9896 | `			goto Cleanup;` |
|        - |  9897 | `		}` |
|    10098 |  9898 | `		if( bTrueReturn ){` |
|        - |  9899 | `			/* Assume a boolean true return value */` |
|     7535 |  9900 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     3768 |  9901 | `		}else{` |
|        - |  9902 | `			/* Assume a null return value */` |
|     2564 |  9903 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  9904 | `		}` |
|        - |  9905 | `		/* Execute the compiled chunk */` |
|    10098 |  9906 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    10098 |  9907 | `		if( pCtx ){` |
|        - |  9908 | `			/* Set the execution result */` |
|     7548 |  9909 | `			ph7_result_value(pCtx,&sResult);` |
|     3773 |  9910 | `		}` |
|    10098 |  9911 | `		PH7_MemObjRelease(&sResult);` |
|        - |  9912 | `	}` |
|     5049 |  9913 | `Cleanup:` |
|        - |  9914 | `	/* Cleanup the mess left behind */` |
|    10100 |  9915 | `	pVm->pByteContainer = pByteCode;` |
|    10100 |  9916 | `	SySetRelease(&aByteCode);` |
|        - |  9917 | `	/* Restore caller's namespace state */` |
|    10100 |  9918 | `	SyBlobReset(&pVm->sNamespace);` |
|    10100 |  9919 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    10100 |  9920 | `	SyBlobRelease(&sSavedNs);` |
|    10100 |  9921 | `	return SXRET_OK;` |
|        2 |  9922 |  |
|        - |  9923 | `/*` |
|        - |  9924 | ` * value eval(string $code)` |
|        - |  9925 | ` *   Evaluate a string as PHP code.` |
|        - |  9926 | ` * Parameter` |
|        - |  9927 | ` *  code: PHP code to evaluate.` |
|        - |  9928 | ` * Return` |
|        - |  9929 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - |  9930 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - |  9931 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - |  9932 | ` */` |
|       16 |  9933 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 |  9934 |  |
|        - |  9935 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       18 |  9936 | `	if( nArg < 1 ){` |
|        - |  9937 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 |  9938 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9939 | `		return SXRET_OK;` |
|        - |  9940 | `	}` |
|        - |  9941 | `	/* Chunk to evaluate */` |
|       18 |  9942 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       18 |  9943 | `	if( sChunk.nByte < 1 ){` |
|        - |  9944 | `		/* Empty string,return NULL */` |
|        3 |  9945 | `		ph7_result_null(pCtx);` |
|        3 |  9946 | `		return SXRET_OK;` |
|        - |  9947 | `	}` |
|        - |  9948 | `	/* Eval the chunk */` |
|       16 |  9949 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       16 |  9950 | `	return SXRET_OK;` |
|       10 |  9951 |  |
|        - |  9952 | `/*` |
|        - |  9953 | ` * Check if a file path is already included.` |
|        - |  9954 | ` */` |
|    15064 |  9955 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        1 |  9956 |  |
|        - |  9957 | `	SyString *aEntries;` |
|        - |  9958 | `	sxu32 n;` |
|    15065 |  9959 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - |  9960 | `	/* Perform a linear search */` |
| 56720651 |  9961 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 56705593 |  9962 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - |  9963 | `			/* Already included */` |
|        7 |  9964 | `			return TRUE;` |
|        - |  9965 | `		}` |
| 28352794 |  9966 | `	}` |
|    15059 |  9967 | `	return FALSE;` |
|     7533 |  9968 |  |
|        - |  9969 | `/*` |
|        - |  9970 | ` * Push a file path in the appropriate VM container.` |
|        - |  9971 | ` */` |
|    17606 |  9972 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 |  9973 |  |
|        - |  9974 | `	SyString sPath;` |
|        - |  9975 | `	char *zDup;` |
|        - |  9976 | `#ifdef __WINNT__` |
|        - |  9977 | `	char *zCur;` |
|        - |  9978 | `#endif` |
|        - |  9979 | `	sxi32 rc;` |
|    17608 |  9980 | `	if( nLen < 0 ){` |
|     2544 |  9981 | `		nLen = SyStrlen(zPath);` |
|     1271 |  9982 | `	}` |
|        - |  9983 | `	/* Duplicate the file path first */` |
|    17608 |  9984 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    17608 |  9985 | `	if( zDup == 0 ){` |
|      ! 0 |  9986 | `		return SXERR_MEM;` |
|        - |  9987 | `	}` |
|        - |  9988 | `#ifdef __WINNT__` |
|        - |  9989 | `	/* Normalize path on windows` |
|        - |  9990 | `	 * Example:` |
|        - |  9991 | `	 *    Path/To/File.php` |
|        - |  9992 | `	 * becomes` |
|        - |  9993 | `	 *   path\to\file.php` |
|        - |  9994 | `	 */` |
|        2 |  9995 | `	zCur = zDup;` |
|        2 |  9996 | `	while( zCur[0] != 0 ){` |
|        2 |  9997 | `		if( zCur[0] == '/' ){` |
|        2 |  9998 | `			zCur[0] = '\\';` |
|        2 |  9999 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 10000 | `			int c = SyToLower(zCur[0]);` |
|        1 | 10001 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 10002 | `		}` |
|        2 | 10003 | `		zCur++;` |
|        2 | 10004 | `	}` |
|        - | 10005 | `#endif` |
|        - | 10006 | `	/* Install the file path */` |
|    17608 | 10007 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    17608 | 10008 | `	if( !bMain ){` |
|    15065 | 10009 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 10010 | `			/* Already included */` |
|        7 | 10011 | `			*pNew = 0;` |
|        4 | 10012 | `		}else{` |
|        - | 10013 | `			/* Insert in the corresponding container */` |
|    15059 | 10014 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    15059 | 10015 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 10016 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 10017 | `				return rc;` |
|        - | 10018 | `			}` |
|    15059 | 10019 | `			*pNew = 1;` |
|        - | 10020 | `		}` |
|     7532 | 10021 | `	}` |
|    17608 | 10022 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    17608 | 10023 | `	return SXRET_OK;` |
|     8805 | 10024 |  |
|        - | 10025 | `/*` |
|        - | 10026 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 10027 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 10028 | ` * indicates failure.` |
|        - | 10029 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 10030 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 10031 | ` * operations.` |
|        - | 10032 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 10033 | ` * this function is a no-op.` |
|        - | 10034 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 10035 | ` * constructs for more information.` |
|        - | 10036 | ` */` |
|     7540 | 10037 | `static sxi32 VmExecIncludedFile(` |
|        - | 10038 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 10039 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 10040 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 10041 | `	 )` |
|        2 | 10042 |  |
|        - | 10043 | `	sxi32 rc;` |
|        - | 10044 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10045 | `	const ph7_io_stream *pStream;` |
|        - | 10046 | `	SyBlob sContents;` |
|        - | 10047 | `	void *pHandle;` |
|        - | 10048 | `	ph7_vm *pVm;` |
|        - | 10049 | `	int isNew;` |
|        - | 10050 | `	/* Initialize fields */` |
|     7542 | 10051 | `	pVm = pCtx->pVm;` |
|     7542 | 10052 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     7542 | 10053 | `	isNew = 0;` |
|        - | 10054 | `	/* Extract the associated stream */` |
|     7542 | 10055 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 10056 | `	/*` |
|        - | 10057 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 10058 | `	 * in a read-only mode.` |
|        - | 10059 | `	 */` |
|     7542 | 10060 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     7542 | 10061 | `	if( pHandle == 0 ){` |
|        3 | 10062 | `		return SXERR_IO;` |
|        - | 10063 | `	}` |
|     7539 | 10064 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     7539 | 10065 | `	if( IncludeOnce && !isNew ){` |
|        - | 10066 | `		/* Already included */` |
|        5 | 10067 | `		rc = SXERR_EXISTS;` |
|        3 | 10068 | `	}else{` |
|        - | 10069 | `		/* Read the whole file contents */` |
|     7535 | 10070 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     7535 | 10071 | `		if( rc == SXRET_OK ){` |
|        - | 10072 | `			SyString sScript;` |
|        - | 10073 | `			/* Compile and execute the script */` |
|     7535 | 10074 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     7535 | 10075 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     3767 | 10076 | `		}` |
|        - | 10077 | `	}` |
|        - | 10078 | `	/* Pop from the set of included file */` |
|     7539 | 10079 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 10080 | `	/* Close the handle */` |
|     7539 | 10081 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 10082 | `	/* Release the working buffer */` |
|     7539 | 10083 | `	SyBlobRelease(&sContents);` |
|        - | 10084 | `#else` |
|        - | 10085 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 10086 | `	SXUNUSED(pPath);` |
|        - | 10087 | `	SXUNUSED(IncludeOnce);` |
|        - | 10088 | `	rc = SXERR_IO;` |
|        - | 10089 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     7539 | 10090 | `	return rc;` |
|     3772 | 10091 |  |
|        - | 10092 | `/*` |
|        - | 10093 | ` * string get_include_path(void)` |
|        - | 10094 | ` *  Gets the current include_path configuration option.` |
|        - | 10095 | ` * Parameter` |
|        - | 10096 | ` *  None` |
|        - | 10097 | ` * Return` |
|        - | 10098 | ` *  Included paths as a string` |
|        - | 10099 | ` */` |
|        2 | 10100 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10101 |  |
|        3 | 10102 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10103 | `	SyString *aEntry;` |
|        - | 10104 | `	int dir_sep;` |
|        - | 10105 | `	sxu32 n;` |
|        - | 10106 | `#ifdef __WINNT__` |
|        1 | 10107 | `	dir_sep = ';';` |
|        - | 10108 | `#else` |
|        - | 10109 | `	/* Assume UNIX path separator */` |
|        2 | 10110 | `	dir_sep = ':';` |
|        - | 10111 | `#endif` |
|        1 | 10112 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 10113 | `	SXUNUSED(apArg);` |
|        - | 10114 | `	/* Point to the list of import paths */` |
|        3 | 10115 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 10116 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 10117 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 10118 | `		if( n > 0 ){` |
|        - | 10119 | `			/* Append dir seprator */` |
|      ! 0 | 10120 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 10121 | `		}` |
|        - | 10122 | `		/* Append path */` |
|        3 | 10123 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 10124 | `	}` |
|        3 | 10125 | `	return PH7_OK;` |
|        1 | 10126 |  |
|        - | 10127 | `/*` |
|        - | 10128 | ` * string get_get_included_files(void)` |
|        - | 10129 | ` *  Gets the current include_path configuration option.` |
|        - | 10130 | ` * Parameter` |
|        - | 10131 | ` *  None` |
|        - | 10132 | ` * Return` |
|        - | 10133 | ` *  Included paths as a string` |
|        - | 10134 | ` */` |
|        2 | 10135 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10136 |  |
|        3 | 10137 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 10138 | `	ph7_value *pArray,*pWorker;` |
|        - | 10139 | `	SyString *pEntry;` |
|        - | 10140 | `	int c,d;` |
|        - | 10141 | `	/* Create an array and a working value */` |
|        3 | 10142 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 10143 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 10144 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 10145 | `		/* Out of memory,return null */` |
|      ! 0 | 10146 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10147 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10148 | `		SXUNUSED(apArg);` |
|      ! 0 | 10149 | `		return PH7_OK;` |
|        - | 10150 | `	}` |
|        3 | 10151 | `	c = d = '/';` |
|        - | 10152 | `#ifdef __WINNT__` |
|        1 | 10153 | `	d = '\\';` |
|        - | 10154 | `#endif` |
|        - | 10155 | `	/* Iterate throw entries */` |
|        3 | 10156 | `	SySetResetCursor(pFiles);` |
|     3691 | 10157 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 10158 | `		const char *zBase,*zEnd;` |
|        - | 10159 | `		int iLen;` |
|        - | 10160 | `		/* reset the string cursor */` |
|     3689 | 10161 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 10162 | `		/* Extract base name */` |
|     3689 | 10163 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 10164 | `		/* Ignore trailing '/' */` |
|     5533 | 10165 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 10166 | `			zEnd--;` |
|      ! 0 | 10167 | `		}` |
|     3689 | 10168 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   113825 | 10169 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   108293 | 10170 | `			zEnd--;` |
|        1 | 10171 | `		}` |
|     3689 | 10172 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3689 | 10173 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 10174 | `		/* Copy entry name */` |
|     3689 | 10175 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 10176 | `		/* Perform the insertion */` |
|     3689 | 10177 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 10178 | `	}` |
|        - | 10179 | `	/* All done,return the created array */` |
|        3 | 10180 | `	ph7_result_value(pCtx,pArray);` |
|        - | 10181 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 10182 | `	 * by the engine as soon we return from this foreign` |
|        - | 10183 | `	 * function.` |
|        - | 10184 | `	 */` |
|        3 | 10185 | `	return PH7_OK;` |
|        2 | 10186 |  |
|        - | 10187 | `/*` |
|        - | 10188 | ` * include:` |
|        - | 10189 | ` * According to the PHP reference manual.` |
|        - | 10190 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 10191 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 10192 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 10193 | ` *  include() will finally check in the calling script's own directory` |
|        - | 10194 | ` *  and the current working directory before failing. The include()` |
|        - | 10195 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 10196 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 10197 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 10198 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 10199 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 10200 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 10201 | ` *  directory to find the requested file.` |
|        - | 10202 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 10203 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 10204 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 10205 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 10206 | ` */` |
|     7528 | 10207 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10208 |  |
|        - | 10209 | `	SyString sFile;` |
|        - | 10210 | `	sxi32 rc;` |
|     7530 | 10211 | `	if( nArg < 1 ){` |
|        - | 10212 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10213 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10214 | `		return SXRET_OK;` |
|        - | 10215 | `	}` |
|        - | 10216 | `	/* File to include */` |
|     7530 | 10217 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     7530 | 10218 | `	if( sFile.nByte < 1 ){` |
|        - | 10219 | `		/* Empty string,return NULL */` |
|      ! 0 | 10220 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10221 | `		return SXRET_OK;` |
|        - | 10222 | `	}` |
|        - | 10223 | `	/* Open,compile and execute the desired script */` |
|     7530 | 10224 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     7530 | 10225 | `	if( rc != SXRET_OK ){` |
|        - | 10226 | `		/* Emit a warning and return false */` |
|        3 | 10227 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 10228 | `		ph7_result_bool(pCtx,0);` |
|        1 | 10229 | `	}` |
|     7530 | 10230 | `	return SXRET_OK;` |
|     3766 | 10231 |  |
|        - | 10232 | `/*` |
|        - | 10233 | ` * include_once:` |
|        - | 10234 | ` *  According to the PHP reference manual.` |
|        - | 10235 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 10236 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 10237 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 10238 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 10239 | ` *   just once.` |
|        - | 10240 | ` */` |
|        4 | 10241 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10242 |  |
|        - | 10243 | `	SyString sFile;` |
|        - | 10244 | `	sxi32 rc;` |
|        5 | 10245 | `	if( nArg < 1 ){` |
|        - | 10246 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10247 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10248 | `		return SXRET_OK;` |
|        - | 10249 | `	}` |
|        - | 10250 | `	/* File to include */` |
|        5 | 10251 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10252 | `	if( sFile.nByte < 1 ){` |
|        - | 10253 | `		/* Empty string,return NULL */` |
|      ! 0 | 10254 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10255 | `		return SXRET_OK;` |
|        - | 10256 | `	}` |
|        - | 10257 | `	/* Open,compile and execute the desired script */` |
|        5 | 10258 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10259 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10260 | `		/* File already included,return TRUE */` |
|        3 | 10261 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10262 | `		return SXRET_OK;` |
|        - | 10263 | `	}` |
|        3 | 10264 | `	if( rc != SXRET_OK ){` |
|        - | 10265 | `		/* Emit a warning and return false */` |
|      ! 0 | 10266 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10267 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10268 | ` 	}` |
|        3 | 10269 | `	return SXRET_OK;` |
|        3 | 10270 |  |
|        - | 10271 | `/*` |
|        - | 10272 | ` * require.` |
|        - | 10273 | ` *  According to the PHP reference manual.` |
|        - | 10274 | ` *   require() is identical to include() except upon failure it will` |
|        - | 10275 | ` *   also produce a fatal level error.` |
|        - | 10276 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 10277 | ` *   emits a warning  which allows the script to continue.` |
|        - | 10278 | ` */` |
|        4 | 10279 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10280 |  |
|        - | 10281 | `	SyString sFile;` |
|        - | 10282 | `	sxi32 rc;` |
|        5 | 10283 | `	if( nArg < 1 ){` |
|        - | 10284 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10285 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10286 | `		return SXRET_OK;` |
|        - | 10287 | `	}` |
|        - | 10288 | `	/* File to include */` |
|        5 | 10289 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10290 | `	if( sFile.nByte < 1 ){` |
|        - | 10291 | `		/* Empty string,return NULL */` |
|      ! 0 | 10292 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10293 | `		return SXRET_OK;` |
|        - | 10294 | `	}` |
|        - | 10295 | `	/* Open,compile and execute the desired script */` |
|        5 | 10296 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        5 | 10297 | `	if( rc != SXRET_OK ){` |
|        - | 10298 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10299 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10300 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10301 | `		return PH7_ABORT;` |
|        - | 10302 | `	}` |
|        5 | 10303 | `	return SXRET_OK;` |
|        3 | 10304 |  |
|        - | 10305 | `/*` |
|        - | 10306 | ` * require_once:` |
|        - | 10307 | ` *  According to the PHP reference manual.` |
|        - | 10308 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 10309 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 10310 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 10311 | ` *   and how it differs from its non _once siblings.` |
|        - | 10312 | ` */` |
|        4 | 10313 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10314 |  |
|        - | 10315 | `	SyString sFile;` |
|        - | 10316 | `	sxi32 rc;` |
|        5 | 10317 | `	if( nArg < 1 ){` |
|        - | 10318 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 10319 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10320 | `		return SXRET_OK;` |
|        - | 10321 | `	}` |
|        - | 10322 | `	/* File to include */` |
|        5 | 10323 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 10324 | `	if( sFile.nByte < 1 ){` |
|        - | 10325 | `		/* Empty string,return NULL */` |
|      ! 0 | 10326 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10327 | `		return SXRET_OK;` |
|        - | 10328 | `	}` |
|        - | 10329 | `	/* Open,compile and execute the desired script */` |
|        5 | 10330 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 10331 | `	if( rc == SXERR_EXISTS ){` |
|        - | 10332 | `		/* File already included,return TRUE */` |
|        3 | 10333 | `		ph7_result_bool(pCtx,1);` |
|        3 | 10334 | `		return SXRET_OK;` |
|        - | 10335 | `	}` |
|        3 | 10336 | `	if( rc != SXRET_OK ){` |
|        - | 10337 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 10338 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 10339 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10340 | `		return PH7_ABORT;` |
|        - | 10341 | `	}` |
|        3 | 10342 | `	return SXRET_OK;` |
|        3 | 10343 |  |
|        - | 10344 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 10345 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 10346 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 10347 | `/* Table of built-in VM functions. */` |
|        - | 10348 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 10349 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 10350 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 10351 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 10352 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 10353 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 10354 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 10355 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 10356 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 10357 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 10358 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 10359 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 10360 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 10361 | `	    /* Constants management */` |
|        - | 10362 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 10363 | `	{ "define",   vm_builtin_define               },` |
|        - | 10364 | `	{ "constant", vm_builtin_constant             },` |
|        - | 10365 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 10366 | `	   /* Class/Object functions */` |
|        - | 10367 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 10368 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 10369 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 10370 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 10371 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 10372 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 10373 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 10374 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 10375 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 10376 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 10377 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 10378 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 10379 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 10380 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 10381 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 10382 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 10383 | `	   /* Random numbers/strings generators */` |
|        - | 10384 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 10385 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 10386 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 10387 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 10388 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 10389 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10390 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 10391 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 10392 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 10393 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10394 | `	   /* Language constructs functions */` |
|        - | 10395 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 10396 | `	{ "print", vm_builtin_print                   },` |
|        - | 10397 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 10398 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 10399 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 10400 | `	  /* Variable handling functions */` |
|        - | 10401 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 10402 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 10403 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 10404 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 10405 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 10406 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 10407 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 10408 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 10409 | `	  /* Ouput control functions */` |
|        - | 10410 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 10411 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 10412 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 10413 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 10414 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 10415 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 10416 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 10417 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 10418 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 10419 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 10420 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 10421 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 10422 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 10423 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 10424 | `	  /* Assertion functions */` |
|        - | 10425 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 10426 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 10427 | `	  /* Error reporting functions */` |
|        - | 10428 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 10429 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 10430 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 10431 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 10432 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 10433 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 10434 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 10435 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 10436 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 10437 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 10438 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 10439 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 10440 | `	  /* Release info */` |
|        - | 10441 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 10442 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 10443 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 10444 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 10445 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 10446 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 10447 | `	  /* hashmap */` |
|        - | 10448 | `	{"compact",          vm_builtin_compact       },` |
|        - | 10449 | `	{"extract",          vm_builtin_extract       },` |
|        - | 10450 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 10451 | `	  /* URL related function */` |
|        - | 10452 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 10453 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 10454 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 10455 | `	   /* XML processing functions */` |
|        - | 10456 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 10457 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 10458 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 10459 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 10460 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 10461 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 10462 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 10463 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 10464 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 10465 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 10466 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 10467 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 10468 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 10469 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 10470 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 10471 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 10472 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 10473 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 10474 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 10475 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 10476 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 10477 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 10478 | `	   /* UTF-8 encoding/decoding */` |
|        - | 10479 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 10480 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 10481 | `	   /* Command line processing */` |
|        - | 10482 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 10483 | `	   /* JSON encoding/decoding */` |
|        - | 10484 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 10485 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 10486 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 10487 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 10488 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 10489 | `	   /* Files/URI inclusion facility */` |
|        - | 10490 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 10491 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 10492 | `	{ "include",      vm_builtin_include          },` |
|        - | 10493 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 10494 | `	{ "require",      vm_builtin_require          },` |
|        - | 10495 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 10496 | `};` |
|        - | 10497 | `/*` |
|        - | 10498 | ` * Register the built-in VM functions defined above.` |
|        - | 10499 | ` */` |
|     2288 | 10500 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 10501 |  |
|        - | 10502 | `	sxi32 rc;` |
|        - | 10503 | `	sxu32 n;` |
|   286002 | 10504 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 10505 | `		/* Note that these special functions have access` |
|        - | 10506 | `		 * to the underlying virtual machine as their` |
|        - | 10507 | `		 * private data.` |
|        - | 10508 | `		 */` |
|   283714 | 10509 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   283714 | 10510 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 10511 | `			return rc;` |
|        - | 10512 | `		}` |
|   141858 | 10513 | `	}` |
|     2290 | 10514 | `	return SXRET_OK;` |
|     1146 | 10515 |  |
|        - | 10516 | `/*` |
|        - | 10517 | ` * Check if the given name refer to an installed class.` |
|        - | 10518 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 10519 | ` */` |
|    16738 | 10520 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 10521 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 10522 | `	const char *zName,  /* Name of the target class */` |
|        - | 10523 | `	sxu32 nByte,        /* zName length */` |
|        - | 10524 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 10525 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 10526 | `						 */` |
|        - | 10527 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 10528 | `	)` |
|        2 | 10529 |  |
|        - | 10530 | `	SyHashEntry *pEntry;` |
|        - | 10531 | `	ph7_class *pClass;` |
|     8369 | 10532 | `	SXUNUSED(iNest);` |
|        - | 10533 | `	/* Exact class lookup.` |
|        - | 10534 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 10535 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    16740 | 10536 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    16740 | 10537 | `	if( pEntry == 0 ){` |
|       10 | 10538 | `		return 0;` |
|        - | 10539 | `	}` |
|    16732 | 10540 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    16732 | 10541 | `	if( !iLoadable ){` |
|    15592 | 10542 | `		return pClass;` |
|        - | 10543 | `	}` |
|        - | 10544 | `	/* Filter for loadable classes (skip interfaces/abstract/traits) */` |
|     1142 | 10545 | `	while(pClass){` |
|     1142 | 10546 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1142 | 10547 | `			return pClass;` |
|        - | 10548 | `		}` |
|      ! 0 | 10549 | `		pClass = pClass->pNextName;` |
|      ! 0 | 10550 | `	}` |
|      ! 0 | 10551 | `	return 0;` |
|     8371 | 10552 |  |
|        - | 10553 | `/*` |
|        - | 10554 | ` * Reference Table Implementation` |
|        - | 10555 | ` * Status: stable <chm@symisc.net>` |
|        - | 10556 | ` * Intro` |
|        - | 10557 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 10558 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 10559 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 10560 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 10561 | ` *  Refer to the official for more information on this powerful` |
|        - | 10562 | ` *  extension.` |
|        - | 10563 | ` */` |
|        - | 10564 | `/*` |
|        - | 10565 | ` * Allocate a new reference entry.` |
|        - | 10566 | ` */` |
|  2996426 | 10567 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 10568 |  |
|        - | 10569 | `	VmRefObj *pRef;` |
|        - | 10570 | `	/* Allocate a new instance */` |
|  2996428 | 10571 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  2996428 | 10572 | `	if( pRef == 0 ){` |
|      ! 0 | 10573 | `		return 0;` |
|        - | 10574 | `	}` |
|        - | 10575 | `	/* Zero the structure */` |
|  2996428 | 10576 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 10577 | `	/* Initialize fields */` |
|  2996428 | 10578 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  2996428 | 10579 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  2996428 | 10580 | `	pRef->nIdx = nIdx;` |
|  2996428 | 10581 | `	return pRef;` |
|  1498215 | 10582 |  |
|        - | 10583 | `/*` |
|        - | 10584 | ` * Default hash function used by the reference table` |
|        - | 10585 | ` * for lookup/insertion operations.` |
|        - | 10586 | ` */` |
| 16621884 | 10587 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 10588 |  |
|        - | 10589 | `	/* Calculate the hash based on the memory object index */` |
| 16621886 | 10590 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 10591 |  |
|        - | 10592 | `/*` |
|        - | 10593 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 10594 | ` * in the reference table.` |
|        - | 10595 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 10596 | ` * otherwise.` |
|        - | 10597 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10598 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10599 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10600 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10601 | ` * Refer to the official for more information on this powerful` |
|        - | 10602 | ` * extension.` |
|        - | 10603 | ` */` |
|  8944504 | 10604 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 10605 |  |
|        - | 10606 | `	VmRefObj *pRef;` |
|        - | 10607 | `	sxu32 nBucket;` |
|        - | 10608 | `	/* Point to the appropriate bucket */` |
|  8944506 | 10609 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 10610 | `	/* Perform the lookup */` |
|  8944506 | 10611 | `	pRef = pVm->apRefObj[nBucket];` |
| 18835993 | 10612 | `	for(;;){` |
| 37664230 | 10613 | `		if( pRef == 0 ){` |
|  3072600 | 10614 | `			break;` |
|        - | 10615 | `		}` |
| 34591632 | 10616 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 10617 | `			/* Entry found */` |
|  5871908 | 10618 | `			return pRef;` |
|        - | 10619 | `		}` |
|        - | 10620 | `		/* Point to the next entry */` |
| 28719726 | 10621 | `		pRef = pRef->pNextCollide;` |
|        2 | 10622 | `	}` |
|        - | 10623 | `	/* No such entry,return NULL */` |
|  3072600 | 10624 | `	return 0;` |
|  4472254 | 10625 |  |
|        - | 10626 | `/*` |
|        - | 10627 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10628 | ` *` |
|        - | 10629 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10630 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10631 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10632 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10633 | ` * Refer to the official for more information on this powerful` |
|        - | 10634 | ` * extension.` |
|        - | 10635 | ` */` |
|  2996426 | 10636 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10637 |  |
|        - | 10638 | `	sxu32 nBucket;` |
|  2996428 | 10639 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 10640 | `		VmRefObj **apNew;` |
|        - | 10641 | `		sxu32 nNew;` |
|        - | 10642 | `		/* Allocate a larger table */` |
|     3588 | 10643 | `		nNew = pVm->nRefSize << 1;` |
|     3588 | 10644 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     3588 | 10645 | `		if( apNew ){` |
|     3588 | 10646 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 10647 | `			sxu32 n;` |
|        - | 10648 | `			/* Zero the structure */` |
|     3588 | 10649 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 10650 | `			/* Rehash all referenced entries */` |
|  2835972 | 10651 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 10652 | `				/* Remove old collision links */` |
|  2832386 | 10653 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 10654 | `				/* Point to the appropriate bucket */` |
|  2832386 | 10655 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 10656 | `				/* Insert the entry  */` |
|  2832386 | 10657 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2832386 | 10658 | `				if( apNew[nBucket] ){` |
|  2298896 | 10659 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 10660 | `				}` |
|  2832386 | 10661 | `				apNew[nBucket] = pEntry;` |
|        - | 10662 | `				/* Point to the next entry */` |
|  2832386 | 10663 | `				pEntry = pEntry->pNext;` |
|  1416194 | 10664 | `			}` |
|        - | 10665 | `			/* Release the old table */` |
|     3588 | 10666 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 10667 | `			/* Install the new one */` |
|     3588 | 10668 | `			pVm->apRefObj = apNew;` |
|     3588 | 10669 | `			pVm->nRefSize = nNew;` |
|     1793 | 10670 | `		}` |
|     1793 | 10671 | `	}` |
|        - | 10672 | `	/* Point to the appropriate bucket */` |
|  2996428 | 10673 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 10674 | `	/* Insert the entry */` |
|  2996428 | 10675 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  2996428 | 10676 | `	if( pVm->apRefObj[nBucket] ){` |
|  2485292 | 10677 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1242807 | 10678 | `	}` |
|  2996428 | 10679 | `	pVm->apRefObj[nBucket] = pRef;` |
|  2996428 | 10680 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  2996428 | 10681 | `	pVm->nRefUsed++;` |
|  2996428 | 10682 | `	return SXRET_OK;` |
|        2 | 10683 |  |
|        - | 10684 | `/*` |
|        - | 10685 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 10686 | ` * the reference table.` |
|        - | 10687 | ` * This function is invoked when the user perform an unset` |
|        - | 10688 | ` * call [i.e: unset($var); ].` |
|        - | 10689 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10690 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10691 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10692 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10693 | ` * Refer to the official for more information on this powerful` |
|        - | 10694 | ` * extension.` |
|        - | 10695 | ` */` |
|  2963988 | 10696 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 10697 |  |
|        - | 10698 | `	ph7_hashmap_node **apNode;` |
|        - | 10699 | `	SyHashEntry **apEntry;` |
|        - | 10700 | `	sxu32 n;` |
|        - | 10701 | `	/* Point to the reference table */` |
|  2963990 | 10702 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  2963990 | 10703 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 10704 | `	/* Unlink the entry from the reference table */` |
|  3045124 | 10705 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|    81136 | 10706 | `		if( apEntry[n] ){` |
|    81086 | 10707 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    40542 | 10708 | `		}` |
|    40569 | 10709 | `	}` |
|  5848682 | 10710 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  2884694 | 10711 | `		if( apNode[n] ){` |
|     5638 | 10712 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     2818 | 10713 | `		}` |
|  1442348 | 10714 | `	}` |
|  2963990 | 10715 | `	if( pRef->pPrevCollide ){` |
|  1115420 | 10716 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   558014 | 10717 | `	}else{` |
|  1848572 | 10718 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 10719 | `	}` |
|  2963990 | 10720 | `	if( pRef->pNextCollide ){` |
|  1673228 | 10721 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   836797 | 10722 | `	}` |
|  2963990 | 10723 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 10724 | `	/* Release the node */` |
|  2963990 | 10725 | `	SySetRelease(&pRef->aReference);` |
|  2963990 | 10726 | `	SySetRelease(&pRef->aArrEntries);` |
|  2963990 | 10727 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  2963990 | 10728 | `	pVm->nRefUsed--;` |
|  2963990 | 10729 | `	return SXRET_OK;` |
|        2 | 10730 |  |
|        - | 10731 | `/*` |
|        - | 10732 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 10733 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10734 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10735 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10736 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10737 | ` * Refer to the official for more information on this powerful` |
|        - | 10738 | ` * extension.` |
|        - | 10739 | ` */` |
|  3025228 | 10740 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 10741 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10742 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10743 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10744 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 10745 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 10746 | `	)` |
|        2 | 10747 |  |
|  3025230 | 10748 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10749 | `	VmRefObj *pRef;` |
|        - | 10750 | `	/* Check if the referenced object already exists */` |
|  3025230 | 10751 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3025230 | 10752 | `	if( pRef == 0 ){` |
|        - | 10753 | `		/* Create a new entry */` |
|  2996428 | 10754 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  2996428 | 10755 | `		if( pRef == 0 ){` |
|      ! 0 | 10756 | `			return SXERR_MEM;` |
|        - | 10757 | `		}` |
|  2996428 | 10758 | `		pRef->iFlags = iFlags;` |
|        - | 10759 | `		/* Install the entry */` |
|  2996428 | 10760 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1498213 | 10761 | `	}` |
|  3025230 | 10762 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3025230 | 10763 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 10764 | `		VmSlot sRef;` |
|        - | 10765 | `		/* Local frame,record referenced entry so that it can` |
|        - | 10766 | `		 * be deleted when we leave this frame.` |
|        - | 10767 | `		 */` |
|    76208 | 10768 | `		sRef.nIdx = nIdx;` |
|    76208 | 10769 | `		sRef.pUserData = pEntry;` |
|    76208 | 10770 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 10771 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 10772 | `		}` |
|    38103 | 10773 | `	}` |
|  3025230 | 10774 | `	if( pEntry ){` |
|        - | 10775 | `		/* Address of the hash-entry */` |
|   104820 | 10776 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    52409 | 10777 | `	}` |
|  3025230 | 10778 | `	if( pMapEntry ){` |
|        - | 10779 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  2915542 | 10780 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1457770 | 10781 | `	}` |
|  3025230 | 10782 | `	return SXRET_OK;` |
|  1512616 | 10783 |  |
|        - | 10784 | `/*` |
|        - | 10785 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 10786 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 10787 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 10788 | ` * the reference implementation is consistent,solid and it's` |
|        - | 10789 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 10790 | ` * Refer to the official for more information on this powerful` |
|        - | 10791 | ` * extension.` |
|        - | 10792 | ` */` |
|  2955268 | 10793 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 10794 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 10795 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 10796 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 10797 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 10798 | `	)` |
|        2 | 10799 |  |
|        - | 10800 | `	VmRefObj *pRef;` |
|        - | 10801 | `	sxu32 n;` |
|        - | 10802 | `	/* Check if the referenced object already exists */` |
|  2955270 | 10803 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  2955270 | 10804 | `	if( pRef == 0 ){` |
|        - | 10805 | `		/* Not such entry */` |
|    76154 | 10806 | `		return SXERR_NOTFOUND;` |
|        - | 10807 | `	}` |
|        - | 10808 | `	/* Remove the desired entry */` |
|  2879118 | 10809 | `	if( pEntry ){` |
|        - | 10810 | `		SyHashEntry **apEntry;` |
|       56 | 10811 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      210 | 10812 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      156 | 10813 | `			if( apEntry[n] == pEntry ){` |
|        - | 10814 | `				/* Nullify the entry */` |
|       56 | 10815 | `				apEntry[n] = 0;` |
|        - | 10816 | `				/*` |
|        - | 10817 | `				 * NOTE:` |
|        - | 10818 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 10819 | `				 * we avoid wasting spaces.` |
|        - | 10820 | `				 */` |
|       27 | 10821 | `			}` |
|       79 | 10822 | `		}` |
|       27 | 10823 | `	}` |
|  2879118 | 10824 | `	if( pMapEntry ){` |
|        - | 10825 | `		ph7_hashmap_node **apNode;` |
|  2879064 | 10826 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  5758214 | 10827 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  2879152 | 10828 | `			if( apNode[n] == pMapEntry ){` |
|        - | 10829 | `				/* nullify the entry */` |
|  2879064 | 10830 | `				apNode[n] = 0;` |
|  1439531 | 10831 | `			}` |
|  1439577 | 10832 | `		}` |
|  1439531 | 10833 | `	}` |
|  2879118 | 10834 | `	return SXRET_OK;` |
|  1477636 | 10835 |  |
|        - | 10836 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 10837 | `/*` |
|        - | 10838 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 10839 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 10840 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 10841 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 10842 | ` * For more information on how to register IO stream devices,please` |
|        - | 10843 | ` * refer to the official documentation.` |
|        - | 10844 | ` */` |
|    23090 | 10845 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 10846 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 10847 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 10848 | `	int nByte              /* *pzDevice length*/` |
|        - | 10849 | `	)` |
|        2 | 10850 |  |
|        - | 10851 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 10852 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 10853 | `	SyString sDev,sCur;` |
|        - | 10854 | `	sxu32 n,nEntry;` |
|        - | 10855 | `	int rc;` |
|        - | 10856 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    23092 | 10857 | `	zNext = zCur = zIn = *pzDevice;` |
|    23092 | 10858 | `	zEnd = &zIn[nByte];` |
|  1477646 | 10859 | `	while( zIn < zEnd ){` |
|  1454558 | 10860 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 10861 | `			/* Got one */` |
|        3 | 10862 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 10863 | `			break;` |
|        - | 10864 | `		}` |
|        - | 10865 | `		/* Advance the cursor */` |
|  1454556 | 10866 | `		zIn++;` |
|        2 | 10867 | `	}` |
|    23092 | 10868 | `	if( zIn >= zEnd ){` |
|        - | 10869 | `		/* No such scheme,return the default stream */` |
|    23090 | 10870 | `		return pVm->pDefStream;` |
|        - | 10871 | `	}` |
|        3 | 10872 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 10873 | `	/* Remove leading and trailing white spaces */` |
|        3 | 10874 | `	SyStringFullTrim(&sDev);` |
|        - | 10875 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 10876 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 10877 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 10878 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 10879 | `		pStream = apStream[n];` |
|        3 | 10880 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 10881 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 10882 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 10883 | `		if( rc == 0 ){` |
|        - | 10884 | `			/* Stream device found */` |
|        3 | 10885 | `			*pzDevice = zNext;` |
|        3 | 10886 | `			return pStream;` |
|        - | 10887 | `		}` |
|      ! 0 | 10888 | `	}` |
|        - | 10889 | `	/* No such stream,return NULL */` |
|      ! 0 | 10890 | `	return 0;` |
|    11547 | 10891 |  |
|        - | 10892 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 10893 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 10894 |  |
